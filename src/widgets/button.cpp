#include "./widgets.hpp"
#include "glib-object.h"
#include "gtk/gtk.h"
#include "pango/pango-font.h"
#include <pango/pango.h>

GtkWidget* button_widget(char* label_content, void (*callback)(GtkButton*, GdkEvent*, void*), void* data) {
	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);

	GtkWidget* ev_box = gtk_event_box_new();
	gtk_widget_add_events(ev_box, GDK_BUTTON_PRESS_MASK);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev_box), FALSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_EVENT_BOX(ev_box), GTK_CAN_FOCUS);

	GtkWidget* label = gtk_label_new(label_content);
	PangoFontDescription* font_desc = pango_font_description_from_string(FONT_10);
	gtk_widget_modify_font(label, font_desc);
	pango_font_description_free(font_desc);
	gtk_container_add(GTK_CONTAINER(ev_box), label);

	g_signal_connect(ev_box, "button-press-event", G_CALLBACK(callback), data);

	gtk_box_pack_start(GTK_BOX(wrapper), ev_box, TRUE, TRUE, 5*SCALE);
	return wrapper;
}
