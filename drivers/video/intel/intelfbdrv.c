/*
 * intelfb
 *
 * Linux framebuffer driver for Intel(R) 830M/845G/852GM/855GM/865G
 * integrated graphics chips.
 *
 * Copyright (C) 2002, 2003 David Dawes <dawes@tungstengraphics.com>
 *
 * This driver consists of two parts.  The first part (intelfbdrv.c) provides
 * the basic fbdev interfaces, is derived in part from the radeonfb and
 * vesafb drivers, and is covered by the GPL.  The second part (intelfbhw.c)
 * provides the code to program the hardware.  Most of it is derived from
 * the i810/i830 XFree86 driver.  The HW-specific code is covered here
 * under a dual license (GPL and MIT/XFree86 license).
 *
 * Author: David Dawes
 *
 */

/* $DHD: intelfb/intelfbdrv.c,v 1.15 2003/02/06 17:50:08 dawes Exp $ */
/* $TG$ */

/*
 * Changes:
 *    01/2003 - Initial driver (0.1.0), no mode switching, no acceleration.
 *		This initial version is a basic core that works a lot like
 *		the vesafb driver.  It must be built-in to the kernel,
 *		and the initial video mode must be set with vga=XXX at
 *		boot time.  (David Dawes)
 *
 *    01/2003 - Version 0.2.0: Mode switching added, colormap support
 *		implemented, Y panning, and soft screen blanking implemented.
 *		No acceleration yet.  (David Dawes)
 *
 *    01/2003 - Version 0.3.0: fbcon acceleration support added.  Module
 *		option handling added.  (David Dawes)
 *
 *    01/2003 - Version 0.4.0: fbcon HW cursor support added.  (David Dawes)
 *
 *    01/2003 - Version 0.4.1: Add auto-generation of built-in modes.
 *		(David Dawes)
 *
 *    02/2003 - Version 0.4.2: Add check for active non-CRT devices, and 
 *		mode validation checks.  (David Dawes)
 *
 *    02/2003 - Version 0.4.3: Check when the VC is in graphics mode so that
 *		acceleration is disabled while an XFree86 server is running.
 *		(David Dawes)
 *
 *    02/2003 - Version 0.4.4: Monitor DPMS support.  (David Dawes)
 *
 *    02/2003 - Version 0.4.5: Basic XFree86 + fbdev working.  (David Dawes)
 *
 *    02/2003 - Version 0.5.0: Modify to work with the 2.5.32 kernel as well
 *		as 2.4.x kernels.  (David Dawes)
 *
 *    02/2003 - Version 0.6.0: Split out HW-specifics into a separate file.
 *		(David Dawes)
 *
 *    02/2003 - Version 0.7.0: Test on 852GM/855GM.  Acceleration and HW
 *		cursor are disabled on this platform.  (David Dawes)
 *
 *    02/2003 - Version 0.7.1: Test on 845G.  Acceleration is disabled
 *		on this platform.  (David Dawes)
 *
 *    02/2003 - Version 0.7.2: Test on 830M.  Acceleration and HW
 *		cursor are disabled on this platform.  (David Dawes)
 *
 *    02/2003 - Version 0.7.3: Fix 8-bit modes for mobile platforms
 *		(David Dawes)
 *
 *    02/2003 - Version 0.7.4: Add checks for FB and FBCON_HAS_CFB* configured
 *		in the kernel, and add mode bpp verification and default
 *		bpp selection based on which FBCON_HAS_CFB* are configured.
 *		(David Dawes)
 *
 *    02/2003 - Version 0.7.5: Add basic package/install scripts based on the
 *		DRI packaging scripts.  (David Dawes)
 *
 * TODO:
 *  - 
 *
 * Wish List:
 *  - Check clock limits for 845G and 830M.
 *  - Test on SMP config.
 *  - Check if any functions/data should be __devinit, etc.
 *  - See if it's feasible to get/use DDC/EDID info.
 *  - MTRR support.
 *  - See if module unloading can work.
 *  - See if driver works built-in to 2.5.32 kernel.
 *  - Check acceleration problems on 830M-855GM.
 *  - Add gtf support so that arbitrary modes can be calculated.
 *  - Port driver to latest 2.5.x fbdev interface.
 */

#include <linux/config.h>
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
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>
#include <linux/pagemap.h>
#include <linux/version.h>

#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb32.h>

#include "intelfb.h"

#include "builtinmodes.c"

/*
 * Limiting the class to PCI_CLASS_DISPLAY_VGA prevents function 1 of the
 * mobile chipsets from being registered.
 */
#if DETECT_VGA_CLASS_ONLY
#define INTELFB_CLASS_MASK ~0 << 8
#else
#define INTELFB_CLASS_MASK 0
#endif

static struct pci_device_id intelfb_pci_table[] __devinitdata = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_830M, PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_DISPLAY_VGA << 8, INTELFB_CLASS_MASK, INTEL_830M },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_845G, PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_DISPLAY_VGA << 8, INTELFB_CLASS_MASK, INTEL_845G },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_85XGM, PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_DISPLAY_VGA << 8, INTELFB_CLASS_MASK, INTEL_85XGM },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_865G, PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_DISPLAY_VGA << 8, INTELFB_CLASS_MASK, INTEL_865G },
	{ 0, }
};

/* Global data */
static int num_registered = 0;


/* Forward declarations */
static int intelfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static int intelfb_get_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
#endif
static int intelfb_set_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
static int intelfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
static int intelfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
static int intelfb_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg, int con,
			 struct fb_info *info);
static int intelfb_switch(int con, struct fb_info *info);
static int intelfb_updatevar(int con, struct fb_info *info);
static int intelfb_blank(int blank, struct fb_info *info);
static int intelfb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			     unsigned *blue, unsigned *transp,
			     struct fb_info *info);
static int intelfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			     unsigned blue, unsigned transp,
			     struct fb_info *info);
static int intelfb_pci_register(struct pci_dev *pdev,
				const struct pci_device_id *ent);
static void __devexit intelfb_pci_unregister(struct pci_dev *pdev);
static int __devinit intelfb_set_fbinfo(struct intelfb_info *dinfo);
static void intelfb_set_dispsw(struct intelfb_info *dinfo,
			       struct display *disp);
static void mode_to_var(const struct fb_videomode *mode,
			struct fb_var_screeninfo *var, u32 bpp);
static int intelfb_set_mode(struct intelfb_info *dinfo,
			    struct fb_var_screeninfo *var,
			    struct display *disp, int blank);
static void intelfb_do_install_cmap(int con, struct fb_info *info);
static void update_dinfo(struct intelfb_info *dinfo,
			 struct fb_var_screeninfo *var, struct display *disp);
static void intelfb_flashcursor(unsigned long ptr);
static void fbcon_intelfb_setup(struct display *p);
static void fbcon_intelfb_bmove(struct display *p, int sy, int sx, int dy,
				int dx, int height, int width);
static void fbcon_intelfb_clear(struct vc_data *conp, struct display *p,
				int sy, int sx, int height, int width);
static void fbcon_intelfb_putc(struct vc_data *conp, struct display *p,
			       int c, int yy, int xx);
static void fbcon_intelfb_putcs(struct vc_data *conp, struct display *p,
				const unsigned short *s, int count,
				int yy, int xx);
static void fbcon_intelfb_revc(struct display *p, int xx, int yy);
static void fbcon_intelfb_clear_margins(struct vc_data *conp,
					struct display *p, int bottom_only);
static void fbcon_intelfb_cursor(struct display *disp, int mode, int x, int y);

/* fb ops */
static struct fb_ops intel_fb_ops = {
	.owner =		THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	.fb_get_fix =		intelfb_get_fix,
	.fb_get_var =		intelfb_get_var,
#else
	.fb_blank =		intelfb_blank,
	.fb_setcolreg =		intelfb_setcolreg,
#endif
	.fb_set_var =		intelfb_set_var,
	.fb_get_cmap =		intelfb_get_cmap,
	.fb_set_cmap =		intelfb_set_cmap,
	.fb_pan_display	=	intelfbhw_pan_display,
	.fb_ioctl =		intelfb_ioctl
};

