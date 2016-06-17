/*
 * linux/drivers/video/hgafb.c -- Hercules graphics adaptor frame buffer device
 * 
 *      Created 25 Nov 1999 by Ferenc Bakonyi (fero@drama.obuda.kando.hu)
 *      Based on skeletonfb.c by Geert Uytterhoeven and
 *               mdacon.c by Andrew Apted
 *
 * History:
 *
 * - Revision 0.1.7 (23 Jan 2001): fix crash resulting from MDA only cards 
 *				   being detected as Hercules.	 (Paul G.)
 * - Revision 0.1.6 (17 Aug 2000): new style structs
 *                                 documentation
 * - Revision 0.1.5 (13 Mar 2000): spinlocks instead of saveflags();cli();etc
 *                                 minor fixes
 * - Revision 0.1.4 (24 Jan 2000): fixed a bug in hga_card_detect() for 
 *                                  HGA-only systems
 * - Revision 0.1.3 (22 Jan 2000): modified for the new fb_info structure
 *                                 screen is cleared after rmmod
 *                                 virtual resolutions
 *                                 kernel parameter 'video=hga:font:{fontname}'
 *                                 module parameter 'font={fontname}'
 *                                 module parameter 'nologo={0|1}'
 *                                 the most important: boot logo :)
 * - Revision 0.1.0  (6 Dec 1999): faster scrolling and minor fixes
 * - First release  (25 Nov 1999)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/vga.h>
#include <video/fbcon.h>
#include <video/fbcon-hga.h>

#ifdef MODULE

#define INCLUDE_LINUX_LOGO_DATA
#include <linux/linux_logo.h>

#endif /* MODULE */

#if 0
#define DPRINTK(args...) printk(KERN_DEBUG __FILE__": " ##args)
#else
#define DPRINTK(args...)
#endif

#if 0
#define CHKINFO(ret) if (info != &fb_info) { printk(KERN_DEBUG __FILE__": This should never happen, line:%d \n", __LINE__); return ret; }
#else
#define CHKINFO(ret)
#endif

/* Description of the hardware layout */

static unsigned long hga_vram_base;		/* Base of video memory */
static unsigned long hga_vram_len;		/* Size of video memory */

#define HGA_TXT			0
#define HGA_GFX			1

static int hga_mode = -1;			/* 0 = txt, 1 = gfx mode */

static enum { TYPE_HERC, TYPE_HERCPLUS, TYPE_HERCCOLOR } hga_type;
static char *hga_type_name;

#define HGA_INDEX_PORT		0x3b4		/* Register select port */
#define HGA_VALUE_PORT		0x3b5		/* Register value port */
#define HGA_MODE_PORT		0x3b8		/* Mode control port */
#define HGA_STATUS_PORT		0x3ba		/* Status and Config port */
#define HGA_GFX_PORT		0x3bf		/* Graphics control port */

/* HGA register values */

#define HGA_CURSOR_BLINKING	0x00
#define HGA_CURSOR_OFF		0x20
#define HGA_CURSOR_SLOWBLINK	0x60

#define HGA_MODE_GRAPHICS	0x02
#define HGA_MODE_VIDEO_EN	0x08
#define HGA_MODE_BLINK_EN	0x20
#define HGA_MODE_GFX_PAGE1	0x80

#define HGA_STATUS_HSYNC	0x01
#define HGA_STATUS_VSYNC	0x80
#define HGA_STATUS_VIDEO	0x08

#define HGA_CONFIG_COL132	0x08
#define HGA_GFX_MODE_EN		0x01
#define HGA_GFX_PAGE_EN		0x02

/* Global locks */

static spinlock_t hga_reg_lock = SPIN_LOCK_UNLOCKED;

/* Framebuffer driver structures */

