#include "cairo.h"
#include "log.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

gboolean draw_image(GtkWidget *w, GdkEventExpose *, gpointer gimg) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);
  cairo_surface_t *img = (cairo_surface_t *)gimg;
  cairo_status_t status;
  if ((status = cairo_surface_status(img))) {
    LOG(PRI_ERR, "img failed to load: %s\n", cairo_status_to_string(status));
    cairo_destroy(cr);
    return TRUE;
  }

  cairo_set_source_surface(cr, img, 0, 0);
  cairo_paint(cr);

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_image_surface(int w, int h, char *path) {
  GtkWidget *a = detail::new_area(w, h);
  cairo_surface_t *image = cairo_image_surface_create_from_png(path);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_image), image);
  return a;
}

} // namespace ui
