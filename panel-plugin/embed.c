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
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-hvbox.h>

#include "ewmh.h"
#include "embed.h"
#include "embed-dialogs.h"

/* default settings */
#define DEFAULT_PROC_NAME    NULL
#define DEFAULT_WINDOW_REGEX NULL
#define DEFAULT_WINDOW_CLASS NULL
#define DEFAULT_LABEL_FMT    _("Embed")
#define DEFAULT_POLL_DELAY   0
#define DEFAULT_MIN_SIZE     EMBED_MIN_SIZE_MATCH_WINDOW
#define DEFAULT_EXPAND       TRUE



/* prototypes */
static void
embed_construct (XfcePanelPlugin *plugin);
static void
embed_popout (GtkMenuItem *popout_menu, EmbedPlugin *embed);
static void
embed_destroyed (EmbedPlugin *embed);
static void
embed_add_socket (EmbedPlugin *embed, gboolean update_size);
static void
embed_add_fake_socket (EmbedPlugin *embed);


/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (embed_construct);


/* Save the plugin settings. */
void
embed_save (XfcePanelPlugin *plugin, EmbedPlugin *embed)
{
  XfceRc *rc;
  gchar  *file;

  /* get the config file location */
  file = xfce_panel_plugin_save_location (plugin, TRUE);

  if (G_UNLIKELY (file == NULL)) {
    DBG ("Failed to open config file");
    return;
  }

  /* open the config file, read/write */
  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  if (G_LIKELY (rc != NULL)) {
    /* save the settings */
    DBG (".");
    if (embed->proc_name)
      xfce_rc_write_entry    (rc, "proc_name",   embed->proc_name);
    if (embed->window_regex)
      xfce_rc_write_entry    (rc, "window_regex", embed->window_regex);
    if (embed->window_class)
      xfce_rc_write_entry    (rc, "window_class", embed->window_class);
    if (embed->label_fmt)
      xfce_rc_write_entry    (rc, "label_fmt",   embed->label_fmt);
    xfce_rc_write_int_entry  (rc, "poll_delay",  embed->poll_delay);
    xfce_rc_write_int_entry  (rc, "min_size",    embed->min_size);
    xfce_rc_write_bool_entry (rc, "expand",      embed->expand);

    /* close the rc file */
    xfce_rc_close (rc);
  }
}



/* Read in the plugin settings. */
static void
embed_read (EmbedPlugin *embed)
{
  XfceRc      *rc;
  gchar       *file;

  /* get the plugin config file location */
  file = xfce_panel_plugin_save_location (embed->plugin, TRUE);

  if (G_LIKELY (file != NULL)) {
    /* open the config file, readonly */
    rc = xfce_rc_simple_open (file, TRUE);

    /* cleanup */
    g_free (file);

    if (G_LIKELY (rc != NULL)) {
      /* read the settings */
      embed->proc_name = g_strdup (xfce_rc_read_entry (rc,
                              "proc_name", DEFAULT_PROC_NAME));
      embed->window_regex = g_strdup (xfce_rc_read_entry (rc,
                              "window_regex", DEFAULT_WINDOW_REGEX));
      embed->window_class = g_strdup (xfce_rc_read_entry (rc,
                              "window_class", DEFAULT_WINDOW_CLASS));
      embed->label_fmt = g_strdup (xfce_rc_read_entry (rc,
                              "label_fmt", DEFAULT_LABEL_FMT));
      embed->poll_delay = xfce_rc_read_int_entry (rc,
                              "poll_delay", DEFAULT_POLL_DELAY);
      embed->min_size = xfce_rc_read_int_entry (rc,
                              "min_size", DEFAULT_MIN_SIZE);
      embed->expand = xfce_rc_read_bool_entry (rc,
                              "expand", DEFAULT_EXPAND);

      /* cleanup */
      xfce_rc_close (rc);

      /* leave the function, everything went well */
      return;
    }
  }

  /* something went wrong, apply default values */
  DBG ("Applying default settings");

  embed->proc_name   = g_strdup (DEFAULT_PROC_NAME);
  embed->window_regex = g_strdup (DEFAULT_WINDOW_REGEX);
  embed->window_class = g_strdup (DEFAULT_WINDOW_CLASS);
  embed->label_fmt   = g_strdup (DEFAULT_LABEL_FMT);
  embed->poll_delay  = DEFAULT_POLL_DELAY;
  embed->min_size    = DEFAULT_MIN_SIZE;
  embed->expand      = DEFAULT_EXPAND;
}



