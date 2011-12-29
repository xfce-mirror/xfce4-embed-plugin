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

#ifndef __EWMH_H__
#define __EWMH_H__

G_BEGIN_DECLS

Window *get_client_list  (Display *disp, gulong *size);
gchar  *get_client_proc  (Display *disp, Window win);
gchar  *get_window_title (Display *disp, Window win);
gchar  *get_window_class (Display *disp, Window win);
void    get_window_size  (Display *disp, Window win, gint *width, gint *height);

void    make_window_toplevel (Display *disp, Window win);

G_END_DECLS

#endif /* !__EWMH_H__ */

