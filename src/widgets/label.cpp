#include "./widgets.hpp"
#include "gtk/gtk.h"

GtkWidget* label_widget(char* label_content) {
	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);

	GtkWidget* label = gtk_label_new(label_content);
	PangoFontDescription* font_desc = pango_font_description_from_string(FONT_10);
	gtk_widget_modify_font(label, font_desc);
	pango_font_description_free(font_desc);

	gtk_box_pack_start(GTK_BOX(wrapper), label, TRUE, TRUE, 5*SCALE);
	return wrapper;
}
