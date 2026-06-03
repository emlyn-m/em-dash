#include "src/config.hpp"
#include "src/log.hpp"
#include "src/net/net.hpp"
#include "src/widgets/widgets.hpp"

#include "gtk/gtk.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

gboolean devices_update(gpointer *data_p) {
  telem_t *data = (telem_t *)data_p;

  char device_name_buf[64];
  for (uint32_t i = 0; i < data->n_devices; i++) {
    memset(device_name_buf, 0, 64);
    snprintf(device_name_buf, 64, data->devices[i]->online ? "$ %s" : "- %s",
             data->devices[i]->alias);
    gtk_label_set_text(GTK_LABEL(data->device_name_widgets[i]),
                       device_name_buf);
    gtk_widget_set_visible(data->device_name_widgets[i], true);
    if (data->devices[i]->online) {
      gtk_widget_set_visible(data->device_ip_widgets[i], true);
      gtk_label_set_text(GTK_LABEL(data->device_ip_widgets[i]),
                         data->devices[i]->ip);
    } else {
      gtk_widget_set_visible(data->device_ip_widgets[i], false);
    }
  }
  for (uint32_t i = data->n_devices; i < data->max_devices; i++) {
    gtk_widget_set_visible(data->device_name_widgets[i], false);
    gtk_widget_set_visible(data->device_ip_widgets[i], false);
  }

  return TRUE;
}

gboolean services_update(gpointer *data_vp) {
  telem_t *data = (telem_t *)data_vp;

  for (uint32_t i = 0; i < data->n_services; i++) {
    gtk_widget_set_visible(data->service_name_widgets[i], true);
    gtk_widget_set_visible(data->service_status_widgets[i], true);
    gtk_label_set_text(GTK_LABEL(data->service_name_widgets[i]),
                       data->services[i]->name);
    gtk_label_set_text(GTK_LABEL(data->service_status_widgets[i]),
                       data->services[i]->status);
  }
  for (uint32_t i = data->n_services; i < data->max_services; i++) {
    gtk_widget_set_visible(data->service_name_widgets[i], false);
    gtk_widget_set_visible(data->service_status_widgets[i], false);
  }

  return TRUE;
}

gboolean ip_update(gpointer *data_p) {
  telem_t *data = (telem_t *)data_p;

  if (data->num_pings) {
    gtk_label_set_text(GTK_LABEL(data->ip_label), data->ip);
    if (data->num_pings >= 2) {
      char jitter_buf[32];
      snprintf(jitter_buf, 31, "%dms", (int)data->jitter);
      gtk_label_set_text(GTK_LABEL(data->jitter_label), jitter_buf);
    }

    char ping_buf[32] = {0};
    snprintf(ping_buf, 31, "%ldms", data->ping_logs[data->num_pings - 1]);
    gtk_label_set_text(GTK_LABEL(data->ping_label), ping_buf);
  }

  if (data->wifi_strength <= 0) {
    char wifi_buf[32] = {0};
    snprintf(wifi_buf, 31, "%ddBm", data->wifi_strength);
    gtk_label_set_text(GTK_LABEL(data->wifi_strength_label), wifi_buf);
  } else {
    gtk_label_set_text(GTK_LABEL(data->wifi_strength_label), "-");
  }

  return TRUE;
}

gboolean battery_update(gpointer *data_p) {
  telem_t *data = (telem_t *)data_p;

  char battery_value[8];
  memset(battery_value, 0, 8);
  char *battery_end =
      battery_value + snprintf(battery_value, 4, "%d", data->battery);
  if (data->current > 0) {
    battery_end[0] = 0xe2;
    battery_end[1] = 0x9a;
    battery_end[2] = 0xa1;
  } else {
    battery_end[0] = '%';
  }
  gtk_label_set_text(GTK_LABEL(data->battery_label), battery_value);

  char current_buf[10] = {0};
  snprintf(current_buf, 9, "%dmA", data->current);
  gtk_label_set_text(GTK_LABEL(data->current_label), current_buf);

  return TRUE;
}

