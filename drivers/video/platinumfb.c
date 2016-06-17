/*
 *  platinumfb.c -- frame buffer device for the PowerMac 'platinum' display
 *
 *  Copyright (C) 1998 Franz Sirl
 *
 *  Frame buffer structure from:
 *    drivers/video/controlfb.c -- frame buffer device for
 *    Apple 'control' display chip.
 *    Copyright (C) 1998 Dan Jacobowitz
 *
 *  Hardware information from:
 *    platinum.c: Console support for PowerMac "platinum" display adaptor.
 *    Copyright (C) 1996 Paul Mackerras and Mark Abene
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/config.h>
#include <linux/module.h>
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
#include <linux/selection.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/nvram.h>
#ifdef CONFIG_FB_COMPAT_XPMAC
#include <asm/vc_ioctl.h>
#endif
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pgtable.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb32.h>
#include <video/macmodes.h>

#include "platinumfb.h"

static char fontname[40] __initdata = { 0 };

static int currcon = 0;

static int default_vmode = VMODE_NVRAM;
static int default_cmode = CMODE_NVRAM;

struct fb_par_platinum {
	int	vmode, cmode;
	int	xres, yres;
	int	vxres, vyres;
	int	xoffset, yoffset;
};

struct fb_info_platinum {
	struct fb_info			fb_info;
	struct display			disp;
	struct fb_par_platinum		default_par;
	struct fb_par_platinum		current_par;

	struct {
		__u8 red, green, blue;
	}				palette[256];
	
	volatile struct cmap_regs	*cmap_regs;
	unsigned long			cmap_regs_phys;
	
	volatile struct platinum_regs	*platinum_regs;
	unsigned long			platinum_regs_phys;
	
	__u8				*frame_buffer;
	volatile __u8			*base_frame_buffer;
	unsigned long			frame_buffer_phys;
	
	unsigned long			total_vram;
	int				clktype;
	int				dactype;

	union {
#ifdef FBCON_HAS_CFB16
		u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB32
		u32 cfb32[16];
#endif
	} fbcon_cmap;
};

/*
 * Frame buffer device API
 */

static int platinum_get_fix(struct fb_fix_screeninfo *fix, int con,
			    struct fb_info *fb);
static int platinum_get_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *fb);
static int platinum_set_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *fb);
static int platinum_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info);
static int platinum_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info);


/*
 * Interface to the low level console driver
 */

static int platinum_switch(int con, struct fb_info *fb);
static int platinum_updatevar(int con, struct fb_info *fb);
static void platinum_blank(int blank, struct fb_info *fb);


/*
 * internal functions
 */

static void platinum_of_init(struct device_node *dp);
static inline int platinum_vram_reqd(const struct fb_info_platinum* info,
					int video_mode,
					int color_mode);
static int read_platinum_sense(struct fb_info_platinum *info);
static void set_platinum_clock(struct fb_info_platinum *info);
static void platinum_set_par(const struct fb_par_platinum *par, struct fb_info_platinum *info);
static int platinum_par_to_var(struct fb_var_screeninfo *var,
			       const struct fb_par_platinum *par,
			       const struct fb_info_platinum *info);
static int platinum_var_to_par(const struct fb_var_screeninfo *var,
			       struct fb_par_platinum *par,
			       const struct fb_info_platinum *info);
static int platinum_encode_fix(struct fb_fix_screeninfo *fix,
			       const struct fb_par_platinum *par,
			       const struct fb_info_platinum *info);
static void platinum_set_dispsw(struct display *disp,
				struct fb_info_platinum *info, int cmode,
				int accel);
static int platinum_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			      u_int *transp, struct fb_info *fb);
static int platinum_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			      u_int transp, struct fb_info *fb);
static void do_install_cmap(int con, struct fb_info *info);


/*
 * Interface used by the world
 */

int platinum_init(void);
int platinum_setup(char*);

static struct fb_ops platinumfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	platinum_get_fix,
	fb_get_var:	platinum_get_var,
	fb_set_var:	platinum_set_var,
	fb_get_cmap:	platinum_get_cmap,
	fb_set_cmap:	platinum_set_cmap,
};

