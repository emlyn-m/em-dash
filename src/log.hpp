#pragma once

#include <cstddef>

// Priority levels, plus a 256-colour code (_C) and short label (_S) for each,
// consumed by the LOG macro via token pasting.
#define PRI_DBG 0
#define PRI_INF 1
#define PRI_WRN 2
#define PRI_ERR 3
#define PRI_DBG_C 030
#define PRI_INF_C 139
#define PRI_WRN_C 208
#define PRI_ERR_C 162
#define PRI_DBG_S "dbg"
#define PRI_INF_S "inf"
#define PRI_WRN_S "wrn"
#define PRI_ERR_S "err"

#define MAX_RECORDS 30

namespace ui {

struct LogRecord {
  bool set;
  int pri;
  int pri_color;
  const char *prefix;
  char *buf;
};

void log_init();
void _log_internal(int pri, int pri_color, const char *pri_str,
                   const char *fmt, ...);
// Copies up to `n` most-recent records into `out` (newest first). Returns the
// count written; each out[i].buf is heap-allocated and owned by the caller.
int fetch_n_logs(int n, LogRecord *out);

}  // namespace ui

// Fully qualified so it works from any namespace, e.g. LOG(PRI_ERR, "x=%d", x).
#define LOG(PRI, FMT, ...) \
  ::ui::_log_internal(PRI, PRI##_C, PRI##_S, FMT, ##__VA_ARGS__)
