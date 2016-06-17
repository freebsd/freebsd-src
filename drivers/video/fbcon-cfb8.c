/*
 *  linux/drivers/video/cfb8.c -- Low level frame buffer operations for 8 bpp
 *				  packed pixels
 *
 *	Created 5 Apr 1997 by Geert Uytterhoeven
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
#include <video/fbcon-cfb8.h>


    /*
     *  8 bpp packed pixels
     */

static u32 nibbletab_cfb8[] = {
#if defined(__BIG_ENDIAN)
    0x00000000,0x000000ff,0x0000ff00,0x0000ffff,
    0x00ff0000,0x00ff00ff,0x00ffff00,0x00ffffff,
    0xff000000,0xff0000ff,0xff00ff00,0xff00ffff,
    0xffff0000,0xffff00ff,0xffffff00,0xffffffff
#elif defined(__LITTLE_ENDIAN)
    0x00000000,0xff000000,0x00ff0000,0xffff0000,
    0x0000ff00,0xff00ff00,0x00ffff00,0xffffff00,
    0x000000ff,0xff0000ff,0x00ff00ff,0xffff00ff,
    0x0000ffff,0xff00ffff,0x00ffffff,0xffffffff
#else
#error FIXME: No endianness??
#endif
};

void fbcon_cfb8_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual;
    p->next_plane = 0;
}

void fbcon_cfb8_bmove(struct display *p, int sy, int sx, int dy, int dx,
		      int height, int width)
{
    int bytes = p->next_line, linesize = bytes * fontheight(p), rows;
    u8 *src,*dst;

    if (sx == 0 && dx == 0 && width * fontwidth(p) == bytes) {
	fb_memmove(p->screen_base + dy * linesize,
		  p->screen_base + sy * linesize,
		  height * linesize);
	return;
    }
    if (fontwidthlog(p)) {
    	sx <<= fontwidthlog(p); dx <<= fontwidthlog(p); width <<= fontwidthlog(p);
    } else {
    	sx *= fontwidth(p); dx *= fontwidth(p); width *= fontwidth(p);
    }
    if (dy < sy || (dy == sy && dx < sx)) {
	src = p->screen_base + sy * linesize + sx;
	dst = p->screen_base + dy * linesize + dx;
	for (rows = height * fontheight(p) ; rows-- ;) {
	    fb_memmove(dst, src, width);
	    src += bytes;
	    dst += bytes;
	}
    } else {
	src = p->screen_base + (sy+height) * linesize + sx - bytes;
	dst = p->screen_base + (dy+height) * linesize + dx - bytes;
	for (rows = height * fontheight(p) ; rows-- ;) {
	    fb_memmove(dst, src, width);
	    src -= bytes;
	    dst -= bytes;
	}
    }
}

static inline void rectfill(u8 *dest, int width, int height, u8 data,
			    int linesize)
{
    while (height-- > 0) {
	fb_memset(dest, data, width);
	dest += linesize;
    }
}

void fbcon_cfb8_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
    u8 *dest;
    int bytes=p->next_line,lines=height * fontheight(p);
    u8 bgx;

    dest = p->screen_base + sy * fontheight(p) * bytes + sx * fontwidth(p);

    bgx=attr_bgcol_ec(p,conp);

    width *= fontwidth(p);
    if (width == bytes)
	rectfill(dest, lines * width, 1, bgx, bytes);
    else
	rectfill(dest, width, lines, bgx, bytes);
}

void fbcon_cfb8_putc(struct vc_data *conp, struct display *p, int c, int yy,
		     int xx)
{
    u8 *dest,*cdat;
    int bytes=p->next_line,rows;
    u32 eorx,fgx,bgx;

    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p);
    if (fontwidth(p) <= 8)
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
    else
	cdat = p->fontdata + ((c & p->charmask) * fontheight(p) << 1);

    fgx=attr_fgcol(p,c);
    bgx=attr_bgcol(p,c);
    fgx |= (fgx << 8);
    fgx |= (fgx << 16);
    bgx |= (bgx << 8);
    bgx |= (bgx << 16);
    eorx = fgx ^ bgx;

    switch (fontwidth(p)) {
    case 4:
	for (rows = fontheight(p) ; rows-- ; dest += bytes)
	    fb_writel((nibbletab_cfb8[*cdat++ >> 4] & eorx) ^ bgx, dest);
        break;
    case 8:
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
	    fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
	    fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
        }
        break;
    case 12:
    case 16:
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
	    fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
	    fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
	    fb_writel((nibbletab_cfb8[(*cdat >> 4) & 0xf] & eorx) ^ bgx, dest+8);
	    if (fontwidth(p) == 16)
		fb_writel((nibbletab_cfb8[*cdat & 0xf] & eorx) ^ bgx, dest+12);
	    cdat++;
        }
        break;
    }
}

