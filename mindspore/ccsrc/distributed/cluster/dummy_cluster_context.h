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

#ifndef MINDSPORE_CCSRC_DISTRIBUTED_CLUSTER_DUMMY_CLUSTER_CONTEXT_H_
#define MINDSPORE_CCSRC_DISTRIBUTED_CLUSTER_DUMMY_CLUSTER_CONTEXT_H_

#include <map>
#include <set>
#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include "distributed/constants.h"
#include "utils/log_adapter.h"
#include "utils/ms_utils.h"

namespace mindspore {
namespace distributed {
namespace cluster {
// The dummy cluster context interface. This class is the stub for some test cases and windows compiling.
class ClusterContext {
 public:
  ~ClusterContext() = default;
  DISABLE_COPY_AND_ASSIGN(ClusterContext)
  static std::shared_ptr<ClusterContext> instance();

  void Initialize() const;
  void Finalize() const;
  std::string node_role() const;

 private:
  ClusterContext() = default;
};
}  // namespace cluster
}  // namespace distributed
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_DISTRIBUTED_CLUSTER_DUMMY_CLUSTER_CONTEXT_H_
