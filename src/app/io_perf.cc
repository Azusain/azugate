#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

// boost asio
#include <boost/asio.hpp>

const char *http_response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 2\r\n"
                            "\r\n"
                            "OK";

void simulate_delay() {
  int delay_ms = 100 + rand() % 400;
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}

class SingleThreadModel {
public:
  SingleThreadModel(int port) : port(port) { srand(time(nullptr)); }

  void start() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(listen_fd, (sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 10);
    std::cout << "SingleThreadModel on port " << port << std::endl;

    while (true) {
      int client_fd = accept(listen_fd, nullptr, nullptr);
      if (client_fd == -1)
        continue;

      simulate_delay();
      write(client_fd, http_response, strlen(http_response));
      close(client_fd);
    }
  }

private:
  int port;
};

class KqueueModel {
public:
  KqueueModel(int port) : port(port) { srand(time(nullptr)); }

  void start() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listen_fd, (sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 10);

    int kq = kqueue();
    struct kevent change;
    EV_SET(&change, listen_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    kevent(kq, &change, 1, nullptr, 0, nullptr);

    std::cout << "KqueueModel on port " << port << std::endl;

    while (true) {
      struct kevent event;
      if (kevent(kq, nullptr, 0, &event, 1, nullptr) <= 0)
        continue;

      if (event.ident == listen_fd) {
        int client_fd = accept(listen_fd, nullptr, nullptr);
        EV_SET(&change, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
               nullptr);
        kevent(kq, &change, 1, nullptr, 0, nullptr);
      } else {
        simulate_delay();
        write(event.ident, http_response, strlen(http_response));
        close(event.ident);
      }
    }
  }

private:
  int port;
};

class AsioReactorModel {
public:
  AsioReactorModel(int port) : port(port), io_context(), acceptor(io_context) {
    srand(time(nullptr));
  }

  void start() {
    using namespace boost::asio;

    ip::tcp::endpoint endpoint(ip::tcp::v4(), port);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen();

    std::cout << "AsioReactorModel on port " << port << std::endl;
    accept();
    io_context.run();
  }

private:
  void accept() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
    acceptor.async_accept(*socket, [this, socket](auto ec) {
      if (!ec) {
        auto delay = std::make_shared<boost::asio::steady_timer>(io_context);
        int delay_ms = 100 + rand() % 400;
        delay->expires_after(std::chrono::milliseconds(delay_ms));
        delay->async_wait([this, socket](auto) {
          auto buf = std::make_shared<std::string>(http_response);
          boost::asio::async_write(
              *socket, boost::asio::buffer(*buf),
              [socket, buf](auto, std::size_t) { socket->close(); });
        });
      }
      accept();
    });
  }

  int port;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::acceptor acceptor;
};

int main() {
  int port = 8080;

  // SingleThreadModel model(port);
  // KqueueModel model(port);
  AsioReactorModel model(port);

  model.start();
  return 0;
}

// thread per connection.
// Running 30s test @ http://localhost:8080
//   5 threads and 20 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     1.26s   479.76ms   1.82s    60.00%
//     Req/Sec     1.57      2.17    10.00     84.69%
//   98 requests in 30.09s, 6.32KB read
//   Socket errors: connect 0, read 140, write 0, timeout 93
// Requests/sec:      3.26
// Transfer/sec:     214.93B

// kqueue
// Running 30s test @ http://localhost:8080
//   5 threads and 20 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     1.18s   564.72ms   1.95s    66.67%
//     Req/Sec     1.44      2.20     9.00     87.25%
//   102 requests in 30.09s, 6.57KB read
//   Socket errors: connect 0, read 147, write 3, timeout 96
// Requests/sec:      3.39
// Transfer/sec:     223.70B

// reactor
// Running 30s test @ http://localhost:8080
// 5 threads and 20 connections
// Thread Stats   Avg      Stdev     Max   +/- Stdev
//   Latency     2.33ms   13.66ms 167.47ms   96.97%
//   Req/Sec     8.22k     1.79k   10.41k    82.17%
// 1215260 requests in 30.20s, 76.49MB read
// Socket errors: connect 0, read 1164992, write 50266, timeout 0
// Requests/sec:  40234.23
// Transfer/sec:      2.53MB
