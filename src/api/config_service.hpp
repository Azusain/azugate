#ifndef __CONFIG_SERVICE_H
#define __CONFIG_SERVICE_H

#include "../../include/config.h"
#include "config_service.grpc.pb.h"
#include "config_service.pb.h"
#include "string_validator.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <utility>

extern uint16_t azugate::g_azugate_port;

class ConfigServiceImpl final : public api::v1::ConfigService::Service {
public:
  ConfigServiceImpl() = default;
  virtual ~ConfigServiceImpl() override {};

  virtual grpc::Status GetConfig(grpc::ServerContext *ctx,
                                 const api::v1::GetConfigRequest *req,
                                 api::v1::GetConfigResponse *resp) override {
    resp->set_http_compression(azugate::GetHttpCompression());
    return grpc::Status::OK;
  }

  virtual ::grpc::Status
  GetIpBlackList(::grpc::ServerContext *context,
                 const ::api::v1::GetIpBlackListRequest *request,
                 ::api::v1::GetIpBlackListResponse *response) override {
    for (auto &ip : azugate::GetIpBlackList()) {
      response->add_ip_list(ip);
    }
    return grpc::Status::OK;
  };

  virtual grpc::Status
  UpdateConfig(grpc::ServerContext *context,
               const api::v1::UpdateConfigRequest *request,
               api::v1::UpdateConfigResponse *response) override {
    for (auto &path : request->path()) {
      if (!path.compare("http_compression")) {
        azugate::SetHttpCompression(request->http_compression());
      }
      if (!path.compare("https")) {
        azugate::SetHttps(request->https());
      }
    }
    response->set_http_compression(azugate::GetHttpCompression());
    response->set_https(azugate::GetHttps());
    return grpc::Status::OK;
  }

  virtual grpc::Status
  UpdateIpBlackList(grpc::ServerContext *context,
                    const api::v1::UpdateIpBlackListRequest *request,
                    api::v1::UpdateIpBlackListResponse *response) override {
    switch (request->action()) {
    case api::v1::UpdateIpBlackListRequest_ActionType::
        UpdateIpBlackListRequest_ActionType_ACTION_TYPE_ADD:
      for (const std::string &ip : request->ip_list()) {
        if (azugate::utils::isValidIpv4(ip)) {
          azugate::AddBlacklistIp(std::move(ip));
        }
      }
      break;
    case api::v1::UpdateIpBlackListRequest_ActionType::
        UpdateIpBlackListRequest_ActionType_ACTION_TYPE_REMOVE:
      for (const std::string &ip : request->ip_list()) {
        azugate::RemoveBlacklistIp(std::move(ip));
      }
      break;
    case api::v1::UpdateIpBlackListRequest_ActionType::
        UpdateIpBlackListRequest_ActionType_ACTION_TYPE_UNSPECIFIED:
    case api::v1::
        UpdateIpBlackListRequest_ActionType_UpdateIpBlackListRequest_ActionType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case api::v1::
        UpdateIpBlackListRequest_ActionType_UpdateIpBlackListRequest_ActionType_INT_MAX_SENTINEL_DO_NOT_USE_:
      return grpc::Status::CANCELLED;
      break;
    }
    return grpc::Status::OK;
  };
};

#endif