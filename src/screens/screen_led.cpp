#include "src/log.hpp"
#include "src/net/cJSON.h"
#include "src/net/tuya.hpp"
#include "src/images/hue.h"
#include "src/images/sat.h"
#include "src/images/val.h"
#include "src/screens/table.hpp"

#include "src/screens/screens.hpp"
#include "src/widgets/widgets.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdk.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include "glib.h"


void set_ctrl_handler(GtkButton* _button, GdkEvent* _event, void* data_v) {
    set_screen_data_t* data = (set_screen_data_t*) data_v;
    data->target_screen_idx = SCREEN_IDX_CTRL;
    set_screen(_button, _event, data_v);
}

struct _tuya_send_args { tuya_led_t* led; char* cmdbuf; };
void* _tuya_send_thread_helper(void* args_vp) { _tuya_send_args* args = (_tuya_send_args*) args_vp; tuya_cmd_send(args->led, COMMAND_CTRL, args->cmdbuf); free(args->cmdbuf); free(args); return NULL; }
void _tuya_color_set(tuya_led_t* led, uint32_t hue, uint32_t sat, uint32_t val) {
    char* cmdbuf = (char*) malloc(32 * sizeof(char)); memset(cmdbuf, 0, 32);
    led->hue = hue; led->sat = sat; led->val = val;
    snprintf(cmdbuf, 32, "{\"24\": \"%04x%04x%04x\"}", led->hue, led->sat, led->val);
    pthread_t _thread_id;
    struct _tuya_send_args* args = (struct _tuya_send_args*) malloc(sizeof(struct _tuya_send_args));
    args->led = led;
    args->cmdbuf = cmdbuf;
    pthread_create(&_thread_id, NULL, _tuya_send_thread_helper, args);
}

void led_power_callback(GtkButton* _button, GdkEvent* _event, void* led_vp) {
    tuya_led_t* led = (tuya_led_t*) led_vp;
    led->power = 1-led->power;
    char* cmdbuf = (char*) malloc(32 * sizeof(char)); memset(cmdbuf, 0, 32);
    snprintf(cmdbuf, 32, "{\"20\": %s}", led->power ? "true" : "false");
    pthread_t _thread_id;
    struct _tuya_send_args* args = (struct _tuya_send_args*) malloc(sizeof(struct _tuya_send_args));
    args->led = led;
    args->cmdbuf = cmdbuf;
    pthread_create(&_thread_id, NULL, _tuya_send_thread_helper, args);
}

void led_hue_callback(float progress, void* led_vp) { tuya_led_t* led = (tuya_led_t*) led_vp; _tuya_color_set(led, progress * 359.0, led->sat, led->val); }
void led_sat_callback(float progress, void* led_vp) { tuya_led_t* led = (tuya_led_t*) led_vp; _tuya_color_set(led, led->hue, progress * 1000.0, led->val); }
void led_val_callback(float progress, void* led_vp) { tuya_led_t* led = (tuya_led_t*) led_vp; _tuya_color_set(led, led->hue, led->sat, progress * 1000.0); }

