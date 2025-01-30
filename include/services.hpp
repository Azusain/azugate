#ifndef __SERVICES_H
#define __SERVICES_H

#include "config.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

template <typename T> void FileProxyHandler(boost::shared_ptr<T> &sock_ptr);

bool TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> &io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    azugate::ConnectionInfo target_connection_info);

#include "../src/common/services.tpp"
#endif
