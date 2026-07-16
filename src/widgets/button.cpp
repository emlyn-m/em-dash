#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

struct ButtonView {
  char *label;
  int w, h;
  double px;
  double halign;
};

gboolean draw_button(GtkWidget *widget, GdkEventExpose *, gpointer data_v) {
  ButtonView *v = static_cast<ButtonView *>(data_v);
  cairo_t *cr = detail::begin_paint(widget);

  paint_dots_at(cr, widget->allocation.width, widget->allocation.height,
                widget->allocation.x, widget->allocation.y);

  // Black face.
  set_rgb(cr, BLACK);
  cairo_rectangle(cr, 0, 0, v->w, v->h);
  cairo_fill(cr);

  const double pad = v->halign == 0.0 ? 12.0 : 0.0;
  draw_text(cr, pad, 0, v->w - pad, v->h, WHITE, v->label, v->px,
            PANGO_WEIGHT_ULTRABOLD, v->halign, 0.5);

  cairo_destroy(cr);
  return TRUE;
}

struct TextView {
  char *label;
  int w, h;
  double px;
  unsigned hex;
};

gboolean draw_text_button(GtkWidget *widget, GdkEventExpose *,
                          gpointer data_v) {
  TextView *v = static_cast<TextView *>(data_v);
  cairo_t *cr = detail::begin_paint(widget);
  set_rgb(cr, WHITE);
  cairo_rectangle(cr, 0, 0, v->w, v->h);
  cairo_fill(cr);
  paint_dots(cr, widget->allocation.width, widget->allocation.height);
  draw_text(cr, 0, 0, v->w, v->h, v->hex, v->label, v->px, PANGO_WEIGHT_BOLD, 0,
            0.5);
  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_button(const char *label, int w, int h, double px,
                       double halign, PressFn cb, gpointer data) {
  GtkWidget *a = detail::new_area(w + detail::SHADOW, h + detail::SHADOW);
  ButtonView *v =
      detail::attach(a, ButtonView{g_strdup(label), w, h, px, halign});
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_button), v);
  detail::make_clickable(a, cb, data);
  return a;
}

GtkWidget *make_text_button(const char *label, int w, int h, double px,
                            unsigned bg, PressFn cb, gpointer data) {
  GtkWidget *a = detail::new_area(w, h);
  TextView *v = detail::attach(a, TextView{g_strdup(label), w, h, px, bg});
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_text_button), v);
  detail::make_clickable(a, cb, data);
  return a;
}

} // namespace ui
