/*
 * linux/drivers/video/fbcon-sti.c -- Low level frame buffer
 *  	operations for generic HP video boards using STI (standard
 *  	text interface) firmware
 *
 *  Based on linux/drivers/video/fbcon-artist.c
 *	Created 5 Apr 1997 by Geert Uytterhoeven
 *	Copyright (C) 2000 Philipp Rumpf <prumpf@tux.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.  */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <asm/gsc.h>		/* for gsc_read/write */
#include <asm/types.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>

#include "sti/sticore.h"

/* Translate an address as it would be found in a 2048x2048x1 bit frame
 * buffer into a logical address Artist actually expects.  Addresses fed
 * into Artist look like this:
 *  fixed          Y               X
 * FFFF FFFF LLLL LLLL LLLC CCCC CCCC CC00
 *
 * our "RAM" addresses look like this:
 * 
 * FFFF FFFF 0000 0LLL LLLL LLLL CCCC CCCC [CCC]
 *
 * */

static inline u32
ram2log(void * addr)
{
#if 1
	return (unsigned long) addr;
#else	
	u32 a = (unsigned long) addr;
	u32 r;

#if 1 
	r  =   a & 0xff000000;		/* fixed part */
	r += ((a & 0x000000ff) << 5);
	r += ((a & 0x00ffff00) << 3);
#else
	r  =   a & 0xff000000;		/* fixed part */
	r += ((a & 0x000000ff) << 5);
	r += ((a & 0x0007ff00) << 5);
#endif

	return r;
#endif
}

/* All those functions need better names. */

static void
memcpy_fromhp_tohp(void *dest, void *src, int count)
{
	unsigned long d = ram2log(dest);
	unsigned long s = ram2log(src);

	count += 3;
	count &= ~3; /* XXX */

	while(count) {
		count --;
		gsc_writel(~gsc_readl(s), d);
		d += 32*4;
		s += 32*4;
	}
}


static void
memset_tohp(void *dest, u32 word, int count)
{
	unsigned long d = ram2log(dest);

	count += 3;
	count &= ~3;

	while(count) {
		count--;
		gsc_writel(word, d);
		d += 32;
	}
}

static u8
readb_hp(void *src)
{
	unsigned long s = ram2log(src);

	return ~gsc_readb(s);
}

static void
writeb_hp(u8 b, void *dst)
{
	unsigned long d = ram2log(dst);

	if((d&0xf0000000) != 0xf0000000) {
		printk("writeb_hp %02x %p (%08lx) (%p)\n",
			b, dst, d, __builtin_return_address(0));
		return;
	}

	gsc_writeb(b, d);
}

static void
fbcon_sti_setup(struct display *p)
{
	if (p->line_length)
		p->next_line = p->line_length;
	else
		p->next_line = p->var.xres_virtual>>3;
	p->next_plane = 0;
}

static void
fbcon_sti_bmove(struct display *p, int sy, int sx,
		int dy, int dx,
		int height, int width)
{
#if 0 /* Unfortunately, still broken */
	sti_bmove(default_sti /* FIXME */, sy, sx, dy, dx, height, width);
#else
	u8 *src, *dest;
	u_int rows;

	if (sx == 0 && dx == 0 && width == p->next_line) {
		src = p->screen_base+sy*fontheight(p)*width;
		dest = p->screen_base+dy*fontheight(p)*width;
		memcpy_fromhp_tohp(dest, src, height*fontheight(p)*width);
	} else if (dy <= sy) {
		src = p->screen_base+sy*fontheight(p)*p->next_line+sx;
		dest = p->screen_base+dy*fontheight(p)*p->next_line+dx;
		for (rows = height*fontheight(p); rows--;) {
			memcpy_fromhp_tohp(dest, src, width);
			src += p->next_line;
			dest += p->next_line;
		}
	} else {
		src = p->screen_base+((sy+height)*fontheight(p)-1)*p->next_line+sx;
		dest = p->screen_base+((dy+height)*fontheight(p)-1)*p->next_line+dx;
		for (rows = height*fontheight(p); rows--;) {
			memcpy_fromhp_tohp(dest, src, width);
			src -= p->next_line;
			dest -= p->next_line;
		}
	}
#endif
}

static void
fbcon_sti_clear(struct vc_data *conp,
		struct display *p, int sy, int sx,
		int height, int width)
{
	u8 *dest;
	u_int rows;
	int inverse = conp ? attr_reverse(p,conp->vc_video_erase_char) : 0;

	dest = p->screen_base+sy*fontheight(p)*p->next_line+sx;

	if (sx == 0 && width == p->next_line) {
		if (inverse)
			memset_tohp(dest, ~0, height*fontheight(p)*width);
		else
			memset_tohp(dest,  0, height*fontheight(p)*width);
	} else
		for (rows = height*fontheight(p); rows--; dest += p->next_line)
			if (inverse)
				memset_tohp(dest, 0xffffffff, width);
			else
				memset_tohp(dest, 0x00000000, width);
}

static void fbcon_sti_putc(struct vc_data *conp,
			   struct display *p, int c,
			   int yy, int xx)
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
		writeb_hp (d, dest);
	}
}

static void fbcon_sti_putcs(struct vc_data *conp,
			    struct display *p, 
			    const unsigned short *s,
			    int count, int yy, int xx)
{
	u8 *dest, *dest0, *cdat;
	u_int rows, bold, revs, underl;
	u8 d;
	u16 c;

	if(((unsigned)xx > 200) || ((unsigned) yy > 200)) {
		printk("refusing to putcs %p %p %p %d %d %d (%p)\n",
			conp, p, s, count, yy, xx, __builtin_return_address(0));
		return;
	}	


	dest0 = p->screen_base+yy*fontheight(p)*p->next_line+xx;
	if(((u32)dest0&0xf0000000)!=0xf0000000) {
		printk("refusing to putcs %p %p %p %d %d %d (%p) %p = %p + %d * %d * %ld + %d\n",
			conp, p, s, count, yy, xx, __builtin_return_address(0),
			dest0, p->screen_base, yy, fontheight(p), p->next_line,
			xx);
		return;
	}	

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
			if (0 && underl && !rows)
				d = 0xff;
			else if (0 && bold)
				d |= d>>1;
			if (revs)
				d = ~d;
			writeb_hp (d, dest);
		}
	}
}

static void fbcon_sti_revc(struct display *p,
			   int xx, int yy)
{
	u8 *dest, d;
	u_int rows;


	dest = p->screen_base+yy*fontheight(p)*p->next_line+xx;
	for (rows = fontheight(p); rows--; dest += p->next_line) {
		d = readb_hp(dest);
		writeb_hp (~d, dest);
	}
}

static void
fbcon_sti_clear_margins(struct vc_data *conp,
			struct display *p,
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
		memset_tohp(dest, 0xffffffff, height * p->next_line);
	else
		memset_tohp(dest, 0x00000000, height * p->next_line);
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_sti = {
	setup:		fbcon_sti_setup, 
	bmove:		fbcon_sti_bmove, 
	clear:		fbcon_sti_clear,
	putc:		fbcon_sti_putc, 
	putcs:		fbcon_sti_putcs, 
	revc:		fbcon_sti_revc,
	clear_margins:	fbcon_sti_clear_margins,
	fontwidthmask:	FONTWIDTH(8)
};

MODULE_LICENSE("GPL");
