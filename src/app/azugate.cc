// Lines of code: 3247 (2570 excluding comments and blank lines).
#include "config.h"
#include "server.hpp"
#include "worker.hpp"
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <iostream>
#include <filesystem>

// TODO:
// ref: https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// memmory pool optimaization.
int main(int argc, char *argv[]) {
  using namespace azugate;
  // parser cmd line.
  cxxopts::Options opts("azugate", "An unsafe and inefficient gateway");
  opts.add_options()
      ("c,config", "Configuration YAML file path", cxxopts::value<std::string>())
      ("p,port", "Server port", cxxopts::value<uint16_t>()->default_value("8080"))
      ("enable-https", "Enable HTTPS", cxxopts::value<bool>()->default_value("false"))
      ("enable-compression", "Enable HTTP compression", cxxopts::value<bool>()->default_value("false"))
      ("enable-rate-limit", "Enable rate limiting", cxxopts::value<bool>()->default_value("false"))
      ("rate-limit-tokens", "Max tokens for rate limiting", cxxopts::value<size_t>()->default_value("100"))
      ("rate-limit-per-sec", "Tokens per second for rate limiting", cxxopts::value<size_t>()->default_value("10"))
      ("enable-file-proxy", "Enable file proxy mode", cxxopts::value<bool>()->default_value("false"))
      ("proxy-dir", "Directory to proxy files from", cxxopts::value<std::string>())
      ("enable-directory-listing", "Enable directory listing", cxxopts::value<bool>()->default_value("true"))
      ("h,help", "Print usage");
  
  auto parsed_opts = opts.parse(argc, argv);
  
  if (parsed_opts.count("help")) {
    std::cout << opts.help() << std::endl;
    return 0;
  }

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
  // Apply command line configurations
  if (parsed_opts.count("port")) {
    g_azugate_port = parsed_opts["port"].as<uint16_t>();
    SPDLOG_INFO("Port set to {} via command line", g_azugate_port);
  } else {
    SPDLOG_INFO("loading configuration from {}", path_config_file);
    if (!LoadServerConfig(path_config_file)) {
      SPDLOG_ERROR("unexpected errors happen when parsing yaml file");
      return -1;
    }
  }
  
  // Apply other command line options
  if (parsed_opts.count("enable-https")) {
    SetHttps(parsed_opts["enable-https"].as<bool>());
  }
  
  if (parsed_opts.count("enable-compression")) {
    SetHttpCompression(parsed_opts["enable-compression"].as<bool>());
  }
  
  if (parsed_opts.count("enable-rate-limit")) {
    SetEnableRateLimitor(parsed_opts["enable-rate-limit"].as<bool>());
    if (parsed_opts["enable-rate-limit"].as<bool>()) {
      ConfigRateLimitor(parsed_opts["rate-limit-tokens"].as<size_t>(),
                       parsed_opts["rate-limit-per-sec"].as<size_t>());
    }
  }
  
  // Handle file proxy mode
  bool enable_file_proxy = parsed_opts["enable-file-proxy"].as<bool>();
  std::string proxy_directory;
  
  if (enable_file_proxy) {
    if (parsed_opts.count("proxy-dir")) {
      proxy_directory = parsed_opts["proxy-dir"].as<std::string>();
      if (!std::filesystem::exists(proxy_directory)) {
        SPDLOG_ERROR("Proxy directory does not exist: {}", proxy_directory);
        return -1;
      }
      SPDLOG_INFO("File proxy enabled for directory: {}", proxy_directory);
      
      // Set up file proxy route
      azugate::AddRoute(
          azugate::ConnectionInfo{
              .type = azugate::ProtocolTypeHttp,
              .http_url = "/", // Catch all paths
          },
          azugate::ConnectionInfo{
              .type = azugate::ProtocolTypeHttp,
              .address = "localhost",
              .port = g_azugate_port,
              .http_url = proxy_directory,
              .remote = false, // Local file access
          });
    } else {
      SPDLOG_ERROR("File proxy enabled but no directory specified. Use --proxy-dir");
      return -1;
    }
  }

  auto io_context_ptr = boost::make_shared<boost::asio::io_context>();


  StartHealthCheckWorker(io_context_ptr);

  Server s(io_context_ptr, g_azugate_port);
  SPDLOG_INFO("azugate is listening on port {}", g_azugate_port);

  s.Run(io_context_ptr);

  SPDLOG_WARN("server exits");

  return 0;
}
