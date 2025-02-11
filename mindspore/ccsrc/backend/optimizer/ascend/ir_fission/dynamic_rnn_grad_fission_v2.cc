/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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
#include "backend/optimizer/ascend/ir_fission/dynamic_rnn_grad_fission_v2.h"
#include <vector>
#include <memory>
#include "backend/session/kernel_graph.h"
#include "backend/session/anf_runtime_algorithm.h"
#include "utils/trace_base.h"
#include "utils/tensor_construct_utils.h"

namespace mindspore {
namespace opt {
namespace {
constexpr size_t kDynamicRNNGradInputNum = 16;
constexpr size_t kSplitVOutputNum = 2;
constexpr size_t kBasicCellOutputNum = 2;
constexpr size_t kBasicLstmCStateGradOutput0DimNum = 3;
constexpr int64_t kAttrNValue = 2;
constexpr int64_t kAttrDynInputSizesValue = 2;
constexpr int64_t kAttrAxis2Value = 2;
constexpr int64_t kAttrNumSplitValue = 2;
constexpr int64_t kAttrSplitDimValue = 2;
constexpr size_t kDimMultiNum = 4;
}  // namespace

void DynamicRnnGradFissionV2::CreateTLoopNode(const FuncGraphPtr &func_graph, const CNodePtr &dynamic_rnn_grad_cnode,
                                              std::vector<std::vector<AnfNodePtr>> *result_nodes) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(dynamic_rnn_grad_cnode);
  MS_EXCEPTION_IF_NULL(result_nodes);
  std::vector<AnfNodePtr> basic_lstm_cell_c_state_grad_nodes;
  std::vector<AnfNodePtr> matmul_nodes;
  std::vector<AnfNodePtr> split_nodes;
  // Get the size of t
  auto origin_input9_shape = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex11), 0);
  size_t t_size = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex9), 0)[0];
  auto input_i_shape = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex12), 0);

  for (size_t i = 0; i < t_size; ++i) {
    // Create basic_lstm_cell_c_state_grad
    std::vector<AnfNodePtr> basic_lstm_cell_c_state_grad_inputs = {
      NewValueNode(std::make_shared<Primitive>(kBasicLSTMCellCStateGradV2OpName))};
    auto basic_lstm_cell_c_state_grad = NewCNode(basic_lstm_cell_c_state_grad_inputs, func_graph);

    std::vector<size_t> output0_dims{
      origin_input9_shape[kDim0],
      kDimMultiNum * (((origin_input9_shape[kDim1] + kCubeSize - 1) / kCubeSize) * kCubeSize)};
    std::vector<size_t> output1_dims{input_i_shape[kDim1], input_i_shape[kDim2]};
    AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat16, kNumberTypeFloat32}, {output0_dims, output1_dims},
                                        basic_lstm_cell_c_state_grad.get());
    AnfAlgo::SetNodeAttr("forget_bias", MakeValue(1.0f), basic_lstm_cell_c_state_grad);
    AnfAlgo::SetNodeAttr("activation", MakeValue("Tanh"), basic_lstm_cell_c_state_grad);

    // Create matmul
    auto origin_input1_shape = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex2), 0);
    std::vector<AnfNodePtr> matmul_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimMatMul->name()))};
    auto matmul = NewCNode(matmul_inputs, func_graph);
    AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat32}, {{IntToSize(1), output0_dims[0], origin_input1_shape[0]}},
                                        matmul.get());
    AnfAlgo::SetNodeAttr("transpose_x1", MakeValue(false), matmul);
    AnfAlgo::SetNodeAttr("transpose_x2", MakeValue(true), matmul);

    // Create split
    std::vector<AnfNodePtr> splitv_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimSplitV->name()))};
    auto split_v = NewCNode(splitv_input, func_graph);
    auto origin_output2_shape = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode, kIndex2);
    auto origin_output3_shape = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode, kIndex3);
    std::vector<size_t> split_v_output0_shape{IntToSize(1), origin_output2_shape[kDim1], origin_output2_shape[kDim2]};
    std::vector<size_t> split_v_output1_shape{IntToSize(1), origin_output3_shape[kDim0], origin_output3_shape[kDim1]};
    AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat32, kNumberTypeFloat32},
                                        {split_v_output0_shape, split_v_output1_shape}, split_v.get());

    AnfAlgo::SetNodeAttr(kAttrSizeSplits,
                         MakeValue(std::vector<int64_t>{
                           SizeToLong((origin_output2_shape[kDim2] + kCubeSize - 1) / kCubeSize * kCubeSize),
                           SizeToLong((origin_output3_shape[kDim1] + kCubeSize - 1) / kCubeSize * kCubeSize)}),
                         split_v);
    AnfAlgo::SetNodeAttr(kAttrSplitDim, MakeValue(static_cast<int64_t>(kAttrSplitDimValue)), split_v);
    AnfAlgo::SetNodeAttr(kAttrNumSplit, MakeValue(static_cast<int64_t>(kAttrNumSplitValue)), split_v);

    basic_lstm_cell_c_state_grad_nodes.emplace_back(basic_lstm_cell_c_state_grad);
    matmul_nodes.emplace_back(matmul);
    split_nodes.emplace_back(split_v);
  }
  result_nodes->emplace_back(basic_lstm_cell_c_state_grad_nodes);
  result_nodes->emplace_back(matmul_nodes);
  result_nodes->emplace_back(split_nodes);
}

