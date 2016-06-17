/*
 *	epson1356fb.c  --  Epson SED1356 Framebuffer Driver
 *
 *	Copyright 2001, 2002, 2003 MontaVista Software Inc.
 *	Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
 *
 *	This program is free software; you can redistribute  it and/or modify it
 *	under  the terms of  the GNU General  Public License as published by the
 *	Free Software Foundation;  either version 2 of the  License, or (at your
 *	option) any later version.
 *
 *	THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *	WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *	NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *	NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *	USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *	ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	You should have received a copy of the  GNU General Public License along
 *	with this program; if not, write  to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *	TODO:
 *
 *	Revision history
 *	03.12.2001  0.1   Initial release
 *
 */

#include <linux/config.h>
#include <linux/version.h>
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
#include <linux/console.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/nvram.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/pagemap.h>

#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/tlb.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include <linux/spinlock.h>

#include <video/e1356fb.h>

#ifdef CONFIG_SOC_AU1X00
#include <asm/au1000.h>
#endif

#define E1356FB_DEBUG 1
#undef E1356FB_VERBOSE_DEBUG
#undef SHADOW_FRAME_BUFFER
#include "epson1356fb.h"

static char *options;
MODULE_PARM(options, "s");

/*
 *  Frame buffer device API
 */
static int e1356fb_open(struct fb_info *fb, int user);
static int e1356fb_release(struct fb_info *fb, int user);
static int e1356fb_get_fix(struct fb_fix_screeninfo* fix, 
			   int con,
			   struct fb_info* fb);
static int e1356fb_get_var(struct fb_var_screeninfo* var, 
			   int con,
			   struct fb_info* fb);
static int e1356fb_set_var(struct fb_var_screeninfo* var,
			   int con,
			   struct fb_info* fb);
static int e1356fb_pan_display(struct fb_var_screeninfo* var, 
			       int con,
			       struct fb_info* fb);
static int e1356fb_get_cmap(struct fb_cmap *cmap, 
			    int kspc, 
			    int con,
			    struct fb_info* info);
static int e1356fb_set_cmap(struct fb_cmap* cmap, 
			    int kspc, 
			    int con,
			    struct fb_info* info);
static int e1356fb_ioctl(struct inode* inode, 
			 struct file* file, 
			 u_int cmd,
			 u_long arg, 
			 int con, 
			 struct fb_info* info);
static int e1356fb_mmap(struct fb_info *info,
			struct file *file,
			struct vm_area_struct *vma);

/*
 *  Interface to the low level console driver
 */
static int  e1356fb_switch_con(int con, 
			       struct fb_info* fb);
static int  e1356fb_updatevar(int con, 
			      struct fb_info* fb);
static void e1356fb_blank(int blank, 
			  struct fb_info* fb);

/*
 *  Internal routines
 */
static void e1356fb_set_par(const struct e1356fb_par* par,
			    struct fb_info_e1356* 
			    info);
static int  e1356fb_var_to_par(const struct fb_var_screeninfo *var,
			       struct e1356fb_par* par,
			       const struct fb_info_e1356* info);
static int  e1356fb_par_to_var(struct fb_var_screeninfo* var,
			       struct e1356fb_par* par,
			       const struct fb_info_e1356* info);
static int  e1356fb_encode_fix(struct fb_fix_screeninfo* fix,
			       const struct e1356fb_par* par,
			       const struct fb_info_e1356* info);
static void e1356fb_set_dispsw(struct display* disp, 
			       struct fb_info_e1356* info,
			       int bpp, 
			       int accel);
static int  e1356fb_getcolreg(u_int regno,
			      u_int* red, 
			      u_int* green, 
			      u_int* blue,
			      u_int* transp, 
			      struct fb_info* fb);
static int  e1356fb_setcolreg(u_int regno, 
			      u_int red, 
			      u_int green, 
			      u_int blue,
			      u_int transp, 
			      struct fb_info* fb);
static void  e1356fb_install_cmap(struct display *d, 
				  struct fb_info *info);

static void e1356fb_hwcursor_init(struct fb_info_e1356* info);
static void e1356fb_createcursorshape(struct display* p);
static void e1356fb_createcursor(struct display * p);  

/*
 * do_xxx: Hardware-specific functions
 */
static void  do_pan_var(struct fb_var_screeninfo* var,
			struct fb_info_e1356* i);
static void  do_flashcursor(unsigned long ptr);
static void  doBlt_Move(const struct e1356fb_par* par,
			struct fb_info_e1356* i,
			blt_info_t* blt);
static void  doBlt_SolidFill(const struct e1356fb_par* par,
			     struct fb_info_e1356* i,
			     blt_info_t* blt);

/*
 *  Interface used by the world
 */
int e1356fb_init(void);
void e1356fb_setup(char *options, int *ints);

static int currcon = 0;

static struct fb_ops e1356fb_ops = {
	owner:	THIS_MODULE,
	fb_open:        e1356fb_open,
	fb_release:     e1356fb_release,
	fb_get_fix:	e1356fb_get_fix,
	fb_get_var:	e1356fb_get_var,
	fb_set_var:	e1356fb_set_var,
	fb_get_cmap:    e1356fb_get_cmap,
	fb_set_cmap:    e1356fb_set_cmap,
	fb_pan_display: e1356fb_pan_display,
	fb_ioctl:	e1356fb_ioctl,
	fb_mmap:        e1356fb_mmap,
};

#define PCI_VENDOR_ID_EPSON         0x10f4
#define PCI_DEVICE_ID_EPSON_SDU1356 0x1300


static struct fb_info_e1356 fb_info;
static struct e1356fb_fix boot_fix; // boot options
static struct e1356fb_par boot_par; // boot options

/* ------------------------------------------------------------------------- 
 *                      Hardware-specific funcions
 * ------------------------------------------------------------------------- */

/*
 * The SED1356 has only a 16-bit wide data bus, so some embedded
 * implementations with 32-bit CPU's (Alchemy Pb1000) may not
 * correctly emulate a 32-bit write to the framebuffer by splitting
 * the write into two seperate 16-bit writes. So it is safest to
 * only do byte or half-word writes to the fb. This routine assumes
 * fbaddr is atleast aligned on a half-word boundary.
 */
static inline void
fbfill(u16* fbaddr, u8 val, int size)
{
	u16 valw = (u16)val | ((u16)val << 8);
	for ( ; size >= 2; size -= 2)
		writew(valw, fbaddr++);
	if (size)
		writeb(val, (u8*)fbaddr);
}

static inline int
e1356_wait_bitclr(u8* reg, u8 bit, int timeout)
{
	while (readb(reg) & bit) {
		udelay(10);
		if (!--timeout)
			break;
	}
	return timeout;
}

static inline int
e1356_wait_bitset(u8* reg, u8 bit, int timeout)
{
	while (!(readb(reg) & bit)) {
		udelay(10);
		if (!--timeout)
			break;
	}
	return timeout;
}


static struct fb_videomode panel_modedb[] = {
	{
		/* 320x240 @ 109 Hz, 33.3 kHz hsync */
		NULL, 109, 320, 240, KHZ2PICOS(MAX_PIXCLOCK/3),
		16, 16, 32, 24, 48, 8,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 640x480 @ 84 Hz, 48.1 kHz hsync */
		NULL, 84, 640, 480, KHZ2PICOS(MAX_PIXCLOCK/1),
		96, 32, 32, 48, 64, 8,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 800x600 @ 76 Hz, 46.3 kHz hsync */
		NULL, 76, 800, 600, KHZ2PICOS(MAX_PIXCLOCK/1),
		32, 10, 1, 1, 22, 1,
		0, FB_VMODE_NONINTERLACED
	}
};
static struct fb_videomode crt_modedb[] = {
	{
		/* 320x240 @ 84 Hz, 31.25 kHz hsync */
		NULL, 84, 320, 240, KHZ2PICOS(MAX_PIXCLOCK/2),
		128, 128, 60, 60, 64, 8,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 320x240 @ 109 Hz, 33.3 kHz hsync */
		NULL, 109, 320, 240, KHZ2PICOS(MAX_PIXCLOCK/3),
		16, 16, 32, 24, 48, 8,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 512x384 @ 77 Hz, 31.25 kHz hsync */
		NULL, 77, 512, 384, KHZ2PICOS(MAX_PIXCLOCK/2),
		48, 16, 16, 1, 64, 3,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 640x400 @ 88 Hz, 43.1 kHz hsync */
		NULL, 88, 640, 400, KHZ2PICOS(MAX_PIXCLOCK/1),
		128, 96, 32, 48, 64, 8,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 640x480 @ 84 Hz, 48.1 kHz hsync */
		NULL, 84, 640, 480, KHZ2PICOS(MAX_PIXCLOCK/1),
		96, 32, 32, 48, 64, 8,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 768x576 @ 62 Hz, 38.5 kHz hsync */
		NULL, 62, 768, 576, KHZ2PICOS(MAX_PIXCLOCK/1),
		144, 16, 28, 6, 112, 4,
		0, FB_VMODE_NONINTERLACED
	}, {
		/* 800x600 @ 60 Hz, 37.9 kHz hsync */
		NULL, 60, 800, 600, KHZ2PICOS(MAX_PIXCLOCK/1),
		88, 40, 23, 1, 128, 4,
		FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED
	}
};

static struct fb_videomode ntsc_modedb[] = {
	{
		/* 640x480 @ 62 Hz, requires flicker filter */
		//NULL, 62, 640, 480, 34921, 213, 57, 20, 2, 0, 0,
		NULL, 62, 640, 480, KHZ2PICOS(2*NTSC_PIXCLOCK),
		200, 70, 15, 7, 0, 0,
		FB_SYNC_BROADCAST, FB_VMODE_NONINTERLACED
	}
};
static struct fb_videomode pal_modedb[] = {
	{
		/* 640x480 @ 56 Hz, requires flicker filter */
		NULL, 56, 640, 480, KHZ2PICOS(2*PAL_PIXCLOCK),
		350, 145, 49, 23, 0, 0,
		FB_SYNC_BROADCAST, FB_VMODE_NONINTERLACED
	}
};


static inline void
fb_videomode_to_var(struct fb_videomode* mode,
		    struct fb_var_screeninfo*var)
{
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode;
}


static int
e1356fb_get_mode(const struct fb_info_e1356 *info,
		 int xres,
		 int yres,
		 struct fb_videomode ** modedb,
		 struct fb_videomode ** mode)
{
	struct fb_videomode * ret;
	int i, dbsize;

	if (IS_PANEL(info->fix.disp_type)) {
		ret = panel_modedb;
		dbsize = sizeof(panel_modedb)/sizeof(struct fb_videomode);
	} else if (info->fix.disp_type == DISP_TYPE_CRT) {
		ret = crt_modedb;
		dbsize = sizeof(crt_modedb)/sizeof(struct fb_videomode);
	} else if (info->fix.disp_type == DISP_TYPE_NTSC) {
		ret = ntsc_modedb;
		dbsize = sizeof(ntsc_modedb)/sizeof(struct fb_videomode);
	} else {
		ret = pal_modedb;
		dbsize = sizeof(pal_modedb)/sizeof(struct fb_videomode);
	}
	
	if (modedb)
		*modedb = ret;
	for (i=0; i<dbsize; i++) {
		if (xres == ret[i].xres && yres == ret[i].yres) {
			*mode = &ret[i];
			break;
		}
	}
	if (i == dbsize)
		return -EINVAL;
	return dbsize;
}



#ifdef E1356FB_VERBOSE_DEBUG
static void
dump_par(const struct e1356fb_par* par)
{
	DPRINTK("width:       %d\n", par->width);
	DPRINTK("height:      %d\n", par->height);
	DPRINTK("width_virt:  %d\n", par->width_virt);
	DPRINTK("height_virt: %d\n", par->height_virt);
	DPRINTK("bpp:         %d\n", par->bpp);
	DPRINTK("pixclock:    %d\n", par->ipclk.pixclk);
	DPRINTK("horiz_ndp:   %d\n", par->horiz_ndp);
	DPRINTK("vert_ndp:    %d\n", par->vert_ndp);
	DPRINTK("hsync_pol:   %d\n", par->hsync_pol);
	DPRINTK("hsync_start: %d\n", par->hsync_start);
	DPRINTK("hsync_width: %d\n", par->hsync_width);
	DPRINTK("vsync_pol:   %d\n", par->vsync_pol);
	DPRINTK("vsync_start: %d\n", par->vsync_start);
	DPRINTK("vsync_width: %d\n", par->vsync_width);
	DPRINTK("cmap_len:    %d\n", par->cmap_len);
}

