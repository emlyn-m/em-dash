#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>

#define CELL_ROWCOUNT 5
#define CELL_COLCOUNT 5
#define PADDING 20
#define OFFSET_SIZE 5
#define SCALE 1

static gboolean shadow_expose(GtkWidget* widget, GdkEventExpose* event, gpointer _data) {
	cairo_t* cr = gdk_cairo_create(widget->window);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, OFFSET_SIZE/2.0, OFFSET_SIZE*1.5, event->area.width - 2*OFFSET_SIZE, event->area.height - 2*OFFSET_SIZE);
	cairo_fill(cr);
	cairo_destroy(cr);

	return TRUE;
}

GtkWidget* gtk_table_custom(guint rows, guint cols) {
   	GtkWidget* table = gtk_table_new(CELL_ROWCOUNT, CELL_COLCOUNT, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 0); //PADDING*SCALE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 0); //PADDING*SCALE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 2*PADDING*SCALE);

	return table;
}

void gtk_table_add(GtkWidget* table, guint left, guint right, guint top, guint bottom, GtkWidget* elem) {
	GdkColor black = {0, 0, 0, 0};
	GdkColor white = {0, 255<<8, 255<<8, 255<<8};

	GtkWidget* event_box_inner = gtk_event_box_new();
	GtkWidget* event_box_outer = gtk_event_box_new();
	GtkWidget* event_box_shadow = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(event_box_inner), elem);
	gtk_container_add(GTK_CONTAINER(event_box_outer), event_box_inner);
	gtk_container_add(GTK_CONTAINER(event_box_shadow), event_box_outer);

	gtk_widget_modify_bg(event_box_inner, GTK_STATE_NORMAL, &white);
	gtk_widget_modify_bg(event_box_outer, GTK_STATE_NORMAL, &black);
	gtk_widget_modify_bg(event_box_shadow, GTK_STATE_NORMAL, &white);


	gtk_container_set_border_width(GTK_CONTAINER(event_box_inner), 2*SCALE);
	gtk_container_set_border_width(GTK_CONTAINER(event_box_outer), OFFSET_SIZE);
	gtk_container_set_border_width(GTK_CONTAINER(event_box_shadow), PADDING);


	g_signal_connect(event_box_shadow, "expose-event", G_CALLBACK(shadow_expose), NULL);

	gtk_table_attach(
	    GTK_TABLE(table), event_box_shadow,
	    left, right,
	    top, bottom,
	    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
	    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
	    0, 0
	);
}
