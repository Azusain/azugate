#include "config_service.grpc.pb.h"
#include "config_service.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <string>
#include <sys/types.h>

extern uint16_t port;
extern std::string path_config_file;

class ConfigServiceImpl final : public api::v1::ConfigService::Service {
public:
  ConfigServiceImpl() = default;
  virtual ~ConfigServiceImpl() override{};

  virtual grpc::Status GetConfig(grpc::ServerContext *ctx,
                                 const api::v1::GetConfigRequest *req,
                                 api::v1::GetConfigResponse *resp) override {
    resp->set_config_value(std::to_string(port));
    return grpc::Status::OK;
  }
};