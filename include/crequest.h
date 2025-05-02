#ifndef __CREQUEST_H
#define __CREQUEST_H

#include "common.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace CRequest {

// http status codes.
constexpr uint16_t kHttpContinue = 100;
constexpr uint16_t kHttpSwitchingProtocols = 101;
constexpr uint16_t kHttpOk = 200;
constexpr uint16_t kHttpCreated = 201;
constexpr uint16_t kHttpAccepted = 202;
constexpr uint16_t kHttpNonAuthoritativeInformation = 203;
constexpr uint16_t kHttpNoContent = 204;
constexpr uint16_t kHttpResetContent = 205;
constexpr uint16_t kHttpPartialContent = 206;
constexpr uint16_t kHttpMultipleChoices = 300;
constexpr uint16_t kHttpMovedPermanently = 301;
constexpr uint16_t kHttpFound = 302;
constexpr uint16_t kHttpSeeOther = 303;
constexpr uint16_t kHttpNotModified = 304;
constexpr uint16_t kHttpUseProxy = 305;
constexpr uint16_t kHttpTemporaryRedirect = 307;
constexpr uint16_t kHttpBadRequest = 400;
constexpr uint16_t kHttpUnauthorized = 401;
constexpr uint16_t kHttpPaymentRequired = 402;
constexpr uint16_t kHttpForbidden = 403;
constexpr uint16_t kHttpNotFound = 404;
constexpr uint16_t kHttpMethodNotAllowed = 405;
constexpr uint16_t kHttpNotAcceptable = 406;
constexpr uint16_t kHttpProxyAuthenticationRequired = 407;
constexpr uint16_t kHttpRequestTimeout = 408;
constexpr uint16_t kHttpConflict = 409;
constexpr uint16_t kHttpGone = 410;
constexpr uint16_t kHttpLengthRequired = 411;
constexpr uint16_t kHttpPreconditionFailed = 412;
constexpr uint16_t kHttpPayloadTooLarge = 413;
constexpr uint16_t kHttpUriTooLong = 414;
constexpr uint16_t kHttpUnsupportedMediaType = 415;
constexpr uint16_t kHttpRangeNotSatisfiable = 416;
constexpr uint16_t kHttpExpectationFailed = 417;
constexpr uint16_t kHttpUpgradeRequired = 426;
constexpr uint16_t kHttpInternalServerError = 500;
constexpr uint16_t kHttpNotImplemented = 501;
constexpr uint16_t kHttpBadGateway = 502;
constexpr uint16_t kHttpServiceUnavailable = 503;
constexpr uint16_t kHttpGatewayTimeout = 504;
constexpr uint16_t kHttpHttpVersionNotSupported = 505;
// http methods.
constexpr std::string_view kHttpGet = "GET";
constexpr std::string_view kHttpPost = "POST";
constexpr std::string_view kHttpOptions = "OPTIONS";
constexpr std::string_view kHttpPut = "PUT";
constexpr std::string_view kHttpDelete = "DELETE";
constexpr std::string_view kHttpHead = "HEAD";
constexpr std::string_view kHttpTrace = "TRACE";
constexpr std::string_view kHttpPatch = "PATCH";
constexpr std::string_view kHttpConnect = "CONNECT";
// http header fields.
// lower case !!!
constexpr std::string_view kHeaderFieldAcceptEncoding = "accept-encoding";
constexpr std::string_view kHeaderFieldContentEncoding = "content-encoding";
constexpr std::string_view kHeaderFieldTransferEncoding = "transfer-encoding";
constexpr std::string_view kHeaderFieldCookie = "cookie";
constexpr std::string_view kHeaderFieldAuthorization = "authorization";
constexpr std::string_view kHeaderFieldContentLength = "content-length";
constexpr std::string_view kHeaderFieldConnection = "connection";
constexpr std::string_view kHeaderFieldHost = "host";
constexpr std::string_view kHeaderFieldReferer = "referer";
constexpr std::string_view kHeaderFieldAccept = "accept";
constexpr std::string_view kHeaderFieldXGrpcWeb = "x-grpc-web";

// http connection.
constexpr std::string_view kConnectionClose = "Close";
constexpr std::string_view kConnectionKeepAlive = "keep-alive";
constexpr std::string_view kConnectionUpgrade = "Upgrade";
// http content type.
// ref: https://www.iana.org/assignments/media-types/media-types.xhtml
constexpr std::string_view kContentTypeAppJson = "application/json";
constexpr std::string_view kContentTypeAppJs = "application/javascript";
constexpr std::string_view kContentTypeAppUrlencoded =
    "application/x-www-form-urlencoded";
