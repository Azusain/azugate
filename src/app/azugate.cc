#include "../api//config_service.hpp"
#include "config.h"
#include "dispatcher.h"
#include "filter.h"
#include "protocols.h"
#include "rate_limiter.h"
#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/bind/bind.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system.hpp>
#include <boost/thread.hpp>
#include <boost/thread/detail/thread.hpp>

#include <cstdlib>
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
#include <utility>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

using namespace boost::asio;
using namespace azugate;

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
    proxy_mode = config[kYamlFieldProxyMode].as<bool>();
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
  std::thread grpc_thread([&]() {
    grpc::ServerBuilder server_builder;
    server_builder.AddListeningPort(fmt::format("0.0.0.0:{}", admin_port),
                                    grpc::InsecureServerCredentials());
    ConfigServiceImpl config_service;
    server_builder.RegisterService(&config_service);
    std::unique_ptr<grpc::Server> server(server_builder.BuildAndStart());
    SPDLOG_INFO("gRPC server runs on port {}", admin_port);
    server->Wait();
  });

  // setup http gateway for gRPC server.
  std::thread http_gateway([]() {
    SPDLOG_INFO("http gateway runs on port 8081");
    // TODO: this is way too violent.
    if (!std::system("../grpc-proxy/proxy")) {
      SPDLOG_WARN("some errors happened in the http gateway");
    };
    SPDLOG_WARN("http gateway process exits");
  });

  // setup a basic OTPL server.
  auto io_context_ptr = boost::make_shared<boost::asio::io_context>();

  // TODO: ipv6?
  ip::tcp::endpoint local_address(ip::tcp::v4(), port);
  ip::tcp::acceptor acc(*io_context_ptr, local_address);
  // dns resovler.
  ip::tcp::resolver resolver(io_context);
  ip::tcp::resolver::query query(target_host, std::to_string(target_port));
  SPDLOG_INFO("azugate runs on port {}", port);

  // TODO: for testing purpose.
  azugate::AddRouterMapping(
      ConnectionInfo{.type = ProtocolTypeTcp, .address = "127.0.0.1"},
      ConnectionInfo{
          .type = ProtocolTypeTcp, .address = "127.0.0.1", .port = 6080});

  // setup timer.
  azugate::TokenBucketRateLimiter rate_limiter(io_context_ptr);
  rate_limiter.Start();

  // setup a basic OTPL server.
  // TODO: optimize it with async io architecture.
  std::thread otpl_server([&]() {
    for (;;) {
      auto sock_ptr = boost::make_shared<ip::tcp::socket>(*io_context_ptr);
      acc.accept(*sock_ptr);
      auto source_endpoint = sock_ptr->remote_endpoint();
      // TODO: why cant this be initialized in the bracklets.
      ConnectionInfo src_conn_info;
      src_conn_info.address = source_endpoint.address().to_string();
      SPDLOG_INFO("connection from {}", src_conn_info.address);
      if (!azugate::Filter(sock_ptr, src_conn_info)) {
        continue;
      }
      Dispatch(io_context_ptr, sock_ptr, ssl_context, std::move(src_conn_info));
    }
  });

  // invoke asynchronous tasks.
  io_context_ptr->run();

  return 0;
}

// TODO:
// let's proxy websockets then.
// design fault tolerances.
// maybe reuse port.
// logging system.
// health check.
// stat prefix in envoy?
// best practices, you can check:
// https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// envoy OAuth, JWT, RBAC.
// async log.
// websockets.
// dynamic protocol detection.
// unit test for utilities.
// persistent storage.
// file mapping optimization.
// memmory pool optimaization.
// fuzzy matching in router.
