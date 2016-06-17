/*
 * linux/drivers/video/sstfb.c -- voodoo graphics frame buffer
 *
 *     Copyright (c) 2000-2002 Ghozlane Toumi <gtoumi@laposte.net>
 *
 *     Created 15 Jan 2000 by Ghozlane Toumi
 *
 * Contributions (and many thanks) :
 *
 * 03/2001 James Simmons   <jsimmons@linux-fbdev.org>
 * 04/2001 Paul Mundt      <lethal@chaoticdreams.org>
 * 05/2001 Urs Ganse       <ursg@uni.de>
 *     (initial work on voodoo2 port, interlace)
 * 09/2002 Helge Deller    <deller@gmx.de>
 *     (enable driver on big-endian machines (hppa), ioctl fixes)
 *
 *
 * $Id: sstfb.c,v 1.26.4.1 2001/08/29 01:30:37 ghoz Exp $
 */

/*
 * The voodoo1 has the following memory mapped adress space:
 * 0x000000 - 0x3fffff : registers              (4Mb)
 * 0x400000 - 0x7fffff : linear frame buffer    (4Mb)
 * 0x800000 - 0xffffff : texture memory         (8Mb)
 */

/*
 * misc notes, TODOs, toASKs, and deep thoughts

-TODO: at one time or another test that the mode is acceptable by the monitor
-ASK: I can choose different ordering for the color bitfields (rgba argb ...)
      wich one should i use ? is there any preferred one ? It seems ARGB is
      the one ...
-TODO: check the error paths . if something get wrong, the error doesn't seem
      to be very well handled...if handled at all.. not good.
-TODO: in  set_var check the validity of timings (hsync vsync)...
-TODO: check and recheck the use of sst_wait_idle : we dont flush the fifo via
       a nop command . so it's ok as long as the commands we pass don't go
       through the fifo. warning: issuing a nop command seems to need pci_fifo
-FIXME: in case of failure in the init sequence, be sure we return to a safe
        state.
-FIXME: 4MB boards have banked memory (FbiInit2 bits 1 & 20)
-ASK: I stole "inverse" but seems it doesn't work... check what it realy does...
-TODO: change struct sst_info fb_info from static to array/dynamic

 *
 */

/*
 * debug info
 * SST_DEBUG : enable debugging
 * SST_DEBUG_REG : debug registers
 *   0 :  no debug
 *   1 : dac calls, [un]set_bits, FbiInit
 *   2 : insane debug level (log every register read/write)
 * SST_DEBUG_FUNC : functions
 *   0 : no debug
 *   1 : function call / debug ioctl
 *   2 : variables
 *   3 : flood . you don't want to do that. trust me.
 * SST_DEBUG_VAR : debug display/var structs
 *   0 : no debug
 *   1 : dumps display, fb_var
 * SST_DEBUG_IOCTL : enable sstfb specific ioctls
 *   0 : disable
 *   1 : enable debug ioctls :
 *   		toggle vga (0x46db) : toggle vga_pass_through
 *   		fill fb    (0x46dc) : fills fb
 *   		dump var   (0x46dd) : logs display[0-5].var
 *   		test disp  (0x46de) : draws a test motif
 */

/* #define SST_DEBUG */
#undef SST_DEBUG

#define SST_DEBUG_REG   0
#define SST_DEBUG_FUNC  0
#define SST_DEBUG_VAR   0
#define SST_DEBUG_IOCTL 1

/* #define EN_24_32_BPP  *//* enable 24/32 bpp functions for testing only */
#undef EN_24_32_BPP

/*
  Default video mode .
  0 800x600@60  took from glide
  1 640x480@75  took from glide
  2 1024x768@76 std fb.mode
  3 640x480@60  glide default */
#define DEFAULT_MODE 1
/*
 * Includes
 */

#include <linux/string.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>
#ifdef  EN_24_32_BPP
#  include <video/fbcon-cfb24.h>
#  include <video/fbcon-cfb32.h>
#endif

#include "sstfb.h"

/* void __Dump_regs(struct sstfb_info *);
#define Dump_regs __Dump_regs(sst_info) */

/********/


/* initialized by setup */
static int inverse; /* =0 */	/* invert colormap */
static int vgapass; /* =0 */	/* enable Vga passthrough cable */
static int mem;     /* =0 */	/* mem size in Mb , 0 = autodetect */
static int clipping = 1;	/* use clipping (slower, safer) */
static int gfxclk;  /* =0 */	/* force FBI freq in Mhz . Dangerous */
static int slowpci; /* =0 */	/* slow PCI settings */
static int dev = -2;		/* specify device (0..n) -2=all -1=none*/

static char * mode_option ;


/********/

int sstfb_init(void);
int sstfb_setup(char *options);
static int __devinit sstfb_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void __devexit sstfb_remove(struct pci_dev *pdev);


/* Framebuffer API */
static int sstfb_open(struct fb_info *info, int user);
static int sstfb_release(struct fb_info *info, int user);
static int sstfb_get_fix(struct fb_fix_screeninfo *fix,
                         int con, struct fb_info *info);
static int sstfb_get_var(struct fb_var_screeninfo *var,
                         int con, struct fb_info *info);
static int sstfb_set_var(struct fb_var_screeninfo *var,
                         int con, struct fb_info *info);
static int sstfb_get_cmap(struct fb_cmap *cmap, int kspc,
                          int con, struct fb_info *info);
static int sstfb_set_cmap(struct fb_cmap *cmap, int kspc,
                          int con, struct fb_info *info);
static int sstfb_pan_display(struct fb_var_screeninfo *var,
                             int con, struct fb_info *info);
static int sstfb_ioctl(struct inode *inode, struct file *file,
                       u_int cmd, u_long arg, int con,
                       struct fb_info *info);

/* Interface to the low level console driver */
static int sstfbcon_switch(int con, struct fb_info *info);
static int sstfbcon_updatevar(int con, struct fb_info *info);
static void sstfbcon_blank(int blank, struct fb_info *info);

/* Internal routines */
static void sstfb_install_cmap(int con, struct fb_info *info);
static int sstfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                           u_int *transp, struct fb_info *info);
static int sstfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                           u_int transp, struct fb_info *info);

static int sstfb_set_par(const struct sstfb_par *par,
                          struct sstfb_info *sst_info);
static int sstfb_decode_var (const struct fb_var_screeninfo *var,
                             struct sstfb_par *par,
                             const struct sstfb_info *sst_info);
static int sstfb_encode_var (struct fb_var_screeninfo *var,
                             const struct sstfb_par *par,
                             const struct sstfb_info *sst_info);

static void sstfb_test16(struct sstfb_info *sst_info);

#ifdef EN_24_32_BPP
static void sstfb_test32(struct sstfb_info *sst_info);
#endif

/* Low level routines */
static int sst_get_memsize(struct sstfb_info *sst_info, u_long *memsize);
static int __sst_wait_idle(u_long vbase);
#define sst_wait_idle()	 __sst_wait_idle(sst_info->mmio.vbase)
static int sst_detect_dactype(struct sstfb_info *sst_info);
static int sst_detect_att(struct sstfb_info *sst_info);
static int sst_detect_ti(struct sstfb_info *sst_info);
static int sst_detect_ics(struct sstfb_info *sst_info);
static int sst_calc_pll(const int freq, int *freq_out, struct pll_timing *t);
static int sst_set_pll_att_ti(struct sstfb_info *sst_info, const struct pll_timing *t, const int clock);
static int sst_set_pll_ics(struct sstfb_info *sst_info, const struct pll_timing *t, const int clock);
static void sst_set_vidmod_att_ti(struct sstfb_info *sst_info, const int bpp);
static void sst_set_vidmod_ics(struct sstfb_info *sst_info, const int bpp);
static int sst_init(struct sstfb_info *sst_info);
static void sst_shutdown(struct sstfb_info *sst_info);

static struct fb_ops sstfb_ops = {
	owner : THIS_MODULE,
	fb_open:	sstfb_open,
	fb_release:	sstfb_release,
	fb_get_fix:	sstfb_get_fix,
	fb_get_var:	sstfb_get_var,
	fb_set_var:	sstfb_set_var,
	fb_get_cmap:	sstfb_get_cmap,
	fb_set_cmap:	sstfb_set_cmap,
	fb_pan_display:	sstfb_pan_display,
	fb_ioctl:	sstfb_ioctl,
};

enum {
	ID_VOODOO1 = 0,
	ID_VOODOO2 = 1,
};

#define IS_VOODOO2(info) ((info)->type == ID_VOODOO2 )

static struct sst_spec voodoo_spec[] __devinitdata = {
	{ name : "Voodoo Graphics",
	default_gfx_clock : 50000,
	max_gfxclk : 60, },
	{ name : "Voodoo2",
	default_gfx_clock : 75000,
	max_gfxclk : 85, },
};

static struct pci_device_id sstfb_id_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, ID_VOODOO1 },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO2,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, ID_VOODOO2 },
	{ 0 },
};

static struct pci_driver sstfb_driver = {
	name:		"sstfb",
	id_table:	sstfb_id_tbl,
	probe:          sstfb_probe,
	remove:		__devexit_p(sstfb_remove),
};

static struct fb_var_screeninfo	sstfb_default =
#if ( DEFAULT_MODE == 0 )
    { /* 800x600@60, 16 bpp .borowed from glide/sst1/include/sst1init.h */
    800, 600, 800, 600, 0, 0, 16, 0,
    {11, 5, 0}, {5, 6, 0}, {0, 5, 0}, {0, 0, 0},
    0, 0, -1, -1, 0,
    25000, 86, 41, 23, 1, 127, 4,
    0, FB_VMODE_NONINTERLACED };
#endif
#if ( DEFAULT_MODE == 1 )
    {/* 640x480@75, 16 bpp .borowed from glide/sst1/include/sst1init.h */
    640, 480, 640, 480, 0, 0, 16, 0,
    {11, 5, 0}, {5, 6, 0}, {0, 5, 0}, {0, 0, 0},
    0, 0, -1, -1, 0,
    31746, 118, 17, 16, 1, 63, 3,
    0, FB_VMODE_NONINTERLACED };
