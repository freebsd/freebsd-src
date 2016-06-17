/* drivers/video/pvr2fb.c
 *
 * Frame buffer and fbcon support for the NEC PowerVR2 found within the Sega
 * Dreamcast.
 *
 * Copyright (c) 2001 M. R. Brown <mrbrown@0xd6.org>
 * Copyright (c) 2001 Paul Mundt  <lethal@chaoticdreams.org>
 *
 * This file is part of the LinuxDC project (linuxdc.sourceforge.net).
 *
 */

/*
 * This driver is mostly based on the excellent amifb and vfb sources.  It uses
 * an odd scheme for converting hardware values to/from framebuffer values, here are
 * some hacked-up formulas:
 *
 *  The Dreamcast has screen offsets from each side of it's four borders and the start
 *  offsets of the display window.  I used these values to calculate 'pseudo' values
 *  (think of them as placeholders) for the fb video mode, so that when it came time
 *  to convert these values back into their hardware values, I could just add mode-
 *  specific offsets to get the correct mode settings:
 *
 *      left_margin = diwstart_h - borderstart_h;
 *      right_margin = borderstop_h - (diwstart_h + xres);
 *      upper_margin = diwstart_v - borderstart_v;
 *      lower_margin = borderstop_v - (diwstart_h + yres);
 *
 *      hsync_len = borderstart_h + (hsync_total - borderstop_h);
 *      vsync_len = borderstart_v + (vsync_total - borderstop_v);
 *
 *  Then, when it's time to convert back to hardware settings, the only constants
 *  are the borderstart_* offsets, all other values are derived from the fb video
 *  mode:
 *  
 *      // PAL
 *      borderstart_h = 116;
 *      borderstart_v = 44;
 *      ...
 *      borderstop_h = borderstart_h + hsync_total - hsync_len;
 *      ...
 *      diwstart_v = borderstart_v - upper_margin;
 *
 *  However, in the current implementation, the borderstart values haven't had
 *  the benefit of being fully researched, so some modes may be broken.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/console.h>

#ifdef CONFIG_SH_DREAMCAST
#include <asm/io.h>
#include <asm/machvec.h>
#include <asm/dc_sysasic.h>
#endif

#ifdef CONFIG_MTRR
  #include <asm/mtrr.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#ifdef CONFIG_FB_PVR2_DEBUG
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/* 2D video registers */
#define DISP_BASE 0xa05f8000

#define DISP_BRDRCOLR (DISP_BASE + 0x40)
#define DISP_DIWMODE (DISP_BASE + 0x44)
#define DISP_DIWADDRL (DISP_BASE + 0x50)
#define DISP_DIWADDRS (DISP_BASE + 0x54)
#define DISP_DIWSIZE (DISP_BASE + 0x5c)
#define DISP_SYNCCONF (DISP_BASE + 0xd0)
#define DISP_BRDRHORZ (DISP_BASE + 0xd4)
#define DISP_SYNCSIZE (DISP_BASE + 0xd8)
#define DISP_BRDRVERT (DISP_BASE + 0xdc)
#define DISP_DIWCONF (DISP_BASE + 0xe8)
#define DISP_DIWHSTRT (DISP_BASE + 0xec)
#define DISP_DIWVSTRT (DISP_BASE + 0xf0)

/* Pixel clocks, one for TV output, doubled for VGA output */
#define TV_CLK 74239
#define VGA_CLK 37119

/* This is for 60Hz - the VTOTAL is doubled for interlaced modes */
#define PAL_HTOTAL 863
#define PAL_VTOTAL 312
#define NTSC_HTOTAL 857
#define NTSC_VTOTAL 262

enum { CT_VGA, CT_NONE, CT_RGB, CT_COMPOSITE };

enum { VO_PAL, VO_NTSC, VO_VGA };

struct pvr2_params { u_short val; char *name; };
static struct pvr2_params cables[] __initdata = {
	{ CT_VGA, "VGA" }, { CT_RGB, "RGB" }, { CT_COMPOSITE, "COMPOSITE" },
};

static struct pvr2_params outputs[] __initdata = {
	{ VO_PAL, "PAL" }, { VO_NTSC, "NTSC" }, { VO_VGA, "VGA" },
};

/*
 * This describes the current video mode
 */

static struct pvr2fb_par {

	int xres;
	int yres;
	int vxres;
	int vyres;
	int xoffset;
	int yoffset;
	u_short bpp;

