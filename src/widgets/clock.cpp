#include "./widgets.hpp"

#include "gtk/gtk.h"
#include "pango/pango-font.h"
#include <cstdlib>
#include <cstring>
#include <ctime>

gboolean clock_update(void* data_p) {

	clock_data_t* data = (clock_data_t*) data_p;
	const char* MONTHS[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };

	const time_t t = time(NULL);
	tm tm_struct = *localtime(&t);

	int day = tm_struct.tm_mday;
	char day_buf[32]; memset(day_buf, '\0', 32);
	snprintf(day_buf, 32, "%d", day);
	const char* month = MONTHS[tm_struct.tm_mon];

	int hours = tm_struct.tm_hour % 12;
	int mins = tm_struct.tm_min;
	char time_buf[32]; memset(time_buf, '\0', 32);
	snprintf(time_buf, 32, "%d:%02d", hours, mins);

	char ampm_buf[3]; memset(ampm_buf, '\0', 3);
	if (tm_struct.tm_hour > 12) { snprintf(ampm_buf, 3, "am");
	} else { snprintf(ampm_buf, 3, "pm"); }


	gtk_label_set_text(GTK_LABEL(data->month_widget), month);
	gtk_label_set_text(GTK_LABEL(data->day_widget), day_buf);
	gtk_label_set_text(GTK_LABEL(data->time_widget), time_buf);
	gtk_label_set_text(GTK_LABEL(data->ampm_widget), ampm_buf);

	return TRUE;
}

GtkWidget* time_widget() {

	clock_data_t* data = (clock_data_t*) malloc(sizeof(clock_data_t));

	// wrapper
	PangoFontDescription* font_desc_month = pango_font_description_from_string(FONT_12);
	PangoFontDescription* font_desc_day = pango_font_description_from_string(FONT_20);
	PangoFontDescription* font_desc_ampm = pango_font_description_from_string(FONT_12);
	PangoFontDescription* font_desc_time = pango_font_description_from_string(FONT_16);

	GtkWidget* wrapper = gtk_hbox_new(FALSE, 0);
	GtkWidget* date_align_top = gtk_alignment_new(0.5, 0, 0.0, 0.0);
	GtkWidget* date_align_bot = gtk_alignment_new(0.5, 1, 0.0, 0.0);

	GtkWidget* date = gtk_vbox_new(FALSE, 0);

	GtkWidget* month = gtk_label_new("");
	data->month_widget = month;
	GtkWidget* day = gtk_label_new("");
	data->day_widget = day;
	gtk_widget_modify_font(month, font_desc_month);
	gtk_widget_modify_font(day, font_desc_day);
	gtk_container_add(GTK_CONTAINER(date_align_bot), month);
	gtk_container_add(GTK_CONTAINER(date_align_top), day);
	gtk_box_pack_start(GTK_BOX(date), date_align_bot, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(date), date_align_top, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(wrapper), date, TRUE, TRUE, 0);

	GtkWidget* sep = gtk_vseparator_new();
	GtkWidget* sep_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(sep_box), 20*SCALE);
	gtk_box_pack_start(GTK_BOX(sep_box), sep, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(wrapper), sep_box, FALSE, FALSE, 0);

	GtkWidget* time_box = gtk_vbox_new(FALSE, 0);

	GtkWidget* alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0); // Center the child, do not scale
	gtk_container_add(GTK_CONTAINER(alignment), time_box);

	GtkWidget* time = gtk_label_new("");
	GtkWidget* ampm = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(time_box), time, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(time_box), ampm, FALSE, FALSE, 0);
	data->time_widget = time;
	data->ampm_widget = ampm;
	gtk_widget_modify_font(time, font_desc_time);
	gtk_widget_modify_font(ampm, font_desc_ampm);
	gtk_box_pack_end(GTK_BOX(wrapper), alignment, FALSE, FALSE, 0);

	pango_font_description_free(font_desc_time);
	pango_font_description_free(font_desc_day);
	pango_font_description_free(font_desc_month);
	pango_font_description_free(font_desc_ampm);

	gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20);
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) clock_update, data);


	return wrapper;
}
