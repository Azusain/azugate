#include <regex>
#include <string>
#include <string_view>

namespace azugate {
namespace utils {

bool isValidIpv4(const std::string &ipv4_address) {
  const std::regex ipv4Pattern(
      "(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[0-9][0-9]|[0-9])(\\.(25[0-5]|2[0-4][0-"
      "9]|1[0-9][0-9]|[0-9][0-9]|[0-9])){3}");
  return std::regex_match(ipv4_address, ipv4Pattern);
}

std::string toLower(const std::string_view &input) {
  auto result = std::string(input);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

}; // namespace utils
} // namespace azugate