// Copyright 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "server_api.h"
#include <stdlib.h>
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>
#define TRITONJSON_STATUSTYPE TRITONSERVER_Error*
#define TRITONJSON_STATUSRETURN(M) \
  return TRITONSERVER_ErrorNew(TRITONSERVER_ERROR_INTERNAL, (M).c_str())
#define TRITONJSON_STATUSSUCCESS nullptr
#include "triton/common/triton_json.h"

namespace triton { namespace triton_developer_tools { namespace server {

Allocator* custom_allocator_ = nullptr;

TritonServer::TritonServer(ServerParams server_params)
{
  TRITONSERVER_ServerOptions* server_options = nullptr;
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsNew(&server_options),
      "creating server options");

  // Set model_repository_path
  for (const auto& model_repository_path :
       server_params.model_repository_paths) {
    FAIL_IF_TRITON_ERR(
        TRITONSERVER_ServerOptionsSetModelRepositoryPath(
            server_options, model_repository_path.c_str()),
        "setting model repository path");
  }

  // Set logging options
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetLogVerbose(
          server_options, server_params.logging.verbose),
      "setting verbose level logging");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetLogInfo(
          server_options, server_params.logging.info),
      "setting info level logging");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetLogWarn(
          server_options, server_params.logging.warn),
      "setting warning level logging");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetLogError(
          server_options, server_params.logging.error),
      "setting error level logging");
  TRITONSERVER_LogFormat log_format;
  FAIL_IF_ERR(
      ToTritonLogFormat(&log_format, server_params.logging.format),
      "converting to triton log format")
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetLogFormat(server_options, log_format),
      "setting logging format");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetLogFile(
          server_options, server_params.logging.log_file.c_str()),
      "setting logging output file");

  // Set metrics options
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetMetrics(
          server_options, server_params.metrics.allow_metrics),
      "setting metrics collection");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetGpuMetrics(
          server_options, server_params.metrics.allow_gpu_metrics),
      "setting GPU metrics collection");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetMetricsInterval(
          server_options, server_params.metrics.metrics_interval_ms),
      "settin the interval for metrics collection");

  // Set backend configuration
  for (const auto& bc : server_params.be_config) {
    FAIL_IF_TRITON_ERR(
        TRITONSERVER_ServerOptionsSetBackendConfig(
            server_options, bc.backend_name.c_str(), bc.setting.c_str(),
            bc.value.c_str()),
        "setting backend configurtion");
  }

  // Set server id
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetServerId(
          server_options, server_params.server_id.c_str()),
      "setting server ID");

  // Set backend directory
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetBackendDirectory(
          server_options, server_params.backend_dir.c_str()),
      "setting backend directory");

  // Set repo agent directory
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetRepoAgentDirectory(
          server_options, server_params.repo_agent_dir.c_str()),
      "setting repo agent directory");

  // Set auto-complete model config
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetStrictModelConfig(
          server_options, server_params.disable_auto_complete_config),
      "setting strict model configuration");

  // Set model control mode
  TRITONSERVER_ModelControlMode model_control_mode;
  FAIL_IF_ERR(
      ToTritonModelControlMode(
          &model_control_mode, server_params.model_control_mode),
      "converting to triton model control mode")
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsSetModelControlMode(
          server_options, model_control_mode),
      "setting model control mode");

  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerNew(&server_, server_options),
      "creating server object");
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerOptionsDelete(server_options),
      "deleting server options");
}

TritonServer::~TritonServer()
{
  for (auto& response : completed_responses_) {
    LOG_IF_ERROR(
        TRITONSERVER_InferenceResponseDelete(response),
        "Failed to delete inference response.");
  }

  if (allocator_ != nullptr) {
    LOG_IF_ERROR(
        TRITONSERVER_ResponseAllocatorDelete(allocator_),
        "Failed to delete allocator.");
  }

  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerDelete(server_), "Failed to delete server object");
}

Error
TritonServer::LoadModel(const std::string model_name)
{
  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_ServerLoadModel(server_, model_name.c_str()));

  return Error::Success;
}

Error
TritonServer::UnloadModel(const std::string model_name)
{
  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_ServerUnloadModelAndDependents(server_, model_name.c_str()));

  return Error::Success;
}

