/*
 * linux/drivers/video/vga16.c -- VGA 16-color framebuffer driver
 * 
 * Copyright 1999 Ben Pfaff <pfaffben@debian.org> and Petr Vandrovec <VANDROVE@vc.cvut.cz>
 * Based on VGA info at http://www.goodnet.com/~tinara/FreeVGA/home.htm
 * Based on VESA framebuffer (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.  */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>

#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-vga-planes.h>
#include "vga.h"

#define dac_reg	(0x3c8)
#define dac_val	(0x3c9)

#define VGA_FB_PHYS 0xA0000
#define VGA_FB_PHYS_LEN 65536

/* --------------------------------------------------------------------- */

/*
 * card parameters
 */

static struct vga16fb_info {
	struct fb_info  fb_info;
	char *video_vbase;			/* 0xa0000 map address */
	int isVGA;
	
	/* structure holding original VGA register settings when the
           screen is blanked */
	struct {
		unsigned char	SeqCtrlIndex;		/* Sequencer Index reg.   */
		unsigned char	CrtCtrlIndex;		/* CRT-Contr. Index reg.  */
		unsigned char	CrtMiscIO;		/* Miscellaneous register */
		unsigned char	HorizontalTotal;	/* CRT-Controller:00h */
		unsigned char	HorizDisplayEnd;	/* CRT-Controller:01h */
		unsigned char	StartHorizRetrace;	/* CRT-Controller:04h */
		unsigned char	EndHorizRetrace;	/* CRT-Controller:05h */
		unsigned char	Overflow;		/* CRT-Controller:07h */
		unsigned char	StartVertRetrace;	/* CRT-Controller:10h */
		unsigned char	EndVertRetrace;		/* CRT-Controller:11h */
		unsigned char	ModeControl;		/* CRT-Controller:17h */
		unsigned char	ClockingMode;		/* Seq-Controller:01h */
	} vga_state;

	int palette_blanked;
	int vesa_blanked;
} vga16fb;


struct vga16fb_par {
	u8 crtc[VGA_CRT_C];
	u8 atc[VGA_ATT_C];
	u8 gdc[VGA_GFX_C];
	u8 seq[VGA_SEQ_C];
	u8 misc;
	u8 vss;
	struct fb_var_screeninfo var;
};

/* --------------------------------------------------------------------- */

static struct fb_var_screeninfo vga16fb_defined = {
	640,480,640,480,/* W,H, W, H (virtual) load xres,xres_virtual*/
	0,0,		/* virtual -> visible no offset */
	4,		/* depth -> load bits_per_pixel */
	0,		/* greyscale ? */
	{0,0,0},	/* R */
	{0,0,0},	/* G */
	{0,0,0},	/* B */
	{0,0,0},	/* transparency */
	0,		/* standard pixel format */
	FB_ACTIVATE_NOW,
	-1,-1,
	0,
	39721, 48, 16, 39, 8,
	96, 2, 0,	/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0,0,0,0,0,0}
};

static struct display disp;
static struct { u_short blue, green, red, pad; } palette[256];

static int             currcon   = 0;

/* --------------------------------------------------------------------- */

static void vga16fb_pan_var(struct fb_info *info, struct fb_var_screeninfo *var)
{
	u32 pos = (var->xres_virtual * var->yoffset + var->xoffset) >> 3;
	outb(VGA_CRTC_START_HI, VGA_CRT_IC);
	outb(pos >> 8, VGA_CRT_DC);
	outb(VGA_CRTC_START_LO, VGA_CRT_IC);
	outb(pos & 0xFF, VGA_CRT_DC);
#if 0
	/* if someone supports xoffset in bit resolution */
	inb(VGA_IS1_RC);		/* reset flip-flop */
	outb(VGA_ATC_PEL, VGA_ATT_IW);
	outb(xoffset & 7, VGA_ATT_IW);
	inb(VGA_IS1_RC);
	outb(0x20, VGA_ATT_IW);
#endif
}

static int vga16fb_update_var(int con, struct fb_info *info)
{
	vga16fb_pan_var(info, &fb_display[con].var);
	return 0;
}

