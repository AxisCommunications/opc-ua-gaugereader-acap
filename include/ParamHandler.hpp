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
        const gchar &app_name,
        void (*restart_opcuaserver)(unsigned int),
        void (*replace_gauge)(),
        void (*set_dynstr_nbr)(const guint8));
    ~ParamHandler();
    static void param_callback(const gchar *name, const gchar *value, void *data);

    gboolean GetClockwise()
    {
        return clockwise_;
    };
    cv::Point GetCenterPoint()
    {
        return center_point_;
    };
    cv::Point GetMinPoint()
    {
        return min_point_;
    };
    cv::Point GetMaxPoint()
    {
        return max_point_;
    };

  private:
    gchar *get_param(const gchar &name);
    gboolean get_param_int(const gchar &name, int &val);
    void update_local_param(const gchar &name, const guint32 val);
    gboolean setup_param(const gchar *name, AXParameterCallback callbackfn);

    void (*restart_opcuaserver_)(const guint32);
    void (*replace_gauge_)();
    void (*set_dynstr_nbr_)(const guint8);

    AXParameter *axparameter_;
    gboolean clockwise_;
    cv::Point center_point_;
    cv::Point min_point_;
    cv::Point max_point_;
};
