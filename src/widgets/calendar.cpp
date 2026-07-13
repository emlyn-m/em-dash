#include "net/calendar.hpp"
#include "cairo.h"
#include "pango/pango-font.h"
#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

namespace ui {

namespace {

gboolean draw_calendar(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  paint_dots_at(cr, w->allocation.width, w->allocation.height, w->allocation.x,
                w->allocation.y);

  draw_text(cr, 20, 20, 285, 39, BLACK, "calendar", 30, PANGO_WEIGHT_BOLD);

  Calendar calendar = calendar_state();
  if (!calendar.last_update) {
    return TRUE;
  }

  char daybuf[16] = {0};  // %A
  char datebuf[32] = {0}; // %B %-d
  char timebuf[64] = {0};

  for (int i = 0; i < MIN(calendar.events.size(), 5); i++) {
    struct tm *t_day = localtime(&calendar.events[i].start_time);
    strftime(daybuf, 16, "%A", t_day);
    daybuf[0] |= 0x20;
    struct tm *t_date = localtime(&calendar.events[i].start_time);
    strftime(datebuf, 32, "%B %-d", t_date);
    datebuf[0] |= 0x20;

    printf("%d %d, %d %d\n", calendar.events[i].start_time,
           calendar.events[i].end_time, calendar.events[i].start_time % 86400,
           calendar.events[i].end_time % 86400);

    if (!((calendar.events[i].start_time % 86400) |
          (calendar.events[i].end_time % 86400))) {
      snprintf(timebuf, 64, "all day");
    } else {
      struct tm *t_0 = localtime(&calendar.events[i].start_time);
      int n = strftime(timebuf, 64, "%-I:%M%p", t_0);
      timebuf[n - 2] |= 0x20;
      timebuf[n - 1] |= 0x20;
      timebuf[n] = ' ';
      timebuf[n + 1] = '|';
      timebuf[n + 2] = ' ';
      struct tm *t_1 = localtime(&calendar.events[i].end_time);
      int n2 = strftime(timebuf + n + 3, 64 - (n + 3), "%-I:%M%p", t_1);
      timebuf[n + n2 + 1] |= 0x20;
      timebuf[n + n2 + 2] |= 0x20;
    }

    draw_text(cr, 28, 73 + 52 * i, 289, 22, BLACK, daybuf, 16,
              PANGO_WEIGHT_BOLD);
    draw_text(cr, 28, 73 + 52 * i, 289, 22, BLACK,
              calendar.events[i].title.c_str(), 16, PANGO_WEIGHT_NORMAL, 1);

    draw_text(cr, 28, 94 + 52 * i, 289, 22, BLACK, datebuf, 16,
              PANGO_WEIGHT_BOLD);
    draw_text(cr, 28, 94 + 52 * i, 289, 22, BLACK, timebuf, 16,
              PANGO_WEIGHT_NORMAL, 1);
  }

  cairo_destroy(cr);
  return TRUE;
}

} // namespace

GtkWidget *make_calendar_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_calendar), nullptr);
  return a;
}

} // namespace ui
