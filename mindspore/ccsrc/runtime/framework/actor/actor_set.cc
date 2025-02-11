/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime/framework/actor/actor_set.h"

namespace mindspore {
namespace runtime {
std::unordered_map<std::string, OpActor<DeviceTensor> *> kActorNameToActor;

// The operation of the map of kActorNameToActor.
void InsertActor(OpActor<DeviceTensor> *actor) {
  MS_EXCEPTION_IF_NULL(actor);
  if (kActorNameToActor.count(actor->GetAID().Name()) > 0) {
    MS_LOG(EXCEPTION) << "The actor already exists: " << actor->GetAID().Name();
  }
  kActorNameToActor[actor->GetAID().Name()] = actor;
}

OpActor<DeviceTensor> *FetchActor(const std::string &actor_name) {
  const auto &iter = kActorNameToActor.find(actor_name);
  if (iter == kActorNameToActor.end()) {
    return nullptr;
  }
  return iter->second;
}

void EraseActor(const std::string &actor_name) { (void)kActorNameToActor.erase(actor_name); }

void ClearAllActors() { kActorNameToActor.clear(); }
}  // namespace runtime
}  // namespace mindspore
