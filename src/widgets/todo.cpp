#include "net/calendar.hpp"

#include "theme.hpp"
#include "widgets/common.hpp"
#include "widgets/widgets.hpp"

#include <set>
#include <string>
#include <vector>

namespace ui {

namespace {

// Panel-local layout, sharing the calendar widget's rhythm.
constexpr int MARGIN = 20;
constexpr int LIST_TOP = 74;
constexpr int ROW_H = 34;
constexpr int BOX = 18;
constexpr int BOX_GAP = 14;

// Ticked todo ids. Lives here rather than in the model so the net layer stays
// gtk-free; keying on the event id means a refresh cannot clear a tick.
std::set<std::string> g_done;

bool is_done(const CalEvent &ev) {
  return !ev.id.empty() && g_done.count(ev.id) > 0;
}

// Pending first in feed order, then done — the single ordering both the expose
// and the click handler read, so hit-testing cannot drift from what was drawn.
std::vector<const CalEvent *> build_rows() {
  const std::vector<CalEvent> &todos = calendar_state().todos;
  std::vector<const CalEvent *> rows;
  rows.reserve(todos.size());
  for (const CalEvent &ev : todos)
    if (!is_done(ev))
      rows.push_back(&ev);
  for (const CalEvent &ev : todos)
    if (is_done(ev))
      rows.push_back(&ev);
  return rows;
}

// How many rows fit in a panel `h` tall — shared so a click below the last
// drawn row cannot toggle a todo the user cannot see.
int visible_rows(int h) {
  const int space = h - MARGIN - LIST_TOP;
  return space > 0 ? space / ROW_H : 0;
}

void draw_checkbox(cairo_t *cr, double x, double y, bool done) {
  set_rgb(cr, done ? MUTED : BLACK);
  cairo_set_line_width(cr, 2);
  cairo_rectangle(cr, x + 1, y + 1, BOX - 2, BOX - 2);
  cairo_stroke(cr);

  if (!done)
    return;
  cairo_set_line_width(cr, 2);
  cairo_move_to(cr, x + 4, y + BOX * 0.55);
  cairo_line_to(cr, x + BOX * 0.42, y + BOX - 5);
  cairo_line_to(cr, x + BOX - 4, y + 4);
  cairo_stroke(cr);
}

gboolean draw_todo(GtkWidget *w, GdkEventExpose *, gpointer) {
  cairo_t *cr = detail::begin_paint(w);
  const int W = w->allocation.width, H = w->allocation.height;
  paint_dots_at(cr, W, H, w->allocation.x, w->allocation.y);

  draw_text(cr, MARGIN, 20, W - 2 * MARGIN, 39, BLACK, "goals", 30,
            PANGO_WEIGHT_BOLD);

  const int text_x = MARGIN + 10 + BOX + BOX_GAP;
  const int text_w = W - MARGIN - text_x;

  std::vector<const CalEvent *> rows = build_rows();
  const size_t limit = MIN(rows.size(), (size_t)visible_rows(H));

  for (size_t i = 0; i < limit; i++) {
    const CalEvent *ev = rows[i];
    const int y = LIST_TOP + (int)i * ROW_H;
    const bool done = is_done(*ev);

    draw_checkbox(cr, MARGIN + 10, y + (ROW_H - BOX) / 2, done);

    cairo_save(cr);
    cairo_rectangle(cr, text_x, y, text_w, ROW_H);
    cairo_clip(cr);
    draw_text(cr, text_x, y, text_w, ROW_H, done ? MUTED : BLACK,
              ev->title.c_str(), 16, PANGO_WEIGHT_MEDIUM);

    if (done) {
      double tw = text_width(cr, ev->title.c_str(), 16, PANGO_WEIGHT_MEDIUM);
      set_rgb(cr, MUTED);
      cairo_set_line_width(cr, 1);
      cairo_move_to(cr, text_x, y + ROW_H / 2 + 0.5);
      cairo_line_to(cr, text_x + MIN(tw, (double)text_w), y + ROW_H / 2 + 0.5);
      cairo_stroke(cr);
    }
    cairo_restore(cr);
  }

  cairo_destroy(cr);
  return TRUE;
}

gboolean todo_press(GtkWidget *w, GdkEventButton *ev, gpointer) {
  if (ev->y < LIST_TOP)
    return TRUE;

  const size_t row = (size_t)((ev->y - LIST_TOP) / ROW_H);
  std::vector<const CalEvent *> rows = build_rows();
  if (row >= MIN(rows.size(), (size_t)visible_rows(w->allocation.height)) ||
      rows[row]->id.empty())
    return TRUE;

  const std::string &id = rows[row]->id;
  if (!g_done.erase(id))
    g_done.insert(id);
  gtk_widget_queue_draw(w);
  return TRUE;
}

} // namespace

GtkWidget *make_todo_surface(int w, int h) {
  GtkWidget *a = detail::new_area(w, h);
  g_signal_connect(a, "expose-event", G_CALLBACK(draw_todo), nullptr);
  detail::make_clickable(a, todo_press, nullptr);
  return a;
}

} // namespace ui
