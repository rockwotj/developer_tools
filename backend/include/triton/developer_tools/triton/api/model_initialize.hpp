/*
 * Copyright (c) 2021, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <triton/backend/backend_common.h>
#include <triton/backend/backend_model.h>
#include <triton/developer_tools/exceptions.hpp>
#include <triton/developer_tools/triton/logging.hpp>
#include <triton/developer_tools/triton/model.hpp>

namespace triton {
namespace developer_tools {
namespace backend {
namespace triton_api {
template <typename ModelState>
auto* model_initialize(TRITONBACKEND_Model* model)
{
  auto* result = static_cast<TRITONSERVER_Error*>(nullptr);
  try {
    auto name = get_model_name(*model);

    auto version = get_model_version(*model);

    log_info(__FILE__, __LINE__) << "TRITONBACKEND_ModelInitialize: " << name << " (version "
                                 << version << ")";

    auto tools_model_state = std::make_unique<ModelState>(*model);
    tools_model_state->load();

    set_model_state(*model, std::move(tools_model_state));
  } catch (TritonException& err) {
    result = err.error();
  }

  return result;
}
}  // namespace triton_api
}  // namespace backend
}  // namespace developer_tools
}  // namespace triton
