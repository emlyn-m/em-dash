#include "cairo.h"
#include "gdk/gdk.h"
#include "gtk/gtk.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <glib.h>

#include "../log.hpp"
#include "./widgets.hpp"
#include "bezier.h"
#include "pango/pango-font.h"

// #define LED_CURVE(i) MAX(1 - (((i - 5.5) * (i - 5.5))/30.25), 0.1)
#define LED_CURVE(i) 1
#define ledg_padw 30
#define ledg_padh 30
#define ledg_gaph 1
#define line_w 4
#define led_r 10
#define led_w 15

#define lg_t 100
#define lg_b 100

#define PADDING 20
#define OFFSET_SIZE 5

void process_graph_update(led_graph_data_t *gdata);
void button_callback_a(GtkButton *_b, GdkEvent *_e, void *gdata_vp) {
  for (int i = 0; i < 12; i++) {
    ((led_graph_data_t *)gdata_vp)->points[i].v =
        &(((led_graph_data_t *)gdata_vp)->points[i]._v_h);
    ((led_graph_data_t *)gdata_vp)->points[i].sliderref->value =
        *((led_graph_data_t *)gdata_vp)->points[i].v;
  }
  gtk_widget_queue_draw(((led_graph_data_t *)gdata_vp)->drawref);
}
void button_callback_r(GtkButton *_b, GdkEvent *_e, void *gdata_vp) {
  for (int i = 0; i < 12; i++) {
    ((led_graph_data_t *)gdata_vp)->points[i].v =
        &(((led_graph_data_t *)gdata_vp)->points[i]._v_s);
    ((led_graph_data_t *)gdata_vp)->points[i].sliderref->value =
        *((led_graph_data_t *)gdata_vp)->points[i].v;
  }
  gtk_widget_queue_draw(((led_graph_data_t *)gdata_vp)->drawref);
}
void button_callback_g(GtkButton *_b, GdkEvent *_e, void *gdata_vp) {
  for (int i = 0; i < 12; i++) {
    ((led_graph_data_t *)gdata_vp)->points[i].v =
        &(((led_graph_data_t *)gdata_vp)->points[i]._v_v);
    ((led_graph_data_t *)gdata_vp)->points[i].sliderref->value =
        *((led_graph_data_t *)gdata_vp)->points[i].v;
  }
  gtk_widget_queue_draw(((led_graph_data_t *)gdata_vp)->drawref);
}

float levels(float tx, float ty, void *_data) {
  bezier_dist_t *data = (bezier_dist_t *)_data;
  int _out_seg;
  double _dist;
  double dist = bezier_path_distance(data->path, tx * data->width,
                                     ty * data->height, &_dist, &_out_seg);

  const double falloff_dist = 500.0;
  double t = fmin(dist / falloff_dist, 0.9);
  double shade = pow(t, 1.2);

  return shade;
}
void draw_lines(cairo_t *cr, led_graph_data_t *gdata, int w, int h) {
  int by_min = INT32_MAX;
  cairo_set_line_width(cr, line_w);
  cairo_set_source_rgb(cr, 0, 0, 0);

  for (int j = 0; j < 12; j++) {
    int x1 = (int)(((float)(j + 0.5) / 12.0) * (w));
    int y1 = h -
             ((int)((*gdata->points[j].v - ((int)*gdata->points[j].v)) *
                    (float)(h - (lg_t + lg_b)))) -
             lg_b;
    if (j == 0) {
      cairo_move_to(cr, x1, y1);
    }
    if (j < 11) {
      int x0 = (int)(((float)(j - 0.5) / 12.0) * (w));
      int y0 = h -
               ((int)((*gdata->points[j > 0 ? j - 1 : 0].v -
                       ((int)*gdata->points[j > 0 ? j - 1 : 0].v)) *
                      (float)(h - (lg_t + lg_b)))) -
               lg_b;

      int x2 = (int)(((float)(j + 1.5) / 12.0) * (w));
      int y2 =
          h -
          ((int)((*gdata->points[j + 1].v - ((int)*gdata->points[j + 1].v)) *
                 (float)(h - (lg_t + lg_b)))) -
          lg_b;

      int x3 = (int)(((float)(j + 2.5) / 12.0) * (w));
      int y3 = h -
               ((int)((*gdata->points[j < 10 ? j + 2 : 0].v -
                       ((int)*gdata->points[j < 10 ? j + 2 : 0].v)) *
                      (float)(h - (lg_t + lg_b)))) -
               lg_b;

      int xc1 = x1 + (x2 - x0) / 6;
      int yc1 = y1 + (y2 - y0) / 6;
      int xc2 = x2 - (x3 - x1) / 6;
      int yc2 = y2 - (y3 - y1) / 6;

      cairo_curve_to(cr, xc1, yc1, xc2, yc2, x2, y2);

      float cby_min, by_max;
      bezier_extrema_axis((float)y1, (float)yc1, (float)yc2, (float)y2,
                          &cby_min, &by_max);
      if (cby_min < by_min) {
        by_min = cby_min;
      }
    }
  }
  cairo_stroke_preserve(cr);

  cairo_line_to(cr, (11.5 / 12.0) * w, h);
  cairo_line_to(cr, (0.5 / 12.0) * w, h);
  cairo_close_path(cr);
  cairo_clip_preserve(cr);

  bezier_dist_t *bdist = (bezier_dist_t *)malloc(sizeof(bezier_dist_t));
  bdist->width = w;
  bdist->height = h;
  bdist->path = cairo_copy_path(cr);

  cairo_set_line_width(cr, 1);
  cairo_set_source_rgba(cr, (2. - ((float)0)) / 5., (2. - ((float)0)) / 5.,
                        (2. - ((float)0)) / 5., 1);
  dither_bb(cr, w, h, &levels, 1., (void *)bdist);
  cairo_path_destroy(bdist->path);

  cairo_restore(cr);
}

