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

#ifndef __EMBED_DIALOGS_H__
#define __EMBED_DIALOGS_H__

G_BEGIN_DECLS

void
embed_configure    (XfcePanelPlugin *plugin,
                     EmbedPlugin     *embed);

void
embed_about        (XfcePanelPlugin *plugin);

G_END_DECLS

#endif
