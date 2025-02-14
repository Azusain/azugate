// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: config_service.proto

#include "config_service.pb.h"
#include "config_service.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>
namespace api {
namespace v1 {

static const char* ConfigService_method_names[] = {
  "/api.v1.ConfigService/GetConfig",
  "/api.v1.ConfigService/UpdateConfig",
  "/api.v1.ConfigService/GetIpBlackList",
  "/api.v1.ConfigService/UpdateIpBlackList",
};

std::unique_ptr< ConfigService::Stub> ConfigService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< ConfigService::Stub> stub(new ConfigService::Stub(channel, options));
  return stub;
}

ConfigService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_GetConfig_(ConfigService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_UpdateConfig_(ConfigService_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_GetIpBlackList_(ConfigService_method_names[2], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_UpdateIpBlackList_(ConfigService_method_names[3], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status ConfigService::Stub::GetConfig(::grpc::ClientContext* context, const ::api::v1::GetConfigRequest& request, ::api::v1::GetConfigResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::api::v1::GetConfigRequest, ::api::v1::GetConfigResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetConfig_, context, request, response);
}

void ConfigService::Stub::async::GetConfig(::grpc::ClientContext* context, const ::api::v1::GetConfigRequest* request, ::api::v1::GetConfigResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::api::v1::GetConfigRequest, ::api::v1::GetConfigResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetConfig_, context, request, response, std::move(f));
}

void ConfigService::Stub::async::GetConfig(::grpc::ClientContext* context, const ::api::v1::GetConfigRequest* request, ::api::v1::GetConfigResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetConfig_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::api::v1::GetConfigResponse>* ConfigService::Stub::PrepareAsyncGetConfigRaw(::grpc::ClientContext* context, const ::api::v1::GetConfigRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::api::v1::GetConfigResponse, ::api::v1::GetConfigRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetConfig_, context, request);
}

::grpc::ClientAsyncResponseReader< ::api::v1::GetConfigResponse>* ConfigService::Stub::AsyncGetConfigRaw(::grpc::ClientContext* context, const ::api::v1::GetConfigRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetConfigRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status ConfigService::Stub::UpdateConfig(::grpc::ClientContext* context, const ::api::v1::UpdateConfigRequest& request, ::api::v1::UpdateConfigResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::api::v1::UpdateConfigRequest, ::api::v1::UpdateConfigResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_UpdateConfig_, context, request, response);
}

void ConfigService::Stub::async::UpdateConfig(::grpc::ClientContext* context, const ::api::v1::UpdateConfigRequest* request, ::api::v1::UpdateConfigResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::api::v1::UpdateConfigRequest, ::api::v1::UpdateConfigResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_UpdateConfig_, context, request, response, std::move(f));
}

void ConfigService::Stub::async::UpdateConfig(::grpc::ClientContext* context, const ::api::v1::UpdateConfigRequest* request, ::api::v1::UpdateConfigResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_UpdateConfig_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::api::v1::UpdateConfigResponse>* ConfigService::Stub::PrepareAsyncUpdateConfigRaw(::grpc::ClientContext* context, const ::api::v1::UpdateConfigRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::api::v1::UpdateConfigResponse, ::api::v1::UpdateConfigRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_UpdateConfig_, context, request);
}

::grpc::ClientAsyncResponseReader< ::api::v1::UpdateConfigResponse>* ConfigService::Stub::AsyncUpdateConfigRaw(::grpc::ClientContext* context, const ::api::v1::UpdateConfigRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncUpdateConfigRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status ConfigService::Stub::GetIpBlackList(::grpc::ClientContext* context, const ::api::v1::GetIpBlacklistRequest& request, ::api::v1::GetIpBlacklistResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::api::v1::GetIpBlacklistRequest, ::api::v1::GetIpBlacklistResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetIpBlackList_, context, request, response);
}

void ConfigService::Stub::async::GetIpBlackList(::grpc::ClientContext* context, const ::api::v1::GetIpBlacklistRequest* request, ::api::v1::GetIpBlacklistResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::api::v1::GetIpBlacklistRequest, ::api::v1::GetIpBlacklistResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetIpBlackList_, context, request, response, std::move(f));
}

void ConfigService::Stub::async::GetIpBlackList(::grpc::ClientContext* context, const ::api::v1::GetIpBlacklistRequest* request, ::api::v1::GetIpBlacklistResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetIpBlackList_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::api::v1::GetIpBlacklistResponse>* ConfigService::Stub::PrepareAsyncGetIpBlackListRaw(::grpc::ClientContext* context, const ::api::v1::GetIpBlacklistRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::api::v1::GetIpBlacklistResponse, ::api::v1::GetIpBlacklistRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetIpBlackList_, context, request);
}

::grpc::ClientAsyncResponseReader< ::api::v1::GetIpBlacklistResponse>* ConfigService::Stub::AsyncGetIpBlackListRaw(::grpc::ClientContext* context, const ::api::v1::GetIpBlacklistRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetIpBlackListRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status ConfigService::Stub::UpdateIpBlackList(::grpc::ClientContext* context, const ::api::v1::UpdateIpBlacklistRequest& request, ::api::v1::UpdateIpBlacklistResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::api::v1::UpdateIpBlacklistRequest, ::api::v1::UpdateIpBlacklistResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_UpdateIpBlackList_, context, request, response);
}

void ConfigService::Stub::async::UpdateIpBlackList(::grpc::ClientContext* context, const ::api::v1::UpdateIpBlacklistRequest* request, ::api::v1::UpdateIpBlacklistResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::api::v1::UpdateIpBlacklistRequest, ::api::v1::UpdateIpBlacklistResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_UpdateIpBlackList_, context, request, response, std::move(f));
}