static int vga16fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info)
{
	struct display *p;

	if (con < 0)
		p = &disp;
	else
		p = fb_display + con;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id,"VGA16 VGA");

	fix->smem_start = VGA_FB_PHYS;
	fix->smem_len = VGA_FB_PHYS_LEN;
	fix->type = FB_TYPE_VGA_PLANES;
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	fix->xpanstep  = 8;
	fix->ypanstep  = 1;
	fix->ywrapstep = 0;
	fix->line_length = p->var.xres_virtual / 8;
	return 0;
}

static int vga16fb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	if(con==-1)
		memcpy(var, &vga16fb_defined, sizeof(struct fb_var_screeninfo));
	else
		*var=fb_display[con].var;
	return 0;
}

static void vga16fb_set_disp(int con, struct vga16fb_info *info)
{
	struct fb_fix_screeninfo fix;
	struct display *display;

	if (con < 0)
		display = &disp;
	else
		display = fb_display + con;

	
	vga16fb_get_fix(&fix, con, &info->fb_info);

	display->screen_base = info->video_vbase;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 1;
	display->inverse = 0;

	if (info->isVGA)
		display->dispsw = &fbcon_vga_planes;
	else
		display->dispsw = &fbcon_ega_planes;
	display->scrollmode = SCROLL_YREDRAW;
}

static void vga16fb_encode_var(struct fb_var_screeninfo *var,
			       const struct vga16fb_par *par,
			       const struct vga16fb_info *info)
{
	*var = par->var;
}

static void vga16fb_clock_chip(struct vga16fb_par *par,
			       unsigned int pixclock,
			       const struct vga16fb_info *info)
{
	static struct {
		u32 pixclock;
		u8  misc;
		u8  seq_clock_mode;
	} *ptr, *best, vgaclocks[] = {
		{ 79442 /* 12.587 */, 0x00, 0x08},
		{ 70616 /* 14.161 */, 0x04, 0x08},
		{ 39721 /* 25.175 */, 0x00, 0x00},
		{ 35308 /* 28.322 */, 0x04, 0x00},
		{     0 /* bad */,    0x00, 0x00}};
	int err;

	best = vgaclocks;
	err = pixclock - best->pixclock;
	if (err < 0) err = -err;
	for (ptr = vgaclocks + 1; ptr->pixclock; ptr++) {
		int tmp;

		tmp = pixclock - ptr->pixclock;
		if (tmp < 0) tmp = -tmp;
		if (tmp < err) {
			err = tmp;
			best = ptr;
		}
	}
	par->misc |= best->misc;
	par->seq[VGA_SEQ_CLOCK_MODE] |= best->seq_clock_mode;
	par->var.pixclock = best->pixclock;		
}
			       
#define FAIL(X) return -EINVAL

