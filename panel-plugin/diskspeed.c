/*
 * Copyright 2003-2007 Bernhard Walle <bernhard@bwalle.de>
 * Copyright 2010 Florian Rivoal <frivoal@gmail.com>
 * Copyright 2017 Tarun Prabhu <tarun.prabhu@gmail.com>
 * ----------------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675 Mass
 * Ave, Cambridge, MA 02139, USA.
 *
 * ----------------------------------------------------------------------------
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "disk.h"
#include "utils.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#define BORDER 8

#define INIT_MAX 4096
#define MINIMAL_MAX 1024
#define SHRINK_MAX 0.75

#define HISTSIZE_CALCULATE 4
#define HISTSIZE_STORE 20

static gchar *DEFAULT_COLOR[] = {"#FF4F00", "#FFE500"};

#define UPDATE_TIMEOUT 250
#define MAX_LENGTH 32

#define IN 0
#define OUT 1
#define TOT 2
#define SUM 2

typedef struct {
  gboolean auto_max;
  gulong max[SUM];
  gint update_interval;
  GdkRGBA color[SUM];
  gchar *device;
} t_monitor_options;

typedef struct {
  GtkWidget *label;
  GtkWidget *status[SUM];

  gulong history[SUM][HISTSIZE_STORE];
  gulong net_max[SUM];

  t_monitor_options options;

  /* for the network part */
  diskdata data;

  /* Container for everything */
  GtkBox *opt_vbox;

  /* Title image */
  GtkWidget* nodisk;
  GtkWidget* sdd;
  GtkWidget* hdd;
  
  /* Update interval */
  GtkWidget *update_spinner;

  /* Disk */
  GtkWidget *disk_entry;

  /* Maximum */
  GtkWidget *max_use_label;
  GtkWidget *max_entry[SUM];
  GtkBox *max_hbox[SUM];

  /* Bars and values */
  GtkWidget *opt_color_hbox[SUM];

  /* Color */
  GtkWidget *opt_button[SUM];
  GtkWidget *opt_da[SUM];
} t_monitor;

typedef struct {
  XfcePanelPlugin *plugin;

  GtkWidget *ebox;
  GtkWidget *box;
  GtkWidget *ebox_bars;
  GtkWidget *box_bars;
  GtkWidget *tooltip_text;
  guint timeout_id;
  t_monitor *monitor;

  /* options dialog */
  GtkWidget *opt_dialog;
} t_global_monitor;

