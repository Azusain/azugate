#include "config.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace azugate {
bool Filter(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &accepted_sock_ptr,
    ConnectionInfo &connection_info) {
  auto ip_blacklist = azugate::GetIpBlackList();
  if (ip_blacklist.contains(std::string(connection_info.address))) {
    accepted_sock_ptr->close();
    SPDLOG_WARN("reject connection from {}", connection_info.address);
    return false;
  }
  return true;
}

} // namespace azugate