struct _spawn_slider_args { tuya_led_t* led; kindle_slider_t** sliders; };
void _update_values_from_buf(char* buf, struct _spawn_slider_args* args) {
    cJSON* values_j = cJSON_Parse(buf);
    cJSON* dps = cJSON_GetObjectItem(values_j, "dps");
    if (!dps) { cJSON_free(values_j); return; }
    
    cJSON *dp20, *dp24;
    if ((dp20 = cJSON_GetObjectItem(dps, "20"))) {
        args->led->power = dp20->valueint;
    }
    if ((dp24 = cJSON_GetObjectItem(dps, "24"))) {
        sscanf(dp24->valuestring, "%04x%04x%04x", &(args->led->hue), &(args->led->sat), &(args->led->val));
        args->sliders[0]->value = ((float) args->led->hue) / 359.0; gtk_widget_queue_draw(args->sliders[0]->drawing_area); 
        args->sliders[1]->value = ((float) args->led->sat) / 1000.0; gtk_widget_queue_draw(args->sliders[1]->drawing_area); 
        args->sliders[2]->value = ((float) args->led->val) / 1000.0; gtk_widget_queue_draw(args->sliders[2]->drawing_area); 
    }
    
    cJSON_free(values_j);
}
void* _spawn_led_strip_thread(void* args_vp) {
    struct _spawn_slider_args* args = (struct _spawn_slider_args*) args_vp;
    tuya_led_t* led = args->led;
    
    tuya_cmd_send(led, COMMAND_QUERY, NULL);
    int status;
    uint32_t expected_cmd = COMMAND_QUERY;
    tuya_msg_t msg = { 0 };
    status = tuya_msg_recv(led, expected_cmd, &msg);
    if (status) {
        if (status == ERR_SOCK_FAIL) { printf("\n%s", LOGFMT("socket fail!\n", LOG_ERR)); }
        else if (status == ERR_SOCK_CLOSE) { printf("\n%s", LOGFMT("socket closed!\n", LOG_ERR)); }
        else { LOG(PRI_ERR, "error code %d in tuya_recv_msg!\n", status); }
    } else {
        LOG(PRI_INF, "rx msg %d\n", msg.seqno);
        printf("      command=%d\n", msg.command);
        printf("      retcode=%d\n", msg.retcode);
        if (msg.payload && msg.payload_len > 0) {
            printf("      payload (%zu bytes)=", msg.payload_len); fflush(stdout);
            for (size_t i=0; i < msg.payload_len; i++) { printf("%02x", msg.payload[i]); }
            printf("\n      decoded=%s\n", msg.payload);
            _update_values_from_buf((char*) msg.payload, args);
        }
    }
    tuya_msg_free(&msg);
    
    while (1) {
        status = tuya_msg_recv(led, 0, &msg);
        if (status) {
            if (status == ERR_SOCK_FAIL) { printf("\n%s", LOGFMT("socket fail!\n", LOG_ERR)); break; }
            else if (status == ERR_SOCK_CLOSE) { printf("\n%s", LOGFMT("socket closed!\n", LOG_ERR)); break; }
            else { LOG(PRI_ERR, "error code %d in tuya_recv_msg!\n", status); break; }
        }
        if (msg.command == COMMAND_STATUS) {
            LOG(PRI_INF, "rx msg %d\n", msg.seqno);
            printf("      command=%d\n", msg.command);
            printf("      retcode=%d\n", msg.retcode);
            if (msg.payload && msg.payload_len > 0) {
                printf("      payload (%zu bytes)=", msg.payload_len); fflush(stdout);
                for (uint32_t i=0; i < msg.payload_len; i++) { printf("%02x", msg.payload[i]); }
                printf("\n      decoded=%s\n", msg.payload);
                _update_values_from_buf((char*) msg.payload, args);
            }
        }
        
        tuya_msg_free(&msg);
        
    }
    return NULL;
}

struct _tuya_power_btn_args { tuya_led_t* led; GtkWidget* widget; };
gboolean power_button_update(void* data_vp) {
    struct _tuya_power_btn_args* data = (struct _tuya_power_btn_args*) data_vp;
    gtk_label_set_text(GTK_LABEL(data->widget), data->led->power ? "/running" : "/paused");
    return TRUE;
}

