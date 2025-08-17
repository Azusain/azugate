#include "../../include/config.h"
#include "auth.h"
#include "protocols.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#endif

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

namespace std {
template <> struct hash<azugate::ConnectionInfo> {
  // TODO: redesign hash func.
  size_t operator()(const azugate::ConnectionInfo &conn) const {
    size_t h1 = hash<azugate::ProtocolType>()(conn.type);
    // size_t h2 = hash<std::string>()(conn.address);
    // size_t h3 = hash<uint16_t>()(conn.port);
    size_t h4 = hash<std::string>()(conn.http_url);
    return /* h1 ^ (h2 << 1) ^ (h3 << 2) ^ */ h1 ^ (h4 << 1);
  }
};
} // namespace std

namespace azugate {
uint16_t g_azugate_port = 443;
uint16_t g_azugate_admin_port = 50051;
std::string g_path_config_file;
std::unordered_set<std::string> g_ip_blacklist;
bool g_enable_http_compression = false;
bool g_enable_https = false;
// TODO: mTLS.
std::string g_ssl_crt;
std::string g_ssl_key;
std::mutex g_config_mutex;

// logger.
// TODO: async logging system.
void InitLogger() {
  // setup logger.
  // ref: https://github.com/gabime/spdlog/wiki/3.-Custom-formatting.
  // for production, use this logger:
  // spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %v");
  // with source file and line when debug:
  spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %s:%# | %v");
  spdlog::set_level(spdlog::level::debug);
}

void IgnoreSignalPipe() {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
  // ignore SIGPIPE on Unix-like systems.
  struct sigaction sa{};
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
#endif
  // On Windows, SIGPIPE doesn't exist, so nothing to do
}

// router.
struct RouterEntry {
  // used for round robin.
  size_t next_index;
  std::vector<ConnectionInfo> targets;

  void AddTarget(ConnectionInfo &&conn) {
    auto pred = [&](const ConnectionInfo &c) {
      return conn.address == c.address && conn.http_url == c.http_url &&
             conn.port == c.port && conn.type == c.type &&
             conn.remote == c.remote;
    };
    auto it = std::find_if(targets.begin(), targets.end(), pred);
    if (it == targets.end()) {
      targets.emplace_back(conn);
    }
    return;
  }

  // TODO: exact match & prefix match.
  void RemoveTarget(const ConnectionInfo &conn) {
    auto it = std::remove(targets.begin(), targets.end(), conn);
    if (it != targets.end()) {
      targets.erase(it, targets.end());
      if (next_index >= targets.size() && !targets.empty()) {
        next_index %= targets.size();
      }
    }
  }

  std::optional<ConnectionInfo> GetNextTarget() {
    if (targets.empty()) {
      return std::nullopt;
    }
    ConnectionInfo &result = targets[next_index];
    next_index = (next_index + 1) % targets.size();
    return result;
  }

  bool Contains(const ConnectionInfo &conn) const {
    return std::find(targets.begin(), targets.end(), conn) != targets.end();
  }
};

std::unordered_map<ConnectionInfo, RouterEntry> g_exact_routes;
std::vector<std::pair<ConnectionInfo, RouterEntry>> g_prefix_routes;
// token.
std::string g_authorization_token_secret;

// rate limitor
bool g_enable_rate_limiter = false;
size_t g_num_token_per_sec = 100;
size_t g_num_token_max = 1000;
// io
size_t g_num_threads = 4;
// healthz.
std::vector<std::string> g_healthz_list;

// external auth
bool g_http_external_authorization = false;
std::string g_external_auth_domain;
std::string g_external_auth_client_id;
std::string g_external_auth_client_secret;
std::string g_external_auth_callback_url;

std::string GetConfigPath() {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  return g_path_config_file;
};

void SetConfigFilePath(const std::string &path) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  g_path_config_file = path;
}

std::unordered_set<std::string> GetIpBlackList() {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  return g_ip_blacklist;
};

void AddBlacklistIp(const std::string &&ip) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  // TODO: return more details.
  g_ip_blacklist.insert(ip);
}

