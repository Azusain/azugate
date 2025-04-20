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
#include <boost/beast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/thread.hpp>
#include <boost/thread/detail/thread.hpp>
#include <chrono>
#include <cstddef>
#include <cstdlib>

#include <fmt/base.h>
#include <fmt/format.h>
#include <functional>
#include <grpcpp/create_channel.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/types.h>

#include <thread>
#include <utility>
#include <vector>

namespace azugate {

inline void
safeCloseSocket(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr) {
  if (sock_ptr && sock_ptr->is_open()) {
    boost::system::error_code ec;
    auto _ = sock_ptr->shutdown(boost::asio::socket_base::shutdown_both, ec);
    _ = sock_ptr->close(ec);
  }
}

class Server : public std::enable_shared_from_this<Server> {
public:
  Server(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
         uint16_t port)
      : io_context_ptr_(io_context_ptr),
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
      return;
    }
    auto source_endpoint = sock_ptr->remote_endpoint(ec);
    if (ec) {
      SPDLOG_WARN("failed to get remote_endpoint");
      safeCloseSocket(sock_ptr);
      accept();
      return;
    }
    ConnectionInfo src_conn_info;
    src_conn_info.address = source_endpoint.address().to_string();
    // TODO: support async log, this is really slow...slow...slow...
    SPDLOG_INFO("connection from {}", src_conn_info.address);
    if (!azugate::Filter(sock_ptr, src_conn_info)) {
      safeCloseSocket(sock_ptr);
      accept();
      return;
    }
    Dispatch(io_context_ptr_, sock_ptr, std::move(src_conn_info), rate_limiter_,
             std::bind(&Server::accept, this));
    accept();
    return;
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
  azugate::TokenBucketRateLimiter rate_limiter_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

} // namespace azugate

void ignoreSigpipe() {
#if defined(__linux__)
  struct sigaction sa{};
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
#endif
}

// TODO: exception is inefficient.
bool healthz(const std::string &addr) {
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace net = boost::asio;
  using tcp = net::ip::tcp;
  try {
    auto pos = addr.find(':');
    if (pos == std::string::npos) {
      SPDLOG_WARN("Invalid address format: {}", addr);
      return false;
    }
    std::string host = addr.substr(0, pos);
    std::string port = addr.substr(pos + 1);
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);
    auto const results = resolver.resolve(host, port);
    stream.connect(results);
    http::request<http::string_body> req{http::verb::get, "/healthz", 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    http::write(stream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);
    if (res.result() != http::status::ok) {
      SPDLOG_WARN("Health check failed for {}: status {}", addr,
                  res.result_int());
      return false;
    }
    beast::error_code ec;
    auto _ = stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (ec && ec != beast::errc::not_connected) {
      SPDLOG_WARN(ec.message());
      return false;
    }
  } catch (const std::exception &e) {
    // TODO: log something.
    // SPDLOG_WARN(e.what())
    return false;
  }
  return true;
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

  if (!LoadServerConfig()) {
    return -1;
  }
  // TODO: default router.
  AddRoute(ConnectionInfo{.type = ProtocolTypeHttp, .http_url = "/*"},
           ConnectionInfo{
               .type = ProtocolTypeHttp,
               .address = "www.baidu.com",
               .port = 80,
               .http_url = "/",
               .remote = true,
           });

  // setup grpc server.
  std::thread grpc_server_thread([&]() {
    grpc::ServerBuilder server_builder;
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

  std::thread healthz_thread([&]() {
    int healthz_gap_sec = 3;
    SPDLOG_INFO("Health check will be performed every {} seconds",
                healthz_gap_sec);
    for (;;) {
      for (auto &addr : azugate::GetHealthzList()) {
        if (!healthz(addr)) {
          SPDLOG_WARN("Health check error for {}", addr);
        }
      }
      std::this_thread::sleep_for(std::chrono::seconds(healthz_gap_sec));
    }
  });

  auto io_context_ptr = boost::make_shared<boost::asio::io_context>();
  Server s(io_context_ptr, g_azugate_port);
  SPDLOG_INFO("azugate is listening on port {}", g_azugate_port);
  s.Start();
  SPDLOG_INFO("server is running with {} thread(s)", g_num_threads);

  // invoke asynchronous tasks.
  std::vector<std::thread> worker_threads;
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
// best practices, you can check:
// https://www.envoyproxy.io/docs/envoy/latest/start/sandboxes.
// async logging system.
// websockets.
// unit test for utilities (HTTP parser).
// persistent storage.
// memmory pool optimaization.
// fuzzy matching in router.
