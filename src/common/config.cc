#include "../../include/config.h"
#include "protocols.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace std {
template <> struct hash<azugate::ConnectionInfo> {
  size_t operator()(const azugate::ConnectionInfo &conn) const {
    size_t h1 = hash<azugate::ProtocolType>()(conn.type);
    size_t h2 = hash<string_view>()(conn.address);
    size_t h3 = hash<uint16_t>()(conn.port);
    size_t h4 = hash<string_view>()(conn.http_url);
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
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
bool g_management_system_authentication = false;
bool g_http_external_authorization = false;
std::string g_external_oauth_server_domain = "localhost";
std::string g_external_oauth_server_path = "/";
std::string g_azugate_domain = "localhost";
// TODO: mTLS.
std::string g_ssl_crt;
std::string g_ssl_key;
bool g_proxy_mode = false;
uint16_t g_target_port;
std::string g_target_host;
std::mutex g_config_mutex;
// router.
std::unordered_map<ConnectionInfo, ConnectionInfo> g_router_map;
// token.
std::string g_authorization_token_secret;
// oauth.
// TODO: configured in config.yaml.
std::string g_azugate_oauth_client_id;
std::string g_azugate_oauth_client_secret;
// rate limitor
bool g_enable_rate_limiter = false;
size_t g_num_token_per_sec = 100;
size_t g_num_token_max = 1000;
// io
size_t g_num_threads = 1;
// healthz.
std::vector<std::string> g_healthz_list;

std::string GetConfigPath() {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  return g_path_config_file;
};

void SetConfigPath(std::string &&path) {
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

void AddRouterMapping(ConnectionInfo &&source, ConnectionInfo &&target) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  g_router_map.emplace(
      std::pair<ConnectionInfo, ConnectionInfo>{source, target});
}

// TODO: implement support for wildcards('*').
std::optional<ConnectionInfo> GetRouterMapping(const ConnectionInfo &source) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  auto it = g_router_map.find(source);
  if (it != g_router_map.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool azugate::ConnectionInfo::operator==(const ConnectionInfo &other) const {
  if (type != other.type) {
    return false;
  }
  auto tcp_eq = type == ProtocolTypeTcp && address == other.address;
  auto http_eq = type == ProtocolTypeHttp && http_url == other.http_url;
  return tcp_eq | http_eq;
}

} // namespace azugate