/*
 *	HP300 Topcat framebuffer support (derived from macfb of all things)
 *	Phil Blundell <philb@gnu.org> 1998
 * 
 * Should this be moved to drivers/dio/video/ ? -- Peter Maydell
 * No! -- Jes
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/dio.h>
#include <asm/io.h>
#include <asm/blinken.h>
#include <asm/hwtest.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>

static struct display disp;
static struct fb_info fb_info;

unsigned long fb_start, fb_size = 1024*768, fb_line_length = 1024;
unsigned long fb_regs;
unsigned char fb_bitmask;

#define TC_WEN		0x4088
#define TC_REN		0x408c
#define TC_FBEN		0x4090
#define TC_NBLANK	0x4080

/* blitter regs */
#define BUSY		0x4044
#define WMRR		0x40ef
#define SOURCE_X	0x40f2
#define SOURCE_Y	0x40f6
#define DEST_X		0x40fa
#define DEST_Y		0x40fe
#define WHEIGHT		0x4106
#define WWIDTH		0x4102
#define WMOVE		0x409c

static struct fb_var_screeninfo hpfb_defined = {
	0,0,0,0,	/* W,H, W, H (virtual) load xres,xres_virtual*/
	0,0,		/* virtual -> visible no offset */
	0,		/* depth -> load bits_per_pixel */
	0,		/* greyscale ? */
	{0,2,0},	/* R */
	{0,2,0},	/* G */
	{0,2,0},	/* B */
	{0,0,0},	/* transparency */
	0,		/* standard pixel format */
	FB_ACTIVATE_NOW,
	274,195,	/* 14" monitor */
	FB_ACCEL_NONE,
	0L,0L,0L,0L,0L,
	0L,0L,0,	/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0,0,0,0,0,0}
};

struct hpfb_par
{
};

static int currcon = 0;
struct hpfb_par current_par;

static void hpfb_encode_var(struct fb_var_screeninfo *var, 
				struct hpfb_par *par)
{
	int i=0;
	var->xres=1024;
	var->yres=768;
	var->xres_virtual=1024;
	var->yres_virtual=768;
	var->xoffset=0;
	var->yoffset=0;
	var->bits_per_pixel = 1;
	var->grayscale=0;
	var->transp.offset=0;
	var->transp.length=0;
	var->transp.msb_right=0;
	var->nonstd=0;
	var->activate=0;
	var->height= -1;
	var->width= -1;
	var->vmode=FB_VMODE_NONINTERLACED;
	var->pixclock=0;
	var->sync=0;
	var->left_margin=0;
	var->right_margin=0;
	var->upper_margin=0;
	var->lower_margin=0;
	var->hsync_len=0;
	var->vsync_len=0;
	for(i=0;i<ARRAY_SIZE(var->reserved);i++)
		var->reserved[i]=0;
}

static void hpfb_get_par(struct hpfb_par *par)
{
	*par=current_par;
}

static int fb_update_var(int con, struct fb_info *info)
{
	return 0;
}

static int do_fb_set_var(struct fb_var_screeninfo *var, int isactive)
{
	struct hpfb_par par;
	
	hpfb_get_par(&par);
	hpfb_encode_var(var, &par);
	return 0;
}

static int hpfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
	return 0;
}

/*
 * Set the palette.  This may not work on all boards but only experimentation will tell.
 * XXX Doesn't work at all.
 */

static int hpfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
	unsigned int i;
	for (i = 0; i < cmap->len; i++)
	{
		while (in_be16(fb_regs + 0x6002) & 0x4) udelay(1);
		out_be16(fb_regs + 0x60f0, 0);
		out_be16(fb_regs + 0x60b8, cmap->start + i);
		out_be16(fb_regs + 0x60b2, cmap->red[i]);
		out_be16(fb_regs + 0x60b4, cmap->green[i]);
		out_be16(fb_regs + 0x60b6, cmap->blue[i]);
		out_be16(fb_regs + 0x60f0, 0xff);
		udelay(100);
	}
	out_be16(fb_regs + 0x60ba, 0xffff);
	return 0;
}

static int hpfb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	struct hpfb_par par;
	if(con==-1)
	{
		hpfb_get_par(&par);
		hpfb_encode_var(var, &par);
	}
	else
		*var=fb_display[con].var;
	return 0;
}

static int hpfb_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	int err;
	
	if ((err=do_fb_set_var(var, 1)))
		return err;
	return 0;
}

static void hpfb_encode_fix(struct fb_fix_screeninfo *fix, 
				struct hpfb_par *par)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "HP300 Topcat");

	/*
	 * X works, but screen wraps ... 
	 */
	fix->smem_start=fb_start;
	fix->smem_len=fb_size;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	fix->xpanstep=0;
	fix->ypanstep=0;
	fix->ywrapstep=0;
	fix->line_length=fb_line_length;
}

static int hpfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
	struct hpfb_par par;
	hpfb_get_par(&par);
	hpfb_encode_fix(fix, &par);
	return 0;
}

static void topcat_blit(int x0, int y0, int x1, int y1, int w, int h)
{
	while (in_8(fb_regs + BUSY) & fb_bitmask);
	out_8(fb_regs + WMRR, 0x3);
	out_be16(fb_regs + SOURCE_X, x0);
	out_be16(fb_regs + SOURCE_Y, y0);
	out_be16(fb_regs + DEST_X, x1);
	out_be16(fb_regs + DEST_Y, y1);
	out_be16(fb_regs + WHEIGHT, h);
	out_be16(fb_regs + WWIDTH, w);
	out_8(fb_regs + WMOVE, fb_bitmask);
}

