/*
 * linux/drivers/video/riva/fbdev.c
 *
 * nVidia RIVA 128/TNT/TNT2/GeForce2/3 fb driver
 *
 * Maintained by Ani Joshi <ajoshi@kernel.crashing.org>
 *
 * Copyright 1999-2000 Jeff Garzik
 * Copyright 2000-2003 Ani Joshi
 *
 * Contributors:
 *
 *	Ani Joshi:  Lots of debugging and cleanup work, really helped
 *	get the driver going
 *
 *	Ferenc Bakonyi:  Bug fixes, cleanup, modularization
 *
 *	Jindrich Makovicka:  Accel code help, hw cursor, mtrr
 *
 * Initial template from skeletonfb.c, created 28 Dec 1997 by Geert Uytterhoeven
 * Includes riva_hw.c from nVidia, see copyright below.
 * KGI code provided the basis for state storage, init, and mode switching.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Known bugs and issues:
 *	restoring text mode fails
 *	doublescan modes are broken
 *	option 'noaccel' has no effect
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/selection.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/console.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif
#include "rivafb.h"
#include "nvreg.h"

#ifndef CONFIG_PCI		/* sanity check */
#error This driver requires PCI support.
#endif



/* version number of this driver */
#define RIVAFB_VERSION "0.9.4"



/* ------------------------------------------------------------------------- *
 *
 * various helpful macros and constants
 *
 * ------------------------------------------------------------------------- */

#undef RIVAFBDEBUG
#ifdef RIVAFBDEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#ifndef RIVA_NDEBUG
#define assert(expr) \
	if(!(expr)) { \
	printk( "Assertion failed! %s,%s,%s,line=%d\n",\
	#expr,__FILE__,__FUNCTION__,__LINE__); \
	BUG(); \
	}
#else
#define assert(expr)
#endif

#define PFX "rivafb: "

/* macro that allows you to set overflow bits */
#define SetBitField(value,from,to) SetBF(to,GetBF(value,from))
#define SetBit(n)		(1<<(n))
#define Set8Bits(value)		((value)&0xff)

/* HW cursor parameters */
#define DEFAULT_CURSOR_BLINK_RATE	(40)
#define CURSOR_HIDE_DELAY		(20)
#define CURSOR_SHOW_DELAY		(3)

#ifdef __BIG_ENDIAN
#define CURSOR_COLOR		0xff7f
#else
#define CURSOR_COLOR		0x7fff
#endif
#define TRANSPARENT_COLOR	0x0000
#define MAX_CURS		32



/* ------------------------------------------------------------------------- *
 *
 * prototypes
 *
 * ------------------------------------------------------------------------- */

static void rivafb_blank(int blank, struct fb_info *info);

extern void riva_setup_accel(struct rivafb_info *rinfo);
extern inline void wait_for_idle(struct rivafb_info *rinfo);



/* ------------------------------------------------------------------------- *
 *
 * card identification
 *
 * ------------------------------------------------------------------------- */

enum riva_chips {
	CH_RIVA_128 = 0,
	CH_RIVA_TNT,
	CH_RIVA_TNT2,
	CH_RIVA_UTNT2,	/* UTNT2 */
	CH_RIVA_VTNT2,	/* VTNT2 */
	CH_RIVA_UVTNT2,	/* VTNT2 */
	CH_RIVA_ITNT2,	/* ITNT2 */
	CH_GEFORCE_SDR,
	CH_GEFORCE_DDR,
	CH_QUADRO,
	CH_GEFORCE2_MX,
	CH_QUADRO2_MXR,
	CH_GEFORCE2_GTS,
	CH_GEFORCE2_ULTRA,
	CH_QUADRO2_PRO,
	CH_GEFORCE2_GO,
        CH_GEFORCE3,
        CH_GEFORCE3_1,
        CH_GEFORCE3_2,
        CH_QUADRO_DDC
};

/* directly indexed by riva_chips enum, above */
static struct riva_chip_info {
	const char *name;
	unsigned arch_rev;
} riva_chip_info[] __devinitdata = {
	{ "RIVA-128", NV_ARCH_03 },
	{ "RIVA-TNT", NV_ARCH_04 },
	{ "RIVA-TNT2", NV_ARCH_04 },
	{ "RIVA-UTNT2", NV_ARCH_04 },
	{ "RIVA-VTNT2", NV_ARCH_04 },
	{ "RIVA-UVTNT2", NV_ARCH_04 },
	{ "RIVA-ITNT2", NV_ARCH_04 },
	{ "GeForce-SDR", NV_ARCH_10},
	{ "GeForce-DDR", NV_ARCH_10},
	{ "Quadro", NV_ARCH_10},
	{ "GeForce2-MX", NV_ARCH_10},
	{ "Quadro2-MXR", NV_ARCH_10},
	{ "GeForce2-GTS", NV_ARCH_10},
	{ "GeForce2-ULTRA", NV_ARCH_10},
	{ "Quadro2-PRO", NV_ARCH_10},
        { "GeForce2-Go", NV_ARCH_10},
        { "GeForce3", NV_ARCH_20}, 
        { "GeForce3 Ti 200", NV_ARCH_20},
        { "GeForce3 Ti 500", NV_ARCH_20},
        { "Quadro DDC", NV_ARCH_20}
};

static struct pci_device_id rivafb_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_NVIDIA_SGS, PCI_DEVICE_ID_NVIDIA_SGS_RIVA128,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_128 },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_TNT,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_TNT },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_TNT2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_TNT2 },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_UTNT2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_UTNT2 },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_VTNT2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_VTNT2 },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_UVTNT2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_VTNT2 },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_ITNT2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RIVA_ITNT2 },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE_SDR,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE_SDR },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE_DDR,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE_DDR },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_QUADRO },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_MX,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE2_MX },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_MX2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE2_MX },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO2_MXR,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_QUADRO2_MXR },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_GTS,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE2_GTS },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_GTS2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE2_GTS },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_ULTRA,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE2_ULTRA },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO2_PRO,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_QUADRO2_PRO },
        { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_GO,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE2_GO },
        { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE3,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE3 },
        { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE3_1,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE3_1 },
        { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE3_2,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_GEFORCE3_2 },
        { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO_DDC,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_QUADRO_DDC },
	{ 0, } /* terminate list */
};
MODULE_DEVICE_TABLE(pci, rivafb_pci_tbl);



/* ------------------------------------------------------------------------- *
 *
 * framebuffer related structures
 *
 * ------------------------------------------------------------------------- */

#ifdef FBCON_HAS_CFB8
extern struct display_switch fbcon_riva8;
#endif
#ifdef FBCON_HAS_CFB16
extern struct display_switch fbcon_riva16;
#endif
#ifdef FBCON_HAS_CFB32
extern struct display_switch fbcon_riva32;
#endif

#if 0
/* describes the state of a Riva board */
struct rivafb_par {
	struct riva_regs state;	/* state of hw board */
	__u32 visual;		/* FB_VISUAL_xxx */
	unsigned depth;		/* bpp of current mode */
};
#endif

struct riva_cursor {
	int enable;
	int on;
	int vbl_cnt;
	int last_move_delay;
	int blink_rate;
	struct {
		u16 x, y;
	} pos, size;
	unsigned short image[MAX_CURS*MAX_CURS];
	struct timer_list *timer;
};



/* ------------------------------------------------------------------------- *
 *
 * global variables
 *
 * ------------------------------------------------------------------------- */

struct rivafb_info *riva_boards = NULL;

/* command line data, set in rivafb_setup() */
static char fontname[40] __initdata = { 0 };
static char noaccel __initdata = 0;
static char nomove = 0;
static char nohwcursor __initdata = 0;
static char noblink = 0;
#ifdef CONFIG_MTRR
static char nomtrr __initdata = 0;
#endif

#ifndef MODULE
static char *mode_option __initdata = NULL;
#else
static char *font = NULL;
#endif

static struct fb_var_screeninfo rivafb_default_var = {
	xres:		640,
	yres:		480,
	xres_virtual:	640,
	yres_virtual:	480,
	xoffset:	0,
	yoffset:	0,
	bits_per_pixel:	8,
	grayscale:	0,
	red:		{0, 6, 0},
	green:		{0, 6, 0},
	blue:		{0, 6, 0},
	transp:		{0, 0, 0},
	nonstd:		0,
	activate:	0,
	height:		-1,
	width:		-1,
	accel_flags:	0,
	pixclock:	39721,
	left_margin:	40,
	right_margin:	24,
	upper_margin:	32,
	lower_margin:	11,
	hsync_len:	96,
	vsync_len:	2,
	sync:		0,
	vmode:		FB_VMODE_NONINTERLACED
};

/* from GGI */
static const struct riva_regs reg_template = {
	{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* ATTR */
	 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	 0x41, 0x01, 0x0F, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* CRT  */
	 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE3,	/* 0x10 */
	 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x20 */
	 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x30 */
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00,							/* 0x40 */
	 },
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,	/* GRA  */
	 0xFF},
	{0x03, 0x01, 0x0F, 0x00, 0x0E},				/* SEQ  */
	0xEB							/* MISC */
};



