#include "compression.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
int main() {
  // try {
  //   // 输入字符串
  //   std::string inputData = "This is a test string for Gzip compression.";
  //   std::istringstream inputStream(inputData); // 将字符串包装为输入流

  //   // 输出流
  //   std::ostringstream compressedStream;

  //   // 调用压缩函数
  //   azugate::utils::CompressGzipStream(inputData.data(), inputData.length(),
  //                                      compressedStream);

  //   // 将压缩结果保存到文件
  //   std::ofstream outputFile("compressed_output.gz", std::ios::binary);
  //   if (!outputFile.is_open()) {
  //     throw std::runtime_error("Failed to open output file.");
  //   }
  //   outputFile << compressedStream.str();
  //   outputFile.close();

  //   std::cout << "Compression successful. Compressed data saved to "
  //                "'compressed_output.gz'."
  //             << std::endl;
  // } catch (const std::exception &e) {
  //   std::cerr << "Error: " << e.what() << std::endl;
  //   return 1;
  // }

  return 0;
}