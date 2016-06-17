/*
 *  linux/drivers/video/fbcon-vga.c -- Low level frame buffer operations for
 *				       VGA characters/attributes
 *
 *	Created 28 Mar 1998 by Geert Uytterhoeven
 *	Monochrome attributes added May 1998 by Andrew Apted
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

#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-vga.h>


    /*
     *  VGA screen access
     */ 

static inline void vga_writew(u16 val, u16 *addr)
{
#ifdef __powerpc__
    st_le16(addr, val);
#else
    writew(val, (unsigned long)addr);
#endif /* !__powerpc__ */
}

static inline u16 vga_readw(u16 *addr)
{
#ifdef __powerpc__
    return ld_le16(addr);
#else
    return readw((unsigned long)addr);
#endif /* !__powerpc__ */	
}

static inline void vga_memsetw(void *s, u16 c, unsigned int count)
{
    u16 *addr = (u16 *)s;

    while (count) {
	count--;
	vga_writew(c, addr++);
    }
}

static inline void vga_memmovew(u16 *to, u16 *from, unsigned int count)
{
    if (to < from) {
	while (count) {
	    count--;
	    vga_writew(vga_readw(from++), to++);
	}
    } else {
	from += count;
	to += count;
	while (count) {
	    count--;
	    vga_writew(vga_readw(--from), --to);
	}
    }
}


    /*
     *  VGA characters/attributes
     */

static inline u16 fbcon_vga_attr(struct display *p,
				 unsigned short s)
{
        /* Underline and reverse-video are mutually exclusive on MDA.
         * Since reverse-video is used for cursors and selected areas, 
	 * it takes precedence.
         */

	return (attr_reverse(p, s) ? 0x7000 :
		(attr_underline(p, s) ? 0x0100 : 0x0700)) |
	       (attr_bold(p, s) ? 0x0800 : 0) |
	       (attr_blink(p, s) ? 0x8000 : 0);
}

void fbcon_vga_setup(struct display *p)
{
    p->next_line = p->line_length;
    p->next_plane = 0;
}

void fbcon_vga_bmove(struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
    u16 *src, *dst;
    int rows;

    if (sx == 0 && dx == 0 && width == p->next_line/2) {
	src = (u16 *)(p->screen_base+sy*p->next_line);
	dst = (u16 *)(p->screen_base+dy*p->next_line);
	vga_memmovew(dst, src, height*width);
    } else if (dy < sy || (dy == sy && dx < sx)) {
	src = (u16 *)(p->screen_base+sy*p->next_line+sx*2);
	dst = (u16 *)(p->screen_base+dy*p->next_line+dx*2);
	for (rows = height; rows-- ;) {
	    vga_memmovew(dst, src, width);
	    src += p->next_line/2;
	    dst += p->next_line/2;
	}
    } else {
	src = (u16 *)(p->screen_base+(sy+height-1)*p->next_line+sx*2);
	dst = (u16 *)(p->screen_base+(dy+height-1)*p->next_line+dx*2);
	for (rows = height; rows-- ;) {
	    vga_memmovew(dst, src, width);
	    src -= p->next_line/2;
	    dst -= p->next_line/2;
	}
    }
}

void fbcon_vga_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
    u16 *dest = (u16 *)(p->screen_base+sy*p->next_line+sx*2);
    int rows;

    if (sx == 0 && width*2 == p->next_line)      
	vga_memsetw(dest, conp->vc_video_erase_char, height*width);
    else
	for (rows = height; rows-- ; dest += p->next_line/2)
	    vga_memsetw(dest, conp->vc_video_erase_char, width);
}

void fbcon_vga_putc(struct vc_data *conp, struct display *p, int c, int y,
		    int x)
{
    u16 *dst = (u16 *)(p->screen_base+y*p->next_line+x*2);
    if (conp->vc_can_do_color)
    	vga_writew(c, dst);
    else
    	vga_writew(fbcon_vga_attr(p, c) | (c & 0xff), dst);
}

void fbcon_vga_putcs(struct vc_data *conp, struct display *p, 
		     const unsigned short *s, int count, int y, int x)
{
    u16 *dst = (u16 *)(p->screen_base+y*p->next_line+x*2);
    u16 sattr;
    if (conp->vc_can_do_color)
    	while (count--)
    	    vga_writew(scr_readw(s++), dst++);
    else {
        sattr = fbcon_vga_attr(p, scr_readw(s));
        while (count--)
	    vga_writew(sattr | ((int) (scr_readw(s++)) & 0xff), dst++);
    }
}

void fbcon_vga_revc(struct display *p, int x, int y)
{
    u16 *dst = (u16 *)(p->screen_base+y*p->next_line+x*2);
    u16 val = vga_readw(dst);
    val = (val & 0x88ff) | ((val<<4) & 0x7000) | ((val>>4) & 0x0700);
    vga_writew(val, dst);
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_vga = {
    setup:		fbcon_vga_setup,
    bmove:		fbcon_vga_bmove,
    clear:		fbcon_vga_clear,
    putc:		fbcon_vga_putc,
    putcs:		fbcon_vga_putcs,
    revc:		fbcon_vga_revc,
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

EXPORT_SYMBOL(fbcon_vga);
EXPORT_SYMBOL(fbcon_vga_setup);
EXPORT_SYMBOL(fbcon_vga_bmove);
EXPORT_SYMBOL(fbcon_vga_clear);
EXPORT_SYMBOL(fbcon_vga_putc);
EXPORT_SYMBOL(fbcon_vga_putcs);
EXPORT_SYMBOL(fbcon_vga_revc);
