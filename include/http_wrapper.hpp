#ifndef __HTTP_WRAPPER_H
#define __HTTP_WRAPPER_H
#include "config.h"
#include "crequest.h"
#include "picohttpparser.h"
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

struct PicoHttpRequest {
  char header_buf[kMaxHttpHeaderSize];
  const char *path = nullptr;
  const char *method = nullptr;
  size_t method_len;
  size_t len_path;
  int minor_version;
  phr_header headers[kMaxHeadersNum];
  size_t num_headers;
};

template <typename T>
inline bool GetHttpHeader(PicoHttpRequest &header,
                          boost::system::error_code &ec,
                          const boost::shared_ptr<T> &sock_ptr) {
  using namespace boost::asio;
  size_t total_parsed = 0;

  for (;;) {
    if (total_parsed >= kMaxHttpHeaderSize) {
      SPDLOG_WARN("HTTP header size exceeded the limit");
      return false;
    }
    size_t bytes_read =
        sock_ptr->read_some(buffer(header.header_buf + total_parsed,
                                   kMaxHttpHeaderSize - total_parsed),
                            ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        break;
      }
      SPDLOG_WARN("failed to read HTTP header: {}", ec.message());
      return false;
    }

    total_parsed += bytes_read;
    header.num_headers = std::size(header.headers);
    int pret = phr_parse_request(
        header.header_buf, total_parsed, &header.method, &header.method_len,
        &header.path, &header.len_path, &header.minor_version, header.headers,
        &header.num_headers, 0);
    if (pret > 0) {
      // successful parse
      break;
    } else if (pret == -2) {
      // need more data.
      continue;
    } else {
      SPDLOG_WARN("failed to parse HTTP request");
      return false;
    }
  }
  return true;
}

} // namespace network
} // namespace azugate
#endif