/* ------------------------------------------------------------------------- *
 *
 * MMIO access macros
 *
 * ------------------------------------------------------------------------- */

static inline void CRTCout(struct rivafb_info *rinfo, unsigned char index,
			   unsigned char val)
{
	VGA_WR08(rinfo->riva.PCIO, 0x3d4, index);
	VGA_WR08(rinfo->riva.PCIO, 0x3d5, val);
}

static inline unsigned char CRTCin(struct rivafb_info *rinfo,
				   unsigned char index)
{
	VGA_WR08(rinfo->riva.PCIO, 0x3d4, index);
	return (VGA_RD08(rinfo->riva.PCIO, 0x3d5));
}

static inline void GRAout(struct rivafb_info *rinfo, unsigned char index,
			  unsigned char val)
{
	VGA_WR08(rinfo->riva.PVIO, 0x3ce, index);
	VGA_WR08(rinfo->riva.PVIO, 0x3cf, val);
}

static inline unsigned char GRAin(struct rivafb_info *rinfo,
				  unsigned char index)
{
	VGA_WR08(rinfo->riva.PVIO, 0x3ce, index);
	return (VGA_RD08(rinfo->riva.PVIO, 0x3cf));
}

static inline void SEQout(struct rivafb_info *rinfo, unsigned char index,
			  unsigned char val)
{
	VGA_WR08(rinfo->riva.PVIO, 0x3c4, index);
	VGA_WR08(rinfo->riva.PVIO, 0x3c5, val);
}

static inline unsigned char SEQin(struct rivafb_info *rinfo,
				  unsigned char index)
{
	VGA_WR08(rinfo->riva.PVIO, 0x3c4, index);
	return (VGA_RD08(rinfo->riva.PVIO, 0x3c5));
}

static inline void ATTRout(struct rivafb_info *rinfo, unsigned char index,
			   unsigned char val)
{
	VGA_WR08(rinfo->riva.PCIO, 0x3c0, index);
	VGA_WR08(rinfo->riva.PCIO, 0x3c0, val);
}

static inline unsigned char ATTRin(struct rivafb_info *rinfo,
				   unsigned char index)
{
	VGA_WR08(rinfo->riva.PCIO, 0x3c0, index);
	return (VGA_RD08(rinfo->riva.PCIO, 0x3c1));
}

static inline void MISCout(struct rivafb_info *rinfo, unsigned char val)
{
	VGA_WR08(rinfo->riva.PVIO, 0x3c2, val);
}

static inline unsigned char MISCin(struct rivafb_info *rinfo)
{
	return (VGA_RD08(rinfo->riva.PVIO, 0x3cc));
}



/* ------------------------------------------------------------------------- *
 *
 * cursor stuff
 *
 * ------------------------------------------------------------------------- */

/**
 * riva_cursor_timer_handler - blink timer
 * @dev_addr: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Cursor blink timer.
 */
static void riva_cursor_timer_handler(unsigned long dev_addr)
{
	struct rivafb_info *rinfo = (struct rivafb_info *)dev_addr;

	if (!rinfo->cursor) return;

	if (!rinfo->cursor->enable) goto out;

	if (rinfo->cursor->last_move_delay < 1000)
		rinfo->cursor->last_move_delay++;

	if (rinfo->cursor->vbl_cnt && --rinfo->cursor->vbl_cnt == 0) {
		rinfo->cursor->on ^= 1;
		if (rinfo->cursor->on)
			*(rinfo->riva.CURSORPOS) = (rinfo->cursor->pos.x & 0xFFFF)
						   | (rinfo->cursor->pos.y << 16);
		rinfo->riva.ShowHideCursor(&rinfo->riva, rinfo->cursor->on);
		if (!noblink)
			rinfo->cursor->vbl_cnt = rinfo->cursor->blink_rate;
	}
out:
	rinfo->cursor->timer->expires = jiffies + (HZ / 100);
	add_timer(rinfo->cursor->timer);
}

/**
 * rivafb_init_cursor - allocates cursor structure and starts blink timer
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Allocates cursor structure and starts blink timer.
 *
 * RETURNS:
 * Pointer to allocated cursor structure.
 *
 * CALLED FROM:
 * rivafb_init_one()
 */
static struct riva_cursor * __init rivafb_init_cursor(struct rivafb_info *rinfo)
{
	struct riva_cursor *cursor;

	cursor = kmalloc(sizeof(struct riva_cursor), GFP_KERNEL);
	if (!cursor) return 0;
	memset(cursor, 0, sizeof(*cursor));

	cursor->timer = kmalloc(sizeof(*cursor->timer), GFP_KERNEL);
	if (!cursor->timer) {
		kfree(cursor);
		return 0;
	}
	memset(cursor->timer, 0, sizeof(*cursor->timer));

	cursor->blink_rate = DEFAULT_CURSOR_BLINK_RATE;

	init_timer(cursor->timer);
	cursor->timer->expires = jiffies + (HZ / 100);
	cursor->timer->data = (unsigned long)rinfo;
	cursor->timer->function = riva_cursor_timer_handler;
	add_timer(cursor->timer);

	return cursor;
}

/**
 * rivafb_exit_cursor - stops blink timer and releases cursor structure
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Stops blink timer and releases cursor structure.
 *
 * CALLED FROM:
 * rivafb_init_one()
 * rivafb_remove_one()
 */
static void rivafb_exit_cursor(struct rivafb_info *rinfo)
{
	struct riva_cursor *cursor = rinfo->cursor;

	if (cursor) {
		if (cursor->timer) {
			del_timer_sync(cursor->timer);
			kfree(cursor->timer);
		}
		kfree(cursor);
		rinfo->cursor = 0;
	}
}

/**
 * rivafb_download_cursor - writes cursor shape into card registers
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Writes cursor shape into card registers.
 *
 * CALLED FROM:
 * riva_load_video_mode()
 */
static void rivafb_download_cursor(struct rivafb_info *rinfo)
{
	int i, save;
	int *image;
	
	if (!rinfo->cursor) return;

	image = (int *)rinfo->cursor->image;
	save = rinfo->riva.ShowHideCursor(&rinfo->riva, 0);
	for (i = 0; i < (MAX_CURS*MAX_CURS*2)/sizeof(int); i++)
		writel(image[i], rinfo->riva.CURSOR + i);

	rinfo->riva.ShowHideCursor(&rinfo->riva, save);
}

/**
 * rivafb_create_cursor - sets rectangular cursor
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 * @width: cursor width in pixels
 * @height: cursor height in pixels
 *
 * DESCRIPTION:
 * Sets rectangular cursor.
 *
 * CALLED FROM:
 * rivafb_set_font()
 * rivafb_set_var()
 */
static void rivafb_create_cursor(struct rivafb_info *rinfo, int width, int height)
{
	struct riva_cursor *c = rinfo->cursor;
	int i, j, idx;

	if (c) {
		if (width <= 0 || height <= 0) {
			width = 8;
			height = 16;
		}
		if (width > MAX_CURS) width = MAX_CURS;
		if (height > MAX_CURS) height = MAX_CURS;

		c->size.x = width;
		c->size.y = height;
		
		idx = 0;

		for (i = 0; i < height; i++) {
			for (j = 0; j < width; j++,idx++)
				c->image[idx] = CURSOR_COLOR;
			for (j = width; j < MAX_CURS; j++,idx++)
				c->image[idx] = TRANSPARENT_COLOR;
		}
		for (i = height; i < MAX_CURS; i++)
			for (j = 0; j < MAX_CURS; j++,idx++)
				c->image[idx] = TRANSPARENT_COLOR;
	}
}

/**
 * rivafb_set_font - change font size
 * @p: pointer to display object
 * @width: font width in pixels
 * @height: font height in pixels
 *
 * DESCRIPTION:
 * Callback function called if font settings changed.
 *
 * RETURNS:
 * 1 (Always succeeds.)
 */
static int rivafb_set_font(struct display *p, int width, int height)
{
	struct rivafb_info *fb = (struct rivafb_info *)(p->fb_info);

	rivafb_create_cursor(fb, width, height);
	return 1;
}

/**
 * rivafb_cursor - cursor handler
 * @p: pointer to display object
 * @mode: cursor mode (see CM_*)
 * @x: cursor x coordinate in characters
 * @y: cursor y coordinate in characters
 *
 * DESCRIPTION:
 * Cursor handler.
 */
static void rivafb_cursor(struct display *p, int mode, int x, int y)
{
	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);
	struct riva_cursor *c = rinfo->cursor;

	if (!c)	return;

	x = x * fontwidth(p) - p->var.xoffset;
	y = y * fontheight(p) - p->var.yoffset;

	if (c->pos.x == x && c->pos.y == y && (mode == CM_ERASE) == !c->enable)
		return;

	c->enable = 0;
	if (c->on) rinfo->riva.ShowHideCursor(&rinfo->riva, 0);
		
	c->pos.x = x;
	c->pos.y = y;

	switch (mode) {
	case CM_ERASE:
		c->on = 0;
		break;
	case CM_DRAW:
	case CM_MOVE:
		if (c->last_move_delay <= 1) { /* rapid cursor movement */
			c->vbl_cnt = CURSOR_SHOW_DELAY;
		} else {
			*(rinfo->riva.CURSORPOS) = (x & 0xFFFF) | (y << 16);
			rinfo->riva.ShowHideCursor(&rinfo->riva, 1);
			if (!noblink) c->vbl_cnt = CURSOR_HIDE_DELAY;
			c->on = 1;
		}
		c->last_move_delay = 0;
		c->enable = 1;
		break;
	}
}



