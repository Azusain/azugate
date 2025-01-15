#ifndef __CONFIG_SERVICE_H
#define __CONFIG_SERVICE_H

#include "config.h"
#include "config_service.grpc.pb.h"
#include "config_service.pb.h"
#include "string_validator.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <utility>

extern uint16_t azugate::port;

class ConfigServiceImpl final : public api::v1::ConfigService::Service {
public:
  ConfigServiceImpl() = default;
  virtual ~ConfigServiceImpl() override {};

  virtual grpc::Status GetConfig(grpc::ServerContext *ctx,
                                 const api::v1::GetConfigRequest *req,
                                 api::v1::GetConfigResponse *resp) override {
    resp->set_config_value(azugate::GetConfigPath());
    return grpc::Status::OK;
  }

  virtual ::grpc::Status
  GetIpBlackList(::grpc::ServerContext *context,
                 const ::api::v1::GetIpBlacklistRequest *request,
                 ::api::v1::GetIpBlacklistResponse *response) override {
    for (auto &ip : azugate::GetIpBlackList()) {
      response->add_ip_list(ip);
    }
    return grpc::Status::OK;
  };

  virtual grpc::Status
  UpdateConfig(grpc::ServerContext *context,
               const api::v1::UpdateConfigRequest *request,
               api::v1::UpdateConfigResponse *response) override {
    for (auto &mask : request->update_mask()) {
      SPDLOG_INFO(mask);
    }
    return grpc::Status::OK;
  }

  virtual grpc::Status
  UpdateIpBlackList(grpc::ServerContext *context,
                    const api::v1::UpdateIpBlacklistRequest *request,
                    api::v1::UpdateIpBlacklistResponse *response) override {

    switch (request->action()) {
    case api::v1::UpdateIpBlacklistRequest_ActionType_ADD:
      for (const std::string &ip : request->ip_list()) {
        if (azugate::utils::isValidIpv4(ip)) {
          azugate::AddBlacklistIp(std::move(ip));
        }
      }
      break;
    case api::v1::UpdateIpBlacklistRequest_ActionType_REMOVE:
      for (const std::string &ip : request->ip_list()) {
        azugate::RemoveBlacklistIp(std::move(ip));
      }
      break;
    case api::v1::UpdateIpBlacklistRequest_ActionType_ACTION_UNSPECIFIED:
    case api::v1::
        UpdateIpBlacklistRequest_ActionType_UpdateIpBlacklistRequest_ActionType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case api::v1::
        UpdateIpBlacklistRequest_ActionType_UpdateIpBlacklistRequest_ActionType_INT_MAX_SENTINEL_DO_NOT_USE_:
      return grpc::Status::CANCELLED;
      break;
    }
    return grpc::Status::OK;
  };
};

#endif