	u_long pixclock;
	u_short hsync_total;	/* Clocks/line */
	u_short vsync_total;	/* Lines/field */
	u_short borderstart_h;
	u_short borderstop_h;
	u_short borderstart_v;
	u_short borderstop_v;
	u_short diwstart_h;	/* Horizontal offset of the display field */
	u_short diwstart_v;	/* Vertical offset of the display field, for
				   interlaced modes, this is the long field */
	u_long disp_start;	/* Address of image within VRAM */

	u_long next_line;	/* Modulo for next line */

	u_char is_interlaced;	/* Is the display interlaced? */
	u_char is_doublescan;	/* Are scanlines output twice? (doublescan) */
	u_char is_lowres;	/* Is horizontal pixel-doubling enabled? */

	u_long bordercolor;	/* RGB888 format border color */

	u_long vmode;
	
} currentpar;

static int currcon = 0;
static int currbpp;
static struct display disp;
static struct fb_info fb_info;
static int pvr2fb_inverse = 0;

static struct { u_short red, green, blue, alpha; } palette[256];
static union {
#ifdef FBCON_HAS_CFB16
	u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB24
	u32 cfb24[16];
#endif
#ifdef FBCON_HAS_CFB32
	u32 cfb32[16];
#endif
} fbcon_cmap;

static char pvr2fb_name[16] = "NEC PowerVR2";

#define VIDEOMEMSIZE (8*1024*1024)
static u_long videomemory = 0xa5000000, videomemorysize = VIDEOMEMSIZE;
static int cable_type = -1;
static int video_output = -1;

#ifdef CONFIG_MTRR
static int enable_mtrr = 1;
static int mtrr_handle;
#endif

/*
 * We do all updating, blanking, etc. during the vertical retrace period
 */

static u_short do_vmode_full = 0;	/* Change the video mode */
static u_short do_vmode_pan = 0;	/* Update the video mode */
static short do_blank = 0;		/* (Un)Blank the screen */

static u_short is_blanked = 0;		/* Is the screen blanked? */

/* Interface used by the world */

int pvr2fb_setup(char*);

static int pvr2fb_get_fix(struct fb_fix_screeninfo *fix, int con,
                            struct fb_info *info);
static int pvr2fb_get_var(struct fb_var_screeninfo *var, int con,
                            struct fb_info *info);
static int pvr2fb_set_var(struct fb_var_screeninfo *var, int con,
                            struct fb_info *info);
static int pvr2fb_pan_display(struct fb_var_screeninfo *var, int con,
                                struct fb_info *info);
static int pvr2fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
                             struct fb_info *info);
static int pvr2fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
                             struct fb_info *info);

	/*
	 * Interface to the low level console driver
	 */

static int pvr2fbcon_switch(int con, struct fb_info *info);
static int pvr2fbcon_updatevar(int con, struct fb_info *info);
static void pvr2fbcon_blank(int blank, struct fb_info *info);

	/*
	 * Internal/hardware-specific routines
	 */

static void do_install_cmap(int con, struct fb_info *info);
static u_long get_line_length(int xres_virtual, int bpp);
static void set_color_bitfields(struct fb_var_screeninfo *var);
static int pvr2_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                            u_int *transp, struct fb_info *info);
static int pvr2_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                            u_int transp, struct fb_info *info);

static int pvr2_encode_fix(struct fb_fix_screeninfo *fix,
                             struct pvr2fb_par *par);
static int pvr2_decode_var(struct fb_var_screeninfo *var,
                          struct pvr2fb_par *par);
static int pvr2_encode_var(struct fb_var_screeninfo *var,
                          struct pvr2fb_par *par);
static void pvr2_get_par(struct pvr2fb_par *par);
static void pvr2_set_var(struct fb_var_screeninfo *var);
static void pvr2_pan_var(struct fb_var_screeninfo *var);
static int pvr2_update_par(void);
static void pvr2_update_display(void);
static void pvr2_init_display(void);
static void pvr2_do_blank(void);
static void pvr2fb_interrupt(int irq, void *dev_id, struct pt_regs *fp);
static int pvr2_init_cable(void);
static int pvr2_get_param(const struct pvr2_params *p, const char *s,
                            int val, int size);

static struct fb_ops pvr2fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	pvr2fb_get_fix,
	fb_get_var:	pvr2fb_get_var,
	fb_set_var:	pvr2fb_set_var,
	fb_get_cmap:	pvr2fb_get_cmap,
	fb_set_cmap:	pvr2fb_set_cmap,
	fb_pan_display: pvr2fb_pan_display,
};

static struct fb_videomode pvr2_modedb[] __initdata = {

    /*
     * Broadcast video modes (PAL and NTSC).  I'm unfamiliar with
     * PAL-M and PAL-N, but from what I've read both modes parallel PAL and
     * NTSC, so it shouldn't be a problem (I hope).
     */

