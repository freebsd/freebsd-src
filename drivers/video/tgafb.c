/*
 *  linux/drivers/video/tgafb.c -- DEC 21030 TGA frame buffer device
 *
 *	Copyright (C) 1999,2000 Martin Lucina, Tom Zerucha
 *  
 *  $Id: tgafb.c,v 1.12.2.3 2000/04/04 06:44:56 mato Exp $
 *
 *  This driver is partly based on the original TGA framebuffer device, which 
 *  was partly based on the original TGA console driver, which are
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *	Copyright (C) 1995 Jay Estabrook
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

/* KNOWN PROBLEMS/TO DO ===================================================== *
 *
 *	- How to set a single color register on 24-plane cards?
 *
 *	- Hardware cursor/other text acceleration methods
 *
 *	- Some redraws can stall kernel for several seconds
 *	  [This should now be solved by the fast memmove() patch in 2.3.6]
 *
 * KNOWN PROBLEMS/TO DO ==================================================== */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb32.h>
#include "tgafb.h"


    /*
     *  Global declarations
     */

static struct tgafb_info fb_info;
static struct tgafb_par current_par;
static int current_par_valid = 0;
static struct display disp;

static char default_fontname[40] __initdata = { 0 };
static struct fb_var_screeninfo default_var;
static int default_var_valid = 0;

static int currcon = 0;

static struct { u_char red, green, blue, pad; } palette[256];
#ifdef FBCON_HAS_CFB32
static u32 fbcon_cfb32_cmap[16];
#endif


    /*
     *  Hardware presets
     */

static unsigned int fb_offset_presets[4] = {
	TGA_8PLANE_FB_OFFSET,
	TGA_24PLANE_FB_OFFSET,
	0xffffffff,
	TGA_24PLUSZ_FB_OFFSET
};

static unsigned int deep_presets[4] = {
  0x00014000,
  0x0001440d,
  0xffffffff,
  0x0001441d
};

static unsigned int rasterop_presets[4] = {
  0x00000003,
  0x00000303,
  0xffffffff,
  0x00000303
};

static unsigned int mode_presets[4] = {
  0x00002000,
  0x00002300,
  0xffffffff,
  0x00002300
};

static unsigned int base_addr_presets[4] = {
  0x00000000,
  0x00000001,
  0xffffffff,
  0x00000001
};


    /*
     *  Predefined video modes
     *  This is a subset of the standard VESA modes, recalculated from XFree86.
     *
     *  XXX Should we store these in terms of the encoded par structs? Even better,
     *      fbcon should provide a general mechanism for doing something like this.
     */