/* PCI driver module table */
static struct pci_driver intelfb_driver = {
	.name =			INTELFB_MODULE_NAME,
	.id_table =		intelfb_pci_table,
	.probe =		intelfb_pci_register,
	.remove =		__devexit_p(intelfb_pci_unregister)
};

/* fbcon acceleration */
static struct display_switch fbcon_intelfb = {
	.setup =		fbcon_intelfb_setup,
	.bmove =		fbcon_intelfb_bmove,
	.clear =		fbcon_intelfb_clear,
	.putc =			fbcon_intelfb_putc,
	.putcs =		fbcon_intelfb_putcs,
	.revc =			fbcon_intelfb_revc,
	.cursor =		fbcon_intelfb_cursor,
	.clear_margins =	fbcon_intelfb_clear_margins,
	.fontwidthmask =	FONTWIDTHRANGE(4, 16)
};

/* Module description/parameters */
MODULE_AUTHOR("David Dawes <dawes@tungstengraphics.com>");
MODULE_DESCRIPTION(
	"Framebuffer driver for Intel(R) " SUPPORTED_CHIPSETS " chipsets");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DEVICE_TABLE(pci, intelfb_pci_table);

INTELFB_INT_PARAM(accel, 1, "Enable console acceleration");
INTELFB_INT_PARAM(hwcursor, 1, "Enable HW cursor");
INTELFB_INT_PARAM(fixed, 0, "Disable mode switching");
INTELFB_INT_PARAM(noinit, 0, "Don't initialise graphics mode when loading");
INTELFB_INT_PARAM(noregister, 0, "Don't register, just probe and exit (debug)");
INTELFB_INT_PARAM(probeonly, 0, "Do a minimal probe (debug)");
INTELFB_INT_PARAM(idonly, 0,
		  "Just identify without doing anything else (debug)");
INTELFB_INT_PARAM(bailearly, 0, "Bail out early, depending on value (debug)");
INTELFB_STR_PARAM(mode, NULL,
		  "Initial video mode \"<xres>x<yres>[-<depth>][@<refresh>]\"");
INTELFB_STR_PARAM(font, NULL, "Specify which built-in font to use");


/* module load/unload entry points */
int __init
intelfb_init(void)
{
	DBG_MSG("intelfb_init\n");

	INF_MSG("Framebuffer driver for "
		"Intel(R) " SUPPORTED_CHIPSETS " chipsets\n");
	INF_MSG("Version " INTELFB_VERSION
		", written by David Dawes <dawes@tungstengraphics.com>\n");

	if (idonly)
		return -ENODEV;

	return pci_module_init(&intelfb_driver);
}

void __exit
intelfb_exit(void)
{
	DBG_MSG("intelfb_exit\n");
	pci_unregister_driver(&intelfb_driver);
}

#ifndef MODULE
#define OPT_EQUAL(opt, name) (!strncmp(opt, name, strlen(name)))
#define OPT_INTVAL(opt, name) simple_strtoul(opt + strlen(name), NULL, 0)
#define OPT_STRVAL(opt, name) (opt + strlen(name))

static __inline__ char *
get_opt_string(const char *this_opt, const char *name)
{
	const char *p;
	int i;
	char *ret;

	p = OPT_STRVAL(this_opt, name);
	i = 0;
	while (p[i] && p[i] != ' ' && p[i] != ',')
		i++;
	ret = kmalloc(i + 1, GFP_KERNEL);
	if (ret) {
		strncpy(ret, p, i);
		ret[i] = '\0';
	}
	return ret;
}

static __inline__ int
get_opt_bool(const char *this_opt, const char *name, int *ret)
{
	if (!ret)
		return 0;

	if (OPT_EQUAL(this_opt, name)) {
		if (this_opt[strlen(name)] == '=')
			*ret = simple_strtoul(this_opt + strlen(name) + 1,
					      NULL, 0);
		else
			*ret = 1;
	} else {
		if (OPT_EQUAL(this_opt, "no") && OPT_EQUAL(this_opt + 2, name))
			*ret = 0;
		else
			return 0;
	}
	return 1;
}

int __init
intelfb_setup(char *options)
{
	char *this_opt;

	DBG_MSG("intelfb_setup\n");

	if (!options || !*options) {
		DBG_MSG("no options\n");
		return 0;
	} else
		DBG_MSG("options: %s\n", options);

	/*
	 * These are the built-in options analogous to the module parameters
	 * defined above.
	 *
	 * The syntax is:
	 *
	 *    video=intelfb:[mode][,<param>=<val>] ...
	 *
	 * e.g.,
	 *
	 *    video=intelfb:1024x768-16@75,accel=0
	 */

	while ((this_opt = strsep(&options, ","))) {
		if (!*this_opt)
			continue;
		if (get_opt_bool(this_opt, "accel", &accel))
			;
		else if (get_opt_bool(this_opt, "hwcursor", &hwcursor))
			;
		else if (get_opt_bool(this_opt, "fixed", &fixed))
			;
		else if (get_opt_bool(this_opt, "init", &noinit))
			noinit = !noinit;
		else if (OPT_EQUAL(this_opt, "font="))
			font = get_opt_string(this_opt, "font=");
		else if (OPT_EQUAL(this_opt, "mode="))
			mode = get_opt_string(this_opt, "mode=");
		else
			mode = this_opt;
	}

	return 0;
}
#endif

#ifdef MODULE
module_init(intelfb_init);
module_exit(intelfb_exit);
#endif

static void
cleanup(struct intelfb_info *dinfo)
{
	DBG_MSG("cleanup\n");

	if (!dinfo)
		return;

	if (dinfo->registered)
		unregister_framebuffer(&(dinfo->info));

	if (&dinfo->cursor.timer)
		del_timer_sync(&dinfo->cursor.timer);

#if USE_SYNC_PAGE
	if (dinfo->syncpage_virt) {
		struct page *pg = virt_to_page((void *)dinfo->syncpage_virt);
		if (pg) {
			put_page(pg);
			UnlockPage(pg);
			free_page(dinfo->syncpage_virt);
		}
	}
#endif

	if (dinfo->cursor_base)
		iounmap((void *)dinfo->cursor_base);
	if (dinfo->ring_base)
		iounmap((void *)dinfo->ring_base);
	if (dinfo->fb_base)
		iounmap((void *)dinfo->fb_base);
	if (dinfo->mmio_base)
		iounmap((void *)dinfo->mmio_base);
	if (dinfo->mmio_base_phys && dinfo->pdev)
		release_mem_region(dinfo->mmio_base_phys,
				   pci_resource_len(dinfo->pdev, 1));
	if (dinfo->fb_base_phys && dinfo->pdev)
		release_mem_region(dinfo->fb_base_phys,
				   pci_resource_len(dinfo->pdev, 0));
	kfree(dinfo);
}

#define bailout(dinfo) do {						\
	DBG_MSG("bailout\n");						\
	cleanup(dinfo);							\
	INF_MSG("Not going to register framebuffer, exiting...\n");	\
	return -ENODEV;							\
} while (0)


int
intelfb_var_to_depth(const struct fb_var_screeninfo *var)
{
#if 0
	DBG_MSG("intelfb_var_to_depth: bpp: %d, green.length is %d\n",
		var->bits_per_pixel, var->green.length);
#endif

	switch (var->bits_per_pixel) {
	case 16:
		return (var->green.length == 6) ? 16 : 15;
	case 32:
		return 24;
	default:
		return var->bits_per_pixel;
	}
}

