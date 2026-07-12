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

  // Hard drop shadow, offset down-right.
  set_rgb(cr, BLACK);
  cairo_rectangle(cr, detail::SHADOW, detail::SHADOW, v->w, v->h);
  cairo_fill(cr);

  // White face.
  set_rgb(cr, WHITE);
  cairo_rectangle(cr, 0, 0, v->w, v->h);
  cairo_fill(cr);

  // 1px black border.
  set_rgb(cr, BLACK);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, 0.5, 0.5, v->w - 1, v->h - 1);
  cairo_stroke(cr);

  const double pad = v->halign == 0.0 ? 12.0 : 0.0;
  draw_text(cr, pad, 0, v->w - pad, v->h, BLACK, v->label, v->px,
            PANGO_WEIGHT_MEDIUM, v->halign, 0.5);

  cairo_destroy(cr);
  return TRUE;
}

struct BoxView {
  char *label;
  int w, h;
  double px;
  unsigned bg;
};

gboolean draw_box_button(GtkWidget *widget, GdkEventExpose *, gpointer data_v) {
  BoxView *v = static_cast<BoxView *>(data_v);
  cairo_t *cr = detail::begin_paint(widget);
  set_rgb(cr, v->bg);
  cairo_rectangle(cr, 0, 0, v->w, v->h);
  cairo_fill(cr);
  draw_text(cr, 0, 0, v->w, v->h, BLACK, v->label, v->px, PANGO_WEIGHT_MEDIUM,
            0.5, 0.5);
  cairo_destroy(cr);
  return TRUE;
}

}  // namespace

GtkWidget *make_button(const char *label, int w, int h, double px,
                       double halign, PressFn cb, gpointer data) {
  GtkWidget *a = detail::new_area(w + detail::SHADOW, h + detail::SHADOW);
  ButtonView *v =
      detail::attach(a, ButtonView{g_strdup(label), w, h, px, halign});
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_button), v);
  detail::make_clickable(a, cb, data);
  return a;
}

GtkWidget *make_box_button(const char *label, int w, int h, double px,
                           unsigned bg, PressFn cb, gpointer data) {
  GtkWidget *a = detail::new_area(w, h);
  BoxView *v = detail::attach(a, BoxView{g_strdup(label), w, h, px, bg});
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_box_button), v);
  detail::make_clickable(a, cb, data);
  return a;
}

}  // namespace ui
