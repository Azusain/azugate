#ifndef __SERVICES_H
#define __SERVICES_H

#include "compression.hpp"
#include "config.h"
#include "crequest.h"
#include "network_wrapper.hpp"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#if defined(__linux__)
#include <sys/sendfile.h>
#endif

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

// TODO: async optimization.
template <typename T>
inline bool handleNoCompression(const boost::shared_ptr<T> sock_ptr,
                                boost::system::error_code &ec,
                                const char *full_local_file_path_str,
                                size_t local_file_size,
                                network::PicoHttpRequest &request) {
  constexpr bool is_ssl =
      std::is_same_v<T, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;
  if constexpr (!is_ssl) {
#if defined(__linux__)
    int file_fd = ::open(full_local_file_path_str, O_RDONLY);
    if (file_fd < 0) {
      SPDLOG_ERROR("failed to open file: {}", strerror(errno));
      return false;
    }
    off_t offset = 0;
    int fd = sock_ptr->lowest_layer().native_handle();
    ssize_t sent_bytes = sendfile(fd, file_fd, &offset, local_file_size);
    if (sent_bytes == -1) {
      SPDLOG_ERROR("sendfile failed: {}", strerror(errno));
      return false;
    }
    return true;
#endif
  }
  // TODO: dead code in linux.
  std::ifstream local_file_stream(full_local_file_path_str, std::ios::binary);
  if (!local_file_stream.is_open()) {
    SPDLOG_ERROR("failed to open file: {}", full_local_file_path_str);
    return false;
  }
  // TODO: extremely inefficient.
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
                                 const char *full_local_file_path_str,
                                 size_t local_file_size,
                                 utils::CompressionType compression_type,
                                 network::PicoHttpRequest &request) {
  boost::system::error_code ec;
  switch (compression_type.code) {
  case utils::kCompressionTypeCodeGzip: {
    std::ifstream local_file_stream(full_local_file_path_str, std::ios::binary);
    if (!local_file_stream.is_open()) {
      SPDLOG_ERROR("failed to open file: {}", full_local_file_path_str);
      return false;
    }
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
    return handleNoCompression(sock_ptr, ec, full_local_file_path_str,
                               local_file_size, request);
  }
}

// helper function to extract token from cookie.
inline std::string
extractAzugateAccessTokenFromCookie(const std::string_view &cookie_header) {
  size_t token_pos = cookie_header.find("azugate_access_token=");
  if (token_pos != std::string_view::npos) {
    // move past 'token='
    token_pos += 6;
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

// ref:
// https://www.envoyproxy.io/docs/envoy/latest/configuration/http/http_filters/oauth2_filter.
//
// template <typename T>
// inline bool
// externalAuthorization(network::PicoHttpRequest &request,
//                       boost::shared_ptr<boost::asio::io_context>
//                       io_context_ptr, const network::HttpClient<T>
//                       &http_client, boost::system::error_code &ec,
//                       std::string &token) {
//   // exract authorization code from url parameters.
//   auto code = network::ExtractParamFromUrl(
//       std::string(request.path, request.len_path), "code");
//   if (code != "") {
//     network::HttpClient<T> code_http_client;
//     code_http_client.Connect(io_context_ptr,
//     g_external_oauth_server_domain,
//                              std::string(kDftHttpPort));
//     // token exchange with the oauth server.
//     std::string body;
//     body.reserve(kDftStringReservedBytes);
//     body.append("grant_type=")
//         .append(azugate::utils::UrlEncode("authorization_code"))
//         .append("&code=")
//         .append(azugate::utils::UrlEncode(code))
//         .append("&redirect_uri=");
//     if (GetHttps()) {
//       body.append(ProtocolTypeHttps);
//     } else {
//       body.append(ProtocolTypeHttp);
//     }
//     body.append(azugate::utils::UrlEncode("://"))
//         .append(g_azugate_domain)
//         .append(":")
//         .append(std::to_string(g_azugate_port))
//         .append("&client_id=")
//         .append(azugate::utils::UrlEncode(g_azugate_oauth_client_id))
//         .append("&client_secret=")
//         .append(azugate::utils::UrlEncode(g_azugate_oauth_client_secret));
//     CRequest::HttpRequest req(std::string(CRequest::kHttpPost),
//                               g_external_oauth_server_path);
//     req.SetContentLength(body.length());
//     req.SetKeepAlive(false);
//     if (!http_client.SendHttpHeader(req, ec)) {
//       SPDLOG_WARN("failed to send token exchange http request");
//       return false;
//     }
//     network::PicoHttpResponse resp;
//     if (!http_client.ParseHttpResponse(resp, ec)) {
//       SPDLOG_WARN("failed to parse http response");
//       return false;
//     }
//     // extract token from body.
//     size_t content_length = 0;
//     for (size_t i = 0; i < resp.num_headers; ++i) {
//       auto &header = request.headers[i];
//       std::string_view header_name(header.name, header.name_len);
//       if (header_name == CRequest::kHeaderAuthorization) {
//         // TODO: watch out for the spaces.
//         content_length = std::stoi(std::string(header.value,
//         header.value_len));
//       }
//     }
//     std::string response_buffer;
//     response_buffer.reserve(content_length);
//     if (http_client.ReadHttpBody(response_buffer, ec)) {
//       SPDLOG_WARN("error occurs while fetching token from body");
//       return false;
//     }

//     try {
//       auto json = nlohmann::json::parse(response_buffer);
//       if (json["access_token"] == "") {
//         SPDLOG_WARN("access token not found in http response");
//         return false;
//       };
//     } catch (...) {
//       SPDLOG_WARN("failed to parse json from body");
//       return false;
//     }

//     // generate azugate access token.
//     std::string azugate_access_token =
//         utils::GenerateToken("{}", g_authorization_token_secret);
//     if (azugate_access_token == "") {
//       SPDLOG_ERROR("failed to generate token");
//       return false;
//     }

//     CRequest::HttpResponse token_resp(CRequest::kHttpOk);
//     token_resp.SetKeepAlive(false);
//     token_resp.SetCookie("azugate_access_token", azugate_access_token);
//     if (!http_client.SendHttpHeader(token_resp, ec)) {
//       SPDLOG_WARN("failed to send back access token to client");
//       return false;
//     }
//     return true;
//   }
//   // verify token.
//   if (token.length() == 0 ||
//       !utils::VerifyToken(token, g_authorization_token_secret)) {
//     // redirect to oauth login page.
//     CRequest::HttpResponse redirect_resp(CRequest::kHttpFound);
//     redirect_resp.SetKeepAlive(false);
//     if (!http_client.SendHttpHeader(redirect_resp, ec)) {
//       SPDLOG_WARN("failed to send redirect http response");
//       return false;
//     };
//   }
//   return true;
// }

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
class HttpProxyHandler
    : public std::enable_shared_from_this<HttpProxyHandler<T>> {
public:
  HttpProxyHandler(boost::shared_ptr<boost::asio::io_context> io_context_ptr,
                   boost::shared_ptr<T> sock_ptr,
                   azugate::ConnectionInfo source_connection_info,
                   std::function<void()> async_accpet_cb)
      : io_context_ptr_(io_context_ptr), sock_ptr_(sock_ptr),
        source_connection_info_(source_connection_info),
        async_accpet_cb_(async_accpet_cb), total_parsed_(0),
        strand_(boost::asio::make_strand(*io_context_ptr)) {}

  // TODO: release connections properly.
  ~HttpProxyHandler() { Close(); }

  void Start() { parseRequest(); }

  // used in parseRequest().
  void onRead(boost::system::error_code ec, size_t bytes_read) {
    if (ec) {
      if (ec == boost::asio::error::eof) {
        SPDLOG_DEBUG("connection closed by peer");
        async_accpet_cb_();
        return;
      }
      // SPDLOG_WARN("failed to read HTTP header: {}", ec.message());
      async_accpet_cb_();
      return;
    }
    total_parsed_ += bytes_read;
    request_.num_headers = std::size(request_.headers);
    int pret = phr_parse_request(
        request_.header_buf, total_parsed_, &request_.method,
        &request_.method_len, &request_.path, &request_.len_path,
        &request_.minor_version, request_.headers, &request_.num_headers, 0);
    bool valid_request =
        !(request_.method == nullptr || request_.method_len == 0 ||
          request_.path == nullptr || request_.len_path == 0 ||
          request_.num_headers < 0 ||
          request_.num_headers > azugate::kMaxHeadersNum);

    if (pret > 0 && valid_request) {
      // successful parse.
      extractMetadata();
    } else if (pret == -2) {
      // need more data.'
      // SPDLOG_WARN("need more data");
      parseRequest();
      return;
    } else {
      SPDLOG_WARN("failed to parse HTTP request");
      async_accpet_cb_();
    }
  }

  void parseRequest() {
    if (total_parsed_ >= azugate::kMaxHttpHeaderSize) {
      SPDLOG_WARN("HTTP header size exceeded the limit");
      async_accpet_cb_();
      return;
    }
    sock_ptr_->async_read_some(
        boost::asio::buffer(request_.header_buf + total_parsed_,
                            azugate::kMaxHttpHeaderSize - total_parsed_),
        std::bind(&HttpProxyHandler<T>::onRead, this->shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
  }

  inline void extractMetadata() {
    if (!extractMetaFromHeaders(compression_type_, request_, token_)) {
      SPDLOG_WARN("failed to extract meta from headers");
      async_accpet_cb_();
    }
    // TODO: external authoriation and router.
    // if (g_http_external_authorization &&
    //     !externalAuthorization(request_, io_context_ptr_, http_client, ec,
    //                            token)) {}
    handleLocalFileRequest();
  }

  // void handleProxyRequest() {
  //   std::string target_host = "bytebase.example.com";
  //   std::string target_port = "80";

  //   auto resolver =
  //       std::make_shared<boost::asio::ip::tcp::resolver>(*io_context_ptr_);
  //   resolver->async_resolve(
  //       target_host, target_port,
  //       boost::asio::bind_executor(
  //           strand_, [this, self = this->shared_from_this(), resolver](
  //                        boost::system::error_code ec,
  //                        boost::asio::ip::tcp::resolver::results_type
  //                        results) {
  //             if (ec) {
  //               SPDLOG_WARN("resolve failed: {}", ec.message());
  //               async_accpet_cb_();
  //               return;
  //             }

  //
  //             auto remote_socket =
  //                 std::make_shared<boost::beast::tcp_stream>(*io_context_ptr_);
  //             remote_socket->expires_after(std::chrono::seconds(30));
  //             remote_socket->async_connect(
  //                 results,
  //                 boost::asio::bind_executor(
  //                     strand_, [this, self, remote_socket](
  //                                  boost::system::error_code ec, const auto
  //                                  &) {
  //                       if (ec) {
  //                         SPDLOG_WARN("connect failed: {}", ec.message());
  //                         async_accpet_cb_();
  //                         return;
  //                       }

  //
  //                       bool is_websocket = false;
  //                       for (int i = 0; i < request_.num_headers; ++i) {
  //                         auto &hdr = request_.headers[i];
  //                         std::string name(hdr.name, hdr.name_len);
  //                         std::string value(hdr.value, hdr.value_len);
  //                         if (boost::iequals(name, "Upgrade") &&
  //                             boost::iequals(value, "websocket")) {
  //                           is_websocket = true;
  //                           break;
  //                         }
  //                       }

  //                       if (is_websocket) {
  //                         handleWebSocketProxy(remote_socket);
  //                       } else {
  //                         handleHttpProxy(remote_socket);
  //                       }
  //                     }));
  //           }));
  // }

  // void
  // handleHttpProxy(std::shared_ptr<boost::beast::tcp_stream> remote_socket) {
  //
  //   boost::beast::http::request<boost::beast::http::string_body> req;
  //   req.method(boost::beast::http::string_to_verb(
  //       std::string(request_.method, request_.method_len)));
  //   req.target(std::string(request_.path, request_.len_path));
  //   req.version(11);
  //   for (int i = 0; i < request_.num_headers; ++i) {
  //     auto &hdr = request_.headers[i];
  //     req.set(std::string(hdr.name, hdr.name_len),
  //             std::string(hdr.value, hdr.value_len));
  //   }

  //
  //   auto write_stream =
  //       std::make_shared<boost::beast::tcp_stream>(std::move(*remote_socket));
  //   boost::beast::http::async_write(
  //       *write_stream, req,
  //       boost::asio::bind_executor(strand_, [this, self = shared_from_this(),
  //                                            write_stream](
  //                                               boost::system::error_code ec,
  //                                               std::size_t) {
  //         if (ec) {
  //           SPDLOG_WARN("http async_write failed: {}", ec.message());
  //           async_accpet_cb_();
  //           return;
  //         }

  //
  //         auto buffer = std::make_shared<boost::beast::flat_buffer>();
  //         auto res = std::make_shared<
  //             boost::beast::http::response<boost::beast::http::string_body>>();
  //         boost::beast::http::async_read(
  //             *write_stream, *buffer, *res,
  //             boost::asio::bind_executor(
  //                 strand_,
  //                 [this, self, res](boost::system::error_code ec,
  //                 std::size_t) {
  //                   if (ec) {
  //                     SPDLOG_WARN("http async_read failed: {}",
  //                     ec.message()); async_accpet_cb_(); return;
  //                   }

  //
  //                   boost::asio::async_write(
  //                       *sock_ptr_, boost::asio::buffer(res->to_string()),
  //                       boost::asio::bind_executor(
  //                           strand_, std::bind(&HttpProxyHandler::onWrite,
  //                           self,
  //                                              std::placeholders::_1,
  //                                              std::placeholders::_2)));
  //                 }));
  //       }));
  // }

  // void handleWebSocketProxy(std::shared_ptr<boost::beast::tcp_stream>
  // remote_socket) {
  //   auto ws_client =
  //   std::make_shared<boost::beast::websocket::stream<boost::beast::tcp_stream>>(std::move(*remote_socket));
  //   std::string host = "bytebase.example.com";

  //   std::string target_path(request_.path, request_.len_path);

  //   ws_client->async_handshake(host, target_path,
  //     boost::asio::bind_executor(strand_,
  //       [this, self = shared_from_this(),
  //       ws_client](boost::system::error_code ec) {
  //     if (ec) {
  //       SPDLOG_WARN("websocket handshake failed: {}", ec.message());
  //       async_accpet_cb_();
  //       return;
  //     }

  //     auto do_proxy = [this, ws_client, self]() {
  //       auto client_ws =
  //       std::make_shared<boost::beast::websocket::stream<boost::beast::tcp_stream>>(std::move(*sock_ptr_));

  //       auto client_buf = std::make_shared<boost::beast::flat_buffer>();
  //       client_ws->async_read(*client_buf,
  //         [client_buf, ws_client](boost::system::error_code ec, std::size_t
  //         bytes) {
  //           if (!ec) {
  //             ws_client->text(true);
  //             ws_client->async_write(client_buf->data(), [](auto, auto) {});
  //           }
  //         });

  //       auto server_buf = std::make_shared<boost::beast::flat_buffer>();
  //       ws_client->async_read(*server_buf,
  //         [server_buf, client_ws](boost::system::error_code ec, std::size_t
  //         bytes) {
  //           if (!ec) {
  //             client_ws->text(true);
  //             client_ws->async_write(server_buf->data(), [](auto, auto) {});
  //           }
  //         });
  //     };

  //     do_proxy();
  //   }));
  // }

  void handleLocalFileRequest() {
    // get local file path from request url.
    std::shared_ptr<char[]> full_local_file_path =
        assembleFullLocalFilePath(kPathResourceFolder, request_);
    // auto full_local_file_path_str = full_local_file_path.get();
    auto full_local_file_path_str = "../resources/welcome.html";
    if (!std::filesystem::exists(full_local_file_path_str)) {
      SPDLOG_ERROR("file not exists: {}", full_local_file_path_str);
      async_accpet_cb_();
    }

    auto local_file_size = std::filesystem::file_size(full_local_file_path_str);

    // setup and send response headers.
    CRequest::HttpResponse resp(CRequest::kHttpOk);
    auto ext =
        utils::FindFileExtension(std::string(request_.path, request_.len_path));
    // resp.SetContentType(CRequest::utils::GetContentTypeFromSuffix(ext));
    resp.SetContentType(CRequest::kContentTypeTextHtml);
    resp.SetKeepAlive(false);
    if (compression_type_.code != utils::kCompressionTypeCodeNone) {
      resp.SetContentEncoding(compression_type_.str);
      resp.SetTransferEncoding(CRequest::kTransferEncodingChunked);
    } else {
      resp.SetContentLength(local_file_size);
    }
    network::HttpClient<T> http_client(sock_ptr_);
    if (!http_client.SendHttpHeader(resp)) {
      SPDLOG_ERROR("failed to send http response");
      async_accpet_cb_();
    }

    // setup and send body.
    // WARN: reuse buffer address.
    memset(request_.header_buf, '\0', sizeof(request_.header_buf));
    if (!compressAndWriteBody(sock_ptr_, full_local_file_path_str,
                              local_file_size, compression_type_, request_)) {
      SPDLOG_WARN("failed to write body");
    };
    async_accpet_cb_();
  }

  // used in onWrite().
  void onWrite(boost::system::error_code ec, size_t bytes_transferred) {
    if (ec) {
      SPDLOG_WARN("failed to write response");
    }

    async_accpet_cb_();
  }

  void Close() {
    if (sock_ptr_ && sock_ptr_->lowest_layer().is_open()) {
      boost::system::error_code ec;
      sock_ptr_->lowest_layer().shutdown(
          boost::asio::socket_base::shutdown_both, ec);
      if (ec) {
        SPDLOG_WARN("failed to do shutdown: {}", ec.message());
      }
      sock_ptr_->lowest_layer().close(ec);
      if (ec) {
        SPDLOG_WARN("failed to do close: {}", ec.message());
      }
    }
  }

private:
  // io and parser.
  boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
  boost::shared_ptr<T> sock_ptr_;
  azugate::ConnectionInfo source_connection_info_;
  std::function<void()> async_accpet_cb_;
  azugate::network::PicoHttpRequest request_;
  boost::asio::strand<boost::asio::any_io_executor> strand_;
  size_t total_parsed_;
  // services.
  utils::CompressionType compression_type_;
  std::string token_;
};

void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    std::optional<azugate::ConnectionInfo> target_connection_info_opt);

#endif
