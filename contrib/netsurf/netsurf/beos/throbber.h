/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#ifndef __BEOS_THROBBER_H__
#define __BEOS_THROBBER_H__

#include <Bitmap.h>

struct nsbeos_throbber
{
	int		nframes;	/**< Number of frames in the throbber */
	BBitmap	**framedata;
};

extern struct nsbeos_throbber *nsbeos_throbber;

bool nsbeos_throbber_initialise_from_gif(const char *fn);
bool nsbeos_throbber_initialise_from_png(const int frames, ...);
void nsbeos_throbber_finalise(void);

#endif /* __BEOS_THROBBER_H__ */
