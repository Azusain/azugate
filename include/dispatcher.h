#ifndef __DISPATCHER_H
#define __DISPATCHER_H
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/shared_ptr.hpp>

namespace azugate {
void Dispatch(const boost::shared_ptr<boost::asio::ip::tcp::socket> &sock_ptr,
              boost::asio::ssl::context &ssl_context);
}

#endif