/* ------------------------------------------------------------------------- *
 *
 * general utility functions
 *
 * ------------------------------------------------------------------------- */

/**
 * riva_set_dispsw - sets dispsw
 * @rinfo: pointer to internal driver struct for a given Riva card
 * @disp: pointer to display object
 *
 * DESCRIPTION:
 * Sets up console low level operations depending on the current? color depth
 * of the display.
 *
 * CALLED FROM:
 * rivafb_set_var()
 * rivafb_switch()
 * riva_init_disp()
 */
static void riva_set_dispsw(struct rivafb_info *rinfo, struct display *disp)
{
	int accel = disp->var.accel_flags & FB_ACCELF_TEXT;

	DPRINTK("ENTER\n");

	assert(rinfo != NULL);

	disp->dispsw_data = NULL;

	disp->screen_base = rinfo->fb_base;
	disp->type = FB_TYPE_PACKED_PIXELS;
	disp->type_aux = 0;
	disp->ypanstep = 1;
	disp->ywrapstep = 0;
	disp->can_soft_blank = 1;
	disp->inverse = 0;

	switch (disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		rinfo->dispsw = accel ? fbcon_riva8 : fbcon_cfb8;
		disp->dispsw = &rinfo->dispsw;
		disp->line_length = disp->var.xres_virtual;
		disp->visual = FB_VISUAL_PSEUDOCOLOR;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		rinfo->dispsw = accel ? fbcon_riva16 : fbcon_cfb16;
		disp->dispsw_data = &rinfo->con_cmap.cfb16;
		disp->dispsw = &rinfo->dispsw;
		disp->line_length = disp->var.xres_virtual * 2;
		disp->visual = FB_VISUAL_DIRECTCOLOR;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		rinfo->dispsw = accel ? fbcon_riva32 : fbcon_cfb32;
		disp->dispsw_data = rinfo->con_cmap.cfb32;
		disp->dispsw = &rinfo->dispsw;
		disp->line_length = disp->var.xres_virtual * 4;
		disp->visual = FB_VISUAL_DIRECTCOLOR;
		break;
#endif
	default:
		DPRINTK("Setting fbcon_dummy renderer\n");
		rinfo->dispsw = fbcon_dummy;
		disp->dispsw = &rinfo->dispsw;
	}

	/* FIXME: verify that the above code sets dsp->* fields correctly */

	if (rinfo->cursor) {
		rinfo->dispsw.cursor = rivafb_cursor;
		rinfo->dispsw.set_font = rivafb_set_font;
	}

	DPRINTK("EXIT\n");
}

/**
 * riva_wclut - set CLUT entry
 * @chip: pointer to RIVA_HW_INST object
 * @regnum: register number
 * @red: red component
 * @green: green component
 * @blue: blue component
 *
 * DESCRIPTION:
 * Sets color register @regnum.
 *
 * CALLED FROM:
 * riva_setcolreg()
 */
static void riva_wclut(RIVA_HW_INST *chip,
		       unsigned char regnum, unsigned char red,
		       unsigned char green, unsigned char blue)
{
	VGA_WR08(chip->PDIO, 0x3c8, regnum);
	VGA_WR08(chip->PDIO, 0x3c9, red);
	VGA_WR08(chip->PDIO, 0x3c9, green);
	VGA_WR08(chip->PDIO, 0x3c9, blue);
}

/**
 * riva_save_state - saves current chip state
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 * @regs: pointer to riva_regs object
 *
 * DESCRIPTION:
 * Saves current chip state to @regs.
 *
 * CALLED FROM:
 * rivafb_init_one()
 */
/* from GGI */
static void riva_save_state(struct rivafb_info *rinfo, struct riva_regs *regs)
{
	int i;

	rinfo->riva.LockUnlock(&rinfo->riva, 0);

	rinfo->riva.UnloadStateExt(&rinfo->riva, &regs->ext);

	regs->misc_output = MISCin(rinfo);

	for (i = 0; i < NUM_CRT_REGS; i++) {
		regs->crtc[i] = CRTCin(rinfo, i);
	}

	for (i = 0; i < NUM_ATC_REGS; i++) {
		regs->attr[i] = ATTRin(rinfo, i);
	}

	for (i = 0; i < NUM_GRC_REGS; i++) {
		regs->gra[i] = GRAin(rinfo, i);
	}

	for (i = 0; i < NUM_SEQ_REGS; i++) {
		regs->seq[i] = SEQin(rinfo, i);
	}
}

/**
 * riva_load_state - loads current chip state
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 * @regs: pointer to riva_regs object
 *
 * DESCRIPTION:
 * Loads chip state from @regs.
 *
 * CALLED FROM:
 * riva_load_video_mode()
 * rivafb_init_one()
 * rivafb_remove_one()
 */
/* from GGI */
static void riva_load_state(struct rivafb_info *rinfo, struct riva_regs *regs)
{
	int i;
	RIVA_HW_STATE *state = &regs->ext;

	CRTCout(rinfo, 0x11, 0x00);

	rinfo->riva.LockUnlock(&rinfo->riva, 0);

	rinfo->riva.LoadStateExt(&rinfo->riva, state);

	MISCout(rinfo, regs->misc_output);

	for (i = 0; i < NUM_CRT_REGS; i++) {
		switch (i) {
		case 0x19:
		case 0x20 ... 0x40:
			break;
		default:
			CRTCout(rinfo, i, regs->crtc[i]);
		}
	}

	for (i = 0; i < NUM_ATC_REGS; i++) {
		ATTRout(rinfo, i, regs->attr[i]);
	}

	for (i = 0; i < NUM_GRC_REGS; i++) {
		GRAout(rinfo, i, regs->gra[i]);
	}

	for (i = 0; i < NUM_SEQ_REGS; i++) {
		SEQout(rinfo, i, regs->seq[i]);
	}
}

/**
 * riva_load_video_mode - calculate timings
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 * @video_mode: video mode to set
 *
 * DESCRIPTION:
 * Calculate some timings and then send em off to riva_load_state().
 *
 * CALLED FROM:
 * rivafb_set_var()
 */
static void riva_load_video_mode(struct rivafb_info *rinfo,
				 struct fb_var_screeninfo *video_mode)
{
	struct riva_regs newmode;
	int bpp, width, hDisplaySize, hDisplay, hStart,
	    hEnd, hTotal, height, vDisplay, vStart, vEnd, vTotal, dotClock;
	int hBlankStart, hBlankEnd, vBlankStart, vBlankEnd;

	/* time to calculate */

	rivafb_blank(1, (struct fb_info *)rinfo);

	bpp = video_mode->bits_per_pixel;
	if (bpp == 16 && video_mode->green.length == 5)
		bpp = 15;
	width = video_mode->xres_virtual;
	hDisplaySize = video_mode->xres;
	hDisplay = (hDisplaySize / 8) - 1;
	hStart = (hDisplaySize + video_mode->right_margin) / 8 + 2;
	hEnd = (hDisplaySize + video_mode->right_margin +
		video_mode->hsync_len) / 8 - 1;
	hTotal = (hDisplaySize + video_mode->right_margin +
		  video_mode->hsync_len + video_mode->left_margin) / 8 - 1;
	hBlankStart = hDisplay;
	hBlankEnd = hTotal;
	height = video_mode->yres_virtual;
	vDisplay = video_mode->yres - 1;
	vStart = video_mode->yres + video_mode->lower_margin - 1;
	vEnd = video_mode->yres + video_mode->lower_margin +
	       video_mode->vsync_len - 1;
	vTotal = video_mode->yres + video_mode->lower_margin +
		 video_mode->vsync_len + video_mode->upper_margin + 2;
	vBlankStart = vDisplay;
	vBlankEnd = vTotal;
	dotClock = 1000000000 / video_mode->pixclock;

	memcpy(&newmode, &reg_template, sizeof(struct riva_regs));

	newmode.ext.screen = SetBitField(hBlankEnd,6:6,4:4)
                  | SetBitField(vBlankStart,10:10,3:3)
                  | SetBitField(vStart,10:10,2:2)
                  | SetBitField(vDisplay,10:10,1:1)
                  | SetBitField(vTotal,10:10,0:0);
    
    	newmode.ext.horiz  = SetBitField(hTotal,8:8,0:0)
                  | SetBitField(hDisplay,8:8,1:1)
                  | SetBitField(hBlankStart,8:8,2:2)
                  | SetBitField(hStart,8:8,3:3);

    	newmode.ext.extra  = SetBitField(vTotal,11:11,0:0)
                    | SetBitField(vDisplay,11:11,2:2)
                    | SetBitField(vStart,11:11,4:4)
                    | SetBitField(vBlankStart,11:11,6:6);