static void
dump_display_regs(reg_dispcfg_t* dispcfg, reg_dispmode_t* dispmode)
{
	DPRINTK("hdw:            0x%02x\n", readb(&dispcfg->hdw));
	DPRINTK("hndp:           0x%02x\n", readb(&dispcfg->hndp));
	DPRINTK("hsync_start:    0x%02x\n", readb(&dispcfg->hsync_start));
	DPRINTK("hsync_pulse:    0x%02x\n", readb(&dispcfg->hsync_pulse));
	DPRINTK("vdh0:           0x%02x\n", readb(&dispcfg->vdh0));
	DPRINTK("vdh1:           0x%02x\n", readb(&dispcfg->vdh1));
	DPRINTK("vndp:           0x%02x\n", readb(&dispcfg->vndp));
	DPRINTK("vsync_start:    0x%02x\n", readb(&dispcfg->vsync_start));
	DPRINTK("vsync_pulse:    0x%02x\n", readb(&dispcfg->vsync_pulse));
	DPRINTK("tv_output_ctrl: 0x%02x\n\n", readb(&dispcfg->tv_output_ctrl));

	DPRINTK("disp_mode:        0x%02x\n", readb(&dispmode->disp_mode));
	DPRINTK("lcd_misc:         0x%02x\n", readb(&dispmode->lcd_misc));
	DPRINTK("start_addr0:      0x%02x\n", readb(&dispmode->start_addr0));
	DPRINTK("start_addr1:      0x%02x\n", readb(&dispmode->start_addr1));
	DPRINTK("start_addr2:      0x%02x\n", readb(&dispmode->start_addr2));
	DPRINTK("mem_addr_offset0: 0x%02x\n", readb(&dispmode->mem_addr_offset0));
	DPRINTK("mem_addr_offset1: 0x%02x\n", readb(&dispmode->mem_addr_offset1));
	DPRINTK("pixel_panning:    0x%02x\n", readb(&dispmode->pixel_panning));
	DPRINTK("fifo_high_thresh: 0x%02x\n", readb(&dispmode->fifo_high_thresh));
	DPRINTK("fifo_low_thresh:  0x%02x\n", readb(&dispmode->fifo_low_thresh));
}

static void
dump_fb(u8* base, int len)
{
	int i;
	DPRINTK("FB memory dump, start 0x%p, len %d", base, len);
	for (i=0; i<len; i++) {
		if (!(i%16))
			printk("\n%p: %02x ", &base[i], readb(&base[i]));
		else
			printk("%02x ", readb(&base[i]));
	}
	printk("\n");
}

#endif // E1356FB_VERBOSE_DEBUG



// Input:  ipclk->clksrc, ipclk->pixclk_d
// Output: ipclk->pixclk, ipclk->error, and ipclk->divisor
static int
get_nearest_pixclk_div(pixclock_info_t* ipclk, int x2)
{
	int pixclk_d = ipclk->pixclk_d;
	int clksrc = ipclk->clksrc;

	if (x2) clksrc *= 2;

	if (clksrc < (3*pixclk_d+1)/2)
		ipclk->divisor = 1;
	else if (clksrc < (5*pixclk_d+1)/2)
		ipclk->divisor = 2;
	else if (clksrc < (7*pixclk_d+1)/2)
		ipclk->divisor = 3;
	else if (clksrc < (9*pixclk_d+1)/2)
		ipclk->divisor = 4;
	else
		return -ENXIO;

	ipclk->pixclk = clksrc / ipclk->divisor;
	ipclk->error = (100*(pixclk_d - ipclk->pixclk)) / pixclk_d;
	return 0;
}

static int
e1356_calc_pixclock(const struct fb_info_e1356 *info,
		    pixclock_info_t* ipclk)
{
	int src_sel=-1, flicker_mult=0;
	pixclock_info_t test, ret;
    
	if (ipclk->pixclk > info->max_pixclock)
		return -ENXIO;

	test.pixclk_d = ipclk->pixclk_d;
	ret.error = 100;
	
	if (IS_TV(info->fix.disp_type) &&
	    (info->fix.tv_filt & TV_FILT_FLICKER))
		flicker_mult = 0x80;
	
	test.clksrc = info->fix.busclk;
	if (get_nearest_pixclk_div(&test, flicker_mult != 0) == 0 &&
	    abs(test.error) < abs(ret.error)) {
		ret = test;
		src_sel = 0x01;
	}

	test.clksrc = info->fix.mclk;
	if (get_nearest_pixclk_div(&test, flicker_mult != 0) == 0 &&
	    abs(test.error) < abs(ret.error)) {
		ret = test;
		src_sel = 0x03;
	}

	test.clksrc = info->fix.clki;
	if (get_nearest_pixclk_div(&test, flicker_mult != 0) == 0 &&
	    abs(test.error) < abs(ret.error)) {
		ret = test;
		src_sel = 0x00;
	}

	test.clksrc = info->fix.clki2;
	if (get_nearest_pixclk_div(&test, flicker_mult != 0) == 0 &&
	    abs(test.error) < abs(ret.error)) {
		ret = test;
		src_sel = 0x02;
	}

	if (ret.error > MAX_PCLK_ERROR_LOWER ||
	    ret.error < MAX_PCLK_ERROR_HIGHER)
		return -ENXIO;
    
	ret.pixclk_bits = flicker_mult | ((ret.divisor-1)<<4) | src_sel;
	*ipclk = ret;
	return 0;
}

static inline int
e1356_engine_wait_complete(reg_bitblt_t* bltreg)
{
	return e1356_wait_bitclr(&bltreg->ctrl0, 0x80, 5000);
}
static inline int
e1356_engine_wait_busy(reg_bitblt_t* bltreg)
{
	return e1356_wait_bitset(&bltreg->ctrl0, 0x80, 5000);
}

static void
e1356fb_engine_init(const struct e1356fb_par* par,
		    struct fb_info_e1356* info)
{
	reg_bitblt_t* bltreg = info->reg.bitblt;
    
	e1356_engine_wait_complete(bltreg);

	writeb(0, &bltreg->ctrl0);
	writeb(0, &bltreg->ctrl1);
	writeb(0, &bltreg->rop_code);
	writeb(0, &bltreg->operation);
	writeb(0, &bltreg->src_start_addr0);
	writeb(0, &bltreg->src_start_addr1);
	writeb(0, &bltreg->src_start_addr2);
	writeb(0, &bltreg->dest_start_addr0);
	writeb(0, &bltreg->dest_start_addr1);
	writeb(0, &bltreg->dest_start_addr2);
	writew(0, &bltreg->mem_addr_offset0);
	writew(0, &bltreg->width0);
	writew(0, &bltreg->height0);
	writew(0, &bltreg->bg_color0);
	writew(0, &bltreg->fg_color0);
}


static void doBlt_Write(const struct e1356fb_par* par,
			struct fb_info_e1356* info,
			blt_info_t* blt)
{
	reg_bitblt_t* bltreg = info->reg.bitblt;
	int nWords, nTotalWords;
	u32 srcphase, dstAddr;
	u16* w16;
	u32 stride = par->width_virt * par->Bpp;

	dstAddr = blt->dst_x * par->Bpp + blt->dst_y * stride;
	srcphase = (u32)blt->src & 1;
    
	if (blt->attribute & BLT_ATTR_TRANSPARENT)
		writew(blt->bg_color, &bltreg->bg_color0);
	else
		writeb(blt->rop, &bltreg->rop_code);
    
	writeb(blt->operation, &bltreg->operation);
	writeb((u8)srcphase, &bltreg->src_start_addr0);
	writew(stride/2, &bltreg->mem_addr_offset0);

	writeb(dstAddr, &bltreg->dest_start_addr0);
	writeb(dstAddr>>8, &bltreg->dest_start_addr1);
	writeb(dstAddr>>16, &bltreg->dest_start_addr2);

	writew(blt->dst_width-1, &bltreg->width0);
	writew(blt->dst_height-1, &bltreg->height0);

	// program color format operation
	writeb(par->bpp == 8 ? 0x00 : 0x01, &bltreg->ctrl1);

	// start it up
	writeb(0x80, &bltreg->ctrl0);

	// wait for it to actually start
	e1356_engine_wait_busy(bltreg);

	// calculate the number of 16 bit words per one blt line

	nWords = srcphase + ((blt->dst_width - srcphase)*par->Bpp + 1) / 2;
	nTotalWords = nWords*blt->dst_height;
	w16 = (u16*)((u32)blt->src & 0xfffffffe);   // Word aligned

	while (nTotalWords > 0) {
		int j, nFIFO;
		u8 ctrl0;

		// read the FIFO status
		ctrl0 = readb(&bltreg->ctrl0);

		if ((ctrl0 & 0x30) == 0x20)
			// FIFO is at least half full, but not full
			nFIFO = 1;
		else if ((ctrl0 & 0x40) == 0)
			// FIFO is empty
			nFIFO = 16;
		else
			// FIFO is full
			continue;

		for (j = 0; j < nFIFO && nTotalWords > 0; j++,nTotalWords--)
			writew(*w16++, info->reg.bitblt_data);
	}

	e1356_engine_wait_complete(bltreg);
}


static void
doBlt_SolidFill(const struct e1356fb_par* par,
		struct fb_info_e1356* info,
		blt_info_t* blt)
{
	reg_bitblt_t* bltreg = info->reg.bitblt;
	u32 width = blt->dst_width, height = blt->dst_height;
	u32 stride = par->width_virt * par->Bpp;
	u32 dest_addr = (blt->dst_y * stride) + (blt->dst_x * par->Bpp);

	if (width == 0 || height == 0)
		return;

	// program dest address
	writeb(dest_addr & 0x00ff, &bltreg->dest_start_addr0);
	writeb((dest_addr>>8) & 0x00ff, &bltreg->dest_start_addr1);
	writeb((dest_addr>>16) & 0x00ff, &bltreg->dest_start_addr2);

	// program width and height of solid-fill blit
	writew(width-1, &bltreg->width0);
	writew(height-1, &bltreg->height0);

	// program color of fill
	writew(blt->fg_color, &bltreg->fg_color0);
	// select solid-fill BLIT
	writeb(BLT_SOLID_FILL, &bltreg->operation);
	// program color format operation
	writeb(par->bpp == 8 ? 0x00 : 0x01, &bltreg->ctrl1);
	// program BLIT memory offset
	writew(stride/2, &bltreg->mem_addr_offset0);

	// start it up (self completes)
	writeb(0x80, &bltreg->ctrl0);

	e1356_engine_wait_complete(bltreg);
}


static void
doBlt_Move(const struct e1356fb_par* par,
	   struct fb_info_e1356* info,
	   blt_info_t* blt)
{
	reg_bitblt_t* bltreg = info->reg.bitblt;
	int neg_dir=0;
	u32 dest_addr, src_addr;
	u32 bpp = par->bpp;
	u32 stride = par->width_virt * par->Bpp; // virt line length in bytes
	u32 srcx = blt->src_x, srcy = blt->src_y;
	u32 dstx = blt->dst_x, dsty = blt->dst_y;
	u32 width = blt->dst_width, height = blt->dst_height;
    
	if (width == 0 || height == 0)
		return;
   
	src_addr = srcx*par->Bpp + srcy*stride;
	dest_addr = dstx*par->Bpp + dsty*stride;

	/*
	 * See if regions overlap and dest region is beyond source region.
	 * If so, we need to do a move BLT in negative direction. Only applies
	 * if the BLT is not transparent.
	 */
	if (!(blt->attribute & BLT_ATTR_TRANSPARENT)) {
		if ((srcx + width  > dstx) && (srcx < dstx + width) &&
		    (srcy + height > dsty) && (srcy < dsty + height) &&
		    (dest_addr > src_addr)) {
			neg_dir = 1;
			// negative direction : get the coords of lower right corner
			src_addr += stride * (height-1) + par->Bpp * (width-1);
			dest_addr += stride * (height-1) + par->Bpp * (width-1);
		}
	}
    
	// program BLIT memory offset
	writew(stride/2, &bltreg->mem_addr_offset0);

	// program src and dest addresses
	writeb(src_addr & 0x00ff, &bltreg->src_start_addr0);
	writeb((src_addr>>8) & 0x00ff, &bltreg->src_start_addr1);
	writeb((src_addr>>16) & 0x00ff, &bltreg->src_start_addr2);
	writeb(dest_addr & 0x00ff, &bltreg->dest_start_addr0);
	writeb((dest_addr>>8) & 0x00ff, &bltreg->dest_start_addr1);
	writeb((dest_addr>>16) & 0x00ff, &bltreg->dest_start_addr2);

	// program width and height of blit
	writew(width-1, &bltreg->width0);
	writew(height-1, &bltreg->height0);

	// program color format operation
	writeb(bpp == 8 ? 0x00 : 0x01, &bltreg->ctrl1);

	// set the blt type
	if (blt->attribute & BLT_ATTR_TRANSPARENT) {
		writew(blt->bg_color, &bltreg->bg_color0);
		writeb(BLT_MOVE_POS_TRANSP, &bltreg->operation); 
	} else {
		writeb(blt->rop, &bltreg->rop_code);
		// select pos/neg move BLIT
		writeb(neg_dir ? BLT_MOVE_NEG_ROP : BLT_MOVE_POS_ROP,
		       &bltreg->operation); 
	}

	// start it up (self completes)
	writeb(0x80, &bltreg->ctrl0);

	e1356_engine_wait_complete(bltreg);
}


