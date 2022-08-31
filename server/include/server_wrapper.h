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
#pragma once

#include <future>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "../src/common.h"
#include "triton/core/tritonserver.h"

namespace triton { namespace server { namespace wrapper {

class InferResult;
class InferRequest;
class Allocator;
using TensorAllocMap =
    std::unordered_map<std::string, std::pair<const void*, size_t>>;

//==============================================================================
/// Structure to hold logging options for server parameters.
///
struct LoggingOptions {
  LoggingOptions();

  LoggingOptions(
      const uint verbose, const bool info, const bool warn, const bool error,
      const LogFormat& format, const std::string& log_file);

  // Verbose logging level. Default is 0.
  uint verbose_;
  // Enable or disable info logging level. Default is true.
  bool info_;
  // Enable or disable warn logging level. Default is true.
  bool warn_;
  // Enable or disable error logging level. Default is true.
  bool error_;
  // The format of logging. For "LOG_DEFAULT", the log severity (L) and
  // timestamp will be logged as "LMMDD hh:mm:ss.ssssss". For "LOG_ISO8601", the
  // log format will be "YYYY-MM-DDThh:mm:ssZ L". Default is 'LOG_DEFAULT'.
  LogFormat format_;
  // Logging output file. If specified, log outputs will be saved to this file.
  // If not specified, log outputs will stream to the console. Default is an
  // empty string.
  std::string log_file_;  // logging output file
};

//==============================================================================
/// Structure to hold metrics options for server parameters.
/// See here for more information:
/// https://github.com/triton-inference-server/server/blob/main/docs/metrics.md.
struct MetricsOptions {
  MetricsOptions();

  MetricsOptions(
      const bool allow_metrics, const bool allow_gpu_metrics,
      const bool allow_cpu_metrics, const uint64_t metrics_interval_ms);

  // Enable or disable metrics. Default is true.
  bool allow_metrics_;
  // Enable or disable GPU metrics. Default is true.
  bool allow_gpu_metrics_;
  // Enable or disable CPU metrics. Default is true.
  bool allow_cpu_metrics_;
  // The interval for metrics collection. Default is 2000.
  uint64_t metrics_interval_ms_;
};

//==============================================================================
/// Structure to hold backend configuration for server parameters.
/// Different Triton-supported backends have different backend configuration
/// options. Please refer to the 'Command line options' section in the
/// documentation of each backend to see the options (e.g. Tensorflow Backend:
/// https://github.com/triton-inference-server/tensorflow_backend#command-line-options)
struct BackendConfig {
  BackendConfig();

  BackendConfig(
      const std::string& backend_name, const std::string& setting,
      const std::string& value);

  // The name of the backend. Default is an empty string.
  std::string backend_name_;
  // The name of the setting. Default is an empty string.
  std::string setting_;
  // The setting value. Default is an empty string.
  std::string value_;
};

//==============================================================================
/// Server options that are used to initialize Triton Server.
///
struct ServerOptions {
  ServerOptions(const std::vector<std::string>& model_repository_paths);

  ServerOptions(
      const std::vector<std::string>& model_repository_paths,
      const LoggingOptions& logging, const MetricsOptions& metrics,
      const std::vector<BackendConfig>& be_config, const std::string& server_id,
      const std::string& backend_dir, const std::string& repo_agent_dir,
      const bool disable_auto_complete_config,
      const ModelControlMode& model_control_mode);