Error
TritonServer::LoadedModels(std::set<std::string>* loaded_models)
{
  std::vector<RepositoryIndex> repository_index;
  RETURN_IF_ERR(ModelIndex(repository_index));

  std::set<std::string> models;
  for (size_t i = 0; i < repository_index.size(); i++) {
    models.insert(repository_index[i].name);
  }

  *loaded_models = models;
  return Error::Success;
}

Error
TritonServer::ModelIndex(std::vector<RepositoryIndex>& repository_index)
{
  TRITONSERVER_Message* message = nullptr;
  uint32_t flags = TRITONSERVER_INDEX_FLAG_READY;
  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_ServerModelIndex(server_, flags, &message));
  const char* buffer;
  size_t byte_size;
  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_MessageSerializeToJson(message, &buffer, &byte_size));

  common::TritonJson::Value repo_index;
  RETURN_ERR_IF_TRITON_ERR(repo_index.Parse(buffer, byte_size));
  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_MessageDelete(message));

  for (size_t i = 0; i < repo_index.ArraySize(); i++) {
    triton::common::TritonJson::Value index;
    RETURN_ERR_IF_TRITON_ERR(repo_index.IndexAsObject(i, &index));
    std::string name, version, state;
    RETURN_ERR_IF_TRITON_ERR(index.MemberAsString("name", &name));
    RETURN_ERR_IF_TRITON_ERR(index.MemberAsString("version", &version));
    RETURN_ERR_IF_TRITON_ERR(index.MemberAsString("state", &state));
    repository_index.push_back(RepositoryIndex(name, version, state));
  }

  return Error::Success;
}

Error
TritonServer::Metrics(std::string* metrics_str)
{
  TRITONSERVER_Metrics* metrics = nullptr;
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_ServerMetrics(server_, &metrics), "fetch metrics");
  const char* base;
  size_t byte_size;
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_MetricsFormatted(
          metrics, TRITONSERVER_METRIC_PROMETHEUS, &base, &byte_size),
      "format metrics string");
  *metrics_str = std::string(base, byte_size);
  FAIL_IF_TRITON_ERR(
      TRITONSERVER_MetricsDelete(metrics), "delete metrics object");

  return Error::Success;
}

void
TritonServer::ClearCompletedResponses()
{
  for (auto& response : completed_responses_) {
    LOG_IF_ERROR(
        TRITONSERVER_InferenceResponseDelete(response),
        "Failed to delete inference response.");
  }

  completed_responses_.clear();
}

Error
TritonServer::InitializeAllocator()
{
  allocator_ = nullptr;
  if (custom_allocator_ == nullptr) {
    RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_ResponseAllocatorNew(
        &allocator_, ResponseAlloc, ResponseRelease, nullptr /* start_fn */));
  } else {
    RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_ResponseAllocatorNew(
        &allocator_, InferRequest::CustomAllocationFn,
        InferRequest::CustomReleaseFn, InferRequest::CustomStartFn));
  }
  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_ResponseAllocatorSetQueryFunction(
      allocator_, OutputBufferQuery));

  return Error::Success;
}

Error
TritonServer::PrepareInferenceRequest(
    TRITONSERVER_InferenceRequest** irequest, InferRequest* request)
{
  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestNew(
      irequest, server_, request->ModelName().c_str(),
      request->ModelVersion()));

  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestSetId(
      *irequest, request->RequestId().c_str()));
  if (request->CorrelationIdStr().empty()) {
    RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestSetCorrelationId(
        *irequest, request->CorrelationId()));
  } else {
    RETURN_ERR_IF_TRITON_ERR(
        TRITONSERVER_InferenceRequestSetCorrelationIdString(
            *irequest, request->CorrelationIdStr().c_str()));
  }

  uint32_t flags = 0;
  if (request->SequenceStart()) {
    flags |= TRITONSERVER_REQUEST_FLAG_SEQUENCE_START;
  }
  if (request->SequenceEnd()) {
    flags |= TRITONSERVER_REQUEST_FLAG_SEQUENCE_END;
  }
  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_InferenceRequestSetFlags(*irequest, flags));

  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_InferenceRequestSetPriority(*irequest, request->Priority()));

  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestSetTimeoutMicroseconds(
      *irequest, request->RequestTimeout()));
  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestSetReleaseCallback(
      *irequest, InferRequestComplete, nullptr /* request_release_userp */));

  return Error::Success;
}

