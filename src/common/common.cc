#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace azugate {
namespace utils {
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