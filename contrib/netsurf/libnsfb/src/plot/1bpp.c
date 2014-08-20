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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "desktop/plotters.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_font.h"

extern struct fb_info_s *fbinfo;


static bool fb_1bpp_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
        LOG(("%s(%d, %d, %d, %d, %d, 0x%lx, %d, %d)\n", __func__, 
             x0,y0,width,height,line_width,c,dotted,dashed));
	return true;
}

static bool fb_1bpp_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed)
{
        LOG(("%s(%d, %d, %d, %d, %d, 0x%lx, %d, %d)\n", __func__, 
             x0,y0,x1,y1,width,c,dotted,dashed));

	return true;
}

static bool fb_1bpp_polygon(const int *p, unsigned int n, colour fill)
{
        LOG(("%s(%p, %d, 0x%lx)\n", __func__, p,n,fill));
	return true;
}


static bool fb_1bpp_fill(int x0, int y0, int x1, int y1, colour c)
{
        int x;
        int y;
        int pent;

        LOG(("%s(%d, %d, %d, %d, 0x%lx)\n", __func__, 
             x0,y0,x1,y1,c));

        if (c != 0)
                pent = 0xff;
        else
                pent = 0;

        fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1);
     
        x = x1 - x0;
        for (y = y0; y < y1; y++) {
                memset(fb_plotters_get_xy_loc(x0, y, fbinfo), pent, x);
        }
	return true;
}

static bool fb_1bpp_clg(colour c)
{
        LOG(("%s(%lx)\n", __func__, c));
        fb_1bpp_fill(fb_plot_ctx.x0, 
                     fb_plot_ctx.y0, 
                     fb_plot_ctx.x1, 
                     fb_plot_ctx.y1, 
                     c);
	return true;
}


static bool fb_1bpp_text(int x, int y, const struct css_style *style,
                  const char *text, size_t length, colour bg, colour c)
{
        const struct fb_font_desc* fb_font = fb_get_font(style);
        u8_t *video_char_start;
        const u8_t *font_data;
        int yloop;
        unsigned char row;
        int chr;

        LOG(("%s(%d, %d, %p, %.*s , %d, 0x%lx, 0x%lx)\n", __func__, 
             x,y,style,length,text,length,bg,c));

        for (chr=0; chr < length; chr++) {
                video_char_start = fb_plotters_get_xy_loc(x + (chr * (fb_font->width)), y, fbinfo);

                /* move our font-data to the correct position */
                font_data = fb_font->data + (text[chr] * fb_font->height);

                for (yloop = 0; yloop < fb_font->height; yloop++) {
                        row = font_data[yloop];
                        *video_char_start = row;
                        video_char_start += fbinfo->line_len;
                }
        }
        return true;


        /* copied from css/css.h - need to open the correct font here
         * font properties *
         css_font_family font_family;
         struct {
         css_font_size_type size;
         union {
         struct css_length length;
         float absolute;
         float percent;
         } value;
         } font_size;
         css_font_style font_style;
         css_font_variant font_variant;
         css_font_weight font_weight;
        */
	return true;
}

static bool fb_1bpp_disc(int x, int y, int radius, colour c, bool filled)
{
        LOG(("%s(%d, %d, %d, 0x%lx, %d)\n", __func__, 
             x, y, radius, c, filled));
	return true;
}

static bool fb_1bpp_arc(int x, int y, int radius, int angle1, int angle2,
	    		colour c)
{
        LOG(("x %d, y %d, radius %d, angle1 %d, angle2 %d, c 0x%lx",
             x, y, radius, angle1, angle2, c));
	return true;
}

static inline colour ablend(colour pixel)
{
        return pixel;
}


static bool fb_1bpp_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg, 
                        struct content *content)
{
        u8_t *video_char_start;
        colour *pixel = (colour *)bitmap->pixdata;
        colour abpixel; /* alphablended pixel */
        int xloop,yloop;

        video_char_start = fb_plotters_get_xy_loc(x, y, fbinfo);

        for (yloop = 0; yloop < height; yloop++) {
                for (xloop = 0; xloop < width; xloop++) {
                        abpixel = pixel[(yloop * bitmap->width) + xloop];
                        if ((abpixel & 0xFF000000) != 0) {
                                if ((abpixel & 0xFF000000) != 0xFF)
                                        abpixel = ablend(abpixel);
                                if (abpixel == 0)
                                        video_char_start[xloop] |= (1 << (xloop % 8));
                                else
                                        video_char_start[xloop] &= ~(1 << (xloop % 8));

                        }
                }
                video_char_start += fbinfo->line_len;
        }

	return true;
}

static bool fb_1bpp_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y, 
                             struct content *content)
{
	unsigned long xf,yf,wf,hf;

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
		return fb_1bpp_bitmap(x,y,width,height,bitmap,bg,content);
	}

	for (xf = 0; xf < width; xf += bitmap->width) {
		for(yf = 0;yf < height; yf += bitmap->height) {
			if(width > xf+bitmap->width)
			{
				wf = width-(xf+bitmap->width);
			}
			else
			{
				wf=bitmap->width;
			}

			if(height > yf+bitmap->height)
			{
				hf = height-(yf+bitmap->height);
			}
			else
			{
				hf=bitmap->height;
			}

                        fb_1bpp_bitmap(x+xf, y+yf, wf, hf, bitmap, bg, content);

		}
	}

	return true;
}

static bool fb_1bpp_flush(void)
{
        LOG(("%s()\n", __func__));
	return true;
}

static bool fb_1bpp_path(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6])
{
        LOG(("%s(%f, %d, 0x%lx, %f, 0x%lx, %f)\n", __func__, 
             *p, n, fill, width, c, *transform));
	return true;
}

const struct plotter_table framebuffer_1bpp_plot = {
	.clg = fb_1bpp_clg,
	.rectangle = fb_1bpp_rectangle,
	.line = fb_1bpp_line,
	.polygon = fb_1bpp_polygon,
	.fill = fb_1bpp_fill,
	.clip = fb_clip,
	.text = fb_1bpp_text,
	.disc = fb_1bpp_disc,
	.arc = fb_1bpp_arc,
	.bitmap = fb_1bpp_bitmap,
	.bitmap_tile = fb_1bpp_bitmap_tile,
	.flush = fb_1bpp_flush, 
	.path = fb_1bpp_path
};

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
