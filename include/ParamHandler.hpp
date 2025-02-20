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
        void (*RestartOpcuaserver)(unsigned int),
        void (*ReplaceGauge)(),
        void (*SetDynstrNbr)(const guint8));
    ~ParamHandler();
    static void param_callback(const gchar *name, const gchar *value, void *data);

    gboolean GetClockwise() const
    {
        return clockwise_;
    };
    cv::Point GetCenterPoint() const
    {
        return center_point_;
    };
    cv::Point GetMinPoint() const
    {
        return min_point_;
    };
    cv::Point GetMaxPoint() const
    {
        return max_point_;
    };

  private:
    gchar *GetParam(const gchar &name) const;
    gboolean GetParam(const gchar &name, gint32 &val) const;
    void UpdateLocalParam(const gchar &name, const guint32 val);
    gboolean SetupParam(const gchar *name, AXParameterCallback callbackfn);

    void (*RestartOpcuaserver_)(const guint32);
    void (*ReplaceGauge_)();
    void (*SetDynstrNbr_)(const guint8);

    AXParameter *axparameter_;
    gboolean clockwise_;
    cv::Point center_point_;
    cv::Point min_point_;
    cv::Point max_point_;
    mutable GMutex mtx_;
};
