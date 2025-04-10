#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string_view>

#include "auth.h"
#include "common.hpp"
#include "compression.hpp"
#include "config.h"
#include "crequest.h"
#include "network_wrapper.hpp"
#include "picohttpparser.h"
#include "protocols.h"
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

#include <memory>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

using namespace azugate;

// TODO: file path should be configured by router.
inline std::shared_ptr<char[]>
assembleFullLocalFilePath(const std::string_view &path_base_folder,
                          const network::PicoHttpRequest &request) {
  const size_t len_base_folder = path_base_folder.length();
  size_t len_full_path = len_base_folder + request.len_path + 1;
  std::shared_ptr<char[]> full_path(new char[len_full_path]);
  const char *base_folder = path_base_folder.data();
  std::memcpy(full_path.get(), base_folder, len_base_folder);
  std::memcpy(full_path.get() + len_base_folder, request.path,
              request.len_path);
  std::memcpy(full_path.get() + len_base_folder + request.len_path, "\0", 1);
  return full_path;
}

template <typename T>
inline bool handleGzipCompression(const boost::shared_ptr<T> sock_ptr,
                                  boost::system::error_code &ec,
                                  std::ifstream &local_file_stream) {
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
    return false;
  }
  sock_ptr->write_some(
      boost::asio::buffer(CRequest::kChunkedEncodingEndingStr, 5), ec);
  if (ec) {
    SPDLOG_ERROR("failed to write chunk ending marker: {}", ec.message());
    return false;
  }
  return true;
}

template <typename T>
inline bool handleNoCompression(const boost::shared_ptr<T> sock_ptr,
                                boost::system::error_code &ec,
                                std::ifstream &local_file_stream,
                                network::PicoHttpRequest &request) {
  for (;;) {
    local_file_stream.read(request.header_buf, sizeof(request.header_buf));
    auto n_read = local_file_stream.gcount();
    if (n_read > 0) {
      sock_ptr->write_some(
          boost::asio::const_buffer(request.header_buf, n_read), ec);
      if (ec) {
        SPDLOG_ERROR("failed to write data to socket: {}", ec.message());
        return false;
      }
    }
    if (local_file_stream.eof()) {
      break;
    }
    if (n_read == 0) {
      SPDLOG_ERROR("errors occur while reading from local file stream");
      return false;
    }
  }
  return true;
}

template <typename T>
inline bool compressAndWriteBody(const boost::shared_ptr<T> sock_ptr,
                                 boost::system::error_code &ec,
                                 std::ifstream &local_file_stream,
                                 utils::CompressionType compression_type,
                                 network::PicoHttpRequest &request) {
  switch (compression_type.code) {
  case utils::kCompressionTypeCodeGzip: {
    return handleGzipCompression(sock_ptr, ec, local_file_stream);
  }
  case utils::kCompressionTypeCodeBrotli:
    SPDLOG_WARN("unsupported compression type: {}",
                utils::kCompressionTypeStrBrotli);
    return false;
  case utils::kCompressionTypeCodeZStandard:
    SPDLOG_WARN("unsupported compression type: {}",
                utils::kCompressionTypeStrZStandard);
    return false;
  case utils::kCompressionTypeCodeDeflate:
    SPDLOG_WARN("unsupported compression type: {}",
                utils::kCompressionTypeStrDeflate);
    return false;
  default:
    return handleNoCompression(sock_ptr, ec, local_file_stream, request);
  }
}

// helper function to extract token from cookie
inline std::string
extractAzugateAccessTokenFromCookie(const std::string_view &cookie_header) {
  // logic to extract token from cookie, for example:
  size_t token_pos = cookie_header.find("azugate_access_token=");
  if (token_pos != std::string_view::npos) {
    token_pos += 6; // move past 'token='
    size_t token_end = cookie_header.find(';', token_pos);
    if (token_end == std::string_view::npos) {
      token_end = cookie_header.length();
    }
    return std::string(cookie_header.substr(token_pos, token_end - token_pos));
  }
  return "";
}

// helper function to extract token from Authorization header
inline std::string
extractTokenFromAuthorization(const std::string_view &auth_header) {
  size_t token_pos = auth_header.find(CRequest::kHeaderAuthorizationTypeBearer);
  if (token_pos != std::string_view::npos) {
    // move past 'Bearer' and a space.
    token_pos += 7;
    return std::string(auth_header.substr(token_pos));
  }
  return "";
}

