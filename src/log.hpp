#include <cstdio>

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
struct _log_record {
    int _set;
    int pri;
    const char* prefix;
    char* buf;
};
struct _log_data {
    int pri;
    size_t record_ptr;
    size_t max_records;
    size_t record_bufsize;
    _log_record* records;
};
void log_init();
void _log_internal(const int prio, const int prio_c, const char* prio_s, const char* fmt, ...);
int fetch_n_logs(int n, struct _log_record* buf);
#define VA_ARGS(...) , ##__VA_ARGS__
#define LOG(PRI, FMT, ...) _log_internal(PRI, PRI##_C, PRI##_S, FMT, ##__VA_ARGS__)
