
#include "config.h"
#include "protocols.h"
#include "rate_limiter.h"
#include "services.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/bind/bind.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <spdlog/spdlog.h>
#include <utility>

namespace azugate {

void Dispatch(const boost::shared_ptr<boost::asio::io_context> &io_context_ptr,
              const boost::shared_ptr<boost::asio::ip::tcp::socket> &sock_ptr,
              boost::asio::ssl::context &ssl_context,
              ConnectionInfo &&source_connection_info,
              TokenBucketRateLimiter &rate_limiter) {
  using namespace boost::asio;
  // rate limiting.
  if (!rate_limiter.GetToken()) {
    return;
  }
  // TODO: configured by router.
  if (proxy_mode) {
    source_connection_info.type = ProtocolTypeTcp;
    boost::thread(boost::bind(TcpProxyHandler, io_context_ptr, sock_ptr,
                              GetRouterMapping(source_connection_info)));
  } else if (azugate::GetHttps()) {
    auto ssl_sock_ptr = boost::make_shared<ssl::stream<ip::tcp::socket>>(
        std::move(*sock_ptr), ssl_context);
    try {
      ssl_sock_ptr->handshake(ssl::stream_base::server);
    } catch (const std::exception &e) {
      std::string what = e.what();
      if (what.compare("handshake: ssl/tls alert certificate unknown (SSL "
                       "routines) [asio.ssl:167773206]")) {
        SPDLOG_ERROR("failed to handshake: {}", what);
        return;
      }
    }
    boost::thread(boost::bind(FileProxyHandler<ssl::stream<ip::tcp::socket>>,
                              ssl_sock_ptr, source_connection_info));
  } else {
    boost::thread(boost::bind(FileProxyHandler<ip::tcp::socket>, sock_ptr,
                              source_connection_info));
  }
}

} // namespace azugate