Error
TritonServer::ParseDataTypeAndShape(
    const std::string model_name, const int64_t model_version,
    const std::string input_name, TRITONSERVER_DataType* datatype,
    std::vector<int64_t>* shape)
{
  TRITONSERVER_Message* message;
  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_ServerModelConfig(
      server_, model_name.c_str(), model_version, 1 /* config_version */,
      &message));
  const char* buffer;
  size_t byte_size;
  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_MessageSerializeToJson(message, &buffer, &byte_size));
  triton::common::TritonJson::Value model_config;
  if (byte_size != 0) {
    RETURN_ERR_IF_TRITON_ERR(model_config.Parse(buffer, byte_size));
    triton::common::TritonJson::Value inputs(
        model_config, triton::common::TritonJson::ValueType::ARRAY);
    model_config.Find("input", &inputs);
    for (size_t i = 0; i < inputs.ArraySize(); i++) {
      triton::common::TritonJson::Value input;
      RETURN_ERR_IF_TRITON_ERR(inputs.IndexAsObject(i, &input));
      if (input.Find("name")) {
        std::string name;
        RETURN_ERR_IF_TRITON_ERR(input.MemberAsString("name", &name));
        if (input_name != name) {
          continue;
        }
        triton::common::TritonJson::Value data_type;
        input.Find("data_type", &data_type);
        std::string data_type_str;
        RETURN_ERR_IF_TRITON_ERR(data_type.AsString(&data_type_str));
        RETURN_IF_ERR(ToTritonDataType(datatype, data_type_str));

        int64_t mbs_value;
        model_config.MemberAsInt("max_batch_size", &mbs_value);
        if (mbs_value) {
          shape->push_back(1);
        }

        triton::common::TritonJson::Value dims;
        RETURN_ERR_IF_TRITON_ERR(input.MemberAsArray("dims", &dims));
        for (size_t j = 0; j < dims.ArraySize(); j++) {
          int64_t value;
          RETURN_ERR_IF_TRITON_ERR(dims.IndexAsInt(j, &value));
          shape->push_back(value);
        }
      }
    }
  }
  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_MessageDelete(message));

  return Error::Success;
}

Error
TritonServer::PrepareInferenceInput(
    TRITONSERVER_InferenceRequest* irequest, InferRequest* request)
{
  for (auto& infer_input : request->Inputs()) {
    TRITONSERVER_DataType input_dtype = infer_input->DataType();
    std::vector<int64_t> input_shape = infer_input->Shape();
    if ((input_dtype == TRITONSERVER_TYPE_INVALID) || input_shape.empty()) {
      TRITONSERVER_DataType dtype;
      std::vector<int64_t> shape;
      RETURN_IF_ERR(ParseDataTypeAndShape(
          request->ModelName(), request->ModelVersion(), infer_input->Name(),
          &dtype, &shape));
      if (input_dtype == TRITONSERVER_TYPE_INVALID) {
        input_dtype = dtype;
      }
      if (input_shape.empty()) {
        input_shape = shape;
      }
    }

    RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestAddInput(
        irequest, infer_input->Name().c_str(), input_dtype, input_shape.data(),
        input_shape.size()));

    RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestAppendInputData(
        irequest, infer_input->Name().c_str(), infer_input->DataPtr(),
        infer_input->ByteSize(), infer_input->MemoryType(),
        infer_input->MemoryTypeId()));
  }

  return Error::Success;
}

Error
TritonServer::PrepareInferenceOutput(
    TRITONSERVER_InferenceRequest* irequest, InferRequest* request)
{
  for (auto& infer_output : request->Outputs()) {
    const char* name = infer_output->Name().c_str();
    RETURN_ERR_IF_TRITON_ERR(
        TRITONSERVER_InferenceRequestAddRequestedOutput(irequest, name));
  }

  return Error::Success;
}

