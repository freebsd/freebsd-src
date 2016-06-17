/*
 *      linux/drivers/video/pmagb-b-fb.c
 *
 *	PMAGB-B TurboChannel framebuffer card support ... derived from:
 *	"HP300 Topcat framebuffer support (derived from macfb of all things)
 *	Phil Blundell <philb@gnu.org> 1998", the original code can be
 *      found in the file hpfb.c in the same directory.
 *
 *      DECstation related code Copyright (C) 1999, 2000, 2001 by
 *      Michael Engel <engel@unix-ag.org>,
 *      Karsten Merker <merker@linuxtag.org> and 
 *	Harald Koerfgen.
 *      This file is subject to the terms and conditions of the GNU General
 *      Public License.  See the file COPYING in the main directory of this
 *      archive for more details.
 *
 */

/*
 *      We currently only support the PMAGB-B in high resolution mode
 *      as I know of no way to detect low resolution mode set via jumper.
 *      KM, 2001/01/07
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <asm/bootinfo.h>
#include <asm/dec/machtype.h>
#include <asm/dec/tc.h>
#include "pmagb-b-fb.h"

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>

#define arraysize(x)    (sizeof(x)/sizeof(*(x)))

struct pmagb_b_ramdac_regs {
	unsigned char addr_low;
	unsigned char pad0[3];
	unsigned char addr_hi;
	unsigned char pad1[3];
	unsigned char data;
	unsigned char pad2[3];
	unsigned char cmap;
};

struct pmagb_b_my_fb_info {
	struct fb_info info;
	struct pmagb_b_ramdac_regs *bt459_regs;
	unsigned long pmagbb_fb_start;
	unsigned long pmagbb_fb_size;
	unsigned long pmagbb_fb_line_length;
};

static struct display disp;
/*
 * Max 3 TURBOchannel slots -> max 3 PMAGB-B :)
 */
static struct pmagb_b_my_fb_info pmagbb_fb_info[3];

static struct fb_var_screeninfo pmagbbfb_defined = {
	0, 0, 0, 0,		/* W,H, W, H (virtual) load xres,xres_virtual */
	0, 0,			/* virtual -> visible no offset */
	0,			/* depth -> load bits_per_pixel */
	0,			/* greyscale ? */
	{0, 0, 0},		/* R */
	{0, 0, 0},		/* G */
	{0, 0, 0},		/* B */
	{0, 0, 0},		/* transparency */
	0,			/* standard pixel format */
	FB_ACTIVATE_NOW,
	274, 195,		/* 14" monitor */
	FB_ACCEL_NONE,
	0L, 0L, 0L, 0L, 0L,
	0L, 0L, 0,		/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0, 0, 0, 0, 0, 0}
};

struct pmagbbfb_par {
};

static int currcon = 0;
static struct pmagbbfb_par current_par;

static void pmagbbfb_encode_var(struct fb_var_screeninfo *var,
				struct pmagbbfb_par *par)
{
	int i = 0;
	var->xres = 1280;
	var->yres = 1024;
	var->xres_virtual = 1280;
	var->yres_virtual = 1024;
	var->xoffset = 0;
	var->yoffset = 0;
	var->bits_per_pixel = 8;
	var->grayscale = 0;
	var->red.offset = 0;
	var->red.length = 8;
	var->red.msb_right = 0;
	var->green.offset = 0;
	var->green.length = 8;
	var->green.msb_right = 0;
	var->blue.offset = 0;
	var->blue.length = 8;
	var->blue.msb_right = 0;
	var->transp.offset = 0;
	var->transp.length = 0;
	var->transp.msb_right = 0;
	var->nonstd = 0;
	var->activate = 1;
	var->height = -1;
	var->width = -1;
	var->vmode = FB_VMODE_NONINTERLACED;
	var->pixclock = 0;
	var->sync = 0;
	var->left_margin = 0;
	var->right_margin = 0;
	var->upper_margin = 0;
	var->lower_margin = 0;
	var->hsync_len = 0;
	var->vsync_len = 0;
	for (i = 0; i < arraysize(var->reserved); i++)
		var->reserved[i] = 0;
}

