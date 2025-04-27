#ifndef __WORKER_H
#define __WORKER_H

#include "../src/api/config_service.hpp"
#include "config.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <grpcpp/create_channel.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>

namespace azugate {

// Health-Check service.
constexpr std::string_view kDftHealthCheckRoute = "/healthz";

inline bool healthz(const std::string &addr,
                    boost::shared_ptr<boost::asio::io_context> io_context_ptr) {
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace net = boost::asio;
  using tcp = net::ip::tcp;
  boost::system::error_code ec;

  auto pos = addr.find(':');
  if (pos == std::string::npos) {
    SPDLOG_DEBUG("invalid address format: {}", addr);
    return false;
  }

  // resolve and connect to host;
  std::string host = addr.substr(0, pos);
  std::string port = addr.substr(pos + 1);
  beast::tcp_stream stream(*io_context_ptr);
  tcp::resolver resolver(*io_context_ptr);
  auto const results = resolver.resolve(host, port, ec);
  if (ec) {
    SPDLOG_DEBUG("failed to resolve host: {}", ec.message());
    return false;
  }
  stream.connect(results, ec);
  if (ec) {
    SPDLOG_DEBUG("failed to connect to target: {}", ec.message());
    return false;
  }
  // send health check request.
  http::request<http::string_body> req{http::verb::get, kDftHealthCheckRoute,
                                       11};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, AZUGATE_VERSION_STRING);
  http::write(stream, req, ec);
  if (ec) {
    SPDLOG_DEBUG("failed to send health check request: {}", ec.message());
    return false;
  }
  // get response from target.
  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res, ec);
  if (ec) {
    SPDLOG_DEBUG("failed to get response from target");
    return false;
  }
  if (res.result() != http::status::ok) {
    SPDLOG_DEBUG("Health check failed for {}: status {}", addr,
                 res.result_int());
    return false;
  }
  // close connection.
  auto _ = stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (ec && ec != beast::errc::not_connected) {
    SPDLOG_DEBUG(ec.message());
    return false;
  }
  return true;
}

inline void StartHealthCheckWorker(
    boost::shared_ptr<boost::asio::io_context> io_context_ptr) {
  std::thread healthz_thread([&]() {
    SPDLOG_INFO("Health check will be performed every {} seconds",
                kDftHealthCheckGapSecond);
    for (;;) {
      for (auto &addr : azugate::GetHealthzList()) {
        if (!healthz(addr, io_context_ptr)) {
          SPDLOG_WARN("Health check error for {}", addr);
        }
      }
      std::this_thread::sleep_for(
          std::chrono::seconds(kDftHealthCheckGapSecond));
    }
  });
  healthz_thread.detach();
}

// gRPC Worker.
inline void StartGrpcWorker() {
  // grpc server worker thread.
  std::thread grpc_server_thread([&]() {
    grpc::ServerBuilder server_builder;
    // TODO: The server should listen only on the local address by default.
    server_builder.AddListeningPort(
        fmt::format("0.0.0.0:{}", g_azugate_admin_port),
        grpc::InsecureServerCredentials());
    ConfigServiceImpl config_service;
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    server_builder.RegisterService(&config_service);
    std::unique_ptr<grpc::Server> server(server_builder.BuildAndStart());
    SPDLOG_INFO("gRPC server is listening on port {}", g_azugate_admin_port);
    server->Wait();
  });
  grpc_server_thread.detach();
}

} // namespace azugate

#endif