static void
get_initial_mode(struct intelfb_info *dinfo)
{
	struct fb_var_screeninfo *var;
	int xtot, ytot;

	DBG_MSG("get_initial_mode\n");

	dinfo->initial_vga = 1;
	dinfo->initial_fb_base = screen_info.lfb_base;
	dinfo->initial_video_ram = screen_info.lfb_size * KB(64);
	dinfo->initial_pitch = screen_info.lfb_linelength;

	var = &dinfo->initial_var;
	memset(var, 0, sizeof(*var));
	var->xres = screen_info.lfb_width;
	var->yres = screen_info.lfb_height;
	var->xres_virtual = var->xres;
#if ALLOCATE_FOR_PANNING
	/* Allow use of half of the video ram for panning */
	var->yres_virtual =
		dinfo->initial_video_ram / 2 / dinfo->initial_pitch;
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;
#else
	var->yres_virtual = var->yres;
#endif
	var->bits_per_pixel = screen_info.lfb_depth;
	switch (screen_info.lfb_depth) {
	case 15:
		var->bits_per_pixel = 16;
		break;
	case 24:
		var->bits_per_pixel = 32;
		break;
	}

	DBG_MSG("Initial info: FB is 0x%x/0x%x (%d kByte)\n",
		dinfo->initial_fb_base, dinfo->initial_video_ram,
		BtoKB(dinfo->initial_video_ram));

	DBG_MSG("Initial info: mode is %dx%d-%d (%d)\n",
		var->xres, var->yres, var->bits_per_pixel,
		dinfo->initial_pitch);

	/* Dummy timing values (assume 60Hz) */
	var->left_margin = (var->xres / 8) & 0xf8;
	var->right_margin = 32;
	var->upper_margin = 16;
	var->lower_margin = 4;
	var->hsync_len = (var->xres / 8) & 0xf8;
	var->vsync_len = 4;

	xtot = var->xres + var->left_margin +
		var->right_margin + var->hsync_len;
	ytot = var->yres + var->upper_margin +
		var->lower_margin + var->vsync_len;
	var->pixclock = 10000000 / xtot * 1000 / ytot * 100 / 60;

	var->height = -1;
	var->width = -1;

	if (var->bits_per_pixel > 8) {
		var->red.offset = screen_info.red_pos;
		var->red.length = screen_info.red_size;
		var->green.offset = screen_info.green_pos;
		var->green.length = screen_info.green_size;
		var->blue.offset = screen_info.blue_pos;
		var->blue.length = screen_info.blue_size;
		var->transp.offset = screen_info.rsvd_pos;
		var->transp.length = screen_info.rsvd_size;
	} else {
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
	}
}

static int
intelfb_pci_register(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct intelfb_info *dinfo;
	int i, j, err;
	const char *s;

	DBG_MSG("intelfb_pci_register\n");

	num_registered++;
	if (num_registered != 1) {
		ERR_MSG("Attempted to register %d devices "
			"(should be only 1).\n", num_registered);
		return -ENODEV;
	}

	if (!(dinfo = kmalloc(sizeof(struct intelfb_info), GFP_KERNEL))) {
		ERR_MSG("Could not allocate memory for intelfb_info.\n");
		return -ENODEV;
	}

	memset(dinfo, 0, sizeof(*dinfo));
	dinfo->pdev = pdev;

	/* Enable device. */
	if ((err = pci_enable_device(pdev))) {
		ERR_MSG("Cannot enable device.\n");
		cleanup(dinfo);
		return -ENODEV;
	}

	/* Set base addresses. */
	dinfo->fb_base_phys = pci_resource_start(pdev, 0);
	dinfo->mmio_base_phys = pci_resource_start(pdev, 1);

	DBG_MSG("fb region: 0x%lx/0x%lx, MMIO region: 0x%lx/0x%lx\n",
		pci_resource_start(pdev, 0), pci_resource_len(pdev, 0),
		pci_resource_start(pdev, 1), pci_resource_len(pdev, 1));

	/* Reserve the fb and MMIO regions */
	if (!request_mem_region(dinfo->fb_base_phys, pci_resource_len(pdev, 0),
				INTELFB_MODULE_NAME)) {
		ERR_MSG("Cannot reserve FB region.\n");
		cleanup(dinfo);
		return -ENODEV;
	}
	if (!request_mem_region(dinfo->mmio_base_phys,
				pci_resource_len(pdev, 1),
				INTELFB_MODULE_NAME)) {
		ERR_MSG("Cannot reserve MMIO region.\n");
		cleanup(dinfo);
		return -ENODEV;
	}

	/* Map the MMIO region. */
	dinfo->mmio_base = (u32)ioremap(dinfo->mmio_base_phys, INTEL_REG_SIZE);
	if (!dinfo->mmio_base) {
		ERR_MSG("Cannot map MMIO.\n");
		cleanup(dinfo);
		return -ENODEV;
	}

	/* Get the chipset info. */
	dinfo->pci_chipset = pdev->device;

	if (intelfbhw_get_chipset(pdev, &dinfo->name, &dinfo->chipset,
				  &dinfo->mobile)) {
		cleanup(dinfo);
		return -ENODEV;
	}

	if (intelfbhw_get_memory(pdev, &dinfo->aperture_size,
				 &dinfo->stolen_size)) {
		cleanup(dinfo);
		return -ENODEV;
	}

	INF_MSG("%02x:%02x.%d: %s, aperture size %dMB, "
		"stolen memory %dkB\n",
		pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
		dinfo->name, BtoMB(dinfo->aperture_size),
		BtoKB(dinfo->stolen_size));

	/* Set these from the options. */
	dinfo->accel = accel;
	dinfo->hwcursor = hwcursor;

	if (NOACCEL_CHIPSET(dinfo) && dinfo->accel == 1) {
		INF_MSG("Acceleration is not supported for the %s chipset.\n",
			dinfo->name);
		dinfo->accel = 0;
	}

	/*
	 * For now, set the video ram size to the stolen size rounded
	 * down to a page multiple.  This means the agpgart driver doesn't
	 * need to get involved.
	 */
	dinfo->video_ram = ROUND_DOWN_TO_PAGE(dinfo->stolen_size);

	/* Allocate space for the ring buffer and HW cursor. */
	if (dinfo->accel) {
		dinfo->ring_size = RINGBUFFER_SIZE;
		dinfo->video_ram -= dinfo->ring_size;
		dinfo->ring_base_phys = dinfo->fb_base_phys + dinfo->video_ram;
		dinfo->ring_tail_mask = dinfo->ring_size - 1;
	}
	if (dinfo->hwcursor && !dinfo->mobile) {
		dinfo->cursor_size = HW_CURSOR_SIZE;
		dinfo->video_ram -= dinfo->cursor_size;
		dinfo->cursor_offset = dinfo->video_ram;
		dinfo->cursor_base_phys = dinfo->fb_base_phys +
					  dinfo->cursor_offset;
	}

	/* Framebuffer starts at offset 0. */
	dinfo->fb_offset = 0;
	dinfo->video_ram -= dinfo->fb_offset;

	/* Map the FB region.  Only map the memory used as video ram. */
	dinfo->fb_base = (u32)ioremap(dinfo->fb_base_phys + dinfo->fb_offset,
				      dinfo->video_ram);
	if (!dinfo->fb_base) {
		ERR_MSG("Cannot map framebuffer.\n");
		cleanup(dinfo);
		return -ENODEV;
	}

	if (dinfo->accel) {
		/* Map the ring buffer. */
		dinfo->ring_base = (u32)ioremap(dinfo->ring_base_phys,
						dinfo->ring_size);
		if (!dinfo->ring_base) {
			dinfo->accel = 0;
			WRN_MSG("Cannot map ring buffer: "
				"acceleration disabled.\n");
		}
	}

	if (dinfo->hwcursor && !dinfo->mobile) {
		/* Map the HW cursor buffer. */
		dinfo->cursor_base = (u32)ioremap(dinfo->cursor_base_phys,
						  dinfo->cursor_size);
		if (!dinfo->cursor_base) {
			WRN_MSG("Cannot map the HW cursor space: "
				"HW cursor disabled.\n");
			dinfo->hwcursor = 0;
		}
	}

	/*
	 * For mobile platforms, a physical page address is needed for the
	 * HW cursor.  Allocate it here and insert it into the GTT.
	 * If/when this driver is modified to be aware of the agpgart
	 * driver, it can be used to obtain/insert the page.
	 */
	if (dinfo->hwcursor && dinfo->mobile) {
#if !MOBILE_HW_CURSOR
		dinfo->hwcursor = 0;
		INF_MSG("HW cursor is not supported for mobile platforms.\n");
#else
		struct page *pg;

		/*
		 * Allocate a page of physical memory for the graphics
		 * engine to write to for synchronisation purposes.
		 */
		pg = alloc_page(GFP_KERNEL);
		if (!pg) {
			WRN_MSG("Cannot allocate a page for the HW cursor.  "
				"Disabling the HW cursor.");
			dinfo->hwcursor = 0;
		} else {
			get_page(pg);
			LockPage(pg);
			dinfo->cursor_page_virt = (u32)page_address(pg);
			dinfo->cursor_base_real =
						virt_to_phys(page_address(pg));

			/*
			 * XXX Add code to insert GTT entry and map the
			 * appropriate part of the aperture as
			 * dinfo->cursor_base.
			 */
		}
#endif
	}
	DBG_MSG("fb: 0x%x(+ 0x%x)/0x%x (0x%x)\n",
		dinfo->fb_base_phys, dinfo->fb_offset, dinfo->video_ram,
		dinfo->fb_base);
	DBG_MSG("MMIO: 0x%x/0x%x (0x%x)\n",
		dinfo->mmio_base_phys, INTEL_REG_SIZE, dinfo->mmio_base);
	DBG_MSG("ring buffer: 0x%x/0x%x (0x%x)\n",
		dinfo->ring_base_phys, dinfo->ring_size, dinfo->ring_base);
	DBG_MSG("HW cursor: 0x%x/0x%x (0x%x) (offset 0x%x) (phys 0x%x)\n",
		dinfo->cursor_base_phys, dinfo->cursor_size,
		dinfo->cursor_base, dinfo->cursor_offset,
		dinfo->cursor_base_real);

	DBG_MSG("options: accel = %d, fixed = %d, noinit = %d\n",
		accel, fixed, noinit);
	DBG_MSG("options: mode = \"%s\", font = \"%s\"\n",
		mode ? mode : "", font ? font : "");

	if (probeonly)
		bailout(dinfo);

	dinfo->fixed_mode = fixed;

	/*
	 * Check if the LVDS port or any DVO ports are enabled.  If so,
	 * don't allow mode switching.
	 */
	if ((s = intelfbhw_check_non_crt(dinfo))) {
		WRN_MSG("Non-CRT device is enabled (%s).  "
			"Disabling mode switching.\n", s);
		dinfo->fixed_mode = 1;
	}

	if (bailearly == 1)
		bailout(dinfo);

	if (dinfo->fixed_mode && ORIG_VIDEO_ISVGA != VIDEO_TYPE_VLFB) {
		ERR_MSG("Video mode must be programmed at boot time.\n");
		cleanup(dinfo);
		return -ENODEV;
	}

	if (bailearly == 2)
		bailout(dinfo);

	/* Initialise dinfo and related data. */
	/* If an initial mode was programmed at boot time, get its details. */
	if (ORIG_VIDEO_ISVGA == VIDEO_TYPE_VLFB)
		get_initial_mode(dinfo);

	if (bailearly == 3)
		bailout(dinfo);

#if 0
	if (dinfo->fixed_mode) {
		/*
		 * XXX Check video ram amounts and remap if the region already
		 * mapped is too small because of ring/cursor allocations.
		 */
		update_dinfo(dinfo, &dinfo->initial_var, NULL);
	}
#endif

	/* currcon will be set by the first switch. */
	dinfo->currcon = -1;
	dinfo->vc_mode = KD_TEXT;

#if USE_SYNC_PAGE
	if (dinfo->accel) {
		struct page *pg;

		/*
		 * Allocate a page of physical memory for the graphics
		 * engine to write to for synchronisation purposes.
		 */
		pg = alloc_page(GFP_KERNEL);
		if (!pg) {
			WRN_MSG("Cannot allocate a page for 2D accel.  "
				"Disabling acceleration.");
			dinfo->accel = 0;
		} else {
			get_page(pg);
			LockPage(pg);
			dinfo->syncpage_virt = (u32)page_address(pg);
			dinfo->syncpage_phys = virt_to_phys(page_address(pg));
			/* Write zero to the first dword */
			writel(0, dinfo->syncpage_virt);
			DBG_MSG("2D sync page: 0x%x (0x%x)\n",
				dinfo->syncpage_phys, dinfo->syncpage_virt);
		}
	}
#endif

	if (bailearly == 4)
		bailout(dinfo);


	if (intelfb_set_fbinfo(dinfo)) {
		cleanup(dinfo);
		return -ENODEV;
	}

	if (bailearly == 5)
		bailout(dinfo);

	for (i = 0; i < 16; i++) {
		j = color_table[i];
		dinfo->palette[i].red = default_red[j];
		dinfo->palette[i].green = default_grn[j];
		dinfo->palette[i].blue = default_blu[j];
	}

	if (bailearly == 6)
		bailout(dinfo);

	pci_set_drvdata(pdev, dinfo);

	/* Save the initial register state. */
	i = intelfbhw_read_hw_state(dinfo, &dinfo->save_state,
				    bailearly > 6 ? bailearly - 6 : 0);
	if (i != 0) {
		DBG_MSG("intelfbhw_read_hw_state returned %d\n", i);
		bailout(dinfo);
	}

	intelfbhw_print_hw_state(dinfo, &dinfo->save_state);

	if (bailearly == 18)
		bailout(dinfo);

#if TEST_MODE_TO_HW
	{
		struct intelfb_hwstate hw;
		struct fb_var_screeninfo var;
		int i;

		for (i = 0; i < num_modes; i++) {
			mode_to_var(&modedb[i], &var, 8);
			intelfbhw_read_hw_state(dinfo, &hw, 0);
			if (intelfbhw_mode_to_hw(dinfo, &hw, &var)) {
				DGB_MSG("Failed to set hw for mode %dx%d\n",
					var.xres, var.yres);
			} else {
				DGB_MSG("HW state for mode %dx%d\n",
					var.xres, var.yres);
				intelfbhw_print_hw_state(dinfo, &hw);
			}
		}
	}
#endif

	/* Cursor initialisation */
	if (dinfo->hwcursor) {
		init_timer(&dinfo->cursor.timer);
		dinfo->cursor.timer.function = intelfb_flashcursor;
		dinfo->cursor.timer.data = (unsigned long)dinfo;
		dinfo->cursor.state = CM_ERASE;
		spin_lock_init(&dinfo->DAClock);
	}
	
	if (bailearly == 19)
		bailout(dinfo);


	if (noregister)
		bailout(dinfo);

	if (register_framebuffer(&(dinfo->info)) < 0) {
		ERR_MSG("Cannot register framebuffer.\n");
		cleanup(dinfo);
		return -ENODEV;
	}

	dinfo->registered = 1;

	return 0;
}

