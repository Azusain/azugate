#include "compression.h"
#include <cstddef>
#include <iostream>
#include <zlib.h>

namespace azugate {
namespace utils {

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

GzipCompressor::GzipCompressor(int level) {
  zstrm_.zalloc = Z_NULL;
  zstrm_.zfree = Z_NULL;
  zstrm_.opaque = Z_NULL;
  auto ret = deflateInit2(&zstrm_, level, Z_DEFLATED, MAX_WBITS + 16, 8,
                          Z_DEFAULT_STRATEGY);
  assert(ret == Z_OK);
}

GzipCompressor::~GzipCompressor() {
  // ignore the return value.
  (void)deflateEnd(&zstrm_);
}

// ref:
// https://github.com/drogonframework/drogon/blob/a3b4779540831cb8c03addb591ced3080b488917/lib/src/Utilities.cc#L893.
// https://www.zlib.net/manual.html.
// https://github.com/madler/zlib/blob/develop/examples/zpipe.c.
bool GzipCompressor::GzipStreamCompress(
    std::istream &source,
    std::function<bool(unsigned char *, size_t)> output_handler) {
  int ret, flush;
  unsigned have;
  unsigned char in[kDefaultCompressChunkBytes];
  unsigned char out[kDefaultCompressChunkBytes];
  do {
    source.read(reinterpret_cast<char *>(in), kDefaultCompressChunkBytes);
    zstrm_.avail_in = source.gcount();
    if (source.fail() && !source.eof()) {
      return false;
    }
    // use Z_FINISH to instruct the compressor to write the stream's end
    // marker.
    flush = source.eof() ? Z_FINISH : Z_NO_FLUSH;
    zstrm_.next_in = in;
    do {
      zstrm_.avail_out = kDefaultCompressChunkBytes;
      zstrm_.next_out = out;
      ret = deflate(&zstrm_, flush);
      assert(ret != Z_STREAM_ERROR);
      auto have = kDefaultCompressChunkBytes - zstrm_.avail_out;
      if (have > 0 && !output_handler(out, have)) {
        return false;
      };
    } while (zstrm_.avail_out == 0);
  } while (flush != Z_FINISH);

  return ret == Z_STREAM_END;
}

} // namespace utils
} // namespace azugate