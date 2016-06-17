/*
 *  drivers/video/tx3912fb.c
 *
 *  Copyright (C) 1999 Harald Koerfgen
 *  Copyright (C) 2001 Steven Hill (sjhill@realitydiluted.com)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 *
 *  Framebuffer for LCD controller in TMPR3912/05 and PR31700 processors
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/fb.h>
#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <asm/io.h>
#include <asm/bootinfo.h>
#include <asm/uaccess.h>
#include <asm/tx3912.h>
#include "tx3912fb.h"

/*
 * Frame buffer, palette and console structures
 */
static struct fb_info fb_info;
static struct { u_char red, green, blue, pad; } palette[256];
#ifdef FBCON_HAS_CFB8
static union { u16 cfb8[16]; } fbcon_cmap;
#endif
static struct display global_disp;
static int currcon = 0;

/*
 * Interface used by the world
 */
static int tx3912fb_get_fix(struct fb_fix_screeninfo *fix, int con,
				struct fb_info *info);
static int tx3912fb_get_var(struct fb_var_screeninfo *var, int con,
				struct fb_info *info);
static int tx3912fb_set_var(struct fb_var_screeninfo *var, int con,
				struct fb_info *info);
static int tx3912fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
				struct fb_info *info);
static int tx3912fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
				struct fb_info *info);
static int tx3912fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
				u_long arg, int con, struct fb_info *info);

/*
 * Interface used by console driver
 */
int tx3912fb_init(void);
static int tx3912fbcon_switch(int con, struct fb_info *info);
static int tx3912fbcon_updatevar(int con, struct fb_info *info);
static void tx3912fbcon_blank(int blank, struct fb_info *info);

/*
 * Macros
 */
#define get_line_length(xres_virtual, bpp) \
		(u_long) (((int) xres_virtual * (int) bpp + 7) >> 3)

/*
 * Internal routines
 */
static int tx3912fb_getcolreg(u_int regno, u_int *red, u_int *green,
			u_int *blue, u_int *transp, struct fb_info *info);
static int tx3912fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			u_int transp, struct fb_info *info);
static void tx3912fb_install_cmap(int con, struct fb_info *info);


/*
 * Frame buffer operations structure used by console driver
 */
static struct fb_ops tx3912fb_ops = {
	owner: THIS_MODULE,
	fb_get_fix: tx3912fb_get_fix,
	fb_get_var: tx3912fb_get_var,
	fb_set_var: tx3912fb_set_var,
	fb_get_cmap: tx3912fb_get_cmap,
	fb_set_cmap: tx3912fb_set_cmap,
	fb_ioctl: tx3912fb_ioctl,
};


/*
 *  Get fixed display data
 */
static int tx3912fb_get_fix(struct fb_fix_screeninfo *fix, int con,
				struct fb_info *info)
{
	struct display *display;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, TX3912FB_NAME);

	if (con == -1)
		display = &global_disp;
	else
		display = &fb_display[con];

	fix->smem_start		= tx3912fb_vaddr;
	fix->smem_len		= tx3912fb_size;
	fix->type		= display->type;
	fix->type_aux		= display->type_aux;
	fix->xpanstep		= 0;
	fix->ypanstep		= display->ypanstep;
	fix->ywrapstep		= display->ywrapstep;
	fix->visual		= display->visual;
	fix->line_length	= display->line_length;
	fix->accel		= FB_ACCEL_NONE;

	return 0;
}

/*
 * Get user display data
 */
static int tx3912fb_get_var(struct fb_var_screeninfo *var, int con,
				struct fb_info *info)
{
	if (con == -1)
		*var = tx3912fb_info;
	else
		*var = fb_display[con].var;

	return 0;
}

/*
 *  Set user display data
 */
static int tx3912fb_set_var(struct fb_var_screeninfo *var, int con,
				struct fb_info *info)
{
	int err, activate = var->activate;
	int oldxres, oldyres, oldvxres, oldvyres, oldbpp;
	u_long line_length;
	struct display *display;

	if (con == -1)
		display = &global_disp;
	else
		display = &fb_display[con];

