#ifndef __CONFIG_H
#define __CONFIG_H
#define AZUGATE_VERSION_STRING "azugate/1.0"

#include "protocols.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
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
constexpr std::string_view kDftConfigFile = "config.default.yaml";
constexpr std::string_view kYamlFieldPort = "port";
constexpr std::string_view kYamlFieldCrt = "crt";
constexpr std::string_view kYamlFieldKey = "key";
constexpr std::string_view kYamlFieldAdminPort = "admin_port";
constexpr std::string_view kYamlFieldExternalHTTPAuthentication =
    "external_http_authentication";
constexpr std::string_view kYamlFieldExternalAuthDomain = "auth_domain";
constexpr std::string_view kYamlFieldExternalAuthClientID = "auth_client_id";
constexpr std::string_view kYamlFieldExternalAuthClientSecret =
    "auth_client_secret";
constexpr std::string_view kYamlFieldExternalAuthCallbackUrl = "callback_url";
// mics.
constexpr std::string_view kDftHttpPort = "80";
constexpr std::string_view kDftHttpsPort = "443";
constexpr size_t kDftStringReservedBytes = 256;
constexpr size_t kDftHealthCheckGapSecond = 3;

// TODO: 100 MB.
constexpr size_t kMaxBodyBufferSize = 1024 * 1024 * 100;
// runtime shared variables.
extern uint16_t g_azugate_port;
extern uint16_t g_azugate_admin_port;
// TODO: mTLS.
extern std::string g_ssl_crt;
extern std::string g_ssl_key;
// TODO: currently, this may seem unnecessary, but
// it will be useful when we add more configuration variables
// and implement hot-reload functionality in the future.
extern std::mutex g_config_mutex;

// http(s) external oauth authorization.
extern bool g_http_external_authorization;
// used for generating and verifying tokens.
extern std::string g_authorization_token_secret;

// rate limitor.
extern bool g_enable_rate_limiter;
extern size_t g_num_token_per_sec;
extern size_t g_num_token_max;

// io
extern size_t g_num_threads;

// exteranl auth.
extern std::string g_external_auth_domain;
extern std::string g_external_auth_client_id;
extern std::string g_external_auth_client_secret;
extern std::string g_external_auth_callback_url;

void InitLogger();

void IgnoreSignalPipe();

std::string GetConfigPath();
void SetConfigFilePath(const std::string &path);

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
  std::string address;
  uint16_t port = 0;
  std::string http_url;
  // access local file or remote endpoint.
  bool remote;
  bool operator==(const ConnectionInfo &other) const;
};

void AddRoute(ConnectionInfo &&source, ConnectionInfo &&target);

std::optional<ConnectionInfo> GetTargetRoute(const ConnectionInfo &source);

size_t GetRouterTableSize();

bool LoadServerConfig(const std::string &path_config_file);

} // namespace azugate

#endif

// TODO: all the g_xxx variables should be thread-safe.