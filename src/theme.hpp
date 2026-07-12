#pragma once

#include <gtk-2.0/gtk/gtk.h>
#include <pango/pangocairo.h>

// Design canvas — matches the "kindlev2" Figma frame 1:1 in pixels.
namespace ui {

constexpr int SCREEN_W = 1440;
constexpr int SCREEN_H = 1080;

// Palette (0xRRGGBB). The whole design is monochrome on white with a single
// neutral grey used for placeholders.
constexpr unsigned BLACK = 0x000000;
constexpr unsigned WHITE = 0xFFFFFF;
constexpr unsigned GREY = 0xD9D9D9;  // placeholder / disabled fill
constexpr unsigned DOT = 0xCBCBCB;   // dotted-grid background
constexpr unsigned DEBUG_RED = 0xFF0000;

// Geist Mono is installed system-wide; faces are selected by weight.
constexpr const char *FONT_FAMILY = "Geist Mono";

// Set the cairo source to an 0xRRGGBB colour.
void set_rgb(cairo_t *cr, unsigned hex, double alpha = 1.0);

// Draw `text` inside the rect (rx, ry, rw, rh). halign/valign are 0=start,
// 0.5=centre, 1=end. `px` is the font size in device pixels.
void draw_text(cairo_t *cr, double rx, double ry, double rw, double rh,
               unsigned hex, const char *text, double px, PangoWeight weight,
               double halign = 0.0, double valign = 0.5);

// Draw a single line anchored at its top-left corner (x, y).
void draw_text_tl(cairo_t *cr, double x, double y, unsigned hex,
                  const char *text, double px, PangoWeight weight);

}  // namespace ui
