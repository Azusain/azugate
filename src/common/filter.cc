#include "config.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <spdlog/spdlog.h>

namespace azugate {
bool Filter(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> &accepted_sock_ptr) {
  auto ipv4_addr = accepted_sock_ptr->remote_endpoint().address().to_string();
  SPDLOG_INFO("connection from {}", ipv4_addr);
  auto ip_blacklist = azugate::GetIpBlackList();
  if (ip_blacklist.contains(ipv4_addr)) {
    accepted_sock_ptr->close();
    SPDLOG_WARN("reject connection from {}", ipv4_addr);
    return false;
  }
  return true;
}

} // namespace azugate