static void doBlt_ColorExpand(const struct e1356fb_par* par,
			      struct fb_info_e1356* info,
			      blt_info_t* blt)
{
	reg_bitblt_t* bltreg = info->reg.bitblt;
	int i, j, nWords, Sx, Sy;
	u32 dstAddr;
	u16* wpt, *wpt1;
	u32 stride = par->width_virt * par->Bpp;

	if (blt->dst_width == 0 || blt->dst_height == 0)
		return;

	Sx = blt->src_x;
	Sy = blt->src_y;

	writeb((7 - Sx%8), &bltreg->rop_code);

	writeb(blt->operation, &bltreg->operation);

	writeb((u8)(Sx & 1), &bltreg->src_start_addr0);

	dstAddr = blt->dst_x*par->Bpp + blt->dst_y * stride;
	writeb(dstAddr, &bltreg->dest_start_addr0);
	writeb(dstAddr>>8, &bltreg->dest_start_addr1);
	writeb(dstAddr>>16, &bltreg->dest_start_addr2);

	// program color format operation
	writeb(par->bpp == 8 ? 0x00 : 0x01, &bltreg->ctrl1);
	writew(stride/2, &bltreg->mem_addr_offset0);
	writew(blt->dst_width-1, &bltreg->width0);
	writew(blt->dst_height-1, &bltreg->height0);
	writew(blt->bg_color, &bltreg->bg_color0);
	writew(blt->fg_color, &bltreg->fg_color0);

	// start it up
	writeb(0x80, &bltreg->ctrl0);

	// wait for it to actually start
	e1356_engine_wait_busy(bltreg);

	// calculate the number of 16 bit words per one blt line

	nWords = (Sx%16 + blt->dst_width + 15)/16;

	wpt = blt->src + (Sy*blt->srcstride + Sx/16)/2;

	for (i = 0; i < blt->dst_height; i++) {
		wpt1 = wpt;

		for (j = 0; j < nWords; j++) {
			// loop until FIFO becomes empty...
			e1356_wait_bitclr(&bltreg->ctrl0, 0x40, 10000);
			writew(*wpt1++, info->reg.bitblt_data);
		}
	
		wpt += blt->srcstride/2;
	}

	e1356_engine_wait_complete(bltreg);
}


/*
 * The BitBLT operation dispatcher
 */
static int
doBlt(const struct e1356fb_par* par,
      struct fb_info_e1356* info,
      blt_info_t* blt)
{
	/*
	 * Make sure we're not reentering in the middle of an
	 * active BitBLT operation. ALWAYS call this dispatcher
	 * and not one of the above BLT routines directly, or you
	 * run the risk of overlapping BLT operations, which can
	 * cause complete system hangs.
     */
	if (readb(&info->reg.bitblt->ctrl0) & 0x80)
		return -ENXIO;
    
	switch (blt->operation) {
	case BLT_MOVE_POS_ROP:
	case BLT_MOVE_NEG_ROP:
	case BLT_MOVE_POS_TRANSP:
		doBlt_Move(par, info, blt);
		break;
	case BLT_COLOR_EXP:
	case BLT_COLOR_EXP_TRANSP:
		doBlt_ColorExpand(par, info, blt);
		break;
	case BLT_SOLID_FILL:
		doBlt_SolidFill(par, info, blt);
		break;
	case BLT_WRITE_ROP:
	case BLT_WRITE_TRANSP:
		doBlt_Write(par, info, blt);
		break;
	case BLT_READ:
	case BLT_PAT_FILL_ROP:
	case BLT_PAT_FILL_TRANSP:
	case BLT_MOVE_COLOR_EXP:
	case BLT_MOVE_COLOR_EXP_TRANSP:
		DPRINTK("BitBLT operation 0x%02x not implemented yet\n",
			blt->operation);
		return -ENXIO;
	default:
		DPRINTK("Unknown BitBLT operation 0x%02x\n", blt->operation);
		return -ENXIO;
	}
    
	return 0;
}


// Initializes blt->src and blt->srcstride
static void fill_putcs_buffer(struct display *p,
			      blt_info_t* blt,
			      const unsigned short* str,
			      int count)
{   
	int row, i, j;
	u8* b1, *b2;
	u32 fw = fontwidth(p);
	u32 fwb = (fw + 7) >> 3;
	u32 fh = fontheight(p);
	int bytesPerChar = fwb * fh;

	if (count*bytesPerChar > PAGE_SIZE) {
		// Truncate the string if it overflows putcs_buffer, which is
		// one page in size.
		count = PAGE_SIZE/bytesPerChar - 1;
	}

	blt->srcstride = (fwb*count + 1) & ~1; //round up to be even
	
	b1 = (u8*)blt->src;

	for (row = 0; row < fh; row++) {
		b2 = b1;
		for (i = 0; i < count; i++) {
			for (j=0; j<fwb; j++)
				*b2++ = p->fontdata[(str[i] & p->charmask) *
						   bytesPerChar +
						   row*fwb + j];
		}
		b1 += blt->srcstride;
	}
}


/*
 * Set the color of a palette entry in 8bpp mode 
 */
static inline void
do_setpalentry(reg_lut_t* lut, unsigned regno,
	       u8 r, u8 g, u8 b)
{
	writeb(0x00, &lut->mode);
	writeb((u8)regno, &lut->addr);
	writeb(r&0xf0, &lut->data);
	writeb(g&0xf0, &lut->data);
	writeb(b&0xf0, &lut->data);
}

   
static void
do_pan_var(struct fb_var_screeninfo* var, struct fb_info_e1356* info)
{
	u32 pixel_start, start_addr;
	u8 pixel_pan;
	struct e1356fb_par* par = &info->current_par;
	reg_misc_t* misc = info->reg.misc;
	reg_dispmode_t* dispmode = (IS_PANEL(info->fix.disp_type)) ?
		info->reg.lcd_mode : info->reg.crttv_mode;
	
	pixel_start = var->yoffset * par->width_virt + var->xoffset;
	start_addr = (pixel_start * par->Bpp) / 2;
	pixel_pan = (par->bpp == 8) ? (u8)(pixel_start & 1) : 0;
    
	if (readb(&misc->disp_mode) != 0) {
		reg_dispcfg_t* dispcfg = (IS_PANEL(info->fix.disp_type)) ?
			info->reg.lcd_cfg : info->reg.crttv_cfg;

		// wait for the end of the current VNDP
		e1356_wait_bitclr(&dispcfg->vndp, 0x80, 5000);
		// now wait for the start of a new VNDP
		e1356_wait_bitset(&dispcfg->vndp, 0x80, 5000);
	}
    
	writeb((u8)(start_addr & 0xff), &dispmode->start_addr0);
	writeb((u8)((start_addr>>8) & 0xff), &dispmode->start_addr1);
	writeb((u8)((start_addr>>16) & 0xff), &dispmode->start_addr2);
	writeb(pixel_pan, &dispmode->pixel_panning);
}


/*
 * Invert the hardware cursor image (timerfunc)  
 */
static void
do_flashcursor(unsigned long ptr)
{
	u8 curs_ctrl;
	struct fb_info_e1356* info = (struct fb_info_e1356 *)ptr;
	reg_inkcurs_t* inkcurs = (IS_PANEL(info->fix.disp_type)) ?
		info->reg.lcd_inkcurs : info->reg.crttv_inkcurs;

	spin_lock(&info->cursor.lock);
	// toggle cursor enable bit
	curs_ctrl = readb(&inkcurs->ctrl);
	writeb((curs_ctrl ^ 0x01) & 0x01, &inkcurs->ctrl);
	info->cursor.timer.expires = jiffies+HZ/2;
	add_timer(&info->cursor.timer);
	spin_unlock(&info->cursor.lock);
}

#ifdef SHADOW_FRAME_BUFFER
/*
 * Write BLT the shadow frame buffer to the real fb (timerfunc)  
 */
static void
do_write_shadow_fb(unsigned long ptr)
{
	blt_info_t blt;
	struct fb_info_e1356 *info = (struct fb_info_e1356*)ptr;
	struct fb_info* fb = &info->fb_info;
	struct e1356fb_par* par = &info->current_par;
	u32 stride = par->width_virt * par->Bpp;

	unsigned long j_start = jiffies;
    
	blt.src_x = blt.src_y = 0;
	blt.attribute = 0;
	blt.dst_width = par->width;
	blt.dst_height = par->height;
	blt.dst_y = fb->var.yoffset;
	blt.dst_x = fb->var.xoffset;
	blt.operation = BLT_WRITE_ROP;
	blt.rop = 0x0c; // ROP: destination = source
	blt.src = (u16*)(info->shadow.fb + blt.dst_x * par->Bpp +
			 blt.dst_y * stride);

	doBlt(par, info, &blt);
    
	info->shadow.timer.expires = jiffies+HZ/2;
	add_timer(&info->shadow.timer);

	//DPRINTK("delta jiffies = %ld\n", jiffies - j_start);
}
#endif


/* ------------------------------------------------------------------------- 
 *              Hardware independent part, interface to the world
 * ------------------------------------------------------------------------- */

static void
e1356_cfbX_clear_margins(struct vc_data* conp, struct display* p,
			 int bottom_only)
{
	blt_info_t blt;
	unsigned int cw=fontwidth(p);
	unsigned int ch=fontheight(p);
	unsigned int rw=p->var.xres % cw;
	unsigned int bh=p->var.yres % ch;
	unsigned int rs=p->var.xres - rw;
	unsigned int bs=p->var.yres - bh;

	//DPRINTK("\n");

	if (!bottom_only && rw) { 
		blt.dst_x = p->var.xoffset+rs;
		blt.dst_y = p->var.yoffset;
		blt.dst_height = p->var.yres;
		blt.dst_width = rw;
		blt.attribute = 0;
		blt.fg_color = 0;
		blt.operation = BLT_SOLID_FILL;
		doBlt (&fb_info.current_par, &fb_info, &blt);
	}
    
	if (bh) { 
		blt.dst_x = p->var.xoffset;
		blt.dst_y = p->var.yoffset+bs;
		blt.dst_height = bh;
		blt.dst_width = rs;
		blt.attribute = 0;
		blt.fg_color = 0;
		blt.operation = BLT_SOLID_FILL;
		doBlt (&fb_info.current_par, &fb_info, &blt);
	}
}

static void
e1356_cfbX_bmove(struct display* p, 
		 int sy, 
		 int sx, 
		 int dy,
		 int dx, 
		 int height, 
		 int width)
{
	blt_info_t blt;
    
	//DPRINTK("(%d,%d) to (%d,%d) size (%d,%d)\n", sx,sy,dx,dy,width,height);

	blt.src_x = fontwidth_x8(p)*sx;
	blt.src_y = fontheight(p)*sy;
	blt.dst_x = fontwidth_x8(p)*dx;
	blt.dst_y = fontheight(p)*dy;
	blt.src_height = blt.dst_height = fontheight(p)*height;
	blt.src_width = blt.dst_width = fontwidth_x8(p)*width;
	blt.attribute = 0;
	blt.rop = 0x0c;
	/*
	 * The move BLT routine will actually decide between a pos/neg
	 * move BLT. This is just so that the BLT dispatcher knows to
	 * call the move BLT routine.
	 */
	blt.operation = BLT_MOVE_POS_ROP;

	doBlt (&fb_info.current_par, &fb_info, &blt);
}

static void
e1356_cfb8_putc(struct vc_data* conp,
		struct display* p,
		int c, int yy,int xx)
{   
	blt_info_t blt;
	u32 fgx,bgx;
	u32 fw = fontwidth_x8(p);
	u32 fh = fontheight(p);
	u16 cs = (u16)c;

	fgx = attr_fgcol(p, c);
	bgx = attr_bgcol(p, c);

	blt.src_x = blt.src_y = 0;
	blt.attribute = 0;
	blt.dst_width = fw;
	blt.dst_height = fh;
	blt.dst_y = yy * fh;
	blt.dst_x = xx * fw;
	blt.bg_color = bgx;
	blt.fg_color = fgx;
	blt.operation = BLT_COLOR_EXP;
	blt.src = fb_info.putcs_buffer;
	fill_putcs_buffer(p, &blt, &cs, 1);

	doBlt(&fb_info.current_par, &fb_info, &blt);

}

static void
e1356_cfb16_putc(struct vc_data* conp,
		 struct display* p,
		 int c, int yy,int xx)
{   
	blt_info_t blt;
	u32 fgx,bgx;
	u32 fw = fontwidth_x8(p);
	u32 fh = fontheight(p);
	u16 cs = (u16)c;
    
	fgx = ((u16*)p->dispsw_data)[attr_fgcol(p,c)];
	bgx = ((u16*)p->dispsw_data)[attr_bgcol(p,c)];

	blt.src_x = blt.src_y = 0;
	blt.attribute = 0;
	blt.dst_width = fw;
	blt.dst_height = fh;
	blt.dst_y = yy * fh;
	blt.dst_x = xx * fw;
	blt.bg_color = bgx;
	blt.fg_color = fgx;
	blt.operation = BLT_COLOR_EXP;
	blt.src = fb_info.putcs_buffer;
	fill_putcs_buffer(p, &blt, &cs, 1);

	doBlt(&fb_info.current_par, &fb_info, &blt);
}


static void
e1356_cfb8_putcs(struct vc_data* conp,
		 struct display* p,
		 const unsigned short *s,int count,int yy,int xx)
{
	blt_info_t blt;
	u32 fgx,bgx;
	u32 fw = fontwidth_x8(p);
	u32 fh = fontheight(p);

	//DPRINTK("\n");

	fgx=attr_fgcol(p, *s);
	bgx=attr_bgcol(p, *s);

	blt.src_x = blt.src_y = 0;
	blt.attribute = 0;
	blt.dst_width = count * fw;
	blt.dst_height = fh;
	blt.dst_y = yy * fh;
	blt.dst_x = xx * fw;
	blt.bg_color = bgx;
	blt.fg_color = fgx;
	blt.operation = BLT_COLOR_EXP;
	blt.src = fb_info.putcs_buffer;
	fill_putcs_buffer(p, &blt, s, count);

	doBlt(&fb_info.current_par, &fb_info, &blt);
}

