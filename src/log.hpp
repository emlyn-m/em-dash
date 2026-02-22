#define LOG_DBG "dbg"
#define LOG_INF "inf"
#define LOG_WRN "wrn"
#define LOG_ERR "err"
#define LOG_DBG_C "030"
#define LOG_INF_C "139"
#define LOG_WRN_C "208"
#define LOG_ERR_C "162"
#define LOGFMT(FMT, LVL_NAME) "\x1b[1m\x1b[38;5;" LVL_NAME##_C "m" LVL_NAME "\x1b[0m " FMT
