/*
 *  linux/drivers/video/fbcon-vga-planes.c -- Low level frame buffer operations
 *				  for VGA 4-plane modes
 *
 * Copyright 1999 Ben Pfaff <pfaffben@debian.org> and Petr Vandrovec <VANDROVE@vc.cvut.cz>
 * Based on code by Michael Schmitz
 * Based on the old macfb.c 4bpp code by Alan Cox
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.  */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/vt_buffer.h>

#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-vga-planes.h>

#define GRAPHICS_ADDR_REG 0x3ce		/* Graphics address register. */
#define GRAPHICS_DATA_REG 0x3cf		/* Graphics data register. */

#define SET_RESET_INDEX 0		/* Set/Reset Register index. */
#define ENABLE_SET_RESET_INDEX 1	/* Enable Set/Reset Register index. */
#define DATA_ROTATE_INDEX 3		/* Data Rotate Register index. */
#define GRAPHICS_MODE_INDEX 5		/* Graphics Mode Register index. */
#define BIT_MASK_INDEX 8		/* Bit Mask Register index. */

/* The VGA's weird architecture often requires that we read a byte and
   write a byte to the same location.  It doesn't matter *what* byte
   we write, however.  This is because all the action goes on behind
   the scenes in the VGA's 32-bit latch register, and reading and writing
   video memory just invokes latch behavior.

   To avoid race conditions (is this necessary?), reading and writing
   the memory byte should be done with a single instruction.  One
   suitable instruction is the x86 bitwise OR.  The following
   read-modify-write routine should optimize to one such bitwise
   OR. */
static inline void rmw(volatile char *p)
{
	readb(p);
	writeb(1, p);
}

/* Set the Graphics Mode Register.  Bits 0-1 are write mode, bit 3 is
   read mode. */
static inline void setmode(int mode)
{
	outb(GRAPHICS_MODE_INDEX, GRAPHICS_ADDR_REG);
	outb(mode, GRAPHICS_DATA_REG);
}

/* Select the Bit Mask Register. */
static inline void selectmask(void)
{
	outb(BIT_MASK_INDEX, GRAPHICS_ADDR_REG);
}

/* Set the value of the Bit Mask Register.  It must already have been
   selected with selectmask(). */
static inline void setmask(int mask)
{
	outb(mask, GRAPHICS_DATA_REG);
}

/* Set the Data Rotate Register.  Bits 0-2 are rotate count, bits 3-4
   are logical operation (0=NOP, 1=AND, 2=OR, 3=XOR). */
static inline void setop(int op)
{
	outb(DATA_ROTATE_INDEX, GRAPHICS_ADDR_REG);
	outb(op, GRAPHICS_DATA_REG);
}

/* Set the Enable Set/Reset Register.  The code here always uses value
   0xf for this register.  */
static inline void setsr(int sr)
{
	outb(ENABLE_SET_RESET_INDEX, GRAPHICS_ADDR_REG);
	outb(sr, GRAPHICS_DATA_REG);
}

/* Set the Set/Reset Register. */
static inline void setcolor(int color)
{
	outb(SET_RESET_INDEX, GRAPHICS_ADDR_REG);
	outb(color, GRAPHICS_DATA_REG);
}

/* Set the value in the Graphics Address Register. */
static inline void setindex(int index)
{
	outb(index, GRAPHICS_ADDR_REG);
}

void fbcon_vga_planes_setup(struct display *p)
{
}

void fbcon_vga_planes_bmove(struct display *p, int sy, int sx, int dy, int dx,
		   int height, int width)
{
	char *src;
	char *dest;
	int line_ofs;
	int x;

	setmode(1);
	setop(0);
	setsr(0xf);

	sy *= fontheight(p);
	dy *= fontheight(p);
	height *= fontheight(p);

	if (dy < sy || (dy == sy && dx < sx)) {
		line_ofs = p->line_length - width;
		dest = p->screen_base + dx + dy * p->line_length;
		src = p->screen_base + sx + sy * p->line_length;
		while (height--) {
			for (x = 0; x < width; x++) {
				readb(src);
				writeb(0, dest);
				dest++;
				src++;
			}
			src += line_ofs;
			dest += line_ofs;
		}
	} else {
		line_ofs = p->line_length - width;
		dest = p->screen_base + dx + width + (dy + height - 1) * p->line_length;
		src = p->screen_base + sx + width + (sy + height - 1) * p->line_length;
		while (height--) {
			for (x = 0; x < width; x++) {
				dest--;
				src--;
				readb(src);
				writeb(0, dest);
			}
			src -= line_ofs;
			dest -= line_ofs;
		}
	}
}

void fbcon_vga_planes_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		   int height, int width)
{
	int line_ofs = p->line_length - width;
	char *where;
	int x;
	
	setmode(0);
	setop(0);
	setsr(0xf);
	setcolor(attr_bgcol_ec(p, conp));
	selectmask();

	setmask(0xff);

	sy *= fontheight(p);
	height *= fontheight(p);

	where = p->screen_base + sx + sy * p->line_length;
	while (height--) {
		for (x = 0; x < width; x++) {
			writeb(0, where);
			where++;
		}
		where += line_ofs;
	}
}

