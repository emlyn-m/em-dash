#include "widgets/clock.hpp"

#include "dispatch.hpp"
#include "pango/pango-font.h"
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
  char day[8] = "mon";
  char date[8] = "jan 1";
  GtkWidget *redraw_target = nullptr;
};
ClockState g_clock;

const char *days[7] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
const char *months[12] = {"jan", "feb", "mar", "apr", "may", "jun",
                          "jul", "aug", "sep", "oct", "nov", "dec"};

void format_now(char *hour, char *minute, char *day, char *date) {
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(hour, 8, "%02d", lt.tm_hour);
  snprintf(minute, 8, "%02d", lt.tm_min);
  snprintf(day, 8, "%s", days[lt.tm_wday]);
  snprintf(date, 8, "%d %s", lt.tm_mday, months[lt.tm_mon]);
}

// Runs on a secondary thread: does its work off-thread, then hands the result
// to the main thread to touch the UI. Stand-in for heavier background work.
void clock_worker() {
  for (;;) {
    char hour[8], minute[8], day[8], date[8];
    format_now(hour, minute, day, date);
    std::string h = hour, m = minute, d = day, dt = date;
    post_to_main([h, m, d, dt] {
      snprintf(g_clock.hour, sizeof g_clock.hour, "%s", h.c_str());
      snprintf(g_clock.minute, sizeof g_clock.minute, "%s", m.c_str());
      snprintf(g_clock.day, sizeof g_clock.day, "%s", d.c_str());
      snprintf(g_clock.date, sizeof g_clock.date, "%s", dt.c_str());
      if (g_clock.redraw_target)
        gtk_widget_queue_draw(g_clock.redraw_target);
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

} // namespace

void clock_start(GtkWidget *redraw_target) {
  g_clock.redraw_target = redraw_target;
  format_now(g_clock.hour, g_clock.minute, g_clock.day, g_clock.date);
  std::thread(clock_worker).detach();
}

void clock_paint(cairo_t *cr) {
  draw_text(cr, 0, 78, 154, 117, BLACK, g_clock.hour, 90, PANGO_WEIGHT_BOLD,
            1.0, 0.0);
  draw_text_tl(cr, 70, 146, BLACK, g_clock.minute, 90, PANGO_WEIGHT_BOLD);

  draw_text_tl(cr, 190, 190, BLACK, g_clock.day, 24, PANGO_WEIGHT_BOLD);
  draw_text_tl(cr, 190, 210, BLACK, g_clock.date, 24, PANGO_WEIGHT_BOLD);
  cairo_fill(cr);
}

} // namespace ui
