#ifndef __SERVICES_H
#define __SERVICES_H

#include "config.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <optional>

// TODO: these interfaces look pretty shity.
template <typename T>
void FileProxyHandler(boost::shared_ptr<T> &sock_ptr,
                      azugate::ConnectionInfo source_connection_info);

void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> &io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    std::optional<azugate::ConnectionInfo> target_connection_info_opt);

#include "../src/common/services.tpp"
#endif
