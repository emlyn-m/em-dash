#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include "screens.hpp"
#include "../widgets/widgets.hpp"
#include "./table.hpp"


void set_ctrl_handler(GtkButton* _button, GdkEvent* _event, void* data_v) {
    set_screen_data_t* data = (set_screen_data_t*) data_v;
    data->target_screen_idx = SCREEN_IDX_CTRL;
    set_screen(_button, _event, data_v);
}

void set_led_power(GtkButton* _b, GdkEvent* _e, void* brightness) {}
void set_led_red(GtkButton* _b, GdkEvent* _e, void* brightness) {}
void set_led_green(GtkButton* _b, GdkEvent* _e, void* brightness) {}
void set_led_blue(GtkButton* _b, GdkEvent* _e, void* brightness) {}

void led_power_callback(GtkButton* _button, GdkEvent* _event, void* data_v) {
}

void led_hue_callback(float progress) {
	void* progress_guint;
	memcpy(&progress_guint, &progress, sizeof(progress));
	set_led_red(NULL, NULL, progress_guint);
}
void led_sat_callback(float progress) {
	void* progress_guint;
	memcpy(&progress_guint, &progress, sizeof(progress));
	set_led_green(NULL, NULL, progress_guint);
}
void led_val_callback(float progress) {
	void* progress_guint;
	memcpy(&progress_guint, &progress, sizeof(progress));
	set_led_blue(NULL, NULL, progress_guint);
}


GtkWidget* generate_led_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {
	set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;

	GtkWidget* table = gtk_table_custom(7,9);

	gtk_table_add(table, 0, 1, 0, 1, button_widget((char*) "on/off", led_power_callback, NULL));
	GtkWidget* labelbox = gtk_hbox_new(FALSE, 0);
	GtkWidget* label = label_widget((char*) "led_strip.0");
	gtk_box_pack_start(GTK_BOX(labelbox), label, FALSE, FALSE, 10);
	gtk_table_add(table, 1, 7, 0, 1, labelbox);
	gtk_table_add(table, 7, 9, 0, 1, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_ctrl_handler,    ctrl_data));

	KindleSlider* hue_slider = kindle_slider_new(NULL, &led_hue_callback);
	gtk_table_add(table, 0, 1, 1, 6, hue_slider->drawing_area);
	gtk_table_add(table, 0, 1, 6, 7, label_widget((char*) "hue"));
	KindleSlider* sat_slider = kindle_slider_new(NULL, &led_sat_callback);
	gtk_table_add(table, 1, 2, 1, 6, sat_slider->drawing_area);
	gtk_table_add(table, 1, 2, 6, 7, label_widget((char*) "sat"));
	KindleSlider* val_slider = kindle_slider_new(NULL, &led_val_callback);
	gtk_table_add(table, 2, 3, 1, 6, val_slider->drawing_area);
	gtk_table_add(table, 2, 3, 6, 7, label_widget((char*) "value"));


	gtk_table_add(table, 3, 9, 1, 7, model_init());

	return table;
}
