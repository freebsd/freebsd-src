/* macfb.c: Generic framebuffer for Macs whose colourmaps/modes we
   don't know how to set */

/* (c) 1999 David Huggins-Daines <dhd@debian.org>

   Primarily based on vesafb.c, by Gerd Knorr
   (c) 1998 Gerd Knorr <kraxel@cs.tu-berlin.de>

   Also uses information and code from:
   
   The original macfb.c from Linux/mac68k 2.0, by Alan Cox, Juergen
   Mellinger, Mikael Forselius, Michael Schmitz, and others.

   valkyriefb.c, by Martin Costabel, Kevin Schoedel, Barry Nathan, Dan
   Jacobowitz, Paul Mackerras, Fabio Riccardi, and Geert Uytterhoeven.
   
   This code is free software.  You may copy, modify, and distribute
   it subject to the terms and conditions of the GNU General Public
   License, version 2, or any later version, at your convenience. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/nubus.h>
#include <linux/init.h>
#include <linux/fb.h>

#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/macintosh.h>
#include <asm/io.h>
#include <asm/machw.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#if defined(FBCON_HAS_CFB8) || defined(FBCON_HAS_CFB4) || defined(FBCON_HAS_CFB2)

/* Common DAC base address for the LC, RBV, Valkyrie, and IIvx */
#define DAC_BASE 0x50f24000

/* Some addresses for the DAFB */
#define DAFB_BASE 0xf9800200

/* Address for the built-in Civic framebuffer in Quadra AVs */
#define CIVIC_BASE 0x50f30800	/* Only tested on 660AV! */

/* GSC (Gray Scale Controller) base address */
#define GSC_BASE 0x50F20000

/* CSC (Color Screen Controller) base address */
#define CSC_BASE 0x50F20000

static int (*macfb_setpalette) (unsigned int regno, unsigned int red,
				unsigned int green, unsigned int blue) = NULL;
static int valkyrie_setpalette (unsigned int regno, unsigned int red,
				unsigned int green, unsigned int blue);
static int dafb_setpalette (unsigned int regno, unsigned int red,
			    unsigned int green, unsigned int blue);
static int rbv_setpalette (unsigned int regno, unsigned int red,
			   unsigned int green, unsigned int blue);
static int mdc_setpalette (unsigned int regno, unsigned int red,
			   unsigned int green, unsigned int blue);
static int toby_setpalette (unsigned int regno, unsigned int red,
			    unsigned int green, unsigned int blue);
static int civic_setpalette (unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue);
static int csc_setpalette (unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue);

static volatile struct {
	unsigned char addr;
	/* Note: word-aligned */
	char pad[3];
	unsigned char lut;
} *valkyrie_cmap_regs;

static volatile struct {
	unsigned char addr;
	unsigned char lut;
} *v8_brazil_cmap_regs;

static volatile struct {
	unsigned char addr;
	char pad1[3]; /* word aligned */
	unsigned char lut;
	char pad2[3]; /* word aligned */
	unsigned char cntl; /* a guess as to purpose */
} *rbv_cmap_regs;

static volatile struct {
	unsigned long reset;
	unsigned long pad1[3];
	unsigned char pad2[3];
	unsigned char lut;
} *dafb_cmap_regs;

static volatile struct {
	unsigned char addr;	/* OFFSET: 0x00 */
	unsigned char pad1[15];
	unsigned char lut;	/* OFFSET: 0x10 */
	unsigned char pad2[15];
	unsigned char status;	/* OFFSET: 0x20 */
	unsigned char pad3[7];
	unsigned long vbl_addr;	/* OFFSET: 0x28 */
	unsigned int  status2;	/* OFFSET: 0x2C */
} *civic_cmap_regs;

static volatile struct {
	char    pad1[0x40];
        unsigned char	clut_waddr;	/* 0x40 */
        char    pad2;
        unsigned char	clut_data;	/* 0x42 */
        char	pad3[0x3];
        unsigned char	clut_raddr;	/* 0x46 */
} *csc_cmap_regs;

/* We will leave these the way they are for the time being */
struct mdc_cmap_regs {
	char pad1[0x200200];
	unsigned char addr;
	char pad2[6];
	unsigned char lut;
};

struct toby_cmap_regs {
	char pad1[0x90018];
	unsigned char lut; /* TFBClutWDataReg, offset 0x90018 */
	char pad2[3];
	unsigned char addr; /* TFBClutAddrReg, offset 0x9001C */
};

struct jet_cmap_regs {
	char pad1[0xe0e000];
	unsigned char addr;
	unsigned char lut;
};