static void set_progressbar_csscolor(GtkWidget *, GdkRGBA *);
/* -------------------------------------------------------------------------- */
static gboolean update_monitors(t_global_monitor *global) {
  char buffer[SUM + 1][BUFSIZ];
  char buffer_panel[SUM][BUFSIZ];
  gchar caption[BUFSIZ];
  gchar received[BUFSIZ];
  gchar sent[BUFSIZ];
  gulong net[SUM + 1];
  gulong display[SUM + 1], max;
  guint64 histcalculate;
  double temp;
  gint i, j;

  if (!check_disk(&(global->monitor->data))) {
    g_snprintf(caption, sizeof(caption), _("Disk: %s\nUnavailable disk"),
               (global->monitor->data.dev_name));
    gtk_label_set_text(GTK_LABEL(global->tooltip_text), caption);
    gtk_widget_hide(global->monitor->hdd);
    gtk_widget_hide(global->monitor->sdd);
    gtk_widget_show(global->monitor->nodisk);

    return TRUE;
  }

  gtk_widget_hide(global->monitor->nodisk);
  if(global->monitor->data.ssd)
    gtk_widget_show(global->monitor->sdd);
  else
    gtk_widget_show(global->monitor->hdd);
  
  get_current_diskspeed(&(global->monitor->data), &(net[IN]), &(net[OUT]),
                        &(net[TOT]));

  for (i = 0; i < SUM; i++) {
    /* correct value to be from 1 ... 100 */
    global->monitor->history[i][0] = net[i];

    if (global->monitor->history[i][0] < 0) {
      global->monitor->history[i][0] = 0;
    }

    histcalculate = 0;
    for (j = 0; j < HISTSIZE_CALCULATE; j++) {
      histcalculate += global->monitor->history[i][j];
    }
    display[i] = histcalculate / HISTSIZE_CALCULATE;

    /* shift for next run */
    for (j = HISTSIZE_STORE - 1; j > 0; j--) {
      global->monitor->history[i][j] = global->monitor->history[i][j - 1];
    }

    /* update maximum */
    if (global->monitor->options.auto_max) {
      max = max_array(global->monitor->history[i], HISTSIZE_STORE);
      if (display[i] > global->monitor->net_max[i]) {
        global->monitor->net_max[i] = display[i];
      } else if (max < global->monitor->net_max[i] * SHRINK_MAX &&
                 global->monitor->net_max[i] * SHRINK_MAX >= MINIMAL_MAX) {
        global->monitor->net_max[i] *= SHRINK_MAX;
      }
    }

#ifdef DEBUG
    switch (i) {
    case IN:
      DBG("input: Max = %lu", global->monitor->net_max[i]);
      break;

    case OUT:
      DBG("output: Max = %lu", global->monitor->net_max[i]);
      break;

    case TOT:
      DBG("total: Max = %lu", global->monitor->net_max[i]);
      break;
    }
#endif /* DEBUG */

    temp = (double)display[i] / global->monitor->net_max[i];
    if (temp > 1) {
      temp = 1.0;
    } else if (temp < 0) {
      temp = 0.0;
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(global->monitor->status[i]),
                                  temp);

    format_byte_humanreadable(buffer[i], BUFSIZ - 1, display[i], 2, FALSE);
    format_byte_humanreadable(buffer_panel[i], BUFSIZ - 1, display[i], 2, FALSE);
   }

  format_byte_humanreadable(buffer[TOT], BUFSIZ - 1,
                            (display[IN] + display[OUT]), 2,
                            FALSE);

  {
    g_snprintf(caption, sizeof(caption),
               _("Disk: %s\n\n"
                 "Read: %s\n"
                 "Write: %s\n"
                 "---------\n"
                 "Total: %s"),
               global->monitor->data.dev_name, buffer[IN],
               buffer[OUT], buffer[TOT]);
    gtk_label_set_text(GTK_LABEL(global->tooltip_text), caption);
  }

  return TRUE;
}

static void run_update(t_global_monitor *global) {
  if (global->timeout_id > 0) {
    g_source_remove(global->timeout_id);
    global->timeout_id = 0;
  }

  if (global->monitor->options.update_interval > 0) {
    global->timeout_id = g_timeout_add(global->monitor->options.update_interval,
                                       (GSourceFunc)update_monitors, global);
  }
}

static gboolean monitor_set_size(XfcePanelPlugin *plugin, int size,
                                 t_global_monitor *global) {
  gint i;
  XfcePanelPluginMode mode = xfce_panel_plugin_get_mode(plugin);

  DBG("monitor_set_size");

  if (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) {
    for (i = 0; i < SUM; i++)
      gtk_widget_set_size_request(GTK_WIDGET(global->monitor->status[i]), -1,
                                  BORDER);
    gtk_widget_set_size_request(GTK_WIDGET(plugin), size, -1);
  } else {
    for (i = 0; i < SUM; i++)
      gtk_widget_set_size_request(GTK_WIDGET(global->monitor->status[i]),
                                  BORDER, -1);
    gtk_widget_set_size_request(GTK_WIDGET(plugin), -1, size);
  }

  gtk_container_set_border_width(GTK_CONTAINER(global->box), 1);

  return TRUE;
}

static void monitor_set_mode(XfcePanelPlugin *plugin, XfcePanelPluginMode mode,
                             t_global_monitor *global) {
  gint i;

  DBG("monitor_set_mode");
  if (global->timeout_id) {
    g_source_remove(global->timeout_id);
  }

  if (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) {
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->box),
                                   GTK_ORIENTATION_VERTICAL);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->box_bars),
                                   GTK_ORIENTATION_VERTICAL);
  } else if (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) {
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->box),
                                   GTK_ORIENTATION_HORIZONTAL);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->box_bars),
                                   GTK_ORIENTATION_HORIZONTAL);
  }
  gtk_label_set_angle(GTK_LABEL(global->monitor->label), 0);
  for (i = 0; i < SUM; i++) {
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->monitor->status[i]),
                                   GTK_ORIENTATION_HORIZONTAL);
    gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(global->monitor->status[i]),
                                  FALSE);
  }

  monitor_set_size(plugin, xfce_panel_plugin_get_size(plugin), global);

  run_update(global);
}

