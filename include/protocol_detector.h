#ifndef __PROTOCOL_DETECTOR_H
#define __PROTOCOL_DETECTOR_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <string_view>
namespace azugate {
constexpr std::string_view ProtocolTypeHttp = "http";
constexpr std::string_view ProtocolTypeHttps = "https";
constexpr std::string_view ProtocolTypeTcp = "tcp";
constexpr std::string_view ProtocolTypeUdp = "udp";
constexpr std::string_view ProtocolTypeGrpc = "grpc";
constexpr std::string_view ProtocolTypeWebSockets = "websockets";
constexpr std::string_view ProtocolTypeUnknown = "";

using ProtocolType = std::string_view;

} // namespace azugate
#endif