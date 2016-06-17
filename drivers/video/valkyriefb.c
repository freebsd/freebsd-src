/*
 *  valkyriefb.c -- frame buffer device for the PowerMac 'valkyrie' display
 *
 *  Created 8 August 1998 by 
 *  Martin Costabel <costabel@wanadoo.fr> and Kevin Schoedel
 *
 *  Vmode-switching changes and vmode 15/17 modifications created 29 August
 *  1998 by Barry K. Nathan <barryn@pobox.com>.
 *
 *  Ported to m68k Macintosh by David Huggins-Daines <dhd@debian.org>
 *
 *  Derived directly from:
 *
 *   controlfb.c -- frame buffer device for the PowerMac 'control' display
 *   Copyright (C) 1998 Dan Jacobowitz <dan@debian.org>
 *
 *   pmc-valkyrie.c -- Console support for PowerMac "valkyrie" display adaptor.
 *   Copyright (C) 1997 Paul Mackerras.
 *
 *  and indirectly:
 *
 *  Frame buffer structure from:
 *    drivers/video/chipsfb.c -- frame buffer device for
 *    Chips & Technologies 65550 chip.
 *
 *    Copyright (C) 1998 Paul Mackerras
 *
 *    This file is derived from the Powermac "chips" driver:
 *    Copyright (C) 1997 Fabio Riccardi.
 *    And from the frame buffer device for Open Firmware-initialized devices:
 *    Copyright (C) 1997 Geert Uytterhoeven.
 *
 *  Hardware information from:
 *    control.c: Console support for PowerMac "control" display adaptor.
 *    Copyright (C) 1996 Paul Mackerras
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
#include <linux/adb.h>
#include <linux/cuda.h>
#include <asm/io.h>
#ifdef CONFIG_MAC
#include <asm/bootinfo.h>
#include <asm/macintosh.h>
#else
#include <asm/prom.h>
#endif
#include <asm/pgtable.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/macmodes.h>

#include "valkyriefb.h"

static int can_soft_blank = 1;

#ifdef CONFIG_MAC
/* We don't yet have functions to read the PRAM... perhaps we can
   adapt them from the PPC code? */
static int default_vmode = VMODE_640_480_67;
static int default_cmode = CMODE_8;
#else
static int default_vmode = VMODE_NVRAM;
static int default_cmode = CMODE_NVRAM;
#endif
static char fontname[40] __initdata = { 0 };

static int currcon = 0;
static int switching = 0;

struct fb_par_valkyrie {
	int	vmode, cmode;
	int	xres, yres;
	int	vxres, vyres;
	int	xoffset, yoffset;
};

struct fb_info_valkyrie {
	struct fb_info			info;
	struct fb_fix_screeninfo	fix;
	struct fb_var_screeninfo	var;
	struct display			disp;
	struct fb_par_valkyrie		par;
	struct {
	    __u8 red, green, blue;
	}			palette[256];
	
	struct cmap_regs	*cmap_regs;
	unsigned long		cmap_regs_phys;
	
	struct valkyrie_regs	*valkyrie_regs;
	unsigned long		valkyrie_regs_phys;
	
	__u8			*frame_buffer;
	unsigned long		frame_buffer_phys;
	
	int			sense;
	unsigned long		total_vram;
#ifdef FBCON_HAS_CFB16
	u16 fbcon_cfb16_cmap[16];
#endif
};

/*
 * Exported functions
 */
int valkyriefb_init(void);
int valkyriefb_setup(char*);

static int valkyrie_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info);
static int valkyrie_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
static int valkyrie_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
static int valkyrie_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);
static int valkyrie_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);

static int read_valkyrie_sense(struct fb_info_valkyrie *p);
static inline int valkyrie_vram_reqd(int video_mode, int color_mode);
static void set_valkyrie_clock(unsigned char *params);
static void valkyrie_set_par(const struct fb_par_valkyrie *p, struct fb_info_valkyrie *info);
static inline int valkyrie_par_to_var(struct fb_par_valkyrie *par, struct fb_var_screeninfo *var);
static int valkyrie_var_to_par(struct fb_var_screeninfo *var,
	struct fb_par_valkyrie *par, const struct fb_info *fb_info);

