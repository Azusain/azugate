#include "auth.h"
#include <iostream>

int main() {
  std::string secret = azugate::utils::GenerateSecret();
  std::cout << "Generated Secret: " << secret << std::endl;

  std::string payload = "{\"user_id\":\"12345\"}";

  std::string token = azugate::utils::GenerateToken(payload, secret);
  std::cout << "Generated Token: " << token << std::endl;

  bool is_valid = azugate::utils::VerifyToken(token, secret);
  std::cout << "Is the token valid? " << (is_valid ? "Yes" : "No") << std::endl;
  return 0;
}