constexpr std::string_view kContentTypeAppXml = "application/xml";
constexpr std::string_view kContentTypeAppOctet = "application/octet-stream";
constexpr std::string_view kContentTypeTextHtml = "text/html";
constexpr std::string_view kContentTypeTextPlain = "text/plain";
constexpr std::string_view kContentTypeTextCss = "text/css";
constexpr std::string_view kContentTypeImgPng = "image/png";
constexpr std::string_view kContentTypeImgJpeg = "image/jpeg";
constexpr std::string_view kContentTypeImgSvg = "image/svg+xml";
// official MIME type registered with IANA as an alternative to image/x-icon.
constexpr std::string_view kContentTypeXIcon = "image/vnd.microsoft.icon";
// supported versions.
constexpr std::string_view kHttpVersion011 = "HTTP/1.1";
// some parser stuffs.
constexpr std::string_view kCrlf = "\r\n";
constexpr std::string_view kSpace = " ";
constexpr std::string_view kHeaderAuthorizationTypeBearer = "Bearer";
// local file extensions;
constexpr uint32_t kFileExtensionJson =
    azugate::utils::HashConstantString("json");
constexpr uint32_t kFileExtensionXml =
    azugate::utils::HashConstantString("xml");
constexpr uint32_t kFileExtensionBin =
    azugate::utils::HashConstantString("bin");
constexpr uint32_t kFileExtensionExe =
    azugate::utils::HashConstantString("exe");
constexpr uint32_t kFileExtensionIso =
    azugate::utils::HashConstantString("iso");
constexpr uint32_t kFileExtensionHtml =
    azugate::utils::HashConstantString("html");
constexpr uint32_t kFileExtensionHtm =
    azugate::utils::HashConstantString("htm");
constexpr uint32_t kFileExtensionTxt =
    azugate::utils::HashConstantString("txt");
constexpr uint32_t kFileExtensionLog =
    azugate::utils::HashConstantString("log");
constexpr uint32_t kFileExtensionCfg =
    azugate::utils::HashConstantString("cfg");
constexpr uint32_t kFileExtensionIni =
    azugate::utils::HashConstantString("ini");
constexpr uint32_t kFileExtensionPng =
    azugate::utils::HashConstantString("png");
constexpr uint32_t kFileExtensionJpg =
    azugate::utils::HashConstantString("jpg");
constexpr uint32_t kFileExtensionJpeg =
    azugate::utils::HashConstantString("jpeg");
constexpr uint32_t kFileExtensionSvg =
    azugate::utils::HashConstantString("svg");
constexpr uint32_t kFileExtensionXIcon =
    azugate::utils::HashConstantString("ico");
constexpr uint32_t kFileExtensionCss =
    azugate::utils::HashConstantString("css");
constexpr uint32_t kFileExtensionJavaScript =
    azugate::utils::HashConstantString("js");
// mics.
constexpr std::string_view kTransferEncodingChunked = "chunked";
constexpr std::string_view kChunkedEncodingEndingStr = "0\r\n\r\n";

namespace utils {
constexpr const char *GetMessageFromStatusCode(uint16_t status_code);

std::string_view GetContentTypeFromSuffix(const std::string_view &path);

} // namespace utils.

class HttpMessage {
public:
  virtual ~HttpMessage();

  // header operations.
  void SetCookie(std::string_view key, std::string_view value);

  void SetKeepAlive(bool keep_alive);

  void SetContentType(std::string_view content_type);

  void SetContentLength(size_t len);

  void SetToken(std::string_view token);

  void SetAllowOrigin(std::string_view origin);

  void SetAllowHeaders(std::vector<std::string> hdrs);

  void SetAllowContentType(std::string_view origin);

  void SetAllowMethods(std::vector<std::string> methods);

  void SetAllowCredentials(std::string_view origin);

  void SetAcceptEncoding(std::vector<std::string> encoding_types);

  void SetContentEncoding(const std::string_view &encoding_type);

  void SetTransferEncoding(const std::string_view &value);

  std::string StringifyHeaders();

  // return true if successful.
  virtual std::string StringifyFirstLine() = 0;

  std::vector<std::string> headers_;
};

class HttpResponse : public HttpMessage {
public:
  explicit HttpResponse(uint16_t status_code);

  std::string StringifyFirstLine() override;

  uint16_t status_code_;

private:
  std::string version_;
};

class HttpRequest : public HttpMessage {
public:
  HttpRequest(const std::string &method, const std::string &url);

  std::string StringifyFirstLine() override;

  std::string method_;
  std::string url_;
  std::string version_;
};

}; // namespace CRequest

#endif
