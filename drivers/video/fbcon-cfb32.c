/*
 *  linux/drivers/video/cfb32.c -- Low level frame buffer operations for 32 bpp
 *				   truecolor packed pixels
 *
 *	Created 28 Dec 1997 by Geert Uytterhoeven
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
#include <video/fbcon-cfb32.h>


    /*
     *  32 bpp packed pixels
     */

void fbcon_cfb32_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual<<2;
    p->next_plane = 0;
}

void fbcon_cfb32_bmove(struct display *p, int sy, int sx, int dy, int dx,
		       int height, int width)
{
    int bytes = p->next_line, linesize = bytes * fontheight(p), rows;
    u8 *src, *dst;

    if (sx == 0 && dx == 0 && width * fontwidth(p) * 4 == bytes) {
	fb_memmove(p->screen_base + dy * linesize,
		  p->screen_base + sy * linesize,
		  height * linesize);
	return;
    }
    if (fontwidthlog(p)) {
	sx <<= fontwidthlog(p)+2;
	dx <<= fontwidthlog(p)+2;
	width <<= fontwidthlog(p)+2;
    } else {
	sx *= fontwidth(p)*4;
	dx *= fontwidth(p)*4;
	width *= fontwidth(p)*4;
    }
    if (dy < sy || (dy == sy && dx < sx)) {
	src = p->screen_base + sy * linesize + sx;
	dst = p->screen_base + dy * linesize + dx;
	for (rows = height * fontheight(p); rows--;) {
	    fb_memmove(dst, src, width);
	    src += bytes;
	    dst += bytes;
	}
    } else {
	src = p->screen_base + (sy+height) * linesize + sx - bytes;
	dst = p->screen_base + (dy+height) * linesize + dx - bytes;
	for (rows = height * fontheight(p); rows--;) {
	    fb_memmove(dst, src, width);
	    src -= bytes;
	    dst -= bytes;
	}
    }
}

static inline void rectfill(u8 *dest, int width, int height, u32 data,
			    int linesize)
{
    int i;

    while (height-- > 0) {
	u32 *p = (u32 *)dest;
	for (i = 0; i < width/4; i++) {
	    fb_writel(data, p++);
	    fb_writel(data, p++);
	    fb_writel(data, p++);
	    fb_writel(data, p++);
	}
	if (width & 2) {
	    fb_writel(data, p++);
	    fb_writel(data, p++);
	}
	if (width & 1)
	    fb_writel(data, p++);
	dest += linesize;
    }
}

void fbcon_cfb32_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		       int height, int width)
{
    u8 *dest;
    int bytes = p->next_line, lines = height * fontheight(p);
    u32 bgx;

    dest = p->screen_base + sy * fontheight(p) * bytes + sx * fontwidth(p) * 4;

    bgx = ((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];

    width *= fontwidth(p)/4;
    if (width * 16 == bytes)
	rectfill(dest, lines * width * 4, 1, bgx, bytes);
    else
	rectfill(dest, width * 4, lines, bgx, bytes);
}

void fbcon_cfb32_putc(struct vc_data *conp, struct display *p, int c, int yy,
		      int xx)
{
    u8 *dest, *cdat, bits;
    int bytes = p->next_line, rows;
    u32 eorx, fgx, bgx, *pt;

    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * 4;
    if (fontwidth(p) <= 8)
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
    else
	cdat = p->fontdata + ((c & p->charmask) * fontheight(p) << 1);
    fgx = ((u32 *)p->dispsw_data)[attr_fgcol(p, c)];
    bgx = ((u32 *)p->dispsw_data)[attr_bgcol(p, c)];
    eorx = fgx ^ bgx;

    for (rows = fontheight(p); rows--; dest += bytes) {
	bits = *cdat++;
	pt = (u32 *) dest;
	fb_writel((-(bits >> 7) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 6 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 5 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 4 & 1) & eorx) ^ bgx, pt++);
	if (fontwidth(p) < 8)
	    continue;
	fb_writel((-(bits >> 3 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 2 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 1 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits & 1) & eorx) ^ bgx, pt++);
	if (fontwidth(p) < 12)
	    continue;
	bits = *cdat++;
	fb_writel((-(bits >> 7) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 6 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 5 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 4 & 1) & eorx) ^ bgx, pt++);
	if (fontwidth(p) < 16)
	    continue;
	fb_writel((-(bits >> 3 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 2 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits >> 1 & 1) & eorx) ^ bgx, pt++);
	fb_writel((-(bits & 1) & eorx) ^ bgx, pt++);
    }
}

