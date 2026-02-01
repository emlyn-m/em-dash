#include "src/net/cJSON.h"
#include "src/widgets/widgets.hpp"
#include "../secrets.h"

#include <cstdio>
#include <cstring>
#include <gtk-2.0/gtk/gtk.h>

gboolean update_telem_net(gpointer* data_vp) {
    const size_t DEVICE_BUFSIZE = 100000;
    const size_t SERVICE_BUFSIZE = 100000;
    
    telem_t* telem = (telem_t*) data_vp;
    
    char device_req_cmdbuf[1024]; memset(device_req_cmdbuf, 0, 1024);
    snprintf(device_req_cmdbuf, 1024, "curl -s %s", TELEM_DEVICE_ENDPOINT);
    FILE* device_fp = popen(device_req_cmdbuf, "r");
    if (!device_fp) { fprintf(stderr, "failed to fetch devices\n"); return TRUE; }
    char device_buf[SERVICE_BUFSIZE]; memset(device_buf, 0, SERVICE_BUFSIZE);
    if ( std::fread(device_buf, sizeof(char), DEVICE_BUFSIZE, device_fp) == DEVICE_BUFSIZE ) {
        printf("err: read filled buffer of weather_req - end: <%s>\n", device_buf + DEVICE_BUFSIZE - 10);
        return TRUE;
    };
    
    char service_req_cmdbuf[1024]; memset(service_req_cmdbuf, 0, 1024);
    snprintf(service_req_cmdbuf, 1024, "curl -s %s", TELEM_SERVICE_ENDPOINT);
    FILE* service_fp = popen(service_req_cmdbuf, "r");
    if (!service_fp) { fprintf(stderr, "failed to fetch devices\n"); return TRUE; }
    char service_buf[SERVICE_BUFSIZE]; memset(service_buf, 0, SERVICE_BUFSIZE);
    if ( std::fread(service_buf, sizeof(char), SERVICE_BUFSIZE, service_fp) == SERVICE_BUFSIZE ) {
        printf("err: read filled buffer of weather_req - end: <%s>\n", service_buf + SERVICE_BUFSIZE - 10);
        return TRUE;
    };
    
    
    cJSON* devices = cJSON_Parse(device_buf);
    telem->n_devices = cJSON_GetArraySize(devices);    
    for (int i=0; i < telem->n_devices; i++) {
        strncpy(telem->devices[i]->name, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "name")->valuestring, 63);
        strncpy(telem->devices[i]->alias, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "alias")->valuestring, 63);
        strncpy(telem->devices[i]->ip, cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "ip")->valuestring, 31);
        telem->devices[i]->online = cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "online")->valueint;
    }
        
    cJSON* services = cJSON_Parse(service_buf);
    telem->n_services = cJSON_GetArraySize(services);
    for (int i=0; i < telem->n_services; i++) {
        strncpy(telem->services[i]->name, cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "name")->valuestring, 63);
        strncpy(telem->services[i]->status, cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "status")->valuestring, 63);
    }
        
    return TRUE;
}
