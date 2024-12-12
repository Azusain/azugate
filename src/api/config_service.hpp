#ifndef __CONFIG_SERVICE_H
#define __CONFIG_SERVICE_H

#include "config.h"
#include "config_service.grpc.pb.h"
#include "config_service.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <sys/types.h>

extern uint16_t azugate::port;

class ConfigServiceImpl final : public api::v1::ConfigService::Service {
public:
  ConfigServiceImpl() = default;
  virtual ~ConfigServiceImpl() override{};

  virtual grpc::Status GetConfig(grpc::ServerContext *ctx,
                                 const api::v1::GetConfigRequest *req,
                                 api::v1::GetConfigResponse *resp) override {
    resp->set_config_value(azugate::GetConfigPath());
    return grpc::Status::OK;
  }
};

#endif