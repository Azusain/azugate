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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace azugate {
namespace network {

template <typename T>
boost::shared_ptr<T>
ResolveAndConnect(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
                  const std::string &host, const std::string &port) {
  auto stream = boost::make_shared<T>(*io_context_ptr);
  boost::asio::ip::tcp::resolver resolver(*io_context_ptr);
  boost::system::error_code ec;

  auto const endpoint_iterator = resolver.resolve(host, port, ec);
  if (ec) {
    SPDLOG_DEBUG("failed to resolve host: {}", ec.message());
    return nullptr;
  }

  if constexpr (std::is_same_v<T, boost::beast::tcp_stream>) {
    stream->connect(endpoint_iterator, ec);
  } else {
    boost::asio::connect(*stream, endpoint_iterator, ec);
  }

  if (ec) {
    SPDLOG_DEBUG("failed to connect to target: {}", ec.message());
    return nullptr;
  }
  return stream;
}

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
  int minor_version;
  int status;
  const char *message = nullptr;
  size_t len_message;
  phr_header headers[kMaxHeadersNum];
  size_t num_headers;
};

// this class can be also used for establishing a tcp connection.
template <typename T> class HttpClient {
public:
  // use Connect() to establish a tcp connection if use this function.
  HttpClient() = default;

  HttpClient(boost::shared_ptr<T> sock_ptr) : sock_ptr_(sock_ptr) {}

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
      boost::system::error_code handshake_ec;
      sock_ptr_->handshake(ssl::stream_base::client, handshake_ec);
      if (handshake_ec) {
        SPDLOG_ERROR("failed to handshake: {}", handshake_ec.message());
        return false;
      }
    }
    return true;
  }

  boost::shared_ptr<T> GetSocket() const { return sock_ptr_; }

  inline bool SendHttpHeader(CRequest::HttpMessage &msg) const {
    using namespace boost::asio;
    boost::system::error_code ec;
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

private:
  boost::shared_ptr<T> sock_ptr_;
};

} // namespace network
} // namespace azugate
#endif