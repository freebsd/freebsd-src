/*
 * linux/drivers/video/sti/stifb.c - 
 * Frame buffer driver for HP workstations with STI (standard text interface) 
 * video firmware.
 *
 * Copyright (C) 2001 Helge Deller <deller@gmx.de>
 * Portions Copyright (C) 2001 Thomas Bogendoerfer <tsbogend@alpha.franken.de>
 * 
 * Based on:
 * - linux/drivers/video/artistfb.c -- Artist frame buffer driver
 *	Copyright (C) 2000 Philipp Rumpf <prumpf@tux.org>
 *   - based on skeletonfb, which was
 *	Created 28 Dec 1997 by Geert Uytterhoeven
 * - HP Xhp cfb-based X11 window driver for XFree86
 *	(c)Copyright 1992 Hewlett-Packard Co.
 *
 * 
 *  The following graphics display devices (NGLE family) are supported by this driver:
 *
 *  HPA4070A	known as "HCRX", a 1280x1024 color device with 8 planes
 *  HPA4071A	known as "HCRX24", a 1280x1024 color device with 24 planes,
 *		optionally available with a hardware accelerator as HPA4071A_Z
 *  HPA1659A	known as "CRX", a 1280x1024 color device with 8 planes
 *  HPA1439A	known as "CRX24", a 1280x1024 color device with 24 planes,
 *		optionally available with a hardware accelerator.
 *  HPA1924A	known as "GRX", a 1280x1024 grayscale device with 8 planes
 *  HPA2269A	known as "Dual CRX", a 1280x1024 color device with 8 planes,
 *		implements support for two displays on a single graphics card.
 *  HP710C	internal graphics support optionally available on the HP9000s710 SPU,
 *		supports 1280x1024 color displays with 8 planes.
 *  HP710G	same as HP710C, 1280x1024 grayscale only
 *  HP710L	same as HP710C, 1024x768 color only
 *  HP712	internal graphics support on HP9000s712 SPU, supports 640x480, 
 *		1024x768 or 1280x1024 color displays on 8 planes (Artist)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/* TODO:
 *	- Artist gfx is the only supported chip atm,
 *	- remove the static fb_info to support multiple cards
 *	- remove the completely untested 1bpp mode
 *	- add support for h/w acceleration
 *	- add hardware cursor
 *	-
 */


/* on supported graphic devices you may:
 * #define FALLBACK_TO_1BPP to fall back to 1 bpp, or
 * #undef  FALLBACK_TO_1BPP to reject support for unsupported cards */
#undef FALLBACK_TO_1BPP


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
#include <linux/init.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/pci.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb32.h>

#include <asm/grfioctl.h>	/* for HP-UX compatibility */

#include "sticore.h"

extern struct display_switch fbcon_sti; /* fbcon-sti.c */

#ifdef __LP64__
/* return virtual address */
#define REGION_BASE(fb_info, index) \
	(fb_info->sti->glob_cfg->region_ptrs[index] | 0xffffffff00000000)
#else
/* return virtual address */
#define REGION_BASE(fb_info, index) \
	fb_info->sti->glob_cfg->region_ptrs[index]
#endif

#define NGLEDEVDEPROM_CRT_REGION 1

typedef struct {
	__s32	video_config_reg;
	__s32	misc_video_start;
	__s32	horiz_timing_fmt;
	__s32	serr_timing_fmt;
	__s32	vert_timing_fmt;
	__s32	horiz_state;
	__s32	vert_state;
	__s32	vtg_state_elements;
	__s32	pipeline_delay;
	__s32	misc_video_end;
} video_setup_t;

typedef struct {                  
	__s16	sizeof_ngle_data;
	__s16	x_size_visible;	    /* visible screen dim in pixels  */
	__s16	y_size_visible;
	__s16	pad2[15];
	__s16	cursor_pipeline_delay;
	__s16	video_interleaves;
	__s32	pad3[11];
} ngle_rom_t;

struct stifb_info {
	struct fb_info info;
        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;
	struct display disp;
	struct sti_struct *sti;
	unsigned int id, real_id;
	int currcon;
	int cmap_reload:1;   
	int deviceSpecificConfig;
	ngle_rom_t ngle_rom;
	struct { u8 red, green, blue; } palette[256];
#ifdef FBCON_HAS_CFB32	
	union {
		u32 cfb32[16];
	} fbcon_cmap;
#endif			
};

static int stifb_force_bpp[MAX_STI_ROMS];

/* ------------------- chipset specific functions -------------------------- */

/* offsets to graphic-chip internal registers */

#define REG_1		0x000118
#define REG_2		0x000480
#define REG_3		0x0004a0
#define REG_4		0x000600
#define REG_6		0x000800
#define REG_8		0x000820
#define REG_9		0x000a04
#define REG_10		0x018000
#define REG_11		0x018004
#define REG_12		0x01800c
#define REG_13		0x018018
#define REG_14  	0x01801c
#define REG_15		0x200000
#define REG_15b0	0x200000
#define REG_16b1	0x200005
#define REG_16b3	0x200007
#define REG_21		0x200218
#define REG_22		0x0005a0
#define REG_23		0x0005c0
#define REG_26		0x200118
#define REG_27		0x200308
#define REG_32		0x21003c
#define REG_33		0x210040
#define REG_34		0x200008
#define REG_35		0x018010
#define REG_38		0x210020
#define REG_39		0x210120
#define REG_40		0x210130
#define REG_42		0x210028
#define REG_43		0x21002c
#define REG_44		0x210030
#define REG_45		0x210034

#define READ_BYTE(fb,reg)		__raw_readb((fb)->fix.mmio_start + (reg))
#define READ_WORD(fb,reg)		__raw_readl((fb)->fix.mmio_start + (reg))
#define WRITE_BYTE(value,fb,reg)	__raw_writeb((value),(fb)->fix.mmio_start + (reg))
#define WRITE_WORD(value,fb,reg)	__raw_writel((value),(fb)->fix.mmio_start + (reg))

#define ENABLE	1	/* for enabling/disabling screen */	
#define DISABLE 0

#define NGLE_LOCK(fb_info)	do { } while (0) 
#define NGLE_UNLOCK(fb_info)	do { } while (0)

static void
SETUP_HW(struct stifb_info *fb)
{
	char stat;

	do {
		stat = READ_BYTE(fb, REG_15b0);
		if (!stat)
	    		stat = READ_BYTE(fb, REG_15b0);
	} while (stat);
}


