#include "net/weather.hpp"
#include "cairo.h"
#include "config.hpp"
#include "log.hpp"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"
#include <cstdio>
#include <ctime>
#include <map>
#include <string>

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
    return "light drizzles";
  case 53:
    return "drizzles";
  case 55:
    return "heavy drizzles";
  case 56:
    return "light freezing drizzles";
  case 57:
    return "freezing drizzles";
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
    return "thunderstorms + light hail";
  case 99:
    return "thunderstorms + hail";
  default:
    return "wouldn't you like to know";
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

constexpr int WMO_CODES = 100;

struct WmoIcon {
  int code;
  const char *file;
};

constexpr WmoIcon ICON_FILES[] = {
    {0, "clearsky_night.png"},
    {1, "fair_night.png"},
    {2, "cloudy.png"},
    {3, "cloudy.png"},
    {45, "fog.png"},
    {48, "icy_fog.png"},
    {51, "lightrain.png"},
    {53, "rain.png"},
    {55, "heavyrain.png"},
    {56, "freezing_drizzle.png"},
    {57, "freezing_drizzle.png"},
    {61, "lightrain.png"},
    {63, "lightrain.png"},
    {65, "rain.png"},
    {66, "freezing_rain.png"},
    {67, "freezing_rain.png"},
    {71, "lightsnow.png"},
    {73, "snow.png"},
    {75, "heavysnow.png"},
    {77, "snow.png"},
    {80, "lightrain.png"},
    {81, "rain.png"},
    {82, "heavyrain.png"},
    {85, "lightsnow.png"},
    {86, "heavysnow.png"},
    {95, "rainandthunder.png"},
    {96, "sleetandthunder.png"},
    {99, "heavysleetandthunder.png"},
};
cairo_surface_t *icons[WMO_CODES] = {};

void load_wmo_icons() {
  static bool loaded = false;
  if (loaded)
    return;
  loaded = true;

  const char *root = get_attr_str("ASSET_PATH");
  std::map<std::string, cairo_surface_t *> by_file;
  for (const WmoIcon &e : ICON_FILES) {
    auto it = by_file.find(e.file);
    if (it == by_file.end()) {
      char path[256];
      snprintf(path, sizeof path, "%s/weather/%s", root ? root : "assets",
               e.file);
      cairo_surface_t *s = cairo_image_surface_create_from_png(path);
      if (cairo_status_t status = cairo_surface_status(s)) {
        LOG(PRI_ERR, "weather icon %s failed to load: %s\n", path,
            cairo_status_to_string(status));
        cairo_surface_destroy(s);
        s = nullptr;
      }
      it = by_file.emplace(e.file, s).first;
    }
    icons[e.code] = it->second;
  }
}

cairo_surface_t *wmo_icon(int code) {
  return code < 0 || code >= WMO_CODES ? nullptr : icons[code];
}

void draw_wmo_icon(cairo_t *cr, double x, double y, int size, int code) {
  cairo_surface_t *icon = wmo_icon(code);
  cairo_save(cr);

  if (!icon) {
    set_rgb(cr, BLACK);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x, y, size, size);
    cairo_line_to(cr, x + size, y + size);
    cairo_stroke(cr);
  } else {
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)size / cairo_image_surface_get_width(icon),
                (double)size / cairo_image_surface_get_height(icon));
    cairo_set_source_surface(cr, icon, 0, 0);
    cairo_paint(cr);
  }

  cairo_restore(cr);
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

    draw_wmo_icon(cr, 25, top + 2, 24,
                  day_wmo_code(weather.events, 24 * i, 24 * (i + 1)));

    char day_buf[32] = {0};
    struct tm *t = localtime(&weather.events[24 * i].time);
    strftime(day_buf, 32, "%a %-d", t);
    *day_buf |= 0x20; // lowercase first letter
    draw_text_tl(cr, 54, top + 1, BLACK, day_buf,
                 18, // GMT+12 offset
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
  load_wmo_icons();
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_weather), nullptr);
  return a;
}

GtkWidget *make_weather_summary_surface(int w, int h) {
  load_wmo_icons();
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_weather_summary),
                   nullptr);
  return a;
}

} // namespace ui
