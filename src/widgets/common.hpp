#pragma once

#include "theme.hpp"
#include "widgets/widgets.hpp"

#include <gtk-2.0/gtk/gtk.h>

// Internal helpers shared by the widget implementations. Not part of the
// public widget API.
namespace ui {
namespace detail {

constexpr int SHADOW = 4;  // hard offset drop shadow, in px

inline cairo_t *begin_paint(GtkWidget *w) { return gdk_cairo_create(w->window); }

inline GtkWidget *new_area(int w, int h) {
  GtkWidget *a = gtk_drawing_area_new();
  gtk_widget_set_size_request(a, w, h);
  return a;
}

inline void make_clickable(GtkWidget *a, PressFn cb, gpointer data) {
  if (!cb) return;
  gtk_widget_add_events(a, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(a, "button-press-event", G_CALLBACK(cb), data);
}

// Attach heap state to a widget so it lives exactly as long as the widget.
template <typename T>
T *attach(GtkWidget *w, const T &value) {
  T *p = g_new0(T, 1);
  *p = value;
  g_object_set_data_full(G_OBJECT(w), "ui-state", p, g_free);
  return p;
}

}  // namespace detail
}  // namespace ui