static struct {
    const char *name;
    struct fb_var_screeninfo var;
} tgafb_predefined[] __initdata = {
    { "640x480-60", {
	640, 480, 640, 480, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 39722, 40, 24, 32, 11, 96, 2,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "800x600-56", {
	800, 600, 800, 600, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 27777, 128, 24, 22, 1, 72, 2,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "640x480-72", {
	640, 480, 640, 480, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 31746, 144, 40, 30, 8, 40, 3,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "800x600-60", {
	800, 600, 800, 600, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 25000, 88, 40, 23, 1, 128, 4,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED
    }},
    { "800x600-72", {
	800, 600, 800, 600, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 20000, 64, 56, 23, 37, 120, 6,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED
    }},
    { "1024x768-60", {
	1024, 768, 1024, 768, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 15384, 168, 8, 29, 3, 144, 6,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1152x864-60", {
	1152, 864, 1152, 864, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 11123, 208, 64, 16, 4, 256, 8,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1024x768-70", {
	1024, 768, 1024, 768, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 13333, 144, 24, 29, 3, 136, 6,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1024x768-76", {
	1024, 768, 1024, 768, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 11764, 208, 8, 36, 16, 120, 3,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1152x864-70", {
	1152, 864, 1152, 864, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 10869, 106, 56, 20, 1, 160, 10,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1280x1024-61", {
	1280, 1024, 1280, 1024, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 9090, 200, 48, 26, 1, 184, 3,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1024x768-85", {
	1024, 768, 1024, 768, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 10111, 192, 32, 34, 14, 160, 6,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1280x1024-70", {
	1280, 1024, 1280, 1024, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 7905, 224, 32, 28, 8, 160, 8,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1152x864-84", {
	1152, 864, 1152, 864, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 7407, 184, 312, 32, 0, 128, 12,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1280x1024-76", {
	1280, 1024, 1280, 1024, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 7407, 248, 32, 34, 3, 104, 3,
	0,
	FB_VMODE_NONINTERLACED
    }},
    { "1280x1024-85", {
	1280, 1024, 1280, 1024, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 6349, 224, 64, 44, 1, 160, 3,
	0,
	FB_VMODE_NONINTERLACED
    }},

    /* These are modes used by the two fixed-frequency monitors I have at home. 
     * You may or may not find these useful.
     */

    { "WYSE1", {			/* 1280x1024 @ 72 Hz, 130 Mhz clock */
	1280, 1024, 1280, 1024, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 7692, 192, 32, 47, 0, 192, 5,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED
    }},
    { "IBM3", {				/* 1280x1024 @ 70 Hz, 120 Mhz clock */
	1280, 1024, 1280, 1024, 0, 0, 0, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, FB_ACCELF_TEXT, 8333, 192, 32, 47, 0, 192, 5,
	0,
	FB_VMODE_NONINTERLACED
    }}
};

#define NUM_TOTAL_MODES    ARRAY_SIZE(tgafb_predefined)


    /*
     *  Interface used by the world
     */

static void tgafb_detect(void);
static int tgafb_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par,
		        struct fb_info_gen *info);
static int tgafb_decode_var(const struct fb_var_screeninfo *var, void *fb_par,
		        struct fb_info_gen *info);
static int tgafb_encode_var(struct fb_var_screeninfo *var, const void *fb_par,
		        struct fb_info_gen *info);
static void tgafb_get_par(void *fb_par, struct fb_info_gen *info);
static void tgafb_set_par(const void *fb_par, struct fb_info_gen *info);
static int tgafb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue, 
		u_int *transp, struct fb_info *info);
static int tgafb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, 
		u_int transp, struct fb_info *info);
static int tgafb_blank(int blank, struct fb_info_gen *info);
static void tgafb_set_disp(const void *fb_par, struct display *disp, 
		struct fb_info_gen *info);

#ifndef MODULE
int tgafb_setup(char*);
#endif

static void tgafb_set_pll(int f);
#if 1
static int tgafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);
static void tgafb_update_palette(void);
#endif


    /*
     *  Chipset specific functions
     */


static void tgafb_detect(void)
{
    return;
}


static int tgafb_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par,
	struct fb_info_gen *info)
{
    struct tgafb_par *par = (struct tgafb_par *)fb_par;

    strcpy(fix->id, fb_info.gen.info.modename);

    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->type_aux = 0;
    if (fb_info.tga_type == TGA_TYPE_8PLANE) {
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
    } else {
	fix->visual = FB_VISUAL_TRUECOLOR;
    }

    fix->line_length = par->xres * (par->bits_per_pixel >> 3);
    fix->smem_start = fb_info.tga_fb_base;
    fix->smem_len = fix->line_length * par->yres;
    fix->mmio_start = fb_info.tga_regs_base;
    fix->mmio_len = 0x1000;		/* Is this sufficient? */
    fix->xpanstep = fix->ypanstep = fix->ywrapstep = 0;
    fix->accel = FB_ACCEL_DEC_TGA;

    return 0;
}


static int tgafb_decode_var(const struct fb_var_screeninfo *var, void *fb_par,
	struct fb_info_gen *info)
{
    struct tgafb_par *par = (struct tgafb_par *)fb_par;

    /* round up some */
    if (fb_info.tga_type == TGA_TYPE_8PLANE) {
	if (var->bits_per_pixel > 8) {
	    return -EINVAL;
	}
	par->bits_per_pixel = 8;
    } else {
	if (var->bits_per_pixel > 32) {
	    return -EINVAL;
	}
	par->bits_per_pixel = 32;
    }

