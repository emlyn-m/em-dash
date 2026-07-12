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

void paint_dots_at(cairo_t *cr, int w, int h, int origin_x, int origin_y) {
  set_rgb(cr, WHITE);
  cairo_paint(cr);

  const int spacing = 30;
  const double r = 1.6;
  // The global grid places dots at absolute (spacing/2 + n*spacing). Offsetting
  // by the widget's origin keeps every overlay's dots on that same lattice.
  auto phase = [&](int origin) {
    return ((spacing / 2 - origin) % spacing + spacing) % spacing;
  };

  set_rgb(cr, DOT);
  for (int y = phase(origin_y); y < h; y += spacing) {
    for (int x = phase(origin_x); x < w; x += spacing) {
      cairo_arc(cr, x, y, r, 0, 2 * G_PI);
      cairo_fill(cr);
    }
  }
}

void paint_dots(cairo_t *cr, int w, int h) { paint_dots_at(cr, w, h, 0, 0); }

GtkWidget *make_dotted_background() {
  GtkWidget *a = detail::new_area(SCREEN_W, SCREEN_H);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_background), nullptr);
  return a;
}

}  // namespace ui
