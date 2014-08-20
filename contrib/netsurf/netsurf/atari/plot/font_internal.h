/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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
 #ifdef WITH_INTERNAL_FONT_DRIVER
#ifndef FONT_PLOTTER_INTERNAL
#define FONT_PLOTTER_INTERNAL

#include "atari/plot/plot.h"

int ctor_font_plotter_internal( FONT_PLOTTER self );

struct fb_font_desc {
    const char *name;
    int width, height;
    const char *encoding;
    const uint32_t *data;
};


#endif
#endif
