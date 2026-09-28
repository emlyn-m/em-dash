#include "net/shell.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <glib.h>
#include <netdb.h>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace ui {
namespace {

struct Shell_Connection {
  int wake_r = -1, wake_w = -1;
};

Shell_State state;
Shell_Connection conn;

int send_logs(const char *host, int port) {
  LogRecord records[MAX_RECORDS];
  int n = fetch_n_logs(MAX_RECORDS, records);
  if (n == 0) {
    LOG(PRI_WRN, "shell: no logs to send\n");
    return 1;
  }

  std::string payload;
  // fetch_n_logs returns newest-first; reverse for chronological order
  for (int i = n - 1; i >= 0; i--) {
    payload += "[";
    payload += records[i].prefix;
    payload += "] ";
    payload += records[i].buf;
    payload += "\n";
    free(records[i].buf);
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  char port_buf[6];
  snprintf(port_buf, sizeof port_buf, "%d", port);

  struct addrinfo *res;
  if (getaddrinfo(host, port_buf, &hints, &res)) {
    LOG(PRI_ERR, "shell: getaddrinfo failed for %s\n", host);
    return 1;
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0 || connect(sockfd, res->ai_addr, res->ai_addrlen)) {
    LOG(PRI_ERR, "shell: log connect failed to %s:%d\n", host, port);
    freeaddrinfo(res);
    if (sockfd >= 0)
      close(sockfd);
    return 1;
  }
  freeaddrinfo(res);

  const char *data = payload.c_str();
  size_t remaining = payload.size();
  while (remaining > 0) {
    ssize_t sent = send(sockfd, data, remaining, 0);
    if (sent <= 0) {
      LOG(PRI_ERR, "shell: log send failed\n");
      close(sockfd);
      return 1;
    }
    data += sent;
    remaining -= sent;
  }

  close(sockfd);
  LOG(PRI_INF, "shell: sent %d log records\n", n);
  return 0;
}

int poll_wait(int timeout) {
  struct pollfd fds[1];
  fds[0].fd = conn.wake_r;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  int nfds = 1;
  if (poll(fds, nfds, timeout) <= 0) // timeout=-1 => wait until woken
    return 0;
  if (fds[0].revents & POLLIN) {
    char buf[1];
    while (read(conn.wake_r, buf, sizeof buf) > 0) {
    } // drain (non-blocking)
    return *buf;
  }
  return 0;
}

void shell_worker(std::function<void()> on_update) {
  char cmdbuf[1024] = {0};

  for (;;) {
    int mode = poll_wait(-1);
    if (mode == 0) {
      if (state.sh_active) {
        continue;
      }

      post_to_main([on_update]() mutable {
        state.sh_active = 1;
        if (on_update)
          on_update();
      });

      snprintf(cmdbuf, 1024,
               "rm /tmp/f; mkfifo /tmp/f; cat /tmp/f | /bin/sh -i 2>&1 | tee "
               "/dev/fd/2 | nc -w %d %s %d > /tmp/f",
               state.timeout, state.ip.c_str(), state.port);
      int ecode = system(cmdbuf);

      post_to_main([ecode, on_update]() mutable {
        state.sh_ecode = ecode;
        state.lg_ecode = 0;
        state.sh_active = 0;
        if (on_update) {
          on_update();
        }
      });
    } else {
      if (state.lg_active) {
        continue;
      }

      post_to_main([on_update]() mutable {
        state.lg_active = 1;
        if (on_update)
          on_update();
      });

      int ecode = send_logs(state.ip.c_str(), state.port);

      post_to_main([ecode, on_update]() mutable {
        state.lg_ecode = ecode;
        state.sh_ecode = 0;
        state.lg_active = 0;
        if (on_update) {
          on_update();
        }
      });
    }
  }
}

} // namespace

void shell_start(std::function<void()> on_update) {
  state.ip = get_attr_str("LOG_IP");
  state.port = get_attr_long("LOG_PORT");
  state.timeout = get_attr_long("SHELL_TIMEOUT");

  state.sh_active = 0;
  state.sh_ecode = 0;
  state.lg_active = 0;
  state.lg_ecode = 0;

  int fds[2];
  if (pipe(fds) != 0) {
    LOG(PRI_ERR, "shell: pipe failed\n");
    return;
  }
  fcntl(fds[0], F_SETFL, O_NONBLOCK);
  fcntl(fds[1], F_SETFL, O_NONBLOCK);

  conn.wake_r = fds[0];
  conn.wake_w = fds[1];

  std::thread(shell_worker, std::move(on_update)).detach();
}

const Shell_State &shell_state() { return state; }

void shell_tryconn() {
  int b = 0;
  ssize_t n = write(conn.wake_w, &b, 1);
  (void)n;
}

void shell_dumplogs() {
  int b = 1;
  ssize_t n = write(conn.wake_w, &b, 1);
  (void)n;
}

} // namespace ui
