/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NS_ATARI_STATUSBAR
#define NS_ATARI_STATUSBAR

#define STATUSBAR_HEIGHT 16
#define STATUSBAR_MAX_SLEN 255

struct s_statusbar
{
#ifdef WITH_COMPONENT_STATUSBAR
	COMPONENT * comp;
#endif
	char text[STATUSBAR_MAX_SLEN+1];
	size_t textlen;
	bool attached;
	short aes_win;
};


CMP_STATUSBAR sb_create( struct gui_window * gw );
void sb_destroy( CMP_STATUSBAR s );
void sb_set_text( CMP_STATUSBAR sb , const char * text );
void sb_attach(CMP_STATUSBAR sb, struct gui_window * gw);
#endif
