/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer interface
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/log.h"
#include "desktop/browser.h"
#include "image/bitmap.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/font.h"

/* netsurf framebuffer library handle */
static nsfb_t *nsfb;


static bool
framebuffer_plot_disc(int x, int y, int radius, const plot_style_t *style)
{
    nsfb_bbox_t ellipse;
    ellipse.x0 = x - radius;
    ellipse.y0 = y - radius;
    ellipse.x1 = x + radius;
    ellipse.y1 = y + radius;

    if (style->fill_type != PLOT_OP_TYPE_NONE) {
        nsfb_plot_ellipse_fill(nsfb, &ellipse, style->fill_colour);
    } 

    if (style->stroke_type != PLOT_OP_TYPE_NONE) {
        nsfb_plot_ellipse(nsfb, &ellipse, style->stroke_colour);
    }
    return true;
}

static bool 
framebuffer_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
    return nsfb_plot_arc(nsfb, x, y, radius, angle1, angle2, style->fill_colour);
}

static bool 
framebuffer_plot_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
    return nsfb_plot_polygon(nsfb, p, n, style->fill_colour);
}


#ifdef FB_USE_FREETYPE
static bool 
framebuffer_plot_text(int x, int y, const char *text, size_t length,
		const plot_font_style_t *fstyle)
{
        uint32_t ucs4;
        size_t nxtchr = 0;
        FT_Glyph glyph;
        FT_BitmapGlyph bglyph;
        nsfb_bbox_t loc;

        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(text + nxtchr, length - nxtchr);
                nxtchr = utf8_next(text, length, nxtchr);

                glyph = fb_getglyph(fstyle, ucs4);
                if (glyph == NULL)
                        continue;

                if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
                        bglyph = (FT_BitmapGlyph)glyph;

                        loc.x0 = x + bglyph->left;
                        loc.y0 = y - bglyph->top;
                        loc.x1 = loc.x0 + bglyph->bitmap.width;
                        loc.y1 = loc.y0 + bglyph->bitmap.rows;

                        /* now, draw to our target surface */
                        if (bglyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                            nsfb_plot_glyph1(nsfb, 
                                             &loc, 
                                             bglyph->bitmap.buffer, 
                                             bglyph->bitmap.pitch, 
                                             fstyle->foreground);
                        } else {
                            nsfb_plot_glyph8(nsfb, 
                                             &loc, 
                                             bglyph->bitmap.buffer, 
                                             bglyph->bitmap.pitch, 
                                             fstyle->foreground);
                        }
                }
                x += glyph->advance.x >> 16;

        }
        return true;

}
#else
static bool framebuffer_plot_text(int x, int y, const char *text, size_t length,
		const plot_font_style_t *fstyle)
{
    const struct fb_font_desc* fb_font = fb_get_font(fstyle);
    const uint32_t *chrp;
    char *buffer = NULL;
    int chr;
    int blen;
    nsfb_bbox_t loc;

    utf8_to_font_encoding(fb_font, text, length, &buffer);
    if (buffer == NULL)
        return true;

        /* y is given as the baseline, at 3/4 from top.
         * we need it to the top */
        y -= ((fb_font->height * 3) / 4);

        /* the coord is the bottom-left of the pixels offset by 1 to make
         *   it work since fb coords are the top-left of pixels
         */
        y += 1;

    blen = strlen(buffer);

    for (chr = 0; chr < blen; chr++) {
        loc.x0 = x;
        loc.y0 = y;
        loc.x1 = loc.x0 + fb_font->width;
        loc.y1 = loc.y0 + fb_font->height;

        chrp = fb_font->data + ((unsigned char)buffer[chr] * fb_font->height);
        nsfb_plot_glyph1(nsfb, &loc, (uint8_t *)chrp, 32, fstyle->foreground);

        x += fb_font->width;

    }

    free(buffer);
    return true;
}
#endif