static struct fb_var_screeninfo hga_default_var = {
	xres:		720,
	yres:		348,
	xres_virtual:	720,
	yres_virtual:	348,
	xoffset:	0,
	yoffset:	0,
	bits_per_pixel:	1,
	grayscale:	0,
	red:		{0, 1, 0},
	green:		{0, 1, 0},
	blue:		{0, 1, 0},
	transp:		{0, 0, 0},
	nonstd:		0,			/* (FB_NONSTD_HGA ?) */
	activate:	0,
	height:		-1,
	width:		-1,
	accel_flags:	0,
	/* pixclock */
	/* left_margin, right_margin */
	/* upper_margin, lower_margin */
	/* hsync_len, vsync_len */
	/* sync */
	/* vmode */
};

static struct fb_fix_screeninfo hga_fix = {
	id:		"HGA",
	smem_start:	(unsigned long) NULL,
	smem_len:	0,
	type:		FB_TYPE_PACKED_PIXELS,	/* (not sure) */
	type_aux:	0,			/* (not sure) */
	visual:		FB_VISUAL_MONO10,
	xpanstep:	8,
	ypanstep:	8,
	ywrapstep:	0,
	line_length:	90,
	mmio_start:	0,
	mmio_len:	0,
	accel:		FB_ACCEL_NONE
};

static struct fb_info fb_info;
static struct display disp;

/* Don't assume that tty1 will be the initial current console. */
static int currcon = -1; 
static int release_io_port = 0;
static int release_io_ports = 0;

#ifdef MODULE
static char *font = NULL;
static int nologo = 0;
#endif

/* -------------------------------------------------------------------------
 *
 * Low level hardware functions
 *
 * ------------------------------------------------------------------------- */

static void write_hga_b(unsigned int val, unsigned char reg)
{
	outb_p(reg, HGA_INDEX_PORT); 
	outb_p(val, HGA_VALUE_PORT);
}

static void write_hga_w(unsigned int val, unsigned char reg)
{
	outb_p(reg,   HGA_INDEX_PORT); outb_p(val >> 8,   HGA_VALUE_PORT);
	outb_p(reg+1, HGA_INDEX_PORT); outb_p(val & 0xff, HGA_VALUE_PORT);
}

static int test_hga_b(unsigned char val, unsigned char reg)
{
	outb_p(reg, HGA_INDEX_PORT); 
	outb  (val, HGA_VALUE_PORT);
	udelay(20); val = (inb_p(HGA_VALUE_PORT) == val);
	return val;
}

static void hga_clear_screen(void)
{
	unsigned char fillchar = 0xbf; /* magic */
	unsigned long flags;

	spin_lock_irqsave(&hga_reg_lock, flags);
	if (hga_mode == HGA_TXT)
		fillchar = ' ';
	else if (hga_mode == HGA_GFX)
		fillchar = 0x00;
	spin_unlock_irqrestore(&hga_reg_lock, flags);
	if (fillchar != 0xbf)
		isa_memset_io(hga_vram_base, fillchar, hga_vram_len);
}


#ifdef MODULE
static void hga_txt_mode(void)
{
	unsigned long flags;

	spin_lock_irqsave(&hga_reg_lock, flags);
	outb_p(HGA_MODE_VIDEO_EN | HGA_MODE_BLINK_EN, HGA_MODE_PORT);
	outb_p(0x00, HGA_GFX_PORT);
	outb_p(0x00, HGA_STATUS_PORT);

	write_hga_b(0x61, 0x00);	/* horizontal total */
	write_hga_b(0x50, 0x01);	/* horizontal displayed */
	write_hga_b(0x52, 0x02);	/* horizontal sync pos */
	write_hga_b(0x0f, 0x03);	/* horizontal sync width */

	write_hga_b(0x19, 0x04);	/* vertical total */
	write_hga_b(0x06, 0x05);	/* vertical total adjust */
	write_hga_b(0x19, 0x06);	/* vertical displayed */
	write_hga_b(0x19, 0x07);	/* vertical sync pos */

	write_hga_b(0x02, 0x08);	/* interlace mode */
	write_hga_b(0x0d, 0x09);	/* maximum scanline */
	write_hga_b(0x0c, 0x0a);	/* cursor start */
	write_hga_b(0x0d, 0x0b);	/* cursor end */

	write_hga_w(0x0000, 0x0c);	/* start address */
	write_hga_w(0x0000, 0x0e);	/* cursor location */

	hga_mode = HGA_TXT;
	spin_unlock_irqrestore(&hga_reg_lock, flags);
}
#endif /* MODULE */

