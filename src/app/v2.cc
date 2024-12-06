#include "core.h"
#include "spdlog/spdlog.h"
#include <cerrno>
#include <netinet/in.h>

#include <sys/select.h>
#include <sys/socket.h>

#include <unistd.h>

int main(int argc, char **argv) {
  int listen_fd = Init(kPort);
  if (listen_fd == -1) {
    SPDLOG_ERROR("faild to init server");
    return -1;
  }

  // TODO: update kMaxFdSize dynamically.
  int clients[kMaxFdSize];
  for (int i = 0; i < kMaxFdSize; ++i) {
    clients[i] = -1;
  }

  fd_set rset, allset;
  FD_ZERO(&rset);
  FD_ZERO(&allset);
  FD_SET(listen_fd, &allset);

  for (;;) {
    rset = allset;
    // set timeval to null so that this func would block permantly.
    int nready = select(kMaxFdSize, &rset, nullptr, nullptr, nullptr);
    if (nready == -1) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to select");
      return -1;
    }

    // check incoming connection.
    if (FD_ISSET(listen_fd, &rset)) {
      --nready;
      sockaddr_in client_addr;
      socklen_t client_addr_len = sizeof(sockaddr_in);
      int conn_fd =
          accept(listen_fd, (sockaddr *)&client_addr, &client_addr_len);
      if (conn_fd == -1) {
        if (errno == EINTR) {
          continue;
        }
        return -1;
      }

      FD_SET(conn_fd, &allset);
      for (int i = 0; i < kMaxFdSize; ++i) {
        if (clients[i] == -1) {
          clients[i] = conn_fd;
          break;
        }
      }
    }

    // handle existed connections.
    for (int i = 0; i < kMaxFdSize && nready > 0; ++i) {
      if (clients[i] == -1) {
        continue;
      }
      if (FD_ISSET(clients[i], &rset)) {
        --nready;
        if (Echo(clients[i]) <= 0) {
          FD_CLR(clients[i], &allset);
          clients[i] = -1;
        };
      }
    }
  }

  return 0;
}