/*  $Id$
 *
 *  Copyright (c) 2011 David Schneider <dnschneid@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "embed.h"
#include "embed-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE \
  "http://goodies.xfce.org/projects/panel-plugins/xfce4-embed-plugin"


#define SETTINGS_PAD_PX 5


static void
embed_configure_response (GtkWidget *dialog, gint response, EmbedPlugin *embed)
{
  gboolean result;

  if (response == GTK_RESPONSE_HELP) {
    /* show help */
    result = g_spawn_command_line_async (
        "exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

    if (G_UNLIKELY (result == FALSE))
      g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
  } else {
    /* remove the dialog data from the plugin */
    g_object_set_data (G_OBJECT (embed->plugin), "dialog", NULL);

    /* unlock the panel menu */
    xfce_panel_plugin_unblock_menu (embed->plugin);

    /* save the plugin */
    embed_save (embed->plugin, embed);

    /* destroy the properties dialog */
    gtk_widget_destroy (dialog);

    /* start a new search if necessary */
    if (embed->criteria_updated)
      embed_search_again (embed);
  }
}



static void
embed_proc_name_changed (GtkEditable *edit, EmbedPlugin *embed)
{
  g_free (embed->proc_name);
  embed->proc_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit)));
  embed->criteria_updated = TRUE;
}



static void
embed_window_class_changed (GtkEditable *edit, EmbedPlugin *embed)
{
  g_free (embed->window_class);
  embed->window_class = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit)));
  embed->criteria_updated = TRUE;
}



static void
embed_entry_set_good (GtkEntry *edit, gboolean good)
{
  if (good) {
    gtk_entry_set_icon_tooltip_text (edit, GTK_ENTRY_ICON_SECONDARY,
                                     _("The REGEX is valid"));
    gtk_entry_set_icon_from_stock (edit, GTK_ENTRY_ICON_SECONDARY,
                                   GTK_STOCK_YES);
  } else {
    gtk_entry_set_icon_tooltip_text (edit, GTK_ENTRY_ICON_SECONDARY,
                                     _("The REGEX is invalid"));
    gtk_entry_set_icon_from_stock (edit, GTK_ENTRY_ICON_SECONDARY,
                                   GTK_STOCK_NO);
  }
}



static void
embed_window_regex_changed (GtkEditable *edit, EmbedPlugin *embed)
{
  const gchar *text;
  GRegex *regex_comp;

  /* Confirm that the REGEX is okay before saving. */
  text = gtk_entry_get_text (GTK_ENTRY (edit));
  regex_comp = g_regex_new (text, G_REGEX_OPTIMIZE, 0, NULL);
  if (regex_comp) {
    g_free (embed->window_regex);
    if (embed->window_regex_comp)
      g_regex_unref (embed->window_regex_comp);
    embed->window_regex = g_strdup (text);
    embed->window_regex_comp = regex_comp;
    embed->criteria_updated = TRUE;
    embed_entry_set_good (GTK_ENTRY (edit), TRUE);
  } else {
    embed_entry_set_good (GTK_ENTRY (edit), FALSE);
  }
}



static void
embed_label_fmt_changed (GtkEditable *edit, EmbedPlugin *embed)
{
  g_free (embed->label_fmt);
  embed->label_fmt = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit)));
  embed_update_label (embed);
}



static void
embed_min_size_changed (GtkSpinButton *spin, EmbedPlugin *embed)
{
  embed->min_size = gtk_spin_button_get_value_as_int (spin);
  embed_size_changed_simple (embed);
}



static void
embed_expand_toggled (GtkToggleButton *toggle, EmbedPlugin *embed)
{
  embed->expand = gtk_toggle_button_get_active (toggle);
  xfce_panel_plugin_set_expand (embed->plugin, embed->expand);
}



