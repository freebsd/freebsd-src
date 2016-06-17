/*
 *  linux/drivers/video/S3Triofb.c -- Open Firmware based frame buffer device
 *
 *	Copyright (C) 1997 Peter De Schrijver
 *
 *  This driver is partly based on the PowerMac console driver:
 *
 *	Copyright (C) 1996 Paul Mackerras
 *
 *  and on the Open Firmware based frame buffer device:
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

/*
	Bugs : + This driver should be merged with the CyberVision driver. The
                 CyberVision is a Zorro III implementation of the S3Trio64 chip.

*/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/selection.h>
#include <linux/pci.h>
#ifdef CONFIG_FB_COMPAT_XPMAC
#include <asm/vc_ioctl.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/s3blit.h>


#define mem_in8(addr)           in_8((void *)(addr))
#define mem_in16(addr)          in_le16((void *)(addr))
#define mem_in32(addr)          in_le32((void *)(addr))

#define mem_out8(val, addr)     out_8((void *)(addr), val)
#define mem_out16(val, addr)    out_le16((void *)(addr), val)
#define mem_out32(val, addr)    out_le32((void *)(addr), val)

#define IO_OUT16VAL(v, r)       (((v) << 8) | (r))


static int currcon = 0;
static int disabled;
static struct display disp;
static struct fb_info fb_info;
static struct { u_char red, green, blue, pad; } palette[256];
static char s3trio_name[16] = "S3Trio ";
static char *s3trio_base;

static struct fb_fix_screeninfo fb_fix;
static struct fb_var_screeninfo S3triofb_default_var = {
	640, 480, 640, 480, 0, 0, 8, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, FB_ACCELF_TEXT, 39722, 40, 24, 32, 11, 96, 2,
	FB_SYNC_COMP_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED
};
static struct fb_var_screeninfo fb_var = { 0,};

    /*
     *  Interface used by the world
     */

static void __init s3triofb_pci_init(struct pci_dev *dp);
static int s3trio_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info);
static int s3trio_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
static int s3trio_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
static int s3trio_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info);
static int s3trio_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info);
static int s3trio_pan_display(struct fb_var_screeninfo *var, int con,
			      struct fb_info *info);


    /*
     *  Interface to the low level console driver
     */

int s3triofb_init(void);
static int s3triofbcon_switch(int con, struct fb_info *info);
static int s3triofbcon_updatevar(int con, struct fb_info *info);
static void s3triofbcon_blank(int blank, struct fb_info *info);
#if 0
static int s3triofbcon_setcmap(struct fb_cmap *cmap, int con);
#endif

    /*
     *  Text console acceleration
     */

#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_trio8;
#endif

    /*
     *    Accelerated Functions used by the low level console driver
     */

static void Trio_WaitQueue(u_short fifo);
static void Trio_WaitBlit(void);
static void Trio_BitBLT(u_short curx, u_short cury, u_short destx,
			u_short desty, u_short width, u_short height,
			u_short mode);
static void Trio_RectFill(u_short x, u_short y, u_short width, u_short height,
			  u_short mode, u_short color);
static void Trio_MoveCursor(u_short x, u_short y);


    /*
     *  Internal routines
     */

static int s3trio_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info);
static int s3trio_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);


static struct fb_ops s3trio_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	s3trio_get_fix,
	fb_get_var:	s3trio_get_var,
	fb_set_var:	s3trio_set_var,
	fb_get_cmap:	s3trio_get_cmap,
	fb_set_cmap:	s3trio_set_cmap,
	fb_pan_display:	s3trio_pan_display,
};

    /*
     *  Get the Fixed Part of the Display
     */

static int s3trio_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info)
{
    memcpy(fix, &fb_fix, sizeof(fb_fix));
    return 0;
}


    /*
     *  Get the User Defined Part of the Display
     */

static int s3trio_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
    memcpy(var, &fb_var, sizeof(fb_var));
    return 0;
}


    /*
     *  Set the User Defined Part of the Display
     */

static int s3trio_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
    if (var->xres > fb_var.xres || var->yres > fb_var.yres ||
	var->bits_per_pixel > fb_var.bits_per_pixel )
	/* || var->nonstd || var->vmode != FB_VMODE_NONINTERLACED) */
	return -EINVAL;
    if (var->xres_virtual > fb_var.xres_virtual) {
	outw(IO_OUT16VAL((var->xres_virtual /8) & 0xff, 0x13), 0x3d4);
	outw(IO_OUT16VAL(((var->xres_virtual /8 ) & 0x300) >> 3, 0x51), 0x3d4);
	fb_var.xres_virtual = var->xres_virtual;
	fb_fix.line_length = var->xres_virtual;
    }
    fb_var.yres_virtual = var->yres_virtual;
    memcpy(var, &fb_var, sizeof(fb_var));
    return 0;
}


    /*
     *  Pan or Wrap the Display
     *
     *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
     */

