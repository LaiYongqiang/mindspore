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
#include "backend/optimizer/ascend/buffer_fusion/batchmatmul_fusedmuladd_fusion_pass.h"
#include "backend/kernel_compiler/kernel_fusion.h"
#include "backend/session/anf_runtime_algorithm.h"
#include "base/core_ops.h"
#include "utils/ms_context.h"
#include "backend/optimizer/common/fusion_id_allocator.h"

namespace mindspore {
namespace opt {
void BatchMatmulFusedMulAddFusionPass::MatchBatchMatmulFusedMulAdd(const CNodePtr &cnode,
                                                                   const session::KernelGraph & /* kernel_graph */,
                                                                   FusedNodeRecord *candidate_fusion) {
  MS_EXCEPTION_IF_NULL(cnode);
  MS_EXCEPTION_IF_NULL(candidate_fusion);
  auto batch_matmul = cnode->input(kIndex2);
  MS_EXCEPTION_IF_NULL(batch_matmul);
  if (batch_matmul->isa<CNode>() && AnfAlgo::CheckPrimitiveType(batch_matmul, prim::kPrimBatchMatMul)) {
    std::unordered_set<AnfNodePtr> record{cnode, batch_matmul};
    candidate_fusion->push_back(record);
    SetRecordFusionId(record);
  }
}

void BatchMatmulFusedMulAddFusionPass::MatchSingleFusionPattern(const session::KernelGraph &kernel_graph,
                                                                FusedNodeRecord *candidate_fusion) {
  MS_EXCEPTION_IF_NULL(candidate_fusion);
  std::vector<AnfNodePtr> node_list = TopoSort(kernel_graph.get_return());
  for (auto &node : node_list) {
    MS_EXCEPTION_IF_NULL(node);
    if (!AnfUtils::IsRealCNodeKernel(node) || fusion_id_allocator->HasFusionIdAttr(node) ||
        AnfAlgo::CheckPrimitiveType(node, prim::kPrimReturn)) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);

    if (AnfAlgo::GetCNodeName(cnode) == kFusedMulAddOpName) {
      MatchBatchMatmulFusedMulAdd(cnode, kernel_graph, candidate_fusion);
    }
  }
}
}  // namespace opt
}  // namespace mindspore
