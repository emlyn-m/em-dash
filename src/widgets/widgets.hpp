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

// As paint_dots, but phase-aligned to the global grid for a widget whose
// top-left sits at (origin_x, origin_y) on screen — so an overlay's dots line
// up seamlessly with the backdrop's.
void paint_dots_at(cairo_t *cr, int w, int h, int origin_x, int origin_y);

// Full-screen dotted grid.
GtkWidget *make_dotted_background();

// --- buttons ---------------------------------------------------------------

// Black button
// `halign`: 0 = left-padded, 0.5 = centred.
GtkWidget *make_button(const char *label, int w, int h, double px,
                       double halign, PressFn cb, gpointer data);

// Text button
GtkWidget *make_text_button(const char *label, int w, int h, double px,
                            unsigned hex, PressFn cb, gpointer data);

// --- primitives ------------------------------------------------------------

// Non-interactive solid rectangle (placeholder fills, the divider bar, ...).
GtkWidget *make_fill(int w, int h, unsigned hex);

// Static text on a solid background — modal title etc.
GtkWidget *make_label(const char *text, int w, int h, double px,
                      PangoWeight weight, double halign, unsigned bg);

// --- weather ---------------------------------------------------------------
GtkWidget *make_weather_surface(int w, int h);

// --- telem -----------------------------------------------------------------
GtkWidget *make_telem_surface(int w, int h);

// --- alerts ----------------------------------------------------------------
GtkWidget *make_alerts_surface(int w, int h);

// --- calendar---------------------------------------------------------------
GtkWidget *make_calendar_surface(int w, int h);

// --- image -----------------------------------------------------------------
GtkWidget *make_image_surface(int w, int h);

} // namespace ui
