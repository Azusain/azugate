#include "config.h"
#include "protocols.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include <unordered_map>
#include <unordered_set>
#include <utility>

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
uint16_t port = 443;
uint16_t admin_port = 50051;
std::string path_config_file;
std::unordered_set<std::string> ip_blacklist;
bool enable_http_compression = false;
bool enable_https = false;
// TODO: mTLS.
std::string sslCrt;
std::string sslKey;
bool proxy_mode = false;
uint16_t target_port;
std::string target_host;
std::mutex config_mutex;
// router.
std::unordered_map<ConnectionInfo, ConnectionInfo> router_map;

std::string GetConfigPath() {
  std::lock_guard<std::mutex> lock(config_mutex);
  return path_config_file;
};

void SetConfigPath(std::string &&path) {
  std::lock_guard<std::mutex> lock(config_mutex);
  path_config_file = path;
}

std::unordered_set<std::string> GetIpBlackList() {
  std::lock_guard<std::mutex> lock(config_mutex);
  return ip_blacklist;
};

void AddBlacklistIp(const std::string &&ip) {
  std::lock_guard<std::mutex> lock(config_mutex);
  // TODO: return more details.
  ip_blacklist.insert(ip);
}

void RemoveBlacklistIp(const std::string &&ip) {
  // TODO: return more details.
  ip_blacklist.erase(ip);
}

bool GetHttpCompression() {
  std::lock_guard<std::mutex> lock(config_mutex);
  return enable_http_compression;
}

void SetHttpCompression(bool http_compression) {
  enable_http_compression = http_compression;
}

void SetHttps(bool https) { enable_https = https; }

bool GetHttps() { return enable_https; }

void AddRouterMapping(ConnectionInfo &&source, ConnectionInfo &&target) {
  std::lock_guard<std::mutex> lock(config_mutex);
  router_map.emplace(std::pair<ConnectionInfo, ConnectionInfo>{source, target});
}

// TODO: implement support for wildcards.
std::optional<ConnectionInfo> GetRouterMapping(const ConnectionInfo &source) {
  std::lock_guard<std::mutex> lock(config_mutex);
  auto it = router_map.find(source);
  if (it != router_map.end()) {
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