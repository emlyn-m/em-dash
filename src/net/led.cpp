#include "net/led.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"
#include "net/tuya.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ui {

namespace {

// One entry per device. The mutex serialises all socket use so control sends
// and state queries can't corrupt each other's protocol state.
struct Device {
  tuya_led_t led;
  std::mutex mtx;
  LedState state;
};

std::vector<std::unique_ptr<Device>> g_devices;

// Build a device from a config key prefix, e.g. "LED_STRIP". Returns nullptr if
// the required keys are absent.
std::unique_ptr<Device> make_device(const char *prefix) {
  auto key = [&](const char *suffix) {
    static char buf[64];
    snprintf(buf, sizeof buf, "%s_%s", prefix, suffix);
    return get_attr_str(buf);
  };

  const char *id = key("ID");
  const char *name = key("NAME");
  const char *ip = key("IP");
  const char *dkey = key("KEY");
  char version_key[64];
  snprintf(version_key, sizeof version_key, "%s_VERSION", prefix);
  long version = get_attr_long(version_key);

  if (!id || !name || !ip || !dkey) {
    LOG(PRI_WRN, "led: %s not configured, skipping\n", prefix);
    return nullptr;
  }

  auto dev = std::make_unique<Device>();
  // Config strings persist for the program's lifetime, so tuya_led_new may
  // borrow them directly.
  tuya_led_new(&dev->led, (char *)id, (char *)name, inet_addr(ip),
               (uint8_t)version, (unsigned char *)dkey);
  return dev;
}

bool parse_dps(const char *json, LedState &out) {
  cJSON *root = cJSON_Parse(json);
  cJSON *dps = cJSON_GetObjectItem(root, "dps");
  if (!dps) {
    cJSON_Delete(root);
    return false;
  }
  if (cJSON *p = cJSON_GetObjectItem(dps, "20")) out.power = p->valueint;
  if (cJSON *c = cJSON_GetObjectItem(dps, "24"))
    sscanf(c->valuestring, "%04x%04x%04x", &out.hue, &out.sat, &out.val);
  cJSON_Delete(root);
  return true;
}

bool valid(int idx) { return idx >= 0 && idx < (int)g_devices.size(); }

// Lock the device and send a control payload. Runs on a worker thread.
void send_locked(int idx, const char *dps) {
  Device &d = *g_devices[idx];
  std::lock_guard<std::mutex> lock(d.mtx);
  tuya_cmd_send(&d.led, COMMAND_CTRL, (char *)dps);
}

}  // namespace

void led_init() {
  if (auto strip = make_device("LED_STRIP")) g_devices.push_back(std::move(strip));
  if (auto lamp = make_device("LED_LAMP")) g_devices.push_back(std::move(lamp));
  LOG(PRI_INF, "led: %d device(s) configured\n", (int)g_devices.size());
}

int led_count() { return (int)g_devices.size(); }

const char *led_name(int idx) {
  return valid(idx) ? g_devices[idx]->led.name : "";
}

const LedState &led_state(int idx) {
  static LedState empty;
  return valid(idx) ? g_devices[idx]->state : empty;
}

void led_set_power(int idx, bool on) {
  if (!valid(idx)) return;
  std::thread([idx, on] {
    char dps[32];
    snprintf(dps, sizeof dps, "{\"20\": %s}", on ? "true" : "false");
    send_locked(idx, dps);
    post_to_main([idx, on] { g_devices[idx]->state.power = on; });
  }).detach();
}

void led_set_hsv(int idx, int hue, int sat, int val) {
  if (!valid(idx)) return;
  std::thread([idx, hue, sat, val] {
    char dps[40];
    snprintf(dps, sizeof dps, "{\"24\": \"%04x%04x%04x\"}", hue, sat, val);
    send_locked(idx, dps);
    post_to_main([idx, hue, sat, val] {
      LedState &s = g_devices[idx]->state;
      s.hue = hue;
      s.sat = sat;
      s.val = val;
    });
  }).detach();
}

void led_refresh(int idx, std::function<void()> on_update) {
  if (!valid(idx)) return;
  std::thread([idx, on_update] {
    LedState parsed;
    bool ok = false;
    {
      Device &d = *g_devices[idx];
      std::lock_guard<std::mutex> lock(d.mtx);
      if (tuya_cmd_send(&d.led, COMMAND_QUERY, nullptr) == 0) {
        tuya_msg_t msg = {};
        if (tuya_msg_recv(&d.led, COMMAND_QUERY, &msg) == 0 && msg.payload &&
            msg.payload_len > 0) {
          ok = parse_dps((char *)msg.payload, parsed);
          parsed.online = ok;
        }
        tuya_msg_free(&msg);
      }
    }
    post_to_main([idx, parsed, ok, on_update] {
      if (ok)
        g_devices[idx]->state = parsed;
      else
        g_devices[idx]->state.online = false;
      if (on_update) on_update();
    });
  }).detach();
}

}  // namespace ui
