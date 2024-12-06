#include "core.h"
#include <boost/asio.hpp>
#include <boost/system.hpp>
#include <boost/thread.hpp>
#include <cstdint>
#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <string>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

using namespace boost::asio;

uint16_t port = 80;

void handler(boost::shared_ptr<ip::tcp::socket> sock_ptr) {
  try {
    int fd = sock_ptr->native_handle();
    if (FileProxy(fd, kPathBaseFolder) == -1) {
      SPDLOG_ERROR("failed to handler file request");
      return;
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failedt to get raw fd, {}", e.what());
    return;
  }
}

int main() {
  // read configuration files.
  auto config =
      YAML::LoadFile(fmt::format("{}/{}", kPathBaseFolder, "config.yaml"));
  port = config["port"].as<uint16_t>();
  // setup logger.
  // ref: https://github.com/gabime/spdlog/wiki/3.-Custom-formatting.
  // for production, use this logger:
  // spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %v");
  // with source file and line when debug:
  spdlog::set_pattern("[%^%l%$] %t | %D %H:%M:%S | %s:%# | %v");
  spdlog::set_level(spdlog::level::debug);

  // setup a basic OTPL server.
  io_service service;
  ip::tcp::endpoint local_address(ip::tcp::v4(), port);
  ip::tcp::acceptor acc(service, local_address);
  SPDLOG_INFO("server runs on port {}", port);
  for (;;) {
    boost::shared_ptr<ip::tcp::socket> sock_ptr(new ip::tcp::socket(service));
    acc.accept(*sock_ptr);
    boost::thread(boost::bind(handler, sock_ptr));
  }

  return 0;
}
