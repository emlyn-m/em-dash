#include "src/net/cJSON.h"
#include "src/net/net.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <gtk-2.0/gtk/gtk.h>

void update_telem_devices_async(telem_t* telem) {
   	const size_t DEVICE_BUFSIZE = 100000;

   	char device_req_cmdbuf[1024]; memset(device_req_cmdbuf, 0, 1024);
	snprintf(device_req_cmdbuf, 1024, "curl -s %s", getenv("TELEM_DEVICE_ENDPOINT"));
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m using command \"%s\"\n", device_req_cmdbuf); fflush(stdout);
	FILE* device_fp = popen(device_req_cmdbuf, "r");
	if (!device_fp) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m failed to fetch devices\n"); fflush(stderr); return; }
	char device_buf[DEVICE_BUFSIZE]; memset(device_buf, 0, DEVICE_BUFSIZE);
	if ( std::fread(device_buf, sizeof(char), DEVICE_BUFSIZE, device_fp) == DEVICE_BUFSIZE ) {
	    printf("\x1b[38;5;139m\x1b[1mERR:\x1b[0m read filled device_buf - end: <%s>\n", device_buf + DEVICE_BUFSIZE - 10); fflush(stdout);
	    return;
	};
	int device_req_status = pclose(device_fp);
	if (device_req_status == -1) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m pclose error on device_req\n"); fflush(stderr); return; }
	if (WEXITSTATUS(device_req_status)) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m device_req returned error %d\n", WEXITSTATUS(device_req_status)); fflush(stderr); return; }
	
	cJSON* devices = cJSON_Parse(device_buf);
	telem->n_devices = cJSON_GetArraySize(devices);
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m found %d devices\n", telem->n_devices); fflush(stdout);
	for (int i=0; i < telem->n_devices; i++) {
	    strncpy(telem->devices[i]->name, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "name")->valuestring, 63);
	    strncpy(telem->devices[i]->alias, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "alias")->valuestring, 63);
	    strncpy(telem->devices[i]->ip, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "ip")->valuestring, 31);
	    telem->devices[i]->online = cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "online")->valueint;
	    printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m found device %s (%s): %s (%s)\n", telem->devices[i]->alias, telem->devices[i]->name, telem->devices[i]->online ? "online" : "offline", telem->devices[i]->online ? telem->devices[i]->ip : "-");
	    fflush(stdout);
	}
}

void update_telem_services_async(telem_t* telem) {
   	const size_t SERVICE_BUFSIZE = 100000;
   	char service_req_cmdbuf[1024]; memset(service_req_cmdbuf, 0, 1024);
	snprintf(service_req_cmdbuf, 1024, "curl -s %s", getenv("TELEM_SERVICE_ENDPOINT"));
	FILE* service_fp = popen(service_req_cmdbuf, "r");
	if (!service_fp) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m failed to fetch devices\n"); fflush(stderr); return; }
	char service_buf[SERVICE_BUFSIZE]; memset(service_buf, 0, SERVICE_BUFSIZE);
	if ( std::fread(service_buf, sizeof(char), SERVICE_BUFSIZE, service_fp) == SERVICE_BUFSIZE ) {
	    printf("\x1b[38;5;139m\x1b[1mERR:\x1b[0m read filled service_buf - end: <%s>\n", service_buf + SERVICE_BUFSIZE - 10); fflush(stdout);
	    return;
	};
	int service_req_status = pclose(service_fp);
	if (service_req_status == -1) { fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m pclose error on service_req\n"); fflush(stderr); return; }
	if (WEXITSTATUS(service_req_status)) { fprintf(stderr, "\x1b[38;5;138m\x1b[1mERR:\x1b[0m device_req returned error %d\n", WEXITSTATUS(service_req_status)); fflush(stderr); return; }

	cJSON* services = cJSON_Parse(service_buf);
	telem->n_services = cJSON_GetArraySize(services);
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m found %d services\n", telem->n_services); fflush(stdout);
	for (int i=0; i < telem->n_services; i++) {
	    strncpy(telem->services[i]->name, cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "name")->valuestring, 63);
	    strncpy(telem->services[i]->status, cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "status")->valuestring, 63);
	    printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m found service %s: %s \n", telem->services[i]->name, telem->services[i]->status);
	    fflush(stdout);
	}
}

void update_telem_battery_async(telem_t* data) {
   	time_t duration;
	http_get((char*) "ipinfo.io", (char*) "ip", 80, &(data->ip), &duration);
	data->num_pings = MIN(data->num_pings+1, data->max_pings);
	data->ping_offset = (data->ping_offset + 1) % data->num_pings;
	data->ping_logs[data->ping_offset] = duration;
	if (data->num_pings >= 2) {
	    data->jitter += abs((float) (data->ping_logs[data->ping_offset]) - data->ping_logs[(data->ping_offset - 1) % data->num_pings]);
	}
}

void update_telem_ip_async(telem_t* data) {
   	FILE* read_battery_fp = popen("cat " STR(BATTERY_PATH), "r");
	if (!read_battery_fp) { return; }
	fscanf(read_battery_fp, "%d", &(data->battery));
	FILE* read_current_fp = popen("cat " STR(CURRENT_PATH), "r");
	if (!read_current_fp) { return; }
	int current_now;
	fscanf(read_current_fp, "%d", &current_now);
	data->charging = current_now > 0;


}

gboolean update_telem_async(gpointer* data_vp) {

	telem_t* telem = (telem_t*) data_vp;
	time_t now = time(NULL);
	if (now < (telem->last_update + telem->update_freq)) {
	    return TRUE;  // skipping
	}
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m begin telemetry network update\n"); fflush(stdout);
	
	if (!fork()) {
	    update_telem_devices_async(telem);
		_exit(0);
	}
	if (!fork()) {
	    update_telem_services_async(telem);
		_exit(0);
	}
	if (!fork()) {
	    update_telem_battery_async(telem);
		_exit(0);
	}
	if (!fork()) {
	    update_telem_ip_async(telem);
		_exit(0);
	}

	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m begin telemetry network update\n"); fflush(stdout);
	return TRUE;
}