static void valkyrie_init_info(struct fb_info *info, struct fb_info_valkyrie *p);
static void valkyrie_par_to_display(struct fb_par_valkyrie *par,
  struct display *disp, struct fb_fix_screeninfo *fix, struct fb_info_valkyrie *p);
static void valkyrie_init_display(struct display *disp);
static void valkyrie_par_to_fix(struct fb_par_valkyrie *par, struct fb_fix_screeninfo *fix,
	struct fb_info_valkyrie *p);
static void valkyrie_init_fix(struct fb_fix_screeninfo *fix, struct fb_info_valkyrie *p);

static struct fb_ops valkyriefb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	valkyrie_get_fix,
	fb_get_var:	valkyrie_get_var,
	fb_set_var:	valkyrie_set_var,
	fb_get_cmap:	valkyrie_get_cmap,
	fb_set_cmap:	valkyrie_set_cmap,
};

static int valkyriefb_getcolreg(u_int regno, u_int *red, u_int *green,
			     u_int *blue, u_int *transp, struct fb_info *info);
static int valkyriefb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			     u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);

static int valkyrie_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
	struct fb_info_valkyrie *cp = (struct fb_info_valkyrie *) info;

	*fix = cp->fix;
	return 0;
}

static int valkyrie_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	struct fb_info_valkyrie *cp = (struct fb_info_valkyrie *) info;

	*var = cp->var;
	return 0;
}

/* Sets everything according to var */
static int valkyrie_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	struct fb_info_valkyrie *p = (struct fb_info_valkyrie *) info;
	struct display *disp;
	struct fb_par_valkyrie par;
	int depthchange, err;

	disp = (con >= 0) ? &fb_display[con] : &p->disp;
	if ((err = valkyrie_var_to_par(var, &par, info))) {
		 /* printk (KERN_ERR "Error in valkyrie_set_var, calling valkyrie_var_to_par: %d.\n", err); */
		return err;
	}
	
	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW) {
		/* printk(KERN_ERR "Not activating, in valkyrie_set_var.\n"); */
		valkyrie_par_to_var(&par, var);
		return 0;
	}

	/*
	 * I know, we want to use fb_display[con], but grab certain info
	 * from p->var instead.
	 */
#define DIRTY(x) (p->var.x != var->x)
	depthchange = DIRTY(bits_per_pixel);
	/* adding "&& !DIRTY(pixclock)" corrects vmode-switching problems */
	if(!DIRTY(xres) && !DIRTY(yres) && !DIRTY(xres_virtual) &&
	   !DIRTY(yres_virtual) && !DIRTY(bits_per_pixel) && !DIRTY(pixclock)) {
	   	valkyrie_par_to_var(&par, var);
		p->var = disp->var = *var;
		return 0;
	}

	p->par = par;
	valkyrie_par_to_var(&par, var);
	p->var = *var;
	valkyrie_par_to_fix(&par, &p->fix, p);
	valkyrie_par_to_display(&par, disp, &p->fix, p);
	p->disp = *disp;
	
	if (info->changevar && !switching) {
		/* Don't want to do this if just switching consoles. */
		(*info->changevar)(con);
	}
	if (con == currcon)
		valkyrie_set_par(&par, p);
	if (depthchange)
		if ((err = fb_alloc_cmap(&disp->cmap, 0, 0)))
			return err;
	if (depthchange || switching)
		do_install_cmap(con, info);
	return 0;
}

static int valkyrie_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
	if (con == currcon)	{
		/* current console? */
		return fb_get_cmap(cmap, kspc, valkyriefb_getcolreg, info);
	}
	if (fb_display[con].cmap.len) { /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc? 0: 2);
	}
	else {
		int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		fb_copy_cmap(fb_default_cmap(size), cmap, kspc ? 0 : 2);
	}
	return 0;
}

static int valkyrie_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			 struct fb_info *info)
{
	struct display *disp = &fb_display[con];
	int err;

	if (disp->cmap.len == 0) {
		int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		err = fb_alloc_cmap(&disp->cmap, size, 0);
		if (err) {
			return err;
		}
	}

	if (con == currcon) {
		return fb_set_cmap(cmap, kspc, valkyriefb_setcolreg, info);
	}
	fb_copy_cmap(cmap, &disp->cmap, kspc ? 0 : 1);
	return 0;
}

