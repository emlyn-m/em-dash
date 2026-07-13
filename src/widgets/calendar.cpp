#include "net/calendar.hpp"

#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

#include <cctype>
#include <cstdio>
#include <ctime>

namespace ui {

namespace {

// Agenda layout (panel-local px).
constexpr int MARGIN = 20;
constexpr int AGENDA_TOP = 74;
constexpr int DAYBAR_H = 26; // sharp black date bar
constexpr int DAYBAR_GAP = 4;
constexpr int ROW_H = 34;         // one event
constexpr int TIME_RESERVE = 130; // width kept clear on the right for the time

void lower(char *s) {
  for (; *s; s++)
    *s = (char)tolower((unsigned char)*s);
}

// Distinct per calendar day, for grouping consecutive events.
int day_key(time_t t) {
  struct tm x = *localtime(&t);
  return x.tm_year * 400 + x.tm_yday;
}

// "all day", else "9:00am–9:30am" (lowercased).
void format_time(const CalEvent &ev, char *buf, size_t n) {
  struct tm s = *localtime(&ev.start_time);
  struct tm e = *localtime(&ev.end_time);
  if (s.tm_hour == 0 && s.tm_min == 0 && e.tm_hour == 0 && e.tm_min == 0) {
    snprintf(buf, n, "all day");
    return;
  }
  char a[16], b[16];
  strftime(a, sizeof a, "%-I:%M%p", &s);
  strftime(b, sizeof b, "%-I:%M%p", &e);
  lower(a);
  lower(b);
  snprintf(buf, n, "%s – %s", a, b); // en dash
}

gboolean draw_calendar(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  const int W = w->allocation.width, H = w->allocation.height;
  paint_dots_at(cr, W, H, w->allocation.x, w->allocation.y);

  draw_text(cr, MARGIN, 20, W - 2 * MARGIN, 39, BLACK, "calendar", 30,
            PANGO_WEIGHT_BOLD);

  Calendar calendar = calendar_state();
  if (!calendar.last_update) {
    cairo_destroy(cr);
    return TRUE;
  }

  const int cw = W - 2 * MARGIN; // content width
  const int bottom = H - MARGIN;
  int y = AGENDA_TOP;
  int last_day = -1;

  for (size_t i = 0; i < calendar.events.size(); i++) {
    const CalEvent &ev = calendar.events[i];
    int dk = day_key(ev.start_time);

    // Sharp black date bar whenever the day changes — but only if there's room
    // for the bar plus at least one event, so we never leave a dangling header.
    if (dk != last_day) {
      if (y + DAYBAR_H + ROW_H > bottom)
        break;
      last_day = dk;

      char day_buf[32];
      struct tm d = *localtime(&ev.start_time);
      strftime(day_buf, sizeof day_buf, "%a %-d %b", &d); // "tue 14 jul"
      lower(day_buf);

      set_rgb(cr, BLACK);
      cairo_rectangle(cr, MARGIN, y, cw, DAYBAR_H);
      cairo_fill(cr);
      draw_text(cr, MARGIN + 10, y, cw - 20, DAYBAR_H, WHITE, day_buf, 15,
                PANGO_WEIGHT_BOLD, 0.0, 0.5);
      y += DAYBAR_H + DAYBAR_GAP;
    }

    if (y + ROW_H > bottom)
      break;

    // Time — muted grey, right-aligned.
    char time_buf[32];
    format_time(ev, time_buf, sizeof time_buf);
    draw_text(cr, MARGIN, y, cw, ROW_H, MUTED, time_buf, 14,
              PANGO_WEIGHT_NORMAL, /*halign=*/1.0);

    // Title — black, clipped so a long one never runs under the time.
    cairo_save(cr);
    cairo_rectangle(cr, MARGIN + 10, y, cw - 10 - TIME_RESERVE, ROW_H);
    cairo_clip(cr);
    draw_text(cr, MARGIN + 10, y, cw, ROW_H, BLACK, ev.title.c_str(), 16,
              PANGO_WEIGHT_MEDIUM, /*halign=*/0.0);
    cairo_restore(cr);

    // Hairline between events of the same day (the next day's bar separates
    // days on its own).
    bool same_day_next = i + 1 < calendar.events.size() &&
                         day_key(calendar.events[i + 1].start_time) == dk;
    if (same_day_next) {
      set_rgb(cr, GREY);
      cairo_set_line_width(cr, 1);
      cairo_move_to(cr, MARGIN + 10, y + ROW_H + 0.5);
      cairo_line_to(cr, MARGIN + cw, y + ROW_H + 0.5);
      cairo_stroke(cr);
    }

    y += ROW_H;
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
