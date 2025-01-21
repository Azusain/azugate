#ifndef __COMPRESSION_H
#define __COMPRESSION_H

#include "common.h"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace azugate {
namespace utils {

constexpr std::string_view kCompressionTypeStrGzip = "gzip";
constexpr std::string_view kCompressionTypeStrBrotli = "brotli";
constexpr std::string_view kCompressionTypeStrDeflate = "deflate";
constexpr std::string_view kCompressionTypeStrZStandard = "zstd";
constexpr std::string_view kCompressionTypeStrNone = "";
constexpr uint32_t kCompressionTypeCodeGzip = HashConstantString("gzip");
constexpr uint32_t kCompressionTypeCodeBrotli = HashConstantString("brotli");
constexpr uint32_t kCompressionTypeCodeDeflate = HashConstantString("deflate");
constexpr uint32_t kCompressionTypeCodeZStandard = HashConstantString("zstd");
constexpr uint32_t kCompressionTypeCodeNone = HashConstantString("");

struct CompressionType {
  uint32_t code;
  std::string_view str;
};

void CompressGzipStream(boost::iostreams::filtering_ostream &fo,
                        char *input_buffer, size_t input_length,
                        std::ostream &out); // namespace utils
} // namespace utils
} // namespace azugate
#endif