static int s3trio_pan_display(struct fb_var_screeninfo *var, int con,
			      struct fb_info *info)
{
    unsigned int base;

    if (var->xoffset > (var->xres_virtual - var->xres))
	return -EINVAL;
    if (var->yoffset > (var->yres_virtual - var->yres))
	return -EINVAL;

    fb_var.xoffset = var->xoffset;
    fb_var.yoffset = var->yoffset;

    base = var->yoffset * fb_fix.line_length + var->xoffset;

    outw(IO_OUT16VAL((base >> 8) & 0xff, 0x0c),0x03D4);
    outw(IO_OUT16VAL(base  & 0xff, 0x0d),0x03D4);
    outw(IO_OUT16VAL((base >> 16) & 0xf, 0x69),0x03D4);
    return 0;
}


    /*
     *  Get the Colormap
     */

static int s3trio_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
    if (con == currcon) /* current console? */
	return fb_get_cmap(cmap, kspc, s3trio_getcolreg, info);
    else if (fb_display[con].cmap.len) /* non default colormap? */
	fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
    else
	fb_copy_cmap(fb_default_cmap(1 << fb_display[con].var.bits_per_pixel),
		     cmap, kspc ? 0 : 2);
    return 0;
}

    /*
     *  Set the Colormap
     */

static int s3trio_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
    int err;


    if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
	if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				 1<<fb_display[con].var.bits_per_pixel, 0)))
	    return err;
    }
    if (con == currcon)			/* current console? */
	return fb_set_cmap(cmap, kspc, s3trio_setcolreg, info);
    else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}

int __init s3triofb_setup(char *options) {
	char *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;

		if (!strcmp(this_opt, "disabled"))
			disabled = 1;
	}
	return 0;
}

int __init s3triofb_init(void)
{
	struct pci_dev *dp = NULL;

	if (disabled)
		return -ENXIO;

	dp = pci_find_device(PCI_VENDOR_ID_S3, PCI_DEVICE_ID_S3_TRIO, dp);
	if ((dp != 0) && ((dp->class >> 16) == PCI_BASE_CLASS_DISPLAY)) {
		s3triofb_pci_init(dp);
		return 0;
	} else
		return -ENODEV;
}

static void __exit s3triofb_exit(void)
{
    unregister_framebuffer(&fb_info);
    iounmap(s3trio_base);
    /* XXX unshare VGA regions */
}

void __init s3trio_resetaccel(void){


#define EC01_ENH_ENB    0x0005
#define EC01_LAW_ENB    0x0010
#define EC01_MMIO_ENB   0x0020

#define EC00_RESET      0x8000
#define EC00_ENABLE     0x4000
#define MF_MULT_MISC    0xE000
#define SRC_FOREGROUND  0x0020
#define SRC_BACKGROUND  0x0000
#define MIX_SRC                 0x0007
#define MF_T_CLIP       0x1000
#define MF_L_CLIP       0x2000
#define MF_B_CLIP       0x3000
#define MF_R_CLIP       0x4000
#define MF_PIX_CONTROL  0xA000
#define MFA_SRC_FOREGR_MIX      0x0000
#define MF_PIX_CONTROL  0xA000

	outw(EC00_RESET,  0x42e8);
	inw(  0x42e8);
	outw(EC00_ENABLE,  0x42e8);
	inw(  0x42e8);
	outw(EC01_ENH_ENB | EC01_LAW_ENB,
		   0x4ae8);
	outw(MF_MULT_MISC,  0xbee8); /* 16 bit I/O registers */

	/* Now set some basic accelerator registers */
	Trio_WaitQueue(0x0400);
	outw(SRC_FOREGROUND | MIX_SRC, 0xbae8);
	outw(SRC_BACKGROUND | MIX_SRC,  0xb6e8);/* direct color*/
	outw(MF_T_CLIP | 0, 0xbee8 );     /* clip virtual area  */
	outw(MF_L_CLIP | 0, 0xbee8 );
	outw(MF_R_CLIP | (640 - 1), 0xbee8);
	outw(MF_B_CLIP | (480 - 1),  0xbee8);
	Trio_WaitQueue(0x0400);
	outw(0xffff,  0xaae8);       /* Enable all planes */
	outw(0xffff, 0xaae8);       /* Enable all planes */
	outw( MF_PIX_CONTROL | MFA_SRC_FOREGR_MIX,  0xbee8);
}

