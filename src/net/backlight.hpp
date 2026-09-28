#pragma once

#include <functional>

// Frontlight control via sysfs.
// Writes happen on a worker thread and are coalesced.
namespace ui {

void backlight_start(std::function<void(float)> on_read,
                     std::function<void(float)> on_change);
void backlight_set(float level);

} // namespace ui