static gboolean tooltip_cb(GtkWidget *widget, gint x, gint y, gboolean keyboard,
                           GtkTooltip *tooltip, t_global_monitor *global) {
  gtk_tooltip_set_custom(tooltip, global->tooltip_text);
  return TRUE;
}

static void monitor_free(XfcePanelPlugin *plugin, t_global_monitor *global) {
  if (global->timeout_id) {
    g_source_remove(global->timeout_id);
  }

  gtk_widget_destroy(global->tooltip_text);

  g_free(global);
}

static t_global_monitor *monitor_new(XfcePanelPlugin *plugin) {
  t_global_monitor *global;
  gint i;
#if GTK_CHECK_VERSION(3, 16, 0)
  GtkCssProvider *css_provider;
#endif
  GtkWidget* hdd, *sdd;

  global = g_new(t_global_monitor, 1);
  global->timeout_id = 0;
  global->ebox = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->ebox), FALSE);
  gtk_event_box_set_above_child(GTK_EVENT_BOX(global->ebox), TRUE);
  gtk_widget_set_has_tooltip(global->ebox, TRUE);
  g_signal_connect(global->ebox, "query-tooltip", G_CALLBACK(tooltip_cb),
                   global);
  gtk_widget_show(global->ebox);

  global->tooltip_text = gtk_label_new(NULL);
  g_object_ref(global->tooltip_text);

  global->plugin = plugin;
  xfce_panel_plugin_add_action_widget(plugin, global->ebox);

  global->monitor = g_new(t_monitor, 1);
  global->monitor->options.device = g_strdup("");
  global->monitor->options.auto_max = TRUE;
  global->monitor->options.update_interval = UPDATE_TIMEOUT;

  for (i = 0; i < SUM; i++) {
    gdk_rgba_parse(&global->monitor->options.color[i], DEFAULT_COLOR[i]);

    global->monitor->history[i][0] = 0;
    global->monitor->history[i][1] = 0;
    global->monitor->history[i][2] = 0;
    global->monitor->history[i][3] = 0;

    global->monitor->net_max[i] = INIT_MAX;

    global->monitor->options.max[i] = INIT_MAX;
  }

  /* Create widget containers */
  global->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width(GTK_CONTAINER(global->box), 2);
  gtk_widget_show(GTK_WIDGET(global->box));

  /* Create the title image */
  global->monitor->nodisk = gtk_image_new_from_icon_name(
      "dialog-warning", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_box_pack_start(GTK_BOX(global->box), GTK_WIDGET(global->monitor->nodisk),
                     FALSE, FALSE, 0);
  gtk_widget_show(GTK_WIDGET(global->monitor->nodisk));

  global->monitor->sdd = gtk_image_new_from_icon_name(
      "drive-harddisk-solidstate", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_box_pack_start(GTK_BOX(global->box), GTK_WIDGET(global->monitor->sdd),
                     FALSE, FALSE, 0);
  gtk_widget_hide(GTK_WIDGET(global->monitor->sdd));

  global->monitor->hdd = gtk_image_new_from_icon_name(
      "drive-harddisk", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_box_pack_start(GTK_BOX(global->box), GTK_WIDGET(global->monitor->hdd),
                     FALSE, FALSE, 0);
  gtk_widget_hide(GTK_WIDGET(global->monitor->hdd));

  /* Create the progress bars */
  global->ebox_bars = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->ebox_bars), FALSE);
  gtk_event_box_set_above_child(GTK_EVENT_BOX(global->ebox_bars), TRUE);
  gtk_widget_show(global->ebox_bars);
  global->box_bars = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show(global->box_bars);
  for (i = 0; i < SUM; i++) {
    global->monitor->status[i] = GTK_WIDGET(gtk_progress_bar_new());

    css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(
        GTK_STYLE_CONTEXT(gtk_widget_get_style_context(
            GTK_WIDGET(global->monitor->status[i]))),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_set_data(G_OBJECT(global->monitor->status[i]), "css_provider",
                      css_provider);

    gtk_box_pack_start(GTK_BOX(global->box_bars),
                       GTK_WIDGET(global->monitor->status[i]), TRUE, TRUE, 0);
    gtk_widget_show(global->monitor->status[i]);
  }
  gtk_container_add(GTK_CONTAINER(global->ebox_bars),
                    GTK_WIDGET(global->box_bars));
  gtk_container_add(GTK_CONTAINER(global->box), GTK_WIDGET(global->ebox_bars));

  gtk_container_add(GTK_CONTAINER(global->ebox), GTK_WIDGET(global->box));

  return global;
}

static void set_progressbar_csscolor(GtkWidget *pbar, GdkRGBA *color) {
  gchar *css;
#if GTK_CHECK_VERSION(3, 20, 0)
  css = g_strdup_printf(
      "progressbar progress { background-color: %s; min-height: 5px; border-radius: 1; } \
       progressbar trough { min-height: 5px; border-radius: 1; } \
       progressbar empty { min-height: 5px; border-radius: 1; }",
#else
  css = g_strdup_printf(
      "progressbar progress { background-color: %s; min-height: 5px; } \
       progressbar trough { min-height: 5px; border-radius: 1; } \
       progressbar empty { min-height: 5px; border-radius: 1; }",
#endif
      gdk_rgba_to_string(color));
  DBG("setting pbar css to %s", css);
  gtk_css_provider_load_from_data(
      g_object_get_data(G_OBJECT(pbar), "css_provider"), css, strlen(css),
      NULL);
  g_free(css);
}

static void setup_monitor(t_global_monitor *global, gboolean supress_warnings) {
  gint i;

  if (global->timeout_id)
    g_source_remove(global->timeout_id);

  gtk_widget_show(global->monitor->label);

  gtk_widget_show(global->ebox_bars);
  for (i = 0; i < SUM; i++) {
    /* Automatic or fixed maximum */
    if (global->monitor->options.auto_max)
      global->monitor->net_max[i] = INIT_MAX;
    else
      global->monitor->net_max[i] = global->monitor->options.max[i];

      /* Set bar colors */
#if GTK_CHECK_VERSION(3, 16, 0)
    set_progressbar_csscolor(global->monitor->status[i],
                             &global->monitor->options.color[i]);
#else
    gtk_widget_override_background_color(GTK_WIDGET(global->monitor->status[i]),
                                         GTK_STATE_PRELIGHT,
                                         &global->monitor->options.color[i]);
    gtk_widget_override_background_color(GTK_WIDGET(global->monitor->status[i]),
                                         GTK_STATE_SELECTED,
                                         &global->monitor->options.color[i]);
    gtk_widget_override_color(GTK_WIDGET(global->monitor->status[i]),
                              GTK_STATE_SELECTED,
                              &global->monitor->options.color[i]);
#endif
  }

  if (!init_diskspeed(&(global->monitor->data),
                      global->monitor->options.device) &&
      !supress_warnings) {
    xfce_dialog_show_error(
        NULL, NULL, _("%s: Error in initializing:\n%s"),
        _("xfce4-applet-diskspeed"),
        _("Disk not found"));
  }

  monitor_set_mode(global->plugin, xfce_panel_plugin_get_mode(global->plugin),
                   global);

  run_update(global);
}

static void monitor_read_config(XfcePanelPlugin *plugin,
                                t_global_monitor *global) {
  const char *value;
  char *file;
  XfceRc *rc;

  if (!(file = xfce_panel_plugin_save_location(plugin, TRUE)))
    return;

  rc = xfce_rc_simple_open(file, FALSE);
  g_free(file);

  if (!rc)
    return;

  if ((value = xfce_rc_read_entry(rc, "Color_In", NULL)) != NULL) {
    gdk_rgba_parse(&global->monitor->options.color[IN], value);
  }
  if ((value = xfce_rc_read_entry(rc, "Color_Out", NULL)) != NULL) {
    gdk_rgba_parse(&global->monitor->options.color[OUT], value);
  }

  if ((value = xfce_rc_read_entry(rc, "Device", NULL)) && *value) {
    if (global->monitor->options.device)
      g_free(global->monitor->options.device);
    global->monitor->options.device = g_strdup(value);
  }
  if ((value = xfce_rc_read_entry(rc, "Max_In", NULL)) != NULL) {
    global->monitor->options.max[IN] = strtol(value, NULL, 0);
  }
  if ((value = xfce_rc_read_entry(rc, "Max_Out", NULL)) != NULL) {
    global->monitor->options.max[OUT] = strtol(value, NULL, 0);
  }

  global->monitor->options.auto_max =
      xfce_rc_read_bool_entry(rc, "Auto_Max", TRUE);

  global->monitor->options.update_interval =
      xfce_rc_read_int_entry(rc, "Update_Interval", UPDATE_TIMEOUT);

  DBG("monitor_read_config");
  setup_monitor(global, TRUE);

  xfce_rc_close(rc);
}

static void monitor_write_config(XfcePanelPlugin *plugin,
                                 t_global_monitor *global) {
  XfceRc *rc;
  char *file;
  char value[20];

  if (!(file = xfce_panel_plugin_save_location(plugin, TRUE)))
    return;

  rc = xfce_rc_simple_open(file, FALSE);
  g_free(file);

  if (!rc)
    return;

  xfce_rc_write_entry(rc, "Color_In",
                      gdk_rgba_to_string(&global->monitor->options.color[IN]));
  xfce_rc_write_entry(rc, "Color_Out",
                      gdk_rgba_to_string(&global->monitor->options.color[OUT]));

  xfce_rc_write_entry(rc, "Device",
                      global->monitor->options.device
                          ? global->monitor->options.device
                          : "");

  g_snprintf(value, 20, "%lu", global->monitor->options.max[IN]);
  xfce_rc_write_entry(rc, "Max_In", value);

  g_snprintf(value, 20, "%lu", global->monitor->options.max[OUT]);
  xfce_rc_write_entry(rc, "Max_Out", value);

  xfce_rc_write_bool_entry(rc, "Auto_Max", global->monitor->options.auto_max);

  xfce_rc_write_int_entry(rc, "Update_Interval",
                          global->monitor->options.update_interval);

  xfce_rc_close(rc);
}

static void monitor_apply_options(t_global_monitor *global) {
  gint i;

  if (global->monitor->options.device) {
    g_free(global->monitor->options.device);
  }
  global->monitor->options.device =
      g_strdup(gtk_entry_get_text(GTK_ENTRY(global->monitor->disk_entry)));

  for (i = 0; i < SUM; i++) {
    global->monitor->options.max[i] =
        strtol(gtk_entry_get_text(GTK_ENTRY(global->monitor->max_entry[i])),
               NULL, 0) *
        1024;
  }

  global->monitor->options.update_interval =
      (gint)(gtk_spin_button_get_value(
                 GTK_SPIN_BUTTON(global->monitor->update_spinner)) *
                 1000 +
             0.5);

  setup_monitor(global, FALSE);
  DBG("monitor_apply_options_cb");
}

static void max_label_changed(GtkWidget *button, t_global_monitor *global) {
  gint i;
  for (i = 0; i < SUM; i++) {
    global->monitor->options.max[i] =
        strtol(gtk_entry_get_text(GTK_ENTRY(global->monitor->max_entry[i])),
               NULL, 0) *
        1024;
  }

  setup_monitor(global, FALSE);
  DBG("max_label_changed");
}

static void device_changed(GtkWidget *button, t_global_monitor *global) {
  if (global->monitor->options.device) {
    g_free(global->monitor->options.device);
  }

  global->monitor->options.device =
      g_strdup(gtk_entry_get_text(GTK_ENTRY(global->monitor->disk_entry)));

  setup_monitor(global, FALSE);
  DBG("device_changed");
}

static void max_label_toggled(GtkWidget *check_button,
                              t_global_monitor *global) {
  gint i;

  global->monitor->options.auto_max = !global->monitor->options.auto_max;

  for (i = 0; i < SUM; i++) {
    gtk_widget_set_sensitive(GTK_WIDGET(global->monitor->max_hbox[i]),
                             !(global->monitor->options.auto_max));

    /* reset maximum if necessary */
    if (global->monitor->options.auto_max) {
      global->monitor->net_max[i] = INIT_MAX;
    }
  }
  setup_monitor(global, FALSE);
  DBG("max_label_toggled");
}

static void change_color(GtkWidget *button, t_global_monitor *global,
                         gint type) {
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button),
                             &global->monitor->options.color[type]);
  setup_monitor(global, FALSE);
  DBG("change_color(%d) with %s", type,
      gdk_rgba_to_string(&global->monitor->options.color[type]));
}

