#include "services.h"
#include "compression.h"
#include "config.h"
#include "crequest.h"
#include "picohttpparser.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace azugate;

std::string findFileExtension(std::string &&path) {
  auto pos = path.find_last_of(".");
  if (pos != std::string::npos && pos + 2 <= path.length()) {
    return path.substr(pos + 1);
  }
  return "";
}

// TODO: this gateway only supports gzip and brotli currently.
// TODO: ignore q-factor currently, for example:
// Accept-Encoding: gzip; q=0.8, br; q=0.9, deflate.
std::string_view
getCompressionType(const std::string_view &supported_compression_types_str) {
  // gzip is the preferred encoding in azugate.
  if (supported_compression_types_str.find(utils::kCompressionTypeGzip) !=
      std::string::npos) {
    return utils::kCompressionTypeGzip;
  }
  if (supported_compression_types_str.find(utils::kCompressionTypeBrotli) !=
      std::string::npos) {
    return utils::kCompressionTypeBrotli;
  }
  return utils::kCompressionTypeNone;
}

struct picoHttpRequest {
  char header_buf[kMaxHttpHeaderSize];
  const char *path = nullptr;
  const char *method = nullptr;
  size_t method_len;
  size_t len_path;
  int minor_version;
  phr_header headers[kMaxHeadersNum];
  size_t num_headers;
};

inline bool httpHeaderParser(
    picoHttpRequest &header, boost::system::error_code &ec,
    const boost::shared_ptr<
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> &ssl_sock_ptr) {
  using namespace boost::asio;
  size_t total_parsed = 0;

  for (;;) {
    if (total_parsed >= kMaxHttpHeaderSize) {
      SPDLOG_WARN("HTTP header size exceeded the limit");
      return false;
    }
    size_t bytes_read =
        ssl_sock_ptr->read_some(buffer(header.header_buf + total_parsed,
                                       kMaxHttpHeaderSize - total_parsed),
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
    header.num_headers = std::size(header.headers);
    int pret = phr_parse_request(
        header.header_buf, total_parsed, &header.method, &header.method_len,
        &header.path, &header.len_path, &header.minor_version, header.headers,
        &header.num_headers, 0);
    if (pret > 0) {
      break; // Successful parse
    } else if (pret == -2) {
      continue; // Need more data
    } else {
      SPDLOG_WARN("failed to parse HTTP request");
      return false;
    }
  }
  return true;
}

inline std::shared_ptr<char[]>
assembleFullLocalFilePath(const std::string_view &path_base_folder,
                          const picoHttpRequest &header) {
  const size_t len_base_folder = path_base_folder.length();
  size_t len_full_path = len_base_folder + header.len_path + 1;
  std::shared_ptr<char[]> full_path(new char[len_full_path]);
  const char *base_folder = path_base_folder.data();
  std::memcpy(full_path.get(), base_folder, len_base_folder);
  std::memcpy(full_path.get() + len_base_folder, header.path, header.len_path);
  std::memcpy(full_path.get() + len_base_folder + header.len_path, "\0", 1);
  return full_path;
}

// return false if err, true if successful
bool FileProxy(
    const boost::shared_ptr<
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> &ssl_sock_ptr,
    const std::string_view &path_base_folder) {
  using namespace boost::asio;

  picoHttpRequest http_req;
  boost::system::error_code ec;

  // read and parse HTTP header
  if (!httpHeaderParser(http_req, ec, ssl_sock_ptr)) {
    SPDLOG_ERROR("failed to parse http headers");
    return false;
  }

  // extra meta from headers.
  std::string_view compression_type;
  for (size_t i = 0; i < http_req.num_headers; ++i) {
    auto &header = http_req.headers[i];
    // accept-encoding.
    if (std::string_view(header.name, header.name_len) ==
        CRequest::kHeaderAcceptEncoding) {
      compression_type =
          getCompressionType(std::string_view(header.value, header.value_len));
    }
  }

  // handle default page.
  // TODO: this can be replaced by a router.
  if (http_req.len_path <= 0 || http_req.path == nullptr ||
      (http_req.len_path == 1 && http_req.path[0] == '/')) {
    http_req.path = kPathDftPage.data();
    http_req.len_path = kPathDftPage.length();
  }

  std::shared_ptr<char[]> full_local_file_path =
      assembleFullLocalFilePath(path_base_folder, http_req);

  // read file from disk.
  int resource_fd = open(full_local_file_path.get(), O_RDONLY);
  if (resource_fd == -1) {
    SPDLOG_ERROR("failed to open file {}", full_local_file_path.get());
    return -1;
  }
  std::unique_ptr<int, decltype([](const int *fd) {
                    if (fd && *fd > 0) {
                      close(*fd);
                    }
                  })>
      _resource_fd(&resource_fd);

  // get the size of the local file.
  struct stat file_stat{};
  if (fstat(resource_fd, &file_stat) == -1) {
    SPDLOG_ERROR("failed to get file stat for {}", full_local_file_path.get());
    return false;
  }

  // setup and send response headers.
  CRequest::HttpResponse resp(CRequest::kHttpOk);
  resp.SetContentLength(file_stat.st_size);
  auto ext = findFileExtension(std::string(http_req.path, http_req.len_path));
  resp.SetContentType(CRequest::utils::GetContentTypeFromSuffix(ext));
  resp.SetKeepAlive(false);
  if (compression_type != utils::kCompressionTypeNone) {
    resp.SetContentEncoding(compression_type);
  }

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

  // setup and send body.
  // WARN: reuse buffer address.
  memset(http_req.header_buf, '\0', sizeof(http_req.header_buf));
  std::ostringstream compressed_stream;
  for (;;) {
    ssize_t n_read =
        read(resource_fd, http_req.header_buf, sizeof(http_req.header_buf));
    if (n_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to read from file {}: {}",
                   full_local_file_path.get(), strerror(errno));
      return false;
    } else if (n_read == 0) {
      break; // eof.
    }

    // http compression.
    // TODO: we only support gzip currently
    if (compression_type == utils::kCompressionTypeGzip) {
      utils::CompressGzipStream(http_req.header_buf, n_read, compressed_stream);
    }
    ssl_sock_ptr->write_some(boost::asio::buffer(compressed_stream.str()), ec);
    if (ec && ec != error::eof) {
      SPDLOG_ERROR("failed to write to SSL socket: {}", ec.message());
      return false;
    }
  }

  return true;
}

bool TcpProxy(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &target_sock_ptr) {
  using namespace boost::asio;
  char buf[kDftBufSize];
  boost::system::error_code ec;
  auto asio_buf = buffer(buf, kDftBufSize);
  // handle request in a simple loop.
  for (;;) {
    source_sock_ptr->read_some(asio_buf, ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        return true;
      }
      SPDLOG_WARN("failed to read from source: {}", ec.message());
      return false;
    }
    target_sock_ptr->write_some(asio_buf, ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        return true;
      }
      SPDLOG_WARN("failed to send data to traget: {}", ec.message());
      return false;
    }
  }

  return false;
}

// TODO: chuncked encoded.
// HTTP/1.1 200 OK
// Content-Type: text/plain
// Transfer-Encoding: chunked
// Content-Encoding: gzip

// A\r\n
// <gzip-compressed-data-part1>\r\n
// B\r\n
// <gzip-compressed-data-part2>\r\n
// 0\r\n\r\n