gboolean graph_ondraw(GtkWidget *widget, GdkEventExpose *_e, gpointer data) {
  led_graph_data_t *gdata = (led_graph_data_t *)data;
  cairo_t *cr = gdk_cairo_create(widget->window);
  int w = (int)ww(gdata->drawref);
  int h = (int)wh(gdata->drawref);
  cairo_save(cr);
  cairo_set_source_rgba(cr, 1, 1, 1, 1);
  cairo_paint(cr);
  cairo_restore(cr);

  draw_lines(cr, gdata, w, h);
  return FALSE;
}

void process_graph_update(led_graph_data_t *gdata) {
  int ox = gdata->ref->allocation.x;
  int oy = gdata->ref->allocation.y;
  int w = (int)refw(gdata);
  int h = (int)refh(gdata);

#define LABEL_PAD 20
  GtkRequisition req_a, req_r, req_g;
  gtk_widget_size_request(gdata->label_a, &req_a);
  req_a.width += LABEL_PAD;
  req_a.height += LABEL_PAD;
  gtk_widget_size_request(gdata->label_r, &req_r);
  gtk_widget_size_request(gdata->label_g, &req_g);

  GtkAllocation a;
  GtkRequisition lreq;
  a = {0, 0, 0, 0};

  for (int i = 0; i < 12; i++) {
    gtk_widget_size_request(gdata->points[i].ref, &lreq);
    int lx = (int)(((float)(i + 0.5) / 12.0) * (w - 2 * ledg_padw)) + ledg_padw;
    a = {lx, h - ledg_padh / 2 - lreq.height / 2, lreq.width, lreq.height};
    gtk_widget_size_allocate(gdata->points[i].ref, &a);

    a = {(int)(((float)i / 13.0) * w) + ledg_padw + 2, ledg_padh + lg_t,
         (w - 2 * ledg_padw) / 12 - 4,
         h - ((int)1.5 * ledg_padh + lreq.height + lg_b + lg_t)};
    gtk_widget_size_allocate(gdata->points[i].sliderref->drawing_area, &a);
  }

  a = {ledg_padw, ledg_padh, w - 2 * ledg_padw,
       h - ((int)1.5 * ledg_padh + lreq.height)};
  gtk_widget_size_allocate(gdata->drawref, &a);
  a = {ox + w - req_a.width - ledg_padw, oy + ledg_padh, req_a.width,
       req_a.height};
  gtk_widget_size_allocate(gdata->label_a, &a);
  a = {ox + w - req_a.width - ledg_padw,
       oy + ledg_padh + req_a.height + ledg_gaph, req_a.width, req_a.height};
  gtk_widget_size_allocate(gdata->label_r, &a);
  a = {ox + w - req_a.width - ledg_padw,
       oy + ledg_padh + req_a.height + ledg_gaph + req_a.height + ledg_gaph,
       req_a.width, req_a.height};
  gtk_widget_size_allocate(gdata->label_g, &a);
}

void graph_resize(GtkWidget *widget, GdkRectangle *alloc, gpointer gdata_vp) {
  led_graph_data_t *gdata = (led_graph_data_t *)gdata_vp;
  if (gdata->ref->parent) {
    process_graph_update(gdata);
  }
}