Error
TritonServer::AsyncExecute(
    TRITONSERVER_InferenceRequest* irequest,
    std::future<TRITONSERVER_InferenceResponse*>* future)
{
  // Perform inference by calling TRITONSERVER_ServerInferAsync. This
  // call is asychronous and therefore returns immediately. The
  // completion of the inference and delivery of the response is done
  // by triton by calling the "response complete" callback functions
  // (InferResponseComplete in this case).
  auto p = new std::promise<TRITONSERVER_InferenceResponse*>();
  *future = p->get_future();

  RETURN_ERR_IF_TRITON_ERR(TRITONSERVER_InferenceRequestSetResponseCallback(
      irequest, allocator_, nullptr /* response_allocator_userp */,
      InferResponseComplete, reinterpret_cast<void*>(p)));

  RETURN_ERR_IF_TRITON_ERR(
      TRITONSERVER_ServerInferAsync(server_, irequest, nullptr /* trace */));

  return Error::Success;
}

Error
TritonServer::AsyncInfer(InferResult* infer_result, InferRequest infer_request)
{
  // The inference request object for sending internal requests.
  TRITONSERVER_InferenceRequest* irequest = nullptr;
  std::future<TRITONSERVER_InferenceResponse*> future;
  std::future<std::vector<std::future<TRITONSERVER_InferenceResponse*>>> futures;
  try {
    bool is_ready = false;
    std::string model_name = infer_request.ModelName().c_str();
    THROW_IF_TRITON_ERROR(TRITONSERVER_ServerModelIsReady(
        server_, model_name.c_str(), infer_request.ModelVersion(), &is_ready));

    if (!is_ready) {
      return Error(
          (std::string("Failed for execute the inference request. Model '") +
           model_name + "' is not ready.")
              .c_str());
    }

    THROW_IF_ERROR(InitializeAllocator());
    THROW_IF_ERROR(PrepareInferenceRequest(&irequest, &infer_request));
    THROW_IF_ERROR(PrepareInferenceInput(irequest, &infer_request));
    THROW_IF_ERROR(PrepareInferenceOutput(irequest, &infer_request));

    THROW_IF_ERROR(AsyncExecute(irequest, &future));
  }
  catch (const Exception& ex) {
    LOG_IF_ERROR(
        TRITONSERVER_InferenceRequestDelete(irequest),
        "Failed to delete inference request.");
    return Error(ex.what());
  }

  RETURN_IF_ERR(FinalizeResponse(infer_result, std::move(future)));

  return Error::Success;
}

Error
TritonServer::AsyncInfer(
    std::unordered_map<std::string, Buffer>* buffer_map,
    InferRequest infer_request)
{
  InferResult results;
  RETURN_IF_ERR(AsyncInfer(&results, infer_request));

  buffer_map->clear();
  for (auto& output : results.Outputs()) {
    std::string name = output.first;
    const char* buf;
    size_t byte_size;
    RETURN_IF_ERR(results.RawData(name, &buf, &byte_size));
    (*buffer_map)
        .insert(std::pair<std::string, Buffer>(name, Buffer(buf, byte_size)));
  }

  return Error::Success;
}

