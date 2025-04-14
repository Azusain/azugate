
#include "../../include/config.h"
#include "rate_limiter.h"
#include "services.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/bind/bind.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/thread.hpp>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>
using namespace boost::asio;

namespace azugate {

inline boost::shared_ptr<ssl::stream<ip::tcp::socket>>
sslHandshake(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr) {
  boost::system::error_code ec;
  // setup ssl connection.
  ssl::context ssl_context(ssl::context::sslv23_server);
  auto _ = ssl_context.use_certificate_chain_file(std::string(g_ssl_crt), ec);
  if (ec) {
    SPDLOG_ERROR("{}", ec.message());
    return nullptr;
  }
  _ = ssl_context.use_private_key_file(std::string(g_ssl_key),
                                       ssl::context::pem, ec);
  if (ec) {
    SPDLOG_ERROR("{}", ec.message());
    return nullptr;
  }
  auto ssl_sock_ptr = boost::make_shared<ssl::stream<ip::tcp::socket>>(
      std::move(*sock_ptr), ssl_context);
  try {
    ssl_sock_ptr->handshake(ssl::stream_base::server);
  } catch (const std::exception &e) {
    std::string what = e.what();
    if (what.compare("handshake: ssl/tls alert certificate unknown (SSL "
                     "routines) [asio.ssl:167773206]")) {
      SPDLOG_WARN("failed to handshake: {}", what);
      return nullptr;
    }
  }
  return ssl_sock_ptr;
}

// TODO: potential concurrency problem using single ssl_context.
void Dispatch(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
              boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr,
              ConnectionInfo &&source_connection_info,
              TokenBucketRateLimiter &rate_limiter,
              std::function<void()> callback) {

  // rate limiting.
  if (g_enable_rate_limiter && !rate_limiter.GetToken()) {
    SPDLOG_WARN("request rejected by rate limiter");
    callback();
    return;
  }
  // TODO: configured by router.
  // TCP.
  // if (g_proxy_mode) {
  //   source_connection_info.type = ProtocolTypeTcp;
  //   TcpProxyHandler(io_context_ptr, sock_ptr,
  //                   GetRouterMapping(source_connection_info));
  // }

  // HTTPS.
  if (azugate::GetHttps()) {
    auto ssl_sock_ptr = sslHandshake(sock_ptr);
    if (!ssl_sock_ptr) {
      SPDLOG_WARN("failed to do ssl handshake");
      callback();
      return;
    }
    auto https_handler =
        std::make_shared<HttpProxyHandler<ssl::stream<ip::tcp::socket>>>(
            io_context_ptr, ssl_sock_ptr, source_connection_info, callback);
    https_handler->Start();
    callback();
    // WARN: remember to return after every callback.
    return;
  }
  // HTTP.
  auto http_handler = std::make_shared<HttpProxyHandler<ip::tcp::socket>>(
      io_context_ptr, sock_ptr, source_connection_info, callback);
  http_handler->Start();
  callback();
  return;
}

} // namespace azugate