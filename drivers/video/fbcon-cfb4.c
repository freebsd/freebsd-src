/*
 *  linux/drivers/video/cfb4.c -- Low level frame buffer operations for 4 bpp
 *				  packed pixels
 *
 *	Created 26 Dec 1997 by Michael Schmitz
 *	Based on the old macfb.c 4bpp code by Alan Cox
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb4.h>


    /*
     *  4 bpp packed pixels
     */

    /*
     *  IFF the font is even pixel aligned (that is to say each
     *  character start is a byte start in the pixel pairs). That
     *  avoids us having to mask bytes and means we won't be here
     *  all week. On a MacII that matters _lots_
     */

static u16 nibbletab_cfb4[] = {
#if defined(__BIG_ENDIAN)
    0x0000,0x000f,0x00f0,0x00ff,
    0x0f00,0x0f0f,0x0ff0,0x0fff,
    0xf000,0xf00f,0xf0f0,0xf0ff,
    0xff00,0xff0f,0xfff0,0xffff
#elif defined(__LITTLE_ENDIAN)
    0x0000,0xf000,0x0f00,0xff00,
    0x00f0,0xf0f0,0x0ff0,0xfff0,
    0x000f,0xf00f,0x0f0f,0xff0f,
    0x00ff,0xf0ff,0x0fff,0xffff
#else
#error FIXME: No endianness??
#endif

};

void fbcon_cfb4_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual>>1;
    p->next_plane = 0;
}

void fbcon_cfb4_bmove(struct display *p, int sy, int sx, int dy, int dx,
		      int height, int width)
{
	int bytes = p->next_line, linesize = bytes * fontheight(p), rows;
	u8 *src,*dst;

	if (sx == 0 && dx == 0 && width * 4 == bytes) {
		fb_memmove(p->screen_base + dy * linesize,
			  p->screen_base + sy * linesize,
			  height * linesize);
	}
	else {
		if (dy < sy || (dy == sy && dx < sx)) {
			src = p->screen_base + sy * linesize + sx * 4;
			dst = p->screen_base + dy * linesize + dx * 4;
			for (rows = height * fontheight(p) ; rows-- ;) {
				fb_memmove(dst, src, width * 4);
				src += bytes;
				dst += bytes;
			}
		}
		else {
			src = p->screen_base + (sy+height) * linesize + sx * 4
				- bytes;
			dst = p->screen_base + (dy+height) * linesize + dx * 4
				- bytes;
			for (rows = height * fontheight(p) ; rows-- ;) {
				fb_memmove(dst, src, width * 4);
				src -= bytes;
				dst -= bytes;
			}
		}
	}
}

void fbcon_cfb4_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
	u8 *dest0,*dest;
	int bytes=p->next_line,lines=height * fontheight(p), rows, i;
	u32 bgx;

/*	if(p->screen_base!=0xFDD00020)
		mac_boom(1);*/
	dest = p->screen_base + sy * fontheight(p) * bytes + sx * 4;

	bgx=attr_bgcol_ec(p,conp);
	bgx |= (bgx << 4);	/* expand the colour to 32bits */
	bgx |= (bgx << 8);
	bgx |= (bgx << 16);

	if (sx == 0 && width * 4 == bytes) {
		for (i = 0 ; i < lines * width ; i++) {
			fb_writel (bgx, dest);
			dest+=4;
		}
	} else {
		dest0=dest;
		for (rows = lines; rows-- ; dest0 += bytes) {
			dest=dest0;
			for (i = 0 ; i < width ; i++) {
				/* memset ?? */
				fb_writel (bgx, dest);
				dest+=4;
			}
		}
	}
}

void fbcon_cfb4_putc(struct vc_data *conp, struct display *p, int c, int yy,
		     int xx)
{
	u8 *dest,*cdat;
	int bytes=p->next_line,rows;
	u32 eorx,fgx,bgx;

	dest = p->screen_base + yy * fontheight(p) * bytes + xx * 4;
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);

	fgx=attr_fgcol(p,c);
	bgx=attr_bgcol(p,c);
	fgx |= (fgx << 4);
	fgx |= (fgx << 8);
	bgx |= (bgx << 4);
	bgx |= (bgx << 8);
	eorx = fgx ^ bgx;

	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
		fb_writew((nibbletab_cfb4[*cdat >> 4] & eorx) ^ bgx, dest+0);
		fb_writew((nibbletab_cfb4[*cdat++ & 0xf] & eorx) ^ bgx, dest+2);
	}
}

void fbcon_cfb4_putcs(struct vc_data *conp, struct display *p, 
		      const unsigned short *s, int count, int yy, int xx)
{
	u8 *cdat, *dest, *dest0;
	u16 c;
	int rows,bytes=p->next_line;
	u32 eorx, fgx, bgx;

	dest0 = p->screen_base + yy * fontheight(p) * bytes + xx * 4;
	c = scr_readw(s);
	fgx = attr_fgcol(p, c);
	bgx = attr_bgcol(p, c);
	fgx |= (fgx << 4);
	fgx |= (fgx << 8);
	fgx |= (fgx << 16);
	bgx |= (bgx << 4);
	bgx |= (bgx << 8);
	bgx |= (bgx << 16);
	eorx = fgx ^ bgx;
	while (count--) {
		c = scr_readw(s++) & p->charmask;
		cdat = p->fontdata + c * fontheight(p);

		for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
			fb_writew((nibbletab_cfb4[*cdat >> 4] & eorx) ^ bgx, dest+0);
			fb_writew((nibbletab_cfb4[*cdat++ & 0xf] & eorx) ^ bgx, dest+2);
		}
		dest0+=4;
	}
}

void fbcon_cfb4_revc(struct display *p, int xx, int yy)
{
	u8 *dest;
	int bytes=p->next_line, rows;

	dest = p->screen_base + yy * fontheight(p) * bytes + xx * 4;
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
		fb_writel(fb_readl(dest+0) ^ 0xffffffff, dest+0);
	}
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_cfb4 = {
    setup:		fbcon_cfb4_setup,
    bmove:		fbcon_cfb4_bmove,
    clear:		fbcon_cfb4_clear,
    putc:		fbcon_cfb4_putc,
    putcs:		fbcon_cfb4_putcs,
    revc:		fbcon_cfb4_revc,
    fontwidthmask:	FONTWIDTH(8)
};


#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{}
#endif /* MODULE */


    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(fbcon_cfb4);
EXPORT_SYMBOL(fbcon_cfb4_setup);
EXPORT_SYMBOL(fbcon_cfb4_bmove);
EXPORT_SYMBOL(fbcon_cfb4_clear);
EXPORT_SYMBOL(fbcon_cfb4_putc);
EXPORT_SYMBOL(fbcon_cfb4_putcs);
EXPORT_SYMBOL(fbcon_cfb4_revc);