static void hga_gfx_mode(void)
{
	unsigned long flags;

	spin_lock_irqsave(&hga_reg_lock, flags);
	outb_p(0x00, HGA_STATUS_PORT);
	outb_p(HGA_GFX_MODE_EN, HGA_GFX_PORT);
	outb_p(HGA_MODE_VIDEO_EN | HGA_MODE_GRAPHICS, HGA_MODE_PORT);

	write_hga_b(0x35, 0x00);	/* horizontal total */
	write_hga_b(0x2d, 0x01);	/* horizontal displayed */
	write_hga_b(0x2e, 0x02);	/* horizontal sync pos */
	write_hga_b(0x07, 0x03);	/* horizontal sync width */

	write_hga_b(0x5b, 0x04);	/* vertical total */
	write_hga_b(0x02, 0x05);	/* vertical total adjust */
	write_hga_b(0x57, 0x06);	/* vertical displayed */
	write_hga_b(0x57, 0x07);	/* vertical sync pos */

	write_hga_b(0x02, 0x08);	/* interlace mode */
	write_hga_b(0x03, 0x09);	/* maximum scanline */
	write_hga_b(0x00, 0x0a);	/* cursor start */
	write_hga_b(0x00, 0x0b);	/* cursor end */

	write_hga_w(0x0000, 0x0c);	/* start address */
	write_hga_w(0x0000, 0x0e);	/* cursor location */

	hga_mode = HGA_GFX;
	spin_unlock_irqrestore(&hga_reg_lock, flags);
}

#ifdef MODULE
static void hga_show_logo(void)
{
	int x, y;
	unsigned long dest = hga_vram_base;
	char *logo = linux_logo_bw;
	for (y = 134; y < 134 + 80 ; y++) /* this needs some cleanup */
		for (x = 0; x < 10 ; x++)
			isa_writeb(~*(logo++),
				   (dest + (y%4)*8192 + (y>>2)*90 + x + 40));
}
#endif /* MODULE */	

static void hga_pan(unsigned int xoffset, unsigned int yoffset)
{
	unsigned int base;
	unsigned long flags;
	
	base = (yoffset / 8) * 90 + xoffset;
	spin_lock_irqsave(&hga_reg_lock, flags);
	write_hga_w(base, 0x0c);	/* start address */
	spin_unlock_irqrestore(&hga_reg_lock, flags);
	DPRINTK("hga_pan: base:%d\n", base);
}

static void hga_blank(int blank_mode)
{
	unsigned long flags;

	spin_lock_irqsave(&hga_reg_lock, flags);
	if (blank_mode) {
		outb_p(0x00, HGA_MODE_PORT);	/* disable video */
	} else {
		outb_p(HGA_MODE_VIDEO_EN | HGA_MODE_GRAPHICS, HGA_MODE_PORT);
	}
	spin_unlock_irqrestore(&hga_reg_lock, flags);
}

