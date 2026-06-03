#include "gdk/gdk.h"
#include "src/config.hpp"
#include <cairo.h>
#include <cmath>
#include <cstdlib>
#include <gtk-2.0/gtk/gtk.h>

typedef struct point_3 {
  double x;
  double y;
  double z;
} point_3_t;

typedef struct edge {
  int v0;
  int v1;
} edge_t;

typedef struct model_info {
  point_3_t *vertices;
  point_3_t *original_vertices;
  edge_t *edges;
  int num_vertices;
  int num_edges;

  float total_theta;
  float total_phi;
} model_info_t;

void rotate_vert(point_3_t *v, float theta, float phi) {
  double x = v->x;
  double y = v->y;
  double z = v->z;

  // Rotate around Y-axis (theta)
  double cos_theta = cos(theta);
  double sin_theta = sin(theta);
  double x_rot = x * cos_theta + z * sin_theta;
  double z_rot = -x * sin_theta + z * cos_theta;

  // Rotate around X-axis (phi)
  double cos_phi = cos(phi);
  double sin_phi = sin(phi);
  double y_rot = y * cos_phi - z_rot * sin_phi;
  double z_final = y * sin_phi + z_rot * cos_phi;

  v->x = x_rot;
  v->y = y_rot;
  v->z = z_final;
}

static model_info_t *create_cat() {
  model_info_t *cat = (model_info_t *)malloc(sizeof(model_info_t));

  cat->num_vertices = 18;
  cat->vertices = (point_3_t *)malloc(sizeof(point_3_t) * cat->num_vertices);
  cat->original_vertices =
      (point_3_t *)malloc(sizeof(point_3_t) * cat->num_vertices);

  // Body (rectangular prism)
  cat->vertices[0] = (point_3_t){-1.5, -0.5, 0.5};  // body front-bottom-left
  cat->vertices[1] = (point_3_t){-1.5, -0.5, -0.5}; // body front-bottom-right
  cat->vertices[2] = (point_3_t){-1.5, 0.5, -0.5};  // body front-top-right
  cat->vertices[3] = (point_3_t){-1.5, 0.5, 0.5};   // body front-top-left
  cat->vertices[4] = (point_3_t){1.5, -0.5, 0.5};   // body back-bottom-left
  cat->vertices[5] = (point_3_t){1.5, -0.5, -0.5};  // body back-bottom-right
  cat->vertices[6] = (point_3_t){1.5, 0.5, -0.5};   // body back-top-right
  cat->vertices[7] = (point_3_t){1.5, 0.5, 0.5};    // body back-top-left

  // Head (smaller box)
  cat->vertices[8] = (point_3_t){-2.2, -0.3, 0.3};  // head front-bottom-left
  cat->vertices[9] = (point_3_t){-2.2, -0.3, -0.3}; // head front-bottom-right
  cat->vertices[10] = (point_3_t){-2.2, 0.7, -0.3}; // head front-top-right
  cat->vertices[11] = (point_3_t){-2.2, 0.7, 0.3};  // head front-top-left

  // Ears (triangular points)
  cat->vertices[12] = (point_3_t){-2.2, 1.2, 0.4};  // left ear tip
  cat->vertices[13] = (point_3_t){-2.2, 1.2, -0.4}; // right ear tip

  // Tail (curved with segments)
  cat->vertices[14] = (point_3_t){1.8, 0.2, 0.0}; // tail base
  cat->vertices[15] = (point_3_t){2.3, 0.8, 0.1}; // tail mid
  cat->vertices[16] = (point_3_t){2.5, 1.2, 0.0}; // tail tip

  // Nose
  cat->vertices[17] = (point_3_t){-2.5, 0.2, 0.0}; // nose point

  // Copy to original vertices
  for (int i = 0; i < cat->num_vertices; i++) {
    cat->original_vertices[i] = cat->vertices[i];
  }

  cat->num_edges = 30;
  cat->edges = (edge_t *)malloc(sizeof(edge_t) * cat->num_edges);

  // Body edges
  cat->edges[0] = (edge_t){0, 1};
  cat->edges[1] = (edge_t){1, 2};
  cat->edges[2] = (edge_t){2, 3};
  cat->edges[3] = (edge_t){3, 0};
  cat->edges[4] = (edge_t){4, 5};
  cat->edges[5] = (edge_t){5, 6};
  cat->edges[6] = (edge_t){6, 7};
  cat->edges[7] = (edge_t){7, 4};
  cat->edges[8] = (edge_t){0, 4};
  cat->edges[9] = (edge_t){1, 5};
  cat->edges[10] = (edge_t){2, 6};
  cat->edges[11] = (edge_t){3, 7};

  // Head edges (front face)
  cat->edges[12] = (edge_t){8, 9};
  cat->edges[13] = (edge_t){9, 10};
  cat->edges[14] = (edge_t){10, 11};
  cat->edges[15] = (edge_t){11, 8};

  // Connect head to body
  cat->edges[16] = (edge_t){0, 8};
  cat->edges[17] = (edge_t){1, 9};
  cat->edges[18] = (edge_t){2, 10};
  cat->edges[19] = (edge_t){3, 11};

  // Ears
  cat->edges[20] = (edge_t){11, 12};
  cat->edges[21] = (edge_t){10, 12};
  cat->edges[22] = (edge_t){10, 13};
  cat->edges[23] = (edge_t){9, 13};

  // Tail
  cat->edges[24] = (edge_t){7, 14};
  cat->edges[25] = (edge_t){14, 15};
  cat->edges[26] = (edge_t){15, 16};

  // Nose
  cat->edges[27] = (edge_t){8, 17};
  cat->edges[28] = (edge_t){9, 17};
  cat->edges[29] = (edge_t){11, 17};

  cat->total_theta = 0.0;
  cat->total_phi = 0.0;

  return cat;
}

