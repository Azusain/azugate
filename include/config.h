#ifndef __CONFIG_H
#define __CONFIG_H

#include "protocols.h"
#include <cstddef>
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
constexpr std::string_view kYamlFieldExternalAuthDomain = "auth_domain";
constexpr std::string_view kYamlFieldExternalAuthClientID = "auth_client_id";
constexpr std::string_view kYamlFieldExternalAuthClientSecret =
    "auth_client_secret";
// mics.
constexpr std::string_view kDftHttpPort = "80";
constexpr std::string_view kDftHttpsPort = "443";
constexpr size_t kDftStringReservedBytes = 256;
// runtime shared variables.
extern uint16_t g_azugate_port;
extern uint16_t g_azugate_admin_port;
// TODO: mTLS.
extern std::string g_ssl_crt;
extern std::string g_ssl_key;
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

// http(s) external oauth authorization.
extern bool g_http_external_authorization;
// used for generating and verifying tokens.
extern std::string g_authorization_token_secret;
// oauth.
extern std::string g_azugate_oauth_client_id;
extern std::string g_azugate_oauth_client_secret;
// rate limitor.
extern bool g_enable_rate_limiter;
extern size_t g_num_token_per_sec;
extern size_t g_num_token_max;

// io
extern size_t g_num_threads;

// TODO: auth (temp).
extern std::string g_external_auth_domain;
extern std::string g_external_auth_client_id;
extern std::string g_external_auth_client_secret;

std::string GetConfigPath();
void SetConfigPath(std::string &&path);

bool GetHttpCompression();
void SetHttpCompression(bool http_compression);

std::unordered_set<std::string> GetIpBlackList();
void AddBlacklistIp(const std::string &&ip);
void RemoveBlacklistIp(const std::string &&ip);

void SetHttps(bool https);
bool GetHttps();

void SetEnableRateLimitor(bool enable);
bool GetEnableRateLimitor();

void ConfigRateLimitor(size_t num_token_max, size_t num_token_per_sec);
// return g_num_token_max and g_num_token_per_sec.
std::pair<size_t, size_t> GetRateLimitorConfig();

void AddHealthzList(std::string &&addr);
const std::vector<std::string> &GetHealthzList();

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

bool LoadServerConfig();

} // namespace azugate

#endif

// TODO: all the g_xxx variables should be thread-safe.