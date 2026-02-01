#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include "./widgets.hpp"

static gboolean on_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
    KindleSlider *slider = (KindleSlider*)data;
    GdkWindow *window = widget->window;
    GdkGC *gc = gdk_gc_new(window);

    GdkColor background_color = { 0, 0xffff, 0xffff, 0xffff };
    GdkColor fill_color       = { 0, 0x0000, 0x0000, 0x0000 };
    const gint border_offset_px = 10;
    const gint icon_height_px = 10;

    int height = widget->allocation.height;
    int width = widget->allocation.width;

    gdk_gc_set_rgb_fg_color(gc, &background_color);
    gdk_draw_rectangle(window, gc, TRUE, 0, 0, width, height);

    int fill_height = (int)(( height - (border_offset_px + icon_height_px) ) * slider->value);
    gdk_gc_set_rgb_fg_color(gc, &fill_color);
    gdk_draw_rectangle(
        window, gc, TRUE,
        border_offset_px, height - fill_height - icon_height_px,
        width - 2*border_offset_px, fill_height + icon_height_px - border_offset_px
    );


    g_object_unref(gc);
    return TRUE;
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    KindleSlider *slider = (KindleSlider*)data;
    slider->dragging = TRUE;
    return TRUE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    KindleSlider *slider = (KindleSlider*)data;
    slider->dragging = FALSE;
    if (slider->callback_release) { slider->callback_release(slider->value); }
    return TRUE;
}

static gboolean on_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    KindleSlider *slider = (KindleSlider*)data;
    if (slider->dragging) {
        int height = widget->allocation.height - 200;
        slider->value = 1.0 - (event->y / height);
        slider->value = CLAMP(slider->value, 0.0, 1.0);
        gtk_widget_queue_draw(widget);
        if (slider->callback_change) { slider->callback_change(slider->value); }
    }
    return TRUE;
}

KindleSlider* kindle_slider_new(void (*callback_change)(float progress), void (*callback_release)(float progress)) {
    KindleSlider *slider = g_new0(KindleSlider, 1);
    slider->value = 1.0;
    slider->dragging = FALSE;
    slider->callback_change = callback_change;
    slider->callback_release = callback_release;

    slider->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(slider->drawing_area, -1, 100);

    gtk_widget_add_events(slider->drawing_area,
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect(slider->drawing_area, "expose-event", G_CALLBACK(on_expose), slider);
    g_signal_connect(slider->drawing_area, "button-press-event", G_CALLBACK(on_button_press), slider);
    g_signal_connect(slider->drawing_area, "button-release-event", G_CALLBACK(on_button_release), slider);
    g_signal_connect(slider->drawing_area, "motion-notify-event", G_CALLBACK(on_motion), slider);

    return slider;
}
