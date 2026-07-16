#include "cairo.h"
#include "pango/pango-font.h"
#include "screens/screens.hpp"
#include <glib.h>

#include "net/led.hpp"
#include "screens/nav.hpp"
#include "theme.hpp"
#include "widgets/common.hpp"
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
};

// The single LED modal, retargeted per open (see led_screen_set_target).
LedControl *g_led = nullptr;

void on_hue(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->hue = (int)(v * 359);
}
void on_sat(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->sat = (int)(v * 1000);
}
void on_val(float v, void *d) {
  auto *c = static_cast<LedControl *>(d);
  c->val = (int)(v * 1000);
}

// On release, send the final value. No explicit re-query — the comm thread
// keeps state live from device status pushes.
void finalize(LedControl *c) { led_set_hsv(c->device, c->hue, c->sat, c->val); }
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

gboolean draw_title(GtkWidget *w, GdkEventExpose *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  cairo_t *cr = gdk_cairo_create(w->window);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);
  draw_text(cr, 0, 0, w->allocation.width, w->allocation.height, BLACK,
            c->label, 50, PANGO_WEIGHT_BOLD, 0.0, 0.0);
  cairo_destroy(cr);
  return TRUE;
}

// conn box: connection indicator (grey/"on"/"off" when reachable, inverted
// "econn" when not) that doubles as a power toggle.
gboolean draw_conn(GtkWidget *w, GdkEventExpose *, gpointer d) {
  cairo_t *cr = gdk_cairo_create(w->window);

  auto *c = static_cast<LedControl *>(d);
  const LedState &s = led_state(c->device);

  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  uint hex;
  if (s.online) {
    hex = (s.power ? BLACK : MUTED);
    draw_text(cr, 0, 0, w->allocation.width, 26, hex,
              s.power ? "disable" : "enable", 20, PANGO_WEIGHT_BOLD, 1.0);
  } else {
    hex = BLACK;
    set_rgb(cr, BLACK);
    cairo_rectangle(cr, 101, 0, 79, 26);
    cairo_fill(cr);

    draw_text_tl(cr, 103, 0, WHITE, "!ECONN", 20, PANGO_WEIGHT_BOLD);
  }

  char ipbuf[32];
  snprintf(ipbuf, 32, "ip TODO");
  draw_text(cr, 0, 35, w->allocation.width, 16, hex, ipbuf, 12,
            PANGO_WEIGHT_MEDIUM, 1.0, 0.5);

  char idbuf[32];
  snprintf(idbuf, 32, "id TODO");
  draw_text(cr, 0, 51, w->allocation.width, 16, hex, idbuf, 12,
            PANGO_WEIGHT_MEDIUM, 1.0, 0.5);

  char verbuf[32];
  snprintf(verbuf, 32, "tuya ver TODO");
  draw_text(cr, 0, 67, w->allocation.width, 16, hex, verbuf, 12,
            PANGO_WEIGHT_MEDIUM, 1.0, 0.5);

  cairo_destroy(cr);
  return TRUE;
}

gboolean conn_press(GtkWidget *, GdkEventButton *e, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  const LedState &s = led_state(c->device);

  if ((!s.online) || (e->x < 101 || e->y > 26)) {
    return TRUE;
  }

  led_set_power(c->device, !led_state(c->device).power);
  return TRUE;
}

// Sync the sliders + conn from device state. slider_sync no-ops while dragging,
// so the knob isn't disturbed mid-drag.
void led_update_cb(LedControl *c) {
  gtk_widget_queue_draw(c->conn);
  const LedState &s = led_state(c->device);
  slider_sync(c->hue_slider, s.hue / 359.0f);
  slider_sync(c->sat_slider, s.sat / 1000.0f);
  slider_sync(c->val_slider, s.val / 1000.0f);
  c->hue = (int)(slider_value(c->hue_slider) * 359);
  c->sat = (int)(slider_value(c->sat_slider) * 1000);
  c->val = (int)(slider_value(c->val_slider) * 1000);
}

// Modal opened: activate the device's comm thread, subscribe for updates, and
// pull fresh state.
void on_modal_shown(GtkWidget *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  led_on_update(c->device, [c] { led_update_cb(c); });
  led_set_active(c->device, true);
  led_query(c->device);
}

