#include "core.h"
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/system.hpp>
#include <boost/thread.hpp>
#include <cstdint>
#include <exception>
#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

using namespace boost::asio;

// TODO: find a great place for these definations.
constexpr std::string_view kDftConfigFile = "config.yaml";
constexpr std::string_view kYamlFieldPort = "port";
constexpr std::string_view kYamlFieldCrt = "crt";
constexpr std::string_view kYamlFieldKey = "key";

uint16_t port = 80;
// TODO: mTLS.
std::string_view sslCrt;
std::string_view sslKey;

void handler(
    const boost::shared_ptr<ssl::stream<ip::tcp::socket>> &ssl_sock_ptr) {
  try {
    // ssl handshake.
    ssl_sock_ptr->handshake(ssl::stream_base::server);
  } catch (const std::exception &e) {
    std::string what = e.what();
    if (what.compare("handshake: ssl/tls alert certificate unknown (SSL "
                     "routines) [asio.ssl:167773206]")) {
      SPDLOG_ERROR("failed to handshake: {}", what);
      return;
    }
    // ignore unknown certificate error.
    SPDLOG_WARN(what);
  }

  if (!FileProxy(ssl_sock_ptr, kPathResourceFolder)) {
    SPDLOG_ERROR("failed to handler file request");
    return;
  }
}

int main() {
  try {
    // parse configuration.
    auto config = YAML::LoadFile(
        fmt::format("{}/{}", kPathResourceFolder, kDftConfigFile));
    port = config[kYamlFieldPort].as<uint16_t>();
    sslCrt = config[kYamlFieldCrt].as<std::string>();
    sslKey = config[kYamlFieldKey].as<std::string>();
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failed to load config file: {}", e.what());
    return 1;
  }
  // setup logger.
  // ref: https://github.com/gabime/spdlog/wiki/3.-Custom-formatting.
  // for production, use this logger:
  // spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %v");
  // with source file and line when debug:
  spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %s:%# | %v");
  spdlog::set_level(spdlog::level::debug);
  // setup ssl connection.
  ssl::context ssl_context(ssl::context::sslv23);
  ssl_context.set_verify_mode(ssl::verify_none);
  try {
    // TODO: file format.
    ssl_context.use_certificate_chain_file(std::string(sslCrt));
    ssl_context.use_private_key_file(std::string(sslKey), ssl::context::pem);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failed to setup ssl context: {}", e.what());
    return 1;
  }
  // setup a basic OTPL server.
  io_service service;
  ip::tcp::endpoint local_address(ip::tcp::v4(), port);
  ip::tcp::acceptor acc(service, local_address);
  SPDLOG_INFO("server runs on port {}", port);
  for (;;) {
    boost::shared_ptr<ssl::stream<ip::tcp::socket>> ssl_sock_ptr(
        new ssl::stream<ip::tcp::socket>(service, ssl_context));
    acc.accept(ssl_sock_ptr->lowest_layer());
    boost::thread(boost::bind(handler, ssl_sock_ptr));
  }
  return 0;
}

// TODO:
// let's proxy websockets then.
// design fault tolerances.
// crush if being frequently requested?
// maybe reuse port.
// logging system.
// health check.
// stat prefix in envoy?
// best practice you can check:
// https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// envoy OAuth, JWT, RBAC.