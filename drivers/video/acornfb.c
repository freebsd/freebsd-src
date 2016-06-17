/*
 *  linux/drivers/video/acornfb.c
 *
 *  Copyright (C) 1998-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Frame buffer code for Acorn platforms
 *
 * NOTE: Most of the modes with X!=640 will disappear shortly.
 * NOTE: Startup setting of HS & VS polarity not supported.
 *       (do we need to support it if we're coming up in 640x480?)
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fb.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb32.h>

#include "acornfb.h"

/*
 * VIDC machines can't do 16 or 32BPP modes.
 */
#ifdef HAS_VIDC
#undef FBCON_HAS_CFB16
#undef FBCON_HAS_CFB32
#endif

/*
 * Default resolution.
 * NOTE that it has to be supported in the table towards
 * the end of this file.
 */
#define DEFAULT_XRES	640
#define DEFAULT_YRES	480
/*
 * The order here defines which BPP we
 * pick depending on which resolutions
 * we have configured.
 */
#if   defined(FBCON_HAS_CFB4)
# define DEFAULT_BPP	4
#elif defined(FBCON_HAS_CFB8)
# define DEFAULT_BPP	8
#elif defined(FBCON_HAS_CFB16)
# define DEFAULT_BPP	16
#elif defined(FBCON_HAS_CFB2)
# define DEFAULT_BPP	2
#elif defined(FBCON_HAS_MFB)
# define DEFAULT_BPP	1
#else
#error No suitable framebuffers configured
#endif


/*
 * define this to debug the video mode selection
 */
#undef DEBUG_MODE_SELECTION

/*
 * Translation from RISC OS monitor types to actual
 * HSYNC and VSYNC frequency ranges.  These are
 * probably not right, but they're the best info I
 * have.  Allow 1% either way on the nominal for TVs.
 */
#define NR_MONTYPES	6
static struct fb_monspecs monspecs[NR_MONTYPES] __initdata = {
	{ 15469, 15781, 49, 51, 0 },	/* TV		*/
	{     0, 99999,  0, 99, 0 },	/* Multi Freq	*/
	{ 58608, 58608, 64, 64, 0 },	/* Hi-res mono	*/
	{ 30000, 70000, 60, 60, 0 },	/* VGA		*/
	{ 30000, 70000, 56, 75, 0 },	/* SVGA		*/
	{ 30000, 70000, 60, 60, 0 }
};

static struct display global_disp;
static struct fb_info fb_info;
static struct acornfb_par current_par;
static struct vidc_timing current_vidc;
static struct fb_var_screeninfo __initdata init_var = {};

extern int acornfb_depth;	/* set by setup.c */
extern unsigned int vram_size;	/* set by setup.c */

#ifdef HAS_VIDC

#define MAX_SIZE	480*1024

/* CTL     VIDC	Actual
 * 24.000  0	 8.000
 * 25.175  0	 8.392
 * 36.000  0	12.000
 * 24.000  1	12.000
 * 25.175  1	12.588
 * 24.000  2	16.000
 * 25.175  2	16.783
 * 36.000  1	18.000
 * 24.000  3	24.000
 * 36.000  2	24.000
 * 25.175  3	25.175
 * 36.000  3	36.000
 */
struct pixclock {
	u_long	min_clock;
	u_long	max_clock;
	u_int	vidc_ctl;
	u_int	vid_ctl;
};

static struct pixclock arc_clocks[] = {
	/* we allow +/-1% on these */
	{ 123750, 126250, VIDC_CTRL_DIV3,   VID_CTL_24MHz },	/*  8.000MHz */
	{  82500,  84167, VIDC_CTRL_DIV2,   VID_CTL_24MHz },	/* 12.000MHz */
	{  61875,  63125, VIDC_CTRL_DIV1_5, VID_CTL_24MHz },	/* 16.000MHz */
	{  41250,  42083, VIDC_CTRL_DIV1,   VID_CTL_24MHz },	/* 24.000MHz */
};

#ifdef CONFIG_ARCH_A5K
static struct pixclock a5k_clocks[] = {
	{ 117974, 120357, VIDC_CTRL_DIV3,   VID_CTL_25MHz },	/*  8.392MHz */
	{  78649,  80238, VIDC_CTRL_DIV2,   VID_CTL_25MHz },	/* 12.588MHz */
	{  58987,  60178, VIDC_CTRL_DIV1_5, VID_CTL_25MHz },	/* 16.588MHz */
	{  55000,  56111, VIDC_CTRL_DIV2,   VID_CTL_36MHz },	/* 18.000MHz */
	{  39325,  40119, VIDC_CTRL_DIV1,   VID_CTL_25MHz },	/* 25.175MHz */
	{  27500,  28055, VIDC_CTRL_DIV1,   VID_CTL_36MHz },	/* 36.000MHz */
};
#endif

static struct pixclock *
acornfb_valid_pixrate(u_long pixclock)
{
	u_int i;

	for (i = 0; i < ARRAY_SIZE(arc_clocks); i++)
		if (pixclock > arc_clocks[i].min_clock &&
		    pixclock < arc_clocks[i].max_clock)
			return arc_clocks + i;

#ifdef CONFIG_ARCH_A5K
	if (machine_is_a5k()) {
		for (i = 0; i < ARRAY_SIZE(a5k_clocks); i++)
			if (pixclock > a5k_clocks[i].min_clock &&
			    pixclock < a5k_clocks[i].max_clock)
				return a5k_clocks + i;
	}
#endif

	return NULL;
}

/* VIDC Rules:
 * hcr  : must be even (interlace, hcr/2 must be even)
 * hswr : must be even
 * hdsr : must be odd
 * hder : must be odd
 *
 * vcr  : must be odd
 * vswr : >= 1
 * vdsr : >= 1
 * vder : >= vdsr
 * if interlaced, then hcr/2 must be even
 */
