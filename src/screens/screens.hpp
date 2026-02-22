#pragma once

#include <gtk-2.0/gtk/gtk.h>

#define N_SCREENS 3
#define SCREEN_IDX_LIFE 0
#define SCREEN_IDX_CTRL 1
#define SCREEN_IDX_LED1 2

typedef struct set_screen_data {
	GtkWidget* stack;
	guint target_screen_idx;
} set_screen_data_t;

void set_screen(GtkButton* _button, GdkEvent* _event, void* data_v);

GtkWidget* generate_ctrl_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) );
GtkWidget* generate_life_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) );
GtkWidget* generate_led_strip_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) );
GtkWidget* generate_lamp_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) );
