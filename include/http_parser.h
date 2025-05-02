#include <fstream>

#include <vector>

#include "crequest.h"

namespace CRequest {

enum class State {
  METHOD,
  URL,
  VERSION,
  HEADERS,
  DONE,
  ERR,
};

class HttpParser {
public:
  CRequest::HttpRequest request_;

  explicit HttpParser(const char *data_src, const size_t kBufferSize = 512,
                      const size_t kMaxMsgLength = 4096);

  explicit HttpParser(int fd, const size_t kBufferSize = 512,
                      const size_t kMaxMsgLength = 4096);

  bool ReadFromFileStream();

  bool ReadFromSocket();

  bool ParseFileStream();

  bool ParseSocketStream();

  void ParseByte();

private:
  std::vector<char> io_buffer_;
  std::string msg_buffer_;
  State state_;
  size_t end_buf_idx_;
  size_t cur_buf_idx_;
  size_t n_total_read_;
  const size_t kBufferSize_;
  const size_t kMaxMsgLength_;

  //    data source (optional)
  std::fstream fin_;
  int fd_;
};

} // namespace CRequest