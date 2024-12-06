// duplicated version of v2.
#include "core.h"
#include "spdlog/spdlog.h"
#include <cerrno>
#include <netinet/in.h>

#include <sys/select.h>
#include <sys/socket.h>

#include <unistd.h>

int main(int argc, char **argv) {
  int listen_fd = Init(80);
  if (listen_fd == -1) {
    SPDLOG_ERROR("failed to init server");
    return -1;
  }

  // TODO: update kMaxFdSize dynamically.
  int clients[kMaxFdSize];
  for (int &client : clients) {
    client = -1;
  }

  fd_set r_set, all_set;
  FD_ZERO(&r_set);
  FD_ZERO(&all_set);
  FD_SET(listen_fd, &all_set);

  for (;;) {
    r_set = all_set;
    // set timeval to null so that this func would block permanently.
    int n_ready = select(kMaxFdSize, &r_set, nullptr, nullptr, nullptr);
    if (n_ready == -1) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to select");
      return -1;
    }

    // check incoming connection.
    if (FD_ISSET(listen_fd, &r_set)) {
      --n_ready;
      sockaddr_in client_addr{};
      socklen_t client_addr_len = sizeof(sockaddr_in);
      const int conn_fd =
          accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr),
                 &client_addr_len);
      if (conn_fd == -1) {
        if (errno == EINTR) {
          continue;
        }
        return -1;
      }

      FD_SET(conn_fd, &all_set);
      for (int &client : clients) {
        if (client == -1) {
          client = conn_fd;
          break;
        }
      }
    }

    // handle existed connections.
    for (int i = 0; i < kMaxFdSize && n_ready > 0; ++i) {
      if (clients[i] == -1) {
        continue;
      }
      if (FD_ISSET(clients[i], &r_set)) {
        --n_ready;
        if (FileProxy(clients[i], kPathBaseFolder) <= 0) {
          FD_CLR(clients[i], &all_set);
          close(clients[i]);
          clients[i] = -1;
        };
      }
    }
  }

  return 0;
}