static void __devexit
intelfb_pci_unregister(struct pci_dev *pdev)
{
	struct intelfb_info *dinfo = pci_get_drvdata(pdev);

	DBG_MSG("intelfb_pci_unregister\n");

	if (!dinfo)
		return;

	cleanup(dinfo);

	pci_set_drvdata(pdev, NULL);
}

/*
 * A simplified version of fb_find_mode.  The latter doesn't seem to work
 * too well -- haven't figured out why yet.
 */
static int
intelfb_find_mode(struct fb_var_screeninfo *var,
		  struct fb_info *info, const char *mode_option,
		  const struct fb_videomode *db, unsigned int dbsize,
		  const struct fb_videomode *default_mode,
		  unsigned int default_bpp)
{
	int i;
	char mname[20] = "", tmp[20] = "", *p, *q;
	unsigned int bpp = 0;

	DBG_MSG("intelfb_find_mode\n");

	/* Set up defaults */
	if (!db) {
		db = modedb;
		dbsize = sizeof(modedb) / sizeof(*modedb);
	}

	if (!default_bpp)
#if defined(FBCON_HAS_CFB8)
		default_bpp = 8;
#elif defined(FBCON_HAS_CFB16)
		default_bpp = 16;
#elif defined(FBCON_HAS_CFB32)
		default_bpp = 32;
#endif

	var->activate = FB_ACTIVATE_TEST;
	if (mode_option && *mode_option) {
		if (strlen(mode_option) < sizeof(tmp) - 1) {
			strcat(tmp, mode_option);
			q = tmp;
			p = strsep(&q, "-");
			strcat(mname, p);
			if (q) {
				p = strsep(&q, "@");
				bpp = simple_strtoul(p, NULL, 10);
				if (q) {
					strcat(mname, "@");
					strcat(mname, q);
				}
			}
		}
		if (!bpp)
			bpp = default_bpp;
		DBG_MSG("Mode is %s, bpp %d\n", mname, bpp);
	}
	if (*mname) {
		for (i = 0; i < dbsize; i++) {
			if (!strncmp(db[i].name, mname, strlen(mname))) {
				mode_to_var(&db[i], var, bpp);
				if (!intelfb_set_var(var, -1, info))
					return 1;
			}
		}
	}

	if (!default_mode)
		return 0;

	mode_to_var(default_mode, var, default_bpp);
	if (!intelfb_set_var(var, -1, info))
		return 3;

	for (i = 0; i < dbsize; i++) {
		mode_to_var(&db[i], var, default_bpp);
		if (!intelfb_set_var(var, -1, info))
			return 4;
	}

	return 0;
}

