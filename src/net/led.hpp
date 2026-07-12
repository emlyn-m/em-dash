#pragma once

#include <functional>

// High-level device control on top of the tuya protocol. Manages the
// configured LEDs (strip + lamp), serialises socket access per device, and runs
// every network operation off the main thread — control actions fire-and-forget
// worker threads; state changes flow back through the dispatcher.
namespace ui {

struct LedState {
  bool online = false;
  int power = 0;
  int hue = 0;  // 0..359
  int sat = 0;  // 0..1000
  int val = 0;  // 0..1000
};

// Create the devices described by LED_STRIP_* / LED_LAMP_* config. Call once.
void led_init();

int led_count();
const char *led_name(int idx);
const LedState &led_state(int idx);  // main-thread snapshot

// Control. Each spawns a worker that sends off-thread and, on success, updates
// the model via the dispatcher.
void led_set_power(int idx, bool on);
void led_set_hsv(int idx, int hue, int sat, int val);

// Query the device's live state once, off-thread. On completion the model is
// updated on the main thread and on_update (if any) is invoked.
void led_refresh(int idx, std::function<void()> on_update);

}  // namespace ui
