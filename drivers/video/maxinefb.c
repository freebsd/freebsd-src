/*
 *      linux/drivers/video/maxinefb.c
 *
 *	DECstation 5000/xx onboard framebuffer support ... derived from:
 *	"HP300 Topcat framebuffer support (derived from macfb of all things)
 *	Phil Blundell <philb@gnu.org> 1998", the original code can be
 *      found in the file hpfb.c in the same directory.
 *
 *      DECstation related code Copyright (C) 1999,2000,2001 by
 *      Michael Engel <engel@unix-ag.org> and
 *      Karsten Merker <merker@linuxtag.org>.
 *      This file is subject to the terms and conditions of the GNU General
 *      Public License.  See the file COPYING in the main directory of this
 *      archive for more details.
 *
 */

/*
 * Changes:
 * 2001-01-27  removed debugging and testing code, fixed fb_ops
 *	       initialization which had caused a crash before,
 *	       general cleanup, first official release (KM)
 *
 * 2003-03-08  Thiemo Seufer <seufer@csv.ica.uni-stuttgart.de>
 *	       Moved ims332 support in its own file. Code cleanup.
 *
 * 2003-09-08  Thiemo Seufer <seufer@csv.ica.uni-stuttgart.de>
 *	       First attempt of mono and hw cursor support.
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
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/console.h>

/*
 * bootinfo.h is needed for checking whether we are really running
 * on a maxine.
 */
#include <asm/bootinfo.h>
#include <asm/addrspace.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>

#include "ims332.h"

/* Version information */
#define DRIVER_VERSION "0.02"
#define DRIVER_AUTHOR "Karsten Merker <merker@linuxtag.org>"
#define DRIVER_DESCRIPTION "Maxine Framebuffer Driver"
#define DRIVER_DESCR "maxinefb"

/* Prototypes */
static int maxinefb_set_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *info);

/* Hardware cursor */
struct maxine_cursor {
	struct timer_list timer;
	int enable;
	int on;
	int vbl_cnt;
	int blink_rate;
	u16 x, y, width, height;
};

#define CURSOR_TIMER_FREQ	(HZ / 50)
#define CURSOR_BLINK_RATE	(20)
#define CURSOR_DRAW_DELAY	(2)

static struct maxinefb_info {
	struct fb_info info;
	struct display disp;
	struct display_switch dispsw;
	struct ims332_regs *ims332;
	unsigned long fb_start;
	u32 fb_size;
	struct maxine_cursor cursor;
} *my_info;

static struct maxinefb_par {
} current_par;

static int currcon = -1;

static void maxine_set_cursor(struct maxinefb_info *ip, int on)
{
	struct maxine_cursor *c = &ip->cursor;

	if (on)
		ims332_position_cursor(ip->ims332, c->x, c->y);

	ims332_enable_cursor(ip->ims332, on);
}

static void maxinefbcon_cursor(struct display *disp, int mode, int x, int y)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)disp->fb_info;
	struct maxine_cursor *c = &ip->cursor;

	x *= fontwidth(disp);
	y *= fontheight(disp);

	if (c->x == x && c->y == y && (mode == CM_ERASE) == !c->enable)
		return;

	c->enable = 0;
	if (c->on)
		maxine_set_cursor(ip, 0);
	c->x = x - disp->var.xoffset;
	c->y = y - disp->var.yoffset;

	switch (mode) {
		case CM_ERASE:
			c->on = 0;
			break;
		case CM_DRAW:
		case CM_MOVE:
			if (c->on)
				maxine_set_cursor(ip, c->on);
			else
				c->vbl_cnt = CURSOR_DRAW_DELAY;
			c->enable = 1;
			break;
	}
}

static int maxinefbcon_set_font(struct display *disp, int width, int height)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)disp->fb_info;
	struct maxine_cursor *c = &ip->cursor;
	u8 fgc = ~attr_bgcol_ec(disp, disp->conp);

	if (width > 64 || height > 64 || width < 0 || height < 0)
		return -EINVAL;

	c->height = height;
	c->width = width;

	ims332_set_font(ip->ims332, fgc, width, height);

	return 1;
}

static void maxine_cursor_timer_handler(unsigned long data)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)data;
	struct maxine_cursor *c = &ip->cursor;

	if (!c->enable)
		goto out;

	if (c->vbl_cnt && --c->vbl_cnt == 0) {
		c->on ^= 1;
		maxine_set_cursor(ip, c->on);
		c->vbl_cnt = c->blink_rate;
	}

out:
	c->timer.expires = jiffies + CURSOR_TIMER_FREQ;
	add_timer(&c->timer);
}

