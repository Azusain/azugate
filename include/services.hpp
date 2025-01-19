#ifndef __SERVICES_H
#define __SERVICES_H

#include <boost/asio.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

template <typename T> void FileProxyHandler(boost::shared_ptr<T> &sock_ptr);

bool TcpProxy(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &target_sock_ptr);

#include "../src/common/services.tpp"
#endif