static int valkyriefb_switch(int con, struct fb_info *fb)
{
	struct fb_info_valkyrie *info = (struct fb_info_valkyrie *) fb;
	struct fb_par_valkyrie par;

	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, valkyriefb_getcolreg,
			    fb);
	currcon = con;
#if 1
	valkyrie_var_to_par(&fb_display[currcon].var, &par, fb);
	valkyrie_set_par(&par, info);
	do_install_cmap(con, fb);
#else
	/* I see no reason not to do this.  Minus info->changevar(). */
	/* DOH.  This makes valkyrie_set_var compare, you guessed it, */
	/* fb_display[con].var (first param), and fb_display[con].var! */
	/* Perhaps I just fixed that... */
	switching = 1;
	valkyrie_set_var(&fb_display[con].var, con, info);
	switching = 0;
#endif
	return 0;
}

static int valkyriefb_updatevar(int con, struct fb_info *info)
{
	return 0;
}

static void valkyriefb_blank(int blank_mode, struct fb_info *info)
{
/*
 *  Blank the screen if blank_mode != 0, else unblank. If blank_mode == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */
	struct fb_info_valkyrie *p = (struct fb_info_valkyrie *) info;
	struct valkyrie_regvals	*init;
	unsigned char vmode;

	if (p->disp.can_soft_blank
	 && ((vmode = p->par.vmode) > 0)
	 && (vmode <= VMODE_MAX)
	 && ((init = valkyrie_reg_init[vmode - 1]) != NULL)) {
		if (blank_mode)
			--blank_mode;
		switch (blank_mode) {
		default:	/* unblank */
			out_8(&p->valkyrie_regs->mode.r, init->mode);
			break;
		case VESA_VSYNC_SUSPEND:
		case VESA_HSYNC_SUSPEND:
			/*
			 * [kps] Value extracted from MacOS. I don't know
			 * whether this bit disables hsync or vsync, or
			 * whether the hardware can do the other as well.
			 */
			out_8(&p->valkyrie_regs->mode.r, init->mode | 0x40);
			break;
		case VESA_POWERDOWN:
			out_8(&p->valkyrie_regs->mode.r, 0x66);
			break;
		}
	}
}

static int valkyriefb_getcolreg(u_int regno, u_int *red, u_int *green,
			     u_int *blue, u_int *transp, struct fb_info *info)
{
	struct fb_info_valkyrie *p = (struct fb_info_valkyrie *) info;

	if (regno > 255)
		return 1;
	*red = (p->palette[regno].red<<8) | p->palette[regno].red;
	*green = (p->palette[regno].green<<8) | p->palette[regno].green;
	*blue = (p->palette[regno].blue<<8) | p->palette[regno].blue;

	return 0;
}

static int valkyriefb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			     u_int transp, struct fb_info *info)
{
	struct fb_info_valkyrie *p = (struct fb_info_valkyrie *) info;
	volatile struct cmap_regs *cmap_regs = p->cmap_regs;


	if (regno > 255)
		return 1;
	red >>= 8;
	green >>= 8;
	blue >>= 8;
	p->palette[regno].red = red;
	p->palette[regno].green = green;
	p->palette[regno].blue = blue;

	/* tell clut which address to fill */
	out_8(&p->cmap_regs->addr, regno);
	udelay(1);
	/* send one color channel at a time */
	out_8(&cmap_regs->lut, red);
	out_8(&cmap_regs->lut, green);
	out_8(&cmap_regs->lut, blue);

	if (regno < 16) {
#ifdef FBCON_HAS_CFB16
		p->fbcon_cfb16_cmap[regno] = (regno << 10) | (regno << 5) | regno;
#endif
	}

	return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len) {
		fb_set_cmap(&fb_display[con].cmap, 1, valkyriefb_setcolreg,
			    info);
	}
	else {
		int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		fb_set_cmap(fb_default_cmap(size), 1, valkyriefb_setcolreg,
			    info);
	}
}

