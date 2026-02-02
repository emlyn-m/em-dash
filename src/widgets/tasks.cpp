#include "./widgets.hpp"
#include "gtk/gtk.h"
#include <cstdlib>


gboolean tasks_update(void* data_p) {
	task_t* data = (task_t*) data_p;

	for (uint32_t i=0; i < data->num_tasks; i++) {
		gtk_label_set_text(GTK_LABEL(data->task_checkboxes[i]), data->tasks[i]->completed ? "■" : "□");
		gtk_label_set_text(GTK_LABEL(data->task_widgets[i]), data->tasks[i]->task);
	}

	return TRUE;
}

struct task_onclick_data { task_t* task; uint32_t idx; };
void update_task(GtkButton* button, GdkEvent* event, void* data_vp) {
    struct task_onclick_data* data = (struct task_onclick_data*) data_vp;
    data->task->tasks[data->idx]->completed = 1-(data->task->tasks[data->idx]->completed);
    tasks_update(data->task);

    gtk_container_get_children(GTK_CONTAINER(button));
}


GtkWidget* tasks_widget() {

	task_t* task_data = (task_t*) malloc(sizeof(task_t));
	task_data->num_tasks = 5;
	char* t0 = (char*) malloc(3 * sizeof(char)); t0[0]='t';t0[1]='0';t0[2]='\0';
	char* t1 = (char*) malloc(3 * sizeof(char)); t1[0]='t';t1[1]='1';t1[2]='\0';
	char* t2 = (char*) malloc(3 * sizeof(char)); t2[0]='t';t2[1]='2';t2[2]='\0';
	char* t3 = (char*) malloc(3 * sizeof(char)); t3[0]='t';t3[1]='3';t3[2]='\0';
	char* t4 = (char*) malloc(3 * sizeof(char)); t4[0]='t';t4[1]='4';t4[2]='\0';
	task_data->tasks = (task_ev_t**) malloc(task_data->num_tasks * sizeof(task_ev_t*));

	task_data->tasks[0] = (task_ev_t*) malloc(sizeof(task_ev_t));
	task_data->tasks[1] = (task_ev_t*) malloc(sizeof(task_ev_t));
	task_data->tasks[2] = (task_ev_t*) malloc(sizeof(task_ev_t));
	task_data->tasks[3] = (task_ev_t*) malloc(sizeof(task_ev_t));
	task_data->tasks[4] = (task_ev_t*) malloc(sizeof(task_ev_t));

	task_data->tasks[0]->completed = TRUE;
	task_data->tasks[1]->completed = FALSE;
	task_data->tasks[2]->completed = TRUE;
	task_data->tasks[3]->completed = FALSE;
	task_data->tasks[4]->completed = FALSE;

	task_data->tasks[0]->task=t0;
	task_data->tasks[1]->task=t1;
	task_data->tasks[2]->task=t2;
	task_data->tasks[4]->task=t4;
	task_data->tasks[3]->task=t3;

	task_data->task_checkboxes = (GtkWidget**) malloc(task_data->num_tasks * sizeof(GtkWidget*));
	task_data->task_widgets = (GtkWidget**) malloc(task_data->num_tasks * sizeof(GtkWidget*));

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
	PangoFontDescription* font_task = pango_font_description_from_string(FONT_12);
	PangoFontDescription* font_checkbox = pango_font_description_from_string(FONT_30);

	for (uint32_t i=0; i < task_data->num_tasks; i++) {
		GtkWidget* task_hbox = gtk_hbox_new(FALSE, 0);

		GtkWidget* checkbox_ev_box = gtk_event_box_new();
		gtk_widget_add_events(checkbox_ev_box, GDK_BUTTON_PRESS_MASK);
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(checkbox_ev_box), FALSE);
		GTK_WIDGET_UNSET_FLAGS(GTK_EVENT_BOX(checkbox_ev_box), GTK_CAN_FOCUS);

		struct task_onclick_data* ocdata = (struct task_onclick_data*) malloc(sizeof(struct task_onclick_data));
		ocdata->task = task_data;
		ocdata->idx = i;

		g_signal_connect(checkbox_ev_box, "button-press-event", G_CALLBACK(update_task), (void*) ocdata);
		GtkWidget* checkbox = gtk_label_new("");
		gtk_container_add(GTK_CONTAINER(checkbox_ev_box), checkbox);

		GtkWidget* label = gtk_label_new("");

		task_data->task_checkboxes[i] = checkbox;
		task_data->task_widgets[i] = label;
		gtk_widget_modify_font(checkbox, font_checkbox);
		gtk_widget_modify_font(label, font_task);

		gtk_box_pack_start(GTK_BOX(task_hbox), checkbox_ev_box, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(task_hbox), label, FALSE, FALSE, 10*SCALE);
		gtk_box_pack_start(GTK_BOX(tasks_vbox), task_hbox, FALSE, FALSE, 0);
	}

	gtk_box_pack_start(GTK_BOX(wrapper), tasks_vbox, FALSE, FALSE, 10*SCALE);
	pango_font_description_free(font_task);
	pango_font_description_free(font_checkbox);
	gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20*SCALE);
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) tasks_update, task_data);

	return wrapper;
}
