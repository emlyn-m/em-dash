#include "src/config.hpp"
#include "src/log.hpp"
#include "src/net/cJSON.h"
#include "src/net/net.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtk-2.0/gtk/gtk.h>
#include <pthread.h>
#include <sys/wait.h>

void _update_telem_async_devices(telem_t *telem) {
  const size_t DEVICE_BUFSIZE = 100000;

  char device_req_cmdbuf[1024];
  memset(device_req_cmdbuf, 0, 1024);
  snprintf(device_req_cmdbuf, 1024, "curl -s %s",
           get_attr_str("TELEM_DEVICE_ENDPOINT"));
  FILE *device_fp = popen(device_req_cmdbuf, "r");
  if (!device_fp) {
    LOG(PRI_ERR, "failed to fetch devices\n");
    fflush(stdout);
    return;
  }
  char device_buf[DEVICE_BUFSIZE];
  memset(device_buf, 0, DEVICE_BUFSIZE);
  if (std::fread(device_buf, sizeof(char), DEVICE_BUFSIZE, device_fp) ==
      DEVICE_BUFSIZE) {
    LOG(PRI_ERR, "read filled device_buf - end: <%s>\n",
        device_buf + DEVICE_BUFSIZE - 10);
    fflush(stdout);
    pclose(device_fp);
    return;
  };
  int device_req_status = pclose(device_fp);
  if (device_req_status == -1) {
    LOG(PRI_ERR, "pclose error on device_req\n");
    fflush(stdout);
    return;
  }
  if (WEXITSTATUS(device_req_status)) {
    LOG(PRI_ERR, "device_req returned error %d\n",
        WEXITSTATUS(device_req_status));
    fflush(stdout);
    return;
  }

  cJSON *devices = cJSON_Parse(device_buf);
  telem->n_devices = cJSON_GetArraySize(devices);
  for (int i = 0; i < telem->n_devices; i++) {
    strncpy(telem->devices[i]->name,
            cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "name")
                ->valuestring,
            63);
    strncpy(telem->devices[i]->alias,
            cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "alias")
                ->valuestring,
            63);
    strncpy(
        telem->devices[i]->ip,
        cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "ip")->valuestring,
        31);
    telem->devices[i]->online =
        cJSON_GetObjectItem(cJSON_GetArrayItem(devices, i), "online")->valueint;
    fflush(stdout);
  }
  cJSON_Delete(devices);
}

void _update_telem_async_services(telem_t *telem) {
  const size_t SERVICE_BUFSIZE = 100000;

  char service_req_cmdbuf[1024];
  memset(service_req_cmdbuf, 0, 1024);
  snprintf(service_req_cmdbuf, 1024, "curl -s %s",
           get_attr_str("TELEM_SERVICE_ENDPOINT"));
  FILE *service_fp = popen(service_req_cmdbuf, "r");
  if (!service_fp) {
    LOG(PRI_ERR, "failed to fetch devices\n");
    fflush(stdout);
    return;
  }
  char service_buf[SERVICE_BUFSIZE];
  memset(service_buf, 0, SERVICE_BUFSIZE);
  if (std::fread(service_buf, sizeof(char), SERVICE_BUFSIZE, service_fp) ==
      SERVICE_BUFSIZE) {
    LOG(PRI_ERR, "read filled service_buf - end: <%s>\n",
        service_buf + SERVICE_BUFSIZE - 10);
    fflush(stdout);
    pclose(service_fp);
    return;
  };
  int service_req_status = pclose(service_fp);
  if (service_req_status == -1) {
    LOG(PRI_ERR, "pclose error on service_req\n");
    fflush(stdout);
    return;
  }
  if (WEXITSTATUS(service_req_status)) {
    LOG(PRI_ERR, "device_req returned error %d\n",
        WEXITSTATUS(service_req_status));
    fflush(stdout);
    return;
  }

  cJSON *services = cJSON_Parse(service_buf);
  telem->n_services = cJSON_GetArraySize(services);
  for (int i = 0; i < telem->n_services; i++) {
    strncpy(telem->services[i]->name,
            cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "name")
                ->valuestring,
            63);
    strncpy(telem->services[i]->status,
            cJSON_GetObjectItem(cJSON_GetArrayItem(services, i), "status")
                ->valuestring,
            63);
    fflush(stdout);
  }
  cJSON_Delete(services);
}

void *update_telem_async(void *data_vp) {
  telem_t *telem = (telem_t *)data_vp;

  char battery_cmdbuf[64] = {0};
  snprintf(battery_cmdbuf, 64, "cat %s", get_attr_str("BATTERY_PATH"));
  FILE *read_battery_fp = popen(battery_cmdbuf, "r");
  if (read_battery_fp) {
    fscanf(read_battery_fp, "%d", &(telem->battery));
    pclose(read_battery_fp);
  }

  char current_cmdbuf[64] = {0};
  snprintf(current_cmdbuf, 64, "cat %s", get_attr_str("CURRENT_PATH"));
  FILE *read_current_fp = popen(current_cmdbuf, "r");
  if (read_current_fp) {
    fscanf(read_current_fp, "%d", &(telem->current));
    pclose(read_current_fp);
  }

  FILE *read_wifi_fp = popen("iw wlan0 link | head -n 6 | tail -n 1", "r");
  if (read_wifi_fp) {
    if (fscanf(read_wifi_fp, "  signal: %d dBm", &(telem->wifi_strength)) > 0) {
    } else {
      telem->wifi_strength = 1;
    }
    pclose(read_wifi_fp);
  }

  telem->last_update = time(NULL);
  return NULL;
}

void *_update_telem_async_net(void *data_vp) {
  telem_t *telem = (telem_t *)data_vp;

  _update_telem_async_devices(telem);
  _update_telem_async_services(telem);

  time_t duration;
  http_get((char *)"ipinfo.io", (char *)"ip", 80, &(telem->ip), &duration);
  telem->num_pings = MIN(telem->num_pings + 1, telem->max_pings);
  telem->ping_offset = (telem->ping_offset + 1) % telem->num_pings;
  telem->ping_logs[telem->ping_offset] = duration;
  if (telem->num_pings >= 2) {
    telem->jitter +=
        abs((float)(telem->ping_logs[telem->ping_offset]) -
            telem->ping_logs[(telem->ping_offset - 1) % telem->num_pings]);
  }

  telem->last_update = time(NULL);
  return NULL;
}

gboolean update_telem_async(gpointer *data_vp) {

  telem_t *telem = (telem_t *)data_vp;
  time_t now = time(NULL);
  if (now < (telem->last_update + telem->update_freq)) {
    return TRUE; // skipping
  }
  telem->last_update = now;
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, update_telem_async, data_vp);
  pthread_detach(thread_id);

  return TRUE;
}

gboolean update_telem_async_net(gpointer *data_vp) {
  telem_t *telem = (telem_t *)data_vp;
  time_t now = time(NULL);
  if (now < (telem->last_update_net + telem->update_freq_net)) {
    return TRUE; // skipping
  }
  telem->last_update_net = now;

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, _update_telem_async_net, data_vp);
  pthread_detach(thread_id);

  return TRUE;
}