static void
e1356_cfb16_putcs(struct vc_data* conp,
		  struct display* p,
		  const unsigned short *s,int count,int yy,int xx)
{
	blt_info_t blt;
	u32 fgx,bgx;
	u32 fw = fontwidth_x8(p);
	u32 fh = fontheight(p);

	//DPRINTK("\n");

	fgx=((u16*)p->dispsw_data)[attr_fgcol(p,*s)];
	bgx=((u16*)p->dispsw_data)[attr_bgcol(p,*s)];

	blt.src_x = blt.src_y = 0;
	blt.attribute = 0;
	blt.dst_width = count * fw;
	blt.dst_height = fh;
	blt.dst_y = yy * fh;
	blt.dst_x = xx * fw;
	blt.bg_color = bgx;
	blt.fg_color = fgx;
	blt.operation = BLT_COLOR_EXP;
	blt.src = fb_info.putcs_buffer;
	fill_putcs_buffer(p, &blt, s, count);

	doBlt(&fb_info.current_par, &fb_info, &blt);
}


static void
e1356_cfb8_clear(struct vc_data* conp, 
		 struct display* p, 
		 int sy,
		 int sx, 
		 int height, 
		 int width)
{
	blt_info_t blt;
	u32 bg = attr_bgcol_ec(p,conp);

	//DPRINTK("(%d,%d) size (%d,%d)\n", sx,sy,width,height);

	blt.dst_x = fontwidth_x8(p)*sx;
	blt.dst_y = fontheight(p)*sy;
	blt.dst_height = fontheight(p)*height;
	blt.dst_width = fontwidth_x8(p)*width;
	blt.attribute = 0;
	blt.fg_color = bg;
	blt.operation = BLT_SOLID_FILL;

	doBlt (&fb_info.current_par, &fb_info, &blt);
}

static void
e1356_cfb16_clear(struct vc_data* conp, 
		  struct display* p, 
		  int sy,
		  int sx, 
		  int height, 
		  int width)
{
	blt_info_t blt;
	u32 bg = ((u16*)p->dispsw_data)[attr_bgcol_ec(p,conp)];

	//DPRINTK("(%d,%d) size (%d,%d)\n", sx,sy,width,height);

	blt.dst_x = fontwidth_x8(p)*sx;
	blt.dst_y = fontheight(p)*sy;
	blt.dst_height = fontheight(p)*height;
	blt.dst_width = fontwidth_x8(p)*width;
	blt.attribute = 0;
	blt.fg_color = bg;
	blt.operation = BLT_SOLID_FILL;

	doBlt (&fb_info.current_par, &fb_info, &blt);
}


static void
e1356_cfbX_revc(struct display *p, int xx, int yy)
{
	// not used if h/w cursor
	//DPRINTK("\n");
}

static void
e1356_cfbX_cursor(struct display *p, int mode, int x, int y) 
{
	unsigned long flags;
	struct fb_info_e1356 *info=(struct fb_info_e1356 *)p->fb_info;
	reg_inkcurs_t* inkcurs = (IS_PANEL(info->fix.disp_type)) ?
		info->reg.lcd_inkcurs : info->reg.crttv_inkcurs;
    
	//DPRINTK("\n");

	if (mode == CM_ERASE) {
		if (info->cursor.state != CM_ERASE) {
			spin_lock_irqsave(&info->cursor.lock,flags);
			info->cursor.state = CM_ERASE;
			del_timer(&(info->cursor.timer));
			writeb(0x00, &inkcurs->ctrl);
			spin_unlock_irqrestore(&info->cursor.lock,flags);
		}
		return;
	}
    
	if ((p->conp->vc_cursor_type & CUR_HWMASK) != info->cursor.type)
		e1356fb_createcursor(p);
    
	x *= fontwidth_x8(p);
	y *= fontheight(p);
	x -= p->var.xoffset;
	y -= p->var.yoffset;
    
	spin_lock_irqsave(&info->cursor.lock,flags);
	if ((x != info->cursor.x) || (y != info->cursor.y) ||
	    (info->cursor.redraw)) {
		info->cursor.x = x;
		info->cursor.y = y;
		info->cursor.redraw = 0;
		writeb(0x01, &inkcurs->ctrl);
		writew(x, &inkcurs->x_pos0);
		writew(y, &inkcurs->y_pos0);
		/* fix cursor color - XFree86 forgets to restore it properly */
		writeb(0x00, &inkcurs->blue0);
		writeb(0x00, &inkcurs->green0);
		writeb(0x00, &inkcurs->red0);
		writeb(0x1f, &inkcurs->blue1);
		writeb(0x3f, &inkcurs->green1);
		writeb(0x1f, &inkcurs->red1);
	}

	info->cursor.state = CM_DRAW;
	mod_timer(&info->cursor.timer, jiffies+HZ/2);
	spin_unlock_irqrestore(&info->cursor.lock,flags);
}

#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_e1356_8 = {
	setup:		fbcon_cfb8_setup, 
	bmove:		e1356_cfbX_bmove, 
	clear:		e1356_cfb8_clear, 
	putc:		e1356_cfb8_putc,
	putcs:		e1356_cfb8_putcs, 
	revc:		e1356_cfbX_revc,   
	cursor:		e1356_cfbX_cursor, 
	clear_margins:	e1356_cfbX_clear_margins,
	fontwidthmask:	FONTWIDTHRANGE(6,16)
};
#endif

#ifdef FBCON_HAS_CFB16
static struct display_switch fbcon_e1356_16 = {
	setup:		fbcon_cfb16_setup, 
	bmove:		e1356_cfbX_bmove, 
	clear:		e1356_cfb16_clear, 
	putc:		e1356_cfb16_putc,
	putcs:		e1356_cfb16_putcs, 
	revc:		e1356_cfbX_revc, 
	cursor:		e1356_cfbX_cursor, 
	clear_margins:	e1356_cfbX_clear_margins,
	fontwidthmask:	FONTWIDTHRANGE(6,16)
};
#endif

/* ------------------------------------------------------------------------- */

static void
e1356fb_set_par(const struct e1356fb_par* par,
		struct fb_info_e1356* info)
{
	reg_dispcfg_t* dispcfg=NULL;
	reg_dispmode_t* dispmode=NULL;
	u8* pclk_cfg=NULL;
	u8 width, hndp=0, hsync_start=0, hsync_width=0;
	u8 vndp, vsync_start, vsync_width=0, display_mode;
	u8 main_display_mode=0;
	u16 height, addr_offset;
	int disp_type = info->fix.disp_type;

	DPRINTK("%dx%d-%dbpp @ %d Hz, %d kHz hsync\n",
		par->width, par->height, par->bpp,
		par->vsync_freq, (((2*par->hsync_freq)/1000)+1)/2);
#ifdef E1356FB_VERBOSE_DEBUG
	dump_par(par);
#endif
    
	info->current_par = *par;

	width = (par->width >> 3) - 1;
	display_mode = (par->bpp == 8) ? 0x03 : 0x05;
	addr_offset = (par->width_virt * par->Bpp) / 2;
	vsync_start = (disp_type == DISP_TYPE_LCD) ? 0 : par->vsync_start - 1;
	height = par->height - 1;
	vndp = par->vert_ndp - 1;

	switch (disp_type) {
	case DISP_TYPE_LCD:
		dispcfg = info->reg.lcd_cfg;
		dispmode = info->reg.lcd_mode;
		pclk_cfg = &info->reg.clk_cfg->lcd_pclk_cfg;
		hndp = (par->horiz_ndp >> 3) - 1;
		hsync_start = 0;
		hsync_width = par->hsync_pol ? 0x00 : 0x80;
		vsync_width = par->vsync_pol ? 0x00 : 0x80;
		main_display_mode = 0x01;
		break;
	case DISP_TYPE_TFT:
		dispcfg = info->reg.lcd_cfg;
		dispmode = info->reg.lcd_mode;
		pclk_cfg = &info->reg.clk_cfg->lcd_pclk_cfg;
		hndp = (par->horiz_ndp >> 3) - 1;
		hsync_start = (par->bpp == 8) ?
			(par->hsync_start - 4) >> 3 :
				(par->hsync_start - 6) >> 3;
		hsync_width =
			(par->hsync_pol ? 0x80 : 0x00) |
			((par->hsync_width >> 3) - 1);
		vsync_width =
			(par->vsync_pol ? 0x80 : 0x00) |
			(par->vsync_width - 1);
		main_display_mode = 0x01;
		break;
	case DISP_TYPE_CRT:
		dispcfg = info->reg.crttv_cfg;
		dispmode = info->reg.crttv_mode;
		pclk_cfg = &info->reg.clk_cfg->crttv_pclk_cfg;
		hndp = (par->horiz_ndp >> 3) - 1;
		hsync_start = (par->bpp == 8) ?
			(par->hsync_start - 3) >> 3 :
				(par->hsync_start - 5) >> 3;
		hsync_width =
			(par->hsync_pol ? 0x80 : 0x00) |
			((par->hsync_width >> 3) - 1);
		vsync_width =
			(par->vsync_pol ? 0x80 : 0x00) |
			(par->vsync_width - 1);
		main_display_mode = 0x02;
		break;
	case DISP_TYPE_NTSC:
	case DISP_TYPE_PAL:
		dispcfg = info->reg.crttv_cfg;
		dispmode = info->reg.crttv_mode;
		pclk_cfg = &info->reg.clk_cfg->crttv_pclk_cfg;
		hndp = (disp_type == DISP_TYPE_PAL) ?
			(par->horiz_ndp - 7) >> 3 :
				(par->horiz_ndp - 6) >> 3;
		hsync_start = (par->bpp == 8) ?
			(par->hsync_start + 7) >> 3 :
				(par->hsync_start + 5) >> 3;
		hsync_width = 0;
		vsync_width = 0;
		main_display_mode = (info->fix.tv_filt & TV_FILT_FLICKER) ?
			0x06 : 0x04;
		break;
	}

	// Blast the regs!
	// note: reset panning/scrolling (set start-addr and
	// pixel pan regs to 0). Panning is handled by pan_display.

	e1356_engine_wait_complete(info->reg.bitblt);

	// disable display while initializing
	writeb(0, &info->reg.misc->disp_mode);

	writeb(par->ipclk.pixclk_bits, pclk_cfg);

	writeb(width, &dispcfg->hdw);
	writeb(hndp, &dispcfg->hndp);
	writeb(hsync_start, &dispcfg->hsync_start);
	writeb(hsync_width, &dispcfg->hsync_pulse);
	writew(height, &dispcfg->vdh0);
	writeb(vndp, &dispcfg->vndp);
	writeb(vsync_start, &dispcfg->vsync_start);
	writeb(vsync_width, &dispcfg->vsync_pulse);

	writeb(display_mode, &dispmode->disp_mode);
	if (info->fix.mmunalign && info->mmaped)
		writeb(1, &dispmode->start_addr0);
	else
		writeb(0, &dispmode->start_addr0);
	writeb(0, &dispmode->start_addr1);
	writeb(0, &dispmode->start_addr2);
	writew(addr_offset, &dispmode->mem_addr_offset0);
	writeb(0, &dispmode->pixel_panning);

	// reset BitBlt engine
	e1356fb_engine_init(par, info);

#ifdef E1356FB_VERBOSE_DEBUG
	dump_display_regs(dispcfg, dispmode);
#endif

	/* clear out framebuffer memory */
	fbfill(fb_info.membase_virt, 0, fb_info.fb_size);
	// finally, enable display!
	writeb(main_display_mode, &info->reg.misc->disp_mode); 
}


static int
e1356fb_verify_timing(struct e1356fb_par* par,
		      const struct fb_info_e1356* info)
{
	int disp_type = info->fix.disp_type;

	// timing boundary checks
	if (par->horiz_ndp > max_hndp[disp_type]) {
		DPRINTK("horiz_ndp too big: %d\n", par->horiz_ndp);
		return -EINVAL;
	}
	if (par->vert_ndp > max_vndp[disp_type]) {
		DPRINTK("vert_ndp too big: %d\n", par->vert_ndp);
		return -EINVAL;
	}

	if (disp_type != DISP_TYPE_LCD) {
		if (par->hsync_start >
		    max_hsync_start[(par->bpp==16)][disp_type]) {
			DPRINTK("hsync_start too big: %d\n",
				par->hsync_start);
			return -EINVAL;
		}
		if (par->vsync_start > max_vsync_start[disp_type]) {
			DPRINTK("vsync_start too big: %d\n",
				par->vsync_start);
			return -EINVAL;
		}
		if (!IS_TV(disp_type)) {
			if (par->hsync_width > max_hsync_width[disp_type]) {
				DPRINTK("hsync_width too big: %d\n",
					par->hsync_width);
				return -EINVAL;
			}
			if (par->vsync_width > max_vsync_width[disp_type]) {
				DPRINTK("vsync_width too big: %d\n",
					par->vsync_width);
				return -EINVAL;
			}
		}
	}

	if (IS_TV(disp_type)) {
		int tv_pixclk = (disp_type == DISP_TYPE_NTSC) ?
			NTSC_PIXCLOCK : PAL_PIXCLOCK;
		if (info->fix.tv_filt & TV_FILT_FLICKER)
			tv_pixclk *= 2;
		
		if (par->ipclk.pixclk_d != tv_pixclk) {
			DPRINTK("invalid TV pixel clock %u kHz\n",
				par->ipclk.pixclk_d);
			return -EINVAL;
		}
	}
	
	if (e1356_calc_pixclock(info, &par->ipclk) < 0) {
		DPRINTK("can't set pixel clock %u kHz\n",
			par->ipclk.pixclk_d);
		return -EINVAL;
	}
 
#ifdef E1356FB_VERBOSE_DEBUG
	DPRINTK("desired pixclock = %d kHz, actual = %d kHz, error = %d%%\n",
		par->ipclk.pixclk_d, par->ipclk.pixclk, par->ipclk.error);
#endif
    
	if (disp_type != DISP_TYPE_LCD) {
		if (par->horiz_ndp < par->hsync_start + par->hsync_width) {
			DPRINTK("invalid horiz. timing\n");
			return -EINVAL;
		}
		if (par->vert_ndp < par->vsync_start + par->vsync_width) {
			DPRINTK("invalid vert. timing\n");
			return -EINVAL;
		}

		// SED1356 Hardware Functional Spec, section 13.5
		if (disp_type == DISP_TYPE_NTSC &&
		    ((par->width + par->horiz_ndp != 910) ||
		     (par->height + 2*par->vert_ndp+1 != 525))) {
			DPRINTK("invalid NTSC timing\n");
			return -EINVAL;
		} else if (disp_type == DISP_TYPE_PAL &&
			   ((par->width + par->horiz_ndp != 1135) ||
			    (par->height + 2*par->vert_ndp+1 != 625))) {
			DPRINTK("invalid PAL timing\n");
			return -EINVAL;
		}
	}
    
	par->hsync_freq = (1000 * par->ipclk.pixclk) /
		(par->width + par->horiz_ndp);
	par->vsync_freq = par->hsync_freq / (par->height + par->vert_ndp);
	
	if (par->hsync_freq < 30000 || par->hsync_freq > 90000) {
		DPRINTK("hsync freq too %s: %u Hz\n",
			par->hsync_freq < 30000 ? "low" : "high",
			par->hsync_freq);
		return -EINVAL;
	}
	if (par->vsync_freq < 50 || par->vsync_freq > 110) {
		DPRINTK("vsync freq too %s: %u Hz\n",
			par->vsync_freq < 50 ? "low" : "high",
			par->vsync_freq);
		return -EINVAL;
	}

	return 0;
}