  // Paths to model repository directory. Note that if a model is not unique
  // across all model repositories at any time, the model will not be available.
  // See here for more information:
  // https://github.com/triton-inference-server/server/blob/main/docs/model_repository.md.
  std::vector<std::string> model_repository_paths_;
  // Logging options. See the 'LoggingOptions' structure for more information.
  LoggingOptions logging_;
  // Metrics options. See the 'MetricsOptions' structure for more information.
  MetricsOptions metrics_;
  // Backend configuration. See the 'BackendConfig' structure for more
  // information.
  std::vector<BackendConfig> be_config_;
  // The ID of the server.
  std::string server_id_;
  // The global directory searched for backend shared libraries. Default is
  // "/opt/tritonserver/backends". See here for more information:
  // https://github.com/triton-inference-server/backend#backends.
  std::string backend_dir_;
  // The global directory searched for repository agent shared libraries.
  // Default is "/opt/tritonserver/repoagents". See here for more information:
  // https://github.com/triton-inference-server/server/blob/main/docs/repository_agents.md.
  std::string repo_agent_dir_;
  // If set, disables the triton and backends from auto completing model
  // configuration files. Model configuration files must be provided and
  // all required configuration settings must be specified. Default is false.
  // See here for more information:
  // https://github.com/triton-inference-server/server/blob/main/docs/model_configuration.md#auto-generated-model-configuration.
  bool disable_auto_complete_config_;
  // Specify the mode for model management. Options are "MODEL_CONTROL_NONE",
  // "MODEL_CONTROL_POLL" and "MODEL_CONTROL_EXPLICIT". Default is
  // "MODEL_CONTROL_NONE". See here for more information:
  // https://github.com/triton-inference-server/server/blob/main/docs/model_management.md.
  ModelControlMode model_control_mode_;
};

//==============================================================================
/// Structure to hold repository index for 'ModelIndex' function.
///
struct RepositoryIndex {
  RepositoryIndex(
      const std::string& name, const std::string& version,
      const ModelReadyState& state);

  // The name of the model.
  std::string name_;
  // The version of the model.
  std::string version_;
  // The state of the model. The states are
  // * UNKNOWN: The model is in an unknown state. The model is not available for
  // inferencing.
  // * READY: The model is ready and available for inferencing.
  // * UNAVAILABLE: The model is unavailable, indicating that the model failed
  // to load or has been implicitly or explicitly unloaded. The model is not
  // available for inferencing.
  // * LOADING: The model is being loaded by the inference server. The model is
  // not available for inferencing.
  // * UNLOADING: The model is being unloaded by the inference server. The model
  // is not available for inferencing.
  ModelReadyState state_;
};

//==============================================================================
/// Structure to hold information of a tensor. This object is used for adding
/// input/requested output to an inference request, and retrieving the output
/// result from inference result.
///
struct Tensor {
  Tensor(
      const std::string& name, char* buffer, const size_t& byte_size,
      DataType data_type, std::vector<int64_t> shape, MemoryType memory_type,
      int64_t memory_type_id);

  Tensor(const std::string& name);

  Tensor(const std::string& name, char* buffer, size_t byte_size);

  // The name of the tensor.
  std::string name_;
  // The pointer to the start of the buffer.
  char* buffer_;
  // The size of buffer in bytes.
  size_t byte_size_;
  // The data type of the tensor.
  DataType data_type_;
  // The shape of the tensor.
  std::vector<int64_t> shape_;
  // The memory type of the tensor. Valid memory types are "CPU", "CPU_PINNED"
  // and "GPU".
  MemoryType memory_type_;
  // The ID of the memory for the tensor. (e.g. '0' is the memory type id of
  // 'GPU-0')
  int64_t memory_type_id_;
};

//==============================================================================
/// Object that encapsulates in-process C API functionalities.
///
class TritonServer {
 public:
  ///  Create a TritonServer instance.
  static std::unique_ptr<TritonServer> Create(
      const ServerOptions& server_options);

  virtual ~TritonServer();

  /// Load the requested model or reload the model if it is already loaded.
  /// \param model_name The name of the model.
  /// \return Error object indicating success or failure.
  Error LoadModel(const std::string& model_name);

  /// Unload the requested model. Unloading a model that is not loaded
  /// on server has no affect and success code will be returned.
  /// \param model_name The name of the model.
  /// \return Error object indicating success or failure.
  Error UnloadModel(const std::string& model_name);

  /// Get the set of names of models that are loaded and ready for inference.
  /// \param[out] loaded_models Returns the set of names of models that are
  /// loaded and ready for inference
  /// \return Error object indicating success or failure.
  Error LoadedModels(std::set<std::string>* loaded_models);

