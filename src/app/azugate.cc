#include "../api//config_service.hpp"
#include "auth.h"
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
#include <boost/system/detail/error_code.hpp>
#include <boost/thread.hpp>
#include <boost/thread/detail/thread.hpp>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <fmt/base.h>
#include <fmt/format.h>
#include <functional>
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
#include <vector>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

namespace azugate {

inline void
safeCloseSocket(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr) {
  if (sock_ptr && sock_ptr->is_open()) {
    boost::system::error_code ec;
    sock_ptr->shutdown(boost::asio::socket_base::shutdown_both, ec);
    sock_ptr->close(ec);
  }
}

class Server : public std::enable_shared_from_this<Server> {
public:
  Server(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
         boost::asio::ssl::context ssl_context, uint16_t port)
      : io_context_ptr_(io_context_ptr), ssl_context_(std::move(ssl_context)),
        acceptor_(*io_context_ptr, boost::asio::ip::tcp::endpoint(
                                       boost::asio::ip::tcp::v4(), port)),
        rate_limiter_(io_context_ptr) {
    rate_limiter_.Start();
  }

  void Start() { accept(); }

  void onAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr,
                boost::system::error_code ec) {
    if (ec) {
      SPDLOG_WARN("failed to accept new connection");
      safeCloseSocket(sock_ptr);
      accept();
    }
    auto source_endpoint = sock_ptr->remote_endpoint(ec);
    if (ec) {
      SPDLOG_WARN("failed to get remote_endpoint");
      safeCloseSocket(sock_ptr);
      accept();
    }
    ConnectionInfo src_conn_info;
    src_conn_info.address = source_endpoint.address().to_string();
    // TODO: async log.
    SPDLOG_DEBUG("connection from {}", src_conn_info.address);
    if (!azugate::Filter(sock_ptr, src_conn_info)) {
      SPDLOG_WARN("failed to pass filter");
      safeCloseSocket(sock_ptr);
      accept();
    }
    Dispatch(io_context_ptr_, sock_ptr, ssl_context_, std::move(src_conn_info),
             rate_limiter_, std::bind(&Server::accept, this));
    accept();
  }

  void accept() {
    boost::system::error_code ec;
    auto sock_ptr =
        boost::make_shared<boost::asio::ip::tcp::socket>(*io_context_ptr_);
    acceptor_.async_accept(
        *sock_ptr,
        std::bind(&Server::onAccept, this, sock_ptr, std::placeholders::_1));
  }

private:
  boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
  boost::asio::ssl::context ssl_context_;
  azugate::TokenBucketRateLimiter rate_limiter_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

} // namespace azugate

void ignoreSigpipe() {
  struct sigaction sa{};
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
}

int main() {
  using namespace boost::asio;
  using namespace azugate;
  // ignore SIGPIPE.
  ignoreSigpipe();

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
  } catch (...) {
    SPDLOG_ERROR("unexpected errors happen when parsing yaml file");
    return 1;
  }

  // token secret.
  g_authorization_token_secret = utils::GenerateSecret();

  // setup ssl connection.
  ssl::context ssl_context(ssl::context::sslv23_server);
  try {
    // TODO: file format.
    ssl_context.use_certificate_chain_file(std::string(g_ssl_crt));
    ssl_context.use_private_key_file(std::string(g_ssl_key), ssl::context::pem);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failed to setup ssl context: {}", e.what());
    return 1;
  }

  // setup grpc server.
  std::thread grpc_server_thread([&]() {
    grpc::ServerBuilder server_builder;
    server_builder.AddListeningPort(
        fmt::format("0.0.0.0:{}", g_azugate_admin_port),
        grpc::InsecureServerCredentials());
    ConfigServiceImpl config_service;
    server_builder.RegisterService(&config_service);
    std::unique_ptr<grpc::Server> server(server_builder.BuildAndStart());
    SPDLOG_INFO("gRPC server is listening on port {}", g_azugate_admin_port);
    server->Wait();
  });

  // setup a basic OTPL server.
  auto io_context_ptr = boost::make_shared<boost::asio::io_context>();

  SPDLOG_INFO("azugate is listening on port {}", g_azugate_port);

  // TODO: for testing purpose.
  azugate::AddRouterMapping(
      ConnectionInfo{.type = ProtocolTypeTcp, .address = "127.0.0.1"},
      ConnectionInfo{
          .type = ProtocolTypeTcp, .address = "127.0.0.1", .port = 6080});

  // setup rate limiter.

  Server s(io_context_ptr, std::move(ssl_context), g_azugate_port);
  s.Start();

  SPDLOG_INFO("server is running with {} thread(s)", g_num_threads);

  std::vector<std::thread> worker_threads;

  // invoke asynchronous tasks.
  for (size_t i = 0; i < g_num_threads; ++i) {
    worker_threads.emplace_back([io_context_ptr]() { io_context_ptr->run(); });
  }
  for (auto &t : worker_threads) {
    t.join();
  }

  SPDLOG_WARN("server exits");
  return 0;
}

// TODO:
// let's proxy websockets then.
// design fault tolerances.
// stat prefix in envoy?
// best practices, you can check:
// https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// async logging system.
// websockets.
// unit test for utilities (HTTP parser).
// persistent storage.
// file mapping optimization.
// memmory pool optimaization.
// fuzzy matching in router.
// gzip only works on file proxy.
