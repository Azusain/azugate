#include "../api//config_service.hpp"
#include "config.h"
#include "http_service.h"
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
#include <exception>
#include <fmt/base.h>
#include <fmt/format.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

using namespace boost::asio;
using namespace azugate;

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

  if (!FileProxy(ssl_sock_ptr, azugate::kPathResourceFolder)) {
    SPDLOG_ERROR("failed to handler file request");
    return;
  }
}

int main() {
  // ref: https://github.com/gabime/spdlog/wiki/3.-Custom-formatting.
  // for production, use this logger:
  // spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %v");
  // with source file and line when debug:
  spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %s:%# | %v");
  spdlog::set_level(spdlog::level::debug);
  azugate::SetConfigPath(fmt::format("{}/{}", azugate::kPathResourceFolder,
                                     azugate::kDftConfigFile));

  try {
    auto path_config_file = azugate::GetConfigPath();
    // parse configuration.
    SPDLOG_INFO("loading config from {}", path_config_file);
    auto config = YAML::LoadFile(path_config_file);
    port = config[kYamlFieldPort].as<uint16_t>();
    admin_port = config[kYamlFieldAdminPort].as<uint16_t>();
    sslCrt = config[kYamlFieldCrt].as<std::string>();
    sslKey = config[kYamlFieldKey].as<std::string>();
  } catch (...) {
    SPDLOG_ERROR("unexpected errors happen when parsing yaml file");
    return 1;
  }

  // setup ssl connection.
  ssl::context ssl_context(ssl::context::sslv23);
  try {
    // TODO: file format.
    ssl_context.use_certificate_chain_file(std::string(sslCrt));
    ssl_context.use_private_key_file(std::string(sslKey), ssl::context::pem);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failed to setup ssl context: {}", e.what());
    return 1;
  }

  // setup grpc server.
  auto grpc_thread = std::thread([&]() {
    grpc::ServerBuilder server_builder;
    server_builder.AddListeningPort(fmt::format("0.0.0.0:{}", admin_port),
                                    grpc::InsecureServerCredentials());
    ConfigServiceImpl config_service;
    server_builder.RegisterService(&config_service);
    std::unique_ptr<grpc::Server> server(server_builder.BuildAndStart());
    SPDLOG_INFO("grpc server runs on {}", admin_port);
    server->Wait();
  });

  // setup a basic OTPL server.
  io_service service;
  ip::tcp::endpoint local_address(ip::tcp::v4(), port);
  ip::tcp::acceptor acc(service, local_address);
  SPDLOG_INFO("azugate runs on port {}", port);
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
// maybe reuse port.
// logging system.
// health check.
// stat prefix in envoy?
// best practice you can check:
// https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// envoy OAuth, JWT, RBAC.
// async log.