AnfNodePtr DynamicRnnGradFissionV2::CreateLSTMSPlitV(const FuncGraphPtr &func_graph, const AnfNodePtr &input,
                                                     const std::vector<std::vector<size_t>> &split_shapes,
                                                     const std::vector<TypeId> &split_types,
                                                     const std::vector<int64_t> &size_split, size_t num_split_x) const {
  std::vector<AnfNodePtr> lstm_split_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimSplitV->name())),
                                              input};
  auto lstm_split = NewCNode(lstm_split_input, func_graph);
  AnfAlgo::SetOutputInferTypeAndShape(split_types, split_shapes, lstm_split.get());
  AnfAlgo::SetNodeAttr(kAttrSizeSplits, MakeValue(size_split), lstm_split);
  AnfAlgo::SetNodeAttr(kAttrSplitDim, MakeValue(static_cast<int64_t>(0)), lstm_split);
  AnfAlgo::SetNodeAttr(kAttrNumSplit, MakeValue(SizeToLong(num_split_x)), lstm_split);
  return lstm_split;
}

void DynamicRnnGradFissionV2::CreateTLoopNodeWithEdge(const FuncGraphPtr &func_graph,
                                                      const CNodePtr &dynamic_rnn_grad_cnode,
                                                      const std::vector<std::vector<AnfNodePtr>> &result_nodes,
                                                      size_t num_split_x,
                                                      std::vector<std::vector<AnfNodePtr>> *loop_node_outputs) const {
  auto &basic_lstm_cell_c_state_grad_nodes = result_nodes[kIndex0];
  auto &matmul_nodes = result_nodes[kIndex1];
  auto &split_nodes = result_nodes[kIndex2];
  auto &lstm_split_c_outputs = result_nodes[kIndex3];
  auto &lstm_split_dy_outputs = result_nodes[kIndex4];
  auto &lstm_split_i_outputs = result_nodes[kIndex5];
  auto &lstm_split_j_outputs = result_nodes[kIndex6];
  auto &lstm_split_f_outputs = result_nodes[kIndex7];
  auto &lstm_split_o_outputs = result_nodes[kIndex8];
  auto &lstm_split_tanh_outputs = result_nodes[kIndex9];
  std::vector<AnfNodePtr> pre_basic_lstm_cell_c_state_grad_outputs;
  std::vector<AnfNodePtr> pre_split_outputs;
  std::vector<AnfNodePtr> lstm_x_concat_input(num_split_x + 1);
  lstm_x_concat_input[0] = NewValueNode(std::make_shared<Primitive>(prim::kPrimConcat->name()));
  std::vector<AnfNodePtr> lstm_gage_concat_input(num_split_x + 1);
  lstm_gage_concat_input[0] = NewValueNode(std::make_shared<Primitive>(prim::kPrimConcat->name()));
  for (size_t i = 0; i < num_split_x; ++i) {
    size_t idx = num_split_x - i - 1;
    // Create basic_lstm_cell_c_state_grad
    std::vector<AnfNodePtr> basic_lstm_cell_c_state_grad_inputs = {
      NewValueNode(std::make_shared<Primitive>(kBasicLSTMCellCStateGradV2OpName))};
    if (i == num_split_x - 1) {
      std::vector<AnfNodePtr> reshape_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReshape->name())),
                                                dynamic_rnn_grad_cnode->input(6)};
      auto reshape = NewCNode(reshape_inputs, func_graph);
      auto reshape_out_shape = {IntToSize(1),
                                AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex6), 0)[0],
                                AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex6), 0)[1]};
      AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat32}, {reshape_out_shape}, reshape.get());
      (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(reshape);
    } else {
      (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_c_outputs[idx - 1]);
    }
    (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_dy_outputs[idx]);
    if (i == 0) {
      (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(dynamic_rnn_grad_cnode->input(kIndex10));
      (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(dynamic_rnn_grad_cnode->input(kIndex11));
    } else {
      (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(pre_split_outputs[1]);
      (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(pre_basic_lstm_cell_c_state_grad_outputs[1]);
    }
    (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_i_outputs[idx]);
    (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_j_outputs[idx]);
    (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_f_outputs[idx]);
    (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_o_outputs[idx]);
    (void)basic_lstm_cell_c_state_grad_inputs.emplace_back(lstm_split_tanh_outputs[idx]);
    auto basic_lstm_cell_c_state_grad = NewCNode(basic_lstm_cell_c_state_grad_inputs, func_graph);
    MS_EXCEPTION_IF_NULL(basic_lstm_cell_c_state_grad);
    basic_lstm_cell_c_state_grad->set_abstract(basic_lstm_cell_c_state_grad_nodes[i]->abstract());
    AnfAlgo::CopyNodeAttrs(basic_lstm_cell_c_state_grad_nodes[i], basic_lstm_cell_c_state_grad);
    // Create outputs for current basic_lstm_cell_c_state_grad node
    std::vector<AnfNodePtr> basic_lstm_cell_c_state_grad_outputs;
    CreateMultipleOutputsOfAnfNode(func_graph, basic_lstm_cell_c_state_grad, kBasicCellOutputNum,
                                   &basic_lstm_cell_c_state_grad_outputs);
    pre_basic_lstm_cell_c_state_grad_outputs = basic_lstm_cell_c_state_grad_outputs;

    // Create MatMul
    std::vector<AnfNodePtr> matmul_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimMatMul->name()))};
    (void)matmul_inputs.emplace_back(basic_lstm_cell_c_state_grad_outputs[0]);
    (void)matmul_inputs.emplace_back(dynamic_rnn_grad_cnode->input(kIndex2));
    auto matmul = NewCNode(matmul_inputs, func_graph);
    MS_EXCEPTION_IF_NULL(matmul);
    matmul->set_abstract(matmul_nodes[i]->abstract());
    AnfAlgo::CopyNodeAttrs(matmul_nodes[i], matmul);

    // Create splitv
    std::vector<AnfNodePtr> splitv_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimSplitV->name())),
                                            matmul};
    auto split_v = NewCNode(splitv_input, func_graph);
    MS_EXCEPTION_IF_NULL(split_v);
    split_v->set_abstract(split_nodes[i]->abstract());
    AnfAlgo::CopyNodeAttrs(split_nodes[i], split_v);

    // Create outputs for current split node
    std::vector<AnfNodePtr> split_outputs;
    CreateMultipleOutputsOfAnfNode(func_graph, split_v, kSplitVOutputNum, &split_outputs);
    pre_split_outputs = split_outputs;

    lstm_x_concat_input[idx + 1] = split_outputs[0];

    auto basic_lstm_cell_c_state_grad_outputs_0_shape =
      AnfAlgo::GetOutputInferShape(basic_lstm_cell_c_state_grad_outputs[0], 0);
    std::vector<size_t> temp_shape;
    if (basic_lstm_cell_c_state_grad_outputs_0_shape.size() == kBasicLstmCStateGradOutput0DimNum) {
      temp_shape = basic_lstm_cell_c_state_grad_outputs_0_shape;
    } else {
      temp_shape = {1, basic_lstm_cell_c_state_grad_outputs_0_shape[0],
                    basic_lstm_cell_c_state_grad_outputs_0_shape[1]};
    }
    std::vector<AnfNodePtr> reshape_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReshape->name())),
                                             basic_lstm_cell_c_state_grad_outputs[0]};
    auto reshape = NewCNode(reshape_input, func_graph);
    AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(basic_lstm_cell_c_state_grad_outputs[0], 0)},
                                        {temp_shape}, reshape.get());
    lstm_gage_concat_input[idx + 1] = reshape;
  }
  loop_node_outputs->push_back(pre_basic_lstm_cell_c_state_grad_outputs);
  loop_node_outputs->push_back(pre_split_outputs);
  loop_node_outputs->push_back(lstm_x_concat_input);
  loop_node_outputs->push_back(lstm_gage_concat_input);
}

