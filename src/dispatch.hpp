#pragma once

#include <functional>

namespace ui {

// Schedule `fn` to run on the GTK main thread. Safe to call from ANY thread.
//
// GTK/GDK widgets may only be touched from the thread running the main loop.
// Worker threads should therefore do their work off-thread, then hand the
// result back through this to apply it to the UI, e.g.:
//
//   std::string result = do_expensive_work();
//   ui::post_to_main([result] { gtk_label_set_text(label, result.c_str()); });
//
// The closure runs once, on the main loop, and is then destroyed.
void post_to_main(std::function<void()> fn);

}  // namespace ui
