#include "./widgets.hpp"
#include "cairo.h"
#include <cairo.h>
#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>
#include <math.h>
#include <stdlib.h>

/* Bayer 8x8 matrix, values 0..63 */
static const uint8_t bayer8[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21},
};

void dither_bb(cairo_t *cr, int width, int height,
               float (*level)(float x, float y)) {
  cairo_surface_t *mask =
      cairo_image_surface_create(CAIRO_FORMAT_A8, width, height);
  int stride = cairo_image_surface_get_stride(mask);
  uint8_t *data = cairo_image_surface_get_data(mask);

  for (int y = 0; y < height; y++) {
    /* lerp level across y */

    for (int x = 0; x < width; x++) {
      float ty = (float)y / (float)(height - 1);
      float tx = (float)x / (float)(width - 1);
      int threshold = (int)(((ty * 0.5) + .1) * 64);
      data[y * stride + x] = (bayer8[y & 7][x & 7] < threshold) ? 255 : 0;
    }
  }
  cairo_surface_mark_dirty(mask);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_mask_surface(cr, mask, 0, 0);

  cairo_surface_destroy(mask);
}
float lvl_const(float _tx, float _ty) { return 0.3; }
static gboolean on_expose(GtkWidget *widget, GdkEventExpose *event,
                          gpointer data) {
  KindleSlider *slider = (KindleSlider *)data;
  cairo_t *cr = gdk_cairo_create(widget->window);

  const gint border_offset_px = 10;
  int height = widget->allocation.height;
  int width = widget->allocation.width;

  if (!slider->invisible) {
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    int y1 = height - border_offset_px;
    int y0 = y1 - (slider->value * (height - 2 * border_offset_px));
    cairo_rectangle(cr, border_offset_px, y0, width - 2 * border_offset_px,
                    y1 - y0);
    cairo_clip(cr);
    dither_bb(cr, width, height, &lvl_const);
    cairo_reset_clip(cr);
  }

  cairo_destroy(cr);
  return TRUE;
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                gpointer data) {
  kindle_slider_t *slider = (kindle_slider_t *)data;
  slider->dragging = TRUE;
  return TRUE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event,
                                  gpointer data) {
  kindle_slider_t *slider = (kindle_slider_t *)data;
  slider->dragging = FALSE;
  if (slider->callback_release) {
    slider->callback_release(slider->value, slider->data);
  }
  return TRUE;
}

static gboolean on_motion(GtkWidget *widget, GdkEventMotion *event,
                          gpointer data) {
  kindle_slider_t *slider = (kindle_slider_t *)data;
  if (slider->dragging) {
    int height = widget->allocation.height - 0; // prev -200
    slider->value = 1.0 - (event->y / height);
    slider->value = CLAMP(slider->value, 0.0, 1.0);
    gtk_widget_queue_draw(widget);
    if (slider->callback_change) {
      slider->callback_change(slider->value, slider->data);
    }
  }
  return TRUE;
}

kindle_slider_t *
kindle_slider_new(void *data, void (*callback_change)(float progress, void *),
                  void (*callback_release)(float progress, void *),
                  int invisible) {
  kindle_slider_t *slider = g_new0(kindle_slider_t, 1);
  slider->value = 1.0;
  slider->dragging = FALSE;
  slider->data = data;
  slider->callback_change = callback_change;
  slider->callback_release = callback_release;
  slider->invisible = invisible;

  if (invisible) {
    slider->drawing_area = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(slider->drawing_area),
                                     FALSE);
  } else {
    slider->drawing_area = gtk_drawing_area_new();
  }

  gtk_widget_add_events(slider->drawing_area, GDK_BUTTON_PRESS_MASK |
                                                  GDK_BUTTON_RELEASE_MASK |
                                                  GDK_POINTER_MOTION_MASK);
  g_signal_connect(slider->drawing_area, "expose-event", G_CALLBACK(on_expose),
                   slider);
  g_signal_connect(slider->drawing_area, "button-press-event",
                   G_CALLBACK(on_button_press), slider);
  g_signal_connect(slider->drawing_area, "button-release-event",
                   G_CALLBACK(on_button_release), slider);
  g_signal_connect(slider->drawing_area, "motion-notify-event",
                   G_CALLBACK(on_motion), slider);

  return slider;
}