static void
SETUP_FB(struct stifb_info *fb)
{	
	unsigned int reg10_value = 0;
	
	SETUP_HW(fb);
	switch (fb->id)
	{
		case CRT_ID_VISUALIZE_EG:
		case S9000_ID_ARTIST:
		case S9000_ID_A1659A:
			reg10_value = 0x13601000;
			break;
		case S9000_ID_A1439A:
			if (fb->var.bits_per_pixel == 32)						
				reg10_value = 0xBBA0A000;
			else 
				reg10_value = 0x13601000;
			break;
		case S9000_ID_HCRX:
			if (fb->var.bits_per_pixel == 32)
				reg10_value = 0xBBA0A000;
			else					
				reg10_value = 0x13602000;
			break;
		case S9000_ID_TIMBER:
		case CRX24_OVERLAY_PLANES:
			reg10_value = 0x13602000;
			break;
	}
	if (reg10_value)
		WRITE_WORD(reg10_value, fb, REG_10);
	WRITE_WORD(0x83000300, fb, REG_14);
	SETUP_HW(fb);
	WRITE_BYTE(1, fb, REG_16b1);
}

static void
START_IMAGE_COLORMAP_ACCESS(struct stifb_info *fb)
{
	SETUP_HW(fb);
	WRITE_WORD(0xBBE0F000, fb, REG_10);
	WRITE_WORD(0x03000300, fb, REG_14);
	WRITE_WORD(~0, fb, REG_13);
}

static void
WRITE_IMAGE_COLOR(struct stifb_info *fb, int index, int color) 
{
	SETUP_HW(fb);
	WRITE_WORD(((0x100+index)<<2), fb, REG_3);
	WRITE_WORD(color, fb, REG_4);
}

static void
FINISH_IMAGE_COLORMAP_ACCESS(struct stifb_info *fb) 
{		
	WRITE_WORD(0x400, fb, REG_2);
	if (fb->var.bits_per_pixel == 32) {
		WRITE_WORD(0x83000100, fb, REG_1);
	} else {
		if (fb->id == S9000_ID_ARTIST || fb->id == CRT_ID_VISUALIZE_EG)
			WRITE_WORD(0x80000100, fb, REG_26);
		else							
			WRITE_WORD(0x80000100, fb, REG_1);
	}
	SETUP_FB(fb);
}

static void
SETUP_RAMDAC(struct stifb_info *fb) 
{
	SETUP_HW(fb);
	WRITE_WORD(0x04000000, fb, 0x1020);
	WRITE_WORD(0xff000000, fb, 0x1028);
}

static void 
CRX24_SETUP_RAMDAC(struct stifb_info *fb) 
{
	SETUP_HW(fb);
	WRITE_WORD(0x04000000, fb, 0x1000);
	WRITE_WORD(0x02000000, fb, 0x1004);
	WRITE_WORD(0xff000000, fb, 0x1008);
	WRITE_WORD(0x05000000, fb, 0x1000);
	WRITE_WORD(0x02000000, fb, 0x1004);
	WRITE_WORD(0x03000000, fb, 0x1008);
}

#if 0
static void 
HCRX_SETUP_RAMDAC(struct stifb_info *fb)
{
	WRITE_WORD(0xffffffff, fb, REG_32);
}
#endif

static void 
CRX24_SET_OVLY_MASK(struct stifb_info *fb)
{
	SETUP_HW(fb);
	WRITE_WORD(0x13a02000, fb, REG_11);
	WRITE_WORD(0x03000300, fb, REG_14);
	WRITE_WORD(0x000017f0, fb, REG_3);
	WRITE_WORD(0xffffffff, fb, REG_13);
	WRITE_WORD(0xffffffff, fb, REG_22);
	WRITE_WORD(0x00000000, fb, REG_23);
}

static void
ENABLE_DISABLE_DISPLAY(struct stifb_info *fb, int enable)
{
	unsigned int value = enable ? 0x43000000 : 0x03000000;
        SETUP_HW(fb);
        WRITE_WORD(0x06000000,	fb, 0x1030);
        WRITE_WORD(value, 	fb, 0x1038);
}

static void 
CRX24_ENABLE_DISABLE_DISPLAY(struct stifb_info *fb, int enable)
{
	unsigned int value = enable ? 0x10000000 : 0x30000000;
	SETUP_HW(fb);
	WRITE_WORD(0x01000000,	fb, 0x1000);
	WRITE_WORD(0x02000000,	fb, 0x1004);
	WRITE_WORD(value,	fb, 0x1008);
}

static void
ARTIST_ENABLE_DISABLE_DISPLAY(struct stifb_info *fb, int enable) 
{
	u32 DregsMiscVideo = REG_21;
	u32 DregsMiscCtl = REG_27;
	
	SETUP_HW(fb);
	if (enable) {
	  WRITE_WORD(READ_WORD(fb, DregsMiscVideo) | 0x0A000000, fb, DregsMiscVideo);
	  WRITE_WORD(READ_WORD(fb, DregsMiscCtl)   | 0x00800000, fb, DregsMiscCtl);
	} else {
	  WRITE_WORD(READ_WORD(fb, DregsMiscVideo) & ~0x0A000000, fb, DregsMiscVideo);
	  WRITE_WORD(READ_WORD(fb, DregsMiscCtl)   & ~0x00800000, fb, DregsMiscCtl);
	}
}

#define GET_ROMTABLE_INDEX(fb) \
	(READ_BYTE(fb, REG_16b3) - 1)

#define HYPER_CONFIG_PLANES_24 0x00000100
	
#define IS_24_DEVICE(fb) \
	(fb->deviceSpecificConfig & HYPER_CONFIG_PLANES_24)

#define IS_888_DEVICE(fb) \
	(!(IS_24_DEVICE(fb)))

#define GET_FIFO_SLOTS(fb, cnt, numslots)			\
{	while (cnt < numslots) 					\
		cnt = READ_WORD(fb, REG_34);	\
	cnt -= numslots;					\
}

#define	    IndexedDcd	0	/* Pixel data is indexed (pseudo) color */
#define	    Otc04	2	/* Pixels in each longword transfer (4) */
#define	    Otc32	5	/* Pixels in each longword transfer (32) */
#define	    Ots08	3	/* Each pixel is size (8)d transfer (1) */
#define	    OtsIndirect	6	/* Each bit goes through FG/BG color(8) */
#define	    AddrLong	5	/* FB address is Long aligned (pixel) */
#define	    BINovly	0x2	/* 8 bit overlay */
#define	    BINapp0I	0x0	/* Application Buffer 0, Indexed */
#define	    BINapp1I	0x1	/* Application Buffer 1, Indexed */
#define	    BINapp0F8	0xa	/* Application Buffer 0, Fractional 8-8-8 */
#define	    BINattr	0xd	/* Attribute Bitmap */
#define	    RopSrc 	0x3
#define	    BitmapExtent08  3	/* Each write hits ( 8) bits in depth */
#define	    BitmapExtent32  5	/* Each write hits (32) bits in depth */
#define	    DataDynamic	    0	/* Data register reloaded by direct access */
#define	    MaskDynamic	    1	/* Mask register reloaded by direct access */
#define	    MaskOtc	    0	/* Mask contains Object Count valid bits */

