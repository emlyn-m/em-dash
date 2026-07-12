#include "net/http.hpp"

#include "log.hpp"

#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace ui {

int http_get(const char *hostname, const char *path, int port, char **out,
             time_t *ping) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  char port_buf[6];
  snprintf(port_buf, sizeof port_buf, "%d", port);

  time_t time_start = time(nullptr);

  struct addrinfo *res;
  if (getaddrinfo(hostname, port_buf, &hints, &res)) {
    LOG(PRI_ERR, "getaddrinfo failed for %s\n", hostname);
    return 1;
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0 || connect(sockfd, res->ai_addr, res->ai_addrlen)) {
    LOG(PRI_ERR, "connect failed for %s\n", hostname);
    freeaddrinfo(res);
    if (sockfd >= 0) close(sockfd);
    return 1;
  }
  freeaddrinfo(res);

  // Connection: close lets us read the whole body by recv'ing until EOF.
  char header[2048];
  int header_len =
      snprintf(header, sizeof header,
               "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path,
               hostname);
  if (send(sockfd, header, header_len, 0) < 0) {
    LOG(PRI_ERR, "send failed for %s\n", hostname);
    close(sockfd);
    return 1;
  }

  std::string response;
  char buf[4096];
  ssize_t n;
  while ((n = recv(sockfd, buf, sizeof buf, 0)) > 0)
    response.append(buf, n);
  close(sockfd);

  if (n < 0) {
    LOG(PRI_ERR, "recv failed for %s\n", hostname);
    return 1;
  }
  *ping = time(nullptr) - time_start;

  size_t sep = response.find("\r\n\r\n");
  if (sep == std::string::npos) {
    LOG(PRI_ERR, "no header/body separator from %s\n", hostname);
    return 1;
  }
  const char *body = response.c_str() + sep + 4;
  size_t body_len = response.size() - (sep + 4);

  char *result = (char *)realloc(*out, body_len + 1);
  if (!result) {
    LOG(PRI_ERR, "realloc %zu bytes failed\n", body_len + 1);
    return 1;
  }
  memcpy(result, body, body_len);
  result[body_len] = '\0';
  *out = result;
  return 0;
}

}  // namespace ui