static int
e1356fb_verify_par(struct e1356fb_par* par,
		   const struct fb_info_e1356* info)
{
	int disp_type = info->fix.disp_type;
    
	if (par->bpp != 8 && par->bpp != 16) {
		DPRINTK("depth not supported: %u bpp\n", par->bpp);
		return -EINVAL;
	}

	if (par->width > par->width_virt) {
		DPRINTK("virtual x resolution < physical x resolution not possible\n");
		return -EINVAL;
	}

	if (par->height > par->height_virt) {
		DPRINTK("virtual y resolution < physical y resolution not possible\n");
		return -EINVAL;
	}

	if (par->width < 320 || par->width > 1024) {
		DPRINTK("width not supported: %u\n", par->width);
		return -EINVAL;
	}

	if ((disp_type == DISP_TYPE_LCD && (par->width % 16)) ||
	    (disp_type == DISP_TYPE_TFT && (par->width % 8))) {
		DPRINTK("invalid width for panel type: %u\n", par->width);
		return -EINVAL;
	}

	if (par->height < 200 || par->height > 1024) {
		DPRINTK("height not supported: %u\n", par->height);
		return -EINVAL;
	}

	if (par->width_virt * par->height_virt * par->Bpp >
	    info->fb_size) {
		DPRINTK("not enough memory for virtual screen (%ux%ux%u)\n",
			par->width_virt, par->height_virt, par->bpp);
		return -EINVAL;
	}

	return e1356fb_verify_timing(par, info);
}


static int
e1356fb_var_to_par(const struct fb_var_screeninfo* var,
		   struct e1356fb_par* par,
		   const struct fb_info_e1356* info)
{
	if ((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		DPRINTK("interlace not supported\n");
		return -EINVAL;
	}

	memset(par, 0, sizeof(struct e1356fb_par));

	par->width       = (var->xres + 15) & ~15; /* could sometimes be 8 */
	par->width_virt  = var->xres_virtual;
	par->height      = var->yres;
	par->height_virt = var->yres_virtual;
	par->bpp         = var->bits_per_pixel;
	par->Bpp         = (par->bpp + 7) >> 3;

	par->ipclk.pixclk_d = PICOS2KHZ(var->pixclock);

	par->hsync_start = var->right_margin;
	par->hsync_width = var->hsync_len;

	par->vsync_start = var->lower_margin;
	par->vsync_width = var->vsync_len;

	par->horiz_ndp = var->left_margin + var->right_margin + var->hsync_len;
	par->vert_ndp = var->upper_margin + var->lower_margin + var->vsync_len;

	par->hsync_pol = (var->sync & FB_SYNC_HOR_HIGH_ACT) ? 1 : 0;
	par->vsync_pol = (var->sync & FB_SYNC_VERT_HIGH_ACT) ? 1 : 0;

	par->cmap_len  = (par->bpp == 8) ? 256 : 16;

	return e1356fb_verify_par(par, info);
}

static int
e1356fb_par_to_var(struct fb_var_screeninfo* var,
		   struct e1356fb_par* par,
		   const struct fb_info_e1356* info)
{
	struct fb_var_screeninfo v;
	int ret;
    
	// First, make sure par is valid.
	if ((ret = e1356fb_verify_par(par, info)))
		return ret;

	memset(&v, 0, sizeof(struct fb_var_screeninfo));
	v.xres_virtual   = par->width_virt;
	v.yres_virtual   = par->height_virt;
	v.xres           = par->width;
	v.yres           = par->height;
	v.right_margin   = par->hsync_start;
	v.hsync_len      = par->hsync_width;
	v.left_margin    = par->horiz_ndp - par->hsync_start - par->hsync_width;
	v.lower_margin   = par->vsync_start;
	v.vsync_len      = par->vsync_width;
	v.upper_margin   = par->vert_ndp - par->vsync_start - par->vsync_width;
	v.bits_per_pixel = par->bpp;

	switch(par->bpp) {
	case 8:
		v.red.offset = v.green.offset = v.blue.offset = 0;
		v.red.length = v.green.length = v.blue.length = 4;
		break;
	case 16:
		v.red.offset   = 11;
		v.red.length   = 5;
		v.green.offset = 5;
		v.green.length = 6;
		v.blue.offset  = 0;
		v.blue.length  = 5;
		break;
	}

	v.height = v.width = -1;
	v.pixclock = KHZ2PICOS(par->ipclk.pixclk);

	if (par->hsync_pol)
		v.sync |= FB_SYNC_HOR_HIGH_ACT;
	if (par->vsync_pol)
		v.sync |= FB_SYNC_VERT_HIGH_ACT;

	*var = v;
	return 0;
}

static int
e1356fb_encode_fix(struct fb_fix_screeninfo*  fix,
		   const struct e1356fb_par*   par,
		   const struct fb_info_e1356* info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    
	strcpy(fix->id, "Epson SED1356");
	fix->smem_start  = info->fix.membase_phys;
	fix->smem_len    = info->fb_size;
	fix->mmio_start  = info->fix.regbase_phys;
	fix->mmio_len    = info->regbase_size;
	fix->accel       = FB_ACCEL_EPSON_SED1356;
	fix->type        = FB_TYPE_PACKED_PIXELS;
	fix->type_aux    = 0;
	fix->line_length = par->width_virt * par->Bpp;
	fix->visual      =
		(par->bpp == 8) ? FB_VISUAL_PSEUDOCOLOR	: FB_VISUAL_TRUECOLOR;
    
	fix->xpanstep    = info->fix.nopan ? 0 : 1;
	fix->ypanstep    = info->fix.nopan ? 0 : 1;
	fix->ywrapstep   = 0;
    
	return 0;
}

static int e1356fb_open(struct fb_info *fb, int user)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
        if (user) {
                info->open++;
	}
	MOD_INC_USE_COUNT;
	return 0;
}

static int e1356fb_release(struct fb_info *fb, int user)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
        if (user && info->open) {
                info->open--;
		if (info->open == 0)
                        info->mmaped = 0;
	}
	MOD_DEC_USE_COUNT;
	return 0;
}

static int
e1356fb_get_fix(struct fb_fix_screeninfo *fix, 
		int con,
		struct fb_info *fb)
{
	const struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	struct e1356fb_par par;

	//DPRINTK("\n");

	if (con == -1)
		par = info->current_par;
	else
		e1356fb_var_to_par(&fb_display[con].var, &par, info);
	e1356fb_encode_fix(fix, &par, info);
	return 0;
}

static int
e1356fb_get_var(struct fb_var_screeninfo *var, 
		int con,
		struct fb_info *fb)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;

	//DPRINTK("\n");

	if (con == -1)
		e1356fb_par_to_var(var, &info->current_par, info);
	else
		*var = fb_display[con].var;
	return 0;
}
 
static void
e1356fb_set_dispsw(struct display *disp, 
		   struct fb_info_e1356 *info,
		   int bpp, 
		   int accel)
{
	struct e1356fb_fix* fix = &info->fix;
	//DPRINTK("\n");

	if (disp->dispsw && disp->conp) 
		fb_con.con_cursor(disp->conp, CM_ERASE);
	switch (bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
		disp->dispsw = fix->noaccel ? &fbcon_cfb8 : &fbcon_e1356_8;
		if (fix->nohwcursor)
			fbcon_e1356_8.cursor = NULL;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw = fix->noaccel ? &fbcon_cfb16 : &fbcon_e1356_16;
		disp->dispsw_data = info->fbcon_cmap16;
		if (fix->nohwcursor)
			fbcon_e1356_16.cursor = NULL;
		break;
#endif
	default:
		disp->dispsw = &fbcon_dummy;
	}
   
}

static int
e1356fb_set_var(struct fb_var_screeninfo *var, 
		int con,
		struct fb_info *fb)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	struct e1356fb_par par;
	struct display *display;
	int oldxres, oldyres, oldvxres, oldvyres, oldbpp, oldaccel, accel, err;
	int activate = var->activate;
	int j,k;
    
	DPRINTK("\n");
	
	if (con >= 0)
		display = &fb_display[con];
	else
		display = fb->disp;	/* used during initialization */
   
	if ((err = e1356fb_var_to_par(var, &par, info))) {
		struct fb_videomode *dm;
		/*
		 * this mode didn't pass the tests. Try the
		 * corresponding mode from our own modedb.
		 */
		DPRINTK("req mode failed, trying SED1356 %dx%d mode\n",
			var->xres, var->yres);
		if (e1356fb_get_mode(info, var->xres,
				     var->yres, NULL, &dm) < 0) {
			DPRINTK("no SED1356 %dx%d mode found, failed\n",
				var->xres, var->yres);
			return err;
		}
		fb_videomode_to_var(dm, var);
		if ((err = e1356fb_var_to_par(var, &par, info))) {
			DPRINTK("SED1356 %dx%d mode failed\n",
				var->xres, var->yres);
			return err;
		}
	}
	
	if (info->fix.tv_filt & TV_FILT_FLICKER)
		printk("e1356fb: TV flicker filter enabled\n");
    
	e1356fb_par_to_var(var, &par, info);
   
	if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		oldxres  = display->var.xres;
		oldyres  = display->var.yres;
		oldvxres = display->var.xres_virtual;
		oldvyres = display->var.yres_virtual;
		oldbpp   = display->var.bits_per_pixel;
		oldaccel = display->var.accel_flags;
		display->var = *var;
		if (con < 0                         ||
		    oldxres  != var->xres           || 
		    oldyres  != var->yres           ||
		    oldvxres != var->xres_virtual   || 
		    oldvyres != var->yres_virtual   ||
		    oldbpp   != var->bits_per_pixel || 
		    oldaccel != var->accel_flags) {
			struct fb_fix_screeninfo fix;
	    
			e1356fb_encode_fix(&fix, &par, info);
			display->screen_base    = info->membase_virt;
			display->visual         = fix.visual;
			display->type           = fix.type;
			display->type_aux       = fix.type_aux;
			display->ypanstep       = fix.ypanstep;
			display->ywrapstep      = fix.ywrapstep;
			display->line_length    = fix.line_length;
			display->next_line      = fix.line_length;
			display->can_soft_blank = 1;
			display->inverse        = 0;
			accel = var->accel_flags & FB_ACCELF_TEXT;
			e1356fb_set_dispsw(display, info, par.bpp, accel);
	 
			if (info->fix.nopan)
				display->scrollmode = SCROLL_YREDRAW;
	
			if (info->fb_info.changevar)
				(*info->fb_info.changevar)(con);
		}
		if (var->bits_per_pixel==8)
			for(j = 0; j < 16; j++) {
				k = color_table[j];
				fb_info.palette[j].red   = default_red[k];
				fb_info.palette[j].green = default_grn[k];
				fb_info.palette[j].blue  = default_blu[k];
			}
      
		del_timer(&(info->cursor.timer)); 
		fb_info.cursor.state=CM_ERASE;
	
		if (!info->fb_info.display_fg ||
		    info->fb_info.display_fg->vc_num == con || con < 0)
			e1356fb_set_par(&par, info);

		if (!info->fix.nohwcursor) 
			if (display && display->conp)
				e1356fb_createcursor( display );
		info->cursor.redraw = 1;

		if (oldbpp != var->bits_per_pixel || con < 0) {
			if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
				return err;
			e1356fb_install_cmap(display, &(info->fb_info));
		}
	}
  
	return 0;
}

static int
e1356fb_pan_display(struct fb_var_screeninfo* var, 
		    int con,
		    struct fb_info* fb)
{
	struct fb_info_e1356* info = (struct fb_info_e1356*)fb;
	struct e1356fb_par* par = &info->current_par;
    
	//DPRINTK("\n");

	if (info->fix.nopan)
		return -EINVAL;

	if ((int)var->xoffset < 0 ||
	    var->xoffset + par->width > par->width_virt ||
	    (int)var->yoffset < 0 ||
	    var->yoffset + par->height > par->height_virt)
		return -EINVAL;
    
	if (con == currcon)
		do_pan_var(var, info);
    
