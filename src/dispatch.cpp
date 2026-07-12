#include "dispatch.hpp"

#include <glib.h>

namespace ui {

namespace {

// Runs on the main loop; applies the closure and frees it.
gboolean run_on_main(gpointer data) {
  auto *fn = static_cast<std::function<void()> *>(data);
  (*fn)();
  delete fn;
  return G_SOURCE_REMOVE;
}

}  // namespace

void post_to_main(std::function<void()> fn) {
  // g_idle_add attaches a source to the default main context, which is
  // thread-safe to do from any thread; the callback then executes on the
  // main loop where GTK calls are legal.
  g_idle_add(run_on_main, new std::function<void()>(std::move(fn)));
}

}  // namespace ui
