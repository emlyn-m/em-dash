#include "cairo.h"
#include "net/calendar.hpp"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

gboolean draw_image(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  // paint_dots_at(cr, w->allocation.width, w->allocation.height,
  // w->allocation.x, w->allocation.y);

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_image_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_image), nullptr);
  return a;
}

} // namespace ui
