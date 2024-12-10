#include "core.h"
#include "crequest.h"
#include "picohttpparser.h"
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/system_error.hpp>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <fmt/format.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

// return false if err, true if successful
bool FileProxy(
    const boost::shared_ptr<
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> &ssl_sock_ptr,
    const std::string_view &path_base_folder) {
  using namespace boost::asio;
  char header_buf[kMaxHttpHeaderSize];
  const char *path = nullptr;
  const char *method = nullptr;
  size_t method_len;
  size_t len_path;
  size_t total_parsed = 0;
  int minor_version;
  phr_header headers[kMaxHeadersNum];
  size_t num_headers;
  boost::system::error_code ec;

  // Read and parse HTTP header
  for (;;) {
    if (total_parsed >= kMaxHttpHeaderSize) {
      SPDLOG_WARN("HTTP header size exceeded the limit");
      return false;
    }
    size_t bytes_read = ssl_sock_ptr->read_some(
        buffer(header_buf + total_parsed, kMaxHttpHeaderSize - total_parsed),
        ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        break;
      }
      SPDLOG_WARN("failed to read HTTP header: {}", ec.message());
      return false;
    }

    total_parsed += bytes_read;
    num_headers = std::size(headers);
    int pret =
        phr_parse_request(header_buf, total_parsed, &method, &method_len, &path,
                          &len_path, &minor_version, headers, &num_headers, 0);
    if (pret > 0) {
      break; // Successful parse
    } else if (pret == -2) {
      continue; // Need more data
    } else {
      SPDLOG_WARN("failed to parse HTTP request");
      return false;
    }
  }

  // handle default page
  if (len_path <= 0 || path == nullptr || (len_path == 1 && path[0] == '/')) {
    path = kPathDftPage.data();
    len_path = kPathDftPage.length();
  }

  // assemble full path
  const char *base_folder = path_base_folder.data();
  const size_t len_base_folder = path_base_folder.length();
  size_t len_full_path = len_base_folder + len_path + 1;
  std::unique_ptr<char[]> full_path(new char[len_full_path]);
  std::memcpy(full_path.get(), base_folder, len_base_folder);
  std::memcpy(full_path.get() + len_base_folder, path, len_path);
  std::memcpy(full_path.get() + len_base_folder + len_path, "\0", 1);

  // read file from disk.
  int resource_fd = open(full_path.get(), O_RDONLY);
  if (resource_fd == -1) {
    SPDLOG_ERROR("failed to open file {}", full_path.get());
    return -1;
  }
  std::unique_ptr<int, decltype([](const int *fd) {
                    if (fd && *fd > 0) {
                      close(*fd);
                    }
                  })>
      _resource_fd(&resource_fd);

  // get file size
  struct stat file_stat {};
  if (fstat(resource_fd, &file_stat) == -1) {
    SPDLOG_ERROR("failed to get file stat for {}", full_path.get());
    return false;
  }

  // send HTTP response header
  CRequest::HttpResponse resp(CRequest::kHttpOk);
  resp.SetContentLen(file_stat.st_size);
  resp.SetContentType(CRequest::kTypeTextHtml); // TODO: support more types
  resp.SetKeepAlive(false);

  // send headers.
  try {
    write(*ssl_sock_ptr, buffer(resp.StringifyFirstLine()));
    write(*ssl_sock_ptr, buffer(resp.StringifyHeaders()));
  } catch (const boost::system::system_error &e) {
    if (e.code() != error::eof) {
      SPDLOG_ERROR("failed to send headers: {}", e.what());
      return false;
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failed to send headers: {}", e.what());
    return false;
  }

  // send body.
  memset(header_buf, '\0', sizeof(header_buf));
  for (;;) {
    ssize_t n_read = read(resource_fd, header_buf, sizeof(header_buf));
    if (n_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to read from file {}: {}", full_path.get(),
                   strerror(errno));
      return false;
    } else if (n_read == 0) {
      break; // eof.
    }

    ssl_sock_ptr->write_some(boost::asio::buffer(header_buf, n_read), ec);
    if (ec && ec != error::eof) {
      SPDLOG_ERROR("failed to write to SSL socket: {}", ec.message());
      return false;
    }
  }

  return true;
}