void RemoveBlacklistIp(const std::string &&ip) {
  // TODO: return more details.
  g_ip_blacklist.erase(ip);
}

bool GetHttpCompression() {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  return g_enable_http_compression;
}

void SetHttpCompression(bool http_compression) {
  g_enable_http_compression = http_compression;
}

void SetHttps(bool https) { g_enable_https = https; }

bool GetHttps() { return g_enable_https; }

void SetEnableRateLimitor(bool enable) { g_enable_rate_limiter = enable; };
bool GetEnableRateLimitor() { return g_enable_rate_limiter; };

void ConfigRateLimitor(size_t num_token_max, size_t num_token_per_sec) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  if (num_token_max > 0) {
    g_num_token_max = num_token_max;
  }
  if (num_token_per_sec > 0) {
    g_num_token_per_sec = num_token_per_sec;
  }
}
// return g_num_token_max and g_num_token_per_sec.
std::pair<size_t, size_t> GetRateLimitorConfig() {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  return std::pair<size_t, size_t>(g_num_token_max, g_num_token_per_sec);
}

void AddHealthzList(std::string &&addr) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  g_healthz_list.emplace_back(addr);
}

const std::vector<std::string> &GetHealthzList() {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  return g_healthz_list;
}

inline bool prefixMatchEqual(const ConnectionInfo &source_conn_info,
                             const ConnectionInfo &rule_conn_info) {
  std::string prefix = rule_conn_info.http_url.substr(
                       0, rule_conn_info.http_url.find('*'));
  bool type_match = source_conn_info.type == rule_conn_info.type;
  bool prefix_match = source_conn_info.http_url.starts_with(prefix);
  
  SPDLOG_DEBUG("prefix match details: source='{}' type={}, rule='{}' type={}, prefix='{}', type_match={}, prefix_match={}",
               source_conn_info.http_url, source_conn_info.type,
               rule_conn_info.http_url, rule_conn_info.type, 
               prefix, type_match, prefix_match);
               
  return prefix_match && type_match;
}

void AddRoute(ConnectionInfo &&source, ConnectionInfo &&target) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  
  if (source.http_url.find("*") != std::string::npos) {
    SPDLOG_DEBUG("add prefix match rule: {} -> {}", source.http_url,
                 target.http_url);
    for (auto &route : g_prefix_routes) {
      auto http_url = route.first.http_url;
      if (prefixMatchEqual(source, route.first)) {
        route.second.AddTarget(std::move(target));
        return;
      }
    }
    RouterEntry router_entry{};
    router_entry.AddTarget(std::move(target));
    g_prefix_routes.emplace_back(std::move(source), std::move(router_entry));
    return;
  }
  // exact match.
  auto er_it = g_exact_routes.find(source);
  if (er_it != g_exact_routes.end()) {
    er_it->second.AddTarget(std::move(target));
    return;
  }
  RouterEntry router_entry{};
  router_entry.AddTarget(std::move(target));
  g_exact_routes.emplace(std::move(source), std::move(router_entry));
  return;
}

std::optional<ConnectionInfo> GetTargetRoute(const ConnectionInfo &source) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  SPDLOG_DEBUG("Looking for route for: {} (type: {})", source.http_url, source.type);
  
  // exact match first.
  auto it = g_exact_routes.find(source);
  if (it != g_exact_routes.end() && !it->second.targets.empty()) {
    SPDLOG_DEBUG("Found exact route match");
    auto next_target = it->second.GetNextTarget();
    return next_target;
  }
  
  // prefix match.
  SPDLOG_DEBUG("Checking {} prefix routes", g_prefix_routes.size());
  for (auto &route : g_prefix_routes) {
    SPDLOG_DEBUG("Checking prefix route: {} vs {}", source.http_url, route.first.http_url);
    if (!prefixMatchEqual(source, route.first)) {
      SPDLOG_DEBUG("Prefix match failed");
      continue;
    }
    SPDLOG_DEBUG("Prefix match succeeded!");
    auto target = route.second.GetNextTarget();
    auto &target_url = target->http_url;
    if (target_url.size() >= 2 &&
        target_url.compare(target_url.size() - 2, 2, "/*") == 0) {
      std::string target_prefix = target_url.substr(0, target_url.size() - 2);
      std::string suffix = source.http_url;
      if (suffix.find(target_prefix) == 0) {
        suffix = suffix.substr(target_prefix.size());
      }
      if (!target_prefix.empty() && target_prefix.back() != '/' &&
          (suffix.empty() || suffix.front() != '/')) {
        target_prefix += '/';
      }
      target->http_url = target_prefix + suffix;
    }
    return target;
  }
  SPDLOG_WARN("no path found for: {}", source.http_url);
  return std::nullopt;
}