static __inline__ int
var_to_refresh(const struct fb_var_screeninfo *var)
{
	int xtot = var->xres + var->left_margin + var->right_margin +
		   var->hsync_len;
	int ytot = var->yres + var->upper_margin + var->lower_margin +
		   var->vsync_len;

	return (1000000000 / var->pixclock * 1000 + 500) / xtot / ytot;
}

/* Various intialisation functions */

static int __devinit
intelfb_init_disp_var(struct intelfb_info *dinfo)
{
	int msrc = 0;

	DBG_MSG("intelfb_init_disp_var\n");

	if (dinfo->fixed_mode) {
		dinfo->disp.var = dinfo->initial_var;
		msrc = 5;
	} else {
		if (mode) {
			msrc = intelfb_find_mode(&dinfo->disp.var,
						 &dinfo->info, mode,
						 modedb, num_modes, NULL, 0);
			if (msrc)
				msrc |= 8;
		}
		if (!msrc) {
			msrc = intelfb_find_mode(&dinfo->disp.var,
						 &dinfo->info, PREFERRED_MODE,
						 modedb, num_modes,
						 &modedb[DFLT_MODE], 0);
		}
	}

	if (!msrc) {
		ERR_MSG("Cannot find a suitable video mode.\n");
		return 1;
	}

	INF_MSG("Initial video mode is %dx%d-%d@%d.\n", dinfo->disp.var.xres,
		dinfo->disp.var.yres, intelfb_var_to_depth(&dinfo->disp.var),
		var_to_refresh(&dinfo->disp.var));

	DBG_MSG("Initial video mode is from %d.\n", msrc);

	if (dinfo->accel)
		dinfo->disp.var.accel_flags |= FB_ACCELF_TEXT;
	else
		dinfo->disp.var.accel_flags &= ~FB_ACCELF_TEXT;

	return 0;
}

static void
intelfb_set_dispsw(struct intelfb_info *dinfo, struct display *disp)
{
	DBG_MSG("intelfb_set_dispsw: (bpp is %d)\n", disp->var.bits_per_pixel);

	disp->dispsw_data = NULL;
	disp->dispsw = NULL;

	switch (disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw_data = &dinfo->con_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		disp->dispsw_data = &dinfo->con_cmap.cfb32;
		break;
#endif
	default:
		WRN_MSG("Setting fbcon_dummy renderer (bpp is %d).\n",
			disp->var.bits_per_pixel);
		disp->dispsw = &fbcon_dummy;
	}
	/* For all supported bpp. */
	if (!disp->dispsw)
		disp->dispsw = &fbcon_intelfb;
}

static int __devinit
intelfb_init_disp(struct intelfb_info *dinfo)
{
	struct fb_info *info;
	struct display *disp;

	DBG_MSG("intelfb_init_disp\n");

	info = &dinfo->info;
	disp = &dinfo->disp;

	if (intelfb_init_disp_var(dinfo))
		return 1;

	info->disp = disp;

	update_dinfo(dinfo, &disp->var, disp);

	intelfb_set_dispsw(dinfo, disp);

	dinfo->currcon_display = disp;

	return 0;
}

static int __devinit
intelfb_set_fbinfo(struct intelfb_info *dinfo)
{
	struct fb_info *info;

	DBG_MSG("intelfb_set_fbinfo\n");

	info = &dinfo->info;

	strcpy(info->modename, dinfo->name);
	info->node = NODEV;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &intel_fb_ops;
	info->display_fg = NULL;
	info->changevar = NULL;
	info->switch_con = intelfb_switch;
	info->updatevar = intelfb_updatevar;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	info->blank = intelfbhw_do_blank;
#endif

	if (intelfb_init_disp(dinfo))
		return 1;
	return 0;
}

/* Update dinfo to match the active video mode. */
static void
update_dinfo(struct intelfb_info *dinfo, struct fb_var_screeninfo *var,
	     struct display *disp)
{
	DBG_MSG("update_dinfo\n");

	dinfo->bpp = var->bits_per_pixel;
	dinfo->depth = intelfb_var_to_depth(var);
	dinfo->xres = var->xres;
	dinfo->yres = var->xres;
	dinfo->pixclock = var->pixclock;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	intelfb_get_fix(&dinfo->info.fix, dinfo->currcon, &dinfo->info);
#endif

	switch (dinfo->bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
		dinfo->visual = FB_VISUAL_PSEUDOCOLOR;
		dinfo->pitch = disp->var.xres_virtual;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		dinfo->visual = FB_VISUAL_TRUECOLOR;
		dinfo->pitch = disp->var.xres_virtual * 2;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		dinfo->visual = FB_VISUAL_TRUECOLOR;
		dinfo->pitch = disp->var.xres_virtual * 4;
		break;
#endif
	}

	/* Make sure the line length is a aligned correctly. */
	dinfo->pitch = ROUND_UP_TO(dinfo->pitch, STRIDE_ALIGNMENT);

	if (dinfo->fixed_mode)
		dinfo->pitch = dinfo->initial_pitch;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	disp->screen_base = (char *)dinfo->fb_base;
	disp->visual = dinfo->visual;
	disp->line_length = dinfo->pitch;
	disp->type = FB_TYPE_PACKED_PIXELS;
	disp->type_aux = 0;
	disp->ypanstep = 1;
	disp->ywrapstep = 0;
#else
	dinfo->info.screen_base = (char *)dinfo->fb_base;
	dinfo->info.fix.line_length = dinfo->pitch;
	dinfo->info.fix.visual = dinfo->visual;
#endif
	disp->can_soft_blank = 1;
	disp->inverse = 0;
	DBG_MSG("disp->scrollmode is 0x%x\n", disp->scrollmode);
	if (disp->var.yres_virtual == disp->var.yres &&
	    !(disp->var.accel_flags & FB_ACCELF_TEXT)) {
		/* No space for panning. */
		disp->scrollmode = SCROLL_YREDRAW;
	} else {
		disp->scrollmode = __SCROLL_YMOVE;
	}
}

/* fbops functions */

static int
intelfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	struct display *disp;

	DBG_MSG("intelfb_get_fix\n");

	disp = GET_DISP(info, con);

	memset(fix, 0, sizeof(*fix));
	strcpy(fix->id, dinfo->name);
	fix->smem_start = dinfo->fb_base_phys;
	fix->smem_len = dinfo->video_ram;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	fix->visual = dinfo->visual;
	fix->xpanstep = 8;
	fix->ypanstep = 1;
	fix->ywrapstep = 0;
	fix->line_length = dinfo->pitch;
	fix->mmio_start = dinfo->mmio_base_phys;
	fix->mmio_len = INTEL_REG_SIZE;
	fix->accel = FB_ACCEL_NONE;
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static int
intelfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);

	DBG_MSG("intelfb_get_var\n");

	if (con == -1)
		*var = dinfo->disp.var;
	else
		*var = fb_display[con].var;

	return 0;
}
#endif