static void
acornfb_set_timing(struct fb_var_screeninfo *var)
{
	struct pixclock *pclk;
	struct vidc_timing vidc;
	u_int horiz_correction;
	u_int sync_len, display_start, display_end, cycle;
	u_int is_interlaced;
	u_int vid_ctl, vidc_ctl;
	u_int bandwidth;

	memset(&vidc, 0, sizeof(vidc));

	pclk = acornfb_valid_pixrate(var->pixclock);
	vidc_ctl = pclk->vidc_ctl;
	vid_ctl  = pclk->vid_ctl;

	bandwidth = var->pixclock * 8 / var->bits_per_pixel;
	/* 25.175, 4bpp = 79.444ns per byte, 317.776ns per word: fifo = 2,6 */
	if (bandwidth > 143500)
		vidc_ctl |= VIDC_CTRL_FIFO_3_7;
	else if (bandwidth > 71750)
		vidc_ctl |= VIDC_CTRL_FIFO_2_6;
	else if (bandwidth > 35875)
		vidc_ctl |= VIDC_CTRL_FIFO_1_5;
	else
		vidc_ctl |= VIDC_CTRL_FIFO_0_4;

	switch (var->bits_per_pixel) {
	case 1:
		horiz_correction = 19;
		vidc_ctl |= VIDC_CTRL_1BPP;
		break;

	case 2:
		horiz_correction = 11;
		vidc_ctl |= VIDC_CTRL_2BPP;
		break;

	case 4:
		horiz_correction = 7;
		vidc_ctl |= VIDC_CTRL_4BPP;
		break;

	default:
	case 8:
		horiz_correction = 5;
		vidc_ctl |= VIDC_CTRL_8BPP;
		break;
	}

	if (var->sync & FB_SYNC_COMP_HIGH_ACT) /* should be FB_SYNC_COMP */
		vidc_ctl |= VIDC_CTRL_CSYNC;
	else {
		if (!(var->sync & FB_SYNC_HOR_HIGH_ACT))
			vid_ctl |= VID_CTL_HS_NHSYNC;

		if (!(var->sync & FB_SYNC_VERT_HIGH_ACT))
			vid_ctl |= VID_CTL_VS_NVSYNC;
	}

	sync_len	= var->hsync_len;
	display_start	= sync_len + var->left_margin;
	display_end	= display_start + var->xres;
	cycle		= display_end + var->right_margin;

	/* if interlaced, then hcr/2 must be even */
	is_interlaced = (var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED;

	if (is_interlaced) {
		vidc_ctl |= VIDC_CTRL_INTERLACE;
		if (cycle & 2) {
			cycle += 2;
			var->right_margin += 2;
		}
	}

	vidc.h_cycle		= (cycle - 2) / 2;
	vidc.h_sync_width	= (sync_len - 2) / 2;
	vidc.h_border_start	= (display_start - 1) / 2;
	vidc.h_display_start	= (display_start - horiz_correction) / 2;
	vidc.h_display_end	= (display_end - horiz_correction) / 2;
	vidc.h_border_end	= (display_end - 1) / 2;
	vidc.h_interlace	= (vidc.h_cycle + 1) / 2;

	sync_len	= var->vsync_len;
	display_start	= sync_len + var->upper_margin;
	display_end	= display_start + var->yres;
	cycle		= display_end + var->lower_margin;

	if (is_interlaced)
		cycle = (cycle - 3) / 2;
	else
		cycle = cycle - 1;

	vidc.v_cycle		= cycle;
	vidc.v_sync_width	= sync_len - 1;
	vidc.v_border_start	= display_start - 1;
	vidc.v_display_start	= vidc.v_border_start;
	vidc.v_display_end	= display_end - 1;
	vidc.v_border_end	= vidc.v_display_end;

	if (machine_is_a5k())
		__raw_writeb(vid_ctl, IOEB_VID_CTL);

	if (memcmp(&current_vidc, &vidc, sizeof(vidc))) {
		current_vidc = vidc;

		vidc_writel(0xe0000000 | vidc_ctl);
		vidc_writel(0x80000000 | (vidc.h_cycle << 14));
		vidc_writel(0x84000000 | (vidc.h_sync_width << 14));
		vidc_writel(0x88000000 | (vidc.h_border_start << 14));
		vidc_writel(0x8c000000 | (vidc.h_display_start << 14));
		vidc_writel(0x90000000 | (vidc.h_display_end << 14));
		vidc_writel(0x94000000 | (vidc.h_border_end << 14));
		vidc_writel(0x98000000);
		vidc_writel(0x9c000000 | (vidc.h_interlace << 14));
		vidc_writel(0xa0000000 | (vidc.v_cycle << 14));
		vidc_writel(0xa4000000 | (vidc.v_sync_width << 14));
		vidc_writel(0xa8000000 | (vidc.v_border_start << 14));
		vidc_writel(0xac000000 | (vidc.v_display_start << 14));
		vidc_writel(0xb0000000 | (vidc.v_display_end << 14));
		vidc_writel(0xb4000000 | (vidc.v_border_end << 14));
		vidc_writel(0xb8000000);
		vidc_writel(0xbc000000);
	}
#ifdef DEBUG_MODE_SELECTION
	printk(KERN_DEBUG "VIDC registers for %dx%dx%d:\n", var->xres,
	       var->yres, var->bits_per_pixel);
	printk(KERN_DEBUG " H-cycle          : %d\n", vidc.h_cycle);
	printk(KERN_DEBUG " H-sync-width     : %d\n", vidc.h_sync_width);
	printk(KERN_DEBUG " H-border-start   : %d\n", vidc.h_border_start);
	printk(KERN_DEBUG " H-display-start  : %d\n", vidc.h_display_start);
	printk(KERN_DEBUG " H-display-end    : %d\n", vidc.h_display_end);
	printk(KERN_DEBUG " H-border-end     : %d\n", vidc.h_border_end);
	printk(KERN_DEBUG " H-interlace      : %d\n", vidc.h_interlace);
	printk(KERN_DEBUG " V-cycle          : %d\n", vidc.v_cycle);
	printk(KERN_DEBUG " V-sync-width     : %d\n", vidc.v_sync_width);
	printk(KERN_DEBUG " V-border-start   : %d\n", vidc.v_border_start);
	printk(KERN_DEBUG " V-display-start  : %d\n", vidc.v_display_start);
	printk(KERN_DEBUG " V-display-end    : %d\n", vidc.v_display_end);
	printk(KERN_DEBUG " V-border-end     : %d\n", vidc.v_border_end);
	printk(KERN_DEBUG " VIDC Ctrl (E)    : 0x%08X\n", vidc_ctl);
	printk(KERN_DEBUG " IOEB Ctrl        : 0x%08X\n", vid_ctl);
#endif
}

static inline void
acornfb_palette_write(u_int regno, union palette pal)
{
	vidc_writel(pal.p);
}

static inline union palette
acornfb_palette_encode(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans)
{
	union palette pal;

	pal.p = 0;
	pal.vidc.reg   = regno;
	pal.vidc.red   = red >> 12;
	pal.vidc.green = green >> 12;
	pal.vidc.blue  = blue >> 12;
	return pal;
}

static void
acornfb_palette_decode(u_int regno, u_int *red, u_int *green, u_int *blue,
		       u_int *trans)
{
	*red   = EXTEND4(current_par.palette[regno].vidc.red);
	*green = EXTEND4(current_par.palette[regno].vidc.green);
	*blue  = EXTEND4(current_par.palette[regno].vidc.blue);
	*trans = current_par.palette[regno].vidc.trans ? -1 : 0;
}
#endif

#ifdef HAS_VIDC20
#include <asm/arch/acornfb.h>

#define MAX_SIZE	2*1024*1024

/* VIDC20 has a different set of rules from the VIDC:
 *  hcr  : must be multiple of 4
 *  hswr : must be even
 *  hdsr : must be even
 *  hder : must be even
 *  vcr  : >= 2, (interlace, must be odd)
 *  vswr : >= 1
 *  vdsr : >= 1
 *  vder : >= vdsr
 */
static void
acornfb_set_timing(struct fb_var_screeninfo *var)
{
	struct vidc_timing vidc;
	u_int vcr, fsize;
	u_int ext_ctl, dat_ctl;
	u_int words_per_line;

	memset(&vidc, 0, sizeof(vidc));

	vidc.h_sync_width	= var->hsync_len - 8;
	vidc.h_border_start	= vidc.h_sync_width + var->left_margin + 8 - 12;
	vidc.h_display_start	= vidc.h_border_start + 12 - 18;
	vidc.h_display_end	= vidc.h_display_start + var->xres;
	vidc.h_border_end	= vidc.h_display_end + 18 - 12;
	vidc.h_cycle		= vidc.h_border_end + var->right_margin + 12 - 8;
	vidc.h_interlace	= vidc.h_cycle / 2;
	vidc.v_sync_width	= var->vsync_len - 1;
	vidc.v_border_start	= vidc.v_sync_width + var->upper_margin;
	vidc.v_display_start	= vidc.v_border_start;
	vidc.v_display_end	= vidc.v_display_start + var->yres;
	vidc.v_border_end	= vidc.v_display_end;
	vidc.control		= acornfb_default_control();

	vcr = var->vsync_len + var->upper_margin + var->yres +
	      var->lower_margin;

	if ((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vidc.v_cycle = (vcr - 3) / 2;
		vidc.control |= VIDC20_CTRL_INT;
	} else
		vidc.v_cycle = vcr - 2;

	switch (var->bits_per_pixel) {
	case  1: vidc.control |= VIDC20_CTRL_1BPP;	break;
	case  2: vidc.control |= VIDC20_CTRL_2BPP;	break;
	case  4: vidc.control |= VIDC20_CTRL_4BPP;	break;
	default:
	case  8: vidc.control |= VIDC20_CTRL_8BPP;	break;
	case 16: vidc.control |= VIDC20_CTRL_16BPP;	break;
	case 32: vidc.control |= VIDC20_CTRL_32BPP;	break;
	}

	acornfb_vidc20_find_rates(&vidc, var);
	fsize = var->vsync_len + var->upper_margin + var->lower_margin - 1;

	if (memcmp(&current_vidc, &vidc, sizeof(vidc))) {
		current_vidc = vidc;

		vidc_writel(VIDC20_CTRL| vidc.control);
		vidc_writel(0xd0000000 | vidc.pll_ctl);
		vidc_writel(0x80000000 | vidc.h_cycle);
		vidc_writel(0x81000000 | vidc.h_sync_width);
		vidc_writel(0x82000000 | vidc.h_border_start);
		vidc_writel(0x83000000 | vidc.h_display_start);
		vidc_writel(0x84000000 | vidc.h_display_end);
		vidc_writel(0x85000000 | vidc.h_border_end);
		vidc_writel(0x86000000);
		vidc_writel(0x87000000 | vidc.h_interlace);
		vidc_writel(0x90000000 | vidc.v_cycle);
		vidc_writel(0x91000000 | vidc.v_sync_width);
		vidc_writel(0x92000000 | vidc.v_border_start);
		vidc_writel(0x93000000 | vidc.v_display_start);
		vidc_writel(0x94000000 | vidc.v_display_end);
		vidc_writel(0x95000000 | vidc.v_border_end);
		vidc_writel(0x96000000);
		vidc_writel(0x97000000);
	}

	iomd_writel(fsize, IOMD_FSIZE);

	ext_ctl = acornfb_default_econtrol();

	if (var->sync & FB_SYNC_COMP_HIGH_ACT) /* should be FB_SYNC_COMP */
		ext_ctl |= VIDC20_ECTL_HS_NCSYNC | VIDC20_ECTL_VS_NCSYNC;
	else {
		if (var->sync & FB_SYNC_HOR_HIGH_ACT)
			ext_ctl |= VIDC20_ECTL_HS_HSYNC;
		else
			ext_ctl |= VIDC20_ECTL_HS_NHSYNC;

		if (var->sync & FB_SYNC_VERT_HIGH_ACT)
			ext_ctl |= VIDC20_ECTL_VS_VSYNC;
		else
			ext_ctl |= VIDC20_ECTL_VS_NVSYNC;
	}

	vidc_writel(VIDC20_ECTL | ext_ctl);

	words_per_line = var->xres * var->bits_per_pixel / 32;

	if (current_par.using_vram && current_par.screen_size == 2048*1024)
		words_per_line /= 2;

	/* RiscPC doesn't use the VIDC's VRAM control. */
	dat_ctl = VIDC20_DCTL_VRAM_DIS | VIDC20_DCTL_SNA | words_per_line;

	/* The data bus width is dependent on both the type
	 * and amount of video memory.
	 *     DRAM	32bit low
	 * 1MB VRAM	32bit
	 * 2MB VRAM	64bit
	 */
	if (current_par.using_vram && current_par.vram_half_sam == 2048) {
		dat_ctl |= VIDC20_DCTL_BUS_D63_0;
	} else 
		dat_ctl |= VIDC20_DCTL_BUS_D31_0;

	vidc_writel(VIDC20_DCTL | dat_ctl);

#ifdef DEBUG_MODE_SELECTION
	printk(KERN_DEBUG "VIDC registers for %dx%dx%d:\n", var->xres,
	       var->yres, var->bits_per_pixel);
	printk(KERN_DEBUG " H-cycle          : %d\n", vidc.h_cycle);
	printk(KERN_DEBUG " H-sync-width     : %d\n", vidc.h_sync_width);
	printk(KERN_DEBUG " H-border-start   : %d\n", vidc.h_border_start);
	printk(KERN_DEBUG " H-display-start  : %d\n", vidc.h_display_start);
	printk(KERN_DEBUG " H-display-end    : %d\n", vidc.h_display_end);
	printk(KERN_DEBUG " H-border-end     : %d\n", vidc.h_border_end);
	printk(KERN_DEBUG " H-interlace      : %d\n", vidc.h_interlace);
	printk(KERN_DEBUG " V-cycle          : %d\n", vidc.v_cycle);
	printk(KERN_DEBUG " V-sync-width     : %d\n", vidc.v_sync_width);
	printk(KERN_DEBUG " V-border-start   : %d\n", vidc.v_border_start);
	printk(KERN_DEBUG " V-display-start  : %d\n", vidc.v_display_start);
	printk(KERN_DEBUG " V-display-end    : %d\n", vidc.v_display_end);
	printk(KERN_DEBUG " V-border-end     : %d\n", vidc.v_border_end);
	printk(KERN_DEBUG " Ext Ctrl  (C)    : 0x%08X\n", ext_ctl);
	printk(KERN_DEBUG " PLL Ctrl  (D)    : 0x%08X\n", vidc.pll_ctl);
	printk(KERN_DEBUG " Ctrl      (E)    : 0x%08X\n", vidc.control);
	printk(KERN_DEBUG " Data Ctrl (F)    : 0x%08X\n", dat_ctl);
	printk(KERN_DEBUG " Fsize            : 0x%08X\n", fsize);
#endif
}

static inline void
acornfb_palette_write(u_int regno, union palette pal)
{
	vidc_writel(0x10000000 | regno);
	vidc_writel(pal.p);
}

static inline union palette
acornfb_palette_encode(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans)
{
	union palette pal;

	pal.p = 0;
	pal.vidc20.red   = red >> 8;
	pal.vidc20.green = green >> 8;
	pal.vidc20.blue  = blue >> 8;
	return pal;
}

static void
acornfb_palette_decode(u_int regno, u_int *red, u_int *green, u_int *blue,
		       u_int *trans)
{
	*red   = EXTEND8(current_par.palette[regno].vidc20.red);
	*green = EXTEND8(current_par.palette[regno].vidc20.green);
	*blue  = EXTEND8(current_par.palette[regno].vidc20.blue);
	*trans = EXTEND4(current_par.palette[regno].vidc20.ext);
}
#endif

/*
 * Before selecting the timing parameters, adjust
 * the resolution to fit the rules.
 */
static int
acornfb_adjust_timing(struct fb_var_screeninfo *var, int con)
{
	u_int font_line_len;
	u_int fontht;
	u_int sam_size, min_size, size;
	u_int nr_y;

	/* xres must be even */
	var->xres = (var->xres + 1) & ~1;

	/*
	 * We don't allow xres_virtual to differ from xres
	 */
	var->xres_virtual = var->xres;
	var->xoffset = 0;

	/*
	 * Find the font height
	 */
	if (con == -1)
		fontht = fontheight(&global_disp);
	else
		fontht = fontheight(fb_display + con);

	if (fontht == 0)
		fontht = 8;

	if (current_par.using_vram)
		sam_size = current_par.vram_half_sam * 2;
	else
		sam_size = 16;

	/*
	 * Now, find a value for yres_virtual which allows
	 * us to do ywrap scrolling.  The value of
	 * yres_virtual must be such that the end of the
	 * displayable frame buffer must be aligned with
	 * the start of a font line.
	 */
	font_line_len = var->xres * var->bits_per_pixel * fontht / 8;
	min_size = var->xres * var->yres * var->bits_per_pixel / 8;

	/*
	 * If minimum screen size is greater than that we have
	 * available, reject it.
	 */
	if (min_size > current_par.screen_size)
		return -EINVAL;

	/* Find int 'y', such that y * fll == s * sam < maxsize
	 * y = s * sam / fll; s = maxsize / sam
	 */
	for (size = current_par.screen_size; min_size <= size;
	     size -= sam_size) {
		nr_y = size / font_line_len;

		if (nr_y * font_line_len == size)
			break;
	}

	if (var->accel_flags & FB_ACCELF_TEXT) {
		if (min_size > size) {
			/*
			 * failed, use ypan
			 */
			size = current_par.screen_size;
			var->yres_virtual = size / (font_line_len / fontht);
		} else
			var->yres_virtual = nr_y * fontht;
	}

	current_par.screen_end = current_par.screen_base_p + size;

	/*
	 * Fix yres & yoffset if needed.
	 */
	if (var->yres > var->yres_virtual)
		var->yres = var->yres_virtual;

	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset > var->yres_virtual)
			var->yoffset = var->yres_virtual;
	} else {
		if (var->yoffset + var->yres > var->yres_virtual)
			var->yoffset = var->yres_virtual - var->yres;
	}

	/* hsync_len must be even */
	var->hsync_len = (var->hsync_len + 1) & ~1;

#ifdef HAS_VIDC
	/* left_margin must be odd */
	if ((var->left_margin & 1) == 0) {
		var->left_margin -= 1;
		var->right_margin += 1;
	}

	/* right_margin must be odd */
	var->right_margin |= 1;
#elif defined(HAS_VIDC20)
	/* left_margin must be even */
	if (var->left_margin & 1) {
		var->left_margin += 1;
		var->right_margin -= 1;
	}

	/* right_margin must be even */
	if (var->right_margin & 1)
		var->right_margin += 1;
#endif

	if (var->vsync_len < 1)
		var->vsync_len = 1;

	return 0;
}

