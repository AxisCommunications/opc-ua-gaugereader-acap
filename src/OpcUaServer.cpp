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

#include "OpcUaServer.hpp"
#include "common.hpp"

using namespace std;

#define LABEL (char *)"GaugeReading"

OpcUaServer::OpcUaServer() : serverthread_(nullptr), running_(false), server_(nullptr)
{
}

OpcUaServer::~OpcUaServer()
{
}

bool OpcUaServer::LaunchServer(const unsigned int serverport)
{
    lock_guard<mutex> lock(mtx_);

    LOG_I("%s/%s: port %u", __FILE__, __FUNCTION__, serverport);
    assert(nullptr == server_);
    assert(nullptr == serverthread_);
    assert(!running_);
    assert(1024 <= serverport && 65535 >= serverport);

    // Create an OPC UA server
    LOG_I("%s/%s: Create UA server serving on port %u", __FILE__, __FUNCTION__, serverport);
    server_ = UA_Server_new();
    if (nullptr == server_)
    {
        LOG_E("%s/%s: Failed to create new UA_Server", __FILE__, __FUNCTION__);
        return false;
    }
    UA_ServerConfig_setMinimal(UA_Server_getConfig(server_), serverport, nullptr);
    AddDouble(LABEL, -1);

    running_ = true;
    serverthread_ = new thread(this->RunUaServer, this);

    return true;
}

void OpcUaServer::ShutDownServer()
{
    LOG_I("%s/%s: Shutting down UA server ...", __FILE__, __FUNCTION__);
    thread *serverthread = nullptr;
    {
        lock_guard<mutex> lock(mtx_);
        running_ = false;
        serverthread = serverthread_;
    }

    if (nullptr != serverthread)
    {
        if (serverthread->joinable())
        {
            serverthread->join();
        }
        delete serverthread;
        lock_guard<mutex> lock(mtx_);
        serverthread_ = nullptr;
    }
    LOG_I("%s/%s: UA server has been shut down", __FILE__, __FUNCTION__);
}

bool OpcUaServer::IsRunning() const
{
    lock_guard<mutex> lock(mtx_);
    return running_;
}

void OpcUaServer::UpdateGaugeValue(double value)
{
    lock_guard<mutex> lock(mtx_);

    // Always update value even if there is no change; that will bump the
    // timestamp on the server so the client can see if the value is fresh or
    // ancient.
    if (nullptr == server_)
    {
        return;
    }
    UA_Variant newvalue;
    UA_Variant_setScalar(&newvalue, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_NodeId currentNodeId = UA_NODEID_STRING(1, LABEL);
    const auto rc = UA_Server_writeValue(server_, currentNodeId, newvalue);
    if (UA_STATUSCODE_GOOD != rc)
    {
        LOG_E("%s/%s: Failed to set OPC UA gauge value (%s)", __FILE__, __FUNCTION__, UA_StatusCode_name(rc));
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

    LOG_I("%s/%s: Starting UA server ...", __FILE__, __FUNCTION__);
    UA_StatusCode status = UA_STATUSCODE_GOOD;
    while (parent->running_)
    {
        lock_guard<mutex> lock(parent->mtx_);
        if (!parent->running_ || nullptr == parent->server_)
        {
            break;
        }
        status = UA_Server_run_iterate(parent->server_, true);
        if (UA_STATUSCODE_GOOD != status)
        {
            break;
        }
    }
    LOG_I("%s/%s: UA Server exit status: %s", __FILE__, __FUNCTION__, UA_StatusCode_name(status));

    lock_guard<mutex> lock(parent->mtx_);
    UA_Server_delete(parent->server_);
    parent->server_ = nullptr;
    return;
}
