#include "../api/config_service.hpp"
#include <fmt/format.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>

int main() {
  grpc::ServerBuilder server_builder;
  server_builder.AddListeningPort(fmt::format("0.0.0.0:{}", 50051),
                                  grpc::InsecureServerCredentials());
  ConfigServiceImpl config_service;
  server_builder.RegisterService(&config_service);
  std::unique_ptr<grpc::Server> server(server_builder.BuildAndStart());
  SPDLOG_INFO("grpc server runs on {}", 50051);
  server->Wait();
}