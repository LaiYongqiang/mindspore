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

#include "ops/grad/hshrink_grad.h"
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "abstract/primitive_infer_map.h"

namespace mindspore {
namespace ops {
abstract::ShapePtr HShrinkGradInferShape(const PrimitivePtr &primitive,
                                         const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  CheckAndConvertUtils::CheckInteger("input number", input_args.size(), kEqual, 2, primitive->name());
  auto prim_name = primitive->name();
  auto gradients_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->BuildShape())[kShape];
  auto features_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->BuildShape())[kShape];

  CheckAndConvertUtils::Check("gradients_shape", gradients_shape, kEqual, "features_shape", features_shape, prim_name,
                              TypeError);
  return std::make_shared<abstract::Shape>(gradients_shape);
}

TypePtr HShrinkGradInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  CheckAndConvertUtils::CheckInteger("input number", input_args.size(), kEqual, 2, prim->name());
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  std::map<std::string, TypePtr> types;
  const std::set<TypePtr> valid_types = {kFloat16, kFloat32};
  types.emplace("gradients", input_args[0]->BuildType());
  types.emplace("features", input_args[1]->BuildType());
  return CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, prim->name());
}

AbstractBasePtr HShrinkGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const std::vector<AbstractBasePtr> &input_args) {
  return std::make_shared<abstract::AbstractTensor>(HShrinkGradInferType(primitive, input_args),
                                                    HShrinkGradInferShape(primitive, input_args)->shape());
}
REGISTER_PRIMITIVE_EVAL_IMPL(HShrinkGrad, prim::kPrimHShrinkGrad, HShrinkGradInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore