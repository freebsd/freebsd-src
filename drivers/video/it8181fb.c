/*  IT8181 console frame buffer driver---it8181fb.c
 *
 *  Copyright (C) 2001 Integrated Technology Express, Inc.
 *  Copyright (C) 2001 MontaVista Software Inc.
 *  Copyright (C) 2002,2003 Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *
 *  Initial work by rich.liu@ite.com.tw
 *
 *  Rewritten by MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 * Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *	- Added support of NEC VR4111 and VR4121.
 *	- rewrote it8181fb_set_par()
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
#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>
#include "vga.h"

static const struct {
	int f_pix_l; // kHz
	int f_pix_h; // kHz
	int m, n, p;
} std_pll_freq[] = {
	{24923, 25427, 1, 14, 3}, // 25.175 MHz +- 1%
	{31185, 31815, 3, 53, 3}, // 31.5 MHz +- 1%
	{39600, 40400, 5, 56, 2}, // 40 MHz +- 1%
	{49005, 49995, 6, 83, 2}, // 49.5 MHz +- 1%
	{49500, 50500, 1, 14, 2}, // 50 MHz +- 1%
	{64350, 65650, 9, 82, 1}, // 65 MHz +- 1%
	{74250, 75750, 2, 21, 1}, // 75 MHz +- 1%
	{0, 0, 0, 0, 0}
};

// FIXME: this is just a guess
#define PLL_MAX_M		18

#define IT8181_GUI_SIZE		0x8000L		/* 32kB */
#define IT8181_MMIO_SIZE	0x10000L	/* 64kB */
#define IT8181_FB_SIZE		0x400000L	/* 4MB  */
#define IT8181_CFG_SIZE		0x50UL

#define PICOS2KHZ(a)		(1000000000UL/(a))
#define KHZ2PICOS(a)		(1000000000UL/(a))

#define CLOCK_REF		14318 // KHz

#define IT8181_STANDBY		0x44	/* Standby register */
#define IT8181_PLL1		0x48	/* PLL1 register */
#define IT8181_PLL2		0x4c	/* PLL2 register */

#define IT8181_PLL1_RST		0x00004000	/* PLL1 reset */
#define IT8181_PLL2_RST		0x00008000

#define IT8181_MODULE_NAME	"IT8181"
#define PFX IT8181_MODULE_NAME

#undef IT8181_DEBUG

#ifdef IT8181_DEBUG
#define dbg(format, arg...) \
    printk(__FUNCTION__ ": " format "\n" , ## arg)
  //printk(KERN_DEBUG __FUNCTION__ ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO PFX ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format "\n" , ## arg)
#define emerg(format, arg...) printk(KERN_EMERG PFX ": " format "\n" , ## arg)

#define CMAPSIZE		16
#define arraysize(x)		(sizeof(x)/sizeof(*(x)))

/*
 * These are fb_var_screeninfo's for 640x480, 800x600, and
 * 1024x768. The bpp field is filled in later.
 */
static const struct fb_var_screeninfo it8181_var_table[] = {
	{	// 640x480@75, 31.5MHz dotclock
		640, 480, 640, 480, 0, 0, -1, 0,
		{0}, {0}, {0}, {0},
		0, FB_ACTIVATE_NOW, -1, -1, 0,
		31747, 120, 16, 16, 1, 64, 3,
		0, FB_VMODE_NONINTERLACED },
	{	// 800x600@72, 50.00MHz dotclock
		800, 600, 800, 600, 0, 0, -1, 0,
		{0}, {0}, {0}, {0},
		0, FB_ACTIVATE_NOW, -1, -1, 0,
		20000, 64, 56, 23, 37, 120, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },
	{	// 1024x768@70, 75.00MHz dotclock
		1024, 768, 1024, 768, 0, 0, -1, 0,
		{0}, {0}, {0}, {0},
		0, FB_ACTIVATE_NOW, -1, -1, 0,
		13334, 144, 24, 29, 3, 136, 6,
		0, FB_VMODE_NONINTERLACED }
};

enum {
	RES_640x480 = 0,
	RES_800x600,
	RES_1024x768
};

/*
 * The default resolution and color depth to use if
 * none was specified on command line.
 */
#define DEFAULT_RES		RES_800x600
#define DEFAULT_BPP		16
#define DEFAULT_MEMORY_BASE	0
#define DEFAULT_CONFIG_OFFSET	0

static const char default_mode[] = "800x600-70";
static char __initdata fontname[40] = { 0 };
static const char *mode_option __initdata = NULL;
static int default_bpp __initdata = DEFAULT_BPP;
static int default_res __initdata = DEFAULT_RES;
static int default_memory_base __initdata = DEFAULT_MEMORY_BASE;
static int default_config_offset __initdata = DEFAULT_CONFIG_OFFSET;

struct it8181fb_par {
        struct fb_var_screeninfo var;

        int HorizRes;          /* The x resolution in pixel */
        int HorizTotal;
        int HorizDispEnd;
        int HorizBlankStart;
        int HorizBlankEnd;
        int HorizSyncStart;
        int HorizSyncEnd;

        int VertRes;           /* the physical y resolution in scanlines */
        int VertTotal;
        int VertDispEnd;
        int VertSyncStart;
        int VertSyncEnd;
        int VertBlankStart;
        int VertBlankEnd;
	
	int pll1_N, pll1_M, pll1_P; // PLL1 settings for pixel clock
	int pll2_N, pll2_M, pll2_P; // PLL2 settings for memory clock
	int pclk;                // pixel clock from above PLL settings, KHz
	
	int line_length;  // in bytes
	int cmap_len;     // color-map length
};

struct it8181fb_info {
        struct fb_info_gen gen;
	struct it8181fb_par current_par;
	struct pci_dev *pdev;
        struct display disp;
        unsigned int itConfigAddrVirt;
        unsigned int itMmioAddrVirt;
        unsigned int itFbAddrVirt;
        unsigned int itGuiCtrlVirt;
        unsigned int itMmioAddrPhys;
        unsigned int itFbAddrPhys;
        unsigned int itGuiCtrlPhys;
	int blank_mode;
	
        struct it8181fb_info *next;
};

static struct it8181fb_info *fb_it8181;
static u32 itMmioAddr;
static int it8181fb_lcd;
static int it8181fb_memory_type;

static char it8181fb_name[16] = IT8181_MODULE_NAME;

static union {
#ifdef FBCON_HAS_CFB16
	u16 cfb16[CMAPSIZE];
#endif
#ifdef FBCON_HAS_CFB24
	u32 cfb24[CMAPSIZE];
#endif
#ifdef FBCON_HAS_CFB32
	u32 cfb32[CMAPSIZE];
#endif
} fbcon_cmap;

/* Interface used by the world */
int it8181fb_init(void);
int it8181fb_setup(char *options, int *ints);
static int it8181fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
			  u_long arg, int con, struct fb_info *info);

