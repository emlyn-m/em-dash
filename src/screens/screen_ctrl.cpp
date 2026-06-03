#include "src/config.hpp"
#include "src/images/val.h"
#include "src/log.hpp"
#include "src/screens/screens.hpp"
#include "src/screens/table.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gtk-2.0/gdk/gdk.h>
#include <gtk-2.0/gtk/gtk.h>
#include <pthread.h>
#include <unistd.h>

void exit_handler(GtkButton *_b, GdkEvent *_e, void *_d) { exit(0); }

float get_brightness() {
  printf("lb %d\n", get_attr_bool("LOCAL_BUILD"));
  if (get_attr_bool("LOCAL_BUILD")) {
    return 0;
  }
  FILE *bfile = fopen(BRIGHTNESS_SYSFILE, "r");
  char brightness[32];
  fgets(brightness, 31, bfile);
  fclose(bfile);

  return atol(brightness) / (double)BRIGHTNESS_SCALE;
}

void set_brightness(GtkButton *_b, GdkEvent *_e, void *brightness) {
  if (get_attr_bool("LOCAL_BUILD")) {
    return;
  }

  char brightness_s[32];
  memset(brightness_s, 0, 32);

  float brightness_mult;
  memcpy(&brightness_mult, &brightness, sizeof(float));
  int target_brightness = (int)(brightness_mult * BRIGHTNESS_SCALE);
  snprintf(brightness_s, 31, "%d", target_brightness);
  FILE *bfile = fopen(BRIGHTNESS_SYSFILE, "w");
  fwrite(brightness_s, sizeof(char), 31, bfile);
  fclose(bfile);
}

void slider_brightness_callback(float progress, void *_v) {
  void *progress_guint;
  memcpy(&progress_guint, &progress, sizeof(progress));
  set_brightness(NULL, NULL, progress_guint);
}
void *_slider_brightness_fetch(void *data) {
  kindle_slider_t *slider = (kindle_slider_t *)data;
  float brightness = get_brightness();
  slider->value = brightness;

  return NULL;
}

void set_life_handler(GtkButton *_button, GdkEvent *_event, void *data_v) {
  set_screen_data_t *data = (set_screen_data *)data_v;
  data->target_screen_idx = SCREEN_IDX_LIFE;
  set_screen(_button, _event, data_v);
}

void set_led1_handler(GtkButton *_button, GdkEvent *_event, void *data_v) {
  set_screen_data_t *data = (set_screen_data_t *)data_v;
  data->target_screen_idx = SCREEN_IDX_LED1;
  set_screen(_button, _event, data_v);
}

GtkWidget *generate_ctrl_screen(GtkWidget *stack,
                                void (*set_screen)(GtkButton *, GdkEvent *,
                                                   void *)) {
  set_screen_data_t *ctrl_data =
      (set_screen_data_t *)malloc(sizeof(set_screen_data_t));
  ctrl_data->stack = stack;

  GtkWidget *table = gtk_table_custom(15, 20);

  struct _img_src_dat *brightness_data =
      (struct _img_src_dat *)malloc(sizeof(struct _img_src_dat));
  gtk_table_add(table, 0, 2, 0, 3, image_widget(&brightness_data->ref));
  KindleSlider *slider =
      kindle_slider_new(NULL, NULL, &slider_brightness_callback, 0);
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, _slider_brightness_fetch, (void *)slider);
  pthread_detach(thread_id);
  gtk_table_add(table, 0, 2, 3, 16, slider->drawing_area);
  brightness_data->img = val;
  brightness_data->size = 40;
  brightness_data->flag = 0;
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"),
                (GSourceFunc)_img_set_src_helper, brightness_data);
  char dumpbuf[32];
  char *ip = get_attr_str("LOG_IP");
  int ip_offset = strlen(ip) - 1, flag = 0;
  while (!(ip[--ip_offset] == '.' && flag)) {
    if (ip[ip_offset] == '.') {
      flag = 1;
    }
  }
  snprintf(dumpbuf, 32, "<b>logdump</b>\n%s\n:%ld", ip + ip_offset,
           get_attr_long("LOG_PORT"));
  gtk_table_add(table, 0, 2, 16, 20,
                button_widget(dumpbuf, dumplog_handler, NULL));

  gtk_table_add(table, 12, 15, 0, 4,
                button_widget((char *)"&lt;SIGTERM&gt;", exit_handler, NULL));
  gtk_table_add(
      table, 12, 15, 4, 8,
      button_widget((char *)"led.strip0", set_led1_handler, ctrl_data));
  gtk_table_add(table, 12, 15, 16, 20,
                button_widget((char *)"۶ৎ₊˚⊹⋆ৎ", set_life_handler, ctrl_data));
  gtk_table_add(table, 2, 12, 0, 20, log_widget());
  return table;
}
