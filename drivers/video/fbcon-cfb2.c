/*
 *  linux/drivers/video/cfb2.c -- Low level frame buffer operations for 2 bpp
 *				  packed pixels
 *
 *	Created 26 Dec 1997 by Michael Schmitz
 *	Based on cfb4.c
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
#include <video/fbcon-cfb2.h>


    /*
     *  2 bpp packed pixels
     */

    /*
     *  IFF the font is even pixel aligned (that is to say each
     *  character start is a byte start in the pixel pairs). That
     *  avoids us having to mask bytes and means we won't be here
     *  all week. On a MacII that matters _lots_
     */

static u_char nibbletab_cfb2[]={
#if defined(__BIG_ENDIAN)
	0x00,0x03,0x0c,0x0f,
	0x30,0x33,0x3c,0x3f,
	0xc0,0xc3,0xcc,0xcf,
	0xf0,0xf3,0xfc,0xff
#elif defined(__LITTLE_ENDIAN)
	0x00,0xc0,0x30,0xf0,
	0x0c,0xcc,0x3c,0xfc,
	0x03,0xc3,0x33,0xf3,
	0x0f,0xcf,0x3f,0xff
#else
#error FIXME: No endianness??
#endif
};
 

void fbcon_cfb2_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual>>2;
    p->next_plane = 0;
}

void fbcon_cfb2_bmove(struct display *p, int sy, int sx, int dy, int dx,
		      int height, int width)
{
	int bytes = p->next_line, linesize = bytes * fontheight(p), rows;
	u8 *src,*dst;

	if (sx == 0 && dx == 0 && width * 2 == bytes) {
		fb_memmove(p->screen_base + dy * linesize,
			  p->screen_base + sy * linesize,
			  height * linesize);
	}
	else {
		if (dy < sy || (dy == sy && dx < sx)) {
			src = p->screen_base + sy * linesize + sx * 2;
			dst = p->screen_base + dy * linesize + dx * 2;
			for (rows = height * fontheight(p) ; rows-- ;) {
				fb_memmove(dst, src, width * 2);
				src += bytes;
				dst += bytes;
			}
		}
		else {
			src = p->screen_base + (sy+height) * linesize + sx * 2
				- bytes;
			dst = p->screen_base + (dy+height) * linesize + dx * 2
				- bytes;
			for (rows = height * fontheight(p) ; rows-- ;) {
				fb_memmove(dst, src, width * 2);
				src -= bytes;
				dst -= bytes;
			}
		}
	}
}

void fbcon_cfb2_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
	u8 *dest0,*dest;
	int bytes=p->next_line,lines=height * fontheight(p), rows, i;
	u32 bgx;

	dest = p->screen_base + sy * fontheight(p) * bytes + sx * 2;

	bgx=attr_bgcol_ec(p,conp);
	bgx |= (bgx << 2);	/* expand the colour to 16 bits */
	bgx |= (bgx << 4);
	bgx |= (bgx << 8);

	if (sx == 0 && width * 2 == bytes) {
		for (i = 0 ; i < lines * width ; i++) {
			fb_writew (bgx, dest);
			dest+=2;
		}
	} else {
		dest0=dest;
		for (rows = lines; rows-- ; dest0 += bytes) {
			dest=dest0;
			for (i = 0 ; i < width ; i++) {
				/* memset ?? */
				fb_writew (bgx, dest);
				dest+=2;
			}
		}
	}
}

void fbcon_cfb2_putc(struct vc_data *conp, struct display *p, int c, int yy,
		     int xx)
{
	u8 *dest,*cdat;
	int bytes=p->next_line,rows;
	u32 eorx,fgx,bgx;

	dest = p->screen_base + yy * fontheight(p) * bytes + xx * 2;
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);

	fgx=3;/*attr_fgcol(p,c);*/
	bgx=attr_bgcol(p,c);
	fgx |= (fgx << 2);	/* expand color to 8 bits */
	fgx |= (fgx << 4);
	bgx |= (bgx << 2);
	bgx |= (bgx << 4);
	eorx = fgx ^ bgx;

	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
		fb_writeb((nibbletab_cfb2[*cdat >> 4] & eorx) ^ bgx, dest+0);
		fb_writeb((nibbletab_cfb2[*cdat++ & 0xf] & eorx) ^ bgx, dest+1);
	}
}

void fbcon_cfb2_putcs(struct vc_data *conp, struct display *p, const unsigned short *s,
		      int count, int yy, int xx)
{
	u8 *cdat, *dest, *dest0;
	u16 c;
	int rows,bytes=p->next_line;
	u32 eorx, fgx, bgx;

	dest0 = p->screen_base + yy * fontheight(p) * bytes + xx * 2;
	c = scr_readw(s);
	fgx = 3/*attr_fgcol(p, c)*/;
	bgx = attr_bgcol(p, c);
	fgx |= (fgx << 2);
	fgx |= (fgx << 4);
	bgx |= (bgx << 2);
	bgx |= (bgx << 4);
	eorx = fgx ^ bgx;
	while (count--) {
		c = scr_readw(s++) & p->charmask;
		cdat = p->fontdata + c * fontheight(p);

		for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
			fb_writeb((nibbletab_cfb2[*cdat >> 4] & eorx) ^ bgx, dest+0);
			fb_writeb((nibbletab_cfb2[*cdat++ & 0xf] & eorx) ^ bgx, dest+1);
		}
		dest0+=2;
	}
}

void fbcon_cfb2_revc(struct display *p, int xx, int yy)
{
	u8 *dest;
	int bytes=p->next_line, rows;

	dest = p->screen_base + yy * fontheight(p) * bytes + xx * 2;
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
		fb_writew(fb_readw(dest) ^ 0xffff, dest);
	}
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_cfb2 = {
    setup:		fbcon_cfb2_setup,
    bmove:		fbcon_cfb2_bmove,
    clear:		fbcon_cfb2_clear,
    putc:		fbcon_cfb2_putc,
    putcs:		fbcon_cfb2_putcs,
    revc:		fbcon_cfb2_revc,
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

EXPORT_SYMBOL(fbcon_cfb2);
EXPORT_SYMBOL(fbcon_cfb2_setup);
EXPORT_SYMBOL(fbcon_cfb2_bmove);
EXPORT_SYMBOL(fbcon_cfb2_clear);
EXPORT_SYMBOL(fbcon_cfb2_putc);
EXPORT_SYMBOL(fbcon_cfb2_putcs);
EXPORT_SYMBOL(fbcon_cfb2_revc);
