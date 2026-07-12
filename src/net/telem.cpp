#include "net/telem.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"
#include "net/http.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <thread>

namespace ui {

namespace {

constexpr size_t PING_HISTORY_MAX = 60;

Telem g_telem;

// Read a single integer from a sysfs-style file. Leaves `out` untouched (0) if
// the path is missing — e.g. when running off-device.
void read_int_file(const char *path, int &out) {
  if (!path) return;
  FILE *fp = fopen(path, "r");
  if (!fp) return;
  if (fscanf(fp, "%d", &out) != 1) out = 0;
  fclose(fp);
}

void read_wifi_strength(int &out) {
  FILE *fp = popen("iw wlan0 link | head -n 6 | tail -n 1", "r");
  if (!fp) return;
  if (fscanf(fp, "  signal: %d dBm", &out) <= 0) out = 0;
  pclose(fp);
}

bool run_curl(const char *url, std::string &out) {
  char cmd[1024];
  snprintf(cmd, sizeof cmd, "curl -s %s", url);
  FILE *fp = popen(cmd, "r");
  if (!fp) return false;
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
    out.append(chunk, got);
  int status = pclose(fp);
  return status != -1 && WEXITSTATUS(status) == 0;
}

const char *jstr(cJSON *obj, const char *key) {
  cJSON *item = cJSON_GetObjectItem(obj, key);
  return (item && item->valuestring) ? item->valuestring : "";
}

bool fetch_devices(std::vector<Device> &out) {
  std::string body;
  if (!run_curl(get_attr_str("TELEM_DEVICE_ENDPOINT"), body)) {
    LOG(PRI_ERR, "telem: device fetch failed\n");
    return false;
  }
  cJSON *arr = cJSON_Parse(body.c_str());
  if (!arr) return false;
  out.clear();
  int n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *o = cJSON_GetArrayItem(arr, i);
    out.push_back(Device{jstr(o, "name"), jstr(o, "alias"), jstr(o, "ip"),
                         cJSON_GetObjectItem(o, "online")->valueint != 0});
  }
  cJSON_Delete(arr);
  return true;
}

bool fetch_services(std::vector<Service> &out) {
  std::string body;
  if (!run_curl(get_attr_str("TELEM_SERVICE_ENDPOINT"), body)) {
    LOG(PRI_ERR, "telem: service fetch failed\n");
    return false;
  }
  cJSON *arr = cJSON_Parse(body.c_str());
  if (!arr) return false;
  out.clear();
  int n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *o = cJSON_GetArrayItem(arr, i);
    out.push_back(Service{jstr(o, "name"), jstr(o, "status")});
  }
  cJSON_Delete(arr);
  return true;
}

double compute_jitter(const std::vector<long> &pings) {
  if (pings.size() < 2) return 0;
  double sum = 0;
  for (size_t i = 1; i < pings.size(); i++)
    sum += std::fabs((double)pings[i] - (double)pings[i - 1]);
  return sum / (pings.size() - 1);
}

void telem_local_worker(std::function<void()> on_update) {
  long freq = get_attr_long("TELEM_UPDATE_FREQUENCY");
  if (freq <= 0) freq = 15;

  for (;;) {
    int battery = 0, current = 0, wifi = 0;
    read_int_file(get_attr_str("BATTERY_PATH"), battery);
    read_int_file(get_attr_str("CURRENT_PATH"), current);
    read_wifi_strength(wifi);

    post_to_main([battery, current, wifi, on_update] {
      g_telem.battery = battery;
      g_telem.current = current;
      g_telem.wifi_strength = wifi;
      g_telem.last_update = time(nullptr);
      if (on_update) on_update();
    });
    std::this_thread::sleep_for(std::chrono::seconds(freq));
  }
}

void telem_net_worker(std::function<void()> on_update) {
  long freq = get_attr_long("TELEM_NET_UPDATE_FREQUENCY");
  if (freq <= 0) freq = 120;

  for (;;) {
    std::vector<Device> devices;
    std::vector<Service> services;
    fetch_devices(devices);
    fetch_services(services);

    std::string ip;
    char *ip_buf = nullptr;
    time_t ping = 0;
    if (http_get("ipinfo.io", "ip", 80, &ip_buf, &ping) == 0 && ip_buf)
      ip = ip_buf;
    free(ip_buf);

    post_to_main([devices = std::move(devices),
                  services = std::move(services), ip = std::move(ip), ping,
                  on_update]() mutable {
      g_telem.devices = std::move(devices);
      g_telem.services = std::move(services);
      if (!ip.empty()) g_telem.ip = std::move(ip);
      g_telem.ping_history.push_back(ping);
      if (g_telem.ping_history.size() > PING_HISTORY_MAX)
        g_telem.ping_history.erase(g_telem.ping_history.begin());
      g_telem.jitter = compute_jitter(g_telem.ping_history);
      g_telem.last_update_net = time(nullptr);
      if (on_update) on_update();
    });
    std::this_thread::sleep_for(std::chrono::seconds(freq));
  }
}

}  // namespace

void telem_start(std::function<void()> on_update) {
  std::thread(telem_local_worker, on_update).detach();
  std::thread(telem_net_worker, std::move(on_update)).detach();
}

const Telem &telem_state() { return g_telem; }

}  // namespace ui