static int
acornfb_validate_timing(struct fb_var_screeninfo *var,
			struct fb_monspecs *monspecs)
{
	unsigned long hs, vs;

	/*
	 * hs(Hz) = 10^12 / (pixclock * xtotal)
	 * vs(Hz) = hs(Hz) / ytotal
	 *
	 * No need to do long long divisions or anything
	 * like that if you factor it correctly
	 */
	hs = 1953125000 / var->pixclock;
	hs = hs * 512 /
	     (var->xres + var->left_margin + var->right_margin + var->hsync_len);
	vs = hs /
	     (var->yres + var->upper_margin + var->lower_margin + var->vsync_len);

	return (vs >= monspecs->vfmin && vs <= monspecs->vfmax &&
		hs >= monspecs->hfmin && hs <= monspecs->hfmax) ? 0 : -EINVAL;
}

static inline void
acornfb_update_dma(struct fb_var_screeninfo *var)
{
	int off = (var->yoffset * var->xres_virtual *
		   var->bits_per_pixel) >> 3;

#if defined(HAS_MEMC)
	memc_write(VDMA_INIT, off >> 2);
#elif defined(HAS_IOMD)
	iomd_writel(current_par.screen_base_p + off, IOMD_VIDINIT);
#endif
}

static int
acornfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
		  u_int *trans, struct fb_info *info)
{
	if (regno >= current_par.palette_size)
		return 1;

	acornfb_palette_decode(regno, red, green, blue, trans);

	return 0;
}

