#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

struct CardView {
  int w, h;
};

gboolean draw_card(GtkWidget *widget, GdkEventExpose *, gpointer data_v) {
  CardView *v = static_cast<CardView *>(data_v);
  cairo_t *cr = detail::begin_paint(widget);

  // Hard drop shadow.
  set_rgb(cr, BLACK);
  cairo_rectangle(cr, detail::SHADOW, detail::SHADOW, v->w, v->h);
  cairo_fill(cr);

  // White panel.
  set_rgb(cr, WHITE);
  cairo_rectangle(cr, 0, 0, v->w, v->h);
  cairo_fill(cr);

  // Border.
  set_rgb(cr, BLACK);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, 0.5, 0.5, v->w - 1, v->h - 1);
  cairo_stroke(cr);

  cairo_destroy(cr);
  return TRUE;
}

}  // namespace

GtkWidget *make_card(int w, int h) {
  GtkWidget *a = detail::new_area(w + detail::SHADOW, h + detail::SHADOW);
  CardView *v = detail::attach(a, CardView{w, h});
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_card), v);
  return a;
}

}  // namespace ui