static int vga16fb_decode_var(const struct fb_var_screeninfo *var,
			      struct vga16fb_par *par,
			      const struct vga16fb_info *info)
{
	u32 xres, right, hslen, left, xtotal;
	u32 yres, lower, vslen, upper, ytotal;
	u32 vxres, xoffset, vyres, yoffset;
	u32 pos;
	u8 r7, rMode;
	int i;

	if (var->bits_per_pixel != 4)
		return -EINVAL;
	xres = (var->xres + 7) & ~7;
	vxres = (var->xres_virtual + 0xF) & ~0xF;
	xoffset = (var->xoffset + 7) & ~7;
	left = (var->left_margin + 7) & ~7;
	right = (var->right_margin + 7) & ~7;
	hslen = (var->hsync_len + 7) & ~7;

	if (vxres < xres)
		vxres = xres;
	if (xres + xoffset > vxres)
		xoffset = vxres - xres;

	par->var.xres = xres;
	par->var.right_margin = right;
	par->var.hsync_len = hslen;
	par->var.left_margin = left;
	par->var.xres_virtual = vxres;
	par->var.xoffset = xoffset;

	xres >>= 3;
	right >>= 3;
	hslen >>= 3;
	left >>= 3;
	vxres >>= 3;
	xtotal = xres + right + hslen + left;
	if (xtotal >= 256)
		FAIL("xtotal too big");
	if (hslen > 32)
		FAIL("hslen too big");
	if (right + hslen + left > 64)
		FAIL("hblank too big");
	par->crtc[VGA_CRTC_H_TOTAL] = xtotal - 5;
	par->crtc[VGA_CRTC_H_BLANK_START] = xres - 1;
	par->crtc[VGA_CRTC_H_DISP] = xres - 1;
	pos = xres + right;
	par->crtc[VGA_CRTC_H_SYNC_START] = pos;
	pos += hslen;
	par->crtc[VGA_CRTC_H_SYNC_END] = pos & 0x1F;
	pos += left - 2; /* blank_end + 2 <= total + 5 */
	par->crtc[VGA_CRTC_H_BLANK_END] = (pos & 0x1F) | 0x80;
	if (pos & 0x20)
		par->crtc[VGA_CRTC_H_SYNC_END] |= 0x80;

	yres = var->yres;
	lower = var->lower_margin;
	vslen = var->vsync_len;
	upper = var->upper_margin;
	vyres = var->yres_virtual;
	yoffset = var->yoffset;

	if (yres > vyres)
		vyres = yres;
	if (vxres * vyres > 65536) {
		vyres = 65536 / vxres;
		if (vyres < yres)
			return -ENOMEM;
	}
	if (yoffset + yres > vyres)
		yoffset = vyres - yres;
	par->var.yres = yres;
	par->var.lower_margin = lower;
	par->var.vsync_len = vslen;
	par->var.upper_margin = upper;
	par->var.yres_virtual = vyres;
	par->var.yoffset = yoffset;

	if (var->vmode & FB_VMODE_DOUBLE) {
		yres <<= 1;
		lower <<= 1;
		vslen <<= 1;
		upper <<= 1;
	}
	ytotal = yres + lower + vslen + upper;
	if (ytotal > 1024) {
		ytotal >>= 1;
		yres >>= 1;
		lower >>= 1;
		vslen >>= 1;
		upper >>= 1;
		rMode = 0x04;
	} else
		rMode = 0x00;
	if (ytotal > 1024)
		FAIL("ytotal too big");
	if (vslen > 16)
		FAIL("vslen too big");
	par->crtc[VGA_CRTC_V_TOTAL] = ytotal - 2;
	r7 = 0x10;	/* disable linecompare */
	if (ytotal & 0x100) r7 |= 0x01;
	if (ytotal & 0x200) r7 |= 0x20;
	par->crtc[VGA_CRTC_PRESET_ROW] = 0;
	par->crtc[VGA_CRTC_MAX_SCAN] = 0x40;	/* 1 scanline, no linecmp */
	par->var.vmode = var->vmode;
	if (var->vmode & FB_VMODE_DOUBLE)
		par->crtc[VGA_CRTC_MAX_SCAN] |= 0x80;
	par->crtc[VGA_CRTC_CURSOR_START] = 0x20;
	par->crtc[VGA_CRTC_CURSOR_END]   = 0x00;
	pos = yoffset * vxres + (xoffset >> 3);
	par->crtc[VGA_CRTC_START_HI]     = pos >> 8;
	par->crtc[VGA_CRTC_START_LO]     = pos & 0xFF;
	par->crtc[VGA_CRTC_CURSOR_HI]    = 0x00;
	par->crtc[VGA_CRTC_CURSOR_LO]    = 0x00;
	pos = yres - 1;
	par->crtc[VGA_CRTC_V_DISP_END] = pos & 0xFF;
	par->crtc[VGA_CRTC_V_BLANK_START] = pos & 0xFF;
	if (pos & 0x100)
		r7 |= 0x0A;	/* 0x02 -> DISP_END, 0x08 -> BLANK_START */
	if (pos & 0x200) {
		r7 |= 0x40;	/* 0x40 -> DISP_END */
		par->crtc[VGA_CRTC_MAX_SCAN] |= 0x20; /* BLANK_START */
	}
	pos += lower;
	par->crtc[VGA_CRTC_V_SYNC_START] = pos & 0xFF;
	if (pos & 0x100)
		r7 |= 0x04;
	if (pos & 0x200)
		r7 |= 0x80;
	pos += vslen;
	par->crtc[VGA_CRTC_V_SYNC_END] = (pos & 0x0F) & ~0x10; /* disabled IRQ */
	pos += upper - 1; /* blank_end + 1 <= ytotal + 2 */
	par->crtc[VGA_CRTC_V_BLANK_END] = pos & 0xFF; /* 0x7F for original VGA,
                     but some SVGA chips requires all 8 bits to set */
	if (vxres >= 512)
		FAIL("vxres too long");
	par->crtc[VGA_CRTC_OFFSET] = vxres >> 1;
	par->crtc[VGA_CRTC_UNDERLINE] = 0x1F;
	par->crtc[VGA_CRTC_MODE] = rMode | 0xE3;
	par->crtc[VGA_CRTC_LINE_COMPARE] = 0xFF;
	par->crtc[VGA_CRTC_OVERFLOW] = r7;

	par->vss = 0x00;	/* 3DA */

	for (i = 0x00; i < 0x10; i++)
		par->atc[i] = i;
	par->atc[VGA_ATC_MODE] = 0x81;
	par->atc[VGA_ATC_OVERSCAN] = 0x00;	/* 0 for EGA, 0xFF for VGA */
	par->atc[VGA_ATC_PLANE_ENABLE] = 0x0F;
	par->atc[VGA_ATC_PEL] = xoffset & 7;
	par->atc[VGA_ATC_COLOR_PAGE] = 0x00;
	
	par->misc = 0xC3;	/* enable CPU, ports 0x3Dx, positive sync */
	par->var.sync = var->sync;
	if (var->sync & FB_SYNC_HOR_HIGH_ACT)
		par->misc &= ~0x40;
	if (var->sync & FB_SYNC_VERT_HIGH_ACT)
		par->misc &= ~0x80;
	
	par->seq[VGA_SEQ_CLOCK_MODE] = 0x01;
	par->seq[VGA_SEQ_PLANE_WRITE] = 0x0F;
	par->seq[VGA_SEQ_CHARACTER_MAP] = 0x00;
	par->seq[VGA_SEQ_MEMORY_MODE] = 0x06;
	
	par->gdc[VGA_GFX_SR_VALUE] = 0x00;
	par->gdc[VGA_GFX_SR_ENABLE] = 0x0F;
	par->gdc[VGA_GFX_COMPARE_VALUE] = 0x00;
	par->gdc[VGA_GFX_DATA_ROTATE] = 0x20;
	par->gdc[VGA_GFX_PLANE_READ] = 0;
	par->gdc[VGA_GFX_MODE] = 0x00;
	par->gdc[VGA_GFX_MISC] = 0x05;
	par->gdc[VGA_GFX_COMPARE_MASK] = 0x0F;
	par->gdc[VGA_GFX_BIT_MASK] = 0xFF;

	vga16fb_clock_chip(par, var->pixclock, info);

	par->var.bits_per_pixel = 4;
	par->var.grayscale = var->grayscale;
	par->var.red.offset = par->var.green.offset = par->var.blue.offset = 
	par->var.transp.offset = 0;
	par->var.red.length = par->var.green.length = par->var.blue.length =
		(info->isVGA) ? 6 : 2;
	par->var.transp.length = 0;
	par->var.nonstd = 0;
	par->var.activate = FB_ACTIVATE_NOW;
	par->var.height = -1;
	par->var.width = -1;
	par->var.accel_flags = 0;
	
	return 0;
}
#undef FAIL