static int platinum_get_fix(struct fb_fix_screeninfo *fix, int con,
			    struct fb_info *fb)
{
	const struct fb_info_platinum *info = (struct fb_info_platinum *)fb;
	struct fb_par_platinum par;

	if (con == -1)
		par = info->default_par;
	else
		platinum_var_to_par(&fb_display[con].var, &par, info);

	platinum_encode_fix(fix, &par, info);
	return 0;
}

static int platinum_get_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *fb)
{
	const struct fb_info_platinum *info = (struct fb_info_platinum *)fb;

	if (con == -1)
		platinum_par_to_var(var, &info->default_par, info);
	else
		*var = fb_display[con].var;

	return 0;
}

static void platinum_set_dispsw(struct display *disp,
				struct fb_info_platinum *info, int cmode,
				int accel)
{
	switch(cmode) {
#ifdef FBCON_HAS_CFB8
	    case CMODE_8:
		disp->dispsw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	    case CMODE_16:
		disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data = info->fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	    case CMODE_32:
		disp->dispsw = &fbcon_cfb32;
		disp->dispsw_data = info->fbcon_cmap.cfb32;
		break;
#endif
	    default:
		disp->dispsw = &fbcon_dummy;
		break;
	}
}

static int platinum_set_var(struct fb_var_screeninfo *var, int con,
			    struct fb_info *fb)
{
	struct fb_info_platinum *info = (struct fb_info_platinum *) fb;
	struct fb_par_platinum par;
	struct display *display;
	int oldxres, oldyres, oldvxres, oldvyres, oldbpp, err;
	int activate = var->activate;
	struct platinum_regvals *init;

	display = (con >= 0) ? &fb_display[con] : fb->disp;

	if((err = platinum_var_to_par(var, &par, info))) {
		printk(KERN_ERR "platinum_set_var: error calling platinum_var_to_par: %d.\n", err);
		return err;
	}
	
	platinum_par_to_var(var, &par, info);

	if ((activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW) {
		printk(KERN_INFO "platinum_set_var: Not activating.\n");
		return 0;
	}

	init = platinum_reg_init[par.vmode-1];

	oldxres = display->var.xres;
	oldyres = display->var.yres;
	oldvxres = display->var.xres_virtual;
	oldvyres = display->var.yres_virtual;
	oldbpp = display->var.bits_per_pixel;
	display->var = *var;

	if (oldxres != var->xres || oldyres != var->yres ||
	    oldvxres != var->xres_virtual || oldyres != var->yres_virtual ||
	    oldbpp != var->bits_per_pixel) {
	    struct fb_fix_screeninfo fix;

	    platinum_encode_fix(&fix, &par, info);
	    display->screen_base = (char *) info->frame_buffer + init->fb_offset + 0x20;
	    display->visual = fix.visual;
	    display->type = fix.type;
	    display->type_aux = fix.type_aux;
	    display->ypanstep = fix.ypanstep;
	    display->ywrapstep = fix.ywrapstep;
	    display->line_length = fix.line_length;
	    display->can_soft_blank = 1;
	    display->inverse = 0;
	    platinum_set_dispsw(display, info, par.cmode, 0);
	    display->scrollmode = SCROLL_YREDRAW;
	    if (info->fb_info.changevar)
	      (*info->fb_info.changevar)(con);
	}

	if (!info->fb_info.display_fg ||
	    info->fb_info.display_fg->vc_num == con)
		platinum_set_par(&par, info);

	if (oldbpp != var->bits_per_pixel) {
	    if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
	      return err;
	    do_install_cmap(con, &info->fb_info);
	}

	return 0;
}

static int platinum_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info)
{
	if (!info->display_fg ||
	    info->display_fg->vc_num == con)	/* current console? */
		return fb_get_cmap(cmap, kspc, platinum_getcolreg, info);
	if (fb_display[con].cmap.len)	/* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else {
		int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		fb_copy_cmap(fb_default_cmap(size), cmap, kspc ? 0 : 2);
	}
	return 0;
}

static int platinum_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			     struct fb_info *info)
{
	int err;
	struct display *disp;

	if (con >= 0)
		disp = &fb_display[con];
	else
		disp = info->disp;
	if (!disp->cmap.len) {     /* no colormap allocated? */
		int size = disp->var.bits_per_pixel == 16 ? 32 : 256;
		if ((err = fb_alloc_cmap(&disp->cmap, size, 0)))
			return err;
	}

	if (!info->display_fg ||
	    info->display_fg->vc_num == con)	/* current console? */
		return fb_set_cmap(cmap, kspc, platinum_setcolreg, info);
	else
		fb_copy_cmap(cmap, &disp->cmap, kspc ? 0 : 1);
	return 0;
}