  /// Get the index of model repository contents.
  /// \param[out] repository_index Returns a vector of RepositoryIndex object
  /// representing the repository index.
  /// \return Error object indicating success or failure.
  Error ModelIndex(std::vector<RepositoryIndex>* repository_index);

  /// Get the metrics of the server.
  /// \param[out] metrics_str Returns a string representing the metrics.
  /// \return Error object indicating success or failure.
  Error Metrics(std::string* metrics_str);

  /// Run asynchronous inference on server.
  /// \param[out] result_future Returns the result of inference as a future of
  /// InferResult object.
  /// \param infer_request The InferRequest object contains
  /// the inputs, outputs and infer options for an inference request.
  /// \return Error object indicating success or failure.
  virtual Error AsyncInfer(
      std::future<std::unique_ptr<InferResult>>* result_future,
      const InferRequest& infer_request) = 0;

 protected:
  Error PrepareInferenceRequest(
      TRITONSERVER_InferenceRequest** irequest, const InferRequest& request);

  Error PrepareInferenceInput(
      TRITONSERVER_InferenceRequest* irequest, const InferRequest& request);

  Error PrepareInferenceOutput(
      TRITONSERVER_InferenceRequest* irequest, InferRequest& request);

  Error AsyncInferHelper(
      TRITONSERVER_InferenceRequest** irequest,
      const InferRequest& infer_request);

  // Helper function for parsing data type and shape of an input tensor from
  // model configuration when 'data_type' or 'shape' field is missing.
  Error ParseDataTypeAndShape(
      const std::string& model_name, const int64_t model_version,
      const std::string& input_name, TRITONSERVER_DataType* datatype,
      std::vector<int64_t>* shape);

  // The server object.
  std::shared_ptr<TRITONSERVER_Server> server_;
};

//==============================================================================
/// Structure to hold options for Inference Request.
///
struct InferOptions {
  InferOptions(const std::string& model_name);

  InferOptions(
      const std::string& model_name, const int64_t& model_version,
      const std::string& request_id, const uint64_t& correlation_id,
      const std::string& correlation_id_str, const bool sequence_start,
      const bool sequence_end, const uint64_t& priority,
      const uint64_t& request_timeout, Allocator* custom_allocator);

  /// The name of the model to run inference.
  std::string model_name_;
  /// The version of the model to use while running inference. The default
  /// value is "-1" which means the server will select the
  /// version of the model based on its internal policy.
  int64_t model_version_;
  /// An identifier for the request. If specified will be returned
  /// in the response. Default value is an empty string which means no
  /// request_id will be used.
  std::string request_id_;
  /// The correlation ID of the inference request to be an unsigned integer.
  /// Should be used exclusively with 'correlation_id_str_'.
  /// Default is 0, which indicates that the request has no correlation ID.
  uint64_t correlation_id_;
  /// The correlation ID of the inference request to be a string.
  /// Should be used exclusively with 'correlation_id_'.
  /// Default value is "".
  std::string correlation_id_str_;
  /// Indicates whether the request being added marks the start of the
  /// sequence. Default value is False. This argument is ignored if
  /// 'sequence_id' is 0.
  bool sequence_start_;
  /// Indicates whether the request being added marks the end of the
  /// sequence. Default value is False. This argument is ignored if
  /// 'sequence_id' is 0.
  bool sequence_end_;
  /// Indicates the priority of the request. Priority value zero
  /// indicates that the default priority level should be used
  /// (i.e. same behavior as not specifying the priority parameter).
  /// Lower value priorities indicate higher priority levels. Thus
  /// the highest priority level is indicated by setting the parameter
  /// to 1, the next highest is 2, etc. If not provided, the server
  /// will handle the request using default setting for the model.
  uint64_t priority_;
  /// The timeout value for the request, in microseconds. If the request
  /// cannot be completed within the time by the server can take a
  /// model-specific action such as terminating the request. If not
  /// provided, the server will handle the request using default setting
  /// for the model.
  uint64_t request_timeout_;
  /// User-provided custom reponse allocator object. Default is nullptr.
  /// If using custom allocator, the lifetime of this 'Allocator' object should
  /// be long enough until `InferResult` object goes out of scope as we need
  /// this `Allocator` object to call 'ResponseAllocatorReleaseFn_t' for
  // releasing the response.
  Allocator* custom_allocator_;
};

//==============================================================================
/// Object that describes an inflight inference request.
///
class InferRequest {
 public:
  ///  Create an InferRequest instance.
  static std::unique_ptr<InferRequest> Create(
      const InferOptions& infer_options);

