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

#include <axevent.h>

class EventPusher
{
  public:
    EventPusher();
    ~EventPusher();
    gboolean Send(const gdouble value, const gboolean verbose_logs = FALSE) const;

  private:
    AXEventHandler *event_handler_;
    gboolean initialized_;
    guint event_id_;
};