GtkWidget* generate_led_strip_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {
	set_screen_data_t* ctrl_data = (set_screen_data_t*) malloc(sizeof(set_screen_data_t));
	ctrl_data->stack = stack;
	tuya_led_t* led = (tuya_led_t*) malloc(sizeof(tuya_led_t));
	tuya_led_new(led, getenv("LED_STRIP_ID"), getenv("LED_STRIP_NAME"), inet_addr(getenv("LED_STRIP_IP")), (unsigned char*) getenv("LED_STRIP_KEY"));
	
	GtkWidget* table = gtk_table_custom(7,9);

	struct _tuya_power_btn_args* pb_args = (struct _tuya_power_btn_args*) malloc(sizeof(struct _tuya_power_btn_args));
	pb_args->led = led;
	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);

	GtkWidget* ev_box = gtk_event_box_new();
	gtk_widget_add_events(ev_box, GDK_BUTTON_PRESS_MASK);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev_box), FALSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_EVENT_BOX(ev_box), GTK_CAN_FOCUS);
	pb_args->widget = gtk_label_new("");
	PangoFontDescription* font_desc = pango_font_description_from_string(FONT_8);
	gtk_widget_modify_font(pb_args->widget, font_desc);
	pango_font_description_free(font_desc);
	gtk_container_add(GTK_CONTAINER(ev_box), pb_args->widget);
	g_signal_connect(ev_box, "button-press-event", G_CALLBACK(led_power_callback), led);
	gtk_box_pack_start(GTK_BOX(wrapper), ev_box, TRUE, TRUE, 5*SCALE);
	
	gtk_table_add(table, 0, 1, 0, 1, wrapper);
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) power_button_update, pb_args);
	GtkWidget* labelbox = gtk_hbox_new(FALSE, 0);
	GtkWidget* label = gtk_label_new((char*) led->name);
	PangoFontDescription* font_desc_label = pango_font_description_from_string(FONT_12);
	gtk_widget_modify_font(label, font_desc_label);
	pango_font_description_free(font_desc_label);
	gtk_box_pack_start(GTK_BOX(labelbox), label, FALSE, FALSE, 20);
	gtk_table_add(table, 1, 7, 0, 1, labelbox);
	
	gtk_table_add(table, 7, 9, 0, 1, button_widget((char*) "۶ৎ₊˚⊹⋆ৎ", set_ctrl_handler,    ctrl_data));

	kindle_slider_t* hue_slider = kindle_slider_new(led, NULL, &led_hue_callback);
	gtk_table_add(table, 0, 1, 1, 6, hue_slider->drawing_area);
	struct _img_src_dat* hue_dat = (struct _img_src_dat*) malloc(sizeof(struct _img_src_dat));
	gtk_table_add(table, 0, 1, 6, 7, image_widget(&hue_dat->ref));
	hue_dat->img = hue;
	hue_dat->size = 40;
	hue_dat->flag = 0;
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) _img_set_src_helper, hue_dat);
	
	kindle_slider_t* sat_slider = kindle_slider_new(led, NULL, &led_sat_callback);
	gtk_table_add(table, 1, 2, 1, 6, sat_slider->drawing_area);
	struct _img_src_dat* sat_dat = (struct _img_src_dat*) malloc(sizeof(struct _img_src_dat));
	gtk_table_add(table, 1, 2, 6, 7, image_widget(&sat_dat->ref));
	sat_dat->img = sat;
	sat_dat->size = 40;
	sat_dat->flag = 0;
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) _img_set_src_helper, sat_dat);

	kindle_slider_t* val_slider = kindle_slider_new(led, NULL, &led_val_callback);
	gtk_table_add(table, 2, 3, 1, 6, val_slider->drawing_area);
	struct _img_src_dat* val_dat = (struct _img_src_dat*) malloc(sizeof(struct _img_src_dat));
	gtk_table_add(table, 2, 3, 6, 7, image_widget(&val_dat->ref));
	val_dat->img = val;
	val_dat->size = 40;
	val_dat->flag = 0;
	g_timeout_add(atol(getenv("UI_UPDATE_FREQUENCY")), (GSourceFunc) _img_set_src_helper, val_dat);
	
	pthread_t _thread_id;
	struct _spawn_slider_args* args = (struct _spawn_slider_args*) malloc(sizeof(struct _spawn_slider_args));
	args->led = led;
	args->sliders = (kindle_slider_t**) malloc(3 * sizeof(kindle_slider_t*));
	args->sliders[0] = hue_slider;
	args->sliders[1] = sat_slider;
	args->sliders[2] = val_slider;
    pthread_create(&_thread_id, NULL, _spawn_led_strip_thread, args);

	gtk_table_add(table, 3, 9, 1, 7, model_init());
	return table;
}


GtkWidget* generate_lamp_screen( GtkWidget* stack, void (*set_screen)(GtkButton*, GdkEvent*, void*) ) {
    return NULL; // todo!!
}