    /* check the values for sanity */
    if (var->xres_virtual != var->xres ||
	var->yres_virtual != var->yres ||
	var->nonstd || (1000000000/var->pixclock) > TGA_PLL_MAX_FREQ ||
	(var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED
#if 0	/* fbmon not done.  uncomment for 2.5.x -brad */
	|| !fbmon_valid_timings(var->pixclock, var->htotal, var->vtotal, info))
#else
	)
#endif
	return -EINVAL;

    /* encode video timings */
    par->htimings = ((var->xres/4) & TGA_HORIZ_ACT_LSB) | 
	(((var->xres/4) & 0x600 << 19) & TGA_HORIZ_ACT_MSB);
    par->vtimings = (var->yres & TGA_VERT_ACTIVE);
    par->htimings |= ((var->right_margin/4) << 9) & TGA_HORIZ_FP;
    par->vtimings |= (var->lower_margin << 11) & TGA_VERT_FP;
    par->htimings |= ((var->hsync_len/4) << 14) & TGA_HORIZ_SYNC;
    par->vtimings |= (var->vsync_len << 16) & TGA_VERT_SYNC;
    par->htimings |= ((var->left_margin/4) << 21) & TGA_HORIZ_BP;
    par->vtimings |= (var->upper_margin << 22) & TGA_VERT_BP;

    if (var->sync & FB_SYNC_HOR_HIGH_ACT)
	par->htimings |= TGA_HORIZ_POLARITY;
    if (var->sync & FB_SYNC_VERT_HIGH_ACT)
	par->vtimings |= TGA_VERT_POLARITY;
    if (var->sync & FB_SYNC_ON_GREEN) {
	par->sync_on_green = 1;
    } else {
	par->sync_on_green = 0;
    }

    /* store other useful values in par */
    par->xres = var->xres; 
    par->yres = var->yres;
    par->pll_freq = 1000000000/var->pixclock;
    par->bits_per_pixel = var->bits_per_pixel;

    return 0;
}


static int tgafb_encode_var(struct fb_var_screeninfo *var, const void *fb_par,
	struct fb_info_gen *info)
{
    struct tgafb_par *par = (struct tgafb_par *)fb_par;

    /* decode video timings */
    var->xres = ((par->htimings & TGA_HORIZ_ACT_LSB) | ((par->htimings & TGA_HORIZ_ACT_MSB) >> 19)) * 4;
    var->yres = (par->vtimings & TGA_VERT_ACTIVE);
    var->right_margin = ((par->htimings & TGA_HORIZ_FP) >> 9) * 4;
    var->lower_margin = ((par->vtimings & TGA_VERT_FP) >> 11);
    var->hsync_len = ((par->htimings & TGA_HORIZ_SYNC) >> 14) * 4;
    var->vsync_len = ((par->vtimings & TGA_VERT_SYNC) >> 16);
    var->left_margin = ((par->htimings & TGA_HORIZ_BP) >> 21) * 4;
    var->upper_margin = ((par->vtimings & TGA_VERT_BP) >> 22);

    if (par->htimings & TGA_HORIZ_POLARITY) 
    	var->sync |= FB_SYNC_HOR_HIGH_ACT;
    if (par->vtimings & TGA_VERT_POLARITY)
    	var->sync |= FB_SYNC_VERT_HIGH_ACT;
    if (par->sync_on_green == 1)
	var->sync |= FB_SYNC_ON_GREEN;

    var->xres_virtual = var->xres;
    var->yres_virtual = var->yres;
    var->xoffset = var->yoffset = 0;

    /* depth-related */
    if (fb_info.tga_type == TGA_TYPE_8PLANE) {
	var->red.offset = 0;
	var->green.offset = 0;
	var->blue.offset = 0;
    } else {
	var->red.offset = 16;
	var->green.offset = 8;
	var->blue.offset = 0;
    }
    var->bits_per_pixel = par->bits_per_pixel;
    var->grayscale = 0;
    var->red.length = var->green.length = var->blue.length = 8;
    var->red.msb_right = var->green.msb_right = var->blue.msb_right = 0;
    var->transp.offset = var->transp.length = var->transp.msb_right = 0;