AnfNodePtr DynamicRnnGradFissionV2::AddLSTMInputGradNode(const FuncGraphPtr &func_graph,
                                                         const CNodePtr &dynamic_rnn_grad_cnode,
                                                         std::vector<AnfNodePtr> *outputs) const {
  std::vector<std::vector<AnfNodePtr>> result_nodes;
  CreateTLoopNode(func_graph, dynamic_rnn_grad_cnode, &result_nodes);

  auto origin_input5_shape = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex6), 0);
  std::vector<size_t> split_c_dims{IntToSize(1), origin_input5_shape[0], origin_input5_shape[1]};

  auto origin_input7 = dynamic_rnn_grad_cnode->input(kIndex8);
  size_t num_split_x = AnfAlgo::GetOutputInferShape(origin_input7, 0)[0];
  std::vector<std::vector<size_t>> split_shapes;
  std::vector<TypeId> split_types;
  std::vector<int64_t> size_split;
  for (size_t i = 0; i < num_split_x; ++i) {
    split_shapes.emplace_back(split_c_dims);
    split_types.emplace_back(kNumberTypeFloat32);
    size_split.emplace_back(1);
  }
  // Create lstm_split_c
  auto lstm_split_c = CreateLSTMSPlitV(func_graph, origin_input7, split_shapes, split_types, size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_c_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_c, num_split_x, &lstm_split_c_outputs);
  result_nodes.push_back(lstm_split_c_outputs);

  // Create lstm_split_dy
  auto lstm_split_dy = CreateLSTMSPlitV(func_graph, dynamic_rnn_grad_cnode->input(kIndex9), split_shapes, split_types,
                                        size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_dy_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_dy, num_split_x, &lstm_split_dy_outputs);
  result_nodes.push_back(lstm_split_dy_outputs);

  // Create lstm_split_i
  auto lstm_split_i = CreateLSTMSPlitV(func_graph, dynamic_rnn_grad_cnode->input(kIndex12), split_shapes, split_types,
                                       size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_i_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_i, num_split_x, &lstm_split_i_outputs);
  result_nodes.push_back(lstm_split_i_outputs);

  // Create lstm_split_j
  auto lstm_split_j = CreateLSTMSPlitV(func_graph, dynamic_rnn_grad_cnode->input(kIndex13), split_shapes, split_types,
                                       size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_j_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_j, num_split_x, &lstm_split_j_outputs);
  result_nodes.push_back(lstm_split_j_outputs);

  // Create lstm_split_f
  auto lstm_split_f = CreateLSTMSPlitV(func_graph, dynamic_rnn_grad_cnode->input(kIndex14), split_shapes, split_types,
                                       size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_f_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_f, num_split_x, &lstm_split_f_outputs);
  result_nodes.push_back(lstm_split_f_outputs);

  // Create lstm_split_o
  auto lstm_split_o = CreateLSTMSPlitV(func_graph, dynamic_rnn_grad_cnode->input(kIndex15), split_shapes, split_types,
                                       size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_o_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_o, num_split_x, &lstm_split_o_outputs);
  result_nodes.push_back(lstm_split_o_outputs);

  // Create lstm_split_tanh
  auto lstm_split_tanh = CreateLSTMSPlitV(func_graph, dynamic_rnn_grad_cnode->input(kIndex16), split_shapes,
                                          split_types, size_split, num_split_x);
  std::vector<AnfNodePtr> lstm_split_tanh_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, lstm_split_tanh, num_split_x, &lstm_split_tanh_outputs);
  result_nodes.push_back(lstm_split_tanh_outputs);

  // Add edges
  std::vector<std::vector<AnfNodePtr>> loop_node_outputs;
  CreateTLoopNodeWithEdge(func_graph, dynamic_rnn_grad_cnode, result_nodes, num_split_x, &loop_node_outputs);
  auto &pre_basic_lstm_cell_c_state_grad_outputs = loop_node_outputs[kIndex0];
  auto &pre_split_outputs = loop_node_outputs[kIndex1];
  auto &lstm_x_concat_input = loop_node_outputs[kIndex2];
  auto &lstm_gage_concat_input = loop_node_outputs[kIndex3];

  // Create lstm_x_concat
  auto lstm_x_concat = NewCNode(lstm_x_concat_input, func_graph);
  AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat32}, {AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode, 2)},
                                      lstm_x_concat.get());
  AnfAlgo::SetNodeAttr(kAttrN, MakeValue(SizeToLong(num_split_x)), lstm_x_concat);
  AnfAlgo::SetNodeAttr(kAttrDynInputSizes, MakeValue(std::vector<int64_t>{SizeToLong(num_split_x)}), lstm_x_concat);
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(static_cast<int64_t>(0)), lstm_x_concat);

  // Create lstm_gage_concat
  auto lstm_gage_concat = NewCNode(lstm_gage_concat_input, func_graph);
  auto origin_input7_shape = AnfAlgo::GetOutputInferShape(origin_input7, 0);
  AnfAlgo::SetOutputInferTypeAndShape(
    {kNumberTypeFloat16},
    {{origin_input7_shape[kDim0], origin_input7_shape[kDim1], kDimMultiNum * origin_input7_shape[kDim2]}},
    lstm_gage_concat.get());
  AnfAlgo::SetNodeAttr(kAttrN, MakeValue(SizeToLong(num_split_x)), lstm_gage_concat);
  AnfAlgo::SetNodeAttr(kAttrDynInputSizes, MakeValue(std::vector<int64_t>{SizeToLong(num_split_x)}), lstm_gage_concat);
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(SizeToLong(0)), lstm_gage_concat);

  outputs->emplace_back(lstm_x_concat);
  outputs->emplace_back(pre_split_outputs[1]);
  outputs->emplace_back(pre_basic_lstm_cell_c_state_grad_outputs[1]);
  return lstm_gage_concat;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateSplitV(const FuncGraphPtr &func_graph,
                                                 const CNodePtr &dynamic_rnn_grad_cnode) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(dynamic_rnn_grad_cnode);
  // Create node
  auto origin_input6 = dynamic_rnn_grad_cnode->input(kIndex7);
  std::vector<AnfNodePtr> splitv_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimSplitV->name())),
                                          origin_input6};
  auto split_v = NewCNode(splitv_input, func_graph);
  // Set infer data type and shape
  auto dtypes = {AnfAlgo::GetOutputInferDataType(origin_input6, 0), AnfAlgo::GetOutputInferDataType(origin_input6, 0)};
  auto origin_input6_shape = AnfAlgo::GetOutputInferShape(origin_input6, 0);
  std::vector<size_t> shape1 = {origin_input6_shape[kDim0] - 1, origin_input6_shape[kDim1], origin_input6_shape[kDim2]};
  std::vector<size_t> shape2 = {1, origin_input6_shape[kDim1], origin_input6_shape[kDim2]};
  std::vector<std::vector<size_t>> shapes = {shape1, shape2};
  AnfAlgo::SetOutputInferTypeAndShape(dtypes, shapes, split_v.get());
  // Set attr
  AnfAlgo::SetNodeAttr(kAttrSplitDim, MakeValue(SizeToLong(0)), split_v);
  AnfAlgo::SetNodeAttr(kAttrNumSplit, MakeValue(SizeToLong(kAttrNumSplitValue)), split_v);
  AnfAlgo::SetNodeAttr(kAttrSizeSplits, MakeValue(std::vector<int64_t>{SizeToLong(origin_input6_shape[0] - 1), 1}),
                       split_v);
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), split_v);
  return split_v;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateHConcat(const FuncGraphPtr &func_graph,
                                                  const CNodePtr &dynamic_rnn_grad_cnode,
                                                  const AnfNodePtr &splitv) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(dynamic_rnn_grad_cnode);
  MS_EXCEPTION_IF_NULL(splitv);
  // Create node
  std::vector<AnfNodePtr> splitv_outputs;
  CreateMultipleOutputsOfAnfNode(func_graph, splitv, kSplitVOutputNum, &splitv_outputs);
  if (splitv_outputs.size() != kSplitVOutputNum) {
    MS_LOG(EXCEPTION) << "Create outputs of node " << splitv->DebugString() << " failed"
                      << " trace: " << trace::DumpSourceLines(dynamic_rnn_grad_cnode);
  }
  auto origin_input4 = dynamic_rnn_grad_cnode->input(kIndex5);
  auto origin_input4_shape = AnfAlgo::GetOutputInferShape(origin_input4, 0);
  // Create reshape to change shape
  std::vector<size_t> shape_tmp;
  if (origin_input4_shape.size() == kShape4dDims) {
    shape_tmp = origin_input4_shape;
  } else {
    shape_tmp = {1, origin_input4_shape[0], origin_input4_shape[1]};
  }
  std::vector<AnfNodePtr> reshape_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReshape->name())),
                                           origin_input4};
  auto reshape = NewCNode(reshape_input, func_graph);
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(origin_input4, 0)}, {shape_tmp}, reshape.get());
  std::vector<AnfNodePtr> concat_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimConcat->name())),
                                           reshape, splitv_outputs[0]};
  auto concat = NewCNode(concat_inputs, func_graph);
  // Set infer data type and shape
  auto splitv_output0_shape = AnfAlgo::GetOutputInferShape(splitv, 0);
  std::vector<size_t> shape = {splitv_output0_shape[0] + 1, origin_input4_shape[0], origin_input4_shape[1]};
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(origin_input4, 0)}, {shape}, concat.get());
  // Set attr
  AnfAlgo::SetNodeAttr(kAttrN, MakeValue(SizeToLong(kAttrNValue)), concat);
  AnfAlgo::SetNodeAttr(kAttrDynInputSizes, MakeValue(std::vector<int64_t>{kAttrDynInputSizesValue}), concat);
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(SizeToLong(0)), concat);
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), concat);
  return concat;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateConcat(const FuncGraphPtr &func_graph, const CNodePtr &dynamic_rnn_grad_cnode,
                                                 const AnfNodePtr &h_concat) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(dynamic_rnn_grad_cnode);
  // Create node
  auto origin_input0 = dynamic_rnn_grad_cnode->input(1);
  std::vector<AnfNodePtr> concat_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimConcat->name())),
                                           origin_input0, h_concat};
  auto concat = NewCNode(concat_inputs, func_graph);
  // Set infer data type and shape
  auto origin_output0_shape = AnfAlgo::GetOutputInferShape(origin_input0, 0);
  auto h_concat_output_shape = AnfAlgo::GetOutputInferShape(h_concat, 0);
  std::vector<size_t> shape = {origin_output0_shape[kDim0], origin_output0_shape[kDim1],
                               origin_output0_shape[kDim2] + h_concat_output_shape[kDim2]};
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(origin_input0, 0)}, {shape}, concat.get());
  // Set attr
  AnfAlgo::SetNodeAttr(kAttrN, MakeValue(SizeToLong(kAttrNValue)), concat);
  AnfAlgo::SetNodeAttr(kAttrDynInputSizes, MakeValue(std::vector<int64_t>{kAttrDynInputSizesValue}), concat);
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(SizeToLong(kAttrAxis2Value)), concat);
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), concat);
  return concat;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateConcatNodeT1(const FuncGraphPtr &func_graph,
                                                       const CNodePtr &dynamic_rnn_grad_cnode) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(dynamic_rnn_grad_cnode);
  // Create node
  auto origin_input0 = dynamic_rnn_grad_cnode->input(kIndex1);
  auto origin_input4 = dynamic_rnn_grad_cnode->input(kIndex5);
  auto origin_input4_shape = AnfAlgo::GetOutputInferShape(origin_input4, 0);
  // Create reshape to change shape
  std::vector<size_t> shape_tmp;
  if (origin_input4_shape.size() == kShape3dDims) {
    shape_tmp = origin_input4_shape;
  } else {
    shape_tmp = {1, origin_input4_shape[0], origin_input4_shape[1]};
  }
  std::vector<AnfNodePtr> reshape_input = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReshape->name())),
                                           origin_input4};
  auto reshape = NewCNode(reshape_input, func_graph);
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(origin_input4, 0)}, {shape_tmp}, reshape.get());

  std::vector<AnfNodePtr> concat_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimConcat->name())),
                                           origin_input0, reshape};
  auto concat = NewCNode(concat_inputs, func_graph);
  // Set infer data type and shape
  auto origin_input0_shape = AnfAlgo::GetOutputInferShape(origin_input0, 0);
  std::vector<size_t> shape = {origin_input0_shape[kDim0], origin_input0_shape[kDim1],
                               origin_input0_shape[kDim2] + shape_tmp[kDim2]};
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(origin_input0, 0)}, {shape}, concat.get());
  // Set attr
  AnfAlgo::SetNodeAttr(kAttrN, MakeValue(SizeToLong(kAttrNValue)), concat);
  AnfAlgo::SetNodeAttr(kAttrDynInputSizes, MakeValue(std::vector<int64_t>{kAttrDynInputSizesValue}), concat);
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(SizeToLong(kAttrAxis2Value)), concat);
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), concat);
  return concat;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateBatchMatMul(const FuncGraphPtr &func_graph, const AnfNodePtr &lstm_input_grad,
                                                      const AnfNodePtr &concat) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  // Create node
  std::vector<AnfNodePtr> matmul_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimBatchMatMul->name())),
                                           concat, lstm_input_grad};
  auto batch_matmul = NewCNode(matmul_inputs, func_graph);
  // Set infer data type and shape
  auto concat_shape = AnfAlgo::GetOutputInferShape(concat, 0);
  auto lstm_input_grad_shape = AnfAlgo::GetOutputInferShape(lstm_input_grad, 0);
  std::vector<size_t> shape = {concat_shape[kDim0], concat_shape[kDim2], lstm_input_grad_shape[kDim2]};
  AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat32}, {shape}, batch_matmul.get());
  // Set attr
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), batch_matmul);
  AnfAlgo::SetNodeAttr("transpose_x1", MakeValue(true), batch_matmul);
  AnfAlgo::SetNodeAttr("transpose_x2", MakeValue(false), batch_matmul);
  return batch_matmul;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateBatchMatMul2(const FuncGraphPtr &func_graph,
                                                       const AnfNodePtr &lstm_input_grad,
                                                       const AnfNodePtr &node) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  // Create node
  std::vector<AnfNodePtr> matmul_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimBatchMatMul->name())),
                                           node, lstm_input_grad};
  auto batch_matmul = NewCNode(matmul_inputs, func_graph);
  // Set infer data type and shape
  auto out_shape = {AnfAlgo::GetOutputInferShape(lstm_input_grad, 0)[kIndex0], IntToSize(1),
                    AnfAlgo::GetOutputInferShape(lstm_input_grad, 0)[kIndex2]};
  AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat16}, {out_shape}, batch_matmul.get());
  // Set attr
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), batch_matmul);
  AnfAlgo::SetNodeAttr("transpose_x1", MakeValue(false), batch_matmul);
  AnfAlgo::SetNodeAttr("transpose_x2", MakeValue(false), batch_matmul);
  return batch_matmul;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateDwReduceSum(const FuncGraphPtr &func_graph,
                                                      const CNodePtr &dynamic_rnn_grad_cnode,
                                                      const AnfNodePtr &batch_matmul) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  // Create node
  std::vector<AnfNodePtr> reduce_sum_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReduceSum->name())),
                                               batch_matmul};
  auto reduce_sum = NewCNode(reduce_sum_inputs, func_graph);
  // Set infer data type and shape
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(dynamic_rnn_grad_cnode, 0)},
                                      {AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode, 0)}, reduce_sum.get());
  // Set attr
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(std::vector<int64_t>{0}), reduce_sum);
  AnfAlgo::SetNodeAttr(kAttrKeepDims, MakeValue(false), reduce_sum);
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), reduce_sum);
  return reduce_sum;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateDwReshape(const FuncGraphPtr &func_graph,
                                                    const CNodePtr &dynamic_rnn_grad_cnode,
                                                    const AnfNodePtr &batch_matmul) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  // Create node
  std::vector<AnfNodePtr> reshape_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReshape->name())),
                                            batch_matmul};
  auto reshape = NewCNode(reshape_inputs, func_graph);
  // Set infer data type and shape
  AnfAlgo::SetOutputInferTypeAndShape({AnfAlgo::GetOutputInferDataType(dynamic_rnn_grad_cnode, 0)},
                                      {AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode, 0)}, reshape.get());
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), reshape);
  return reshape;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateValueNode(const FuncGraphPtr &func_graph,
                                                    const CNodePtr &dynamic_rnn_grad_cnode) const {
  auto origin_input7 = dynamic_rnn_grad_cnode->input(kIndex8);
  auto origin_input7_shape = AnfAlgo::GetOutputInferShape(origin_input7, 0);
  auto t_size = origin_input7_shape[0];
  auto n_size = origin_input7_shape[1];

  std::vector<size_t> shape = {t_size, IntToSize(1), n_size};
  std::vector<int64_t> output_shape = {SizeToLong(t_size), SizeToLong(1), SizeToLong(n_size)};
  std::vector<int64_t> output_tensor = {SizeToLong(t_size) * SizeToLong(n_size)};
  auto tensor = TensorConstructUtils::CreateOnesTensor(kFloat32, output_tensor);
  auto x_abstract = std::make_shared<abstract::AbstractTensor>(kFloat32, output_shape);
  auto kernel_graph = func_graph->cast<KernelGraphPtr>();
  auto value_node = kernel_graph->NewValueNode(x_abstract, tensor);
  kernel_graph->AddValueNodeToGraph(value_node);
  AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat32}, {shape}, value_node.get());
  return value_node;
}

