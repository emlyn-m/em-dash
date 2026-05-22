#ifndef BEZIER_DISTANCE_H
#define BEZIER_DISTANCE_H

#include <cairo.h>

/* Returns the minimum Euclidean distance from point (px, py) to any
 * cubic Bezier segment (CAIRO_PATH_CURVE_TO) in `path`. Non-curve
 * elements (MOVE_TO, LINE_TO, CLOSE_PATH) are used only to track the
 * current point for chaining curves; they don't contribute to the
 * distance themselves.
 *
 * Returns INFINITY if the path contains no curve segments.
 * Returns NAN if path is NULL or has status != CAIRO_STATUS_SUCCESS.
 *
 * If `out_t` and `out_seg` are non-NULL, they receive the t parameter
 * (in [0,1]) and the zero-based segment index of the closest point. */
double bezier_path_distance(const cairo_path_t *path, double px, double py,
                            double *out_t, int *out_seg);

#endif

void bezier_extrema_axis(float p0, float p1, float p2, float p3, float *out_min,
                         float *out_max);
double bezier_path_distance(const cairo_path_t *path, double px, double py,
                            double *out_t, int *out_seg);

typedef struct bezier_dist_data {
  int width;
  int height;
  cairo_path_t *path;
} bezier_dist_t;