static void pmagbbfb_get_par(struct pmagbbfb_par *par)
{
	*par = current_par;
}

static int pmagbb_fb_update_var(int con, struct fb_info *info)
{
	return 0;
}

static int pmagbb_do_fb_set_var(struct fb_var_screeninfo *var,
				int isactive)
{
	struct pmagbbfb_par par;

	pmagbbfb_get_par(&par);
	pmagbbfb_encode_var(var, &par);
	return 0;
}

/*
 * Turn hardware cursor off
 */
void pmagbbfb_erase_cursor(struct pmagb_b_my_fb_info *info)
{
	info->bt459_regs->addr_low = 0;
	info->bt459_regs->addr_hi = 3;
	info->bt459_regs->data = 0;
}

/*
 * Write to a Bt459 color map register
 */
void pmagb_b_bt459_write_colormap(struct pmagb_b_my_fb_info *info,
				  int reg, __u8 red, __u8 green, __u8 blue)
{
	info->bt459_regs->addr_low = (__u8) reg;
	info->bt459_regs->addr_hi = 0;
	info->bt459_regs->cmap = red;
	info->bt459_regs->cmap = green;
	info->bt459_regs->cmap = blue;
}

/*
 * Get the palette
 */

static int pmagbbfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info)
{
	unsigned int i;
	unsigned int length;

	if (((cmap->start) + (cmap->len)) >= 256) {
		length = 256 - (cmap->start);
	} else {
		length = cmap->len;
	}
	for (i = 0; i < length; i++) {
		/*
		 * TODO
		 */
	}
	return 0;
}

/*
 * Set the palette. 
 */
static int pmagbbfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info)
{
	unsigned int i;
	__u8 cmap_red, cmap_green, cmap_blue;
	unsigned int length;

	if (((cmap->start) + (cmap->len)) >= 256)
		length = 256 - (cmap->start);
	else
		length = cmap->len;

	for (i = 0; i < length; i++) {
		cmap_red = ((cmap->red[i]) >> 8);	/* The cmap fields are 16 bits    */
		cmap_green = ((cmap->green[i]) >> 8);	/* wide, but the harware colormap */
		cmap_blue = ((cmap->blue[i]) >> 8);	/* registers are only 8 bits wide */

		pmagb_b_bt459_write_colormap((struct pmagb_b_my_fb_info *)
					     info, cmap->start + i,
					     cmap_red, cmap_green,
					     cmap_blue);
	}
	return 0;
}

static int pmagbbfb_get_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *info)
{
	struct pmagbbfb_par par;
	if (con == -1) {
		pmagbbfb_get_par(&par);
		pmagbbfb_encode_var(var, &par);
	} else
		*var = fb_display[con].var;
	return 0;
}


static int pmagbbfb_set_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *info)
{
	int err;

	if ((err = pmagbb_do_fb_set_var(var, 1)))
		return err;
	return 0;
}

static void pmagbbfb_encode_fix(struct fb_fix_screeninfo *fix,
				struct pmagbbfb_par *par,
				struct pmagb_b_my_fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "PMAGB-BA");

	fix->smem_start = info->pmagbb_fb_start;
	fix->smem_len = info->pmagbb_fb_size;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->ywrapstep = 0;
	fix->line_length = info->pmagbb_fb_line_length;
}

static int pmagbbfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			    struct fb_info *info)
{
	struct pmagbbfb_par par;

	pmagbbfb_get_par(&par);
	pmagbbfb_encode_fix(fix, &par, (struct pmagb_b_my_fb_info *) info);

	return 0;
}


static int pmagbbfb_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg, int con,
			  struct fb_info *info)
{
	return -EINVAL;
}

