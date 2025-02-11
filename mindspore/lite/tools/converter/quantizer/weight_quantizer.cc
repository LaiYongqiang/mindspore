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

#include "tools/converter/quantizer/weight_quantizer.h"
#include <list>
#include <string>
#include <utility>
#include <set>
#include "tools/optimizer/common/gllo_utils.h"

namespace mindspore::lite::quant {
WeightQuantizer::WeightQuantizer(FuncGraphPtr graph, const converter::Flags &config) : Quantizer(std::move(graph)) {
  this->flags = config;
  this->bit_num_ = config.commonQuantParam.bit_num;
  if (this->bit_num_ == 0) {
    type_id_ = kNumberTypeInt16;
    this->is_mixed_bit_ = true;
    mixed_bit_init_scale_ = config.mixedBitWeightQuantParam.init_scale;
  }
  quant_strategy_ = std::make_unique<QuantStrategy>(config.commonQuantParam.min_quant_weight_size,
                                                    config.commonQuantParam.min_quant_weight_channel);
  // parse param for fixed bit quant.
  if (!this->is_mixed_bit_) {
    quant_max_ = (1 << (unsigned int)(this->bit_num_ - 1)) - 1;
    quant_min_ = -(1 << (unsigned int)(this->bit_num_ - 1));
    // parse type_id_
    if (this->bit_num_ > 0 && this->bit_num_ <= kMaxBit) {
      type_id_ = kNumberTypeInt8;
    } else if (this->bit_num_ <= (kMaxBit * 2)) {
      type_id_ = kNumberTypeInt16;
    } else {
      MS_LOG(ERROR) << "invalid input bits";
    }
  }
}

WeightQuantizer::~WeightQuantizer() {
  for (const auto &fp32_output_tensor : fp32_output_tensors_) {
    for (const auto &kv : fp32_output_tensor) {
      delete kv.second;
    }
  }
}

STATUS WeightQuantizer::DoWeightQuantize(const CNodePtr &cnode) {
  CHECK_NULL_RETURN(cnode);
  auto primitive = GetValueNode<PrimitivePtr>(cnode->input(0));
  CHECK_NULL_RETURN(primitive);
  WeightQuantType weight_quant_type = WeightQuantType::FIXED_BIT_PER_CHANNEL;
  auto manager = api::FuncGraphManager::Manage(funcGraph, true);
  CHECK_NULL_RETURN(manager);
  std::set<PrimitivePtr> per_layer_primitive_types = {prim::kPrimAdam, prim::kPrimSGD, prim::kPrimApplyMomentum};
  if (CheckNodeInSet(cnode, per_layer_primitive_types)) {
    weight_quant_type = WeightQuantType::FIXED_BIT_PER_LAYER;
  }
  std::vector<int> weight_indices;
  if (opt::CheckPrimitiveType(cnode, prim::kPrimAdam)) {
    weight_indices = {2, 3};
  } else if (opt::CheckPrimitiveType(cnode, prim::kPrimSGD)) {
    weight_indices = {4, 6};
  } else if (opt::CheckPrimitiveType(cnode, prim::kPrimApplyMomentum)) {
    weight_indices = {2};
  } else {
    for (size_t i = 1; i < cnode->size(); ++i) {
      weight_indices.push_back(i);
    }
  }
  for (auto idx : weight_indices) {
    auto input = cnode->input(idx);
    // support for shared weight
    auto node_map = manager->node_users();
    auto node_user = node_map[input];
    if (node_user.size() > 1 && opt::CheckPrimitiveType(cnode, prim::kPrimMatMul)) {
      MS_LOG(INFO) << input->fullname_with_scope() << " is shared weight.";
      weight_quant_type = WeightQuantType::FIXED_BIT_PER_LAYER;
    }
    ParameterPtr parameter;
    tensor::TensorPtr tensor_info;
    GetLiteParameter(input, &parameter, &tensor_info);
    if (parameter == nullptr || tensor_info == nullptr || tensor_info->data_type() != TypeId::kNumberTypeFloat32) {
      MS_LOG(INFO) << "This op " << cnode->fullname_with_scope() << " can not quant weight";
      continue;
    }
    int preferred_dim = GetPreferredDim(primitive, idx - 1, ConvertShapeVectorToInt32(tensor_info->shape()));
    if (!quant_strategy_->CanTensorQuantized(input, preferred_dim)) {
      MS_LOG(INFO) << "Input " << idx << "of Optimizer is not quantizable";
      continue;
    }

    auto status = RET_ERROR;
    if (is_mixed_bit_) {
      status = MixedBitQuantFilter(parameter, tensor_info, primitive, QuantType_QUANT_WEIGHT,
                                   WeightQuantType::MIXED_BIT_PER_LAYER, type_id_, mixed_bit_init_scale_, idx - 1);
    } else if (type_id_ == kNumberTypeInt8) {
      status = FixedBitQuantFilter<int8_t>(parameter, tensor_info, primitive, QuantType_QUANT_WEIGHT, quant_max_,
                                           quant_min_, bit_num_, weight_quant_type, type_id_, idx - 1);
    } else if (type_id_ == kNumberTypeInt16) {
      status = FixedBitQuantFilter<int16_t>(parameter, tensor_info, primitive, QuantType_QUANT_WEIGHT, quant_max_,
                                            quant_min_, bit_num_, weight_quant_type, type_id_, idx - 1);
    }
    if (status == RET_NO_CHANGE) {
      continue;
    } else if (status != RET_OK) {
      MS_LOG(ERROR) << "QuantFilter failed : " << status;
      return status;
    }
    weight_quantized_tensors_.insert(tensor_info);
  }
  return RET_OK;
}

STATUS WeightQuantizer::DoMarkWeightQuantizeIfQuantized(const CNodePtr &cnode) {
  MS_CHECK_TRUE_RET(cnode != nullptr, RET_NULL_PTR);
  auto primitive = GetValueNode<PrimitivePtr>(cnode->input(0));
  if (primitive == nullptr) {
    MS_LOG(ERROR) << "primitive is nullptr";
    return RET_ERROR;
  }

  auto quant_param_holder = GetCNodeQuantHolder(primitive);
  if (quant_param_holder->quant_type() == schema::QuantType_QUANT_WEIGHT) {
    // already marked with QUANT_WEIGHT
    return RET_OK;
  }

  for (size_t i = 1; i < cnode->size(); i++) {
    auto inputNode = cnode->input(i);
    if (inputNode->isa<Parameter>()) {
      ParameterPtr param_node;
      tensor::TensorPtr tensor_info;
      GetLiteParameter(inputNode, &param_node, &tensor_info);
      auto param = weight_quantized_tensors_.find(tensor_info);
      if (param != weight_quantized_tensors_.end()) {
        quant_param_holder->set_quant_type(schema::QuantType_QUANT_WEIGHT);
        continue;
      }
    }
  }
  return RET_OK;
}

STATUS WeightQuantizer::MarkWeightQuantizationInNodes(const FuncGraphPtr &func_graph) {
  MS_CHECK_TRUE_RET(func_graph != nullptr, RET_NULL_PTR);
  for (auto &cnode : func_graph->GetOrderedCnodes()) {
    auto primitive = GetValueNode<std::shared_ptr<ops::PrimitiveC>>(cnode->input(0));
    if (primitive == nullptr) {
      MS_LOG(DEBUG) << cnode->fullname_with_scope() << " : primitive is nullptr";
      continue;
    }
    auto status = DoMarkWeightQuantizeIfQuantized(cnode);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "MarkWeightQuantizationInNodes error marking " << cnode->fullname_with_scope();
      return RET_ERROR;
    }
  }
  return RET_OK;
}

