#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 8080
#define FILENAME "welcome.html"

// 切换使用 sendfile（true）或 read+write（false）
constexpr bool use_sendfile = true;

void ignore_sigpipe() {
  struct sigaction sa{};
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
}

void send_with_read_write(int client_fd, const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open file: " << filename << "\n";
    return;
  }

  std::string content((std::istreambuf_iterator<char>(file)), {});
  file.close();

  std::string header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/html\r\n";
  header += "Content-Length: " + std::to_string(content.size()) + "\r\n";
  header += "Connection: close\r\n\r\n";

  if (send(client_fd, header.c_str(), header.size(), 0) <= 0)
    return;

  size_t sent = 0;
  while (sent < content.size()) {
    ssize_t n =
        send(client_fd, content.data() + sent, content.size() - sent, 0);
    if (n <= 0)
      break;
    sent += n;
  }
}

void send_with_sendfile(int client_fd, const std::string &filename) {
  int file_fd = open(filename.c_str(), O_RDONLY);
  if (file_fd < 0) {
    std::cerr << "Failed to open file: " << filename << "\n";
    return;
  }

  struct stat st{};
  if (fstat(file_fd, &st) < 0) {
    std::cerr << "Failed to stat file\n";
    close(file_fd);
    return;
  }

  std::string header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/html\r\n";
  header += "Content-Length: " + std::to_string(st.st_size) + "\r\n";
  header += "Connection: close\r\n\r\n";

  if (send(client_fd, header.c_str(), header.size(), 0) <= 0) {
    close(file_fd);
    return;
  }

  off_t offset = 0;
  while (offset < st.st_size) {
    ssize_t n = sendfile(client_fd, file_fd, &offset, st.st_size - offset);
    if (n <= 0)
      break;
  }

  close(file_fd);
}

// wrk -t4 -c100 -d10s http://localhost:8080
int main() {
  ignore_sigpipe();

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);

  if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(server_fd, 1024) < 0) {
    perror("listen");
    return 1;
  }

  std::cout << "Listening on http://localhost:" << PORT << "\n";

  while (true) {
    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      perror("accept");
      continue;
    }

    if (use_sendfile) {
      send_with_sendfile(client_fd, FILENAME);
    } else {
      send_with_read_write(client_fd, FILENAME);
    }

    close(client_fd);
  }

  close(server_fd);
  return 0;
}

// send file
// Running 10s test @ http://localhost:8080
//   4 threads and 100 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     1.37ms  312.63us   7.69ms   89.33%
//     Req/Sec    18.25k     1.09k   21.59k    72.77%
//   733348 requests in 10.10s, 207.71MB read
// Requests/sec:  72609.88
// Transfer/sec:     20.57MB

// read() + write()
// Running 10s test @ http://localhost:8080
//   4 threads and 100 connections
//   Thread Stats   Avg      Stdev     Max   +/- Stdev
//     Latency     2.35ms  336.05us   9.04ms   90.03%
//     Req/Sec    10.64k     1.59k   41.66k    99.75%
//   424462 requests in 10.10s, 120.23MB read
// Requests/sec:  42029.64
// Transfer/sec:     11.90MB
