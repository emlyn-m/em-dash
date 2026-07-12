#include "screens/nav.hpp"

namespace ui {

namespace {
GtkWidget *g_stack = nullptr;
}

void set_stack(GtkWidget *notebook) { g_stack = notebook; }

void navigate(int screen) {
  if (g_stack) gtk_notebook_set_current_page(GTK_NOTEBOOK(g_stack), screen);
}

gboolean nav_press(GtkWidget *, GdkEventButton *, gpointer data) {
  navigate(GPOINTER_TO_INT(data));
  return TRUE;
}

gboolean noop_press(GtkWidget *, GdkEventButton *, gpointer data) {
  g_print("[button] %s pressed\n", static_cast<const char *>(data));
  return TRUE;
}

gboolean quit_press(GtkWidget *, GdkEventButton *, gpointer) {
  gtk_main_quit();
  return TRUE;
}

}  // namespace ui