static int platinum_switch(int con, struct fb_info *fb)
{
	struct fb_info_platinum *info = (struct fb_info_platinum *) fb;
	struct fb_par_platinum par;

	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, platinum_getcolreg,
			    fb);
	currcon = con;

	platinum_var_to_par(&fb_display[con].var, &par, info);
	platinum_set_par(&par, info);
	platinum_set_dispsw(&fb_display[con], info, par.cmode, 0);
	do_install_cmap(con, fb);

	return 1;
}

static int platinum_updatevar(int con, struct fb_info *fb)
{
	printk(KERN_ERR "platinum_updatevar is doing nothing yet.\n");
	return 0;
}

static void platinum_blank(int blank,  struct fb_info *fb)
{
/*
 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */
/* [danj] I think there's something fishy about those constants... */
/*
	struct fb_info_platinum *info = (struct fb_info_platinum *) fb;
	int	ctrl;

	ctrl = ld_le32(&info->platinum_regs->ctrl.r) | 0x33;
	if (blank)
		--blank_mode;
	if (blank & VESA_VSYNC_SUSPEND)
		ctrl &= ~3;
	if (blank & VESA_HSYNC_SUSPEND)
		ctrl &= ~0x30;
	out_le32(&info->platinum_regs->ctrl.r, ctrl);
*/
/* TODO: Figure out how the heck to powerdown this thing! */
    return;
}

static int platinum_getcolreg(u_int regno, u_int *red, u_int *green,
			      u_int *blue, u_int *transp, struct fb_info *fb)
{
	struct fb_info_platinum *info = (struct fb_info_platinum *) fb;

	if (regno > 255)
		return 1;

	*red = (info->palette[regno].red<<8) | info->palette[regno].red;
	*green = (info->palette[regno].green<<8) | info->palette[regno].green;
	*blue = (info->palette[regno].blue<<8) | info->palette[regno].blue;
	*transp = 0;
	return 0;
}

static int platinum_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			      u_int transp, struct fb_info *fb)
{
	struct fb_info_platinum *info = (struct fb_info_platinum *) fb;
	volatile struct cmap_regs *cmap_regs = info->cmap_regs;

	if (regno > 255)
		return 1;

	red >>= 8;
	green >>= 8;
	blue >>= 8;

	info->palette[regno].red = red;
	info->palette[regno].green = green;
	info->palette[regno].blue = blue;

	out_8(&cmap_regs->addr, regno);		/* tell clut what addr to fill	*/
	out_8(&cmap_regs->lut, red);		/* send one color channel at	*/
	out_8(&cmap_regs->lut, green);		/* a time...			*/
	out_8(&cmap_regs->lut, blue);

	if(regno < 16) {
#ifdef FBCON_HAS_CFB16
		info->fbcon_cmap.cfb16[regno] = (regno << 10) | (regno << 5) | (regno << 0);
#endif
#ifdef FBCON_HAS_CFB32
		info->fbcon_cmap.cfb32[regno] = (regno << 24) | (regno << 16) | (regno << 8) | regno;
#endif
	}
	return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, platinum_setcolreg,
			    info);
	else {
		int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		fb_set_cmap(fb_default_cmap(size), 1, platinum_setcolreg,
			    info);
	}
}

static inline int platinum_vram_reqd(const struct fb_info_platinum *info, int video_mode, int color_mode)
{
	unsigned int pitch = 
	       (vmode_attrs[video_mode-1].hres * (1<<color_mode) + 0x20);
	fixup_pitch(pitch, info, color_mode);
	return vmode_attrs[video_mode-1].vres * pitch;
}

