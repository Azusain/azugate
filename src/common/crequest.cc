#include "crequest.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

namespace CRequest {
namespace utils {
constexpr const char *GetMessageFromStatusCode(uint16_t status_code) {
  switch (status_code) {
  case kHttpContinue:
    return "Continue";
  case kHttpSwitchingProtocols:
    return "Switching Protocols";
  case kHttpOk:
    return "OK";
  case kHttpCreated:
    return "Created";
  case kHttpAccepted:
    return "Accepted";
  case kHttpNonAuthoritativeInformation:
    return "Non-Authoritative Information";
  case kHttpNoContent:
    return "No Content";
  case kHttpResetContent:
    return "Reset Content";
  case kHttpPartialContent:
    return "Partial Content";
  case kHttpMultipleChoices:
    return "Multiple Choices";
  case kHttpMovedPermanently:
    return "Moved Permanently";
  case kHttpFound:
    return "Found";
  case kHttpSeeOther:
    return "See Other";
  case kHttpNotModified:
    return "Not Modified";
  case kHttpUseProxy:
    return "Use Proxy";
  case kHttpTemporaryRedirect:
    return "Temporary Redirect";
  case kHttpBadRequest:
    return "Bad Request";
  case kHttpUnauthorized:
    return "Unauthorized";
  case kHttpPaymentRequired:
    return "Payment Required";
  case kHttpForbidden:
    return "Forbidden";
  case kHttpNotFound:
    return "Not Found";
  case kHttpMethodNotAllowed:
    return "Method Not Allowed";
  case kHttpNotAcceptable:
    return "Not Acceptable";
  case kHttpProxyAuthenticationRequired:
    return "Proxy Authentication Required";
  case kHttpRequestTimeout:
    return "Request Timeout";
  case kHttpConflict:
    return "Conflict";
  case kHttpGone:
    return "Gone";
  case kHttpLengthRequired:
    return "Length Required";
  case kHttpPreconditionFailed:
    return "Precondition Failed";
  case kHttpPayloadTooLarge:
    return "Payload Too Large";
  case kHttpUriTooLong:
    return "URI Too Long";
  case kHttpUnsupportedMediaType:
    return "Unsupported Media Type";
  case kHttpRangeNotSatisfiable:
    return "Range Not Satisfiable";
  case kHttpExpectationFailed:
    return "Expectation Failed";
  case kHttpUpgradeRequired:
    return "Upgrade Required";
  case kHttpInternalServerError:
    return "Internal Server Error";
  case kHttpNotImplemented:
    return "Not Implemented";
  case kHttpBadGateway:
    return "Bad Gateway";
  case kHttpServiceUnavailable:
    return "Service Unavailable";
  case kHttpGatewayTimeout:
    return "Gateway Timeout";
  case kHttpHttpVersionNotSupported:
    return "HTTP Version Not Supported";
  default:
    return "Unknown Status";
  }
}

std::string_view GetContentTypeFromSuffix(const std::string_view &path) {
  uint32_t hashed_path = azugate::utils::HashConstantString(path);
  switch (hashed_path) {
  case kFileExtensionJson:
    return kContentTypeAppJson;

  case kFileExtensionXml:
    return kContentTypeAppXml;

  case kFileExtensionIso:
  case kFileExtensionExe:
  case kFileExtensionBin:
    return kContentTypeAppOctet;

  case kFileExtensionHtm:
  case kFileExtensionHtml:
    return kContentTypeTextHtml;

  case kFileExtensionTxt:
  case kFileExtensionLog:
  case kFileExtensionIni:
  case kFileExtensionCfg:
    return kContentTypeTextPlain;

  case kFileExtensionPng:
    return kContentTypeImgPng;

  case kFileExtensionJpg:
  case kFileExtensionJpeg:
    return kContentTypeImgJpeg;

  case kFileExtensionSvg:
    return kContentTypeImgSvg;

  case kFileExtensionXIcon:
    return kContentTypeXIcon;

  case kFileExtensionCss:
    return kContentTypeTextCss;

  case kFileExtensionJavaScript:
    return kContentTypeAppJs;

  default:
    return kContentTypeAppOctet;
  }
};
} // namespace utils

HttpMessage::~HttpMessage() = default;

// class HttpMessage.
void HttpMessage::SetCookie(std::string_view key, std::string_view val) {
  headers_.emplace_back(fmt::format("Set-Cookie:{}={}", key, val));
}

void HttpMessage::SetKeepAlive(bool keep_alive) {
  headers_.emplace_back(fmt::format(
      "Connection:{}", keep_alive ? kConnectionKeepAlive : kConnectionClose));
}

void HttpMessage::SetContentType(std::string_view content_type) {
  headers_.emplace_back(fmt::format("Content-Type:{}", content_type));
}

void HttpMessage::SetContentLength(size_t len) {
  headers_.emplace_back(fmt::format("Content-Length:{}", len));
}

void HttpMessage::SetToken(std::string_view token) {
  headers_.emplace_back(fmt::format("Token:{}", token));
}

void HttpMessage::SetAllowOrigin(std::string_view origin) {
  headers_.emplace_back(fmt::format("Access-Control-Allow-Origin:{}", origin));
}

// cors
void HttpMessage::SetAllowHeaders(std::vector<std::string> hdrs) {
  std::string allow_hdrs{"Access-Control-Allow-Headers: "};
  size_t hdr_count = hdrs.size();
  for (size_t i = 0; i < hdr_count; ++i) {
    allow_hdrs.append(hdrs[i]);
    if (i != (hdr_count - 1)) {
      allow_hdrs.append(",");
    }
  }
  headers_.emplace_back(std::move(allow_hdrs));
}

// cors.
void HttpMessage::SetAllowMethods(std::vector<std::string> methods) {
  std::string allow_methods{"Access-Control-Allow-Methods: "};
  for (size_t i = 0; i < methods.size(); ++i) {
    allow_methods.append(methods[i]);
    if (i != (methods.size() - 1)) {
      allow_methods.append(",");
    }
  }
  headers_.emplace_back(std::move(allow_methods));
}

// compression.
void HttpMessage::SetAcceptEncoding(std::vector<std::string> encoding_types) {
  std::string encodings{fmt::format("{}: ", kHeaderFieldAcceptEncoding)};
  for (size_t i = 0; i < encoding_types.size(); ++i) {
    encodings.append(encoding_types[i]);
    if (i != (encoding_types.size() - 1)) {
      encodings.append(",");
    }
  }
}

void HttpMessage::SetContentEncoding(const std::string_view &encoding_type) {
  headers_.emplace_back(
      fmt::format("{}: {}", kHeaderFieldContentEncoding, encoding_type));
}

void HttpMessage::SetTransferEncoding(const std::string_view &value) {
  headers_.emplace_back(
      fmt::format("{}: {}", kHeaderFieldTransferEncoding, value));
};

std::string HttpMessage::StringifyHeaders() {
  std::string str_headers;
  for (auto &h : headers_) {
    str_headers.append(h);
    str_headers.append(kCrlf);
  }
  str_headers.append(kCrlf);
  return str_headers;
}

// class HttpRequest.
std::string HttpRequest::StringifyFirstLine() {
  return fmt::format("{} {} {}", method_, url_, version_);
}

HttpRequest::HttpRequest(const std::string &method, const std::string &url)
    : method_(method), url_(url), version_(kHttpVersion011) {}

// class HttpResponse.
HttpResponse::HttpResponse(uint16_t status_code)
    : status_code_(status_code), version_(kHttpVersion011) {}

std::string HttpResponse::StringifyFirstLine() {
  return fmt::format("{} {} {}{}", version_, status_code_,
                     utils::GetMessageFromStatusCode(status_code_), kCrlf);
}

} // namespace CRequest