	if (rinfo->riva.flatPanel) {
		vStart = vTotal - 3;
		vEnd = vTotal - 2;
		vBlankStart = vStart;
		hStart = hTotal - 3;
		hEnd = hTotal - 2;
		hBlankEnd = hTotal + 4;
	}

	newmode.crtc[0x0] = Set8Bits (hTotal - 4);
	newmode.crtc[0x1] = Set8Bits (hDisplay);
	newmode.crtc[0x2] = Set8Bits (hBlankStart);
	newmode.crtc[0x3] = SetBitField(hBlankEnd,4:0,4:0)
                | SetBit(7);
	newmode.crtc[0x4] = Set8Bits (hStart);
	newmode.crtc[0x5] = SetBitField (hBlankEnd, 5: 5, 7:7)
 		| SetBitField (hEnd, 4: 0, 4:0);
	newmode.crtc[0x6] = SetBitField (vTotal, 7: 0, 7:0);
	newmode.crtc[0x7] = SetBitField (vTotal, 8: 8, 0:0)
		| SetBitField (vDisplay, 8: 8, 1:1)
		| SetBitField (vStart, 8: 8, 2:2)
		| SetBitField (vBlankStart, 8: 8, 3:3)
		| SetBit (4)
		| SetBitField (vTotal, 9: 9, 5:5)
		| SetBitField (vDisplay, 9: 9, 6:6)
		| SetBitField (vStart, 9: 9, 7:7);
	newmode.crtc[0x9] = SetBitField (vBlankStart, 9: 9, 5:5)
		| SetBit (6);
	newmode.crtc[0x10] = Set8Bits (vStart);
	newmode.crtc[0x11] = SetBitField (vEnd, 3: 0, 3:0)
		| SetBit (5);
	newmode.crtc[0x12] = Set8Bits (vDisplay);
	newmode.crtc[0x13] = ((width / 8) * ((bpp + 1) / 8)) & 0xFF;
	newmode.crtc[0x15] = Set8Bits (vBlankStart);
	newmode.crtc[0x16] = Set8Bits (vBlankEnd);

	newmode.ext.bpp = bpp;
	newmode.ext.width = width;
	newmode.ext.height = height;

	rinfo->riva.CalcStateExt(&rinfo->riva, &newmode.ext, bpp, width,
				  hDisplaySize, hDisplay, hStart, hEnd,
				  hTotal, height, vDisplay, vStart, vEnd,
				  vTotal, dotClock);

	newmode.ext.scale = rinfo->riva.PRAMDAC[0x00000848/4] & 0xfff000ff;

	if (rinfo->riva.flatPanel) {
		newmode.ext.pixel |= (1 << 7);
		newmode.ext.scale |= (1 << 8);
	}

	newmode.ext.vpll2 = rinfo->riva.PRAMDAC[0x00000520/4];

#if defined(__powerpc__)
	/*
	 * XXX only Mac cards use second DAC for flat panel
	 */
	if (rinfo->riva.flatPanel) {
		newmode.ext.pllsel |= 0x20000800;
		newmode.ext.vpll2 = newmode.ext.vpll;
	}
#endif
	rinfo->current_state = newmode;
	riva_load_state(rinfo, &rinfo->current_state);

	rinfo->riva.LockUnlock(&rinfo->riva, 0); /* important for HW cursor */
	rivafb_download_cursor(rinfo);

	rivafb_blank(0, (struct fb_info *)rinfo);
}

/**
 * riva_board_list_add - maintains board list
 * @board_list: root node of list of boards
 * @new_node: new node to be added
 *
 * DESCRIPTION:
 * Adds @new_node to the list referenced by @board_list.
 *
 * RETURNS:
 * New root node
 *
 * CALLED FROM:
 * rivafb_init_one()
 */
static struct rivafb_info *riva_board_list_add(struct rivafb_info *board_list,
					       struct rivafb_info *new_node)
{
	struct rivafb_info *i_p = board_list;

	new_node->next = NULL;

	if (board_list == NULL)
		return new_node;

	while (i_p->next != NULL)
		i_p = i_p->next;
	i_p->next = new_node;

	return board_list;
}

/**
 * riva_board_list_del - maintains board list
 * @board_list: root node of list of boards
 * @del_node: node to be removed
 *
 * DESCRIPTION:
 * Removes @del_node from the list referenced by @board_list.
 *
 * RETURNS:
 * New root node
 *
 * CALLED FROM:
 * rivafb_remove_one()
 */
static struct rivafb_info *riva_board_list_del(struct rivafb_info *board_list,
					       struct rivafb_info *del_node)
{
	struct rivafb_info *i_p = board_list;

	if (board_list == del_node)
		return del_node->next;

	while (i_p->next != del_node)
		i_p = i_p->next;
	i_p->next = del_node->next;

	return board_list;
}

/**
 * rivafb_do_maximize - 
 * @rinfo: pointer to rivafb_info object containing info for current riva board
 * @var:
 * @v:
 * @nom:
 * @den:
 *
 * DESCRIPTION:
 * .
 *
 * RETURNS:
 * -EINVAL on failure, 0 on success
 * 
 *
 * CALLED FROM:
 * rivafb_set_var()
 */
static int rivafb_do_maximize(struct rivafb_info *rinfo,
			      struct fb_var_screeninfo *var,
			      struct fb_var_screeninfo *v,
			      int nom, int den)
{
	static struct {
		int xres, yres;
	} modes[] = {
		{1600, 1280},
		{1280, 1024},
		{1024, 768},
		{800, 600},
		{640, 480},
		{-1, -1}
	};
	int i;

	/* use highest possible virtual resolution */
	if (v->xres_virtual == -1 && v->yres_virtual == -1) {
		printk(KERN_WARNING PFX
		       "using maximum available virtual resolution\n");
		for (i = 0; modes[i].xres != -1; i++) {
			if (modes[i].xres * nom / den * modes[i].yres <
			    rinfo->ram_amount / 2)
				break;
		}
		if (modes[i].xres == -1) {
			printk(KERN_ERR PFX
			       "could not find a virtual resolution that fits into video memory!!\n");
			DPRINTK("EXIT - EINVAL error\n");
			return -EINVAL;
		}
		v->xres_virtual = modes[i].xres;
		v->yres_virtual = modes[i].yres;

		printk(KERN_INFO PFX
		       "virtual resolution set to maximum of %dx%d\n",
		       v->xres_virtual, v->yres_virtual);
	} else if (v->xres_virtual == -1) {
		v->xres_virtual = (rinfo->ram_amount * den /
			(nom * v->yres_virtual * 2)) & ~15;
		printk(KERN_WARNING PFX
		       "setting virtual X resolution to %d\n", v->xres_virtual);
	} else if (v->yres_virtual == -1) {
		v->xres_virtual = (v->xres_virtual + 15) & ~15;
		v->yres_virtual = rinfo->ram_amount * den /
			(nom * v->xres_virtual * 2);
		printk(KERN_WARNING PFX
		       "setting virtual Y resolution to %d\n", v->yres_virtual);
	} else {
		v->xres_virtual = (v->xres_virtual + 15) & ~15;
		if (v->xres_virtual * nom / den * v->yres_virtual > rinfo->ram_amount) {
			printk(KERN_ERR PFX
			       "mode %dx%dx%d rejected...resolution too high to fit into video memory!\n",
			       var->xres, var->yres, var->bits_per_pixel);
			DPRINTK("EXIT - EINVAL error\n");
			return -EINVAL;
		}
	}
	
	if (v->xres_virtual * nom / den >= 8192) {
		printk(KERN_WARNING PFX
		       "virtual X resolution (%d) is too high, lowering to %d\n",
		       v->xres_virtual, 8192 * den / nom - 16);
		v->xres_virtual = 8192 * den / nom - 16;
	}
	
	if (v->xres_virtual < v->xres) {
		printk(KERN_ERR PFX
		       "virtual X resolution (%d) is smaller than real\n", v->xres_virtual);
		return -EINVAL;
	}

	if (v->yres_virtual < v->yres) {
		printk(KERN_ERR PFX
		       "virtual Y resolution (%d) is smaller than real\n", v->yres_virtual);
		return -EINVAL;
	}
	
	return 0;
}



/* ------------------------------------------------------------------------- *
 *
 * internal fb_ops helper functions
 *
 * ------------------------------------------------------------------------- */

/**
 * riva_get_cmap_len - query current color map length
 * @var: standard kernel fb changeable data
 *
 * DESCRIPTION:
 * Get current color map length.
 *
 * RETURNS:
 * Length of color map
 *
 * CALLED FROM:
 * riva_getcolreg()
 * riva_setcolreg()
 * rivafb_get_cmap()
 * rivafb_set_cmap()
 */
static int riva_get_cmap_len(const struct fb_var_screeninfo *var)
{
	int rc = 16;		/* reasonable default */

	assert(var != NULL);

	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		rc = 256;	/* pseudocolor... 256 entries HW palette */
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
		rc = 15;	/* fix for 15 bpp depths on Riva 128 based cards */
		break;
	case 16:
		rc = 16;	/* directcolor... 16 entries SW palette */
		break;		/* Mystique: truecolor, 16 entries SW palette, HW palette hardwired into 1:1 mapping */
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		rc = 16;	/* directcolor... 16 entries SW palette */
		break;		/* Mystique: truecolor, 16 entries SW palette, HW palette hardwired into 1:1 mapping */
#endif
	default:
		/* should not occur */
		break;
	}

	return rc;
}