    {
	/* 640x480 @ 60Hz interlaced (NTSC) */
	"ntsc_640x480i", 60, 640, 480, TV_CLK, 38, 33, 0, 18, 146, 26,
	FB_SYNC_BROADCAST, FB_VMODE_INTERLACED | FB_VMODE_YWRAP
    },

    {
	/* 640x240 @ 60Hz (NTSC) */
	/* XXX: Broken! Don't use... */
	"ntsc_640x240", 60, 640, 240, TV_CLK, 38, 33, 0, 0, 146, 22,
	FB_SYNC_BROADCAST, FB_VMODE_YWRAP
    },

    {
	/* 640x480 @ 60hz (VGA) */
	"vga_640x480", 60, 640, 480, VGA_CLK, 38, 33, 0, 18, 146, 26,
	0, FB_VMODE_YWRAP
    },

};

#define NUM_TOTAL_MODES  ARRAY_SIZE(pvr2_modedb)

#define DEFMODE_NTSC	0
#define DEFMODE_PAL	0
#define DEFMODE_VGA	2

static int defmode = DEFMODE_NTSC;
static char *mode_option __initdata = NULL;

/* Get the fixed part of the display */

static int pvr2fb_get_fix(struct fb_fix_screeninfo *fix, int con,
                            struct fb_info *info)
{
	struct pvr2fb_par par;

	if (con == -1)
		pvr2_get_par(&par);
	else {
		int err;

		if ((err = pvr2_decode_var(&fb_display[con].var, &par)))
			return err;
	}
	return pvr2_encode_fix(fix, &par);
}

/* Get the user-defined part of the display */

static int pvr2fb_get_var(struct fb_var_screeninfo *var, int con,
                            struct fb_info *info)
{
	int err = 0;

	if (con == -1) {
		struct pvr2fb_par par;

		pvr2_get_par(&par);
		err = pvr2_encode_var(var, &par);
	} else
		*var = fb_display[con].var;
	
	return err;
}

/* Set the user-defined part of the display */

static int pvr2fb_set_var(struct fb_var_screeninfo *var, int con,
                            struct fb_info *info)
{
	int err, activate = var->activate;
	int oldxres, oldyres, oldvxres, oldvyres, oldbpp;
	struct pvr2fb_par par;

	struct display *display;
	if (con >= 0)
		display = &fb_display[con];
	else
		display = &disp;        /* used during initialization */

	/*
	 * FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 * as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = display->var.xoffset;
		var->yoffset = display->var.yoffset;
	}
	if ((err = pvr2_decode_var(var, &par)))
		return err;
	pvr2_encode_var(var, &par);

	/* Do memory check and bitfield set here?? */

	if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		oldxres = display->var.xres;
		oldyres = display->var.yres;
		oldvxres = display->var.xres_virtual;
		oldvyres = display->var.yres_virtual;
		oldbpp = display->var.bits_per_pixel;
		display->var = *var;
		if (oldxres != var->xres || oldyres != var->yres ||
		    oldvxres != var->xres_virtual || oldvyres != var->yres_virtual ||
		    oldbpp != var->bits_per_pixel) {
			struct fb_fix_screeninfo fix;

			pvr2_encode_fix(&fix, &par);
			display->screen_base = (char *)fix.smem_start;
			display->scrollmode = SCROLL_YREDRAW;
			display->visual = fix.visual;
			display->type = fix.type;
			display->type_aux = fix.type_aux;
			display->ypanstep = fix.ypanstep;
			display->ywrapstep = fix.ywrapstep;
			display->line_length = fix.line_length;
			display->can_soft_blank = 1;
			display->inverse = pvr2fb_inverse;
			switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
			    case 16:
				display->dispsw = &fbcon_cfb16;
				display->dispsw_data = fbcon_cmap.cfb16;
				break;
#endif
#ifdef FBCON_HAS_CFB24
			    case 24:
				display->dispsw = &fbcon_cfb24;
				display->dispsw_data = fbcon_cmap.cfb24;
				break;
#endif
#ifdef FBCON_HAS_CFB32
			    case 32:
				display->dispsw = &fbcon_cfb32;
				display->dispsw_data = fbcon_cmap.cfb32;
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
			do_install_cmap(con, info);
		}
		if (con == currcon)
			pvr2_set_var(&display->var);
	}

	return 0;
}

/*
 * Pan or wrap the display.
 * This call looks only at xoffset, yoffset and the FB_VMODE_YRAP flag
 */

