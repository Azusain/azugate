#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <brotli/encode.h>
#include <cstddef>
#include <iostream>
#include <ostream>

namespace azugate {
namespace utils {

// ref:
// https://github.com/drogonframework/drogon/blob/a3b4779540831cb8c03addb591ced3080b488917/lib/src/Utilities.cc#L893.
// https://www.zlib.net/manual.html.
void CompressGzipStream(boost::iostreams::filtering_ostream &fo,
                        char *input_buffer, size_t input_length,
                        std::ostream &out) {
  fo.write(input_buffer, input_length);
  return;
}

// TODO: implement this.
void CompressBrotliStream(char *input_buffer, size_t input_length,
                          char *output_buffer, size_t output_length) {
  BrotliEncoderState *encoder =
      BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
  // TODO: these params should be configured by the user.
  BrotliEncoderSetParameter(encoder, BROTLI_PARAM_QUALITY, 11);
  BrotliEncoderSetParameter(encoder, BROTLI_PARAM_LGWIN, 22);

  BrotliEncoderDestroyInstance(encoder);
}

} // namespace utils
} // namespace azugate