/* Hardware Specific Routines */
static void it8181fb_detect (void);
static int it8181fb_encode_fix (struct fb_fix_screeninfo *fix, const void *par,
				struct fb_info_gen *info);
static int it8181fb_decode_var (const struct fb_var_screeninfo *var, void *par,
				struct fb_info_gen *info);
static int it8181fb_encode_var (struct fb_var_screeninfo *var, const void *par,
				struct fb_info_gen *info);
static void it8181fb_get_par (void *par, struct fb_info_gen *info);
static void it8181fb_set_par (const void *par, struct fb_info_gen *info);
static int it8181fb_getcolreg (unsigned regno, unsigned *red, unsigned *green,
			       unsigned *blue, unsigned *transp,
			       struct fb_info *info);
static int it8181fb_setcolreg (unsigned regno, unsigned red, unsigned green,
                            unsigned blue, unsigned transp,
			       struct fb_info *info);
static int it8181fb_pan_display (const struct fb_var_screeninfo *var,
				 struct fb_info_gen *info);
static int it8181fb_blank (int blank_mode, struct fb_info_gen *info);
static void it8181fb_set_disp (const void *par, struct display *disp,
			       struct fb_info_gen *info);

/* Internal routines */
static void set_color_bitfields(struct fb_var_screeninfo *var);
static int it8181fb_calc_pixclock(struct it8181fb_par* par);
static void it8181SetPLL(const struct it8181fb_par* par,
			 const struct it8181fb_info* info);

/* function table of the above functions */
static struct fb_ops it8181fb_ops = {
        owner:          THIS_MODULE,
        fb_get_fix:     fbgen_get_fix,
        fb_get_var:     fbgen_get_var,
        fb_set_var:     fbgen_set_var,
        fb_get_cmap:    fbgen_get_cmap,
        fb_set_cmap:    fbgen_set_cmap,
        fb_pan_display: fbgen_pan_display,
        fb_ioctl:       it8181fb_ioctl,
};

/* function table of the above functions */
static struct fbgen_hwswitch it8181fb_hwswitch =
{
        it8181fb_detect,
        it8181fb_encode_fix,
        it8181fb_decode_var,
        it8181fb_encode_var,
        it8181fb_get_par,
        it8181fb_set_par,
        it8181fb_getcolreg,
        it8181fb_setcolreg,
        it8181fb_pan_display,
        it8181fb_blank,
        it8181fb_set_disp
};

static int itReadConfigDword(const struct it8181fb_info *info, int where, u32 *val)
{
	int result = 0;

#ifdef CONFIG_PCI
	if (info->pdev)
		result =  pci_read_config_dword(info->pdev, where, val);
	else
#endif
		*val = readl(info->itConfigAddrVirt + where);

	return result;
}

static int itWriteConfigDword(const struct it8181fb_info *info, int where, u32 val)
{
	int result = 0;

#ifdef CONFIG_PCI
	if (info->pdev)
		result =  pci_write_config_dword(info->pdev, where, val);
	else
#endif
		writel(val, info->itConfigAddrVirt+ where);

	return result;
}

static inline int itReadRegExtB(unsigned char addr)
{	
	writeb(addr,itMmioAddr+0x3ce);
	return readb(itMmioAddr+0x3cf);
}

static inline void itWriteRegExtB(unsigned char addr, unsigned char value)
{
	int tmp;
	writeb(addr,itMmioAddr+0x3ce);
	writeb(value,itMmioAddr+0x3cf);
	tmp = itReadRegExtB(addr);
}

static inline void itExtBLock(void)
{
	itWriteRegExtB(0x0b,0x35);
	if (itReadRegExtB(0x0b) & 1)
		dbg("ExtB still unlocked!");
}

static inline void itExtBUnlock(void)
{
	itWriteRegExtB(0x0b,0xca);
	if (!(itReadRegExtB(0x0b) & 1))
		dbg("ExtB still locked!");
}

static inline int itReadRegExtA(unsigned char addr)
{	
	writeb(addr,itMmioAddr+0x3d6);
	return readb(itMmioAddr+0x3d7);
}

static inline void itWriteRegExtA(unsigned char addr,unsigned char value)
{
	int tmp;
	writeb(addr,itMmioAddr+0x3d6);
	writeb(value,itMmioAddr+0x3d7);
	tmp = itReadRegExtA(addr);
}

static inline void itExtALock(void)
{
	itWriteRegExtA(0x0b,0xce);
}