static int pvr2fb_pan_display(struct fb_var_screeninfo *var, int con,
                                struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset<0 || var->yoffset >=
		    fb_display[con].var.yres_virtual || var->xoffset)
			return -EINVAL;
	 } else {
		if (var->xoffset+fb_display[con].var.xres >
		    fb_display[con].var.xres_virtual ||
		    var->yoffset+fb_display[con].var.yres >
		    fb_display[con].var.yres_virtual)
		    return -EINVAL;
	}
	if (con == currcon)
		pvr2_pan_var(var);
	fb_display[con].var.xoffset = var->xoffset;
	fb_display[con].var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		fb_display[con].var.vmode |= FB_VMODE_YWRAP;
	else
		fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;
			
	return 0;
}

/* Get the colormap */

static int pvr2fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
                             struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, pvr2_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
		             cmap, kspc ? 0 : 2);
	return 0;
}

/* Set the colormap */

static int pvr2fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
	                     struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {        /* no colormap allocated? */
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
		                         1<<fb_display[con].var.bits_per_pixel,
					 0)))
			 return err;
	}
	if (con == currcon)                     /* current console? */
		return fb_set_cmap(cmap, kspc, pvr2_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	return 0;
}

static int pvr2fbcon_switch(int con, struct fb_info *info)
{
	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, pvr2_getcolreg, info);

	currcon = con;
	pvr2_set_var(&fb_display[con].var);
	/* Install new colormap */
	do_install_cmap(con, info);
	return 0;
}

static int pvr2fbcon_updatevar(int con, struct fb_info *info)
{
	pvr2_pan_var(&fb_display[con].var);
	return 0;
}

static void pvr2fbcon_blank(int blank, struct fb_info *info)
{
	do_blank = blank ? blank : -1;
}

/* Setup the colormap */

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, pvr2_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
                            1, pvr2_setcolreg, info);
}

static inline u_long get_line_length(int xres_virtual, int bpp)
{
	return (u_long)((((xres_virtual*bpp)+31)&~31) >> 3);
}

static void set_color_bitfields(struct fb_var_screeninfo *var)
{
	switch (var->bits_per_pixel) {
	    case 16:        /* RGB 565 */
		var->red.offset = 11;    var->red.length = 5;
		var->green.offset = 5;   var->green.length = 6;
		var->blue.offset = 0;    var->blue.length = 5;
		var->transp.offset = 0;  var->transp.length = 0;
		break;
	    case 24:        /* RGB 888 */
		var->red.offset = 16;    var->red.length = 8;
		var->green.offset = 8;   var->green.length = 8;
		var->blue.offset = 0;    var->blue.length = 8;
		var->transp.offset = 0;  var->transp.length = 0;
		break;
	    case 32:        /* ARGB 8888 */
		var->red.offset = 16;    var->red.length = 8;
		var->green.offset = 8;   var->green.length = 8;
		var->blue.offset = 0;    var->blue.length = 8;
		var->transp.offset = 24; var->transp.length = 8;
		break;
	}
}

static int pvr2_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                            u_int *transp, struct fb_info *info)
{
	if (regno > 255)
	    return 1;
	
	*red = palette[regno].red;
	*green = palette[regno].green;
	*blue = palette[regno].blue;
	*transp = 0;
	return 0;
}
	
static int pvr2_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                            u_int transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;

	palette[regno].red = red;
	palette[regno].green = green;
	palette[regno].blue = blue;

	if (regno < 16) {
		switch (currbpp) {
#ifdef FBCON_HAS_CFB16
		    case 16: /* RGB 565 */
			fbcon_cmap.cfb16[regno] = (red & 0xf800) |
			                          ((green & 0xfc00) >> 5) |
						  ((blue & 0xf800) >> 11);
			break;
#endif
#ifdef FBCON_HAS_CFB24
		    case 24: /* RGB 888 */
			red >>= 8; green >>= 8; blue >>= 8;
			fbcon_cmap.cfb24[regno] = (red << 16) | (green << 8) | blue;
			break;
#endif
#ifdef FBCON_HAS_CFB32
		    case 32: /* ARGB 8888 */
			red >>= 8; green >>= 8; blue >>= 8;
			fbcon_cmap.cfb32[regno] = (red << 16) | (green << 8) | blue;
			break;
#endif
		    default:
			DPRINTK("Invalid bit depth %d?!?\n", currbpp);
			return 1;
		}
	}

	return 0;
}


static int pvr2_encode_fix(struct fb_fix_screeninfo *fix,
                             struct pvr2fb_par *par)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, pvr2fb_name);
	fix->smem_start = videomemory;
	fix->smem_len = videomemorysize;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	fix->visual = FB_VISUAL_TRUECOLOR;

	if (par->vmode & FB_VMODE_YWRAP) {
		fix->ywrapstep = 1;
		fix->xpanstep = fix->ypanstep = 0;
	} else {
		fix->ywrapstep = 0;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
	}
	fix->line_length = par->next_line;

	return 0;
}