static int __init hga_card_detect(void)
{
	int count=0;
	unsigned long p, q;
	unsigned short p_save, q_save;

	hga_vram_base = 0xb0000;
	hga_vram_len  = 0x08000;

	if (request_region(0x3b0, 12, "hgafb"))
		release_io_ports = 1;
	if (request_region(0x3bf, 1, "hgafb"))
		release_io_port = 1;

	/* do a memory check */

	p = hga_vram_base;
	q = hga_vram_base + 0x01000;

	p_save = isa_readw(p); q_save = isa_readw(q);

	isa_writew(0xaa55, p); if (isa_readw(p) == 0xaa55) count++;
	isa_writew(0x55aa, p); if (isa_readw(p) == 0x55aa) count++;
	isa_writew(p_save, p);

	if (count != 2) {
		return 0;
	}

	/* Ok, there is definitely a card registering at the correct
	 * memory location, so now we do an I/O port test.
	 */
	
	if (!test_hga_b(0x66, 0x0f)) {	    /* cursor low register */
		return 0;
	}
	if (!test_hga_b(0x99, 0x0f)) {     /* cursor low register */
		return 0;
	}

	/* See if the card is a Hercules, by checking whether the vsync
	 * bit of the status register is changing.  This test lasts for
	 * approximately 1/10th of a second.
	 */
	
	p_save = q_save = inb_p(HGA_STATUS_PORT) & HGA_STATUS_VSYNC;

	for (count=0; count < 50000 && p_save == q_save; count++) {
		q_save = inb(HGA_STATUS_PORT) & HGA_STATUS_VSYNC;
		udelay(2);
	}

	if (p_save == q_save) 
		return 0;

	switch (inb_p(HGA_STATUS_PORT) & 0x70) {
		case 0x10:
			hga_type = TYPE_HERCPLUS;
			hga_type_name = "HerculesPlus";
			break;
		case 0x50:
			hga_type = TYPE_HERCCOLOR;
			hga_type_name = "HerculesColor";
			break;
		default:
			hga_type = TYPE_HERC;
			hga_type_name = "Hercules";
			break;
	}
	return 1;
}

/* ------------------------------------------------------------------------- *
 *
 * dispsw functions
 *
 * ------------------------------------------------------------------------- */

/**
 *	hga_get_fix - get the fixed part of the display
 *	@fix:struct fb_fix_screeninfo to fill in
 *	@con:unused
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This wrapper function copies @info->fix to @fix.
 *	A zero is returned on success and %-EINVAL for failure.
 */

int hga_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hga_get_fix: con:%d, info:%x, fb_info:%x\n", con, (unsigned)info, (unsigned)&fb_info);

	*fix = info->fix;
	return 0;
}

/**
 *	hga_get_var - get the user defined part of the display
 *	@var:struct fb_var_screeninfo to fill in
 *	@con:unused
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This wrapper function copies @info->var to @var.
 *	A zero is returned on success and %-EINVAL for failure.
 */

int hga_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hga_get_var: con:%d, info:%x, fb_info:%x\n", con, (unsigned)info, (unsigned)&fb_info);

	*var = info->var;
	return 0;
}

/**
 *	hga_set_var - set the user defined part of the display
 *	@var:new video mode
 *	@con:unused
 *	@info:pointer to fb_info object containing info for current hga board
 *	
 *	This function is called for changing video modes. Since HGA cards have
 *	only one fixed mode we have not much to do. After checking input 
 *	parameters @var is copied to @info->var and @info->changevar is called.
 *	A zero is returned on success and %-EINVAL for failure.
 *	
 *	FIXME:
 *	This is the most mystical function (at least for me).
 *	What is the exact specification of xxx_set_var()?
 *	Should it handle xoffset, yoffset? Should it do panning?
 *	What does vmode mean?
 */

int hga_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hga_set_var: con:%d, activate:%x, info:0x%x, fb_info:%x\n", con, var->activate, (unsigned)info, (unsigned)&fb_info);
	
	if (var->xres != 720 ||	var->yres != 348 ||
	    var->xres_virtual != 720 ||
	    var->yres_virtual < 348 || var->yres_virtual > 348 + 16 ||
	    var->bits_per_pixel != 1 || var->grayscale != 0) {
		return -EINVAL;
	}
	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		info->var = *var;
		if (info->changevar) 
			(*info->changevar)(con);
	}
	return 0;
}

