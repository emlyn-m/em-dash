#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <unistd.h>

#include "screens.hpp"

void set_screen(GtkButton* _button, GdkEvent* _event, void* data_v) {
	set_screen_data_t* data = (set_screen_data*) data_v;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(data->stack), data->target_screen_idx);
}
