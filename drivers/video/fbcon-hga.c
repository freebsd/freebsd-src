/*
 *  linux/drivers/video/fbcon-hga.c -- Low level frame buffer operations for
 *				       the Hercules graphics adaptor
 *
 *	Created 25 Nov 1999 by Ferenc Bakonyi (fero@drama.obuda.kando.hu)
 *	Based on fbcon-mfb.c by Geert Uytterhoeven
 *
 * History:
 *
 * - Revision 0.1.0 (6 Dec 1999): comment changes
 * - First release (25 Nov 1999)
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
#include <video/fbcon-hga.h>

#if 0
#define DPRINTK(args...) printk(KERN_DEBUG __FILE__": " ##args)
#else
#define DPRINTK(args...)
#endif

#define HGA_ROWADDR(row) ((row%4)*8192 + (row>>2)*90)

    /*
     *  Hercules monochrome
     */

static inline u8* rowaddr(struct display *p, u_int row)
{
	return p->screen_base + HGA_ROWADDR(row);
}
	
void fbcon_hga_setup(struct display *p)
{
	DPRINTK("fbcon_hga_setup: ll:%d\n", (int)p->line_length);

	p->next_line = p->line_length;
	p->next_plane = 0;
}

void fbcon_hga_bmove(struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
	u8 *src, *dest;
	u_int rows, y1, y2;
	
#if 0
	if (sx == 0 && dx == 0 && width == p->next_line) {
		src = p->screen_base+sy*fontheight(p)*width;
		dest = p->screen_base+dy*fontheight(p)*width;
		fb_memmove(dest, src, height*fontheight(p)*width);
	} else 
#endif
	if (dy <= sy) {
		y1 = sy*fontheight(p);
		y2 = dy*fontheight(p);
		for (rows = height*fontheight(p); rows--; ) {
			src = rowaddr(p, y1)+sx;
			dest = rowaddr(p, y2)+dx;
			fb_memmove(dest, src, width);
			y1++;
			y2++;
		}
	} else {
		y1 = (sy+height)*fontheight(p)-1;
		y2 = (dy+height)*fontheight(p)-1;
		for (rows = height*fontheight(p); rows--;) {
			src = rowaddr(p, y1)+sx;
			dest = rowaddr(p, y2)+dx;
			fb_memmove(dest, src, width);
			y1--;
			y2--;
		}
	}
}

void fbcon_hga_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
	u8 *dest;
	u_int rows, y;
	int inverse = conp ? attr_reverse(p,conp->vc_video_erase_char) : 0;

	DPRINTK("fbcon_hga_clear: sx:%d, sy:%d, height:%d, width:%d\n", sx, sy, height, width);
	
	y = sy*fontheight(p);
#if 0
	if (sx == 0 && width == p->next_line) {
		if (inverse) {
			fb_memset255(dest, height*fontheight(p)*width);
		} else {
			fb_memclear(dest, height*fontheight(p)*width);
		}
	} else
#endif	    
	for (rows = height*fontheight(p); rows--; y++) {
		dest = rowaddr(p, y)+sx;
		if (inverse) {
			fb_memset255(dest, width);
		} else {
			fb_memclear(dest, width);
		}
	}
}

void fbcon_hga_putc(struct vc_data *conp, struct display *p, int c, int yy,
		    int xx)
{
	u8 *dest, *cdat;
	u_int rows, y, bold, revs, underl;
	u8 d;

	cdat = p->fontdata+(c&p->charmask)*fontheight(p);
	bold = attr_bold(p, c);
	revs = attr_reverse(p, c);
	underl = attr_underline(p, c);
	y = yy*fontheight(p);

	for (rows = fontheight(p); rows--; y++) {
		d = *cdat++;
		if (underl && !rows)
			d = 0xff;
		else if (bold)
			d |= d>>1;
		if (revs)
			d = ~d;
		dest = rowaddr(p, y)+xx;
		*dest = d;
	}
}

void fbcon_hga_putcs(struct vc_data *conp, struct display *p, 
		     const unsigned short *s, int count, int yy, int xx)
{
	u8 *dest, *cdat;
	u_int rows, y, y0, bold, revs, underl;
	u8 d;
	u16 c;
	
	c = scr_readw(s);
	bold = attr_bold(p, c);
	revs = attr_reverse(p, c);
	underl = attr_underline(p, c);
	y0 = yy*fontheight(p);

	while (count--) {
		c = scr_readw(s++) & p->charmask;
		cdat = p->fontdata+c*fontheight(p);
		y = y0;
		for (rows = fontheight(p); rows--; y++) {
			d = *cdat++;
	    		if (underl && !rows)
				d = 0xff;
	    		else if (bold)
				d |= d>>1;
	    		if (revs)
				d = ~d;
			dest = rowaddr(p, y)+xx;
	    		*dest = d;
		}
		xx++;
	}
}

void fbcon_hga_revc(struct display *p, int xx, int yy)
{
	u8 *dest;
	u_int rows, y;

	y = yy*fontheight(p);
	for (rows = fontheight(p); rows--; y++) {
		dest = rowaddr(p, y)+xx;
		*dest = ~*dest;
	}
}

void fbcon_hga_clear_margins(struct vc_data *conp, struct display *p,
			     int bottom_only)
{
	u8 *dest;
	u_int height, y;
	int inverse = conp ? attr_reverse(p,conp->vc_video_erase_char) : 0;

	DPRINTK("fbcon_hga_clear_margins: enter\n");

	/* No need to handle right margin. */

	y = conp->vc_rows * fontheight(p);
	for (height = p->var.yres - y; height-- > 0; y++) {
		DPRINTK("fbcon_hga_clear_margins: y:%d, height:%d\n", y, height);
		dest = rowaddr(p, y);
		if (inverse) {
			fb_memset255(dest, p->next_line);
		} else {
			fb_memclear(dest, p->next_line);
		}
	}
}


	/*
	 *  `switch' for the low level operations
	 */

struct display_switch fbcon_hga = {
	setup:		fbcon_hga_setup,
	bmove:		fbcon_hga_bmove,
	clear:		fbcon_hga_clear,
	putc:		fbcon_hga_putc,
	putcs:		fbcon_hga_putcs,
	revc:		fbcon_hga_revc,
	clear_margins:	fbcon_hga_clear_margins,
	fontwidthmask:	FONTWIDTH(8)
};


#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
	return 0;
}

void cleanup_module(void)
{
}
#endif /* MODULE */


	/*
	 *  Visible symbols for modules
	 */

EXPORT_SYMBOL(fbcon_hga);
EXPORT_SYMBOL(fbcon_hga_setup);
EXPORT_SYMBOL(fbcon_hga_bmove);
EXPORT_SYMBOL(fbcon_hga_clear);
EXPORT_SYMBOL(fbcon_hga_putc);
EXPORT_SYMBOL(fbcon_hga_putcs);
EXPORT_SYMBOL(fbcon_hga_revc);
EXPORT_SYMBOL(fbcon_hga_clear_margins);