/*
 * Create a hardware video mode using the framebuffer values.  If a value needs
 * to be clipped or constrained it's done here.  This routine needs a bit more
 * work to make sure we're doing the right tests at the right time.
 */
static int pvr2_decode_var(struct fb_var_screeninfo *var,
                             struct pvr2fb_par *par)
{
	u_long line_length;
	u_short vtotal;

	if (var->pixclock != TV_CLK && var->pixclock != VGA_CLK) {
		DPRINTK("Invalid pixclock value %d\n", var->pixclock);
		return -EINVAL;
	}
	par->pixclock = var->pixclock;
	
	if ((par->xres = var->xres) < 320)
		par->xres = 320;
	if ((par->yres = var->yres) < 240)
		par->yres = 240;
	if ((par->vxres = var->xres_virtual) < par->xres)
		par->vxres = par->xres;
	if ((par->vyres = var->yres_virtual) < par->yres)
		par->vyres = par->yres;

	if ((par->bpp = var->bits_per_pixel) <= 16)
		par->bpp = 16;
	else if ((par->bpp = var->bits_per_pixel) <= 24)
		par->bpp = 24;
	else if ((par->bpp = var->bits_per_pixel) <= 32)
		par->bpp = 32;

	currbpp = par->bpp;

	/*
	 * XXX: It's possible that a user could use a VGA box, change the cable
	 * type in hardware (i.e. switch from VGA<->composite), then change modes
	 * (i.e. switching to another VT).  If that happens we should automagically
	 * change the output format to cope, but currently I don't have a VGA box
	 * to make sure this works properly.
	 */
	cable_type = pvr2_init_cable();
	if (cable_type == CT_VGA && video_output != VO_VGA)
		video_output = VO_VGA;

	par->vmode = var->vmode & FB_VMODE_MASK;
	if (par->vmode & FB_VMODE_INTERLACED && video_output != VO_VGA)
		par->is_interlaced = 1;
	/* 
	 * XXX: Need to be more creative with this (i.e. allow doublecan for
	 * PAL/NTSC output).
	 */
	par->is_doublescan = (par->yres < 480 && video_output == VO_VGA);
	
	par->hsync_total = var->left_margin + var->xres + var->right_margin +
	                   var->hsync_len;
	par->vsync_total = var->upper_margin + var->yres + var->lower_margin +
	                   var->vsync_len;

	if (var->sync & FB_SYNC_BROADCAST) {
		vtotal = par->vsync_total;
		if (par->is_interlaced)
			vtotal /= 2;
		if (vtotal > (PAL_VTOTAL + NTSC_VTOTAL)/2) {
			/* PAL video output */
			/* XXX: Should be using a range here ... ? */
			if (par->hsync_total != PAL_HTOTAL) {
				DPRINTK("invalid hsync total for PAL\n");
				return -EINVAL;
			}
			/* XXX: Check for start values here... */
			/* XXX: Check hardware for PAL-compatibility */
			par->borderstart_h = 116;
			par->borderstart_v = 44;
		} else {
			/* NTSC video output */
			if (par->hsync_total != NTSC_HTOTAL) {
				DPRINTK("invalid hsync total for NTSC\n");
				return -EINVAL;
			}
			par->borderstart_h = 126;
			par->borderstart_v = 18;
		}
	} else {
		/* VGA mode */
		/* XXX: What else needs to be checked? */
		/* 
		 * XXX: We have a little freedom in VGA modes, what ranges should
		 * be here (i.e. hsync/vsync totals, etc.)?
		 */
		par->borderstart_h = 126;
		par->borderstart_v = 40;
	}

	/* Calculate the remainding offsets */
	par->borderstop_h = par->borderstart_h + par->hsync_total -
	                    var->hsync_len;
	par->borderstop_v = par->borderstart_v + par->vsync_total -
	                    var->vsync_len;
	par->diwstart_h = par->borderstart_h + var->left_margin;
	par->diwstart_v = par->borderstart_v + var->upper_margin;
	if (!par->is_interlaced)
		par->borderstop_v /= 2;

	if (par->xres < 640)
		par->is_lowres = 1;

	/* XXX: Needs testing. */
	if (!((par->vmode ^ var->vmode) & FB_VMODE_YWRAP)) {
		par->xoffset = var->xoffset;
		par->yoffset = var->yoffset;
		if (par->vmode & FB_VMODE_YWRAP) {
			if (par->xoffset || par->yoffset < 0 || par->yoffset >=
			    par->vyres)
				par->xoffset = par->yoffset = 0;
		} else {
			if (par->xoffset < 0 || par->xoffset > par->vxres-par->xres ||
			    par->yoffset < 0 || par->yoffset > par->vyres-par->yres)
				par->xoffset = par->yoffset = 0;
		}
	} else
		par->xoffset = par->yoffset = 0;