#endif

#define PIXEL_TO_MM(a)	(((a)*10)/28)	/* width in mm at 72 dpi */	

static unsigned long video_base;
static int   video_size;
static char* video_vbase;        /* mapped */

/* mode */
static int  video_bpp;
static int  video_width;
static int  video_height;
static int  video_type = FB_TYPE_PACKED_PIXELS;
static int  video_visual;
static int  video_linelength;
static int  video_cmap_len;
static int  video_slot = 0;

static struct fb_var_screeninfo macfb_defined={
	0,0,0,0,	/* W,H, W, H (virtual) load xres,xres_virtual*/
	0,0,		/* virtual -> visible no offset */
	8,		/* depth -> load bits_per_pixel */
	0,		/* greyscale ? */
	{0,0,0},	/* R */
	{0,0,0},	/* G */
	{0,0,0},	/* B */
	{0,0,0},	/* transparency */
	0,		/* standard pixel format */
	FB_ACTIVATE_NOW,
	-1, -1,
	FB_ACCEL_NONE,	/* The only way to accelerate a mac is .. */
	0L,0L,0L,0L,0L,
	0L,0L,0,	/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0,0,0,0,0,0}
};

static struct display disp;
static struct fb_info fb_info;
static struct { u_short blue, green, red, pad; } palette[256];
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

static int             inverse   = 0;
static int             vidtest   = 0;
static int             currcon   = 0;

static int macfb_update_var(int con, struct fb_info *info)
{
	return 0;
}

static int macfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "Mac Generic");

	fix->smem_start = video_base;
	fix->smem_len = video_size;
	fix->type = video_type;
	fix->visual = video_visual;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->line_length=video_linelength;
	return 0;
}

static int macfb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	if(con==-1)
		memcpy(var, &macfb_defined, sizeof(struct fb_var_screeninfo));
	else
		*var=fb_display[con].var;
	return 0;
}

static void macfb_set_disp(int con)
{
	struct fb_fix_screeninfo fix;
	struct display *display;
	
	if (con >= 0)
		display = &fb_display[con];
	else
		display = &disp;	/* used during initialization */

	macfb_get_fix(&fix, con, &fb_info);

	memset(display, 0, sizeof(struct display));
	display->screen_base = video_vbase;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 0;
	display->inverse = inverse;
	display->scrollmode = SCROLL_YREDRAW;
	macfb_get_var(&display->var, -1, &fb_info);

	switch (video_bpp) {
#ifdef FBCON_HAS_MFB
	case 1:
		display->dispsw = &fbcon_mfb;
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		display->dispsw = &fbcon_cfb2;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		display->dispsw = &fbcon_cfb4;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		display->dispsw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
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
		return;
	}
}

static int macfb_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	static int first = 1;

	if (var->xres           != macfb_defined.xres           ||
	    var->yres           != macfb_defined.yres           ||
	    var->xres_virtual   != macfb_defined.xres_virtual   ||
	    var->yres_virtual   != macfb_defined.yres           ||
	    var->xoffset                                        ||
	    var->bits_per_pixel != macfb_defined.bits_per_pixel ||
	    var->nonstd) {
		if (first) {
			printk("macfb does not support changing the video mode\n");
			first = 0;
		}
		return -EINVAL;
	}

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;

	if (var->yoffset)
		return -EINVAL;
	return 0;
}

#if defined(FBCON_HAS_CFB8) || defined(FBCON_HAS_CFB4) || defined(FBCON_HAS_CFB2)
static int valkyrie_setpalette (unsigned int regno, unsigned int red,
				unsigned int green, unsigned int blue)
{
	unsigned long flags;
	
	red >>= 8;
	green >>= 8;
	blue >>= 8;

	save_flags(flags);
	cli();
	
	/* tell clut which address to fill */
	nubus_writeb(regno, &valkyrie_cmap_regs->addr);
	nop();

	/* send one color channel at a time */
	nubus_writeb(red, &valkyrie_cmap_regs->lut);
	nop();
	nubus_writeb(green, &valkyrie_cmap_regs->lut);
	nop();
	nubus_writeb(blue, &valkyrie_cmap_regs->lut);

	restore_flags(flags);

	return 0;
}

/* Unlike the Valkyrie, the DAFB cannot set individual colormap
   registers.  Therefore, we do what the MacOS driver does (no
   kidding!) and simply set them one by one until we hit the one we
   want. */
