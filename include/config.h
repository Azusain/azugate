#ifndef __CONFIG_H
#define __CONFIG_H

#include "protocols.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace azugate {
// http server
constexpr size_t kNumMaxListen = 5;
constexpr size_t kDefaultBufSize = 1024 * 4;
// TODO: this needs more consideration.
constexpr size_t kMaxFdSize = 1024 / 2;
// TODO: this needs some configuration file.
constexpr std::string_view kPathResourceFolder = "../resources";
constexpr std::string_view kPathDftPage = "/welcome.html";
// ref to Nginx, the value is 8kb, but 60kb in Envoy.
constexpr size_t kMaxHttpHeaderSize = 1024 * 8;
constexpr size_t kMaxHeadersNum = 20;
// yaml.
constexpr std::string_view kDftConfigFile = "config.yaml";
constexpr std::string_view kYamlFieldPort = "port";
constexpr std::string_view kYamlFieldCrt = "crt";
constexpr std::string_view kYamlFieldKey = "key";
constexpr std::string_view kYamlFieldAdminPort = "admin_port";
constexpr std::string_view kYamlFieldProxyMode = "proxy_mode";
constexpr std::string_view kYamlFieldProxyTargetPort = "target_port";
constexpr std::string_view kYamlFieldProxyTargetHost = "target_host";
constexpr std::string_view kYamlFieldManagementSysAuth = "authentication";

// runtime shared variables.
extern uint16_t g_port;
extern uint16_t g_admin_port;

// TODO: mTLS.
extern std::string g_sslCrt;
extern std::string g_sslKey;
// TODO: need a better alternaive for these 3 variables..
extern bool g_proxy_mode;
extern uint16_t g_target_port;
extern std::string g_target_host;
// TODO: currently, this may seem unnecessary, but
// it will be useful when we add more configuration variables
// and implement hot-reload functionality in the future.
extern std::mutex g_config_mutex;
// set it to true when integrating with external authentication provider.
extern bool g_management_system_authentication;
// used for generating and verifying tokens.
extern std::string g_token_secret;

extern std::string GetConfigPath();
void SetConfigPath(std::string &&path);

bool GetHttpCompression();
void SetHttpCompression(bool http_compression);

std::unordered_set<std::string> GetIpBlackList();
void AddBlacklistIp(const std::string &&ip);
void RemoveBlacklistIp(const std::string &&ip);

void SetHttps(bool https);
bool GetHttps();

// router.
struct ConnectionInfo {
  ProtocolType type;
  // currently IPv4.
  std::string_view address;
  uint16_t port;
  std::string_view http_url;

  bool operator==(const ConnectionInfo &other) const;
};

void AddRouterMapping(ConnectionInfo &&source, ConnectionInfo &&target);

std::optional<ConnectionInfo> GetRouterMapping(const ConnectionInfo &source);

} // namespace azugate

#endif