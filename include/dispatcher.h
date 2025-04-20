#ifndef __DISPATCHER_H
#define __DISPATCHER_H
#include "config.h"
#include "rate_limiter.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/shared_ptr.hpp>

namespace azugate {
boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
SslHandshake(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr);

void Dispatch(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
              boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr,
              ConnectionInfo &&source_connection_info,
              TokenBucketRateLimiter &rate_limiter,
              std::function<void()> callback);
} // namespace azugate
#endif