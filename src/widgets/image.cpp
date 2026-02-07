#include "./widgets.hpp"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gtk-2.0/gtk/gtk.h"

void set_image_src(GtkImage* image, const guint8* icon_dat, int target_width, int target_height) {

   	GdkPixbuf* icon = gdk_pixbuf_new_from_inline(-1, icon_dat, FALSE, NULL);

	int width = gdk_pixbuf_get_width(icon);
	int height = gdk_pixbuf_get_height(icon);

	GtkWidget* fixed = gtk_widget_get_parent((GtkWidget*) image);
	GtkAllocation fixed_alloc;
	gtk_widget_get_allocation(fixed, &fixed_alloc);

	double scale = MIN((double)target_width / width,
	                    (double)target_height / height);
	int new_width = width * scale;
	int new_height = height * scale;

	double offset_x = (fixed_alloc.width - new_width) / 2.0;
	double offset_y = (fixed_alloc.height - new_height) / 2.0;

	GdkPixbuf* scaled = gdk_pixbuf_scale_simple(
	    icon, new_width, new_height, GDK_INTERP_BILINEAR
	);
	gtk_fixed_move(GTK_FIXED(fixed), (GtkWidget*) image, offset_x, offset_y);
	gtk_image_set_from_pixbuf(image, scaled);

}

GtkWidget* image_widget(GtkWidget** image_ref) {

	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);
	GtkWidget* fixed = gtk_fixed_new();
	GtkWidget* image = gtk_image_new();

	gtk_fixed_put(GTK_FIXED(fixed), image, 0, 0); // Set the desired offset
	gtk_box_pack_start(GTK_BOX(wrapper), fixed, TRUE, TRUE, 0);
	*image_ref = image;

	return wrapper;
}
