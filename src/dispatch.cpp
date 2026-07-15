#include "dispatch.hpp"

#include <glib.h>

namespace ui {

namespace {

gboolean run_on_main(gpointer data) {
  auto *fn = static_cast<std::function<void()> *>(data);
  (*fn)();
  delete fn;
  return FALSE;
}

} // namespace

void post_to_main(std::function<void()> fn) {
  g_idle_add(run_on_main, new std::function<void()>(std::move(fn)));
}

} // namespace ui