  virtual ~InferRequest();

  /// Add an input tensor to be sent within an InferRequest object.
  /// \param input A Tensor object that describes an input tensor.
  /// \return Error object indicating success or failure.
  Error AddInput(const Tensor& input);

  /// Add an input tensor to be sent within an InferRequest object. This
  /// function is for containers holding 'non-string' data elements. Data in the
  /// container should be contiguous, and the the container must
  // not be modified before the result is returned.
  /// \param model_name The name of the input tensor.
  /// \param begin The begin iterator of the container.
  /// \param end  The end iterator of the container.
  /// \param data_type The data type of the input. This field is optional.
  /// \param shape The shape of the input. This field is optional.
  /// \param memory_type The memory type of the input.
  /// This field is optional. Default is CPU.
  /// \param memory_type_id The ID of the memory for the tensor. (e.g. '0' is
  /// the memory type id of 'GPU-0') This field is optional. Default is 0.
  /// \return Error object indicating success or failure.
  template <typename Iterator>
  Error AddInput(
      const std::string& name, const Iterator begin, const Iterator end,
      DataType data_type = INVALID, std::vector<int64_t> shape = {},
      MemoryType memory_type = CPU, int64_t memory_type_id = 0);

  /// Add an input tensor to be sent within an InferRequest object. This
  /// function is for containers holding 'string' elements. Data in the
  /// container should be contiguous, and the the container must
  // not be modified before the result is returned.
  /// \param model_name The name of the input tensor.
  /// \param begin The begin iterator of the container.
  /// \param end  The end iterator of the container.
  /// \param shape The shape of the input. This field is optional.
  /// \param memory_type The memory type of the input.
  /// This field is optional. Default is CPU.
  /// \param memory_type_id The ID of the memory for the tensor. (e.g. '0' is
  /// the memory type id of 'GPU-0') This field is optional. Default is 0.
  /// \return Error object indicating success or failure.
  template <
      typename Iterator,
      std::enable_if_t<
          std::is_same<typename Iterator::value_type, std::string>::value,
          bool> = true>
  Error AddInput(
      const std::string& name, const Iterator begin, const Iterator end,
      std::vector<int64_t> shape = {}, MemoryType memory_type = CPU,
      int64_t memory_type_id = 0);

  /// Add a requested output tensor to be sent within an InferRequest object.
  /// Calling this function is optional. If no output(s) are specifically
  /// requested then all outputs defined by the model will be calculated and
  /// returned. Pre-allocated buffer for each output can be specified within the
  /// 'Tensor' object.
  /// \param output A Tensor object that describes an output tensor.
  /// \return Error object indicating success or failure.
  Error AddOutput(Tensor& output);

  /// Clear inputs and outputs of the request except for the callback functions.
  /// This allows users to reuse the InferRequest object if needed.
  /// \return Error object indicating success or failure.
  Error Reset();

  friend class TritonServer;
  friend class InternalServer;

 protected:
  std::unique_ptr<InferOptions> infer_options_;
  std::list<std::string> str_bufs_;
  std::vector<std::unique_ptr<InferInput>> inputs_ = {};
  std::vector<std::unique_ptr<InferRequestedOutput>> outputs_ = {};

  // The map for each output tensor and a pair of it's pre-allocated buffer and
  // byte size. [key:value -> output name : pair<pre-allocated buffer, byte
  // size>]
  TensorAllocMap tensor_alloc_map_;
};

//==============================================================================
/// An interface for InferResult object to interpret the response to an
/// inference request.
///
class InferResult {
 public:
  ~InferResult();

  /// Get the name of the model which generated this response.
  /// \return Returns the name of the model.
  std::string ModelName();

  /// Get the version of the model which generated this response.
  /// \return Returns the version of the model.
  std::string ModelVersion();

