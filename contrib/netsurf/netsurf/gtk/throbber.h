/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
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

#ifndef __GTK_THROBBER_H__
#define __GTK_THROBBER_H__

#include <gtk/gtk.h>

struct nsgtk_throbber
{
	int		nframes;	/**< Number of frames in the throbber */
	GdkPixbuf	**framedata;
};

extern struct nsgtk_throbber *nsgtk_throbber;

bool nsgtk_throbber_initialise_from_png(const int frames, char** frame_files);
void nsgtk_throbber_finalise(void);

#endif /* __GTK_THROBBER_H__ */