/* Remove any old separator and add a new one, if necessary.
 * This is used to add a separator widget, although currently as there is no
 * separator widget, nothing happens.  The function is still called in the
 * correct places though, so it is trivial to add. */
static void
embed_update_separator (EmbedPlugin* embed, GtkOrientation orientation)
{
}



/* Creates a new EmbedPlugin structure and initializes everything. */
static EmbedPlugin *
embed_new (XfcePanelPlugin *plugin)
{
  EmbedPlugin   *embed;
  GtkOrientation  orientation;

  /* allocate memory for the plugin structure */
  embed = panel_slice_new0 (EmbedPlugin);

  /* Since a plug can spontaneously connect, assume we have a gtkplug until we
   * explicitly steal a window. */
  embed->plug_is_gtkplug = TRUE;

  /* pointer to plugin */
  embed->plugin = plugin;

  /* read the user settings */
  embed_read (embed);

  /* set expand */
  xfce_panel_plugin_set_expand (plugin, embed->expand);

  /* Compile the window name regex */
  if (embed->window_regex)
    embed->window_regex_comp = g_regex_new (embed->window_regex,
                                            G_REGEX_OPTIMIZE, 0, NULL);

  /* Open X11 display */
  embed->disp = XOpenDisplay (NULL);

  /* get the current orientation */
  orientation = xfce_panel_plugin_get_orientation (plugin);

  /* main panel hvbox */
  embed->hvbox = xfce_hvbox_new (orientation, FALSE, 2);
  gtk_widget_show (embed->hvbox);

  /* separator */
  embed_update_separator (embed, orientation);

  /* label */
  embed->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (embed->hvbox), embed->label, FALSE, FALSE, 0);
  embed_update_label (embed);

  /* socket */
  embed_add_socket (embed, FALSE);

  /* pop out menu item, not shown by default */
  embed->popout_menu = gtk_image_menu_item_new_with_mnemonic (_("Pop _Out"));

  /* embed menu item, shown by default. */
  embed->embed_menu = gtk_image_menu_item_new_with_mnemonic (_("_Embed"));
  gtk_widget_show (embed->embed_menu);

  /* focus menu item, not shown by default */
  embed->focus_menu = gtk_image_menu_item_new_with_mnemonic (_("_Focus"));

  return embed;
}



/* Frees up the EmbedPlugin structure, popping out the embedded window if there
 * still is one embedded. */
static void
embed_free (XfcePanelPlugin *plugin, EmbedPlugin *embed)
{
  GtkWidget *dialog;

  DBG (".");

  /* Don't hold onto the embedded window, as if it is a normal window, it will
   * get lost if we don't reparent it. */
  embed_popout (GTK_MENU_ITEM (embed->popout_menu), embed);

  /* check if the dialog is still open. if so, destroy it */
  dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
  if (G_UNLIKELY (dialog != NULL))
    gtk_widget_destroy (dialog);

  /* destroy the panel widgets */
  gtk_widget_destroy (embed->hvbox);

  /* cleanup the settings */
  g_free (embed->proc_name);
  g_free (embed->window_regex);
  g_free (embed->window_class);
  g_free (embed->label_fmt);

  /* Close the X11 display */
  XCloseDisplay (embed->disp);

  /* Release the compiled regex */
  if (embed->window_regex_comp)
    g_regex_unref (embed->window_regex_comp);

  /* free the plugin structure */
  panel_slice_free (EmbedPlugin, embed);
}



/* Callback for the focus menu button. Activates focus on the plugin. */
static void
embed_focus_menu (GtkMenuItem *focus_menu, EmbedPlugin *embed)
{
  if (embed->plug_is_gtkplug) {
    if (embed->socket)
      xfce_panel_plugin_focus_widget (embed->plugin, embed->socket);
  } else if (embed->plug) {
    focus_window (embed->disp, embed->plug);
  }
}



/* Callback when the orientation of the panel is changed. */
static void
embed_orientation_changed (XfcePanelPlugin *plugin,
                           GtkOrientation   orientation,
                           EmbedPlugin     *embed)
{
  /* change the orienation of the box */
  xfce_hvbox_set_orientation (XFCE_HVBOX (embed->hvbox), orientation);
  embed_update_separator (embed, orientation);
}