static int hpfb_switch(int con, struct fb_info *info)
{
	do_fb_set_var(&fb_display[con].var,1);
	currcon=con;
	return 0;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */

static void hpfb_blank(int blank, struct fb_info *info)
{
	/* Not supported */
}

static void hpfb_set_disp(int con)
{
	struct fb_fix_screeninfo fix;
	struct display *display;
	
	if (con >= 0)
		display = &fb_display[con];
	else
		display = &disp;	/* used during initialization */

	hpfb_get_fix(&fix, con, 0);

	display->screen_base = (char *)fix.smem_start;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 0;
	display->inverse = 0;

#ifdef FBCON_HAS_CFB8
	display->dispsw = &fbcon_cfb8;
#else
	display->dispsw = &fbcon_dummy;
#endif
}

static struct fb_ops hpfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	hpfb_get_fix,
	fb_get_var:	hpfb_get_var,
	fb_set_var:	hpfb_set_var,
	fb_get_cmap:	hpfb_get_cmap,
	fb_set_cmap:	hpfb_set_cmap,
};

#define TOPCAT_FBOMSB	0x5d
#define TOPCAT_FBOLSB	0x5f

int __init hpfb_init_one(unsigned long base)
{
	unsigned long fboff;

	fboff = (in_8(base + TOPCAT_FBOMSB) << 8) | in_8(base + TOPCAT_FBOLSB);

	fb_start = 0xf0000000 | (in_8(base + fboff) << 16);
	fb_regs = base;

#if 0
	/* This is the magic incantation NetBSD uses to make Catseye boards work. */
	out_8(base+0x4800, 0);
	out_8(base+0x4510, 0);
	out_8(base+0x4512, 0);
	out_8(base+0x4514, 0);
	out_8(base+0x4516, 0);
	out_8(base+0x4206, 0x90);
#endif

	/*
	 *	Fill in the available video resolution
	 */
	 
	hpfb_defined.xres = 1024;
	hpfb_defined.yres = 768;
	hpfb_defined.xres_virtual = 1024;
	hpfb_defined.yres_virtual = 768;
	hpfb_defined.bits_per_pixel = 8;

	/* 
	 *	Give the hardware a bit of a prod and work out how many bits per
	 *	pixel are supported.
	 */
	
	out_8(base + TC_WEN, 0xff);
	out_8(base + TC_FBEN, 0xff);
	out_8(fb_start, 0xff);
	fb_bitmask = in_8(fb_start);

	/*
	 *	Enable reading/writing of all the planes.
	 */
	out_8(base + TC_WEN, fb_bitmask);
	out_8(base + TC_REN, fb_bitmask);
	out_8(base + TC_FBEN, fb_bitmask);
	out_8(base + TC_NBLANK, 0x1);

	/*
	 *	Let there be consoles..
	 */
	strcpy(fb_info.modename, "Topcat");
	fb_info.changevar = NULL;
	fb_info.node = -1;
	fb_info.fbops = &hpfb_ops;
	fb_info.disp = &disp;
	fb_info.switch_con = &hpfb_switch;
	fb_info.updatevar = &fb_update_var;
	fb_info.blank = &hpfb_blank;
	fb_info.flags = FBINFO_FLAG_DEFAULT;
	do_fb_set_var(&hpfb_defined, 1);

	hpfb_get_var(&disp.var, -1, &fb_info);
	hpfb_set_disp(-1);

	if (register_framebuffer(&fb_info) < 0)
		return 1;

	return 0;
}

/* 
 * Check that the secondary ID indicates that we have some hope of working with this
 * framebuffer.  The catseye boards are pretty much like topcats and we can muddle through.
 */

#define topcat_sid_ok(x)  (((x) == DIO_ID2_LRCATSEYE) || ((x) == DIO_ID2_HRCCATSEYE)    \
			   || ((x) == DIO_ID2_HRMCATSEYE) || ((x) == DIO_ID2_TOPCAT))

/* 
 * Initialise the framebuffer
 */

int __init hpfb_init(void)
{
	unsigned int sid;

	/* Topcats can be on the internal IO bus or real DIO devices.
	 * The internal variant sits at 0xf0560000; it has primary
	 * and secondary ID registers just like the DIO version.
	 * So we merge the two detection routines.
	 *
	 * Perhaps this #define should be in a global header file:
	 * I believe it's common to all internal fbs, not just topcat.
	 */
#define INTFBADDR 0xf0560000

	if (hwreg_present((void *)INTFBADDR) && (DIO_ID(INTFBADDR) == DIO_ID_FBUFFER)
		&& topcat_sid_ok(sid = DIO_SECID(INTFBADDR)))
	{
		printk("Internal Topcat found (secondary id %02x)\n", sid); 
		hpfb_init_one(INTFBADDR);
	}
	else
	{
		int sc = dio_find(DIO_ID_FBUFFER);
		if (sc)
		{
			unsigned long addr = (unsigned long)dio_scodetoviraddr(sc);
			unsigned int sid = DIO_SECID(addr);

			if (topcat_sid_ok(sid))
			{
				printk("Topcat found at DIO select code %02x "
				       "(secondary id %02x)\n", sc, sid);
				hpfb_init_one(addr);
			}
		}
	}

	return 0;
}

MODULE_LICENSE("GPL");
