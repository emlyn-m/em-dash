#include "src/log.hpp"
#include "src/widgets/widgets.hpp"

#include "glib.h"
#include <cstdlib>
#include <cstring>
#include "gtk/gtk.h"

gboolean _update_log_display(void* data_vp) {
    GtkWidget** views = (GtkWidget**) data_vp;
    struct _log_record logs[MAX_RECORDS] = { {0} };
    int nlogs = fetch_n_logs(MAX_RECORDS, logs);
    
   	for (int i=0; i < nlogs; i++) {
	    gtk_widget_set_visible(views[i], TRUE);
	    char* task_markup = (char*) malloc(256); memset(task_markup, 0, 256);
		snprintf(task_markup, 256, "<b>%s:</b> %s", logs[i].prefix, logs[i].buf);
		gtk_label_set_markup(GTK_LABEL(views[i]), task_markup);
	}
	for (int i=nlogs; i < MAX_RECORDS; i++) { gtk_widget_set_visible(views[i], FALSE); }
    
    return TRUE;
}

void dumplog_handler(GtkButton* _button, GdkEvent* _event, void* _data) {
    struct _log_record logs[MAX_RECORDS] = { {0} };
    int nlogs = fetch_n_logs(MAX_RECORDS, logs);
    
    size_t logbuf_size = 200*nlogs;
    char* logbuf = (char*) malloc(logbuf_size); memset(logbuf, 0, logbuf_size);
    char* logbuf_wp = logbuf + snprintf(logbuf, 10, "echo -e \'");
    for (int i=0; i < nlogs; i++) { logbuf_wp += snprintf(logbuf_wp, logbuf_size-(logbuf_wp - logbuf) - 34, "\x1b[1m\x1b[38;5;%dm %s \x1b[0m %s\n", logs[i].pri_color, logs[i].prefix, logs[i].buf); }
    for (int i=10; i < (int) (logbuf_wp - logbuf); i++) {
        if (logbuf[i] == '\'') { logbuf[i] = '`'; }
    }
    snprintf(logbuf_wp, 34, "\' | nc %s %ld", getenv("LOG_IP"), atol(getenv("LOG_PORT")));
    popen(logbuf, "r");
    free(logbuf);
}

GtkWidget* log_widget() {
	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);
	
	GtkWidget* log_hbox = gtk_hbox_new(false, 0);
	GtkWidget* title = gtk_label_new("logs");
	PangoFontDescription* font_title = pango_font_description_from_string(FONT_BOLD_16);
	gtk_widget_modify_font(title, font_title);
	pango_font_description_free(font_title);
	gtk_box_pack_start(GTK_BOX(log_hbox), title, FALSE, FALSE, 10*SCALE);
	gtk_box_pack_start(GTK_BOX(wrapper), log_hbox, FALSE, FALSE, 10*SCALE);

	GtkWidget** data = (GtkWidget**) malloc(MAX_RECORDS * sizeof(GtkWidget*));
	GtkWidget* list_vbox = gtk_vbox_new(FALSE, 0);
	PangoFontDescription* font_desc = pango_font_description_from_string(FONT_10);
	for (int i=0; i < MAX_RECORDS; i++) {
	    GtkWidget* log_hbox = gtk_hbox_new(false, 0);
	    data[i] = gtk_label_new("");
		gtk_widget_modify_font(data[i], font_desc);
		gtk_box_pack_start(GTK_BOX(log_hbox), data[i], FALSE, FALSE, 15*SCALE);
		gtk_box_pack_start(GTK_BOX(list_vbox), log_hbox, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(wrapper), list_vbox, TRUE, TRUE, 5*SCALE);
	pango_font_description_free(font_desc);

	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) _update_log_display, (void*) data);
	return wrapper;
}