/* Callback when the size of the panel is changed.
 * Updates the requested sizes of the plugin and the socket window.
 * Also used manually to apply size changes when the plug changes. */
static gboolean
embed_size_changed (XfcePanelPlugin *plugin, gint size, EmbedPlugin *embed)
{
  GtkOrientation orientation;
  gint altsize;

  /* get the orientation of the plugin */
  orientation = xfce_panel_plugin_get_orientation (plugin);

  /* set the socket widget size.
   * For the adjustable dimension, use the minimum size if set, otherwise if it
   * is set to the window size and we don't have a window embedded, set to
   * square. */
  if (embed->min_size == EMBED_MIN_SIZE_MATCH_WINDOW)
    altsize = size;
  else
    altsize = embed->min_size;
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    /* If requested, use the window size detected earlier. This will be -1 for
     * true GtkPlugs, which will pass through the window size request. */
    if (embed->min_size == EMBED_MIN_SIZE_MATCH_WINDOW && embed->plug)
      altsize = embed->plug_width;
    gtk_widget_set_size_request (GTK_WIDGET (embed->socket), altsize, size);
    /* Widget altsize should just adapt, since it might include a label and/or
     * separator. */
    gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
  } else {
    /* Same for vertical orientation. */
    if (embed->min_size == EMBED_MIN_SIZE_MATCH_WINDOW && embed->plug)
      altsize = embed->plug_height;
    gtk_widget_set_size_request (GTK_WIDGET (embed->socket), size, altsize);
    gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);
  }

  /* we handled the orientation */
  return TRUE;
}



/* Convenience function to call embed_size_changed. */
void
embed_size_changed_simple (EmbedPlugin *embed)
{
  embed_size_changed (embed->plugin,
                      xfce_panel_plugin_get_size (embed->plugin), embed);
}



/* Updates the text of the label, using the label_fmt. */
void
embed_update_label (EmbedPlugin *embed)
{
  /* Only show the label if the format is non-empty. */
  if (embed->label_fmt && *embed->label_fmt) {
    gchar *titlepos;
    /* If we have an embedded plug and the label wants the title, process it. */
    if (embed->plug &&
        (titlepos = strstr (embed->label_fmt, EMBED_LABEL_FMT_TITLE))) {
      gchar *title, *label;
      title = get_window_title (embed->disp, embed->plug);
      /* Construct the label, replacing the EMBED_LABEL_FMT_TITLE with the
       * actual window title. */
      label = g_strdup_printf ("%.*s%s%s",
          (gint)(titlepos - embed->label_fmt), embed->label_fmt,
          title, titlepos + strlen (EMBED_LABEL_FMT_TITLE));
      gtk_label_set_text (GTK_LABEL (embed->label), label);
      g_free (title);
      g_free (label);
    } else {
      /* Othewise just display the format string directly. */
      gtk_label_set_text (GTK_LABEL (embed->label), embed->label_fmt);
    }
    gtk_widget_show (embed->label);
  } else {
    gtk_widget_hide (embed->label);
  }
}



/* Performs a single pass through the windows managed by the window manager,
 * searching for the first window that meets all of the criteria.
 * If one is found, it embeds it.
 * Returns TRUE if no viable plug is found, so that if this is used as a timer
 * callback, it will keep polling. */
