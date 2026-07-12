#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

gboolean draw_fill(GtkWidget *widget, GdkEventExpose *, gpointer data_v) {
  unsigned hex = static_cast<unsigned>(GPOINTER_TO_UINT(data_v));
  cairo_t *cr = detail::begin_paint(widget);
  set_rgb(cr, hex);
  cairo_paint(cr);
  cairo_destroy(cr);
  return TRUE;
}

struct LabelView {
  char *text;
  int w, h;
  double px;
  PangoWeight weight;
  double halign;
  unsigned bg;
};

gboolean draw_label(GtkWidget *widget, GdkEventExpose *, gpointer data_v) {
  LabelView *v = static_cast<LabelView *>(data_v);
  cairo_t *cr = detail::begin_paint(widget);
  set_rgb(cr, v->bg);
  cairo_paint(cr);
  draw_text(cr, 0, 0, v->w, v->h, BLACK, v->text, v->px, v->weight, v->halign,
            0.5);
  cairo_destroy(cr);
  return TRUE;
}

}  // namespace

GtkWidget *make_fill(int w, int h, unsigned hex) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_fill),
                   GUINT_TO_POINTER(hex));
  return a;
}

GtkWidget *make_label(const char *text, int w, int h, double px,
                      PangoWeight weight, double halign, unsigned bg) {
  GtkWidget *a = detail::new_area(w, h);
  LabelView *v = detail::attach(
      a, LabelView{g_strdup(text), w, h, px, weight, halign, bg});
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_label), v);
  return a;
}

}  // namespace ui