/**
 *	hga_getcolreg - read color registers
 *	@regno:register index to read out
 *	@red:red value
 *	@green:green value
 *	@blue:blue value
 *	@transp:transparency value
 *	@info:unused
 *
 *	This callback function is used to read the color registers of a HGA
 *	board. Since we have only two fixed colors, RGB values are 0x0000 
 *	for register0 and 0xaaaa for register1.
 *	A zero is returned on success and 1 for failure.
 */

static int hga_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			 u_int *transp, struct fb_info *info)
{
	if (regno == 0) {
		*red = *green = *blue = 0x0000;
		*transp = 0;
	} else if (regno == 1) {
		*red = *green = *blue = 0xaaaa;
		*transp = 0;
	} else
		return 1;
	return 0;
}

/**
 *	hga_get_cmap - get the colormap
 *	@cmap:struct fb_cmap to fill in
 *	@kspc:called from kernel space?
 *	@con:unused
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This wrapper function passes it's input parameters to fb_get_cmap().
 *	Callback function hga_getcolreg() is used to read the color registers.
 */

int hga_get_cmap(struct fb_cmap *cmap, int kspc, int con,
                 struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hga_get_cmap: con:%d\n", con);
	return fb_get_cmap(cmap, kspc, hga_getcolreg, info);
}
	
/**
 *	hga_setcolreg - set color registers
 *	@regno:register index to set
 *	@red:red value, unused
 *	@green:green value, unused
 *	@blue:blue value, unused
 *	@transp:transparency value, unused
 *	@info:unused
 *
 *	This callback function is used to set the color registers of a HGA
 *	board. Since we have only two fixed colors only @regno is checked.
 *	A zero is returned on success and 1 for failure.
 */

static int hga_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info)
{
	if (regno > 1)
		return 1;
	return 0;
}

/**
 *	hga_set_cmap - set the colormap
 *	@cmap:struct fb_cmap to set
 *	@kspc:called from kernel space?
 *	@con:unused
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This wrapper function passes it's input parameters to fb_set_cmap().
 *	Callback function hga_setcolreg() is used to set the color registers.
 */

int hga_set_cmap(struct fb_cmap *cmap, int kspc, int con,
                 struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hga_set_cmap: con:%d\n", con);
	return fb_set_cmap(cmap, kspc, hga_setcolreg, info);
}

/**
 *	hga_pan_display - pan or wrap the display
 *	@var:contains new xoffset, yoffset and vmode values
 *	@con:unused
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This function looks only at xoffset, yoffset and the %FB_VMODE_YWRAP
 *	flag in @var. If input parameters are correct it calls hga_pan() to 
 *	program the hardware. @info->var is updated to the new values.
 *	A zero is returned on success and %-EINVAL for failure.
 */

int hga_pan_display(struct fb_var_screeninfo *var, int con,
                    struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("pan_disp: con:%d, wrap:%d, xoff:%d, yoff:%d\n", con, var->vmode & FB_VMODE_YWRAP, var->xoffset, var->yoffset);

	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0 || 
		    var->yoffset >= info->var.yres_virtual ||
		    var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual
		 || var->yoffset + var->yres > info->var.yres_virtual
		 || var->yoffset % 8)
			return -EINVAL;
	}

	hga_pan(var->xoffset, var->yoffset);

	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

    
static struct fb_ops hgafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	hga_get_fix,
	fb_get_var:	hga_get_var,
	fb_set_var:	hga_set_var,
	fb_get_cmap:	hga_get_cmap,
	fb_set_cmap:	hga_set_cmap,
	fb_pan_display:	hga_pan_display,
};
		

/* ------------------------------------------------------------------------- *
 *
 * Functions in fb_info
 * 
 * ------------------------------------------------------------------------- */

/**
 *	hgafbcon_switch - switch console
 *	@con:new console to switch to
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This function should install a new colormap and change the video mode.
 *	Since we have fixed colors and only one video mode we have nothing to 
 *	do.
 *	Only console administration is done but it should go to fbcon.c IMHO.
 *	A zero is returned on success and %-EINVAL for failure.
 */