/**
 * riva_getcolreg
 * @regno: register index
 * @red: red component
 * @green: green component
 * @blue: blue component
 * @transp: transparency
 * @info: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Read a single color register and split it into colors/transparent.
 * The return values must have a 16 bit magnitude.
 *
 * RETURNS:
 * Return != 0 for invalid regno.
 *
 * CALLED FROM:
 * rivafb_get_cmap()
 * rivafb_switch()
 * fbcmap.c:fb_get_cmap()
 *	fbgen.c:fbgen_get_cmap()
 *	fbgen.c:fbgen_switch()
 */
static int riva_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			  unsigned *blue, unsigned *transp,
			  struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;

	if (regno >= riva_get_cmap_len(&rivainfo->currcon_display->var))
		return 1;

	*red = rivainfo->palette[regno].red;
	*green = rivainfo->palette[regno].green;
	*blue = rivainfo->palette[regno].blue;
	*transp = 0;

	return 0;
}

/**
 * riva_setcolreg
 * @regno: register index
 * @red: red component
 * @green: green component
 * @blue: blue component
 * @transp: transparency
 * @info: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Set a single color register. The values supplied have a 16 bit
 * magnitude.
 *
 * RETURNS:
 * Return != 0 for invalid regno.
 *
 * CALLED FROM:
 * rivafb_set_cmap()
 * fbcmap.c:fb_set_cmap()
 *	fbgen.c:fbgen_get_cmap()
 *	fbgen.c:fbgen_install_cmap()
 *		fbgen.c:fbgen_set_var()
 *		fbgen.c:fbgen_switch()
 *		fbgen.c:fbgen_blank()
 *	fbgen.c:fbgen_blank()
 */
static int riva_setcolreg(unsigned regno, unsigned red, unsigned green,
			  unsigned blue, unsigned transp,
			  struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;
	RIVA_HW_INST *chip = &rivainfo->riva;
	struct display *p;

	DPRINTK("ENTER\n");

	assert(rivainfo != NULL);
	assert(rivainfo->currcon_display != NULL);

	p = rivainfo->currcon_display;

	if (regno >= riva_get_cmap_len(&p->var))
		return -EINVAL;

	rivainfo->palette[regno].red = red;
	rivainfo->palette[regno].green = green;
	rivainfo->palette[regno].blue = blue;

	if (p->var.grayscale) {
		/* gray = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue =
		    (red * 77 + green * 151 + blue * 28) >> 8;
	}

	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		/* "transparent" stuff is completely ignored. */
		riva_wclut(chip, regno, red >> 8, green >> 8, blue >> 8);
		break;
#endif /* FBCON_HAS_CFB8 */
#ifdef FBCON_HAS_CFB16
	case 16:
		assert(regno < 16);
		if (p->var.green.length == 5) {
			/* 0rrrrrgg gggbbbbb */
			rivainfo->con_cmap.cfb16[regno] =
			    ((red & 0xf800) >> 1) |
			    ((green & 0xf800) >> 6) | ((blue & 0xf800) >> 11);
		} else {
			/* rrrrrggg gggbbbbb */
			rivainfo->con_cmap.cfb16[regno] =
			    ((red & 0xf800) >> 0) |
			    ((green & 0xf800) >> 5) | ((blue & 0xf800) >> 11);
		}
		break;
#endif /* FBCON_HAS_CFB16 */
#ifdef FBCON_HAS_CFB32
	case 32:
		assert(regno < 16);
		rivainfo->con_cmap.cfb32[regno] =
		    ((red & 0xff00) << 8) |
		    ((green & 0xff00)) | ((blue & 0xff00) >> 8);
		break;
#endif /* FBCON_HAS_CFB32 */
	default:
		/* do nothing */
		break;
	}

	return 0;
}



/* ------------------------------------------------------------------------- *
 *
 * framebuffer operations
 *
 * ------------------------------------------------------------------------- */

static int rivafb_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;
	struct display *p;

	DPRINTK("ENTER\n");

	assert(fix != NULL);
	assert(info != NULL);
	assert(rivainfo->drvr_name && rivainfo->drvr_name[0]);
	assert(rivainfo->fb_base_phys > 0);
	assert(rivainfo->ram_amount > 0);

	p = (con < 0) ? rivainfo->info.disp : &fb_display[con];

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	sprintf(fix->id, "nVidia %s", rivainfo->drvr_name);

	fix->type = p->type;
	fix->type_aux = p->type_aux;
	fix->visual = p->visual;

	fix->xpanstep = 1;
	fix->ypanstep = 1;
	fix->ywrapstep = 0;	/* FIXME: no ywrap for now */

	fix->line_length = p->line_length;

	fix->mmio_start = rivainfo->ctrl_base_phys;
	fix->mmio_len = rivainfo->base0_region_size;
	fix->smem_start = rivainfo->fb_base_phys;
	fix->smem_len = rivainfo->ram_amount;

	switch (rivainfo->riva.Architecture) {
	case NV_ARCH_03:
		fix->accel = FB_ACCEL_NV3;
		break;
	case NV_ARCH_04:	/* riva_hw.c now doesn't distinguish between TNT & TNT2 */
		fix->accel = FB_ACCEL_NV4;
		break;
	case NV_ARCH_10:	/* FIXME: ID for GeForce */
	case NV_ARCH_20:
		fix->accel = FB_ACCEL_NV4;
		break;

	}

	DPRINTK("EXIT, returning 0\n");

	return 0;
}

static int rivafb_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;

	DPRINTK("ENTER\n");

	assert(info != NULL);
	assert(var != NULL);

	*var = (con < 0) ? rivainfo->disp.var : fb_display[con].var;

	DPRINTK("EXIT, returning 0\n");

	return 0;
}

static int rivafb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;
	struct display *dsp;
	struct fb_var_screeninfo v;
	int nom, den;		/* translating from pixels->bytes */
	int accel;
	unsigned chgvar = 0;

	DPRINTK("ENTER\n");

	assert(info != NULL);
	assert(var != NULL);

	DPRINTK("Requested: %dx%dx%d\n", var->xres, var->yres,
		var->bits_per_pixel);
	DPRINTK("  virtual: %dx%d\n", var->xres_virtual,
		var->yres_virtual);
	DPRINTK("   offset: (%d,%d)\n", var->xoffset, var->yoffset);
	DPRINTK("grayscale: %d\n", var->grayscale);

	dsp = (con < 0) ? rivainfo->info.disp : &fb_display[con];
	assert(dsp != NULL);

	/* if var has changed, we should call changevar() later */
	if (con >= 0) {
		chgvar = ((dsp->var.xres != var->xres) ||
			  (dsp->var.yres != var->yres) ||
			  (dsp->var.xres_virtual != var->xres_virtual) ||
			  (dsp->var.yres_virtual != var->yres_virtual) ||
			  (dsp->var.accel_flags != var->accel_flags) ||
			  (dsp->var.bits_per_pixel != var->bits_per_pixel)
			  || memcmp(&dsp->var.red, &var->red,
				    sizeof(var->red))
			  || memcmp(&dsp->var.green, &var->green,
				    sizeof(var->green))
			  || memcmp(&dsp->var.blue, &var->blue,
				    sizeof(var->blue)));
	}

	memcpy(&v, var, sizeof(v));

	accel = v.accel_flags & FB_ACCELF_TEXT;

	switch (v.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 1 ... 8:
		v.bits_per_pixel = 8;
		nom = 1;
		den = 1;
		v.red.offset = 0;
		v.red.length = 8;
		v.green.offset = 0;
		v.green.length = 8;
		v.blue.offset = 0;
		v.blue.length = 8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 9 ... 15:
		v.green.length = 5;
		/* fall through */
	case 16:
		v.bits_per_pixel = 16;
		nom = 2;
		den = 1;
		if (v.green.length == 5) {
			/* 0rrrrrgg gggbbbbb */
			v.red.offset = 10;
			v.green.offset = 5;
			v.blue.offset = 0;
			v.red.length = 5;
			v.green.length = 5;
			v.blue.length = 5;
		} else {
			/* rrrrrggg gggbbbbb */
			v.red.offset = 11;
			v.green.offset = 5;
			v.blue.offset = 0;
			v.red.length = 5;
			v.green.length = 6;
			v.blue.length = 5;
		}
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 17 ... 32:
		v.bits_per_pixel = 32;
		nom = 4;
		den = 1;
		v.red.offset = 16;
		v.green.offset = 8;
		v.blue.offset = 0;
		v.red.length = 8;
		v.green.length = 8;
		v.blue.length = 8;
		break;
#endif
	default:
		printk(KERN_ERR PFX
		       "mode %dx%dx%d rejected...color depth not supported.\n",
		       var->xres, var->yres, var->bits_per_pixel);
		DPRINTK("EXIT, returning -EINVAL\n");
		return -EINVAL;
	}

	if (rivafb_do_maximize(rivainfo, var, &v, nom, den) < 0)
		return -EINVAL;

	if (v.xoffset < 0)
		v.xoffset = 0;
	if (v.yoffset < 0)
		v.yoffset = 0;

	/* truncate xoffset and yoffset to maximum if too high */
	if (v.xoffset > v.xres_virtual - v.xres)
		v.xoffset = v.xres_virtual - v.xres - 1;

	if (v.yoffset > v.yres_virtual - v.yres)
		v.yoffset = v.yres_virtual - v.yres - 1;

	v.red.msb_right =
	    v.green.msb_right =
	    v.blue.msb_right =
	    v.transp.offset = v.transp.length = v.transp.msb_right = 0;

	switch (v.activate & FB_ACTIVATE_MASK) {
	case FB_ACTIVATE_TEST:
		DPRINTK("EXIT - FB_ACTIVATE_TEST\n");
		return 0;
	case FB_ACTIVATE_NXTOPEN:	/* ?? */
	case FB_ACTIVATE_NOW:
		break;		/* continue */
	default:
		DPRINTK("EXIT - unknown activation type\n");
		return -EINVAL;	/* unknown */
	}

	memcpy(&dsp->var, &v, sizeof(v));
	if (chgvar) {
		riva_set_dispsw(rivainfo, dsp);

		if (accel) {
			if (nomove)
				dsp->scrollmode = SCROLL_YNOMOVE;
			else
				dsp->scrollmode = 0;
		} else
			dsp->scrollmode = SCROLL_YREDRAW;

		if (info && info->changevar)
			info->changevar(con);
	}

	rivafb_create_cursor(rivainfo, fontwidth(dsp), fontheight(dsp));
	riva_load_video_mode(rivainfo, &v);
	if (accel) riva_setup_accel(rivainfo);

	DPRINTK("EXIT, returning 0\n");
	return 0;
}

