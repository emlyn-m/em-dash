#include "net/alerts.hpp"
#include "cairo.h"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"
#include <cstdio>

namespace ui {

namespace {

gboolean draw_alerts(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  draw_text(cr, 20, 20, 285, 39, BLACK, "alerts", 30, PANGO_WEIGHT_BOLD);

  Alerts alerts = alerts_state();
  if (!alerts.last_update) {
    cairo_destroy(cr);
    return TRUE;
  }

  struct tm *t;
  for (int i = 0; i < MIN(5, alerts.alerts.size()); i++) {
    Alert alert = alerts.alerts[i];

    char timebuf[64] = {0};
    t = localtime(&alert.time);
    strftime(timebuf, 64, "%d-%m %H:%M %Z", t);

    char sevbuf[8] = {8};
    snprintf(sevbuf, 8, "sev %d", alert.severity);

    if (alert.severity <= 2) {
      cairo_set_source_rgb(cr, 0, 0, 0);
      cairo_rectangle(cr, 25, 69 + 64 * i, 295, 59);
      cairo_fill(cr);
    }
    draw_text(cr, 30, 74 + 64 * i, 286, 16, alert.severity <= 2 ? WHITE : BLACK,
              timebuf, 12, PANGO_WEIGHT_BOLD);
    draw_text(cr, 30, 74 + 64 * i, 286, 16, alert.severity <= 2 ? WHITE : BLACK,
              sevbuf, 12, PANGO_WEIGHT_BOLD, 1);
    draw_text(cr, 30, 90 + 64 * i, 286, 16, alert.severity <= 2 ? WHITE : BLACK,
              alert.category.c_str(), 12, PANGO_WEIGHT_BOLD);
    draw_text(cr, 30, 102 + 64 * i, 286, 21,
              alert.severity <= 2 ? WHITE : BLACK, alert.msg.c_str(), 16,
              PANGO_WEIGHT_BOLD);
  }

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_alerts_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_alerts), nullptr);
  return a;
}

} // namespace ui
