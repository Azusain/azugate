#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/core/demangle.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <iomanip>
#include <string_view>

#include "compression.h"
#include "config.h"
#include "crequest.h"
#include "picohttpparser.h"
#include <array>
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
#include <format>

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

inline std::string findFileExtension(std::string &&path) {
  auto pos = path.find_last_of(".");
  if (pos != std::string::npos && pos + 2 <= path.length()) {
    return path.substr(pos + 1);
  }
  return "";
}

// TODO: this gateway only supports gzip and brotli currently.
// TODO: ignore q-factor currently, for example:
// Accept-Encoding: gzip; q=0.8, br; q=0.9, deflate.
inline utils::CompressionType
getCompressionType(const std::string_view &supported_compression_types_str) {
  // // TODO: just for testing purpose.
  // return utils::CompressionType{.code = utils::kCompressionTypeCodeNone,
  //                               .str = utils::kCompressionTypeStrNone};
  // gzip is the preferred encoding in azugate.
  if (supported_compression_types_str.find(utils::kCompressionTypeStrGzip) !=
      std::string::npos) {
    return utils::CompressionType{.code = utils::kCompressionTypeCodeGzip,
                                  .str = utils::kCompressionTypeStrGzip};
  } else if (supported_compression_types_str.find(
                 utils::kCompressionTypeStrBrotli) != std::string::npos) {
    return utils::CompressionType{.code = utils::kCompressionTypeCodeBrotli,
                                  .str = utils::kCompressionTypeStrBrotli};
  }
  return utils::CompressionType{.code = utils::kCompressionTypeCodeNone,
                                .str = utils::kCompressionTypeStrNone};
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

template <typename T>
inline bool httpHeaderParser(picoHttpRequest &header,
                             boost::system::error_code &ec,
                             const boost::shared_ptr<T> &sock_ptr) {
  using namespace boost::asio;
  size_t total_parsed = 0;

  for (;;) {
    if (total_parsed >= kMaxHttpHeaderSize) {
      SPDLOG_WARN("HTTP header size exceeded the limit");
      return false;
    }
    size_t bytes_read =
        sock_ptr->read_some(buffer(header.header_buf + total_parsed,
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

inline void compressBody(picoHttpRequest &http_req,
                         const utils::CompressionType &compression_type,
                         std::array<boost::asio::const_buffer, 3> &buffers,
                         size_t n_read) {}

inline void print_buffer_as_hex(const boost::asio::const_buffer &buffer) {
  // 获取缓冲区的起始指针和大小
  const uint8_t *data = boost::asio::buffer_cast<const uint8_t *>(buffer);
  size_t size = boost::asio::buffer_size(buffer);
  std::cout << "HEX DATA -> ";
  // 输出十六进制格式的数据
  for (size_t i = 0; i < size; ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i]
              << " ";
  }
  std::cout << std::endl;
  std::cout << "<- HEX DATA END\n";
}
template <typename T>
concept isSslSocket = requires(T socket) {
  socket.handshake(boost::asio::ssl::stream_base::server);
};

template <typename T> void FileProxyHandler(boost::shared_ptr<T> &sock_ptr) {
  using namespace boost::asio;
  // ssl handshake if necessary.
  if constexpr (isSslSocket<T>) {
    try {
      sock_ptr->handshake(ssl::stream_base::server);
    } catch (const std::exception &e) {
      std::string what = e.what();
      if (what.compare("handshake: ssl/tls alert certificate unknown (SSL "
                       "routines) [asio.ssl:167773206]")) {
        SPDLOG_ERROR("failed to handshake: {}", what);
        return;
      }
    }
  }

  picoHttpRequest http_req;
  boost::system::error_code ec;

  // read and parse HTTP header
  if (!httpHeaderParser(http_req, ec, sock_ptr)) {
    SPDLOG_ERROR("failed to parse http headers");
    return;
  }

  // extra meta from headers.
  utils::CompressionType compression_type;
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
      assembleFullLocalFilePath(kPathResourceFolder, http_req);

  // read file from disk.
  int resource_fd = open(full_local_file_path.get(), O_RDONLY);
  if (resource_fd == -1) {
    SPDLOG_ERROR("failed to open file {}", full_local_file_path.get());
    return;
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
    return;
  }

  // setup and send response headers.
  CRequest::HttpResponse resp(CRequest::kHttpOk);
  auto ext = findFileExtension(std::string(http_req.path, http_req.len_path));
  resp.SetContentType(CRequest::utils::GetContentTypeFromSuffix(ext));
  resp.SetKeepAlive(false);
  if (compression_type.code != utils::kCompressionTypeCodeNone) {
    resp.SetContentEncoding(compression_type.str);
    resp.SetTransferEncoding(CRequest::kTransferEncodingChunked);
  } else {
    resp.SetContentLength(file_stat.st_size);
  }

  try {
    // TODO: code style, write() or write_some()?
    write(*sock_ptr, buffer(resp.StringifyFirstLine()));
    write(*sock_ptr, buffer(resp.StringifyHeaders()));
  } catch (const boost::system::system_error &e) {
    if (e.code() != error::eof) {
      SPDLOG_ERROR("failed to send headers: {}", e.what());
      return;
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("failed to send headers: {}", e.what());
    return;
  }

  // setup and send body.
  // WARN: reuse buffer address.
  memset(http_req.header_buf, '\0', sizeof(http_req.header_buf));
  boost::iostreams::filtering_ostream fo;
  auto compressor_opts = boost::iostreams::gzip_params(
      boost::iostreams::gzip::default_compression);
  fo.push(boost::iostreams::gzip_compressor(compressor_opts, true));
  std::ostringstream compressed_output;
  fo.push(compressed_output);

  // read data from local file and write it to socket.
  for (;;) {
    ssize_t n_read = read(resource_fd, http_req.header_buf, 128);
    if (n_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to read from file {}: {}",
                   full_local_file_path.get(), strerror(errno));
      return;
    } else if (n_read == 0) {
      if (compression_type.code != utils::kCompressionTypeCodeNone) {
        // send the ending chunk.
        sock_ptr->write_some(
            boost::asio::buffer(CRequest::kChunkedEncodingEndingStr), ec);
        if (ec && ec != error::eof) {
          SPDLOG_ERROR("failed to write ending chunked: {}", ec.message());
          return;
        }
      }
      break; // eof.
    }
    print_buffer_as_hex(boost::asio::const_buffer(http_req.header_buf, 128));
    // http compression and chuncked encoding. ref:
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding.
    // TODO: we only support gzip currently
    std::array<boost::asio::const_buffer, 3> buffers;
    std::string compressed_str;
    switch (compression_type.code) {
    case utils::kCompressionTypeCodeGzip: {
      fo.write(http_req.header_buf, n_read);

      compressed_str = compressed_output.str();
      std::cout << "compressed_str -> " << compressed_str.length() << "\n";
      // scatter gather i/o.
      buffers[0] = boost::asio::buffer(
          std::format("{:x}{}", compressed_str.length(), CRequest::kCrlf));
      buffers[1] =
          boost::asio::buffer(compressed_str.data(), compressed_str.length());
      buffers[2] = boost::asio::buffer("\r\n", 2);
      break;
    }
    case utils::kCompressionTypeCodeBrotli:
      break;
    case utils::kCompressionTypeCodeZStandard:
      break;
    case utils::kCompressionTypeCodeDeflate:
      break;
    default:
      buffers[0] = boost::asio::buffer(http_req.header_buf, n_read);
    }

    // print_buffer_as_hex(buffers[1]);

    sock_ptr->write_some(buffers, ec);
    if (ec && ec != error::eof) {
      SPDLOG_ERROR("failed to write to socket: {}", ec.message());
      return;
    }
  }

  return;
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
