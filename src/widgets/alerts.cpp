#include "../net/net.hpp"
#include "./widgets.hpp"
#include "src/config.hpp"

#include "gtk/gtk.h"
#include <cstdlib>

const char *SEV_LABELS[7] = {"fatal!", "err",  "sev2", "wrn",
                             "inf",    "sev5", "dbg"}; // 2, 5 SHOULD be unused!

gboolean alerts_update(gpointer *data_p) {
  alert_t *data = (alert_t *)data_p;

  for (uint32_t i = 0; i < data->num_alerts; i++) {
    alert_ev_t *alert = data->alerts[i];
    gtk_widget_set_visible(data->alert_widgets[i], TRUE);
    gtk_widget_set_visible(data->alert_meta_widgets[i], TRUE);

    char *meta_buf = (char *)malloc(30 * sizeof(char));
    const char *sev_label = (alert->severity >= 0 && alert->severity <= 6)
                                ? SEV_LABELS[alert->severity]
                                : "sev?";
    snprintf(meta_buf, 30, "%s;%s", alert->category, sev_label);
    gtk_label_set_text(GTK_LABEL(data->alert_meta_widgets[i]), meta_buf);
    gtk_label_set_text(GTK_LABEL(data->alert_widgets[i]), alert->msg);
    free(meta_buf);
  }
  for (uint32_t i = data->num_alerts; i < data->max_alerts; i++) {
    gtk_widget_set_visible(data->alert_widgets[i], FALSE);
    gtk_widget_set_visible(data->alert_meta_widgets[i], FALSE);
  }

  return TRUE;
}

GtkWidget *alerts_widget() {

  alert_t *alert_data = (alert_t *)malloc(sizeof(alert_t));
  alert_data->max_alerts = 4;
  alert_data->num_alerts = 0;
  alert_data->alerts =
      (alert_ev_t **)malloc(alert_data->max_alerts * sizeof(alert_ev_t *));
  alert_data->last_update = 0;
  alert_data->update_freq = get_attr_long("ALERT_UPDATE_FREQUENCY");
  alert_data->alert_widgets =
      (GtkWidget **)malloc(alert_data->max_alerts * sizeof(GtkWidget *));
  alert_data->alert_meta_widgets =
      (GtkWidget **)malloc(alert_data->max_alerts * sizeof(GtkWidget *));

  GtkWidget *wrapper = gtk_vbox_new(FALSE, 0);

  // alerts title
  GtkWidget *alerts_title = gtk_label_new("Alerts");
  gtk_misc_set_alignment(GTK_MISC(alerts_title), 0.0, 0.5);
  PangoFontDescription *font_title =
      pango_font_description_from_string(FONT_BOLD_18);
  gtk_widget_modify_font(alerts_title, font_title);
  pango_font_description_free(font_title);
  gtk_box_pack_start(GTK_BOX(wrapper), alerts_title, FALSE, FALSE, 0);

  // alerts log
  GtkWidget *events = gtk_vbox_new(FALSE, 0);
  PangoFontDescription *font_alert_meta =
      pango_font_description_from_string(FONT_BOLD_10);
  PangoFontDescription *font_alert =
      pango_font_description_from_string(FONT_12);

  for (uint32_t i = 0; i < alert_data->max_alerts; i++) {

    alert_data->alerts[i] = (alert_ev_t *)malloc(sizeof(alert_ev_t));
    alert_data->alerts[i]->category = (char *)malloc(32 * sizeof(char));
    alert_data->alerts[i]->msg = (char *)malloc(2048 * sizeof(char));

    GtkWidget *event_vbox = gtk_vbox_new(FALSE, 0);

    alert_data->alert_widgets[i] = gtk_label_new("");
    alert_data->alert_meta_widgets[i] = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(alert_data->alert_widgets[i]), 0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(alert_data->alert_meta_widgets[i]), 0, 0.5);
    gtk_widget_modify_font(alert_data->alert_widgets[i], font_alert);
    gtk_widget_modify_font(alert_data->alert_meta_widgets[i], font_alert_meta);
    gtk_box_pack_end(GTK_BOX(event_vbox), alert_data->alert_widgets[i], FALSE,
                     FALSE, 0);
    gtk_box_pack_start(GTK_BOX(event_vbox), alert_data->alert_meta_widgets[i],
                       FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(events), event_vbox, FALSE, FALSE, 5 * SCALE);
  }
  gtk_box_pack_end(GTK_BOX(wrapper), events, TRUE, TRUE, 0);
  pango_font_description_free(font_alert_meta);
  pango_font_description_free(font_alert);

  gtk_container_set_border_width(GTK_CONTAINER(wrapper), 20 * SCALE);
  g_timeout_add(get_attr_long("UI_UPDATE_FREQUENCY"),
                (GSourceFunc)alerts_update, alert_data);
  g_timeout_add(get_attr_long("NET_UPDATE_FREQUENCY"),
                (GSourceFunc)update_alerts_net, alert_data);

  return wrapper;
}
