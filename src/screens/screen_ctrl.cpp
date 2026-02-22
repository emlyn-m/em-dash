#include "src/screens/table.hpp"
#include "src/screens/screens.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstring>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>


// #define BRIGHTNESS_SYSFILE "/sys/class/backlight/intel_backlight/brightness"
// #define BRIGHTNESS_SYSFILE "./test"
// #define BRIGHTNESS_SCALE 96000
#define BRIGHTNESS_SYSFILE "/sys/class/backlight/max77696-bl/brightness"
#define BRIGHTNESS_SCALE 4095


void exit_handler(GtkButton* _b, GdkEvent* _e, void* _d) { 
    exit(0);
}

float get_brightness() {
	FILE* bfile = fopen(BRIGHTNESS_SYSFILE, "r");
	char brightness[32];
	fgets(brightness, 31, bfile);
	fclose(bfile);

	return atol(brightness) / (double) BRIGHTNESS_SCALE;
}

void set_brightness(GtkButton* _b, GdkEvent* _e, void* brightness) {
	char brightness_s[32];
	memset(brightness_s, 0, 32);

	float brightness_mult;
	memcpy(&brightness_mult, &brightness, sizeof(float));
	int target_brightness = (int) (brightness_mult * BRIGHTNESS_SCALE);
	printf("%d\n", target_brightness); fflush(stdout);
	snprintf(brightness_s, 31, "%d", target_brightness);
	FILE* bfile = fopen(BRIGHTNESS_SYSFILE, "w");
	fwrite(brightness_s, sizeof(char), 31, bfile);
	fclose(bfile);
}

void slider_brightness_callback(float progress, void* _v) {
	void* progress_guint;
	memcpy(&progress_guint, &progress, sizeof(progress));
	set_brightness(NULL, NULL, progress_guint);
}

void set_life_handler(GtkButton* _button, GdkEvent* _event, void* data_v) {
    set_screen_data_t* data = (set_screen_data*) data_v;
    data->target_screen_idx = SCREEN_IDX_LIFE;
    set_screen(_button, _event, data_v);
}

void set_led1_handler(GtkButton* _button, GdkEvent* _event, void* data_v) {
    set_screen_data_t* data = (set_screen_data_t*) data_v;
    data->target_screen_idx = SCREEN_IDX_LED1;
    set_screen(_button, _event, data_v);
}

GtkWidget* generate_ctrl_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {
	set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;

	GtkWidget* table = gtk_table_custom(15,10);

	KindleSlider* slider = kindle_slider_new(NULL, NULL, &slider_brightness_callback);
	gtk_table_add(table, 0, 2, 0, 9, slider->drawing_area);
	gtk_table_add(table, 0, 2, 9, 10, label_widget((char*) "γ"));
	gtk_table_add(table, 2, 5, 0, 2, button_widget((char*) "LED Strip", set_led1_handler, ctrl_data));
	
	gtk_table_add(table, 12, 15, 0, 2, button_widget((char*) "term",  exit_handler,   NULL));
	gtk_table_add(table, 12, 15, 8, 10, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_life_handler,    ctrl_data));
	gtk_table_add(table, 2, 12, 2, 10, model_init());
	return table;
}
