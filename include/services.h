#ifndef __SERVICES_H
#define __SERVICES_H
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/core/demangle.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <string_view>

bool FileProxy(
    const boost::shared_ptr<
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> &ssl_sock_ptr,
    const std::string_view &path_base_folder);

bool TcpProxy(const boost::shared_ptr<boost::asio::ip::tcp::socket> &sock_ptr);

#endif