/*
 * We have to take note of the VIDC20's 16-bit palette here.
 * The VIDC20 looks up a 16 bit pixel as follows:
 *
 *   bits   111111
 *          5432109876543210
 *   red            ++++++++  (8 bits,  7 to 0)
 *  green       ++++++++      (8 bits, 11 to 4)
 *   blue   ++++++++          (8 bits, 15 to 8)
 *
 * We use a pixel which looks like:
 *
 *   bits   111111
 *          5432109876543210
 *   red               +++++  (5 bits,  4 to  0)
 *  green         +++++       (5 bits,  9 to  5)
 *   blue    +++++            (5 bits, 14 to 10)
 */
static int
acornfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		  u_int trans, struct fb_info *info)
{
	union palette pal;
	int bpp = fb_display[current_par.currcon].var.bits_per_pixel;

	if (regno >= current_par.palette_size)
		return 1;

	pal = acornfb_palette_encode(regno, red, green, blue, trans);
	current_par.palette[regno] = pal;

#ifdef FBCON_HAS_CFB32
	if (bpp == 32 && regno < 16) {
		current_par.cmap.cfb32[regno] =
				regno | regno << 8 | regno << 16;
	}
#endif
#ifdef FBCON_HAS_CFB16
	if (bpp == 16 && regno < 16) {
		int i;

		current_par.cmap.cfb16[regno] =
				regno | regno << 5 | regno << 10;

		pal.p = 0;
		vidc_writel(0x10000000);
		for (i = 0; i < 256; i += 1) {
			pal.vidc20.red   = current_par.palette[ i       & 31].vidc20.red;
			pal.vidc20.green = current_par.palette[(i >> 1) & 31].vidc20.green;
			pal.vidc20.blue  = current_par.palette[(i >> 2) & 31].vidc20.blue;
			vidc_writel(pal.p);
			/* Palette register pointer auto-increments */
		}
	} else
#endif
		acornfb_palette_write(regno, pal);

	return 0;
}