#endif
#if ( DEFAULT_MODE == 2 )
    { /* 1024x768@76 took from my /etc/fb.modes */
    1024, 768, 1024, 768,0, 0, 16,0,
    {11, 5, 0}, {5, 6, 0}, {0, 5, 0}, {0, 0, 0},
    0, 0, -1, -1, 0,
    11764, 208, 8, 36, 16, 120, 3 ,
    0, FB_VMODE_NONINTERLACED };
#endif
#if ( DEFAULT_MODE == 3 )
    { /* 640x480@60 , 16bpp glide default ?*/
    640, 480, 640, 480, 0, 0, 16, 0,
    {11, 5, 0}, {5, 6, 0}, {0, 5, 0}, {0, 0, 0},
    0, 0, -1, -1, 0,
    39721 ,  38, 26 ,  25 ,18  , 96 ,2,
    0, FB_VMODE_NONINTERLACED };
#endif

static struct dac_switch dacs[] __devinitdata = {
	{	name:		"TI TVP3409",
		detect:		sst_detect_ti,
		set_pll:	sst_set_pll_att_ti,
		set_vidmod:	sst_set_vidmod_att_ti },

	{	name:		"AT&T ATT20C409",
		detect:		sst_detect_att,
		set_pll:	sst_set_pll_att_ti,
		set_vidmod:	sst_set_vidmod_att_ti },
	{	name:		"ICS ICS5342",
		detect:		sst_detect_ics,
		set_pll:	sst_set_pll_ics,
		set_vidmod:	sst_set_vidmod_ics },
};


/*
 *
 *  Definitions
 *
 */

#if (SST_DEBUG_VAR > 0)
/* debug info / dump a fb_var_screeninfo */
static void sst_dbg_print_var(struct fb_var_screeninfo *var) {
	dprintk(" {%d, %d, %d, %d, %d, %d, %d, %d,\n",
	        var->xres, var->yres, var->xres_virtual, var->yres_virtual,
	        var->xoffset, var->yoffset,
	        var->bits_per_pixel, var->grayscale);
	dprintk(" {%d, %d, %d}, {%d, %d, %d}, {%d, %d, %d}, {%d, %d, %d},\n",
	        var->red.offset, var->red.length, var->red.msb_right,
	        var->green.offset, var->green.length, var->green.msb_right,
	        var->blue.offset, var->blue.length, var->blue.msb_right,
	        var->transp.offset, var->transp.length,
	        var->transp.msb_right);
	dprintk(" %d, %d, %d, %d, %d,\n",
	        var->nonstd, var->activate,
	        var->height, var->width, var->accel_flags);
	dprintk(" %d, %d, %d, %d, %d, %d, %d,\n",
	        var->pixclock, var->left_margin, var->right_margin,
	        var->upper_margin, var->lower_margin,
	        var->hsync_len, var->vsync_len);
	dprintk(" %#x, %#x}\n",var->sync, var->vmode);
}
#endif /* (SST_DEBUG_VAR > 0) */

#if (SST_DEBUG_REG > 0)
static void sst_dbg_print_read_reg (u32 reg, u32 val) {
	char * regname = NULL;
	switch (reg) {
	case FBIINIT0: regname="FbiInit0"; break;
	case FBIINIT1: regname="FbiInit1"; break;
	case FBIINIT2: regname="FbiInit2"; break;
	case FBIINIT3: regname="FbiInit3"; break;
	case FBIINIT4: regname="FbiInit4"; break;
	case FBIINIT5: regname="FbiInit5"; break;
	case FBIINIT6: regname="FbiInit6"; break;
	}
	if (regname == NULL)
		r_ddprintk("sst_read(%#x): %#x\n", reg, val);
	else
		r_dprintk(" sst_read(%s): %#x\n", regname, val);
}

static void sst_dbg_print_write_reg (u32 reg, u32 val) {
	char * regname = NULL;

	switch (reg) {
	case FBIINIT0: regname="FbiInit0"; break;
	case FBIINIT1: regname="FbiInit1"; break;
	case FBIINIT2: regname="FbiInit2"; break;
	case FBIINIT3: regname="FbiInit3"; break;
	case FBIINIT4: regname="FbiInit4"; break;
	case FBIINIT5: regname="FbiInit5"; break;
	case FBIINIT6: regname="FbiInit6"; break;
	}
	if (regname == NULL)
		r_ddprintk("sst_write(%#x, %#x)\n", reg, val);
	else
		r_dprintk(" sst_write(%s, %#x)\n", regname, val);
}
#else /*  (SST_DEBUG_REG > 0) */
#  define sst_dbg_print_read_reg(reg, val)	do {} while(0)
#  define sst_dbg_print_write_reg(reg, val)	do {} while(0)
#endif /*  (SST_DEBUG_REG > 0) */

/* register access */
#define sst_read(reg)		__sst_read(sst_info->mmio.vbase, reg)
#define sst_write(reg,val)	__sst_write(sst_info->mmio.vbase, reg, val)
#define sst_set_bits(reg,val)	__sst_set_bits(sst_info->mmio.vbase, reg, val)
#define sst_unset_bits(reg,val)	__sst_unset_bits(sst_info->mmio.vbase, reg, val)
#define sst_dac_read(reg)	__sst_dac_read(sst_info->mmio.vbase, reg)
#define sst_dac_write(reg,val)	__sst_dac_write(sst_info->mmio.vbase, reg, val)
#define dac_i_read(reg)		__dac_i_read(sst_info->mmio.vbase, reg)
#define dac_i_write(reg,val)	__dac_i_write(sst_info->mmio.vbase, reg, val)

static inline u32 __sst_read(u_long vbase, u32 reg)
{
	u32 ret;

	ret = readl(vbase + reg);
	sst_dbg_print_read_reg(reg, ret);
	return ret;
}

static inline void __sst_write(u_long vbase, u32 reg, u32 val)
{
	sst_dbg_print_write_reg(reg, val);
	writel(val, vbase + reg);
}

static inline void __sst_set_bits(u_long vbase, u32 reg, u32 val)
{
	r_dprintk("sst_set_bits(%#x, %#x)\n", reg, val);
	__sst_write(vbase, reg, __sst_read(vbase, reg) | val);
}

static inline void __sst_unset_bits(u_long vbase, u32 reg, u32 val)
{
	r_dprintk("sst_unset_bits(%#x, %#x)\n", reg, val);
	__sst_write(vbase, reg, __sst_read(vbase, reg) & ~val);
}

/* dac access */
/* dac_read should be remaped to FbiInit2 (via the pci reg init_enable) */
static u8 __sst_dac_read(u_long vbase, u8 reg)
{
	u8 ret;

#ifdef SST_DEBUG
	if ((reg & 0x07) != reg) {
		dprintk("bug line %d: register adress '%d' is too high\n",
		         __LINE__,reg);
	}
#endif
	reg &= 0x07;
	__sst_write(vbase, DAC_DATA, ((u32)reg << 8) | DAC_READ_CMD );
	__sst_wait_idle(vbase);
	/*udelay(10);*/
	ret=(__sst_read(vbase, DAC_READ) & 0xff);
	r_dprintk("sst_dac_read(%#x): %#x\n", reg, ret);
	return (u8)ret;
}

static void __sst_dac_write(u_long vbase, u8 reg, u8 val)
{
	r_dprintk("sst_dac_write(%#x, %#x)\n", reg, val);
#ifdef SST_DEBUG
	if ((reg & 0x07) != reg)
		dprintk("bug line %d: register adress '%d' is too high\n",
		         __LINE__,reg);
#endif
	reg &= 0x07;
	__sst_write(vbase, DAC_DATA,(((u32)reg << 8)) | (u32)val);
}

/* indexed access to ti/att dacs */
static u32 __dac_i_read(u_long vbase, u8 reg)
{
	u32 ret;

	__sst_dac_write(vbase, DACREG_ADDR_I, reg);
	ret = __sst_dac_read(vbase, DACREG_DATA_I);
	r_dprintk("sst_dac_read_i(%#x): %#x\n", reg, ret);
	return ret;
}
static void __dac_i_write(u_long vbase, u8 reg,u8 val)
{
	r_dprintk("sst_dac_write_i(%#x, %#x)\n", reg, val);
	__sst_dac_write(vbase, DACREG_ADDR_I, reg);
	__sst_dac_write(vbase, DACREG_DATA_I, val);
}

/*
 *
 *  Internal routines
 *
 */

static void sstfb_install_cmap(int con, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	f_dprintk("sstfb_install_cmap(con: %d)\n",con);
	f_ddprintk("currcon: %d\n", sst_info->currcon);
	if (con != sst_info->currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, sstfb_setcolreg, info);
	else
		fb_set_cmap(
			fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
			1, sstfb_setcolreg, info);
#undef sst_info
}

static int sstfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                           u_int *transp, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	f_dddprintk("sstfb_getcolreg\n");
	if (regno >= 16) return 1;

	*red    = sst_info->palette[regno].red;
	*green  = sst_info->palette[regno].green;
	*blue   = sst_info->palette[regno].blue;
	*transp = sst_info->palette[regno].transp;
	f_dddprintk("%-2d rvba: %#x, %#x, %#x, %#x\n",
	            regno,*red, *green, *blue, *transp);
	return 0;
#undef sst_info
}

static int sstfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                           u_int transp, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	u32 col;
	struct display * disp;

	f_dddprintk("sstfb_setcolreg\n");
	f_dddprintk("%-2d rvba: %#x, %#x, %#x, %#x\n",
	            regno, red, green, blue, transp);
	if (regno >= 16) return 1;

	sst_info->palette[regno].red   = red;
	sst_info->palette[regno].green = green;
	sst_info->palette[regno].blue  = blue;
	sst_info->palette[regno].transp= transp;

	disp=&sst_info->disp;
	red    >>= (16 - disp->var.red.length);
	green  >>= (16 - disp->var.green.length);
	blue   >>= (16 - disp->var.blue.length);
	transp >>= (16 - disp->var.transp.length);
	col = (red << disp->var.red.offset)
	    | (green << disp->var.green.offset)
	    | (blue  << disp->var.blue.offset)
	    | (transp << disp->var.transp.offset);

	switch(disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
	case 16:
		sst_info->fbcon_cmap.cfb16[regno]=(u16)col;
		break;
#endif
#ifdef EN_24_32_BPP
#if defined(FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)
	case 24:
	case 32:
		sst_info->fbcon_cmap.cfb32[regno]=col;
		break;
#endif
#endif
	default:
		BUG();
		break;
	}
	f_dddprintk("bpp: %d . encoded color: %#x\n",
	            disp->var.bits_per_pixel, col);
	return 0;