#define STORE_D2(a, d) { \
	out_8(&cmap_regs->addr, (a+32)); \
	out_8(&cmap_regs->d2, (d)); \
}

static void set_platinum_clock(struct fb_info_platinum *info)
{
	volatile struct cmap_regs *cmap_regs = info->cmap_regs;
	struct platinum_regvals	*init;

	init = platinum_reg_init[info->current_par.vmode-1];

	STORE_D2(6, 0xc6);
	out_8(&cmap_regs->addr,3+32);

	if (in_8(&cmap_regs->d2) == 2) {
		STORE_D2(7, init->clock_params[info->clktype][0]);
		STORE_D2(8, init->clock_params[info->clktype][1]);
		STORE_D2(3, 3);
	} else {
		STORE_D2(4, init->clock_params[info->clktype][0]);
		STORE_D2(5, init->clock_params[info->clktype][1]);
		STORE_D2(3, 2);
	}

	__delay(5000);
	STORE_D2(9, 0xa6);
}


/* Now how about actually saying, Make it so! */
/* Some things in here probably don't need to be done each time. */
static void platinum_set_par(const struct fb_par_platinum *par, struct fb_info_platinum *info)
{
	volatile struct platinum_regs	*platinum_regs = info->platinum_regs;
	volatile struct cmap_regs	*cmap_regs = info->cmap_regs;
	struct platinum_regvals		*init;
	int				i;
	int				vmode, cmode, pitch;
	
	info->current_par = *par;

	vmode = par->vmode;
	cmode = par->cmode;

	init = platinum_reg_init[vmode - 1];

	/* Initialize display timing registers */
	out_be32(&platinum_regs->reg[24].r, 7);	/* turn display off */

	for (i = 0; i < 26; ++i)
		out_be32(&platinum_regs->reg[i+32].r, init->regs[i]);

	out_be32(&platinum_regs->reg[26+32].r, (info->total_vram == 0x100000 ?
						init->offset[cmode] + 4 - cmode :
						init->offset[cmode]));
	out_be32(&platinum_regs->reg[16].r, (unsigned) info->frame_buffer_phys+init->fb_offset+0x10);
	pitch = init->pitch[cmode];
	fixup_pitch(pitch, info, cmode);
	out_be32(&platinum_regs->reg[18].r, pitch);
	out_be32(&platinum_regs->reg[19].r, (info->total_vram == 0x100000 ?
					     init->mode[cmode+1] :
					     init->mode[cmode]));
	out_be32(&platinum_regs->reg[20].r, (info->total_vram == 0x100000 ? 0x11 : 0x1011));
	out_be32(&platinum_regs->reg[21].r, 0x100);
	out_be32(&platinum_regs->reg[22].r, 1);
	out_be32(&platinum_regs->reg[23].r, 1);
	out_be32(&platinum_regs->reg[26].r, 0xc00);
	out_be32(&platinum_regs->reg[27].r, 0x235);
	/* out_be32(&platinum_regs->reg[27].r, 0x2aa); */

	STORE_D2(0, (info->total_vram == 0x100000 ?
		     init->dacula_ctrl[cmode] & 0xf :
		     init->dacula_ctrl[cmode]));
	STORE_D2(1, 4);
	STORE_D2(2, 0);

	set_platinum_clock(info);

	out_be32(&platinum_regs->reg[24].r, 0);	/* turn display on */

#ifdef CONFIG_FB_COMPAT_XPMAC
	if (console_fb_info == &info->fb_info) {
		display_info.height = par->yres;
		display_info.width = par->xres;
		display_info.depth = ( (cmode == CMODE_32) ? 32 :
				      ((cmode == CMODE_16) ? 16 : 8));
		display_info.pitch = vmode_attrs[vmode-1].hres * (1<<cmode) + 0x20;
		fixup_pitch(display_info.pitch, info, cmode);
		display_info.mode = vmode;
		strncpy(display_info.name, "platinum",
			sizeof(display_info.name));
		display_info.fb_address = info->frame_buffer_phys + init->fb_offset + 0x20;
		display_info.cmap_adr_address = info->cmap_regs_phys;
		display_info.cmap_data_address = info->cmap_regs_phys + 0x30;
		display_info.disp_reg_address = info->platinum_regs_phys;
		
	}
#endif /* CONFIG_FB_COMPAT_XPMAC */
}


