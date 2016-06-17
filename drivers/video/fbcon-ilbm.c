/*
 *  linux/drivers/video/ilbm.c -- Low level frame buffer operations for
 *				  interleaved bitplanes à la Amiga
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
#include <video/fbcon-ilbm.h>


    /*
     *  Interleaved bitplanes à la Amiga
     *
     *  This code heavily relies on the fact that
     *
     *      next_line == interleave == next_plane*bits_per_pixel
     *
     *  But maybe it can be merged with the code for normal bitplanes without
     *  much performance loss?
     */

void fbcon_ilbm_setup(struct display *p)
{
    if (p->line_length) {
	p->next_line = p->line_length*p->var.bits_per_pixel;
	p->next_plane = p->line_length;
    } else {
	p->next_line = p->type_aux;
	p->next_plane = p->type_aux/p->var.bits_per_pixel;
    }
}

void fbcon_ilbm_bmove(struct display *p, int sy, int sx, int dy, int dx,
		      int height, int width)
{
    if (sx == 0 && dx == 0 && width == p->next_plane)
	fb_memmove(p->screen_base+dy*fontheight(p)*p->next_line,
		  p->screen_base+sy*fontheight(p)*p->next_line,
		  height*fontheight(p)*p->next_line);
    else {
	u8 *src, *dest;
	u_int i;

	if (dy <= sy) {
	    src = p->screen_base+sy*fontheight(p)*p->next_line+sx;
	    dest = p->screen_base+dy*fontheight(p)*p->next_line+dx;
	    for (i = p->var.bits_per_pixel*height*fontheight(p); i--;) {
		fb_memmove(dest, src, width);
		src += p->next_plane;
		dest += p->next_plane;
	    }
	} else {
	    src = p->screen_base+(sy+height)*fontheight(p)*p->next_line+sx;
	    dest = p->screen_base+(dy+height)*fontheight(p)*p->next_line+dx;
	    for (i = p->var.bits_per_pixel*height*fontheight(p); i--;) {
		src -= p->next_plane;
		dest -= p->next_plane;
		fb_memmove(dest, src, width);
	    }
	}
    }
}

void fbcon_ilbm_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
    u8 *dest;
    u_int i, rows;
    int bg, bg0;

    dest = p->screen_base+sy*fontheight(p)*p->next_line+sx;

    bg0 = attr_bgcol_ec(p,conp);
    for (rows = height*fontheight(p); rows--;) {
	bg = bg0;
	for (i = p->var.bits_per_pixel; i--; dest += p->next_plane) {
	    if (bg & 1)
		fb_memset255(dest, width);
	    else
		fb_memclear(dest, width);
	    bg >>= 1;
	}
    }
}

void fbcon_ilbm_putc(struct vc_data *conp, struct display *p, int c, int yy,
		     int xx)
{
    u8 *dest, *cdat;
    u_int rows, i;
    u8 d;
    int fg0, bg0, fg, bg;

    dest = p->screen_base+yy*fontheight(p)*p->next_line+xx;
    cdat = p->fontdata+(c&p->charmask)*fontheight(p);
    fg0 = attr_fgcol(p,c);
    bg0 = attr_bgcol(p,c);

    for (rows = fontheight(p); rows--;) {
	d = *cdat++;
	fg = fg0;
	bg = bg0;
	for (i = p->var.bits_per_pixel; i--; dest += p->next_plane) {
	    if (bg & 1){
		if (fg & 1)
		    *dest = 0xff;
		else
		    *dest = ~d;
	    }else{
		if (fg & 1)
		    *dest = d;
		else
		    *dest = 0x00;
	    }
	    bg >>= 1;
	    fg >>= 1;
	}
    }
}

    /*
     *  I've split the console character loop in two parts:
     *
     *      - slow version: this blits one character at a time
     *
     *      - fast version: this blits 4 characters at a time at a longword
     *			    aligned address, to reduce the number of expensive
     *			    Chip RAM accesses.
     *
     *  Experiments on my A4000/040 revealed that this makes a console switch
     *  on a 640x400 screen with 256 colors about 3 times faster.
     *
     *  -- Geert
     */

