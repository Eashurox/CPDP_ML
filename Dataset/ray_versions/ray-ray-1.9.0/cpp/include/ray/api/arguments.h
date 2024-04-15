// Copyright 2020-2021 The Ray Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <ray/api/object_ref.h>
#include <ray/api/serializer.h>
#include <ray/api/type_traits.h>

#include <msgpack.hpp>

namespace ray {
namespace internal {

class Arguments {
 public:
  template <typename OriginArgType, typename InputArgTypes>
  static void WrapArgsImpl(std::vector<TaskArg> *task_args, InputArgTypes &&arg) {
    if constexpr (is_object_ref_v<OriginArgType>) {
      PushReferenceArg(task_args, std::forward<InputArgTypes>(arg));
    } else if constexpr (is_object_ref_v<InputArgTypes>) {
      // core_worker submitting task callback will get the value of an ObjectRef arg, but
      // local mode we don't call core_worker submit task, so we need get the value of an
      // ObjectRef arg only for local mode.
      if (RayRuntimeHolder::Instance().Runtime()->IsLocalMode()) {
        auto buffer = RayRuntimeHolder::Instance().Runtime()->Get(arg.ID());
        PushValueArg(task_args, std::move(*buffer));
      } else {
        PushReferenceArg(task_args, std::forward<InputArgTypes>(arg));
      }
    } else {
      msgpack::sbuffer buffer = Serializer::Serialize(std::forward<InputArgTypes>(arg));
      PushValueArg(task_args, std::move(buffer));
    }
  }

  template <typename OriginArgsTuple, size_t... I, typename... InputArgTypes>
  static void WrapArgs(std::vector<TaskArg> *task_args, std::index_sequence<I...>,
                       InputArgTypes &&...args) {
    (void)std::initializer_list<int>{
        (WrapArgsImpl<std::tuple_element_t<I, OriginArgsTuple>>(
             task_args, std::forward<InputArgTypes>(args)),
         0)...};
    /// Silence gcc warning error.
    (void)task_args;
  }

 private:
  template <typename TaskArg>
  static void PushValueArg(std::vector<TaskArg> *task_args, msgpack::sbuffer &&buffer) {
    /// Pass by value.
    TaskArg task_arg;
    task_arg.buf = std::move(buffer);
    task_args->emplace_back(std::move(task_arg));
  }

  template <typename TaskArg, typename T>
  static void PushReferenceArg(std::vector<TaskArg> *task_args, T &&arg) {
    /// Pass by reference.
    TaskArg task_arg{};
    task_arg.id = arg.ID();
    task_args->emplace_back(std::move(task_arg));
  }
};

}  // namespace internal
}  // namespace ray