    /* others */
    var->xoffset = var->yoffset = 0;
    var->pixclock = 1000000000/par->pll_freq;
    var->nonstd = 0;
    var->activate = 0;
    var->height = var->width = -1;
    var->accel_flags = 0;

    return 0;
}


static void tgafb_get_par(void *fb_par, struct fb_info_gen *info)
{
    struct tgafb_par *par = (struct tgafb_par *)fb_par;

    if (current_par_valid)
	*par = current_par;
    else {
	if (fb_info.tga_type == TGA_TYPE_8PLANE)
	    default_var.bits_per_pixel = 8;
	else
	    default_var.bits_per_pixel = 32;

	tgafb_decode_var(&default_var, par, info);
    }
}


static void tgafb_set_par(const void *fb_par, struct fb_info_gen *info)
{
    int i, j;
    struct tgafb_par *par = (struct tgafb_par *)fb_par;

#if 0
    /* XXX this will break console switching with X11, maybe I need to test KD_GRAPHICS? */
    /* if current_par is valid, check to see if we need to change anything */
    if (current_par_valid) {
	if (!memcmp(par, &current_par, sizeof current_par)) {
	    return;
	}
    }
#endif
    current_par = *par;
    current_par_valid = 1;

    /* first, disable video */
    TGA_WRITE_REG(TGA_VALID_VIDEO | TGA_VALID_BLANK, TGA_VALID_REG);
    
    /* write the DEEP register */
    while (TGA_READ_REG(TGA_CMD_STAT_REG) & 1) /* wait for not busy */
      continue;

    mb();
    TGA_WRITE_REG(deep_presets[fb_info.tga_type], TGA_DEEP_REG);
    while (TGA_READ_REG(TGA_CMD_STAT_REG) & 1) /* wait for not busy */
	continue;
    mb();

    /* write some more registers */
    TGA_WRITE_REG(rasterop_presets[fb_info.tga_type], TGA_RASTEROP_REG);
    TGA_WRITE_REG(mode_presets[fb_info.tga_type], TGA_MODE_REG);
    TGA_WRITE_REG(base_addr_presets[fb_info.tga_type], TGA_BASE_ADDR_REG);

    /* calculate & write the PLL */
    tgafb_set_pll(par->pll_freq);

    /* write some more registers */
    TGA_WRITE_REG(0xffffffff, TGA_PLANEMASK_REG);
    TGA_WRITE_REG(0xffffffff, TGA_PIXELMASK_REG);
    TGA_WRITE_REG(0x12345678, TGA_BLOCK_COLOR0_REG);
    TGA_WRITE_REG(0x12345678, TGA_BLOCK_COLOR1_REG);

    /* init video timing regs */
    TGA_WRITE_REG(par->htimings, TGA_HORIZ_REG);
    TGA_WRITE_REG(par->vtimings, TGA_VERT_REG);

    /* initalise RAMDAC */
    if (fb_info.tga_type == TGA_TYPE_8PLANE) { 

	/* init BT485 RAMDAC registers */
	BT485_WRITE(0xa2 | (par->sync_on_green ? 0x8 : 0x0), BT485_CMD_0);
	BT485_WRITE(0x01, BT485_ADDR_PAL_WRITE);
	BT485_WRITE(0x14, BT485_CMD_3); /* cursor 64x64 */
	BT485_WRITE(0x40, BT485_CMD_1);
	BT485_WRITE(0x20, BT485_CMD_2); /* cursor off, for now */
	BT485_WRITE(0xff, BT485_PIXEL_MASK);

	/* fill palette registers */
	BT485_WRITE(0x00, BT485_ADDR_PAL_WRITE);
	TGA_WRITE_REG(BT485_DATA_PAL, TGA_RAMDAC_SETUP_REG);

	for (i = 0; i < 16; i++) {
	    j = color_table[i];
	    TGA_WRITE_REG(default_red[j]|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(default_grn[j]|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(default_blu[j]|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	    palette[i].red=default_red[j];
	    palette[i].green=default_grn[j];
	    palette[i].blue=default_blu[j];
	}
	for (i = 0; i < 240*3; i += 4) {
	    TGA_WRITE_REG(0x55|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x00|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x00|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x00|(BT485_DATA_PAL<<8), TGA_RAMDAC_REG);
	}	  

    } else { /* 24-plane or 24plusZ */

	/* init BT463 registers */
	BT463_WRITE(BT463_REG_ACC, BT463_CMD_REG_0, 0x40);
	BT463_WRITE(BT463_REG_ACC, BT463_CMD_REG_1, 0x08);
	BT463_WRITE(BT463_REG_ACC, BT463_CMD_REG_2, 
		(par->sync_on_green ? 0x80 : 0x40));

	BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_0, 0xff);
	BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_1, 0xff);
	BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_2, 0xff);
	BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_3, 0x0f);

	BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_0, 0x00);
	BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_1, 0x00);
	BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_2, 0x00);
	BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_3, 0x00);

	/* fill the palette */
	BT463_LOAD_ADDR(0x0000);
	TGA_WRITE_REG((BT463_PALETTE<<2), TGA_RAMDAC_REG);

	for (i = 0; i < 16; i++) {
	    j = color_table[i];
	    TGA_WRITE_REG(default_red[j]|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(default_grn[j]|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(default_blu[j]|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	}
	for (i = 0; i < 512*3; i += 4) {
	    TGA_WRITE_REG(0x55|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x00|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x00|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x00|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	}	  

	/* fill window type table after start of vertical retrace */
	while (!(TGA_READ_REG(TGA_INTR_STAT_REG) & 0x01))
	    continue;
	TGA_WRITE_REG(0x01, TGA_INTR_STAT_REG);
	mb();
	while (!(TGA_READ_REG(TGA_INTR_STAT_REG) & 0x01))
	    continue;
	TGA_WRITE_REG(0x01, TGA_INTR_STAT_REG);

	BT463_LOAD_ADDR(BT463_WINDOW_TYPE_BASE);
	TGA_WRITE_REG((BT463_REG_ACC<<2), TGA_RAMDAC_SETUP_REG);
	
	for (i = 0; i < 16; i++) {
	    TGA_WRITE_REG(0x00|(BT463_REG_ACC<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x01|(BT463_REG_ACC<<10), TGA_RAMDAC_REG);
	    TGA_WRITE_REG(0x80|(BT463_REG_ACC<<10), TGA_RAMDAC_REG);
	}
   
    }

    /* finally, enable video scan
	(and pray for the monitor... :-) */
    TGA_WRITE_REG(TGA_VALID_VIDEO, TGA_VALID_REG);
}


#define DIFFCHECK(x) { if( m <= 0x3f ) { \
      int delta = f - (TGA_PLL_BASE_FREQ * (x)) / (r << shift); \
      if (delta < 0) delta = -delta; \
      if (delta < min_diff) min_diff = delta, vm = m, va = a, vr = r; } }

static void tgafb_set_pll(int f)
{
    int                 n, shift, base, min_diff, target;
    int                 r,a,m,vm = 34, va = 1, vr = 30;

    for( r = 0 ; r < 12 ; r++ )
	TGA_WRITE_REG(!r, TGA_CLOCK_REG);

    if (f > TGA_PLL_MAX_FREQ)
	f = TGA_PLL_MAX_FREQ;

    if (f >= TGA_PLL_MAX_FREQ / 2)
	shift = 0;
    else if (f >= TGA_PLL_MAX_FREQ / 4)
	shift = 1;
    else
	shift = 2;

    TGA_WRITE_REG(shift & 1, TGA_CLOCK_REG);
    TGA_WRITE_REG(shift >> 1, TGA_CLOCK_REG);

    for( r = 0 ; r < 10 ; r++ ) {
	TGA_WRITE_REG(0, TGA_CLOCK_REG);
    }

    if (f <= 120000) {
	TGA_WRITE_REG(0, TGA_CLOCK_REG);
	TGA_WRITE_REG(0, TGA_CLOCK_REG);
    }
    else if (f <= 200000) {
	TGA_WRITE_REG(1, TGA_CLOCK_REG);
	TGA_WRITE_REG(0, TGA_CLOCK_REG);
    }
    else {
	TGA_WRITE_REG(0, TGA_CLOCK_REG);
	TGA_WRITE_REG(1, TGA_CLOCK_REG);
    }

    TGA_WRITE_REG(1, TGA_CLOCK_REG);
    TGA_WRITE_REG(0, TGA_CLOCK_REG);
    TGA_WRITE_REG(0, TGA_CLOCK_REG);
    TGA_WRITE_REG(1, TGA_CLOCK_REG);
    TGA_WRITE_REG(0, TGA_CLOCK_REG);
    TGA_WRITE_REG(1, TGA_CLOCK_REG);

    target = (f << shift) / TGA_PLL_BASE_FREQ;
    min_diff = TGA_PLL_MAX_FREQ;

    r = 7 / target;
    if (!r)
	r = 1;

    base = target * r;
    while (base < 449) {
	for (n = base < 7 ? 7 : base ; n < base + target && n < 449; n++) {
	m = ((n + 3) / 7) - 1;
	a = 0;
	DIFFCHECK((m + 1) * 7);
	m++;
	DIFFCHECK((m + 1) * 7);
	m = (n / 6) - 1;
	if( (a = n % 6))
	    DIFFCHECK( n );
	}
	r++;
	base += target;
    }

    vr--;

    for( r=0; r<8 ; r++) {
	TGA_WRITE_REG((vm >> r) & 1, TGA_CLOCK_REG);
    }
    for( r=0; r<8 ; r++) {
	TGA_WRITE_REG((va >> r) & 1, TGA_CLOCK_REG);
    }
    for( r=0; r<7 ; r++) {
	TGA_WRITE_REG((vr >> r) & 1, TGA_CLOCK_REG);
    }
    TGA_WRITE_REG(((vr >> 7) & 1)|2, TGA_CLOCK_REG);
}


static int tgafb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
    if (regno > 255)
	return 1;
    *red = (palette[regno].red<<8) | palette[regno].red;
    *green = (palette[regno].green<<8) | palette[regno].green;
    *blue = (palette[regno].blue<<8) | palette[regno].blue;
    *transp = 0;
    return 0;
}