#undef sst_info
}

/* set par according to var ( checks var ) */
static int sstfb_decode_var (const struct fb_var_screeninfo *var,
                             struct sstfb_par *par,
                             const struct sstfb_info *sst_info)
{
	int real_length;

	f_dprintk("sstfb_decode_var\n");
	/* Check var validity */
	par->valid=0;

	memset(par, 0, sizeof(par));
	par->xDim       = var->xres;
	par->hSyncOn    = var->hsync_len;
	par->hSyncOff   = var->xres + var->right_margin + var->left_margin;
	par->hBackPorch = var->left_margin;
	par->yDim       = var->yres;
	par->vSyncOn    = var->vsync_len;
	par->vSyncOff   = var->yres + var->lower_margin + var->upper_margin;
	par->vBackPorch = var->upper_margin;

	if(sst_calc_pll (PS2KHZ(var->pixclock), &par->freq, &par->pll)) {
		eprintk("Pixclock %d out of range\n", var->pixclock);
		return -EINVAL; //XXX
	}

	par->sync=var->sync & (FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT);
	par->vmode=var->vmode & (FB_VMODE_INTERLACED | FB_VMODE_DOUBLE);

	/* in laced mode, vBackPorch should be even. odd -> funky display */
	if (par->vmode & FB_VMODE_INTERLACED)
		par->vBackPorch += (par->vBackPorch % 2);
	if (par->vmode & FB_VMODE_DOUBLE) {
		par->vBackPorch <<= 1;
		par->yDim <<=1;
		par->vSyncOn <<=1;
		par->vSyncOff <<=1;
	}

	switch (var->bits_per_pixel) {
	case 0 ... 16 :
		par->bpp = 16;
		break;
#ifdef EN_24_32_BPP
	case 17 ... 24 :
		par->bpp = 24;
		break;
	case 25 ... 32 :
		par->bpp = 32;
		break;
#endif
	default :
		eprintk ("Unsupported bpp %d\n", par->bpp);
		return -EINVAL;
		break;
	}

	if (IS_VOODOO2(sst_info)) {
		/* voodoo2 has 32 pixel wide tiles , BUT stange things
		 happen with odd number of tiles */
		par->tiles_in_X= (par->xDim + 63 ) / 64 * 2;
	} else {
		/* voodoo1 has 64 pixels wide tiles. */
		par->tiles_in_X= (par->xDim + 63 ) / 64;
	}

	/* validity tests */
	if((par->xDim <= 1) || (par->yDim <= 0 )
	   || (par->hSyncOn <= 1)
	   || (par->hSyncOff <= 1)
	   || (par->hBackPorch <= 2)
	   || (par->vSyncOn <= 0)
	   || (par->vSyncOff <= 0)
	   || (par->vBackPorch <= 0)
	   || (par->tiles_in_X <= 0)) {
		return -EINVAL;
	}
	if (IS_VOODOO2(sst_info)) {
		/* Voodoo 2 limits */
		if(((par->xDim-1) >= POW2(11)) || (par->yDim >= POW2(11))) {
			eprintk ("Unsupported resolution %dx%d\n",
			         var->xres, var->yres);
			return -EINVAL;
		}
		if (((par->hSyncOn-1) >= POW2(9))
		   || ((par->hSyncOff-1) >= POW2(11))
		   || ((par->hBackPorch-2) >= POW2(9))
		   || (par->vSyncOn >= POW2(13))
		   || (par->vSyncOff >= POW2(13))
		   || (par->vBackPorch >= POW2(9))
		   || (par->tiles_in_X >= POW2(6))) {
			eprintk ("Unsupported Timing\n");
			return -EINVAL;
		}
	} else {
		/* Voodoo limits */
		if (par->vmode) {
			eprintk("Interlace/Doublescan not supported %#x\n",
				par->vmode);
			return -EINVAL;
		}
		if(((par->xDim-1) >= POW2(10)) || (par->yDim >= POW2(10))) {
			eprintk ("Unsupported resolution %dx%d\n",
			         var->xres, var->yres);
			return -EINVAL;
		}
		if (((par->hSyncOn-1) >= POW2(8))
		   || ((par->hSyncOff-1) >= POW2(10))
		   || ((par->hBackPorch-2) >= POW2(8))
		   || (par->vSyncOn >= POW2(12))
		   || (par->vSyncOff >= POW2(12))
		   || (par->vBackPorch >= POW2(8))
		   || (par->tiles_in_X >= POW2(4))) {
			eprintk ("Unsupported Timings\n");
			return -EINVAL;
		}
	}

	/* it seems that the fbi uses tiles of 64x16 pixels to "map" the mem*/
	/* FIXME: i don't like this... looks wrong*/
	real_length = par->tiles_in_X  * (IS_VOODOO2(sst_info) ? 32 : 64 )
	              * ((par->bpp == 16) ? 2 : 4);

	if ((real_length * var->yres) > sst_info->video.len) {
		eprintk ("Not enough video memory\n");
		return -ENOMEM;
	}
	par->valid=1;
	return 0;
}

/* sets var according to par (basicaly, sets sane values) */
static int sstfb_encode_var (struct fb_var_screeninfo *var,
                             const struct sstfb_par *par,
                             const struct sstfb_info *sst_info)
{
	memset(var,0,sizeof(struct fb_var_screeninfo));

	var->xres           = par->xDim;
	var->yres           = par->yDim;
	var->xres_virtual   = par->xDim;
	var->yres_virtual   = par->yDim;
	var->bits_per_pixel = par->bpp;
	/* {x|y}offset = 0 ; sync=0 */
	var->height         = -1;
	var->width          = -1;
	var->pixclock       = KHZ2PS(par->freq);
	var->left_margin    = par->hBackPorch;
	var->right_margin   = par->hSyncOff - par->xDim - par->hBackPorch;
	var->upper_margin   = par->vBackPorch;
	var->lower_margin   = par->vSyncOff - par->yDim - par->vBackPorch;
	var->hsync_len      = par->hSyncOn;
	var->vsync_len      = par->vSyncOn;
	var->sync           = par->sync;
	var->vmode          = par->vmode;
	if (var->vmode & FB_VMODE_DOUBLE) {
		var->yres           >>=1;
		var->yres_virtual   >>=1;
		var->vsync_len      >>=1;
		var->upper_margin   >>=1;
		var->lower_margin   >>=1;
	}

	/*
	 * correct the color bit fields
	 */
	/* var->{red|green|blue}.msb_right    = 0; */

	switch (par->bpp) {
	case 16:	/* RGB 565  LfbMode 0 */
		var->red.length    = 5;
		var->green.length  = 6;
		var->blue.length   = 5;
		var->transp.length = 0;

		var->red.offset    = 11;
		var->green.offset  = 5;
		var->blue.offset   = 0;
		var->transp.offset = 0;
		break;
#ifdef EN_24_32_BPP
	case 24:	/* RGB 888 LfbMode 4 */
	case 32:	/* ARGB 8888 LfbMode 5 */
	       	/* in 24bpp we fake a 32 bpp mode */
		var->red.length    = 8;
		var->green.length  = 8;
		var->blue.length   = 8;
		var->transp.length = 0;

		var->red.offset    = 16;
		var->green.offset  = 8;
		var->blue.offset   = 0;
		var->transp.offset = 0;
		break;
#endif
	default:
		eprintk("bug line %d: bad depth '%u'\n", __LINE__, par->bpp);
		break;
	}
	return 0;
}

/*
 * Frame buffer API
 */

static int sstfb_open(struct fb_info *info, int user)
{
	f_dprintk("sstfb_open(user: %d)\n",user);
	return 0;
}

static int sstfb_release(struct fb_info *info, int user)
{
	f_dprintk("sstfb_release(user: %d)\n",user);
	return 0;
}

static int sstfb_get_fix(struct fb_fix_screeninfo *fix,
                         int con, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)

	struct fb_var_screeninfo *var;
	struct fb_var_screeninfo var2;

	f_dprintk("sstfb_get_fix(con: %d)\n",con);
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	
	if (con == -1) {
		sstfb_encode_var(&var2, &sst_info->current_par, sst_info);
		var = &var2;
	} else
		var = &fb_display[con].var;

	strcpy(fix->id, sst_info->info.modename);
	/* lfb phys address = membase + 4Mb */
	fix->smem_start  = sst_info->video.base;
	fix->smem_len    = sst_info->video.len;

	fix->type        = FB_TYPE_PACKED_PIXELS;
	fix->visual      = FB_VISUAL_TRUECOLOR;
	fix->accel       = FB_ACCEL_NONE;
	/*
	 *   According to the specs, the linelength must be of 1024 *pixels*.
	 * and the 24bpp mode is in fact a 32 bpp mode.
	 */
	fix->line_length = (var->bits_per_pixel == 16) ? 2048 : 4096 ;
	return 0;
#undef sst_info
}

static int sstfb_get_var(struct fb_var_screeninfo *var,
                         int con, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	f_dprintk("sstfb_get_var(con: %d)\n",con);
	if (con == -1)
		sstfb_encode_var(var, &sst_info->current_par, sst_info);
	else
		*var = fb_display[con].var;
	print_var(var, "var");
	return 0;
#undef sst_info
 }

