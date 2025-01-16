#include <iostream>
#include <string_view>

namespace azugate {
namespace utils {

constexpr std::string_view kCompressionTypeGzip = "gzip";
constexpr std::string_view kCompressionTypeBrotli = "brotli";
constexpr std::string_view kCompressionTypeDeflate = "deflate";
constexpr std::string_view kCompressionTypeZStandard = "zstd";
constexpr std::string_view kCompressionTypeNone = "";

void CompressGzipStream(char *input_buffer, size_t input_length,
                        std::ostream &out);
} // namespace utils
} // namespace azugate