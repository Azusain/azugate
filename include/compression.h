#ifndef __COMPRESSION_H
#define __COMPRESSION_H

#include "common.h"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <zlib.h>

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
constexpr size_t kDefaultCompressChunkSize = 100;

struct CompressionType {
  uint32_t code;
  std::string_view str;
};

class GzipCompressor {
public:
  GzipCompressor(int level = Z_DEFAULT_COMPRESSION);

  ~GzipCompressor();

  // return false if any errors occur.
  // if any errors happening in the output_handler(), simply return
  // false to terminate the internal process and prevent the errors from
  // propagating further.
  bool GzipStreamCompress(
      std::istream &source,
      std::function<bool(unsigned char *, size_t)> output_handler);

private:
  z_stream zstrm_;
};
} // namespace utils
} // namespace azugate
#endif