static int rivafb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;
	struct display *dsp;

	DPRINTK("ENTER\n");

	assert(rivainfo != NULL);
	assert(cmap != NULL);

	dsp = (con < 0) ? rivainfo->info.disp : &fb_display[con];

	if (con == rivainfo->currcon) {	/* current console? */
		int rc = fb_get_cmap(cmap, kspc, riva_getcolreg, info);
		DPRINTK("EXIT - returning %d\n", rc);
		return rc;
	} else if (dsp->cmap.len)	/* non default colormap? */
		fb_copy_cmap(&dsp->cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap
			     (riva_get_cmap_len(&dsp->var)), cmap,
			     kspc ? 0 : 2);

	DPRINTK("EXIT, returning 0\n");

	return 0;
}

static int rivafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;
	struct display *dsp;
	unsigned int cmap_len;

	DPRINTK("ENTER\n");
	
	assert(rivainfo != NULL);
	assert(cmap != NULL);

	dsp = (con < 0) ? rivainfo->info.disp : &fb_display[con];

	cmap_len = riva_get_cmap_len(&dsp->var);
	if (dsp->cmap.len != cmap_len) {
		int err = fb_alloc_cmap(&dsp->cmap, cmap_len, 0);
		if (err) {
			DPRINTK("EXIT - returning %d\n", err);
			return err;
		}
	}
	if (con == rivainfo->currcon) {	/* current console? */
		int rc = fb_set_cmap(cmap, kspc, riva_setcolreg, info);
		DPRINTK("EXIT - returning %d\n", rc);
		return rc;
	} else
		fb_copy_cmap(cmap, &dsp->cmap, kspc ? 0 : 1);

	DPRINTK("EXIT, returning 0\n");

	return 0;
}

/**
 * rivafb_pan_display
 * @var: standard kernel fb changeable data
 * @con: TODO
 * @info: pointer to rivafb_info object containing info for current riva board
 *
 * DESCRIPTION:
 * Pan (or wrap, depending on the `vmode' field) the display using the
 * `xoffset' and `yoffset' fields of the `var' structure.
 * If the values don't fit, return -EINVAL.
 *
 * This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 */
static int rivafb_pan_display(struct fb_var_screeninfo *var, int con,
			      struct fb_info *info)
{
	unsigned int base;
	struct display *dsp;
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;

	DPRINTK("ENTER\n");

	assert(rivainfo != NULL);

	if (var->xoffset > (var->xres_virtual - var->xres))
		return -EINVAL;
	if (var->yoffset > (var->yres_virtual - var->yres))
		return -EINVAL;

	dsp = (con < 0) ? rivainfo->info.disp : &fb_display[con];

	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= dsp->var.yres_virtual
		    || var->xoffset) return -EINVAL;
	} else {
		if (var->xoffset + dsp->var.xres > dsp->var.xres_virtual ||
		    var->yoffset + dsp->var.yres > dsp->var.yres_virtual)
			return -EINVAL;
	}

	base = var->yoffset * dsp->line_length + var->xoffset;

	if (con == rivainfo->currcon) {
		rivainfo->riva.SetStartAddress(&rivainfo->riva, base);
	}

	dsp->var.xoffset = var->xoffset;
	dsp->var.yoffset = var->yoffset;

	if (var->vmode & FB_VMODE_YWRAP)
		dsp->var.vmode |= FB_VMODE_YWRAP;
	else
		dsp->var.vmode &= ~FB_VMODE_YWRAP;

	DPRINTK("EXIT, returning 0\n");

	return 0;
}

static int rivafb_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
			unsigned long arg, int con, struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;

	DPRINTK("ENTER\n");

	assert(rivainfo != NULL);

	/* no rivafb-specific ioctls */

	DPRINTK("EXIT, returning -EINVAL\n");

	return -EINVAL;
}

static int rivafb_rasterimg(struct fb_info *info, int start)
{
	struct rivafb_info *rinfo = (struct rivafb_info *)info;

	wait_for_idle(rinfo);

	return 0;
}

static int rivafb_switch(int con, struct fb_info *info)
{
	struct rivafb_info *rivainfo = (struct rivafb_info *)info;
	struct fb_cmap *cmap;
	struct display *dsp;
	
	DPRINTK("ENTER\n");
	
	assert(rivainfo != NULL);

	dsp = (con < 0) ? rivainfo->info.disp : &fb_display[con];

	if (rivainfo->currcon >= 0) {
		/* Do we have to save the colormap? */
		cmap = &(rivainfo->currcon_display->cmap);
		DPRINTK("switch1: con = %d, cmap.len = %d\n",
			 rivainfo->currcon, cmap->len);

		if (cmap->len) {
			DPRINTK("switch1a: %p %p %p %p\n", cmap->red,
				 cmap->green, cmap->blue, cmap->transp);
			fb_get_cmap(cmap, 1, riva_getcolreg, info);
		}
	}
	rivainfo->currcon = con;
	rivainfo->currcon_display = dsp;

	rivafb_set_var(&dsp->var, con, info);
	riva_set_dispsw(rivainfo, dsp);

	DPRINTK("EXIT, returning 0\n");
	return 0;
}

static int rivafb_updatevar(int con, struct fb_info *info)
{
	int rc;

	DPRINTK("ENTER\n");

	rc = (con < 0) ? -EINVAL : rivafb_pan_display(&fb_display[con].var,
						      con, info);
	DPRINTK("EXIT, returning %d\n", rc);
	return rc;
}

static void rivafb_blank(int blank, struct fb_info *info)
{
	unsigned char tmp, vesa;
	struct rivafb_info *rinfo = (struct rivafb_info *)info;

	DPRINTK("ENTER\n");

	assert(rinfo != NULL);

	tmp = SEQin(rinfo, 0x01) & ~0x20;	/* screen on/off */
	vesa = CRTCin(rinfo, 0x1a) & ~0xc0;	/* sync on/off */

	if (blank) {
		tmp |= 0x20;
		switch (blank - 1) {
		case VESA_NO_BLANKING:
			break;
		case VESA_VSYNC_SUSPEND:
			vesa |= 0x80;
			break;
		case VESA_HSYNC_SUSPEND:
			vesa |= 0x40;
			break;
		case VESA_POWERDOWN:
			vesa |= 0xc0;
			break;
		}
	}

	SEQout(rinfo, 0x01, tmp);
	CRTCout(rinfo, 0x1a, vesa);

	DPRINTK("EXIT\n");
}



/* ------------------------------------------------------------------------- *
 *
 * initialization helper functions
 *
 * ------------------------------------------------------------------------- */

/* kernel interface */
static struct fb_ops riva_fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	rivafb_get_fix,
	fb_get_var:	rivafb_get_var,
	fb_set_var:	rivafb_set_var,
	fb_get_cmap:	rivafb_get_cmap,
	fb_set_cmap:	rivafb_set_cmap,
	fb_pan_display:	rivafb_pan_display,
	fb_ioctl:	rivafb_ioctl,
	fb_rasterimg:	rivafb_rasterimg,
};

static int __devinit riva_init_disp_var(struct rivafb_info *rinfo)
{
#ifndef MODULE
	if (mode_option)
		fb_find_mode(&rinfo->disp.var, &rinfo->info, mode_option,
			     NULL, 0, NULL, 8);
#endif
	if (rinfo->use_default_var)
		/* We will use the modified default var */
		rinfo->disp.var = rivafb_default_var;

	return 0;
}