static int pmagbbfb_switch(int con, struct fb_info *info)
{
	pmagbb_do_fb_set_var(&fb_display[con].var, 1);
	currcon = con;

	return 0;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */

static void pmagbbfb_blank(int blank, struct fb_info *info)
{
	/* Not supported */
}

static int pmagbbfb_open(struct fb_info *info, int user)
{
	/*
	 * Nothing, only a usage count for the moment
	 */
	MOD_INC_USE_COUNT;
	return (0);
}

static void pmagbbfb_set_disp(int con, struct pmagb_b_my_fb_info *info)
{
	struct fb_fix_screeninfo fix;
	struct display *display;

	if (con >= 0)
		display = &fb_display[con];
	else
		display = &disp;	/* used during initialization */

	pmagbbfb_get_fix(&fix, con, (struct fb_info *) info);

	display->screen_base = (char *) fix.smem_start;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 0;
	display->inverse = 0;
	display->scrollmode = SCROLL_YREDRAW;
	display->dispsw = &fbcon_cfb8;
}

static int pmagbbfb_release(struct fb_info *info, int user)
{
	MOD_DEC_USE_COUNT;
	return (0);
}

static struct fb_ops pmagbbfb_ops = {
	owner:THIS_MODULE,
	fb_open:pmagbbfb_open,
	fb_release:pmagbbfb_release,
	fb_get_fix:pmagbbfb_get_fix,
	fb_get_var:pmagbbfb_get_var,
	fb_set_var:pmagbbfb_set_var,
	fb_get_cmap:pmagbbfb_get_cmap,
	fb_set_cmap:pmagbbfb_set_cmap,
	fb_ioctl:pmagbbfb_ioctl,
	fb_mmap:0,
	fb_rasterimg:0
};

int __init pmagbbfb_init_one(int slot)
{
	unsigned long base_addr = get_tc_base_addr(slot);
	struct pmagb_b_my_fb_info *ip =
	    (struct pmagb_b_my_fb_info *) &pmagbb_fb_info[slot];

	printk("PMAGB-BA framebuffer in slot %d\n", slot);

	/*
	 * Framebuffer display memory base address and friends
	 */
	ip->bt459_regs =
	    (struct pmagb_b_ramdac_regs *) (base_addr +
					    PMAGB_B_BT459_OFFSET);
	ip->pmagbb_fb_start = base_addr + PMAGB_B_ONBOARD_FBMEM_OFFSET;
	ip->pmagbb_fb_size = 1280 * 1024;
	ip->pmagbb_fb_line_length = 1280;

	/*
	 * Configure the Bt459 RAM DAC
	 */
	pmagbbfb_erase_cursor(ip);

	/*
	 *      Fill in the available video resolution
	 */

	pmagbbfb_defined.xres = 1280;
	pmagbbfb_defined.yres = 1024;
	pmagbbfb_defined.xres_virtual = 1280;
	pmagbbfb_defined.yres_virtual = 1024;
	pmagbbfb_defined.bits_per_pixel = 8;

	/*
	 *      Let there be consoles..
	 */
	strcpy(ip->info.modename, "PMAGB-BA");
	ip->info.changevar = NULL;
	ip->info.node = -1;
	ip->info.fbops = &pmagbbfb_ops;
	ip->info.disp = &disp;
	ip->info.switch_con = &pmagbbfb_switch;
	ip->info.updatevar = &pmagbb_fb_update_var;
	ip->info.blank = &pmagbbfb_blank;
	ip->info.flags = FBINFO_FLAG_DEFAULT;

	pmagbb_do_fb_set_var(&pmagbbfb_defined, 1);
	pmagbbfb_get_var(&disp.var, -1, (struct fb_info *) ip);
	pmagbbfb_set_disp(-1, ip);

	if (register_framebuffer((struct fb_info *) ip) < 0)
		return 1;

	return 0;
}

/* 
 * Initialise the framebuffer
 */

int __init pmagbbfb_init(void)
{
	int sid;
	int found = 0;

	if (TURBOCHANNEL) {
		while ((sid = search_tc_card("PMAGB-BA")) >= 0) {
			found = 1;
			claim_tc_card(sid);
			pmagbbfb_init_one(sid);
		}
		return found ? 0 : -ENODEV;
	} else {
		return -ENODEV;
	}
}

MODULE_LICENSE("GPL");
