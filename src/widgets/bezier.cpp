#include "bezier.h"
#include <float.h>
#include <math.h>
#include <stddef.h>

/* Number of initial samples per curve for Newton seed selection.
 * Bumping this helps on highly curved segments; 16 is comfortable for
 * most well-behaved paths. */
#define BEZIER_SEED_SAMPLES 16

/* Newton iteration cap. Quadratic convergence means we almost never
 * need this many; it's just a safety net against pathological seeds. */
#define BEZIER_NEWTON_ITERS 8

/* Stop Newton when |Δt| drops below this. ~1e-9 is well under any
 * practical pixel precision but cheap to reach with good convergence. */
#define BEZIER_NEWTON_EPS 1e-10

/* Evaluate cubic Bezier B(t) and its first two derivatives.
 * Using the expanded polynomial form (rather than de Casteljau) since
 * we need B, B', B'' at the same t and the polynomial form shares work
 * naturally through the coefficients. */
static inline void bezier_eval(double t, double p0x, double p0y, double p1x,
                               double p1y, double p2x, double p2y, double p3x,
                               double p3y, double *bx, double *by, double *dx,
                               double *dy, double *ddx, double *ddy) {
  const double u = 1.0 - t;
  const double uu = u * u;
  const double tt = t * t;
  const double uut = uu * t;
  const double utt = u * tt;

  /* B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3 */
  *bx = uu * u * p0x + 3.0 * uut * p1x + 3.0 * utt * p2x + tt * t * p3x;
  *by = uu * u * p0y + 3.0 * uut * p1y + 3.0 * utt * p2y + tt * t * p3y;

  /* B'(t) = 3(1-t)^2 (P1-P0) + 6(1-t)t (P2-P1) + 3t^2 (P3-P2) */
  *dx = 3.0 * uu * (p1x - p0x) + 6.0 * u * t * (p2x - p1x) +
        3.0 * tt * (p3x - p2x);
  *dy = 3.0 * uu * (p1y - p0y) + 6.0 * u * t * (p2y - p1y) +
        3.0 * tt * (p3y - p2y);

  /* B''(t) = 6(1-t)(P2 - 2P1 + P0) + 6t(P3 - 2P2 + P1) */
  *ddx = 6.0 * u * (p2x - 2.0 * p1x + p0x) + 6.0 * t * (p3x - 2.0 * p2x + p1x);
  *ddy = 6.0 * u * (p2y - 2.0 * p1y + p0y) + 6.0 * t * (p3y - 2.0 * p2y + p1y);
}

/* Squared distance from (px,py) to curve at parameter t. Squared
 * avoids a sqrt per evaluation; we only sqrt the final answer. */
static inline double bezier_dist2_at(double t, double p0x, double p0y,
                                     double p1x, double p1y, double p2x,
                                     double p2y, double p3x, double p3y,
                                     double px, double py) {
  double bx, by, dx_, dy_, ddx, ddy;
  bezier_eval(t, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, &bx, &by, &dx_, &dy_,
              &ddx, &ddy);
  const double ex = bx - px;
  const double ey = by - py;
  return ex * ex + ey * ey;
}

/* Minimum squared distance from (px,py) to one cubic Bezier segment.
 *
 * Strategy:
 *   1. Sample BEZIER_SEED_SAMPLES+1 points uniformly in t -> pick the
 *      closest as Newton seed. (Endpoints included.)
 *   2. Newton on f(t) = (B(t)-P) . B'(t), whose roots are the critical
 *      points of squared distance. f'(t) = |B'(t)|^2 + (B(t)-P).B''(t).
 *   3. Clamp final t to [0,1]; compare against endpoint distances.
 *
 * Newton can in principle wander toward a maximum or land outside [0,1]
 * with a poor seed, but a 16-sample seed makes this very rare for any
 * reasonable curve. We clamp at the end either way. */
static double bezier_segment_min_dist2(double p0x, double p0y, double p1x,
                                       double p1y, double p2x, double p2y,
                                       double p3x, double p3y, double px,
                                       double py, double *best_t_out) {
  /* Step 1: seed by sampling. */
  double best_t = 0.0;
  double best_d2 =
      bezier_dist2_at(0.0, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, px, py);

  for (int i = 1; i <= BEZIER_SEED_SAMPLES; i++) {
    const double t = (double)i / (double)BEZIER_SEED_SAMPLES;
    const double d2 =
        bezier_dist2_at(t, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, px, py);
    if (d2 < best_d2) {
      best_d2 = d2;
      best_t = t;
    }
  }

  /* Step 2: Newton refinement. */
  double t = best_t;
  for (int i = 0; i < BEZIER_NEWTON_ITERS; i++) {
    double bx, by, dx_, dy_, ddx, ddy;
    bezier_eval(t, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, &bx, &by, &dx_, &dy_,
                &ddx, &ddy);
    const double ex = bx - px;
    const double ey = by - py;
    const double f = ex * dx_ + ey * dy_; /* derivative of d^2/2 */
    const double fp =
        dx_ * dx_ + dy_ * dy_ + ex * ddx + ey * ddy; /* second derivative */

    /* If fp is ~0 we're at an inflection in d^2; bail out and keep
     * the current t — the seed sample was probably already optimal. */
    if (fabs(fp) < 1e-14)
      break;

    const double dt = f / fp;
    t -= dt;

    /* If Newton jumps outside [0,1], clamp once and continue: the
     * minimum is likely at an endpoint, but one more iteration from
     * the boundary can still polish things. */
    if (t < 0.0)
      t = 0.0;
    else if (t > 1.0)
      t = 1.0;

    if (fabs(dt) < BEZIER_NEWTON_EPS)
      break;
  }

  /* Step 3: compare against the best seed and against endpoints, in
   * case Newton drifted to a worse critical point. */
  const double d2_newton =
      bezier_dist2_at(t, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, px, py);
  const double d2_t0 =
      bezier_dist2_at(0.0, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, px, py);
  const double d2_t1 =
      bezier_dist2_at(1.0, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, px, py);

  double final_t = t, final_d2 = d2_newton;
  if (d2_t0 < final_d2) {
    final_d2 = d2_t0;
    final_t = 0.0;
  }
  if (d2_t1 < final_d2) {
    final_d2 = d2_t1;
    final_t = 1.0;
  }
  if (best_d2 < final_d2) {
    final_d2 = best_d2;
    final_t = best_t;
  }

  if (best_t_out)
    *best_t_out = final_t;
  return final_d2;
}

