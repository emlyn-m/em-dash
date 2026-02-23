#include "src/log.hpp"
#include "src/screens/table.hpp"
#include "src/screens/screens.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>


GtkWidget* generate_life_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {

	set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;
	ctrl_data->target_screen_idx = SCREEN_IDX_CTRL;

	GtkWidget* table = gtk_table_custom(5,5);

   	gtk_table_add(table, 0, 1, 0, 1, time_widget());
	gtk_table_add(table, 1, 4, 0, 1, weather_widget());
	gtk_table_add(table, 4, 5, 0, 4, telem_widget());
	gtk_table_add(table, 0, 2, 1, 3, calendar_widget());
	gtk_table_add(table, 2, 4, 1, 5, tasks_widget());
	// gtk_table_add(table, 2, 4, 1, 5, keyboard_widget(keyboard_onpress, keyboard_onenter));

	gtk_table_add(table, 0, 2, 3, 5, alerts_widget());
	gtk_table_add(table, 4, 5, 4, 5, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_screen, ctrl_data));

	return table;
}