static void change_color_in(GtkWidget *button, t_global_monitor *global) {
  change_color(button, global, IN);
}

static void change_color_out(GtkWidget *button, t_global_monitor *global) {
  change_color(button, global, OUT);
}

static void monitor_dialog_response(GtkWidget *dlg, int response,
                                    t_global_monitor *global) {

  monitor_apply_options(global);
  gtk_widget_destroy(dlg);
  xfce_panel_plugin_unblock_menu(global->plugin);
  monitor_write_config(global->plugin, global);
}

static void monitor_create_options(XfcePanelPlugin *plugin,
                                   t_global_monitor *global) {
  GtkWidget *dlg;
  GtkBox *vbox, *global_vbox, *net_hbox;
  GtkWidget *device_label, *unit_label[SUM], *max_label[SUM];
  GtkWidget *sep1, *sep2;
  GtkBox *bits_hbox;
  GtkBox *update_hbox;
  GtkWidget *update_label, *update_unit_label;
  GtkWidget *color_label[SUM];
  gint present_data_active;
  GtkSizeGroup *sg;
  gint i;
  gchar buffer[BUFSIZ];
  gchar *color_text[] = {N_("Bar color (i_ncoming):"),
                         N_("Bar color (_outgoing):")};
  gchar *maximum_text_label[] = {N_("Maximum (inco_ming):"),
                                 N_("Maximum (o_utgoing):")};

  xfce_panel_plugin_block_menu(plugin);

  dlg = xfce_titled_dialog_new_with_buttons(_("Network Monitor"), NULL,
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            "gtk-close", GTK_RESPONSE_OK, NULL);

  gtk_window_set_icon_name(GTK_WINDOW(dlg), "xfce4-settings");
  g_signal_connect(dlg, "response", G_CALLBACK(monitor_dialog_response),
                   global);

  global->opt_dialog = dlg;

  global_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, BORDER));
  gtk_container_set_border_width(GTK_CONTAINER(global_vbox), BORDER - 2);
  gtk_widget_show(GTK_WIDGET(global_vbox));
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
                     GTK_WIDGET(global_vbox), TRUE, TRUE, 0);

  sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

  vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
  gtk_widget_show(GTK_WIDGET(vbox));

  global->monitor->opt_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
  gtk_widget_show(GTK_WIDGET(global->monitor->opt_vbox));

  /* Disk */
  net_hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox), GTK_WIDGET(net_hbox),
                     FALSE, FALSE, 0);

  device_label = gtk_label_new_with_mnemonic(_("_Device:"));
  gtk_widget_set_valign(device_label, GTK_ALIGN_CENTER);
  gtk_widget_show(GTK_WIDGET(device_label));
  gtk_box_pack_start(GTK_BOX(net_hbox), GTK_WIDGET(device_label), FALSE, FALSE,
                     0);

  global->monitor->disk_entry = gtk_entry_new();
  gtk_label_set_mnemonic_widget(GTK_LABEL(device_label),
                                global->monitor->disk_entry);
  gtk_entry_set_max_length(GTK_ENTRY(global->monitor->disk_entry), MAX_LENGTH);
  gtk_entry_set_text(GTK_ENTRY(global->monitor->disk_entry),
                     global->monitor->options.device);
  gtk_widget_show(global->monitor->disk_entry);

  gtk_box_pack_start(GTK_BOX(net_hbox), GTK_WIDGET(global->monitor->disk_entry),
                     FALSE, FALSE, 0);

  gtk_size_group_add_widget(sg, device_label);

  gtk_widget_show_all(GTK_WIDGET(net_hbox));

  /* Update timevalue */
  update_hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox),
                     GTK_WIDGET(update_hbox), FALSE, FALSE, 0);

  update_label = gtk_label_new_with_mnemonic(_("Update _interval:"));
  gtk_widget_set_valign(update_label, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(update_hbox), GTK_WIDGET(update_label), FALSE,
                     FALSE, 0);

  global->monitor->update_spinner =
      gtk_spin_button_new_with_range(0.1, 10.0, 0.05);
  gtk_label_set_mnemonic_widget(GTK_LABEL(update_label),
                                global->monitor->update_spinner);
  gtk_spin_button_set_digits(GTK_SPIN_BUTTON(global->monitor->update_spinner),
                             2);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(global->monitor->update_spinner),
                            global->monitor->options.update_interval / 1000.0);
  gtk_box_pack_start(GTK_BOX(update_hbox),
                     GTK_WIDGET(global->monitor->update_spinner), FALSE, FALSE,
                     0);

  update_unit_label = gtk_label_new(_("s"));
  gtk_box_pack_start(GTK_BOX(update_hbox), GTK_WIDGET(update_unit_label), FALSE,
                     FALSE, 0);

  gtk_widget_show_all(GTK_WIDGET(update_hbox));
  gtk_size_group_add_widget(sg, update_label);

  sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox), GTK_WIDGET(sep1),
                     FALSE, FALSE, 0);
  gtk_widget_show(sep1);

  global->monitor->max_use_label =
      gtk_check_button_new_with_mnemonic(_("_Automatic maximum"));
  gtk_widget_show(global->monitor->max_use_label);
  gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox),
                     GTK_WIDGET(global->monitor->max_use_label), FALSE, FALSE,
                     0);

  gtk_toggle_button_set_active(
      GTK_TOGGLE_BUTTON(global->monitor->max_use_label),
      global->monitor->options.auto_max);

  /* Input maximum */
  for (i = 0; i < SUM; i++) {
    global->monitor->max_hbox[i] =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox),
                       GTK_WIDGET(global->monitor->max_hbox[i]), FALSE, FALSE,
                       0);

    max_label[i] = gtk_label_new_with_mnemonic(_(maximum_text_label[i]));
    gtk_widget_set_valign(max_label[i], GTK_ALIGN_CENTER);
    gtk_widget_show(GTK_WIDGET(max_label[i]));
    gtk_box_pack_start(GTK_BOX(global->monitor->max_hbox[i]),
                       GTK_WIDGET(max_label[i]), FALSE, FALSE, 0);

    global->monitor->max_entry[i] = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(max_label[i]),
                                  global->monitor->max_entry[i]);
    gtk_entry_set_max_length(GTK_ENTRY(global->monitor->max_entry[i]),
                             MAX_LENGTH);

    g_snprintf(buffer, BUFSIZ, "%.2f",
               global->monitor->options.max[i] / 1024.0);
    gtk_entry_set_text(GTK_ENTRY(global->monitor->max_entry[i]), buffer);

    gtk_entry_set_width_chars(GTK_ENTRY(global->monitor->max_entry[i]), 7);
    gtk_widget_show(global->monitor->max_entry[i]);

    gtk_box_pack_start(GTK_BOX(global->monitor->max_hbox[i]),
                       GTK_WIDGET(global->monitor->max_entry[i]), FALSE, FALSE,
                       0);

    unit_label[i] = gtk_label_new(_("KiB/s"));
    gtk_box_pack_start(GTK_BOX(global->monitor->max_hbox[i]),
                       GTK_WIDGET(unit_label[i]), FALSE, FALSE, 0);

    gtk_size_group_add_widget(sg, max_label[i]);

    gtk_widget_show_all(GTK_WIDGET(global->monitor->max_hbox[i]));

    gtk_widget_set_sensitive(GTK_WIDGET(global->monitor->max_hbox[i]),
                             !(global->monitor->options.auto_max));

    g_signal_connect(GTK_WIDGET(global->monitor->max_entry[i]), "activate",
                     G_CALLBACK(max_label_changed), global);
  }

  sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox), GTK_WIDGET(sep2),
                     FALSE, FALSE, 0);
  gtk_widget_show(sep2);

  /* Color 1 */
  for (i = 0; i < SUM; i++) {
    global->monitor->opt_color_hbox[i] =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_show(GTK_WIDGET(global->monitor->opt_color_hbox[i]));
    gtk_box_pack_start(GTK_BOX(global->monitor->opt_vbox),
                       GTK_WIDGET(global->monitor->opt_color_hbox[i]), FALSE,
                       FALSE, 0);

    color_label[i] = gtk_label_new_with_mnemonic(_(color_text[i]));
    gtk_widget_set_valign(color_label[i], GTK_ALIGN_CENTER);
    gtk_widget_show(GTK_WIDGET(color_label[i]));
    gtk_box_pack_start(GTK_BOX(global->monitor->opt_color_hbox[i]),
                       GTK_WIDGET(color_label[i]), FALSE, FALSE, 0);

    global->monitor->opt_button[i] =
        gtk_color_button_new_with_rgba(&global->monitor->options.color[i]);
    gtk_label_set_mnemonic_widget(GTK_LABEL(color_label[i]),
                                  global->monitor->opt_button[i]);
    gtk_widget_show(GTK_WIDGET(global->monitor->opt_button[i]));
    gtk_box_pack_start(GTK_BOX(global->monitor->opt_color_hbox[i]),
                       GTK_WIDGET(global->monitor->opt_button[i]), FALSE, FALSE,
                       0);

    gtk_size_group_add_widget(sg, color_label[i]);
  }

  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(global->monitor->opt_vbox),
                     FALSE, FALSE, 0);

  gtk_widget_set_halign(GTK_WIDGET(vbox), GTK_ALIGN_START);
  gtk_widget_set_valign(GTK_WIDGET(vbox), GTK_ALIGN_START);

  gtk_box_pack_start(GTK_BOX(global_vbox), GTK_WIDGET(vbox), FALSE, FALSE, 0);

  g_signal_connect(GTK_WIDGET(global->monitor->max_use_label), "toggled",
                   G_CALLBACK(max_label_toggled), global);
  g_signal_connect(GTK_WIDGET(global->monitor->opt_button[IN]), "color-set",
                   G_CALLBACK(change_color_in), global);
  g_signal_connect(GTK_WIDGET(global->monitor->opt_button[OUT]), "color-set",
                   G_CALLBACK(change_color_out), global);
  g_signal_connect(GTK_WIDGET(global->monitor->disk_entry), "activate",
                   G_CALLBACK(device_changed), global);

  gtk_widget_show(dlg);
}

static void netload_construct(XfcePanelPlugin *plugin) {
  t_global_monitor *global;

  global = monitor_new(plugin);

  monitor_read_config(plugin, global);

  g_signal_connect(plugin, "free-data", G_CALLBACK(monitor_free), global);

  g_signal_connect(plugin, "save", G_CALLBACK(monitor_write_config), global);

  xfce_panel_plugin_menu_show_configure(plugin);
  g_signal_connect(plugin, "configure-plugin",
                   G_CALLBACK(monitor_create_options), global);

  g_signal_connect(plugin, "size-changed", G_CALLBACK(monitor_set_size),
                   global);

  g_signal_connect(plugin, "mode-changed", G_CALLBACK(monitor_set_mode),
                   global);

  gtk_container_add(GTK_CONTAINER(plugin), global->ebox);

  run_update(global);
}

XFCE_PANEL_PLUGIN_REGISTER(netload_construct);