static inline void itExtAUnlock(void)
{
	itWriteRegExtA(0x0b,0xec);
}

static inline int itReadRegCrtc(unsigned char addr)
{
	writeb(addr,itMmioAddr+VGA_CRT_IC);
	return readb(itMmioAddr+VGA_CRT_DC);
}

static inline void itWriteRegCrtc(unsigned char addr,unsigned char value)
{
	int tmp;
	writeb(addr,itMmioAddr+VGA_CRT_IC);
	writeb(value,itMmioAddr+VGA_CRT_DC);
	tmp = itReadRegCrtc(addr);
}

static inline uint8_t itReadRegMisc(void)
{
	return readb(itMmioAddr+VGA_MIS_R);
}

static inline void itWriteRegMisc(unsigned char value)
{
	writeb(value, itMmioAddr+VGA_MIS_W);		
}

static inline int itReadRegSEQ(unsigned char addr)
{
	writeb(addr, itMmioAddr+VGA_SEQ_I);
	return readb(itMmioAddr+VGA_SEQ_D);
}

static inline void itWriteRegSEQ(unsigned char addr,unsigned char value)
{
	writeb(addr, itMmioAddr+VGA_SEQ_I);
	writeb(value, itMmioAddr+VGA_SEQ_D);
}

static void set_color_bitfields(struct fb_var_screeninfo *var)
{
	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:	/* RGB 565 */
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 24:	/* RGB 888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:	/* RGBA 8888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
}


static void it8181fb_detect (void)
{
	dbg("");
}

static int it8181fb_encode_fix(struct fb_fix_screeninfo *fix,
				const void* _par,
				struct fb_info_gen* _info)
{
        struct it8181fb_par *par = (struct it8181fb_par *) _par;
        struct it8181fb_info *info = (struct it8181fb_info *) _info;
	struct fb_var_screeninfo* var = &par->var;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, it8181fb_name);

	fix->smem_start = info->itFbAddrPhys;
	fix->smem_len = IT8181_FB_SIZE;
	fix->mmio_start = info->itMmioAddrPhys;
	fix->mmio_len = IT8181_MMIO_SIZE;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
        fix->visual = (var->bits_per_pixel <= 8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
	fix->ywrapstep = 0;
	fix->xpanstep = 1;
	fix->ypanstep = 1;
	fix->line_length = par->line_length;

	return 0;
}

static int  it8181fb_decode_var(const struct fb_var_screeninfo *var,
				void *_par,
				struct fb_info_gen *_info)
{
        struct it8181fb_par *par = (struct it8181fb_par *) _par;
	int xres, rm, hsync, lm;
	int yres, bm, vsync, um;
	
	if (var->vmode & FB_VMODE_DOUBLE) {
		dbg("double-scan modes not supported");
		return -EINVAL;
	}
	if (var->vmode & FB_VMODE_INTERLACED) {
		dbg("interlaced modes not supported");
		return -EINVAL;
	}

	if (!((var->xres == 640 && var->yres == 480) ||
	      (var->xres == 800 && var->yres == 600) ||
	      (var->xres == 1024 && var->yres == 768))) {
		dbg("resolution not supported: %dx%d",
		    var->xres, var->yres);
		return -EINVAL;
	}

        memset (par, 0, sizeof (struct it8181fb_par));
        par->var = *var;

	switch (var->bits_per_pixel) {
	case 2 ... 8:
		par->var.bits_per_pixel = 8;
		break;
	case 9 ... 16:
		par->var.bits_per_pixel = 16;
		break;
	case 17 ... 24:
		par->var.bits_per_pixel = 24;
		break;
	case 25 ... 32:
		par->var.bits_per_pixel = 32;
		break;
	default:
		dbg("color depth %d bpp not supported", var->bits_per_pixel);
		return -EINVAL;
	}

	par->var.width = par->var.height = -1;
	/* no virtual display for now (no panning/scrolling) */
	par->var.xoffset = par->var.yoffset = 0;
	par->var.xres_virtual = par->var.xres;
	par->var.yres_virtual = par->var.yres;
	/* no accels */
	par->var.accel_flags = 0;
	
	set_color_bitfields(&par->var);
	par->cmap_len = (par->var.bits_per_pixel == 8) ? 256 : 16;

	/*
	 *  check memory limit
	 */
	par->line_length =
		par->var.xres_virtual * (par->var.bits_per_pixel>>3);
	if (par->line_length * par->var.yres_virtual > IT8181_FB_SIZE) {
		dbg("not enough video memory for virtual res %dx%dx%d!",
		    par->var.xres_virtual, par->var.yres_virtual,
		    par->var.bits_per_pixel);
		return -ENOMEM;
	}
	
	if (it8181fb_calc_pixclock(par)) {
		return -EINVAL;
	}
	
	xres = var->xres;
	rm = var->right_margin;
	hsync = var->hsync_len;
	lm = var->left_margin;

	yres = var->yres;
	bm = var->lower_margin;
	vsync = var->vsync_len;
	um = var->upper_margin;

	par->HorizRes = xres;
	par->HorizTotal = (xres + rm + hsync + lm) / 8 - 5;
	par->HorizDispEnd = xres / 8 - 1;
	par->HorizBlankStart = xres / 8;
	par->HorizBlankEnd = par->HorizTotal + 5; // does not count with "-5"
	par->HorizSyncStart = (xres + rm) / 8 + 1;
	par->HorizSyncEnd = (xres + rm + hsync) / 8 + 1;

	par->VertRes = yres;
	par->VertTotal = yres + bm + vsync + um - 2;
	par->VertDispEnd = yres - 1;
	par->VertBlankStart = yres;
	par->VertBlankEnd = par->VertTotal;
	par->VertSyncStart = yres + bm - 1;
	par->VertSyncEnd = yres + bm + vsync - 1;

	return 0;
}