  /// Get the id of the request which generated this response.
  /// \return Returns the id of the request.
  std::string Id();

  /// Get the result output as a 'Tensor' object. The 'buffer' field of the
  /// output is owned by InferResult instance. Users can copy out the data if
  /// required to extend the lifetime. Note that for string data, need to use
  /// 'StringData' function for string data result.
  /// \param[out] output Contains the requested output tensor name. The output
  /// data
  // will be returned in the same 'Tensor' object.
  /// \return Error object indicating success or failure of the request.
  Error Output(std::shared_ptr<Tensor> output);

  /// Get the result data as a vector of strings. The vector will
  /// receive a copy of result data. An error will be generated if
  /// the datatype of output is not 'BYTES'.
  /// \param output_name The name of the output to get result data.
  /// \param[out] string_result Returns the result data represented as
  /// a vector of strings. The strings are stored in the
  /// row-major order.
  /// \return Error object indicating success or failure of the
  /// request.
  Error StringData(
      const std::string& output_name, std::vector<std::string>* string_result);

  /// Returns the complete response as a user friendly string.
  /// \return The string describing the complete response.
  Error DebugString(std::string* string_result);

  /// Returns if there is an error within this result. If so, should not call
  /// other member functions to retreive result as the output result may be
  /// incorrect.
  /// \return True if this 'InferResult' object has an error, false if no error.
  bool HasError();

  /// Returns the error message of the error.
  /// \return The messsage for the error. Empty if no error.
  std::string ErrorMsg();

 protected:
  /// Get the shape of output result returned in the response.
  /// \param output_name The name of the ouput to get shape.
  /// \param shape Returns the shape of result for specified output name.
  /// \return Error object indicating success or failure.
  void ShapeHelper(const std::string& output_name, std::vector<int64_t>* shape);

  /// Get access to the buffer holding raw results of specified output
  /// returned by the server.
  /// \param output_name The name of the output to get result data.
  /// \param buf Returns the pointer to the start of the buffer.
  /// \param byte_size Returns the size of buffer in bytes.
  /// \return Error object indicating success or failure of the
  /// request.
  void RawData(
      const std::string& output_name, const char** buf, size_t* byte_size);

  const char* model_name_;
  int64_t model_version_;
  const char* request_id_;
  std::vector<std::unique_ptr<ResponseParameters>> params_;
  std::unordered_map<std::string, std::unique_ptr<InferOutput>> infer_outputs_;
  Error response_error_;