int __init s3trio_init(struct pci_dev *dp)
{
	/* unlock s3 */
	outb(0x01, 0x3C3);
	outb(inb(0x03CC) | 1, 0x3c2);
	outw(IO_OUT16VAL(0x48, 0x38),0x03D4);
	outw(IO_OUT16VAL(0xA0, 0x39),0x03D4);
	outb(0x33,0x3d4);
	outw(IO_OUT16VAL((inb(0x3d5) & ~(0x2 | 0x10 |  0x40)) |
			  0x20, 0x33), 0x3d4);

	outw(IO_OUT16VAL(0x6, 0x8), 0x3c4);
	printk("S3trio: unlocked s3\n");

	/* switch to MMIO only mode */
	outb(0x58, 0x3d4);
	outw(IO_OUT16VAL(inb(0x3d5) | 3 | 0x10, 0x58), 0x3d4);
	outw(IO_OUT16VAL(8, 0x53), 0x3d4);
	printk("S3trio: switched to mmio only mode\n");

	return 1;
}


    /*
     *  Initialisation
     */

static void __init s3triofb_pci_init(struct pci_dev *dp)
{
    int i;
    unsigned long address, size;
    u_long *CursorBase;
    u16 cmd;

    strcpy(fb_fix.id, s3trio_name);

    fb_var = S3triofb_default_var;
    fb_fix.line_length = fb_var.xres_virtual;
    fb_fix.smem_len = fb_fix.line_length*fb_var.yres;

    /* This driver cannot cope if the firmware has not initialised the
     * device.  So, if the device isn't enabled we simply return
     */
    pci_read_config_word(dp, PCI_COMMAND, &cmd);
    if (!(cmd & PCI_COMMAND_MEMORY)) {
	    printk(KERN_NOTICE "S3trio: card was not initialised by firmware\n");
	    return;
    }

    /* Enable it anyway */
    if (pci_enable_device(dp)) {
	    printk(KERN_ERR "S3trio: failed to enable PCI device\n");
	    return;
    }

    /* There is only one memory region and it covers the mmio and fb areas */
    address = pci_resource_start(dp, 0);
    size    = pci_resource_len(dp, 0); /* size = 64*1024*1024; */
    if (!request_mem_region(address, size, "S3triofb")) {
	printk("S3trio: failed to allocate memory region\n");
	return;
    }

    s3trio_init(dp);
    s3trio_base = ioremap(address, size);
    fb_fix.smem_start = address;
    fb_fix.type = FB_TYPE_PACKED_PIXELS;
    fb_fix.type_aux = 0;
    fb_fix.accel = FB_ACCEL_S3_TRIO64;
    fb_fix.mmio_start = address+0x1000000;
    fb_fix.mmio_len = 0x1000000;

    fb_fix.xpanstep = 1;
    fb_fix.ypanstep = 1;

    s3trio_resetaccel();

    mem_out8(0x30, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0x2d, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0x2e, s3trio_base+0x1008000 + 0x03D4);

    mem_out8(0x50, s3trio_base+0x1008000 + 0x03D4);

    /* disable HW cursor */

    mem_out8(0x39, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0xa0, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x45, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x4e, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x4f, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0, s3trio_base+0x1008000 + 0x03D5);

    /* init HW cursor */

    CursorBase = (u_long *)(s3trio_base + 2*1024*1024 - 0x400);
	for (i = 0; i < 8; i++) {
		*(CursorBase  +(i*4)) = 0xffffff00;
		*(CursorBase+1+(i*4)) = 0xffff0000;
		*(CursorBase+2+(i*4)) = 0xffff0000;
		*(CursorBase+3+(i*4)) = 0xffff0000;
	}
	for (i = 8; i < 64; i++) {
		*(CursorBase  +(i*4)) = 0xffff0000;
		*(CursorBase+1+(i*4)) = 0xffff0000;
		*(CursorBase+2+(i*4)) = 0xffff0000;
		*(CursorBase+3+(i*4)) = 0xffff0000;
	}


    mem_out8(0x4c, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(((2*1024 - 1)&0xf00)>>8, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x4d, s3trio_base+0x1008000 + 0x03D4);
    mem_out8((2*1024 - 1) & 0xff, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x45, s3trio_base+0x1008000 + 0x03D4);
    mem_in8(s3trio_base+0x1008000 + 0x03D4);

    mem_out8(0x4a, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0x80, s3trio_base+0x1008000 + 0x03D5);
    mem_out8(0x80, s3trio_base+0x1008000 + 0x03D5);
    mem_out8(0x80, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x4b, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0x00, s3trio_base+0x1008000 + 0x03D5);
    mem_out8(0x00, s3trio_base+0x1008000 + 0x03D5);
    mem_out8(0x00, s3trio_base+0x1008000 + 0x03D5);

    mem_out8(0x45, s3trio_base+0x1008000 + 0x03D4);
    mem_out8(0, s3trio_base+0x1008000 + 0x03D5);

    /* setup default color table */

	for(i = 0; i < 16; i++) {
		int j = color_table[i];
		palette[i].red=default_red[j];
		palette[i].green=default_grn[j];
		palette[i].blue=default_blu[j];
	}

    s3trio_setcolreg(255, 56, 100, 160, 0, NULL /* not used */);
    s3trio_setcolreg(254, 0, 0, 0, 0, NULL /* not used */);
    memset((char *)s3trio_base, 0, 640*480);

#if 0
    Trio_RectFill(0, 0, 90, 90, 7, 1);
#endif

    fb_fix.visual = FB_VISUAL_PSEUDOCOLOR ;
    fb_var.xoffset = fb_var.yoffset = 0;
    fb_var.bits_per_pixel = 8;
    fb_var.grayscale = 0;
    fb_var.red.offset = fb_var.green.offset = fb_var.blue.offset = 0;
    fb_var.red.length = fb_var.green.length = fb_var.blue.length = 8;
    fb_var.red.msb_right = fb_var.green.msb_right = fb_var.blue.msb_right = 0;
    fb_var.transp.offset = fb_var.transp.length = fb_var.transp.msb_right = 0;
    fb_var.nonstd = 0;
    fb_var.activate = 0;
    fb_var.height = fb_var.width = -1;
    fb_var.accel_flags = FB_ACCELF_TEXT;
#warning FIXME: always obey fb_var.accel_flags
    fb_var.pixclock = 1;
    fb_var.left_margin = fb_var.right_margin = 0;
    fb_var.upper_margin = fb_var.lower_margin = 0;
    fb_var.hsync_len = fb_var.vsync_len = 0;
    fb_var.sync = 0;
    fb_var.vmode = FB_VMODE_NONINTERLACED;

    disp.var = fb_var;
    disp.cmap.start = 0;
    disp.cmap.len = 0;
    disp.cmap.red = disp.cmap.green = disp.cmap.blue = disp.cmap.transp = NULL;
    disp.screen_base = s3trio_base;
    disp.visual = fb_fix.visual;
    disp.type = fb_fix.type;
    disp.type_aux = fb_fix.type_aux;
    disp.ypanstep = 0;
    disp.ywrapstep = 0;
    disp.line_length = fb_fix.line_length;
    disp.can_soft_blank = 1;
    disp.inverse = 0;
#ifdef FBCON_HAS_CFB8
    if (fb_var.accel_flags & FB_ACCELF_TEXT)
	disp.dispsw = &fbcon_trio8;
    else
	disp.dispsw = &fbcon_cfb8;
#else
    disp.dispsw = &fbcon_dummy;
#endif
    disp.scrollmode = fb_var.accel_flags & FB_ACCELF_TEXT ? 0 : SCROLL_YREDRAW;

    strcpy(fb_info.modename, "Trio64");
    fb_info.node = -1;
    fb_info.fbops = &s3trio_ops;
#if 0
    fb_info.fbvar_num = 1;
    fb_info.fbvar = &fb_var;
#endif
    fb_info.disp = &disp;
    fb_info.fontname[0] = '\0';
    fb_info.changevar = NULL;
    fb_info.switch_con = &s3triofbcon_switch;
    fb_info.updatevar = &s3triofbcon_updatevar;
    fb_info.blank = &s3triofbcon_blank;
#if 0
    fb_info.setcmap = &s3triofbcon_setcmap;
#endif

#ifdef CONFIG_FB_COMPAT_XPMAC
    if (!console_fb_info) {
	display_info.height = fb_var.yres;
	display_info.width = fb_var.xres;
	display_info.depth = 8;
	display_info.pitch = fb_fix.line_length;
	display_info.mode = 0;
	strncpy(display_info.name, dp->name, sizeof(display_info.name));
	display_info.fb_address = (unsigned long)fb_fix.smem_start;
	display_info.disp_reg_address = address + 0x1008000;
	display_info.cmap_adr_address = address + 0x1008000 + 0x3c8;
	display_info.cmap_data_address = address + 0x1008000 + 0x3c9;
	console_fb_info = &fb_info;
    }
#endif /* CONFIG_FB_COMPAT_XPMAC) */

    fb_info.flags = FBINFO_FLAG_DEFAULT;
    if (register_framebuffer(&fb_info) < 0)
	return;

    printk("fb%d: S3 Trio frame buffer device on %s\n",
	   GET_FB_IDX(fb_info.node), dp->name);
}


static int s3triofbcon_switch(int con, struct fb_info *info)
{
    /* Do we have to save the colormap? */
    if (fb_display[currcon].cmap.len)
	fb_get_cmap(&fb_display[currcon].cmap, 1, s3trio_getcolreg, info);

    currcon = con;
    /* Install new colormap */
    do_install_cmap(con,info);
    return 0;
}

    /*
     *  Update the `var' structure (called by fbcon.c)
     */

static int s3triofbcon_updatevar(int con, struct fb_info *info)
{
    /* Nothing */
    return 0;
}

    /*
     *  Blank the display.
     */

static void s3triofbcon_blank(int blank, struct fb_info *info)
{
    unsigned char x;

    mem_out8(0x1, s3trio_base+0x1008000 + 0x03c4);
    x = mem_in8(s3trio_base+0x1008000 + 0x03c5);
    mem_out8((x & (~0x20)) | (blank << 5), s3trio_base+0x1008000 + 0x03c5);
}

    /*
     *  Set the colormap
     */

#if 0
static int s3triofbcon_setcmap(struct fb_cmap *cmap, int con)
{
    return(s3trio_set_cmap(cmap, 1, con, &fb_info));
}
#endif


    /*
     *  Read a single color register and split it into
     *  colors/transparent. Return != 0 for invalid regno.
     */

static int s3trio_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
    if (regno > 255)
	return 1;
    *red = (palette[regno].red << 8) | palette[regno].red;
    *green = (palette[regno].green << 8) | palette[regno].green;
    *blue = (palette[regno].blue << 8) | palette[regno].blue;
    *transp = 0;
    return 0;
}


    /*
     *  Set a single color register. Return != 0 for invalid regno.
     */