#ifdef CONFIG_FB_COMPAT_XPMAC
extern struct vc_mode display_info;
extern struct fb_info *console_fb_info;
#endif /* CONFIG_FB_COMPAT_XPMAC */

static int valkyrie_vram_reqd(int video_mode, int color_mode)
{
	int pitch;
	
	if ((pitch = valkyrie_reg_init[video_mode-1]->pitch[color_mode]) == 0)
		pitch = 2 * valkyrie_reg_init[video_mode-1]->pitch[0];
	return valkyrie_reg_init[video_mode-1]->vres * pitch;
}

static void set_valkyrie_clock(unsigned char *params)
{
	struct adb_request req;
	int i;

#ifdef CONFIG_ADB_CUDA
	for (i = 0; i < 3; ++i) {
		cuda_request(&req, NULL, 5, CUDA_PACKET, CUDA_GET_SET_IIC,
			     0x50, i + 1, params[i]);
		while (!req.complete)
			cuda_poll();
	}
#endif
}

static void __init init_valkyrie(struct fb_info_valkyrie *p)
{
	struct fb_par_valkyrie *par = &p->par;
	struct fb_var_screeninfo var;
	int j, k;

	p->sense = read_valkyrie_sense(p);
	printk(KERN_INFO "Monitor sense value = 0x%x, ", p->sense);

#ifdef CONFIG_NVRAM
	/* Try to pick a video mode out of NVRAM if we have one. */
	if (default_vmode == VMODE_NVRAM) {
		default_vmode = nvram_read_byte(NV_VMODE);
		if (default_vmode <= 0
		 || default_vmode > VMODE_MAX
		 || !valkyrie_reg_init[default_vmode - 1])
			default_vmode = VMODE_CHOOSE;
	}
	if (default_cmode == CMODE_NVRAM)
		default_cmode = nvram_read_byte(NV_CMODE);
#endif
	if (default_vmode == VMODE_CHOOSE)
		default_vmode = mac_map_monitor_sense(p->sense);
	if (!valkyrie_reg_init[default_vmode - 1])
		default_vmode = VMODE_640_480_67;

	/*
	 * Reduce the pixel size if we don't have enough VRAM or bandwitdh.
	 */
	if (default_cmode < CMODE_8
	 || default_cmode > CMODE_16
	 || valkyrie_reg_init[default_vmode-1]->pitch[default_cmode] == 0
	 || valkyrie_vram_reqd(default_vmode, default_cmode) > p->total_vram)
		default_cmode = CMODE_8;
	
	printk(KERN_INFO "using video mode %d and color mode %d.\n", default_vmode, default_cmode);

	mac_vmode_to_var(default_vmode, default_cmode, &var);
	if (valkyrie_var_to_par(&var, par, &p->info)) {
	    printk(KERN_ERR "valkyriefb: can't set default video mode\n");
	    return ;
	}
	
	valkyrie_init_fix(&p->fix, p);
	valkyrie_par_to_fix(&p->par, &p->fix, p);
	valkyrie_par_to_var(&p->par, &p->var);
	valkyrie_init_display(&p->disp);
	valkyrie_par_to_display(&p->par, &p->disp, &p->fix, p);
	valkyrie_init_info(&p->info, p);

	/* Initialize colormap */
	for (j = 0; j < 16; j++) {
		k = color_table[j];
		p->palette[j].red = default_red[k];
		p->palette[j].green = default_grn[k];
		p->palette[j].blue = default_blu[k];
	}
	
	valkyrie_set_var (&var, -1, &p->info);

	if (register_framebuffer(&p->info) < 0) {
		kfree(p);
		return;
	}
	
	printk(KERN_INFO "fb%d: valkyrie frame buffer device\n", GET_FB_IDX(p->info.node));	
}

static void valkyrie_set_par(const struct fb_par_valkyrie *par,
			     struct fb_info_valkyrie *p)
{
	struct valkyrie_regvals	*init;
	volatile struct valkyrie_regs *valkyrie_regs = p->valkyrie_regs;
	int vmode, cmode;
	
	vmode = par->vmode;
	cmode = par->cmode;
	
