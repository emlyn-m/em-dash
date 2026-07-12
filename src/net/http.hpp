#pragma once

#include <ctime>

namespace ui {

// Minimal blocking HTTP/1.1 GET. Writes the response body into *out (realloc'd
// to fit; caller frees) and the round-trip time into *ping. Returns 0 on
// success, non-zero on error. Intended to be called from worker threads.
int http_get(const char *hostname, const char *path, int port, char **out,
             time_t *ping);

}  // namespace ui