static gboolean
embed_search (EmbedPlugin *embed)
{
  Window *client_list;
  gulong client_list_size;
  gulong i;

  DBG (".");
  
  /* Grab a list of windows managed by the window manager.
   * They will not necessarily be on the current workspace. */
  if ((client_list = get_client_list (embed->disp, &client_list_size))) {
    /* Loop through each window */
    for (i = 0; i < client_list_size / sizeof (Window); i++) {
      gchar *str;
      gboolean match;
      match = TRUE;

      /* AND-match each specified criteria, starting with the presumably
       * lightest-weight/most-specific ones first. */
      if (match && embed->proc_name && embed->proc_name[0]) {
        str = get_client_proc (embed->disp, client_list[i]);
        match = !g_strcmp0 (embed->proc_name, str);
        g_free (str);
      }
      if (match && embed->window_class && embed->window_class[0]) {
        str = get_window_class (embed->disp, client_list[i]);
        match = !g_strcmp0 (embed->window_class, str);
        g_free (str);
      }
      if (match && embed->window_regex && embed->window_regex[0]
          && embed->window_regex_comp) {
        str = get_window_title (embed->disp, client_list[i]);
        match = g_regex_match (embed->window_regex_comp, str, 0, NULL);
        g_free (str);
      }

      /* If it's a match, make a fake socket and embed the window in it. */
      if (match) {
        /* Destroy the true GtkSocket if it exists, as don't need it. */
        if (embed->socket)
          gtk_widget_destroy (embed->socket);
        embed->plug_is_gtkplug = FALSE;
        embed->plug = client_list[i];
        /* Store the old size of the window for both restoring and for deciding
         * on the panel size. */
        get_window_size (embed->disp, client_list[i],
                         &embed->plug_width, &embed->plug_height);
        DBG ("found window 0x%X of geometry %dx%d",
             embed->plug, embed->plug_width, embed->plug_height);
        /* Make the fake socket and embed the window. */
        embed_add_fake_socket (embed);
        break;
      }
    }
    g_free (client_list);
  }

  /* Return TRUE if we haven't found a plug yet, so that the function is called
   * again. */
  return embed->plug == 0;
}



/* Starts the search for a viable plug. Does one initial search, and then sets
 * up X11 monitoring and possibly polling if no plug is found right away.
 * Does not start a search if there are no criteria, as that would be stupid. */
static void
embed_start_search (GtkWidget *socket, EmbedPlugin *embed)
{
  if (embed->disable_search) {
    DBG ("search disabled");
    return;
  }
  /* Make sure there's a non-empty search criteria */
  if (!(
        (embed->proc_name && embed->proc_name[0]) ||
        (embed->window_regex && embed->window_regex[0]
         && embed->window_regex_comp) ||
        (embed->window_class && embed->window_class[0])
       )) {
    DBG ("no search criteria specified");
    return;
  }
  /* TODO: handle the case where we want to launch an application that will
   * generate a plug */
  if (embed_search (embed)) {
    /* Reset the _NET_CLIENT_LIST detector */
    embed->monitor_saw_net_client_list = FALSE;
    /* Watch for property changes (primarily the window list ones) on the root
     * window.
     * Note that gdk_x11_get_default_xdisplay () is not the same as the display
     * stored in embed->disp.  embed->disp will be valid through the destruction
     * of the plugin, but GDK expects its own Display reference to access its
     * internal hashmaps for routing events. */
    XSelectInput (gdk_x11_get_default_xdisplay (),
                  gdk_x11_get_default_root_xwindow (), PropertyChangeMask);
    if (embed->poll_delay > 0)
      embed->search_timer = g_timeout_add (embed->poll_delay,
                                          (GSourceFunc)embed_search, embed);
  }
}



/* Stops the search process, disabling both X11 monitoring and polling. */
void
embed_stop_search (EmbedPlugin *embed)
{
  /* Set the event mask to 0 to stop receiving X11 events for the root window.
   */
  XSelectInput (gdk_x11_get_default_xdisplay (),
                gdk_x11_get_default_root_xwindow (), 0);
  if (embed->search_timer) {
    g_source_remove (embed->search_timer);
    embed->search_timer = 0;
  }
}



/* Callback for the embed menu button. (Re)starts searching. */
static void
embed_embed_menu (GtkMenuItem *embed_menu, EmbedPlugin *embed)
{
  embed->disable_search = FALSE;
  embed_stop_search (embed);
  embed_start_search (embed->socket, embed);
}



/* X11 event monitor for a plug. Detects window title changes, unmap, and
 * destroy events. */
static GdkFilterReturn
embed_plug_filter (XPropertyEvent *xevent, GdkEvent *_, EmbedPlugin *embed)
{
  if (xevent->type == PropertyNotify) {
    /* To avoid double-handling window name changes, if we see a _NET_WM_NAME we
     * will stop responding to WM_NAME events.
     * This is reset every time a new window is embedded, in case the window
     * manager changed. */
    if (xevent->atom == XInternAtom (xevent->display, "_NET_WM_NAME", False)) {
      embed->monitor_saw_net_wm_name = TRUE;
      embed_update_label (embed);
    } else if (!embed->monitor_saw_net_wm_name &&
        xevent->atom == XInternAtom (xevent->display, "WM_NAME", False)) {
      embed_update_label (embed);
    }
  } else if (xevent->type == UnmapNotify || xevent->type == DestroyNotify) {
    /* The plug window was destroyed, and not by us! */
    DBG ("destroyed");
    embed_destroyed (embed);
  }
  return GDK_FILTER_CONTINUE;
}