void fbcon_ega_planes_putc(struct vc_data *conp, struct display *p, int c, int yy, int xx)
{
	int fg = attr_fgcol(p,c);
	int bg = attr_bgcol(p,c);

	int y;
	u8 *cdat = p->fontdata + (c & p->charmask) * fontheight(p);
	char *where = p->screen_base + xx + yy * p->line_length * fontheight(p);

	setmode(0);
	setop(0);
	setsr(0xf);
	setcolor(bg);
	selectmask();

	setmask(0xff);
	for (y = 0; y < fontheight(p); y++, where += p->line_length) 
		rmw(where);

	where -= p->line_length * y;
	setcolor(fg);
	selectmask();
	for (y = 0; y < fontheight(p); y++, where += p->line_length) 
		if (cdat[y]) {
			setmask(cdat[y]);
			rmw(where);
		}
}

void fbcon_vga_planes_putc(struct vc_data *conp, struct display *p, int c, int yy, int xx)
{
	int fg = attr_fgcol(p,c);
	int bg = attr_bgcol(p,c);

	int y;
	u8 *cdat = p->fontdata + (c & p->charmask) * fontheight(p);
	char *where = p->screen_base + xx + yy * p->line_length * fontheight(p);

	setmode(2);
	setop(0);
	setsr(0xf);
	setcolor(fg);
	selectmask();

	setmask(0xff);
	writeb(bg, where);
	rmb();
	readb(where); /* fill latches */
	setmode(3);
	wmb();
	for (y = 0; y < fontheight(p); y++, where += p->line_length) 
		writeb(cdat[y], where);
	wmb();
}

/* 28.50 in my test */
void fbcon_ega_planes_putcs(struct vc_data *conp, struct display *p, const unsigned short *s,
		   int count, int yy, int xx)
{
	u16 c = scr_readw(s);
	int fg = attr_fgcol(p, c);
	int bg = attr_bgcol(p, c);

	char *where;
	int n;

	setmode(2);
	setop(0);
	selectmask();

	setmask(0xff);
	where = p->screen_base + xx + yy * p->line_length * fontheight(p);
	writeb(bg, where);
	rmb();
	readb(where); /* fill latches */
	wmb();
	selectmask();
	for (n = 0; n < count; n++) {
		int c = scr_readw(s++) & p->charmask;
		u8 *cdat = p->fontdata + c * fontheight(p);
		u8 *end = cdat + fontheight(p);

		while (cdat < end) {
			outb(*cdat++, GRAPHICS_DATA_REG);	
			wmb();
			writeb(fg, where);
			where += p->line_length;
		}
		where += 1 - p->line_length * fontheight(p);
	}
	
	wmb();
}

/* 6.96 in my test */
void fbcon_vga_planes_putcs(struct vc_data *conp, struct display *p, const unsigned short *s,
		   int count, int yy, int xx)
{
	u16 c = scr_readw(s);
	int fg = attr_fgcol(p, c);
	int bg = attr_bgcol(p, c);

	char *where;
	int n;

	setmode(2);
	setop(0);
	setsr(0xf);
	setcolor(fg);
	selectmask();

	setmask(0xff);
	where = p->screen_base + xx + yy * p->line_length * fontheight(p);
	writeb(bg, where);
	rmb();
	readb(where); /* fill latches */
	setmode(3);	
	wmb();
	for (n = 0; n < count; n++) {
		int y;
		int c = scr_readw(s++) & p->charmask;
		u8 *cdat = p->fontdata + (c & p->charmask) * fontheight(p);

		for (y = 0; y < fontheight(p); y++, cdat++) {
			writeb (*cdat, where);
			where += p->line_length;
		}
		where += 1 - p->line_length * fontheight(p);
	}
	
	wmb();
}

void fbcon_vga_planes_revc(struct display *p, int xx, int yy)
{
	char *where = p->screen_base + xx + yy * p->line_length * fontheight(p);
	int y;
	
	setmode(0);
	setop(0x18);
	setsr(0xf);
	setcolor(0xf);
	selectmask();

	setmask(0xff);
	for (y = 0; y < fontheight(p); y++) {
		rmw(where);
		where += p->line_length;
	}
}

struct display_switch fbcon_vga_planes = {
    setup:		fbcon_vga_planes_setup,
    bmove:		fbcon_vga_planes_bmove,
    clear:		fbcon_vga_planes_clear,
    putc:		fbcon_vga_planes_putc,
    putcs:		fbcon_vga_planes_putcs,
    revc:		fbcon_vga_planes_revc,
    fontwidthmask:	FONTWIDTH(8)
};

struct display_switch fbcon_ega_planes = {
    setup:		fbcon_vga_planes_setup,
    bmove:		fbcon_vga_planes_bmove,
    clear:		fbcon_vga_planes_clear,
    putc:		fbcon_ega_planes_putc,
    putcs:		fbcon_ega_planes_putcs,
    revc:		fbcon_vga_planes_revc,
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

EXPORT_SYMBOL(fbcon_vga_planes);
EXPORT_SYMBOL(fbcon_vga_planes_setup);
EXPORT_SYMBOL(fbcon_vga_planes_bmove);
EXPORT_SYMBOL(fbcon_vga_planes_clear);
EXPORT_SYMBOL(fbcon_vga_planes_putc);
EXPORT_SYMBOL(fbcon_vga_planes_putcs);
EXPORT_SYMBOL(fbcon_vga_planes_revc);

EXPORT_SYMBOL(fbcon_ega_planes);
EXPORT_SYMBOL(fbcon_ega_planes_putc);
EXPORT_SYMBOL(fbcon_ega_planes_putcs);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