static int s3trio_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                            u_int transp, struct fb_info *info)
{
    if (regno > 255)
	return 1;

    red >>= 8;
    green >>= 8;
    blue >>= 8;
    palette[regno].red = red;
    palette[regno].green = green;
    palette[regno].blue = blue;

    mem_out8(regno,s3trio_base+0x1008000 + 0x3c8);
    mem_out8((red & 0xff) >> 2,s3trio_base+0x1008000 + 0x3c9);
    mem_out8((green & 0xff) >> 2,s3trio_base+0x1008000 + 0x3c9);
    mem_out8((blue & 0xff) >> 2,s3trio_base+0x1008000 + 0x3c9);

    return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
    if (con != currcon)
	return;
    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, s3trio_setcolreg, &fb_info);
    else
	fb_set_cmap(fb_default_cmap(fb_display[con].var.bits_per_pixel), 1,
		    s3trio_setcolreg, &fb_info);
}

static void Trio_WaitQueue(u_short fifo) {

	u_short status;

        do
        {
		status = mem_in16(s3trio_base + 0x1000000 + 0x9AE8);
	}  while (!(status & fifo));

}

static void Trio_WaitBlit(void) {

	u_short status;

        do
        {
		status = mem_in16(s3trio_base + 0x1000000 + 0x9AE8);
	}  while (status & 0x200);

}