STATUS WeightQuantizer::DoQuantize(FuncGraphPtr func_graph, double init_scale) {
  mixed_bit_init_scale_ = init_scale;
  return DoQuantize(std::move(func_graph));
}

STATUS WeightQuantizer::DoQuantize(FuncGraphPtr func_graph) {
  MS_CHECK_TRUE_RET(func_graph != nullptr, RET_NULL_PTR);
  weight_quantized_tensors_.clear();

  for (auto &cnode : func_graph->GetOrderedCnodes()) {
    auto primitive = GetValueNode<std::shared_ptr<ops::PrimitiveC>>(cnode->input(0));
    if (primitive == nullptr) {
      MS_LOG(DEBUG) << cnode->fullname_with_scope() << " : primitive is nullptr";
      continue;
    }
    auto op_name = cnode->fullname_with_scope();
    std::set<PrimitivePtr> support_primitive_types = {prim::kPrimConv2DFusion, prim::kPrimConv2dTransposeFusion,
                                                      prim::kPrimMatMul,       prim::kPrimFullConnection,
                                                      prim::kPrimLstm,         prim::kPrimGather,
                                                      prim::kPrimAdam,         prim::kPrimSGD,
                                                      prim::kPrimApplyMomentum};
    if (CheckNodeInSet(cnode, support_primitive_types)) {
      auto status = DoWeightQuantize(cnode);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "DoWeightQuantize error";
        return RET_ERROR;
      }
    } else {
      MS_LOG(DEBUG) << op_name << " of type: " << primitive->name() << " no need quant";
    }
  }
  return MarkWeightQuantizationInNodes(func_graph);
}
}  // namespace mindspore::lite::quant