	/* Check memory sizes */
	line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;
	par->disp_start = videomemory + (get_line_length(par->vxres, par->bpp) *
	                  par->yoffset) * get_line_length(par->xoffset, par->bpp);
	par->next_line = line_length;
	
	return 0;
}

static int pvr2_encode_var(struct fb_var_screeninfo *var,
                             struct pvr2fb_par *par)
{
	memset(var, 0, sizeof(struct fb_var_screeninfo));

	var->xres = par->xres;
	var->yres = par->yres;
	var->xres_virtual = par->vxres;
	var->yres_virtual = par->vyres;
	var->xoffset = par->xoffset;
	var->yoffset = par->yoffset;

	var->bits_per_pixel = par->bpp;
	set_color_bitfields(var);

	var->activate = FB_ACTIVATE_NOW;
	var->height = -1;
	var->width = -1;

	var->pixclock = par->pixclock;

	if (par->is_doublescan)
		var->vmode = FB_VMODE_DOUBLE;

	if (par->is_interlaced)
		var->vmode |= FB_VMODE_INTERLACED;
	else
		var->vmode |= FB_VMODE_NONINTERLACED;

	var->right_margin = par->borderstop_h - (par->diwstart_h + par->xres);
	var->left_margin = par->diwstart_h - par->borderstart_h;
	var->hsync_len = par->borderstart_h + (par->hsync_total - par->borderstop_h);
	var->upper_margin = par->diwstart_v - par->borderstart_v;
	var->lower_margin = par->borderstop_v - (par->diwstart_v + par->yres);
	var->vsync_len = par->borderstart_v + (par->vsync_total - par->borderstop_v);
	if (video_output != VO_VGA)
		var->sync = FB_SYNC_BROADCAST;

	if (par->vmode & FB_VMODE_YWRAP)
		var->vmode |= FB_VMODE_YWRAP;
	
	return 0;
}

static void pvr2_get_par(struct pvr2fb_par *par)
{
	*par = currentpar;
}

/* Setup the new videomode in hardware */

static void pvr2_set_var(struct fb_var_screeninfo *var)
{
	do_vmode_pan = 0;
	do_vmode_full = 0;
	pvr2_decode_var(var, &currentpar);

	do_vmode_full = 1;
}

/* 
 * Pan or wrap the display
 * This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag in `var'.
 */
static void pvr2_pan_var(struct fb_var_screeninfo *var)
{
	struct pvr2fb_par *par = &currentpar;

	par->xoffset = var->xoffset;
	par->yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		par->vmode |= FB_VMODE_YWRAP;
	else
		par->vmode &= ~FB_VMODE_YWRAP;

	do_vmode_pan = 0;
	pvr2_update_par();
	do_vmode_pan = 1;
}

static int pvr2_update_par(void)
{
	struct pvr2fb_par *par = &currentpar;
	u_long move;

	move = get_line_length(par->xoffset, par->bpp);
	if (par->yoffset) {
		par->disp_start += (par->next_line * par->yoffset) + move;
	} else
		par->disp_start += move;

	return 0;
}

static void pvr2_update_display(void)
{
	struct pvr2fb_par *par = &currentpar;

	/* Update the start address of the display image */
	ctrl_outl(par->disp_start, DISP_DIWADDRL);
	ctrl_outl(par->disp_start +
		  get_line_length(par->xoffset + par->xres, par->bpp),
	          DISP_DIWADDRS);
}

/* 
 * Initialize the video mode.  Currently, the 16bpp and 24bpp modes aren't
 * very stable.  It's probably due to the fact that a lot of the 2D video
 * registers are still undocumented.
 */

