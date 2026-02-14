#include "./widgets.hpp"
#include "../net/net.hpp"
#include "../images/cloud.h"
#include "../images/rain.h"
#include "../images/sun.h"
#include "../images/thunder.h"


#include "gtk/gtk.h"
#include <cstdlib>

void generate_time(time_t time_d, char* res) {
	struct tm* time = localtime(&time_d);
	strftime(res, 10, (char*) "%-I %p", time);
}

gboolean wmo_compare(int code, const int codes[], int n_codes) {
	for (int i=0; i < n_codes; i++) {
	    if (code == codes[i]) { return true; }
	}
	return false;
}

gboolean update_weather_display(gpointer* weather_vp) {
	const int WMO_SUNNY[1] = { 0 };
	const int WMO_CLOUDY[3] = { 1, 2, 3 };
	const int WMO_RAINY[15] = { 45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82 };
	const int WMO_THUNDER[3] = { 95, 96, 99 };

	weather_t* weather_data = (weather_t*) weather_vp;
	for (uint32_t event_idx=0; event_idx < weather_data->num_weather_events; event_idx++) {
	    weather_ev_t* event = weather_data->events[event_idx];
	    char temp_s[11];
	    snprintf(temp_s, 11, "%.1lf°", event->temp_c);
	    gtk_label_set_text(GTK_LABEL(event->widget_temp), temp_s);

	    char time_s[10];
	    generate_time(event->time, time_s);
	    gtk_label_set_text(GTK_LABEL(event->widget_time), time_s);

	    const int icon_width = 30;
	    const int icon_height = 30;

	    if (wmo_compare(event->wmo_code, WMO_SUNNY, 1)) {
	        set_image_src(GTK_IMAGE(event->widget_icon), sun, icon_width, icon_height);
	    } else if (wmo_compare(event->wmo_code, WMO_CLOUDY, 3)) {
	        set_image_src(GTK_IMAGE(event->widget_icon), cloud, icon_width, icon_height);
	    } else if (wmo_compare(event->wmo_code, WMO_RAINY, 15)) {
	        set_image_src(GTK_IMAGE(event->widget_icon), rain, icon_width, icon_height);
	    } else if (wmo_compare(event->wmo_code, WMO_THUNDER, 3)) {
	        set_image_src(GTK_IMAGE(event->widget_icon), thunder, icon_width, icon_height);
	    }


	}
	return TRUE;
}

GtkWidget* weather_widget() {

	weather_t* weather = (weather_t*) malloc(sizeof(weather_t));
	weather->num_weather_events = 10;
	weather->last_update = 0;
	weather->update_freq = atol(getenv("WEATHER_UPDATE_FREQUENCY"));
	weather->events = (weather_ev_t**) malloc(weather->num_weather_events * sizeof(weather_ev_t*));
	for (uint32_t i=0; i < weather->num_weather_events; i++) {
		weather->events[i] = (weather_ev_t*) malloc(sizeof(weather_ev_t));
	}

	// wrapper
	GtkWidget* wrapper = gtk_hbox_new(FALSE, 30*SCALE);
	gtk_container_set_border_width(GTK_CONTAINER(wrapper), 10);
	PangoFontDescription* font_temp = pango_font_description_from_string(FONT_8);
	PangoFontDescription* font_time = pango_font_description_from_string(FONT_BOLD_8);

	for (uint32_t i=0; i < weather->num_weather_events; i++) {
		GtkWidget* event_vbox = gtk_vbox_new(FALSE, 0);
		GtkWidget* temp = gtk_label_new("");
		GtkWidget* time = gtk_label_new("");
		GtkWidget* icon = image_widget(&(weather->events[i]->widget_icon));

		gtk_widget_modify_font(temp, font_temp);
		gtk_widget_modify_font(time, font_time);
		gtk_misc_set_alignment(GTK_MISC(temp), 0.5, 0.0);
		gtk_misc_set_alignment(GTK_MISC(time), 0.5, 1.0);
		gtk_box_pack_start(GTK_BOX(event_vbox), temp, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(event_vbox), icon, TRUE, TRUE, 0);
		gtk_box_pack_end(GTK_BOX(event_vbox), time, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(wrapper), event_vbox, TRUE, TRUE, 0);

		(weather->events[i])->widget_temp = temp;
		(weather->events[i])->widget_time = time;
		weather->events[i]->time = 0;
		weather->events[i]->wmo_code= 0;
	}

	pango_font_description_free(font_temp);
	pango_font_description_free(font_time);

	g_timeout_add(atol(getenv("NET_UPDATE_FREQUENCY")), (GSourceFunc) update_weather, weather);
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")),	  (GSourceFunc) update_weather_display, weather);

	return wrapper;
}
