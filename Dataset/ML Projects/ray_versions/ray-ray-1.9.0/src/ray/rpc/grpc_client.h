// Copyright 2017 The Ray Authors.
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

#include <grpcpp/grpcpp.h>

#include <boost/asio.hpp>

#include "ray/common/grpc_util.h"
#include "ray/common/ray_config.h"
#include "ray/common/status.h"
#include "ray/rpc/client_call.h"
#include "ray/rpc/common.h"

namespace ray {
namespace rpc {

// This macro wraps the logic to call a specific RPC method of a service,
// to make it easier to implement a new RPC client.
#define INVOKE_RPC_CALL(SERVICE, METHOD, request, callback, rpc_client) \
  (rpc_client->CallMethod<METHOD##Request, METHOD##Reply>(              \
      &SERVICE::Stub::PrepareAsync##METHOD, request, callback,          \
      #SERVICE ".grpc_client." #METHOD))

// Define a void RPC client method.
#define VOID_RPC_CLIENT_METHOD(SERVICE, METHOD, rpc_client, SPECS)   \
  void METHOD(const METHOD##Request &request,                        \
              const ClientCallback<METHOD##Reply> &callback) SPECS { \
    INVOKE_RPC_CALL(SERVICE, METHOD, request, callback, rpc_client); \
  }

template <class GrpcService>
class GrpcClient {
 public:
  GrpcClient(const std::string &address, const int port, ClientCallManager &call_manager,
             bool use_tls = false)
      : client_call_manager_(call_manager), use_tls_(use_tls) {
    grpc::ChannelArguments argument;
    // Disable http proxy since it disrupts local connections. TODO(ekl) we should make
    // this configurable, or selectively set it for known local connections only.
    argument.SetInt(GRPC_ARG_ENABLE_HTTP_PROXY, 0);
    argument.SetMaxSendMessageSize(::RayConfig::instance().max_grpc_message_size());
    argument.SetMaxReceiveMessageSize(::RayConfig::instance().max_grpc_message_size());

    std::shared_ptr<grpc::Channel> channel = BuildChannel(argument, address, port);

    stub_ = GrpcService::NewStub(channel);
  }

  GrpcClient(const std::string &address, const int port, ClientCallManager &call_manager,
             int num_threads, bool use_tls = false)
      : client_call_manager_(call_manager), use_tls_(use_tls) {
    grpc::ResourceQuota quota;
    quota.SetMaxThreads(num_threads);
    grpc::ChannelArguments argument;
    argument.SetResourceQuota(quota);
    argument.SetInt(GRPC_ARG_ENABLE_HTTP_PROXY, 0);
    argument.SetMaxSendMessageSize(::RayConfig::instance().max_grpc_message_size());
    argument.SetMaxReceiveMessageSize(::RayConfig::instance().max_grpc_message_size());

    std::shared_ptr<grpc::Channel> channel = BuildChannel(argument, address, port);

    stub_ = GrpcService::NewStub(channel);
  }

  /// Create a new `ClientCall` and send request.
  ///
  /// \tparam Request Type of the request message.
  /// \tparam Reply Type of the reply message.
  ///
  /// \param[in] prepare_async_function Pointer to the gRPC-generated
  /// `FooService::Stub::PrepareAsyncBar` function.
  /// \param[in] request The request message.
  /// \param[in] callback The callback function that handles reply.
  ///
  /// \return Status.
  template <class Request, class Reply>
  void CallMethod(
      const PrepareAsyncFunction<GrpcService, Request, Reply> prepare_async_function,
      const Request &request, const ClientCallback<Reply> &callback,
      std::string call_name = "UNKNOWN_RPC") {
    auto call = client_call_manager_.CreateCall<GrpcService, Request, Reply>(
        *stub_, prepare_async_function, request, callback, std::move(call_name));
    RAY_CHECK(call != nullptr);
  }

 private:
  ClientCallManager &client_call_manager_;
  /// The gRPC-generated stub.
  std::unique_ptr<typename GrpcService::Stub> stub_;
  /// Whether to use TLS.
  bool use_tls_;

  std::shared_ptr<grpc::Channel> BuildChannel(const grpc::ChannelArguments &argument,
                                              const std::string &address, int port) {
    std::shared_ptr<grpc::Channel> channel;
    if (::RayConfig::instance().USE_TLS()) {
      std::string server_cert_file =
          std::string(::RayConfig::instance().TLS_SERVER_CERT());
      std::string server_key_file = std::string(::RayConfig::instance().TLS_SERVER_KEY());
      std::string root_cert_file = std::string(::RayConfig::instance().TLS_CA_CERT());
      std::string server_cert_chain = ReadCert(server_cert_file);
      std::string private_key = ReadCert(server_key_file);
      std::string cacert = ReadCert(root_cert_file);

      grpc::SslCredentialsOptions ssl_opts;
      ssl_opts.pem_root_certs = cacert;
      ssl_opts.pem_private_key = private_key;
      ssl_opts.pem_cert_chain = server_cert_chain;
      auto ssl_creds = grpc::SslCredentials(ssl_opts);
      channel = grpc::CreateCustomChannel(address + ":" + std::to_string(port), ssl_creds,
                                          argument);
    } else {
      channel = grpc::CreateCustomChannel(address + ":" + std::to_string(port),
                                          grpc::InsecureChannelCredentials(), argument);
    }
    return channel;
  };
};

}  // namespace rpc
}  // namespace ray