static void pvr2_init_display(void)
{
	struct pvr2fb_par *par = &currentpar;
	u_short diw_height, diw_width, diw_modulo = 1;
	u_short bytesperpixel = par->bpp / 8;

	/* hsync and vsync totals */
	ctrl_outl((par->vsync_total << 16) | par->hsync_total, DISP_SYNCSIZE);

	/* column height, modulo, row width */
	/* since we're "panning" within vram, we need to offset things based
	 * on the offset from the virtual x start to our real gfx. */
	if (video_output != VO_VGA && par->is_interlaced)
		diw_modulo += par->next_line / 4;
	diw_height = (par->is_interlaced ? par->yres / 2 : par->yres);
	diw_width = get_line_length(par->xres, par->bpp) / 4;
	ctrl_outl((diw_modulo << 20) | (--diw_height << 10) | --diw_width,
	          DISP_DIWSIZE);

	/* display address, long and short fields */
	ctrl_outl(par->disp_start, DISP_DIWADDRL);
	ctrl_outl(par->disp_start +
	          get_line_length(par->xoffset + par->xres, par->bpp),
	          DISP_DIWADDRS);

	/* border horizontal, border vertical, border color */
	ctrl_outl((par->borderstart_h << 16) | par->borderstop_h, DISP_BRDRHORZ);
	ctrl_outl((par->borderstart_v << 16) | par->borderstop_v, DISP_BRDRVERT);
	ctrl_outl(0, DISP_BRDRCOLR);

	/* display window start position */
	ctrl_outl(par->diwstart_h, DISP_DIWHSTRT);
	ctrl_outl((par->diwstart_v << 16) | par->diwstart_v, DISP_DIWVSTRT);
	
	/* misc. settings */
	ctrl_outl((0x16 << 16) | par->is_lowres, DISP_DIWCONF);

	/* clock doubler (for VGA), scan doubler, display enable */
	ctrl_outl(((video_output == VO_VGA) << 23) | 
	          (par->is_doublescan << 1) | 1, DISP_DIWMODE);

	/* bits per pixel */
	ctrl_outl(ctrl_inl(DISP_DIWMODE) | (--bytesperpixel << 2), DISP_DIWMODE);

	/* video enable, color sync, interlace, 
	 * hsync and vsync polarity (currently unused) */
	ctrl_outl(0x100 | ((par->is_interlaced /*|4*/) << 4), DISP_SYNCCONF);

}

/* Simulate blanking by making the border cover the entire screen */

#define BLANK_BIT (1<<3)

static void pvr2_do_blank(void)
{
	u_long diwconf;

	diwconf = ctrl_inl(DISP_DIWCONF);
	if (do_blank > 0)
		ctrl_outl(diwconf | BLANK_BIT, DISP_DIWCONF);
	else
		ctrl_outl(diwconf & ~BLANK_BIT, DISP_DIWCONF);

	is_blanked = do_blank > 0 ? do_blank : 0;
}

static void pvr2fb_interrupt(int irq, void *dev_id, struct pt_regs *fp)
{
	if (do_vmode_pan || do_vmode_full)
		pvr2_update_display();

	if (do_vmode_full)
		pvr2_init_display();

	if (do_vmode_pan)
		do_vmode_pan = 0;

	if (do_blank) {
		pvr2_do_blank();
		do_blank = 0;
	}

	if (do_vmode_full) {
		do_vmode_full = 0;
	}
}

/*
 * Determine the cable type and initialize the cable output format.  Don't do
 * anything if the cable type has been overidden (via "cable:XX").
 */

#define PCTRA 0xff80002c
#define PDTRA 0xff800030
#define VOUTC 0xa0702c00

static int pvr2_init_cable(void)
{
	if (cable_type < 0) {
		ctrl_outl((ctrl_inl(PCTRA) & 0xfff0ffff) | 0x000a0000, 
	                  PCTRA);
		cable_type = (ctrl_inw(PDTRA) >> 8) & 3;
	}

	/* Now select the output format (either composite or other) */
	/* XXX: Save the previous val first, as this reg is also AICA
	  related */
	if (cable_type == CT_COMPOSITE)
		ctrl_outl(3 << 8, VOUTC);
	else
		ctrl_outl(0, VOUTC);

	return cable_type;
}

