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

#ifndef MINDSPORE_CORE_UTILS_InterpretNodeRecorder_H_
#define MINDSPORE_CORE_UTILS_InterpretNodeRecorder_H_

#include <unordered_set>
#include <string>

namespace mindspore {
class InterpretNodeRecorder {
 public:
  explicit InterpretNodeRecorder(InterpretNodeRecorder &&) = delete;
  explicit InterpretNodeRecorder(const InterpretNodeRecorder &) = delete;
  void operator=(const InterpretNodeRecorder &) = delete;
  void operator=(const InterpretNodeRecorder &&) = delete;
  static InterpretNodeRecorder &GetInstance() {
    static InterpretNodeRecorder instance;
    return instance;
  }

  void PushLineInfo(const std::string &line) { interpret_nodes_lines_.emplace(line); }

  const std::unordered_set<std::string> &LineInfos() const { return interpret_nodes_lines_; }

  void Clear() { interpret_nodes_lines_.clear(); }

 protected:
  InterpretNodeRecorder() = default;
  virtual ~InterpretNodeRecorder() = default;

 private:
  std::unordered_set<std::string> interpret_nodes_lines_;
};
}  // namespace mindspore
#endif  // MINDSPORE_CORE_UTILS_InterpretNodeRecorder_H_
