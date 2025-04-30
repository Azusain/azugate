
#ifndef __SERVER_H
#define __SERVER_H

#include "config.h"
#include "dispatcher.h"
#include "filter.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <spdlog/spdlog.h>

#include "rate_limiter.h"

namespace azugate {

inline void
safeCloseSocket(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr) {
  if (sock_ptr && sock_ptr->is_open()) {
    boost::system::error_code ec;
    auto _ = sock_ptr->shutdown(boost::asio::socket_base::shutdown_both, ec);
    _ = sock_ptr->close(ec);
  }
}

class Server : public std::enable_shared_from_this<Server> {
public:
  Server(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
         uint16_t port)
      : io_context_ptr_(io_context_ptr),
        acceptor_(*io_context_ptr, boost::asio::ip::tcp::endpoint(
                                       boost::asio::ip::tcp::v4(), port)),
        rate_limiter_(io_context_ptr) {
    rate_limiter_.Start();
  }

  void Run(boost::shared_ptr<boost::asio::io_context> io_context_ptr) {
    accept();
    // run the server with multiple worker threads.
    SPDLOG_INFO("server is running with {} thread(s)", g_num_threads);
    std::vector<std::thread> worker_threads;
    for (size_t i = 0; i < g_num_threads; ++i) {
      worker_threads.emplace_back(
          [io_context_ptr]() { io_context_ptr->run(); });
    }
    for (auto &t : worker_threads) {
      t.join();
      SPDLOG_ERROR("worker thread exits");
    }
  }

  void onAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr,
                boost::system::error_code ec) {
    if (ec) {
      SPDLOG_WARN("failed to accept new connection");
      safeCloseSocket(sock_ptr);
      accept();
      return;
    }
    auto source_endpoint = sock_ptr->remote_endpoint(ec);
    if (ec) {
      SPDLOG_WARN("failed to get remote endpoint");
      safeCloseSocket(sock_ptr);
      accept();
      return;
    }
    ConnectionInfo src_conn_info;
    src_conn_info.address = source_endpoint.address().to_string();
    // TODO: support async log, this is really slow...slow...slow...
    SPDLOG_INFO("connection from {}", src_conn_info.address);
    if (!azugate::Filter(sock_ptr, src_conn_info)) {
      safeCloseSocket(sock_ptr);
      accept();
      return;
    }
    Dispatch(io_context_ptr_, sock_ptr, std::move(src_conn_info), rate_limiter_,
             std::bind(&Server::accept, this));
    accept();
    return;
  }

  void accept() {
    boost::system::error_code ec;
    auto sock_ptr =
        boost::make_shared<boost::asio::ip::tcp::socket>(*io_context_ptr_);
    acceptor_.async_accept(
        *sock_ptr,
        std::bind(&Server::onAccept, this, sock_ptr, std::placeholders::_1));
  }

private:
  boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
  azugate::TokenBucketRateLimiter rate_limiter_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

} // namespace azugate

#endif