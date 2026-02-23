#include "src/log.hpp"
#include "src/net/cJSON.h"
#include "src/net/net.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <gtk-2.0/gtk/gtk.h>
#include <pthread.h>

void* update_telem_async(void* data_vp) {
    telem_t* telem = (telem_t*) data_vp;
   	const size_t DEVICE_BUFSIZE = 100000;
    
   	time_t duration;
	http_get((char*) "ipinfo.io", (char*) "ip", 80, &(telem->ip), &duration);
	telem->num_pings = MIN(telem->num_pings+1, telem->max_pings);
	telem->ping_offset = (telem->ping_offset + 1) % telem->num_pings;
	telem->ping_logs[telem->ping_offset] = duration;
	if (telem->num_pings >= 2) {
	    telem->jitter += abs((float) (telem->ping_logs[telem->ping_offset]) - telem->ping_logs[(telem->ping_offset - 1) % telem->num_pings]);
	}
	printf(LOGFMT("ping of duration %ldms (%d total)\n", LOG_DBG), duration, telem->num_pings);


   	char device_req_cmdbuf[1024]; memset(device_req_cmdbuf, 0, 1024);
	snprintf(device_req_cmdbuf, 1024, "curl -s %s", getenv("TELEM_DEVICE_ENDPOINT"));
	printf(LOGFMT("using command \"%s\"\n", LOG_INF), device_req_cmdbuf); fflush(stdout);
	FILE* device_fp = popen(device_req_cmdbuf, "r");
	if (!device_fp) { fprintf(stderr, LOGFMT("failed to fetch devices\n", LOG_ERR)); fflush(stderr); return NULL; }
	char device_buf[DEVICE_BUFSIZE]; memset(device_buf, 0, DEVICE_BUFSIZE);
	if ( std::fread(device_buf, sizeof(char), DEVICE_BUFSIZE, device_fp) == DEVICE_BUFSIZE ) {
	    printf(LOGFMT("read filled device_buf - end: <%s>\n", LOG_ERR), device_buf + DEVICE_BUFSIZE - 10); fflush(stdout);
	    return NULL;
	};
	int device_req_status = pclose(device_fp);
	if (device_req_status == -1) { fprintf(stderr, LOGFMT("pclose error on device_req\n", LOG_ERR)); fflush(stderr); return NULL; }
	if (WEXITSTATUS(device_req_status)) { fprintf(stderr, LOGFMT("device_req returned error %d\n", LOG_ERR), WEXITSTATUS(device_req_status)); fflush(stderr); return NULL; }
	
	cJSON* devices = cJSON_Parse(device_buf);
	telem->n_devices = cJSON_GetArraySize(devices);
	printf(LOGFMT("found %d devices\n", LOG_INF), telem->n_devices); fflush(stdout);
	for (int i=0; i < telem->n_devices; i++) {
	    strncpy(telem->devices[i]->name, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "name")->valuestring, 63);
	    strncpy(telem->devices[i]->alias, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "alias")->valuestring, 63);
	    strncpy(telem->devices[i]->ip, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "ip")->valuestring, 31);
	    telem->devices[i]->online = cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "online")->valueint;
	    printf(LOGFMT("found device %s (%s): %s (%s)\n", LOG_DBG), telem->devices[i]->alias, telem->devices[i]->name, telem->devices[i]->online ? "online" : "offline", telem->devices[i]->online ? telem->devices[i]->ip : "-");
	    fflush(stdout);
	}
	
	const size_t SERVICE_BUFSIZE = 100000;
    
   	char service_req_cmdbuf[1024]; memset(service_req_cmdbuf, 0, 1024);
	snprintf(service_req_cmdbuf, 1024, "curl -s %s", getenv("TELEM_SERVICE_ENDPOINT"));
	FILE* service_fp = popen(service_req_cmdbuf, "r");
	if (!service_fp) { fprintf(stderr, LOGFMT("failed to fetch devices\n", LOG_ERR)); fflush(stderr); return NULL; }
	char service_buf[SERVICE_BUFSIZE]; memset(service_buf, 0, SERVICE_BUFSIZE);
	if ( std::fread(service_buf, sizeof(char), SERVICE_BUFSIZE, service_fp) == SERVICE_BUFSIZE ) {
	    printf(LOGFMT("read filled service_buf - end: <%s>\n", LOG_ERR), service_buf + SERVICE_BUFSIZE - 10); fflush(stdout);
	    return NULL;
	};
	int service_req_status = pclose(service_fp);
	if (service_req_status == -1) { fprintf(stderr, LOGFMT("pclose error on service_req\n", LOG_ERR)); fflush(stderr); return NULL; }
	if (WEXITSTATUS(service_req_status)) { fprintf(stderr, "\x1b[38;5;138m\x1b[1mERR:\x1b[0m device_req returned error %d\n", WEXITSTATUS(service_req_status)); fflush(stderr); return NULL; }

	cJSON* services = cJSON_Parse(service_buf);
	telem->n_services = cJSON_GetArraySize(services);
	printf(LOGFMT("found %d services\n", LOG_INF), telem->n_services); fflush(stdout);
	for (int i=0; i < telem->n_services; i++) {
	    strncpy(telem->services[i]->name, cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "name")->valuestring, 63);
	    strncpy(telem->services[i]->status, cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "status")->valuestring, 63);
	    printf(LOGFMT("found service %s: %s \n", LOG_INF), telem->services[i]->name, telem->services[i]->status);
	    fflush(stdout);
	}


	char battery_cmdbuf[64] = { 0 };
	snprintf(battery_cmdbuf, 64, "cat %s", getenv("BATTERY_PATH"));
   	FILE* read_battery_fp = popen(battery_cmdbuf, "r");
	if (!read_battery_fp) { return NULL; }
	fscanf(read_battery_fp, "%d", &(telem->battery));
	char current_cmdbuf[64] = { 0 };
	snprintf(current_cmdbuf, 64, "cat %s", getenv("CURRENT_PATH"));
	FILE* read_current_fp = popen(current_cmdbuf, "r");
	if (!read_current_fp) { return NULL; }
	int current_now;
	fscanf(read_current_fp, "%d", &current_now);
	telem->charging = current_now > 0;
	
	telem->last_update = time(NULL);
	return NULL;
}

gboolean update_telem_async(gpointer* data_vp) {

	telem_t* telem = (telem_t*) data_vp;
	time_t now = time(NULL);
	if (now < (telem->last_update + telem->update_freq)) {
	    return TRUE;  // skipping
	}
	telem->last_update = now;
	printf(LOGFMT("begin telemetry network update\n", LOG_INF)); fflush(stdout);
	
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, update_telem_async, data_vp);

	printf(LOGFMT("end telemetry network update\n", LOG_INF)); fflush(stdout);
	return TRUE;
}
