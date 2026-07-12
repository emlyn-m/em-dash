#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

// TODO: replace the debug fill with the real forecast rendering. This is the
// Cairo surface the weather drawing will target.
gboolean draw_weather(GtkWidget *widget, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(widget);
  set_rgb(cr, DEBUG_RED);
  cairo_paint(cr);
  cairo_destroy(cr);
  return TRUE;
}

}  // namespace

GtkWidget *make_weather_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_weather), nullptr);
  return a;
}

}  // namespace ui
