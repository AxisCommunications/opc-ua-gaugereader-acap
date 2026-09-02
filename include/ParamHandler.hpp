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

#pragma once

#include <axparameter.h>
#include <opencv2/core/core.hpp>

class ParamHandler
{
  public:
    ParamHandler(
        const gchar *app_name,
        void (*RestartOpcuaserver)(const guint32),
        void (*ReplaceGauge)(),
        void (*SetDynstrNbr)(const guint8));
    ~ParamHandler();
    static void ParamCallback(const gchar *name, const gchar *value, void *data);

    gboolean GetClockwise() const
    {
        g_mutex_lock(&mtx_);
        const auto clockwise = clockwise_;
        g_mutex_unlock(&mtx_);
        return clockwise;
    };
    cv::Point GetCenterPoint() const
    {
        g_mutex_lock(&mtx_);
        const auto center_point = center_point_;
        g_mutex_unlock(&mtx_);
        return center_point;
    };
    cv::Point GetMinPoint() const
    {
        g_mutex_lock(&mtx_);
        const auto min_point = min_point_;
        g_mutex_unlock(&mtx_);
        return min_point;
    };
    cv::Point GetMaxPoint() const
    {
        g_mutex_lock(&mtx_);
        const auto max_point = max_point_;
        g_mutex_unlock(&mtx_);
        return max_point;
    };
    gint8 GetRoundToDecimals() const
    {
        g_mutex_lock(&mtx_);
        const auto round_to_decimals = round_to_decimals_;
        g_mutex_unlock(&mtx_);
        return round_to_decimals;
    };

  private:
    gchar *GetParam(const gchar &name) const;
    gboolean GetParam(const gchar &name, gint32 &val) const;
    void UpdateLocalParam(const gchar &name, const gint32 val);
    gboolean SetupParam(const gchar *name, AXParameterCallback callbackfn);

    void (*RestartOpcuaserver_)(const guint32);
    void (*ReplaceGauge_)();
    void (*SetDynstrNbr_)(const guint8);

    AXParameter *axparameter_;
    gboolean clockwise_;
    gint8 round_to_decimals_;
    cv::Point center_point_;
    cv::Point min_point_;
    cv::Point max_point_;
    mutable GMutex mtx_;
};