	fb_display[con].var.xoffset = var->xoffset;
	fb_display[con].var.yoffset = var->yoffset; 

	return 0;
}

static int
e1356fb_get_cmap(struct fb_cmap *cmap, 
		 int kspc, 
		 int con,
		 struct fb_info *fb)
{
	struct fb_info_e1356* info = (struct fb_info_e1356*)fb;
	struct display *d = (con<0) ? fb->disp : fb_display + con;
   
	//DPRINTK("\n");

	if (con == currcon) {
		/* current console? */
		return fb_get_cmap(cmap, kspc, e1356fb_getcolreg, fb);
	} else if (d->cmap.len) {
		/* non default colormap? */
		fb_copy_cmap(&d->cmap, cmap, kspc ? 0 : 2);
	} else {
		fb_copy_cmap(fb_default_cmap(info->current_par.cmap_len),
			     cmap, kspc ? 0 : 2);
	}
	return 0;
}

static int
e1356fb_set_cmap(struct fb_cmap *cmap, 
		 int kspc, 
		 int con,
		 struct fb_info *fb)
{
	struct display *d = (con<0) ? fb->disp : fb_display + con;
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	int cmap_len = (info->current_par.bpp == 8) ? 256 : 16;

	//DPRINTK("\n");

	if (d->cmap.len!=cmap_len) {
		int err;
		if ((err = fb_alloc_cmap(&d->cmap, cmap_len, 0)))
			return err;
	}
    
	if (con == currcon) {
		/* current console? */
		return fb_set_cmap(cmap, kspc, e1356fb_setcolreg, fb);
	} else {
		fb_copy_cmap(cmap, &d->cmap, kspc ? 0 : 1);
	}
	return 0;
}

static int
e1356fb_ioctl(struct inode *inode, 
	      struct file *file, 
	      u_int cmd,
	      u_long arg, 
	      int con, 
	      struct fb_info *fb)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	blt_info_t blt;
	u16* src = NULL;
	int ret=0;
    
	switch (cmd) {
	case FBIO_SED1356_BITBLT:
		if (copy_from_user(&blt, (void *)arg, sizeof(blt_info_t)))
			return -EFAULT;
		if (blt.src) {
			if ((ret = verify_area(VERIFY_READ,
					       (void*)blt.src, blt.srcsize)))
				return ret;
			if ((src = (u16*)kmalloc(blt.srcsize,
						 GFP_KERNEL)) == NULL)
				return -ENOMEM;
			if (copy_from_user(src, (void *)blt.src, blt.srcsize))
				return -EFAULT;
			blt.src = src;
		}
		ret = doBlt(&info->current_par, info, &blt);
		if (src)
			kfree(src);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}


static int
e1356fb_mmap(struct fb_info *fb,
	     struct file *file,
	     struct vm_area_struct *vma)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	unsigned int len;
	phys_t start=0, off;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) {
		DPRINTK("invalid vma->vm_pgoff\n");
		return -EINVAL;
	}
    
#ifdef SHADOW_FRAME_BUFFER
	if (!info->shadow.fb) {
		int order = 0;
		while (info->fb_size > (PAGE_SIZE * (1 << order)))
			order++;
		info->shadow.fb = (void*)__get_free_pages(GFP_KERNEL, order);
		if (!info->shadow.fb) {
			DPRINTK("shadow fb alloc failed\n");
			return -ENXIO;
		}
		memset(info->shadow.fb, 0, info->fb_size);
		init_timer(&info->shadow.timer);
		info->shadow.timer.function = do_write_shadow_fb;
		info->shadow.timer.data = (unsigned long)info;
	}
	mod_timer(&info->shadow.timer, jiffies+HZ/2);
	start = virt_to_phys(info->shadow.fb) & PAGE_MASK;
#else
	start = info->fix.membase_phys & PAGE_MASK;
#endif

	len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fb_size);

	off = vma->vm_pgoff << PAGE_SHIFT;
    
	if ((vma->vm_end - vma->vm_start + off) > len) {
		DPRINTK("invalid vma\n");
		return -EINVAL;
	}

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
#ifdef SHADOW_FRAME_BUFFER
	vma->vm_flags |= VM_RESERVED;
	pgprot_val(vma->vm_page_prot) &= ~_CACHE_UNCACHED;
#else
	pgprot_val(vma->vm_page_prot) |= _CACHE_UNCACHED;