static int hgafbcon_switch(int con, struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hgafbcon_switch: currcon:%d, con:%d, info:%x, fb_info:%x\n", currcon, con, (unsigned)info, (unsigned)&fb_info);

	/* Save the colormap and video mode */
#if 0	/* Not necessary in hgafb, we use fixed colormap */
	fb_copy_cmap(&info->cmap, &fb_display[currcon].cmap, 0);
#endif

	if (currcon != -1) /* this check is absolute necessary! */
		memcpy(&fb_display[currcon].var, &info->var,
				sizeof(struct fb_var_screeninfo));

	/* Install a new colormap and change the video mode. By default fbcon
	 * sets all the colormaps and video modes to the default values at
	 * bootup.
	 */
#if 0
	fb_copy_cmap(&fb_display[con].cmap, &info->cmap, 0);
	fb_set_cmap(&info->cmap, 1, hga_setcolreg, info);
#endif

	memcpy(&info->var, &fb_display[con].var,
			sizeof(struct fb_var_screeninfo));
	/* hga_set_var(&info->var, con, &fb_info); is it necessary? */
	currcon = con;

	/* Hack to work correctly with XF86_Mono */
	hga_gfx_mode();
	return 0;
}

/**
 *	hgafbcon_updatevar - update the user defined part of the display
 *	@con:console to update or -1 when no consoles defined on this fb
 *	@info:pointer to fb_info object containing info for current hga board
 *
 *	This function is called when @var is changed by fbcon.c without calling 
 *	hga_set_var(). It usually means scrolling.  hga_pan_display() is called
 *	to update the hardware and @info->var.
 *	A zero is returned on success and %-EINVAL for failure.
 */

static int hgafbcon_updatevar(int con, struct fb_info *info)
{
	CHKINFO(-EINVAL);
	DPRINTK("hga_update_var: con:%d, info:%x, fb_info:%x\n", con, (unsigned)info, (unsigned)&fb_info);
	return (con < 0) ? -EINVAL : hga_pan_display(&fb_display[con].var, con, info);
}

/**
 *	hgafbcon_blank - (un)blank the screen
 *	@blank_mode:blanking method to use
 *	@info:unused
 *	
 *	Blank the screen if blank_mode != 0, else unblank. 
 *	Implements VESA suspend and powerdown modes on hardware that supports 
 *	disabling hsync/vsync:
 *		@blank_mode == 2 means suspend vsync,
 *		@blank_mode == 3 means suspend hsync,
 *		@blank_mode == 4 means powerdown.
 */

static void hgafbcon_blank(int blank_mode, struct fb_info *info)
{
	CHKINFO( );
	DPRINTK("hga_blank: blank_mode:%d, info:%x, fb_info:%x\n", blank_mode, (unsigned)info, (unsigned)&fb_info);

	hga_blank(blank_mode);
}


/* ------------------------------------------------------------------------- */
    
	/*
	 *  Initialization
	 */