static int tgafb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
    if (regno > 255)
	return 1;
    red >>= 8;
    green >>= 8;
    blue >>= 8;
    palette[regno].red = red;
    palette[regno].green = green;
    palette[regno].blue = blue;

#ifdef FBCON_HAS_CFB32
    if (regno < 16 && fb_info.tga_type != TGA_TYPE_8PLANE)
	fbcon_cfb32_cmap[regno] = (red << 16) | (green << 8) | blue;
#endif

    if (fb_info.tga_type == TGA_TYPE_8PLANE) { 
        BT485_WRITE(regno, BT485_ADDR_PAL_WRITE);
        TGA_WRITE_REG(BT485_DATA_PAL, TGA_RAMDAC_SETUP_REG);
        TGA_WRITE_REG(red|(BT485_DATA_PAL<<8),TGA_RAMDAC_REG);
        TGA_WRITE_REG(green|(BT485_DATA_PAL<<8),TGA_RAMDAC_REG);
        TGA_WRITE_REG(blue|(BT485_DATA_PAL<<8),TGA_RAMDAC_REG);
    }                                                    
    /* How to set a single color register on 24-plane cards?? */

    return 0;
}

#if 1
    /*
     *	FIXME: since I don't know how to set a single arbitrary color register
     *  on 24-plane cards, all color palette registers have to be updated
     */

