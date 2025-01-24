
#include "config.h"
#include "services.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/bind/bind.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace azugate {

void Dispatch(const boost::shared_ptr<boost::asio::ip::tcp::socket> &sock_ptr,
              boost::asio::ssl::context &ssl_context) {
  using namespace boost::asio;
  // TODO: dynamic protocol detector.
  if (azugate::GetHttps()) {
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
                              ssl_sock_ptr));

  } else {
    boost::thread(boost::bind(FileProxyHandler<ip::tcp::socket>, sock_ptr));
  }
}

} // namespace azugate