	/*
	 * FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal
	 * as FB_VMODE_SMOOTH_XPAN is only used internally
	 */
	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->xoffset = display->var.xoffset;
		var->yoffset = display->var.yoffset;
		var->vmode |= FB_VMODE_YWRAP;
	}

	/*
	 * Make sure values are in range
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 2)
		var->bits_per_pixel = 2;
	else if (var->bits_per_pixel <= 4)
		var->bits_per_pixel = 4;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else
		return -EINVAL;

	/*
	 * Memory limit
	 */
	line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
	if ((line_length * var->yres_virtual) > tx3912fb_size)
		return -ENOMEM;

	/*
	 * This is only for color and we only support 8-bit color
	 */
	if (var->bits_per_pixel) {
		/* RGB 332 */
		var->red.offset = 5;
		var->red.length = 3;
		var->green.offset = 2;
		var->green.length = 3;
		var->blue.offset = 0;
		var->blue.length = 2;
		var->transp.offset = 0;
		var->transp.length = 0;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	/*
	 * Make changes if necessary
	 */
	if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {

		oldxres = display->var.xres;
		oldyres = display->var.yres;
		oldvxres = display->var.xres_virtual;
		oldvyres = display->var.yres_virtual;
		oldbpp = display->var.bits_per_pixel;
		display->var = *var;

		if (oldxres != var->xres || oldyres != var->yres ||
		    oldvxres != var->xres_virtual ||
		    oldvyres != var->yres_virtual ||
		    oldbpp != var->bits_per_pixel) {

			display->screen_base = (u_char *) tx3912fb_vaddr;

			switch (var->bits_per_pixel) {
			case 1:
				display->visual = FB_VISUAL_MONO10;
				break;
			case 2:
				display->visual = FB_VISUAL_PSEUDOCOLOR;
			case 4:
			case 8:
				display->visual = FB_VISUAL_TRUECOLOR;
				break;
			}

			display->type = FB_TYPE_PACKED_PIXELS;
			display->type_aux = 0;
			display->ypanstep = 0;
			display->ywrapstep = 0;
			display->next_line =
			display->line_length =
				get_line_length(var->xres_virtual,
					var->bits_per_pixel);
			display->can_soft_blank = 0;
			display->inverse = FB_IS_INVERSE;

			switch (var->bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
			case 1:
				display->dispsw = &fbcon_mfb;
				break;
#endif
#ifdef CONFIG_FBCON_CFB2
			case 2:
				display->dispsw = &fbcon_cfb2;
				break;
#endif
#ifdef CONFIG_FBCON_CFB4
			case 4:
				display->dispsw = &fbcon_cfb4;
				break;
#endif
#ifdef CONFIG_FBCON_CFB8
			case 8:
				display->dispsw = &fbcon_cfb8;
				display->dispsw_data = fbcon_cmap.cfb8;
				break;
#endif
			default:
				display->dispsw = &fbcon_dummy;
				break;
	    		}

			if (fb_info.changevar)
				(*fb_info.changevar)(con);
		}

		if (oldbpp != var->bits_per_pixel) {
			if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
				return err;
			tx3912fb_install_cmap(con, info);
		}
	}

	return 0;
}

/*
 *  Get the colormap
 */
static int tx3912fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	if (con == currcon)
		return fb_get_cmap(cmap, kspc, tx3912fb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), cmap, kspc ? 0 : 2);

	return 0;
}

/*
 *  Set the Colormap
 */
static int tx3912fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len)
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				1<<fb_display[con].var.bits_per_pixel, 0)))
			return err;

	if (con == currcon)
		return fb_set_cmap(cmap, kspc, tx3912fb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	return 0;
}

/*
 *  Framebuffer ioctl
 */
static int tx3912fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
				u_long arg, int con, struct fb_info *info)
{
	return -EINVAL;
}

/*
 * Initialization of the framebuffer
 */
