#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>

int main(int argc, char **argv) {
  gtk_init(&argc, &argv);

  GtkWidget *window;
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window),
                       "L:A_N:application_ID:xyz.emlyn.kindle_PC:N_O:R");
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  GdkColor color = {0, 255 << 8, 255 << 8, 255 << 8};
  gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &color);

  gtk_widget_show_all(window);
  gtk_main();
}