Error
TritonServer::FinalizeResponse(
    InferResult* infer_result,
    std::future<TRITONSERVER_InferenceResponse*> future)
{
  TRITONSERVER_InferenceResponse* completed_response = nullptr;
  completed_response = future.get();
  try {
    THROW_IF_TRITON_ERROR(
        TRITONSERVER_InferenceResponseError(completed_response));

    const char* model_name;
    int64_t model_version;
    THROW_IF_TRITON_ERROR(TRITONSERVER_InferenceResponseModel(
        completed_response, &model_name, &model_version));
    const char* request_id = nullptr;
    THROW_IF_TRITON_ERROR(
        TRITONSERVER_InferenceResponseId(completed_response, &request_id));
    THROW_IF_ERROR(
        infer_result->SetResultInfo(model_name, model_version, request_id));

    uint32_t parameter_count;
    THROW_IF_TRITON_ERROR(TRITONSERVER_InferenceResponseParameterCount(
        completed_response, &parameter_count));
    for (uint32_t pidx = 0; pidx < parameter_count; ++pidx) {
      const char* name;
      TRITONSERVER_ParameterType type;
      const void* vvalue;
      THROW_IF_TRITON_ERROR(TRITONSERVER_InferenceResponseParameter(
          completed_response, pidx, &name, &type, &vvalue));
      infer_result->Params().push_back(
          std::move(new ResponseParameters(name, type, vvalue)));
    }

    uint32_t output_count;
    THROW_IF_TRITON_ERROR(TRITONSERVER_InferenceResponseOutputCount(
        completed_response, &output_count));

    std::unordered_map<std::string, std::vector<char>> output_data;
    for (uint32_t idx = 0; idx < output_count; ++idx) {
      const char* cname;
      TRITONSERVER_DataType datatype;
      const int64_t* shape;
      uint64_t dim_count;
      const void* base;
      size_t byte_size;
      TRITONSERVER_MemoryType memory_type;
      int64_t memory_type_id;
      void* userp;
      THROW_IF_TRITON_ERROR(TRITONSERVER_InferenceResponseOutput(
          completed_response, idx, &cname, &datatype, &shape, &dim_count, &base,
          &byte_size, &memory_type, &memory_type_id, &userp));
      InferOutput* output;
      RETURN_IF_ERR(InferOutput::Create(
          &output, cname, datatype, shape, dim_count, byte_size, memory_type,
          memory_type_id, base, userp));
      std::string name(cname);
      infer_result->AddInferOutput(name, output);
    }
  }
  catch (const Exception& ex) {
    if (completed_response != nullptr) {
      LOG_IF_ERROR(
          TRITONSERVER_InferenceResponseDelete(completed_response),
          "Failed to delete inference response.");
      completed_response = nullptr;
    }
    return Error(ex.what());
  }

  completed_responses_.push_back(completed_response);

  return Error::Success;
}

InferRequest::InferRequest(InferOptions options)
{
  model_name_ = options.model_name;
  model_version_ = options.model_version;
  request_id_ = options.request_id;
  correlation_id_ = options.correlation_id;
  correlation_id_str_ = options.correlation_id_str;
  sequence_start_ = options.sequence_start;
  sequence_end_ = options.sequence_end;
  priority_ = options.priority;
  request_timeout_ = options.request_timeout;
  custom_allocator_ = options.custom_allocator;
}

Error
InferRequest::AddInput(
    const std::string name, char* buffer_ptr, const uint64_t byte_size,
    std::string data_type, std::vector<int64_t> shape,
    const MemoryType input_memory_type, const int64_t intput_memory_type_id)
{
  InferInput* input;
  RETURN_IF_ERR(InferInput::Create(
      &input, name, shape, data_type, buffer_ptr, byte_size, input_memory_type,
      intput_memory_type_id));
  inputs_.push_back(input);

  return Error::Success;
}