static int __init init_platinum(struct fb_info_platinum *info)
{
	struct fb_var_screeninfo var;
	struct display *disp;
	int sense;
	int j,k;

	sense = read_platinum_sense(info);
	printk(KERN_INFO "Monitor sense value = 0x%x, ", sense);

#ifdef CONFIG_NVRAM
	if (default_vmode == VMODE_NVRAM) {
		default_vmode = nvram_read_byte(NV_VMODE);
		if (default_vmode <= 0 || default_vmode > VMODE_MAX ||
		    !platinum_reg_init[default_vmode-1])
			default_vmode = VMODE_CHOOSE;
	}
	if (default_cmode == CMODE_NVRAM)
		default_cmode = nvram_read_byte(NV_CMODE);
#endif
	if (default_vmode == VMODE_CHOOSE)
		default_vmode = mac_map_monitor_sense(sense);
	if (default_vmode <= 0 || default_vmode > VMODE_MAX)
		default_vmode = VMODE_640_480_60;
	if (default_cmode < CMODE_8 || default_cmode > CMODE_32)
		default_cmode = CMODE_8;
	/*
	 * Reduce the pixel size if we don't have enough VRAM.
	 */
	while(default_cmode > CMODE_8 && platinum_vram_reqd(info, default_vmode, default_cmode)
	    > info->total_vram)
		default_cmode--;

	printk("using video mode %d and color mode %d.\n", default_vmode, default_cmode);

	mac_vmode_to_var(default_vmode, default_cmode, &var);

	if (platinum_var_to_par(&var, &info->default_par, info)) {
		printk(KERN_ERR "platinumfb: can't set default video mode\n");
		return 0;
	}

	disp = &info->disp;

	strcpy(info->fb_info.modename, "platinum");
	info->fb_info.node = -1;
	info->fb_info.fbops = &platinumfb_ops;
	info->fb_info.disp = disp;
	strcpy(info->fb_info.fontname, fontname);
	info->fb_info.changevar = NULL;
	info->fb_info.switch_con = &platinum_switch;
	info->fb_info.updatevar = &platinum_updatevar;
	info->fb_info.blank = &platinum_blank;
	info->fb_info.flags = FBINFO_FLAG_DEFAULT;

	for (j = 0; j < 16; j++) {
		k = color_table[j];
		info->palette[j].red = default_red[k];
		info->palette[j].green = default_grn[k];
		info->palette[j].blue = default_blu[k];
	}
	platinum_set_var(&var, -1, &info->fb_info);

	if (register_framebuffer(&info->fb_info) < 0)
		return 0;

	printk(KERN_INFO "fb%d: platinum frame buffer device\n",
	       GET_FB_IDX(info->fb_info.node));

	return 1;
}

int __init platinum_init(void)
{
	struct device_node *dp;

	dp = find_devices("platinum");
	if (dp != 0)
		platinum_of_init(dp);
	return 0;
}

#ifdef __powerpc__
#define invalidate_cache(addr) \
	asm volatile("eieio; dcbf 0,%1" \
	: "=m" (*(addr)) : "r" (addr) : "memory");
#else
#define invalidate_cache(addr)
#endif

