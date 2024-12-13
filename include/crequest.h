#ifndef __CREQUEST_H
#define __CREQUEST_H

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
// http connection.
constexpr std::string_view kConnectionClose = "Close";
constexpr std::string_view kConnectionKeepAlive = "keep-alive";
// http content type.
constexpr std::string_view kContentTypeAppJson = "application/json";
constexpr std::string_view kContentTypeAppUrlencoded =
    "application/x-www-form-urlencoded";
constexpr std::string_view kContentTypeAppXml = "application/xml";
constexpr std::string_view kContentTypeAppOctet = "application/octet-stream";
constexpr std::string_view kContentTypeTextHtml = "text/html";
constexpr std::string_view kContentTypeTextPlain = "text/plain";
constexpr std::string_view kContentTypeImgPng = "image/png";
constexpr std::string_view kContentTypeImgJpeg = "image/jpeg";
constexpr std::string_view kContentTypeXIcon = "image/x-icon";
// supported versions.
constexpr std::string_view kHttpVersion011 = "HTTP/1.1";
// some parser stuffs.
constexpr std::string_view kCrlf = "\r\n";
constexpr std::string_view kSpace = " ";
// local file extensions;
static constexpr uint32_t
HashFileSuffix(const std::string_view &file_extension) {
  uint32_t hash = 0;
  for (const char &c : file_extension) {
    hash = hash * 31 + static_cast<uint32_t>(c);
  }
  return hash;
};
constexpr uint32_t kFileExtensionJson = HashFileSuffix("json");
constexpr uint32_t kFileExtensionXml = HashFileSuffix("xml");
constexpr uint32_t kFileExtensionBin = HashFileSuffix("bin");
constexpr uint32_t kFileExtensionExe = HashFileSuffix("exe");
constexpr uint32_t kFileExtensionIso = HashFileSuffix("iso");
constexpr uint32_t kFileExtensionHtml = HashFileSuffix("html");
constexpr uint32_t kFileExtensionHtm = HashFileSuffix("htm");
constexpr uint32_t kFileExtensionTxt = HashFileSuffix("txt");
constexpr uint32_t kFileExtensionLog = HashFileSuffix("log");
constexpr uint32_t kFileExtensionCfg = HashFileSuffix("cfg");
constexpr uint32_t kFileExtensionIni = HashFileSuffix("ini");
constexpr uint32_t kFileExtensionPng = HashFileSuffix("png");
constexpr uint32_t kFileExtensionJpg = HashFileSuffix("jpg");
constexpr uint32_t kFileExtensionJpeg = HashFileSuffix("jpeg");
constexpr uint32_t kFileExtensionXIcon = HashFileSuffix("ico");

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

  void SetContentLen(size_t len);

  void SetToken(std::string_view token);

  void SetAllowOrigin(std::string_view origin);

  void SetAllowHeaders(std::vector<std::string> hdrs);

  void SetAllowContentType(std::string_view origin);

  void SetAllowMethods(std::vector<std::string> methods);

  void SetAllowCredentials(std::string_view origin);

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