static int  it8181fb_encode_var(struct fb_var_screeninfo *var,
				const void *par,
				struct fb_info_gen *info)
{
	*var = ((struct it8181fb_par *)par)->var;
	return 0;
}

static void it8181fb_set_disp(const void *_par, struct display *disp,
			      struct fb_info_gen *_info)
{
        struct it8181fb_par *par = (struct it8181fb_par *) _par;
        struct it8181fb_info *info = (struct it8181fb_info *) _info;

	disp->screen_base = (char *)info->itFbAddrVirt;

	switch (par->var.bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1:
		disp->dispsw = &fbcon_mfb;
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		disp->dispsw = &fbcon_cfb2;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		disp->dispsw = &fbcon_cfb4;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		disp->dispsw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data = fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		disp->dispsw = &fbcon_cfb24;
		disp->dispsw_data = fbcon_cmap.cfb24;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		disp->dispsw = &fbcon_cfb32;
		disp->dispsw_data = fbcon_cmap.cfb32;
		break;
#endif
	default:
		disp->dispsw = &fbcon_dummy;
		disp->dispsw_data = NULL;
		break;
	}
}

static int it8181fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
			  u_long arg, int con, struct fb_info *info)
{
	/* nothing to do yet */
	return -EINVAL;
}

static inline void it8181fb_panel_poweron(void)
{
	if (it8181fb_lcd != 0) {
		itExtAUnlock();
		itWriteRegExtA(0x9b, 0x01);
		itExtALock();

		mdelay(120);
	}
}

static inline void it8181fb_panel_poweroff(void)
{
	if (it8181fb_lcd != 0) {
		itExtAUnlock();
		itWriteRegExtA(0x9b, 0x00);
		itExtALock();

		mdelay(120);
	}
}

/*
 *  Blank the display.
 *
 * 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off
 */
static int it8181fb_blank(int mode, struct fb_info_gen *_info)
{
	struct it8181fb_info *p = (struct it8181fb_info *)_info;

        if (p->blank_mode == mode)
                return 0;

	itExtBUnlock();

	mdelay(25);

	switch (mode) {
	case 0:
		// turn display sync's back on
		itWriteRegExtB(0x3c, 0x00);
		itWriteRegSEQ(VGA_SEQ_CLOCK_MODE, 0); // unblank

		it8181fb_panel_poweron();
		break;
	case 1:
		// turn display sync's back on
		itWriteRegExtB(0x3c, 0x00);
                itWriteRegSEQ(VGA_SEQ_CLOCK_MODE, 0x20); // blank

		it8181fb_panel_poweroff();
		break;
	case 2:
                itWriteRegSEQ(VGA_SEQ_CLOCK_MODE, 0x20); // blank
		itWriteRegExtB(0x3c, 0x08); // disable vsync

		it8181fb_panel_poweroff();
		break;
	case 3:
                itWriteRegSEQ(VGA_SEQ_CLOCK_MODE, 0x20); // blank
		itWriteRegExtB(0x3c, 0x02); // disable hsync

		it8181fb_panel_poweroff();
		break;
	case 4:
                itWriteRegSEQ(VGA_SEQ_CLOCK_MODE, 0x20); // blank
		itWriteRegExtB(0x3c, 0x0a); // disable sync

		it8181fb_panel_poweroff();
		break;
	}

	itExtBLock();

	p->blank_mode = mode;

	return 0;
}

static int it8181fb_calc_pixclock(struct it8181fb_par* par)
{
	int i;
	int f_pix, f_actual;

	/*
	 * If the desired pixel clock is one of the standard
	 * VESA frequencies, pull the {N,M,P} values from the
	 * std_pll_freq[] table, otherwise we have to calculate
	 * the {N,M,P} values.
	 */

	f_pix = PICOS2KHZ(par->var.pixclock);

	for (i=0; std_pll_freq[i].f_pix_l; i++) {
		if (f_pix >= std_pll_freq[i].f_pix_l &&
		    f_pix <= std_pll_freq[i].f_pix_h) {
			par->pll1_N = std_pll_freq[i].n;
			par->pll1_M = std_pll_freq[i].m;
			par->pll1_P = std_pll_freq[i].p;
			break;
		}
	}

	if (!std_pll_freq[i].f_pix_l) {
		int r, N, M, P;
		// Pixel clock is not one of the standard VESA frequencies.
		if (f_pix < 2*CLOCK_REF)      P = 3;
		else if (f_pix < 4*CLOCK_REF) P = 2;
		else if (f_pix < 6*CLOCK_REF) P = 1;
		else                          P = 0;
		
		r = ((1<<10) * f_pix * (1<<P)) / CLOCK_REF;
		
		if (r > 255*(1<<10) || r < (1<<10)/PLL_MAX_M) {
			dbg("desired pixclock out of range");
			return -EINVAL;
		}
		
		if (r == (1<<10)) {
			// r = 1.0
			N = M = PLL_MAX_M;
		} else if (r > (1<<10)) {
			// r > 1.0
			for (N=255; N>1; N--) {
				// round-up
				M = ((N*(1<<11) + (1<<10)) / r) / 2;
				if (M <= PLL_MAX_M)
					break;
			}
		} else {
			// r < 1.0
			M = PLL_MAX_M;
			N = (PLL_MAX_M * r + (1<<9)) / (1<<10); // round-up
		}
		
		par->pll1_N = N;
		par->pll1_M = M;
		par->pll1_P = P;
	}
	

	f_actual = (par->pll1_N * CLOCK_REF) / (par->pll1_M * (1<<par->pll1_P));
	
	dbg("N=%d, M=%d, P=%d", par->pll1_N, par->pll1_M, par->pll1_P);
	dbg("desired=%d KHz, actual=%d KHz, error=%d%%",
	    f_pix, f_actual, (100*abs(f_actual - f_pix)) / f_pix);

	par->var.pixclock = KHZ2PICOS(f_actual);
	return 0;
}

