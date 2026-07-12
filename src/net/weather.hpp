#pragma once

#include <ctime>
#include <functional>
#include <vector>

// Weather feed. A background worker fetches + parses the forecast off-thread,
// then hands the result to the main thread (via the dispatcher) which swaps it
// into the model and invokes the on_update callback. The model is pure data:
// UI code reads weather_state() from its expose handler.
namespace ui {

struct WeatherEvent {
  time_t time;
  double temp_c;
  double rain_prob;  // percent, 0..100
  int wmo_code;      // WMO weather-interpretation code
};

struct Weather {
  std::vector<WeatherEvent> events;
  time_t last_update = 0;
};

// Start the periodic fetch loop. `on_update` runs on the main thread after each
// successful refresh (e.g. queue a redraw). Call once.
void weather_start(std::function<void()> on_update);

// Current model snapshot. Main-thread only.
const Weather &weather_state();

}  // namespace ui
