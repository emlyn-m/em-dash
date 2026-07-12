#include "widgets/slider.hpp"

#include "widgets/common.hpp"

namespace ui {

namespace {

struct SliderState {
  int w, h;
  float value;
  bool dragging;
  SliderFn on_change;
  SliderFn on_release;
  void *data;
};

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

SliderState *state_of(GtkWidget *w) {
  return static_cast<SliderState *>(g_object_get_data(G_OBJECT(w), "ui-state"));
}

gboolean draw_slider(GtkWidget *widget, GdkEventExpose *, gpointer d) {
  SliderState *s = static_cast<SliderState *>(d);
  cairo_t *cr = detail::begin_paint(widget);

  set_rgb(cr, WHITE);
  cairo_paint(cr);

  int fill = (int)(s->value * s->h);  // filled from the bottom
  set_rgb(cr, BLACK);
  cairo_rectangle(cr, 0, s->h - fill, s->w, fill);
  cairo_fill(cr);

  set_rgb(cr, BLACK);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, 0.5, 0.5, s->w - 1, s->h - 1);
  cairo_stroke(cr);

  cairo_destroy(cr);
  return TRUE;
}

void set_from_y(GtkWidget *widget, SliderState *s, double y) {
  s->value = clamp01(1.0 - y / s->h);
  gtk_widget_queue_draw(widget);
  if (s->on_change) s->on_change(s->value, s->data);
}

gboolean slider_press(GtkWidget *widget, GdkEventButton *e, gpointer d) {
  SliderState *s = static_cast<SliderState *>(d);
  s->dragging = true;
  set_from_y(widget, s, e->y);
  return TRUE;
}

gboolean slider_motion(GtkWidget *widget, GdkEventMotion *e, gpointer d) {
  SliderState *s = static_cast<SliderState *>(d);
  if (s->dragging) set_from_y(widget, s, e->y);
  return TRUE;
}

gboolean slider_release(GtkWidget *widget, GdkEventButton *e, gpointer d) {
  SliderState *s = static_cast<SliderState *>(d);
  if (!s->dragging) return TRUE;
  s->dragging = false;
  s->value = clamp01(1.0 - e->y / s->h);
  gtk_widget_queue_draw(widget);
  if (s->on_release) s->on_release(s->value, s->data);
  return TRUE;
}

}  // namespace

GtkWidget *make_slider(int w, int h, void *data, SliderFn on_change,
                       SliderFn on_release) {
  GtkWidget *a = detail::new_area(w, h);
  SliderState *s = detail::attach(
      a, SliderState{w, h, 0.0f, false, on_change, on_release, data});
  gtk_widget_add_events(a, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                               GDK_POINTER_MOTION_MASK);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_slider), s);
  g_signal_connect(a, "button-press-event", G_CALLBACK(slider_press), s);
  g_signal_connect(a, "motion-notify-event", G_CALLBACK(slider_motion), s);
  g_signal_connect(a, "button-release-event", G_CALLBACK(slider_release), s);
  return a;
}

void slider_sync(GtkWidget *slider, float value) {
  SliderState *s = state_of(slider);
  if (!s || s->dragging) return;  // never fight an in-progress drag
  s->value = clamp01(value);
  gtk_widget_queue_draw(slider);
}

float slider_value(GtkWidget *slider) {
  SliderState *s = state_of(slider);
  return s ? s->value : 0.0f;
}

}  // namespace ui
