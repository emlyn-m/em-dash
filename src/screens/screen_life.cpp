#include <cstdio>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>

#include "screens.hpp"
#include "../widgets/widgets.hpp"
#include "./table.hpp"

gboolean keyboard_onpress(kb_data_t* data) {
	char char_val = match_keycode(data->lookup_table, data->layer, data->x, data->y);
	if (char_val == '\n') {
		return FALSE; // newline - end of input, so exit keyboard
	}

	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m pressed char '%c' at coords %lf, %lf  layer %d\n", char_val, data->x, data->y, data->layer);
	fflush(stdout);

	return TRUE;
}

void keyboard_onenter(kb_data_t* data) {
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m typed \"%s\"\n", data->kb_buf);
	fflush(stdout);
}

GtkWidget* generate_life_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {

	set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;
	// ctrl_data->target_screen_idx = SCREEN_IDX_CTRL;
	ctrl_data->target_screen_idx = SCREEN_IDX_LED1;

	GtkWidget* table = gtk_table_custom(5,5);

   	gtk_table_add(table, 0, 1, 0, 1, time_widget());
	gtk_table_add(table, 1, 4, 0, 1, weather_widget());
	gtk_table_add(table, 4, 5, 0, 4, telem_widget());
	gtk_table_add(table, 0, 2, 1, 3, calendar_widget());
	// gtk_table_add(table, 2, 4, 1, 5, tasks_widget());
	gtk_table_add(table, 2, 4, 1, 5, keyboard_widget(keyboard_onpress, keyboard_onenter));

	gtk_table_add(table, 0, 2, 3, 5, alerts_widget());
	gtk_table_add(table, 4, 5, 4, 5, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_screen, ctrl_data));

	return table;
}
