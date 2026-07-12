#pragma once

#include <ctime>
#include <functional>
#include <string>
#include <vector>

// Google Calendar feed. The worker keeps an OAuth access token (refreshed via a
// signed JWT when it expires) private to itself, and publishes only parsed
// events into the model.
namespace ui {

struct CalEvent {
  std::string title;
  time_t start_time;
  time_t end_time;
};

struct Calendar {
  std::vector<CalEvent> events;
  time_t last_update = 0;
};

void calendar_start(std::function<void()> on_update);
const Calendar &calendar_state();

}  // namespace ui