int __init hgafb_init(void)
{
	if (! hga_card_detect()) {
		printk(KERN_INFO "hgafb: HGA card not detected.\n");
		return -EINVAL;
	}

	printk(KERN_INFO "hgafb: %s with %ldK of memory detected.\n",
		hga_type_name, hga_vram_len/1024);

	hga_gfx_mode();
	hga_clear_screen();
#ifdef MODULE
	if (!nologo) hga_show_logo();
#endif /* MODULE */

	hga_fix.smem_start = VGA_MAP_MEM(hga_vram_base);
	hga_fix.smem_len = hga_vram_len;

	disp.var = hga_default_var;
/*	disp.cmap = ???; */
	disp.screen_base = (char*)hga_fix.smem_start;
	disp.visual = hga_fix.visual;
	disp.type = hga_fix.type;
	disp.type_aux = hga_fix.type_aux;
	disp.ypanstep = hga_fix.ypanstep;
	disp.ywrapstep = hga_fix.ywrapstep;
	disp.line_length = hga_fix.line_length;
	disp.can_soft_blank = 1;
	disp.inverse = 0;
#ifdef FBCON_HAS_HGA
	disp.dispsw = &fbcon_hga;
#else
#warning HGAFB will not work as a console!
	disp.dispsw = &fbcon_dummy;
#endif
	disp.dispsw_data = NULL;

	disp.scrollmode = SCROLL_YREDRAW;
	
	strcpy (fb_info.modename, hga_fix.id);
	fb_info.node = -1;
	fb_info.flags = FBINFO_FLAG_DEFAULT;
/*	fb_info.open = ??? */
	fb_info.var = hga_default_var;
	fb_info.fix = hga_fix;
	fb_info.monspecs.hfmin = 0;
	fb_info.monspecs.hfmax = 0;
	fb_info.monspecs.vfmin = 10000;
	fb_info.monspecs.vfmax = 10000;
	fb_info.monspecs.dpms = 0;
	fb_info.fbops = &hgafb_ops;
	fb_info.screen_base = (char *)hga_fix.smem_start;
	fb_info.disp = &disp;
/*	fb_info.display_fg = ??? */
/*	fb_info.fontname initialized later */
	fb_info.changevar = NULL;
	fb_info.switch_con = hgafbcon_switch;
	fb_info.updatevar = hgafbcon_updatevar;
	fb_info.blank = hgafbcon_blank;
	fb_info.pseudo_palette = NULL; /* ??? */
	fb_info.par = NULL;

        if (register_framebuffer(&fb_info) < 0)
                return -EINVAL;

        printk(KERN_INFO "fb%d: %s frame buffer device\n",
               GET_FB_IDX(fb_info.node), fb_info.modename);
	
	return 0;
}

	/*
	 *  Setup
	 */

#ifndef MODULE
int __init hgafb_setup(char *options)
{
	/* 
	 * Parse user speficied options
	 * `video=hga:font:VGA8x16' or
	 * `video=hga:font:SUN8x16' recommended
	 * Other supported fonts: VGA8x8, Acorn8x8, PEARL8x8
	 * More different fonts can be used with the `setfont' utility.
	 */

	char *this_opt;

	fb_info.fontname[0] = '\0';

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ","))) {
		if (!strncmp(this_opt, "font:", 5))
			strcpy(fb_info.fontname, this_opt+5);
	}
	return 0;
}
#endif /* !MODULE */


	/*
	 * Cleanup
	 */

#ifdef MODULE
static void hgafb_cleanup(struct fb_info *info)
{
	hga_txt_mode();
	hga_clear_screen();
	unregister_framebuffer(info);
	if (release_io_ports) release_region(0x3b0, 12);
	if (release_io_port) release_region(0x3bf, 1);
}
#endif /* MODULE */



/* -------------------------------------------------------------------------
 *
 *  Modularization
 *
 * ------------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void)
{
	if (font)
		strncpy(fb_info.fontname, font, sizeof(fb_info.fontname)-1);
	else
		fb_info.fontname[0] = '\0';

	return hgafb_init();
}

void cleanup_module(void)
{
	hgafb_cleanup(&fb_info);
}

MODULE_AUTHOR("Ferenc Bakonyi (fero@drama.obuda.kando.hu)");
MODULE_DESCRIPTION("FBDev driver for Hercules Graphics Adaptor");
MODULE_LICENSE("GPL");

MODULE_PARM(font, "s");
MODULE_PARM_DESC(font, "Specifies one of the compiled-in fonts (VGA8x8, VGA8x16, SUN8x16, Acorn8x8, PEARL8x8) (default=none)");
MODULE_PARM(nologo, "i");
MODULE_PARM_DESC(nologo, "Disables startup logo if != 0 (default=0)");

#endif /* MODULE */