int __init tx3912fb_init(void)
{
	/* Disable the video logic */
	outl(inl(TX3912_VIDEO_CTRL1) &
		~(TX3912_VIDEO_CTRL1_ENVID | TX3912_VIDEO_CTRL1_DISPON),
		TX3912_VIDEO_CTRL1);
	udelay(200);

	/* Set start address for DMA transfer */
	outl(tx3912fb_paddr, TX3912_VIDEO_CTRL3);

	/* Set end address for DMA transfer */
	outl((tx3912fb_paddr + tx3912fb_size + 1), TX3912_VIDEO_CTRL4);

	/* Set the pixel depth */
	switch (tx3912fb_info.bits_per_pixel) {
	case 1:
		/* Monochrome */
		outl(inl(TX3912_VIDEO_CTRL1) & ~TX3912_VIDEO_CTRL1_BITSEL_MASK,
			TX3912_VIDEO_CTRL1);
		break;
	case 4:
		/* 4-bit gray */
		outl(inl(TX3912_VIDEO_CTRL1) & ~TX3912_VIDEO_CTRL1_BITSEL_MASK,
			TX3912_VIDEO_CTRL1);
		outl(inl(TX3912_VIDEO_CTRL1) |
			TX3912_VIDEO_CTRL1_BITSEL_4BIT_GRAY,
			TX3912_VIDEO_CTRL1);
		break;
	case 8:
		/* 8-bit color */
		outl(inl(TX3912_VIDEO_CTRL1) & ~TX3912_VIDEO_CTRL1_BITSEL_MASK,
			TX3912_VIDEO_CTRL1);
		outl(inl(TX3912_VIDEO_CTRL1) |
			TX3912_VIDEO_CTRL1_BITSEL_8BIT_COLOR,
			TX3912_VIDEO_CTRL1);
		break;
	case 2:
	default:
		/* 2-bit gray */
		outl(inl(TX3912_VIDEO_CTRL1) & ~TX3912_VIDEO_CTRL1_BITSEL_MASK,
			TX3912_VIDEO_CTRL1);
		outl(inl(TX3912_VIDEO_CTRL1) |
			TX3912_VIDEO_CTRL1_BITSEL_2BIT_GRAY,
			TX3912_VIDEO_CTRL1);
		break;
	}

	/* Enable the video clock */
	outl(inl(TX3912_CLK_CTRL) | TX3912_CLK_CTRL_ENVIDCLK,
		TX3912_CLK_CTRL);

	/* Unfreeze video logic and enable DF toggle */
	outl(inl(TX3912_VIDEO_CTRL1) &
		~(TX3912_VIDEO_CTRL1_ENFREEZEFRAME | TX3912_VIDEO_CTRL1_DFMODE),
		TX3912_VIDEO_CTRL1);
	udelay(200);

	/* Clear the framebuffer */
	memset((void *) tx3912fb_vaddr, 0xff, tx3912fb_size);
	udelay(200);

	/* Enable the video logic */
	outl(inl(TX3912_VIDEO_CTRL1) |
		(TX3912_VIDEO_CTRL1_ENVID | TX3912_VIDEO_CTRL1_DISPON),
		TX3912_VIDEO_CTRL1);

	strcpy(fb_info.modename, TX3912FB_NAME);
	fb_info.changevar = NULL;
	fb_info.node = -1;
	fb_info.fbops = &tx3912fb_ops;
	fb_info.disp = &global_disp;
	fb_info.switch_con = &tx3912fbcon_switch;
	fb_info.updatevar = &tx3912fbcon_updatevar;
	fb_info.blank = &tx3912fbcon_blank;
	fb_info.flags = FBINFO_FLAG_DEFAULT;

	tx3912fb_set_var(&tx3912fb_info, -1, &fb_info);

	if (register_framebuffer(&fb_info) < 0)
		return -1;

	printk (KERN_INFO "fb%d: TX3912 frame buffer using %uKB.\n",
		GET_FB_IDX(fb_info.node), (u_int) (tx3912fb_size >> 10));

	return 0;
}

/*
 * Switch the console to be the framebuffer
 */
static int tx3912fbcon_switch(int con, struct fb_info *info)
{
	/* Save off the color map if needed */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1,
			tx3912fb_getcolreg, info);

	/* Make the switch */
	currcon = con;

	/* Install new colormap */
	tx3912fb_install_cmap(con, info);

	return 0;
}

/*
 * Update variable structure
 */
static int tx3912fbcon_updatevar(int con, struct fb_info *info)
{
	/* Nothing */
	return 0;
}

/*
 * Blank the display
 */
static void tx3912fbcon_blank(int blank, struct fb_info *info)
{
	/* FIXME */
	printk("tx3912fbcon_blank\n");
}

/*
 * Read a single color register
 */
static int tx3912fb_getcolreg(u_int regno, u_int *red, u_int *green,
			u_int *blue, u_int *transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;

#if FB_IS_GREY
	{
		u_int grey;
            
		grey = regno * 255 / 15;

#if FB_IS_INVERSE
		grey ^= 255;
#endif
		grey |= grey << 8;
		*red = grey;
		*green = grey;
		*blue = grey;
	}
#else
	*red = (palette[regno].red<<8) | palette[regno].red;
	*green = (palette[regno].green<<8) | palette[regno].green;
	*blue = (palette[regno].blue<<8) | palette[regno].blue;
#endif
	*transp = 0;

	return 0;
}

/*
 * Set a single color register
 */
static int tx3912fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
				u_int transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;

#ifdef FBCON_HAS_CFB8
	if( regno < 16 )
		fbcon_cmap.cfb8[regno] = ((red & 0xe000) >> 8)
					| ((green & 0xe000) >> 11)
					| ((blue & 0xc000) >> 14);
#endif 

	red >>= 8;
	green >>= 8;
	blue >>= 8;
	palette[regno].red = red;
	palette[regno].green = green;
	palette[regno].blue = blue;

	return 0;
}

/*
 * Install the color map
 */
static void tx3912fb_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;

	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, tx3912fb_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(1 << fb_display[con].var.bits_per_pixel), 1, tx3912fb_setcolreg, info);
}

MODULE_LICENSE("GPL");
