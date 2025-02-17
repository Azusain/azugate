#ifndef __COMMON_H
#define __COMMON_H

#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace azugate {
namespace utils {

inline std::string FindFileExtension(std::string &&path) {
  auto pos = path.find_last_of(".");
  if (pos != std::string::npos && pos + 2 <= path.length()) {
    return path.substr(pos + 1);
  }
  return "";
}

static constexpr uint32_t HashConstantString(const std::string_view &str) {
  uint32_t hash = 0;
  for (const char &c : str) {
    hash = hash * 31 + static_cast<uint32_t>(c);
  }
  return hash;
};

// only used for testing.
inline void PrintBufferAsHex(const boost::asio::const_buffer &buffer) {
  const uint8_t *data = boost::asio::buffer_cast<const uint8_t *>(buffer);
  size_t size = boost::asio::buffer_size(buffer);
  std::cout << "HEX DATA START-> \n";
  for (size_t i = 0; i < size; ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i]
              << " ";
  }
  std::cout << std::endl;
  std::cout << "<- HEX DATA END\n";
}
} // namespace utils

} // namespace azugate

#endif