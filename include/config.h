#ifndef __CONFIG_H
#define __CONFIG_H

#include <string_view>

// http server
constexpr size_t kNumMaxListen = 5;
constexpr size_t kDftBufSize = 1024 * 4;
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

// runtime shared variables.
extern uint16_t port;
extern uint16_t admin_port;
extern std::string path_config_file;
// TODO: mTLS.
extern std::string sslCrt;
extern std::string sslKey;

#endif