#define MaskAddrOffset(offset) (offset)
#define StaticReg(en) (en)
#define BGx(en) (en)
#define FGx(en) (en)

#define BAJustPoint(offset) (offset)
#define BAIndexBase(base) (base)
#define BA(F,C,S,A,J,B,I) \
	(((F)<<31)|((C)<<27)|((S)<<24)|((A)<<21)|((J)<<16)|((B)<<12)|(I))

#define IBOvals(R,M,X,S,D,L,B,F) \
	(((R)<<8)|((M)<<16)|((X)<<24)|((S)<<29)|((D)<<28)|((L)<<31)|((B)<<1)|(F))

#define NGLE_QUICK_SET_IMAGE_BITMAP_OP(fb, val) \
	WRITE_WORD(val, fb, REG_14)

#define NGLE_QUICK_SET_DST_BM_ACCESS(fb, val) \
	WRITE_WORD(val, fb, REG_11)

#define NGLE_QUICK_SET_CTL_PLN_REG(fb, val) \
	WRITE_WORD(val, fb, REG_12)

#define NGLE_REALLY_SET_IMAGE_PLANEMASK(fb, plnmsk32) \
	WRITE_WORD(plnmsk32, fb, REG_13)

#define NGLE_REALLY_SET_IMAGE_FG_COLOR(fb, fg32) \
	WRITE_WORD(fg32, fb, REG_35)

#define NGLE_SET_TRANSFERDATA(fb, val) \
	WRITE_WORD(val, fb, REG_8)

#define NGLE_SET_DSTXY(fb, val) \
	WRITE_WORD(val, fb, REG_6)

#define NGLE_LONG_FB_ADDRESS(fbaddrbase, x, y) (		\
	(u32) (fbaddrbase) +				\
	    (	(unsigned int)  ( (y) << 13      ) |		\
		(unsigned int)  ( (x) << 2       )	)	\
	)

#define NGLE_BINC_SET_DSTADDR(fb, addr) \
	WRITE_WORD(addr, fb, REG_3)

#define NGLE_BINC_SET_SRCADDR(fb, addr) \
	WRITE_WORD(addr, fb, REG_2)

#define NGLE_BINC_SET_DSTMASK(fb, mask) \
	WRITE_WORD(mask, fb, REG_22)

#define NGLE_BINC_WRITE32(fb, data32) \
	WRITE_WORD(data32, fb, REG_23)

#define START_COLORMAPLOAD(fb, cmapBltCtlData32) \
	WRITE_WORD((cmapBltCtlData32), fb, REG_38)

#define SET_LENXY_START_RECFILL(fb, lenxy) \
	WRITE_WORD(lenxy, fb, REG_9)

static void
HYPER_ENABLE_DISABLE_DISPLAY(struct stifb_info *fb, int enable)
{
	u32 DregsHypMiscVideo = REG_33;
	unsigned int value;
	SETUP_HW(fb);
	value = READ_WORD(fb, DregsHypMiscVideo);
	if (enable)
		value |= 0x0A000000;
	else
		value &= ~0x0A000000;
	WRITE_WORD(value, fb, DregsHypMiscVideo);
}


/* BufferNumbers used by SETUP_ATTR_ACCESS() */
#define BUFF0_CMAP0	0x00001e02
#define BUFF1_CMAP0	0x02001e02
#define BUFF1_CMAP3	0x0c001e02
#define ARTIST_CMAP0	0x00000102
#define HYPER_CMAP8	0x00000100
#define HYPER_CMAP24	0x00000800

static void
SETUP_ATTR_ACCESS(struct stifb_info *fb, unsigned BufferNumber)
{
	SETUP_HW(fb);
	WRITE_WORD(0x2EA0D000, fb, REG_11);
	WRITE_WORD(0x23000302, fb, REG_14);
	WRITE_WORD(BufferNumber, fb, REG_12);
	WRITE_WORD(0xffffffff, fb, REG_8);
}

static void
SET_ATTR_SIZE(struct stifb_info *fb, int width, int height) 
{
	WRITE_WORD(0x00000000, fb, REG_6);
	WRITE_WORD((width<<16) | height, fb, REG_9);
	WRITE_WORD(0x05000000, fb, REG_6);
	WRITE_WORD(0x00040001, fb, REG_9);
}

static void
FINISH_ATTR_ACCESS(struct stifb_info *fb) 
{
	SETUP_HW(fb);
	WRITE_WORD(0x00000000, fb, REG_12);
}

static void
elkSetupPlanes(struct stifb_info *fb)
{
	SETUP_RAMDAC(fb);
	SETUP_FB(fb);
}

static void 
ngleSetupAttrPlanes(struct stifb_info *fb, int BufferNumber)
{
	SETUP_ATTR_ACCESS(fb, BufferNumber);
	SET_ATTR_SIZE(fb, fb->var.xres, fb->var.yres);
	FINISH_ATTR_ACCESS(fb);
	SETUP_FB(fb);
}


static void
rattlerSetupPlanes(struct stifb_info *fb)
{
	CRX24_SETUP_RAMDAC(fb);
    
	/* replacement for: SETUP_FB(fb, CRX24_OVERLAY_PLANES); */
	WRITE_WORD(0x83000300, fb, REG_14);
	SETUP_HW(fb);
	WRITE_BYTE(1, fb, REG_16b1);

	/* XXX: replace by fb_setmem(), smem_start or screen_base ? */
	memset_io(fb->fix.smem_start, 0xff,
		fb->var.yres*fb->fix.line_length);
    
	CRX24_SET_OVLY_MASK(fb);
	SETUP_FB(fb);
}


#define HYPER_CMAP_TYPE				0
#define NGLE_CMAP_INDEXED0_TYPE			0
#define NGLE_CMAP_OVERLAY_TYPE			3

/* typedef of LUT (Colormap) BLT Control Register */
typedef union	/* Note assumption that fields are packed left-to-right */
{	u32 all;
	struct
	{
		unsigned enable              :  1;
		unsigned waitBlank           :  1;
		unsigned reserved1           :  4;
		unsigned lutOffset           : 10;   /* Within destination LUT */
		unsigned lutType             :  2;   /* Cursor, image, overlay */
		unsigned reserved2           :  4;
		unsigned length              : 10;
	} fields;
} NgleLutBltCtl;


