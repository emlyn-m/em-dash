#include "net/weather.hpp"
#include "cairo.h"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"
#include <cstdio>
#include <ctime>

namespace ui {

namespace {

constexpr int ROWS = 6;
constexpr int ROW_TOP = 0;    // first row's top
constexpr int ROW_PITCH = 74; // vertical gap between rows
constexpr int BOX_W = 221;
constexpr int BOX_H = 32;

const char *get_wmo_label(int code) {
  switch (code) {
  case 0:
    return "clear skies";
  case 1:
    return "mainly clear";
  case 2:
    return "cloudy";
  case 3:
    return "very cloudy";
  case 45:
    return "foggy";
  case 48:
    return "icy fog";
  case 51:
    return "light drizzle";
  case 53:
    return "drizzle";
  case 55:
    return "heavy drizzle";
  case 56:
    return "light freezing drizzle";
  case 57:
    return "freezing drizzle";
  case 61:
    return "light rain";
  case 63:
    return "rain";
  case 65:
    return "heavy rain";
  case 66:
    return "light freezing rain";
  case 67:
    return "freezing rain";
  case 71:
    return "light snow";
  case 73:
    return "snow";
  case 75:
    return "heavy snow";
  case 77:
    return "snow grains";
  case 80:
    return "light showers";
  case 81:
    return "showers";
  case 82:
    return "heavy showers";
  case 85:
    return "light snow showers";
  case 86:
    return "snow showers";
  case 95:
    return "thunderstorms";
  case 96:
    return "thunderstorms, light hail";
  case 99:
    return "thunderstorms + hail";
  default:
    return "secret";
  }
}

constexpr int TIERS = 7;
constexpr int TIER_WEIGHT[TIERS] = {1, 1, 2, 3, 4, 5, 20};

int wmo_tier(int code) {
  if (code <= 1)
    return 0; // clear
  if (code <= 3)
    return 1; // cloud
  if (code <= 48)
    return 2; // fog
  if (code <= 57)
    return 3; // drizzle
  if (code <= 67 || (code >= 80 && code <= 82))
    return 4; // rain
  if (code <= 86)
    return 5; // snow
  return 6;   // thunder
}

// max(tier count in waking hours * tier weight), then most observed code in
// teir. -1 if no waking hours in range
int day_wmo_code(const std::vector<WeatherEvent> &events, size_t begin,
                 size_t end) {
  int score[TIERS] = {};
  int hours[100] = {};
  for (size_t i = begin; i < end && i < events.size(); i++) {
    struct tm t;
    localtime_r(&events[i].time, &t);
    int code = events[i].wmo_code;
    if (t.tm_hour < 8 || t.tm_hour >= 21 || code < 0 || code >= 100)
      continue;
    score[wmo_tier(code)] += TIER_WEIGHT[wmo_tier(code)];
    hours[code]++;
  }

  int tier = 0;
  for (int k = 1; k < TIERS; k++)
    if (score[k] >= score[tier])
      tier = k;

  int best = -1;
  for (int c = 0; c < 100; c++)
    if (hours[c] && wmo_tier(c) == tier &&
        (best < 0 || hours[c] >= hours[best]))
      best = c;
  return best;
}

void draw_wmo_icon(cairo_t *cr, double x, double y, int size, int code) {
  set_rgb(cr, BLACK);
  cairo_rectangle(cr, x, y, size, size);
  cairo_fill(cr);
}

gboolean draw_weather(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  Weather weather = weather_state();
  if (!weather.last_update) {
    cairo_destroy(cr);
    return TRUE;
  }

  // precompute ranges
  double global_tmin = 999, global_tmax = -999;
  double tmin[ROWS] = {0};
  double tmax[ROWS] = {0};
  for (int i = 0; i < ROWS; i++) {
    tmin[i] = global_tmin;
    tmax[i] = global_tmax;
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

  double HOUR_WIDTH = BOX_W / 23.0; // 24 samples span 23 gaps
  double T_RANGE = global_tmax - global_tmin;

  for (int i = 0; i < ROWS; i++) {
    int top = ROW_TOP + i * ROW_PITCH;

    draw_wmo_icon(cr, 25, top + 6, 16,
                  day_wmo_code(weather.events, 24 * i, 24 * (i + 1)));

    char day_buf[32] = {0};
    struct tm *t = localtime(&weather.events[24 * i].time);
    strftime(day_buf, 32, "%a %-d", t);
    *day_buf |= 0x20; // lowercase first letter
    draw_text_tl(cr, 50, top, BLACK, day_buf,
                 20, // GMT+12 offset
                 PANGO_WEIGHT_NORMAL);

    set_rgb(cr, BLACK);
    cairo_set_line_width(cr, 2);
    cairo_move_to(
        cr, 25,
        top + 26 +
            (BOX_H) * (1.0 - ((weather.events[24 * i].temp_c - global_tmin) /
                              T_RANGE)));
    for (int j = 1; j < 24; j++) {
      double t = weather.events[24 * i + j].temp_c;
      cairo_line_to(cr, 25 + j * HOUR_WIDTH,
                    top + 26 + (BOX_H) * (1.0 - ((t - global_tmin) / T_RANGE)));
    }
    cairo_stroke(cr);

    // min/max temps
    char temp_buf[32] = {0};
    snprintf(temp_buf, 32, "%d° | %d°", (int)tmin[i], (int)tmax[i]);
    draw_text(cr, 25, top + 5, BOX_W, 21, BLACK, temp_buf, 16,
              PANGO_WEIGHT_NORMAL, /*halign=*/1.0, /*valign=*/0.0);
  }

  cairo_destroy(cr);
  return TRUE;
}

gboolean draw_weather_summary(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  const int show = 2;

  Weather weather = weather_state();
  if (!weather.last_update) {
    cairo_destroy(cr);
    return TRUE;
  }

  int offset = 0;
  time_t now = time(NULL);
  while (offset + 1 < weather.events.size() &&
         weather.events[offset + 1].time <= now) {
    offset++;
  }

  draw_wmo_icon(cr, w->allocation.width - 40, 11, 40, // 11: cap top of temp
                weather.events[offset].wmo_code);

  char summary[128] = {0};
  snprintf(summary, 128, "%.0f°", weather.events[offset].temp_c);
  draw_text_tl(cr, 0, 0, BLACK, summary, 32, PANGO_WEIGHT_NORMAL);
  snprintf(summary, 128, "%s", get_wmo_label(weather.events[offset].wmo_code));
  draw_text_tl(cr, 0, 40, BLACK, summary, 16, PANGO_WEIGHT_NORMAL);
  snprintf(summary, 128,
           weather.events[offset].uv_index >= 0 ? "uv %d" : "uv unknown",
           weather.events[offset].uv_index);
  draw_text_tl(cr, 0, 60, BLACK, summary, 16, PANGO_WEIGHT_NORMAL);

  struct tm t;
  uint start = offset, end;
  for (int i = 0; i < 1 + show; i++) {
    if (start >= weather.events.size()) {
      break;
    }

    end = start + 1;
    while (end < weather.events.size() &&
           weather.events[start].wmo_code == weather.events[end].wmo_code) {
      end++;
    };

    if (i > 0) {
      localtime_r(&weather.events[start].time, &t);
      int written = snprintf(summary, 128, "%s at ",
                             get_wmo_label(weather.events[start].wmo_code));
      strftime(summary + written, 128 - written, "%H:%M", &t);
      draw_text_tl(cr, 0, 85 + 16 * (i - 1), BLACK, summary, 14,
                   PANGO_WEIGHT_LIGHT);
    }

    start = end;
  }

  // cairo_save(cr);
  // set_rgb(cr, BLACK);
  // cairo_rectangle(cr, 0, w->allocation.height - 30, w->allocation.width, 2);
  // cairo_fill(cr);
  // cairo_restore(cr);

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_weather_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_weather), nullptr);
  return a;
}

GtkWidget *make_weather_summary_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_weather_summary),
                   nullptr);
  return a;
}

} // namespace ui