static void it8181SetPLL(const struct it8181fb_par* par,
			 const struct it8181fb_info* info)
{
	itWriteConfigDword(info, IT8181_STANDBY, 0x0000c000);

	// program PLL1
	itWriteConfigDword(info, IT8181_PLL1,
	                   (par->pll1_P << 16) |
	                   (((-par->pll1_N) & 0xff) << 8) |
	                   (((-par->pll1_M) & 0x3f)));

	// PLL2 = 49.5 MHz
	itWriteConfigDword(info, IT8181_PLL2, 0x0002AD3A);

	itWriteConfigDword(info, IT8181_STANDBY, 0x00000110);
	mdelay(25); // wait 25 msec
}

/* get current video mode */
static void it8181fb_get_par (void *_par, struct fb_info_gen *_info)
{
        struct it8181fb_par *par = (struct it8181fb_par *) _par;
        struct it8181fb_info *info = (struct it8181fb_info *) _info;

        *par = info->current_par;
}

/*
  setmod for IT8181 chip
*/
static void it8181fb_set_par(const void *_par, struct fb_info_gen *_info)
{
        struct it8181fb_par *par = (struct it8181fb_par *) _par;
        struct it8181fb_info *info = (struct it8181fb_info *) _info;

	dbg("%dx%dx%d", par->var.xres, par->var.yres, par->var.bits_per_pixel);
	dbg("%d %d %d %d %d %d %d",
	    par->var.pixclock,
	    par->var.left_margin,
	    par->var.right_margin,
	    par->var.upper_margin,
	    par->var.lower_margin,
	    par->var.hsync_len,
	    par->var.vsync_len);
	    
	/* disable cursor */
	writeb(0x00, info->itGuiCtrlVirt + 0x100);

	itExtBUnlock();
	itExtAUnlock();

	mdelay(25);

	// disable display sync
	itWriteRegExtB(0x3c, 0x0a);

	// Set RAM to 1M*16bit Symmetric DRAM	
	itWriteRegExtB(0x2f,0x80);

	// enable GUI engine registers
	itWriteRegExtB(0x2b,0x10);

	// set pixel clock
	it8181SetPLL(par, info);

	// Step2
	if (par->var.xres == 640 && par->var.yres == 480 &&
	    (par->var.bits_per_pixel == 8 || par->var.bits_per_pixel == 16)) {
		itWriteRegMisc(0xc3);

		itWriteRegCrtc(VGA_CRTC_H_TOTAL, 0x5f);
		itWriteRegCrtc(VGA_CRTC_H_DISP, 0x4f);
		itWriteRegCrtc(VGA_CRTC_H_BLANK_START, 0x40);
		itWriteRegCrtc(VGA_CRTC_H_BLANK_END, 0x80);
		itWriteRegCrtc(VGA_CRTC_H_SYNC_START, 0x52);
		itWriteRegCrtc(VGA_CRTC_H_SYNC_END, 0x81);
		itWriteRegCrtc(VGA_CRTC_V_TOTAL, 0x0b);
		itWriteRegCrtc(VGA_CRTC_OVERFLOW, 0x3e);
		itWriteRegCrtc(VGA_CRTC_PRESET_ROW, 0x00);
		itWriteRegCrtc(VGA_CRTC_MAX_SCAN, 0x40);
		itWriteRegCrtc(VGA_CRTC_CURSOR_START, 0x00);
		itWriteRegCrtc(VGA_CRTC_CURSOR_END, 0x00);
		itWriteRegCrtc(VGA_CRTC_START_HI, 0x00);
		itWriteRegCrtc(VGA_CRTC_START_LO, 0x00);
		itWriteRegCrtc(VGA_CRTC_CURSOR_HI, 0x01);
		itWriteRegCrtc(VGA_CRTC_CURSOR_LO, 0x49);
		itWriteRegCrtc(VGA_CRTC_V_SYNC_START, 0xea);
		itWriteRegCrtc(VGA_CRTC_V_SYNC_END, 0x8c);
		itWriteRegCrtc(VGA_CRTC_V_DISP_END, 0xdf);
		itWriteRegCrtc(VGA_CRTC_OFFSET, 0x50);
		itWriteRegCrtc(VGA_CRTC_UNDERLINE, 0x00);
		itWriteRegCrtc(VGA_CRTC_V_BLANK_START, 0xe7);
		itWriteRegCrtc(VGA_CRTC_V_BLANK_END, 0x04);
		itWriteRegCrtc(VGA_CRTC_MODE, 0xe3);
		itWriteRegCrtc(VGA_CRTC_LINE_COMPARE, 0xff);
	} else if (par->var.xres == 800 && par->var.yres == 600 &&
	           (par->var.bits_per_pixel == 8 || par->var.bits_per_pixel == 16)) {
		itWriteRegMisc(0x03);

		itWriteRegCrtc(VGA_CRTC_H_TOTAL, 0x07);
		itWriteRegCrtc(VGA_CRTC_H_DISP, 0x63);
		itWriteRegCrtc(VGA_CRTC_H_BLANK_START, 0x64);
		itWriteRegCrtc(VGA_CRTC_H_BLANK_END, 0x81);
		itWriteRegCrtc(VGA_CRTC_H_SYNC_START, 0x6c);
		itWriteRegCrtc(VGA_CRTC_H_SYNC_END, 0x18);
		itWriteRegCrtc(VGA_CRTC_V_TOTAL, 0x72);
		itWriteRegCrtc(VGA_CRTC_OVERFLOW, 0x0f);
		itWriteRegCrtc(VGA_CRTC_PRESET_ROW, 0x00);
		itWriteRegCrtc(VGA_CRTC_MAX_SCAN, 0x60);
		itWriteRegCrtc(VGA_CRTC_CURSOR_START, 0x00);
		itWriteRegCrtc(VGA_CRTC_CURSOR_END, 0x00);
		itWriteRegCrtc(VGA_CRTC_START_HI, 0x00);
		itWriteRegCrtc(VGA_CRTC_START_LO, 0x00);
		itWriteRegCrtc(VGA_CRTC_CURSOR_HI, 0x00);
		itWriteRegCrtc(VGA_CRTC_CURSOR_LO, 0xaa);
		itWriteRegCrtc(VGA_CRTC_V_SYNC_START, 0x59);
		itWriteRegCrtc(VGA_CRTC_V_SYNC_END, 0xad);
		itWriteRegCrtc(VGA_CRTC_V_DISP_END, 0x57);
		itWriteRegCrtc(VGA_CRTC_OFFSET, 0x64);
		itWriteRegCrtc(VGA_CRTC_UNDERLINE, 0x00);
		itWriteRegCrtc(VGA_CRTC_V_BLANK_START, 0x59);
		itWriteRegCrtc(VGA_CRTC_V_BLANK_END, 0x00);
		itWriteRegCrtc(VGA_CRTC_MODE, 0xe3);
		itWriteRegCrtc(VGA_CRTC_LINE_COMPARE, 0xff);
	}


	if (par->var.bits_per_pixel == 8) {
		if (par->var.xres == 640 && par->var.yres == 480)
			itWriteRegCrtc(VGA_CRTC_OFFSET, 0x28);
		else if (par->var.xres == 800 && par->var.yres == 600)
			itWriteRegCrtc(VGA_CRTC_OFFSET, 0x32);
	}

	itWriteRegSEQ(VGA_SEQ_RESET, 0x03);
	itWriteRegExtB(0x06, 0x01);
	itWriteRegExtB(0x20, 0x01);

	if (par->var.xres == 640 && par->var.yres == 480) {
		if (par->var.bits_per_pixel == 8)
			itWriteRegExtB(0x38, 0x01);
		else if (par->var.bits_per_pixel == 16)
			itWriteRegExtB(0x38, 0x03);
		itWriteRegExtB(0x39, 0x40);
	} else if (par->var.xres == 800 && par->var.yres == 600) {
		if (par->var.bits_per_pixel == 8)
			itWriteRegExtB(0x38, 0x21);
		else if (par->var.bits_per_pixel == 16)
			itWriteRegExtB(0x38, 0x23);
		itWriteRegExtB(0x39, 0x44);
	}

	writeb(0xff, info->itMmioAddrVirt+VGA_PEL_MSK);

	// Step3
	if (it8181fb_lcd) {
		if (par->var.xres == 640 && par->var.yres == 480 &&
		    (par->var.bits_per_pixel == 8 ||
		     par->var.bits_per_pixel == 16)) {
			dbg("LCD 640x480-8/16 selected");
			itWriteRegExtA(0x80, 0x4e);
			itWriteRegExtA(0x81, 0x00);
			itWriteRegExtA(0x82, 0x13);
			itWriteRegExtA(0x83, 0x00);
			itWriteRegExtA(0x84, 0x10);
			itWriteRegExtA(0x85, 0xdf);
			itWriteRegExtA(0x86, 0x00);
			itWriteRegExtA(0x87, 0x28);
			itWriteRegExtA(0x88, 0x00);
			itWriteRegExtA(0x89, 0x00);
			itWriteRegExtA(0x8a, 0x81);
			itWriteRegExtA(0x8b, 0x4e);
			itWriteRegExtA(0x8c, 0x00);
			itWriteRegExtA(0x8d, 0x2b);
			itWriteRegExtA(0x8e, 0x00);
			itWriteRegExtA(0x8f, 0x00);
			itWriteRegExtA(0x90, 0x01);
			itWriteRegExtA(0x91, 0x00);
			itWriteRegExtA(0x92, 0x00);
			itWriteRegExtA(0x93, 0x10);

			itWriteRegExtA(0xa0, 0x23);

			itWriteRegExtA(0xb0, 0x04);
			itWriteRegExtA(0xb1, 0x04);
			itWriteRegExtA(0xb2, 0x40);
			itWriteRegExtA(0xb3, 0x40);
			itWriteRegExtA(0xb4, 0x08);
			itWriteRegExtA(0xb5, 0x08);
			itWriteRegExtA(0xb6, 0x80);
			itWriteRegExtA(0xb7, 0x80);
			itWriteRegExtA(0xb8, 0x2a);
			itWriteRegExtA(0xb9, 0x3b);
			itWriteRegExtA(0xba, 0x6e);
			itWriteRegExtA(0xbb, 0x7f);
			itWriteRegExtA(0xbc, 0x80);
			itWriteRegExtA(0xbd, 0x19);
			itWriteRegExtA(0xbe, 0xc4);
			itWriteRegExtA(0xbf, 0xd5);
		} else if (par->var.xres == 800 && par->var.yres == 600 &&
		           (par->var.bits_per_pixel == 8 ||
			    par->var.bits_per_pixel == 16)) {
			dbg("LCD 800x600-8/16 selected");
			itWriteRegExtA(0x80, 0x62);
			itWriteRegExtA(0x81, 0x00);
			itWriteRegExtA(0x82, 0x1f);
			itWriteRegExtA(0x83, 0x00);
			itWriteRegExtA(0x84, 0x10);
			itWriteRegExtA(0x85, 0x57);
			itWriteRegExtA(0x86, 0x18);
			itWriteRegExtA(0x87, 0x48);
			itWriteRegExtA(0x88, 0x00);
			itWriteRegExtA(0x89, 0x00);
			itWriteRegExtA(0x8a, 0xf9);
			itWriteRegExtA(0x8b, 0xc7);
			itWriteRegExtA(0x8c, 0x00);
			itWriteRegExtA(0x8d, 0x1a);
			itWriteRegExtA(0x8e, 0x00);
			itWriteRegExtA(0x8f, 0x00);
			itWriteRegExtA(0x90, 0x41);
			itWriteRegExtA(0x91, 0x00);
			itWriteRegExtA(0x92, 0x00);
			itWriteRegExtA(0x93, 0x10);

			itWriteRegExtA(0xa0, 0x23);

			itWriteRegExtA(0xb0, 0x00);
			itWriteRegExtA(0xb1, 0x00);
			itWriteRegExtA(0xb2, 0x00);
			itWriteRegExtA(0xb3, 0x00);
			itWriteRegExtA(0xb4, 0x00);
			itWriteRegExtA(0xb5, 0x00);
			itWriteRegExtA(0xb6, 0x00);
			itWriteRegExtA(0xb7, 0x00);
			itWriteRegExtA(0xb8, 0x50);
			itWriteRegExtA(0xb9, 0x7b);
			itWriteRegExtA(0xba, 0x8c);
			itWriteRegExtA(0xbb, 0x2e);
			itWriteRegExtA(0xbc, 0x14);
			itWriteRegExtA(0xbd, 0xa6);
			itWriteRegExtA(0xbe, 0xd9);
			itWriteRegExtA(0xbf, 0xf3);
		}

		if (it8181fb_memory_type == 0) {
			itWriteRegExtA(0xa8, 0x30);
			itWriteRegExtA(0xa9, 0x3c);
		} else {
			itWriteRegExtA(0xa8, 0xc0);
			itWriteRegExtA(0xa9, 0xe0);
		}

		itWriteRegExtA(0xa1, 0x01);
		itWriteRegExtA(0xa2, 0x00);

		itWriteRegExtA(0x9a, 0x22);
		itWriteRegExtA(0x9b, 0x01);
	}

	// turn display sync's back on
	itWriteRegExtB(0x3c, 0x00);

	itExtBLock();
	itExtALock();

	mdelay(25);

	/* this par is now current par */
	info->current_par = *par;
}

