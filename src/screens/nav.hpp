#pragma once

#include <gtk-2.0/gtk/gtk.h>

namespace ui {

enum Screen { SCREEN_MAIN = 0, SCREEN_LED = 1, N_SCREENS = 2 };

// Register the GtkNotebook that screen-switching buttons navigate.
void set_stack(GtkWidget *notebook);

// Switch to a screen programmatically (same effect as nav_press).
void navigate(int screen);

// Screen-switching press handler: `data` carries the target screen index
// (via GINT_TO_POINTER).
gboolean nav_press(GtkWidget *, GdkEventButton *, gpointer data);

// Wired-but-inert press handler: proves the listener path without doing work
// yet. `data` carries the button's name (const char *) for logging.
gboolean noop_press(GtkWidget *, GdkEventButton *, gpointer data);

// Quit the application (exits the GTK main loop).
gboolean quit_press(GtkWidget *, GdkEventButton *, gpointer data);

}  // namespace ui
