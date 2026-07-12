#pragma once

#include <gtk-2.0/gtk/gtk.h>

// Factory functions for the self-drawing widgets that make up the UI. Each
// returns a GtkDrawingArea sized in Figma pixels, to be placed on a GtkFixed
// screen at an absolute (x, y). Widgets with their own state/lifecycle (clock,
// and later sliders) live in their own headers instead.
namespace ui {

using PressFn = gboolean (*)(GtkWidget *, GdkEventButton *, gpointer);

// --- background ------------------------------------------------------------

// Paint the white background + light grey dot lattice onto an existing cairo
// context (for screens that composite their own art over the dots).
void paint_dots(cairo_t *cr, int w, int h);

// Full-screen dotted grid.
GtkWidget *make_dotted_background();

// --- buttons ---------------------------------------------------------------

// White button with a 1px black border and a hard 4px offset drop shadow.
// The returned widget is (w+4) x (h+4) to leave room for the shadow.
// `halign`: 0 = left-padded, 0.5 = centred.
GtkWidget *make_button(const char *label, int w, int h, double px,
                       double halign, PressFn cb, gpointer data);

// Solid filled box (grey by default) with centred text — the modal's small
// "conn"/"exit btn" controls. Clickable when cb is non-null.
GtkWidget *make_box_button(const char *label, int w, int h, double px,
                           unsigned bg, PressFn cb, gpointer data);

// --- primitives ------------------------------------------------------------

// Non-interactive solid rectangle (placeholder fills, the divider bar, ...).
GtkWidget *make_fill(int w, int h, unsigned hex);

// Static text on a solid background — modal title etc.
GtkWidget *make_label(const char *text, int w, int h, double px,
                      PangoWeight weight, double halign, unsigned bg);

// --- card ------------------------------------------------------------------

// The modal's floating card: white panel, 1px black border, hard drop shadow.
GtkWidget *make_card(int w, int h);

// --- weather ---------------------------------------------------------------

// Weather panel: a bare Cairo surface for the caller to draw onto later. For
// now it renders a solid-red debug fill (no dashed border).
GtkWidget *make_weather_surface(int w, int h);

}  // namespace ui
