#pragma once

#include <gtk-2.0/gtk/gtk.h>

// The clock digits sit directly on the dotted field (no opaque box), so rather
// than being a standalone widget they are composited into a host drawing area.
// This module owns the clock's state, its background worker thread, and its
// rendering; the host screen just calls clock_paint() from its expose handler.
namespace ui {

// Start ticking. `redraw_target` is queued for redraw on each update; its
// expose handler must call clock_paint(). Call once.
void clock_start(GtkWidget *redraw_target);

// Draw the current time into the (30, 30) 276 x 204 clock frame.
void clock_paint(cairo_t *cr);

}  // namespace ui