double bezier_path_distance(const cairo_path_t *path, double px, double py,
                            double *out_t, int *out_seg) {
  if (path == NULL || path->status != CAIRO_STATUS_SUCCESS) {
    return NAN;
  }

  double best_d2 = INFINITY;
  double best_t = 0.0;
  int best_seg = -1;
  int seg_idx = 0;

  /* Track current point so CURVE_TO segments can pick up P0 from
   * whatever came before. MOVE_TO and the implicit start-of-path are
   * the only places this matters; LINE_TO updates it too in case we
   * later extend to handle lines. */
  double cur_x = 0.0, cur_y = 0.0;
  int have_cur = 0;

  for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
    const cairo_path_data_t *data = &path->data[i];

    switch (data->header.type) {
    case CAIRO_PATH_MOVE_TO:
      cur_x = data[1].point.x;
      cur_y = data[1].point.y;
      have_cur = 1;
      break;

    case CAIRO_PATH_LINE_TO:
      cur_x = data[1].point.x;
      cur_y = data[1].point.y;
      have_cur = 1;
      break;

    case CAIRO_PATH_CURVE_TO: {
      /* Skip orphan curves with no preceding point — cairo
       * normally guarantees one, but defensive coding is cheap. */
      if (!have_cur) {
        cur_x = data[3].point.x;
        cur_y = data[3].point.y;
        have_cur = 1;
        seg_idx++;
        break;
      }

      const double p0x = cur_x, p0y = cur_y;
      const double p1x = data[1].point.x, p1y = data[1].point.y;
      const double p2x = data[2].point.x, p2y = data[2].point.y;
      const double p3x = data[3].point.x, p3y = data[3].point.y;

      double seg_t;
      const double d2 = bezier_segment_min_dist2(p0x, p0y, p1x, p1y, p2x, p2y,
                                                 p3x, p3y, px, py, &seg_t);

      if (d2 < best_d2) {
        best_d2 = d2;
        best_t = seg_t;
        best_seg = seg_idx;
      }

      cur_x = p3x;
      cur_y = p3y;
      seg_idx++;
      break;
    }

    case CAIRO_PATH_CLOSE_PATH:
      /* No-op for distance: closing a subpath would draw a line
       * back to the subpath start, but we're ignoring lines. */
      break;
    }
  }

  if (best_seg < 0) {
    /* Path had no curves. */
    if (out_t)
      *out_t = 0.0;
    if (out_seg)
      *out_seg = -1;
    return INFINITY;
  }

  if (out_t)
    *out_t = best_t;
  if (out_seg)
    *out_seg = best_seg;
  return sqrt(best_d2);
}

void bezier_extrema_axis(float p0, float p1, float p2, float p3, float *out_min,
                         float *out_max) {
  float lo = p0 < p3 ? p0 : p3;
  float hi = p0 > p3 ? p0 : p3;

  float a = -p0 + 3.0f * p1 - 3.0f * p2 + p3;
  float b = 2.0f * (p0 - 2.0f * p1 + p2);
  float c = p1 - p0;

  float roots[2];
  int n = 0;

  if (fabsf(a) < 1e-6f) {
    if (fabsf(b) > 1e-6f)
      roots[n++] = -c / b;
  } else {
    float disc = b * b - 4.0f * a * c;
    if (disc >= 0.0f) {
      float s = sqrtf(disc);
      roots[n++] = (-b + s) / (2.0f * a);
      roots[n++] = (-b - s) / (2.0f * a);
    }
  }

  for (int i = 0; i < n; i++) {
    float t = roots[i];
    if (t <= 0.0f || t >= 1.0f)
      continue;
    float u = 1.0f - t;
    float v = u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 +
              t * t * t * p3;
    if (v < lo)
      lo = v;
    if (v > hi)
      hi = v;
  }

  *out_min = lo;
  *out_max = hi;
}
