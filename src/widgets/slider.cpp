#include "./widgets.hpp"
#include "cairo.h"
#include "glibconfig.h"
#include <cairo.h>
#include <cmath>
#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>
#include <math.h>
#include <stdlib.h>

void dither_bb(cairo_t *cr, int width, int height,
               float (*level)(float x, float y, void *data), float alpha,
               void *data) {
  const int scale = 20;
  cairo_surface_t *mask = cairo_image_surface_create(
      CAIRO_FORMAT_A8, width / scale, height / scale);
  int stride = cairo_image_surface_get_stride(mask);
  uint8_t *idata = cairo_image_surface_get_data(mask);

  for (int y = 0; y < height / scale; y++) {
    float ty = (float)y / (float)(((float)height / scale) - 1);
    for (int x = 0; x < width / scale; x++) {
      float tx = (float)x / (float)(((float)width / scale) - 1);
      float new_level = 255 * level(tx, ty, data);
#ifndef N_QUANTIZE_LEVELS
#define N_QUANTIZE_LEVELS 10.
#endif

      float newpx = (255. / N_QUANTIZE_LEVELS) *
                    round((new_level / 255.) * (N_QUANTIZE_LEVELS));
      idata[y * stride + x] = newpx;
    }
  }
  cairo_surface_mark_dirty(mask);

  cairo_pattern_t *pat = cairo_pattern_create_for_surface(mask);
  cairo_pattern_set_filter(pat, CAIRO_FILTER_NEAREST);
  cairo_matrix_t m;
  cairo_matrix_init_scale(&m, 1.0 / ((float)scale), 1.0 / ((float)scale));
  cairo_pattern_set_matrix(pat, &m);

  cairo_set_source_rgba(cr, 0, 0, 0, alpha);
  cairo_mask(cr, pat);

  cairo_pattern_destroy(pat);
  cairo_surface_destroy(mask);
}
float lvl_v_noise(float tx, float ty, void *_data) {
  float natural = (ty * .9) + .1;
  double _void;
  float rseed = modf(sin(tx * 12.9898 + ty * 78.233) * 43758.5453, &_void);
  float rand = rseed / 100.;
  return CLAMP(natural + rand, 0., 1.);
}
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
    dither_bb(cr, width, height, &lvl_v_noise, 1, NULL);
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
