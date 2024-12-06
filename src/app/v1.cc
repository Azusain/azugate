#include <cerrno>

#include "core.h"
#include "spdlog/spdlog.h"
#include <cstddef>

#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// TODO: why it works when we add SA_RESTART?
int Signal(int signo, void(func)(int)) {
  struct sigaction act;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0 | SA_RESTART;
  if (sigaction(signo, &act, NULL) < -1) {
    return -1;
  }
  return 0;
}

void sigchild_handler(int signo) {
  pid_t pid;
  int stat;
  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
    SPDLOG_INFO("child {} exited", pid);
  }
  return;
}

int main() {
  // signal handler.
  if (Signal(SIGCHLD, sigchild_handler) == -1) {
    SPDLOG_ERROR("failed to setup signal handler");
    return -1;
  };

  int listen_fd = Init(kPort);
  if (listen_fd == -1) {
    SPDLOG_ERROR("failed to init server");
  }

  for (;;) {
    sockaddr_in addr;
    socklen_t sock_len;
    int conn_fd = accept(listen_fd, (sockaddr *)&addr, &sock_len);
    if (conn_fd == -1) {
      if (errno == EINTR) {
        continue;
      }
      SPDLOG_ERROR("failed to accept new connections");
      return -1;
    }
    pid_t pid = fork();
    if (!pid) {
      close(listen_fd);
      // TODO: more elegant way?
      if (Echo(conn_fd) == -1) {
        SPDLOG_ERROR("failed to echo");
      }
      exit(0);
    }
    close(conn_fd);
  }

  return 0;
}
