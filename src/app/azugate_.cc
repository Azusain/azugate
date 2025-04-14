#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstdint>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::vector<std::pair<std::string, std::string>> upstreams = {
    {"localhost", "9001"}, {"localhost", "9002"}, {"localhost", "9003"}};

std::atomic<size_t> round_robin_index{0};

void handle_connection(tcp::socket client_socket, net::io_context &ioc) {
  try {

    auto source_endpoint = client_socket.remote_endpoint();
    SPDLOG_INFO("connection from {}", source_endpoint.address().to_string());
    beast::flat_buffer buffer;
    http::request<http::string_body> client_req;
    http::read(client_socket, buffer, client_req);

    size_t idx = round_robin_index++ % upstreams.size();
    auto [host, port] = upstreams[idx];

    tcp::resolver resolver(ioc);
    auto results = resolver.resolve(host, port);
    beast::tcp_stream upstream_stream(ioc);
    upstream_stream.connect(results);

    client_req.set(http::field::host, host);

    http::write(upstream_stream, client_req);

    http::response<http::string_body> upstream_resp;
    beast::flat_buffer upstream_buf;
    http::read(upstream_stream, upstream_buf, upstream_resp);

    http::write(client_socket, upstream_resp);
    client_socket.shutdown(tcp::socket::shutdown_send);
  } catch (std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
  }
}

int main() {
  uint16_t port = 8080;
  SPDLOG_INFO("loading config from {}", "../resources/config.yaml");
  SPDLOG_INFO("gRPC server is listening on port {}", 50051);
  SPDLOG_INFO("azugate is listening on port {}", port);

  try {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port));

    for (;;) {
      tcp::socket socket(ioc);
      acceptor.accept(socket);
      handle_connection(std::move(socket), ioc);
    }
  } catch (std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
  }
}