static int __devinit riva_init_disp(struct rivafb_info *rinfo)
{
	struct fb_info *info;
	struct display *disp;

	DPRINTK("ENTER\n");

	assert(rinfo != NULL);

	info = &rinfo->info;
	disp = &rinfo->disp;

	disp->var = rivafb_default_var;
	
	if (noaccel)
		disp->var.accel_flags &= ~FB_ACCELF_TEXT;
	else
		disp->var.accel_flags |= FB_ACCELF_TEXT;
	
	info->disp = disp;

	/* FIXME: assure that disp->cmap is completely filled out */

	rinfo->currcon_display = disp;

	if ((riva_init_disp_var(rinfo)) < 0) {
		DPRINTK("EXIT, returning -1\n");
		return -1;
	}

	riva_set_dispsw(rinfo, disp);

	DPRINTK("EXIT, returning 0\n");
	return 0;

}

static int __devinit riva_set_fbinfo(struct rivafb_info *rinfo)
{
	struct fb_info *info;

	assert(rinfo != NULL);

	info = &rinfo->info;

	strcpy(info->modename, rinfo->drvr_name);
	info->node = -1;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &riva_fb_ops;

	/* FIXME: set monspecs to what??? */

	info->display_fg = NULL;
	strncpy(info->fontname, fontname, sizeof(info->fontname));
	info->fontname[sizeof(info->fontname) - 1] = 0;

	info->changevar = NULL;
	info->switch_con = rivafb_switch;
	info->updatevar = rivafb_updatevar;
	info->blank = rivafb_blank;

	if (riva_init_disp(rinfo) < 0)	/* must be done last */
		return -1;

	return 0;
}

#ifdef CONFIG_ALL_PPC
static int riva_get_EDID_OF(struct rivafb_info *rinfo)
{
	struct device_node *dp;
	unsigned char *pedid = NULL;

	dp = pci_device_to_OF_node(rinfo->pd);
	pedid = (unsigned char *)get_property(dp, "EDID,B", 0);

	if (pedid) {
		rinfo->EDID = pedid;
		return 1;
	} else
		return 0;
}
#endif /* CONFIG_ALL_PPC */

static int riva_dfp_parse_EDID(struct rivafb_info *rinfo)
{
	unsigned char *block = rinfo->EDID;

	if (!block)
		return 0;

	/* jump to detailed timing block section */
	block += 54;

        rinfo->clock = (block[0] + (block[1] << 8));
        rinfo->panel_xres = (block[2] + ((block[4] & 0xf0) << 4));
        rinfo->hblank = (block[3] + ((block[4] & 0x0f) << 8));
        rinfo->panel_yres = (block[5] + ((block[7] & 0xf0) << 4));
        rinfo->vblank = (block[6] + ((block[7] & 0x0f) << 8));
        rinfo->hOver_plus = (block[8] + ((block[11] & 0xc0) << 2));
        rinfo->hSync_width = (block[9] + ((block[11] & 0x30) << 4));
        rinfo->vOver_plus = ((block[10] >> 4) + ((block[11] & 0x0c) << 2));
        rinfo->vSync_width = ((block[10] & 0x0f) + ((block[11] & 0x03) << 4));
        rinfo->interlaced = ((block[17] & 0x80) >> 7);
        rinfo->synct = ((block[17] & 0x18) >> 3);
        rinfo->misc = ((block[17] & 0x06) >> 1);
        rinfo->hAct_high = rinfo->vAct_high = 0;
        if (rinfo->synct == 3) {
                if (rinfo->misc & 2)
                        rinfo->hAct_high = 1;
                if (rinfo->misc & 1)
                        rinfo->vAct_high = 1;
	}

	printk("rivafb: detected DFP panel size from EDID: %dx%d\n",
		rinfo->panel_xres, rinfo->panel_yres);

	rinfo->got_dfpinfo = 1;

	return 1;
}

static void riva_update_default_var(struct rivafb_info *rinfo)
{
	struct fb_var_screeninfo *var = &rivafb_default_var;

        var->xres = rinfo->panel_xres;
        var->yres = rinfo->panel_yres;
        var->xres_virtual = rinfo->panel_xres;
        var->yres_virtual = rinfo->panel_yres;
        var->xoffset = var->yoffset = 0;
        var->bits_per_pixel = 8;
        var->pixclock = 100000000 / rinfo->clock;
        var->left_margin = (rinfo->hblank - rinfo->hOver_plus - rinfo->hSync_width);
        var->right_margin = rinfo->hOver_plus;
        var->upper_margin = (rinfo->vblank - rinfo->vOver_plus - rinfo->vSync_width);
        var->lower_margin = rinfo->vOver_plus;
        var->hsync_len = rinfo->hSync_width;
        var->vsync_len = rinfo->vSync_width;
        var->sync = 0;

        if (rinfo->synct == 3) {
                if (rinfo->hAct_high)
                        var->sync |= FB_SYNC_HOR_HIGH_ACT;
                if (rinfo->vAct_high)
                        var->sync |= FB_SYNC_VERT_HIGH_ACT;
        }
 
        var->vmode = 0;
        if (rinfo->interlaced)
                var->vmode |= FB_VMODE_INTERLACED;

	if (!noaccel)
		var->accel_flags |= FB_ACCELF_TEXT;
        
        rinfo->use_default_var = 1;
}


static void riva_get_EDID(struct rivafb_info *rinfo)
{
#ifdef CONFIG_ALL_PPC
	if (!riva_get_EDID_OF(rinfo))
		printk("rivafb: could not retrieve EDID from OF\n");
#else
	/* XXX use other methods later */
#endif
}


static void riva_get_dfpinfo(struct rivafb_info *rinfo)
{
	if (riva_dfp_parse_EDID(rinfo))
		riva_update_default_var(rinfo);

	rinfo->riva.flatPanel = rinfo->got_dfpinfo;
}



/* ------------------------------------------------------------------------- *
 *
 * PCI bus
 *
 * ------------------------------------------------------------------------- */

