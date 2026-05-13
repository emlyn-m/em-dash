#include "./widgets.hpp"
#include "cairo.h"
#include <cmath>
#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>

void crosshatch_bb(cairo_t *cr, int width, int height, int spacing,
                   float (*getlevel)(float, float)) {
  cairo_surface_t *hatch =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_t *tcr = cairo_create(hatch);
  cairo_set_antialias(tcr, CAIRO_ANTIALIAS_NONE);
  cairo_set_source_rgba(tcr, 0, 0, 0, 1);
  cairo_set_line_width(tcr, 1);

  float x = 0, y;
  float level;
  while (x * spacing < width) {
    y = 0;

    while (y * spacing < height) {
      float xv = ((float)(x * spacing)) / ((float)width);
      float yv = ((float)(y * spacing)) / ((float)height);

      level = getlevel(xv, yv);
      cairo_move_to(tcr, (x + 1) * spacing, y * spacing);
      cairo_line_to(tcr, x * spacing, (y + 1) * spacing);
      cairo_stroke(tcr);
      if (level > 0.33) {
        cairo_move_to(tcr, x * spacing, y * spacing);
        cairo_line_to(tcr, (x + 1) * spacing, (y + 1) * spacing);
        cairo_stroke(tcr);
      }
      if (level > 0.67) {
        cairo_move_to(tcr, (x + .5) * spacing, (y)*spacing);
        cairo_line_to(tcr, (x + .5) * spacing, (y + 1) * spacing);
        cairo_stroke(tcr);
      }

      y += 1;
    }
    x += 1;
  }
  cairo_destroy(tcr);

  cairo_set_source_surface(cr, hatch, 0, 0);
  cairo_paint(cr);
  cairo_surface_destroy(hatch);
}

float getlevel_slider(float x, float y) { return .3; }

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
    crosshatch_bb(cr, width, height, 8, getlevel_slider);
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