static bool 
framebuffer_plot_bitmap(int x, int y,
                        int width, int height,
                        struct bitmap *bitmap, colour bg,
                        bitmap_flags_t flags)
{
        nsfb_bbox_t loc;
        nsfb_bbox_t clipbox;
        bool repeat_x = (flags & BITMAPF_REPEAT_X);
        bool repeat_y = (flags & BITMAPF_REPEAT_Y);
	int bmwidth;
	int bmheight;
	int bmstride;
	enum nsfb_format_e bmformat;
	unsigned char *bmptr;
	nsfb_t *bm = (nsfb_t *)bitmap;

	/* x and y define coordinate of top left of of the initial explicitly
	 * placed tile. The width and height are the image scaling and the
	 * bounding box defines the extent of the repeat (which may go in all
	 * four directions from the initial tile).
	 */

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just plot it */
                loc.x0 = x;
                loc.y0 = y;
                loc.x1 = loc.x0 + width;
                loc.y1 = loc.y0 + height;

		return nsfb_plot_copy(bm, NULL, nsfb, &loc);		
	}

        nsfb_plot_get_clip(nsfb, &clipbox);
	nsfb_get_geometry(bm, &bmwidth, &bmheight, &bmformat);
	nsfb_get_buffer(bm, &bmptr, &bmstride);

	/* Optimise tiled plots of 1x1 bitmaps by replacing with a flat fill
	 * of the area.  Can only be done when image is fully opaque. */
	if ((bmwidth == 1) && (bmheight == 1)) {
		if ((*(nsfb_colour_t *)bmptr & 0xff000000) != 0) {
			return nsfb_plot_rectangle_fill(nsfb, &clipbox,
					*(nsfb_colour_t *)bmptr);
		}
	}

	/* Optimise tiled plots of bitmaps scaled to 1x1 by replacing with
	 * a flat fill of the area.  Can only be done when image is fully
	 * opaque. */
	if ((width == 1) && (height == 1)) {
		if (bitmap_get_opaque(bm)) {
			/** TODO: Currently using top left pixel. Maybe centre
			 *        pixel or average value would be better. */
			return nsfb_plot_rectangle_fill(nsfb, &clipbox,
					*(nsfb_colour_t *)bmptr);
		}
	}

	/* get left most tile position */
	if (repeat_x)
		for (; x > clipbox.x0; x -= width);

	/* get top most tile position */
	if (repeat_y)
		for (; y > clipbox.y0; y -= height);

	/* set up top left tile location */
        loc.x0 = x;
        loc.y0 = y;
        loc.x1 = loc.x0 + width;
        loc.y1 = loc.y0 + height;

	/* plot tiling across and down to extents */
	nsfb_plot_bitmap_tiles(nsfb, &loc,
			repeat_x ? ((clipbox.x1 - x) + width  - 1) / width  : 1,
			repeat_y ? ((clipbox.y1 - y) + height - 1) / height : 1,
			(nsfb_colour_t *)bmptr, bmwidth, bmheight,
			bmstride * 8 / 32, bmformat == NSFB_FMT_ABGR8888);

	return true;
}

static bool 
framebuffer_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	nsfb_bbox_t rect;
	bool dotted = false; 
	bool dashed = false;

	rect.x0 = x0;
	rect.y0 = y0;
	rect.x1 = x1;
	rect.y1 = y1;

	if (style->fill_type != PLOT_OP_TYPE_NONE) {  
		nsfb_plot_rectangle_fill(nsfb, &rect, style->fill_colour);
	}
    
	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) 
			dotted = true;

		if (style->stroke_type == PLOT_OP_TYPE_DASH) 
			dashed = true;

		nsfb_plot_rectangle(nsfb, &rect, style->stroke_width, style->stroke_colour, dotted, dashed); 
	}

	return true;
}

