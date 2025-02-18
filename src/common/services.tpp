#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

#include "compression.hpp"
#include "config.h"
#include "crequest.h"
#include "http_wrapper.hpp"
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

#include <fmt/format.h>
#include <format>

#include "http_wrapper.hpp"
#include <memory>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace azugate;

// TODO: file path should be configured by router.
inline std::shared_ptr<char[]>
assembleFullLocalFilePath(const std::string_view &path_base_folder,
                          const network::PicoHttpRequest &header) {
  const size_t len_base_folder = path_base_folder.length();
  size_t len_full_path = len_base_folder + header.len_path + 1;
  std::shared_ptr<char[]> full_path(new char[len_full_path]);
  const char *base_folder = path_base_folder.data();
  std::memcpy(full_path.get(), base_folder, len_base_folder);
  std::memcpy(full_path.get() + len_base_folder, header.path, header.len_path);
  std::memcpy(full_path.get() + len_base_folder + header.len_path, "\0", 1);
  return full_path;
}

template <typename T>
void HttpProxyHandler(boost::shared_ptr<T> &sock_ptr,
                      ConnectionInfo source_connection_info) {
  using namespace boost::asio;

  network::PicoHttpRequest http_req;
  boost::system::error_code ec;

  // read and parse HTTP header
  if (!network::GetHttpHeader(http_req, ec, sock_ptr)) {
    SPDLOG_ERROR("failed to parse http headers");
    return;
  }

  // extract meta from headers.
  utils::CompressionType compression_type;
  for (size_t i = 0; i < http_req.num_headers; ++i) {
    auto &header = http_req.headers[i];
    // accept-encoding.
    if (std::string_view(header.name, header.name_len) ==
        CRequest::kHeaderAcceptEncoding) {
      compression_type = utils::GetCompressionType(
          std::string_view(header.value, header.value_len));
    }
  }

  // handle default page.
  source_connection_info.http_url = http_req.path;
  source_connection_info.type = ProtocolTypeHttp;
  auto target_conn_info_opt = GetRouterMapping(source_connection_info);
  if (http_req.len_path <= 0 || http_req.path == nullptr ||
      !target_conn_info_opt) {
    // TODO: Redirect to error message page.
    http_req.path = kPathDftPage.data();
    http_req.len_path = kPathDftPage.length();
  }

  std::shared_ptr<char[]> full_local_file_path =
      assembleFullLocalFilePath(kPathResourceFolder, http_req);
  auto full_local_file_path_str = full_local_file_path.get();

  if (!std::filesystem::exists(full_local_file_path_str)) {
    SPDLOG_ERROR("file not exists: {}", full_local_file_path_str);
    return;
  }
  std::ifstream local_file_stream(full_local_file_path_str, std::ios::binary);
  if (!local_file_stream.is_open()) {
    SPDLOG_ERROR("failed to open file: {}", full_local_file_path_str);
    return;
  }
  auto local_file_size = std::filesystem::file_size(full_local_file_path_str);

  // setup and send response headers.
  CRequest::HttpResponse resp(CRequest::kHttpOk);
  auto ext =
      utils::FindFileExtension(std::string(http_req.path, http_req.len_path));
  resp.SetContentType(CRequest::utils::GetContentTypeFromSuffix(ext));
  resp.SetKeepAlive(false);
  if (compression_type.code != utils::kCompressionTypeCodeNone) {
    resp.SetContentEncoding(compression_type.str);
    resp.SetTransferEncoding(CRequest::kTransferEncodingChunked);
  } else {
    resp.SetContentLength(local_file_size);
  }

  if (!network::SendHttpMessage<T>(resp, sock_ptr, ec)) {
    SPDLOG_ERROR("failed to send http response");
    return;
  }

  // setup and send body.
  // WARN: reuse buffer address.
  memset(http_req.header_buf, '\0', sizeof(http_req.header_buf));
  switch (compression_type.code) {
  case utils::kCompressionTypeCodeGzip: {
    utils::GzipCompressor gzip_compressor;
    auto compressed_output_handler = [&sock_ptr,
                                      &ec](unsigned char *compressed_data,
                                           size_t size) {
      std::array<boost::asio::const_buffer, 3> buffers = {
          boost::asio::buffer(std::format("{:x}{}", size, CRequest::kCrlf)),
          boost::asio::buffer(compressed_data, size),
          boost::asio::buffer(CRequest::kCrlf, 2)};
      sock_ptr->write_some(buffers, ec);
      if (ec) {
        SPDLOG_ERROR("failed to write chunk data to socket: {}", ec.message());
        return false;
      }
      return true;
    };
    auto ret = gzip_compressor.GzipStreamCompress(local_file_stream,
                                                  compressed_output_handler);
    if (!ret) {
      SPDLOG_ERROR("errors occur while compressing data chunk");
      return;
    }
    // write chunk ending marker.
    sock_ptr->write_some(
        boost::asio::buffer(CRequest::kChunkedEncodingEndingStr, 5), ec);
    if (ec) {
      SPDLOG_ERROR("failed to write chunk ending marker: {}", ec.message());
      return;
    }
    break;
  }
  case utils::kCompressionTypeCodeBrotli:
    SPDLOG_WARN("unsupported compression type: {}",
                utils::kCompressionTypeStrBrotli);
    break;
  case utils::kCompressionTypeCodeZStandard:
    SPDLOG_WARN("unsupported compression type:{}",
                utils::kCompressionTypeStrZStandard);
    break;
  case utils::kCompressionTypeCodeDeflate:
    SPDLOG_WARN("unsupported compression type: {}",
                utils::kCompressionTypeStrDeflate);
    break;
  default: {
    for (;;) {
      local_file_stream.read(http_req.header_buf, sizeof(http_req.header_buf));
      auto n_read = local_file_stream.gcount();
      if (n_read > 0) {
        sock_ptr->write_some(
            boost::asio::const_buffer(http_req.header_buf, n_read), ec);
        if (ec) {
          SPDLOG_ERROR("failed to write data to socket: {}", ec.message());
          return;
        }
      }
      if (local_file_stream.eof()) {
        break;
      }
      if (n_read == 0) {
        SPDLOG_ERROR("errors occur while reading from local file stream");
        return;
      }
    }
  }
  }
  return;
}

