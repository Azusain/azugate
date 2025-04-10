#include <aio.h>
#include <boost/asio.hpp>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class async_file_sender {
public:
  async_file_sender(boost::asio::io_service &io_service,
                    const std::string &filename,
                    boost::asio::ip::tcp::socket &socket)
      : io_service_(io_service), socket_(socket), filename_(filename), fd_(-1),
        offset_(0), buffer_(nullptr) {
    // 打开文件
    fd_ = open(filename.c_str(), O_RDONLY);
    if (fd_ == -1) {
      std::cerr << "Error opening file: " << strerror(errno) << std::endl;
      return;
    }

    struct stat statbuf;
    int result = fstat(fd_, &statbuf);
    if (result == -1) {
      std::cerr << "Error getting file stats: " << strerror(errno) << std::endl;
      return;
    }
    file_size_ = statbuf.st_size;
  }

  ~async_file_sender() {
    if (fd_ != -1)
      close(fd_);
  }

  void start() {
    // 使用 aio_sendfile 来异步发送文件
    send_file();
  }

private:
  void send_file() {
    struct aiocb aio_req;
    std::memset(&aio_req, 0, sizeof(aio_req));

    // AIO配置
    aio_req.aio_fildes = fd_;
    aio_req.aio_offset = offset_;
    aio_req.aio_nbytes = file_size_ - offset_;

    // 使用 sendfile 将数据从文件直接发送到套接字
    aio_req.aio_buf = nullptr; // sendfile 不需要缓冲区

    // 假设 socket_ 是一个已连接的 TCP socket，异步将数据发送到客户端
    ssize_t result =
        sendfile(socket_.native(), fd_, &offset_, aio_req.aio_nbytes);
    if (result == -1) {
      std::cerr << "Error in sendfile: " << strerror(errno) << std::endl;
      return;
    }

    // 将处理过程推送到 async_write 的回调函数
    socket_.async_write_some(
        boost::asio::null_buffers(), [this](const boost::system::error_code &ec,
                                            std::size_t bytes_transferred) {
          if (ec) {
            std::cerr << "Error writing data: " << ec.message() << std::endl;
            return;
          }
          std::cout << "Sent " << bytes_transferred << " bytes" << std::endl;
        });

    // 完成后继续发送
    offset_ += result;

    if (offset_ < file_size_) {
      // 还有更多数据，继续调用
      send_file();
    } else {
      std::cout << "File transmission complete!" << std::endl;
    }
  }

  boost::asio::io_service &io_service_;
  boost::asio::ip::tcp::socket &socket_;
  std::string filename_;
  int fd_;
  size_t file_size_;
  off_t offset_;
  char *buffer_;
};

class server {
public:
  server(boost::asio::io_service &io_service, short port)
      : acceptor_(io_service, boost::asio::ip::tcp::endpoint(
                                  boost::asio::ip::tcp::v4(), port)),
        socket_(io_service) {
    start_accept();
  }

private:
  void start_accept() {
    acceptor_.async_accept(socket_,
                           boost::bind(&server::handle_accept, this,
                                       boost::asio::placeholders::error));
  }

  void handle_accept(const boost::system::error_code &error) {
    if (error) {
      std::cerr << "Accept error: " << error.message() << std::endl;
      return;
    }

    std::cout << "Client connected" << std::endl;

    // 传递文件路径并启动文件发送
    std::string filename = "sample_file.txt"; // 你要发送的文件
    std::shared_ptr<async_file_sender> sender =
        std::make_shared<async_file_sender>(io_service_, filename, socket_);
    sender->start();

    start_accept();
  }

  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ip::tcp::socket socket_;
};

int main() {
  try {
    boost::asio::io_service io_service;
    server s(io_service, 9999);
    io_service.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }

  return 0;
}