static bool 
framebuffer_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	nsfb_bbox_t rect;
	nsfb_plot_pen_t pen;

	rect.x0 = x0;
	rect.y0 = y0;
	rect.x1 = x1;
	rect.y1 = y1;
    
	if (style->stroke_type != PLOT_OP_TYPE_NONE) {

		if (style->stroke_type == PLOT_OP_TYPE_DOT) {
			pen.stroke_type = NFSB_PLOT_OPTYPE_PATTERN; 
			pen.stroke_pattern = 0xAAAAAAAA;
		} else if (style->stroke_type == PLOT_OP_TYPE_DASH) {
			pen.stroke_type = NFSB_PLOT_OPTYPE_PATTERN; 
			pen.stroke_pattern = 0xF0F0F0F0;
		} else {
			pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID; 
		}

		pen.stroke_colour = style->stroke_colour;
		pen.stroke_width = style->stroke_width;
		nsfb_plot_line(nsfb, &rect, &pen); 
	}

	return true;
}


static bool 
framebuffer_plot_path(const float *p, 
                      unsigned int n, 
                      colour fill, 
                      float width,
                      colour c, 
                      const float transform[6])
{
	LOG(("path unimplemented"));
	return true;
}

static bool 
framebuffer_plot_clip(const struct rect *clip)
{
	nsfb_bbox_t nsfb_clip;
	nsfb_clip.x0 = clip->x0;
	nsfb_clip.y0 = clip->y0;
	nsfb_clip.x1 = clip->x1;
	nsfb_clip.y1 = clip->y1;

	return nsfb_plot_set_clip(nsfb, &nsfb_clip);
}

const struct plotter_table fb_plotters = {
	.clip = framebuffer_plot_clip,
	.arc = framebuffer_plot_arc,
	.disc = framebuffer_plot_disc,
	.line = framebuffer_plot_line,
	.rectangle = framebuffer_plot_rectangle,
	.polygon = framebuffer_plot_polygon,
	.path = framebuffer_plot_path,
	.bitmap = framebuffer_plot_bitmap,
	.text = framebuffer_plot_text,
        .option_knockout = true,
};



nsfb_t *
framebuffer_initialise(const char *fename, int width, int height, int bpp)
{
    enum nsfb_type_e fbtype;
    enum nsfb_format_e fbfmt;

    /* bpp is a proxy for the framebuffer format */
    switch (bpp) {
    case 32:
	    fbfmt = NSFB_FMT_XRGB8888;
	    break;

    case 24:
	    fbfmt = NSFB_FMT_RGB888;
	    break;

    case 16:
	    fbfmt = NSFB_FMT_RGB565;
	    break;

    case 8:
	    fbfmt = NSFB_FMT_I8;
	    break;

    case 4:
	    fbfmt = NSFB_FMT_I4;
	    break;

    case 1:
	    fbfmt = NSFB_FMT_I1;
	    break;

    default:
        LOG(("Bad bits per pixel (%d)\n", bpp));
        return NULL;	    
    }

    fbtype = nsfb_type_from_name(fename);
    if (fbtype == NSFB_SURFACE_NONE) {
        LOG(("The %s surface is not available from libnsfb\n", fename));
        return NULL;
    }

    nsfb = nsfb_new(fbtype);
    if (nsfb == NULL) {
        LOG(("Unable to create %s fb surface\n", fename));
        return NULL;
    }
    
    if (nsfb_set_geometry(nsfb, width, height, fbfmt) == -1) {
        LOG(("Unable to set surface geometry\n"));
        nsfb_free(nsfb);
        return NULL;
    }

    nsfb_cursor_init(nsfb);
    
    if (nsfb_init(nsfb) == -1) {
        LOG(("Unable to initialise nsfb surface\n"));
        nsfb_free(nsfb);
        return NULL;
    }

    return nsfb;

}

void
framebuffer_finalise(void)
{
    nsfb_free(nsfb);    
}

bool
framebuffer_set_cursor(struct fbtk_bitmap *bm)
{
    return nsfb_cursor_set(nsfb, (nsfb_colour_t *)bm->pixdata, bm->width, bm->height, bm->width, bm->hot_x, bm->hot_y);
} 

nsfb_t *framebuffer_set_surface(nsfb_t *new_nsfb)
{
	nsfb_t *old_nsfb;
	old_nsfb = nsfb;
	nsfb = new_nsfb;
	return old_nsfb;
}
