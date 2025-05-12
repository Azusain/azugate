#ifndef __CONFIG_SERVICE_H
#define __CONFIG_SERVICE_H

#include "../../include/config.h"
#include "config_service.grpc.pb.h"
#include "config_service.pb.h"
#include "protocols.h"
#include "string_op.h"
#include <fmt/format.h>
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
    auto auth_config = response->mutable_external_auth_config();
    auth_config->set_enable(azugate::g_http_external_authorization);
    auth_config->set_callback_url(azugate::g_external_auth_callback_url);
    auth_config->set_domain(azugate::g_external_auth_domain);
    auth_config->set_client_id(azugate::g_external_auth_client_id);
    auth_config->set_client_secret(azugate::g_external_auth_client_secret);
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
    bool set_auth = false;

    if (request->has_http_compression()) {
      azugate::SetHttpCompression(request->http_compression());
    }
    if (request->has_https()) {
      azugate::SetHttps(request->https());
    }
    if (request->has_enable_rate_limitor()) {
      azugate::SetEnableRateLimitor(request->enable_rate_limitor());
    }
    if (request->has_enable_rate_limitor()) {
      azugate::ConfigRateLimitor(0, request->num_token_per_sec());
    }
    if (request->has_num_token_max()) {
      azugate::ConfigRateLimitor(request->num_token_max(), 0);
    }
    if (request->has_external_auth_config()) {
      // TODO: concurrency problem.
      auto &config = request->external_auth_config();
      azugate::g_http_external_authorization = config.enable();
      azugate::g_external_auth_domain = config.domain();
      azugate::g_external_auth_client_id = config.client_id();
      azugate::g_external_auth_client_secret = config.client_secret();
      azugate::g_external_auth_callback_url = config.callback_url();
    }
    response->set_http_compression(azugate::GetHttpCompression());
    response->set_https(azugate::GetHttps());
    response->set_enable_rate_limitor(azugate::GetEnableRateLimitor());
    auto [num_token_max, num_token_per_sec] = azugate::GetRateLimitorConfig();
    response->set_num_token_max(num_token_max);
    response->set_num_token_per_sec(num_token_per_sec);
    auto auth_config = response->mutable_external_auth_config();
    auth_config->set_enable(azugate::g_http_external_authorization);
    auth_config->set_callback_url(azugate::g_external_auth_callback_url);
    auth_config->set_domain(azugate::g_external_auth_domain);
    auth_config->set_client_id(azugate::g_external_auth_client_id);
    auth_config->set_client_secret(azugate::g_external_auth_client_secret);
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
    for (const auto &rule : request->rules()) {
      azugate::ProtocolType protocol;
      auto pb_protocol = rule.protocol();
      if (pb_protocol == api::v1::RouterRule_ProtocolType_PROTOCOL_TYPE_HTTP) {
        protocol = azugate::ProtocolTypeHttp;
      } else if (pb_protocol ==
                 api::v1::RouterRule_ProtocolType_PROTOCOL_TYPE_WEBSOCKET) {
        protocol = azugate::ProtocolTypeWebSocket;
      } else {
        response->set_message("invalid protocol type");
        return grpc::Status::CANCELLED;
      }
      SPDLOG_INFO("{}: [{}] {} -> {}", rule.remote() ? "remote" : "local",
                  protocol, rule.match_path(), rule.dest_path());
      azugate::AddRoute(
          azugate::ConnectionInfo{
              .type = protocol,
              .http_url = rule.match_path(),
          },
          azugate::ConnectionInfo{
              .type = protocol,
              .address = rule.dest_host(),
              // TODO: truncation.
              .port = uint16_t(rule.dest_port()),
              .http_url = rule.dest_path(),
              .remote = rule.remote(),
          });
    }
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