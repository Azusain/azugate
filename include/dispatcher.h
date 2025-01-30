#ifndef __DISPATCHER_H
#define __DISPATCHER_H
#include "config.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/shared_ptr.hpp>

namespace azugate {
void Dispatch(const boost::shared_ptr<boost::asio::io_context> &io_context_ptr,
              const boost::shared_ptr<boost::asio::ip::tcp::socket> &sock_ptr,
              boost::asio::ssl::context &ssl_context,
              ConnectionInfo source_connection_info);
}
#endif