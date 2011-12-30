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

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "ewmh.h"

/* EWMH functions adapted from wmctrl project. */

#define MAX_PROPERTY_VALUE_LEN 4096


static gchar *get_property (Display *disp, Window win, Atom xa_prop_type,
                            gchar *prop_name, gulong *size)
{
    Atom xa_prop_name;
    Atom xa_ret_type;
    gint ret_format;
    gulong ret_nitems;
    gulong ret_bytes_after;
    gulong tmp_size;
    guchar *ret_prop;
    gchar *ret;
    
    xa_prop_name = XInternAtom(disp, prop_name, False);
    
    /* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
     *
     * long_length = Specifies the length in 32-bit multiples of the
     *               data to be retrieved.
     *
     * NOTE:  see 
     * http://mail.gnome.org/archives/wm-spec-list/2003-March/msg00067.html
     * In particular:
     *
     * 	When the X window system was ported to 64-bit architectures, a
     * rather peculiar design decision was made. 32-bit quantities such
     * as Window IDs, atoms, etc, were kept as longs in the client side
     * APIs, even when long was changed to 64 bits.
     *
     */
    if (XGetWindowProperty(disp, win, xa_prop_name, 0,
            MAX_PROPERTY_VALUE_LEN / 4, False,
            xa_prop_type, &xa_ret_type, &ret_format,     
            &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        DBG("Cannot get %s property.\n", prop_name);
        return NULL;
    }
  
    if (xa_ret_type != xa_prop_type) {
        DBG("Invalid type of %s property.\n", prop_name);
        XFree(ret_prop);
        return NULL;
    }

    /* null terminate the result to make string handling easier */
    tmp_size = (ret_format / 8) * ret_nitems;
    /* Correct 64 Architecture implementation of 32 bit data */
    if(ret_format==32) tmp_size *= sizeof(glong)/4;
    ret = g_malloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) {
        *size = tmp_size;
    }
    
    XFree(ret_prop);
    return ret;
}

static void client_msg(Display *disp, Window win, gchar *msg, gulong data0) {
    XEvent event;
    glong mask = SubstructureRedirectMask | SubstructureNotifyMask;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(disp, msg, False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = 0;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    if (!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event))
        DBG("Cannot send %s event.", msg);
    XSync (disp, False);
}

Window *get_client_list (Display *disp, gulong *size)
{
    Window *client_list;

    if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp), 
                    XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
        if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
                        XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
            DBG("Cannot get client list properties. \n"
                  "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)\n");
            return NULL;
        }
    }

    return client_list;
}


gchar *get_client_proc (Display *disp, Window win)
{
    gulong *pid;
    gchar  proc[25];
    gchar *contents;
    gsize  length;
    gchar *start;
    gchar *end;
    gchar *procname;

    if ((pid = (gulong *)get_property(disp, win, XA_CARDINAL,
                                      "_NET_WM_PID", NULL)) == NULL)
        return NULL;

    procname = NULL;

    /* First attempt the full command line */
    g_snprintf (proc, 25, "/proc/%lu/cmdline", *pid);
    if ((g_file_get_contents (proc, &contents, &length, NULL))) {
        if (length) {
            contents[length-1] = '\0';
            start = strrchr (contents, '/');
            start = start ? start+1 : contents;
            procname = g_strdup (start);
        }
        g_free (contents);
    }
    if (procname == NULL) {
        /* Otherwise, grab the potentially-truncated version from stat */
        g_snprintf (proc, 25, "/proc/%lu/stat", *pid);
        if (g_file_get_contents (proc, &contents, &length, NULL)) {
            if (length) {
                contents[length-1] = '\0';
                start = strchr (contents, '(');
                if (start) {
                    end = strchr (start, ')');
                    if (end) {
                        *end = '\0';
                        procname = g_strdup (start+1);
                    }
                }
            }
            g_free (contents);
        }
    }

    g_free (pid);
    return procname;
}


gchar *get_window_title (Display *disp, Window win)
{
    gchar *title;
    gchar *wm_name;

    wm_name = get_property(disp, win, XInternAtom(disp, "UTF8_STRING", False),
                           "_NET_WM_NAME", NULL);

    if (wm_name) {
        if ((title = g_locale_from_utf8(wm_name, -1, NULL, NULL, NULL))) {
            g_free(wm_name);
            return title;
        }
        return wm_name;
    } else {
        return get_property(disp, win, XA_STRING, "WM_NAME", NULL);
    }
}


gchar *get_window_class (Display *disp, Window win)
{
    gchar *wm_class;
    unsigned long size;

    wm_class = get_property(disp, win, XA_STRING, "WM_CLASS", &size);
    if (wm_class) {
        gchar *p_0 = strchr(wm_class, '\0');
        if (wm_class + size - 1 > p_0) {
            *(p_0) = '.';
        }
    }

    return wm_class;
}


void get_window_size (Display *disp, Window win, gint *width, gint *height)
{
    Window root;
    gint relx, rely;
    guint bw, depth;
    XGetGeometry (disp, win, &root, &relx, &rely,
                  (guint *)width, (guint *)height, &bw, &depth);
}


void make_window_toplevel (Display *disp, Window win, gint width, gint height)
{
    reparent_window (disp, win, DefaultRootWindow (disp), width, height);
}

void reparent_window (Display *disp, Window win, Window parent,
                      gint width, gint height)
{
    XReparentWindow (disp, win, parent, 0, 0);
    resize_window (disp, win, width, height);
}

void resize_window (Display *disp, Window win, gint width, gint height)
{
    if (width > 0 && height > 0)
        XResizeWindow (disp, win, (guint)width, (guint)height);
    XSync (disp, False);
}

void show_window (Display *disp, Window win)
{
    gulong *cur_desktop = NULL;
    Window root = DefaultRootWindow(disp);

    if (! (cur_desktop = (unsigned long *)get_property(disp, root,
            XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
        if (! (cur_desktop = (unsigned long *)get_property(disp, root,
                XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
            DBG("Cannot get current desktop properties. "
                "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)");
            return;
        }
    }

    client_msg(disp, win, "_NET_WM_DESKTOP", *cur_desktop);
    g_free(cur_desktop);
}

void focus_window (Display *disp, Window win)
{
    XSetInputFocus (disp, win, RevertToNone, CurrentTime);
    XFlush (disp);
}
