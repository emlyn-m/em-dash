#include "widgets/clock.hpp"

#include "dispatch.hpp"
#include "theme.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>

namespace ui {

namespace {

// Only ever touched on the main thread (the worker posts updates via the
// dispatcher), so no locking is needed.
struct ClockState {
  char hour[8] = "12";
  char minute[8] = "20";
  GtkWidget *redraw_target = nullptr;
};
ClockState g_clock;

void format_now(char *hour, char *minute) {
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(hour, 8, "%02d", lt.tm_hour);
  snprintf(minute, 8, "%02d", lt.tm_min);
}

// Runs on a secondary thread: does its work off-thread, then hands the result
// to the main thread to touch the UI. Stand-in for heavier background work.
void clock_worker() {
  for (;;) {
    char hour[8], minute[8];
    format_now(hour, minute);
    std::string h = hour, m = minute;
    post_to_main([h, m] {
      snprintf(g_clock.hour, sizeof g_clock.hour, "%s", h.c_str());
      snprintf(g_clock.minute, sizeof g_clock.minute, "%s", m.c_str());
      if (g_clock.redraw_target) gtk_widget_queue_draw(g_clock.redraw_target);
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

}  // namespace

void clock_start(GtkWidget *redraw_target) {
  g_clock.redraw_target = redraw_target;
  format_now(g_clock.hour, g_clock.minute);  // correct on first paint
  std::thread(clock_worker).detach();
}

void clock_paint(cairo_t *cr) {
  // Hour over minute in the (30, 30) 276 x 204 frame.
  draw_text(cr, 30, 30, 172, 117, BLACK, g_clock.hour, 90, PANGO_WEIGHT_BOLD,
            /*halign=*/1.0, /*valign=*/0.0);
  draw_text_tl(cr, 116, 125, BLACK, g_clock.minute, 90, PANGO_WEIGHT_BOLD);
}

}  // namespace ui
