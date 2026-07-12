#pragma once

#include <string>

namespace ui {

// Build a signed RS256 JWT assertion for a Google service account (used to
// redeem an OAuth access token). Shells out to openssl for the signature.
// Returns true on success, with the token written to `out`.
bool generate_gcal_jwt(const char *service_email, const char *privkey,
                       std::string &out);

}  // namespace ui
