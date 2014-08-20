/*
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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

#ifndef FONT_PLOTTER_FREETYPE
#define FONT_PLOTTER_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include "utils/utf8.h"

/* defines for accesing the faces */
#define FONT_FACE_DEFAULT 0

#define FONT_FACE_SANS_SERIF 0
#define FONT_FACE_SANS_SERIF_BOLD 1
#define FONT_FACE_SANS_SERIF_ITALIC 2
#define FONT_FACE_SANS_SERIF_ITALIC_BOLD 3
#define FONT_FACE_MONOSPACE 4
#define FONT_FACE_MONOSPACE_BOLD 5
#define FONT_FACE_SERIF 6
#define FONT_FACE_SERIF_BOLD 7
#define FONT_FACE_CURSIVE 8
#define FONT_FACE_FANTASY 9

#define FONT_FACE_COUNT 10

struct font_desc {
    const char *name;
    int width, height;
    const char *encoding;
};

/* extern int ft_load_type; */

int ctor_font_plotter_freetype( FONT_PLOTTER self );
#endif
