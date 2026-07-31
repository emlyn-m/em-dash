#include "net/led.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"
#include "net/tuya.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <poll.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace ui {

namespace {

enum class CmdType { Power, Hsv, Query };
struct Cmd {
  CmdType type;
  bool power = false;
  int hue = 0, sat = 0, val = 0;
};

struct Device {
  tuya_led_t led;
  LedState state;                   // main thread only (via post_to_main)
  std::function<void()> on_update;  // main thread only

  // Reflect commands in the model immediately rather than waiting for the
  // device's ack/status push. Lets the UI stay responsive with slow devices.
  bool optimistic = false;

  std::mutex mtx;  // guards queue + active + running
  std::deque<Cmd> queue;
  bool active = false;
  bool running = true;

  int wake_r = -1, wake_w = -1;  // self-pipe to interrupt poll()
};

std::vector<std::unique_ptr<Device>> g_devices;

bool valid(int idx) { return idx >= 0 && idx < (int)g_devices.size(); }

// --- helpers running on the comm thread -----------------------------------

void wake(Device *d) {
  char b = 1;
  ssize_t n = write(d->wake_w, &b, 1);
  (void)n;
}

// Run the device's update callback on the main thread (callback is main-only).
void fire(Device *d) {
  if (d->on_update) d->on_update();
}

void post_online(Device *d, bool on) {
  post_to_main([d, on] {
    if (d->state.online != on) {
      d->state.online = on;
      fire(d);
    }
  });
}

// Parse a dps JSON blob and fold what it carries into the model. Power always
// reflects the device; HSV whenever the reply carries a colour.
void apply_dps(Device *d, const char *json) {
  int power = -1, hue = -1, sat = -1, val = -1;
  cJSON *root = cJSON_Parse(json);
  if (cJSON *dps = cJSON_GetObjectItem(root, "dps")) {
    if (cJSON *p = cJSON_GetObjectItem(dps, "20")) power = p->valueint;
    if (cJSON *c = cJSON_GetObjectItem(dps, "24"))
      sscanf(c->valuestring, "%04x%04x%04x", &hue, &sat, &val);
  }
  cJSON_Delete(root);
  bool set_hsv = hue >= 0;
  post_to_main([d, power, hue, sat, val, set_hsv] {
    if (power >= 0) d->state.power = power;
    if (set_hsv) {
      d->state.hue = hue;
      d->state.sat = sat;
      d->state.val = val;
    }
    fire(d);
  });
}

// Wait for: a wake byte, the socket becoming readable, or timeout. Pass sock<=0
// to poll only the wake pipe. Returns 0=timeout, 1=wake, 2=socket-readable.
int poll_wait(Device *d, int sock, int timeout_ms) {
  struct pollfd fds[2];
  fds[0].fd = d->wake_r;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  int nfds = 1;
  if (sock > 0) {
    fds[1].fd = sock;
    fds[1].events = POLLIN;
    fds[1].revents = 0;
    nfds = 2;
  }
  if (poll(fds, nfds, timeout_ms) <= 0) return 0;
  if (fds[0].revents & POLLIN) {
    char buf[64];
    while (read(d->wake_r, buf, sizeof buf) > 0) {
    }  // drain (non-blocking)
    return 1;
  }
  if (nfds == 2 && (fds[1].revents & (POLLIN | POLLHUP | POLLERR))) return 2;
  return 0;
}

// Send one queued command. Returns false if the send failed (=> reconnect).
bool send_cmd(Device *d, const Cmd &c) {
  char dps[48];
  uint32_t cmd = COMMAND_CTRL;
  switch (c.type) {
    case CmdType::Power:
      snprintf(dps, sizeof dps, "{\"20\": %s}", c.power ? "true" : "false");
      break;
    case CmdType::Hsv:
      snprintf(dps, sizeof dps, "{\"24\": \"%04x%04x%04x\"}", c.hue, c.sat,
               c.val);
      break;
    case CmdType::Query:
      cmd = COMMAND_QUERY;
      break;
  }
  return tuya_cmd_send(&d->led, cmd,
                       c.type == CmdType::Query ? nullptr : dps) == 0;
}

void comm_loop(Device *d) {
  bool connected = false;

  for (;;) {
    bool active, running;
    {
      std::lock_guard<std::mutex> lk(d->mtx);
      active = d->active;
      running = d->running;
    }
    if (!running) break;

    if (!active) {
      if (connected) {
        tuya_disconnect(&d->led);
        connected = false;
        post_online(d, false);
      }
      poll_wait(d, -1, -1);  // block until woken (activate / shutdown)
      continue;
    }

    if (!connected) {
      if (tuya_connect(&d->led) == 0) {
        connected = true;
        LOG(PRI_INF, "led: %s connected\n", d->led.name);
        post_online(d, true);
        std::lock_guard<std::mutex> lk(d->mtx);
        d->queue.push_back(Cmd{CmdType::Query});  // pull initial state
      } else {
        post_online(d, false);
        poll_wait(d, -1, 3000);  // backoff (interruptible)
        continue;
      }
    }

    // Drain and send queued commands.
    std::deque<Cmd> todo;
    {
      std::lock_guard<std::mutex> lk(d->mtx);
      todo.swap(d->queue);
    }
    for (const Cmd &c : todo) {
      if (!send_cmd(d, c)) {
        tuya_disconnect(&d->led);
        connected = false;
        post_online(d, false);
        break;
      }
    }
    if (!connected) continue;

    // Read any reply / status push and fold its dps into the model.
    int ev = poll_wait(d, d->led.sock, 1000);
    if (ev == 2) {
      tuya_msg_t msg = {};
      int r = tuya_msg_recv(&d->led, COMMAND_QUERY, &msg);
      if (r == ERR_SOCK_CLOSE || r == ERR_SOCK_FAIL) {
        tuya_msg_free(&msg);
        tuya_disconnect(&d->led);
        connected = false;
        post_online(d, false);
        continue;
      }
      if (msg.payload && msg.payload_len > 0) {
        apply_dps(d, (char *)msg.payload);
      }
      tuya_msg_free(&msg);
    }
  }
  if (connected) tuya_disconnect(&d->led);
}

// Build a device (config + tuya struct) from a key prefix, e.g. "LED_STRIP".
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
  char optimistic_key[64];
  snprintf(optimistic_key, sizeof optimistic_key, "%s_OPTIMISTIC", prefix);