static void __init platinum_of_init(struct device_node *dp)
{
	struct fb_info_platinum	*info;
	unsigned long		addr, size;
	volatile __u8		*fbuffer;
	int			i, bank0, bank1, bank2, bank3;

	if(dp->n_addrs != 2) {
		printk(KERN_ERR "expecting 2 address for platinum (got %d)", dp->n_addrs);
		return;
	}

	info = kmalloc(sizeof(*info), GFP_ATOMIC);
	if (info == 0)
		return;
	memset(info, 0, sizeof(*info));

	/* Map in frame buffer and registers */
	for (i = 0; i < dp->n_addrs; ++i) {
		addr = dp->addrs[i].address;
		size = dp->addrs[i].size;
		/* Let's assume we can request either all or nothing */
		if (!request_mem_region(addr, size, "platinumfb")) {
			kfree(info);
			return;
		}
		if (size >= 0x400000) {
			/* frame buffer - map only 4MB */
			info->frame_buffer_phys = addr;
			info->frame_buffer = __ioremap(addr, 0x400000, _PAGE_WRITETHRU);
			info->base_frame_buffer = info->frame_buffer;
		} else {
			/* registers */
			info->platinum_regs_phys = addr;
			info->platinum_regs = ioremap(addr, size);
		}
	}

	info->cmap_regs_phys = 0xf301b000;	/* XXX not in prom? */
	request_mem_region(info->cmap_regs_phys, 0x1000, "platinumfb cmap");
	info->cmap_regs = ioremap(info->cmap_regs_phys, 0x1000);

	/* Grok total video ram */
	out_be32(&info->platinum_regs->reg[16].r, (unsigned)info->frame_buffer_phys);
	out_be32(&info->platinum_regs->reg[20].r, 0x1011);	/* select max vram */
	out_be32(&info->platinum_regs->reg[24].r, 0);	/* switch in vram */

	fbuffer = info->base_frame_buffer;
	fbuffer[0x100000] = 0x34;
	fbuffer[0x100008] = 0x0;
	invalidate_cache(&fbuffer[0x100000]);
	fbuffer[0x200000] = 0x56;
	fbuffer[0x200008] = 0x0;
	invalidate_cache(&fbuffer[0x200000]);
	fbuffer[0x300000] = 0x78;
	fbuffer[0x300008] = 0x0;
	invalidate_cache(&fbuffer[0x300000]);
	bank0 = 1; /* builtin 1MB vram, always there */
	bank1 = fbuffer[0x100000] == 0x34;
	bank2 = fbuffer[0x200000] == 0x56;
	bank3 = fbuffer[0x300000] == 0x78;
	info->total_vram = (bank0 + bank1 + bank2 + bank3) * 0x100000;
	printk(KERN_INFO "Total VRAM = %dMB %d%d%d%d\n", (int) (info->total_vram / 1024 / 1024), bank3, bank2, bank1, bank0);

	/*
	 * Try to determine whether we have an old or a new DACula.
	 */
	out_8(&info->cmap_regs->addr, 0x40);
	info->dactype = in_8(&info->cmap_regs->d2);
	switch (info->dactype) {
	case 0x3c:
		info->clktype = 1;
		break;
	case 0x84:
		info->clktype = 0;
		break;
	default:
		info->clktype = 0;
		printk(KERN_INFO "Unknown DACula type: %x\n", info->dactype);
		break;
	}

	if (!init_platinum(info)) {
		kfree(info);
		return;
	}

#ifdef CONFIG_FB_COMPAT_XPMAC
	if (!console_fb_info)
		console_fb_info = &info->fb_info;
#endif
}

/*
 * Get the monitor sense value.
 * Note that this can be called before calibrate_delay,
 * so we can't use udelay.
 */
static int read_platinum_sense(struct fb_info_platinum *info)
{
	volatile struct platinum_regs *platinum_regs = info->platinum_regs;
	int sense;

	out_be32(&platinum_regs->reg[23].r, 7);	/* turn off drivers */
	__delay(2000);
	sense = (~in_be32(&platinum_regs->reg[23].r) & 7) << 8;

	/* drive each sense line low in turn and collect the other 2 */
	out_be32(&platinum_regs->reg[23].r, 3);	/* drive A low */
	__delay(2000);
	sense |= (~in_be32(&platinum_regs->reg[23].r) & 3) << 4;
	out_be32(&platinum_regs->reg[23].r, 5);	/* drive B low */
	__delay(2000);
	sense |= (~in_be32(&platinum_regs->reg[23].r) & 4) << 1;
	sense |= (~in_be32(&platinum_regs->reg[23].r) & 1) << 2;
	out_be32(&platinum_regs->reg[23].r, 6);	/* drive C low */
	__delay(2000);
	sense |= (~in_be32(&platinum_regs->reg[23].r) & 6) >> 1;

	out_be32(&platinum_regs->reg[23].r, 7);	/* turn off drivers */

	return sense;
}