#if 0
static NgleLutBltCtl
setNgleLutBltCtl(struct stifb_info *fb, int offsetWithinLut, int length)
{
	NgleLutBltCtl lutBltCtl;

	/* set enable, zero reserved fields */
	lutBltCtl.all           = 0x80000000;
	lutBltCtl.fields.length = length;

	switch (fb->id) 
	{
	case S9000_ID_A1439A:		/* CRX24 */
		if (fb->var.bits_per_pixel == 8) {
			lutBltCtl.fields.lutType = NGLE_CMAP_OVERLAY_TYPE;
			lutBltCtl.fields.lutOffset = 0;
		} else {
			lutBltCtl.fields.lutType = NGLE_CMAP_INDEXED0_TYPE;
			lutBltCtl.fields.lutOffset = 0 * 256;
		}
		break;
		
	case S9000_ID_ARTIST:
		lutBltCtl.fields.lutType = NGLE_CMAP_INDEXED0_TYPE;
		lutBltCtl.fields.lutOffset = 0 * 256;
		break;
		
	default:
		lutBltCtl.fields.lutType = NGLE_CMAP_INDEXED0_TYPE;
		lutBltCtl.fields.lutOffset = 0;
		break;
	}

	/* Offset points to start of LUT.  Adjust for within LUT */
	lutBltCtl.fields.lutOffset += offsetWithinLut;

	return lutBltCtl;
}
#endif

static NgleLutBltCtl
setHyperLutBltCtl(struct stifb_info *fb, int offsetWithinLut, int length) 
{
	NgleLutBltCtl lutBltCtl;

	/* set enable, zero reserved fields */
	lutBltCtl.all = 0x80000000;

	lutBltCtl.fields.length = length;
	lutBltCtl.fields.lutType = HYPER_CMAP_TYPE;

	/* Expect lutIndex to be 0 or 1 for image cmaps, 2 or 3 for overlay cmaps */
	if (fb->var.bits_per_pixel == 8)
		lutBltCtl.fields.lutOffset = 2 * 256;
	else
		lutBltCtl.fields.lutOffset = 0 * 256;

	/* Offset points to start of LUT.  Adjust for within LUT */
	lutBltCtl.fields.lutOffset += offsetWithinLut;

	return lutBltCtl;
}


static void hyperUndoITE(struct stifb_info *fb)
{
	int nFreeFifoSlots = 0;
	u32 fbAddr;

	NGLE_LOCK(fb);

	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 1);
	WRITE_WORD(0xffffffff, fb, REG_32);

	/* Write overlay transparency mask so only entry 255 is transparent */

	/* Hardware setup for full-depth write to "magic" location */
	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 7);
	NGLE_QUICK_SET_DST_BM_ACCESS(fb, 
		BA(IndexedDcd, Otc04, Ots08, AddrLong,
		BAJustPoint(0), BINovly, BAIndexBase(0)));
	NGLE_QUICK_SET_IMAGE_BITMAP_OP(fb,
		IBOvals(RopSrc, MaskAddrOffset(0),
		BitmapExtent08, StaticReg(0),
		DataDynamic, MaskOtc, BGx(0), FGx(0)));

	/* Now prepare to write to the "magic" location */
	fbAddr = NGLE_LONG_FB_ADDRESS(0, 1532, 0);
	NGLE_BINC_SET_DSTADDR(fb, fbAddr);
	NGLE_REALLY_SET_IMAGE_PLANEMASK(fb, 0xffffff);
	NGLE_BINC_SET_DSTMASK(fb, 0xffffffff);

	/* Finally, write a zero to clear the mask */
	NGLE_BINC_WRITE32(fb, 0);

	NGLE_UNLOCK(fb);
}

static void 
ngleDepth8_ClearImagePlanes(struct stifb_info *fb)
{
	/* FIXME! */
}

static void 
ngleDepth24_ClearImagePlanes(struct stifb_info *fb)
{
	/* FIXME! */
}

static void
ngleResetAttrPlanes(struct stifb_info *fb, unsigned int ctlPlaneReg)
{
	int nFreeFifoSlots = 0;
	u32 packed_dst;
	u32 packed_len;

	NGLE_LOCK(fb);

	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 4);
	NGLE_QUICK_SET_DST_BM_ACCESS(fb, 
				     BA(IndexedDcd, Otc32, OtsIndirect,
					AddrLong, BAJustPoint(0),
					BINattr, BAIndexBase(0)));
	NGLE_QUICK_SET_CTL_PLN_REG(fb, ctlPlaneReg);
	NGLE_SET_TRANSFERDATA(fb, 0xffffffff);

	NGLE_QUICK_SET_IMAGE_BITMAP_OP(fb,
				       IBOvals(RopSrc, MaskAddrOffset(0),
					       BitmapExtent08, StaticReg(1),
					       DataDynamic, MaskOtc,
					       BGx(0), FGx(0)));
	packed_dst = 0;
	packed_len = (fb->var.xres << 16) | fb->var.yres;
	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 2);
	NGLE_SET_DSTXY(fb, packed_dst);
	SET_LENXY_START_RECFILL(fb, packed_len);

	/*
	 * In order to work around an ELK hardware problem (Buffy doesn't
	 * always flush it's buffers when writing to the attribute
	 * planes), at least 4 pixels must be written to the attribute
	 * planes starting at (X == 1280) and (Y != to the last Y written
	 * by BIF):
	 */

	if (fb->id == S9000_ID_A1659A) {   /* ELK_DEVICE_ID */
		/* It's safe to use scanline zero: */
		packed_dst = (1280 << 16);
		GET_FIFO_SLOTS(fb, nFreeFifoSlots, 2);
		NGLE_SET_DSTXY(fb, packed_dst);
		packed_len = (4 << 16) | 1;
		SET_LENXY_START_RECFILL(fb, packed_len);
	}   /* ELK Hardware Kludge */

	/**** Finally, set the Control Plane Register back to zero: ****/
	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 1);
	NGLE_QUICK_SET_CTL_PLN_REG(fb, 0);
	
	NGLE_UNLOCK(fb);
}
    
