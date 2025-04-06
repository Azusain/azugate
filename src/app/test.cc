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
#include <vector>

// boost asio
#include <boost/asio.hpp>

const char *http_response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 2\r\n"
                            "\r\n"
                            "OK";

// 返回 100ms～500ms 的随机延时
void simulate_delay() {
  int delay_ms = 100 + rand() % 400;
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}

// 单连接一线程模型
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

// kqueue 多路复用模型
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

// Boost Asio 模型
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

// 启动主函数：只启用一个架构
int main() {
  int port = 8080;

  SingleThreadModel model(port);
  // KqueueModel model(port);
  // AsioReactorModel model(port);

  model.start();
  return 0;
}

// reactor
// azusain@Nakanos-MacBook-Air Desktop % wrk -t5 -c20 -d10s
// http://localhost:8080 Running 10s test @ http://localhost:8080
//   5 threads and 20 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     1.84ms   11.11ms 160.74ms   97.19%
//     Req/Sec     8.39k     1.76k   10.83k    86.46%
//   415251 requests in 10.01s, 26.14MB read
//   Socket errors: connect 0, read 398288, write 16960, timeout 0
// Requests/sec:  41467.87
// Transfer/sec:      2.61MB
//
// multiplexing
// azusain@Nakanos-MacBook-Air Desktop % wrk -t5 -c20 -d10s
// http://localhost:8080 Running 10s test @ http://localhost:8080
//   5 threads and 20 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     1.00s   545.08ms   1.93s    71.43%
//     Req/Sec     2.03      2.75    10.00     90.00%
//   30 requests in 10.09s, 1.93KB read
//   Socket errors: connect 0, read 54, write 0, timeout 23
// Requests/sec:      2.97
// Transfer/sec:     196.16B
//
// thread per connection
// azusain@Nakanos-MacBook-Air Desktop % wrk -t5 -c20 -d10s
// http://localhost:8080 Running 10s test @ http://localhost:8080
//   5 threads and 20 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     1.30s   446.31ms   1.76s    60.00%
//     Req/Sec     1.07      1.25     4.00     85.71%
//   28 requests in 10.09s, 1.80KB read
//   Socket errors: connect 0, read 54, write 0, timeout 23
// Requests/sec:      2.77
// Transfer/sec:     183.08B