static int sstfb_set_var(struct fb_var_screeninfo *var,
                         int con, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)

	struct sstfb_par par;
	struct display *display;
	int err;
	int old_bpp,old_xres,old_yres;

	f_dprintk("sstfb_set_var(con: %d)\n",con);
	f_ddprintk("xres yres vxres vyres bpp activate\n");
	f_ddprintk("%-4d %-4d %-5d %-5d %-3d %#-8x\n",
		 var->xres,var->yres,var->xres_virtual,var->yres_virtual,
		 var->bits_per_pixel,var->activate);
	if (con < 0)
		display = &sst_info->disp;
	else
		display = &fb_display[con];

	if ((err = sstfb_decode_var(var, &par, sst_info)))
		return err;
	sstfb_encode_var (var, &par, sst_info);

	switch (var->activate & FB_ACTIVATE_MASK) {
		case FB_ACTIVATE_TEST:
			return 0;
		case FB_ACTIVATE_NXTOPEN:
		case FB_ACTIVATE_NOW:
			break;
		default:
			return -EINVAL;
	}
	old_xres = display->var.xres;
	old_yres = display->var.yres;
	old_bpp  = display->var.bits_per_pixel;
	display->var = *var;

	if ((old_xres != var->xres) || (old_yres != var->yres)
	    || (old_bpp != var->bits_per_pixel)) {
		/* 2-3  lignes redondantes avec get_fix */
		display->screen_base = (char *) sst_info->video.vbase;
		display->visual = FB_VISUAL_TRUECOLOR;
		display->type = FB_TYPE_PACKED_PIXELS;
		display->type_aux = 0;
		display->ypanstep = 0;
		display->ywrapstep = 0;
		display->line_length = (var->bits_per_pixel==16) ? 2048 : 4096;
		display->inverse = 0;
		switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
		case 16:
			display->dispsw = &fbcon_cfb16;
			display->dispsw_data = sst_info->fbcon_cmap.cfb16;
			break;
#endif
#ifdef EN_24_32_BPP
#if defined (FBCON_HAS_CFB24) || defined (FBCON_HAS_CFB32 )
		case 24: /*24bpp non packed <=> 32 bpp */
		case 32:
			display->dispsw = &fbcon_cfb32;
			display->dispsw_data = sst_info->fbcon_cmap.cfb32;
			break;
#endif
#endif
		default:
			display->dispsw = &fbcon_dummy;
			break;
		}
		display->scrollmode = SCROLL_YREDRAW;
		if (sst_info->info.changevar) {
			v_dprintk("fb_info.changevar(con: %d)\n", con);
			(*sst_info->info.changevar)(con);
			v_dprintk("fb_info.changevar: done \n");
		} else {
			v_dprintk("fb_info.changevar() == NULL . \n");
		}
	}

	if ((con == -1) || (con==sst_info->currcon)) {
		sstfb_set_par (&par, sst_info);
	}
	print_var(var, "var");
	print_var(&display->var, "&display->var");

	if (old_bpp != var->bits_per_pixel) {
	    if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
		return err;
	    sstfb_install_cmap(con, info);
	}

	return 0;
#undef sst_info
}


static int sstfb_set_cmap(struct fb_cmap *cmap, int kspc,
                          int con, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	struct display *d = (con<0) ? info->disp : fb_display + con;

	f_dprintk("sstfb_set_cmap\n");
	f_ddprintk("con: %d, currcon: %d, d->cmap.len %d\n",
		 con, sst_info->currcon, d->cmap.len);

	if (d->cmap.len != 16 ) {	/* or test if cmap.len == 0 ? */
		int err;
		err = fb_alloc_cmap(&d->cmap, 16, 0); /* cmap size=16 */
		if (err) return err;
	}
	if (con == sst_info->currcon) {
		return fb_set_cmap(cmap, kspc, sstfb_setcolreg, info);
	} else {
		fb_copy_cmap(cmap, &d->cmap, kspc ? 0 : 1);
	}
	return 0;
#undef sst_info
}

static int sstfb_get_cmap(struct fb_cmap *cmap, int kspc,
                          int con, struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	f_dprintk("sstfb_get_cmap\n");
	f_ddprintk("con %d, curcon %d, cmap.len %d\n",
		 con, sst_info->currcon, fb_display[con].cmap.len);

	/* FIXME: check if con = -1 ? cf sstfb_set_cmap...  */
	if (con == sst_info->currcon)
		return fb_get_cmap(cmap, kspc, sstfb_getcolreg, info);
	else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(
			fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
			cmap, kspc ? 0 : 2);
	return 0;
#undef sst_info
}

/* TODO */
static int sstfb_pan_display(struct fb_var_screeninfo *var,
                             int con, struct fb_info *info)
{
	f_dprintk("sstfb_pan_display\n");
	return -EINVAL;
}

static int sstfb_ioctl(struct inode *inode, struct file *file,
                       u_int cmd, u_long arg, int con,
                       struct fb_info *info)
{
#define sst_info	((struct sstfb_info *) info)
	int i;
	u_long p;
	u32 tmp, val;
	u32 fbiinit0;
	struct pci_dev * sst_dev = sst_info->dev;

	f_dprintk("sstfb_ioctl(%x)\n", cmd);
	
	switch (cmd) {
		
#if (SST_DEBUG_VAR >0)
	/* tmp ioctl : dumps fb_display[0-5] */
	case _IO('F', 0xdb):		/* 0x46db */
		f_dprintk("dumping fb_display[0-5].var\n");
		for (i = 0 ; i< 6 ; i++) {
			print_var(&fb_display[i].var, "var(%d)", i);
		}
		return 0;
#endif /* (SST_DEBUG_VAR >0) */

	/* fills the lfb up to given count of pixels */
	case _IOW('F', 0xdc, u32):	/* 0x46dc */
		if (copy_from_user(&val, (void *) arg, sizeof(val)))
			return -EFAULT;
		if (val > 0x400000 )
			val = 0x400000;
		f_dprintk("filling %#x \n", val);
		for (p = 0 ; p < val; p+=2)
			writew( p >> 6 , sst_info->video.vbase + p);
		return 0;
		
	/* enable/disable VGA pass_through */
	case _IOW('F', 0xdd, u32):	/* 0x46dd */
		if (copy_from_user(&val, (void *) arg, sizeof(val)))
			return -EFAULT;
		f_dprintk("switch VGA pass-through\n");
		pci_read_config_dword(sst_dev, PCI_INIT_ENABLE, &tmp);
		pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
				       tmp | PCI_EN_INIT_WR );
		fbiinit0 = sst_read (FBIINIT0);
		if (val) {
			sst_write(FBIINIT0, fbiinit0 & ~EN_VGA_PASSTHROUGH);
			iprintk ( "Disabling VGA pass-through\n");
		} else {
			sst_write(FBIINIT0, fbiinit0 | EN_VGA_PASSTHROUGH);
			iprintk ( "Enabling VGA pass-through\n");
		}
		pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, tmp);
		return 0;

	/* display test pattern */
	case _IO('F', 0xde):		/* 0x46de */
		f_dprintk("test color display\n");
		f_ddprintk("currcon: %d, bpp %d\n", sst_info->currcon,
			  sst_info->current_par.bpp);
		memset_io(sst_info->video.vbase, 0, sst_info->video.len);
		switch (sst_info->current_par.bpp) {
	       	case 16:
			sstfb_test16(sst_info);
			break;
#ifdef EN_24_32_BPP
		case 24:
		case 32:
			sstfb_test32(sst_info);
			break;
#endif
		default:
			return -EFAULT;
		}
		return 0;
	}
	
	return -EINVAL;
#undef sst_info
}


/*
 * Low level routines
 */

/* get lfb size */

static int __devinit sst_get_memsize(struct sstfb_info *sst_info, u_long *memsize)
{
	u_long fbbase_virt = sst_info->video.vbase;
	f_dprintk("sst_get_memsize\n");

	/* force memsize */
	if ((mem >= 1 ) &&  (mem <= 4)) {
		*memsize = (mem * 0x100000);
		iprintk("supplied memsize: %#lx\n", *memsize);
		return 1;
	}

	writel (0xdeadbeef, fbbase_virt);
	writel (0xdeadbeef, fbbase_virt+0x100000);
	writel (0xdeadbeef, fbbase_virt+0x200000);
	f_ddprintk("0Mb: %#x, 1Mb: %#x, 2Mb: %#x\n",
	           readl(fbbase_virt), readl(fbbase_virt + 0x100000),
	           readl(fbbase_virt + 0x200000));

	writel (0xabcdef01, fbbase_virt);

	f_ddprintk("0Mb: %#x, 1Mb: %#x, 2Mb: %#x\n",
	           readl(fbbase_virt), readl(fbbase_virt + 0x100000),
	           readl(fbbase_virt + 0x200000));

	/* checks for 4mb lfb , then 2, then defaults to 1*/
	if (readl(fbbase_virt + 0x200000) == 0xdeadbeef) {
		*memsize = 0x400000;
	} else if (readl(fbbase_virt + 0x100000) == 0xdeadbeef) {
		*memsize = 0x200000;
	} else {
		*memsize = 0x100000;
	}
	f_ddprintk("detected memsize: %#lx\n", *memsize);
	return 1;
}


/*
 * wait for the fbi chip. ASK: what happens if the fbi is stuck ?
 *
 * the FBI is supposed to be ready if we receive 5 time
 * in a row a "idle" answer to our requests
 */

static int __sst_wait_idle(u_long vbase)
{
	int count = 0;

	f_ddprintk("sst_wait_idle\n");
	while(1) {
		if (__sst_read(vbase, STATUS) & STATUS_FBI_BUSY) {
			f_dddprintk("status: busy\n");
/* FIXME basicaly, this is a busy wait. maybe not that good. oh well; this is a small loop after all ...*/
			count = 0;
		} else {
			count++;
			f_dddprintk("status: idle(%d)\n", count);
		}
		if (count >= 5) return 1;
//XXX  do something to avoid hanging the machine if the voodoo is out
	}
}

/*
 * detect dac type
 * prerequisite : write to FbiInitx enabled, video and fbi and pci fifo reset,
 * dram refresh disabled, FbiInit remaped.
 * TODO: mmh.. maybe i shoud put the "prerequisite" in the func ...
 */
static int __devinit sst_detect_dactype(struct sstfb_info * sst_info)
{
	int ret=0,i;
	f_dprintk("sst_detect_dactype\n");
	for (i=0; i< sizeof(dacs)/sizeof(dacs[0]) ; i++) {
		ret = dacs[i].detect(sst_info);
		if (ret) break;
	}
	if (!ret)
		return 0;
	f_dprintk("found %s\n", dacs[i].name);
	sst_info->dac_sw=dacs[i];
	return 1;
}