/*
 * Set/Get the color of a palette entry in 8bpp mode 
 */
static inline void
do_setpalentry(unsigned regno, u8 r, u8 g, u8 b)
{
	writeb((u8)regno, itMmioAddr+VGA_PEL_IW);
	writeb(r, itMmioAddr+VGA_PEL_D);
	writeb(g, itMmioAddr+VGA_PEL_D);
	writeb(b, itMmioAddr+VGA_PEL_D);	
}

static inline void
do_getpalentry(unsigned regno, u8* r, u8* g, u8* b)
{
	writeb((u8)regno, itMmioAddr+VGA_PEL_IW);
	*r = readb(itMmioAddr+VGA_PEL_D);
	*g = readb(itMmioAddr+VGA_PEL_D);
	*b = readb(itMmioAddr+VGA_PEL_D);	
}

static int it8181fb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			      unsigned *blue, unsigned *transp,
			      struct fb_info *_info)
{
	u8 r,g,b;

        if (regno > 255)
                return 1;

	do_getpalentry(regno, &r, &g, &b);
	
        *red    = (unsigned)r << 10;
        *green  = (unsigned)g << 10;
        *blue   = (unsigned)b << 10;
        *transp = 0;

        return 0;
}

static int it8181fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			      unsigned blue, unsigned transp,
			      struct fb_info *_info)
{
	struct it8181fb_info *p = (struct it8181fb_info *) _info;
	struct it8181fb_par* par = &p->current_par;
	
