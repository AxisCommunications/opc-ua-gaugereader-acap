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

#include <assert.h>

#include "ParamHandler.hpp"
#include "common.hpp"

using namespace cv;

ParamHandler::ParamHandler(
    const gchar *app_name,
    void (*RestartOpcuaserver)(const guint32),
    void (*ReplaceGauge)(),
    void (*SetDynstrNbr)(const guint8))
    : RestartOpcuaserver_(RestartOpcuaserver), ReplaceGauge_(ReplaceGauge), SetDynstrNbr_(SetDynstrNbr),
      axparameter_(nullptr), clockwise_(true), center_point_(0, 0), min_point_(0, 0), max_point_(0, 0)
{
    LOG_I("Init parameter handling ...");
    g_mutex_init(&mtx_);
    GError *error = nullptr;
    axparameter_ = ax_parameter_new(app_name, &error);
    if (nullptr != error)
    {
        LOG_E("%s/%s: ax_parameter_new failed (%s)", __FILE__, __FUNCTION__, error->message);
        g_error_free(error);
        assert(FALSE);
    }
    assert(nullptr != axparameter_);
    // clang-format off
    LOG_I("Setting up parameters ...");
    if (!SetupParam("DynamicStringNumber", param_callback) ||
        !SetupParam("centerX", param_callback) ||
        !SetupParam("centerY", param_callback) ||
        !SetupParam("clockwise", param_callback) ||
        !SetupParam("maxX", param_callback) ||
        !SetupParam("maxY", param_callback) ||
        !SetupParam("minX", param_callback) ||
        !SetupParam("minY", param_callback) ||
        !SetupParam("port", param_callback))
    // clang-format on
    {
        LOG_E("%s/%s: Failed to set up parameters", __FILE__, __FUNCTION__);
        assert(FALSE);
    }

    // Log retrieved param values
    LOG_I("%s/%s: center: (%u, %u)", __FILE__, __FUNCTION__, center_point_.x, center_point_.y);
    LOG_I("%s/%s: min: (%u, %u)", __FILE__, __FUNCTION__, min_point_.x, min_point_.y);
    LOG_I("%s/%s: max: (%u, %u)", __FILE__, __FUNCTION__, max_point_.x, max_point_.y);
}

ParamHandler::~ParamHandler()
{
    assert(nullptr != axparameter_);
    ax_parameter_free(axparameter_);
    g_mutex_clear(&mtx_);
}

gchar *ParamHandler::GetParam(const gchar &name) const
{
    assert(nullptr != axparameter_);
    GError *error = nullptr;
    gchar *value = nullptr;
    if (!ax_parameter_get(axparameter_, &name, &value, &error))
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

gboolean ParamHandler::GetParam(const gchar &name, gint32 &val) const
{
    const auto valuestr = GetParam(name);
    if (nullptr == valuestr)
    {
        return FALSE;
    }
    val = atoi(valuestr);
    g_free(valuestr);
    return TRUE;
}

void ParamHandler::UpdateLocalParam(const gchar &name, const guint32 val)
{
    // Parameters that do not change the Gauge reader go here
    if (0 == strncmp("port", &name, 4))
    {
        assert(nullptr != RestartOpcuaserver_);
        RestartOpcuaserver_(val);
        return;
    }
    else if (0 == strncmp("DynamicStringNumber", &name, 19))
    {
        assert(nullptr != SetDynstrNbr_);
        SetDynstrNbr_(val);
        return;
    }

    // The following parameters trigger recalibration of the Gauge
    g_mutex_lock(&mtx_);
    if (0 == strncmp("cloc", &name, 4))
    {
        clockwise_ = (1 == val);
    }
    else if (0 == strncmp("centerX", &name, 7))
    {
        center_point_.x = val;
    }
    else if (0 == strncmp("centerY", &name, 7))
    {
        center_point_.y = val;
    }
    else if (0 == strncmp("minX", &name, 4))
    {
        min_point_.x = val;
    }
    else if (0 == strncmp("minY", &name, 4))
    {
        min_point_.y = val;
    }
    else if (0 == strncmp("maxX", &name, 4))
    {
        max_point_.x = val;
    }
    else if (0 == strncmp("maxY", &name, 4))
    {
        max_point_.y = val;
    }
    else
    {
        LOG_E("%s/%s: FAILED to act on param %s", __FILE__, __FUNCTION__, &name);
        assert(false);
    }
    g_mutex_unlock(&mtx_);

    // Recalibrate gauge at next frame
    assert(nullptr != ReplaceGauge_);
    ReplaceGauge_();
}

void ParamHandler::param_callback(const gchar *name, const gchar *value, void *data)
{
    assert(nullptr != name);
    assert(nullptr != value);
    auto param_handler = static_cast<ParamHandler *>(data);
    if (nullptr == value)
    {
        LOG_E("%s/%s: Unexpected nullptr value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    LOG_I("Update for parameter %s (%s)", name, value);
    const auto lastdot = strrchr(name, '.');
    assert(nullptr != lastdot);
    assert(1 < strlen(name) - strlen(lastdot));
    param_handler->UpdateLocalParam(lastdot[1], atoi(value));
}

gboolean ParamHandler::SetupParam(const gchar *name, AXParameterCallback callbackfn)
{
    assert(nullptr != axparameter_);
    GError *error = nullptr;

    assert(nullptr != name);
    assert(nullptr != callbackfn);

    if (!ax_parameter_register_callback(axparameter_, name, callbackfn, this, &error))
    {
        LOG_E("%s/%s: failed to register %s callback", __FILE__, __FUNCTION__, name);
        if (nullptr != error)
        {
            LOG_E("%s/%s: %s", __FILE__, __FUNCTION__, error->message);
            g_error_free(error);
        }
        return FALSE;
    }

    gint32 val;
    if (!GetParam(*name, val))
    {
        LOG_E("%s/%s: Failed to get initial value for %s", __FILE__, __FUNCTION__, name);
        return FALSE;
    }
    UpdateLocalParam(*name, val);
    LOG_I("%s/%s: Set up parameter %s", __FILE__, __FUNCTION__, name);
    usleep(50000); // mitigate timing issue in parameter handling

    return TRUE;
}