/* fbi should be idle, and fifo emty and mem disabled */
/* supposed to detect AT&T ATT20C409 and Ti TVP3409 ramdacs */

static int __devinit sst_detect_att(struct sstfb_info * sst_info)
{
	int i, mir, dir;

	f_dprintk("sst_detect_att\n");
	for (i = 0; i<3; i++) {
		sst_dac_write(DACREG_WMA, 0); 	/* backdoor */
		sst_dac_read(DACREG_RMR);	/* read 4 times RMR */
		sst_dac_read(DACREG_RMR);
		sst_dac_read(DACREG_RMR);
		sst_dac_read(DACREG_RMR);
		/* the fifth time,  CR0 is read */
		sst_dac_read(DACREG_RMR);
		/* the 6th, manufacturer id register */
		mir = sst_dac_read(DACREG_RMR);
		/*the 7th, device ID register */
		dir = sst_dac_read(DACREG_RMR);
		f_ddprintk("mir: %#x, dir: %#x\n", mir, dir);
		if ((mir == DACREG_MIR_ATT ) && (dir == DACREG_DIR_ATT)) {
			return 1;
		}
	}
	return 0;
}

static int __devinit sst_detect_ti(struct sstfb_info * sst_info)
{
	int i, mir, dir;

	f_dprintk("sst_detect_ti\n");
	for (i = 0; i<3; i++) {
		sst_dac_write(DACREG_WMA, 0); 	/* backdoor */
		sst_dac_read(DACREG_RMR);	/* read 4 times RMR */
		sst_dac_read(DACREG_RMR);
		sst_dac_read(DACREG_RMR);
		sst_dac_read(DACREG_RMR);
		/* the fifth time,  CR0 is read */
		sst_dac_read(DACREG_RMR);
		/* the 6th, manufacturer id register */
		mir = sst_dac_read(DACREG_RMR);
		/*the 7th, device ID register */
		dir = sst_dac_read(DACREG_RMR);
		f_ddprintk("mir: %#x, dir: %#x\n", mir, dir);
		if ((mir == DACREG_MIR_TI ) && (dir == DACREG_DIR_TI)) {
			return 1;
		}
	}
	return 0;
}

/*
 * try to detect ICS5342  ramdac
 * we get the 1st byte (M value) of preset f1,f7 and fB
 * why those 3 ? mmmh... for now, i'll do it the glide way...
 * and ask questions later. anyway, it seems that all the freq registers are
 * realy at their default state (cf specs) so i ask again, why those 3 regs ?
 * mmmmh.. it seems that's much more ugly than i thought. we use f0 and fA for
 * pll programming, so in fact, we *hope* that the f1, f7 & fB won't be
 * touched...
 * is it realy safe ? how can i reset this ramdac ? geee...
 */
static int __devinit sst_detect_ics(struct sstfb_info * sst_info)
{
	int i;
	int m_clk0_1, m_clk0_7, m_clk1_b;
	int n_clk0_1, n_clk0_7, n_clk1_b;
	f_dprintk("sst_detect_ics\n");
	for (i = 0; i<5; i++ ) {
		sst_dac_write(DACREG_ICS_PLLRMA, 0x1);	/* f1 */
		m_clk0_1 = sst_dac_read(DACREG_ICS_PLLDATA);
		n_clk0_1 = sst_dac_read(DACREG_ICS_PLLDATA);
		sst_dac_write(DACREG_ICS_PLLRMA, 0x7);	/* f7 */
		m_clk0_7 = sst_dac_read(DACREG_ICS_PLLDATA);
		n_clk0_7 = sst_dac_read(DACREG_ICS_PLLDATA);
		sst_dac_write(DACREG_ICS_PLLRMA, 0xb);	/* fB */
		m_clk1_b= sst_dac_read(DACREG_ICS_PLLDATA);
		n_clk1_b= sst_dac_read(DACREG_ICS_PLLDATA);
		f_ddprintk("m_clk0_1: %#x, m_clk0_7: %#x, m_clk1_b: %#x\n",
			m_clk0_1, m_clk0_7, m_clk1_b);
		f_ddprintk("n_clk0_1: %#x, n_clk0_7: %#x, n_clk1_b: %#x\n",
			n_clk0_1, n_clk0_7, n_clk1_b);
		if ((   m_clk0_1 == DACREG_ICS_PLL_CLK0_1_INI)
		    && (m_clk0_7 == DACREG_ICS_PLL_CLK0_7_INI)
		    && (m_clk1_b == DACREG_ICS_PLL_CLK1_B_INI)) {
			return 1;
		}
	}
	return 0;
}

/* compute the m,n,p  , returns the real freq
 * (ics datasheet :  N <-> N1 , P <-> N2)
 *
 * Fout= Fref * (M+2)/( 2^P * (N+2))
 *  we try to get close to the asked freq
 *  with P as high, and M as low as possible
 * range:
 * ti/att : 0 <= M <= 255; 0 <= P <= 3; 0<= N <= 63
 * ics    : 1 <= M <= 127; 0 <= P <= 3; 1<= N <= 31
 * we'll use the lowest limitation, should be precise enouth
 */
static int sst_calc_pll(const int freq, int *freq_out, struct pll_timing *t)
{
	int m, m2, n, p, best_err, fout;
	int best_n=-1;
	int best_m=-1;

	f_dprintk("sst_calc_pll(%dKhz)\n", freq);
	best_err = freq;
	p=3;
	/* f * 2^P = vco should be less than VCOmax ~ 250 MHz for ics*/
	while (((1 << p) * freq > VCO_MAX) && (p >= 0))
		p--;
	if (p == -1)
		return -EINVAL;
	for (n = 1; n < 32; n++) {
		/* calc 2 * m so we can round it later*/
		m2 = (2 * freq * (1 << p) * (n + 2) ) / DAC_FREF - 4 ;

		m = (m2 % 2 ) ? m2/2+1 : m2/2 ;
		if (m >= 128)
			break;
		fout = (DAC_FREF * (m + 2)) / ((1 << p) * (n + 2));
		if ((ABS(fout - freq) < best_err) && (m > 0)) {
			best_n = n;
			best_m = m;
			best_err = ABS(fout - freq);
			/* we get the lowest m , allowing 0.5% error in freq*/
			if (200*best_err < freq) break;
		}
	}
	if (best_n == -1)  /* unlikely, but who knows ? */
		return -EINVAL;
	t->p=p;
	t->n=best_n;
	t->m=best_m;
	*freq_out=(DAC_FREF * (t->m + 2)) / ((1 << t->p) * (t->n + 2));
	f_ddprintk ("m: %d, n: %d, p: %d, F: %dKhz\n",
		  t->m, t->n, t->p, *freq_out);
	return 0;
}

/*
 * gfx, video, pci fifo should be reset, dram refresh disabled
 * see detect_dac
 */

static int sst_set_pll_att_ti(struct sstfb_info * sst_info, const struct pll_timing *t, const int clock)
{
	u8 cr0, cc;
	f_dprintk("sst_set_pll_att_ti\n");

	/* enable indexed mode */

	sst_dac_write(DACREG_WMA, 0); 	/* backdoor */
	sst_dac_read(DACREG_RMR);	/* 1 time:  RMR */
	sst_dac_read(DACREG_RMR);	/* 2 RMR */
	sst_dac_read(DACREG_RMR);	/* 3 //  */
	sst_dac_read(DACREG_RMR);	/* 4 //  */
	cr0 = sst_dac_read(DACREG_RMR);	/* 5 CR0 */

	sst_dac_write(DACREG_WMA, 0);
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	sst_dac_write(DACREG_RMR, (cr0 & 0xf0)
	              | DACREG_CR0_EN_INDEXED
	              | DACREG_CR0_8BIT
	              | DACREG_CR0_PWDOWN );
	/* so, now we are in indexed mode . dunno if its common, but
	   i find this way of doing things a little bit weird :p */

	udelay(300);
	cc = dac_i_read(DACREG_CC_I);
	switch (clock) {
	case VID_CLOCK:
		dac_i_write(DACREG_AC0_I, t->m);
		dac_i_write(DACREG_AC1_I, t->p << 6 | t->n);
		dac_i_write(DACREG_CC_I,
		            (cc & 0x0f) | DACREG_CC_CLKA | DACREG_CC_CLKA_C);
		break;
	case GFX_CLOCK:
		dac_i_write(DACREG_BD0_I, t->m);
		dac_i_write(DACREG_BD1_I, t->p << 6 | t->n);
		dac_i_write(DACREG_CC_I,
		            (cc & 0xf0) | DACREG_CC_CLKB | DACREG_CC_CLKB_D);
		break;
	default:
		dprintk("bug line %d: wrong clock code '%d'\n",
		        __LINE__,clock);
		return 0;
		}
	udelay(300);

	/* power up the dac & return to "normal" non-indexed mode */
	dac_i_write(DACREG_CR0_I,
	            cr0 & ~DACREG_CR0_PWDOWN & ~DACREG_CR0_EN_INDEXED);
	return 1;
}

static int sst_set_pll_ics(struct sstfb_info * sst_info, const struct pll_timing *t, const int clock)
{
	u8 pll_ctrl;

	f_dprintk("sst_set_pll_ics\n");

	sst_dac_write(DACREG_ICS_PLLRMA, DACREG_ICS_PLL_CTRL);
	pll_ctrl = sst_dac_read(DACREG_ICS_PLLDATA);
	switch(clock) {
	case VID_CLOCK:
		sst_dac_write(DACREG_ICS_PLLWMA, 0x0);	/* CLK0, f0 */
		sst_dac_write(DACREG_ICS_PLLDATA, t->m);
		sst_dac_write(DACREG_ICS_PLLDATA, t->p << 5 | t->n);
		/* selects freq f0 for clock 0 */
		sst_dac_write(DACREG_ICS_PLLWMA, DACREG_ICS_PLL_CTRL);
		sst_dac_write(DACREG_ICS_PLLDATA,
		              (pll_ctrl & 0xd8)
		              | DACREG_ICS_CLK0
		              | DACREG_ICS_CLK0_0);
		break;
	case GFX_CLOCK :
		sst_dac_write(DACREG_ICS_PLLWMA, 0xa);	/* CLK1, fA */
		sst_dac_write(DACREG_ICS_PLLDATA, t->m);
		sst_dac_write(DACREG_ICS_PLLDATA, t->p << 5 | t->n);
		/* selects freq fA for clock 1 */
		sst_dac_write(DACREG_ICS_PLLWMA, DACREG_ICS_PLL_CTRL);
		sst_dac_write(DACREG_ICS_PLLDATA,
		              (pll_ctrl & 0xef) | DACREG_ICS_CLK1_A);
		break;
	default:
		dprintk("bug line %d: wrong clock code '%d'\n",
		        __LINE__, clock);
		return 0;
		}
	udelay(300);
	return 1;
}