gboolean on_draw(GtkWidget *widget, GdkEventExpose *event, void *model_vp) {
  model_info_t *model = (model_info_t *)model_vp;
  cairo_t *cr;

  cr = gdk_cairo_create(widget->window);

  // Get current widget size
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  float width = allocation.width;
  float height = allocation.height;

  // Calculate dynamic scale and center
  float scale = (width < height ? width : height) * 0.15;
  float center_x = width / 2.0;
  float center_y = height / 2.0;

  // Clear background
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_paint(cr);

  // Set up drawing style for wireframe
  cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
  cairo_set_line_width(cr, 1.5);

  // Reset to original vertices
  for (int i = 0; i < model->num_vertices; i++) {
    model->vertices[i] = model->original_vertices[i];
  }

  // Rotate all vertices
  for (int i = 0; i < model->num_vertices; i++) {
    rotate_vert(&(model->vertices[i]), model->total_theta, model->total_phi);
  }

  // Draw edges
  for (int i = 0; i < model->num_edges; i++) {
    point_3_t p0 = model->vertices[model->edges[i].v0];
    point_3_t p1 = model->vertices[model->edges[i].v1];

    cairo_move_to(cr, center_x + p0.x * scale, center_y - p0.y * scale);
    cairo_line_to(cr, center_x + p1.x * scale, center_y - p1.y * scale);
  }

  cairo_stroke(cr);
  cairo_destroy(cr);
  return FALSE;
}

static gboolean on_timeout(gpointer data) {
  GtkWidget *widget = GTK_WIDGET(data);
  model_info_t *model =
      (model_info_t *)g_object_get_data(G_OBJECT(widget), "model");

  // Increment rotation
  model->total_theta += 0.02;
  model->total_phi += 0.01;

  gtk_widget_queue_draw(widget);
  return TRUE;
}

GtkWidget *model_init() {
  model_info_t *cat = create_cat();

  GtkWidget *draw_obj = gtk_drawing_area_new();
  g_object_set_data(G_OBJECT(draw_obj), "model", cat);
  g_signal_connect(draw_obj, "expose-event", G_CALLBACK(on_draw), cat);
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"), on_timeout, draw_obj);

  return draw_obj;
}
