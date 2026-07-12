#pragma once

#include <ctime>
#include <functional>
#include <string>
#include <vector>

// Device/service telemetry. Two independent workers refresh the model on
// different cadences: a fast "local" one (battery/current/wifi from sysfs) and
// a slower "net" one (tailscale devices/services, public IP, ping/jitter).
namespace ui {

struct Device {
  std::string name;
  std::string alias;
  std::string ip;
  bool online;
};

struct Service {
  std::string name;
  std::string status;
};

struct Telem {
  // local
  int battery = 0;
  int current = 0;
  int wifi_strength = 0;
  // net
  std::vector<Device> devices;
  std::vector<Service> services;
  std::string ip;
  std::vector<long> ping_history;  // recent round-trip times, seconds
  double jitter = 0;

  time_t last_update = 0;      // local
  time_t last_update_net = 0;  // net
};

void telem_start(std::function<void()> on_update);
const Telem &telem_state();

}  // namespace ui