static int sstfb_set_par(const struct sstfb_par * par, struct sstfb_info * sst_info)
{
	u32 lfbmode, fbiinit1, fbiinit2, fbiinit3, fbiinit5, fbiinit6=0;
	int ntiles;
	struct pci_dev * sst_dev = sst_info->dev;

	f_dprintk("sst_set_par(%dx%d)\n", par->xDim, par->yDim);
	f_ddprintk("hSyncOn hSyncOff vSyncOn vSyncOff\n");
	f_ddprintk("%-7d %-8d %-7d %-8d\n",
	           par->hSyncOn, par->hSyncOff,
	           par->vSyncOn, par->vSyncOff);
	f_ddprintk("hBackPorch vBackPorch xDim yDim Freq\n");
	f_ddprintk("%-10d %-10d %-4d %-4d %-8d\n",
	           par->hBackPorch, par->vBackPorch,
	           par->xDim, par->yDim, par->freq);

	if (!par->valid) {
		BUG();
		return -1;
	}
	sst_write(NOPCMD, 0);
	sst_wait_idle();
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, PCI_EN_INIT_WR);
	sst_set_bits(FBIINIT1, VIDEO_RESET);
	sst_set_bits(FBIINIT0, FBI_RESET | FIFO_RESET);
	sst_unset_bits(FBIINIT2, EN_DRAM_REFRESH);
	sst_wait_idle();

	/*sst_unset_bits (FBIINIT0, FBI_RESET); / reenable FBI ? */

	sst_write(BACKPORCH, par->vBackPorch << 16 | (par->hBackPorch - 2));
	sst_write(VIDEODIMENSIONS, par->yDim << 16 | (par->xDim - 1));
	sst_write(HSYNC, (par->hSyncOff - 1) << 16 | (par->hSyncOn - 1));
	sst_write(VSYNC,       par->vSyncOff << 16 | par->vSyncOn);

	fbiinit2=sst_read(FBIINIT2);
	fbiinit3=sst_read(FBIINIT3);

	/* everything is reset. we enable fbiinit2/3 remap : dac acces ok */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
	                       PCI_EN_INIT_WR | PCI_REMAP_DAC );

	sst_info->dac_sw.set_vidmod(sst_info, par->bpp);

	/* set video clock */
	sst_info->dac_sw.set_pll(sst_info, &par->pll, VID_CLOCK);

	/* disable fbiinit2/3 remap */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
	                       PCI_EN_INIT_WR);

	/* restore fbiinit2/3 */
	sst_write(FBIINIT2,fbiinit2);
	sst_write(FBIINIT3,fbiinit3);

	fbiinit1 = (sst_read(FBIINIT1) & VIDEO_MASK)
	            | EN_DATA_OE
	            | EN_BLANK_OE
	            | EN_HVSYNC_OE
	            | EN_DCLK_OE
/*	            | (15 << TILES_IN_X_SHIFT)*/
	            | SEL_INPUT_VCLK_2X
/*	            | (2 << VCLK_2X_SEL_DEL_SHIFT)
	            | (2 << VCLK_DEL_SHIFT)*/;
/* try with vclk_in_delay =0 (bits 29:30) , vclk_out_delay =0 (bits(27:28)
 in (near) future set them accordingly to revision + resolution (cf glide)
 first understand what it stands for :)
 FIXME: there are some artefacts... check for the vclk_in_delay
 lets try with 6ns delay in both vclk_out & in...
 doh... they're still there :\
*/

	ntiles = par->tiles_in_X;
	if (IS_VOODOO2(sst_info)) {
		fbiinit1 |= ((ntiles & 0x20) >> 5) << TILES_IN_X_MSB_SHIFT
		            | ((ntiles & 0x1e) >> 1) << TILES_IN_X_SHIFT ;
/* as the only value of importance for us in fbiinit6 is tiles in X (lsb),
   and as reading fbinit 6 will return crap (see FBIINIT6_DEFAULT) we just
   write our value. BTW due to the dac unable to read odd number of tiles, this
   field is always null ... */
		fbiinit6 = (ntiles & 0x1) << TILES_IN_X_LSB_SHIFT;
	}
	else
		fbiinit1 |= ntiles << TILES_IN_X_SHIFT;

	switch(par->bpp) {
	case 16:
		fbiinit1 |=  SEL_SOURCE_VCLK_2X_SEL;
		break;
#ifdef EN_24_32_BPP
	case 24:
	case 32:
/*	orig	sst_set_bits(FBIINIT1, SEL_SOURCE_VCLK_2X_DIV2 | EN_24BPP); */
		fbiinit1 |= SEL_SOURCE_VCLK_2X_SEL | EN_24BPP;
		break;
#endif
	default:
		dprintk("bug line %d: bad depth '%u'\n", __LINE__,
			par->bpp );
		return 0;
		break;
	}
	sst_write(FBIINIT1, fbiinit1);
	if (IS_VOODOO2(sst_info)) {
		sst_write(FBIINIT6, fbiinit6);
		fbiinit5=sst_read(FBIINIT5) & FBIINIT5_MASK ;
		if (par->vmode & FB_VMODE_INTERLACED)
			fbiinit5 |= INTERLACE;
		if (par->vmode & FB_VMODE_DOUBLE )
			fbiinit5 |= VDOUBLESCAN;
		if (par->sync & FB_SYNC_HOR_HIGH_ACT)
			fbiinit5 |= HSYNC_HIGH;
		if (par->sync & FB_SYNC_VERT_HIGH_ACT)
			fbiinit5 |= VSYNC_HIGH;
		sst_write(FBIINIT5, fbiinit5);
	}
	sst_wait_idle();
	sst_unset_bits(FBIINIT1, VIDEO_RESET);
	sst_unset_bits(FBIINIT0, FBI_RESET | FIFO_RESET);
	sst_set_bits(FBIINIT2, EN_DRAM_REFRESH);
/* disables fbiinit writes */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, PCI_EN_FIFO_WR);

	/* set lfbmode : set mode + front buffer for reads/writes
	   + disable pipeline  */
	switch(par->bpp) {
	case 16:
		lfbmode = LFB_565;
		break;
#ifdef EN_24_32_BPP
	case 24:
		lfbmode = LFB_888;
		break;
	case 32:
		lfbmode = LFB_8888;
		break;
#endif
	default:
		BUG();
		return 0;
	}

#if defined(__BIG_ENDIAN)
	/* enable byte-swizzle functionality in hardware */
	lfbmode |= ( LFB_WORD_SWIZZLE_WR | LFB_BYTE_SWIZZLE_WR | 
		     LFB_WORD_SWIZZLE_RD | LFB_BYTE_SWIZZLE_RD );
#endif

	if (clipping) {
		sst_write(LFBMODE, lfbmode | EN_PXL_PIPELINE);
	/*
	 * Set "clipping" dimensions. If clipping is disabled and
	 * writes to offscreen areas of the framebuffer are performed,
	 * the "behaviour is undefined" (_very_ undefined) - Urs
	 */
	/* btw, it requires enabling pixel pipeline in LFBMODE.
	   off screen read/writes will just wrap and read/print pixels
	   on screen. Ugly but not that dangerous */

		f_ddprintk("setting clipping dimensions 0..%d, 0..%d\n",
		            par->xDim-1, par->yDim-1);

		sst_write(CLIP_LEFT_RIGHT, par->xDim );
		sst_write(CLIP_LOWY_HIGHY, par->yDim );
		sst_set_bits(FBZMODE, EN_CLIPPING | EN_RGB_WRITE);
	} else {
		/* no clipping : direct access, no pipeline */
		sst_write(LFBMODE, lfbmode );
	}

	sst_info->current_par = *par;
	return 1;
}

static void sst_set_vidmod_att_ti(struct sstfb_info * sst_info, const int bpp)
{
	u8 cr0;

	f_dprintk("sst_set_vidmod_att_ti(bpp: %d)\n", bpp);

	sst_dac_write(DACREG_WMA, 0); 	/* backdoor */
	sst_dac_read(DACREG_RMR);	/* read 4 times RMR */
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	/* the fifth time,  CR0 is read */
	cr0 = sst_dac_read(DACREG_RMR);

	sst_dac_write(DACREG_WMA, 0); 	/* backdoor */
	sst_dac_read(DACREG_RMR);	/* read 4 times RMR */
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	sst_dac_read(DACREG_RMR);
	/* cr0 */
	switch(bpp) {
	case 16:
		sst_dac_write(DACREG_RMR, (cr0 & 0x0f) | DACREG_CR0_16BPP);
		break;
#ifdef EN_24_32_BPP
	case 24:
	case 32:
		sst_dac_write(DACREG_RMR, (cr0 & 0x0f) | DACREG_CR0_24BPP);
		break;
#endif
	default:
		BUG();
	}
}

static void sst_set_vidmod_ics(struct sstfb_info * sst_info, const int bpp)
{
	f_dprintk("sst_set_vidmod_ics(bpp: %d)\n", bpp);
	switch(bpp) {
	case 16:
		sst_dac_write(DACREG_ICS_CMD, DACREG_ICS_CMD_16BPP);
		break;
#ifdef EN_24_32_BPP
	case 24:
	case 32:
		sst_dac_write(DACREG_ICS_CMD, DACREG_ICS_CMD_24BPP);
		break;
#endif
	default:
		BUG();
	}
}