static int dafb_setpalette (unsigned int regno, unsigned int red,
			    unsigned int green, unsigned int blue)
{
	/* FIXME: really, really need to use ioremap() here,
           phys_to_virt() doesn't work anymore */
	static int lastreg = -1;
	unsigned long flags;
	
	red >>= 8;
	green >>= 8;
	blue >>= 8;

	save_flags(flags);
	cli();
	
	/* fbcon will set an entire colourmap, but X won't.  Hopefully
	   this should accomodate both of them */
	if (regno != lastreg+1) {
		int i;
		
		/* Stab in the dark trying to reset the CLUT pointer */
		nubus_writel(0, &dafb_cmap_regs->reset);
		nop();
		
		/* Loop until we get to the register we want */
		for (i = 0; i < regno; i++) {
			nubus_writeb(palette[i].red >> 8, &dafb_cmap_regs->lut);
			nop();
			nubus_writeb(palette[i].green >> 8, &dafb_cmap_regs->lut);
			nop();
			nubus_writeb(palette[i].blue >> 8, &dafb_cmap_regs->lut);
			nop();
		}
	}
		
	nubus_writeb(red, &dafb_cmap_regs->lut);
	nop();
	nubus_writeb(green, &dafb_cmap_regs->lut);
	nop();
	nubus_writeb(blue, &dafb_cmap_regs->lut);
	
	restore_flags(flags);
	
	lastreg = regno;
	return 0;
}

/* V8 and Brazil seem to use the same DAC.  Sonora does as well. */
static int v8_brazil_setpalette (unsigned int regno, unsigned int red,
				 unsigned int green, unsigned int blue)
{
	unsigned char _red  =red>>8;
	unsigned char _green=green>>8;
	unsigned char _blue =blue>>8;
	unsigned char _regno;
	unsigned long flags;

	if (video_bpp>8) return 1; /* failsafe */

	save_flags(flags);
	cli();

	/* On these chips, the CLUT register numbers are spread out
	   across the register space.  Thus:

	   In 8bpp, all regnos are valid.
	   
	   In 4bpp, the regnos are 0x0f, 0x1f, 0x2f, etc, etc
	   
	   In 2bpp, the regnos are 0x3f, 0x7f, 0xbf, 0xff */
  	_regno = (regno<<(8-video_bpp)) | (0xFF>>video_bpp);
	nubus_writeb(_regno, &v8_brazil_cmap_regs->addr); nop();

	/* send one color channel at a time */
	nubus_writeb(_red, &v8_brazil_cmap_regs->lut); nop();
	nubus_writeb(_green, &v8_brazil_cmap_regs->lut); nop();
	nubus_writeb(_blue, &v8_brazil_cmap_regs->lut);

	restore_flags(flags);
	
	return 0;
}

static int rbv_setpalette (unsigned int regno, unsigned int red,
			   unsigned int green, unsigned int blue)
{
	/* use MSBs */
	unsigned char _red  =red>>8;
	unsigned char _green=green>>8;
	unsigned char _blue =blue>>8;
	unsigned char _regno;
	unsigned long flags;

	if (video_bpp>8) return 1; /* failsafe */

	save_flags(flags);
	cli();
	
	/* From the VideoToolbox driver.  Seems to be saying that
	 * regno #254 and #255 are the important ones for 1-bit color,
	 * regno #252-255 are the important ones for 2-bit color, etc.
	 */
	_regno = regno + (256-(1<<video_bpp));

	/* reset clut? (VideoToolbox sez "not necessary") */
	nubus_writeb(0xFF, &rbv_cmap_regs->cntl); nop();
	
	/* tell clut which address to use. */
	nubus_writeb(_regno, &rbv_cmap_regs->addr); nop();
	
	/* send one color channel at a time. */
	nubus_writeb(_red,   &rbv_cmap_regs->lut); nop();
	nubus_writeb(_green, &rbv_cmap_regs->lut); nop();
	nubus_writeb(_blue,  &rbv_cmap_regs->lut);
	
	restore_flags(flags);
	/* done. */
	return 0;
}

/* Macintosh Display Card (8x24) */
static int mdc_setpalette(unsigned int regno, unsigned int red,
			  unsigned int green, unsigned int blue)
{
	volatile struct mdc_cmap_regs *cmap_regs =
		nubus_slot_addr(video_slot);
	/* use MSBs */
	unsigned char _red  =red>>8;
	unsigned char _green=green>>8;
	unsigned char _blue =blue>>8;
	unsigned char _regno=regno;
	unsigned long flags;

	save_flags(flags);
	cli();
	
	/* the nop's are there to order writes. */
	nubus_writeb(_regno, &cmap_regs->addr); nop();
	nubus_writeb(_red, &cmap_regs->lut);    nop();
	nubus_writeb(_green, &cmap_regs->lut);  nop();
	nubus_writeb(_blue, &cmap_regs->lut);

	restore_flags(flags);
	return 0;
}