static void
ngleClearOverlayPlanes(struct stifb_info *fb, int mask, int data)
{
	int nFreeFifoSlots = 0;
	u32 packed_dst;
	u32 packed_len;
    
	NGLE_LOCK(fb);

	/* Hardware setup */
	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 8);
	NGLE_QUICK_SET_DST_BM_ACCESS(fb, 
				     BA(IndexedDcd, Otc04, Ots08, AddrLong,
					BAJustPoint(0), BINovly, BAIndexBase(0)));

        NGLE_SET_TRANSFERDATA(fb, 0xffffffff);  /* Write foreground color */

        NGLE_REALLY_SET_IMAGE_FG_COLOR(fb, data);
        NGLE_REALLY_SET_IMAGE_PLANEMASK(fb, mask);
    
        packed_dst = 0;
        packed_len = (fb->var.xres << 16) | fb->var.yres;
        NGLE_SET_DSTXY(fb, packed_dst);
    
        /* Write zeroes to overlay planes */		       
	NGLE_QUICK_SET_IMAGE_BITMAP_OP(fb,
				       IBOvals(RopSrc, MaskAddrOffset(0),
					       BitmapExtent08, StaticReg(0),
					       DataDynamic, MaskOtc, BGx(0), FGx(0)));
		       
        SET_LENXY_START_RECFILL(fb, packed_len);

	NGLE_UNLOCK(fb);
}

static void 
hyperResetPlanes(struct stifb_info *fb, int enable)
{
	unsigned int controlPlaneReg;

	NGLE_LOCK(fb);

	if (IS_24_DEVICE(fb))
		if (fb->var.bits_per_pixel == 32)
			controlPlaneReg = 0x04000F00;
		else
			controlPlaneReg = 0x00000F00;   /* 0x00000800 should be enought, but lets clear all 4 bits */
	else
		controlPlaneReg = 0x00000F00; /* 0x00000100 should be enought, but lets clear all 4 bits */

	switch (enable) {
	case 1:		/* ENABLE */
		/* clear screen */
		if (IS_24_DEVICE(fb))
			ngleDepth24_ClearImagePlanes(fb);
		else
			ngleDepth8_ClearImagePlanes(fb);

		/* Paint attribute planes for default case.
		 * On Hyperdrive, this means all windows using overlay cmap 0. */
		ngleResetAttrPlanes(fb, controlPlaneReg);

		/* clear overlay planes */
	        ngleClearOverlayPlanes(fb, 0xff, 255);

		/**************************************************
		 ** Also need to counteract ITE settings 
		 **************************************************/
		hyperUndoITE(fb);
		break;

	case 0:		/* DISABLE */
		/* clear screen */
		if (IS_24_DEVICE(fb))
			ngleDepth24_ClearImagePlanes(fb);
		else
			ngleDepth8_ClearImagePlanes(fb);
		ngleResetAttrPlanes(fb, controlPlaneReg);
		ngleClearOverlayPlanes(fb, 0xff, 0);
		break;

	case -1:	/* RESET */
		hyperUndoITE(fb);
		ngleResetAttrPlanes(fb, controlPlaneReg);
		break;
    	}
	
	NGLE_UNLOCK(fb);
}

/* Return pointer to in-memory structure holding ELK device-dependent ROM values. */

static void 
ngleGetDeviceRomData(struct stifb_info *fb)
{
#if 0
XXX: FIXME: !!!
	int	*pBytePerLongDevDepData;/* data byte == LSB */
	int 	*pRomTable;
	NgleDevRomData	*pPackedDevRomData;
	int	sizePackedDevRomData = sizeof(*pPackedDevRomData);
	char	*pCard8;
	int	i;
	char	*mapOrigin = NULL;
    
	int romTableIdx;

	pPackedDevRomData = fb->ngle_rom;

	SETUP_HW(fb);
	if (fb->id == S9000_ID_ARTIST) {
		pPackedDevRomData->cursor_pipeline_delay = 4;
		pPackedDevRomData->video_interleaves     = 4;
	} else {
		/* Get pointer to unpacked byte/long data in ROM */
		pBytePerLongDevDepData = fb->sti->regions[NGLEDEVDEPROM_CRT_REGION];

		/* Tomcat supports several resolutions: 1280x1024, 1024x768, 640x480 */
		if (fb->id == S9000_ID_TOMCAT)
	{
	    /*  jump to the correct ROM table  */
	    GET_ROMTABLE_INDEX(romTableIdx);
	    while  (romTableIdx > 0)
	    {
		pCard8 = (Card8 *) pPackedDevRomData;
		pRomTable = pBytePerLongDevDepData;
		/* Pack every fourth byte from ROM into structure */
		for (i = 0; i < sizePackedDevRomData; i++)
		{
		    *pCard8++ = (Card8) (*pRomTable++);
		}

		pBytePerLongDevDepData = (Card32 *)
			((Card8 *) pBytePerLongDevDepData +
			       pPackedDevRomData->sizeof_ngle_data);

		romTableIdx--;
	    }
	}

	pCard8 = (Card8 *) pPackedDevRomData;

	/* Pack every fourth byte from ROM into structure */
	for (i = 0; i < sizePackedDevRomData; i++)
	{
	    *pCard8++ = (Card8) (*pBytePerLongDevDepData++);
	}
    }

    SETUP_FB(fb);
#endif
}


#define HYPERBOWL_MODE_FOR_8_OVER_88_LUT0_NO_TRANSPARENCIES	4
#define HYPERBOWL_MODE01_8_24_LUT0_TRANSPARENT_LUT1_OPAQUE	8
#define HYPERBOWL_MODE01_8_24_LUT0_OPAQUE_LUT1_OPAQUE		10
#define HYPERBOWL_MODE2_8_24					15

/* HCRX specific boot-time initialization */
static void __init
SETUP_HCRX(struct stifb_info *fb)
{
	int	hyperbowl;
        int	nFreeFifoSlots = 0;

	if (fb->id != S9000_ID_HCRX)
		return;

	/* Initialize Hyperbowl registers */
	GET_FIFO_SLOTS(fb, nFreeFifoSlots, 7);
	
	if (IS_24_DEVICE(fb)) {
		hyperbowl = (fb->var.bits_per_pixel == 32) ?
			HYPERBOWL_MODE01_8_24_LUT0_TRANSPARENT_LUT1_OPAQUE :
			HYPERBOWL_MODE01_8_24_LUT0_OPAQUE_LUT1_OPAQUE;

		/* First write to Hyperbowl must happen twice (bug) */
		WRITE_WORD(hyperbowl, fb, REG_40);
		WRITE_WORD(hyperbowl, fb, REG_40);
		
		WRITE_WORD(HYPERBOWL_MODE2_8_24, fb, REG_39);
		
		WRITE_WORD(0x014c0148, fb, REG_42); /* Set lut 0 to be the direct color */
		WRITE_WORD(0x404c4048, fb, REG_43);
		WRITE_WORD(0x034c0348, fb, REG_44);
		WRITE_WORD(0x444c4448, fb, REG_45);
	} else {
		hyperbowl = HYPERBOWL_MODE_FOR_8_OVER_88_LUT0_NO_TRANSPARENCIES;

		/* First write to Hyperbowl must happen twice (bug) */
		WRITE_WORD(hyperbowl, fb, REG_40);
		WRITE_WORD(hyperbowl, fb, REG_40);

		WRITE_WORD(0x00000000, fb, REG_42);
		WRITE_WORD(0x00000000, fb, REG_43);
		WRITE_WORD(0x00000000, fb, REG_44);
		WRITE_WORD(0x444c4048, fb, REG_45);
	}
}