AnfNodePtr DynamicRnnGradFissionV2::CreateDbReduceSum(const FuncGraphPtr &func_graph, const CNodePtr &,
                                                      const AnfNodePtr &lstm_input_grad,
                                                      const AnfNodePtr &value_node) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  // Create node
  auto batch_matmul = CreateBatchMatMul2(func_graph, lstm_input_grad, value_node);
  std::vector<AnfNodePtr> reduce_sum_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReduceSum->name())),
                                               batch_matmul};
  auto reduce_sum = NewCNode(reduce_sum_inputs, func_graph);
  // Set infer data type and shape
  auto out_shape = {AnfAlgo::GetOutputInferShape(lstm_input_grad, 0)[kDim2]};
  AnfAlgo::SetOutputInferTypeAndShape({kNumberTypeFloat16}, {out_shape}, reduce_sum.get());
  // Set attr
  AnfAlgo::SetNodeAttr(kAttrAxis, MakeValue(std::vector<int64_t>{0}), reduce_sum);
  AnfAlgo::SetNodeAttr(kAttrKeepDims, MakeValue(false), reduce_sum);
  AnfAlgo::SetNodeAttr("is_backend_insert", MakeValue(true), reduce_sum);
  return reduce_sum;
}

const BaseRef DynamicRnnGradFissionV2::DefinePattern() const {
  VarPtr Xs = std::make_shared<SeqVar>();
  return VectorRef({prim::kPrimDynamicRNNGrad, Xs});
}

