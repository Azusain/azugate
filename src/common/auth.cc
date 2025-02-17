#include "auth.h"
#include <iomanip>
#include <iostream>
#include <jwt-cpp/jwt.h>
#include <random>
#include <spdlog/spdlog.h>
#include <string>

namespace azugate {
namespace utils {

std::string GenerateSecret(size_t length) {
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, 255);
  std::ostringstream ss;
  for (size_t i = 0; i < length; ++i) {
    unsigned char byte = static_cast<unsigned char>(dist(rd));
    ss << std::setw(2) << std::setfill('0') << std::hex
       << static_cast<int>(byte);
  }
  return ss.str();
}

std::string GenerateToken(const std::string &payload,
                          const std::string &secret) {
  auto token = jwt::create()
                   .set_payload_claim("data", jwt::claim(payload))
                   .set_issued_at(std::chrono::system_clock::now())
                   .set_expires_at(std::chrono::system_clock::now() +
                                   std::chrono::hours(kDftExpiredDurationHour))
                   .set_issuer(std::string(kDftTokenIssuer))
                   .sign(jwt::algorithm::hs256{secret});
  return token;
}

bool VerifyToken(const std::string &token, const std::string &secret) {
  try {
    auto decoded_token = jwt::decode(token);
    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{secret})
                        .with_issuer(std::string(kDftTokenIssuer));
    verifier.verify(decoded_token);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("error verifying token: {}", e.what());
    return false;
  }
}

} // namespace utils
} // namespace azugate