inline bool extractMetaFromHeaders(utils::CompressionType &compression_type,
                                   network::PicoHttpRequest &request,
                                   std::string &token) {
  if (request.num_headers <= 0 || request.num_headers > kMaxHeadersNum) {
    SPDLOG_WARN("No headers found in the request.");
    return false;
  }

  for (size_t i = 0; i < request.num_headers; ++i) {
    auto &header = request.headers[i];
    if (header.name == nullptr || header.value == nullptr) {
      SPDLOG_WARN("Header name or value is null at index {}", i);
      return false;
    }
    if (header.name_len <= 0 || header.value_len <= 0) {
      SPDLOG_WARN(
          "Invalid header length at index {}: name_len={}, value_len={}", i,
          header.name_len, header.value_len);
      return false;
    }
    std::string_view header_name(header.name, header.name_len);
    if (header_name == CRequest::kHeaderAcceptEncoding) {
      compression_type = utils::GetCompressionType(
          std::string_view(header.value, header.value_len));
      continue;
    }
    if (header_name == CRequest::kHeaderCookie) {
      std::string_view header_value(header.value, header.value_len);
      token = extractAzugateAccessTokenFromCookie(header_value);
      continue;
    }
    if (header_name == CRequest::kHeaderAuthorization) {
      std::string_view header_value(header.value, header.value_len);
      token = extractTokenFromAuthorization(header_value);
      continue;
    }
  }
  if (!GetHttpCompression()) {
    compression_type =
        utils::CompressionType{.code = utils::kCompressionTypeCodeNone,
                               .str = utils::kCompressionTypeStrNone};
  }

  return true;
}

template <typename T>
inline bool
externalAuthorization(network::PicoHttpRequest &request,
                      boost::shared_ptr<boost::asio::io_context> io_context_ptr,
                      const network::HttpClient<T> &http_client,
                      boost::system::error_code &ec, std::string &token) {
  // exract authorization code from url parameters.
  auto code = network::ExtractParamFromUrl(
      std::string(request.path, request.len_path), "code");
  if (code != "") {
    network::HttpClient<T> code_http_client;
    code_http_client.Connect(io_context_ptr, g_external_oauth_server_domain,
                             std::string(kDftHttpPort));
    // token exchange with the oauth server.
    std::string body;
    body.reserve(kDftStringReservedBytes);
    body.append("grant_type=")
        .append(azugate::utils::UrlEncode("authorization_code"))
        .append("&code=")
        .append(azugate::utils::UrlEncode(code))
        .append("&redirect_uri=");
    if (GetHttps()) {
      body.append(ProtocolTypeHttps);
    } else {
      body.append(ProtocolTypeHttp);
    }
    body.append(azugate::utils::UrlEncode("://"))
        .append(g_azugate_domain)
        .append(":")
        .append(std::to_string(g_azugate_port))
        .append("&client_id=")
        .append(azugate::utils::UrlEncode(g_azugate_oauth_client_id))
        .append("&client_secret=")
        .append(azugate::utils::UrlEncode(g_azugate_oauth_client_secret));
    CRequest::HttpRequest req(std::string(CRequest::kHttpPost),
                              g_external_oauth_server_path);
    req.SetContentLength(body.length());
    req.SetKeepAlive(false);
    if (!http_client.SendHttpMessage(req, ec)) {
      SPDLOG_WARN("failed to send token exchange http request");
      return false;
    }
    network::PicoHttpResponse resp;
    if (!http_client.ParseHttpResponse(resp, ec)) {
      SPDLOG_WARN("failed to parse http response");
      return false;
    }
    // extract token from body.
    size_t content_length = 0;
    for (size_t i = 0; i < resp.num_headers; ++i) {
      auto &header = request.headers[i];
      std::string_view header_name(header.name, header.name_len);
      if (header_name == CRequest::kHeaderAuthorization) {
        // TODO: watch out for the spaces.
        content_length = std::stoi(std::string(header.value, header.value_len));
      }
    }
    std::string response_buffer;
    response_buffer.reserve(content_length);
    if (http_client.ReadHttpBody(response_buffer, ec)) {
      SPDLOG_WARN("error occurs while fetching token from body");
      return false;
    }

    try {
      auto json = nlohmann::json::parse(response_buffer);
      if (json["access_token"] == "") {
        SPDLOG_WARN("access token not found in http response");
        return false;
      };
    } catch (...) {
      SPDLOG_WARN("failed to parse json from body");
      return false;
    }

    // generate azugate access token.
    std::string azugate_access_token =
        utils::GenerateToken("{}", g_authorization_token_secret);
    if (azugate_access_token == "") {
      SPDLOG_ERROR("failed to generate token");
      return false;
    }

    CRequest::HttpResponse token_resp(CRequest::kHttpOk);
    token_resp.SetKeepAlive(false);
    token_resp.SetCookie("azugate_access_token", azugate_access_token);
    if (!http_client.SendHttpMessage(token_resp, ec)) {
      SPDLOG_WARN("failed to send back access token to client");
      return false;
    }
    return true;
  }
  // verify token.
  if (token.length() == 0 ||
      !utils::VerifyToken(token, g_authorization_token_secret)) {
    // redirect to oauth login page.
    CRequest::HttpResponse redirect_resp(CRequest::kHttpFound);
    redirect_resp.SetKeepAlive(false);
    if (!http_client.SendHttpMessage(redirect_resp, ec)) {
      SPDLOG_WARN("failed to send redirect http response");
      return false;
    };
  }
  return true;
}

