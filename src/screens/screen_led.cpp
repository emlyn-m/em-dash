#include "screens/screens.hpp"

#include "net/led.hpp"
#include "screens/nav.hpp"
#include "theme.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

void put(GtkWidget *fixed, GtkWidget *child, int x, int y) {
  gtk_fixed_put(GTK_FIXED(fixed), child, x, y);
}

// conn: query the strip's live state off-thread (proves device access).
gboolean conn_press(GtkWidget *, GdkEventButton *, gpointer) {
  led_refresh(0, nullptr);
  return TRUE;
}

}  // namespace

GtkWidget *build_led_screen() {
  GtkWidget *fixed = gtk_fixed_new();

  put(fixed, make_dotted_background(), 0, 0);

  // Floating card.
  put(fixed, make_card(1240, 880), 100, 100);

  // Title on the white card.
  put(fixed,
      make_label("led.strip0", 300, 65, 48, PANGO_WEIGHT_BOLD, 0.0, WHITE), 248,
      155);

  // conn: query the device.  exit btn: returns to the main screen.
  put(fixed,
      make_box_button("conn", 90, 66, 16, GREY, conn_press, nullptr), 140, 154);
  put(fixed,
      make_box_button("exit\nbtn", 90, 90, 16, GREY, nav_press,
                      GINT_TO_POINTER(SCREEN_MAIN)),
      1202, 154);

  // Three slider tracks + preview image, left as grey placeholders.
  put(fixed, make_fill(127, 671, GREY), 150, 261);
  put(fixed, make_fill(127, 671, GREY), 309, 261);
  put(fixed, make_fill(127, 671, GREY), 468, 261);
  put(fixed, make_fill(522, 575, GREY), 770, 357);

  return fixed;
}

}  // namespace ui
