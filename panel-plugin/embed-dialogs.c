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
#include <X11/Xlib.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "embed.h"
#include "embed-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE \
  "http://goodies.xfce.org/projects/panel-plugins/xfce4-embed-plugin"



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
    gtk_entry_set_icon_from_stock (edit, GTK_ENTRY_ICON_SECONDARY,
                                   GTK_STOCK_YES);
    gtk_entry_set_icon_tooltip_text (edit, GTK_ENTRY_ICON_SECONDARY,
                                     _("Input is valid"));
  } else {
    gtk_entry_set_icon_from_stock (edit, GTK_ENTRY_ICON_SECONDARY,
                                   GTK_STOCK_NO);
    gtk_entry_set_icon_tooltip_text (edit, GTK_ENTRY_ICON_SECONDARY,
                                     _("Input is invalid"));
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
embed_launch_cmd_changed (GtkEditable *edit, EmbedPlugin *embed)
{
  const gchar *text;
  gint argc;
  gchar **argv;

  /* Confirm that the command line is okay before saving. */
  text = gtk_entry_get_text (GTK_ENTRY (edit));
  if (*text) {
    if (!g_shell_parse_argv (text, &argc, &argv, NULL)) {
      embed_entry_set_good (GTK_ENTRY (edit), FALSE);
      return;
    }
    g_strfreev (argv);
  }

  g_free (embed->launch_cmd);
  embed->launch_cmd = g_strdup (text);
  embed->criteria_updated = TRUE;
  embed_entry_set_good (GTK_ENTRY (edit), TRUE);
}



static void
embed_label_fmt_changed (GtkEditable *edit, EmbedPlugin *embed)
{
  g_free (embed->label_fmt);
  embed->label_fmt = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit)));
  embed_update_label (embed);
}



static void
embed_label_font_changed (GtkFontButton *font_button, EmbedPlugin *embed)
{
  g_free (embed->label_font);
  embed->label_font = g_strdup (gtk_font_button_get_font_name (font_button));
  embed_update_label_font (embed);
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



static void
set_tooltips (GtkWidget *widgetA, GtkWidget *widgetB, const gchar *text)
{
  gtk_widget_set_tooltip_text (widgetA, text);
  gtk_widget_set_tooltip_text (widgetB, text);
}



static GtkWidget *
add_frame (GtkWidget *content, gint rows, const gchar *title)
{
  GtkWidget *table, *frame;
  table = gtk_table_new (rows, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  frame = xfce_gtk_frame_box_new_with_content (title, table);
  gtk_box_pack_start_defaults (GTK_BOX (content), frame);
  return table;
}




static GtkWidget *
add_label (GtkWidget *table, gint row,
           GtkWidget *mnemonic_widget, const gchar *text)
{
  GtkWidget *label = gtk_label_new_with_mnemonic (text);
  if (mnemonic_widget) {
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5f);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), mnemonic_widget);
  }
  gtk_table_attach_defaults (GTK_TABLE (table), label,
      0, mnemonic_widget ? 1 : 2, row, row+1);
  return label;
}



static void
add_entry (EmbedPlugin *embed, GtkWidget *table, gint row,
           const gchar *value, gboolean show_icon, gpointer callback,
           const gchar *labeltext, const gchar *tooltiptext)
{
  GtkWidget *label, *entry;
  entry = gtk_entry_new ();
  label = add_label (table, row, entry, labeltext);
  if (value) gtk_entry_set_text (GTK_ENTRY (entry), value);
  if (show_icon) embed_entry_set_good (GTK_ENTRY (entry), TRUE);
  g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (callback), embed);
  set_tooltips (label, entry, tooltiptext);
  gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, row, row+1);
}



static void
add_fontbutton (EmbedPlugin *embed, GtkWidget *table, gint row,
                const gchar *value, gpointer callback,
                const gchar *labeltext, const gchar *tooltiptext)
{
  GtkWidget *label, *button;
  button = gtk_font_button_new ();
  label = add_label (table, row, button, labeltext);
  if (value) gtk_font_button_set_font_name (GTK_FONT_BUTTON (button), value);
  g_signal_connect (G_OBJECT (button), "font-set",
                    G_CALLBACK (callback), embed);
  set_tooltips (label, button, tooltiptext);
  gtk_table_attach_defaults (GTK_TABLE (table), button, 1, 2, row, row+1);
}



