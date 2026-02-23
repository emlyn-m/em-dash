#include "src/screens/screens.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdlib>
#include <unistd.h>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>


gboolean _img_set_src_helper(void* data_vp) { 
    struct _img_src_dat* dat = (struct _img_src_dat*) data_vp; 
    set_image_src(GTK_IMAGE(dat->ref), dat->img, dat->size, dat->size);
    if (dat->flag == 3) {
        free(dat);
        return FALSE;
    }
    dat->flag++;
    return TRUE;
}
void set_screen(GtkButton* _button, GdkEvent* _event, void* data_v) {
	set_screen_data_t* data = (set_screen_data*) data_v;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(data->stack), data->target_screen_idx);
}