static int vga16fb_set_par(const struct vga16fb_par *par,
			   struct vga16fb_info *info)
{
	int i;

	outb(inb(VGA_MIS_R) | 0x01, VGA_MIS_W);

	/* Enable graphics register modification */
	if (!info->isVGA) {
		outb(0x00, EGA_GFX_E0);
		outb(0x01, EGA_GFX_E1);
	}
	
	/* update misc output register */
	outb(par->misc, VGA_MIS_W);
	
	/* synchronous reset on */
	outb(0x00, VGA_SEQ_I);
	outb(0x01, VGA_SEQ_D);
	
	/* write sequencer registers */
	outb(1, VGA_SEQ_I);
	outb(par->seq[1] | 0x20, VGA_SEQ_D);
	for (i = 2; i < VGA_SEQ_C; i++) {
		outb(i, VGA_SEQ_I);
		outb(par->seq[i], VGA_SEQ_D);
	}
	
	/* synchronous reset off */
	outb(0x00, VGA_SEQ_I);
	outb(0x03, VGA_SEQ_D);
	
	/* deprotect CRT registers 0-7 */
	outb(0x11, VGA_CRT_IC);
	outb(par->crtc[0x11], VGA_CRT_DC);

	/* write CRT registers */
	for (i = 0; i < VGA_CRT_C; i++) {
		outb(i, VGA_CRT_IC);
		outb(par->crtc[i], VGA_CRT_DC);
	}
	