/* ------------------- driver specific functions --------------------------- */

static int
stifb_getcolreg(u_int regno, u_int *red, u_int *green,
	      u_int *blue, u_int *transp, struct fb_info *info)
{
	struct stifb_info *fb = (struct stifb_info *) info;

	if (regno > 255)
		return 1;
	*red = (fb->palette[regno].red<<8) | fb->palette[regno].red;
	*green = (fb->palette[regno].green<<8) | fb->palette[regno].green;
	*blue = (fb->palette[regno].blue<<8) | fb->palette[regno].blue;
	*transp = 0;

	return 0;
}

static int
stifb_setcolreg(u_int regno, u_int red, u_int green,
	      u_int blue, u_int transp, struct fb_info *info)
{
	struct stifb_info *fb = (struct stifb_info *) info;

	if (regno > 255)
		return 1;

	red >>= 8;
	green >>= 8;
	blue >>= 8;
	
	if ((fb->palette[regno].red != red) ||
	    (fb->palette[regno].green != green) ||
	    (fb->palette[regno].blue != blue))
		fb->cmap_reload = 1;
	    
	fb->palette[regno].red = red;
	fb->palette[regno].green = green;
	fb->palette[regno].blue = blue;
    
#ifdef FBCON_HAS_CFB32
	if (regno < 16 && fb->var.bits_per_pixel == 32) {
		fb->fbcon_cmap.cfb32[regno] = ((red << 16) |
					       (green << 8) |
					       (blue << 0) |
					       (transp << 24));
	}
#endif
	return 0;
}

static void
stifb_loadcmap(struct stifb_info *fb)
{
	u32 color;
	int i;

	if (!fb->cmap_reload)
		return;

	START_IMAGE_COLORMAP_ACCESS(fb);
	for (i = 0; i < 256; i++) {
		if (fb->var.bits_per_pixel > 8) {
			color = (i << 16) | (i << 8) | i;
		} else {
			if (fb->var.grayscale) {
				/* gray = 0.30*R + 0.59*G + 0.11*B */
				color = ((fb->palette[i].red * 77) +
					 (fb->palette[i].green * 151) +
					 (fb->palette[i].blue * 28)) >> 8;
			} else {
				color = ((fb->palette[i].red << 16) |
					 (fb->palette[i].green << 8) |
					 (fb->palette[i].blue));
			}
		}
		WRITE_IMAGE_COLOR(fb, i, color);
	}
	if (fb->id == S9000_ID_HCRX) {
		NgleLutBltCtl lutBltCtl;

		lutBltCtl = setHyperLutBltCtl(fb,
				0,	/* Offset w/i LUT */
				256);	/* Load entire LUT */
		NGLE_BINC_SET_SRCADDR(fb,
				NGLE_LONG_FB_ADDRESS(0, 0x100, 0)); 
				/* 0x100 is same as used in WRITE_IMAGE_COLOR() */
		START_COLORMAPLOAD(fb, lutBltCtl.all);
		SETUP_FB(fb);
	} else {
		/* cleanup colormap hardware */
		FINISH_IMAGE_COLORMAP_ACCESS(fb);
	}
	fb->cmap_reload = 0;
}

static int
stifb_get_fix(struct fb_fix_screeninfo *fix, int con,
	      struct fb_info *info)
{
	memcpy (fix, &((struct stifb_info *)info)->fix, sizeof (*fix));
	return 0;
}

static int
stifb_get_var(struct fb_var_screeninfo *var, int con,
	      struct fb_info *info)
{
	memcpy (var, &((struct stifb_info *)info)->var, sizeof (*var));
	return 0;
}

static int
stifb_set_var(struct fb_var_screeninfo *var, int con,
	      struct fb_info *info)
{
	struct display *disp;
	
	if (con >= 0)
		disp = &fb_display[con];
	else
		disp = info->disp;

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		if (disp->var.xres != var->xres ||
		    disp->var.yres != var->yres ||
		    disp->var.xres_virtual != var->xres_virtual ||
		    disp->var.yres_virtual != var->yres_virtual ||
		    disp->var.bits_per_pixel != var->bits_per_pixel ||
		    disp->var.accel_flags != var->accel_flags)
			return -EINVAL;
	}
	return 0;
}

static int
stifb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
	       struct fb_info *info)
{
	struct stifb_info *fb = (struct stifb_info *)info;
	
	if (con == fb->currcon) /* current console ? */
		return fb_get_cmap(cmap, kspc, stifb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap ? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(fb_display[con].var.bits_per_pixel > 8 ? 16 : 256), cmap, kspc ? 0: 2);
	return 0;
}

static int
stifb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
	       struct fb_info *info)
{
	struct stifb_info *fb = (struct stifb_info *)info;
	struct display *disp;
	int err;
	
	if (con >= 0)
		disp = &fb_display[con];
	else
		disp = info->disp;

	if (!disp->cmap.len) { /* no colormap allocated ? */
		if ((err = fb_alloc_cmap(&disp->cmap, disp->var.bits_per_pixel > 8 ? 16 : 256, 0)))
			return err;
	}
	if (con == fb->currcon || con == -1) {
		err = fb_set_cmap(cmap, kspc, stifb_setcolreg, info);
		if (!err)
			stifb_loadcmap ((struct stifb_info *)info);
		return err;
	} else
		fb_copy_cmap(cmap, &disp->cmap, kspc ? 0 : 1);
	return 0;
}
					 
static void
stifb_blank(int blank_mode, struct fb_info *info)
{
	struct stifb_info *fb = (struct stifb_info *) info;
	int enable = (blank_mode == 0) ? ENABLE : DISABLE;

	switch (fb->id) {
	case S9000_ID_A1439A:
		CRX24_ENABLE_DISABLE_DISPLAY(fb, enable);
		break;
	case CRT_ID_VISUALIZE_EG:
	case S9000_ID_ARTIST:
		ARTIST_ENABLE_DISABLE_DISPLAY(fb, enable);
		break;
	case S9000_ID_HCRX:
		HYPER_ENABLE_DISABLE_DISPLAY(fb, enable);
		break;
	case S9000_ID_A1659A:;	/* fall through */
	case S9000_ID_TIMBER:;
	case CRX24_OVERLAY_PLANES:;
	default:
		ENABLE_DISABLE_DISPLAY(fb, enable);
		break;
	}
	
	SETUP_FB(fb);
}

