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

#include <chrono>
#include <curl/curl.h>
#include <glib.h>
#include <string>

/**
 * brief A type for handling setting dynamic text overlay string via VAPIX.
 *
 * This is not needed for OPC UA, but enables the camera to use the extracted
 * Gauge reading in overlays, which can add value to the live view.
 */
class DynamicStringHandler
{
  public:
    DynamicStringHandler(const guint8 nbr);
    DynamicStringHandler() : DynamicStringHandler(1)
    {
    }
    ~DynamicStringHandler();
    void SetStrNumber(const guint8 newnbr);
    void UpdateStr(const double value);

  private:
    std::string RetrieveVapixCredentials(const char &username) const;
    gboolean VapixGet(const std::string &url);

    CURL *curl_;
    guint8 nbr_;
    std::chrono::time_point<std::chrono::steady_clock> lastupdate_;
};