static void __init maxine_cursor_init(struct maxinefb_info *ip)
{
	struct maxine_cursor *c = &ip->cursor;

	c->enable = 1;
	c->on = 1;
	c->x = c->y = 0;
	c->width = c->height = 0;
	c->vbl_cnt = CURSOR_DRAW_DELAY;
	c->blink_rate = CURSOR_BLINK_RATE;

	init_timer(&c->timer);
	c->timer.data = (unsigned long)ip;
	c->timer.function = maxine_cursor_timer_handler;
	mod_timer(&c->timer, jiffies + CURSOR_TIMER_FREQ);
}

static void __exit maxine_cursor_exit(struct maxinefb_info *ip)
{
	struct maxine_cursor *c = &ip->cursor;

	del_timer_sync(&c->timer);
}

#ifdef FBCON_HAS_MFB
extern void fbcon_mfb_clear_margins(struct vc_data *conp, struct display *p,
				    int bottom_only);

static struct display_switch maxine_switch1 = {
	.setup = fbcon_mfb_setup,
	.bmove = fbcon_mfb_bmove,
	.clear = fbcon_mfb_clear,
	.putc = fbcon_mfb_putc,
	.putcs = fbcon_mfb_putcs,
	.revc = fbcon_mfb_revc,
	.cursor = maxinefbcon_cursor,
	.set_font = maxinefbcon_set_font,
	.clear_margins = fbcon_mfb_clear_margins,
	.fontwidthmask = FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB2
static struct display_switch maxine_switch2 = {
	.setup = fbcon_cfb2_setup,
	.bmove = fbcon_cfb2_bmove,
	.clear = fbcon_cfb2_clear,
	.putc = fbcon_cfb2_putc,
	.putcs = fbcon_cfb2_putcs,
	.revc = fbcon_cfb2_revc,
	.cursor = maxinefbcon_cursor,
	.set_font = maxinefbcon_set_font,
	.clear_margins = fbcon_cfb8_clear_margins, /* sigh.. */
	.fontwidthmask = FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB4
static struct display_switch maxine_switch4 = {
	.setup = fbcon_cfb4_setup,
	.bmove = fbcon_cfb4_bmove,
	.clear = fbcon_cfb4_clear,
	.putc = fbcon_cfb4_putc,
	.putcs = fbcon_cfb4_putcs,
	.revc = fbcon_cfb4_revc,
	.cursor = maxinefbcon_cursor,
	.set_font = maxinefbcon_set_font,
	.clear_margins = fbcon_cfb8_clear_margins, /* sigh.. */
	.fontwidthmask = FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
static struct display_switch maxine_switch8 = {
	.setup = fbcon_cfb8_setup,
	.bmove = fbcon_cfb8_bmove,
	.clear = fbcon_cfb8_clear,
	.putc = fbcon_cfb8_putc,
	.putcs = fbcon_cfb8_putcs,
	.revc = fbcon_cfb8_revc,
	.cursor = maxinefbcon_cursor,
	.set_font = maxinefbcon_set_font,
	.clear_margins = fbcon_cfb8_clear_margins,
	.fontwidthmask = FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};

static void maxinefb_get_par(struct maxinefb_par *par)
{
	*par = current_par;
}

static int maxinefb_get_fix(struct fb_fix_screeninfo *fix, int con,
			    struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);
	struct fb_var_screeninfo *var = &disp->var;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, DRIVER_DESCR);
	fix->smem_start = ip->fb_start;
	fix->smem_len = ip->fb_size;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->ypanstep = 1;
	fix->ywrapstep = 1;
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1: fix->line_length = var->xres / 8; break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2: fix->line_length = var->xres / 4; break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4: fix->line_length = var->xres / 2; break;
#endif
	case 8:
	default:
		fix->line_length = var->xres;
		var->bits_per_pixel = 8;
		break;
	}
	fix->accel = FB_ACCEL_NONE;

	return 0;
}

static int maxinefb_set_dispsw(struct display *disp, struct maxinefb_info *ip)
{
	if (disp->conp && disp->conp->vc_sw && disp->conp->vc_sw->con_cursor)
		disp->conp->vc_sw->con_cursor(disp->conp, CM_ERASE);

	switch (disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1: ip->dispsw = maxine_switch1; break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2: ip->dispsw = maxine_switch2; break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4: ip->dispsw = maxine_switch4; break;
#endif
	case 8:
	default:
		ip->dispsw = maxine_switch8;
		disp->var.bits_per_pixel = 8;
		break;
	}

	ims332_set_color_depth(ip->ims332, disp->var.bits_per_pixel);
	disp->dispsw = &ip->dispsw;
	disp->dispsw_data = 0;
	return 0;
}

static void maxinefb_set_disp(struct display *disp, int con,
			      struct maxinefb_info *ip)
{
	struct fb_fix_screeninfo fix;

