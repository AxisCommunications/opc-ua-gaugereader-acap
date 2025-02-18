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

#include <utility>

#include <mutex>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <string>
#include <syslog.h>

#include "DynamicStringHandler.hpp"
#include "Gauge.hpp"
#include "ImageProvider.hpp"
#include "OpcUaServer.hpp"
#include "ParamHandler.hpp"
#include "common.hpp"

using namespace cv;
using namespace std;

static GMainLoop *loop = nullptr;

static mutex mtx;

static Gauge *gauge = nullptr;
static OpcUaServer opcuaserver;

static ImageProvider *provider = nullptr;
static Mat nv12_mat;
static Mat gray_mat;

static DynamicStringHandler *dynstr_handler = nullptr;
static ParamHandler *param_handler = nullptr;

static void restart_opcuaserver(const guint32 port)
{
    mtx.lock();
    if (opcuaserver.IsRunning())
    {
        opcuaserver.ShutDownServer();
    }
    if (!opcuaserver.LaunchServer(port))
    {
        LOG_E("%s/%s: Failed to launch OPC UA server", __FILE__, __FUNCTION__);
        assert(false);
    }
    mtx.unlock();
}

static void replace_gauge()
{
    mtx.lock();
    if (nullptr != gauge)
    {
        delete gauge;
        gauge = nullptr;
    }
    mtx.unlock();
}

static void set_dynstr_nbr(const guint8 port)
{
    assert(nullptr != dynstr_handler);
    mtx.lock();
    dynstr_handler->SetStrNumber(port);
    mtx.unlock();
}

static gboolean imageanalysis(gpointer data)
{
    (void)data;
    // Get the latest NV12 image frame from VDO using the imageprovider
    assert(nullptr != provider);
    VdoBuffer *buf = ImageProvider::GetLastFrameBlocking(*provider);
    if (!buf)
    {
        LOG_I("%s/%s: No more frames available, exiting", __FILE__, __FUNCTION__);
        return TRUE;
    }

    // Assign the VDO image buffer to the nv12_mat OpenCV Mat.
    // This specific Mat is used as it is the one we created for NV12,
    // which has a different layout than e.g., BGR.
    mtx.lock();
    nv12_mat.data = static_cast<uint8_t *>(vdo_buffer_get_data(buf));

    // Convert the NV12 data to GRAY
    cvtColor(nv12_mat, gray_mat, COLOR_YUV2GRAY_NV12, 1);

    // Create gauge if nonexistent
    if (nullptr == gauge)
    {
        LOG_I("%s/%s: Set up new Gauge", __FILE__, __FUNCTION__);
        assert(nullptr != param_handler);
        gauge = new Gauge(
            gray_mat,
            param_handler->GetCenterPoint(),
            param_handler->GetMinPoint(),
            param_handler->GetMaxPoint(),
            param_handler->GetClockwise());
    }
    assert(nullptr != gauge);
    double value = gauge->ComputeGaugeValue(gray_mat);
    mtx.unlock();
    // Successfully read values range between 0 and 100 percent; if no value
    // could be read the computation will return -1
    assert(value <= 100.0);
    if (0 > value)
    {
        LOG_E("%s/%s: Failed to read out Gauge value from current scene/setup", __FILE__, __FUNCTION__);
    }
    else
    {
        LOG_I("%s/%s: Value was %f", __FILE__, __FUNCTION__, value);
        opcuaserver.UpdateGaugeValue(value);
        if (nullptr != dynstr_handler)
        {
            dynstr_handler->UpdateStr(value);
        }
    }

    // Release the VDO frame buffer
    ImageProvider::ReturnFrame(*provider, *buf);

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
    provider = new ImageProvider(streamWidth, streamHeight, 2, VDO_FORMAT_YUV);
    if (!provider)
    {
        LOG_E("%s/%s: Failed to create ImageProvider", __FILE__, __FUNCTION__);
        return FALSE;
    }
    if (!provider->InitImageProvider())
    {
        LOG_E("%s/%s: Failed to init ImageProvider", __FILE__, __FUNCTION__);
        return FALSE;
    }

    LOG_I("Start fetching video frames from VDO");
    if (!ImageProvider::StartFrameFetch(*provider))
    {
        LOG_E("%s/%s: Failed to fetch frames from VDO", __FILE__, __FUNCTION__);
        return FALSE;
    }

    // OpenCV represents NV12 with 1.5 bytes per pixel
    nv12_mat = Mat(height * 3 / 2, width, CV_8UC1);
    gray_mat = Mat(height, width, CV_8UC1);

    return TRUE;
}

static void signalHandler(int signal_num)
{
    switch (signal_num)
    {
    case SIGTERM:
    case SIGABRT:
    case SIGINT:
        if (nullptr != provider)
        {
            ImageProvider::StopFrameFetch(*provider);
        }
        g_main_loop_quit(loop);
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

    auto app_name = basename(argv[0]);
    openlog(app_name, LOG_PID | LOG_CONS, LOG_USER);

    int result = EXIT_SUCCESS;
    if (!initializeSignalHandler())
    {
        result = EXIT_FAILURE;
        goto exit;
    }

    // Init dynamic string handling
    dynstr_handler = new DynamicStringHandler();

    // Init parameter handling (will also launch OPC UA server)
    LOG_I("Init parameter handling and launch OPC UA server ...");
    param_handler = new ParamHandler(*app_name, restart_opcuaserver, replace_gauge, set_dynstr_nbr);
    if (nullptr == param_handler)
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
    assert(nullptr == loop);
    loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    // Cleanup
    LOG_I("Shutdown ...");
    g_main_loop_unref(loop);
    if (nullptr != provider)
    {
        delete provider;
    }
    opcuaserver.ShutDownServer();

exit_param:
    delete param_handler;

exit:
    delete dynstr_handler;
    LOG_I("Exiting!");
    closelog();

    return result;
}
