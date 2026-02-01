#include <cstdio>
#include <cstring>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>

#include "screens.hpp"
#include "../widgets/widgets.hpp"
#include "./table.hpp"

// #define BRIGHTNESS_SYSFILE "/sys/class/backlight/intel_backlight/brightness"
// #define BRIGHTNESS_SYSFILE "./test"
// #define BRIGHTNESS_SCALE 96000
#define BRIGHTNESS_SYSFILE "/sys/class/backlight/max77696-bl/brightness"
#define BRIGHTNESS_SCALE 4095

void set_screen(GtkButton* _button, GdkEvent* _event, void* data_v) {
    set_screen_data_t* data = (set_screen_data*) data_v;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(data->stack), data->target_screen_idx);
}

GtkWidget* generate_life_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {

    set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;
	ctrl_data->target_screen_idx = SCREEN_IDX_CTRL;

    GtkWidget* table = gtk_table_custom(5,5);

   	gtk_table_add(table, 0, 1, 0, 1, time_widget());
	gtk_table_add(table, 1, 4, 0, 1, weather_widget());
	gtk_table_add(table, 4, 5, 0, 4, telem_widget(5000.0));
	gtk_table_add(table, 0, 2, 1, 3, calendar_widget());
	gtk_table_add(table, 2, 4, 1, 5, tasks_widget());
	gtk_table_add(table, 0, 2, 3, 5, alerts_widget());
	gtk_table_add(table, 4, 5, 4, 5, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_screen, ctrl_data));

	return table;

}


void exit_handler(GtkButton* _b, GdkEvent* _e, void* _d) { exit(0); }
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

void slider_brightness_callback(float progress) {
    void* progress_guint;
    memcpy(&progress_guint, &progress, sizeof(progress));
    set_brightness(NULL, NULL, progress_guint);
}

GtkWidget* generate_ctrl_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {

    set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;
	ctrl_data->target_screen_idx = SCREEN_IDX_LIFE;


    GtkWidget* table = gtk_table_custom(15,5);

    KindleSlider* slider = kindle_slider_new(NULL, &slider_brightness_callback);
    gtk_table_add(table, 0, 2, 0, 4, slider->drawing_area);
    gtk_table_add(table, 0, 2, 4, 5, label_widget((char*) "screen brightness"));

    gtk_table_add(table, 12, 15, 0, 1, button_widget((char*) "term",  exit_handler,   NULL));
    gtk_table_add(table, 12, 15, 4, 5, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_screen,    ctrl_data));
    gtk_table_add(table, 2, 12, 0, 5, model_init());
    return table;
}