static int __devinit sst_init(struct sstfb_info *sst_info)
{
	struct pll_timing gfx_timings;
	struct sst_spec * spec;
	struct pci_dev * sst_dev = sst_info->dev;
	int Fout;
	u32 fbiinit0, fbiinit1, fbiinit4;

	spec = &voodoo_spec[sst_info->type];
	f_dprintk("sst_init\n");
	f_ddprintk(" fbiinit0   fbiinit1   fbiinit2   fbiinit3   fbiinit4  "
	           " fbiinit6\n");
	f_ddprintk("%0#10x %0#10x %0#10x %0#10x %0#10x %0#10x\n",
	            sst_read(FBIINIT0), sst_read(FBIINIT1), sst_read(FBIINIT2),
	            sst_read(FBIINIT3), sst_read(FBIINIT4), sst_read(FBIINIT6));
	/* disable video clock */
	pci_write_config_dword(sst_dev, PCI_VCLK_DISABLE,0);

	/* enable writing to init registers ,disable pci fifo*/
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, PCI_EN_INIT_WR);
	/* reset video */
	sst_set_bits(FBIINIT1, VIDEO_RESET);
	sst_wait_idle();
	/* reset gfx + pci fifo */
	sst_set_bits(FBIINIT0, FBI_RESET | FIFO_RESET);
	sst_wait_idle();

	/* unreset fifo */
	/*sst_unset_bits(FBIINIT0, FIFO_RESET);
	sst_wait_idle();*/
	/* unreset FBI */
	/*sst_unset_bits(FBIINIT0, FBI_RESET);
	sst_wait_idle();*/

	/* disable dram refresh */
	sst_unset_bits(FBIINIT2, EN_DRAM_REFRESH);
	sst_wait_idle();
	/* remap fbinit2/3 to dac */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
	                               PCI_EN_INIT_WR | PCI_REMAP_DAC );
	/* detect dac type */
	if (!sst_detect_dactype(sst_info)) {
		eprintk("Unknown dac type\n");
		//FIXME watch it : we are not in a safe state , bad bad bad .
		return 0;
	}

	/* set graphic clock */
	sst_info->gfx_clock = spec->default_gfx_clock;
	if ((gfxclk >10 ) && (gfxclk < spec->max_gfxclk)) {
		iprintk ("Using supplied graphic freq : %dMHz\n", gfxclk);
		 sst_info->gfx_clock = gfxclk *1000;
	} else if (gfxclk) {
		wprintk ("You fool, %dMhz is way out of spec! Using default\n", gfxclk);
	}

	sst_calc_pll(sst_info->gfx_clock, &Fout, &gfx_timings);
	sst_info->dac_sw.set_pll(sst_info, &gfx_timings, GFX_CLOCK);

	/* disable fbiinit remap */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
	                       PCI_EN_INIT_WR| PCI_EN_FIFO_WR );
	/* defaults init registers */
	/* FbiInit0: unreset gfx, unreset fifo */
	fbiinit0 = FBIINIT0_DEFAULT;
	fbiinit1 = FBIINIT1_DEFAULT;
	fbiinit4 = FBIINIT4_DEFAULT;
	if (vgapass)
		fbiinit0 &= ~EN_VGA_PASSTHROUGH;
	else
		fbiinit0 |= EN_VGA_PASSTHROUGH;
	if (slowpci) {
		fbiinit1 |= SLOW_PCI_WRITES;
		fbiinit4 |= SLOW_PCI_READS;
	} else {
		fbiinit1 &= ~SLOW_PCI_WRITES;
		fbiinit4 &= ~SLOW_PCI_READS;
	}
	sst_write(FBIINIT0, fbiinit0);
	sst_wait_idle();
	sst_write(FBIINIT1, fbiinit1);
	sst_wait_idle();
	sst_write(FBIINIT2, FBIINIT2_DEFAULT);
	sst_wait_idle();
	sst_write(FBIINIT3, FBIINIT3_DEFAULT);
	sst_wait_idle();
	sst_write(FBIINIT4, fbiinit4);
	sst_wait_idle();
	if (IS_VOODOO2(sst_info)) {
		sst_write(FBIINIT6, FBIINIT6_DEFAULT);
		sst_wait_idle();
	}

	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, PCI_EN_FIFO_WR );
	pci_write_config_dword(sst_dev, PCI_VCLK_ENABLE, 0);

	return 1;
}

static void  __devexit sst_shutdown(struct sstfb_info *sst_info)
{
	struct pci_dev * sst_dev = sst_info->dev;
	struct pll_timing gfx_timings;
	int Fout;

	f_dprintk("sst_shutdown\n");
	/* reset video, gfx, fifo, disable dram + remap fbiinit2/3 */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, PCI_EN_INIT_WR);
	sst_set_bits(FBIINIT1, VIDEO_RESET | EN_BLANKING);
	sst_unset_bits(FBIINIT2, EN_DRAM_REFRESH);
	sst_set_bits(FBIINIT0, FBI_RESET | FIFO_RESET);
	sst_wait_idle();
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
	                       PCI_EN_INIT_WR | PCI_REMAP_DAC );
	/*set 20Mhz gfx clock */
	sst_calc_pll(20000, &Fout, &gfx_timings);
	sst_info->dac_sw.set_pll(sst_info, &gfx_timings, GFX_CLOCK);
	/* TODO maybe shutdown the dac, vrefresh and so on... */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE,
	                       PCI_EN_INIT_WR);
	sst_unset_bits(FBIINIT0, FBI_RESET | FIFO_RESET | EN_VGA_PASSTHROUGH);
	pci_write_config_dword(sst_dev, PCI_VCLK_DISABLE,0);
/* maybe keep fbiinit* and PCI_INIT_enable in the fb_info struct at the beginining ? */
	pci_write_config_dword(sst_dev, PCI_INIT_ENABLE, 0);

}

/*
 * Interface to the world
 */

int  __init sstfb_setup(char *options)
{
	char *this_opt;

	f_dprintk("sstfb_setup\n");

	if (!options || !*options)
		return 0;

	for(this_opt = strtok(options, ","); this_opt;
		this_opt = strtok(NULL, ",")) { //XXX
//XXX	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt) continue;

		f_ddprintk("option %s\n", this_opt);

		if (!strcmp(this_opt, "inverse")) {
			inverse = 1;
			fb_invert_cmaps();
		}
		else if (!strcmp(this_opt, "vganopass"))
			vgapass = 0;
		else if (!strcmp(this_opt, "vgapass"))
			vgapass = 1;
		else if (!strcmp(this_opt, "clipping"))
		        clipping = 1;
		else if (!strcmp(this_opt, "noclipping"))
		        clipping = 0;
		else if (!strcmp(this_opt, "fastpci"))
		        slowpci = 0;
		else if (!strcmp(this_opt, "slowpci"))
		        slowpci = 1;
		else if (!strncmp(this_opt, "mem:",4))
			mem=simple_strtoul (this_opt+4, NULL, 0);
		else if (!strncmp(this_opt, "gfxclk:",7))
			gfxclk=simple_strtoul (this_opt+7, NULL, 0);
		else if (!strncmp(this_opt, "dev:",4))
			dev=simple_strtoul (this_opt+4, NULL, 0);
		else
			mode_option=this_opt;
	}
	return 0;
}

int __devinit sstfb_init(void)
{
	f_dprintk("sstfb_init\n");
	dprintk("Compile date: "__DATE__" "__TIME__"\n");
	return pci_module_init(&sstfb_driver);
}

void __devexit sstfb_exit(void)
{
	f_dprintk("sstfb_exit\n");
	pci_unregister_driver(&sstfb_driver);
}

#ifdef MODULE
module_init(sstfb_init);
module_exit(sstfb_exit);
#endif

static int __devinit sstfb_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct fb_var_screeninfo var;
	struct sstfb_info * sst_info;
	struct sst_spec * spec;
	int tmp,err;
	f_dprintk("sstfb_probe\n");

	/* dev >  0  the device is not the one asked for.   skip */
	/* dev == 0  this is the device the user asked.     init */
	/* dev == -1 we already inited the asked device.    skip */
	/* dev < -1  init all devices. including this one.  init */
	if ((dev == -1 ) || (dev-- > 0))
		return -1;

	if ((err=pci_enable_device(pdev))) {
		eprintk("cannot enable device\n");
		return err;
	}

	sst_info = (struct sstfb_info*)kmalloc(sizeof(*sst_info), GFP_KERNEL);
	if (!sst_info)
		goto fail_kmalloc;
	memset(sst_info, 0, sizeof(*sst_info));
	
	pci_set_drvdata(pdev, sst_info);
	sst_info->type = id->driver_data;
	spec = &voodoo_spec[sst_info->type];
	f_ddprintk("found device : %s\n", spec->name);

	sst_info->dev = pdev;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &sst_info->revision);

	sst_info->mmio.base = pci_resource_start(pdev,0);
	sst_info->video.base = sst_info->mmio.base+0x400000;

	if (!request_mem_region(sst_info->mmio.base,0x400000,"sstfb MMIO")) {
		eprintk ("cannot reserve mmio memory\n");
		goto fail_mmio_mem;
	}

	if (!request_mem_region(sst_info->video.base,0x400000,"sstfb FB")) {
		eprintk ("cannot reserve fb memory\n");
		goto fail_fb_mem;
	}

	sst_info->mmio.vbase = (u_long) ioremap_nocache(sst_info->mmio.base, 0x400000);
	if (!sst_info->mmio.vbase) {
		eprintk("cannot remap register area %#lx\n",
		        sst_info->mmio.base);
		goto fail_mmio_remap;
	}
	sst_info->video.vbase = (u_long) ioremap_nocache(sst_info->video.base, 0x400000);
	if (!sst_info->video.vbase) {
		eprintk("cannot remap framebuffer %#lx\n",
		        sst_info->video.base);
		goto fail_fb_remap;
	}

	if(!sst_init(sst_info)) {
		eprintk("Init failed\n");
		goto fail;
	}
	sst_get_memsize(sst_info, &sst_info->video.len);
	strncpy(sst_info->info.modename, spec->name, 16);

	iprintk("%s with %s dac\n", sst_info->info.modename, sst_info->dac_sw.name);
	iprintk("framebuffer at %#lx, mapped to %#lx,"
	        " size %ldMb\n",
	        sst_info->video.base, sst_info->video.vbase,
	        sst_info->video.len >> 20);

	f_ddprintk("revision: %d\n", sst_info->revision);
	f_ddprintk("regbase_virt: %#lx\n", sst_info->mmio.vbase);
	f_ddprintk("membase_phys: %#lx\n", sst_info->video.base);
	f_ddprintk("fbbase_virt: %#lx\n", sst_info->video.vbase);

	sst_info->info.node       = -1 ;
	sst_info->info.flags      = FBINFO_FLAG_DEFAULT;
	sst_info->info.fbops      = &sstfb_ops;
	sst_info->info.disp       = &sst_info->disp;
	sst_info->info.changevar  = NULL;
	sst_info->info.switch_con = &sstfbcon_switch;
	sst_info->info.updatevar  = &sstfbcon_updatevar;
	sst_info->info.blank      = &sstfbcon_blank;

	tmp=0;
	var = sstfb_default;
	if ( mode_option  &&
	     fb_find_mode(&var, &sst_info->info, mode_option,
	                   NULL, 0, NULL, 16)) {
		if (sstfb_set_var(&var, -1, &sst_info->info)) {
			eprintk("can't set supplied video mode. Using default\n");
			var = sstfb_default;
		} else {
			/* set the new default */
			sstfb_default = var;
			tmp=1;  /* no need to set the mode. */
		}
	}

	if (!tmp && sstfb_set_var(&var, -1, &sst_info->info)) {
		eprintk("can't set default video mode.\n");
		goto fail;
	}
	/*clear fb */
	memset_io(sst_info->video.vbase, 0, sst_info->video.len);
	/* print some squares ... */
	sstfb_test16(sst_info); /* FIXME this is only for 16bpp */

	/* register fb */
	if (register_framebuffer(&sst_info->info) < 0) {
		eprintk("can't register framebuffer.\n");
		goto fail;
	}
	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	       GET_FB_IDX(sst_info->info.node),sst_info->info.modename);

	return 0;

