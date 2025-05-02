#include "config.h"
#include "server.hpp"
#include "worker.hpp"

// TODO:
// ref: https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// memmory pool optimaization.
int main() {
  using namespace azugate;
  IgnoreSignalPipe();

  InitLogger();

  // Load the configurations from the local file.
  auto path_config_file = fmt::format("{}/{}", azugate::kPathResourceFolder,
                                      azugate::kDftConfigFile);
  SPDLOG_INFO("loading configuration from {}", path_config_file);
  if (!LoadServerConfig(path_config_file)) {
    SPDLOG_ERROR("unexpected errors happen when parsing yaml file");
    return -1;
  }

  auto io_context_ptr = boost::make_shared<boost::asio::io_context>();

  StartGrpcWorker();

  StartHealthCheckWorker(io_context_ptr);

  Server s(io_context_ptr, g_azugate_port);
  SPDLOG_INFO("azugate is listening on port {}", g_azugate_port);

  s.Run(io_context_ptr);

  SPDLOG_WARN("server exits");

  return 0;
}
