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
#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_CORE_GRAPH_KERNEL_CALLBACK_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_CORE_GRAPH_KERNEL_CALLBACK_H_
#include <string>
#include <memory>
#include <functional>

#include "ir/anf.h"
#include "ir/dtype/type_id.h"
#include "utils/shape_utils.h"

namespace mindspore::graphkernel {
class Callback {
 public:
  static Callback *Instance() { return instance_.get(); }

  /**
   * @brief Get the real input shape of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the input index, start from 0
   */
  virtual ShapeVector GetInputShape(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the real output shape of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the output index, start from 0
   */
  virtual ShapeVector GetOutputShape(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the inferred input shape of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the input index, start from 0
   */
  virtual ShapeVector GetInputInferShape(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the inferred output shape of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the output index, start from 0
   */
  virtual ShapeVector GetOutputInferShape(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the real input data type of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the input index, start from 0
   */
  virtual TypeId GetInputType(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the real output data type of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the output index, start from 0
   */
  virtual TypeId GetOutputType(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the inferred input data type of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the input index, start from 0
   */
  virtual TypeId GetInputInferType(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the inferred output data type of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the output index, start from 0
   */
  virtual TypeId GetOutputInferType(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the input data format of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the input index, start from 0
   */
  virtual std::string GetInputFormat(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the output data format of the `node`.
   *
   * @param node the AnfNodePtr
   * @param i    the output index, start from 0
   */
  virtual std::string GetOutputFormat(const AnfNodePtr &node, size_t i) = 0;

  /**
   * @brief Get the processor of the `node`.
   *
   * @param node the AnfNodePtr
   */
  virtual std::string GetProcessor(const AnfNodePtr &node) = 0;

  /**
   * @brief Get the backend processor from context.
   */
  virtual std::string GetProcessorFromContext() = 0;

 private:
  friend class CallbackImplRegister;
  static void RegImpl(Callback *cb) { instance_.reset(cb); }

  static std::unique_ptr<Callback> instance_;
};

class CallbackImplRegister {
 public:
  explicit CallbackImplRegister(std::function<Callback *()> fn) { Callback::RegImpl(fn()); }
  ~CallbackImplRegister() = default;
};

#define GRAPH_KERNEL_CALLBACK_REGISTER(cls) \
  static const CallbackImplRegister g_graphkernel_callback([]() { return static_cast<Callback *>(new cls()); })
}  // namespace mindspore::graphkernel
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_CORE_GRAPH_KERNEL_CALLBACK_H_