	if (regno > 255)
		return -EINVAL;

	do_setpalentry(regno, (u8)(red>>10),
		       (u8)(green>>10), (u8)(blue>>10));

	switch (par->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
	case 16:
		if(regno < CMAPSIZE)
			fbcon_cmap.cfb16[regno] =
				((red & 0xf800) >> 0) |
				((green & 0xf800) >> 5) |
				((blue & 0xf800) >> 11);
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		if (regno < CMAPSIZE)
			fbcon_cmap.cfb24[regno] =
				((red & 0xff00) << 8) |
				((green & 0xff00)) |
				((blue & 0xff00) >> 8);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		if(regno < CMAPSIZE)
			fbcon_cmap.cfb32[regno] =
				((red & 0xff00) >> 8) |
				((green & 0xff00)) |
				((blue & 0xff00) << 8);
		break;
#endif
	default: 
		break;
	}

	return 0;
}

static int it8181fb_pan_display (const struct fb_var_screeninfo *var,
				 struct fb_info_gen *info)
{
	/* not implemented */
	return 0;
}

/*
 *  Initialisation
 */
int it8181fb_init(void)
{
	struct fb_var_screeninfo var;
	struct it8181fb_info *p = NULL;

	fb_it8181 = p =
		(struct it8181fb_info *) kmalloc(sizeof(*p), GFP_ATOMIC);
	if(p==NULL)
		return -ENOMEM;
	memset(p, 0, sizeof(*p));

#ifdef CONFIG_PCI
	p->pdev = (struct pci_dev *) pci_find_device(PCI_VENDOR_ID_ITE,
						     PCI_DEVICE_ID_ITE_IT8181,
						     NULL);
#endif
	if(!p->pdev) {
		u32 id = 0;
		if (default_config_offset)
			p->itConfigAddrVirt = (u32) ioremap(default_memory_base +
			                                    default_config_offset,
			                                    IT8181_CFG_SIZE);
		if (p->itConfigAddrVirt)
			itReadConfigDword(p, PCI_VENDOR_ID, &id);
		if (!(id == (PCI_VENDOR_ID_ITE | (PCI_DEVICE_ID_ITE_IT8181 << 16))))
			return -ENODEV;
	}
	
	itReadConfigDword(p, PCI_BASE_ADDRESS_0, &(p->itFbAddrPhys));
	itReadConfigDword(p, PCI_BASE_ADDRESS_1, &(p->itGuiCtrlPhys));
	itReadConfigDword(p, PCI_BASE_ADDRESS_2, &(p->itMmioAddrPhys));

	p->itFbAddrVirt = (u32) ioremap(p->itFbAddrPhys + default_memory_base,
	                                IT8181_FB_SIZE);
	p->itGuiCtrlVirt = (u32) ioremap(p->itGuiCtrlPhys + default_memory_base,
	                                 IT8181_GUI_SIZE);
	p->itMmioAddrVirt = itMmioAddr = (u32) ioremap(p->itMmioAddrPhys +
	                                               default_memory_base,
	                                               IT8181_MMIO_SIZE);

	info("Mmio at 0x%08x, Framebuffer at 0x%08x", itMmioAddr, p->itFbAddrVirt);

        /* set up a few more things, register framebuffer driver etc */
        p->gen.parsize = sizeof (struct it8181fb_par);
        p->gen.fbhw = &it8181fb_hwswitch;

	strcpy(p->gen.info.modename, "ITE "); 
	strcat(p->gen.info.modename, it8181fb_name);
	p->gen.info.changevar = NULL;
	p->gen.info.node = -1;

	p->gen.info.fbops = &it8181fb_ops;
	p->gen.info.disp = &p->disp;
	p->gen.info.switch_con = &fbgen_switch;
	p->gen.info.updatevar = &fbgen_update_var;
	p->gen.info.blank = &fbgen_blank;
	p->gen.info.flags = FBINFO_FLAG_DEFAULT;

	if(!mode_option || !fb_find_mode(&var, &p->gen.info,
					 mode_option, NULL, 0, NULL, 16)) {
		var = it8181_var_table[default_res];
		var.bits_per_pixel = default_bpp;
	}
	
        if (fbgen_do_set_var(&var, 1, &p->gen)) {
                /*
                 * Can't use the mode from the mode db or the default
                 * mode - give up
                 */
                err("boot video mode failed");
                goto ret_enxio;
        }

        p->disp.var = var;
        fbgen_set_disp(-1, &p->gen);
        fbgen_install_cmap(0, &p->gen);

	if (register_framebuffer(&p->gen.info) < 0) {
		goto ret_enxio;
	}

	return 0;

 ret_enxio:
	kfree(p);
	iounmap((void *)itMmioAddr);
	iounmap((void *)p->itGuiCtrlVirt);
	iounmap((void *)p->itFbAddrVirt);
	return -ENXIO;
}