/* Toby frame buffer */
static int toby_setpalette(unsigned int regno, unsigned int red,
			   unsigned int green, unsigned int blue)
{
	volatile struct toby_cmap_regs *cmap_regs =
		nubus_slot_addr(video_slot);
	/* use MSBs */
	unsigned char _red  =~(red>>8);
	unsigned char _green=~(green>>8);
	unsigned char _blue =~(blue>>8);
	unsigned char _regno = (regno<<(8-video_bpp)) | (0xFF>>video_bpp);
	unsigned long flags;

	save_flags(flags);
	cli();
	
	nubus_writeb(_regno, &cmap_regs->addr); nop();
	nubus_writeb(_red, &cmap_regs->lut);    nop();
	nubus_writeb(_green, &cmap_regs->lut);  nop();
	nubus_writeb(_blue, &cmap_regs->lut);

	restore_flags(flags);
	return 0;
}

/* Jet frame buffer */
static int jet_setpalette(unsigned int regno, unsigned int red,
			  unsigned int green, unsigned int blue)
{
	volatile struct jet_cmap_regs *cmap_regs =
		nubus_slot_addr(video_slot);
	/* use MSBs */
	unsigned char _red   = (red>>8);
	unsigned char _green = (green>>8);
	unsigned char _blue  = (blue>>8);
	unsigned long flags;

	save_flags(flags);
	cli();
	
	nubus_writeb(regno, &cmap_regs->addr); nop();
	nubus_writeb(_red, &cmap_regs->lut); nop();
	nubus_writeb(_green, &cmap_regs->lut); nop();
	nubus_writeb(_blue, &cmap_regs->lut);

	restore_flags(flags);
	return 0;
}

/*
 * Civic framebuffer -- Quadra AV built-in video.  A chip
 * called Sebastian holds the actual color palettes, and
 * apparently, there are two different banks of 512K RAM 
 * which can act as separate framebuffers for doing video
 * input and viewing the screen at the same time!  The 840AV
 * Can add another 1MB RAM to give the two framebuffers 
 * 1MB RAM apiece.
 *
 * FIXME: this doesn't seem to work anymore.
 */
static int civic_setpalette (unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue)
{
	static int lastreg = -1;
	unsigned long flags;
	int clut_status;
	
	if (video_bpp > 8) return 1; /* failsafe */

	red   >>= 8;
	green >>= 8;
	blue  >>= 8;

	save_flags(flags);
	cli();
	
	/*
	 * Set the register address
	 */
	nubus_writeb(regno, &civic_cmap_regs->addr); nop();

	/*
	 * Wait for VBL interrupt here;
	 * They're usually not enabled from Penguin, so we won't check
	 */
#if 0
	{
#define CIVIC_VBL_OFFSET	0x120
		volatile unsigned long *vbl = nubus_readl(civic_cmap_regs->vbl_addr + CIVIC_VBL_OFFSET);
		/* do interrupt setup stuff here? */
		*vbl = 0L; nop();	/* clear */
		*vbl = 1L; nop();	/* set */
		while (*vbl != 0L)	/* wait for next vbl */
		{
			usleep(10);	/* needed? */
		}
		/* do interrupt shutdown stuff here? */
	}
#endif

	/*
	 * Grab a status word and do some checking;
	 * Then finally write the clut!
	 */
	clut_status =  nubus_readb(&civic_cmap_regs->status2);

	if ((clut_status & 0x0008) == 0)
	{
#if 0
		if ((clut_status & 0x000D) != 0)
		{
			nubus_writeb(0x00, &civic_cmap_regs->lut); nop();
			nubus_writeb(0x00, &civic_cmap_regs->lut); nop();
		}
#endif

		nubus_writeb(  red, &civic_cmap_regs->lut); nop();
		nubus_writeb(green, &civic_cmap_regs->lut); nop();
		nubus_writeb( blue, &civic_cmap_regs->lut); nop();
		nubus_writeb( 0x00, &civic_cmap_regs->lut); nop();
	}
	else
	{
		unsigned char junk;

		junk = nubus_readb(&civic_cmap_regs->lut); nop();
		junk = nubus_readb(&civic_cmap_regs->lut); nop();
		junk = nubus_readb(&civic_cmap_regs->lut); nop();
		junk = nubus_readb(&civic_cmap_regs->lut); nop();

		if ((clut_status & 0x000D) != 0)
		{
			nubus_writeb(0x00, &civic_cmap_regs->lut); nop();
			nubus_writeb(0x00, &civic_cmap_regs->lut); nop();
		}

		nubus_writeb(  red, &civic_cmap_regs->lut); nop();
		nubus_writeb(green, &civic_cmap_regs->lut); nop();
		nubus_writeb( blue, &civic_cmap_regs->lut); nop();
		nubus_writeb( junk, &civic_cmap_regs->lut); nop();
	}

	restore_flags(flags);

	lastreg = regno;
	return 0;
}