static void
add_spinbutton (EmbedPlugin *embed, GtkWidget *table, gint row,
                gint value, gpointer callback,
                const gchar *labeltext, const gchar *tooltiptext)
{
  GtkWidget *label, *button;
  button = gtk_spin_button_new_with_range (0, G_MAXINT, 1);
  label = add_label (table, row, button, labeltext);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (button), value);
  g_signal_connect (G_OBJECT (button), "value-changed",
                    G_CALLBACK (callback), embed);
  set_tooltips (label, button, tooltiptext);
  gtk_table_attach_defaults (GTK_TABLE (table), button, 1, 2, row, row+1);
}



static void
add_checkbutton (EmbedPlugin *embed, GtkWidget *table, gint row,
                 gboolean value, gpointer callback,
                 const gchar *labeltext, const gchar *tooltiptext)
{
  GtkWidget *button = gtk_check_button_new_with_mnemonic (labeltext);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), value);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (callback), embed);
  gtk_widget_set_tooltip_text (button, tooltiptext);
  gtk_table_attach_defaults (GTK_TABLE (table), button, 1, 2, row, row+1);
}



void
embed_configure (XfcePanelPlugin *plugin, EmbedPlugin *embed)
{
  GtkWidget *dialog, *content, *table;

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

  table = add_frame (content, 2, _("Application Launching"));
  add_label (table, 0, NULL,
      _("If a window is not found (or there are no criteria), a command can\n"
        "optionally be launched. The command can either result in a window\n"
        "that matches the below criteria, or it can use the socket ID passed\n"
        "to it (" EMBED_LAUNCH_CMD_SOCKET ") to embed itself automatically."));
  /* launch_cmd */
  add_entry (embed, table, 1, embed->launch_cmd, TRUE, embed_launch_cmd_changed,
           _("L_aunch command"),
           _("Leave blank to not launch anything\n"
             EMBED_LAUNCH_CMD_SOCKET " expands to the socket ID"));

  /* poll_delay */
  /* No UI element. Generally polling is unnecessary, unless you have a very
   * strange window that you're trying to match that is not uniquely
   * identifiable when it is mapped. */


  table = add_frame (content, 4, _("Selection Criteria"));
  add_label (table, 0, NULL,
      _("The window to embed must match all of the non-blank criteria.\n"
        "Leave everything blank to rely on a launch command with socket ID."));
  /* proc_name */
  add_entry (embed, table, 1, embed->proc_name, FALSE, embed_proc_name_changed,
           _("_Process name"),
           _("Match the window's application's process name\n"
             "Leave blank if it is not a criterion"));

  /* window_class */
  add_entry (embed, table, 2, embed->window_class, FALSE,
             embed_window_class_changed,
           _("_Window class"),
           _("Match the window's class\n"
             "Leave blank if it is not a criterion"));

  /* window_regex */
  add_entry (embed, table, 3, embed->window_regex, TRUE,
             embed_window_regex_changed,
           _("Window _title"),
           _("Match the window's title using a REGEX\n"
             "Leave blank if it is not a criterion"));


  table = add_frame (content, 3, _("Display"));
  /* label_fmt */
  add_entry (embed, table, 0, embed->label_fmt, FALSE, embed_label_fmt_changed,
           _("_Label format"),
           _("Leave blank to hide the label\n"
             EMBED_LABEL_FMT_TITLE " expands to the embedded window's title"));

  /* label font */
  add_fontbutton (embed, table, 1, embed->label_font, embed_label_font_changed,
                _("Label _font"),
                _("Choose the label font"));

  /* min_size */
  add_spinbutton (embed, table, 2, embed->min_size, embed_min_size_changed,
                _("Minimum _size (px)"),
                _("Minimum size of the embedded window\n"
                  "Set to 0 to keep the original window size"));

  /* expand */
  add_checkbutton (embed, table, 3, embed->expand, embed_expand_toggled,
                 _("_Expand"),
                 _("Use up all available panel space"));


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
  gtk_widget_show_all (dialog);
}