	if (vmode <= 0
	 || vmode > VMODE_MAX
	 || (init = valkyrie_reg_init[vmode - 1]) == NULL)
		panic("valkyrie: display mode %d not supported", vmode);

	/* Reset the valkyrie */
	out_8(&valkyrie_regs->status.r, 0);
	udelay(100);

	/* Initialize display timing registers */
	out_8(&valkyrie_regs->mode.r, init->mode | 0x80);
	out_8(&valkyrie_regs->depth.r, cmode + 3);
	set_valkyrie_clock(init->clock_params);
	udelay(100);

	/* Turn on display */
	out_8(&valkyrie_regs->mode.r, init->mode);

#ifdef CONFIG_FB_COMPAT_XPMAC
	/* And let the world know the truth. */
	if (!console_fb_info || console_fb_info == &p->info) {
		display_info.height = p->var.yres;
		display_info.width = p->var.xres;
		display_info.depth = (cmode == CMODE_16) ? 16 : 8;
		display_info.pitch = p->fix.line_length;
		display_info.mode = vmode;
		strncpy(display_info.name, "valkyrie",
			sizeof(display_info.name));
		display_info.fb_address = p->frame_buffer_phys + 0x1000;
		display_info.cmap_adr_address = p->cmap_regs_phys;
		display_info.cmap_data_address = p->cmap_regs_phys + 8;
		display_info.disp_reg_address = p->valkyrie_regs_phys;
		console_fb_info = &p->info;
	}
#endif /* CONFIG_FB_COMPAT_XPMAC */
}

int __init valkyriefb_init(void)
{
	struct fb_info_valkyrie	*p;
	unsigned long frame_buffer_phys, cmap_regs_phys, flags;

#ifdef CONFIG_MAC
	if (!MACH_IS_MAC)
		return 0;
	if (!(mac_bi_data.id == MAC_MODEL_Q630
	      /* I'm not sure about this one */
	    || mac_bi_data.id == MAC_MODEL_P588))
		return 0;

	/* Hardcoded addresses... welcome to 68k Macintosh country :-) */
	frame_buffer_phys = 0xf9000000;
	cmap_regs_phys = 0x50f24000;
	flags = IOMAP_NOCACHE_SER; /* IOMAP_WRITETHROUGH?? */
#else /* ppc (!CONFIG_MAC) */
	struct device_node *dp;

	dp = find_devices("valkyrie");
	if (dp == 0)
		return 0;

	if(dp->n_addrs != 1) {
		printk(KERN_ERR "expecting 1 address for valkyrie (got %d)", dp->n_addrs);
		return 0;
	}	

	frame_buffer_phys = dp->addrs[0].address;
	cmap_regs_phys = dp->addrs[0].address+0x304000;
	flags = _PAGE_WRITETHRU;
#endif /* ppc (!CONFIG_MAC) */

	p = kmalloc(sizeof(*p), GFP_ATOMIC);
	if (p == 0)
		return 0;
	memset(p, 0, sizeof(*p));

	/* Map in frame buffer and registers */
	if (!request_mem_region(frame_buffer_phys, 0x100000, "valkyriefb")) {
		kfree(p);
		return 0;
	}
	p->total_vram = 0x100000;
	p->frame_buffer_phys  = frame_buffer_phys;
	p->frame_buffer = __ioremap(frame_buffer_phys, p->total_vram, flags);
	p->cmap_regs_phys = cmap_regs_phys;
	p->cmap_regs = ioremap(p->cmap_regs_phys, 0x1000);
	p->valkyrie_regs_phys = cmap_regs_phys+0x6000;
	p->valkyrie_regs = ioremap(p->valkyrie_regs_phys, 0x1000);
	init_valkyrie(p);
	return 0;
}

/*
 * Get the monitor sense value.
 */
