#include "../../include/config.h"
#include "auth.h"
#include "protocols.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

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
    // TODO: hash func for TCP routing.
    // size_t h2 = hash<std::string>()(conn.address);
    // size_t h3 = hash<uint16_t>()(conn.port);
    return h1 /*^ (h2 << 1) ^ (h3 << 2)*/;
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
// TODO: mTLS.
std::string g_ssl_crt;
std::string g_ssl_key;
bool g_proxy_mode = false;
uint16_t g_target_port;
std::string g_target_host;
std::mutex g_config_mutex;
// router.
struct RouterEntry {
  // used for round robin.
  size_t next_index;
  std::vector<ConnectionInfo> targets;

  void AddTarget(const ConnectionInfo &&conn) {
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

std::unordered_map<ConnectionInfo, RouterEntry> g_router_table;
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
size_t g_num_threads = 4;
// healthz.
std::vector<std::string> g_healthz_list;
// external auth
bool g_http_external_authorization = false;
std::string g_external_auth_domain;
std::string g_external_auth_client_id;
std::string g_external_auth_client_secret;

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

void AddRoute(ConnectionInfo &&source, ConnectionInfo &&target) {
  auto it = g_router_table.find(source);
  if (it != g_router_table.end()) {
    it->second.AddTarget(std::move(target));
    return;
  }
  RouterEntry router_entry{};
  router_entry.AddTarget(std::move(target));
  g_router_table.emplace(std::move(source), std::move(router_entry));
}

std::optional<ConnectionInfo> GetTargetRoute(const ConnectionInfo &source) {
  std::lock_guard<std::mutex> lock(g_config_mutex);
  auto it = g_router_table.find(source);
  if (it != g_router_table.end() && !it->second.targets.empty()) {
    auto target = it->second.GetNextTarget();
    if (!target.has_value()) {
      return std::nullopt;
    }
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
  return std::nullopt;
}

size_t GetRouterTableSize() { return g_router_table.size(); }

// perfect match and prefix match.
// TODO: support priority by using seperate std::unordered_map.
bool azugate::ConnectionInfo::operator==(const ConnectionInfo &other) const {
  if (type != other.type) {
    return false;
  }
  if (type == ProtocolTypeTcp) {
    return address == other.address;
  }
  if (type == ProtocolTypeHttp) {
    // perfect match.
    if (http_url == other.http_url) {
      return true;
    }
    if (http_url.find('*') != std::string::npos) {
      return other.http_url.starts_with(http_url.substr(0, http_url.find('*')));
    }
  }
  return false;
}

bool LoadServerConfig() {
  try {
    auto path_config_file = GetConfigPath();
    // parse and load configuration.
    SPDLOG_INFO("loading config from {}", path_config_file);
    auto config = YAML::LoadFile(path_config_file);
    g_azugate_port = config[kYamlFieldPort].as<uint16_t>();
    g_azugate_admin_port = config[kYamlFieldAdminPort].as<uint16_t>();
    g_ssl_crt = config[kYamlFieldCrt].as<std::string>();
    g_ssl_key = config[kYamlFieldKey].as<std::string>();
    g_proxy_mode = config[kYamlFieldProxyMode].as<bool>();
    g_management_system_authentication =
        config[kYamlFieldManagementSysAuth].as<bool>();
    // external auth.
    g_external_auth_domain =
        config[kYamlFieldExternalAuthDomain].as<std::string>();
    g_external_auth_client_id =
        config[kYamlFieldExternalAuthClientID].as<std::string>();
    g_external_auth_client_secret =
        config[kYamlFieldExternalAuthClientSecret].as<std::string>();
  } catch (...) {
    SPDLOG_ERROR("unexpected errors happen when parsing yaml file");
    return false;
  }
  // token secret.
  g_authorization_token_secret = utils::GenerateSecret();
  return true;
};

} // namespace azugate