// Lines of code: 3247 (2570 excluding comments and blank lines).
#include "config.h"
#include "server.hpp"
#include "worker.hpp"
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <string>

// TODO:
// ref: https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// memmory pool optimaization.
int main(int argc, char *argv[]) {
  using namespace azugate;
  // parser cmd line.
  cxxopts::Options opts("azugate", "An unsafe and inefficient gateway");
  opts.add_options()("c,config", "Configuration YAML file path",
                     cxxopts::value<std::string>());
  auto parsed_opts = opts.parse(argc, argv);

  IgnoreSignalPipe();

  InitLogger();

  // Load the configurations from the local file.
  std::string path_config_file;
  if (parsed_opts.count("config")) {
    path_config_file = parsed_opts["config"].as<std::string>();
  } else {
    SPDLOG_INFO("use default configuration file");
    path_config_file = fmt::format("{}/{}", azugate::kPathResourceFolder,
                                   azugate::kDftConfigFile);
  }
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