#endif

	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO;
	// FIXME: shouldn't have to do this. If the pages are marked writeable,
	// the TLB fault handlers should set these.
	pgprot_val(vma->vm_page_prot) |= (_PAGE_DIRTY | _PAGE_VALID);
    
	/*
	 * The SED1356 has only a 16-bit wide data bus, and some
	 * embedded platforms, such as the Pb1000, do not automatically
	 * split 32-bit word accesses to the framebuffer into
	 * seperate half-word accesses. Hence the upper half-word
	 * never gets to the framebuffer. The following solution is
	 * to intentionally return a non-32-bit-aligned VA. As long
	 * as the user app assumes (and doesn't check) that the returned
	 * VA is 32-bit aligned, all (assumed aligned) 32-bit accesses
	 * will actually be unaligned and will get trapped by the MIPS
	 * unaligned exception handler. This handler will emulate the
	 * load/store instructions by splitting up the load/store
	 * into two 16-bit load/stores. (This emulation is currently
	 * enabled by default, but may be disabled in the future, when
	 * alignment problems in user-level programs get fixed. When
	 * that happens, this solution won't work anymore, unless the
	 * process that mmap's the fb also calls sysmips(MIPS_FIXADE, 1),
	 * which turns address-error emulation back on).
	 *
	 * Furthermore, this solution only seems to work for TinyX
	 * (Xfbdev). Others, like Qt/E, do snoop the returned VA
	 * and compensate, or do originally unaligned 32-bit accesses
	 * which then become aligned, hence breaking this solution.
	 */
	if (info->fix.mmunalign)
		vma->vm_start += 2;
	
	if (io_remap_page_range(vma->vm_start, off,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	info->mmaped = 1;
	return 0;
}


int __init
e1356fb_init(void)
{
	struct fb_var_screeninfo var;
	struct e1356fb_fix * epfix = &fb_info.fix;
	e1356_reg_t* reg;
	void* regbase;
	char* name = "SED1356";
	int periodMCLK, periodBCLK;
	int dram_timing, rr_div, mclk_src;
	u8 rev_code, btmp, mclk_cfg;

	if (options) {
		e1356fb_setup(options, 0);
	}

	// clear out fb_info
	memset(&fb_info, 0, sizeof(struct fb_info_e1356));

	// copy boot options
	fb_info.fix = boot_fix;
	fb_info.default_par = boot_par;

	fb_info.regbase_size = E1356_REG_SIZE;

	if (!epfix->system) {
		printk(KERN_ERR "e1356/86fb: no valid system found\n");
		return -ENODEV;
	}

	if (epfix->system == SYS_SDU1356) {
		// it's the SDU1356B0C PCI eval card.
		struct pci_dev *pdev = NULL;
		if (!pci_present())   /* No PCI bus in this machine! */
			return -ENODEV;
		if (!(pdev = pci_find_device(PCI_VENDOR_ID_EPSON,
					     PCI_DEVICE_ID_EPSON_SDU1356, pdev)))
			return -ENODEV;
		if (pci_enable_device(pdev))
			return -ENODEV;
		epfix->regbase_phys = pci_resource_start(pdev, 0);
		epfix->membase_phys = epfix->regbase_phys + E1356_REG_SIZE;
	}
	
	fb_info.regbase_virt = ioremap_nocache(epfix->regbase_phys,
					       E1356_REG_SIZE);

	if (!fb_info.regbase_virt) {
		printk("e1356fb: Can't remap %s register area.\n", name);
		return -ENXIO;
	}

	regbase = fb_info.regbase_virt;
	reg = &fb_info.reg;
    
	// Initialize the register pointers
	reg->basic =         (reg_basic_t*)   (regbase + REG_BASE_BASIC);
	reg->genio =         (reg_genio_t*)   (regbase + REG_BASE_GENIO);
	reg->md_cfg =        (reg_mdcfg_t*)   (regbase + REG_BASE_MDCFG);
	reg->clk_cfg =       (reg_clkcfg_t*)  (regbase + REG_BASE_CLKCFG);
	reg->mem_cfg =       (reg_memcfg_t*)  (regbase + REG_BASE_MEMCFG);
	reg->panel_cfg =     (reg_panelcfg_t*)(regbase + REG_BASE_PANELCFG);
	reg->lcd_cfg =       (reg_dispcfg_t*) (regbase + REG_BASE_LCD_DISPCFG);
	reg->crttv_cfg =     (reg_dispcfg_t*) (regbase + REG_BASE_CRTTV_DISPCFG);
	reg->lcd_mode =      (reg_dispmode_t*)(regbase + REG_BASE_LCD_DISPMODE);
	reg->crttv_mode =    (reg_dispmode_t*)(regbase + REG_BASE_CRTTV_DISPMODE);
	reg->lcd_inkcurs =   (reg_inkcurs_t*) (regbase + REG_BASE_LCD_INKCURS);
	reg->crttv_inkcurs = (reg_inkcurs_t*) (regbase + REG_BASE_CRTTV_INKCURS);
	reg->bitblt =        (reg_bitblt_t*)  (regbase + REG_BASE_BITBLT);
	reg->lut =           (reg_lut_t*)     (regbase + REG_BASE_LUT);
	reg->pwr_save =      (reg_pwrsave_t*) (regbase + REG_BASE_PWRSAVE);
	reg->misc =          (reg_misc_t*)    (regbase + REG_BASE_MISC);
	reg->mediaplug =     (reg_mediaplug_t*)(regbase + REG_BASE_MEDIAPLUG);
	reg->bitblt_data =   (u16*)           (regbase + REG_BASE_BITBLT_DATA);
    
	// Enable all register access
	writeb(0, &reg->basic->misc);

	rev_code = readb(&reg->basic->rev_code);
	if ((rev_code >> 2) == 0x04) {
		printk("Found EPSON1356 Display Controller\n");
	}
	else if ((rev_code >> 2) == 0x07) {
		printk("Found EPSON13806 Display Controller\n");
	}
	else {
		iounmap(fb_info.regbase_virt);
		printk("e1356/806fb: %s not found, rev_code=0x%02x.\n",
		       name, rev_code);
		return -ENODEV;
	}

	fb_info.chip_rev = rev_code & 0x03;

	// Determine frame-buffer size
	switch (readb(&reg->md_cfg->md_cfg_stat0) >> 6) {
	case 0:
	case 2:
		fb_info.fb_size = 0x80000;   /* 512K bytes */
		break;
	case 1:
		if ((rev_code >> 2) == 7) /* 806 */
			fb_info.fb_size = 0x140000;  /* 1.2M bytes */
		else
			fb_info.fb_size = 0x200000;  /* 2M bytes */
		break;
	default:
		fb_info.fb_size = 0x200000;  /* 2M bytes */
		break;
	}

	fb_info.membase_virt = ioremap_nocache(epfix->membase_phys,
					       fb_info.fb_size);
    
	if (!fb_info.membase_virt) {
		printk("e1356fb: Can't remap %s framebuffer.\n", name);
		iounmap(fb_info.regbase_virt);
		return -ENXIO;
	}
    
	printk("e1356/806fb: Detected  %dKB framebuffer\n", 
			(unsigned)fb_info.fb_size/1000);

#ifdef CONFIG_MTRR
	if (!epfix->nomtrr) {
		fb_info.mtrr_idx = mtrr_add(epfix->membase_phys, fb_info.fb_size,
					    MTRR_TYPE_WRCOMB, 1);
		printk("e1356fb: MTRR's turned on\n");
	}
#endif
    
	if (!boot_fix.noaccel) {
		/*
		  Allocate a page for string BLTs. A 4K page is
		  enough for a 256 character string at an 8x16 font.
		*/
		fb_info.putcs_buffer = (void*)__get_free_pages(GFP_KERNEL, 0);
		if (fb_info.putcs_buffer == NULL) {
			printk("e1356fb: Can't allocate putcs buffer\n");
			goto unmap_ret_enxio;
		}
	}

	// Begin SED1356 initialization

	// disable display while initializing
	writeb(0, &reg->misc->disp_mode);
	// Set the GPIO1 and 2 to inputs
	writeb(0, &reg->genio->gpio_cfg);
	writeb(0, &reg->genio->gpio_ctrl);
	if (fb_info.chip_rev == 7) /* 806 */
		writeb(0, &reg->genio->gpio_ctrl2);

	/*
	 * Program the clocks
	 */

#ifdef CONFIG_SOC_AU1X00
	if ((epfix->system == SYS_PB1000) || (epfix->system == SYS_PB1500))
		epfix->busclk = get_au1x00_lcd_clock();
#endif
	
	if (epfix->busclk > 80000) {
		printk("e1356fb: specified busclk too high\n");
		goto ret_enxio;
	}

	epfix->mclk = mclk_cfg = 0;
	if (epfix->system == SYS_PB1500) {
		epfix->mclk = epfix->busclk;
		mclk_cfg = 0x01;
	}
	else {
		// Find the highest allowable MCLK
		if (epfix->busclk <= MAX_PIXCLOCK && 
				epfix->busclk > epfix->mclk) {
			epfix->mclk = epfix->busclk;
			mclk_cfg = 0x01;
		}
		if (epfix->clki <= MAX_PIXCLOCK && epfix->clki > epfix->mclk) {
			epfix->mclk = epfix->clki;
			mclk_cfg = 0x00;
		}
		if (epfix->busclk/2 <= MAX_PIXCLOCK && 
				epfix->busclk/2 > epfix->mclk) {
			epfix->mclk = epfix->busclk/2;
			mclk_cfg = 0x11;
		}
		if (epfix->clki/2 <= MAX_PIXCLOCK && 
				epfix->clki/2 > epfix->mclk) {
			epfix->mclk = epfix->clki/2;
			mclk_cfg = 0x10;
		}
	}
	
	if (!epfix->mclk) {
		printk("e1356fb: couldn't find an allowable MCLK!\n");
		goto ret_enxio;
	}

	// When changing mclk src, you must first set bit 4 to 1.
	writeb(readb(&reg->clk_cfg->mem_clk_cfg) | 0x10,
	       &reg->clk_cfg->mem_clk_cfg);
	writeb(mclk_cfg, &reg->clk_cfg->mem_clk_cfg);

	printk("e1356fb: clocks (kHz): busclk=%d mclk=%d clki=%d clki2=%d\n",
	       epfix->busclk, epfix->mclk, epfix->clki, epfix->clki2);

	// Set max pixel clock
	switch (epfix->disp_type) {
	case DISP_TYPE_LCD:
	case DISP_TYPE_TFT:
	case DISP_TYPE_CRT:
		fb_info.max_pixclock = epfix->mclk;
		break;
	case DISP_TYPE_NTSC:
	case DISP_TYPE_PAL:
		fb_info.max_pixclock = (epfix->disp_type == DISP_TYPE_NTSC) ?
			NTSC_PIXCLOCK : PAL_PIXCLOCK;
		if (epfix->tv_filt & TV_FILT_FLICKER)
			fb_info.max_pixclock *= 2;
		break;
	default:
		printk("e1356fb: invalid specified display type\n");
		goto ret_enxio;
	}

	periodMCLK = 1000000L / epfix->mclk;   // in nano-seconds
	periodBCLK = 1000000L / epfix->busclk; // in nano-seconds
	if (readb(&reg->md_cfg->md_cfg_stat1) & (1<<4))
		periodBCLK *= 2;
    
	if ((epfix->system == SYS_PB1000) || (epfix->system == SYS_PB1500))
		writeb(0x00, &reg->clk_cfg->cpu2mem_wait_sel);
	else if (periodMCLK - 4 > periodBCLK)
		writeb(0x02, &reg->clk_cfg->cpu2mem_wait_sel);
	else if (2*periodMCLK - 4 > periodBCLK)
		writeb(0x01, &reg->clk_cfg->cpu2mem_wait_sel);
	else
		writeb(0x00, &reg->clk_cfg->cpu2mem_wait_sel);

	// Program memory config
	if (epfix->mem_type < MEM_TYPE_EDO_2CAS ||
	    epfix->mem_type > MEM_TYPE_EMBEDDED_SDRAM) {
		printk("e1356fb: bad memory type specified\n");
		goto ret_enxio;
	}
	writeb((u8)epfix->mem_type, &reg->mem_cfg->mem_cfg);

	// calc closest refresh rate
	rr_div = 7;
	mclk_src = (mclk_cfg & 1) ? epfix->busclk : epfix->clki;
	while ((mclk_src >> (6 + rr_div)) < epfix->mem_refresh)
		if (--rr_div < 0) {
			printk("e1356fb: can't set specified refresh rate\n");
			goto ret_enxio;
		}
    
	DPRINTK("refresh rate = %d kHz\n", (mclk_src >> (6 + rr_div)));

	// add Suspend-Mode Refresh bits
	if (epfix->mem_smr < MEM_SMR_CBR || epfix->mem_smr > MEM_SMR_NONE) {
		printk("e1356fb: invalid specified suspend-mode refresh type\n");
		goto ret_enxio;
	}
	writeb(rr_div | (epfix->mem_smr << 6), &reg->mem_cfg->dram_refresh);

	// set DRAM speed
	switch (epfix->mem_speed) {
	case 50:
		dram_timing = epfix->mclk >= 33000 ? 0x0101 : 0x0212;
		break;
	case 60:
		if (epfix->mclk >= 30000)
			dram_timing = 0x0101;
		else if (epfix->mclk >= 25000)
			dram_timing =
				(epfix->mem_type == MEM_TYPE_EDO_2CAS ||
				 epfix->mem_type == MEM_TYPE_EDO_2WE) ?
				0x0212 : 0x0101;
		else
			dram_timing = 0x0212;
		break;
	case 70:
		if (epfix->mclk >= 30000)
			dram_timing = 0x0000;
		else if (epfix->mclk >= 25000)
			dram_timing = 0x0101;
		else
			dram_timing =
				(epfix->mem_type == MEM_TYPE_EDO_2CAS ||
				 epfix->mem_type == MEM_TYPE_EDO_2WE) ?
				0x0212 : 0x0211;
		break;
	case 80:
		if (epfix->mclk >= 25000)
			dram_timing = 0x0100;
		else
			dram_timing = 0x0101;
		break;
	default:
		printk("e1356fb: invalid specified memory speed\n");
		goto ret_enxio;
	}

	writew(dram_timing, &reg->mem_cfg->dram_timings_ctrl0);
    
	currcon = -1;
	if (!epfix->nohwcursor)
		e1356fb_hwcursor_init(&fb_info);
    
	init_timer(&fb_info.cursor.timer);
	fb_info.cursor.timer.function = do_flashcursor; 
	fb_info.cursor.timer.data = (unsigned long)(&fb_info);
	fb_info.cursor.state = CM_ERASE;
	spin_lock_init(&fb_info.cursor.lock);
    
	strcpy(fb_info.fb_info.modename, "Epson "); 
	strcat(fb_info.fb_info.modename, name);
	fb_info.fb_info.changevar  = NULL;
	fb_info.fb_info.node       = -1;

	fb_info.fb_info.fbops      = &e1356fb_ops;
	fb_info.fb_info.disp       = &fb_info.disp;
	strcpy(fb_info.fb_info.fontname, epfix->fontname);
	fb_info.fb_info.switch_con = &e1356fb_switch_con;
	fb_info.fb_info.updatevar  = &e1356fb_updatevar;
	fb_info.fb_info.blank      = &e1356fb_blank;
	fb_info.fb_info.flags      = FBINFO_FLAG_DEFAULT;
    
	// Set-up display
	// clear out unused stuff
	writeb(0, &reg->panel_cfg->mod_rate);
	writeb(0x01, &reg->lcd_mode->lcd_misc);
	writeb(0, &reg->lcd_mode->fifo_high_thresh);
	writeb(0, &reg->lcd_mode->fifo_low_thresh);
	writeb(0, &reg->crttv_mode->fifo_high_thresh);
	writeb(0, &reg->crttv_mode->fifo_low_thresh);
    
	switch (epfix->disp_type) {
	case DISP_TYPE_LCD:
		switch (epfix->panel_width) {
		case 4: btmp = (u8)(((epfix->panel_el & 1)<<7) | 0x04); break;
		case 8: btmp = (u8)(((epfix->panel_el & 1)<<7) | 0x14); break;
		case 16: btmp = (u8)(((epfix->panel_el & 1)<<7) | 0x24); break;
		default:
			printk("e1356fb: invalid specified LCD panel data width\n");
			goto ret_enxio;
		}
		writeb(btmp, &reg->panel_cfg->panel_type);
		break;
	case DISP_TYPE_TFT:
		switch (epfix->panel_width) {
		case 9: btmp = (u8)(((epfix->panel_el & 1)<<7) | 0x05); break;
		case 12: btmp = (u8)(((epfix->panel_el & 1)<<7) | 0x15); break;
		case 18: btmp = (u8)(((epfix->panel_el & 1)<<7) | 0x25); break;
		default:
			printk("e1356fb: invalid specified TFT panel data width\n");
			goto ret_enxio;
		}
		writeb(btmp, &reg->panel_cfg->panel_type);
		break;
	case DISP_TYPE_CRT:
		writeb(0x00, &reg->crttv_cfg->tv_output_ctrl);
		break;
	case DISP_TYPE_NTSC:
	case DISP_TYPE_PAL:
		if (epfix->tv_fmt < TV_FMT_COMPOSITE ||
		    epfix->tv_fmt > TV_FMT_S_VIDEO) {
			printk("e1356fb: invalid specified TV output format\n");
			goto ret_enxio;
		}
		btmp = epfix->disp_type == DISP_TYPE_PAL ? 0x01 : 0x00;
		btmp |= (epfix->tv_fmt == TV_FMT_S_VIDEO ? 0x02 : 0x00);
		btmp |= ((epfix->tv_filt & TV_FILT_LUM) ? 0x10 : 0x00);
		btmp |= ((epfix->tv_filt & TV_FILT_CHROM) ? 0x20 : 0x00);
		writeb(btmp, &reg->crttv_cfg->tv_output_ctrl);
		break;
	}

	memset(&var, 0, sizeof(var));
	/*
	 * If mode_option wasn't given at boot, assume all the boot
	 * option timing parameters were specified individually, in
	 * which case we convert par_to_var instead of calling
	 * fb_find_mode.
	 */
	if (epfix->mode_option) {
		struct fb_videomode* modedb, *dm;
		int dbsize = e1356fb_get_mode(&fb_info, 640, 480, &modedb, &dm);

		// first try the generic modedb
		if (!fb_find_mode(&var, &fb_info.fb_info, epfix->mode_option,
				  NULL, 0, NULL, boot_par.bpp)) {
			printk("e1356fb: mode %s failed, trying e1356 modedb\n",
			       epfix->mode_option);
			// didn't work in generic modedb, try ours
			if (!fb_find_mode(&var, &fb_info.fb_info,
					  epfix->mode_option,
					  modedb, dbsize, dm, boot_par.bpp)) {
				printk("e1356fb: mode %s failed e1356 modedb too, sorry\n",
				       epfix->mode_option);
				
				goto ret_enxio;
			}
		}

		var.xres_virtual = boot_par.width_virt ?
			boot_par.width_virt : boot_par.width;
		var.yres_virtual = boot_par.height_virt ?
			boot_par.height_virt : boot_par.height;
	} else {
		if (e1356fb_par_to_var(&var, &fb_info.default_par, &fb_info)) {
			printk("e1356fb: boot option mode failed\n");
			goto ret_enxio;
		}
	}
    
	if (boot_fix.noaccel)
		var.accel_flags &= ~FB_ACCELF_TEXT;
	else
		var.accel_flags |= FB_ACCELF_TEXT;
    
	if (e1356fb_var_to_par(&var, &fb_info.default_par, &fb_info)) {
		/*
		 * Can't use the mode from the mode db or the default
		 * mode or the boot options - give up
		 */
		printk("e1356fb: mode failed var_to_par\n");
		goto ret_enxio;
	}
    
	fb_info.disp.screen_base    = fb_info.membase_virt;
	fb_info.disp.var            = var; // struct copy
    
	// here's where the screen is actually initialized and enabled
	if (e1356fb_set_var(&var, -1, &fb_info.fb_info)) {
		printk("e1356fb: can't set video mode\n");
		goto ret_enxio;
	}
    
	writeb(0, &reg->pwr_save->cfg);     // disable power-save mode
	writeb(0, &reg->misc->cpu2mem_watchdog); // disable watchdog timer

#ifdef E1356FB_VERBOSE_DEBUG
	dump_fb(fb_info.membase_virt + 0x100000, 512);
#endif

	if (register_framebuffer(&fb_info.fb_info) < 0) {
		writeb(0, &reg->misc->disp_mode); 
		printk("e1356fb: can't register framebuffer\n");
		goto ret_enxio;
	}
    
	printk("fb%d: %s frame buffer device\n", 
	       GET_FB_IDX(fb_info.fb_info.node),
	       fb_info.fb_info.modename);
    
    
	return 0;

 ret_enxio:
	free_pages((unsigned long)fb_info.putcs_buffer, 0);
 unmap_ret_enxio:
	iounmap(fb_info.regbase_virt);
	iounmap(fb_info.membase_virt);
	return -ENXIO;
}

/**
 *	e1356fb_exit - Driver cleanup
 *
 *	Releases all resources allocated during the
 *	course of the driver's lifetime.
 *
 *	FIXME - do results of fb_alloc_cmap need disposal?
 */
static void __exit
e1356fb_exit (void)
{
	unregister_framebuffer(&fb_info.fb_info);
	del_timer_sync(&fb_info.cursor.timer);

#ifdef CONFIG_MTRR
	if (!fb_info.fix.nomtrr) {
		mtrr_del(fb_info.mtrr_idx, fb_info.fix.membase_phys,
			 fb_info.fb_size);
		printk("fb: MTRR's  turned off\n");
	}
#endif

	free_pages((unsigned long)fb_info.putcs_buffer, 0);
	iounmap(fb_info.regbase_virt);
	iounmap(fb_info.membase_virt);
}

MODULE_AUTHOR("Steve Longerbeam <stevel@mvista.com>");
MODULE_DESCRIPTION("SED1356 framebuffer device driver");

#ifdef MODULE
module_init(e1356fb_init);
#endif
module_exit(e1356fb_exit);


void
e1356fb_setup(char *options, int *ints)
{
	char* this_opt;
    
	memset(&boot_fix, 0, sizeof(struct e1356fb_fix));
	memset(&boot_par, 0, sizeof(struct e1356fb_par));
	boot_fix.system = -1;
    
	if (!options || !*options)
		return;
    
	for(this_opt=strtok(options, ","); this_opt;
	    this_opt=strtok(NULL, ",")) {
		if (!strncmp(this_opt, "noaccel", 7)) {
			boot_fix.noaccel = 1;
		} else if (!strncmp(this_opt, "nopan", 5)) {
			boot_fix.nopan = 1;
		} else if (!strncmp(this_opt, "nohwcursor", 10)) {
			boot_fix.nohwcursor = 1;
		} else if (!strncmp(this_opt, "mmunalign:", 10)) {
			boot_fix.mmunalign = simple_strtoul(this_opt+10,
							    NULL, 0);
#ifdef CONFIG_MTRR
		} else if (!strncmp(this_opt, "nomtrr", 6)) {
			boot_fix.nomtrr = 1;
#endif
		} else if (!strncmp(this_opt, "font:", 5)) {
			strncpy(boot_fix.fontname, this_opt+5,
				sizeof(boot_fix.fontname)-1);
		} else if (!strncmp(this_opt, "regbase:", 8)) {
			boot_fix.regbase_phys = simple_strtoul(this_opt+8,
							       NULL, 0);
		} else if (!strncmp(this_opt, "membase:", 8)) {
			boot_fix.membase_phys = simple_strtoul(this_opt+8,
							       NULL, 0);
		} else if (!strncmp(this_opt, "memsp:", 6)) {
			boot_fix.mem_speed = simple_strtoul(this_opt+6,
							    NULL, 0);
		} else if (!strncmp(this_opt, "memtyp:", 7)) {
			boot_fix.mem_type = simple_strtoul(this_opt+7,
							   NULL, 0);
		} else if (!strncmp(this_opt, "memref:", 7)) {
			boot_fix.mem_refresh = simple_strtoul(this_opt+7,
							      NULL, 0);
		} else if (!strncmp(this_opt, "memsmr:", 7)) {
			boot_fix.mem_smr = simple_strtoul(this_opt+7, NULL, 0);
		} else if (!strncmp(this_opt, "busclk:", 7)) {
			boot_fix.busclk = simple_strtoul(this_opt+7, NULL, 0);
		} else if (!strncmp(this_opt, "clki:", 5)) {
			boot_fix.clki = simple_strtoul(this_opt+5, NULL, 0);
		} else if (!strncmp(this_opt, "clki2:", 6)) {
			boot_fix.clki2 = simple_strtoul(this_opt+6, NULL, 0);
		} else if (!strncmp(this_opt, "display:", 8)) {
			if (!strncmp(this_opt+8, "lcd", 3))
				boot_fix.disp_type = DISP_TYPE_LCD;
			else if (!strncmp(this_opt+8, "tft", 3))
				boot_fix.disp_type = DISP_TYPE_TFT;
			else if (!strncmp(this_opt+8, "crt", 3))
				boot_fix.disp_type = DISP_TYPE_CRT;
			else if (!strncmp(this_opt+8, "pal", 3))
				boot_fix.disp_type = DISP_TYPE_PAL;
			else if (!strncmp(this_opt+8, "ntsc", 4))
				boot_fix.disp_type = DISP_TYPE_NTSC;
		} else if (!strncmp(this_opt, "width:", 6)) {
			boot_par.width = simple_strtoul(this_opt+6, NULL, 0);
		} else if (!strncmp(this_opt, "height:", 7)) {
			boot_par.height = simple_strtoul(this_opt+7, NULL, 0);
		} else if (!strncmp(this_opt, "bpp:", 4)) {
			boot_par.bpp = simple_strtoul(this_opt+4, NULL, 0);
			boot_par.cmap_len = (boot_par.bpp == 8) ? 256 : 16;
		} else if (!strncmp(this_opt, "elpanel:", 8)) {
			boot_fix.panel_el = simple_strtoul(this_opt+8,
							   NULL, 0);
		} else if (!strncmp(this_opt, "pdataw:", 7)) {
			boot_fix.panel_width = simple_strtoul(this_opt+7,
							      NULL, 0);
		} else if (!strncmp(this_opt, "hndp:", 5)) {
			boot_par.horiz_ndp = simple_strtoul(this_opt+5,
							    NULL, 0);
		} else if (!strncmp(this_opt, "vndp:", 5)) {
			boot_par.vert_ndp = simple_strtoul(this_opt+5,
							   NULL, 0);
		} else if (!strncmp(this_opt, "hspol:", 6)) {
			boot_par.hsync_pol = simple_strtoul(this_opt+6,
							    NULL, 0);
		} else if (!strncmp(this_opt, "vspol:", 6)) {
			boot_par.vsync_pol = simple_strtoul(this_opt+6,
							    NULL, 0);
		} else if (!strncmp(this_opt, "hsstart:", 8)) {
			boot_par.hsync_start = simple_strtoul(this_opt+8,
							      NULL, 0);
		} else if (!strncmp(this_opt, "hswidth:", 8)) {
			boot_par.hsync_width = simple_strtoul(this_opt+8,
							      NULL, 0);
		} else if (!strncmp(this_opt, "vsstart:", 8)) {
			boot_par.vsync_start = simple_strtoul(this_opt+8,
							      NULL, 0);
		} else if (!strncmp(this_opt, "vswidth:", 8)) {
			boot_par.vsync_width = simple_strtoul(this_opt+8,
							      NULL, 0);
		} else if (!strncmp(this_opt, "tvfilt:", 7)) {
			boot_fix.tv_filt = simple_strtoul(this_opt+7, NULL, 0);
		} else if (!strncmp(this_opt, "tvfmt:", 6)) {
			boot_fix.tv_fmt = simple_strtoul(this_opt+6, NULL, 0);
		} else if (!strncmp(this_opt, "system:", 7)) {
			if (!strncmp(this_opt+7, "pb1000", 10)) {
				boot_fix = systems[SYS_PB1000].fix;
				boot_par = systems[SYS_PB1000].par;
			} else if (!strncmp(this_opt+7, "pb1500", 7)) {
				boot_fix = systems[SYS_PB1500].fix;
				boot_par = systems[SYS_PB1500].par;
			} else if (!strncmp(this_opt+7, "sdu1356", 7)) {
				boot_fix = systems[SYS_SDU1356].fix;
				boot_par = systems[SYS_SDU1356].par;
			} else if (!strncmp(this_opt+7, "clio1050", 7)) {
				boot_fix = systems[SYS_CLIO1050].fix;
				boot_par = systems[SYS_CLIO1050].par;
			}
		} else {
			boot_fix.mode_option = this_opt;
		}
	} 
}


/*
 * FIXME: switching consoles could be dangerous. What if switching
 * from a panel to a CRT/TV, or vice versa? More needs to be
 * done here.
 */
static int
e1356fb_switch_con(int con, struct fb_info *fb)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	struct e1356fb_par par;
	int old_con = currcon;
	int set_par = 1;

	//DPRINTK("\n");

	/* Do we have to save the colormap? */
	if (currcon>=0)
		if (fb_display[currcon].cmap.len)
			fb_get_cmap(&fb_display[currcon].cmap, 1,
				    e1356fb_getcolreg, fb);
   
	currcon = con;
	fb_display[currcon].var.activate = FB_ACTIVATE_NOW; 
	e1356fb_var_to_par(&fb_display[con].var, &par, info);
	if (old_con>=0 && vt_cons[old_con]->vc_mode!=KD_GRAPHICS) {
		/* check if we have to change video registers */
		struct e1356fb_par old_par;
		e1356fb_var_to_par(&fb_display[old_con].var, &old_par, info);
		if (!memcmp(&par,&old_par,sizeof(par)))
			set_par = 0;	/* avoid flicker */
	}
	if (set_par)
		e1356fb_set_par(&par, info);
    
	if (fb_display[con].dispsw && fb_display[con].conp)
		fb_con.con_cursor(fb_display[con].conp, CM_ERASE);
   
	del_timer(&(info->cursor.timer));
	fb_info.cursor.state=CM_ERASE; 
   
	if (!info->fix.nohwcursor) 
		if (fb_display[con].conp)
			e1356fb_createcursor( &fb_display[con] );
   
	info->cursor.redraw=1;
   
	e1356fb_set_dispsw(&fb_display[con], 
			   info, 
			   par.bpp,
			   fb_display[con].var.accel_flags & FB_ACCELF_TEXT);
   
	e1356fb_install_cmap(&fb_display[con], fb);
	e1356fb_updatevar(con, fb);
   
	return 1;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */
static void
e1356fb_blank(int blank, struct fb_info *fb)
{
	struct fb_info_e1356 *info = (struct fb_info_e1356*)fb;
	reg_dispmode_t* dispmode = (IS_PANEL(info->fix.disp_type)) ?
		info->reg.lcd_mode : info->reg.crttv_mode;
	reg_pwrsave_t* pwrsave = info->reg.pwr_save;

	//DPRINTK("\n");

	switch (blank) {
	case 0:
		// Get out of power save mode
		writeb(0x00, &pwrsave->cfg);
		writeb(readb(&dispmode->disp_mode) & ~0x80,
		       &dispmode->disp_mode);
		break;
	case 1:
		// Get out of power save mode
		writeb(0x00, &pwrsave->cfg);
		writeb(readb(&dispmode->disp_mode) | 0x80,
		       &dispmode->disp_mode);
		break;
		// No support for turning off horiz or vert sync, so just treat
		// it as a power off.
	case 2:
	case 3:
	case 4:
		writeb(0x01, &pwrsave->cfg);
		break;
	}
}


static int
e1356fb_updatevar(int con, struct fb_info* fb)
{
	struct fb_info_e1356* i = (struct fb_info_e1356*)fb;

	//DPRINTK("\n");

	if ((con==currcon) && (!i->fix.nopan)) 
		do_pan_var(&fb_display[con].var,i);
	return 0;
}

static int
e1356fb_getcolreg(unsigned        regno, 
		  unsigned*       red, 
		  unsigned*       green,
		  unsigned*       blue, 
		  unsigned*       transp,
		  struct fb_info* fb)
{
	struct fb_info_e1356* i = (struct fb_info_e1356*)fb;

	if (regno > i->current_par.cmap_len)
		return 1;
   
	*red    = i->palette[regno].red; 
	*green  = i->palette[regno].green; 
	*blue   = i->palette[regno].blue; 
	*transp = 0;
   
	return 0;
}

static int
e1356fb_setcolreg(unsigned        regno, 
		  unsigned        red, 
		  unsigned        green,
		  unsigned        blue, 
		  unsigned        transp,
		  struct fb_info* info)
{
	struct fb_info_e1356* i = (struct fb_info_e1356*)info;

	if (regno > 255)
		return 1;

	i->palette[regno].red    = red;
	i->palette[regno].green  = green;
	i->palette[regno].blue   = blue;
   
	switch(i->current_par.bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
		do_setpalentry(i->reg.lut, regno,
			       (u8)(red>>8), (u8)(green>>8), (u8)(blue>>8));
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		i->fbcon_cmap16[regno] = (regno << 10) | (regno << 5) | regno;
		break;
#endif
	default:
		DPRINTK("bad depth %u\n", i->current_par.bpp);
		break;
	}
	return 0;
}

static void
e1356fb_install_cmap(struct display *d, struct fb_info *info) 
{
	struct fb_info_e1356* i = (struct fb_info_e1356*)info;

	//DPRINTK("\n");

	if (d->cmap.len) {
		fb_set_cmap(&(d->cmap), 1, e1356fb_setcolreg, info);
	} else {
		fb_set_cmap(fb_default_cmap(i->current_par.cmap_len), 1,
			    e1356fb_setcolreg, info);
	}
}

static void
e1356fb_createcursorshape(struct display* p) 
{
	int h,u;
   
	h = fontheight(p);

	fb_info.cursor.type = p->conp->vc_cursor_type & CUR_HWMASK;

	switch (fb_info.cursor.type) {
	case CUR_NONE: 
		u = h; 
		break;
	case CUR_UNDERLINE: 
		u = h - 2; 
		break;
	case CUR_LOWER_THIRD: 
		u = (h * 2) / 3; 
		break;
	case CUR_LOWER_HALF: 
		u = h / 2; 
		break;
	case CUR_TWO_THIRDS: 
		u = h / 3; 
		break;
	case CUR_BLOCK:
	default:
		u = 0;
		break;
	}
    
	fb_info.cursor.w = fontwidth_x8(p);
	fb_info.cursor.u = u;
	fb_info.cursor.h = h;
}
   
static void
e1356fb_createcursor(struct display *p)
{
	void* memcursor;
	int y, w, h, u;
    
	e1356fb_createcursorshape(p);

	h = fb_info.cursor.h;
	w = fb_info.cursor.w;
	u = fb_info.cursor.u;
	memcursor = fb_info.membase_virt + fb_info.fb_size;

	// write cursor to display memory
	for (y=0; y<64; y++) {
		if (y >= h || y < u) {
			fbfill((u16*)memcursor, 0xaa, 16); // b/g
		} else {
			fbfill((u16*)memcursor, 0xff, w/4); // inverted b/g
			fbfill((u16*)memcursor + w/4, 0xaa, (64 - w)/4); // b/g
		}
		memcursor += 16;
	}
}
   
static void
e1356fb_hwcursor_init(struct fb_info_e1356* info)
{
	reg_inkcurs_t* inkcurs = (IS_PANEL(info->fix.disp_type)) ?
		info->reg.lcd_inkcurs : info->reg.crttv_inkcurs;

	fb_info.fb_size -= 1024;
	// program cursor base address
	writeb(0x00, &inkcurs->start_addr);
	printk("e1356fb: reserving 1024 bytes for the hwcursor at %p\n",
	       fb_info.membase_virt + fb_info.fb_size);
}