/*
 * The CSC is the framebuffer on the PowerBook 190 series
 * (and the 5300 too, but that's a PowerMac). This function
 * brought to you in part by the ECSC driver for MkLinux.
 */

static int csc_setpalette (unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue)
{
	mdelay(1);
	csc_cmap_regs->clut_waddr = regno;
	csc_cmap_regs->clut_data = red;
	csc_cmap_regs->clut_data = green;
	csc_cmap_regs->clut_data = blue;
	return 0;
}

#endif /* FBCON_HAS_CFB8 || FBCON_HAS_CFB4 || FBCON_HAS_CFB2 */

static int macfb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			   unsigned *blue, unsigned *transp,
			   struct fb_info *fb_info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  Return != 0 for invalid regno.
	 */

	if (regno >= video_cmap_len)
		return 1;

	*red   = palette[regno].red;
	*green = palette[regno].green;
	*blue  = palette[regno].blue;
	*transp = 0;
	return 0;
}

static int macfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *fb_info)
{
	/*
	 *  Set a single color register. The values supplied are
	 *  already rounded down to the hardware's capabilities
	 *  (according to the entries in the `var' structure). Return
	 *  != 0 for invalid regno.
	 */
	
	if (regno >= video_cmap_len)
		return 1;

	palette[regno].red   = red;
	palette[regno].green = green;
	palette[regno].blue  = blue;

	switch (video_bpp) {
#ifdef FBCON_HAS_MFB
	case 1:
		/* We shouldn't get here */
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		if (macfb_setpalette)
			macfb_setpalette(regno, red, green, blue);
		else
			return 1;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		if (macfb_setpalette)
			macfb_setpalette(regno, red, green, blue);
		else
			return 1;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		if (macfb_setpalette)
			macfb_setpalette(regno, red, green, blue);
		else
			return 1;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
	case 16:
		/* 1:5:5:5 */
		fbcon_cmap.cfb16[regno] =
			((red   & 0xf800) >>  1) |
			((green & 0xf800) >>  6) |
			((blue  & 0xf800) >> 11) |
			((transp != 0) << 15);
		break;
#endif
		/* I'm pretty sure that one or the other of these
		   doesn't exist on 68k Macs */
#ifdef FBCON_HAS_CFB24
	case 24:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		fbcon_cmap.cfb24[regno] =
			(red   << macfb_defined.red.offset)   |
			(green << macfb_defined.green.offset) |
			(blue  << macfb_defined.blue.offset);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		fbcon_cmap.cfb32[regno] =
			(red   << macfb_defined.red.offset)   |
			(green << macfb_defined.green.offset) |
			(blue  << macfb_defined.blue.offset);
		break;
#endif
    }
    return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, macfb_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(video_cmap_len), 1,
			    macfb_setcolreg, info);
}

static int macfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, macfb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(video_cmap_len),
		     cmap, kspc ? 0 : 2);
	return 0;
}

static int macfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		err = fb_alloc_cmap(&fb_display[con].cmap,video_cmap_len,0);
		if (err)
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, macfb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

static struct fb_ops macfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	macfb_get_fix,
	fb_get_var:	macfb_get_var,
	fb_set_var:	macfb_set_var,
	fb_get_cmap:	macfb_get_cmap,
	fb_set_cmap:	macfb_set_cmap,
};

void __init macfb_setup(char *options)
{
	char *this_opt;
	
	fb_info.fontname[0] = '\0';
	
	if (!options || !*options)
		return;
	
	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt) continue;
		
		if (! strcmp(this_opt, "inverse"))
			inverse=1;
		else if (!strncmp(this_opt, "font:", 5))
			strcpy(fb_info.fontname, this_opt+5);
		/* This means "turn on experimental CLUT code" */
		else if (!strcmp(this_opt, "vidtest"))
			vidtest=1;
	}
}

static int macfb_switch(int con, struct fb_info *info)
{
	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, macfb_getcolreg,
			    info);
	
	currcon = con;
	/* Install new colormap */
	do_install_cmap(con, info);
	macfb_update_var(con, info);
	return 1;
}