static int
acornfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		 struct fb_info *info)
{
	int err = 0;

	if (con == current_par.currcon)
		err = fb_get_cmap(cmap, kspc, acornfb_getcolreg, info);
	else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(current_par.palette_size),
			     cmap, kspc ? 0 : 2);
	return err;
}

static int
acornfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		 struct fb_info *info)
{
	int err = 0;

	if (!fb_display[con].cmap.len)
		err = fb_alloc_cmap(&fb_display[con].cmap,
				    current_par.palette_size, 0);
	if (!err) {
		if (con == current_par.currcon)
			err = fb_set_cmap(cmap, kspc, acornfb_setcolreg,
					  info);
		else
			fb_copy_cmap(cmap, &fb_display[con].cmap,
				     kspc ? 0 : 1);
	}
	return err;
}

static int
acornfb_decode_var(struct fb_var_screeninfo *var, int con)
{
	int err;

#if defined(HAS_VIDC20)
	var->red.offset    = 0;
	var->red.length    = 8;
	var->green         = var->red;
	var->blue          = var->red;
	var->transp.offset = 0;
	var->transp.length = 4;
#elif defined(HAS_VIDC)
	var->red.length	   = 4;
	var->green         = var->red;
	var->blue          = var->red;
	var->transp.length = 1;
#endif

	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1:
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		var->red.offset    = 0;
		var->red.length    = 5;
		var->green.offset  = 5;
		var->green.length  = 5;
		var->blue.offset   = 10;
		var->blue.length   = 5;
		var->transp.offset = 15;
		var->transp.length = 1;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		var->red.offset    = 0;
		var->red.length    = 8;
		var->green.offset  = 8;
		var->green.length  = 8;
		var->blue.offset   = 16;
		var->blue.length   = 8;
		var->transp.offset = 24;
		var->transp.length = 4;
		break;
#endif
	default:
		return -EINVAL;
	}

	/*
	 * Check to see if the pixel rate is valid.
	 */
	if (!var->pixclock || !acornfb_valid_pixrate(var->pixclock))
		return -EINVAL;

	/*
	 * Validate and adjust the resolution to
	 * match the video generator hardware.
	 */
	err = acornfb_adjust_timing(var, con);
	if (err)
		return err;

	/*
	 * Validate the timing against the
	 * monitor hardware.
	 */
	return acornfb_validate_timing(var, &fb_info.monspecs);
}

static int
acornfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct display *display;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "Acorn");

	if (con >= 0)
		display = fb_display + con;
	else
		display = &global_disp;

	fix->smem_start	 = current_par.screen_base_p;
	fix->smem_len	 = current_par.screen_size;
	fix->type	 = display->type;
	fix->type_aux	 = display->type_aux;
	fix->xpanstep	 = 0;
	fix->ypanstep	 = display->ypanstep;
	fix->ywrapstep	 = display->ywrapstep;
	fix->visual	 = display->visual;
	fix->line_length = display->line_length;
	fix->accel	 = FB_ACCEL_NONE;

	return 0;
}

static int
acornfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	if (con == -1) {
		*var = global_disp.var;
	} else
		*var = fb_display[con].var;

	return 0;
}

static int
acornfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct display *display;
	int err, chgvar = 0;

	if (con >= 0)
		display = fb_display + con;
	else
		display = &global_disp;

	err = acornfb_decode_var(var, con);
	if (err)
		return err;

	switch (var->activate & FB_ACTIVATE_MASK) {
	case FB_ACTIVATE_TEST:
		return 0;

	case FB_ACTIVATE_NXTOPEN:
	case FB_ACTIVATE_NOW:
		break;

	default:
		return -EINVAL;
	}

	if (con >= 0) {
		if (display->var.xres != var->xres)
			chgvar = 1;
		if (display->var.yres != var->yres)
			chgvar = 1;
		if (display->var.xres_virtual != var->xres_virtual)
			chgvar = 1;
		if (display->var.yres_virtual != var->yres_virtual)
			chgvar = 1;
		if (memcmp(&display->var.red, &var->red, sizeof(var->red)))
			chgvar = 1;
		if (memcmp(&display->var.green, &var->green, sizeof(var->green)))
			chgvar = 1;
		if (memcmp(&display->var.blue, &var->blue, sizeof(var->blue)))
			chgvar = 1;
	}

	display->var = *var;
	display->var.activate &= ~FB_ACTIVATE_ALL;

	if (var->activate & FB_ACTIVATE_ALL)
		global_disp.var = display->var;

	switch (display->var.bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1:
		current_par.palette_size = 2;
		display->dispsw = &fbcon_mfb;
		display->visual = FB_VISUAL_MONO10;
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		current_par.palette_size = 4;
		display->dispsw = &fbcon_cfb2;
		display->visual = FB_VISUAL_PSEUDOCOLOR;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		current_par.palette_size = 16;
		display->dispsw = &fbcon_cfb4;
		display->visual = FB_VISUAL_PSEUDOCOLOR;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		current_par.palette_size = VIDC_PALETTE_SIZE;
		display->dispsw = &fbcon_cfb8;
#ifdef HAS_VIDC
		display->visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
#else
		display->visual = FB_VISUAL_PSEUDOCOLOR;
#endif
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		current_par.palette_size = 32;
		display->dispsw = &fbcon_cfb16;
		display->dispsw_data = current_par.cmap.cfb16;
		display->visual = FB_VISUAL_DIRECTCOLOR;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		current_par.palette_size = VIDC_PALETTE_SIZE;
		display->dispsw = &fbcon_cfb32;
		display->dispsw_data = current_par.cmap.cfb32;
		display->visual = FB_VISUAL_TRUECOLOR;
		break;
#endif
	default:
		display->dispsw = &fbcon_dummy;
		break;
	}

	display->screen_base	= (char *)current_par.screen_base;
	display->type		= FB_TYPE_PACKED_PIXELS;
	display->type_aux	= 0;
	display->ypanstep	= 1;
	display->ywrapstep	= 1;
	display->line_length	=
	display->next_line      = (var->xres * var->bits_per_pixel) / 8;
	display->can_soft_blank	= display->visual == FB_VISUAL_PSEUDOCOLOR ? 1 : 0;
	display->inverse	= 0;

	if (chgvar && info && info->changevar)
		info->changevar(con);

	if (con == current_par.currcon) {
		struct fb_cmap *cmap;
		unsigned long start, size;
		int control;

#if defined(HAS_MEMC)
		start   = 0;
		size    = current_par.screen_size - VDMA_XFERSIZE;
		control = 0;

		memc_write(VDMA_START, start);
		memc_write(VDMA_END, size >> 2);
#elif defined(HAS_IOMD)

		start = current_par.screen_base_p;
		size  = current_par.screen_end;

		if (current_par.using_vram) {
			size -= current_par.vram_half_sam;
			control = DMA_CR_E | (current_par.vram_half_sam / 256);
		} else {
			size -= 16;
			control = DMA_CR_E | DMA_CR_D | 16;
		}

		iomd_writel(start,   IOMD_VIDSTART);
		iomd_writel(size,    IOMD_VIDEND);
		iomd_writel(control, IOMD_VIDCR);
#endif
		acornfb_update_dma(var);
		acornfb_set_timing(var);

		if (display->cmap.len)
			cmap = &display->cmap;
		else
			cmap = fb_default_cmap(current_par.palette_size);

		fb_set_cmap(cmap, 1, acornfb_setcolreg, info);
	}
	return 0;
}

