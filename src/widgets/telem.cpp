#include "./widgets.hpp"
#include "../net/net.hpp"

#include "gtk/gtk.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

gboolean devices_update(gpointer* data_p) {
	telem_t* data = (telem_t*) data_p;
	
	// for (uint32_t i=0; i < data->n_devices; i++) {
	// 	char* device_buf = (char*) malloc(64 * sizeof(char));
	// 	snprintf(device_buf, 30, data->devices[i]->online ? "◉ %s" : "○ %s", data->devices[i]->name);
	// 	gtk_label_set_text(GTK_LABEL(data->device_widgets[i]), device_buf);
	// 	free(device_buf);
	// }
	
	return TRUE;
}

gboolean ip_update(gpointer* data_p) { 
	telem_t* data = (telem_t*) data_p;
	
	time_t duration;
	http_get((char*) "ipinfo.io", (char*) "ip", 80, &(data->ip), &duration);
	gtk_label_set_text(GTK_LABEL(data->ip_label), data->ip);

	data->num_pings = MIN(data->num_pings+1, data->max_pings);
	data->ping_offset = (data->ping_offset + 1) % data->num_pings;
	
	data->ping_logs[data->ping_offset] = duration;
	if (data->num_pings >= 2) {
	    data->jitter += abs((float) (data->ping_logs[data->ping_offset]) - data->ping_logs[(data->ping_offset - 1) % data->num_pings]);

		char jitter_buf[32];
		snprintf(jitter_buf, 31, "%dms", (int) data->jitter);
		gtk_label_set_text(GTK_LABEL(data->jitter_label), jitter_buf);

	}
	
	char ping_buf[32];
	snprintf(ping_buf, 31, "%ldms", data->ping_logs[data->num_pings - 1]);
	gtk_label_set_text(GTK_LABEL(data->ping_label), ping_buf);
	
   	return TRUE;
}

GtkWidget* telem_widget(double update_freq_ms) {
	
	// wrapper
	telem_t* data = (telem_t*) malloc(sizeof(telem_t));
	data->n_devices = 0;
	data->device_widgets = (GtkWidget**) malloc(data->n_devices * sizeof(GtkWidget*));
	data->devices = (device_t**) malloc(data->n_devices * sizeof(device_t*));
	data->n_services = 0;
	data->service_name_widgets = (GtkWidget**) malloc(data->n_devices * sizeof(GtkWidget*));
	data->service_status_widgets = (GtkWidget**) malloc(data->n_devices * sizeof(GtkWidget*));
	data->services = (service_t**) malloc(data->n_services * sizeof(service_t*));
	data->max_pings = 20;
	data->num_pings = 0;
	data->ping_logs = (unsigned long*) malloc(data->max_pings * sizeof(unsigned long));
	memset(data->ping_logs, 0, data->max_pings * sizeof(unsigned long));
	data->jitter = 0;
	data->ip = (char*) malloc(30 * sizeof(char));
	memset(data->ip, 0, 30);

	GtkWidget* wrapper = gtk_vbox_new(FALSE, 5*SCALE);
	
	
	// telem label
	GtkWidget* telem_label = gtk_label_new("Telemetry");
	PangoFontDescription* font_desc_telem = pango_font_description_from_string(FONT_BOLD_14);
	gtk_misc_set_alignment (GTK_MISC(telem_label), 0.0, 0.5);
	gtk_widget_modify_font(telem_label, font_desc_telem);
	pango_font_description_free(font_desc_telem);
	gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20*SCALE);
	gtk_box_pack_start(GTK_BOX(wrapper), telem_label, FALSE, FALSE, 0);

	GtkWidget* telem_info = gtk_vbox_new(false, 0);
	
	
	// devices
	GtkWidget* device_block = gtk_vbox_new(false, 5*SCALE);
	PangoFontDescription* font_desc_dev = pango_font_description_from_string(FONT_12);
	
	for (uint32_t i=0; i < data->n_devices; i++) {
		data->device_widgets[i] = gtk_label_new("");;
		gtk_misc_set_alignment (GTK_MISC(data->device_widgets[i]), 0.0, 0.5);
		gtk_widget_modify_font(data->device_widgets[i], font_desc_dev);
		gtk_box_pack_start(GTK_BOX(device_block), data->device_widgets[i], FALSE, FALSE, 5*SCALE);
	}
	
	pango_font_description_free(font_desc_dev);
	gtk_box_pack_start(GTK_BOX(telem_info), device_block, FALSE, FALSE, 5*SCALE);
	gtk_box_pack_start(GTK_BOX(wrapper), telem_info, TRUE, TRUE, 5*SCALE);
	g_timeout_add(update_freq_ms, (GSourceFunc) devices_update, data);


	
	// telem_stats
	PangoFontDescription* font_desc_label = pango_font_description_from_string(FONT_10);
	GtkWidget* telem_stats = gtk_vbox_new(FALSE, 0);
	
	// ping label
	GtkWidget* ping_info = gtk_hbox_new(false, 0);
	GtkWidget* ping_label = gtk_label_new("ping");
	GtkWidget* ping_value = gtk_label_new("");
	data->ping_label = ping_value;
	gtk_widget_modify_font(ping_label, font_desc_label);
	gtk_widget_modify_font(ping_value, font_desc_label);
	gtk_box_pack_start(GTK_BOX(ping_info), ping_label, FALSE, FALSE, 5*SCALE);
	gtk_box_pack_end(GTK_BOX(ping_info), ping_value, FALSE, FALSE, 5*SCALE);
	gtk_box_pack_start(GTK_BOX(telem_stats), ping_info, FALSE, FALSE, 0);
	
	// jitter label
	GtkWidget* jitter_info = gtk_hbox_new(false, 0);
	GtkWidget* jitter_label = gtk_label_new("jitter");
	GtkWidget* jitter_value = gtk_label_new("");
	data->jitter = 0;
	data->jitter_label = jitter_value;
	gtk_widget_modify_font(jitter_label, font_desc_label);
	gtk_widget_modify_font(jitter_value, font_desc_label);
	gtk_box_pack_start(GTK_BOX(jitter_info), jitter_label, FALSE, FALSE, 5*SCALE);
	gtk_box_pack_end(GTK_BOX(jitter_info), jitter_value, FALSE, FALSE, 5*SCALE);
	gtk_box_pack_start(GTK_BOX(telem_stats), jitter_info, FALSE, FALSE, 0);

	// ip label
	GtkWidget* ip_info = gtk_hbox_new(false, 0);
	GtkWidget* ip_value = gtk_label_new("");
	data->ip_label = ip_value;
	gtk_widget_modify_font(ip_value, font_desc_label);
	gtk_misc_set_alignment(GTK_MISC(ip_value), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(ip_info), ip_value, TRUE, TRUE, 5*SCALE);
	gtk_box_pack_start(GTK_BOX(telem_stats), ip_info, TRUE, TRUE, 0);
	
	g_timeout_add(update_freq_ms, (GSourceFunc) ip_update, data);

	gtk_box_pack_start(GTK_BOX(wrapper), telem_stats, FALSE, FALSE, 5);
	pango_font_description_free(font_desc_label);
	
	return wrapper;
}
