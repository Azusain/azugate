#ifndef __CORE_H
#define __CORE_H
#include <cstddef>
#include <string_view>
#include <unistd.h>

constexpr size_t kNumMaxListen = 5;
constexpr size_t kDftBufSize = 1024 * 4;
// TODO: this needs more consideration.
constexpr size_t kMaxFdSize = 1024 / 2;
// TODO: this needs some configuration file.
constexpr std::string_view kPathBaseFolder = "../resources";
constexpr std::string_view kPathDftPage = "/welcome.html";
// ref to Nginx, the value is 8kb, but 60kb in Envoy.
constexpr size_t kMaxHttpHeaderSize = 1024 * 8;
constexpr size_t kMaxHeadersNum = 20;

// init and bind socket.
int Init(int bind_port);

ssize_t Writen(int fd, const char *buf, size_t len);

int FileProxy(int fd, std::string_view path_base_folder);

#endif