  TRITONSERVER_InferenceResponse* completed_response_ = nullptr;
};

//==============================================================================
/// Custom Allocator object for providing custom functions for allocator.
/// If there are no custom functions set, will use the provided default
/// functions for allocator.
///
class Allocator {
  /***
  * ResponseAllocatorAllocFn_t: The custom response allocation that allocates a
  buffer to hold an output tensor. If not set, will use the provided default
  allocator.

  * ResponseAllocatorReleaseFn_t: The custom response release callback function
  that is called when the server no longer holds any reference to a buffer
  allocated by 'ResponseAllocatorAllocFn_t'. f not set, will use the provided
  default response release callback function.

  * ResponseAllocatorStartFn_t: The custom start callback function that is
  called to indicate that subsequent allocation requests will refer to a new
  response. If not set, will not provide any start callback function as it’s
  typically not used.

  The signature of each function:

    \param tensor_name The name of the output tensor to allocate for.
    \param byte_size The size of the buffer to allocate.
    \param memory_type The type of memory that the caller prefers for
    the buffer allocation.
    \param memory_type_id The ID of the memory that the caller prefers
    for the buffer allocation.
    \param userp The user data pointer that is passed to the
    'ResponseAllocatorAllocFn_t' callback function.
    \param buffer Returns a pointer to the allocated memory.
    \param buffer_userp Returns a user-specified value to associate
    with the buffer, or nullptr if no user-specified value should be
    associated with the buffer. This value will be passed to the
    'ResponseAllocatorReleaseFn_t' callback function when the buffer
    is released.
    \param actual_memory_type Returns the type of memory where the
    allocation resides. May be different than the type of memory
    requested by 'memory_type'.
    \param actual_memory_type_id Returns the ID of the memory where
    the allocation resides. May be different than the ID of the memory
    requested by 'memory_type_id'.
    \return Error object indicating if a failure occurs while
    attempting an allocation. If an error is returned all other return
    values will be ignored.
  * using ResponseAllocatorAllocFn_t = Error (*)(const char* tensor_name,
    size_t byte_size, MemoryType memory_type, int64_t memory_type_id, void*
    userp, void** buffer, void** buffer_userp, MemoryType* actual_memory_type,
    int64_t* actual_memory_type_id);

    \param buffer Pointer to the buffer to be freed.
    \param buffer_userp The user-specified value associated
    with the buffer in 'ResponseAllocatorAllocFn_t'.
    \param byte_size The size of the buffer.
    \param memory_type The type of memory holding the buffer.
    \param memory_type_id The ID of the memory holding the buffer.
    \return Error object indicating if a failure occurs while
    attempting the release. If an error is returned Triton will not
    attempt to release the buffer again.
  * using ResponseAllocatorReleaseFn_t = Error (*)(
    void* buffer, void* buffer_userp, size_t byte_size, MemoryType memory_type,
    int64_t memory_type_id);

    \param userp The user data pointer that is passed to the
    'ResponseAllocatorStartFn_t' callback function.
    \return Error object indicating  if a failure occurs
   * using ResponseAllocatorStartFn_t = Error (*)(void* userp);
  ***/
 public:
  explicit Allocator(
      ResponseAllocatorAllocFn_t alloc_fn,
      ResponseAllocatorReleaseFn_t release_fn,
      ResponseAllocatorStartFn_t start_fn = nullptr)
      : alloc_fn_(alloc_fn), release_fn_(release_fn), start_fn_(start_fn)
  {
  }

  ResponseAllocatorAllocFn_t AllocFn() { return alloc_fn_; }
  ResponseAllocatorReleaseFn_t ReleaseFn() { return release_fn_; }
  ResponseAllocatorStartFn_t StartFn() { return start_fn_; }

 private:
  ResponseAllocatorAllocFn_t alloc_fn_;
  ResponseAllocatorReleaseFn_t release_fn_;
  ResponseAllocatorStartFn_t start_fn_;
};

//==============================================================================
/// Helper functions to convert Wrapper enum to string.
///
std::string WrapperMemoryTypeString(const MemoryType& memory_type);
std::string WrapperDataTypeString(const DataType& data_type);
std::string WrapperModelReadyStateString(const ModelReadyState& state);

//==============================================================================
/// Implementation of template functions
///
template <
    typename Iterator,
    std::enable_if_t<
        std::is_same<typename Iterator::value_type, std::string>::value, bool> =
        true>
Error
InferRequest::AddInput(
    const std::string& name, const Iterator begin, const Iterator end,
    std::vector<int64_t> shape, MemoryType memory_type, int64_t memory_type_id)
{
  // Serialize the strings into a "raw" buffer. The first 4-bytes are
  // the length of the string length. Next are the actual string
  // characters. There is *not* a null-terminator on the string.
  str_bufs_.emplace_back();
  std::string& sbuf = str_bufs_.back();

  Iterator it;
  for (it = begin; it != end; it++) {
    auto len = it->size();
    sbuf.append(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    sbuf.append(*it);
  }
  Tensor input(
      name, reinterpret_cast<char*>(&sbuf[0]), sbuf.size(), BYTES, shape,
      memory_type, memory_type_id);

  return AddInput(input);
}

template <typename Iterator>
Error
InferRequest::AddInput(
    const std::string& name, const Iterator begin, const Iterator end,
    DataType data_type, std::vector<int64_t> shape, MemoryType memory_type,
    int64_t memory_type_id)
{
  size_t bytes = sizeof(*begin) * std::distance(begin, end);

  Tensor input(
      name, reinterpret_cast<char*>(&(*begin)), bytes, data_type, shape,
      memory_type, memory_type_id);
  return AddInput(input);
}

}}}  // namespace triton::server::wrapper
