#include "net/weather.hpp"
#include "cairo.h"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"
#include <cstdio>

namespace ui {

namespace {

// Figma weather layout: a "weather" header, then 7 forecast rows. Each row has
// a day label, a right-aligned temp, and a small box beneath. Positions are in
// the widget's local coordinates (see the Figma "weather" frame).
constexpr int ROWS = 7;
constexpr int ROW_TOP = 69;   // first row's top
constexpr int ROW_PITCH = 76; // vertical gap between rows
constexpr int BOX_W = 226;
constexpr int BOX_H = 40;
constexpr int BOX_PAD = 5;

gboolean draw_weather(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  draw_text_tl(cr, 20, 20, BLACK, "weather", 30, PANGO_WEIGHT_BOLD);

  Weather weather = weather_state();
  if (!weather.last_update) {
    return TRUE;
  }

  // precompute ranges
  double global_tmin = 999, global_tmax = -999;
  double tmin[ROWS] = {999, 999, 999, 999, 999, 999, 999};
  double tmax[ROWS] = {-999, -999, -999, -999, -999, -999, -999};
  for (int i = 0; i < ROWS; i++) {
    for (int j = 24 * i; j < MIN(weather.events.size(), 24 * (i + 1)); j++) {
      if (weather.events[j].temp_c < tmin[i]) {
        tmin[i] = weather.events[j].temp_c;
      }
      if (weather.events[j].temp_c > tmax[i]) {
        tmax[i] = weather.events[j].temp_c;
      }
    }
    if (tmin[i] < global_tmin) {
      global_tmin = tmin[i];
    }
    if (tmax[i] > global_tmax) {
      global_tmax = tmax[i];
    }
  }

  int HOUR_WIDTH = (BOX_W - 2 * BOX_PAD) / 24;
  double T_RANGE = global_tmax - global_tmin;

  for (int i = 0; i < ROWS; i++) {
    int top = ROW_TOP + i * ROW_PITCH;
    char day_buf[32] = {0};
    snprintf(day_buf, 32, "test");
    struct tm *t = localtime(&weather.events[24 * i].time);
    strftime(day_buf, 32, "%a %-d", t);
    *day_buf |= 0x20; // lowercase first letter
    draw_text_tl(cr, 25, top, BLACK, day_buf,
                 20, // GMT+12 offset
                 PANGO_WEIGHT_NORMAL);

    set_rgb(cr, BLACK);
    cairo_set_line_width(cr, 2);
    cairo_move_to(
        cr, 25 + BOX_PAD,
        top + BOX_PAD + 26 +
            (BOX_H - 2 * BOX_PAD) *
                (1.0 -
                 ((weather.events[24 * i].temp_c - global_tmin) / T_RANGE)));
    for (int j = 1; j < 24; j++) {
      double t = weather.events[24 * i + j].temp_c;
      cairo_line_to(cr, 25 + BOX_PAD + j * HOUR_WIDTH,
                    top + BOX_PAD + 26 +
                        (BOX_H - 2 * BOX_PAD) *
                            (1.0 - ((t - global_tmin) / T_RANGE)));
    }
    cairo_stroke(cr);

    // min/max temps
    char temp_buf[32] = {0};
    snprintf(temp_buf, 32, "%d°c | %d°c", (int)tmin[i], (int)tmax[i]);
    draw_text(cr, 25, top + 5, BOX_W, 21, BLACK, temp_buf, 16,
              PANGO_WEIGHT_NORMAL, /*halign=*/1.0, /*valign=*/0.0);
  }

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_weather_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_weather), nullptr);
  return a;
}

} // namespace ui
