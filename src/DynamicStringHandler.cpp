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
#include <cstdlib>
#include <gio/gio.h>

#include "DynamicStringHandler.hpp"
#include "common.hpp"

using namespace std;
using namespace std::chrono;

static size_t append_to_string_callback(char *ptr, size_t size, size_t nmemb, string *response)
{
    assert(nullptr != response);
    auto totalsize = size * nmemb;
    response->append(ptr, totalsize);

    return totalsize;
}

DynamicStringHandler::DynamicStringHandler(const guint8 nbr)
    : curl_(nullptr), nbr_(nbr), lastupdate_(steady_clock::now())
{
    assert(1 <= nbr);
    assert(16 >= nbr);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    assert(nullptr != curl_);

    const gchar *user = "example-vapix-user";
    auto credentials = RetrieveVapixCredentials(*user);

    auto curl_init =
        (CURLE_OK == curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, (long)(CURLAUTH_DIGEST | CURLAUTH_BASIC)) &&
         CURLE_OK == curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 2L) &&
         CURLE_OK == curl_easy_setopt(curl_, CURLOPT_USERPWD, credentials.c_str()) &&
         CURLE_OK == curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L) &&
         CURLE_OK == curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 1) &&
         CURLE_OK == curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, append_to_string_callback));

    assert(curl_init);
    LOG_I("%s/%s: Dynamic string handler constructor is done!", __FILE__, __FUNCTION__);
}

DynamicStringHandler::~DynamicStringHandler()
{
    assert(nullptr != curl_);
    curl_easy_cleanup(curl_);
    curl_global_cleanup();
}

void DynamicStringHandler::SetStrNumber(const guint8 newnbr)
{
    nbr_ = newnbr;
    LOG_I("Now using dynamic string number %u", newnbr);
}

void DynamicStringHandler::UpdateStr(const double value)
{
    // We don't need to update too frequently
    auto nowtime = steady_clock::now();
    if (1 > duration_cast<seconds>(nowtime - lastupdate_).count())
    {
        return;
    }

    auto url = "http://127.0.0.12/axis-cgi/dynamicoverlay.cgi?action=settext&text_index=" + to_string(nbr_) +
               "&text=" + to_string(value);
    if (!VapixGet(url))
    {
        LOG_E("%s/%s: Failed to update dynamic string", __FILE__, __FUNCTION__);
    }
    lastupdate_ = nowtime;
}

string DynamicStringHandler::RetrieveVapixCredentials(const char &username) const
{
    GError *error = nullptr;
    auto connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (nullptr == connection)
    {
        LOG_E("Error connecting to D-Bus: %s", error->message);
        g_error_free(error);
        return nullptr;
    }

    const char *bus_name = "com.axis.HTTPConf1";
    const char *object_path = "/com/axis/HTTPConf1/VAPIXServiceAccounts1";
    const char *interface_name = "com.axis.HTTPConf1.VAPIXServiceAccounts1";
    const char *method_name = "GetCredentials";

    auto result = g_dbus_connection_call_sync(
        connection,
        bus_name,
        object_path,
        interface_name,
        method_name,
        g_variant_new("(s)", &username),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);
    if (nullptr == result)
    {
        LOG_E("Error invoking D-Bus method: %s", error->message);
        g_error_free(error);
        return "";
    }

    // Extract credentials string
    const char *credentials_string = nullptr;
    g_variant_get(result, "(&s)", &credentials_string);
    string credentials(credentials_string);
    g_variant_unref(result);

    return credentials;
}

gboolean DynamicStringHandler::VapixGet(const string &url)
{
    assert(nullptr != curl_);

    string response;

    if (CURLE_OK != curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()) ||
        CURLE_OK != curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response))
    {
        LOG_E("%s/%s: Failed to set up cURL options", __FILE__, __FUNCTION__);
        return FALSE;
    }

    auto res = curl_easy_perform(curl_);
    if (res != CURLE_OK)
    {
        LOG_E("%s/%s: curl fail %d '%s''", __FILE__, __FUNCTION__, res, curl_easy_strerror(res));
        return FALSE;
    }

    long response_code;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    if (200 != response_code)
    {
        LOG_E("Got response code %ld with response '%s'", response_code, response.c_str());
        return FALSE;
    }

    return TRUE;
}