static void
stifb_set_disp(struct stifb_info *fb)
{
	int id = fb->id;

	SETUP_FB(fb);

	/* HCRX specific initialization */
	SETUP_HCRX(fb);
	
	/*
	if (id == S9000_ID_HCRX)
		hyperInitSprite(fb);
	else
		ngleInitSprite(fb);
	*/
	
	/* Initialize the image planes. */ 
        switch (id) {
	 case S9000_ID_HCRX:
	    hyperResetPlanes(fb, ENABLE);
	    break;
	 case S9000_ID_A1439A:
	    rattlerSetupPlanes(fb);
	    break;
	 case S9000_ID_A1659A:
	 case S9000_ID_ARTIST:
	 case CRT_ID_VISUALIZE_EG:
	    elkSetupPlanes(fb);
	    break;
	}

	/* Clear attribute planes on non HCRX devices. */
        switch (id) {
	 case S9000_ID_A1659A:
	 case S9000_ID_A1439A:
	    if (fb->var.bits_per_pixel == 32)
		ngleSetupAttrPlanes(fb, BUFF1_CMAP3);
	    else {
		ngleSetupAttrPlanes(fb, BUFF1_CMAP0);
	    }
	    if (id == S9000_ID_A1439A)
		ngleClearOverlayPlanes(fb, 0xff, 0);
	    break;
	 case S9000_ID_ARTIST:
	 case CRT_ID_VISUALIZE_EG:
	    if (fb->var.bits_per_pixel == 32)
		ngleSetupAttrPlanes(fb, BUFF1_CMAP3);
	    else {
		ngleSetupAttrPlanes(fb, ARTIST_CMAP0);
	    }
	    break;
	}
	stifb_blank(0, (struct fb_info *)fb);	/* 0=enable screen */

	SETUP_FB(fb);
}

static int
stifb_switch(int con, struct fb_info *info)
{
	struct stifb_info *fb = (struct stifb_info *)info;
	
	/* Do we have to save the colormap ? */
	if (fb->currcon != -1 && fb_display[fb->currcon].cmap.len)
		fb_get_cmap(&fb_display[fb->currcon].cmap, 1, stifb_getcolreg, info);

	fb->currcon = con;
	/* Install new colormap */
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, stifb_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(fb_display[con].var.bits_per_pixel > 8 ? 16 : 256),
			    1, stifb_setcolreg, info);
	stifb_loadcmap ((struct stifb_info *)info);
	return 0;
}

static int
stifb_update_var(int con, struct fb_info *info)
{
	return 0;
}

/* ------------ Interfaces to hardware functions ------------ */

static struct fb_ops stifb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	stifb_get_fix,
	fb_get_var:	stifb_get_var,
	fb_set_var:	stifb_set_var,
	fb_get_cmap:	stifb_get_cmap,
	fb_set_cmap:	stifb_set_cmap,
//	fb_pan_display:	fbgen_pan_display,
//	fb_ioctl:       xxxfb_ioctl,   /* optional */      
};


    /*
     *  Initialization
     */

