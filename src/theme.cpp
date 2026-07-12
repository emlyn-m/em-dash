#include "theme.hpp"

namespace ui {

void set_rgb(cairo_t *cr, unsigned hex, double alpha) {
  cairo_set_source_rgba(cr, ((hex >> 16) & 0xFF) / 255.0,
                        ((hex >> 8) & 0xFF) / 255.0, (hex & 0xFF) / 255.0,
                        alpha);
}

static PangoLayout *make_layout(cairo_t *cr, const char *text, double px,
                                PangoWeight weight) {
  PangoLayout *layout = pango_cairo_create_layout(cr);
  PangoFontDescription *desc = pango_font_description_new();
  pango_font_description_set_family(desc, FONT_FAMILY);
  pango_font_description_set_weight(desc, weight);
  // Absolute size keeps text pixel-accurate to the Figma regardless of DPI.
  pango_font_description_set_absolute_size(desc, px * PANGO_SCALE);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);
  pango_layout_set_text(layout, text, -1);
  return layout;
}

void draw_text(cairo_t *cr, double rx, double ry, double rw, double rh,
               unsigned hex, const char *text, double px, PangoWeight weight,
               double halign, double valign) {
  PangoLayout *layout = make_layout(cr, text, px, weight);
  int tw, th;
  pango_layout_get_pixel_size(layout, &tw, &th);
  set_rgb(cr, hex);
  cairo_move_to(cr, rx + halign * (rw - tw), ry + valign * (rh - th));
  pango_cairo_show_layout(cr, layout);
  g_object_unref(layout);
}

void draw_text_tl(cairo_t *cr, double x, double y, unsigned hex,
                  const char *text, double px, PangoWeight weight) {
  PangoLayout *layout = make_layout(cr, text, px, weight);
  set_rgb(cr, hex);
  cairo_move_to(cr, x, y);
  pango_cairo_show_layout(cr, layout);
  g_object_unref(layout);
}

}  // namespace ui
