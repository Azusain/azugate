#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::atomic<int> success_count{0};
std::atomic<int> failure_count{0};

void worker(const std::string &host, const std::string &port, int reqs) {
  try {
    net::io_context ioc;
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, port);

    for (int i = 0; i < reqs; ++i) {
      beast::tcp_stream stream{ioc};
      stream.connect(results);

      http::request<http::string_body> req{http::verb::get, "/", 11};
      req.set(http::field::host, host);
      req.set(http::field::user_agent, "perf_http_client");

      http::write(stream, req);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;
      http::read(stream, buffer, res);

      if (res.result() == http::status::ok)
        success_count++;
      else
        failure_count++;

      beast::error_code ec;
      stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }
  } catch (...) {
    failure_count++;
  }
}

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0]
              << " <host> <port> <threads> <requests>\n";
    return 1;
  }

  std::string host = argv[1];
  std::string port = argv[2];
  int threads = std::stoi(argv[3]);
  int total_requests = std::stoi(argv[4]);
  int per_thread = total_requests / threads;

  std::vector<std::thread> workers;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < threads; ++i) {
    workers.emplace_back(worker, host, port, per_thread);
  }

  for (auto &t : workers)
    t.join();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  std::cout << "Time elapsed: " << elapsed.count() << "s\n";
  std::cout << "Successful requests: " << success_count << "\n";
  std::cout << "Failed requests: " << failure_count << "\n";
  std::cout << "Requests/sec: " << (success_count.load() / elapsed.count())
            << "\n";

  return 0;
}