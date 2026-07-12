#include "net/telem.hpp"
#include "cairo.h"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

void draw_telem_services(cairo_t *cr, Telem telem) {
  draw_text_tl(cr, 20, 20, BLACK, "services", 30, PANGO_WEIGHT_BOLD);
  for (int i = 0; i < MIN(6, telem.services.size()); i++) {
    PangoWeight weight = PANGO_WEIGHT_NORMAL;
    if (telem.services[i].status != "healthy") {
      weight = PANGO_WEIGHT_BOLD;
    }
    draw_text(cr, 25, 69 + 29 * i, 274, 21, BLACK,
              telem.services[i].name.c_str(), 16, weight, 0.0);
    draw_text(cr, 25, 69 + 29 * i, 274, 21, BLACK,
              telem.services[i].status.c_str(), 16, weight, 1.0);
  }
}

void draw_telem_devices(cairo_t *cr, Telem telem) {
  draw_text_tl(cr, 20, 266 + 29, BLACK, "devices", 30, PANGO_WEIGHT_BOLD);
  for (int i = 0; i < MIN(6, telem.devices.size()); i++) {
    PangoWeight weight = PANGO_WEIGHT_NORMAL;
    if (telem.devices[i].online) {
      weight = PANGO_WEIGHT_BOLD;
    }
    draw_text(cr, 25, 315 + 29 + 29 * i, 274, 21, BLACK,
              telem.devices[i].alias.c_str(), 16, weight, 0.0);
    draw_text(cr, 25, 315 + 29 + 29 * i, 274, 21, BLACK,
              telem.devices[i].online ? telem.devices[i].ip.c_str() : "offline",
              16, weight, 1.0);
  }
}

void draw_telem_health(cairo_t *cr, Telem telem) {
  draw_text_tl(cr, 20, 570, BLACK, "health", 30, PANGO_WEIGHT_BOLD);

  char battery_buf[16] = {0};
  snprintf(battery_buf, 16, "%s %d%%", telem.current > 0 ? "⚡ " : "",
           telem.battery);
  draw_text(cr, 25, 619, 274, 21, BLACK, "battery", 16,
            telem.current > 0 ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, 0.0);
  draw_text(cr, 25, 619, 274, 21, BLACK, battery_buf, 16,
            telem.current > 0 ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, 1.0);

  char rssi_buf[16] = {0};
  snprintf(rssi_buf, 16, "%ddB", telem.wifi_strength);
  draw_text(cr, 25, 648, 274, 21, BLACK, "rssi", 16, PANGO_WEIGHT_NORMAL, 0.0);
  draw_text(cr, 25, 648, 274, 21, BLACK, rssi_buf, 16, PANGO_WEIGHT_NORMAL,
            1.0);

  char ping_buf[16] = {0};
  float ping = 0;
  float npings = telem.ping_history.size();
  if (npings) {
    for (int i = 0; i < npings; i++) {
      ping += telem.ping_history[i];
    }
  }
  snprintf(ping_buf, 16, npings ? "%dms" : "-", (int)(ping / npings));
  draw_text(cr, 25, 677, 274, 21, BLACK, "ping", 16, PANGO_WEIGHT_NORMAL, 0.0);
  draw_text(cr, 25, 677, 274, 21, BLACK, ping_buf, 16, PANGO_WEIGHT_NORMAL,
            1.0);

  draw_text(cr, 25, 706, 274, 21, BLACK, "ipv4", 16, PANGO_WEIGHT_NORMAL, 0.0);
  draw_text(cr, 25, 706, 274, 21, BLACK, telem.ip.c_str(), 16,
            PANGO_WEIGHT_NORMAL, 1.0);
}

gboolean draw_telem(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  Telem telem = telem_state();

  draw_telem_services(cr, telem);
  draw_telem_devices(cr, telem);
  draw_telem_health(cr, telem);

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_telem_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_telem), nullptr);
  return a;
}

} // namespace ui
