// Lines of code: 3247 (2570 excluding comments and blank lines).
#include "config.h"
#include "server.hpp"
#include "worker.hpp"
#include "http_cache.hpp"
#include "config_manager.hpp"
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <csignal>
#include <atomic>

// Global shutdown flag for graceful shutdown
std::atomic<bool> g_shutdown_requested{false};

// Signal handler for graceful shutdown
void signal_handler(int signum) {
  SPDLOG_INFO("Received signal {}, initiating graceful shutdown...", signum);
  g_shutdown_requested.store(true);
}

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
      ("s,enable-https", "Enable HTTPS", cxxopts::value<bool>()->default_value("false"))
      ("z,enable-compression", "Enable HTTP compression", cxxopts::value<bool>()->default_value("false"))
      ("r,enable-rate-limit", "Enable rate limiting", cxxopts::value<bool>()->default_value("false"))
      ("T,rate-limit-tokens", "Max tokens for rate limiting", cxxopts::value<size_t>()->default_value("100"))
      ("R,rate-limit-per-sec", "Tokens per second for rate limiting", cxxopts::value<size_t>()->default_value("10"))
      ("f,enable-file-proxy", "Enable file proxy mode", cxxopts::value<bool>()->default_value("false"))
      ("d,proxy-dir", "Directory to proxy files from", cxxopts::value<std::string>())
      ("l,enable-directory-listing", "Enable directory listing", cxxopts::value<bool>()->default_value("true"))
      ("g,generate-config", "Generate sample configuration file", cxxopts::value<std::string>())
      ("v,validate-config", "Validate configuration file", cxxopts::value<std::string>())
      ("t,config-template", "Configuration template type (full, minimal, dev, prod)", cxxopts::value<std::string>()->default_value("full"))
      ("H,hot-reload", "Enable configuration hot-reload", cxxopts::value<bool>()->default_value("false"))
      ("h,help", "Print usage");
  
  auto parsed_opts = opts.parse(argc, argv);
  
  if (parsed_opts.count("help")) {
    std::cout << opts.help() << std::endl;
    return 0;
  }
  
  // Handle configuration generation
  if (parsed_opts.count("generate-config")) {
    std::string output_path = parsed_opts["generate-config"].as<std::string>();
    std::string template_type = parsed_opts["config-template"].as<std::string>();
    
    std::cout << "Generating configuration file: " << output_path << std::endl;
    std::cout << "Template type: " << template_type << std::endl;
    
    // Create the appropriate template content
    std::string template_content;
    if (template_type == "minimal") {
      template_content = ConfigTemplateGenerator::generate_minimal_template();
    } else if (template_type == "dev" || template_type == "development") {
      template_content = ConfigTemplateGenerator::generate_development_template();
    } else if (template_type == "prod" || template_type == "production") {
      template_content = ConfigTemplateGenerator::generate_production_template();
    } else {
      template_content = ConfigTemplateGenerator::generate_full_template();
    }
    
    // Write to file
    std::ofstream file(output_path);
    if (!file.is_open()) {
      std::cerr << "Error: Cannot create configuration file: " << output_path << std::endl;
      return -1;
    }
    
    file << template_content;
    file.close();
    
    std::cout << "Configuration file generated successfully!" << std::endl;
    return 0;
  }
  
  // Handle configuration validation
  if (parsed_opts.count("validate-config")) {
    std::string config_path = parsed_opts["validate-config"].as<std::string>();
    
    std::cout << "Validating configuration file: " << config_path << std::endl;
    
    auto& config_manager = ConfigManager::instance();
    ValidationResult result = config_manager.validate_config(config_path);
    
    std::cout << result.to_string() << std::endl;
    
    if (!result.valid) {
      return -1;
    }
    
    std::cout << "Configuration is valid!" << std::endl;
    return 0;
  }

  IgnoreSignalPipe();

  InitLogger();

  // Determine configuration file path
  std::string path_config_file;
  if (parsed_opts.count("config")) {
    path_config_file = parsed_opts["config"].as<std::string>();
  } else {
    SPDLOG_INFO("use default configuration file");
    path_config_file = std::string(azugate::kDftConfigFile);
  }

  // Load initial configuration
  auto& config_manager = ConfigManager::instance();
  if (!config_manager.load_config(path_config_file)) {
    SPDLOG_ERROR("Failed to load initial configuration. Exiting.");
    return -1;
  }
  
  // Enable hot-reload if specified
  if (parsed_opts.count("hot-reload") && parsed_opts["hot-reload"].as<bool>()) {
      config_manager.enable_hot_reload(true);
  }
  
  // Apply initial configuration from manager
  const auto& initial_config = config_manager.get_config();
  g_azugate_port = initial_config["server"]["port"].as<uint16_t>(8080);
  
  // Apply command-line overrides
  if (parsed_opts.count("port")) {
    g_azugate_port = parsed_opts["port"].as<uint16_t>();
    SPDLOG_INFO("Port overridden to {} via command line", g_azugate_port);
  }
  
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
  
  // Initialize HTTP cache system
  HttpCacheConfig cache_config;
  cache_config.max_size_bytes = 100 * 1024 * 1024;  // 100MB cache
  cache_config.max_entries = 10000;
  cache_config.default_ttl = std::chrono::seconds(300);  // 5 minutes
  cache_config.respect_cache_control = true;
  cache_config.enable_conditional_requests = true;
  
  HttpCacheManager::instance().initialize(cache_config);
  SPDLOG_INFO("HTTP cache initialized with {}MB capacity", cache_config.max_size_bytes / (1024 * 1024));

  // Set up signal handlers for graceful shutdown
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
  std::signal(SIGBREAK, signal_handler);
#else
  std::signal(SIGQUIT, signal_handler);
  std::signal(SIGHUP, signal_handler);
#endif

  SPDLOG_INFO("Signal handlers installed for graceful shutdown");

  StartHealthCheckWorker(io_context_ptr);

  Server s(io_context_ptr, g_azugate_port);
  SPDLOG_INFO("🚀 AzuGate v1.0.0 started successfully!");
  SPDLOG_INFO("📊 Dashboard: http://localhost:{}/dashboard", g_azugate_port);
  SPDLOG_INFO("🏥 Health: http://localhost:{}/health", g_azugate_port);
  SPDLOG_INFO("📈 Metrics: http://localhost:{}/metrics", g_azugate_port);
  SPDLOG_INFO("⚙️ Config: http://localhost:{}/config", g_azugate_port);
  SPDLOG_INFO("Press Ctrl+C for graceful shutdown");

  s.Run(io_context_ptr);

  SPDLOG_WARN("server exits");

  return 0;
}
