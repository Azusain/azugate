#include "config.h"
#include <mutex>
#include <string>

namespace azugate {
uint16_t port = 443;
uint16_t admin_port = 50051;
std::string path_config_file;
// TODO: mTLS.
std::string sslCrt;
std::string sslKey;

std::mutex config_mutex;

std::string GetConfigPath() {
  std::lock_guard<std::mutex> lock(config_mutex);
  return path_config_file;
};

void SetConfigPath(std::string &&path) {
  std::lock_guard<std::mutex> lock(config_mutex);
  path_config_file = path;
}

} // namespace azugate