GtkWidget *telem_widget() {

  // wrapper
  telem_t *data = (telem_t *)malloc(sizeof(telem_t));
  data->max_devices = 8;
  data->max_services = 10;
  data->n_devices = 0;
  data->last_update = 0;
  data->last_update_net = 0;
  data->update_freq = get_attr_long("TELEM_UPDATE_FREQUENCY");
  data->update_freq_net = get_attr_long("TELEM_NET_UPDATE_FREQUENCY");
  data->devices = (device_t **)malloc(data->max_devices * sizeof(device_t *));
  for (int i = 0; i < data->max_devices; i++) {
    data->devices[i] = (device_t *)malloc(sizeof(device_t));
    data->devices[i]->name = (char *)malloc(64 * sizeof(char));
    memset(data->devices[i]->name, 0, 64);
    data->devices[i]->alias = (char *)malloc(64 * sizeof(char));
    memset(data->devices[i]->alias, 0, 64);
    data->devices[i]->ip = (char *)malloc(32 * sizeof(char));
    memset(data->devices[i]->ip, 0, 32);
  }
  data->device_name_widgets =
      (GtkWidget **)malloc(data->max_devices * sizeof(GtkWidget *));
  data->device_ip_widgets =
      (GtkWidget **)malloc(data->max_devices * sizeof(GtkWidget *));
  data->n_services = 0;
  data->services =
      (service_t **)malloc(data->max_services * sizeof(service_t *));
  for (int i = 0; i < data->max_services; i++) {
    data->services[i] = (service_t *)malloc(sizeof(service_t));
    data->services[i]->name = (char *)malloc(64 * sizeof(char));
    memset(data->services[i]->name, 0, 64);
    data->services[i]->status = (char *)malloc(64 * sizeof(char));
    memset(data->services[i]->status, 0, 64);
  }
  data->service_name_widgets =
      (GtkWidget **)malloc(data->max_services * sizeof(GtkWidget *));
  data->service_status_widgets =
      (GtkWidget **)malloc(data->max_services * sizeof(GtkWidget *));
  data->battery = 0;
  data->current = 0;
  data->max_pings = 20;
  data->num_pings = 0;
  data->ping_logs =
      (unsigned long *)malloc(data->max_pings * sizeof(unsigned long));
  memset(data->ping_logs, 0, data->max_pings * sizeof(unsigned long));
  data->jitter = 0;
  data->ip = (char *)malloc(30 * sizeof(char));
  memset(data->ip, 0, 30);

  GtkWidget *wrapper = gtk_vbox_new(FALSE, 5 * SCALE);

  // telem label
  GtkWidget *telem_label = gtk_label_new("Telemetry");
  PangoFontDescription *font_desc_telem =
      pango_font_description_from_string(FONT_BOLD_14);
  gtk_misc_set_alignment(GTK_MISC(telem_label), 0.0, 0.5);
  gtk_widget_modify_font(telem_label, font_desc_telem);
  pango_font_description_free(font_desc_telem);
  gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20 * SCALE);
  gtk_box_pack_start(GTK_BOX(wrapper), telem_label, FALSE, FALSE, 0);

  GtkWidget *telem_info = gtk_vbox_new(false, 0);

  // devices
  GtkWidget *device_block = gtk_vbox_new(false, 5 * SCALE);
  PangoFontDescription *font_desc_lg =
      pango_font_description_from_string(FONT_10);
  PangoFontDescription *font_desc_sm =
      pango_font_description_from_string(FONT_8);
  PangoFontDescription *font_desc_lg_bold =
      pango_font_description_from_string(FONT_BOLD_10);
  PangoFontDescription *font_desc_sm_bold =
      pango_font_description_from_string(FONT_BOLD_8);

  for (uint32_t i = 0; i < data->max_devices; i++) {
    GtkWidget *instance_block = gtk_vbox_new(FALSE, 0);
    data->device_name_widgets[i] = gtk_label_new("");
    data->device_ip_widgets[i] = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(data->device_name_widgets[i]), 0.0, 1.0);
    gtk_misc_set_alignment(GTK_MISC(data->device_ip_widgets[i]), 1.0, 0.0);
    gtk_widget_modify_font(data->device_name_widgets[i], font_desc_sm_bold);
    gtk_widget_modify_font(data->device_ip_widgets[i], font_desc_sm);
    gtk_box_pack_start(GTK_BOX(instance_block), data->device_name_widgets[i],
                       FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(instance_block), data->device_ip_widgets[i], FALSE,
                     FALSE, 0);
    gtk_box_pack_start(GTK_BOX(device_block), instance_block, FALSE, FALSE, 0);
  }

  // services
  GtkWidget *service_block = gtk_vbox_new(false, 0);

  for (uint32_t i = 0; i < data->max_services; i++) {
    GtkWidget *instance_block = gtk_hbox_new(FALSE, 0);
    data->service_name_widgets[i] = gtk_label_new("");
    data->service_status_widgets[i] = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(data->service_name_widgets[i]), 0.0, 0.0);
    gtk_misc_set_alignment(GTK_MISC(data->service_status_widgets[i]), 1.0, 1.0);
    gtk_widget_modify_font(data->service_name_widgets[i], font_desc_sm_bold);
    gtk_widget_modify_font(data->service_status_widgets[i], font_desc_sm);
    gtk_box_pack_start(GTK_BOX(instance_block), data->service_name_widgets[i],
                       FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(instance_block), data->service_status_widgets[i],
                     FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(service_block), instance_block, FALSE, FALSE, 0);
  }

  pango_font_description_free(font_desc_sm);
  pango_font_description_free(font_desc_lg);
  pango_font_description_free(font_desc_sm_bold);
  pango_font_description_free(font_desc_lg_bold);
  gtk_box_pack_start(GTK_BOX(telem_info), device_block, FALSE, FALSE,
                     5 * SCALE);
  gtk_box_pack_end(GTK_BOX(telem_info), service_block, FALSE, FALSE, 5 * SCALE);
  gtk_box_pack_start(GTK_BOX(wrapper), telem_info, TRUE, TRUE, 5 * SCALE);

  // telem_stats
  PangoFontDescription *font_desc_label =
      pango_font_description_from_string(FONT_8);
  GtkWidget *telem_stats = gtk_vbox_new(FALSE, 0);

  // battery label
  GtkWidget *battery_info = gtk_hbox_new(false, 0);
  GtkWidget *battery_label = gtk_label_new("battery");
  GtkWidget *battery_value = gtk_label_new("");
  data->battery_label = battery_value;
  gtk_widget_modify_font(battery_label, font_desc_label);
  gtk_widget_modify_font(battery_value, font_desc_label);
  gtk_box_pack_start(GTK_BOX(battery_info), battery_label, FALSE, FALSE,
                     5 * SCALE);
  gtk_box_pack_end(GTK_BOX(battery_info), battery_value, FALSE, FALSE,
                   5 * SCALE);
  gtk_box_pack_start(GTK_BOX(telem_stats), battery_info, FALSE, FALSE, 0);

  GtkWidget *current_info = gtk_hbox_new(false, 0);
  GtkWidget *current_label = gtk_label_new("current");
  GtkWidget *current_value = gtk_label_new("");
  data->current_label = current_value;
  gtk_widget_modify_font(current_label, font_desc_label);
  gtk_widget_modify_font(current_value, font_desc_label);
  gtk_box_pack_start(GTK_BOX(current_info), current_label, FALSE, FALSE,
                     5 * SCALE);
  gtk_box_pack_end(GTK_BOX(current_info), current_value, FALSE, FALSE,
                   5 * SCALE);
  gtk_box_pack_start(GTK_BOX(telem_stats), current_info, FALSE, FALSE, 0);

  // ping label
  GtkWidget *wifi_strength_info = gtk_hbox_new(false, 0);
  GtkWidget *wifi_strength_label = gtk_label_new("wifi");
  GtkWidget *wifi_strength_value = gtk_label_new("");
  data->wifi_strength_label = wifi_strength_value;
  gtk_widget_modify_font(wifi_strength_label, font_desc_label);
  gtk_widget_modify_font(wifi_strength_value, font_desc_label);
  gtk_box_pack_start(GTK_BOX(wifi_strength_info), wifi_strength_label, FALSE,
                     FALSE, 5 * SCALE);
  gtk_box_pack_end(GTK_BOX(wifi_strength_info), wifi_strength_value, FALSE,
                   FALSE, 5 * SCALE);
  gtk_box_pack_start(GTK_BOX(telem_stats), wifi_strength_info, FALSE, FALSE, 0);

  GtkWidget *ping_info = gtk_hbox_new(false, 0);
  GtkWidget *ping_label = gtk_label_new("ping");
  GtkWidget *ping_value = gtk_label_new("");
  data->ping_label = ping_value;
  gtk_widget_modify_font(ping_label, font_desc_label);
  gtk_widget_modify_font(ping_value, font_desc_label);
  gtk_box_pack_start(GTK_BOX(ping_info), ping_label, FALSE, FALSE, 5 * SCALE);
  gtk_box_pack_end(GTK_BOX(ping_info), ping_value, FALSE, FALSE, 5 * SCALE);
  gtk_box_pack_start(GTK_BOX(telem_stats), ping_info, FALSE, FALSE, 0);

  // jitter label
  GtkWidget *jitter_info = gtk_hbox_new(false, 0);
  GtkWidget *jitter_label = gtk_label_new("jitter");
  GtkWidget *jitter_value = gtk_label_new("");
  data->jitter = 0;
  data->jitter_label = jitter_value;
  gtk_widget_modify_font(jitter_label, font_desc_label);
  gtk_widget_modify_font(jitter_value, font_desc_label);
  gtk_box_pack_start(GTK_BOX(jitter_info), jitter_label, FALSE, FALSE,
                     5 * SCALE);
  gtk_box_pack_end(GTK_BOX(jitter_info), jitter_value, FALSE, FALSE, 5 * SCALE);
  gtk_box_pack_start(GTK_BOX(telem_stats), jitter_info, FALSE, FALSE, 0);

  // ip label
  GtkWidget *ip_info = gtk_hbox_new(false, 0);
  GtkWidget *ip_value = gtk_label_new("");
  data->ip_label = ip_value;
  gtk_widget_modify_font(ip_value, font_desc_label);
  gtk_misc_set_alignment(GTK_MISC(ip_value), 0.5, 0.0);
  gtk_box_pack_start(GTK_BOX(ip_info), ip_value, TRUE, TRUE, 5 * SCALE);
  gtk_box_pack_start(GTK_BOX(telem_stats), ip_info, TRUE, TRUE, 0);

  g_timeout_add(get_attr_long("NET_UPDATE_FREQUENCY"),
                (GSourceFunc)update_telem_async, data);
  g_timeout_add(get_attr_long("NET_UPDATE_FREQUENCY"),
                (GSourceFunc)update_telem_async_net, data);
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"), (GSourceFunc)ip_update,
                data);
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"),
                (GSourceFunc)battery_update, data);
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"),
                (GSourceFunc)devices_update, data);
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"),
                (GSourceFunc)services_update, data);

  gtk_box_pack_start(GTK_BOX(wrapper), telem_stats, FALSE, FALSE, 5);
  pango_font_description_free(font_desc_label);

  return wrapper;
}