template <typename T>
void HttpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr,
    boost::shared_ptr<T> &sock_ptr, ConnectionInfo source_connection_info) {
  using namespace boost::asio;

  network::PicoHttpRequest request;
  boost::system::error_code ec;

  // init http client using established tcp connection.
  network::HttpClient<T> http_client(std::move(sock_ptr));

  // read and parse HTTP header
  if (!http_client.ParseHttpRequest(request, ec)) {
    SPDLOG_ERROR("failed to parse http headers");
    return;
  }

  utils::CompressionType compression_type;
  std::string token;
  if (!extractMetaFromHeaders(compression_type, request, token)) {
    SPDLOG_WARN("failed to extract meta from headers");
    CRequest::HttpResponse resp(CRequest::kHttpBadRequest);
    resp.SetKeepAlive(false);
    if (!http_client.SendHttpMessage(resp, ec)) {
      SPDLOG_WARN("failed to write error response");
    }
    int fd = sock_ptr->lowest_layer().native_handle();
    shutdown(fd, SHUT_RDWR);
    return;
  }

  // external authoriation.
  if (g_http_external_authorization &&
      !externalAuthorization(request, io_context_ptr, http_client, ec, token)) {
    return;
  }

  // handle default page.
  source_connection_info.http_url = request.path;
  source_connection_info.type = ProtocolTypeHttp;
  auto target_conn_info_opt = GetRouterMapping(source_connection_info);
  if (request.len_path <= 0 || request.path == nullptr ||
      !target_conn_info_opt) {
    // TODO: redirect to error message page.
    request.path = kPathDftPage.data();
    request.len_path = kPathDftPage.length();
  }

  std::shared_ptr<char[]> full_local_file_path =
      assembleFullLocalFilePath(kPathResourceFolder, request);
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
      utils::FindFileExtension(std::string(request.path, request.len_path));
  resp.SetContentType(CRequest::utils::GetContentTypeFromSuffix(ext));
  resp.SetKeepAlive(false);
  if (compression_type.code != utils::kCompressionTypeCodeNone) {
    resp.SetContentEncoding(compression_type.str);
    resp.SetTransferEncoding(CRequest::kTransferEncodingChunked);
  } else {
    resp.SetContentLength(local_file_size);
  }

  if (!http_client.SendHttpMessage(resp, ec)) {
    SPDLOG_ERROR("failed to send http response");
    return;
  }

  // setup and send body.
  // WARN: reuse buffer address.
  memset(request.header_buf, '\0', sizeof(request.header_buf));
  if (!compressAndWriteBody(sock_ptr, ec, local_file_stream, compression_type,
                            request)) {
    SPDLOG_WARN("failed to write body");
  };
  int fd = sock_ptr->lowest_layer().native_handle();
  shutdown(fd, SHUT_RDWR);
  close(fd);
  return;
}

inline void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    std::optional<ConnectionInfo> target_connection_info_opt) {
  using namespace boost::asio;
  boost::system::error_code ec;
  if (!target_connection_info_opt) {
    SPDLOG_ERROR("failed to get proxy target");
    return;
  }
  network::HttpClient<boost::asio::ip::tcp::socket> tcp_client;
  tcp_client.Connect(io_context_ptr,
                     std::string(target_connection_info_opt->address),
                     std::to_string(target_connection_info_opt->port));
  auto target_sock_ptr = tcp_client.GetSocket();
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