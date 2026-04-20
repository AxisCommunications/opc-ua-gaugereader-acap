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
#include <future>
#include <stdexcept>

#include "EventPusher.hpp"
#include "common.hpp"

using namespace std;

static void declaration_complete(guint declaration, gpointer user_data)
{
    assert(NULL != user_data);
    (void)declaration;

    auto init = static_cast<gboolean *>(user_data);
    *init = TRUE;
    LOG_I("Event declaration complete!");
}

EventPusher::EventPusher() : event_handler_(ax_event_handler_new()), initialized_(FALSE)
{
    assert(nullptr != event_handler_);
    GError *error = nullptr;

    // Create keys, namespaces, and nice names for the event
    auto set = ax_event_key_value_set_new();
    const gdouble no_reading = -1.0;
    ax_event_key_value_set_add_key_values(
        set,
        &error,
        "topic0",
        "tnsaxis",
        "CameraApplicationPlatform",
        AX_VALUE_TYPE_STRING,
        "topic1",
        "tnsaxis",
        "GaugeReader",
        AX_VALUE_TYPE_STRING,
        "topic2",
        "tnsaxis",
        "Value",
        AX_VALUE_TYPE_STRING,
        "Value",
        NULL,
        &no_reading,
        AX_VALUE_TYPE_DOUBLE,
        NULL);

    if (nullptr != error)
    {
        LOG_E("%s/%s: Could not add key values: %s", __FILE__, __FUNCTION__, error->message);
        throw runtime_error(error->message);
        g_error_free(error);
    }

    // Set nice names
    ax_event_key_value_set_add_nice_names(set, "topic0", "tnsaxis", NULL, "Application", NULL);
    ax_event_key_value_set_add_nice_names(set, "topic1", "tnsaxis", NULL, "OPC UA Gauge Reader", NULL);
    ax_event_key_value_set_add_nice_names(set, "topic2", "tnsaxis", NULL, "Gauge Reader", NULL);
    ax_event_key_value_set_add_nice_names(set, "Value", NULL, NULL, "Gauge value (percent)", NULL);

    // Mark data value
    ax_event_key_value_set_mark_as_data(set, "Value", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(set, "Value", NULL, "wstype:xs:float", NULL);

    // Declare event
    if (!ax_event_handler_declare(
            event_handler_,
            set,
            TRUE, // Data event: only sent on explicit trigger, no automatic initial state
            &event_id_,
            declaration_complete,
            &initialized_,
            &error))
    {
        LOG_E("%s/%s: Could not declare: %s", __FILE__, __FUNCTION__, error->message);
        throw runtime_error(error->message);
        g_error_free(error);
    }

    // The key/value set is no longer needed
    ax_event_key_value_set_free(set);
}

EventPusher::~EventPusher()
{
    assert(nullptr != event_handler_);
    assert(0 != event_id_);

    LOG_I("%s/%s: Undeclare event ...", __FILE__, __FUNCTION__);
    ax_event_handler_undeclare(event_handler_, event_id_, nullptr);

    LOG_I("%s/%s: Free eventhandler ...", __FILE__, __FUNCTION__);
    ax_event_handler_free(event_handler_);
}

gboolean EventPusher::Send(const gdouble value, const gboolean verbose_logs) const
{
    if (!initialized_)
    {
        LOG_E("%s/%s: Event handling not yet initialized", __FILE__, __FUNCTION__);
        return FALSE;
    }

    auto set = ax_event_key_value_set_new();

    // Add the variable elements of the event to the set
    ax_event_key_value_set_add_key_value(set, "Value", NULL, &value, AX_VALUE_TYPE_DOUBLE, NULL);

    // Create the event
    auto event = ax_event_new2(set, NULL);

    // The key/value set is no longer needed
    ax_event_key_value_set_free(set);

    // Send the event
    assert(nullptr != event_handler_);
    GError *error = nullptr;
    ax_event_handler_send_event(event_handler_, event_id_, event, &error);
    if (nullptr != error)
    {
        LOG_E("Failed to send event: %s", error->message);
        g_error_free(error);
    }
    else if (verbose_logs)
    {
        LOG_I("Data event sent with gauge value %f%%", value);
    }

    ax_event_free(event);

    return TRUE;
}
