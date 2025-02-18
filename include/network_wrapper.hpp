// Due to performance considerations, I have to use both the Pico HTTP library
// and my own HTTP class simultaneously. Maybe one day I will implement my own
// network library.(if needed) :XD
#ifndef __HTTP_WRAPPER_H
#define __HTTP_WRAPPER_H
#include "config.h"
#include "crequest.h"
#include "picohttpparser.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace azugate {
namespace network {

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

struct PicoHttpResponse {
  char header_buf[kMaxHttpHeaderSize];
  const char *status_code = nullptr;
  const char *status_message = nullptr;
  size_t status_message_len;
  int *status;
  int minor_version;
  phr_header headers[kMaxHeadersNum];
  size_t num_headers;
};

// this class can be also used for establishing a tcp connection.
template <typename T> class HttpClient {
public:
  // use Connect() to establish a tcp connection if use this function.
  HttpClient() = default;

  HttpClient(boost::shared_ptr<T> &&sock_ptr) : sock_ptr_(sock_ptr) {}

  inline bool Connect(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
                      const std::string &host, const std::string &port) {
    using namespace boost::asio;
    boost::system::error_code ec;

    ip::tcp::resolver resolver(*io_context_ptr);
    ip::tcp::resolver::query query(host, port);
    auto endpoint_iterator = resolver.resolve(query, ec);
    if (ec) {
      SPDLOG_WARN("failed to resolve domain: {}", ec.message());
      return false;
    }
    // connect to target.
    auto tcp_sock_ptr = boost::make_shared<ip::tcp::socket>(*io_context_ptr);
    boost::asio::connect(*tcp_sock_ptr, endpoint_iterator, ec);
    if (ec) {
      SPDLOG_WARN("failed to connect to target: {}", ec.message());
      return false;
    }
    // ssl client.
    if constexpr (std::is_same<T, ssl::stream<ip::tcp::socket>>::value) {
      ssl::context ssl_client_ctx(ssl::context::sslv23_client);
      sock_ptr_ = boost::make_shared<ssl::stream<ip::tcp::socket>>(
          std::move(*tcp_sock_ptr), ssl_client_ctx);
      try {
        sock_ptr_->handshake(ssl::stream_base::client);
      } catch (const std::exception &e) {
        SPDLOG_ERROR("failed to handshake: {}", e.what());
        return false;
      } catch (...) {
        SPDLOG_WARN("failedt to handshake due to unknown reason");
        return false;
      }
    }
    return true;
  }

  boost::shared_ptr<T> GetSocket() const { return sock_ptr_; }

  inline bool SendHttpMessage(CRequest::HttpMessage &msg,

                              boost::system::error_code &ec) {
    using namespace boost::asio;
    sock_ptr_->write_some(boost::asio::buffer(msg.StringifyFirstLine()), ec);
    if (ec) {
      SPDLOG_ERROR("failed to write the first line: {}", ec.message());
      return false;
    }
    sock_ptr_->write_some(boost::asio::buffer(msg.StringifyHeaders()), ec);
    if (ec) {
      SPDLOG_ERROR("failed to write the headers: {}", ec.message());
      return false;
    }
    return true;
  };

  inline bool GetHttpHeader(PicoHttpRequest &header,
                            boost::system::error_code &ec) {
    using namespace boost::asio;
    size_t total_parsed = 0;

    for (;;) {
      if (total_parsed >= kMaxHttpHeaderSize) {
        SPDLOG_WARN("HTTP header size exceeded the limit");
        return false;
      }
      size_t bytes_read =
          sock_ptr_->read_some(buffer(header.header_buf + total_parsed,
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

  inline bool GetHttpResponse(PicoHttpResponse &response,
                              boost::system::error_code &ec) {
    using namespace boost::asio;
    size_t total_parsed = 0;

    for (;;) {
      if (total_parsed >= kMaxHttpHeaderSize) {
        SPDLOG_WARN("HTTP header size exceeded the limit");
        return false;
      }

      // 读取数据
      size_t bytes_read =
          sock_ptr_->read_some(buffer(response.header_buf + total_parsed,
                                      kMaxHttpHeaderSize - total_parsed),
                               ec);
      if (ec) {
        if (ec == boost::asio::error::eof) {
          SPDLOG_DEBUG("connection closed by peer");
          break;
        }
        SPDLOG_WARN("failed to read HTTP response: {}", ec.message());
        return false;
      }

      total_parsed += bytes_read;
      response.num_headers = std::size(response.headers);
      // TODO: last update here.................
      int pret = phr_parse_response(
          response.header_buf, total_parsed, &response.status_code,
          &response.status_code_len, &response.status_message,
          &response.status_message_len, &response.minor_version,
          response.headers, &response.num_headers, 0);

      if (pret > 0) {
        break;
      } else if (pret == -2) {
        continue;
      } else {
        SPDLOG_WARN("failed to parse HTTP response");
        return false;
      }
    }

    return true;
  }

private:
  boost::shared_ptr<T> sock_ptr_;
};

} // namespace network
} // namespace azugate
#endif