#pragma once

#include <gtk-2.0/gtk/gtk.h>

namespace ui {

GtkWidget *build_main_screen();
GtkWidget *build_led_screen();

// Point the LED modal at a device (index into net/led) before navigating to it,
// so its title/controls/state reflect the device that was opened.
void led_screen_set_target(int device, const char *label);

}  // namespace ui