  if (!id || !name || !ip || !dkey) {
    LOG(PRI_WRN, "led: %s not configured, skipping\n", prefix);
    return nullptr;
  }

  auto dev = std::make_unique<Device>();
  // Config strings persist for the program's lifetime, so borrow them directly.
  tuya_led_new(&dev->led, (char *)id, (char *)name, inet_addr(ip),
               (uint8_t)version, (unsigned char *)dkey);
  dev->optimistic = get_attr_bool(optimistic_key);
  return dev;
}

// Enqueue a command and wake the device's thread.
void enqueue(int idx, const Cmd &c, bool coalesce_hsv) {
  Device *d = g_devices[idx].get();
  {
    std::lock_guard<std::mutex> lk(d->mtx);
    if (coalesce_hsv && !d->queue.empty() &&
        d->queue.back().type == CmdType::Hsv)
      d->queue.back() = c;  // collapse a burst of drags into the latest
    else
      d->queue.push_back(c);
  }
  wake(d);
}

}  // namespace

void led_init() {
  auto add = [](const char *prefix) {
    auto dev = make_device(prefix);
    if (!dev) return;
    int fds[2];
    if (pipe(fds) != 0) {
      LOG(PRI_ERR, "led: pipe failed for %s\n", prefix);
      return;
    }
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);
    dev->wake_r = fds[0];
    dev->wake_w = fds[1];
    Device *raw = dev.get();
    g_devices.push_back(std::move(dev));
    std::thread(comm_loop, raw).detach();
  };
  add("LED_STRIP");
  add("LED_LAMP");
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

void led_set_active(int idx, bool active) {
  if (!valid(idx)) return;
  Device *d = g_devices[idx].get();
  {
    std::lock_guard<std::mutex> lk(d->mtx);
    d->active = active;
    if (!active) d->queue.clear();
  }
  wake(d);
}

void led_on_update(int idx, std::function<void()> cb) {
  if (!valid(idx)) return;
  g_devices[idx]->on_update = std::move(cb);  // set on the main thread
}

void led_set_power(int idx, bool on) {
  if (!valid(idx)) return;
  Device *d = g_devices[idx].get();
  if (d->optimistic) {
    d->state.power = on;
    fire(d);
  }
  enqueue(idx, Cmd{CmdType::Power, on}, false);
}

void led_set_hsv(int idx, int hue, int sat, int val) {
  if (!valid(idx)) return;
  Device *d = g_devices[idx].get();
  if (d->optimistic) {
    d->state.hue = hue;
    d->state.sat = sat;
    d->state.val = val;
    fire(d);
  }
  enqueue(idx, Cmd{CmdType::Hsv, false, hue, sat, val}, true);
}

void led_query(int idx) {
  if (valid(idx)) enqueue(idx, Cmd{CmdType::Query}, false);
}

}  // namespace ui