static int
intelfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct display *disp;
	int change_var = 0;
	struct fb_var_screeninfo v;
	struct intelfb_info *dinfo;
	int ret;
	static int first = 1;

	DBG_MSG("intelfb_set_var: con is %d, accel_flags is %d\n",
		con, var->accel_flags);

	dinfo = GET_DINFO(info);
	disp = GET_DISP(info, con);

	if (intelfbhw_validate_mode(dinfo, con, var) != 0)
		return -EINVAL;
	
	memcpy(&v, var, sizeof(v));

	/* Check for a supported bpp. */
	if (v.bits_per_pixel <= 8) {
#ifdef FBCON_HAS_CFB8
		v.bits_per_pixel = 8;
#else
		return -EINVAL;
#endif
	} else if (v.bits_per_pixel <= 16) {
#ifdef FBCON_HAS_CFB16
		if (v.bits_per_pixel == 16)
			v.green.length = 6;
		v.bits_per_pixel = 16;
#else
		return -EINVAL;
#endif
	} else if (v.bits_per_pixel <= 32) {
#ifdef FBCON_HAS_CFB32
		v.bits_per_pixel = 32;
#else
		return -EINVAL;
#endif
	} else
		return -EINVAL;

	if (con < 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		info->var = *var;
#endif
		return 0;
	}


	change_var = ((disp->var.xres != var->xres) ||
		      (disp->var.yres != var->yres) ||
		      (disp->var.xres_virtual != var->xres_virtual) ||
		      (disp->var.yres_virtual != var->yres_virtual) ||
		      (disp->var.bits_per_pixel != var->bits_per_pixel) ||
		      memcmp(&disp->var.red, &var->red, sizeof(var->red)) ||
		      memcmp(&disp->var.green, &var->green,
			     sizeof(var->green)) ||
		      memcmp(&disp->var.blue, &var->blue, sizeof(var->blue)));

	if (dinfo->fixed_mode &&
	    (change_var ||
	     var->yres_virtual > dinfo->initial_var.yres_virtual ||
	     var->yres_virtual < dinfo->initial_var.yres ||
	     var->xoffset || var->nonstd)) {
		if (first) {
			ERR_MSG("Changing the video mode is not supported.\n");
			first = 0;
		}
		return -EINVAL;
	}

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW)
		return 0;

	switch (intelfb_var_to_depth(&v)) {
#ifdef FBCON_HAS_CFB8
	case 8:
		v.red.offset = v.green.offset = v.blue.offset = 0;
		v.red.length = v.green.length = v.blue.length = 8;
		v.transp.offset = v.transp.length = 0;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
		v.red.offset = 10;
		v.green.offset = 5;
		v.blue.offset = 0;
		v.red.length = v.green.length = v.blue.length = 5;
		v.transp.offset = v.transp.length = 0;
		break;
	case 16:
		v.red.offset = 11;
		v.green.offset = 5;
		v.blue.offset = 0;
		v.red.length = 5;
		v.green.length = 6;
		v.blue.length = 5;
		v.transp.offset = v.transp.length = 0;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 24:
		v.red.offset = 16;
		v.green.offset = 8;
		v.blue.offset = 0;
		v.red.length = v.green.length = v.blue.length = 8;
		v.transp.offset = v.transp.length = 0;
		break;
	case 32:
		v.red.offset = 16;
		v.green.offset = 8;
		v.blue.offset = 0;
		v.red.length = v.green.length = v.blue.length = 8;
		v.transp.offset = 24;
		v.transp.length = 8;
		break;
#endif
	}

	if (v.xoffset < 0)
		v.xoffset = 0;
	if (v.yoffset < 0)
		v.yoffset = 0;

	if (v.xoffset > v.xres_virtual - v.xres)
		v.xoffset = v.xres_virtual - v.xres;
	if (v.yoffset > v.yres_virtual - v.yres)
		v.yoffset = v.yres_virtual - v.yres;

	v.red.msb_right = v.green.msb_right = v.blue.msb_right =
			  v.transp.msb_right = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	info->var = v;
#endif

	memcpy(&disp->var, &v, sizeof(v));

	update_dinfo(dinfo, &v, disp);

	intelfb_set_dispsw(dinfo, disp);

	if (change_var) {
		if (info && info->changevar)
			info->changevar(con);
	}

	intelfb_blank(1, info);

#if 0
	if (dinfo->hwcursor)
		intelfbhw_cursor_hide(dinfo);
#endif

	intelfbhw_2d_stop(dinfo);

	if (!dinfo->fixed_mode) {
		mdelay(100);
		if ((ret = intelfb_set_mode(dinfo, &v, disp, 1)))
			return ret;

		mdelay(100);
	}

	if (dinfo->vc_mode == KD_TEXT) {
		intelfb_do_install_cmap(con, info);

		if (dinfo->hwcursor) {
			del_timer(&dinfo->cursor.timer);
			dinfo->cursor.state = CM_ERASE;
			if (disp->conp) {
				intelfbhw_cursor_init(dinfo);
				intelfbhw_cursor_load(dinfo, disp);
				dinfo->cursor.redraw = 1;
			}
		}

		mdelay(100);

		intelfbhw_2d_start(dinfo);
#if 0
		if (dinfo->hwcursor)
			intelfbhw_cursor_show(dinfo);
#endif

		if (v.yoffset != 0)
			intelfbhw_pan_display(&v, con, info);
	} else {
		dinfo->cursor.enabled = 0;
	}

	intelfb_blank(0, info);

	return 0;
}

static int
intelfb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	struct display *disp = GET_DISP(info, con);

	DBG_MSG("intelfb_get_cmap: con = %d, bpp = %d\n", con,
		disp->var.bits_per_pixel);

	if (con == dinfo->currcon)
		return fb_get_cmap(cmap, kspc, intelfb_getcolreg, info);
	else if (disp->cmap.len)
		fb_copy_cmap(&disp->cmap, cmap, kspc ? 0 : 2);
	else {
		int cmap_len = (disp->var.bits_per_pixel > 8) ? 16 : 256;
		fb_copy_cmap(fb_default_cmap(cmap_len), cmap, kspc ? 0 : 2);
	}
	return 0;
}

static int
intelfb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	struct display *disp = GET_DISP(info, con);
	unsigned int cmap_len;
	int err;

	DBG_MSG("intelfb_set_cmap: con = %d, bpp = %d\n", con,
		disp->var.bits_per_pixel);

	cmap_len = (disp->var.bits_per_pixel > 8) ? 16 : 256;
	if (!disp->cmap.len) {
		err = fb_alloc_cmap(&disp->cmap, cmap_len, 0);
		if (err)
			return err;
	}
	if (con == dinfo->currcon)
		return FB_SET_CMAP(cmap, kspc, intelfb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &disp->cmap, kspc ? 0 : 1);
	
	return 0;
}

/* When/if we have our own ioctls. */
static int
intelfb_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	      unsigned long arg, int con, struct fb_info *info)
{
	DBG_MSG("intelfb_ioctl\n");
	return -EINVAL;
}

static int
intelfb_switch(int con, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	struct display *disp;
	struct fb_cmap *cmap;
	int switchmode = 0;

	DBG_MSG("intelfb_switch: con = %d, currcon = %d\n",
		con, dinfo->currcon);

	disp = GET_DISP(info, con);
	if (dinfo->currcon >= 0) {
		cmap = &dinfo->currcon_display->cmap;
		if (cmap->len)
			fb_get_cmap(cmap, 1, intelfb_getcolreg, info);
	}

	switchmode = (con != dinfo->currcon);

	dinfo->currcon = con;
	dinfo->vc_mode = con >= 0 ? vt_cons[con]->vc_mode : KD_TEXT;
	dinfo->currcon_display = disp;
	disp->var.activate = FB_ACTIVATE_NOW;

	if (switchmode) {
		intelfb_set_var(&disp->var, con, info);
		intelfb_do_install_cmap(con, info);
	}

	return 0;
}

/* Called whenever there's panning. */
static int
intelfb_updatevar(int con, struct fb_info *info)
{
	struct display *disp = GET_DISP(info, con);

	DBG_MSG("intelfb_updatevar\n");

	if (con < 0)
		return -EINVAL;
	else
		return intelfbhw_pan_display(&disp->var, con, info);
}

static int
intelfb_blank(int blank, struct fb_info *info)
{
	intelfbhw_do_blank(blank, info);
	return 0;
}

static int
intelfb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
		  unsigned *blue, unsigned *transp, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);

#if 0
	DBG_MSG("intelfb_getcolreg\n");
#endif

	if (regno > 255)
		return 1;

	*red = (dinfo->palette[regno].red<<8) | dinfo->palette[regno].red;
	*green = (dinfo->palette[regno].green<<8) | dinfo->palette[regno].green;
	*blue = (dinfo->palette[regno].blue<<8) | dinfo->palette[regno].blue;
	*transp = 0;

	return 0;
}

