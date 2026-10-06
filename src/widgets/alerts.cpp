#include "net/alerts.hpp"
#include "cairo.h"
#include "config.hpp"
#include "log.hpp"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"
#include <cstdio>

namespace ui {

namespace {

cairo_surface_t *img_surf;

gboolean draw_alerts(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  Alerts alerts = alerts_state();
  if (0 && !alerts.last_update) {
    cairo_destroy(cr);
    return TRUE;
  }

  if (alerts.alerts.size() == 0) {
    cairo_status_t status;
    if ((status = cairo_surface_status(img_surf))) {
      LOG(PRI_ERR, "alert img failed to load: %s\n",
          cairo_status_to_string(status));
    } else {
      cairo_set_source_surface(cr, img_surf, 0, 0);
      cairo_paint(cr);
    }
  } else {

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
        cairo_rectangle(cr, 20, 20 + 64 * i, 305, 59);
        cairo_fill(cr);
      }
      draw_text(cr, 30, 25 + 64 * i, 286, 16,
                alert.severity <= 2 ? WHITE : BLACK, timebuf, 12,
                PANGO_WEIGHT_BOLD);
      draw_text(cr, 30, 25 + 64 * i, 286, 16,
                alert.severity <= 2 ? WHITE : BLACK, sevbuf, 12,
                PANGO_WEIGHT_BOLD, 1);
      draw_text(cr, 30, 41 + 64 * i, 286, 16,
                alert.severity <= 2 ? WHITE : BLACK, alert.category.c_str(), 12,
                PANGO_WEIGHT_BOLD);
      draw_text(cr, 30, 53 + 64 * i, 286, 21,
                alert.severity <= 2 ? WHITE : BLACK, alert.msg.c_str(), 16,
                PANGO_WEIGHT_BOLD);
    }
  }
  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_alerts_surface(int w, int h) {
  char img[128] = {0};
  snprintf(img, 128, "%s/priv-alert-variant.png", get_attr_str("ASSET_PATH"));
  img_surf = cairo_image_surface_create_from_png(img);

  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_alerts), nullptr);
  return a;
}

} // namespace ui
