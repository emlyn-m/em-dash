#pragma once

#include "net/tuya.hpp"
#include <functional>

// High-level device control on top of the tuya protocol. Each configured LED
// (strip + lamp) gets its own persistent comm thread that owns the socket.
//
// A device's thread only connects (and reconnects) while the device is ACTIVE
// — the screen marks it active on open, inactive on close — so idle devices
// never churn the network or flap ECONN. Control commands are enqueued
// (callable from anywhere, e.g. a future "mirror" feature) and applied in order
// once connected. Every reply/push the device sends is read and its dps folded
// into the model, so state stays live without correlating replies to commands.
namespace ui {

struct LedState {
  bool online = false;
  int power = 0;
  int hue = 0; // 0..359
  int sat = 0; // 0..1000
  int val = 0; // 0..1000
};

// Create devices from LED_STRIP_* / LED_LAMP_* config and start their threads.
void led_init();

int led_count();

const tuya_led_t *led_device(int idx);
const LedState &led_state(int idx); // main-thread snapshot

// Gate connection to when the device's screen is open. Inactive => disconnect
// and stop attempting.
void led_set_active(int idx, bool active);

// Invoked on the main thread whenever the device's state changes. nullptr
// clears.
void led_on_update(int idx, std::function<void()> cb);

// Enqueue commands to the device's comm thread. Non-blocking; sent in order
// once connected. Callable from any thread.
void led_set_power(int idx, bool on);
void led_set_hsv(int idx, int hue, int sat, int val);
void led_query(int idx); // request a fresh state read

} // namespace ui