static void Trio_BitBLT(u_short curx, u_short cury, u_short destx,
			u_short desty, u_short width, u_short height,
			u_short mode) {

	u_short blitcmd = 0xc011;

	/* Set drawing direction */
        /* -Y, X maj, -X (default) */

	if (curx > destx)
		blitcmd |= 0x0020;  /* Drawing direction +X */
	else {
		curx  += (width - 1);
		destx += (width - 1);
	}

	if (cury > desty)
		blitcmd |= 0x0080;  /* Drawing direction +Y */
	else {
		cury  += (height - 1);
		desty += (height - 1);
	}

	Trio_WaitQueue(0x0400);

	outw(0xa000,  0xBEE8);
	outw(0x60 | mode,  0xBAE8);

	outw(curx,  0x86E8);
	outw(cury,  0x82E8);

	outw(destx,  0x8EE8);
	outw(desty,  0x8AE8);

	outw(height - 1,  0xBEE8);
	outw(width - 1,  0x96E8);

	outw(blitcmd,  0x9AE8);

}

static void Trio_RectFill(u_short x, u_short y, u_short width, u_short height,
			  u_short mode, u_short color) {

	u_short blitcmd = 0x40b1;

	Trio_WaitQueue(0x0400);

	outw(0xa000,  0xBEE8);
	outw((0x20 | mode),  0xBAE8);
	outw(0xe000,  0xBEE8);
	outw(color,  0xA6E8);
	outw(x,  0x86E8);
	outw(y,  0x82E8);
	outw((height - 1), 0xBEE8);
	outw((width - 1), 0x96E8);
	outw(blitcmd,  0x9AE8);

}


