#include "config.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>

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

} // namespace azugate