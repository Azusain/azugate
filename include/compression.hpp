#ifndef __COMPRESSION_H
#define __COMPRESSION_H

#include "common.hpp"
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
constexpr size_t kDefaultCompressChunkBytes = 100;

struct CompressionType {
  uint32_t code;
  std::string_view str;
};

// TODO: ignore q-factor currently, for example:
// Accept-Encoding: gzip; q=0.8, br; q=0.9, deflate.
inline CompressionType
GetCompressionType(const std::string_view &supported_compression_types_str) {
  // gzip is the preferred encoding in azugate.
  if (supported_compression_types_str.find(utils::kCompressionTypeStrGzip) !=
      std::string::npos) {
    return utils::CompressionType{.code = utils::kCompressionTypeCodeGzip,
                                  .str = utils::kCompressionTypeStrGzip};
  } else if (supported_compression_types_str.find(
                 utils::kCompressionTypeStrBrotli) != std::string::npos) {
    return utils::CompressionType{.code = utils::kCompressionTypeCodeBrotli,
                                  .str = utils::kCompressionTypeStrBrotli};
  }
  return utils::CompressionType{.code = utils::kCompressionTypeCodeNone,
                                .str = utils::kCompressionTypeStrNone};
}

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