static int read_valkyrie_sense(struct fb_info_valkyrie *p)
{
	int sense, in;

    out_8(&p->valkyrie_regs->msense.r, 0);   /* release all lines */
    __delay(20000);
    sense = ((in = in_8(&p->valkyrie_regs->msense.r)) & 0x70) << 4;
    /* drive each sense line low in turn and collect the other 2 */
    out_8(&p->valkyrie_regs->msense.r, 4);   /* drive A low */
    __delay(20000);
    sense |= ((in = in_8(&p->valkyrie_regs->msense.r)) & 0x30);
    out_8(&p->valkyrie_regs->msense.r, 2);   /* drive B low */
    __delay(20000);
    sense |= ((in = in_8(&p->valkyrie_regs->msense.r)) & 0x40) >> 3;
	sense |= (in & 0x10) >> 2;
    out_8(&p->valkyrie_regs->msense.r, 1);   /* drive C low */
    __delay(20000);
    sense |= ((in = in_8(&p->valkyrie_regs->msense.r)) & 0x60) >> 5;

    out_8(&p->valkyrie_regs->msense.r, 7);

	return sense;
}

/*
 * This routine takes a user-supplied var,
 * and picks the best vmode/cmode from it.
 */
static int valkyrie_var_to_par(struct fb_var_screeninfo *var,
	struct fb_par_valkyrie *par, const struct fb_info *fb_info)

/* [bkn] I did a major overhaul of this function.
 *
 * Much of the old code was "swiped by jonh from atyfb.c". Because
 * macmodes has mac_var_to_vmode, I felt that it would be better to
 * rework this function to use that, instead of reinventing the wheel to
 * add support for vmode 17. This was reinforced by the fact that
 * the previously swiped atyfb.c code is no longer there.
 *
 * So, I swiped and adapted platinum_var_to_par (from platinumfb.c), replacing
 * most, but not all, of the old code in the process. One side benefit of
 * swiping the platinumfb code is that we now have more comprehensible error
 * messages when a vmode/cmode switch fails. (Most of the error messages are
 * platinumfb.c, but I added two of my own, and I also changed some commas
 * into colons to make the messages more consistent with other Linux error
 * messages.) In addition, I think the new code *might* fix some vmode-
 * switching oddities, but I'm not sure.
 *
 * There may be some more opportunities for cleanup in here, but this is a
 * good start...
 */

{
	int bpp = var->bits_per_pixel;
	struct valkyrie_regvals *init;
	struct fb_info_valkyrie *p = (struct fb_info_valkyrie *) fb_info;


	if(mac_var_to_vmode(var, &par->vmode, &par->cmode) != 0) {
		printk(KERN_ERR "valkyrie_var_to_par: %dx%dx%d unsuccessful.\n",var->xres,var->yres,var->bits_per_pixel);
		return -EINVAL;
	}

	/* Check if we know about the wanted video mode */
	if(!valkyrie_reg_init[par->vmode-1]) {
		printk(KERN_ERR "valkyrie_var_to_par: vmode %d not valid.\n", par->vmode);
		return -EINVAL;
	}

	par->xres = var->xres;
	par->yres = var->yres;
	par->xoffset = 0;
	par->yoffset = 0;
	par->vxres = par->xres;
	par->vyres = par->yres;
	
	if (var->xres_virtual > var->xres || var->yres_virtual > var->yres
		|| var->xoffset != 0 || var->yoffset != 0) {
		return -EINVAL;
	}

	if (bpp <= 8)
		par->cmode = CMODE_8;
	else if (bpp <= 16)
		par->cmode = CMODE_16;
	else {
		printk(KERN_ERR "valkyrie_var_to_par: cmode %d not supported.\n", par->cmode);
		return -EINVAL;
	}

	init = valkyrie_reg_init[par->vmode-1];
	if (init->pitch[par->cmode] == 0) {
		printk(KERN_ERR "valkyrie_var_to_par: vmode %d does not support cmode %d.\n", par->vmode, par->cmode);
		return -EINVAL;
	}

	if (valkyrie_vram_reqd(par->vmode, par->cmode) > p->total_vram) {
		printk(KERN_ERR "valkyrie_var_to_par: not enough ram for vmode %d, cmode %d.\n", par->vmode, par->cmode);
		return -EINVAL;
	}

	return 0;
}

static int valkyrie_par_to_var(struct fb_par_valkyrie *par, struct fb_var_screeninfo *var)
{
	return mac_vmode_to_var(par->vmode, par->cmode, var);
}

