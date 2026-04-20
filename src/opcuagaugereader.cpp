/**
 * Copyright (C) 2025, Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <format>
#include <mutex>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <string>
#include <syslog.h>
#include <utility>

#include "DynamicStringHandler.hpp"
#include "EventPusher.hpp"
#include "Gauge.hpp"
#include "ImageProvider.hpp"
#include "OpcUaServer.hpp"
#include "ParamHandler.hpp"
#include "common.hpp"

using namespace cv;
using namespace std;

static GMainLoop *loop_ = nullptr;

static mutex mtx_;

static Gauge *gauge_ = nullptr;
static OpcUaServer opcuaserver_;
static EventPusher evpusher_;
static gdouble lastvalue_ = -1.0;

static ImageProvider *provider_ = nullptr;
static Mat nv12_mat_;
static Mat gray_mat_;

static DynamicStringHandler *dynstr_handler_ = nullptr;
static ParamHandler *param_handler_ = nullptr;

static void restart_opcuaserver(const guint32 port)
{
    mtx_.lock();
    if (opcuaserver_.IsRunning())
    {
        opcuaserver_.ShutDownServer();
    }
    if (!opcuaserver_.LaunchServer(port))
    {
        LOG_E("%s/%s: Failed to launch OPC UA server", __FILE__, __FUNCTION__);
        assert(false);
    }
    mtx_.unlock();
}

static void replace_gauge()
{
    mtx_.lock();
    if (nullptr != gauge_)
    {
        delete gauge_;
        gauge_ = nullptr;
    }
    mtx_.unlock();
}

static void set_dynstr_nbr(const guint8 port)
{
    assert(nullptr != dynstr_handler_);
    mtx_.lock();
    dynstr_handler_->SetStrNumber(port);
    mtx_.unlock();
}

static gboolean imageanalysis(gpointer data)
{
    (void)data;
    // Get the latest NV12 image frame from VDO using the imageprovider
    assert(nullptr != provider_);
    auto buf = provider_->GetLastFrameBlocking();
    if (nullptr == buf)
    {
        LOG_I("%s/%s: No more frames available, exiting", __FILE__, __FUNCTION__);
        return TRUE;
    }

    // Assign the VDO image buffer to the nv12_mat OpenCV Mat.
    // This specific Mat is used as it is the one we created for NV12,
    // which has a different layout than e.g., BGR.
    mtx_.lock();
    nv12_mat_.data = static_cast<uint8_t *>(vdo_buffer_get_data(buf));

    // Convert the NV12 data to GRAY
    cvtColor(nv12_mat_, gray_mat_, COLOR_YUV2GRAY_NV12, 1);

    // Create gauge if nonexistent
    if (nullptr == gauge_)
    {
        LOG_I("%s/%s: Set up new Gauge", __FILE__, __FUNCTION__);
        assert(nullptr != param_handler_);
        gauge_ = new Gauge(
            gray_mat_,
            param_handler_->GetCenterPoint(),
            param_handler_->GetMinPoint(),
            param_handler_->GetMaxPoint(),
            param_handler_->GetClockwise());
    }
    assert(nullptr != gauge_);
    auto value = gauge_->ComputeGaugeValue(gray_mat_);
    mtx_.unlock();
    // Successfully read values range between 0 and 100 percent; if no value
    // could be read the computation will return -1
    assert(value <= 100.0);
    if (0 > value)
    {
        LOG_E("%s/%s: Failed to read out Gauge value from current scene/setup", __FILE__, __FUNCTION__);
    }
    else
    {
        const auto rounddecimals = param_handler_->GetRoundToDecimals();
        string value_str;
        if (-1 < rounddecimals)
        {
            // Round value if limited amount of decimals is requested
            const double factor = pow(10.0, rounddecimals);
            value = round(value * factor) / factor;
            value_str = std::format("{:.{}f}", value, rounddecimals);
            LOG_I("%s/%s: Value (with %i decimals) was %s", __FILE__, __FUNCTION__, rounddecimals, value_str.c_str());
        }
        else
        {
            value_str = std::to_string(value);
            LOG_I("%s/%s: Value (with unlimited decimals) was %s", __FILE__, __FUNCTION__, value_str.c_str());
        }
        opcuaserver_.UpdateGaugeValue(value);
        if (value != lastvalue_)
        {
            assert(nullptr != dynstr_handler_);
            dynstr_handler_->UpdateStr(value_str);
            if (evpusher_.Send(value, TRUE))
            {
                lastvalue_ = value;
            }
        }
    }

    // Release the VDO frame buffer
    provider_->ReturnFrame(*buf);

    return TRUE;
}

static gboolean initimageanalysis(void)
{
    // The desired width and height of the BGR frame
    const unsigned int width = 640;
    const unsigned int height = 360;

    // chooseStreamResolution gets the least resource intensive stream
    // that exceeds or equals the desired resolution specified above
    unsigned int streamWidth = 0;
    unsigned int streamHeight = 0;
    if (!ImageProvider::ChooseStreamResolution(width, height, streamWidth, streamHeight))
    {
        LOG_E("%s/%s: Failed choosing stream resolution", __FILE__, __FUNCTION__);
        return FALSE;
    }

    LOG_I("Creating VDO image provider and creating stream %d x %d", streamWidth, streamHeight);
    // TODO: Could we use the subformat Y800 to get gray 1-channel image directly?
    provider_ = new ImageProvider(streamWidth, streamHeight, 2, VDO_FORMAT_YUV);
    if (!provider_)
    {
        LOG_E("%s/%s: Failed to create ImageProvider", __FILE__, __FUNCTION__);
        return FALSE;
    }

    LOG_I("Start fetching video frames from VDO");
    if (!ImageProvider::StartFrameFetch(*provider_))
    {
        LOG_E("%s/%s: Failed to fetch frames from VDO", __FILE__, __FUNCTION__);
        return FALSE;
    }

    // OpenCV represents NV12 with 1.5 bytes per pixel
    nv12_mat_ = Mat(height * 3 / 2, width, CV_8UC1);
    gray_mat_ = Mat(height, width, CV_8UC1);

    return TRUE;
}

static void signalHandler(int signal_num)
{
    switch (signal_num)
    {
    case SIGTERM:
    case SIGABRT:
    case SIGINT:
        if (nullptr != provider_)
        {
            ImageProvider::StopFrameFetch(*provider_);
        }
        g_main_loop_quit(loop_);
        break;
    default:
        break;
    }
}

static bool initializeSignalHandler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));

    if (-1 == sigemptyset(&sa.sa_mask))
    {
        LOG_E("Failed to initialize signal handler (%s)", strerror(errno));
        return false;
    }

    sa.sa_handler = signalHandler;

    if (0 > sigaction(SIGTERM, &sa, NULL) || 0 > sigaction(SIGABRT, &sa, NULL) || 0 > sigaction(SIGINT, &sa, NULL))
    {
        LOG_E("Failed to install signal handler (%s)", strerror(errno));
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    (void)argc;

    const auto app_name = basename(argv[0]);
    openlog(app_name, LOG_PID | LOG_CONS, LOG_USER);

    int result = EXIT_SUCCESS;
    if (!initializeSignalHandler())
    {
        result = EXIT_FAILURE;
        goto exit;
    }

    // Init dynamic string handling
    dynstr_handler_ = new DynamicStringHandler();

    // Init parameter handling (will also launch OPC UA server)
    LOG_I("Init parameter handling and launch OPC UA server ...");
    param_handler_ = new ParamHandler(app_name, restart_opcuaserver, replace_gauge, set_dynstr_nbr);
    if (nullptr == param_handler_)
    {
        LOG_E("%s/%s: Failed to set up parameter handler and launch OPC UA server", __FILE__, __FUNCTION__);
        result = EXIT_FAILURE;
        goto exit;
    }

    // Initialize image analysis
    if (!initimageanalysis())
    {
        LOG_E("%s/%s: Failed to init image analysis", __FILE__, __FUNCTION__);
        result = EXIT_FAILURE;
        goto exit_param;
    }

    // Add image analysis as idle function
    if (1 > g_idle_add(imageanalysis, nullptr))
    {
        LOG_E("%s/%s: Failed to add idle function", __FILE__, __FUNCTION__);
        result = EXIT_FAILURE;
        goto exit_param;
    }

    LOG_I("Start main loop ...");
    assert(nullptr == loop_);
    loop_ = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop_);

    // Cleanup
    LOG_I("Shutdown ...");
    g_main_loop_unref(loop_);
    if (nullptr != provider_)
    {
        delete provider_;
    }
    opcuaserver_.ShutDownServer();

exit_param:
    delete param_handler_;

exit:
    delete dynstr_handler_;
    LOG_I("Exiting!");
    closelog();

    return result;
}
