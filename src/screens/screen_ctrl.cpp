#include "src/images/val.h"
#include "src/screens/table.hpp"
#include "src/screens/screens.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>


#define DISABLE_BRIGHTNESS 0
#define BRIGHTNESS_SYSFILE "/sys/class/backlight/max77696-bl/brightness"
// #define BRIGHTNESS_SCALE 4095
#define BRIGHTNESS_SCALE 200


void exit_handler(GtkButton* _b, GdkEvent* _e, void* _d) { 
    exit(0);
}

float get_brightness() {
    if (DISABLE_BRIGHTNESS) { return 0; }
	FILE* bfile = fopen(BRIGHTNESS_SYSFILE, "r");
	char brightness[32];
	fgets(brightness, 31, bfile);
	fclose(bfile);

	return atol(brightness) / (double) BRIGHTNESS_SCALE;
}

void set_brightness(GtkButton* _b, GdkEvent* _e, void* brightness) {
    if (DISABLE_BRIGHTNESS) { return; }
	char brightness_s[32];
	memset(brightness_s, 0, 32);

	float brightness_mult;
	memcpy(&brightness_mult, &brightness, sizeof(float));
	int target_brightness = (int) (brightness_mult * BRIGHTNESS_SCALE);
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
void* _slider_brightness_fetch(void* data) {
    kindle_slider_t* slider = (kindle_slider_t*) data;
    float brightness = get_brightness();
    slider->value = brightness;
    
    return NULL;
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
	pthread_t _t;
	pthread_create(&_t, NULL, _slider_brightness_fetch, (void*) slider);
	gtk_table_add(table, 0, 2, 0, 8, slider->drawing_area);
	struct _img_src_dat* brightness_data = (struct _img_src_dat*) malloc(sizeof(struct _img_src_dat));
	gtk_table_add(table, 0, 2, 8, 10, image_widget(&brightness_data->ref));
	brightness_data->img = val;
	brightness_data->size = 80;
	brightness_data->flag = 0;
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) _img_set_src_helper, brightness_data);
	
	gtk_table_add(table, 12, 15, 0, 2, button_widget((char*) "<SIGTERM>",  exit_handler,   NULL));
	gtk_table_add(table, 12, 15, 2, 4, button_widget((char*) "led.strip0", set_led1_handler, ctrl_data));
	gtk_table_add(table, 12, 15, 8, 10, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_life_handler,    ctrl_data));
	gtk_table_add(table, 2, 12, 0, 10, log_widget());
	return table;
}
