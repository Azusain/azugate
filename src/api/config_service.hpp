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

  virtual grpc::Status
  GetConfig(grpc::ServerContext *ctx, const api::v1::GetConfigRequest *request,
            api::v1::GetConfigResponse *response) override {
    response->set_http_compression(azugate::GetHttpCompression());
    response->set_https(azugate::GetHttps());
    response->set_enable_rate_limitor(azugate::GetEnableRateLimitor());
    auto [num_token_max, num_token_per_sec] = azugate::GetRateLimitorConfig();
    response->set_num_token_max(num_token_max);
    response->set_num_token_per_sec(num_token_per_sec);
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
      if (!path.compare("enable_rate_limitor")) {
        azugate::SetEnableRateLimitor(request->enable_rate_limitor());
      }
      if (!path.compare("num_token_per_sec")) {
        azugate::ConfigRateLimitor(0, request->num_token_per_sec());
      }
      if (!path.compare("num_token_max")) {
        azugate::ConfigRateLimitor(request->num_token_max(), 0);
      }
    }
    response->set_http_compression(azugate::GetHttpCompression());
    response->set_https(azugate::GetHttps());
    response->set_enable_rate_limitor(azugate::GetEnableRateLimitor());
    auto [num_token_max, num_token_per_sec] = azugate::GetRateLimitorConfig();
    response->set_num_token_max(num_token_max);
    response->set_num_token_per_sec(num_token_per_sec);
    // TODO: remove this.
    response->set_enable_external_auth(true);
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

  virtual grpc::Status
  ConfigRouter(::grpc::ServerContext *context,
               const ::api::v1::ConfigRouterRequest *request,
               ::api::v1::ConfigRouterResponse *response) override {
    // TODO: impl this.
    response->set_message("success");
    return grpc::Status::OK;
  }

  grpc::Status
  UpdateHealthzList(::grpc::ServerContext *context,
                    const ::api::v1::UpdateHealthzListRequest *request,
                    ::api::v1::UpdateHealthzListResponse *response) override {
    for (std::string addr : request->addrs()) {
      // TODO: using std::unordered_set for unique addr.
      azugate::AddHealthzList(std::move(addr));
    }
    response->set_message("success");
    return grpc::Status::OK;
  }
};

#endif