static int __devinit rivafb_init_one(struct pci_dev *pd,
				     const struct pci_device_id *ent)
{
	struct rivafb_info *rinfo;
	struct riva_chip_info *rci = &riva_chip_info[ent->driver_data];

	assert(pd != NULL);
	assert(rci != NULL);

	rinfo = kmalloc(sizeof(struct rivafb_info), GFP_KERNEL);
	if (!rinfo)
		goto err_out;

	memset(rinfo, 0, sizeof(struct rivafb_info));

	rinfo->drvr_name = rci->name;
	rinfo->riva.Architecture = rci->arch_rev;

	rinfo->pd = pd;
	rinfo->base0_region_size = pci_resource_len(pd, 0);
	rinfo->base1_region_size = pci_resource_len(pd, 1);

	{
		/* enable IO and mem if not already done */
		unsigned short cmd;

		pci_read_config_word(pd, PCI_COMMAND, &cmd);
		cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
		pci_write_config_word(pd, PCI_COMMAND, cmd);
	}
 
	rinfo->ctrl_base_phys = pci_resource_start(rinfo->pd, 0);
	rinfo->fb_base_phys = pci_resource_start(rinfo->pd, 1);

	rinfo->ctrl_base = ioremap(rinfo->ctrl_base_phys,
				   rinfo->base0_region_size);
	if (!rinfo->ctrl_base) {
		printk(KERN_ERR PFX "cannot ioremap MMIO base\n");
		goto err_out_free_base1;
	}
	
	riva_get_EDID(rinfo);

	riva_get_dfpinfo(rinfo);

	rinfo->riva.EnableIRQ = 0;
	rinfo->riva.PRAMDAC = (unsigned *)(rinfo->ctrl_base + 0x00680000);
	rinfo->riva.PFB = (unsigned *)(rinfo->ctrl_base + 0x00100000);
	rinfo->riva.PFIFO = (unsigned *)(rinfo->ctrl_base + 0x00002000);
	rinfo->riva.PGRAPH = (unsigned *)(rinfo->ctrl_base + 0x00400000);
	rinfo->riva.PEXTDEV = (unsigned *)(rinfo->ctrl_base + 0x00101000);
	rinfo->riva.PTIMER = (unsigned *)(rinfo->ctrl_base + 0x00009000);
	rinfo->riva.PMC = (unsigned *)(rinfo->ctrl_base + 0x00000000);
	rinfo->riva.FIFO = (unsigned *)(rinfo->ctrl_base + 0x00800000);

	rinfo->riva.PCIO = (U008 *)(rinfo->ctrl_base + 0x00601000);
	rinfo->riva.PDIO = (U008 *)(rinfo->ctrl_base + 0x00681000);
	rinfo->riva.PVIO = (U008 *)(rinfo->ctrl_base + 0x000C0000);

	rinfo->riva.IO = (MISCin(rinfo) & 0x01) ? 0x3D0 : 0x3B0;

	if (rinfo->riva.Architecture == NV_ARCH_03) {
		/*
		 * We have to map the full BASE_1 aperture for Riva128's
		 * because they use the PRAMIN set in "framebuffer" space
		 */
		if (!request_mem_region(rinfo->fb_base_phys,
					rinfo->base1_region_size, "rivafb")) {
			printk(KERN_ERR PFX "cannot reserve FB region\n");
			goto err_out_free_base0;
		}
	
		rinfo->fb_base = ioremap(rinfo->fb_base_phys,
					 rinfo->base1_region_size);
		if (!rinfo->fb_base) {
			printk(KERN_ERR PFX "cannot ioremap FB base\n");
			goto err_out_iounmap_ctrl;
		}
	}


	switch (rinfo->riva.Architecture) {
	case NV_ARCH_03:
		rinfo->riva.PRAMIN = (unsigned *)(rinfo->fb_base + 0x00C00000);
		break;
	case NV_ARCH_04:
	case NV_ARCH_10:
	case NV_ARCH_20:
		rinfo->riva.PCRTC = (unsigned *)(rinfo->ctrl_base + 0x00600000);
		rinfo->riva.PRAMIN = (unsigned *)(rinfo->ctrl_base + 0x00710000);
		break;
	}

#if defined(__powerpc__)
	/*
	 * XXX Mac cards use the second DAC for the panel
	 */
	if (rinfo->riva.flatPanel) {
		printk("rivafb: using second CRTC\n");
		rinfo->riva.PCIO = rinfo->riva.PCIO + 0x2000;
		rinfo->riva.PCRTC = rinfo->riva.PCRTC + 0x800;
		rinfo->riva.PRAMDAC = rinfo->riva.PRAMDAC + 0x800;
		rinfo->riva.PDIO = rinfo->riva.PDIO + 0x2000;
	}
#endif

	RivaGetConfig(&rinfo->riva);

	rinfo->ram_amount = rinfo->riva.RamAmountKBytes * 1024;
	rinfo->dclk_max = rinfo->riva.MaxVClockFreqKHz * 1000;

	if (rinfo->riva.Architecture != NV_ARCH_03) {
		/*
		 * Now the _normal_ chipsets can just map the amount of
		 * real physical ram instead of the whole aperture
		 */
		if (!request_mem_region(rinfo->fb_base_phys,
					rinfo->ram_amount, "rivafb")) {
			printk(KERN_ERR PFX "cannot reserve FB region\n");
			goto err_out_free_base0;
		}
	
		rinfo->fb_base = ioremap(rinfo->fb_base_phys,
					 rinfo->ram_amount);
		if (!rinfo->fb_base) {
			printk(KERN_ERR PFX "cannot ioremap FB base\n");
			goto err_out_iounmap_ctrl;
		}
	}

#ifdef CONFIG_MTRR
	if (!nomtrr) {
		rinfo->mtrr.vram = mtrr_add(rinfo->fb_base_phys,
					    rinfo->ram_amount,
					    MTRR_TYPE_WRCOMB, 1);
		if (rinfo->mtrr.vram < 0) {
			printk(KERN_ERR PFX "unable to setup MTRR\n");
		} else {
			rinfo->mtrr.vram_valid = 1;
			/* let there be speed */
			printk(KERN_INFO PFX "RIVA MTRR set to ON\n");
		}
	}
#endif /* CONFIG_MTRR */

	/* unlock io */
	CRTCout(rinfo, 0x11, 0xFF);	/* vgaHWunlock() + riva unlock (0x7F) */
	rinfo->riva.LockUnlock(&rinfo->riva, 0);

	riva_save_state(rinfo, &rinfo->initial_state);

	if (!nohwcursor) rinfo->cursor = rivafb_init_cursor(rinfo);

	if (riva_set_fbinfo(rinfo) < 0) {
		printk(KERN_ERR PFX "error setting initial video mode\n");
		goto err_out_cursor;
	}

	if (register_framebuffer((struct fb_info *)rinfo) < 0) {
		printk(KERN_ERR PFX
			"error registering riva framebuffer\n");
		goto err_out_load_state;
	}

	riva_boards = riva_board_list_add(riva_boards, rinfo);

	pci_set_drvdata(pd, rinfo);

	printk(KERN_INFO PFX
		"PCI nVidia NV%x framebuffer ver %s (%s, %dMB @ 0x%lX)\n",
		rinfo->riva.Architecture,
		RIVAFB_VERSION,
		rinfo->drvr_name,
		rinfo->ram_amount / (1024 * 1024),
		rinfo->fb_base_phys);

	return 0;

err_out_load_state:
	riva_load_state(rinfo, &rinfo->initial_state);
err_out_cursor:
	rivafb_exit_cursor(rinfo);
/* err_out_iounmap_fb: */
	iounmap(rinfo->fb_base);
err_out_iounmap_ctrl:
	iounmap(rinfo->ctrl_base);
err_out_free_base1:
	release_mem_region(rinfo->fb_base_phys, rinfo->base1_region_size);
err_out_free_base0:
	release_mem_region(rinfo->ctrl_base_phys, rinfo->base0_region_size);
err_out_kfree:
	kfree(rinfo);
err_out:
	return -ENODEV;
}

static void __devexit rivafb_remove_one(struct pci_dev *pd)
{
	struct rivafb_info *board = pci_get_drvdata(pd);
	
	if (!board)
		return;

	riva_boards = riva_board_list_del(riva_boards, board);

	riva_load_state(board, &board->initial_state);

	unregister_framebuffer((struct fb_info *)board);

	rivafb_exit_cursor(board);

#ifdef CONFIG_MTRR
	if (board->mtrr.vram_valid)
		mtrr_del(board->mtrr.vram, board->fb_base_phys,
			 board->ram_amount);
#endif /* CONFIG_MTRR */

	iounmap(board->ctrl_base);
	iounmap(board->fb_base);

	release_mem_region(board->ctrl_base_phys,
			   board->base0_region_size);
	release_mem_region(board->fb_base_phys,
			   board->ram_amount);

	kfree(board);

	pci_set_drvdata(pd, NULL);
}



/* ------------------------------------------------------------------------- *
 *
 * initialization
 *
 * ------------------------------------------------------------------------- */

#ifndef MODULE
int __init rivafb_setup(char *options)
{
	char *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		if (!strncmp(this_opt, "font:", 5)) {
			char *p;
			int i;

			p = this_opt + 5;
			for (i = 0; i < sizeof(fontname) - 1; i++)
				if (!*p || *p == ' ' || *p == ',')
					break;
			memcpy(fontname, this_opt + 5, i);
			fontname[i] = 0;

		} else if (!strncmp(this_opt, "noblink", 7)) {
			noblink = 1;
		} else if (!strncmp(this_opt, "noaccel", 7)) {
			noaccel = 1;
		} else if (!strncmp(this_opt, "nomove", 6)) {
			nomove = 1;
#ifdef CONFIG_MTRR
		} else if (!strncmp(this_opt, "nomtrr", 6)) {
			nomtrr = 1;
#endif
		} else if (!strncmp(this_opt, "nohwcursor", 10)) {
			nohwcursor = 1;
		} else
			mode_option = this_opt;
	}
	return 0;
}
#endif /* !MODULE */

static struct pci_driver rivafb_driver = {
	name:		"rivafb",
	id_table:	rivafb_pci_tbl,
	probe:		rivafb_init_one,
	remove:		__devexit_p(rivafb_remove_one),
};



/* ------------------------------------------------------------------------- *
 *
 * modularization
 *
 * ------------------------------------------------------------------------- */

int __init rivafb_init(void)
{
	int err;
#ifdef MODULE
	if (font) strncpy(fontname, font, sizeof(fontname)-1);
#endif
	err = pci_module_init(&rivafb_driver);
	if (err)
		return err;
	return 0;
}


#ifdef MODULE
static void __exit rivafb_exit(void)
{
	pci_unregister_driver(&rivafb_driver);
}

module_init(rivafb_init);
module_exit(rivafb_exit);

MODULE_PARM(font, "s");
MODULE_PARM_DESC(font, "Specifies one of the compiled-in fonts (default=none)");
MODULE_PARM(noaccel, "i");
MODULE_PARM_DESC(noaccel, "Disables hardware acceleration (0 or 1=disabled) (default=0)");
MODULE_PARM(nomove, "i");
MODULE_PARM_DESC(nomove, "Enables YSCROLL_NOMOVE (0 or 1=enabled) (default=0)");
MODULE_PARM(nohwcursor, "i");
MODULE_PARM_DESC(nohwcursor, "Disables hardware cursor (0 or 1=disabled) (default=0)");
MODULE_PARM(noblink, "i");
MODULE_PARM_DESC(noblink, "Disables hardware cursor blinking (0 or 1=disabled) (default=0)");
#ifdef CONFIG_MTRR
MODULE_PARM(nomtrr, "i");
MODULE_PARM_DESC(nomtrr, "Disables MTRR support (0 or 1=disabled) (default=0)");
#endif
#endif /* MODULE */

MODULE_AUTHOR("Ani Joshi, maintainer");
MODULE_DESCRIPTION("Framebuffer driver for nVidia Riva 128, TNT, TNT2");
MODULE_LICENSE("GPL");