void ConfigService::Stub::async::UpdateIpBlackList(::grpc::ClientContext* context, const ::api::v1::UpdateIpBlacklistRequest* request, ::api::v1::UpdateIpBlacklistResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_UpdateIpBlackList_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::api::v1::UpdateIpBlacklistResponse>* ConfigService::Stub::PrepareAsyncUpdateIpBlackListRaw(::grpc::ClientContext* context, const ::api::v1::UpdateIpBlacklistRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::api::v1::UpdateIpBlacklistResponse, ::api::v1::UpdateIpBlacklistRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_UpdateIpBlackList_, context, request);
}

::grpc::ClientAsyncResponseReader< ::api::v1::UpdateIpBlacklistResponse>* ConfigService::Stub::AsyncUpdateIpBlackListRaw(::grpc::ClientContext* context, const ::api::v1::UpdateIpBlacklistRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncUpdateIpBlackListRaw(context, request, cq);
  result->StartCall();
  return result;
}

ConfigService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ConfigService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ConfigService::Service, ::api::v1::GetConfigRequest, ::api::v1::GetConfigResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](ConfigService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::api::v1::GetConfigRequest* req,
             ::api::v1::GetConfigResponse* resp) {
               return service->GetConfig(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ConfigService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ConfigService::Service, ::api::v1::UpdateConfigRequest, ::api::v1::UpdateConfigResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](ConfigService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::api::v1::UpdateConfigRequest* req,
             ::api::v1::UpdateConfigResponse* resp) {
               return service->UpdateConfig(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ConfigService_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ConfigService::Service, ::api::v1::GetIpBlacklistRequest, ::api::v1::GetIpBlacklistResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](ConfigService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::api::v1::GetIpBlacklistRequest* req,
             ::api::v1::GetIpBlacklistResponse* resp) {
               return service->GetIpBlackList(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ConfigService_method_names[3],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ConfigService::Service, ::api::v1::UpdateIpBlacklistRequest, ::api::v1::UpdateIpBlacklistResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](ConfigService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::api::v1::UpdateIpBlacklistRequest* req,
             ::api::v1::UpdateIpBlacklistResponse* resp) {
               return service->UpdateIpBlackList(ctx, req, resp);
             }, this)));
}

ConfigService::Service::~Service() {
}

::grpc::Status ConfigService::Service::GetConfig(::grpc::ServerContext* context, const ::api::v1::GetConfigRequest* request, ::api::v1::GetConfigResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ConfigService::Service::UpdateConfig(::grpc::ServerContext* context, const ::api::v1::UpdateConfigRequest* request, ::api::v1::UpdateConfigResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ConfigService::Service::GetIpBlackList(::grpc::ServerContext* context, const ::api::v1::GetIpBlacklistRequest* request, ::api::v1::GetIpBlacklistResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ConfigService::Service::UpdateIpBlackList(::grpc::ServerContext* context, const ::api::v1::UpdateIpBlacklistRequest* request, ::api::v1::UpdateIpBlacklistResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace api
}  // namespace v1

