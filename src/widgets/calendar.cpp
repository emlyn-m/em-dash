#include "./widgets.hpp"
#include "../net/net.hpp"
#include "gtk/gtk.h"
#include <algorithm>
#include <gtk-2.0/gtk/gtk.h>

#include <chrono>
#include "pango/pango-font.h"
#include <cstdlib>
#include <cstring>

gboolean title_update(gpointer* data_p) {
	const char* DAYS[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
	const char* MONTHS[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
	calendar_t* data = (calendar_t*) data_p;

 	const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_struct = *std::localtime(&t);

    char* message_buf = (char*) malloc(64 * sizeof(char));
	if (data->active_event != NULL) {
		snprintf(message_buf, 64, "%s, %s %d", DAYS[tm_struct.tm_wday], MONTHS[tm_struct.tm_mon], tm_struct.tm_mday);
		gtk_label_set_text(GTK_LABEL(data->title_day_widget), message_buf);
		if (data->title_date_widget != NULL) {
			gtk_container_remove(GTK_CONTAINER(data->title_widget), data->title_date_widget);
			data->title_date_widget = NULL;
		}
	} else {
		if (data->title_date_widget == NULL) {
			PangoFontDescription* font_desc_date = pango_font_description_from_string(FONT_BOLD_20);
			data->title_date_widget = gtk_label_new("date");
			gtk_widget_modify_font(data->title_date_widget, font_desc_date);
			gtk_misc_set_alignment (GTK_MISC(data->title_date_widget), 0.0, 0.0);

			pango_font_description_free(font_desc_date);
			gtk_box_pack_start(GTK_BOX(data->title_widget), data->title_date_widget, FALSE, FALSE, 0);
			gtk_widget_show(data->title_date_widget);
		}

		snprintf(message_buf, 64, "%d %s", tm_struct.tm_mday, MONTHS[tm_struct.tm_mon]);
		gtk_label_set_text(GTK_LABEL(data->title_day_widget), DAYS[tm_struct.tm_wday]);
		gtk_label_set_text(GTK_LABEL(data->title_date_widget), message_buf);
	}

	return TRUE;
}

gboolean active_event_update(gpointer* data_p) {


	calendar_t* data = (calendar_t*) data_p;

	time_t most_recent_start = 0;
	time_t ctime = time(NULL);
	cal_event_t* active_event = NULL;
	for (uint i=0; i < data->num_events; i++) {

	    if ((data->events[i]->start_time < ctime) && (data->events[i]->end_time > ctime) && (data->events[i]->start_time > most_recent_start)) {
			active_event = data->events[i];
			most_recent_start = data->events[i]->start_time;
		}
	}
	data->active_event = active_event;


	if (data->active_event == NULL) {  return TRUE;  }

	std::tm tm_start = *std::localtime((time_t*) &data->active_event->start_time);
	std::tm tm_end = *std::localtime((time_t*) &data->active_event->end_time);
	char* time_buf = (char*) malloc(64 * sizeof(char));
	memset(time_buf, '\0', 64);
   	snprintf(
  		time_buf, 64, "%d:%02d %s - %d:%02d %s",
       	tm_start.tm_hour % 12, tm_start.tm_min, tm_start.tm_hour < 12 ? (char*) "am" : (char*) "pm",
       	tm_end.tm_hour % 12, tm_end.tm_min, tm_end.tm_hour < 12 ? (char*) "am" : (char*) "pm"
   	);


    gtk_label_set_text(GTK_LABEL(data->active_event_title_widget), data->active_event->title);
    gtk_label_set_text(GTK_LABEL(data->active_event_duration_widget), time_buf);

	return TRUE;
}

gboolean pending_events_update(gpointer* data_p) {

	calendar_t* data = (calendar_t*) data_p;
	const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);


    int32_t show_offset;
    if (data->num_events <= data->show_events) {
    	show_offset = 0;
    } else {
	    int32_t current_event_idx = 0;
	   	for (uint32_t i=0; i < data->num_events; i++) {
	    	if (data->events[i] == data->active_event) {
	     		current_event_idx = i;
	       		break;
	     	}
	  		if (data->events[i]->end_time > t && ((current_event_idx < 0) || (data->events[i]->start_time > data->events[current_event_idx]->start_time))) {
     			current_event_idx = i;
	  		}
	    }

	    int32_t low = current_event_idx - 1;
	    int32_t high = current_event_idx + data->show_events - 2;
	    if (low < 0) {
			show_offset = 0;
		} else if (high >= (int) data->num_events) {
			show_offset = low - (high - data->num_events + 1);
		} else { show_offset = low; }
    }

   	PangoFontDescription* font_desc_event = pango_font_description_from_string(FONT_12);
	PangoFontDescription* font_desc_event_active = pango_font_description_from_string(FONT_BOLD_12);

   	char* time_buf = (char*) malloc(128 * sizeof(char));
    const uint32_t widget_offset = std::max((uint32_t) 0, data->show_events - data->num_events);
	for (uint32_t i=0; i < std::min(data->show_events, data->num_events); i++) {

		cal_event_t* event = (data->events)[i+show_offset];
  		std::tm tm_start = *std::localtime((time_t*) &(event->start_time));
  		std::tm tm_end = *std::localtime((time_t*) &(event->end_time));

    	memset(time_buf, '\0', 128);
     	snprintf(
      		time_buf, 127, "%d:%02d %s - %d:%02d %s",
        	tm_start.tm_hour % 12, tm_start.tm_min, tm_start.tm_hour < 12 ? (char*) "am" : (char*) "pm",
         	tm_end.tm_hour % 12, tm_end.tm_min, tm_end.tm_hour < 12 ? (char*) "am" : (char*) "pm"
      	);

		gtk_label_set_text(GTK_LABEL(data->events_time_widgets[i+widget_offset]), time_buf);
		char event_title_buf[11]; memcpy(event_title_buf, event->title, 7);
		event_title_buf[7] = '.'; event_title_buf[8] = '.'; event_title_buf[9] = '.';  event_title_buf[10] = 0;
		gtk_label_set_text(GTK_LABEL(data->events_title_widgets[i+widget_offset]), event_title_buf);

		if (event == data->active_event) {
			gtk_widget_modify_font(data->events_time_widgets[i+widget_offset], font_desc_event_active);
			gtk_widget_modify_font(data->events_title_widgets[i+widget_offset], font_desc_event_active);
		} else {
			gtk_widget_modify_font(data->events_time_widgets[i+widget_offset], font_desc_event);
			gtk_widget_modify_font(data->events_title_widgets[i+widget_offset], font_desc_event);
		}
	}
	free(time_buf);
	pango_font_description_free(font_desc_event);
	pango_font_description_free(font_desc_event_active);


	return TRUE;
}

GtkWidget* calendar_widget() {

	calendar_t* calendar_data = (calendar_t*) malloc(sizeof(calendar_t));
	calendar_data->show_events = SHOW_CAL_EVENTS;
	calendar_data->num_events = 0;
	calendar_data->events = (cal_event_t**) malloc(MAX_CAL_EVENTS * sizeof(cal_event_t*));
	for (int i=0; i < MAX_CAL_EVENTS; i++) { calendar_data->events[i] = NULL; }
	calendar_data->active_event = NULL;
	calendar_data->token_buf = NULL;
	calendar_data->token_exp = 0;
	calendar_data->update_frequency = 300000;  // 5 minutes
	calendar_data->last_updated = 0;

	// wrapper
	GtkWidget* wrapper = gtk_vbox_new(FALSE, 30*SCALE);

	// title
	calendar_data->title_widget = gtk_vbox_new(FALSE, 0);
	PangoFontDescription* font_desc_day = pango_font_description_from_string(FONT_BOLD_12);

	calendar_data->title_day_widget = gtk_label_new("day");
	calendar_data->title_date_widget = NULL;
	gtk_widget_modify_font(calendar_data->title_day_widget, font_desc_day);
	gtk_misc_set_alignment (GTK_MISC(calendar_data->title_day_widget), 0.0, 0.0);

	pango_font_description_free(font_desc_day);
	gtk_box_pack_start(GTK_BOX(calendar_data->title_widget), calendar_data->title_day_widget, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(wrapper), calendar_data->title_widget, FALSE, FALSE, 0);


	// active event name
	calendar_data->active_event_widget = gtk_vbox_new(FALSE, 0);

	PangoFontDescription* font_desc_title = pango_font_description_from_string(FONT_BOLD_20);
	PangoFontDescription* font_desc_duration = pango_font_description_from_string(FONT_BOLD_12);
	calendar_data->active_event_title_widget = gtk_label_new("");
	calendar_data->active_event_duration_widget = gtk_label_new("");

	gtk_widget_modify_font(calendar_data->active_event_title_widget, font_desc_title);
	gtk_widget_modify_font(calendar_data->active_event_duration_widget, font_desc_duration);
	gtk_misc_set_alignment (GTK_MISC(calendar_data->active_event_title_widget), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC(calendar_data->active_event_duration_widget), 0.0, 0.5);
	pango_font_description_free(font_desc_title);
	pango_font_description_free(font_desc_duration);

	gtk_box_pack_start(GTK_BOX(calendar_data->active_event_widget), calendar_data->active_event_title_widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(calendar_data->active_event_widget), calendar_data->active_event_duration_widget, FALSE, FALSE, 0);


	// pending events
	GtkWidget* pending_events_vbox = gtk_vbox_new(FALSE, 0);
	PangoFontDescription* font_desc_event = pango_font_description_from_string(FONT_12);
	PangoFontDescription* font_desc_event_active = pango_font_description_from_string(FONT_BOLD_12);

	calendar_data->events_title_widgets = (GtkWidget**) malloc(calendar_data->show_events * sizeof(GtkWidget*));
	calendar_data->events_time_widgets = (GtkWidget**) malloc(calendar_data->show_events * sizeof(GtkWidget*));

	for (uint32_t i=0; i < calendar_data->show_events; i++) {
		GtkWidget* event_hbox = gtk_hbox_new(FALSE, 0);
		calendar_data->events_title_widgets[i] = gtk_label_new("");
		calendar_data->events_time_widgets[i] = gtk_label_new("");
		if (( calendar_data->events[i] ) == calendar_data->active_event) {
			gtk_widget_modify_font(calendar_data->events_title_widgets[i], font_desc_event_active);
			gtk_widget_modify_font(calendar_data->events_time_widgets[i], font_desc_event_active);
		} else {
			gtk_widget_modify_font(calendar_data->events_title_widgets[i], font_desc_event);
			gtk_widget_modify_font(calendar_data->events_time_widgets[i], font_desc_event);
		}
		gtk_box_pack_start(GTK_BOX(event_hbox), calendar_data->events_title_widgets[i], FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(event_hbox), calendar_data->events_time_widgets[i], FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(pending_events_vbox), event_hbox, FALSE, FALSE, 2);
	}
	gtk_box_pack_start(GTK_BOX(wrapper), calendar_data->active_event_widget, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(wrapper), pending_events_vbox, FALSE, FALSE, 0);
	pango_font_description_free(font_desc_event);
	pango_font_description_free(font_desc_event_active);

	g_timeout_add(getenv(CALENDAR_UPDATE_FREQUENCY), (GSourceFunc) update_events,         calendar_data);   // todo: find some way to trigger this once immediately, then again every idk however long - should probs store as a field in cal_data (alternatively we should call the callback over and over but return immediately 99/100 times lmao (based on time not rng obv - wow im tired))
	g_timeout_add(getenv(UI_UPDATE_FREQUENCY), (GSourceFunc) title_update,          calendar_data);
	g_timeout_add(getenv(UI_UPDATE_FREQUENCY), (GSourceFunc) active_event_update,   calendar_data);
	g_timeout_add(getenv(UI_UPDATE_FREQUENCY), (GSourceFunc) pending_events_update, calendar_data);
	gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20*SCALE);
	return wrapper;
}