static void valkyrie_init_fix(struct fb_fix_screeninfo *fix, struct fb_info_valkyrie *p)
{
	memset(fix, 0, sizeof(*fix));
	strcpy(fix->id, "valkyrie");
	fix->mmio_start = p->valkyrie_regs_phys;
	fix->mmio_len = sizeof(struct valkyrie_regs);
	fix->type = FB_TYPE_PACKED_PIXELS;
	
	fix->type_aux = 0;
	fix->ywrapstep = 0;
	fix->ypanstep = 0;
	fix->xpanstep = 0;
	
}

/* Fix must already be inited above */
static void valkyrie_par_to_fix(struct fb_par_valkyrie *par,
	struct fb_fix_screeninfo *fix,
	struct fb_info_valkyrie *p)
{
	fix->smem_start = p->frame_buffer_phys + 0x1000;
#if 1
	fix->smem_len = valkyrie_vram_reqd(par->vmode, par->cmode);
#else
	fix->smem_len = p->total_vram;
#endif
	fix->visual = (par->cmode == CMODE_8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_DIRECTCOLOR;
	fix->line_length = par->vxres << par->cmode;
		/* ywrapstep, xpanstep, ypanstep */
}

static void valkyrie_init_display(struct display *disp)
{
	memset(disp, 0, sizeof(*disp));
	disp->type = /* fix->type */ FB_TYPE_PACKED_PIXELS;
	disp->can_soft_blank = can_soft_blank;
	disp->scrollmode = SCROLL_YREDRAW;
}

static void valkyrie_par_to_display(struct fb_par_valkyrie *par,
  struct display *disp, struct fb_fix_screeninfo *fix, struct fb_info_valkyrie *p)
{
	disp->var = p->var;
	disp->screen_base = (char *) p->frame_buffer + 0x1000;
	disp->visual = fix->visual;
	disp->line_length = fix->line_length;

	if(disp->scrollmode != SCROLL_YREDRAW) {
		printk(KERN_ERR "Scroll mode not YREDRAW in valkyrie_par_to_display\n");
		disp->scrollmode = SCROLL_YREDRAW;
	}
	switch (par->cmode) {
#ifdef FBCON_HAS_CFB8
                case CMODE_8:
                        disp->dispsw = &fbcon_cfb8;
                        break;
#endif
#ifdef FBCON_HAS_CFB16
                case CMODE_16:
                        disp->dispsw = &fbcon_cfb16;
                        disp->dispsw_data = p->fbcon_cfb16_cmap;
                        break;
#endif
                default:
                        disp->dispsw = &fbcon_dummy;
                        break;
        }
}

static void __init valkyrie_init_info(struct fb_info *info, struct fb_info_valkyrie *p)
{
	strcpy(info->modename, p->fix.id);
	info->node = -1;	/* ??? danj */
	info->fbops = &valkyriefb_ops;
	info->disp = &p->disp;
	strcpy(info->fontname, fontname);
	info->changevar = NULL;
	info->switch_con = &valkyriefb_switch;
	info->updatevar = &valkyriefb_updatevar;
	info->blank = &valkyriefb_blank;
	info->flags = FBINFO_FLAG_DEFAULT;
}


/*
 * Parse user speficied options (`video=valkyriefb:')
 */
int __init valkyriefb_setup(char *options)
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
		else if (!strncmp(this_opt, "vmode:", 6)) {
	    		int vmode = simple_strtoul(this_opt+6, NULL, 0);
	    	if (vmode > 0 && vmode <= VMODE_MAX)
				default_vmode = vmode;
		}
		else if (!strncmp(this_opt, "cmode:", 6)) {
			int depth = simple_strtoul(this_opt+6, NULL, 0);
			switch (depth) {
			 case 8:
			    default_cmode = CMODE_8;
			    break;
			 case 15:
			 case 16:
			    default_cmode = CMODE_16;
			    break;
			}
		}
		/* XXX - remove these options once blanking has been tested */
		else if (!strncmp(this_opt, "noblank", 7)) {
			can_soft_blank = 0;
		}
		else if (!strncmp(this_opt, "blank", 5)) {
			can_soft_blank = 1;
		}
	}
	return 0;
}

MODULE_LICENSE("GPL");