/* Callback for when a plug is added.  This is either automatically called by
 * the GtkSocket, or manually when a fake socket embeds the plug.
 * Does everything that should happen when a plug is added, including all UI and
 * monitoring/polling changes. */
static void
embed_plug_added (GtkWidget *socket, EmbedPlugin *embed)
{
  DBG (".");

  /* Flip the menu items */
  gtk_widget_hide (embed->embed_menu);
  gtk_widget_show (embed->popout_menu);
  gtk_widget_show (embed->focus_menu);

  /* Stop any searching that is going on */
  embed_stop_search (embed);

  /* If we just got plugged by a gtkplug, set the "known" width and height to -1
   * so that the socket will completely pass through the minimum size request if
   * the user so desires */
  if (embed->plug_is_gtkplug)
    embed->plug_width = embed->plug_height = -1;

  /* Monitor the plug for destruction, along with title changes if we need it
   * for the label. */
  if (embed->plug) {
    /* Construct a GdkWindow wrapper around the plug window.
     * Throughout here we must use GDK's Display reference, not our own, since
     * GDK uses this to determine whether to pass on events or not. */
    embed->plug_window = gdk_x11_window_foreign_new_for_display (
        gdk_display_get_default (), embed->plug);
    if (embed->plug_window) {
      /* Monitor for unmap/destroy events. If the label is based on the title,
       * also monitor property change events. */
      glong monitor_mask = StructureNotifyMask;
      if (embed->label_fmt && strstr (embed->label_fmt, EMBED_LABEL_FMT_TITLE))
        monitor_mask |= PropertyChangeMask;
      /* Reset the _NET_WM_NAME detector. */
      embed->monitor_saw_net_wm_name = FALSE;
      /* Set up the callback function, and start monitoring for the specified X
       * events. */
      gdk_window_add_filter (embed->plug_window,
                             (GdkFilterFunc)embed_plug_filter, embed);
      XSelectInput (gdk_x11_get_default_xdisplay (),
                    embed->plug, monitor_mask);
    }
  }

  /* Update the label */
  embed_update_label (embed);

  /* Update the size of the panel. */
  embed_size_changed_simple (embed);
}



/* Callback wrapper function for embed_add_socket. */
static gboolean
embed_add_socket_and_resize (EmbedPlugin *embed)
{
  embed_add_socket (embed, TRUE);
  return FALSE;
}



/* Callback for when a plug is removed.  This is either automatically called by
 * the GtkSocket, or manually when a plug is popped out or destroyed.
 * Does everything that should happen when a plug is removed, including all UI
 * and monitoring/polling changes.
 * Assumes that the GtkSocket is about to be destroyed right after the function
 * returns, so it adds an idle callback that creates a new one. */
static gboolean
embed_plug_removed (GtkWidget *socket, EmbedPlugin *embed)
{
  DBG (".");

  g_assert (embed->socket);

  /* Flip the menu items */
  gtk_widget_hide (embed->popout_menu);
  gtk_widget_hide (embed->focus_menu);
  gtk_widget_show (embed->embed_menu);

  /* Assume the socket will be destroyed after this returns, so get rid of our
   * reference. */
  embed->socket = NULL;
  /* Stop monitoring the plug window for changes */
  if (embed->plug_window) {
    XSelectInput (gdk_x11_get_default_xdisplay (), embed->plug, 0);
    gdk_window_remove_filter (embed->plug_window, 
                              (GdkFilterFunc)embed_plug_filter, embed);
    g_object_unref (embed->plug_window);
    embed->plug_window = NULL;
  }
  /* Reset info */
  embed->plug = 0;
  embed->plug_is_gtkplug = TRUE;
  embed_update_label (embed);
  /* Create a new socket once this one is destroyed */
  g_idle_add ((GSourceFunc)embed_add_socket_and_resize, embed);
  /* Returning false will destroy the socket */
  return FALSE;
}



/* Callback for when the size of the socket is determined.
 * Used to resize the embedded window to match the socket. */
