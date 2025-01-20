#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <brotli/encode.h>
#include <cstddef>
#include <iostream>
#include <ostream>

namespace azugate {
namespace utils {

void CompressGzipStream(const boost::iostreams::gzip_compressor &compressor,
                        char *input_buffer, size_t input_length,
                        std::ostream &out) {
  boost::iostreams::filtering_ostream fo;
  fo.push(compressor);
  fo.push(out);
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