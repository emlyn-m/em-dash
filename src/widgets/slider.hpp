#pragma once

#include <gtk-2.0/gtk/gtk.h>

// Vertical slider: value 0..1, filled from the bottom. `on_change` fires
// continuously while dragging; `on_release` fires once when the drag ends.
// Either callback may be null.
namespace ui {

using SliderFn = void (*)(float value, void *data);

GtkWidget *make_slider(int w, int h, void *data, SliderFn on_change,
                       SliderFn on_release);

// Set the displayed value from an external source (e.g. a device confirmation).
// No-op while the user is dragging, so it never snaps the knob off the finger.
void slider_sync(GtkWidget *slider, float value);

float slider_value(GtkWidget *slider);

}  // namespace ui
