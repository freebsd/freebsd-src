/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef NETSURF_FB_FONT_INTERNAL_H
#define NETSURF_FB_FONT_INTERNAL_H

struct fb_font_desc {
    const char *name;
    int width, height;
    const char *encoding;
    const uint32_t *data;
};

extern const struct fb_font_desc font_regular;
extern const struct fb_font_desc font_italic;
extern const struct fb_font_desc font_bold;
extern const struct fb_font_desc font_italic_bold;

extern const struct fb_font_desc* fb_get_font(const plot_font_style_t *fstyle);

extern nserror utf8_to_font_encoding(const struct fb_font_desc* font,
				       const char *string,
				       size_t len,
				       char **result);

#endif /* NETSURF_FB_FONT_INTERNAL_H */