size_t GetRouterTableSize() { return g_exact_routes.size(); }

// perfect match and prefix match.
bool azugate::ConnectionInfo::operator==(const ConnectionInfo &other) const {
  if (type != other.type) {
    return false;
  }
  if (type == ProtocolTypeTcp) {
    return address == other.address;
  }
  return (type == ProtocolTypeHttp || type == ProtocolTypeWebSocket) &&
         http_url == other.http_url;
}

bool LoadServerConfig(const std::string &path_config_file) {
  YAML::Node config;
  // Check if file exists and can be loaded
  if (!std::filesystem::exists(path_config_file)) {
    SPDLOG_ERROR("Config file not found: {}", path_config_file);
    return false;
  }
  
  // Load and parse YAML file
  try {
    config = YAML::LoadFile(path_config_file);
  } catch (const YAML::Exception& e) {
    SPDLOG_ERROR("Failed to parse YAML config: {}", e.what());
    return false;
  }
  
  // Safely extract configuration values with validation
  try {
    if (!config[std::string(kYamlFieldPort)]) {
      SPDLOG_ERROR("Missing required field: {}", kYamlFieldPort);
      return false;
    }
    g_azugate_port = config[std::string(kYamlFieldPort)].as<uint16_t>();
    
    if (!config[std::string(kYamlFieldAdminPort)]) {
      SPDLOG_ERROR("Missing required field: {}", kYamlFieldAdminPort);
      return false;
    }
    g_azugate_admin_port = config[std::string(kYamlFieldAdminPort)].as<uint16_t>();
    
    // SSL certificates (optional)
    if (config[std::string(kYamlFieldCrt)]) {
      g_ssl_crt = config[std::string(kYamlFieldCrt)].as<std::string>();
    }
    if (config[std::string(kYamlFieldKey)]) {
      g_ssl_key = config[std::string(kYamlFieldKey)].as<std::string>();
    }
    
    // External auth (optional)
    if (config[std::string(kYamlFieldExternalHTTPAuthentication)]) {
      g_http_external_authorization = config[std::string(kYamlFieldExternalHTTPAuthentication)].as<bool>();
    }
    if (config[std::string(kYamlFieldExternalAuthDomain)]) {
      g_external_auth_domain = config[std::string(kYamlFieldExternalAuthDomain)].as<std::string>();
    }
    if (config[std::string(kYamlFieldExternalAuthClientID)]) {
      g_external_auth_client_id = config[std::string(kYamlFieldExternalAuthClientID)].as<std::string>();
    }
    if (config[std::string(kYamlFieldExternalAuthClientSecret)]) {
      g_external_auth_client_secret = config[std::string(kYamlFieldExternalAuthClientSecret)].as<std::string>();
    }
    if (config[std::string(kYamlFieldExternalAuthCallbackUrl)]) {
      g_external_auth_callback_url = config[std::string(kYamlFieldExternalAuthCallbackUrl)].as<std::string>();
    }
  } catch (const YAML::Exception& e) {
    SPDLOG_ERROR("Error parsing config value: {}", e.what());
    return false;
  }
  
  // token secret.
  g_authorization_token_secret = utils::GenerateSecret();
  return true;
};

} // namespace azugate