#include "src/log.hpp"
#include "src/widgets/widgets.hpp"

#include "gtk/gtk.h"
#include <cstdlib>
#include <cstring>


gboolean tasks_update(void* data_p) {
	task_t* data = (task_t*) data_p;

	for (int i=0; i < data->num_tasks; i++) {
	    gtk_widget_set_visible(data->task_widgets[i], TRUE);
	    char* task_markup = (char*) malloc(256); memset(task_markup, 0, 256);
		snprintf(task_markup, 256, data->tasks[i]->completed ? "<s>%s</s>" : "%s", data->tasks[i]->task);
		gtk_label_set_markup(GTK_LABEL(data->task_widgets[i]), task_markup);
	}
	for (int i=data->num_tasks; i < data->max_tasks; i++) { gtk_widget_set_visible(data->task_widgets[i], FALSE); }

	return TRUE;
}

struct task_onclick_data { task_t* task; int idx; };
void update_task(GtkButton* button, GdkEvent* event, void* data_vp) {
	struct task_onclick_data* data = (struct task_onclick_data*) data_vp;
	data->task->tasks[data->idx]->completed = 1-(data->task->tasks[data->idx]->completed);
	tasks_update(data->task);
}

gboolean keyboard_onpress(kb_data_t* data) {
	char char_val = match_keycode(data->lookup_table, data->layer, data->x, data->y);
	if (char_val == '\n') {
		return FALSE; // newline - end of input, so exit keyboard
	}

	printf(LOGFMT("dressed char '%c' at coords %lf, %lf  layer %d\n", LOG_INF), char_val, data->x, data->y, data->layer);
	fflush(stdout);

	return TRUE;
}

void keyboard_onenter(kb_data_t* data) {
	fflush(stdout);
	
	int actual_buf_len = 0;
	for (int i=0; i < strlen(data->kb_buf) - 1; i++) {
	    if (data->kb_buf[i] >= 0x2e && data->kb_buf[i] <= 0x7e) { actual_buf_len++; }
	};
	
	if (actual_buf_len <= 0) { 
	    return;
	}
	
	task_t* task_data = (task_t*) data->add_data;
	if (task_data->num_tasks++ >= task_data->max_tasks) {
	    task_data->num_tasks = task_data->max_tasks;
		free(task_data->tasks[0]->task);
	    for (int i=0; i < task_data->max_tasks - 1; i++) {
			task_data->tasks[i]->task = task_data->tasks[i+1]->task;
			task_data->tasks[i]->completed = task_data->tasks[i+1]->completed;
		}
	}
	
	task_data->tasks[task_data->num_tasks-1]->completed = false;
	task_data->tasks[task_data->num_tasks-1]->task = (char*) malloc(actual_buf_len+1 * sizeof(char));
	memset(task_data->tasks[task_data->num_tasks-1]->task, 0, actual_buf_len+1);
	strncpy(task_data->tasks[task_data->num_tasks-1]->task, data->kb_buf, actual_buf_len);
}



GtkWidget* tasks_widget() {
	task_t* task_data = (task_t*) malloc(sizeof(task_t));
	task_data->num_tasks = 0;
	task_data->max_tasks = 7;
	task_data->tasks = (task_ev_t**) malloc(task_data->max_tasks * sizeof(task_ev_t*));
	for (int i=0; i < task_data->max_tasks; i++) { task_data->tasks[i] = (task_ev_t*) malloc(sizeof(task_ev_t)); memset(task_data->tasks[i], 0, sizeof(task_ev_t)); }

	task_data->task_widgets = (GtkWidget**) malloc(task_data->max_tasks * sizeof(GtkWidget*));

	// wrapper
	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);

	// title
	GtkWidget* tasks_title = gtk_label_new("Tasks");
	gtk_misc_set_alignment (GTK_MISC(tasks_title), 0.0, 0.5);
	PangoFontDescription* font_title = pango_font_description_from_string(FONT_BOLD_18);
	gtk_widget_modify_font(tasks_title, font_title);
	pango_font_description_free(font_title);
	gtk_box_pack_start(GTK_BOX(wrapper), tasks_title, FALSE, FALSE, 0);

	// tasks
	GtkWidget* tasks_vbox = gtk_vbox_new(FALSE, 0);
	PangoFontDescription* font_task = pango_font_description_from_string(FONT_16);

	for (int i=0; i < task_data->max_tasks; i++) {
		struct task_onclick_data* ocdata = (struct task_onclick_data*) malloc(sizeof(struct task_onclick_data));
		ocdata->task = task_data;
		ocdata->idx = i;

		GtkWidget* evbox = gtk_event_box_new();
		gtk_widget_add_events(evbox, GDK_BUTTON_PRESS_MASK);
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(evbox), FALSE);
		GTK_WIDGET_UNSET_FLAGS(GTK_EVENT_BOX(evbox), GTK_CAN_FOCUS);
		g_signal_connect(evbox, "button-press-event", G_CALLBACK(update_task), (void*) ocdata);

		GtkWidget* task_hbox = gtk_hbox_new(false, 0);
		GtkWidget* label = gtk_label_new("");
		task_data->task_widgets[i] = label;
		gtk_widget_modify_font(label, font_task);
		
		gtk_box_pack_start(GTK_BOX(task_hbox), label, FALSE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(evbox), task_hbox);
		gtk_box_pack_start(GTK_BOX(tasks_vbox), evbox, FALSE, FALSE, 10*SCALE);
	}
	pango_font_description_free(font_task);
	gtk_box_pack_start(GTK_BOX(wrapper), tasks_vbox, FALSE, FALSE, 10*SCALE);
	
	GtkWidget* keyboard_listener = keyboard_widget((void*) task_data, keyboard_onpress, keyboard_onenter);
	gtk_box_pack_start(GTK_BOX(wrapper), keyboard_listener, TRUE, TRUE, 10*SCALE);
	
	gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20*SCALE);
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) tasks_update, task_data);

	return wrapper;
}
