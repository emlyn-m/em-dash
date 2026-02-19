#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>

#include "screens.hpp"
#include "../widgets/widgets.hpp"
#include "./table.hpp"


void set_ctrl_handler(GtkButton* _button, GdkEvent* _event, void* data_v) {
    set_screen_data_t* data = (set_screen_data_t*) data_v;
    data->target_screen_idx = SCREEN_IDX_CTRL;
    set_screen(_button, _event, data_v);
}

void set_led_brightness(GtkButton* _b, GdkEvent* _e, void* brightness) {
}

void led_brightness_callback(float progress) {
	void* progress_guint;
	memcpy(&progress_guint, &progress, sizeof(progress));
	set_led_brightness(NULL, NULL, progress_guint);
}

void set_led_color(GtkButton* _b, GdkEvent* _e, void* brightness) {
}

void led_color_callback(float progress) {
	void* progress_guint;
	memcpy(&progress_guint, &progress, sizeof(progress));
	set_led_color(NULL, NULL, progress_guint);
}



GtkWidget* generate_led_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {
	set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;

	GtkWidget* table = gtk_table_custom(5,5);
	
	GtkWidget* labelbox = gtk_hbox_new(FALSE, 0);
	GtkWidget* label = label_widget((char*) "led_strip.0");
	gtk_box_pack_start(GTK_BOX(labelbox), label, FALSE, FALSE, 5);
	gtk_table_add(table, 0, 4, 0, 1, labelbox);
	gtk_table_add(table, 4, 5, 0, 1, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_ctrl_handler,    ctrl_data));
	
	KindleSlider* brightness_slider = kindle_slider_new(NULL, &led_brightness_callback);
	gtk_table_add(table, 0, 1, 1, 5, brightness_slider->drawing_area);
	KindleSlider* color_slider = kindle_slider_new(NULL, &led_color_callback);
	gtk_table_add(table, 1, 2, 1, 5, color_slider->drawing_area);
	
	gtk_table_add(table, 2, 5, 1, 5, model_init());
	
	return table;
}
