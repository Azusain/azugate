#ifndef __HTTP_WRAPPER_H
#define __HTTP_WRAPPER_H
#include "crequest.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

namespace azugate {
namespace network {

template <typename T>
inline bool SendHttpMessage(CRequest::HttpMessage &msg,
                            const boost::shared_ptr<T> &sock_ptr,
                            boost::system::error_code &ec) {
  using namespace boost::asio;
  sock_ptr->write_some(boost::asio::buffer(msg.StringifyFirstLine()), ec);
  if (ec) {
    SPDLOG_ERROR("failed to write the first line: {}", ec.message());
    return false;
  }
  sock_ptr->write_some(boost::asio::buffer(msg.StringifyHeaders()), ec);
  if (ec) {
    SPDLOG_ERROR("failed to write the headers: {}", ec.message());
    return false;
  }
  return true;
};

} // namespace network
} // namespace azugate
#endif