	/* write graphics controller registers */
	for (i = 0; i < VGA_GFX_C; i++) {
		outb(i, VGA_GFX_I);
		outb(par->gdc[i], VGA_GFX_D);
	}
	
	/* write attribute controller registers */
	for (i = 0; i < VGA_ATT_C; i++) {
		inb_p(VGA_IS1_RC);		/* reset flip-flop */
		outb_p(i, VGA_ATT_IW);
		outb_p(par->atc[i], VGA_ATT_IW);
	}

	/* Wait for screen to stabilize. */
	mdelay(50);

	outb(0x01, VGA_SEQ_I);
	outb(par->seq[1], VGA_SEQ_D);

	inb(VGA_IS1_RC);
	outb(0x20, VGA_ATT_IW);
	
	return 0;
}

static int vga16fb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *fb)
{
	struct vga16fb_info *info = (struct vga16fb_info*)fb;
	struct vga16fb_par par;
	struct display *display;
	int err;

	if (con < 0)
		display = fb->disp;
	else
		display = fb_display + con;
	if ((err = vga16fb_decode_var(var, &par, info)) != 0)
		return err;
	vga16fb_encode_var(var, &par, info);
	
	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		u32 oldxres, oldyres, oldvxres, oldvyres, oldbpp;

		oldxres = display->var.xres;
		oldyres = display->var.yres;
		oldvxres = display->var.xres_virtual;
		oldvyres = display->var.yres_virtual;
		oldbpp = display->var.bits_per_pixel;

		display->var = *var;

		if (oldxres != var->xres || oldyres != var->yres ||
		    oldvxres != var->xres_virtual || oldvyres != var->yres_virtual ||
		    oldbpp != var->bits_per_pixel) {
			vga16fb_set_disp(con, info);
			if (info->fb_info.changevar)
				info->fb_info.changevar(con);
		}
		if (con == currcon)
			vga16fb_set_par(&par, info);
	}

	return 0;
}

static void ega16_setpalette(int regno, unsigned red, unsigned green, unsigned blue)
{
	static unsigned char map[] = { 000, 001, 010, 011 };
	int val;
	
	val = map[red>>14] | ((map[green>>14]) << 1) | ((map[blue>>14]) << 2);
	inb_p(0x3DA);   /* ! 0x3BA */
	outb_p(regno, 0x3C0);
	outb_p(val, 0x3C0);
	inb_p(0x3DA);   /* some clones need it */
	outb_p(0x20, 0x3C0); /* unblank screen */
}

static int vga16_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			  unsigned *blue, unsigned *transp,
			  struct fb_info *fb_info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  Return != 0 for invalid regno.
	 */

	if (regno >= 16)
		return 1;

	*red   = palette[regno].red;
	*green = palette[regno].green;
	*blue  = palette[regno].blue;
	*transp = 0;
	return 0;
}

static void vga16_setpalette(int regno, unsigned red, unsigned green, unsigned blue)
{
	outb(regno,       dac_reg);
	outb(red   >> 10, dac_val);
	outb(green >> 10, dac_val);
	outb(blue  >> 10, dac_val);
}

static int vga16_setcolreg(unsigned regno, unsigned red, unsigned green,
			  unsigned blue, unsigned transp,
			  struct fb_info *fb_info)
{
	int gray;

	/*
	 *  Set a single color register. The values supplied are
	 *  already rounded down to the hardware's capabilities
	 *  (according to the entries in the `var' structure). Return
	 *  != 0 for invalid regno.
	 */
	
	if (regno >= 16)
		return 1;

	palette[regno].red   = red;
	palette[regno].green = green;
	palette[regno].blue  = blue;
	
