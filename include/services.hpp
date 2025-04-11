#ifndef __SERVICES_H
#define __SERVICES_H

#include "config.h"
#include "network_wrapper.hpp"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

template <typename T>
class HttpProxyHandler
    : public std::enable_shared_from_this<HttpProxyHandler<T>> {
public:
  HttpProxyHandler(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
                   boost::shared_ptr<T> sock_ptr,
                   azugate::ConnectionInfo source_connection_info,
                   std::function<void()> async_accpet_cb)
      : io_context_ptr_(io_context_ptr), sock_ptr_(sock_ptr),
        source_connection_info_(source_connection_info),
        async_accpet_cb_(async_accpet_cb), total_parsed_(0) {}

  // TODO: release connections properly.
  ~HttpProxyHandler() {
    sock_ptr_->shutdown(boost::asio::socket_base::shutdown_both);
    sock_ptr_->close();
  }

  void Start() { parseRequest(); }

  // used in parseRequest().
  void onRead(boost::system::error_code ec, size_t bytes_read) {
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        async_accpet_cb_();
      }
      SPDLOG_WARN("failed to read HTTP header: {}", ec.message());
      async_accpet_cb_();
    }
    total_parsed_ += bytes_read;
    request_.num_headers = std::size(request_.headers);
    int pret = phr_parse_request(
        request_.header_buf, total_parsed_, &request_.method,
        &request_.method_len, &request_.path, &request_.len_path,
        &request_.minor_version, request_.headers, &request_.num_headers, 0);
    bool valid_request =
        !(request_.method == nullptr || request_.method_len == 0 ||
          request_.path == nullptr || request_.len_path == 0 ||
          request_.num_headers < 0 ||
          request_.num_headers > azugate::kMaxHeadersNum);

    if (pret > 0 && valid_request) {
      // successful parse
      writeResponse();
    } else if (pret == -2) {
      // need more data.'
      SPDLOG_WARN("need more data");
      parseRequest();
    } else {
      SPDLOG_WARN("failed to parse HTTP request");
      async_accpet_cb_();
    }
  }

  void parseRequest() {
    if (total_parsed_ >= azugate::kMaxHttpHeaderSize) {
      SPDLOG_WARN("HTTP header size exceeded the limit");
      async_accpet_cb_();
    }
    sock_ptr_->async_read_some(
        boost::asio::buffer(request_.header_buf + total_parsed_,
                            azugate::kMaxHttpHeaderSize - total_parsed_),
        std::bind(&HttpProxyHandler<T>::onRead, this->shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
  };

  // used in onWrite().
  void onWrite(boost::system::error_code ec, size_t bytes_transferred) {
    if (ec) {
      SPDLOG_WARN("failed to write response");
    }

    async_accpet_cb_();
  }

  void writeResponse() {
    std::string response =
        "HTTP/1.1 200 OK\r\nConnection: Close\r\nContent-Length: "
        "23\r\n\r\nHello, this is azugate\n";
    boost::asio::async_write(
        *sock_ptr_, boost::asio::buffer(response),
        std::bind(&HttpProxyHandler<T>::onWrite, this->shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
  };

private:
  boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
  boost::shared_ptr<T> sock_ptr_;
  azugate::ConnectionInfo source_connection_info_;
  std::function<void()> async_accpet_cb_;
  azugate::network::PicoHttpRequest request_;
  size_t total_parsed_;
};

void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    std::optional<azugate::ConnectionInfo> target_connection_info_opt);

#include "../src/common/services.tpp"
#endif
