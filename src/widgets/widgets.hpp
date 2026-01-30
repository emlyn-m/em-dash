#pragma once

#include <cstdint>
#include <gtk-2.0/gtk/gtk.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define SCALE 1

#define FONT_8  "Geist Mono, 4"
#define FONT_10 "Geist Mono, 5"
#define FONT_12 "Geist Mono, 6"
#define FONT_14 "Geist Mono, 7"
#define FONT_16 "Geist Mono, 8"
#define FONT_18 "Geist Mono, 9"
#define FONT_20 "Geist Mono, 10"
#define FONT_30 "Geist Mono, 15"
#define FONT_BOLD_8  "Geist Mono, Bold 4" 
#define FONT_BOLD_10 "Geist Mono, Bold 5" 
#define FONT_BOLD_12 "Geist Mono, Bold 6" 
#define FONT_BOLD_14 "Geist Mono, Bold 7" 
#define FONT_BOLD_16 "Geist Mono, Bold 8" 
#define FONT_BOLD_18 "Geist Mono, Bold 9" 
#define FONT_BOLD_20 "Geist Mono, Bold 10" 
#define FONT_BOLD_30 "Geist Mono, Bold 15" 


typedef struct ClockData {
	GtkWidget* month_widget;
	GtkWidget* day_widget;
	GtkWidget* time_widget;
	GtkWidget* ampm_widget;
} clock_data_t;
typedef struct DeviceData {
	char* name;
	gboolean online;
} device_t;
typedef struct ServiceData {
	char* name;
	char* status;
} service_t;
typedef struct TelemetryData {
	guint8 n_devices;
	guint8 n_services;
	device_t** devices;
	service_t** services;
	gboolean charging;
	unsigned int battery;
	unsigned long* ping_logs;
	int ping_offset;
	unsigned int num_pings;
	unsigned int max_pings;
	double jitter;
	char* ip;
	
	GtkWidget** device_widgets;
	GtkWidget** service_name_widgets;
	GtkWidget** service_status_widgets;
	GtkWidget* battery_label;
	GtkWidget* ping_label;
	GtkWidget* jitter_label;
	GtkWidget* ip_label;
} telem_t;

#define MAX_CAL_EVENTS 20
#define SHOW_CAL_EVENTS 4

typedef struct CalendarEvent {
	uint id;
	time_t start_time;
	time_t end_time;
	char* title;
} cal_event_t;
typedef struct CalendarData {
	uint32_t num_events;
	uint32_t show_events;
	cal_event_t** events;
	cal_event_t* active_event;
	
	char* token_buf;
	uint64_t token_exp;
	time_t last_updated;
	uint32_t update_frequency;
	
	GtkWidget* title_widget;
	GtkWidget* title_date_widget;
	GtkWidget* title_day_widget;
	
	GtkWidget* active_event_widget;
	GtkWidget* active_event_title_widget;
	GtkWidget* active_event_duration_widget;
	
	GtkWidget** events_title_widgets;
	GtkWidget** events_time_widgets;
} calendar_t;

typedef struct WeatherEvent {
	time_t time;
	double rain_prob;
	double temp_c;
	int wmo_code;
	
	GtkWidget* widget_time;
	GtkWidget* widget_temp;
	GtkWidget* widget_icon;
} weather_ev_t;
typedef struct WeatherData {
	uint32_t num_weather_events;
	weather_ev_t** events;
	
	time_t last_update;
	time_t update_freq;
	gboolean icons_changed;
} weather_t;

typedef struct AlertEvent {
	uint64_t time;
	char* category;
	uint8_t severity;
	char* msg;
} alert_ev_t;
typedef struct AlertData {
	uint32_t num_alerts;
	alert_ev_t** alerts;
	GtkWidget** alert_meta_widgets;
	GtkWidget** alert_widgets;
} alert_t;

typedef struct Task {
	char* task;
	gboolean completed;
} task_ev_t;
typedef struct TaskData {
	uint32_t num_tasks;
	task_ev_t** tasks;
	GtkWidget** task_checkboxes;
	GtkWidget** task_widgets;
} task_t;
typedef struct {
    GtkWidget *drawing_area;
    float value;      // 0.0 to 1.0
    gboolean dragging;
    void (*callback_change)(float);
    void (*callback_release)(float);
} KindleSlider;


GtkWidget* time_widget();
GtkWidget* telem_widget(double update_freq_ms);
GtkWidget* button_widget(char* label_content, void (*callback)(GtkButton*, GdkEvent*, void*), void* data);
GtkWidget* weather_widget();
GtkWidget* calendar_widget();
GtkWidget* alerts_widget();
GtkWidget* tasks_widget();
GtkWidget* label_widget(char* label_content);
GtkWidget* image_widget(GtkWidget** image_ref);
void set_image_src(GtkImage* image, const guint8* icon_dat, int target_width, int target_height);
KindleSlider* kindle_slider_new(void (*callback_change)(float), void (*callback_release)(float));
GtkWidget* model_init();
