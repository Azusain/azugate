#include <regex>
#include <string>

namespace azugate {
namespace utils {

bool isValidIpv4(const std::string &ipv4_address) {
  const std::regex ipv4Pattern(
      "(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[0-9][0-9]|[0-9])(\\.(25[0-5]|2[0-4][0-"
      "9]|1[0-9][0-9]|[0-9][0-9]|[0-9])){3}");
  return std::regex_match(ipv4_address, ipv4Pattern);
}

}; // namespace utils
} // namespace azugate