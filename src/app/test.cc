#include <cassert>
#include <fstream>
#include <iostream>
#include <zlib.h>

#define CHUNK 1024

class StreamCompressor {
public:
  virtual void Compress() = 0;
  virtual void Finish() = 0;

protected:
  virtual ~StreamCompressor() = 0;
};

class GzipStreamCompressor : public StreamCompressor {
public:
  GzipStreamCompressor(int level) {
    zstrm_.zalloc = Z_NULL;
    zstrm_.zfree = Z_NULL;
    zstrm_.opaque = Z_NULL;
    assert(deflateInit2(&zstrm_, level, Z_DEFLATED, MAX_WBITS + 16, 8,
                        Z_DEFAULT_STRATEGY) == Z_OK);
  }

  ~GzipStreamCompressor() { deflateEnd(&zstrm_); }

  void Compress() override {}

private:
  z_stream zstrm_;
};

int def(std::ifstream &source, std::ofstream &dest, int level) {
  int ret, flush;
  unsigned have;
  z_stream strm{.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];

  ret = deflateInit2(&strm, level, Z_DEFLATED, MAX_WBITS + 16, 8,
                     Z_DEFAULT_STRATEGY);
  if (ret != Z_OK)
    return ret;

  do {
    source.read(reinterpret_cast<char *>(in), CHUNK);
    strm.avail_in = source.gcount();
    if (source.fail() && !source.eof()) {
      (void)deflateEnd(&strm);
      return Z_ERRNO;
    }
    // use Z_FINISH to instruct the compressor to write the stream's end marker.
    flush = source.eof() ? Z_FINISH : Z_NO_FLUSH;
    strm.next_in = in;

    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = deflate(&strm, flush);
      assert(ret != Z_STREAM_ERROR);
      have = CHUNK - strm.avail_out;
      dest.write(reinterpret_cast<char *>(out), have);
      if (dest.fail()) {
        (void)deflateEnd(&strm);
        return Z_ERRNO;
      }
    } while (strm.avail_out == 0);
  } while (flush != Z_FINISH);

  assert(ret == Z_STREAM_END);
  // ignore the return value.
  (void)deflateEnd(&strm);
  return Z_OK;
}

int main() {
  const std::string input_file = "welcome.html";
  const std::string output_file = "welcome.html.gz";

  std::ifstream source(input_file, std::ios::binary);
  if (!source) {
    std::cerr << "Failed to open source file: " << input_file << std::endl;
    return 1;
  }

  std::ofstream dest(output_file, std::ios::binary);
  if (!dest) {
    std::cerr << "Failed to open destination file: " << output_file
              << std::endl;
    return 1;
  }

  // 调用 def 函数进行压缩
  int ret = def(source, dest, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) {
    std::cerr << "Compression failed!" << std::endl;
    return 1;
  }

  std::cout << "File compressed successfully: " << output_file << std::endl;

  // 关闭文件流
  source.close();
  dest.close();

  return 0;
}