static int
intelfb_setcolreg(unsigned regno, unsigned red, unsigned green,
		  unsigned blue, unsigned transp, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);

#if 1
	DBG_MSG("intelfb_setcolreg: regno %d, depth %d\n", regno, dinfo->depth);
#endif

	if ((dinfo->depth > 8 && regno > 16) || regno > 255)
		return 1;

	switch (dinfo->depth) {
#ifdef FBCON_HAS_CFB8
	case 8:
		{
			red >>= 8;
			green >>= 8;
			blue >>= 8;

			dinfo->palette[regno].red = red;
			dinfo->palette[regno].green = green;
			dinfo->palette[regno].blue = blue;

			intelfbhw_setcolreg(dinfo, regno, red, green, blue,
					    transp);
		}
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
		dinfo->con_cmap.cfb16[regno] = ((red & 0xf800) >>  1) |
					       ((green & 0xf800) >>  6) |
					       ((blue & 0xf800) >> 11);
		break;
	case 16:
		dinfo->con_cmap.cfb16[regno] = (red & 0xf800) |
					       ((green & 0xfc00) >>  5) |
					       ((blue  & 0xf800) >> 11);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 24:
		dinfo->con_cmap.cfb32[regno] = ((red & 0xff00) << 8) |
					       (green & 0xff00) |
					       ((blue  & 0xff00) >> 8);
		break;
#endif
	}
	return 0;
}

/* Convert a mode to a var, also making the bpp a supported value. */
static void
mode_to_var(const struct fb_videomode *mode, struct fb_var_screeninfo *var,
	    u32 bpp)
{
	if (!mode || !var)
		return;

	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres;
	var->xoffset = 0;
	var->yoffset = 0;
	if (bpp <= 8)
		var->bits_per_pixel = 8;
	else if (bpp <= 16) {
		if (bpp == 16)
			var->green.length = 6;
		var->bits_per_pixel = 16;
	} else if (bpp <= 32)
		var->bits_per_pixel = 32;
	else {
		WRN_MSG("var_to_mode: bad bpp: %d\n", bpp);
		var->bits_per_pixel = bpp;
	}
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode;
	var->width = -1;
	var->height = -1;
}

static int
intelfb_set_mode(struct intelfb_info *dinfo, struct fb_var_screeninfo *var,
		 struct display *disp, int blank)
{
	struct intelfb_hwstate hw;

	DBG_MSG("intelfb_set_mode (%dx%d-%d)\n", var->xres, var->yres,
		intelfb_var_to_depth(var));

	memcpy(&hw, &dinfo->save_state, sizeof(hw));
	if (intelfbhw_mode_to_hw(dinfo, &hw, var))
		return -EINVAL;
	intelfbhw_print_hw_state(dinfo, &hw);
	if (intelfbhw_program_mode(dinfo, &hw, blank))
		return -EINVAL;

	update_dinfo(dinfo, var, disp);

	return 0;
}

static void
intelfb_do_install_cmap(int con, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	struct display *disp;

	if (con != dinfo->currcon)
		return;

	disp = GET_DISP(info, con);
	if (disp->cmap.len)
		FB_SET_CMAP(&disp->cmap, 1, intelfb_setcolreg, info);
	else {
		int size = (disp->var.bits_per_pixel > 8) ? 16 : 256;
		FB_SET_CMAP(fb_default_cmap(size), 1, intelfb_setcolreg, info);
	}
}

static void
check_vc_mode(struct intelfb_info *dinfo)
{
#if VERBOSE > 1
	DBG_MSG("check_vc_mode\n");
#endif

	if (dinfo->currcon >= 0) {
		if (vt_cons[dinfo->currcon]->vc_mode != dinfo->vc_mode) {
			dinfo->vc_mode = vt_cons[dinfo->currcon]->vc_mode;
			if (dinfo->vc_mode == KD_GRAPHICS) {
				DBG_MSG("vc_mode changed to KD_GRAPHICS\n");
				intelfbhw_2d_stop(dinfo);
				if (dinfo->cursor.enabled) {
					intelfbhw_cursor_hide(dinfo);
					dinfo->cursor.enabled = 1;
				}
			} else {
				DBG_MSG("vc_mode changed to KD_TEXT\n");
				intelfbhw_2d_start(dinfo);
#if 0
				if (dinfo->cursor.enabled)
					intelfbhw_cursor_show(dinfo);
#endif
			}
		}
	}
}

static void
fbcon_intelfb_setup(struct display *p)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

	DBG_MSG("fbcon_intelfb_setup: accel: %d bpp: %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel);

	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.setup(p);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.setup(p);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.setup(p);
		break;
#endif
	}
}

static void
fbcon_intelfb_bmove(struct display *p, int sy, int sx, int dy, int dx,
		    int height, int width)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_bmove: accel: %d, bpp: %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel);
#endif

	if (TEXT_ACCEL(dinfo, &p->var)) {
		intelfbhw_do_bitblt(dinfo, fontwidth(p) * sx,
				    fontheight(p) * sy,
				    fontwidth(p) * dx, fontheight(p) * dy,
				    fontwidth(p) * width,
				    fontheight(p) * height,
				    dinfo->pitch, p->var.bits_per_pixel);
		intelfbhw_do_sync(dinfo);
		return;
	}

	/* Non-accel fallback */
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.bmove(p, sy, sx, dy, dx, height, width);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.bmove(p, sy, sx, dy, dx, height, width);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.bmove(p, sy, sx, dy, dx, height, width);
		break;
#endif
	}
}

static void
fbcon_intelfb_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		    int height, int width)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_clear: accel: %d, bpp: %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel);
#endif

	if (TEXT_ACCEL(dinfo, &p->var)) {
		u32 bg = 0;

		switch (p->var.bits_per_pixel) {
		case 8:
			bg = attr_bgcol_ec(p, conp);
			break;
		case 16:
			bg = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
			break;
		case 32:
			bg = ((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
			break;
		}
		intelfbhw_do_fillrect(dinfo, fontwidth(p) * sx,
				      fontheight(p) * sy, fontwidth(p) * width,
				      fontheight(p) * height, bg, dinfo->pitch,
				      p->var.bits_per_pixel, PAT_ROP_GXCOPY);
		intelfbhw_do_sync(dinfo);
		return;
	}

	/* Non-accel fallback */
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.clear(conp, p, sy, sx, height, width);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.clear(conp, p, sy, sx, height, width);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.clear(conp, p, sy, sx, height, width);
		break;
#endif
	}
}

static void
fbcon_intelfb_putc(struct vc_data *conp, struct display *p, int c,
		   int yy, int xx)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_putc: accel: %d, bpp: %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel);
#endif

	/* intelfbhw_do_drawglyph() has problems with the 852GM/855GM */
	if (TEXT_ACCEL(dinfo, &p->var) && USE_DRAWGLYPH(dinfo)) {
		u32 bg = 0, fg = 0;
		u8 *cdat;
		int fw;

		switch (p->var.bits_per_pixel) {
		case 8:
			bg = attr_bgcol(p, c);
			fg = attr_fgcol(p, c);
			break;
		case 16:
			bg = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
			fg = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
			break;
		case 32:
			bg = ((u32 *)p->dispsw_data)[attr_bgcol(p, c)];
			fg = ((u32 *)p->dispsw_data)[attr_fgcol(p, c)];
			break;
		}

		fw = ROUND_UP_TO(fontwidth(p), 8) / 8;
		cdat = p->fontdata + (c & p->charmask) * fontheight(p) * fw;

		if (intelfbhw_do_drawglyph(dinfo, fg, bg, fontwidth(p),
					   fontheight(p), cdat,
					   xx * fontwidth(p),
					   yy * fontheight(p), dinfo->pitch,
					   p->var.bits_per_pixel)) {
			intelfbhw_do_sync(dinfo);
			return;
		}
	}

	if (TEXT_ACCEL(dinfo, &p->var))
		intelfbhw_do_sync(dinfo);

	/* Non-accel fallback */
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.putc(conp, p, c, yy, xx);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.putc(conp, p, c, yy, xx);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.putc(conp, p, c, yy, xx);
		break;
#endif
	}
}

