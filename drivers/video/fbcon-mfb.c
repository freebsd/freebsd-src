/*
 *  linux/drivers/video/mfb.c -- Low level frame buffer operations for
 *				 monochrome
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
#include <video/fbcon-mfb.h>


    /*
     *  Monochrome
     */

void fbcon_mfb_setup(struct display *p)
{
    if (p->line_length)
	p->next_line = p->line_length;
    else
	p->next_line = p->var.xres_virtual>>3;
    p->next_plane = 0;
}

void fbcon_mfb_bmove(struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
    u8 *src, *dest;
    u_int rows;

    if (sx == 0 && dx == 0 && width == p->next_line) {
	src = p->screen_base+sy*fontheight(p)*width;
	dest = p->screen_base+dy*fontheight(p)*width;
	fb_memmove(dest, src, height*fontheight(p)*width);
    } else if (dy <= sy) {
	src = p->screen_base+sy*fontheight(p)*p->next_line+sx;
	dest = p->screen_base+dy*fontheight(p)*p->next_line+dx;
	for (rows = height*fontheight(p); rows--;) {
	    fb_memmove(dest, src, width);
	    src += p->next_line;
	    dest += p->next_line;
	}
    } else {
	src = p->screen_base+((sy+height)*fontheight(p)-1)*p->next_line+sx;
	dest = p->screen_base+((dy+height)*fontheight(p)-1)*p->next_line+dx;
	for (rows = height*fontheight(p); rows--;) {
	    fb_memmove(dest, src, width);
	    src -= p->next_line;
	    dest -= p->next_line;
	}
    }
}

void fbcon_mfb_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
    u8 *dest;
    u_int rows;
    int inverse = conp ? attr_reverse(p,conp->vc_video_erase_char) : 0;

    dest = p->screen_base+sy*fontheight(p)*p->next_line+sx;

    if (sx == 0 && width == p->next_line) {
	if (inverse)
	    fb_memset255(dest, height*fontheight(p)*width);
	else
	    fb_memclear(dest, height*fontheight(p)*width);
    } else
	for (rows = height*fontheight(p); rows--; dest += p->next_line)
	    if (inverse)
		fb_memset255(dest, width);
	    else
		fb_memclear_small(dest, width);
}

void fbcon_mfb_putc(struct vc_data *conp, struct display *p, int c, int yy,
		    int xx)
{
    u8 *dest, *cdat;
    u_int rows, bold, revs, underl;
    u8 d;

    dest = p->screen_base+yy*fontheight(p)*p->next_line+xx;
    cdat = p->fontdata+(c&p->charmask)*fontheight(p);
    bold = attr_bold(p,c);
    revs = attr_reverse(p,c);
    underl = attr_underline(p,c);

    for (rows = fontheight(p); rows--; dest += p->next_line) {
	d = *cdat++;
	if (underl && !rows)
	    d = 0xff;
	else if (bold)
	    d |= d>>1;
	if (revs)
	    d = ~d;
    	fb_writeb (d, dest);
    }
}

void fbcon_mfb_putcs(struct vc_data *conp, struct display *p, 
		     const unsigned short *s, int count, int yy, int xx)
{
    u8 *dest, *dest0, *cdat;
    u_int rows, bold, revs, underl;
    u8 d;
    u16 c;

    dest0 = p->screen_base+yy*fontheight(p)*p->next_line+xx;
    c = scr_readw(s);
    bold = attr_bold(p, c);
    revs = attr_reverse(p, c);
    underl = attr_underline(p, c);

    while (count--) {
	c = scr_readw(s++) & p->charmask;
	dest = dest0++;
	cdat = p->fontdata+c*fontheight(p);
	for (rows = fontheight(p); rows--; dest += p->next_line) {
	    d = *cdat++;
	    if (underl && !rows)
		d = 0xff;
	    else if (bold)
		d |= d>>1;
	    if (revs)
		d = ~d;
    	    fb_writeb (d, dest);
	}
    }
}

void fbcon_mfb_revc(struct display *p, int xx, int yy)
{
    u8 *dest, d;
    u_int rows;

    dest = p->screen_base+yy*fontheight(p)*p->next_line+xx;
    for (rows = fontheight(p); rows--; dest += p->next_line) {
    	d = fb_readb(dest);
	fb_writeb (~d, dest);
    }
}

void fbcon_mfb_clear_margins(struct vc_data *conp, struct display *p,
			     int bottom_only)
{
    u8 *dest;
    int height, bottom;
    int inverse = conp ? attr_reverse(p,conp->vc_video_erase_char) : 0;

    /* XXX Need to handle right margin? */

    height = p->var.yres - conp->vc_rows * fontheight(p);
    if (!height)
	return;
    bottom = conp->vc_rows + p->yscroll;
    if (bottom >= p->vrows)
	bottom -= p->vrows;
    dest = p->screen_base + bottom * fontheight(p) * p->next_line;
    if (inverse)
	fb_memset255(dest, height * p->next_line);
    else
	fb_memclear(dest, height * p->next_line);
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_mfb = {
    setup:		fbcon_mfb_setup,
    bmove:		fbcon_mfb_bmove,
    clear:		fbcon_mfb_clear,
    putc:		fbcon_mfb_putc,
    putcs:		fbcon_mfb_putcs,
    revc:		fbcon_mfb_revc,
    clear_margins:	fbcon_mfb_clear_margins,
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

EXPORT_SYMBOL(fbcon_mfb);
EXPORT_SYMBOL(fbcon_mfb_setup);
EXPORT_SYMBOL(fbcon_mfb_bmove);
EXPORT_SYMBOL(fbcon_mfb_clear);
EXPORT_SYMBOL(fbcon_mfb_putc);
EXPORT_SYMBOL(fbcon_mfb_putcs);
EXPORT_SYMBOL(fbcon_mfb_revc);
EXPORT_SYMBOL(fbcon_mfb_clear_margins);