void fbcon_cfb32_putcs(struct vc_data *conp, struct display *p,
		       const unsigned short *s, int count, int yy, int xx)
{
    u8 *cdat, *dest, *dest0, bits;
    u16 c;
    int rows, bytes = p->next_line;
    u32 eorx, fgx, bgx, *pt;

    dest0 = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * 4;
    c = scr_readw(s);
    fgx = ((u32 *)p->dispsw_data)[attr_fgcol(p, c)];
    bgx = ((u32 *)p->dispsw_data)[attr_bgcol(p, c)];
    eorx = fgx ^ bgx;
    while (count--) {
	c = scr_readw(s++) & p->charmask;
	if (fontwidth(p) <= 8)
	    cdat = p->fontdata + c * fontheight(p);
	else
	    cdat = p->fontdata + (c * fontheight(p) << 1);
	for (rows = fontheight(p), dest = dest0; rows--; dest += bytes) {
	    bits = *cdat++;
	    pt = (u32 *) dest;
	    fb_writel((-(bits >> 7) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 6 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 5 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 4 & 1) & eorx) ^ bgx, pt++);
	    if (fontwidth(p) < 8)
		continue;
	    fb_writel((-(bits >> 3 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 2 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 1 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits & 1) & eorx) ^ bgx, pt++);
	    if (fontwidth(p) < 12)
		continue;
	    bits = *cdat++;
	    fb_writel((-(bits >> 7) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 6 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 5 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 4 & 1) & eorx) ^ bgx, pt++);
	    if (fontwidth(p) < 16)
		continue;
	    fb_writel((-(bits >> 3 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 2 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits >> 1 & 1) & eorx) ^ bgx, pt++);
	    fb_writel((-(bits & 1) & eorx) ^ bgx, pt++);
	}
	dest0 += fontwidth(p)*4;
    }
}

void fbcon_cfb32_revc(struct display *p, int xx, int yy)
{
    u8 *dest;
    int bytes = p->next_line, rows;

    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * 4;
    for (rows = fontheight(p); rows--; dest += bytes) {
	switch (fontwidth(p)) {
	case 16:
	    fb_writel(fb_readl(dest+(4*12)) ^ 0xffffffff, dest+(4*12));
	    fb_writel(fb_readl(dest+(4*13)) ^ 0xffffffff, dest+(4*13));
	    fb_writel(fb_readl(dest+(4*14)) ^ 0xffffffff, dest+(4*14));
	    fb_writel(fb_readl(dest+(4*15)) ^ 0xffffffff, dest+(4*15));
	    /* FALL THROUGH */
	case 12:
	    fb_writel(fb_readl(dest+(4*8)) ^ 0xffffffff, dest+(4*8));
	    fb_writel(fb_readl(dest+(4*9)) ^ 0xffffffff, dest+(4*9));
	    fb_writel(fb_readl(dest+(4*10)) ^ 0xffffffff, dest+(4*10));
	    fb_writel(fb_readl(dest+(4*11)) ^ 0xffffffff, dest+(4*11));
	    /* FALL THROUGH */
	case 8:
	    fb_writel(fb_readl(dest+(4*4)) ^ 0xffffffff, dest+(4*4));
	    fb_writel(fb_readl(dest+(4*5)) ^ 0xffffffff, dest+(4*5));
	    fb_writel(fb_readl(dest+(4*6)) ^ 0xffffffff, dest+(4*6));
	    fb_writel(fb_readl(dest+(4*7)) ^ 0xffffffff, dest+(4*7));
	    /* FALL THROUGH */
	case 4:
	    fb_writel(fb_readl(dest+(4*0)) ^ 0xffffffff, dest+(4*0));
	    fb_writel(fb_readl(dest+(4*1)) ^ 0xffffffff, dest+(4*1));
	    fb_writel(fb_readl(dest+(4*2)) ^ 0xffffffff, dest+(4*2));
	    fb_writel(fb_readl(dest+(4*3)) ^ 0xffffffff, dest+(4*3));
	    /* FALL THROUGH */
	}
    }
}

void fbcon_cfb32_clear_margins(struct vc_data *conp, struct display *p,
			       int bottom_only)
{
    int bytes = p->next_line;
    u32 bgx;

    unsigned int right_start = conp->vc_cols*fontwidth(p);
    unsigned int bottom_start = conp->vc_rows*fontheight(p);
    unsigned int right_width, bottom_width;

    bgx = ((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];

    if (!bottom_only && (right_width = p->var.xres-right_start))
	rectfill(p->screen_base+right_start*4, right_width,
		 p->var.yres_virtual, bgx, bytes);
    if ((bottom_width = p->var.yres-bottom_start))
	rectfill(p->screen_base+(p->var.yoffset+bottom_start)*bytes,
		 right_start, bottom_width, bgx, bytes);
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_cfb32 = {
    setup:		fbcon_cfb32_setup,
    bmove:		fbcon_cfb32_bmove,
    clear:		fbcon_cfb32_clear,
    putc:		fbcon_cfb32_putc,
    putcs:		fbcon_cfb32_putcs,
    revc:		fbcon_cfb32_revc,
    clear_margins:	fbcon_cfb32_clear_margins,
    fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
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

EXPORT_SYMBOL(fbcon_cfb32);
EXPORT_SYMBOL(fbcon_cfb32_setup);
EXPORT_SYMBOL(fbcon_cfb32_bmove);
EXPORT_SYMBOL(fbcon_cfb32_clear);
EXPORT_SYMBOL(fbcon_cfb32_putc);
EXPORT_SYMBOL(fbcon_cfb32_putcs);
EXPORT_SYMBOL(fbcon_cfb32_revc);
EXPORT_SYMBOL(fbcon_cfb32_clear_margins);
