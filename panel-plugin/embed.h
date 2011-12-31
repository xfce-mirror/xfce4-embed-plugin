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

#ifndef __EMBED_H__
#define __EMBED_H__

G_BEGIN_DECLS

/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget       *hvbox;
    GtkWidget       *label;
    GtkWidget       *socket;
    GtkWidget       *popout_menu;
    GtkWidget       *embed_menu;
    GtkWidget       *focus_menu;

    /* panel data */
    GdkNativeWindow  plug;
    GdkWindow       *plug_window;
    gint             plug_width;
    gint             plug_height;
    gboolean         plug_is_gtkplug;

    Display         *disp;

    guint            search_timer;
    gboolean         disable_search;
    gboolean         monitor_saw_net_client_list;
    gboolean         monitor_saw_net_wm_name;

    GRegex          *window_regex_comp;

    gboolean         criteria_updated;

    /* embed settings */
    gchar           *proc_name;
    gchar           *window_regex;
    gchar           *window_class;
    gchar           *launch_cmd;
    gchar           *label_fmt;
    gchar           *label_font;
    gint             poll_delay;
    gint             min_size;
    gboolean         expand;
}
EmbedPlugin;

/* Special values for EmbedPlugin::min_size */
#define EMBED_MIN_SIZE_MATCH_WINDOW 0

/* Special contents of launch_cmd */
#define EMBED_LAUNCH_CMD_SOCKET "%s"

/* Special contents of label_fmt */
#define EMBED_LABEL_FMT_TITLE "%t"

void
embed_search_again (EmbedPlugin *embed);
void
embed_stop_search (EmbedPlugin *embed);
void
embed_update_label (EmbedPlugin *embed);
void
embed_update_label_font (EmbedPlugin *embed);
void
embed_size_changed_simple (EmbedPlugin *embed);
void
embed_save (XfcePanelPlugin *plugin, EmbedPlugin *embed);

G_END_DECLS

#endif /* !__EMBED_H__ */