static int tgafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
    int err;

    if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
	if ((err = fb_alloc_cmap(&fb_display[con].cmap, 256, 0)))
	    return err;
    }
    if (con == currcon) {		/* current console? */
	err = fb_set_cmap(cmap, kspc, tgafb_setcolreg, info);
#if 1
	if (fb_info.tga_type != TGA_TYPE_8PLANE)
		tgafb_update_palette();
#endif
	return err;
    } else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}

static void tgafb_update_palette(void)
{
    int i;

    BT463_LOAD_ADDR(0x0000);
    TGA_WRITE_REG((BT463_PALETTE<<2), TGA_RAMDAC_REG);

    for (i = 0; i < 256; i++) {
	 TGA_WRITE_REG(palette[i].red|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	 TGA_WRITE_REG(palette[i].green|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
	 TGA_WRITE_REG(palette[i].blue|(BT463_PALETTE<<10), TGA_RAMDAC_REG);
    }
}
#endif


static int tgafb_blank(int blank, struct fb_info_gen *info)
{
    static int tga_vesa_blanked = 0;
    u32 vhcr, vvcr, vvvr;
    unsigned long flags;
    
    save_flags(flags);
    cli();

    vhcr = TGA_READ_REG(TGA_HORIZ_REG);
    vvcr = TGA_READ_REG(TGA_VERT_REG);
    vvvr = TGA_READ_REG(TGA_VALID_REG) & ~(TGA_VALID_VIDEO | TGA_VALID_BLANK);

    switch (blank) {
    case 0: /* Unblanking */
        if (tga_vesa_blanked) {
	   TGA_WRITE_REG(vhcr & 0xbfffffff, TGA_HORIZ_REG);
	   TGA_WRITE_REG(vvcr & 0xbfffffff, TGA_VERT_REG);
	   tga_vesa_blanked = 0;
	}
 	TGA_WRITE_REG(vvvr | TGA_VALID_VIDEO, TGA_VALID_REG);
	break;

    case 1: /* Normal blanking */
	TGA_WRITE_REG(vvvr | TGA_VALID_VIDEO | TGA_VALID_BLANK, TGA_VALID_REG);
	break;

    case 2: /* VESA blank (vsync off) */
	TGA_WRITE_REG(vvcr | 0x40000000, TGA_VERT_REG);
	TGA_WRITE_REG(vvvr | TGA_VALID_BLANK, TGA_VALID_REG);
	tga_vesa_blanked = 1;
	break;

    case 3: /* VESA blank (hsync off) */
	TGA_WRITE_REG(vhcr | 0x40000000, TGA_HORIZ_REG);
	TGA_WRITE_REG(vvvr | TGA_VALID_BLANK, TGA_VALID_REG);
	tga_vesa_blanked = 1;
	break;

    case 4: /* Poweroff */
	TGA_WRITE_REG(vhcr | 0x40000000, TGA_HORIZ_REG);
	TGA_WRITE_REG(vvcr | 0x40000000, TGA_VERT_REG);
	TGA_WRITE_REG(vvvr | TGA_VALID_BLANK, TGA_VALID_REG);
	tga_vesa_blanked = 1;
	break;
    }

    restore_flags(flags);
    return 0;
}


static void tgafb_set_disp(const void *fb_par, struct display *disp,
	struct fb_info_gen *info)
{
    disp->screen_base = (char *)fb_info.tga_fb_base;
    switch (fb_info.tga_type) {
#ifdef FBCON_HAS_CFB8
	case TGA_TYPE_8PLANE:
	    disp->dispsw = &fbcon_cfb8;
            break;
#endif
#ifdef FBCON_HAS_CFB32
        case TGA_TYPE_24PLANE:
        case TGA_TYPE_24PLUSZ:
	    disp->dispsw = &fbcon_cfb32; 
            disp->dispsw_data = &fbcon_cfb32_cmap;
            break;
#endif
        default:
            disp->dispsw = &fbcon_dummy;
    }

    disp->scrollmode = SCROLL_YREDRAW;
}


struct fbgen_hwswitch tgafb_hwswitch = {
    tgafb_detect, tgafb_encode_fix, tgafb_decode_var, tgafb_encode_var, tgafb_get_par,
    tgafb_set_par, tgafb_getcolreg, tgafb_setcolreg, NULL, tgafb_blank, 
    tgafb_set_disp
};


    /*
     *  Hardware Independent functions
     */


    /* 
     *  Frame buffer operations
     */

static struct fb_ops tgafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	tgafb_set_cmap,
};


#ifndef MODULE
    /*
     *  Setup
     */

int __init tgafb_setup(char *options) {
    char *this_opt;
    int i;
    
    if (options && *options) {
    	while ((this_opt = strsep(&options, ",")) != NULL) {
       	    if (!*this_opt) { continue; }
        
	    if (!strncmp(this_opt, "font:", 5)) {
	     	strncpy(default_fontname, this_opt+5, sizeof default_fontname);
	    }

	    else if (!strncmp(this_opt, "mode:", 5)) {
    		for (i = 0; i < NUM_TOTAL_MODES; i++) {
    		    if (!strcmp(this_opt+5, tgafb_predefined[i].name))
    			default_var = tgafb_predefined[i].var;
		    	default_var_valid = 1;
    		}
    	    } 
	    
	    else {
      		printk(KERN_ERR "tgafb: unknown parameter %s\n", this_opt);
    	    }
      	}
    }
    return 0;
}
#endif


    /*
     *  Initialisation
     */

int __init tgafb_init(void)
{
    struct pci_dev *pdev;

    pdev = pci_find_device(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_TGA, NULL);
    if (!pdev)
	return -ENXIO;

    /* divine board type */

    fb_info.tga_mem_base = (unsigned long)ioremap(pdev->resource[0].start, 0);
    fb_info.tga_type = (readl(fb_info.tga_mem_base) >> 12) & 0x0f;
    fb_info.tga_regs_base = fb_info.tga_mem_base + TGA_REGS_OFFSET;
    fb_info.tga_fb_base = (fb_info.tga_mem_base
			   + fb_offset_presets[fb_info.tga_type]);
    pci_read_config_byte(pdev, PCI_REVISION_ID, &fb_info.tga_chip_rev);

    /* setup framebuffer */

    fb_info.gen.info.node = -1;
    fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
    fb_info.gen.info.fbops = &tgafb_ops;
    fb_info.gen.info.disp = &disp;
    fb_info.gen.info.changevar = NULL;
    fb_info.gen.info.switch_con = &fbgen_switch;
    fb_info.gen.info.updatevar = &fbgen_update_var;
    fb_info.gen.info.blank = &fbgen_blank;
    strcpy(fb_info.gen.info.fontname, default_fontname);
    fb_info.gen.parsize = sizeof (struct tgafb_par);
    fb_info.gen.fbhw = &tgafb_hwswitch;
    fb_info.gen.fbhw->detect();

    printk (KERN_INFO "tgafb: DC21030 [TGA] detected, rev=0x%02x\n", fb_info.tga_chip_rev);
    printk (KERN_INFO "tgafb: at PCI bus %d, device %d, function %d\n", 
	    pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
	    
    switch (fb_info.tga_type) 
    { 
	case TGA_TYPE_8PLANE:
	    strcpy (fb_info.gen.info.modename,"Digital ZLXp-E1"); 
	    break;

	case TGA_TYPE_24PLANE:
	    strcpy (fb_info.gen.info.modename,"Digital ZLXp-E2"); 
	    break;

	case TGA_TYPE_24PLUSZ:
	    strcpy (fb_info.gen.info.modename,"Digital ZLXp-E3"); 
	    break;
    }

    /* This should give a reasonable default video mode */

    if (!default_var_valid) {
	default_var = tgafb_predefined[0].var;
    }
    fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
    disp.var.activate = FB_ACTIVATE_NOW;
    fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
    fbgen_set_disp(-1, &fb_info.gen);
    fbgen_install_cmap(0, &fb_info.gen);
    if (register_framebuffer(&fb_info.gen.info) < 0)
	return -EINVAL;
    printk(KERN_INFO "fb%d: %s frame buffer device at 0x%lx\n", 
	    GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename, 
	    pdev->resource[0].start);
    return 0;
}


    /*
     *  Cleanup
     */

void __exit tgafb_cleanup(void)
{
    unregister_framebuffer(&fb_info.gen.info);
}


    /*
     *  Modularisation
     */

#ifdef MODULE
MODULE_LICENSE("GPL");
module_init(tgafb_init);
#endif

module_exit(tgafb_cleanup);
