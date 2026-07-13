#include "net/alerts.hpp"
#include "net/calendar.hpp"
#include "net/telem.hpp"
#include "screens/screens.hpp"

#include "net/weather.hpp"
#include "screens/nav.hpp"
#include "theme.hpp"
#include "widgets/clock.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

void put(GtkWidget *fixed, GtkWidget *child, int x, int y) {
  gtk_fixed_put(GTK_FIXED(fixed), child, x, y);
}

// A main-screen device button: point the modal at this device, then open it.
struct LedButton {
  int device;
  const char *label;
};
gboolean open_led(GtkWidget *, GdkEventButton *, gpointer d) {
  auto *b = static_cast<LedButton *>(d);
  led_screen_set_target(b->device, b->label);
  navigate(SCREEN_LED);
  return TRUE;
}

// Static art painted directly on the dotted field: dots, divider, and the
// clock (which needs to composite over the dots rather than sit in a box).
gboolean draw_backdrop(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = gdk_cairo_create(w->window);
  paint_dots(cr, w->allocation.width, w->allocation.height);

  set_rgb(cr, BLACK);
  cairo_rectangle(cr, 30, 259, 1380, 10);
  cairo_fill(cr);

  clock_paint(cr);

  cairo_destroy(cr);
  return TRUE;
}

GtkWidget *make_backdrop() {
  GtkWidget *a = gtk_drawing_area_new();
  gtk_widget_set_size_request(a, SCREEN_W, SCREEN_H);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_backdrop), nullptr);
  return a;
}

} // namespace

GtkWidget *build_main_screen() {
  GtkWidget *fixed = gtk_fixed_new();

  GtkWidget *backdrop = make_backdrop();
  put(fixed, backdrop, 0, 0);
  clock_start(backdrop);

  // Weather
  GtkWidget *weather = make_weather_surface(276, 591);
  put(fixed, weather, 30, 293);
  weather_start([weather] { gtk_widget_queue_draw(weather); });

  // Telemetry
  GtkWidget *telem = make_telem_surface(324, 737);
  put(fixed, telem, 336, 293);
  telem_start([telem] { gtk_widget_queue_draw(telem); });

  // Calendar
  GtkWidget *calendar = make_calendar_surface(345, 354);
  put(fixed, calendar, 1065, 293);
  calendar_start([calendar] { gtk_widget_queue_draw(calendar); });

  // Alerts
  GtkWidget *alerts = make_alerts_surface(345, 353);
  put(fixed, alerts, 690, 677);
  alerts_start([alerts] { gtk_widget_queue_draw(alerts); });

  static LedButton strip_btn{0, "led.strip0"};
  static LedButton lamp_btn{1, "led.lamp0"};
  put(fixed, make_button("led.strip0", 245, 50, 12, 0.0, open_led, &strip_btn),
      910, 129);
  put(fixed, make_button("led.lamp0", 245, 50, 12, 0.0, open_led, &lamp_btn),
      910, 189);
  put(fixed,
      make_button("ping pixel", 245, 50, 12, 0.0, noop_press,
                  (gpointer) "ping pixel"),
      1165, 129);
  put(fixed, make_button("sigterm", 245, 50, 12, 0.0, quit_press, nullptr),
      1165, 189);

  // Shell controls
  put(fixed,
      make_button("dumplogs", 236, 50, 12, 0.0, noop_press,
                  (gpointer) "dumplogs"),
      50, 915);
  put(fixed,
      make_button("revshell", 236, 50, 12, 0.0, noop_press,
                  (gpointer) "revshell"),
      50, 976);

  // Sketches

  put(fixed, make_image_surface(345, 353), 690, 294);
  put(fixed, make_image_surface(345, 354), 1065, 677);

  return fixed;
}

} // namespace ui