const AnfNodePtr DynamicRnnGradFissionV2::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                                  const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(node);
  auto dynamic_rnn_grad_cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(dynamic_rnn_grad_cnode);
  if (dynamic_rnn_grad_cnode->inputs().size() < kDynamicRNNGradInputNum + 1) {
    MS_LOG(INFO) << "The node " << dynamic_rnn_grad_cnode->DebugString() << " has less than "
                 << (kDynamicRNNGradInputNum + 1) << " inputs";
    return nullptr;
  }
  if (AnfAlgo::IsDynamicShape(node)) {
    MS_LOG(INFO) << "DynamicRnnGrad is dynamic shape, can not do fission.";
    return nullptr;
  }
  std::vector<AnfNodePtr> new_outputs;
  auto lstm_input_grad = AddLSTMInputGradNode(func_graph, dynamic_rnn_grad_cnode, &new_outputs);

  size_t t_size = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex7), 0)[0];
  size_t hidden_size = AnfAlgo::GetOutputInferShape(dynamic_rnn_grad_cnode->input(kIndex7), 0)[kDim2];
  if (hidden_size % kCubeSize != 0) {
    MS_LOG(EXCEPTION) << "`hidden_size` in this node should be multiple of 16, but got " << hidden_size << ". "
                      << dynamic_rnn_grad_cnode->DebugString();
  }
  AnfNodePtr concat = nullptr;
  if (t_size != 1) {
    auto splitv = CreateSplitV(func_graph, dynamic_rnn_grad_cnode);
    auto h_concat = CreateHConcat(func_graph, dynamic_rnn_grad_cnode, splitv);
    concat = CreateConcat(func_graph, dynamic_rnn_grad_cnode, h_concat);
  } else {
    concat = CreateConcatNodeT1(func_graph, dynamic_rnn_grad_cnode);
  }

  auto batch_matmul = CreateBatchMatMul(func_graph, lstm_input_grad, concat);
  std::vector<AnfNodePtr> make_tuple_inputs = {NewValueNode(prim::kPrimMakeTuple)};
  if (t_size != 1) {
    auto dw_reduce_sum = CreateDwReduceSum(func_graph, dynamic_rnn_grad_cnode, batch_matmul);
    make_tuple_inputs.emplace_back(dw_reduce_sum);
  } else {
    auto dw_reshape = CreateDwReshape(func_graph, dynamic_rnn_grad_cnode, batch_matmul);
    make_tuple_inputs.emplace_back(dw_reshape);
  }

  auto value_node = CreateValueNode(func_graph, dynamic_rnn_grad_cnode);
  // create reduce_sum_2
  auto db_reduce_sum = CreateDbReduceSum(func_graph, dynamic_rnn_grad_cnode, lstm_input_grad, value_node);
  make_tuple_inputs.emplace_back(db_reduce_sum);
  make_tuple_inputs.insert(make_tuple_inputs.end(), new_outputs.begin(), new_outputs.end());
  auto make_tuple = func_graph->NewCNode(make_tuple_inputs);
  return make_tuple;
}
}  // namespace opt
}  // namespace mindspore