static void
fbcon_intelfb_putcs(struct vc_data *conp, struct display *p,
		    const unsigned short *s, int count, int yy, int xx)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_putcs: accel: %d, bpp: %d, (%d,%d) %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel,
		xx, yy, count);
#endif

	if (TEXT_ACCEL(dinfo, &p->var) && USE_DRAWGLYPH(dinfo)) {
		u32 bg = 0, fg = 0;
		u8 *cdat;
		int fw;
		u16 c = scr_readw(s);

		switch (p->var.bits_per_pixel) {
		case 8:
			bg = attr_bgcol(p, c);
			fg = attr_fgcol(p, c);
			break;
		case 16:
			bg = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
			fg = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
			break;
		case 32:
			bg = ((u32 *)p->dispsw_data)[attr_bgcol(p, c)];
			fg = ((u32 *)p->dispsw_data)[attr_fgcol(p, c)];
			break;
		}
		fw = ROUND_UP_TO(fontwidth(p), 8) / 8;
		while (count--) {
			cdat = p->fontdata + (scr_readw(s++) & p->charmask) *
			       fontheight(p) * fw;

			intelfbhw_do_drawglyph(dinfo, fg, bg, fontwidth(p),
					       fontheight(p), cdat,
					       xx * fontwidth(p),
					       yy * fontheight(p),
					       dinfo->pitch,
					       p->var.bits_per_pixel);
			xx++;
		}
		intelfbhw_do_sync(dinfo);
		return;
	}

	if (TEXT_ACCEL(dinfo, &p->var))
		intelfbhw_do_sync(dinfo);

	/* Non-accel fallback */
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.putcs(conp, p, s, count, yy, xx);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.putcs(conp, p, s, count, yy, xx);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.putcs(conp, p, s, count, yy, xx);
		break;
#endif
	}
}

static void
fbcon_intelfb_revc(struct display *p, int xx, int yy)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_revc: accel: %d, bpp: %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel);
#endif

	if (TEXT_ACCEL(dinfo, &p->var)) {
		int bpp = p->var.bits_per_pixel;
		u32 xor_mask = bpp == 8 ? 0x0f : 0xffffffff;

		intelfbhw_do_fillrect(dinfo, xx * fontwidth(p),
				      yy * fontheight(p), fontwidth(p),
				      fontheight(p), xor_mask, dinfo->pitch,
				      bpp, PAT_ROP_GXXOR);
		intelfbhw_do_sync(dinfo);
		return;
	}

	/* Non-accel fallback */
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.revc(p, xx, yy);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.revc(p, xx, yy);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.revc(p, xx, yy);
		break;
#endif
	}
}

static void
fbcon_intelfb_clear_margins(struct vc_data *conp, struct display *p,
			    int bottom_only)
{
	struct intelfb_info *dinfo = GET_DINFO(p->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_clear_margins: accel: %d, bpp: %d\n",
		TEXT_ACCEL(dinfo, &p->var), p->var.bits_per_pixel);
#endif

	if (TEXT_ACCEL(dinfo, &p->var)) {
		u32 cw, ch, rw, bh, rs, bs;

		cw = fontwidth(p);
		ch = fontheight(p);
		rw = p->var.xres % cw;
		bh = p->var.yres % ch;
		rs = p->var.xres - rw;
		bs = p->var.yres - bh;

		if (!bottom_only && rw) {
			intelfbhw_do_fillrect(dinfo, p->var.xoffset + rs, 0,
					      rw, p->var.yres_virtual, 0,
					      dinfo->pitch,
					      p->var.bits_per_pixel,
					      PAT_ROP_GXCOPY);
		}
		if (bh) {
			intelfbhw_do_fillrect(dinfo, p->var.xoffset,
					      p->var.yoffset + bs, rs, bh, 0,
					      dinfo->pitch,
					      p->var.bits_per_pixel,
					      PAT_ROP_GXCOPY);
		}
		if ((!bottom_only && rw) || bh) {
			intelfbhw_do_sync(dinfo);
		}
		return;
	}

	/* Non-accel fallback */
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:
		fbcon_cfb8.clear_margins(conp, p, bottom_only);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		fbcon_cfb16.clear_margins(conp, p, bottom_only);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		fbcon_cfb32.clear_margins(conp, p, bottom_only);
		break;
#endif
	}
}

void
intelfb_create_cursor_shape(struct intelfb_info *dinfo, struct display *disp)
{
	u32 h, cu, cd;

	DBG_MSG("intelfb_create_cursor_shape\n");

	h = fontheight(disp);
	cd = h;
	if (cd >= 10)
		cd --;
	dinfo->cursor.type = disp->conp->vc_cursor_type & CUR_HWMASK;
	switch (dinfo->cursor.type) {
	case CUR_NONE:
		cu = cd;
		break;
	case CUR_UNDERLINE:
		cu = cd - 2;
		break;
	case CUR_LOWER_THIRD:
		cu = (h * 2) / 3;
		break;
	case CUR_LOWER_HALF:
		cu = h / 2;
		break;
	case CUR_TWO_THIRDS:
		cu = h / 3;
		break;
	case CUR_BLOCK:
	default:
		cu = 0;
		cd = h;
		break;
	}
	dinfo->cursor.w = fontwidth(disp);
	dinfo->cursor.u = cu;
	dinfo->cursor.d = cd;
}

static void
intelfb_flashcursor(unsigned long ptr)
{
	struct intelfb_info *dinfo = (struct intelfb_info *)ptr;
	unsigned long flags;

#if VERBOSE > 2
	DBG_MSG("intelfb_flashcursor\n");
#endif

	spin_lock_irqsave(&dinfo->DAClock, flags);
	if (dinfo->cursor.enabled) {
		if (dinfo->cursor.on)
			intelfbhw_cursor_hide(dinfo);
		else
			intelfbhw_cursor_show(dinfo);
		dinfo->cursor.enabled = 1;
	}
	dinfo->cursor.timer.expires = jiffies + HZ / 2;
	add_timer(&dinfo->cursor.timer);
	spin_unlock_irqrestore(&dinfo->DAClock, flags);
}

static void
fbcon_intelfb_cursor(struct display *disp, int mode, int x, int y)
{
	unsigned long flags = 0;
	struct intelfb_info *dinfo = GET_DINFO(disp->fb_info);

#if VERBOSE > 0
	DBG_MSG("fbcon_intelfb_cursor, mode is %d (%d)\n",
		mode, dinfo->cursor.state);
#endif

	/*
	 * This appears to be the first place that we can detect changes
	 * in vc_mode.  We need to know this so that acceleration can be
	 * enabled or disabled so that it won't interfere with an XFree86
	 * server.
	 */
	check_vc_mode(dinfo);

	if (!dinfo->hwcursor) {
		disp->dispsw->cursor = NULL;
		fb_con.con_cursor(disp->conp, mode);
		disp->dispsw->cursor = fbcon_intelfb_cursor;
		return;
	}

	if (mode == CM_ERASE) {
		if (dinfo->cursor.state != CM_ERASE) {
			spin_lock_irqsave(&dinfo->DAClock, flags);
			dinfo->cursor.state = CM_ERASE;
			intelfbhw_cursor_hide(dinfo);
			del_timer(&dinfo->cursor.timer);
			spin_unlock_irqrestore(&dinfo->DAClock, flags);
		}
		return;
	}
	if ((disp->conp->vc_cursor_type & CUR_HWMASK) != dinfo->cursor.type)
		intelfbhw_cursor_load(dinfo, disp);
	x *= fontwidth(disp);
	y *= fontheight(disp);
	y -= disp->var.yoffset;
	x -= disp->var.xoffset;
	spin_lock_irqsave(&dinfo->DAClock, flags);
	if (x != dinfo->cursor.x || y != dinfo->cursor.y ||
	    dinfo->cursor.redraw) {
#if 1
		intelfbhw_cursor_hide(dinfo);
#endif
		intelfbhw_cursor_setcolor(dinfo, 0, 0xffffff);
		intelfbhw_cursor_setpos(dinfo, x, y);
	}
	dinfo->cursor.state = CM_DRAW;
	mod_timer(&dinfo->cursor.timer, jiffies + HZ / 2);
	intelfbhw_cursor_show(dinfo);
	spin_unlock_irqrestore(&dinfo->DAClock, flags);
}