int __init
stifb_init_fb(struct sti_struct *sti, int force_bpp)
{
	struct fb_fix_screeninfo *fix;
	struct fb_var_screeninfo *var;
	struct display *disp;
	struct stifb_info *fb;
	unsigned long sti_rom_address;
	char *dev_name;
	int bpp, xres, yres;

	fb = kmalloc(sizeof(struct stifb_info), GFP_ATOMIC);
	if (!fb) {
		printk(KERN_ERR "stifb: Could not allocate stifb structure\n");
		return -ENODEV;
	}
	
	/* set struct to a known state */
	memset(fb, 0, sizeof(struct stifb_info));
	fix = &fb->fix;
	var = &fb->var;
	disp = &fb->disp;

	fb->currcon = -1;
	fb->cmap_reload = 1;
	fb->sti = sti;
	/* store upper 32bits of the graphics id */
	fb->id = fb->sti->graphics_id[0];
	fb->real_id = fb->id;	/* save the real id */

	/* only supported cards are allowed */
	switch (fb->id) {
	case S9000_ID_ARTIST:
	case S9000_ID_HCRX:
	case S9000_ID_TIMBER:
	case S9000_ID_A1659A:
	case S9000_ID_A1439A:
	case CRT_ID_VISUALIZE_EG:
		break;
	default:
		printk(KERN_WARNING "stifb: Unsupported gfx card id 0x%08x\n",
			fb->id);
		goto out_err1;
	}
	
	/* default to 8 bpp on most graphic chips */
	bpp = 8;
	xres = sti_onscreen_x(fb->sti);
	yres = sti_onscreen_y(fb->sti);

	ngleGetDeviceRomData(fb);

	/* get (virtual) io region base addr */
	fix->mmio_start = REGION_BASE(fb,2);
	fix->mmio_len   = 0x400000;

       	/* Reject any device not in the NGLE family */
	switch (fb->id) {
	case S9000_ID_A1659A:	/* CRX/A1659A */
		break;
	case S9000_ID_ELM:	/* GRX, grayscale but else same as A1659A */
		var->grayscale = 1;
		fb->id = S9000_ID_A1659A;
		break;
	case S9000_ID_TIMBER:	/* HP9000/710 Any (may be a grayscale device) */
		dev_name = fb->sti->outptr.dev_name;
		if (strstr(dev_name, "GRAYSCALE") || 
		    strstr(dev_name, "Grayscale") ||
		    strstr(dev_name, "grayscale"))
			var->grayscale = 1;
		break;
	case S9000_ID_TOMCAT:	/* Dual CRX, behaves else like a CRX */
		/* FIXME: TomCat supports two heads:
		 * fb.iobase = REGION_BASE(fb_info,3);
		 * fb.screen_base = (void*) REGION_BASE(fb_info,2);
		 * for now we only support the left one ! */
		xres = fb->ngle_rom.x_size_visible;
		yres = fb->ngle_rom.y_size_visible;
		fb->id = S9000_ID_A1659A;
		break;
	case S9000_ID_A1439A:	/* CRX24/A1439A */
		if (force_bpp == 8 || force_bpp == 32)
		  bpp = force_bpp;
		else
		  bpp = 32;
		break;
	case S9000_ID_HCRX:	/* Hyperdrive/HCRX */
		memset(&fb->ngle_rom, 0, sizeof(fb->ngle_rom));
		if ((fb->sti->regions_phys[0] & 0xfc000000) ==
		    (fb->sti->regions_phys[2] & 0xfc000000))
			sti_rom_address = fb->sti->regions_phys[0];
		else
			sti_rom_address = fb->sti->regions_phys[1];
#ifdef __LP64__
	        sti_rom_address |= 0xffffffff00000000;
#endif
		fb->deviceSpecificConfig = __raw_readl(sti_rom_address);
		if (IS_24_DEVICE(fb)) {
			if (force_bpp == 8 || force_bpp == 32)
				bpp = force_bpp;
			else
				bpp = 32;
		} else
			bpp = 8;
		READ_WORD(fb, REG_15);
		SETUP_HW(fb);
		break;
	case CRT_ID_VISUALIZE_EG:
	case S9000_ID_ARTIST:	/* Artist */
		break;
	default: 
#ifdef FALLBACK_TO_1BPP
	       	printk(KERN_WARNING 
			"stifb: Unsupported graphics card (id=0x%08x) "
				"- now trying 1bpp mode instead\n",
			fb->id);
		bpp = 1;	/* default to 1 bpp */
		break;
#else
	       	printk(KERN_WARNING 
			"stifb: Unsupported graphics card (id=0x%08x) "
				"- skipping.\n",
			fb->id);
		goto out_err1;
#endif
	}


	/* get framebuffer pysical and virtual base addr & len (64bit ready) */
	fix->smem_start = fb->sti->regions_phys[1] | 0xffffffff00000000;
	fix->smem_len = fb->sti->regions[1].region_desc.length * 4096;

	fix->line_length = (fb->sti->glob_cfg->total_x * bpp) / 8;
	if (!fix->line_length)
		fix->line_length = 2048; /* default */
	fix->accel = FB_ACCEL_NONE;

	switch (bpp) {
	    case 1:
		fix->type = FB_TYPE_PLANES;	/* well, sort of */
		fix->visual = FB_VISUAL_MONO10;
		disp->dispsw = &fbcon_sti;
		break;
#ifdef FBCON_HAS_CFB8		
	    case 8:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	 	disp->dispsw = &fbcon_cfb8;
		var->red.length = var->green.length = var->blue.length = 8;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	    case 32:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->visual = FB_VISUAL_TRUECOLOR;
		disp->dispsw = &fbcon_cfb32;
		disp->dispsw_data = fb->fbcon_cmap.cfb32;
		var->red.length = var->green.length = var->blue.length = var->transp.length = 8;
		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
		var->transp.offset = 24;
		break;
#endif
	    default:
		disp->dispsw = &fbcon_dummy;
		break;
	}
	
	var->xres = var->xres_virtual = xres;
	var->yres = var->yres_virtual = yres;
	var->bits_per_pixel = bpp;

	disp->var = *var;
	disp->visual = fix->visual;
	disp->type = fix->type;
	disp->type_aux = fix->type_aux;
	disp->line_length = fix->line_length;
	disp->var.activate = FB_ACTIVATE_NOW;
	disp->screen_base = (void*) REGION_BASE(fb,1);
	disp->can_soft_blank = 1;
	disp->scrollmode = SCROLL_YREDRAW;
	
	strcpy(fb->info.modename, "stifb");
	fb->info.node = -1;
	fb->info.flags = FBINFO_FLAG_DEFAULT;
	fb->info.fbops = &stifb_ops;
	fb->info.disp = disp;
	fb->info.changevar = NULL;
	fb->info.switch_con = &stifb_switch;
	fb->info.updatevar = &stifb_update_var;
	fb->info.blank = &stifb_blank;
	fb->info.flags = FBINFO_FLAG_DEFAULT;
	
	stifb_set_var(&disp->var, 1, &fb->info);
	
	stifb_set_disp(fb);

	if (!request_mem_region(fix->smem_start, fix->smem_len, "stifb")) {
		printk(KERN_ERR "stifb: cannot reserve fb region 0x%04lx-0x%04lx\n",
				fix->smem_start, fix->smem_start+fix->smem_len);
		goto out_err1;
	}
		
	if (!request_mem_region(fix->mmio_start, fix->mmio_len, "stifb mmio")) {
		printk(KERN_ERR "stifb: cannot reserve sti mmio region 0x%04lx-0x%04lx\n",
				fix->mmio_start, fix->mmio_start+fix->mmio_len);
		goto out_err2;
	}

	if (register_framebuffer(&fb->info) < 0)
		goto out_err3;

	printk(KERN_INFO 
	    "fb%d: %s %dx%d-%d frame buffer device, id: %04x, mmio: 0x%04lx\n",
		GET_FB_IDX(fb->info.node), 
		fb->info.modename,
		disp->var.xres, 
		disp->var.yres,
		disp->var.bits_per_pixel,
		fb->id, 
		fix->mmio_start);

	return 0;


out_err3:
	release_mem_region(fix->mmio_start, fix->mmio_len);
out_err2:
	release_mem_region(fix->smem_start, fix->smem_len);
out_err1:
	kfree(fb);
	return -ENXIO;
}

int __init
stifb_init(void)
{
	struct sti_struct *sti;
	int i;
	
	
	if (sti_init_roms() == NULL)
		return -ENXIO; /* no STI cards available */

	for (i = 0; i < MAX_STI_ROMS; i++) {
		sti = sti_get_rom(i);
		if (sti)
			stifb_init_fb (sti, stifb_force_bpp[i]);
		else
			break;
	}
	return 0;
}

/*
 *  Cleanup
 */

void __exit
stifb_cleanup(struct fb_info *info)
{
	// unregister_framebuffer(info); 
}

int __init
stifb_setup(char *options)
{
	int i;
	
	if (!options || !*options)
		return 0;
	
	if (strncmp(options, "bpp", 3) == 0) {
		options += 3;
		for (i = 0; i < MAX_STI_ROMS; i++) {
			if (*options++ == ':')
				stifb_force_bpp[i] = simple_strtoul(options, &options, 10);
			else
				break;
		}
	}
	return 0;
}

__setup("stifb=", stifb_setup);

#ifdef MODULE
module_init(stifb_init);
#endif
module_exit(stifb_cleanup);

MODULE_AUTHOR("Helge Deller <deller@gmx.de>, Thomas Bogendoerfer <tsbogend@alpha.franken.de>");
MODULE_DESCRIPTION("Framebuffer driver for HP's NGLE series graphics cards in HP PARISC machines");
MODULE_LICENSE("GPL");

MODULE_PARM(bpp, "i");
MODULE_PARM_DESC(mem, "Bits per pixel (default: 8)");

