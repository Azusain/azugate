#ifndef __STRING_VALIDATOR_H
#define __STRING_VALIDATOR_H

#include <string>
namespace azugate {
namespace utils {

bool isValidIpv4(const std::string &ipv4_address);

std::string toLower(const std::string_view &input);

}; // namespace utils
} // namespace azugate

#endif