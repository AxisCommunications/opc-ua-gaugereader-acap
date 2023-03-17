/**
 * Copyright (C) 2022, Axis Communications AB, Lund, Sweden
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

#include <axparameter.h>
#include <utility>

#include <mutex>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <string>
#include <syslog.h>

#include "common.hpp"
#include "gauge.hpp"
#include "imgprovider.hpp"
#include "opcuaserver.hpp"

using namespace cv;
using namespace std;

static mutex mtx;

static bool clockwise;
static Point center_point;
static Point min_point;
static Point max_point;
static Gauge *gauge = nullptr;
static OpcUaServer opcuaserver;

static ImgProvider *provider = nullptr;
static Mat nv12_mat;
static Mat gray_mat;

static gchar *get_param(AXParameter &axparameter, const gchar &name)
{
    GError *error = nullptr;
    gchar *value = nullptr;
    if (!ax_parameter_get(&axparameter, &name, &value, &error))
    {
        LOG_E("%s/%s: failed to get %s parameter", __FILE__, __FUNCTION__, &name);
        if (nullptr != error)
        {
            LOG_E("%s/%s: %s", __FILE__, __FUNCTION__, error->message);
            g_error_free(error);
        }
        return nullptr;
    }
    LOG_I("Got %s value: %s", &name, value);
    return value;
}

static gboolean get_param_int(AXParameter &axparameter, const gchar &name, int &val)
{
    auto valuestr = get_param(axparameter, name);
    if (nullptr == valuestr)
    {
        return FALSE;
    }
    val = atoi(valuestr);
    free(valuestr);
    return TRUE;
}

static void update_local_param(const gchar &name, const uint32_t val)
{
    // Parameters that do not change the gauge reader go here
    if (0 == strncmp("port", &name, 4))
    {
        if (opcuaserver.IsRunning())
        {
            opcuaserver.ShutDownServer();
        }
        if (!opcuaserver.LaunchServer(val))
        {
            LOG_E("%s/%s: Failed to launch OPC UA server", __FILE__, __FUNCTION__);
            assert(false);
        }
        return;
    }

    // The following parameters trigger recalibration of the gauge
    if (0 == strncmp("cloc", &name, 4))
    {
        clockwise = (1 == val);
    }
    else if (0 == strncmp("centerX", &name, 7))
    {
        center_point.x = val;
    }
    else if (0 == strncmp("centerY", &name, 7))
    {
        center_point.y = val;
    }
    else if (0 == strncmp("minX", &name, 4))
    {
        min_point.x = val;
    }
    else if (0 == strncmp("minY", &name, 4))
    {
        min_point.y = val;
    }
    else if (0 == strncmp("maxX", &name, 4))
    {
        max_point.x = val;
    }
    else if (0 == strncmp("maxY", &name, 4))
    {
        max_point.y = val;
    }
    else
    {
        LOG_E("%s/%s: FAILED to act on param %s", __FILE__, __FUNCTION__, &name);
        assert(false);
    }

    // Recalibrate gauge at next frame
    mtx.lock();
    if (nullptr != gauge)
    {
        delete gauge;
        gauge = nullptr;
    }
    mtx.unlock();
}

static void param_callback(const gchar *name, const gchar *value, void *data)
{
    assert(nullptr != name);
    assert(nullptr != value);
    (void)data;
    if (nullptr == value)
    {
        LOG_E("%s/%s: Unexpected nullptr value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    LOG_I("Update for parameter %s (%s)", name, value);
    auto lastdot = strrchr(name, '.');
    assert(nullptr != lastdot);
    assert(1 < strlen(name) - strlen(lastdot));
    update_local_param(lastdot[1], atoi(value));
}

static gboolean setup_param(AXParameter &axparameter, const gchar *name, AXParameterCallback callbackfn)
{
    GError *error = nullptr;

    assert(nullptr != name);
    assert(nullptr != callbackfn);

    if (!ax_parameter_register_callback(&axparameter, name, callbackfn, &axparameter, &error))
    {
        LOG_E("%s/%s: failed to register %s callback", __FILE__, __FUNCTION__, name);
        if (nullptr != error)
        {
            LOG_E("%s/%s: %s", __FILE__, __FUNCTION__, error->message);
            g_error_free(error);
        }
        return FALSE;
    }

    int val;
    if (!get_param_int(axparameter, *name, val))
    {
        LOG_E("%s/%s: Failed to get initial value for %s", __FILE__, __FUNCTION__, name);
        return FALSE;
    }
    update_local_param(*name, val);
    LOG_I("%s/%s: Set up parameter %s", __FILE__, __FUNCTION__, name);

    return TRUE;
}

static gboolean imageanalysis(gpointer data)
{
    (void)data;
    // Get the latest NV12 image frame from VDO using the imageprovider
    assert(nullptr != provider);
    VdoBuffer *buf = ImgProvider::GetLastFrameBlocking(*provider);
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
        LOG_I("%s/%s: Set up new gauge", __FILE__, __FUNCTION__);
        gauge = new Gauge(gray_mat, center_point, min_point, max_point, clockwise);
    }
    assert(nullptr != gauge);
    double value = gauge->ComputeGaugeValue(gray_mat);
    mtx.unlock();
    // Successfully read values range between 0 and 100 percent; if no value
    // could be read the computation will return -1
    assert(value <= 100.0);
    if (0 > value)
    {
        LOG_E("%s/%s: Failed to read out gauge value from current scene/setup", __FILE__, __FUNCTION__);
    }
    else
    {
        LOG_I("%s/%s: Value was %f", __FILE__, __FUNCTION__, value);
        opcuaserver.UpdateGaugeValue(value);
    }

    // Release the VDO frame buffer
    ImgProvider::ReturnFrame(*provider, *buf);

    return TRUE;
}

static gboolean initimageanalysis(void)
{
    // The desired width and height of the BGR frame
    const unsigned int width = 1024;
    const unsigned int height = 576;

    // chooseStreamResolution gets the least resource intensive stream
    // that exceeds or equals the desired resolution specified above
    unsigned int streamWidth = 0;
    unsigned int streamHeight = 0;
    if (!ImgProvider::ChooseStreamResolution(width, height, streamWidth, streamHeight))
    {
        LOG_E("%s/%s: Failed choosing stream resolution", __FILE__, __FUNCTION__);
        return FALSE;
    }

    LOG_I("Creating VDO image provider and creating stream %d x %d", streamWidth, streamHeight);
    // TODO: Could we use the subformat Y800 to get gray 1-channel image directly?
    provider = new ImgProvider(streamWidth, streamHeight, 2, VDO_FORMAT_YUV);
    if (!provider)
    {
        LOG_E("%s/%s: Failed to create ImgProvider", __FILE__, __FUNCTION__);
        return FALSE;
    }
    if (!provider->InitImgProvider())
    {
        LOG_E("%s/%s: Failed to init ImgProvider", __FILE__, __FUNCTION__);
        return FALSE;
    }

    LOG_I("Start fetching video frames from VDO");
    if (!ImgProvider::StartFrameFetch(*provider))
    {
        LOG_E("%s/%s: Failed to fetch frames from VDO", __FILE__, __FUNCTION__);
        return FALSE;
    }

    // OpenCV represents NV12 with 1.5 bytes per pixel
    nv12_mat = Mat(height * 3 / 2, width, CV_8UC1);
    gray_mat = Mat(height, width, CV_8UC1);

    return TRUE;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const char *app_name = "opcuagaugereader";
    openlog(app_name, LOG_PID | LOG_CONS, LOG_USER);
    LOG_I("Running OpenCV example with VDO as video source");

    int result = EXIT_SUCCESS;
    GMainLoop *loop;

    // Init parameter handling (will also launch OPC UA server)
    LOG_I("Init parameter handling ...");
    GError *error = nullptr;
    AXParameter *axparameter = ax_parameter_new(app_name, &error);
    if (nullptr != error)
    {
        LOG_E("%s/%s: ax_parameter_new failed (%s)", __FILE__, __FUNCTION__, error->message);
        g_error_free(error);
        return EXIT_FAILURE;
    }
    LOG_I("%s/%s: ax_parameter_new success", __FILE__, __FUNCTION__);
    // clang-format off
    if (!setup_param(*axparameter, "clockwise", param_callback) ||
        !setup_param(*axparameter, "port", param_callback) ||
	!setup_param(*axparameter, "centerX", param_callback) ||
        !setup_param(*axparameter, "centerY", param_callback) ||
        !setup_param(*axparameter, "minX", param_callback) ||
        !setup_param(*axparameter, "minY", param_callback) ||
	!setup_param(*axparameter, "maxX", param_callback) ||
        !setup_param(*axparameter, "maxY", param_callback))
    // clang-format on
    {
        ax_parameter_free(axparameter);
        return EXIT_FAILURE;
    }
    // Log retrieved param values
    LOG_I("%s/%s: center: (%u, %u)", __FILE__, __FUNCTION__, center_point.x, center_point.y);
    LOG_I("%s/%s: min: (%u, %u)", __FILE__, __FUNCTION__, min_point.x, min_point.y);
    LOG_I("%s/%s: max: (%u, %u)", __FILE__, __FUNCTION__, max_point.x, max_point.y);

    // Initialize image analysis
    if (!initimageanalysis())
    {
        LOG_E("%s/%s: Failed to init image analysis", __FILE__, __FUNCTION__);
        result = EXIT_FAILURE;
        goto exit;
    }

    // Add image analysis as idle function
    if (1 > g_idle_add(imageanalysis, nullptr))
    {
        LOG_E("%s/%s: Failed to add idle function", __FILE__, __FUNCTION__);
        result = EXIT_FAILURE;
        goto exit;
    }

    LOG_I("Start main loop ...");
    loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);
    LOG_I("Shutdown ...");
    g_main_loop_unref(loop);

exit:
    ax_parameter_free(axparameter);
    if (nullptr != provider)
    {
        ImgProvider::StopFrameFetch(*provider);
        delete provider;
    }
    return result;
}
