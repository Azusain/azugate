#ifndef __CORE_H
#define __CORE_H
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/core/demangle.hpp>
#include <cstddef>
#include <string_view>

constexpr size_t kNumMaxListen = 5;
constexpr size_t kDftBufSize = 1024 * 4;
// TODO: this needs more consideration.
constexpr size_t kMaxFdSize = 1024 / 2;
// TODO: this needs some configuration file.
constexpr std::string_view kPathResourceFolder = "../resources";
constexpr std::string_view kPathDftPage = "/welcome.html";
// ref to Nginx, the value is 8kb, but 60kb in Envoy.
constexpr size_t kMaxHttpHeaderSize = 1024 * 8;
constexpr size_t kMaxHeadersNum = 20;

bool FileProxy(
    const boost::shared_ptr<
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> &ssl_sock_ptr,
    const std::string_view &path_base_folder);

#endif
