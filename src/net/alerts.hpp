#pragma once

#include <ctime>
#include <functional>
#include <string>
#include <vector>

// Alert feed (XMPP-sourced). Same worker/model pattern as weather.
namespace ui {

struct Alert {
  time_t time;
  int severity;
  std::string category;
  std::string msg;
};

struct Alerts {
  std::vector<Alert> alerts;
  time_t last_update = 0;
};

void alerts_start(std::function<void()> on_update);
const Alerts &alerts_state();

}  // namespace ui
