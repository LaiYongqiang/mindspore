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

#include "backend/session/single_kernel_graph.h"
#include "backend/session/anf_runtime_algorithm.h"

namespace mindspore {
namespace session {
std::shared_ptr<session::KernelGraph> SingleKernelGraph::ConstructKernelGraphBasedOnSingleOp(
  const std::string &op_name, const std::vector<TypeId> &input_dtypes, const std::vector<ShapeVector> &input_shapes,
  const std::vector<TypeId> &output_dtypes, const std::vector<std::vector<size_t>> &output_shapes) {
  auto graph = std::make_shared<session::KernelGraph>();
  MS_EXCEPTION_IF_NULL(graph);
  std::vector<AnfNodePtr> inputs;
  // set input[0]
  PrimitivePtr op_prim = std::make_shared<Primitive>(op_name);
  MS_EXCEPTION_IF_NULL(op_prim);
  inputs.push_back(std::make_shared<ValueNode>(op_prim));
  // construct real input
  if (input_dtypes.size() != input_shapes.size()) {
    MS_LOG(EXCEPTION) << " input_dtypes size should equal to input_shapes size, the op name is: " << op_name;
  }
  auto input_num = input_dtypes.size();
  for (size_t i = 0; i < input_num; ++i) {
    auto tensor = std::make_shared<tensor::Tensor>(input_dtypes[i], input_shapes[i]);
    auto value_node = graph->NewValueNode(tensor);
    inputs.push_back(value_node);
  }
  // obtain cnode
  auto cnode = graph->NewCNode(inputs);
  MS_EXCEPTION_IF_NULL(cnode);
  // get output dynamic shape info
  AnfAlgo::SetNodeAttr(kAttrOutputIsDynamicShape, MakeValue(false), cnode);
  if (output_dtypes.size() != output_shapes.size()) {
    MS_LOG(EXCEPTION) << " output_dtypes size should equal to output_shapes size, the op name is: " << op_name;
  }
  AnfAlgo::SetOutputInferTypeAndShape(output_dtypes, output_shapes, cnode.get());
  // set execution order
  std::vector<CNodePtr> exe_order = {cnode};
  graph->set_execution_order(exe_order);
  // set graph output
  graph->set_output(cnode);
  graph->SetInputNodes();
  return graph;
}
}  // namespace session
}  // namespace mindspore