int __init pvr2fb_init(void)
{
	struct fb_var_screeninfo var;
	u_long modememused;

	if (!MACH_DREAMCAST)
		return -ENXIO;

	/* Make a guess at the monitor based on the attached cable */
	if (pvr2_init_cable() == CT_VGA) {
		fb_info.monspecs.hfmin = 30000;
		fb_info.monspecs.hfmax = 70000;
		fb_info.monspecs.vfmin = 60;
		fb_info.monspecs.vfmax = 60;
	}
	else { /* Not VGA, using a TV (taken from acornfb) */
		fb_info.monspecs.hfmin = 15469;
		fb_info.monspecs.hfmax = 15781;
		fb_info.monspecs.vfmin = 49;
		fb_info.monspecs.vfmax = 51;
	}

	/* XXX: This needs to pull default video output via BIOS or other means */
	if (video_output < 0) {
		if (cable_type == CT_VGA)
			video_output = VO_VGA;
		else
			video_output = VO_NTSC;
	}
	
	strcpy(fb_info.modename, pvr2fb_name);
	fb_info.changevar = NULL;
	fb_info.node = -1;
	fb_info.fbops = &pvr2fb_ops;
	fb_info.disp = &disp;
	fb_info.switch_con = &pvr2fbcon_switch;
	fb_info.updatevar = &pvr2fbcon_updatevar;
	fb_info.blank = &pvr2fbcon_blank;
	fb_info.flags = FBINFO_FLAG_DEFAULT;
	memset(&var, 0, sizeof(var));

	if (video_output == VO_VGA)
		defmode = DEFMODE_VGA;

	if (!fb_find_mode(&var, &fb_info, mode_option, pvr2_modedb,
	                  NUM_TOTAL_MODES, &pvr2_modedb[defmode], 16)) {
		return -EINVAL;
	}

	if (request_irq(HW_EVENT_VSYNC, pvr2fb_interrupt, 0,
	                "pvr2 VBL handler", &currentpar)) {
		DPRINTK("couldn't register VBL int\n");
		return -EBUSY;
	}

#ifdef CONFIG_MTRR
	if (enable_mtrr) {
		mtrr_handle = mtrr_add(videomemory, videomemorysize, MTRR_TYPE_WRCOMB, 1);
		printk("pvr2fb: MTRR turned on\n");
	}
#endif

	pvr2fb_set_var(&var, -1, &fb_info);

	if (register_framebuffer(&fb_info) < 0)
		return -EINVAL;

	modememused = get_line_length(var.xres_virtual, var.bits_per_pixel);
	modememused *= var.yres_virtual;
	printk("fb%d: %s frame buffer device, using %ldk/%ldk of video memory\n",
	       GET_FB_IDX(fb_info.node), fb_info.modename, modememused>>10,
	       videomemorysize>>10);
	printk("fb%d: Mode %dx%d-%d pitch = %ld cable: %s video output: %s\n", 
	       GET_FB_IDX(fb_info.node), var.xres, var.yres, var.bits_per_pixel, 
	       get_line_length(var.xres, var.bits_per_pixel),
	       (char *)pvr2_get_param(cables, NULL, cable_type, 3),
	       (char *)pvr2_get_param(outputs, NULL, video_output, 3));

	return 0;
}

static void __exit pvr2fb_exit(void)
{
#ifdef CONFIG_MTRR
	if (enable_mtrr) {
		mtrr_del(mtrr_handle, videomemory, videomemorysize);
		printk("pvr2fb: MTRR turned off\n");
	}
#endif
	unregister_framebuffer(&fb_info);
}

static int __init pvr2_get_param(const struct pvr2_params *p, const char *s,
                                   int val, int size)
{
	int i;

	for (i = 0 ; i < size ; i++ ) {
		if (s != NULL) {
			if (!strnicmp(p[i].name, s, strlen(s)))
				return p[i].val;
		} else {
			if (p[i].val == val)
				return (int)p[i].name;
		}
	}
	return -1;
}

/*
 * Parse command arguments.  Supported arguments are:
 *    inverse                             Use inverse color maps
 *    nomtrr                              Disable MTRR usage
 *    font:<fontname>                     Specify console font
 *    cable:composite|rgb|vga             Override the video cable type
 *    output:NTSC|PAL|VGA                 Override the video output format
 *
 *    <xres>x<yres>[-<bpp>][@<refresh>]   or,
 *    <name>[-<bpp>][@<refresh>]          Startup using this video mode
 */

#ifndef MODULE
int __init pvr2fb_setup(char *options)
{
	char *this_opt;
	char cable_arg[80];
	char output_arg[80];

	fb_info.fontname[0] = '\0';

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ","))) {
		if (!*this_opt)
			continue;
		if (!strcmp(this_opt, "inverse")) {
			pvr2fb_inverse = 1;
			fb_invert_cmaps();
		} else if (!strncmp(this_opt, "font:", 5))
			strcpy(fb_info.fontname, this_opt + 5);
		else if (!strncmp(this_opt, "cable:", 6))
			strcpy(cable_arg, this_opt + 6);
		else if (!strncmp(this_opt, "output:", 7))
			strcpy(output_arg, this_opt + 7);
#ifdef CONFIG_MTRR
		else if (!strncmp(this_opt, "nomtrr", 6))
			enable_mtrr = 0;
#endif
		else
			mode_option = this_opt;
	}

	if (*cable_arg)
		cable_type = pvr2_get_param(cables, cable_arg, 0, 3);

	if (*output_arg)
		video_output = pvr2_get_param(outputs, output_arg, 0, 3);

	return 0;
}
#endif

#ifdef MODULE
MODULE_LICENSE("GPL");
module_init(pvr2fb_init);
#endif
module_exit(pvr2fb_exit);

