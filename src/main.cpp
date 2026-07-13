#include "config.hpp"
#include "log.hpp"
#include "net/led.hpp"
#include "screens/nav.hpp"
#include "screens/screens.hpp"
#include "theme.hpp"

#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>

int main(int argc, char **argv) {
  ui::log_init();
  ui::load_config_file(".env");
  ui::load_config_file(".env.config");
  ui::led_init();

  gtk_init(&argc, &argv);
  ui::prewarm_fonts(); // pay glyph-caching cost at launch, not on first switch

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // Window-manager hint for the Kindle's awesome/KUAL launcher.
  gtk_window_set_title(GTK_WINDOW(window),
                       "L:A_N:application_ID:xyz.emlyn.kindle_PC:N_O:R");
  gtk_window_set_default_size(GTK_WINDOW(window), ui::SCREEN_W, ui::SCREEN_H);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

  // Screens live in a tabless notebook; buttons flip pages to navigate.
  GtkWidget *stack = gtk_notebook_new();
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(stack), FALSE);
  gtk_notebook_set_show_border(GTK_NOTEBOOK(stack), FALSE);
  ui::set_stack(stack);

  gtk_notebook_append_page(GTK_NOTEBOOK(stack), ui::build_main_screen(),
                           nullptr);
  gtk_notebook_append_page(GTK_NOTEBOOK(stack), ui::build_led_screen(),
                           nullptr);

  gtk_container_add(GTK_CONTAINER(window), stack);

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