	if (currcon < 0)
		gray = disp.var.grayscale;
	else
		gray = fb_display[currcon].var.grayscale;
	if (gray) {
		/* gray = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
	}
	if (((struct vga16fb_info *) fb_info)->isVGA) 
		vga16_setpalette(regno,red,green,blue);
	else
		ega16_setpalette(regno,red,green,blue);
	
	return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, vga16_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(16), 1, vga16_setcolreg,
			    info);
}

static int vga16fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, vga16_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(16),
		     cmap, kspc ? 0 : 2);
	return 0;
}

static int vga16fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		err = fb_alloc_cmap(&fb_display[con].cmap,16,0);
		if (err)
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, vga16_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

static int vga16fb_pan_display(struct fb_var_screeninfo *var, int con,
			       struct fb_info *info) 
{
	if (var->xoffset + fb_display[con].var.xres > fb_display[con].var.xres_virtual ||
	    var->yoffset + fb_display[con].var.yres > fb_display[con].var.yres_virtual)
		return -EINVAL;
	if (con == currcon)
		vga16fb_pan_var(info, var);
	fb_display[con].var.xoffset = var->xoffset;
	fb_display[con].var.yoffset = var->yoffset;
	fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

static struct fb_ops vga16fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	vga16fb_get_fix,
	fb_get_var:	vga16fb_get_var,
	fb_set_var:	vga16fb_set_var,
	fb_get_cmap:	vga16fb_get_cmap,
	fb_set_cmap:	vga16fb_set_cmap,
	fb_pan_display:	vga16fb_pan_display,
};

int vga16fb_setup(char *options)
{
	char *this_opt;
	
	vga16fb.fb_info.fontname[0] = '\0';
	
	if (!options || !*options)
		return 0;
	
	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt) continue;
		
		if (!strncmp(this_opt, "font:", 5))
			strcpy(vga16fb.fb_info.fontname, this_opt+5);
	}
	return 0;
}

static int vga16fb_switch(int con, struct fb_info *fb)
{
	struct vga16fb_par par;
	struct vga16fb_info *info = (struct vga16fb_info*)fb;

	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, vga16_getcolreg,
			    fb);
	
	currcon = con;
	vga16fb_decode_var(&fb_display[con].var, &par, info);
	vga16fb_set_par(&par, info);
	vga16fb_set_disp(con, info);

	/* Install new colormap */
	do_install_cmap(con, fb);
/*	vga16fb_update_var(con, fb); */
	return 1;
}

/* The following VESA blanking code is taken from vgacon.c.  The VGA
   blanking code was originally by Huang shi chao, and modified by
   Christoph Rimek (chrimek@toppoint.de) and todd j. derr
   (tjd@barefoot.org) for Linux. */
#define attrib_port	0x3c0
#define seq_port_reg	0x3c4
#define seq_port_val	0x3c5
#define gr_port_reg	0x3ce
#define gr_port_val	0x3cf
#define video_misc_rd	0x3cc
#define video_misc_wr	0x3c2
#define vga_video_port_reg	0x3d4
#define vga_video_port_val	0x3d5

static void vga_vesa_blank(struct vga16fb_info *info, int mode)
{
	unsigned char SeqCtrlIndex;
	unsigned char CrtCtrlIndex;
	
	cli();
	SeqCtrlIndex = inb_p(seq_port_reg);
	CrtCtrlIndex = inb_p(vga_video_port_reg);

	/* save original values of VGA controller registers */
	if(!info->vesa_blanked) {
		info->vga_state.CrtMiscIO = inb_p(video_misc_rd);
		sti();

		outb_p(0x00,vga_video_port_reg);	/* HorizontalTotal */
		info->vga_state.HorizontalTotal = inb_p(vga_video_port_val);
		outb_p(0x01,vga_video_port_reg);	/* HorizDisplayEnd */
		info->vga_state.HorizDisplayEnd = inb_p(vga_video_port_val);
		outb_p(0x04,vga_video_port_reg);	/* StartHorizRetrace */
		info->vga_state.StartHorizRetrace = inb_p(vga_video_port_val);
		outb_p(0x05,vga_video_port_reg);	/* EndHorizRetrace */
		info->vga_state.EndHorizRetrace = inb_p(vga_video_port_val);
		outb_p(0x07,vga_video_port_reg);	/* Overflow */
		info->vga_state.Overflow = inb_p(vga_video_port_val);
		outb_p(0x10,vga_video_port_reg);	/* StartVertRetrace */
		info->vga_state.StartVertRetrace = inb_p(vga_video_port_val);
		outb_p(0x11,vga_video_port_reg);	/* EndVertRetrace */
		info->vga_state.EndVertRetrace = inb_p(vga_video_port_val);
		outb_p(0x17,vga_video_port_reg);	/* ModeControl */
		info->vga_state.ModeControl = inb_p(vga_video_port_val);
		outb_p(0x01,seq_port_reg);		/* ClockingMode */
		info->vga_state.ClockingMode = inb_p(seq_port_val);
	}

	/* assure that video is enabled */
	/* "0x20" is VIDEO_ENABLE_bit in register 01 of sequencer */
	cli();
	outb_p(0x01,seq_port_reg);
	outb_p(info->vga_state.ClockingMode | 0x20,seq_port_val);

	/* test for vertical retrace in process.... */
	if ((info->vga_state.CrtMiscIO & 0x80) == 0x80)
		outb_p(info->vga_state.CrtMiscIO & 0xef,video_misc_wr);

	/*
	 * Set <End of vertical retrace> to minimum (0) and
	 * <Start of vertical Retrace> to maximum (incl. overflow)
	 * Result: turn off vertical sync (VSync) pulse.
	 */
	if (mode & VESA_VSYNC_SUSPEND) {
		outb_p(0x10,vga_video_port_reg);	/* StartVertRetrace */
		outb_p(0xff,vga_video_port_val); 	/* maximum value */
		outb_p(0x11,vga_video_port_reg);	/* EndVertRetrace */
		outb_p(0x40,vga_video_port_val);	/* minimum (bits 0..3)  */
		outb_p(0x07,vga_video_port_reg);	/* Overflow */
		outb_p(info->vga_state.Overflow | 0x84,vga_video_port_val); /* bits 9,10 of vert. retrace */
	}

	if (mode & VESA_HSYNC_SUSPEND) {
		/*
		 * Set <End of horizontal retrace> to minimum (0) and
		 *  <Start of horizontal Retrace> to maximum
		 * Result: turn off horizontal sync (HSync) pulse.
		 */
		outb_p(0x04,vga_video_port_reg);	/* StartHorizRetrace */
		outb_p(0xff,vga_video_port_val);	/* maximum */
		outb_p(0x05,vga_video_port_reg);	/* EndHorizRetrace */
		outb_p(0x00,vga_video_port_val);	/* minimum (0) */
	}

	/* restore both index registers */
	outb_p(SeqCtrlIndex,seq_port_reg);
	outb_p(CrtCtrlIndex,vga_video_port_reg);
	sti();
}

static void vga_vesa_unblank(struct vga16fb_info *info)
{
	unsigned char SeqCtrlIndex;
	unsigned char CrtCtrlIndex;
	
	cli();
	SeqCtrlIndex = inb_p(seq_port_reg);
	CrtCtrlIndex = inb_p(vga_video_port_reg);

	/* restore original values of VGA controller registers */
	outb_p(info->vga_state.CrtMiscIO,video_misc_wr);

	outb_p(0x00,vga_video_port_reg);		/* HorizontalTotal */
	outb_p(info->vga_state.HorizontalTotal,vga_video_port_val);
	outb_p(0x01,vga_video_port_reg);		/* HorizDisplayEnd */
	outb_p(info->vga_state.HorizDisplayEnd,vga_video_port_val);
	outb_p(0x04,vga_video_port_reg);		/* StartHorizRetrace */
	outb_p(info->vga_state.StartHorizRetrace,vga_video_port_val);
	outb_p(0x05,vga_video_port_reg);		/* EndHorizRetrace */
	outb_p(info->vga_state.EndHorizRetrace,vga_video_port_val);
	outb_p(0x07,vga_video_port_reg);		/* Overflow */
	outb_p(info->vga_state.Overflow,vga_video_port_val);
	outb_p(0x10,vga_video_port_reg);		/* StartVertRetrace */
	outb_p(info->vga_state.StartVertRetrace,vga_video_port_val);
	outb_p(0x11,vga_video_port_reg);		/* EndVertRetrace */
	outb_p(info->vga_state.EndVertRetrace,vga_video_port_val);
	outb_p(0x17,vga_video_port_reg);		/* ModeControl */
	outb_p(info->vga_state.ModeControl,vga_video_port_val);
	outb_p(0x01,seq_port_reg);		/* ClockingMode */
	outb_p(info->vga_state.ClockingMode,seq_port_val);

	/* restore index/control registers */
	outb_p(SeqCtrlIndex,seq_port_reg);
	outb_p(CrtCtrlIndex,vga_video_port_reg);
	sti();
}

static void vga_pal_blank(void)
{
	int i;

	for (i=0; i<16; i++) {
		outb_p (i, dac_reg) ;
		outb_p (0, dac_val) ;
		outb_p (0, dac_val) ;
		outb_p (0, dac_val) ;
	}
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */
static void vga16fb_blank(int blank, struct fb_info *fb_info)
{
	struct vga16fb_info *info = (struct vga16fb_info*)fb_info;

	switch (blank) {
	case 0:				/* Unblank */
		if (info->vesa_blanked) {
			vga_vesa_unblank(info);
			info->vesa_blanked = 0;
		}
		if (info->palette_blanked) {
			do_install_cmap(currcon, fb_info);
			info->palette_blanked = 0;
		}
		break;
	case 1:				/* blank */
		vga_pal_blank();
		info->palette_blanked = 1;
		break;
	default:			/* VESA blanking */
		vga_vesa_blank(info, blank-1);
		info->vesa_blanked = 1;
		break;
	}
}

int __init vga16fb_init(void)
{
	int i,j;

	printk(KERN_DEBUG "vga16fb: initializing\n");

	/* XXX share VGA_FB_PHYS region with vgacon */

        vga16fb.video_vbase = ioremap(VGA_FB_PHYS, VGA_FB_PHYS_LEN);
	if (!vga16fb.video_vbase) {
		printk(KERN_ERR "vga16fb: unable to map device\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "vga16fb: mapped to 0x%p\n", vga16fb.video_vbase);

	vga16fb.isVGA = ORIG_VIDEO_ISVGA;
	vga16fb.palette_blanked = 0;
	vga16fb.vesa_blanked = 0;

	i = vga16fb.isVGA? 6 : 2;
	
	vga16fb_defined.red.length   = i;
	vga16fb_defined.green.length = i;
	vga16fb_defined.blue.length  = i;	
	for(i = 0; i < 16; i++) {
		j = color_table[i];
		palette[i].red   = default_red[j];
		palette[i].green = default_grn[j];
		palette[i].blue  = default_blu[j];
	}

	/* XXX share VGA I/O region with vgacon and others */

	disp.var = vga16fb_defined;

	/* name should not depend on EGA/VGA */
	strcpy(vga16fb.fb_info.modename, "VGA16 VGA");
	vga16fb.fb_info.changevar = NULL;
	vga16fb.fb_info.node = -1;
	vga16fb.fb_info.fbops = &vga16fb_ops;
	vga16fb.fb_info.disp=&disp;
	vga16fb.fb_info.switch_con=&vga16fb_switch;
	vga16fb.fb_info.updatevar=&vga16fb_update_var;
	vga16fb.fb_info.blank=&vga16fb_blank;
	vga16fb.fb_info.flags=FBINFO_FLAG_DEFAULT;
	vga16fb_set_disp(-1, &vga16fb);

	if (register_framebuffer(&vga16fb.fb_info)<0) {
		iounmap(vga16fb.video_vbase);
		return -EINVAL;
	}

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	       GET_FB_IDX(vga16fb.fb_info.node), vga16fb.fb_info.modename);

	return 0;
}

static void __exit vga16fb_exit(void)
{
    unregister_framebuffer(&vga16fb.fb_info);
    iounmap(vga16fb.video_vbase);
    /* XXX unshare VGA regions */
}

#ifdef MODULE
MODULE_LICENSE("GPL");
module_init(vga16fb_init);
#endif
module_exit(vga16fb_exit);


/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

