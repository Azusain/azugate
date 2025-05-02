#include <cctype>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "crequest.h"
#include "http_parser.h"

namespace CRequest {

HttpParser::HttpParser(const char *data_src, const size_t kBufferSize,
                       const size_t kMaxMsgLength)
    : state_(State::METHOD), n_total_read_(0), cur_buf_idx_(kBufferSize),
      end_buf_idx_(kBufferSize), kBufferSize_(kBufferSize),
      kMaxMsgLength_(kMaxMsgLength), io_buffer_(kBufferSize) {
  fin_.open(data_src, std::ios_base::in);
  if (!fin_) {
    std::cout << "failed to open source file as stream";
    std::terminate();
  }
}

HttpParser::HttpParser(int fd, const size_t kBufferSize,
                       const size_t kMaxMsgLength)
    : fd_(fd), state_(State::METHOD), n_total_read_(0),
      cur_buf_idx_(kBufferSize), end_buf_idx_(kBufferSize),
      kBufferSize_(kBufferSize), kMaxMsgLength_(kMaxMsgLength),
      io_buffer_(kBufferSize) {}

bool HttpParser::ReadFromFileStream() {
  if (!fin_) {
    return false;
  }
  fin_.read(io_buffer_.data(), static_cast<std::streamsize>(kBufferSize_));
  end_buf_idx_ = fin_.gcount();
  return true;
}

bool HttpParser::ReadFromSocket() {
  int len = recv(fd_, io_buffer_.data(), kBufferSize_, MSG_DONTWAIT);
  return len > 0;
}

bool HttpParser::ParseSocketStream() {
  while (true) {
    if (state_ == State::ERR) {
      return false;
    } else if (state_ == State::DONE) {
      return true;
    }
    if (cur_buf_idx_ == end_buf_idx_) {
      if (!this->ReadFromSocket()) {
        return false;
      }
      cur_buf_idx_ = 0;
    }
    this->ParseByte();
    ++cur_buf_idx_;
  }
  return false;
}

bool HttpParser::ParseFileStream() {
  while (true) {
    if (state_ == State::ERR) {
      return false;
    } else if (state_ == State::DONE) {
      return true;
    }
    if (cur_buf_idx_ == end_buf_idx_) {
      if (!this->ReadFromFileStream()) {
        return false;
      }
      cur_buf_idx_ = 0;
    }
    this->ParseByte();
    ++cur_buf_idx_;
  }
}

void HttpParser::ParseByte() {
  char cur_char = io_buffer_[cur_buf_idx_];
  // should be ascii encoded except for 'body'!!!
  if ((!isascii(cur_char)) || ++n_total_read_ > kMaxMsgLength_) {
    state_ = State::ERR;
    return;
  }
  std::cout << cur_char; // @todo -> remove this line after debugging
  switch (state_) {
  case State::METHOD:
    if (cur_char == ' ') {
      request_.method_ = msg_buffer_;
      msg_buffer_.clear();
      state_ = State::URL;
      break;
    }
    msg_buffer_.push_back(cur_char);
    break;

  case State::URL:
    if (cur_char == ' ') {
      request_.url_ = msg_buffer_;
      msg_buffer_.clear();
      state_ = State::VERSION;
      break;
    }
    msg_buffer_.push_back(cur_char);
    break;

  case State::VERSION:
    if (cur_char == '\r') {
      break;
    } else if (cur_char == '\n') {
      request_.version_ = msg_buffer_;
      msg_buffer_.clear();
      state_ = State::HEADERS;
      break;
    } else if (cur_char == ' ') {
      state_ = State::ERR;
      break;
    } else
      msg_buffer_.push_back(cur_char);
    break;

  case State::HEADERS:
    if (cur_char == '\r') {
      break;
    } else if (cur_char == '\n') {
      if (!msg_buffer_.empty()) {
        auto pos = msg_buffer_.find(':');
        if (pos == std::string::npos) {
          state_ = State::ERR;
          break;
        }
        std::string key = msg_buffer_.substr(0, pos);
        std::string val = msg_buffer_.substr(pos + 1, msg_buffer_.length());
        request_.headers_.push_back(fmt::format("{}: {}", key, val));
        msg_buffer_.clear();
        break;
      } else {
        state_ = State::DONE;
        break;
      }
    } else if (cur_char == ' ') {
      break;
    }
    msg_buffer_.push_back(static_cast<char>(::tolower(cur_char)));
    break;

  default:
    break;
  }
}

} // namespace CRequest