void
embed_configure (XfcePanelPlugin *plugin, EmbedPlugin *embed)
{
  GtkWidget *dialog, *content, *hbox, *label, *widget;
  const gchar *tooltip;

  /* block the plugin menu */
  xfce_panel_plugin_block_menu (plugin);

  /* stop searches */
  embed_stop_search (embed);

  /* create the dialog */
  dialog = xfce_titled_dialog_new_with_buttons (
      _("Embed Plugin"),
      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_HELP, GTK_RESPONSE_HELP,
      GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
      NULL);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

#define ADD1(widgetA) \
    gtk_box_pack_start_defaults (GTK_BOX (content), widgetA)
#define ADD2(widgetA, widgetB) \
    hbox = gtk_hbox_new (TRUE, SETTINGS_PAD_PX); \
    gtk_box_pack_start_defaults (GTK_BOX (hbox), widgetA); \
    gtk_box_pack_start_defaults (GTK_BOX (hbox), widgetB); \
    gtk_box_pack_start_defaults (GTK_BOX (content), hbox)
#define TOOLTIP2(widgetA, widgetB, tooltiptext) \
    tooltip = tooltiptext; \
    gtk_widget_set_tooltip_text (widgetA, tooltip); \
    gtk_widget_set_tooltip_text (widgetB, tooltip);
#define ENTRY(labeltext, tooltiptext, value, callback) \
    label = gtk_label_new_with_mnemonic (labeltext); \
    widget = gtk_entry_new (); \
    if (value) \
      gtk_entry_set_text (GTK_ENTRY (widget), value); \
    g_signal_connect (G_OBJECT (widget), "changed", \
                      G_CALLBACK (callback), embed); \
    TOOLTIP2(label, widget, tooltiptext); \
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5f); \
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget); \
    ADD2(label, widget);
#define SPIN(labeltext, tooltiptext, value, callback) \
    label = gtk_label_new_with_mnemonic (labeltext); \
    widget = gtk_spin_button_new_with_range (0, G_MAXINT, 1); \
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value); \
    g_signal_connect (G_OBJECT (widget), "value-changed", \
                      G_CALLBACK (callback), embed); \
    TOOLTIP2(label, widget, tooltiptext); \
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5f); \
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget); \
    ADD2(label, widget);

  /* proc_name */
  ENTRY(_("_Process name"), _("Match the window's application's process name\n"
                             "Leave blank if it is not a criterion"),
        embed->proc_name, embed_proc_name_changed);

  /* window_class */
  ENTRY(_("_Window class"), _("Match the window's class\n"
                             "Leave blank if it is not a criterion"),
        embed->window_class, embed_window_class_changed);

  /* window_regex */
  ENTRY(_("Window _title"), _("Match the window's title using a REGEX\n"
                             "Leave blank if it is not a criterion"),
        embed->window_regex, embed_window_regex_changed);
  embed_entry_set_good (GTK_ENTRY (widget), TRUE);

  /* label_fmt */
  ENTRY(_("_Label format"), _("Leave blank to hide the label\n"
        EMBED_LABEL_FMT_TITLE " expands to the embedded window's title"),
        embed->label_fmt, embed_label_fmt_changed);

  /* poll_delay */
  /* No UI element. Generally polling is unnecessary, unless you have a very
   * strange window that you're trying to match that is not uniquely
   * identifiable when it is mapped. */

  /* min_size */
  SPIN(_("Minimum _size (px)"), _("Minimum size of the embedded window\n"
                                  "Set to 0 to keep the original window size"),
       embed->min_size, embed_min_size_changed);

  /* expand */
  widget = gtk_check_button_new_with_mnemonic (_("_Expand"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), embed->expand);
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (embed_expand_toggled), embed);
  gtk_widget_set_tooltip_text (widget, _("Use up all available panel space"));
  ADD1(widget);

#undef ADD1
#undef ADD2
#undef TOOLTIP2
#undef ENTRY
#undef SPIN

  gtk_widget_show_all (content);

  /* center dialog on the screen */
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  /* set dialog icon */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  /* criteria have not been updated */
  embed->criteria_updated = FALSE;

  /* connect the reponse signal to the dialog */
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (embed_configure_response), embed);

  /* show the entire dialog */
  gtk_widget_show (dialog);
}



void
embed_about (XfcePanelPlugin *plugin)
{
  /* about dialog code. you can use the GtkAboutDialog
   * or the XfceAboutInfo widget */
}