static gboolean _shadow_expose(GtkWidget *widget, GdkEventExpose *event,
                               gpointer _data) {
  cairo_t *cr = gdk_cairo_create(widget->window);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_rectangle(cr, OFFSET_SIZE / 2.0, OFFSET_SIZE * 1.5,
                  event->area.width - 2 * OFFSET_SIZE,
                  event->area.height - 2 * OFFSET_SIZE);
  cairo_fill(cr);
  cairo_destroy(cr);

  return TRUE;
}
GtkWidget *table_wrapping(GtkWidget *c) {
  GdkColor black = {0, 0, 0, 0};
  GdkColor white = {0, 255 << 8, 255 << 8, 255 << 8};

  GtkWidget *event_box_inner = gtk_event_box_new();
  GtkWidget *event_box_outer = gtk_event_box_new();
  GtkWidget *event_box_shadow = gtk_event_box_new();

  gtk_container_add(GTK_CONTAINER(event_box_inner), c);
  gtk_container_add(GTK_CONTAINER(event_box_outer), event_box_inner);
  gtk_container_add(GTK_CONTAINER(event_box_shadow), event_box_outer);

  gtk_widget_modify_bg(event_box_inner, GTK_STATE_NORMAL, &white);
  gtk_widget_modify_bg(event_box_outer, GTK_STATE_NORMAL, &black);
  gtk_widget_modify_bg(event_box_shadow, GTK_STATE_NORMAL, &white);

  gtk_container_set_border_width(GTK_CONTAINER(event_box_inner), 2 * SCALE);
  gtk_container_set_border_width(GTK_CONTAINER(event_box_outer), OFFSET_SIZE);
  gtk_container_set_border_width(GTK_CONTAINER(event_box_shadow), 0);
  g_signal_connect(event_box_shadow, "expose-event", G_CALLBACK(_shadow_expose),
                   NULL);

  return event_box_shadow;
}

void slider_update(float v, void *data_vp) {
  graph_point_t *data = (graph_point_t *)data_vp;
  if (data->v != &(data->_v_tmp)) {
    data->_v_tmp = data->v - &data->_v_h;
    data->v = &data->_v_tmp;
  }
  *(data->v) = ((int)*data->v) + MIN(MAX(0, v), 0.9999f);
  gtk_widget_queue_draw(data->drawref);
}

void slider_release(float v, void *data_vp) {
  graph_point_t *data = (graph_point_t *)data_vp;
  if (data->v == &(data->_v_tmp)) {
    data->v = &(data->_v_h) + (int)data->_v_tmp;
  }
  *(data->v) = MIN(MAX(0, v), 0.9999f);
}

GtkWidget *led_graph() {
  led_graph_data_t *gdata =
      (led_graph_data_t *)malloc(sizeof(led_graph_data_t));

  gdata->ref = gtk_fixed_new();
  gdata->drawref = gtk_drawing_area_new();
  gtk_fixed_put(GTK_FIXED(gdata->ref), gdata->drawref, 0, 0);
  g_signal_connect(gdata->drawref, "expose-event", G_CALLBACK(graph_ondraw),
                   gdata);

  g_signal_connect(gdata->ref, "size-allocate", G_CALLBACK(graph_resize),
                   gdata);
  PangoFontDescription *font_desc = pango_font_description_from_string(FONT_8);

  time_t start = time(NULL) % 3600;
  gdata->points = (graph_point_t *)malloc(12 * sizeof(graph_point_t));

  for (int i = 0; i < 12; i++) {
    gdata->points[i].drawref = gdata->drawref;

    gdata->points[i]._v_h = LED_CURVE(i);
    gdata->points[i]._v_s = LED_CURVE(i);
    gdata->points[i]._v_v = LED_CURVE(i);
    gdata->points[i].v = &(gdata->points[i]._v_v);

    gdata->points[i].t = start + 3600 * i;
    char labelbuf[8];
    snprintf(labelbuf, 8, "%02ld:00", ((gdata->points[i].t) % 86400) / 3600);
    gdata->points[i].ref = label_widget(labelbuf);
    gtk_widget_modify_font(gdata->points[i].ref, font_desc);
    gtk_fixed_put(GTK_FIXED(gdata->ref), gdata->points[i].ref, 0, 0);

    gdata->points[i].sliderref = kindle_slider_new(
        &(gdata->points[i]), slider_update, slider_release, 1);
    gdata->points[i].sliderref->value = *gdata->points[i].v;

    gtk_fixed_put(GTK_FIXED(gdata->ref),
                  gdata->points[i].sliderref->drawing_area, 0, 1);
  }

  pango_font_description_free(font_desc);
  font_desc = pango_font_description_from_string(FONT_12);
  gdata->label_a =
      table_wrapping(button_widget((char *)"value", button_callback_a, gdata));
  gtk_widget_modify_font(gdata->label_a, font_desc);
  gtk_fixed_put(GTK_FIXED(gdata->ref), gdata->label_a, refw(gdata),
                refh(gdata));
  gdata->label_r =
      table_wrapping(button_widget((char *)"hue", button_callback_r, gdata));
  gtk_widget_modify_font(gdata->label_r, font_desc);
  gtk_fixed_put(GTK_FIXED(gdata->ref), gdata->label_r, refw(gdata),
                refh(gdata));
  gdata->label_g =
      table_wrapping(button_widget((char *)"sat", button_callback_g, gdata));
  gtk_widget_modify_font(gdata->label_g, font_desc);
  gtk_fixed_put(GTK_FIXED(gdata->ref), gdata->label_g, refw(gdata),
                refh(gdata));
  pango_font_description_free(font_desc);

  return gdata->ref;
}
