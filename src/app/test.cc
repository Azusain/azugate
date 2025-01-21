#include "compression.h"
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>

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

  azugate::utils::GzipCompressor compressor;

  int ret = compressor.GzipStreamCompress(
      source, [&dest](unsigned char *data, size_t size) {
        dest.write(reinterpret_cast<char *>(data), size);
        return !dest.fail();
      });
  if (!ret) {
    std::cerr << "Compression failed!" << std::endl;
    return 1;
  }

  std::cout << "File compressed successfully: " << output_file << std::endl;

  source.close();
  dest.close();

  return 0;
}