static void macfb_blank(int blank, struct fb_info *info)
{
	/* Not supported */
}

void __init macfb_init(void)
{
	struct nubus_dev* ndev = NULL;
	int video_is_nubus = 0;

	if (!MACH_IS_MAC) 
		return;

	/* There can only be one internal video controller anyway so
	   we're not too worried about this */
	video_width      = mac_bi_data.dimensions & 0xFFFF;
	video_height     = mac_bi_data.dimensions >> 16;
	video_bpp        = mac_bi_data.videodepth;
	video_linelength = mac_bi_data.videorow;
	video_size       = video_linelength * video_height;
	/* Note: physical address (since 2.1.127) */
	video_base       = mac_bi_data.videoaddr;
	/* This is actually redundant with the initial mappings.
	   However, there are some non-obvious aspects to the way
	   those mappings are set up, so this is in fact the safest
	   way to ensure that this driver will work on every possible
	   Mac */
	video_vbase	 = ioremap(mac_bi_data.videoaddr, video_size);
	
	printk("macfb: framebuffer at 0x%08lx, mapped to 0x%p, size %dk\n",
	       video_base, video_vbase, video_size/1024);
	printk("macfb: mode is %dx%dx%d, linelength=%d\n",
	       video_width, video_height, video_bpp, video_linelength);
	
	/*
	 *	Fill in the available video resolution
	 */
	 
	macfb_defined.xres           = video_width;
	macfb_defined.yres           = video_height;
	macfb_defined.xres_virtual   = video_width;
	macfb_defined.yres_virtual   = video_height;
	macfb_defined.bits_per_pixel = video_bpp;
	macfb_defined.height = PIXEL_TO_MM(macfb_defined.yres);
	macfb_defined.width  = PIXEL_TO_MM(macfb_defined.xres);	 

	printk("macfb: scrolling: redraw\n");
	macfb_defined.yres_virtual = video_height;

	/* some dummy values for timing to make fbset happy */
	macfb_defined.pixclock     = 10000000 / video_width * 1000 / video_height;
	macfb_defined.left_margin  = (video_width / 8) & 0xf8;
	macfb_defined.right_margin = 32;
	macfb_defined.upper_margin = 16;
	macfb_defined.lower_margin = 4;
	macfb_defined.hsync_len    = (video_width / 8) & 0xf8;
	macfb_defined.vsync_len    = 4;

	switch (video_bpp) {
	case 1:
		/* XXX: I think this will catch any program that tries
		   to do FBIO_PUTCMAP when the visual is monochrome */
		video_cmap_len = 0;
		video_visual = FB_VISUAL_MONO01;
		break;
	case 2:
	case 4:
	case 8:
		macfb_defined.red.length = video_bpp;
		macfb_defined.green.length = video_bpp;
		macfb_defined.blue.length = video_bpp;
		video_cmap_len = 1 << video_bpp;
		video_visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	case 16:
		macfb_defined.transp.offset = 15;
		macfb_defined.transp.length = 1;
		macfb_defined.red.offset = 10;
		macfb_defined.red.length = 5;
		macfb_defined.green.offset = 5;
		macfb_defined.green.length = 5;
		macfb_defined.blue.offset = 0;
		macfb_defined.blue.length = 5;
		printk("macfb: directcolor: "
		       "size=1:5:5:5, shift=15:10:5:0\n");
		video_cmap_len = 16;
		/* Should actually be FB_VISUAL_DIRECTCOLOR, but this
		   works too */
		video_visual = FB_VISUAL_TRUECOLOR;
		break;
	case 24:
	case 32:
		/* XXX: have to test these... can any 68k Macs
		   actually do this on internal video? */
		macfb_defined.red.offset = 16;
		macfb_defined.red.length = 8;
		macfb_defined.green.offset = 8;
		macfb_defined.green.length = 8;
		macfb_defined.blue.offset = 0;
		macfb_defined.blue.length = 8;
		printk("macfb: truecolor: "
		       "size=0:8:8:8, shift=0:16:8:0\n");
		video_cmap_len = 16;
		video_visual = FB_VISUAL_TRUECOLOR;
	default:
		video_cmap_len = 0;
		video_visual = FB_VISUAL_MONO01;
		printk("macfb: unknown or unsupported bit depth: %d\n", video_bpp);
		break;
	}
	
	/* Hardware dependent stuff */
	/*  We take a wild guess that if the video physical address is
	 *  in nubus slot space, that the nubus card is driving video.
	 *  Penguin really ought to tell us whether we are using internal
	 *  video or not.
	 */
	/* Hopefully we only find one of them.  Otherwise our NuBus
           code is really broken :-) */

	while ((ndev = nubus_find_type(NUBUS_CAT_DISPLAY, NUBUS_TYPE_VIDEO, ndev))
		!= NULL)
	{
		if (!(mac_bi_data.videoaddr >= ndev->board->slot_addr
		      && (mac_bi_data.videoaddr <
			  (unsigned long)nubus_slot_addr(ndev->board->slot+1))))
			continue;
		video_is_nubus = 1;
		/* We should probably just use the slot address... */
		video_slot = ndev->board->slot;

		switch(ndev->dr_hw) {
		case NUBUS_DRHW_APPLE_MDC:
			strcpy( fb_info.modename, "Macintosh Display Card" );
			macfb_setpalette = mdc_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			break;
		case NUBUS_DRHW_APPLE_TFB:
			strcpy( fb_info.modename, "Toby" );
			macfb_setpalette = toby_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			break;
		case NUBUS_DRHW_APPLE_JET:
			strcpy(fb_info.modename, "Jet");
			macfb_setpalette = jet_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			break;			
		default:
			strcpy( fb_info.modename, "Generic NuBus" );
			break;
		}
	}

	/* If it's not a NuBus card, it must be internal video */
	/* FIXME: this function is getting way too big.  (this driver
           is too...) */
	if (!video_is_nubus)
		switch( mac_bi_data.id )
		{
			/* These don't have onboard video.  Eventually, we may
			   be able to write separate framebuffer drivers for
			   them (tobyfb.c, hiresfb.c, etc, etc) */
		case MAC_MODEL_II:
		case MAC_MODEL_IIX:
		case MAC_MODEL_IICX:
		case MAC_MODEL_IIFX:
			strcpy( fb_info.modename, "Generic NuBus" );
			break;

			/* Valkyrie Quadras */
		case MAC_MODEL_Q630:
			/* I'm not sure about this one */
		case MAC_MODEL_P588:
			strcpy( fb_info.modename, "Valkyrie built-in" );
			macfb_setpalette = valkyrie_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			valkyrie_cmap_regs = ioremap(DAC_BASE, 0x1000);
			break;

			/* DAFB Quadras */
			/* Note: these first four have the v7 DAFB, which is
			   known to be rather unlike the ones used in the
			   other models */
		case MAC_MODEL_P475:
		case MAC_MODEL_P475F:
		case MAC_MODEL_P575:
		case MAC_MODEL_Q605:
	
		case MAC_MODEL_Q800:
		case MAC_MODEL_Q650:
		case MAC_MODEL_Q610:
		case MAC_MODEL_C650:
		case MAC_MODEL_C610:
		case MAC_MODEL_Q700:
		case MAC_MODEL_Q900:
		case MAC_MODEL_Q950:
			strcpy( fb_info.modename, "DAFB built-in" );
			macfb_setpalette = dafb_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			dafb_cmap_regs = ioremap(DAFB_BASE, 0x1000);
			break;

			/* LC II uses the V8 framebuffer */
		case MAC_MODEL_LCII:
			strcpy( fb_info.modename, "V8 built-in" );
			macfb_setpalette = v8_brazil_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			v8_brazil_cmap_regs = ioremap(DAC_BASE, 0x1000);
			break;
		
			/* IIvi, IIvx use the "Brazil" framebuffer (which is
			   very much like the V8, it seems, and probably uses
			   the same DAC) */
		case MAC_MODEL_IIVI:
		case MAC_MODEL_IIVX:
		case MAC_MODEL_P600:
			strcpy( fb_info.modename, "Brazil built-in" );
			macfb_setpalette = v8_brazil_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			v8_brazil_cmap_regs = ioremap(DAC_BASE, 0x1000);
			break;
		
			/* LC III (and friends) use the Sonora framebuffer */
			/* Incidentally this is also used in the non-AV models
			   of the x100 PowerMacs */
			/* These do in fact seem to use the same DAC interface
			   as the LC II. */
		case MAC_MODEL_LCIII:
		case MAC_MODEL_P520:
		case MAC_MODEL_P550:
		case MAC_MODEL_P460:
			macfb_setpalette = v8_brazil_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			strcpy( fb_info.modename, "Sonora built-in" );
			v8_brazil_cmap_regs = ioremap(DAC_BASE, 0x1000);
			break;

			/* IIci and IIsi use the infamous RBV chip
                           (the IIsi is just a rebadged and crippled
                           IIci in a different case, BTW) */
		case MAC_MODEL_IICI:
		case MAC_MODEL_IISI:
			macfb_setpalette = rbv_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			strcpy( fb_info.modename, "RBV built-in" );
			rbv_cmap_regs = ioremap(DAC_BASE, 0x1000);
			break;

			/* AVs use the Civic framebuffer */
		case MAC_MODEL_Q840:
		case MAC_MODEL_C660:
			macfb_setpalette = civic_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			strcpy( fb_info.modename, "Civic built-in" );
			civic_cmap_regs = ioremap(CIVIC_BASE, 0x1000);
			break;

		
			/* Write a setpalette function for your machine, then
			   you can add something similar here.  These are
			   grouped by classes of video chipsets.  Some of this
			   information is from the VideoToolbox "Bugs" web
			   page at
			   http://rajsky.psych.nyu.edu/Tips/VideoBugs.html */

			/* Assorted weirdos */
			/* We think this may be like the LC II */
		case MAC_MODEL_LC:
			if (vidtest) {
				macfb_setpalette = v8_brazil_setpalette;
				macfb_defined.activate = FB_ACTIVATE_NOW;
				v8_brazil_cmap_regs =
					ioremap(DAC_BASE, 0x1000);
			}
			strcpy( fb_info.modename, "LC built-in" );
			break;
			/* We think this may be like the LC II */
		case MAC_MODEL_CCL:
			if (vidtest) {
				macfb_setpalette = v8_brazil_setpalette;
				macfb_defined.activate = FB_ACTIVATE_NOW;
				v8_brazil_cmap_regs =
					ioremap(DAC_BASE, 0x1000);
			}
			strcpy( fb_info.modename, "Color Classic built-in" );
			break;

			/* And we *do* mean "weirdos" */
		case MAC_MODEL_TV:
			strcpy( fb_info.modename, "Mac TV built-in" );
			break;

			/* These don't have colour, so no need to worry */
		case MAC_MODEL_SE30:
		case MAC_MODEL_CLII:
			strcpy( fb_info.modename, "Monochrome built-in" );
			break;

			/* Powerbooks are particularly difficult.  Many of
			   them have separate framebuffers for external and
			   internal video, which is admittedly pretty cool,
			   but will be a bit of a headache to support here.
			   Also, many of them are grayscale, and we don't
			   really support that. */

		case MAC_MODEL_PB140:
		case MAC_MODEL_PB145:
		case MAC_MODEL_PB170:
			strcpy( fb_info.modename, "DDC built-in" );
			break;

			/* Internal is GSC, External (if present) is ViSC */
		case MAC_MODEL_PB150:	/* no external video */
		case MAC_MODEL_PB160:
		case MAC_MODEL_PB165:
		case MAC_MODEL_PB180:
		case MAC_MODEL_PB210:
		case MAC_MODEL_PB230:
			strcpy( fb_info.modename, "GSC built-in" );
			break;

			/* Internal is TIM, External is ViSC */
		case MAC_MODEL_PB165C:
		case MAC_MODEL_PB180C:
			strcpy( fb_info.modename, "TIM built-in" );
			break;

			/* Internal is CSC, External is Keystone+Ariel. */
		case MAC_MODEL_PB190:	/* external video is optional */
		case MAC_MODEL_PB520:
		case MAC_MODEL_PB250:
		case MAC_MODEL_PB270C:
		case MAC_MODEL_PB280:
		case MAC_MODEL_PB280C:
			macfb_setpalette = csc_setpalette;
			macfb_defined.activate = FB_ACTIVATE_NOW;
			strcpy( fb_info.modename, "CSC built-in" );
			csc_cmap_regs = ioremap(CSC_BASE, 0x1000);
			break;
		
		default:
			strcpy( fb_info.modename, "Unknown/Unsupported built-in" );
			break;
		}
	
	fb_info.changevar  = NULL;
	fb_info.node       = -1;
	fb_info.fbops      = &macfb_ops;
	fb_info.disp       = &disp;
	fb_info.switch_con = &macfb_switch;
	fb_info.updatevar  = &macfb_update_var;
	fb_info.blank      = &macfb_blank;
	fb_info.flags      = FBINFO_FLAG_DEFAULT;
	macfb_set_disp(-1);
	do_install_cmap(0, &fb_info);
	
	if (register_framebuffer(&fb_info) < 0)
		return;

	printk("fb%d: %s frame buffer device\n",
	       GET_FB_IDX(fb_info.node), fb_info.modename);
}

MODULE_LICENSE("GPL");
