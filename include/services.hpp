#ifndef __SERVICES_H
#define __SERVICES_H

#include "auth.h"
#include "compression.hpp"
#include "config.h"
#include "crequest.h"
#include "network_wrapper.hpp"
#include "protocols.h"
#include "string_op.h"
#include <boost/asio.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/url.hpp>
#include <boost/url/url.hpp>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <sys/sendfile.h>
#endif

using namespace azugate;

// TODO: file path should be configured by router.
inline std::shared_ptr<char[]>
assembleFullLocalFilePath(const std::string_view &path_base_folder,
                          const std::string &target_url) {
  auto len_path = target_url.length();
  const size_t len_base_folder = path_base_folder.length();
  size_t len_full_path = len_base_folder + len_path + 1;
  std::shared_ptr<char[]> full_path(new char[len_full_path]);
  const char *base_folder = path_base_folder.data();
  std::memcpy(full_path.get(), base_folder, len_base_folder);
  std::memcpy(full_path.get() + len_base_folder, target_url.data(), len_path);
  std::memcpy(full_path.get() + len_base_folder + len_path, "\0", 1);
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
  // TODO: dead code on linux.
  std::ifstream local_file_stream(full_local_file_path_str, std::ios::binary);
  if (!local_file_stream.is_open()) {
    SPDLOG_ERROR("failed to open file: {}", full_local_file_path_str);
    return false;
  }
  while (!local_file_stream.eof()) {
    local_file_stream.read(request.header_buf, sizeof(request.header_buf));
    std::streamsize n_read = local_file_stream.gcount();
    if (n_read > 0) {
      boost::system::error_code ec;
      boost::asio::write(*sock_ptr,
                         boost::asio::buffer(request.header_buf, n_read), ec);
      if (ec) {
        SPDLOG_ERROR("failed to write data to socket: {}", ec.message());
        return false;
      }
    } else if (local_file_stream.bad()) {
      SPDLOG_ERROR("error occurred while reading file: {}",
                   full_local_file_path_str);
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
    // move past 'azugate_access_token='
    token_pos += 21;
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
    SPDLOG_WARN(auth_header.substr(token_pos));
    return std::string(auth_header.substr(token_pos));
  }
  return "";
}

// ref:
// https://www.envoyproxy.io/docs/envoy/latest/configuration/http/http_filters/oauth2_filter.
template <typename T>
inline bool externalAuthorization(network::PicoHttpRequest &request,
                                  boost::shared_ptr<T> sock_ptr,
                                  const std::string &token) {
  using namespace boost::beast;

  // get authorization code.
  auto code = network::ExtractParamFromUrl(
      std::string(request.path, request.len_path), "code");
  if (code != "") {
    using namespace boost::asio;
    boost::system::error_code ec;
    ssl::context ctx(ssl::context::sslv23_client);
    // TODO: integrate the io_context with the server's.
    io_context ioc;
    ip::tcp::resolver resolver(ioc);
    ssl::stream<boost::beast::tcp_stream> stream(ioc, ctx);
    auto results = resolver.resolve(g_external_auth_domain, kDftHttpsPort, ec);
    if (ec) {
      SPDLOG_WARN("failed to resolve host: {}", ec.message());
      return false;
    }
    boost::beast::get_lowest_layer(stream).connect(results, ec);
    if (ec) {
      SPDLOG_WARN("failed to connect to host: {}", ec.message());
      return false;
    }
    auto _ = stream.handshake(ssl::stream_base::client, ec);
    if (ec) {
      SPDLOG_WARN("failed to do handshake: {}", ec.message());
      return false;
    }
    // TODO: standard oauth workflow.
    // Send code to Auth0 server.
    http::request<http::string_body> req{http::verb::post, "/oauth/token", 11};
    req.set(http::field::content_type, "application/x-www-form-urlencoded");
    req.set(http::field::host, g_external_auth_domain);
    req.set(http::field::user_agent, AZUGATE_VERSION_STRING);
    boost::urls::url u;
    auto params = u.params();
    params.set("grant_type", "authorization_code");
    params.set("client_id", g_external_auth_client_id);
    params.set("client_secret", g_external_auth_client_secret);
    params.set("code", code);
    // TODO: callback url.
    params.set("redirect_uri", "http://localhost:8089/callback");
    req.body() = u.encoded_query();
    req.prepare_payload();
    http::write(stream, req, ec);
    if (ec) {
      SPDLOG_WARN("failed to send http request: {}", ec.message());
      return false;
    }
    // read response from Auth0.
    boost::beast::flat_buffer buffer;
    http::response<http::string_body> auth0_resp;
    http::read(stream, buffer, auth0_resp);
    auto json = nlohmann::json::parse(auth0_resp.body());
    if (!json.contains("access_token")) {
      SPDLOG_WARN("failed to get access token from ID provider");
      return false;
    }
    auto token = json["access_token"].get<std::string>();
    // generate azugate access_token and send it back to client.
    auto payload = "{}";
    std::string azugate_access_token =
        utils::GenerateToken(payload, g_authorization_token_secret);
    if (azugate_access_token == "") {
      SPDLOG_ERROR("failed to generate token");
      return false;
    }
    http::response<http::string_body> client_resp{http::status::found, 11};
    client_resp.set(
        http::field::set_cookie,
        fmt::format("azugate_access_token={}", azugate_access_token));
    client_resp.set(http::field::location, "/welcome.html");
    // TODO: redirect web page.
    client_resp.body() = "<h1>Login Successfully</h1>";
    client_resp.prepare_payload();
    http::write(*sock_ptr, client_resp, ec);
    if (ec) {
      SPDLOG_ERROR("failed to write response to client");
    }
    return false;
  }
  // verify token or get authorization code from client.
  if (token.length() == 0 ||
      !utils::VerifyToken(token, g_authorization_token_secret)) {
    boost::urls::url u(
        fmt::format("https://{}/authorize", g_external_auth_domain));
    auto params = u.params();
    params.set("response_type", "code");
    params.set("client_id", g_external_auth_client_id);
    params.set("redirect_uri", "http://localhost:8089/callback");
    params.set("scope", "openid");
    // TODO: deal with state.
    params.set("state", "1111");
    // redirect to oauth login page.
    http::response<http::string_body> resp{http::status::found, 11};
    resp.set(http::field::location, u.buffer());
    resp.set(http::field::connection, CRequest::kConnectionClose);
    resp.prepare_payload();
    boost::system::error_code ec;
    http::write(*sock_ptr, resp, ec);
    if (ec) {
      SPDLOG_WARN("failed to write http response");
    }
    return false;
  }
  return true;
}

inline bool extractMetaFromHeaders(utils::CompressionType &compression_type,
                                   network::PicoHttpRequest &request,
                                   std::string &token,
                                   size_t &request_content_length) {
  if (request.num_headers <= 0 || request.num_headers > kMaxHeadersNum) {
    SPDLOG_WARN("No headers found in the request.");
    return false;
  }

  for (size_t i = 0; i < request.num_headers; ++i) {
    auto &header = request.headers[i];
    // validate the header struct.
    if (header.name == nullptr || header.value == nullptr) {
      SPDLOG_WARN("header name or value is null at index {}", i);
      return false;
    }
    if (header.name_len <= 0 || header.value_len <= 0) {
      SPDLOG_WARN(
          "invalid header length at index {}: name_len={}, value_len={}", i,
          header.name_len, header.value_len);
      return false;
    }
    auto header_value = std::string(header.value, header.value_len);
    // header switch.
    std::string_view header_name(header.name, header.name_len);
    header_name = utils::toLower(header_name);
    if (header_name == CRequest::kHeaderFieldAcceptEncoding) {
      compression_type = utils::GetCompressionType(header_value);
      continue;
    }
    if (header_name == CRequest::kHeaderFieldCookie) {
      token = extractAzugateAccessTokenFromCookie(header_value);
      continue;
    }
    if (header_name == CRequest::kHeaderFieldContentLength) {
      auto [_, err] =
          std::from_chars(header.value, header.value + header.value_len,
                          request_content_length);
      if (err != std::errc()) {
        SPDLOG_ERROR("failed to convert std::string '{}' to int", header_value);
        return false;
      }
      continue;
    }
    // TODO: fix it when needed.
    // if (header_name == CRequest::kHeaderAuthorization) {
    //   std::string_view header_value(header.value, header.value_len);
    //   token = extractTokenFromAuthorization(header_value);
    //   continue;
    // }
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
        async_accpet_cb_(async_accpet_cb), total_parsed_(0),
        source_connection_info_(source_connection_info),
        request_content_length_(0) {}

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
      extra_body_len_ = total_parsed_ - pret;
      total_parsed_ = pret;
      extractMetadata();
    } else if (pret == -2) {
      // need more data.'
      // SPDLOG_WARN("need more data");
      parseRequest();
      return;
    } else {
      SPDLOG_WARN("failed to parse HTTP request");
      async_accpet_cb_();
      return;
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
    if (!extractMetaFromHeaders(compression_type_, request_, token_,
                                request_content_length_)) {
      SPDLOG_WARN("failed to extract meta from headers");
      async_accpet_cb_();
      return;
    }
    // TODO: external authoriation and router.
    if (g_http_external_authorization &&
        !externalAuthorization(request_, sock_ptr_, token_)) {
      async_accpet_cb_();
      return;
    }
    route();
  }

  // convert string constants to boost::beast::http::verb.
  std::optional<boost::beast::http::verb>
  stringToVerb(const std::string &methodStr) {
    using namespace boost::beast;
    static const std::unordered_map<std::string, http::verb> methodMap = {
        {"GET", http::verb::get},     {"POST", http::verb::post},
        {"PUT", http::verb::put},     {"DELETE", http::verb::delete_},
        {"HEAD", http::verb::head},   {"OPTIONS", http::verb::options},
        {"PATCH", http::verb::patch}, {"CONNECT", http::verb::connect},
        {"TRACE", http::verb::trace}};

    auto it = methodMap.find(methodStr);
    if (it != methodMap.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  inline void route() {
    using namespace boost::beast;
    boost::system::error_code ec;
    source_connection_info_.http_url =
        std::string(request_.path, request_.len_path);
    // TODO: differ by "Connection upgrade".
    source_connection_info_.type = ProtocolTypeHttp;
    auto target_conn_info_opt = GetTargetRoute(source_connection_info_);
    if (!target_conn_info_opt) {
      SPDLOG_WARN("no path found for {}", source_connection_info_.http_url);
      http::response<http::string_body> err_not_found_resp{
          http::status::not_found, 11};
      http::write(*sock_ptr_, err_not_found_resp, ec);
      if (ec) {
        SPDLOG_WARN("failed to write http response");
      }
      async_accpet_cb_();
      return;
    }
    target_url_ = target_conn_info_opt->http_url;

    if (!target_conn_info_opt->remote) {
      handleLocalFileRequest();
      async_accpet_cb_();
      return;
    }
    auto target_address = target_conn_info_opt->address;
    auto target_port = target_conn_info_opt->port;
    auto target_protocol = target_conn_info_opt->type;
    if (target_address == "") {
      SPDLOG_ERROR("invalid target address");
      async_accpet_cb_();
      return;
    }
    // TODO: only for testing purpose.
    SPDLOG_INFO("[{}] {}:{}{}", target_protocol, target_address, target_port,
                target_url_);
    if (target_protocol == ProtocolTypeWebSocket) {
      handleWebSocketRequest(std::move(target_address), std::move(target_port));
      return;
    } else if (target_protocol == ProtocolTypeHttp) {
      handleHttpRequest(std::move(target_address), std::move(target_port));
      return;
    }
    SPDLOG_WARN("unknown protocol: {}", target_protocol);
    async_accpet_cb_();
  }

  // grpc-web protocol data frame:
  // +---------------+----------------------------------+------+
  // | compressed(1) | data length (4 bytes big-endian) | data |
  // +---------------+----------------------------------+------+
  inline void
  handleGrpcWebRequest(boost::shared_ptr<std::vector<char>> buffer_ptr,
                       std::string &response_str) {
    SPDLOG_WARN("grpc-web request: {}", target_url_);
    auto &buffer = *buffer_ptr;
    const size_t kGrpcFrameLength = 5;
    auto left_frame_bytes = kGrpcFrameLength - extra_body_len_;
    boost::system::error_code ec;
    if (left_frame_bytes) {
      size_t n_read = boost::asio::read(
          *sock_ptr_, boost::asio::buffer(buffer),
          boost::asio::transfer_exactly(left_frame_bytes), ec);
      if (ec) {
        SPDLOG_ERROR("failed to read grpc-web body: {}", ec.message());
        async_accpet_cb_();
        return;
      }
      if (n_read + left_frame_bytes < kGrpcFrameLength) {
        SPDLOG_WARN("invalid gRPC-Web frame");
        async_accpet_cb_();
        return;
      }
    }
    uint8_t compressed = buffer[0];
    uint32_t msg_len = (uint8_t(buffer[1]) << 24) | (uint8_t(buffer[2]) << 16) |
                       (uint8_t(buffer[3]) << 8) | (uint8_t(buffer[4]));

    if (kGrpcFrameLength + msg_len > buffer.size()) {
      SPDLOG_ERROR("message length({}) exceeds buffer({})", msg_len,
                   buffer.size());
      async_accpet_cb_();
      return;
    }

    response_str.assign(buffer.begin() + kGrpcFrameLength,
                        buffer.begin() + kGrpcFrameLength + msg_len);
    SPDLOG_INFO("received gRPC-Web message, length: {}", msg_len);
  }

  void handleHttpRequest(std::string &&target_host, uint16_t &&target_port) {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ssl = boost::asio::ssl;
    namespace beast = boost::beast;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;
    boost::system::error_code ec;
    constexpr bool is_ssl =
        std::is_same_v<T,
                       boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;
    // proxy request to target.
    tcp::resolver resolver(*io_context_ptr_);
    auto results =
        resolver.resolve(target_host, std::to_string(target_port), ec);
    if (ec) {
      SPDLOG_ERROR("resolver failed: {}", ec.message());
      async_accpet_cb_();
      return;
    }
    boost::shared_ptr<T> stream;

    if constexpr (is_ssl) {
      // SPDLOG_WARN("using ssl connection");
      auto ssl_ctx =
          boost::make_shared<ssl::context>(ssl::context::sslv23_client);
      stream = boost::make_shared<T>(*io_context_ptr_, *ssl_ctx);
      net::connect(beast::get_lowest_layer(*stream), results, ec);
      if (ec) {
        SPDLOG_ERROR("SSL connect failed: {}", ec.message());
        async_accpet_cb_();
        return;
      }
      stream->handshake(ssl::stream_base::client, ec);
      if (ec) {
        SPDLOG_ERROR("SSL handshake failed: {}", ec.message());
        async_accpet_cb_();
        return;
      }
    } else {
      stream = boost::make_shared<T>(*io_context_ptr_);
      boost::asio::connect(*stream, results, ec);
      if (ec) {
        SPDLOG_ERROR("TCP connect failed: {}", ec.message());
        async_accpet_cb_();
        return;
      }
    }

    std::string method_string(request_.method, request_.method_len);
    auto http_verb = stringToVerb(method_string);
    if (!http_verb) {
      SPDLOG_ERROR("unknown HTTP method: {}", method_string);
      async_accpet_cb_();
      return;
    }
    http::request<http::empty_body> req{*http_verb, target_url_, 11};
    // rewirte headers field.
    for (size_t i = 0; i < request_.num_headers; ++i) {
      auto &header = request_.headers[i];
      auto header_name = std::string(header.name, header.name_len);
      auto lower_header_name = utils::toLower(header_name);
      if (lower_header_name == CRequest::kHeaderFieldConnection) {
        continue;
      }
      if (lower_header_name == CRequest::kHeaderFieldHost ||
          lower_header_name == CRequest::kHeaderFieldReferer ||
          lower_header_name == CRequest::kHeaderFieldAcceptEncoding ||
          lower_header_name == CRequest::kHeaderFieldAccept ||
          lower_header_name.find("sec-") != std::string::npos) {
        continue;
      }
      req.set(header_name, std::string(header.value, header.value_len));
    }
    req.set(http::field::connection, CRequest::kConnectionClose);
    req.set(http::field::host, target_host);
    http::serializer<true, http::empty_body> sr(req);
    http::write_header(*stream, sr, ec);
    if (ec) {
      SPDLOG_ERROR("write header failed: {}", ec.message());
      async_accpet_cb_();
      return;
    }

    auto src_read_buffer = boost::make_shared<std::vector<char>>();
    src_read_buffer->resize(kDefaultBufSize);
    // TODO: async write.
    // SPDLOG_WARN("content-length: {}", request_content_length_);

    if (extra_body_len_ > 0) {
      std::memcpy(src_read_buffer->data(), request_.header_buf + total_parsed_,
                  extra_body_len_);
      // TODO: refactor these pieces of shit.
      boost::asio::write(
          *stream, boost::asio::buffer(*src_read_buffer, extra_body_len_), ec);
      if (ec) {
        SPDLOG_ERROR("error writing body to target: {}", ec.message());
        async_accpet_cb_();
        return;
      }
    }
    auto left_body_length = request_content_length_ - extra_body_len_;
    for (size_t n_read = 0; n_read < left_body_length;) {
      SPDLOG_WARN("write data to target");
      size_t n =
          sock_ptr_->read_some(boost::asio::buffer(*src_read_buffer), ec);
      n_read += n;
      if (ec == boost::asio::error::eof) {
        break;
      } else if (ec) {
        SPDLOG_ERROR("error reading body from client: {}", ec.message());
        async_accpet_cb_();
        return;
      }
      boost::asio::write(*stream, boost::asio::buffer(*src_read_buffer, n), ec);
      if (ec) {
        SPDLOG_ERROR("error writing body to target: {}", ec.message());
        async_accpet_cb_();
        return;
      }
    }

    std::string response_str;
    beast::flat_buffer response_buffer;
    http::response_parser<http::dynamic_body> parser;
    parser.body_limit(kMaxBodyBufferSize);
    http::read(*stream, response_buffer, parser, ec);
    if (ec && ec != boost::beast::http::error::end_of_stream) {
      SPDLOG_ERROR("http read failed: {}", ec.message());
      async_accpet_cb_();
      return;
    }
    auto res = parser.get();
    std::stringstream ss;
    ss << res;
    response_str = ss.str();

    // send back the reponse (header & body) from the target.
    boost::asio::write(*sock_ptr_, boost::asio::buffer(response_str), ec);
    if (ec) {
      SPDLOG_ERROR("write back to client failed: {}", ec.message());
      async_accpet_cb_();
      return;
    }

    // close connection.
    // TODO: gracefully shutdown.
    // stream->lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
    // if (ec && ec != net::error::eof &&
    //     ec.message() != "Socket is not connected") {
    //   SPDLOG_WARN("shutdown failed: {}", ec.message());
    // }
    async_accpet_cb_();
  }

  void handleWebSocketRequest(std::string &&target_host,
                              uint16_t &&target_port) {
    if constexpr (std::is_same_v<T, boost::asio::ssl::stream<
                                        boost::asio::ip::tcp::socket>>) {
      SPDLOG_ERROR("ssl websocket not implemented");
      async_accpet_cb_();
      return;
    } else {
      namespace beast = boost::beast;
      namespace websocket = beast::websocket;
      namespace net = boost::asio;
      using tcp = net::ip::tcp;
      beast::flat_buffer header_buffer;
      boost::system::error_code ec;
      header_buffer.commit(
          boost::asio::buffer_copy(header_buffer.prepare(total_parsed_),
                                   boost::asio::buffer(request_.header_buf)));
      //  TODO: support wss.
      auto src_ws_stream =
          boost::make_shared<websocket::stream<boost::asio::ip::tcp::socket>>(
              std::move(*sock_ptr_));
      src_ws_stream->accept(header_buffer.data(), ec);
      if (ec) {
        SPDLOG_WARN("failed to do WebSocket handshake: {}", ec.message());
        async_accpet_cb_();
        return;
      }

      // connect to target.
      tcp::resolver resolver(*io_context_ptr_);
      auto results =
          resolver.resolve(target_host, std::to_string(target_port), ec);
      if (ec) {
        SPDLOG_WARN("failed to resolve host: {}", ec.message());
        async_accpet_cb_();
        return;
      }
      tcp::socket target_socket(*io_context_ptr_);
      net::connect(target_socket, results.begin(), results.end(), ec);
      if (ec) {
        SPDLOG_WARN("failed to connect to target: {}", ec.message());
        async_accpet_cb_();
        return;
      }
      auto target_ws_stream =
          boost::make_shared<websocket::stream<boost::asio::ip::tcp::socket>>(
              std::move(target_socket));
      target_ws_stream->handshake(target_host, target_url_, ec);
      if (ec) {
        SPDLOG_WARN("failed to do websocket handshake: {}", ec.message());
        async_accpet_cb_();
        return;
      }

      // handle bi-direction connection.
      auto source_data_buffer = boost::make_shared<boost::beast::flat_buffer>();
      auto target_data_buffer = boost::make_shared<boost::beast::flat_buffer>();
      readWebSocketPayloadFromSource(src_ws_stream, target_ws_stream,
                                     source_data_buffer);
      readWebSocketPayloadFromSource(target_ws_stream, src_ws_stream,
                                     target_data_buffer);
    }
  }

  void readWebSocketPayloadFromSource(
      boost::shared_ptr<
          boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>
          src_ws_stream,
      boost::shared_ptr<
          boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>
          target_ws_stream,
      boost::shared_ptr<boost::beast::flat_buffer> data_buffer) {
    src_ws_stream->async_read(
        *data_buffer,
        std::bind(&HttpProxyHandler<T>::onWebSocketRead,
                  this->shared_from_this(), src_ws_stream, target_ws_stream,
                  data_buffer, std::placeholders::_1, std::placeholders::_2));
    return;
  }

  void onWebSocketRead(
      boost::shared_ptr<
          boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>
          src_ws_stream,
      boost::shared_ptr<
          boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>
          target_ws_stream,
      boost::shared_ptr<boost::beast::flat_buffer> data_buffer,
      boost::system::error_code ec, size_t bytes_read) {
    if (ec) {
      SPDLOG_WARN("failed to read WebSocket data: {}", ec.message());
      async_accpet_cb_();
      return;
    }

    // TODO: only for debugging.
    // auto buffer_begin = boost::asio::buffers_begin(data_buffer->data());
    // auto buffer_end = boost::asio::buffers_end(data_buffer->data());
    // SPDLOG_WARN("Get Message: {}", std::string(buffer_begin, buffer_end));

    // send response.
    target_ws_stream->write(data_buffer->data(), ec);
    if (ec) {
      SPDLOG_ERROR("failed to write WebSocket message to target: {}",
                   ec.message());
      async_accpet_cb_();
      return;
    }
    data_buffer->consume(bytes_read);
    readWebSocketPayloadFromSource(src_ws_stream, target_ws_stream,
                                   data_buffer);
  }

  void handleLocalFileRequest() {
    // get local file path from request url.
    std::shared_ptr<char[]> full_local_file_path =
        assembleFullLocalFilePath(kPathResourceFolder, target_url_);
    auto full_local_file_path_str = full_local_file_path.get();
    if (!std::filesystem::exists(full_local_file_path_str) ||
        !std::filesystem::is_regular_file(full_local_file_path_str)) {
      SPDLOG_WARN("file not exists: {}", full_local_file_path_str);
      async_accpet_cb_();
      return;
    }

    auto local_file_size = std::filesystem::file_size(full_local_file_path_str);

    // setup and send response headers.
    CRequest::HttpResponse resp(CRequest::kHttpOk);
    auto ext = utils::FindFileExtension(target_url_);
    resp.SetContentType(CRequest::utils::GetContentTypeFromSuffix(ext));
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
      return;
    }

    // setup and send body.
    memset(request_.header_buf, '\0', sizeof(request_.header_buf));
    if (!compressAndWriteBody(sock_ptr_, full_local_file_path_str,
                              local_file_size, compression_type_, request_)) {
      SPDLOG_WARN("failed to write body");
    };
    async_accpet_cb_();
  }

  void Close() {
    if (sock_ptr_ && sock_ptr_->lowest_layer().is_open()) {
      boost::system::error_code ec;
      sock_ptr_->lowest_layer().shutdown(
          boost::asio::socket_base::shutdown_both, ec);
      // TODO: correct 'sock not connected' message?
      if (ec && ec.message() != "Socket is not connected") {
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
  std::function<void()> async_accpet_cb_;
  azugate::network::PicoHttpRequest request_;
  size_t total_parsed_;
  // parseRequest() might consume part of the body during header parsing.
  // We call this the "extra body":
  // +-------------+----------------+
  // | extra body  | remaining body |
  // +-------------+----------------+
  size_t extra_body_len_;
  // services.
  utils::CompressionType compression_type_;
  std::string token_;
  std::string target_url_;
  size_t request_content_length_;
  ConnectionInfo source_connection_info_;
};

void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &source_sock_ptr,
    std::optional<azugate::ConnectionInfo> target_connection_info_opt);

#endif
