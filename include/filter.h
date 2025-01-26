#ifndef __FILTER_H
#define __FILTER_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <spdlog/spdlog.h>

namespace azugate {
bool Filter(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &accepted_sock_ptr);

}

#endif