inline void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> &io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    std::optional<ConnectionInfo> target_connection_info_opt) {
  using namespace boost::asio;
  boost::system::error_code ec;
  if (!target_connection_info_opt) {
    SPDLOG_ERROR("failed to get proxy target");
    return;
  }
  // router.
  ip::tcp::resolver resolver(*io_context_ptr);
  ip::tcp::resolver::query query(
      std::string(target_connection_info_opt->address),
      std::to_string(target_connection_info_opt->port));
  auto endpoint_iterator = resolver.resolve(query, ec);
  if (ec) {
    SPDLOG_WARN("failed to resolve domain: {}", ec.message());
    return;
  }

  auto target_sock_ptr = std::make_shared<ip::tcp::socket>(*io_context_ptr);
  boost::asio::connect(*target_sock_ptr, endpoint_iterator, ec);
  if (ec) {
    SPDLOG_WARN("failed to connect to target: {}", ec.message());
    return;
  }

  char buf[kDefaultBufSize];
  while (true) {
    size_t bytes_read = source_sock_ptr->read_some(buffer(buf), ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        return;
      } else {
        SPDLOG_WARN("failed to read from source: {}", ec.message());
        return;
      }
    }

    size_t bytes_written = 0;
    while (bytes_written < bytes_read) {
      bytes_written += target_sock_ptr->write_some(
          buffer(buf + bytes_written, bytes_read - bytes_written), ec);
      if (ec) {
        SPDLOG_WARN("failed to write to target: {}", ec.message());
        return;
      }
    }

    size_t target_bytes_read = target_sock_ptr->read_some(buffer(buf), ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        return;
      } else {
        SPDLOG_WARN("failed to read from target: {}", ec.message());
        return;
      }
    }

    size_t target_bytes_written = 0;
    while (target_bytes_written < target_bytes_read) {
      target_bytes_written += source_sock_ptr->write_some(
          buffer(buf + target_bytes_written,
                 target_bytes_read - target_bytes_written),
          ec);
      if (ec) {
        SPDLOG_WARN("failed to write back to source: {}", ec.message());
        return;
      }
    }
  }

  return;
}