	disp->fb_info = &ip->info;
	maxinefb_set_var(&disp->var, con, &ip->info);
	maxinefb_set_dispsw(disp, ip);

	maxinefb_get_fix(&fix, con, &ip->info);
	disp->screen_base = (u8 *)fix.smem_start;
	disp->visual = fix.visual;
	disp->type = fix.type;
	disp->type_aux = fix.type_aux;
	disp->ypanstep = fix.ypanstep;
	disp->ywrapstep = fix.ywrapstep;
	disp->line_length = fix.line_length;
	disp->next_line = fix.line_length;
	disp->can_soft_blank = 1;
	disp->inverse = 0;
	disp->scrollmode = SCROLL_YREDRAW;

	maxinefbcon_set_font(disp, fontwidth(disp), fontheight(disp));
}

static int getcolreg(u32 reg, u32 *red, u32 *green, u32 *blue, u32 *transp,
		     struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;

	u8 r;
	u8 g;
	u8 b;

	if (reg > 255)
		return 1;

	/*
	 * Cmap fields are 16 bits wide, but the hardware colormap
	 * has only 8 bits.
	 */
	ims332_read_cmap(ip->ims332, reg, &r, &g, &b);
	*red = r << 8;
	*green = g << 8;
	*blue = b << 8;
	*transp = 0;

	return 0;
}

static int setcolreg(u32 reg, u32 red, u32 green, u32 blue, u32 transp,
		     struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;

	if (reg > 255)
		return 1;

	/*
	 * Cmap fields are 16 bits wide, but the hardware colormap
	 * has only 8 bits.
	 */
	red = (red >> 8) & 0xff;
	green = (green >> 8) & 0xff;
	blue = (blue >> 8) & 0xff;

	ims332_write_cmap(ip->ims332, reg, red, green, blue);

	return 0;
}

#define CMAP_LEN(disp) ((disp->visual == FB_VISUAL_PSEUDOCOLOR) \
			? (1 << disp->var.bits_per_pixel) : 16)

static void do_install_cmap(int con, struct maxinefb_info *ip)
{
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);
	int len = CMAP_LEN(disp);

	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, setcolreg, &ip->info);
	else
		fb_set_cmap(fb_default_cmap(len), 1, setcolreg, &ip->info);
}

static void do_save_cmap(int con, struct maxinefb_info *ip)
{
	if (con != currcon)
		return;
	fb_get_cmap(&fb_display[con].cmap, 1, getcolreg, &ip->info);
}

static int maxinefb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);
	int len = CMAP_LEN(disp);

	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, getcolreg, info);

	if (disp->cmap.len) /* non default colormap? */
		fb_copy_cmap(&disp->cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(len), cmap, kspc ? 0 : 2);

	return 0;
}

static int maxinefb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);
	int len = CMAP_LEN(disp);
	int err;

	/* No colormap allocated? */
	if ((err = fb_alloc_cmap(&disp->cmap, len, 0)))
		return err;

	if (con == currcon) /* current console? */
		return fb_set_cmap(cmap, kspc, setcolreg, info);

	fb_copy_cmap(cmap, &disp->cmap, kspc ? 0 : 1);

	return 0;
}

static int maxinefb_ioctl(struct inode *inode, struct file *file, u32 cmd,
			  unsigned long arg, int con, struct fb_info *info)
{
	/* TODO: Not yet implemented */
	return -ENOIOCTLCMD;
}

static int maxinefb_switch(int con, struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *old = (currcon < 0) ? &ip->disp : (fb_display + currcon);
	struct display *new = (con < 0) ? &ip->disp : (fb_display + con);

	do_save_cmap(currcon, ip);
	if (old->conp && old->conp->vc_sw && old->conp->vc_sw->con_cursor)
		old->conp->vc_sw->con_cursor(old->conp, CM_ERASE);

	/* Set the current console. */
	currcon = con;
	maxinefb_set_disp(new, con, ip);

	return 0;
}