template <typename Iterator>
Error
InferRequest::AddInput(
    const std::string name, Iterator& begin, Iterator& end,
    std::string data_type, std::vector<int64_t> shape,
    const MemoryType input_memory_type, const int64_t intput_memory_type_id)
{
  // Serialize the strings into a "raw" buffer. The first 4-bytes are
  // the length of the string length. Next are the actual string
  // characters. There is *not* a null-terminator on the string.
  str_bufs_.emplace_back();
  std::string& sbuf = str_bufs_.back();

  // std::string sbuf;
  Iterator it;
  for (it = begin; it != end; it++) {
    uint32_t len = (*it).size();
    sbuf.append(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    sbuf.append(*it);
  }

  return AddInput(
      name, reinterpret_cast<char*>(&sbuf[0]), sbuf.size(), data_type, shape,
      input_memory_type, intput_memory_type_id);
}

// Explicit template instantiation
template Error InferRequest::AddInput<std::vector<std::string>::iterator>(
    const std::string name, std::vector<std::string>::iterator& begin,
    std::vector<std::string>::iterator& end, std::string data_type,
    std::vector<int64_t> shape, const MemoryType input_memory_type,
    const int64_t intput_memory_type_id);

Error
InferRequest::AddRequestedOutputName(const std::string name)
{
  InferRequestedOutput* output;
  RETURN_IF_ERR(InferRequestedOutput::Create(&output, name));
  outputs_.push_back(output);

  return Error::Success;
}

Error
InferRequest::Reset()
{
  inputs_.clear();
  outputs_.clear();

  return Error::Success;
}

TRITONSERVER_Error*
InferRequest::CustomAllocationFn(
    TRITONSERVER_ResponseAllocator* allocator, const char* tensor_name,
    size_t byte_size, TRITONSERVER_MemoryType preferred_memory_type,
    int64_t preferred_memory_type_id, void* userp, void** buffer,
    void** buffer_userp, TRITONSERVER_MemoryType* actual_memory_type,
    int64_t* actual_memory_type_id)
{
  if (custom_allocator_->AllocFn() != nullptr) {
    MemoryType preferred_mem_type;
    MemoryType actual_mem_type;
    RETURN_TRITON_ERR_IF_ERR(
        ToMemoryType(&preferred_mem_type, preferred_memory_type));
    RETURN_TRITON_ERR_IF_ERR(
        ToMemoryType(&actual_mem_type, *actual_memory_type));

    RETURN_TRITON_ERR_IF_ERR(custom_allocator_->AllocFn()(
        tensor_name, byte_size, preferred_mem_type, preferred_memory_type_id,
        userp, buffer, buffer_userp, &actual_mem_type, actual_memory_type_id));

    RETURN_TRITON_ERR_IF_ERR(
        ToTritonMemoryType(actual_memory_type, actual_mem_type));
  }

  return nullptr;  // Success
}

TRITONSERVER_Error*
InferRequest::CustomReleaseFn(
    TRITONSERVER_ResponseAllocator* allocator, void* buffer, void* buffer_userp,
    size_t byte_size, TRITONSERVER_MemoryType memory_type,
    int64_t memory_type_id)
{
  if (custom_allocator_->ReleaseFn() != nullptr) {
    MemoryType mem_type;
    RETURN_TRITON_ERR_IF_ERR(ToMemoryType(&mem_type, memory_type));

    RETURN_TRITON_ERR_IF_ERR(custom_allocator_->ReleaseFn()(
        buffer, buffer_userp, byte_size, mem_type, memory_type_id));
  }

  return nullptr;  // Success
}

TRITONSERVER_Error*
InferRequest::CustomStartFn(
    TRITONSERVER_ResponseAllocator* allocator, void* userp)
{
  if (custom_allocator_->StartFn() != nullptr) {
    RETURN_TRITON_ERR_IF_ERR(custom_allocator_->StartFn()(userp));
  }

  return nullptr;  // Success
}

Error
InferResult::SetResultInfo(
    const char* model_name, int64_t model_version, const char* request_id)
{
  model_name_ = model_name;
  model_version_ = model_version;
  request_id_ = request_id;

  return Error::Success;
}

Error
InferResult::ModelName(std::string* name)
{
  *name = model_name_;
  return Error::Success;
}

Error
InferResult::ModelVersion(std::string* version)
{
  *version = std::to_string(model_version_);
  return Error::Success;
}

Error
InferResult::Id(std::string* id)
{
  *id = request_id_;
  return Error::Success;
}

Error
InferResult::Shape(const std::string& output_name, std::vector<int64_t>* shape)
{
  if (infer_outputs_.find(output_name) != infer_outputs_.end()) {
    shape->clear();
    const int64_t* output_shape = infer_outputs_[output_name]->Shape();
    for (uint64_t i = 0; i < infer_outputs_[output_name]->DimsCount(); i++) {
      shape->push_back(*(output_shape + i));
    }
  } else {
    return Error(
        "The response does not contain results for output name " + output_name);
  }

  return Error::Success;
}

Error
InferResult::DataType(const std::string& output_name, std::string* datatype)
{
  if (infer_outputs_.find(output_name) != infer_outputs_.end()) {
    *datatype =
        TRITONSERVER_DataTypeString(infer_outputs_[output_name]->DataType());
  } else {
    return Error(
        "The response does not contain results for output name " + output_name);
  }

  return Error::Success;
}

Error
InferResult::RawData(
    const std::string output_name, const char** buf, size_t* byte_size)
{
  if (infer_outputs_.find(output_name) != infer_outputs_.end()) {
    *buf =
        reinterpret_cast<const char*>(infer_outputs_[output_name]->DataPtr());
    *byte_size = infer_outputs_[output_name]->ByteSize();
  } else {
    return Error(
        "The response does not contain results for output name " + output_name);
  }

  return Error::Success;
}

Error
InferResult::StringData(
    const std::string& output_name, std::vector<std::string>* string_result)
{
  if (infer_outputs_.find(output_name) != infer_outputs_.end()) {
    const char* buf;
    size_t byte_size;
    RETURN_IF_ERR(RawData(output_name, &buf, &byte_size));

    string_result->clear();
    size_t buf_offset = 0;
    while (byte_size > buf_offset) {
      const uint32_t element_size =
          *(reinterpret_cast<const char*>(buf + buf_offset));
      string_result->emplace_back(
          (buf + buf_offset + sizeof(element_size)), element_size);
      buf_offset += (sizeof(element_size) + element_size);
    }
  } else {
    return Error(
        "The response does not contain results for output name " + output_name);
  }

  return Error::Success;
}

Error
InferResult::DebugString(std::string* string_result)
{
  triton::common::TritonJson::Value response_json(
      triton::common::TritonJson::ValueType::OBJECT);
  if ((request_id_ != nullptr) && (request_id_[0] != '\0')) {
    RETURN_ERR_IF_TRITON_ERR(response_json.AddStringRef("id", request_id_));
  }
  RETURN_ERR_IF_TRITON_ERR(
      response_json.AddStringRef("model_name", model_name_));
  RETURN_ERR_IF_TRITON_ERR(response_json.AddString(
      "model_version", std::move(std::to_string(model_version_))));

  if (!params_.empty()) {
    triton::common::TritonJson::Value params_json(
        response_json, triton::common::TritonJson::ValueType::OBJECT);
    for (size_t i = 0; i < params_.size(); i++) {
      switch (params_[i]->type) {
        case TRITONSERVER_PARAMETER_BOOL:
          RETURN_ERR_IF_TRITON_ERR(params_json.AddBool(
              params_[i]->name,
              *(reinterpret_cast<const bool*>(params_[i]->vvalue))));
          break;
        case TRITONSERVER_PARAMETER_INT:
          RETURN_ERR_IF_TRITON_ERR(params_json.AddInt(
              params_[i]->name,
              *(reinterpret_cast<const int64_t*>(params_[i]->vvalue))));
          break;
        case TRITONSERVER_PARAMETER_STRING:
          RETURN_ERR_IF_TRITON_ERR(params_json.AddStringRef(
              params_[i]->name,
              reinterpret_cast<const char*>(params_[i]->vvalue)));
          break;
        case TRITONSERVER_PARAMETER_BYTES:
          return Error(
              "Response parameter of type 'TRITONSERVER_PARAMETER_BYTES' is "
              "not currently supported");
          break;
      }
    }
    RETURN_ERR_IF_TRITON_ERR(
        response_json.Add("parameters", std::move(params_json)));
  }

  triton::common::TritonJson::Value response_outputs(
      response_json, triton::common::TritonJson::ValueType::ARRAY);
  for (auto& infer_output : infer_outputs_) {
    InferOutput* output = infer_output.second;
    triton::common::TritonJson::Value output_json(
        response_json, triton::common::TritonJson::ValueType::OBJECT);
    RETURN_ERR_IF_TRITON_ERR(output_json.AddStringRef("name", output->Name()));
    const char* datatype_str = TRITONSERVER_DataTypeString(output->DataType());
    RETURN_ERR_IF_TRITON_ERR(
        output_json.AddStringRef("datatype", datatype_str));
    triton::common::TritonJson::Value shape_json(
        response_json, triton::common::TritonJson::ValueType::ARRAY);
    for (size_t j = 0; j < output->DimsCount(); j++) {
      RETURN_ERR_IF_TRITON_ERR(shape_json.AppendUInt(output->Shape()[j]));
    }
    RETURN_ERR_IF_TRITON_ERR(output_json.Add("shape", std::move(shape_json)));
    RETURN_ERR_IF_TRITON_ERR(response_outputs.Append(std::move(output_json)));
  }
  RETURN_ERR_IF_TRITON_ERR(
      response_json.Add("outputs", std::move(response_outputs)));


  triton::common::TritonJson::WriteBuffer buffer;
  RETURN_ERR_IF_TRITON_ERR(response_json.Write(&buffer));
  *string_result = buffer.Contents();

  return Error::Success;
}

}}}  // namespace triton::triton_developer_tools::server