// Modal closed: stop the device from connecting while it isn't being viewed.
void on_modal_hidden(GtkWidget *, gpointer d) {
  auto *c = static_cast<LedControl *>(d);
  led_set_active(c->device, false);
  led_on_update(c->device, nullptr);
}

// Static art painted directly on the dotted field: dots, divider, and the
// clock (which needs to composite over the dots rather than sit in a box).
gboolean draw_backdrop(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = gdk_cairo_create(w->window);
  paint_dots(cr, w->allocation.width, w->allocation.height);

  set_rgb(cr, BLACK);
  cairo_rectangle(cr, 30, 259, 1380, 10);
  cairo_fill(cr);

  cairo_destroy(cr);
  return TRUE;
}

GtkWidget *make_backdrop() {
  GtkWidget *a = gtk_drawing_area_new();
  gtk_widget_set_size_request(a, SCREEN_W, SCREEN_H);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_backdrop), nullptr);
  return a;
}

gboolean draw_slider_info(GtkWidget *widget, GdkEventExpose *, gpointer d) {
  cairo_t *cr = detail::begin_paint(widget);
  paint_dots_at(cr, widget->allocation.width, widget->allocation.height,
                widget->allocation.x, widget->allocation.y);

  draw_text(cr, 0, 0, 60, 16, BLACK, "hue", 12, PANGO_WEIGHT_BOLD, 0.5);
  draw_text(cr, 80, 0, 60, 16, BLACK, "sat", 12, PANGO_WEIGHT_BOLD, 0.5);
  draw_text(cr, 160, 0, 60, 16, BLACK, "val", 12, PANGO_WEIGHT_BOLD, 0.5);

  set_rgb(cr, MUTED);
  cairo_rectangle(cr, 0, 18, 60, 60);
  cairo_rectangle(cr, 80, 18, 60, 60);
  cairo_rectangle(cr, 160, 18, 60, 60);
  cairo_fill(cr);

  cairo_destroy(cr);
  return TRUE;
}

GtkWidget *make_slider_info() {
  GtkWidget *a = detail::new_area(220, 78);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_slider_info), NULL);
  return a;
}

} // namespace

GtkWidget *build_led_screen() {
  GtkWidget *fixed = gtk_fixed_new();

  put(fixed, make_backdrop(), 0, 0);

  LedControl *c = new LedControl();
  g_led = c;

  // exit btn (top-left): back to the main screen.
  put(fixed,
      make_text_button("🭮🭪🭮🭪🭮🭪", 144, 31, 24, MUTED, nav_press,
                       GINT_TO_POINTER(SCREEN_MAIN)),
      30, 30);

  // Title reflects the active device's label.
  GtkWidget *title = gtk_drawing_area_new();
  gtk_widget_set_size_request(title, 800, 65);
  g_signal_connect(title, "expose-event", G_CALLBACK(draw_title), c);
  c->title = title;
  put(fixed, title, 164, 164);

  // conn: indicator + power toggle (top-left).
  GtkWidget *conn = gtk_drawing_area_new();
  gtk_widget_set_size_request(conn, 180, 83);
  gtk_widget_add_events(conn, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(conn, "expose-event", G_CALLBACK(draw_conn), c);
  g_signal_connect(conn, "button-press-event", G_CALLBACK(conn_press), c);
  c->conn = conn;
  put(fixed, conn, 1230, 162);

  // HSV sliders.
  c->hue_slider = make_slider(60, 611, c, on_hue, on_hue_end);
  c->sat_slider = make_slider(60, 611, c, on_sat, on_sat_end);
  c->val_slider = make_slider(60, 611, c, on_val, on_val_end);
  put(fixed, c->hue_slider, 60, 329);
  put(fixed, c->sat_slider, 140, 329);
  put(fixed, c->val_slider, 220, 329);

  put(fixed, make_slider_info(), 60, 942);

  // Preview image (still a placeholder).
  put(fixed, make_fill(200, 200, GREY), 1180, 820);

  g_signal_connect(fixed, "map", G_CALLBACK(on_modal_shown), c);
  g_signal_connect(fixed, "unmap", G_CALLBACK(on_modal_hidden), c);

  return fixed;
}

void led_screen_set_target(int device, const char *label) {
  if (!g_led)
    return;
  g_led->device = device;
  g_led->label = label;
  // Reflect the new device immediately; the map handler pulls its live state.
  gtk_widget_queue_draw(g_led->title);
  gtk_widget_queue_draw(g_led->conn);
}

} // namespace ui