static int maxinefb_encode_var(struct fb_var_screeninfo *var,
			       struct maxinefb_par *par)
{
	var->xres = 1024;
	var->yres = 768;
	var->xres_virtual = 1024;
	var->yres_virtual = 1024;
	var->xoffset = 0;
	var->yoffset = 0;
	var->grayscale = 0;
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1:
		var->red.offset = 0;
		var->red.length = 1;
		var->red.msb_right = 0;
		var->green.offset = 0;
		var->green.length = 1;
		var->green.msb_right = 0;
		var->blue.offset = 0;
		var->blue.length = 1;
		var->blue.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 0;
		var->transp.msb_right = 0;
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		var->red.offset = 0;
		var->red.length = 2;
		var->red.msb_right = 0;
		var->green.offset = 0;
		var->green.length = 2;
		var->green.msb_right = 0;
		var->blue.offset = 0;
		var->blue.length = 2;
		var->blue.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 0;
		var->transp.msb_right = 0;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		var->red.offset = 0;
		var->red.length = 4;
		var->red.msb_right = 0;
		var->green.offset = 0;
		var->green.length = 4;
		var->green.msb_right = 0;
		var->blue.offset = 0;
		var->blue.length = 4;
		var->blue.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 0;
		var->transp.msb_right = 0;
		break;
#endif
	case 8:
	default:
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
		var->bits_per_pixel = 8;
		break;
	}
	var->nonstd = 0;
	var->activate &= ~FB_ACTIVATE_MASK & FB_ACTIVATE_NOW;
	var->accel_flags = 0;
	var->sync = FB_SYNC_ON_GREEN;
	var->vmode &= ~FB_VMODE_MASK & FB_VMODE_NONINTERLACED;

	return 0;
}

static int maxinefb_get_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *info)
{
	struct maxinefb_par par;
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);

	if (con < 0) {
		maxinefb_get_par(&par);
		memset(var, 0, sizeof(struct fb_var_screeninfo));
		return maxinefb_encode_var(var, &par);
	} else
		*var = disp->var;

	return 0;
}

static int maxinefb_set_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *info)
{
	struct maxinefb_par par;
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);
	int ret;

	maxinefb_get_par(&par);
	ret = maxinefb_encode_var(var, &par);
	if (ret)
		goto out;
	disp->var = *var;

	/* Set default colormap. */
	ret = fb_alloc_cmap(&disp->cmap, 0, 0);
	if (ret)
		goto out;
	do_install_cmap(con, ip);

out:
	return ret;
}

static int maxinefb_fb_update_var(int con, struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;
	struct display *disp = (con < 0) ? &ip->disp : (fb_display + con);

	if (con == currcon)
		maxinefbcon_cursor(disp, CM_ERASE, ip->cursor.x,
				   ip->cursor.y);

	return 0;
}

/* 0 unblanks, anything else blanks. */

static void maxinefb_blank(int blank, struct fb_info *info)
{
	struct maxinefb_info *ip = (struct maxinefb_info *)info;

	ims332_blank_screen(ip->ims332, !!blank);
}

static struct fb_ops maxinefb_ops = {
	.owner = THIS_MODULE,
	.fb_get_fix = maxinefb_get_fix,
	.fb_get_var = maxinefb_get_var,
	.fb_set_var = maxinefb_set_var,
	.fb_get_cmap = maxinefb_get_cmap,
	.fb_set_cmap = maxinefb_set_cmap,
	.fb_ioctl = maxinefb_ioctl
};

int __init maxinefb_init(void)
{
	/* Validate we're on the proper machine type. */
	if (mips_machtype != MACH_DS5000_XX)
		return -ENXIO;

	my_info = (struct maxinefb_info *)kmalloc(sizeof(struct maxinefb_info), GFP_ATOMIC);
	if (!my_info) {
		printk(KERN_ERR DRIVER_DESCR ": can't alloc maxinefb_info\n");
		return -ENOMEM;
	}
	memset(my_info, 0, sizeof(struct maxinefb_info));

	/*
	 * Let there be consoles..
	 */
	strcpy(my_info->info.modename, DRIVER_DESCRIPTION);
	my_info->info.node = -1;
	my_info->info.flags = FBINFO_FLAG_DEFAULT;
	my_info->info.fbops = &maxinefb_ops;
	my_info->info.disp = &my_info->disp;
	my_info->info.changevar = NULL;
	my_info->info.switch_con = &maxinefb_switch;
	my_info->info.updatevar = &maxinefb_fb_update_var;
	my_info->info.blank = &maxinefb_blank;

	/* Initialize IMS G332 video controller. */
	my_info->ims332 = (struct ims332_regs *)KSEG1ADDR(0x1c140000);
	ims332_bootstrap(my_info->ims332);

	/* Framebuffer memory, default resolution is 1024x768x8. */
	my_info->fb_start = KSEG1ADDR(0x0a000000);
	my_info->fb_size = 1024 * 1024;
	memset((void *) my_info->fb_start, 0, my_info->fb_size);

	maxine_cursor_init(my_info);
	maxinefb_set_disp(&my_info->disp, currcon, my_info);

	if (register_framebuffer(&my_info->info) < 0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s\n", GET_FB_IDX(my_info->info.node),
	       my_info->info.modename);

	return 0;
}

static void __exit maxinefb_exit(void)
{
	unregister_framebuffer(&my_info->info);
	maxine_cursor_exit(my_info);
	kfree(my_info);
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
#ifdef MODULE
module_init(maxinefb_init);
module_exit(maxinefb_exit);
#endif
