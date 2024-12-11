#include "config_service.grpc.pb.h"
#include "config_service.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

namespace api {
namespace v1 {
class ConfigServiceImpl final : public ConfigService::Service {
public:
  grpc::Status GetConfig(grpc::ServerContext *ctx, const GetConfigRequest *req,
                         GetConfigResponse *resp) override {
    resp->set_config_value("just a test");
    return grpc::Status::OK;
  }
};
} // namespace v1
} // namespace api.