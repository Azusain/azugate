#ifndef __COMMON_H
#define __COMMON_H

#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <string_view>

namespace azugate {
namespace utils {
static constexpr uint32_t HashConstantString(const std::string_view &str) {
  uint32_t hash = 0;
  for (const char &c : str) {
    hash = hash * 31 + static_cast<uint32_t>(c);
  }
  return hash;
};

inline void PrintBufferAsHex(const boost::asio::const_buffer &buffer);

} // namespace utils

} // namespace azugate

#endif