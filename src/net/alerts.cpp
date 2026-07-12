#include "net/alerts.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <thread>

namespace ui {

namespace {

Alerts g_alerts;

// Runs curl over the alert endpoint and reads the whole response.
bool run_curl(const char *url, std::string &out) {
  char cmd[1024];
  snprintf(cmd, sizeof cmd, "curl -s '%s'", url);
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    LOG(PRI_ERR, "alerts: popen failed\n");
    return false;
  }
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
    out.append(chunk, got);
  int status = pclose(fp);
  if (status == -1 || WEXITSTATUS(status)) {
    LOG(PRI_ERR, "alerts: curl exited %d\n", WEXITSTATUS(status));
    return false;
  }
  return true;
}

bool fetch_alerts(std::vector<Alert> &out) {
  std::string body;
  if (!run_curl(get_attr_str("ALERT_ENDPOINT"), body)) return false;

  cJSON *arr = cJSON_Parse(body.c_str());
  if (!arr) {
    LOG(PRI_ERR, "alerts: parse failed\n");
    return false;
  }

  out.clear();
  int n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *obj = cJSON_GetArrayItem(arr, i);
    cJSON *cat = cJSON_GetObjectItem(obj, "msg_class");
    cJSON *msg = cJSON_GetObjectItem(obj, "msg_body");
    cJSON *ts = cJSON_GetObjectItem(obj, "msg_timestamp");
    cJSON *sev = cJSON_GetObjectItem(obj, "msg_sev");
    if (!cat || !msg || !ts || !sev) {
      LOG(PRI_WRN, "alerts: entry missing fields, skipping\n");
      continue;
    }
    out.push_back(Alert{(time_t)ts->valueint, sev->valueint,
                        cat->valuestring, msg->valuestring});
  }

  cJSON_Delete(arr);
  LOG(PRI_INF, "alerts: %zu entries\n", out.size());
  return true;
}

void alerts_worker(std::function<void()> on_update) {
  long freq = get_attr_long("ALERT_UPDATE_FREQUENCY");
  if (freq <= 0) freq = 120;

  for (;;) {
    std::vector<Alert> parsed;
    if (fetch_alerts(parsed)) {
      post_to_main([parsed = std::move(parsed), on_update]() mutable {
        g_alerts.alerts = std::move(parsed);
        g_alerts.last_update = time(nullptr);
        if (on_update) on_update();
      });
    }
    std::this_thread::sleep_for(std::chrono::seconds(freq));
  }
}

}  // namespace

void alerts_start(std::function<void()> on_update) {
  std::thread(alerts_worker, std::move(on_update)).detach();
}

const Alerts &alerts_state() { return g_alerts; }

}  // namespace ui
