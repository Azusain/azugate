#include "core.h"
#include "crequest.h"
#include "picohttpparser.h"
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int Init(int bind_port) {
  // main.
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    SPDLOG_ERROR("error happens when creating socket");
    return -1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(bind_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  int ret = bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  if (ret == -1) {
    SPDLOG_ERROR("failed to bind port {}", bind_port);
    return -1;
  }

  ret = listen(listen_fd, kNumMaxListen);
  if (ret == -1) {
    SPDLOG_ERROR("failed to listen");
    return -1;
  }
  SPDLOG_INFO("server runs on port {}", bind_port);

  return listen_fd;
}

// return -1 if err, else return the bytes written.
ssize_t Writen(int fd, const char *buf, size_t len) {
  ssize_t n_write = 0;
  ssize_t n_real_write = 0;
  while (n_write < len) {
    n_real_write = write(fd, buf + n_write, len);
    if (n_real_write == -1) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    n_write += n_real_write;
  }
  return n_write;
}

int Echo(const int fd) {
  char buf[kDftBufSize];
  ssize_t n_read = 0;
  while (true) {
    // read.
    n_read = read(fd, buf, kDftBufSize);
    if (n_read <= 0) {
      if (errno == EINTR) {
        continue;
      }
      close(fd);
      SPDLOG_DEBUG("connection closed");
      return 0;
    }
    // write.
    if (Writen(fd, buf, n_read) == -1) {
      SPDLOG_ERROR("failed to write {} bytes", n_read);
      return -1;
    }
  }
}

struct FdCloser {
  void operator()(const int *fd) const {
    if (fd && *fd > 0) {
      close(*fd);
    }
  }
};

// return -1 if err, 0 if successful;
int FileProxy(int sock_fd, std::string_view path_base_folder) {
  std::unique_ptr<int, FdCloser> sock_fd_(&sock_fd);
  ssize_t n_read = 0;
  // TODO: extract path from http data.
  char header_buf[kMaxHttpHeaderSize];
  const char *path = nullptr;
  const char *method = nullptr;
  size_t method_len;
  size_t len_path;
  size_t total_parsed = 0;
  int minor_version;
  phr_header headers[kMaxHeadersNum];
  size_t num_headers;

  for (;;) {
    if (total_parsed >= kMaxHttpHeaderSize) {
      SPDLOG_WARN("the size of the header has exceeded the limit");
      return -1;
    }

    if (fcntl(sock_fd, F_GETFD) == -1) {
      SPDLOG_WARN("invalid descriptor");
      return -1;
    }

    // read from client.
    n_read = read(sock_fd, header_buf + total_parsed,
                  kMaxHttpHeaderSize - total_parsed);
    if (n_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      // maybe RST
      SPDLOG_WARN(strerror(errno));
      return -1;
      // FIN
    } else if (n_read == 0) {
      SPDLOG_DEBUG("connection closed");
      break;
    }

    // TODO: this line is super weird.
    num_headers = std::size(headers);

    // parse path from http request.
    // TODO:  method unchecked.
    int pret = phr_parse_request(header_buf, total_parsed + n_read, &method,
                                 &method_len, &path, &len_path, &minor_version,
                                 headers, &num_headers, total_parsed);
    if (pret > 0) {
      break;
    } else if (pret == -2) {
      total_parsed += n_read;
      continue;
    }
    SPDLOG_WARN("unexpected error while parsing http message");
    return -1;
  }

  // route to default page.
  if (len_path == 1 && path[0] == '/') {
    path = kPathDftPage.data();
    len_path = kPathDftPage.length();
  }

  // assemble the full path.
  const char *base_folder = path_base_folder.data();
  const size_t len_base_folder = path_base_folder.length();
  size_t len_full_path = len_base_folder + len_path + 1;
  std::unique_ptr<char[]> full_path(new char[len_full_path]);
  std::memcpy(full_path.get(), base_folder, len_base_folder);
  std::memcpy(full_path.get() + len_base_folder, path, len_path);
  std::memcpy(full_path.get() + len_base_folder + len_path, "\0", 1);

  // read files from disk.
  int resource_fd = open(full_path.get(), O_RDONLY);
  if (resource_fd == -1) {
    SPDLOG_ERROR("failed to open file {}", full_path.get());
    return -1;
  }
  std::unique_ptr<int, FdCloser> _resource_fd(&resource_fd);

  // get the size of the file.
  // TODO: initialization?
  struct stat file_stat {};
  if (fstat(resource_fd, &file_stat) == -1) {
    SPDLOG_ERROR("failed to get file stat");
    return -1;
  };

  CRequest::HttpResponse resp(CRequest::kHttpOk);
  resp.SetContentLen(file_stat.st_size);
  // TODO: this only supports html temporarily.
  resp.SetContentType(CRequest::kTypeTextHtml);
  resp.SetKeepAlive(false);
  if (!resp.SendHeader(sock_fd)) {
    SPDLOG_ERROR("failed to send header");
    return -1;
  }

  // reuse the header buffer.
  memset(header_buf, 0x0, sizeof(header_buf));
  for (;;) {
    size_t n_resource_read = read(resource_fd, header_buf, sizeof(header_buf));
    if (n_resource_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to read from {}", full_path.get());
      return -1;
    } else if (n_resource_read == 0) {
      break;
    }

    if (Writen(sock_fd, header_buf, n_resource_read) == -1) {
      SPDLOG_ERROR("failed to write {} bytes to client", n_resource_read);
      return -1;
    }
  }

  return 0;
}

// TODO: let's proxy websockets then.
// TODO: logger level.
// TODO: design fault tolerances.
// TODO: crush if being frequently requested?
// TODO: maybe reuse port.
// TODO: config file?