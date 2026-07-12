#include "screens/screens.hpp"

#include "net/led.hpp"
#include "screens/nav.hpp"
#include "theme.hpp"
#include "widgets/slider.hpp"
#include "widgets/widgets.hpp"

#include <functional>

namespace ui {

namespace {

void put(GtkWidget *fixed, GtkWidget *child, int x, int y) {
  gtk_fixed_put(GTK_FIXED(fixed), child, x, y);
}

// Per-screen control state. Lives for the program (built once).
struct LedControl {
  int device = 0;
  const char *label = "led.strip0";
  int hue = 0, sat = 0, val = 0;
  GtkWidget *title = nullptr;
  GtkWidget *hue_slider = nullptr;
  GtkWidget *sat_slider = nullptr;
  GtkWidget *val_slider = nullptr;
  GtkWidget *conn = nullptr;
  gint64 last_send = 0;
  std::function<void()> sync;  // refresh UI from device state (drag-safe)
};

// The single LED modal, retargeted per open (see led_screen_set_target).
LedControl *g_led = nullptr;

// Throttled live send while dragging — bounds device traffic / worker spawns.
void push_hsv(LedControl *c) {
  gint64 now = g_get_monotonic_time();
  if (now - c->last_send < 80000) return;  // ~12 updates/sec
  c->last_send = now;
  led_set_hsv(c->device, c->hue, c->sat, c->val);
}

void on_hue(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->hue = (int)(v * 359);
  push_hsv(c);
}
void on_sat(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->sat = (int)(v * 1000);
  push_hsv(c);
}
void on_val(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->val = (int)(v * 1000);
  push_hsv(c);
}

// On release the drag is over, so it's safe to send the final value and pull
// the device's confirmed state back into the sliders.
void finalize(LedControl *c) {
  led_set_hsv(c->device, c->hue, c->sat, c->val);
  led_refresh(c->device, c->sync);
}
void on_hue_end(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->hue = (int)(v * 359);
  finalize(c);
}
void on_sat_end(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->sat = (int)(v * 1000);
  finalize(c);
}
void on_val_end(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->val = (int)(v * 1000);
  finalize(c);
}

// Title: the active device's label, drawn on the white card.
gboolean draw_title(GtkWidget *w, GdkEventExpose *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  cairo_t *cr = gdk_cairo_create(w->window);
  set_rgb(cr, WHITE);
  cairo_paint(cr);
  draw_text(cr, 0, 0, w->allocation.width, w->allocation.height, BLACK,
            c->label, 48, PANGO_WEIGHT_BOLD, 0.0, 0.5);
  cairo_destroy(cr);
  return TRUE;
}

// conn box: connection indicator (grey/"on"/"off" when reachable, inverted
// "econn" when not) that doubles as a power toggle.
gboolean draw_conn(GtkWidget *w, GdkEventExpose *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  const LedState &s = led_state(c->device);
  cairo_t *cr = gdk_cairo_create(w->window);
  set_rgb(cr, s.online ? GREY : BLACK);
  cairo_paint(cr);
  const char *label = !s.online ? "econn" : (s.power ? "on" : "off");
  draw_text(cr, 0, 0, w->allocation.width, w->allocation.height,
            s.online ? BLACK : WHITE, label, 16, PANGO_WEIGHT_MEDIUM, 0.5, 0.5);
  cairo_destroy(cr);
  return TRUE;
}

gboolean conn_press(GtkWidget *, GdkEventButton *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  led_set_power(c->device, !led_state(c->device).power);
  led_refresh(c->device, c->sync);  // reflect connection + confirmed power
  return TRUE;
}

// Fires each time the modal becomes visible (notebook page mapped): pull fresh
// device state so the sliders/conn reflect the device whenever it's opened.
void on_modal_shown(GtkWidget *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  led_refresh(c->device, c->sync);
}

}  // namespace

GtkWidget *build_led_screen() {
  GtkWidget *fixed = gtk_fixed_new();

  put(fixed, make_dotted_background(), 0, 0);
  put(fixed, make_card(1240, 880), 100, 100);

  LedControl *c = new LedControl();
  g_led = c;

  // Title reflects the active device's label.
  GtkWidget *title = gtk_drawing_area_new();
  gtk_widget_set_size_request(title, 300, 65);
  g_signal_connect(title, "expose-event", G_CALLBACK(draw_title), c);
  c->title = title;
  put(fixed, title, 248, 155);

  // conn: indicator + power toggle (top-left).
  GtkWidget *conn = gtk_drawing_area_new();
  gtk_widget_set_size_request(conn, 90, 66);
  gtk_widget_add_events(conn, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(conn, "expose-event", G_CALLBACK(draw_conn), c);
  g_signal_connect(conn, "button-press-event", G_CALLBACK(conn_press), c);
  c->conn = conn;
  put(fixed, conn, 140, 154);

  // exit btn (top-right): back to the main screen.
  put(fixed,
      make_box_button("exit\nbtn", 90, 90, 16, GREY, nav_press,
                      GINT_TO_POINTER(SCREEN_MAIN)),
      1202, 154);

  // HSV sliders.
  c->hue_slider = make_slider(127, 671, c, on_hue, on_hue_end);
  c->sat_slider = make_slider(127, 671, c, on_sat, on_sat_end);
  c->val_slider = make_slider(127, 671, c, on_val, on_val_end);
  put(fixed, c->hue_slider, 150, 261);
  put(fixed, c->sat_slider, 309, 261);
  put(fixed, c->val_slider, 468, 261);

  // Preview image (still a placeholder).
  put(fixed, make_fill(522, 575, GREY), 770, 357);

  // Sync the sliders + conn from device state. slider_sync is a no-op while
  // dragging, so a confirmation arriving mid-drag never snaps the knob.
  c->sync = [c]() {
    const LedState &s = led_state(c->device);
    slider_sync(c->hue_slider, s.hue / 359.0f);
    slider_sync(c->sat_slider, s.sat / 1000.0f);
    slider_sync(c->val_slider, s.val / 1000.0f);
    c->hue = (int)(slider_value(c->hue_slider) * 359);
    c->sat = (int)(slider_value(c->sat_slider) * 1000);
    c->val = (int)(slider_value(c->val_slider) * 1000);
    gtk_widget_queue_draw(c->conn);
  };

  g_signal_connect(fixed, "map", G_CALLBACK(on_modal_shown), c);

  return fixed;
}

void led_screen_set_target(int device, const char *label) {
  if (!g_led) return;
  g_led->device = device;
  g_led->label = label;
  // Reflect the new device immediately; the map handler pulls its live state.
  gtk_widget_queue_draw(g_led->title);
  gtk_widget_queue_draw(g_led->conn);
}

}  // namespace ui
