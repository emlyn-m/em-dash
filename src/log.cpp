#include "src/log.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

struct _log_data* data;
void log_init() {
    data = (struct _log_data*) malloc(sizeof(struct _log_data));
    data->pri = PRI_DBG;
    data->record_ptr = 0;
    data->max_records = MAX_RECORDS;
    data->record_bufsize = 64;
    data->records = (_log_record*) malloc(data->max_records * sizeof(struct _log_record));
    for (size_t i=0; i < data->max_records; i++) {
        data->records[i]._set = 0;
        data->records[i].buf = (char*) malloc(data->record_bufsize);
        memset(data->records[i].buf, 0, data->record_bufsize);
    }
}


void _log_internal(const int prio, const int prio_c, const char* prio_s, const char* fmt, ...) {
   	va_list ap;
	if (prio >= data->pri) {
	    va_start(ap, fmt);
	    char* fmt_modified = (char*) malloc(32 + strlen(fmt));
		snprintf(fmt_modified, 32+strlen(fmt), "\x1b[1m\x1b[38;5;%dm %s \x1b[0m %s", prio_c, prio_s, fmt);
	    vprintf(fmt_modified, ap);
		free(fmt_modified);
		va_end(ap);
		
        va_start(ap, fmt);
        data->records[data->record_ptr].pri = prio;
        data->records[data->record_ptr].pri_color = prio_c;
		data->records[data->record_ptr].prefix =  prio_s;
		vsnprintf(data->records[data->record_ptr].buf, data->record_bufsize, fmt, ap);
		data->records[data->record_ptr].buf[strcspn(data->records[data->record_ptr].buf, "\n")] = 0;
		data->record_ptr = (++data->record_ptr) % data->max_records;
		data->records[data->record_ptr]._set = 1;
		va_end(ap);
	}
}

int fetch_n_logs(int n, struct _log_record* buf) {
    int fetched = 0;
    int i = ((data->record_ptr-1) % data->max_records + data->max_records) % data->max_records;
    while (data->records[i]._set && fetched < n) {
        if (data->records[i].pri >= data->pri) {
            buf[fetched].prefix = data->records[i].prefix;
            buf[fetched].buf = (char*) malloc(data->record_bufsize);
            memcpy(buf[fetched].buf, data->records[i].buf, data->record_bufsize);
            fetched++;
        }
        
        i = ((i-1) % data->max_records + data->max_records) % data->max_records;
    }
    return fetched;
}
