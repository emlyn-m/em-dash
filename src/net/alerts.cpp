#include "src/net/cJSON.h"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <gtk-2.0/gtk/gtk.h>

const char* CATEGORY_NAMES[4] = {
	"class_0",
	"class_1",
	"class_2",
	"class_3"
};

gboolean update_alerts_net(gpointer* data_vp) {
    const size_t ALERT_BUFSIZE = 100000;

    alert_t* alert_data = (alert_t*) data_vp;
    printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m begin alert network update\n"); fflush(stdout);

    char alert_req_cmdbuf[1024]; memset(alert_req_cmdbuf, 0, 1024);
    snprintf(alert_req_cmdbuf, 1024, "curl -s '%s'", getenv("ALERT_ENDPOINT"));
    FILE* alert_fp = popen(alert_req_cmdbuf, "r");
    if (!alert_fp) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m failed to fetch alerts\n"); fflush(stderr); return TRUE; }
    char alert_buf[ALERT_BUFSIZE]; memset(alert_buf, 0, ALERT_BUFSIZE);
    if ( std::fread(alert_buf, sizeof(char), ALERT_BUFSIZE, alert_fp) == ALERT_BUFSIZE ) {
        printf("\x1b[38;5;139m\x1b[1mERR:\x1b[0m read filled alert_buf - end: <%s>\n", alert_buf + ALERT_BUFSIZE - 10); fflush(stdout);
        return TRUE;
    };
    int alert_req_status = pclose(alert_fp);
	if (alert_req_status == -1) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m pclose error on alert_req\n"); fflush(stderr); return TRUE; }
	if (WEXITSTATUS(alert_req_status)) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m alert_req returned error %d\n", WEXITSTATUS(alert_req_status)); fflush(stderr); return TRUE; }


    cJSON* alerts = cJSON_Parse(alert_buf);
	unsigned int rx_alert_count = cJSON_GetArraySize(alerts);
    alert_data->num_alerts = std::min(alert_data->max_alerts, rx_alert_count);
    printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m found %d alerts\n", alert_data->num_alerts); fflush(stdout);
    for (guint i=0; i < alert_data->num_alerts; i++) {
		alert_ev_t* alert_obj = alert_data->alerts[i];
		cJSON* alert_jobj = cJSON_GetArrayItem(alerts, i);

		strncpy(alert_obj->category, CATEGORY_NAMES[cJSON_GetObjectItem(alert_jobj, "msg_class")->valueint], 31);
        strncpy(alert_obj->msg, cJSON_GetObjectItem(alert_jobj, "msg_body")->valuestring, 2047);
        alert_obj->time = cJSON_GetObjectItem(alert_jobj, "msg_timestamp")->valueint;
		alert_obj->severity = cJSON_GetObjectItem(alert_jobj, "msg_sev")->valueint;
        printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m found %s alert \"%s\" (sev %d, sent at %ld)\n", alert_obj->category, alert_obj->msg, alert_obj->severity, alert_obj->time);
        fflush(stdout);
    }

    return TRUE;
}