#ifdef MODULE

int init_module(void)
{
	return it8181fb_init();
}

void cleanup_module(void)
{
	unregister_framebuffer(&(fb_it8181->gen.info));
	iounmap((void*)itMmioAddr);
	iounmap((void*)fb_it8181->itFbAddrVirt);
	kfree(fb_it8181);
}

#else

int it8181fb_setup(char *options, int *ints)
{
	char *this_opt;
	int xres, cpu = 0, mod;
	
	if (!options || !*options)
		return 1;

	for (this_opt = strtok(options, ","); this_opt;
	     this_opt = strtok(NULL, ",")) {
		if (!strncmp(this_opt, "font:", 5)) {
			strcpy(fontname, this_opt+5);
		} else if (!strncmp(this_opt, "bpp:", 4)) {
			default_bpp = simple_strtoul(this_opt+4, NULL, 0);
		} else if (!strncmp(this_opt, "xres:", 5)) {
			xres = simple_strtoul(this_opt+5, NULL, 0);
			if (xres == 640)
				default_res = RES_640x480;
			else if (xres == 800)
				default_res = RES_800x600;
			else if (xres == 1024)
				default_res = RES_1024x768;
		} else if (!strncmp(this_opt, "cpu:", 4)) {
			if (!strncmp(this_opt+4, "vr4111", 6)) {
				cpu = 1;
				default_memory_base = 0xa000000;
			} else if (!strncmp(this_opt + 4, "sh7750", 6)) {
				cpu = 2;
			}
		} else if (!strncmp(this_opt, "mod:", 4)) {
			mod = simple_strtoul(this_opt+4, NULL, 0);
			if (cpu == 1) {
				/* NEC VR4111 or VR4121 */
				if (mod == 1)
					default_memory_base += 0x800000;
				default_config_offset = 0x800000 - 0x400;
			} else if (cpu == 2) {
				/* Hitachi SH7750 */
				if (mod == 0)
					default_config_offset = 0x2000000 - 0x400;
				else if (mod == 1)
					default_config_offset = 0x3000000 - 0x400;
			}
		}else if (!strncmp(this_opt, "lcd:", 4)) {
			it8181fb_lcd = simple_strtoul(this_opt+4, NULL, 0);
		}else if (!strncmp(this_opt, "mtype:", 6)) {
			it8181fb_memory_type = simple_strtoul(this_opt+6, NULL, 0);
		} else {
			mode_option = this_opt;
		}
	}

	return 0;
}

#endif /* MODULE */
