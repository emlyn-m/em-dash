#include "net/jwt.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace ui {

bool generate_gcal_jwt(const char *service_email, const char *privkey,
                       std::string &out) {
  // Header is a constant: {"alg":"RS256","typ":"JWT"}.
  const char *header = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";

  char claims[1024];
  snprintf(claims, sizeof claims,
           "{\\\"iss\\\": \\\"%s\\\", \\\"scope\\\": "
           "\\\"https://www.googleapis.com/auth/calendar\\\", \\\"aud\\\": "
           "\\\"https://oauth2.googleapis.com/token\\\", \\\"exp\\\": %ld, "
           "\\\"iat\\\": %ld}",
           service_email, time(nullptr) + 3600, time(nullptr));

  // Base64url the header.claims, sign with the private key via openssl, then
  // append the base64url signature. TODO: replace shell-out with a real lib.
  const char *tmp_path = "/tmp/jwt_header_claim.dat";
  const char *sig_path = "/tmp/jwt_header_claim.sig";
  char cmd[8192];
  snprintf(cmd, sizeof cmd,
           "echo -n $(echo -n %s).$(echo -n %s | base64 | tr '+/' '-_' | tr -d "
           "'=') | tr -d ' ' > %s && echo -n $'%s' | openssl dgst -sha256 "
           "-sign /dev/stdin -out %s %s 2>/dev/null && echo -n $(cat %s).$(cat "
           "%s | base64 -w 0 | tr '+/' '-_' | tr -d '=')",
           header, claims, tmp_path, privkey, sig_path, tmp_path, tmp_path,
           sig_path);

  FILE *fp = popen(cmd, "r");
  if (!fp) return false;

  char buf[8192] = {0};
  bool ok = fgets(buf, sizeof buf, fp) != nullptr;
  pclose(fp);
  if (!ok) return false;

  out = buf;
  return true;
}

}  // namespace ui