fail:
	iounmap((void *)sst_info->video.base);
fail_fb_remap:
	iounmap((void *)sst_info->mmio.base);
fail_mmio_remap:
	release_mem_region(sst_info->video.base,0x400000);
fail_fb_mem:
	release_mem_region(sst_info->mmio.base,0x400000);
fail_mmio_mem:
	kfree(sst_info);
fail_kmalloc:
	return -ENXIO; 	/* no voodoo detected */
}

static void __devexit sstfb_remove(struct pci_dev *pdev)
{
	struct sstfb_info * sst_info;

	f_dprintk("sstfb_remove\n");
	sst_info=pci_get_drvdata(pdev);
	sst_shutdown(sst_info);

	unregister_framebuffer(&sst_info->info);
	iounmap((void*)sst_info->video.vbase);
	iounmap((void*)sst_info->mmio.vbase);
	release_mem_region(sst_info->video.base,0x400000);
	release_mem_region(sst_info->mmio.base,0x400000);
	kfree(sst_info);
}


/*
 * console driver
 */
static int sstfbcon_switch(int con, struct fb_info *info)
{
#define sst_info	 ((struct sstfb_info *) info)
	struct sstfb_par par;

	f_dprintk("sstfbcon_switch(con: %d)\n",con);
	f_ddprintk("currcon: %d\n", sst_info->currcon);
	v_dprintk("currcon: %d\n", sst_info->currcon);

	if (sst_info->currcon >=  0) {
		if (fb_display[sst_info->currcon].cmap.len)
			fb_get_cmap(&fb_display[sst_info->currcon].cmap, 1,
			            sstfb_getcolreg, info);
	}
	sst_info->currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;

	print_var(&fb_display[con].var, "&fb_display[con: %d].var",con);
	sstfb_decode_var(&fb_display[con].var, &par, sst_info);
	if (memcmp(&par,&(sst_info->current_par),sizeof(par))) {
		sstfb_set_par(&par, sst_info);
	}
	sstfb_install_cmap(con, info);
	return 0;
#undef sst_info
}

static int sstfbcon_updatevar(int con, struct fb_info *info)
{
	f_dprintk("sstfbcon_updatevar\n");
	return -EINVAL;
}

static void sstfbcon_blank(int blank, struct fb_info *info)
{
	f_dprintk("sstfbcon_blank(level %d)\n", blank);
}


/* print some squares on the fb (presuming 16bpp)  */
static void sstfb_test16(struct sstfb_info *sst_info)
{
	int i,j;
	u_long p;
	u_long fbbase_virt = sst_info->video.vbase;

	f_dprintk("sstfb_test16\n");
	/* white rectangle 20x100+200+0 */
	for (i=0 ; i< 100; i++) {
	  p = fbbase_virt + 2048 *i+400;
	  for (j=0; j < 10; j++) {
	    writel(0xffffffff, p);
	    p+=4;
	  }
	}
	/* blue rectangle 180x200+0+0 */
	for (i=0 ; i< 200; i++) {
	  p = fbbase_virt + 2048 *i;
	  for (j=0; j < 90; j++) {
	    writel(0x001f001f, p);
	    p+=4;
	  }
	}
	/* green rectangle 40x40+100+0 */
	for (i=0 ; i< 40 ; i++) {
	  p = fbbase_virt + 2048 *i + 200;
	  for (j=0; j <20; j++) {
	    writel(0x07e007e0, p);
	    p+=4;
	  }
	}
	/* red rectangle 40x40+100+40 */
	for (i=0; i<40; i++) {
	  p = fbbase_virt + 2048 * (i+40) + 200;
	  for (j=0; j <20; j++) {
	    writel( 0xf800f800, p);
	    p+=4;
	  }
	}
}

/* print some squares on the fb (24/32bpp)  */
#ifdef EN_24_32_BPP
static void sstfb_test32(struct sstfb_info *sst_info)
{
	int i,j;
	u_long p;
	u_long fbbase_virt = sst_info->video.vbase;

	f_dprintk("sstfb_test32\n");
	/* rect blanc 20x100+200+0 */
	for (i=0 ; i< 100; i++) {
	  p = fbbase_virt + 4096*i + 800;
	  for (j=0 ; j < 20 ; j++) {
	    writel(0x00ffffff, p);
	    p+=4;
	  }
	}
	/* rect bleu 180x200+0+0 */
	for (i=0 ; i< 200; i++) {
	  p = fbbase_virt + 4096 * i;
	  for (j=0 ; j < 180; j++) {
	    writel(0x000000ff, p);
	    p+=4;
	  }
	}
	/* carre vert 40x40+100+0 */
	for (i=0 ; i< 40 ; i++) {
	  p = fbbase_virt + 4096 *i + 400;
	  for (j=0; j <40; j++) {
	    writel(0x0000ff00, p);
	    p+=4;
	  }
	}
	/*carre rouge 40x40+100+10 */
	for (i=0; i<40; i++) {
	  p = fbbase_virt + 4096 * (i+40) + 400;
	  for (j=0; j <40; j++) {
	    writel(0x00ff0000, p);
	    p+=4;
	  }
	}
}
#endif /* EN_24_32_BPP */

MODULE_AUTHOR("(c) 2000,2002 Ghozlane Toumi <gtoumi@laposte.net>");
MODULE_DESCRIPTION("FBDev driver for 3dfx Voodoo Graphics and Voodoo2 based video boards");
MODULE_LICENSE("GPL");

MODULE_PARM(mem, "i");
MODULE_PARM_DESC(mem, "Size of frame buffer memory in MiB (1, 2, 4 Mb, default=autodetect)");
MODULE_PARM(vgapass, "i");
MODULE_PARM_DESC(vgapass, "Enable VGA PassThrough cable (0 or 1) (default=0)");
MODULE_PARM(inverse, "i");
MODULE_PARM_DESC(inverse, "Inverse colormap (0 or 1) (default=0)");
MODULE_PARM(clipping , "i");
MODULE_PARM_DESC(clipping, "Enable clipping (slower, safer) (0 or 1) (default=1)");
MODULE_PARM(gfxclk , "i");
MODULE_PARM_DESC(gfxclk, "Force graphic chip frequency in Mhz. DANGEROUS. (default=auto)");
MODULE_PARM(slowpci, "i");
MODULE_PARM_DESC(slowpci, "Uses slow PCI settings (0 or 1) (default=0)");
MODULE_PARM(dev,"i");
MODULE_PARM_DESC(dev , "Attach to device ID (0..n) (default=1st device)");

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

#if 0
void __Dump_regs (struct sstfb_info * sst_info)
{
	struct { u32 reg ; char * reg_name;}  pci_regs [] =  {
		{ PCI_INIT_ENABLE, "initenable"},
		{ PCI_VCLK_ENABLE, "enable vclk"},
		{ PCI_VCLK_DISABLE, "disable vclk"},
	};

	struct { u32 reg ; char * reg_name;}  sst_regs [] =  {
		{FBIINIT0,"fbiinit0"},
		{FBIINIT1,"fbiinit1"},
		{FBIINIT2,"fbiinit2"},
		{FBIINIT3,"fbiinit3"},
		{FBIINIT4,"fbiinit4"},
		{FBIINIT5,"fbiinit5"},
		{FBIINIT6,"fbiinit6"},
		{FBIINIT7,"fbiinit7"},
		{LFBMODE,"lfbmode"},
		{FBZMODE,"fbzmode"},
	};
	int pci_s = sizeof(pci_regs)/sizeof(*pci_regs);
	int sst_s = sizeof(sst_regs)/sizeof(*sst_regs);
	u32 pci_res[pci_s];
	u32 sst_res[sst_s];

	struct pci_dev * dev = sst_info->dev;

	int i;

	for (i=0; i < pci_s ; i++ ) {
		pci_read_config_dword ( dev, pci_regs[i].reg , &pci_res[i]) ;
	}
	for (i=0; i < sst_s ; i++ ) {
		sst_res[i]=sst_read(sst_regs[i].reg);
	}

	dprintk ("Dump regs\n");
	for (i=0; i < pci_s ; i++ ) {
		dprintk("%s = %0#10x\n", pci_regs[i].reg_name , pci_res[i]) ;
	}
	for (i=0; i < sst_s ; i++ ) {
		dprintk("%s = %0#10x\n", sst_regs[i].reg_name , sst_res[i]) ;
	}
}
#endif