static void
embed_size_allocate (GtkSocket *socket, GdkRectangle *allocation,
                     EmbedPlugin *embed)
{
  if (!embed->plug || embed->plug_is_gtkplug)
    return;
  /* Only manually resize if the embedded window is just a normal window. */
  resize_window (embed->disp, embed->plug,
                 allocation->width, allocation->height);
}



/* Adds a GtkSocket to the plugin and hooks up the signals, optionally updating
 * the size of the plugin to match. Generally update_size should be true unless
 * the plugin is being initialized.
 * Once the GtkSocket is realized, a search is started. This can be prevented by
 * setting embed->disable_search to true. */
static void
embed_add_socket (EmbedPlugin *embed, gboolean update_size)
{
  if (embed->socket) {
    DBG ("socket already exists");
    return;
  }

  embed->socket = gtk_socket_new ();

  g_signal_connect (G_OBJECT (embed->socket), "plug-added",
                    G_CALLBACK (embed_plug_added), embed);
  g_signal_connect (G_OBJECT (embed->socket), "plug-removed",
                    G_CALLBACK (embed_plug_removed), embed);
  g_signal_connect (G_OBJECT (embed->socket), "realize",
                    G_CALLBACK (embed_start_search), embed);

  /* Add it to the plugin hvbox */
  gtk_widget_show (embed->socket);
  gtk_box_pack_start (GTK_BOX (embed->hvbox), embed->socket, TRUE, TRUE, 0);

  /* Update the size of the plugin if needed. */
  if (update_size)
    embed_size_changed_simple (embed);
}



/* Creates a simple GTK widget (GtkDrawingArea) to dumbly house the embedded
 * plug window, and reparents the plug window.  This is done because a GtkSocket
 * will destroy a plug when it is removed, which doesn't play nice when we're
 * embedding random (unsuspecting) windows.
 * Assumes the true GtkSocket was already destroyed. */
static void
embed_add_fake_socket (EmbedPlugin *embed)
{
  /* GtkDrawingArea is pretty much as simple as you can get, and it is not a
   * container (since containers would destroy their children when destroyed) */
  embed->socket = gtk_drawing_area_new ();

  /* We use the size-allocate signal to keep the size of the plug up-to-date. */
  g_signal_connect (G_OBJECT (embed->socket), "size-allocate",
                    G_CALLBACK (embed_size_allocate), embed);

  /* Add it to the plugin hvbox */
  gtk_widget_show (embed->socket);
  gtk_box_pack_start (GTK_BOX (embed->hvbox), embed->socket, TRUE, TRUE, 0);

  /* Embed the plug window.
   * First we have to ensure that it is on the current workspace, otherwise
   * things tend to break (the window gets separated from the decorations) */
  show_window (embed->disp, embed->plug);
  reparent_window (embed->disp, embed->plug,
      gdk_x11_drawable_get_xid (gtk_widget_get_window (embed->socket)), 0, 0);

  embed_plug_added (embed->socket, embed);
}



/* Pops out any plugs that are embedded and destroys the socket.
 * This is called automatically by the popout menu item, and can be called
 * manually even if no plug is actually embedded at the time.
 * If the plug is just a normal window, the window will be reparented to the top
 * level and its size restored.
 * Since we assume the pop out was manual, we also disable searching for a new
 * plug to embed until the user intervenes again.
 */
static void
embed_popout (GtkMenuItem *popout_menu, EmbedPlugin *embed)
{
  GtkWidget *socket;

  DBG (".");

  if (!embed->plug_is_gtkplug) {
    /* Since we're not hosting a gtkplug, we should reparent the window so we
     * don't break the program we were hosting.
     * We reparent it both using GDK and using X11: GDK is to ensure that the
     * destruction of the socket doesn't destroy the plug window. X11 is to
     * ensure that the window is reparented if the panel plugin is closing.
     * We do X11 first as it also resizes in one fell swoop. */
    make_window_toplevel (embed->disp, embed->plug,
                          embed->plug_width, embed->plug_height);
    gdk_window_reparent (embed->plug_window,
                         gdk_get_default_root_window (), 0, 0);
  }
  /* Don't enable searching for a new window. */
  embed->disable_search = TRUE;
  /* destroy socket and make a new one. destroy does not trigger plug_removed */
  socket = embed->socket;
  embed_plug_removed (embed->socket, embed);
  gtk_widget_destroy (socket);
}


