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
  // Basic validation first
  if (token.empty() || secret.empty()) {
    SPDLOG_WARN("empty token or secret provided");
    return false;
  }
  
  // Check token format (JWT should have 3 parts separated by dots)
  size_t first_dot = token.find('.');
  if (first_dot == std::string::npos) {
    SPDLOG_WARN("invalid token format: missing first dot");
    return false;
  }
  
  size_t second_dot = token.find('.', first_dot + 1);
  if (second_dot == std::string::npos) {
    SPDLOG_WARN("invalid token format: missing second dot");
    return false;
  }
  
  try {
    auto decoded_token = jwt::decode(token);
    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{secret})
                        .with_issuer(std::string(kDftTokenIssuer));
    verifier.verify(decoded_token);
    SPDLOG_DEBUG("validate token successfully");
    return true;
  } catch (const jwt::error::invalid_json_exception& e) {
    SPDLOG_WARN("token contains invalid JSON: {}", e.what());
    return false;
  } catch (const jwt::error::token_verification_exception& e) {
    SPDLOG_WARN("token verification failed: {}", e.what());
    return false;
  } catch (const jwt::error::signature_verification_exception& e) {
    SPDLOG_WARN("token signature verification failed: {}", e.what());
    return false;
  } catch (const std::exception &e) {
    SPDLOG_WARN("unexpected error verifying token: {}", e.what());
    return false;
  }
}

} // namespace utils
} // namespace azugate