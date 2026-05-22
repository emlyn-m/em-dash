#include "src/log.hpp"
#include "src/screens/screens.hpp"

#include "glib-object.h"
#include "src/storage/storage.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>

#define CELL_ROWCOUNT 5
#define CELL_COLCOUNT 5
#define PADDING 20
#define OFFSET_SIZE 5

int main(int argc, char *argv[]) {

  log_init();
  storage_init();

  GtkWidget *window;
  gtk_init(&argc, &argv);
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window),
                       "L:A_N:application_ID:xyz.emlyn.kindle_PC:N_O:R");
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  GdkColor color = {0, 255 << 8, 255 << 8, 255 << 8};
  gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &color);

  GtkWidget *stack = gtk_notebook_new();
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(stack), false);
  gtk_notebook_set_show_border(GTK_NOTEBOOK(stack), false);

  GtkWidget *screens[N_SCREENS] = {
      generate_life_screen(stack, set_screen),
      generate_ctrl_screen(stack, set_screen),
      generate_led_strip_screen(stack, set_screen),
  };
  for (int i = 0; i < N_SCREENS; i++) {
    gtk_container_add(GTK_CONTAINER(stack), screens[i]);
    gtk_widget_show(screens[i]);
  }
  gtk_container_add(GTK_CONTAINER(window), stack);

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
