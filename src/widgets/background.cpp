#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

gboolean draw_background(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots(cr, w->allocation.width, w->allocation.height);
  cairo_destroy(cr);
  return TRUE;
}

}  // namespace

void paint_dots(cairo_t *cr, int w, int h) {
  set_rgb(cr, WHITE);
  cairo_paint(cr);

  const int spacing = 30;
  const double r = 1.6;
  set_rgb(cr, DOT);
  for (int y = spacing / 2; y < h; y += spacing) {
    for (int x = spacing / 2; x < w; x += spacing) {
      cairo_arc(cr, x, y, r, 0, 2 * G_PI);
      cairo_fill(cr);
    }
  }
}

GtkWidget *make_dotted_background() {
  GtkWidget *a = detail::new_area(SCREEN_W, SCREEN_H);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_background), nullptr);
  return a;
}

}  // namespace ui