/* This routine takes a user-supplied var, and picks the best vmode/cmode from it. */
static int platinum_var_to_par(const struct fb_var_screeninfo *var, 
			       struct fb_par_platinum *par,
			       const struct fb_info_platinum *info)
{
	if(mac_var_to_vmode(var, &par->vmode, &par->cmode) != 0) {
		printk(KERN_ERR "platinum_var_to_par: mac_var_to_vmode unsuccessful.\n");
		printk(KERN_ERR "platinum_var_to_par: var->xres = %d\n", var->xres);
		printk(KERN_ERR "platinum_var_to_par: var->yres = %d\n", var->yres);
		printk(KERN_ERR "platinum_var_to_par: var->xres_virtual = %d\n", var->xres_virtual);
		printk(KERN_ERR "platinum_var_to_par: var->yres_virtual = %d\n", var->yres_virtual);
		printk(KERN_ERR "platinum_var_to_par: var->bits_per_pixel = %d\n", var->bits_per_pixel);
		printk(KERN_ERR "platinum_var_to_par: var->pixclock = %d\n", var->pixclock);
		printk(KERN_ERR "platinum_var_to_par: var->vmode = %d\n", var->vmode);
		return -EINVAL;
	}

	if(!platinum_reg_init[par->vmode-1]) {
		printk(KERN_ERR "platinum_var_to_par, vmode %d not valid.\n", par->vmode);
		return -EINVAL;
	}

	if (platinum_vram_reqd(info, par->vmode, par->cmode) > info->total_vram) {
		printk(KERN_ERR "platinum_var_to_par, not enough ram for vmode %d, cmode %d.\n", par->vmode, par->cmode);
		return -EINVAL;
	}

	par->xres = vmode_attrs[par->vmode-1].hres;
	par->yres = vmode_attrs[par->vmode-1].vres;
	par->xoffset = 0;
	par->yoffset = 0;
	par->vxres = par->xres;
	par->vyres = par->yres;
	
	return 0;
}

static int platinum_par_to_var(struct fb_var_screeninfo *var,
			       const struct fb_par_platinum *par,
			       const struct fb_info_platinum *info)
{
	return mac_vmode_to_var(par->vmode, par->cmode, var);
}

static int platinum_encode_fix(struct fb_fix_screeninfo *fix,
			       const struct fb_par_platinum *par,
			       const struct fb_info_platinum *info)
{
	struct platinum_regvals *init;

	init = platinum_reg_init[par->vmode-1];

	memset(fix, 0, sizeof(*fix));
	strcpy(fix->id, "platinum");
	fix->smem_start = (info->frame_buffer_phys) + init->fb_offset + 0x20;
	fix->smem_len = (u32) info->total_vram;
	fix->mmio_start = (info->platinum_regs_phys);
	fix->mmio_len = 0x1000;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	fix->ywrapstep = 0;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->visual = (par->cmode == CMODE_8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_DIRECTCOLOR;
	fix->line_length = vmode_attrs[par->vmode-1].hres * (1<<par->cmode) + 0x20;
	fixup_pitch(fix->line_length, info, par->cmode);
	
	return 0;
}


/* 
 * Parse user speficied options (`video=platinumfb:')
 */
int __init platinum_setup(char *options)
{
	char *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!strncmp(this_opt, "font:", 5)) {
			char *p;
			int i;

			p = this_opt + 5;
			for (i = 0; i < sizeof(fontname) - 1; i++)
				if (!*p || *p == ' ' || *p == ',')
					break;
			memcpy(fontname, this_opt + 5, i);
			fontname[i] = 0;
		}
		if (!strncmp(this_opt, "vmode:", 6)) {
	    		int vmode = simple_strtoul(this_opt+6, NULL, 0);
	    	if (vmode > 0 && vmode <= VMODE_MAX)
			default_vmode = vmode;
		} else if (!strncmp(this_opt, "cmode:", 6)) {
			int depth = simple_strtoul(this_opt+6, NULL, 0);
			switch (depth) {
			 case 0:
			 case 8:
			    default_cmode = CMODE_8;
			    break;
			 case 15:
			 case 16:
			    default_cmode = CMODE_16;
			    break;
			 case 24:
			 case 32:
			    default_cmode = CMODE_32;
			    break;
			}
		}
	}
	return 0;
}

MODULE_LICENSE("GPL");
