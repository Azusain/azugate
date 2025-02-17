#ifndef __AUTH_H
#define __AUTH_H

#include <jwt-cpp/jwt.h>
#include <string>
#include <string_view>

namespace azugate {
namespace utils {

constexpr int kDftExpiredDurationHour = 1;
constexpr std::string_view kDftTokenIssuer = "azugate";

std::string GenerateSecret(size_t length = 32);

std::string GenerateToken(const std::string &payload,
                          const std::string &secret);

bool VerifyToken(const std::string &token, const std::string &secret);

} // namespace utils
} // namespace azugate

#endif // AUTH_H