void fbcon_ilbm_putcs(struct vc_data *conp, struct display *p, 
		      const unsigned short *s, int count, int yy, int xx)
{
    u8 *dest0, *dest, *cdat1, *cdat2, *cdat3, *cdat4;
    u_int rows, i;
    u16 c1, c2, c3, c4;
    u32 d;
    int fg0, bg0, fg, bg;

    dest0 = p->screen_base+yy*fontheight(p)*p->next_line+xx;
    c1 = scr_readw(s);
    fg0 = attr_fgcol(p, c1);
    bg0 = attr_bgcol(p, c1);

    while (count--)
	if (xx&3 || count < 3) {	/* Slow version */
	    c1 = scr_readw(s++) & p->charmask;
	    dest = dest0++;
	    xx++;

	    cdat1 = p->fontdata+c1*fontheight(p);
	    for (rows = fontheight(p); rows--;) {
		d = *cdat1++;
		fg = fg0;
		bg = bg0;
		for (i = p->var.bits_per_pixel; i--; dest += p->next_plane) {
		    if (bg & 1){
			if (fg & 1)
			    *dest = 0xff;
			else
			    *dest = ~d;
		    }else{
			if (fg & 1)
			    *dest = d;
			else
			    *dest = 0x00;
		    }
		    bg >>= 1;
		    fg >>= 1;
		}
	    }
	} else {		/* Fast version */
	    c1 = scr_readw(&s[0]) & p->charmask;
	    c2 = scr_readw(&s[1]) & p->charmask;
	    c3 = scr_readw(&s[2]) & p->charmask;
	    c4 = scr_readw(&s[3]) & p->charmask;

	    dest = dest0;
	    cdat1 = p->fontdata+c1*fontheight(p);
	    cdat2 = p->fontdata+c2*fontheight(p);
	    cdat3 = p->fontdata+c3*fontheight(p);
	    cdat4 = p->fontdata+c4*fontheight(p);
	    for (rows = fontheight(p); rows--;) {
#if defined(__BIG_ENDIAN)
		d = *cdat1++<<24 | *cdat2++<<16 | *cdat3++<<8 | *cdat4++;
#elif defined(__LITTLE_ENDIAN)
		d = *cdat1++ | *cdat2++<<8 | *cdat3++<<16 | *cdat4++<<24;
#else
#error FIXME: No endianness??
#endif
		fg = fg0;
		bg = bg0;
		for (i = p->var.bits_per_pixel; i--; dest += p->next_plane) {
		    if (bg & 1){
			if (fg & 1)
			    *(u32 *)dest = 0xffffffff;
			else
			    *(u32 *)dest = ~d;
		    }else{
			if (fg & 1)
			    *(u32 *)dest = d;
			else
			    *(u32 *)dest = 0x00000000;
		    }
		    bg >>= 1;
		    fg >>= 1;
		}
	    }
	    s += 4;
	    dest0 += 4;
	    xx += 4;
	    count -= 3;
	}
}

void fbcon_ilbm_revc(struct display *p, int xx, int yy)
{
    u8 *dest, *dest0;
    u_int rows, i;
    int mask;

    dest0 = p->screen_base+yy*fontheight(p)*p->next_line+xx;
    mask = p->fgcol ^ p->bgcol;

    /*
     *  This should really obey the individual character's
     *  background and foreground colors instead of simply
     *  inverting.
     */

    for (i = p->var.bits_per_pixel; i--; dest0 += p->next_plane) {
	if (mask & 1) {
	    dest = dest0;
	    for (rows = fontheight(p); rows--; dest += p->next_line)
		*dest = ~*dest;
	}
	mask >>= 1;
    }
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_ilbm = {
    setup:		fbcon_ilbm_setup,
    bmove:		fbcon_ilbm_bmove,
    clear:		fbcon_ilbm_clear,
    putc:		fbcon_ilbm_putc,
    putcs:		fbcon_ilbm_putcs,
    revc:		fbcon_ilbm_revc,
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

EXPORT_SYMBOL(fbcon_ilbm);
EXPORT_SYMBOL(fbcon_ilbm_setup);
EXPORT_SYMBOL(fbcon_ilbm_bmove);
EXPORT_SYMBOL(fbcon_ilbm_clear);
EXPORT_SYMBOL(fbcon_ilbm_putc);
EXPORT_SYMBOL(fbcon_ilbm_putcs);
EXPORT_SYMBOL(fbcon_ilbm_revc);
