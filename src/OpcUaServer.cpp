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
#include <utility>

#include "OpcUaServer.hpp"
#include "common.hpp"

using namespace std;

#define LABEL (char *)"GaugeReading"

OpcUaServer::OpcUaServer()
    : serverthread_(nullptr), running_(false), server_(nullptr), gauge_value_(-1), gauge_value_pending_(false)
{
}

OpcUaServer::~OpcUaServer()
{
    ShutDownServer();
}

bool OpcUaServer::LaunchServer(const unsigned int serverport)
{
    lock_guard<mutex> lock(mtx_);

    assert(nullptr == server_);
    assert(nullptr == serverthread_);
    assert(!running_);
    assert(1024 <= serverport && 65535 >= serverport);

    // Create an OPC UA server
    LOG_I("⏳ Creating UA server serving on port %u ...", serverport);
    server_ = UA_Server_new();
    if (nullptr == server_)
    {
        LOG_E("%s/%s: Failed to create new UA_Server", __FILE__, __func__);
        return false;
    }
    const auto config_status = UA_ServerConfig_setMinimal(UA_Server_getConfig(server_), serverport, nullptr);
    if (UA_STATUSCODE_GOOD != config_status)
    {
        LOG_E(
            "%s/%s: Failed configuring UA server on port %u (%s)",
            __FILE__,
            __func__,
            serverport,
            UA_StatusCode_name(config_status));
        UA_Server_delete(exchange(server_, nullptr));
        return false;
    }
    AddDouble(LABEL, -1);

    running_ = true;
    serverthread_ = new thread(this->RunUaServer, this);

    LOG_I("✅ UA server configured for port %u", serverport);

    return true;
}

void OpcUaServer::ShutDownServer()
{
    thread *serverthread = nullptr;
    {
        lock_guard<mutex> lock(mtx_);
        serverthread = exchange(serverthread_, nullptr);
        if (nullptr == serverthread)
        {
            return;
        }
        running_ = false;
    }

    LOG_I("⏳ Shutting down UA server ...");
    if (serverthread->joinable())
    {
        serverthread->join();
    }
    delete serverthread;
    LOG_I("✅ UA server has been shut down");
}

bool OpcUaServer::IsRunning() const
{
    lock_guard<mutex> lock(mtx_);
    return running_;
}

void OpcUaServer::UpdateGaugeValue(double value)
{
    lock_guard<mutex> lock(mtx_);

    gauge_value_ = value;
    gauge_value_pending_ = true;
}

void OpcUaServer::WriteGaugeValue(double value)
{
    assert(nullptr != server_);

    UA_Variant newvalue;
    UA_Variant_setScalar(&newvalue, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_NodeId currentNodeId = UA_NODEID_STRING(1, LABEL);
    const auto rc = UA_Server_writeValue(server_, currentNodeId, newvalue);
    if (UA_STATUSCODE_GOOD != rc)
    {
        LOG_E("%s/%s: Failed to set OPC UA gauge value (%s)", __FILE__, __func__, UA_StatusCode_name(rc));
    }
}

void OpcUaServer::AddDouble(char *label, UA_Double value)
{
    assert(nullptr != server_);
    assert(nullptr != label);

    // Define attributes
    char *enUS = (char *)"en-US";
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT(enUS, label);
    attr.displayName = UA_LOCALIZEDTEXT(enUS, label);
    attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    // Add the variable node to the information model
    UA_NodeId node_id = UA_NODEID_STRING(1, label);
    UA_QualifiedName name = UA_QUALIFIEDNAME(1, label);
    UA_NodeId parent_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parent_ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    const auto rc = UA_Server_addVariableNode(
        server_,
        node_id,
        parent_node_id,
        parent_ref_node_id,
        name,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr,
        nullptr,
        nullptr);
    assert(UA_STATUSCODE_GOOD == rc);
}

void OpcUaServer::RunUaServer(OpcUaServer *parent)
{
    assert(nullptr != parent);

    LOG_I("⏳ Starting UA server ...");
    auto status = UA_Server_run_startup(parent->server_);
    while (UA_STATUSCODE_GOOD == status && parent->running_)
    {
        double gauge_value = 0;
        bool gauge_value_pending = false;
        {
            lock_guard<mutex> lock(parent->mtx_);
            if (!parent->running_ || nullptr == parent->server_)
            {
                break;
            }
            gauge_value = parent->gauge_value_;
            gauge_value_pending = exchange(parent->gauge_value_pending_, false);
        }
        if (gauge_value_pending)
        {
            parent->WriteGaugeValue(gauge_value);
        }
        UA_Server_run_iterate(parent->server_, true);
    }
    if (UA_STATUSCODE_GOOD == status)
    {
        status = UA_Server_run_shutdown(parent->server_);
    }
    LOG_I("UA Server exit status: %s", UA_StatusCode_name(status));

    lock_guard<mutex> lock(parent->mtx_);
    parent->running_ = false;
    UA_Server_delete(exchange(parent->server_, nullptr));
    return;
}
