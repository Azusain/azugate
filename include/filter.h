#ifndef __FILTER_H
#define __FILTER_H

#include "config.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <spdlog/spdlog.h>

namespace azugate {
bool Filter(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &accepted_sock_ptr,
    azugate::ConnectionInfo &connection_info);

}

#endif