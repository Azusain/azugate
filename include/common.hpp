#ifndef __COMMON_H
#define __COMMON_H

#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string_view>

namespace azugate {
namespace utils {
inline std::string UrlEncode(const std::string &str) {
  std::ostringstream encoded;
  for (size_t i = 0; i < str.length(); ++i) {
    unsigned char c = str[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      encoded << c;
    } else {
      encoded << '%' << std::uppercase << std::setw(2) << std::setfill('0')
              << static_cast<int>(c);
    }
  }
  return encoded.str();
}

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

namespace network {

// return an empty string if target not found in the url.
inline std::string ExtractParamFromUrl(const std::string &url,
                                       const std::string &key) {
  // find the position of the query string in the URL.
  size_t query_start = url.find("?");
  if (query_start == std::string::npos) {
    return "";
  }
  // extract the query string (substring after '?').
  std::string query = url.substr(query_start + 1);
  // split the query string by '&' to get individual key-value pairs.
  std::map<std::string, std::string> params;
  size_t pos = 0;
  while ((pos = query.find("&")) != std::string::npos) {
    std::string param = query.substr(0, pos);
    size_t eq_pos = param.find("=");
    if (eq_pos != std::string::npos) {
      std::string param_key = param.substr(0, eq_pos);
      std::string param_value = param.substr(eq_pos + 1);
      params[param_key] = param_value;
    }
    query.erase(0, pos + 1);
  }
  // handle the last parameter (if any).
  size_t eq_pos = query.find("=");
  if (eq_pos != std::string::npos) {
    std::string param_key = query.substr(0, eq_pos);
    std::string param_value = query.substr(eq_pos + 1);
    params[param_key] = param_value;
  }
  // return the value for the requested key if found, otherwise return empty
  // string.
  auto it = params.find(key);
  if (it != params.end()) {
    return it->second;
  }
  return "";
}

} // namespace network
} // namespace azugate

#endif