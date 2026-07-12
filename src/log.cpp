#include "log.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace ui {

namespace {

struct LogData {
  int pri = PRI_DBG;
  size_t record_ptr = 0;
  size_t max_records = MAX_RECORDS;
  size_t record_bufsize = 256;
  LogRecord *records = nullptr;
};

LogData g_log;
// Workers on secondary threads log too, so guard the ring buffer and stdout.
std::mutex g_log_mutex;

}  // namespace

void log_init() {
  g_log.records = (LogRecord *)calloc(g_log.max_records, sizeof(LogRecord));
  for (size_t i = 0; i < g_log.max_records; i++) {
    g_log.records[i].set = false;
    g_log.records[i].buf = (char *)calloc(g_log.record_bufsize, 1);
  }
}

void _log_internal(int pri, int pri_color, const char *pri_str,
                   const char *fmt, ...) {
  if (pri < g_log.pri) return;

  std::lock_guard<std::mutex> lock(g_log_mutex);

  va_list ap;
  va_start(ap, fmt);
  printf("\x1b[1m\x1b[38;5;%dm %s \x1b[0m ", pri_color, pri_str);
  vprintf(fmt, ap);
  fflush(stdout);
  va_end(ap);

  LogRecord &rec = g_log.records[g_log.record_ptr];
  rec.pri = pri;
  rec.pri_color = pri_color;
  rec.prefix = pri_str;
  va_start(ap, fmt);
  vsnprintf(rec.buf, g_log.record_bufsize, fmt, ap);
  va_end(ap);
  rec.buf[strcspn(rec.buf, "\n")] = '\0';  // records are single-line
  rec.set = true;
  g_log.record_ptr = (g_log.record_ptr + 1) % g_log.max_records;
}

int fetch_n_logs(int n, LogRecord *out) {
  std::lock_guard<std::mutex> lock(g_log_mutex);

  int fetched = 0;
  size_t i = (g_log.record_ptr + g_log.max_records - 1) % g_log.max_records;
  for (size_t seen = 0; seen < g_log.max_records && fetched < n; seen++) {
    LogRecord &rec = g_log.records[i];
    if (!rec.set) break;
    if (rec.pri >= g_log.pri) {
      out[fetched].prefix = rec.prefix;
      out[fetched].pri = rec.pri;
      out[fetched].pri_color = rec.pri_color;
      out[fetched].set = true;
      out[fetched].buf = (char *)malloc(g_log.record_bufsize);
      memcpy(out[fetched].buf, rec.buf, g_log.record_bufsize);
      fetched++;
    }
    i = (i + g_log.max_records - 1) % g_log.max_records;
  }
  return fetched;
}

}  // namespace ui