static void Trio_MoveCursor(u_short x, u_short y) {

	mem_out8(0x39, s3trio_base + 0x1008000 + 0x3d4);
	mem_out8(0xa0, s3trio_base + 0x1008000 + 0x3d5);

	mem_out8(0x46, s3trio_base + 0x1008000 + 0x3d4);
	mem_out8((x & 0x0700) >> 8, s3trio_base + 0x1008000 + 0x3d5);
	mem_out8(0x47, s3trio_base + 0x1008000 + 0x3d4);
	mem_out8(x & 0x00ff, s3trio_base + 0x1008000 + 0x3d5);

	mem_out8(0x48, s3trio_base + 0x1008000 + 0x3d4);
	mem_out8((y & 0x0700) >> 8, s3trio_base + 0x1008000 + 0x3d5);
	mem_out8(0x49, s3trio_base + 0x1008000 + 0x3d4);
	mem_out8(y & 0x00ff, s3trio_base + 0x1008000 + 0x3d5);

}


    /*
     *  Text console acceleration
     */

#ifdef FBCON_HAS_CFB8
static void fbcon_trio8_bmove(struct display *p, int sy, int sx, int dy,
			      int dx, int height, int width)
{
    sx *= 8; dx *= 8; width *= 8;
    Trio_BitBLT((u_short)sx, (u_short)(sy*fontheight(p)), (u_short)dx,
		 (u_short)(dy*fontheight(p)), (u_short)width,
		 (u_short)(height*fontheight(p)), (u_short)S3_NEW);
}

static void fbcon_trio8_clear(struct vc_data *conp, struct display *p, int sy,
			      int sx, int height, int width)
{
    unsigned char bg;

    sx *= 8; width *= 8;
    bg = attr_bgcol_ec(p,conp);
    Trio_RectFill((u_short)sx,
		   (u_short)(sy*fontheight(p)),
		   (u_short)width,
		   (u_short)(height*fontheight(p)),
		   (u_short)S3_NEW,
		   (u_short)bg);
}

static void fbcon_trio8_putc(struct vc_data *conp, struct display *p, int c,
			     int yy, int xx)
{
    Trio_WaitBlit();
    fbcon_cfb8_putc(conp, p, c, yy, xx);
}

static void fbcon_trio8_putcs(struct vc_data *conp, struct display *p,
			      const unsigned short *s, int count, int yy, int xx)
{
    Trio_WaitBlit();
    fbcon_cfb8_putcs(conp, p, s, count, yy, xx);
}

static void fbcon_trio8_revc(struct display *p, int xx, int yy)
{
    Trio_WaitBlit();
    fbcon_cfb8_revc(p, xx, yy);
}

static struct display_switch fbcon_trio8 = {
   setup:		fbcon_cfb8_setup,
   bmove:		fbcon_trio8_bmove,
   clear:		fbcon_trio8_clear,
   putc:		fbcon_trio8_putc,
   putcs:		fbcon_trio8_putcs,
   revc:		fbcon_trio8_revc,
   clear_margins:	fbcon_cfb8_clear_margins,
   fontwidthmask:	FONTWIDTH(8)
};
#endif

#ifdef MODULE
module_init(s3triofb_init);
MODULE_LICENSE("GPL");
#endif
module_exit(s3triofb_exit);
