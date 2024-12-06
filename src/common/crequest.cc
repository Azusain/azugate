#include "crequest.h"

#include "core.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

namespace CRequest {

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

// class HttpMessage.
void HttpMessage::SetCookie(std::string_view key, std::string_view val) {
  headers_.emplace_back(std::format("Set-Cookie:{}={}", key, val));
}

void HttpMessage::SetKeepAlive(bool keep_alive) {
  headers_.emplace_back(std::format(
      "Connection:{}", keep_alive ? kConnectionKeepAlive : kConnectionClose));
}

void HttpMessage::SetContentType(std::string_view content_type) {
  headers_.emplace_back(std::format("Content-Type:{}", content_type));
}

void HttpMessage::SetContentLen(size_t len) {
  headers_.emplace_back(std::format("Content-Length:{}", len));
}

void HttpMessage::SetToken(std::string_view token) {
  headers_.emplace_back(std::format("Token:{}", token));
}

void HttpMessage::SetAllowOrigin(std::string_view origin) {
  headers_.emplace_back(std::format("Access-Control-Allow-Origin:{}", origin));
}

// TODO: this seems incorrect.
void HttpMessage::SetAllowHeaders(std::vector<std::string> hdrs) {
  std::string allow_hdrs;
  size_t hdr_count = hdrs.size();
  for (size_t i = 0; i < hdr_count; ++i) {
    allow_hdrs.append(hdrs[i]);
    if (i != (hdr_count - 1)) {
      allow_hdrs.append(",");
    }
  }
  headers_.emplace_back(std::move(allow_hdrs));
}

// TODO: this seems incorrect.
void HttpMessage::SetAllowMethods(std::vector<std::string> methods) {
  std::string allow_methods;
  for (size_t i = 0; i < methods.size(); ++i) {
    allow_methods.append(methods[i]);
    if (i != (methods.size() - 1)) {
      allow_methods.append(",");
    }
  }
  headers_.emplace_back(std::move(allow_methods));
}

inline bool HttpMessage::writeDelimiter(int fd) {
  if (Writen(fd, kCrlf.data(), kCrlf.length()) == -1) {
    SPDLOG_ERROR("failed to write delimiters");
    return false;
  }
  return true;
}

// return true if successful.
bool HttpMessage::SendHeader(int fd) {
  if (!sendFirstLine(fd)) {
    SPDLOG_ERROR("failed to write header line");
    return false;
  }
  if (!writeDelimiter(fd)) {
    return false;
  }

  if (headers_.size() > kMaxHeadersNum) {
    SPDLOG_ERROR("the number of the headers has excceded the limit");
    return false;
  }

  // write headers.
  for (auto &header : headers_) {
    if (Writen(fd, header.c_str(), header.length()) == -1) {
      SPDLOG_ERROR("failed to write header");
      return false;
    }
    if (!writeDelimiter(fd)) {
      return false;
    }
  }
  if (!writeDelimiter(fd)) {
    return false;
  }

  return true;
}

// class HttpRequest.
bool HttpRequest::sendFirstLine(int fd) {
  std::string header_line = std::format("{} {} {}", method_, url_, version_);
  return Writen(fd, header_line.c_str(), header_line.length()) != -1;
}

HttpRequest::HttpRequest(const std::string &method, const std::string &url)
    : method_(method), url_(url), version_(kHttpVersion011) {}

// class HttpResponse.
HttpResponse::HttpResponse(uint16_t status_code)
    : status_code_(status_code), version_(kHttpVersion011) {}

bool HttpResponse::sendFirstLine(int fd) {
  std::string header_line = std::format("{} {} {}", version_, status_code_,
                                        GetMessageFromStatusCode(status_code_));
  return Writen(fd, header_line.c_str(), header_line.length()) != -1;
}

} // namespace CRequest