/* Callback for when the embedded plug window is ungracefully destroyed, not by
 * our doing, and not as part of XEmbed.
 * Calls the removed callback and destroys the socket. */
static void
embed_destroyed (EmbedPlugin *embed)
{
  GtkWidget *socket;
  if ((socket = embed->socket) == NULL)
    return;
  embed_plug_removed (embed->socket, embed);
  gtk_widget_destroy (socket);
}



/* Pop out whatever is embedded and start a new search.
 * Called when the window criteria have been updated. */
void
embed_search_again (EmbedPlugin *embed)
{
  embed_popout (GTK_MENU_ITEM (embed->popout_menu), embed);
  embed_embed_menu (GTK_MENU_ITEM (embed->embed_menu), embed);
}



/* X11 event monitor for the root window. Detects when windows become managed by
 * the window manager so that we can scan for matches. */
static GdkFilterReturn
embed_root_filter (XPropertyEvent *xevent, GdkEvent *_, EmbedPlugin *embed)
{
  if (!embed->plug && xevent->type == PropertyNotify) {
    /* To avoid double-handling window list changes, if we see a
     * _NET_CLIENT_LIST we will stop responding to _WIN_CLIENT_LIST events.
     * This is reset every time a new search is started, in case the window
     * manager changed. */
    if (xevent->atom == XInternAtom (xevent->display,
                                     "_NET_CLIENT_LIST", False)) {
      embed->monitor_saw_net_client_list = TRUE;
      embed_search (embed);
    } else if (!embed->monitor_saw_net_client_list &&
        xevent->atom == XInternAtom (xevent->display,
                                     "_WIN_CLIENT_LIST", False)) {
      embed_search (embed);
    }
  }
  return GDK_FILTER_REMOVE;
}



/* Entry point for the panel plugin. */
static void
embed_construct (XfcePanelPlugin *plugin)
{
  EmbedPlugin *embed;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* create the plugin */
  embed = embed_new (plugin);

  /* add the hvbox to the panel */
  gtk_container_add (GTK_CONTAINER (plugin), embed->hvbox);

  /* show the panel's right-click menu on this hvbox */
  xfce_panel_plugin_add_action_widget (plugin, embed->hvbox);

  /* connect plugin signals */
  g_signal_connect (G_OBJECT (plugin), "free-data",
                    G_CALLBACK (embed_free), embed);

  g_signal_connect (G_OBJECT (plugin), "save",
                    G_CALLBACK (embed_save), embed);

  g_signal_connect (G_OBJECT (plugin), "size-changed",
                    G_CALLBACK (embed_size_changed), embed);

  g_signal_connect (G_OBJECT (plugin), "orientation-changed",
                    G_CALLBACK (embed_orientation_changed), embed);

  /* Add the "pop out" menu item */
  xfce_panel_plugin_menu_insert_item (plugin,
                                      GTK_MENU_ITEM (embed->popout_menu));
  g_signal_connect (G_OBJECT (embed->popout_menu), "activate",
                    G_CALLBACK (embed_popout), embed);

  /* Add the "embed" menu item */
  xfce_panel_plugin_menu_insert_item (plugin,
                                      GTK_MENU_ITEM (embed->embed_menu));
  g_signal_connect (G_OBJECT (embed->embed_menu), "activate",
                    G_CALLBACK (embed_embed_menu), embed);

  /* Add the "focus" menu item */
  xfce_panel_plugin_menu_insert_item (plugin,
                                      GTK_MENU_ITEM (embed->focus_menu));
  g_signal_connect (G_OBJECT (embed->focus_menu), "activate",
                    G_CALLBACK (embed_focus_menu), embed);


  /* show the configure menu item and connect signal */
  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                    G_CALLBACK (embed_configure), embed);

  /* show the about menu item and connect signal */
  xfce_panel_plugin_menu_show_about (plugin);
  g_signal_connect (G_OBJECT (plugin), "about",
                    G_CALLBACK (embed_about), NULL);

  /* Register our own event filter to avoid having to poll X11 properties.
   * No events will actually trigger until we call XSelectInput elsewhere. */
  gdk_window_add_filter (gdk_get_default_root_window (),
                         (GdkFilterFunc)embed_root_filter, embed);
}