static int
acornfb_pan_display(struct fb_var_screeninfo *var, int con,
		    struct fb_info *info)
{
	u_int y_bottom;

	if (var->xoffset)
		return -EINVAL;

	y_bottom = var->yoffset;

	if (!(var->vmode & FB_VMODE_YWRAP))
		y_bottom += var->yres;

	if (y_bottom > fb_display[con].var.yres_virtual)
		return -EINVAL;

	acornfb_update_dma(var);

	fb_display[con].var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		fb_display[con].var.vmode |= FB_VMODE_YWRAP;
	else
		fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;

	return 0;
}

/*
 * Note that we are entered with the kernel locked.
 */
static int
acornfb_mmap(struct fb_info *info, struct file *file, struct vm_area_struct *vma)
{
	unsigned long off, start;
	u32 len;

	off = vma->vm_pgoff << PAGE_SHIFT;

	start = current_par.screen_base_p;
	len = PAGE_ALIGN(start & ~PAGE_MASK) + current_par.screen_size;
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

#ifdef CONFIG_CPU_32
	pgprot_val(vma->vm_page_prot) &= ~L_PTE_CACHEABLE;
#endif

	/*
	 * Don't alter the page protection flags; we want to keep the area
	 * cached for better performance.  This does mean that we may miss
	 * some updates to the screen occasionally, but process switches
	 * should cause the caches and buffers to be flushed often enough.
	 */
	if (io_remap_page_range(vma->vm_start, off,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

static struct fb_ops acornfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	acornfb_get_fix,
	fb_get_var:	acornfb_get_var,
	fb_set_var:	acornfb_set_var,
	fb_get_cmap:	acornfb_get_cmap,
	fb_set_cmap:	acornfb_set_cmap,
	fb_pan_display:	acornfb_pan_display,
	fb_mmap:	acornfb_mmap,
};

static int
acornfb_updatevar(int con, struct fb_info *info)
{
	if (con == current_par.currcon)
		acornfb_update_dma(&fb_display[con].var);

	return 0;
}

static int
acornfb_switch(int con, struct fb_info *info)
{
	struct fb_cmap *cmap;

	if (current_par.currcon >= 0) {
		cmap = &fb_display[current_par.currcon].cmap;

		if (cmap->len)
			fb_get_cmap(cmap, 1, acornfb_getcolreg, info);
	}

	current_par.currcon = con;

	fb_display[con].var.activate = FB_ACTIVATE_NOW;

	acornfb_set_var(&fb_display[con].var, con, info);

	return 0;
}

static void
acornfb_blank(int blank, struct fb_info *info)
{
	union palette p;
	int i, bpp = fb_display[current_par.currcon].var.bits_per_pixel;

#ifdef FBCON_HAS_CFB16
	if (bpp == 16) {
		p.p = 0;

		for (i = 0; i < 256; i++) {
			if (blank)
				p = acornfb_palette_encode(i, 0, 0, 0, 0);
			else {
				p.vidc20.red   = current_par.palette[ i       & 31].vidc20.red;
				p.vidc20.green = current_par.palette[(i >> 1) & 31].vidc20.green;
				p.vidc20.blue  = current_par.palette[(i >> 2) & 31].vidc20.blue;
			}
			acornfb_palette_write(i, current_par.palette[i]);
		}
	} else
#endif
	{
		for (i = 0; i < current_par.palette_size; i++) {
			if (blank)
				p = acornfb_palette_encode(i, 0, 0, 0, 0);
			else
				p = current_par.palette[i];

			acornfb_palette_write(i, p);
		}
	}
}

/*
 * Everything after here is initialisation!!!
 */
static struct fb_videomode modedb[] __initdata = {
	{	/* 320x256 @ 50Hz */
		NULL, 50,  320,  256, 125000,  92,  62,  35, 19,  38, 2,
		FB_SYNC_COMP_HIGH_ACT,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x250 @ 50Hz, 15.6 kHz hsync */
		NULL, 50,  640,  250,  62500, 185, 123,  38, 21,  76, 3,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x256 @ 50Hz, 15.6 kHz hsync */
		NULL, 50,  640,  256,  62500, 185, 123,  35, 18,  76, 3,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x512 @ 50Hz, 26.8 kHz hsync */
		NULL, 50,  640,  512,  41667, 113,  87,  18,  1,  56, 3,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x250 @ 70Hz, 31.5 kHz hsync */
		NULL, 70,  640,  250,  39722,  48,  16, 109, 88,  96, 2,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x256 @ 70Hz, 31.5 kHz hsync */
		NULL, 70,  640,  256,  39722,  48,  16, 106, 85,  96, 2,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x352 @ 70Hz, 31.5 kHz hsync */
		NULL, 70,  640,  352,  39722,  48,  16,  58, 37,  96, 2,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 640x480 @ 60Hz, 31.5 kHz hsync */
		NULL, 60,  640,  480,  39722,  48,  16,  32, 11,  96, 2,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 800x600 @ 56Hz, 35.2 kHz hsync */
		NULL, 56,  800,  600,  27778, 101,  23,  22,  1, 100, 2,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 896x352 @ 60Hz, 21.8 kHz hsync */
		NULL, 60,  896,  352,  41667,  59,  27,   9,  0, 118, 3,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 1024x 768 @ 60Hz, 48.4 kHz hsync */
		NULL, 60, 1024,  768,  15385, 160,  24,  29,  3, 136, 6,
		0,
		FB_VMODE_NONINTERLACED
	}, {	/* 1280x1024 @ 60Hz, 63.8 kHz hsync */
		NULL, 60, 1280, 1024,   9090, 186,  96,  38,  1, 160, 3,
		0,
		FB_VMODE_NONINTERLACED
	}
};

static struct fb_videomode __initdata
acornfb_default_mode = {
	name:		NULL,
	refresh:	60,
	xres:		640,
	yres:		480,
	pixclock:	39722,
	left_margin:	56,
	right_margin:	16,
	upper_margin:	34,
	lower_margin:	9,
	hsync_len:	88,
	vsync_len:	2,
	sync:		0,
	vmode:		FB_VMODE_NONINTERLACED
};

static void __init
acornfb_init_fbinfo(void)
{
	static int first = 1;

	if (!first)
		return;
	first = 0;

	strcpy(fb_info.modename, "Acorn");
	strcpy(fb_info.fontname, "Acorn8x8");

	fb_info.node		   = -1;
	fb_info.fbops		   = &acornfb_ops;
	fb_info.disp		   = &global_disp;
	fb_info.changevar	   = NULL;
	fb_info.switch_con	   = acornfb_switch;
	fb_info.updatevar	   = acornfb_updatevar;
	fb_info.blank		   = acornfb_blank;
	fb_info.flags		   = FBINFO_FLAG_DEFAULT;

	global_disp.dispsw	   = &fbcon_dummy;

	/*
	 * setup initial parameters
	 */
	memset(&init_var, 0, sizeof(init_var));

#if defined(HAS_VIDC20)
	init_var.red.length	   = 8;
	init_var.transp.length	   = 4;
#elif defined(HAS_VIDC)
	init_var.red.length	   = 4;
	init_var.transp.length	   = 1;
#endif
	init_var.green		   = init_var.red;
	init_var.blue		   = init_var.red;
	init_var.nonstd		   = 0;
	init_var.activate	   = FB_ACTIVATE_NOW;
	init_var.height		   = -1;
	init_var.width		   = -1;
	init_var.vmode		   = FB_VMODE_NONINTERLACED;
	init_var.accel_flags	   = FB_ACCELF_TEXT;

	current_par.dram_size	   = 0;
	current_par.montype	   = -1;
	current_par.dpms	   = 0;
}

/*
 * setup acornfb options:
 *
 *  font:fontname
 *	Set fontname
 *
 *  mon:hmin-hmax:vmin-vmax:dpms:width:height
 *	Set monitor parameters:
 *		hmin   = horizontal minimum frequency (Hz)
 *		hmax   = horizontal maximum frequency (Hz)	(optional)
 *		vmin   = vertical minimum frequency (Hz)
 *		vmax   = vertical maximum frequency (Hz)	(optional)
 *		dpms   = DPMS supported?			(optional)
 *		width  = width of picture in mm.		(optional)
 *		height = height of picture in mm.		(optional)
 *
 * montype:type
 *	Set RISC-OS style monitor type:
 *		0 (or tv)	- TV frequency
 *		1 (or multi)	- Multi frequency
 *		2 (or hires)	- Hi-res monochrome
 *		3 (or vga)	- VGA
 *		4 (or svga)	- SVGA
 *		auto, or option missing
 *				- try hardware detect
 *
 * dram:size
 *	Set the amount of DRAM to use for the frame buffer
 *	(even if you have VRAM).
 *	size can optionally be followed by 'M' or 'K' for
 *	MB or KB respectively.
 */
static void __init
acornfb_parse_font(char *opt)
{
	strcpy(fb_info.fontname, opt);
}

static void __init
acornfb_parse_mon(char *opt)
{
	char *p = opt;

	current_par.montype = -2;

	fb_info.monspecs.hfmin = simple_strtoul(p, &p, 0);
	if (*p == '-')
		fb_info.monspecs.hfmax = simple_strtoul(p + 1, &p, 0);
	else
		fb_info.monspecs.hfmax = fb_info.monspecs.hfmin;

	if (*p != ':')
		goto bad;

	fb_info.monspecs.vfmin = simple_strtoul(p + 1, &p, 0);
	if (*p == '-')
		fb_info.monspecs.vfmax = simple_strtoul(p + 1, &p, 0);
	else
		fb_info.monspecs.vfmax = fb_info.monspecs.vfmin;

	if (*p != ':')
		goto check_values;

	fb_info.monspecs.dpms = simple_strtoul(p + 1, &p, 0);

	if (*p != ':')
		goto check_values;

	init_var.width = simple_strtoul(p + 1, &p, 0);

	if (*p != ':')
		goto check_values;

	init_var.height = simple_strtoul(p + 1, NULL, 0);

check_values:
	if (fb_info.monspecs.hfmax < fb_info.monspecs.hfmin ||
	    fb_info.monspecs.vfmax < fb_info.monspecs.vfmin)
		goto bad;
	return;

bad:
	printk(KERN_ERR "Acornfb: bad monitor settings: %s\n", opt);
	current_par.montype = -1;
}

static void __init
acornfb_parse_montype(char *opt)
{
	current_par.montype = -2;

	if (strncmp(opt, "tv", 2) == 0) {
		opt += 2;
		current_par.montype = 0;
	} else if (strncmp(opt, "multi", 5) == 0) {
		opt += 5;
		current_par.montype = 1;
	} else if (strncmp(opt, "hires", 5) == 0) {
		opt += 5;
		current_par.montype = 2;
	} else if (strncmp(opt, "vga", 3) == 0) {
		opt += 3;
		current_par.montype = 3;
	} else if (strncmp(opt, "svga", 4) == 0) {
		opt += 4;
		current_par.montype = 4;
	} else if (strncmp(opt, "auto", 4) == 0) {
		opt += 4;
		current_par.montype = -1;
	} else if (isdigit(*opt))
		current_par.montype = simple_strtoul(opt, &opt, 0);

	if (current_par.montype == -2 ||
	    current_par.montype > NR_MONTYPES) {
		printk(KERN_ERR "acornfb: unknown monitor type: %s\n",
			opt);
		current_par.montype = -1;
	} else
	if (opt && *opt) {
		if (strcmp(opt, ",dpms") == 0)
			current_par.dpms = 1;
		else
			printk(KERN_ERR
			       "acornfb: unknown monitor option: %s\n",
			       opt);
	}
}

static void __init
acornfb_parse_dram(char *opt)
{
	unsigned int size;

	size = simple_strtoul(opt, &opt, 0);

	if (opt) {
		switch (*opt) {
		case 'M':
		case 'm':
			size *= 1024;
		case 'K':
		case 'k':
			size *= 1024;
		default:
			break;
		}
	}

	current_par.dram_size = size;
}

static struct options {
	char *name;
	void (*parse)(char *opt);
} opt_table[] __initdata = {
	{ "font",    acornfb_parse_font    },
	{ "mon",     acornfb_parse_mon     },
	{ "montype", acornfb_parse_montype },
	{ "dram",    acornfb_parse_dram    },
	{ NULL, NULL }
};

int __init
acornfb_setup(char *options)
{
	struct options *optp;
	char *opt;

	if (!options || !*options)
		return 0;

	acornfb_init_fbinfo();

	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;

		for (optp = opt_table; optp->name; optp++) {
			int optlen;

			optlen = strlen(optp->name);

			if (strncmp(opt, optp->name, optlen) == 0 &&
			    opt[optlen] == ':') {
				optp->parse(opt + optlen + 1);
				break;
			}
		}

		if (!optp->name)
			printk(KERN_ERR "acornfb: unknown parameter: %s\n",
			       opt);
	}
	return 0;
}

/*
 * Detect type of monitor connected
 *  For now, we just assume SVGA
 */
static int __init
acornfb_detect_monitortype(void)
{
	return 4;
}

/*
 * This enables the unused memory to be freed on older Acorn machines.
 */
static inline void
free_unused_pages(unsigned int virtual_start, unsigned int virtual_end)
{
	int mb_freed = 0;

	/*
	 * Align addresses
	 */
	virtual_start = PAGE_ALIGN(virtual_start);
	virtual_end = PAGE_ALIGN(virtual_end);

	while (virtual_start < virtual_end) {
		struct page *page;

		/*
		 * Clear page reserved bit,
		 * set count to 1, and free
		 * the page.
		 */
		page = virt_to_page(virtual_start);
		ClearPageReserved(page);
		atomic_set(&page->count, 1);
		free_page(virtual_start);

		virtual_start += PAGE_SIZE;
		mb_freed += PAGE_SIZE / 1024;
	}

	printk("acornfb: freed %dK memory\n", mb_freed);
}

int __init
acornfb_init(void)
{
	unsigned long size;
	u_int h_sync, v_sync;
	int rc, i;

	acornfb_init_fbinfo();

	if (current_par.montype == -1)
		current_par.montype = acornfb_detect_monitortype();

	if (current_par.montype == -1 || current_par.montype > NR_MONTYPES)
		current_par.montype = 4;

	if (current_par.montype >= 0) {
		fb_info.monspecs = monspecs[current_par.montype];
		fb_info.monspecs.dpms = current_par.dpms;
	}

	/*
	 * Try to select a suitable default mode
	 */
	for (i = 0; i < sizeof(modedb) / sizeof(*modedb); i++) {
		unsigned long hs;

		hs = modedb[i].refresh *
		     (modedb[i].yres + modedb[i].upper_margin +
		      modedb[i].lower_margin + modedb[i].vsync_len);
		if (modedb[i].xres == DEFAULT_XRES &&
		    modedb[i].yres == DEFAULT_YRES &&
		    modedb[i].refresh >= fb_info.monspecs.vfmin &&
		    modedb[i].refresh <= fb_info.monspecs.vfmax &&
		    hs                >= fb_info.monspecs.hfmin &&
		    hs                <= fb_info.monspecs.hfmax) {
			acornfb_default_mode = modedb[i];
			break;
		}
	}

	current_par.currcon	   = -1;
	current_par.screen_base	   = SCREEN_BASE;
	current_par.screen_base_p  = SCREEN_START;
	current_par.using_vram     = 0;

	/*
	 * If vram_size is set, we are using VRAM in
	 * a Risc PC.  However, if the user has specified
	 * an amount of DRAM then use that instead.
	 */
	if (vram_size && !current_par.dram_size) {
		size = vram_size;
		current_par.vram_half_sam = vram_size / 1024;
		current_par.using_vram = 1;
	} else if (current_par.dram_size)
		size = current_par.dram_size;
	else
		size = MAX_SIZE;

	/*
	 * Limit maximum screen size.
	 */
	if (size > MAX_SIZE)
		size = MAX_SIZE;

	size = PAGE_ALIGN(size);

#if defined(HAS_VIDC20)
	if (!current_par.using_vram) {
		/*
		 * RiscPC needs to allocate the DRAM memory
		 * for the framebuffer if we are not using
		 * VRAM.  Archimedes/A5000 machines use a
		 * fixed address for their framebuffers.
		 */
		int order = 0;
		unsigned long page, top;
		while (size > (PAGE_SIZE * (1 << order)))
			order++;
		current_par.screen_base = __get_free_pages(GFP_KERNEL, order);
		if (current_par.screen_base == 0) {
			printk(KERN_ERR "acornfb: unable to allocate screen "
			       "memory\n");
			return -ENOMEM;
		}
		top = current_par.screen_base + (PAGE_SIZE * (1 << order));
		/* Mark the framebuffer pages as reserved so mmap will work. */
		for (page = current_par.screen_base; 
		     page < PAGE_ALIGN(current_par.screen_base + size);
		     page += PAGE_SIZE)
			SetPageReserved(virt_to_page(page));
		/* Hand back any excess pages that we allocated. */
		for (page = current_par.screen_base + size; page < top; page += PAGE_SIZE)
			free_page(page);
		current_par.screen_base_p =
			virt_to_phys((void *)current_par.screen_base);
	}
#endif
#if defined(HAS_VIDC)
	/*
	 * Free unused pages
	 */
	free_unused_pages(PAGE_OFFSET + size, PAGE_OFFSET + MAX_SIZE);
#endif
	
	current_par.screen_size	   = size;
	current_par.palette_size   = VIDC_PALETTE_SIZE;

	/*
	 * Lookup the timing for this resolution.  If we can't
	 * find it, then we can't restore it if we change
	 * the resolution, so we disable this feature.
	 */
	do {
		rc = fb_find_mode(&init_var, &fb_info, NULL, modedb,
				 sizeof(modedb) / sizeof(*modedb),
				 &acornfb_default_mode, DEFAULT_BPP);
		/*
		 * If we found an exact match, all ok.
		 */
		if (rc == 1)
			break;

		rc = fb_find_mode(&init_var, &fb_info, NULL, NULL, 0,
				  &acornfb_default_mode, DEFAULT_BPP);
		/*
		 * If we found an exact match, all ok.
		 */
		if (rc == 1)
			break;

		rc = fb_find_mode(&init_var, &fb_info, NULL, modedb,
				 sizeof(modedb) / sizeof(*modedb),
				 &acornfb_default_mode, DEFAULT_BPP);
		if (rc)
			break;

		rc = fb_find_mode(&init_var, &fb_info, NULL, NULL, 0,
				  &acornfb_default_mode, DEFAULT_BPP);
	} while (0);

	/*
	 * If we didn't find an exact match, try the
	 * generic database.
	 */
	if (rc == 0) {
		printk("Acornfb: no valid mode found\n");
		return -EINVAL;
	}

	h_sync = 1953125000 / init_var.pixclock;
	h_sync = h_sync * 512 / (init_var.xres + init_var.left_margin +
		 init_var.right_margin + init_var.hsync_len);
	v_sync = h_sync / (init_var.yres + init_var.upper_margin +
		 init_var.lower_margin + init_var.vsync_len);

	printk(KERN_INFO "Acornfb: %ldkB %cRAM, %s, using %dx%d, "
		"%d.%03dkHz, %dHz\n",
		current_par.screen_size / 1024,
		current_par.using_vram ? 'V' : 'D',
		VIDC_NAME, init_var.xres, init_var.yres,
		h_sync / 1000, h_sync % 1000, v_sync);

	printk(KERN_INFO "Acornfb: Monitor: %d.%03d-%d.%03dkHz, %d-%dHz%s\n",
		fb_info.monspecs.hfmin / 1000, fb_info.monspecs.hfmin % 1000,
		fb_info.monspecs.hfmax / 1000, fb_info.monspecs.hfmax % 1000,
		fb_info.monspecs.vfmin, fb_info.monspecs.vfmax,
		fb_info.monspecs.dpms ? ", DPMS" : "");

	if (acornfb_set_var(&init_var, -1, &fb_info))
		printk(KERN_ERR "Acornfb: unable to set display parameters\n");

	if (register_framebuffer(&fb_info) < 0)
		return -EINVAL;
	return 0;
}

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("VIDC 1/1a/20 framebuffer driver");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