void fbcon_cfb8_putcs(struct vc_data *conp, struct display *p, 
		      const unsigned short *s, int count, int yy, int xx)
{
    u8 *cdat, *dest, *dest0;
    u16 c;
    int rows,bytes=p->next_line;
    u32 eorx, fgx, bgx;

    dest0 = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p);
    c = scr_readw(s);
    fgx = attr_fgcol(p, c);
    bgx = attr_bgcol(p, c);
    fgx |= (fgx << 8);
    fgx |= (fgx << 16);
    bgx |= (bgx << 8);
    bgx |= (bgx << 16);
    eorx = fgx ^ bgx;
    switch (fontwidth(p)) {
    case 4:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + c * fontheight(p);

	    for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes)
		fb_writel((nibbletab_cfb8[*cdat++ >> 4] & eorx) ^ bgx, dest);
	    dest0+=4;
        }
        break;
    case 8:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + c * fontheight(p);

	    for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
		fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
		fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
	    }
	    dest0+=8;
        }
        break;
    case 12:
    case 16:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + (c * fontheight(p) << 1);

	    for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
		fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
		fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
		fb_writel((nibbletab_cfb8[(*cdat >> 4) & 0xf] & eorx) ^ bgx, dest+8);
		if (fontwidth(p) == 16)
		   fb_writel((nibbletab_cfb8[*cdat & 0xf] & eorx) ^ bgx, dest+12);
		cdat++;
	    }
	    dest0+=fontwidth(p);
        }
        break;
    }
}

void fbcon_cfb8_revc(struct display *p, int xx, int yy)
{
    u8 *dest;
    int bytes=p->next_line, rows;

    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p);
    for (rows = fontheight(p) ; rows-- ; dest += bytes) {
    	switch (fontwidth(p)) {
    	case 16: fb_writel(fb_readl(dest+12) ^ 0x0f0f0f0f, dest+12); /* fall thru */
    	case 12: fb_writel(fb_readl(dest+8) ^ 0x0f0f0f0f, dest+8); /* fall thru */
    	case 8: fb_writel(fb_readl(dest+4) ^ 0x0f0f0f0f, dest+4); /* fall thru */
    	case 4: fb_writel(fb_readl(dest) ^ 0x0f0f0f0f, dest); /* fall thru */
    	default: break;
    	}
    }
}

void fbcon_cfb8_clear_margins(struct vc_data *conp, struct display *p,
			      int bottom_only)
{
    int bytes=p->next_line;
    u8 bgx;

    unsigned int right_start = conp->vc_cols*fontwidth(p);
    unsigned int bottom_start = conp->vc_rows*fontheight(p);
    unsigned int right_width, bottom_width;

    bgx=attr_bgcol_ec(p,conp);

    if (!bottom_only && (right_width = p->var.xres-right_start))
	rectfill(p->screen_base+right_start, right_width, p->var.yres_virtual,
		 bgx, bytes);
    if ((bottom_width = p->var.yres-bottom_start))
	rectfill(p->screen_base+(p->var.yoffset+bottom_start)*bytes,
		 right_start, bottom_width, bgx, bytes);
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_cfb8 = {
    setup:		fbcon_cfb8_setup,
    bmove:		fbcon_cfb8_bmove,
    clear:		fbcon_cfb8_clear,
    putc:		fbcon_cfb8_putc,
    putcs:		fbcon_cfb8_putcs,
    revc:		fbcon_cfb8_revc,
    clear_margins:	fbcon_cfb8_clear_margins,
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

EXPORT_SYMBOL(fbcon_cfb8);
EXPORT_SYMBOL(fbcon_cfb8_setup);
EXPORT_SYMBOL(fbcon_cfb8_bmove);
EXPORT_SYMBOL(fbcon_cfb8_clear);
EXPORT_SYMBOL(fbcon_cfb8_putc);
EXPORT_SYMBOL(fbcon_cfb8_putcs);
EXPORT_SYMBOL(fbcon_cfb8_revc);
EXPORT_SYMBOL(fbcon_cfb8_clear_margins);
