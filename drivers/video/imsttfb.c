/*
 *  drivers/video/imsttfb.c -- frame buffer device for IMS TwinTurbo
 *
 *  This file is derived from the powermac console "imstt" driver:
 *  Copyright (C) 1997 Sigurdur Asgeirsson
 *  With additional hacking by Jeffrey Kuskin (jsk@mojave.stanford.edu)
 *  Modified by Danilo Beuche 1998
 *  Some register values added by Damien Doligez, INRIA Rocquencourt
 *  Various cleanups by Paul Mundt (lethal@chaoticdreams.org)
 *
 *  This file was written by Ryan Nielsen (ran@krazynet.com)
 *  Most of the frame buffer device stuff was copied from atyfb.c
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
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#if defined(CONFIG_PPC)
#include <linux/nvram.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <video/macmodes.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#ifndef __powerpc__
#define eieio()		/* Enforce In-order Execution of I/O */
#endif

/* TwinTurbo (Cosmo) registers */
enum {
	S1SA	=  0, /* 0x00 */
	S2SA	=  1, /* 0x04 */
	SP	=  2, /* 0x08 */
	DSA	=  3, /* 0x0C */
	CNT	=  4, /* 0x10 */
	DP_OCTL	=  5, /* 0x14 */
	CLR	=  6, /* 0x18 */
	BI	=  8, /* 0x20 */
	MBC	=  9, /* 0x24 */
	BLTCTL	= 10, /* 0x28 */

	/* Scan Timing Generator Registers */
	HES	= 12, /* 0x30 */
	HEB	= 13, /* 0x34 */
	HSB	= 14, /* 0x38 */
	HT	= 15, /* 0x3C */
	VES	= 16, /* 0x40 */
	VEB	= 17, /* 0x44 */
	VSB	= 18, /* 0x48 */
	VT	= 19, /* 0x4C */
	HCIV	= 20, /* 0x50 */
	VCIV	= 21, /* 0x54 */
	TCDR	= 22, /* 0x58 */
	VIL	= 23, /* 0x5C */
	STGCTL	= 24, /* 0x60 */

	/* Screen Refresh Generator Registers */
	SSR	= 25, /* 0x64 */
	HRIR	= 26, /* 0x68 */
	SPR	= 27, /* 0x6C */
	CMR	= 28, /* 0x70 */
	SRGCTL	= 29, /* 0x74 */

	/* RAM Refresh Generator Registers */
	RRCIV	= 30, /* 0x78 */
	RRSC	= 31, /* 0x7C */
	RRCR	= 34, /* 0x88 */

	/* System Registers */
	GIOE	= 32, /* 0x80 */
	GIO	= 33, /* 0x84 */
	SCR	= 35, /* 0x8C */
	SSTATUS	= 36, /* 0x90 */
	PRC	= 37, /* 0x94 */

#if 0	
	/* PCI Registers */
	DVID	= 0x00000000L,
	SC	= 0x00000004L,
	CCR	= 0x00000008L,
	OG	= 0x0000000CL,
	BARM	= 0x00000010L,
	BARER	= 0x00000030L,
#endif
};

/* IBM 624 RAMDAC Direct Registers */
enum {
	PADDRW	= 0x00,
	PDATA	= 0x04,
	PPMASK	= 0x08,
	PADDRR	= 0x0c,
	PIDXLO	= 0x10,	
	PIDXHI	= 0x14,	
	PIDXDATA= 0x18,
	PIDXCTL	= 0x1c
};

/* IBM 624 RAMDAC Indirect Registers */
enum {
	CLKCTL		= 0x02,	/* (0x01) Miscellaneous Clock Control */
	SYNCCTL		= 0x03,	/* (0x00) Sync Control */
	HSYNCPOS	= 0x04,	/* (0x00) Horizontal Sync Position */
	PWRMNGMT	= 0x05,	/* (0x00) Power Management */
	DACOP		= 0x06,	/* (0x02) DAC Operation */
	PALETCTL	= 0x07,	/* (0x00) Palette Control */
	SYSCLKCTL	= 0x08,	/* (0x01) System Clock Control */
	PIXFMT		= 0x0a,	/* () Pixel Format  [bpp >> 3 + 2] */
	BPP8		= 0x0b,	/* () 8 Bits/Pixel Control */
	BPP16		= 0x0c, /* () 16 Bits/Pixel Control  [bit 1=1 for 565] */
	BPP24		= 0x0d,	/* () 24 Bits/Pixel Control */
	BPP32		= 0x0e,	/* () 32 Bits/Pixel Control */
	PIXCTL1		= 0x10, /* (0x05) Pixel PLL Control 1 */
	PIXCTL2		= 0x11,	/* (0x00) Pixel PLL Control 2 */
	SYSCLKN		= 0x15,	/* () System Clock N (System PLL Reference Divider) */
	SYSCLKM		= 0x16,	/* () System Clock M (System PLL VCO Divider) */
	SYSCLKP		= 0x17,	/* () System Clock P */
	SYSCLKC		= 0x18,	/* () System Clock C */
	/*
	 * Dot clock rate is 20MHz * (m + 1) / ((n + 1) * (p ? 2 * p : 1)
	 * c is charge pump bias which depends on the VCO frequency  
	 */
	PIXM0		= 0x20,	/* () Pixel M 0 */
	PIXN0		= 0x21,	/* () Pixel N 0 */
	PIXP0		= 0x22,	/* () Pixel P 0 */
	PIXC0		= 0x23,	/* () Pixel C 0 */
	CURSCTL		= 0x30,	/* (0x00) Cursor Control */
	CURSXLO		= 0x31,	/* () Cursor X position, low 8 bits */
	CURSXHI		= 0x32,	/* () Cursor X position, high 8 bits */
	CURSYLO		= 0x33,	/* () Cursor Y position, low 8 bits */
	CURSYHI		= 0x34,	/* () Cursor Y position, high 8 bits */
	CURSHOTX	= 0x35,	/* () Cursor Hot Spot X */
	CURSHOTY	= 0x36,	/* () Cursor Hot Spot Y */
	CURSACCTL	= 0x37,	/* () Advanced Cursor Control Enable */
	CURSACATTR	= 0x38,	/* () Advanced Cursor Attribute */
	CURS1R		= 0x40,	/* () Cursor 1 Red */
	CURS1G		= 0x41,	/* () Cursor 1 Green */
	CURS1B		= 0x42,	/* () Cursor 1 Blue */
	CURS2R		= 0x43,	/* () Cursor 2 Red */
	CURS2G		= 0x44,	/* () Cursor 2 Green */
	CURS2B		= 0x45,	/* () Cursor 2 Blue */
	CURS3R		= 0x46,	/* () Cursor 3 Red */
	CURS3G		= 0x47,	/* () Cursor 3 Green */
	CURS3B		= 0x48,	/* () Cursor 3 Blue */
	BORDR		= 0x60,	/* () Border Color Red */
	BORDG		= 0x61,	/* () Border Color Green */
	BORDB		= 0x62,	/* () Border Color Blue */
	MISCTL1		= 0x70,	/* (0x00) Miscellaneous Control 1 */
	MISCTL2		= 0x71,	/* (0x00) Miscellaneous Control 2 */
	MISCTL3		= 0x72,	/* (0x00) Miscellaneous Control 3 */
	KEYCTL		= 0x78	/* (0x00) Key Control/DB Operation */
};

/* TI TVP 3030 RAMDAC Direct Registers */
enum {
	TVPADDRW = 0x00,	/* 0  Palette/Cursor RAM Write Address/Index */
	TVPPDATA = 0x04,	/* 1  Palette Data RAM Data */
	TVPPMASK = 0x08,	/* 2  Pixel Read-Mask */
	TVPPADRR = 0x0c,	/* 3  Palette/Cursor RAM Read Address */
	TVPCADRW = 0x10,	/* 4  Cursor/Overscan Color Write Address */
	TVPCDATA = 0x14,	/* 5  Cursor/Overscan Color Data */
				/* 6  reserved */
	TVPCADRR = 0x1c,	/* 7  Cursor/Overscan Color Read Address */
				/* 8  reserved */
	TVPDCCTL = 0x24,	/* 9  Direct Cursor Control */
	TVPIDATA = 0x28,	/* 10 Index Data */
	TVPCRDAT = 0x2c,	/* 11 Cursor RAM Data */
	TVPCXPOL = 0x30,	/* 12 Cursor-Position X LSB */
	TVPCXPOH = 0x34,	/* 13 Cursor-Position X MSB */
	TVPCYPOL = 0x38,	/* 14 Cursor-Position Y LSB */
	TVPCYPOH = 0x3c,	/* 15 Cursor-Position Y MSB */
};

/* TI TVP 3030 RAMDAC Indirect Registers */
enum {
	TVPIRREV = 0x01,	/* Silicon Revision [RO] */
	TVPIRICC = 0x06,	/* Indirect Cursor Control 	(0x00) */
	TVPIRBRC = 0x07,	/* Byte Router Control 	(0xe4) */
	TVPIRLAC = 0x0f,	/* Latch Control 		(0x06) */
	TVPIRTCC = 0x18,	/* True Color Control  	(0x80) */
	TVPIRMXC = 0x19,	/* Multiplex Control		(0x98) */
	TVPIRCLS = 0x1a,	/* Clock Selection		(0x07) */
	TVPIRPPG = 0x1c,	/* Palette Page		(0x00) */
	TVPIRGEC = 0x1d,	/* General Control 		(0x00) */
	TVPIRMIC = 0x1e,	/* Miscellaneous Control	(0x00) */
	TVPIRPLA = 0x2c,	/* PLL Address */
	TVPIRPPD = 0x2d,	/* Pixel Clock PLL Data */
	TVPIRMPD = 0x2e,	/* Memory Clock PLL Data */
	TVPIRLPD = 0x2f,	/* Loop Clock PLL Data */
	TVPIRCKL = 0x30,	/* Color-Key Overlay Low */
	TVPIRCKH = 0x31,	/* Color-Key Overlay High */
	TVPIRCRL = 0x32,	/* Color-Key Red Low */
	TVPIRCRH = 0x33,	/* Color-Key Red High */
	TVPIRCGL = 0x34,	/* Color-Key Green Low */
	TVPIRCGH = 0x35,	/* Color-Key Green High */
	TVPIRCBL = 0x36,	/* Color-Key Blue Low */
	TVPIRCBH = 0x37,	/* Color-Key Blue High */
	TVPIRCKC = 0x38,	/* Color-Key Control 		(0x00) */
	TVPIRMLC = 0x39,	/* MCLK/Loop Clock Control	(0x18) */
	TVPIRSEN = 0x3a,	/* Sense Test			(0x00) */
	TVPIRTMD = 0x3b,	/* Test Mode Data */
	TVPIRRML = 0x3c,	/* CRC Remainder LSB [RO] */
	TVPIRRMM = 0x3d,	/* CRC Remainder MSB [RO] */
	TVPIRRMS = 0x3e,	/* CRC  Bit Select [WO] */
	TVPIRDID = 0x3f,	/* Device ID [RO] 		(0x30) */
	TVPIRRES = 0xff		/* Software Reset [WO] */
};

struct initvalues {
	__u8 addr, value;
};

static struct initvalues ibm_initregs[] __initdata = {
	{ CLKCTL,	0x21 },
	{ SYNCCTL,	0x00 },
	{ HSYNCPOS,	0x00 },
	{ PWRMNGMT,	0x00 },
	{ DACOP,	0x02 },
	{ PALETCTL,	0x00 },
	{ SYSCLKCTL,	0x01 },

	/*
	 * Note that colors in X are correct only if all video data is
	 * passed through the palette in the DAC.  That is, "indirect
	 * color" must be configured.  This is the case for the IBM DAC
	 * used in the 2MB and 4MB cards, at least.
	 */
	{ BPP8,		0x00 },
	{ BPP16,	0x01 },
	{ BPP24,	0x00 },
	{ BPP32,	0x00 },

	{ PIXCTL1,	0x05 },
	{ PIXCTL2,	0x00 },
	{ SYSCLKN,	0x08 },
	{ SYSCLKM,	0x4f },
	{ SYSCLKP,	0x00 },
	{ SYSCLKC,	0x00 },
	{ CURSCTL,	0x00 },
	{ CURSACCTL,	0x01 },
	{ CURSACATTR,	0xa8 },
	{ CURS1R,	0xff },
	{ CURS1G,	0xff },
	{ CURS1B,	0xff },
	{ CURS2R,	0xff },
	{ CURS2G,	0xff },
	{ CURS2B,	0xff },
	{ CURS3R,	0xff },
	{ CURS3G,	0xff },
	{ CURS3B,	0xff },
	{ BORDR,	0xff },
	{ BORDG,	0xff },
	{ BORDB,	0xff },
	{ MISCTL1,	0x01 },
	{ MISCTL2,	0x45 },
	{ MISCTL3,	0x00 },
	{ KEYCTL,	0x00 }
};

static struct initvalues tvp_initregs[] __initdata = {
	{ TVPIRICC,	0x00 },
	{ TVPIRBRC,	0xe4 },
	{ TVPIRLAC,	0x06 },
	{ TVPIRTCC,	0x80 },
	{ TVPIRMXC,	0x4d },
	{ TVPIRCLS,	0x05 },
	{ TVPIRPPG,	0x00 },
	{ TVPIRGEC,	0x00 },
	{ TVPIRMIC,	0x08 },
	{ TVPIRCKL,	0xff },
	{ TVPIRCKH,	0xff },
	{ TVPIRCRL,	0xff },
	{ TVPIRCRH,	0xff },
	{ TVPIRCGL,	0xff },
	{ TVPIRCGH,	0xff },
	{ TVPIRCBL,	0xff },
	{ TVPIRCBH,	0xff },
	{ TVPIRCKC,	0x00 },
	{ TVPIRPLA,	0x00 },
	{ TVPIRPPD,	0xc0 },
	{ TVPIRPPD,	0xd5 },
	{ TVPIRPPD,	0xea },
	{ TVPIRPLA,	0x00 },
	{ TVPIRMPD,	0xb9 },
	{ TVPIRMPD,	0x3a },
	{ TVPIRMPD,	0xb1 },
	{ TVPIRPLA,	0x00 },
	{ TVPIRLPD,	0xc1 },
	{ TVPIRLPD,	0x3d },
	{ TVPIRLPD,	0xf3 },
};

struct imstt_regvals {
	__u32 pitch;
	__u16 hes, heb, hsb, ht, ves, veb, vsb, vt, vil;
	__u8 pclk_m, pclk_n, pclk_p;
	/* Values of the tvp which change depending on colormode x resolution */
	__u8 mlc[3];	/* Memory Loop Config 0x39 */
	__u8 lckl_p[3];	/* P value of LCKL PLL */
};

struct imstt_cursor {
	struct timer_list timer;
	int enable;
	int on;
	int vbl_cnt;
	int blink_rate;
	__u16 x, y, width, height;
};

struct fb_info_imstt {
	struct fb_info info;
	struct fb_fix_screeninfo fix;
	struct display disp;
	struct display_switch dispsw;
	union {
#ifdef FBCON_HAS_CFB16
		__u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB24
		__u32 cfb24[16];
#endif
#ifdef FBCON_HAS_CFB32
		__u32 cfb32[16];
#endif
	} fbcon_cmap;
	struct {
		__u8 red, green, blue;
	} palette[256];
	struct imstt_regvals init;
	struct imstt_cursor cursor;
	unsigned long frame_buffer_phys;
	unsigned long board_size;
	__u8 *frame_buffer;
	unsigned long dc_regs_phys;
	__u32 *dc_regs;
	unsigned long cmap_regs_phys;
	__u8 *cmap_regs;
	__u32 total_vram;
	__u32 ramdac;
};

enum {
	IBM = 0,
	TVP = 1
};

#define INIT_BPP		8
#define INIT_XRES		640
#define INIT_YRES		480
#define CURSOR_BLINK_RATE	20
#define CURSOR_DRAW_DELAY	2

static int currcon = 0;
static int inverse = 0;
static char fontname[40] __initdata = { 0 };
static char curblink __initdata = 1;
static char noaccel __initdata = 0;
#if defined(CONFIG_PPC)
static signed char init_vmode __initdata = VMODE_NVRAM;
static signed char init_cmode __initdata = CMODE_NVRAM;
#endif

static struct imstt_regvals tvp_reg_init_2 = {
	512,
	0x0002, 0x0006, 0x0026, 0x0028, 0x0003, 0x0016, 0x0196, 0x0197, 0x0196,
	0xec, 0x2a, 0xf3,
	{ 0x3c, 0x3b, 0x39 }, { 0xf3, 0xf3, 0xf3 }
};

static struct imstt_regvals tvp_reg_init_6 = {
	640,
	0x0004, 0x0009, 0x0031, 0x0036, 0x0003, 0x002a, 0x020a, 0x020d, 0x020a,
	0xef, 0x2e, 0xb2,
	{ 0x39, 0x39, 0x38 }, { 0xf3, 0xf3, 0xf3 }
};

static struct imstt_regvals tvp_reg_init_12 = {
	800,
	0x0005, 0x000e, 0x0040, 0x0042, 0x0003, 0x018, 0x270, 0x271, 0x270,
	0xf6, 0x2e, 0xf2,
	{ 0x3a, 0x39, 0x38 }, { 0xf3, 0xf3, 0xf3 }
};

static struct imstt_regvals tvp_reg_init_13 = {
	832,
	0x0004, 0x0011, 0x0045, 0x0048, 0x0003, 0x002a, 0x029a, 0x029b, 0x0000,
	0xfe, 0x3e, 0xf1,
	{ 0x39, 0x38, 0x38 }, { 0xf3, 0xf3, 0xf2 }
};

static struct imstt_regvals tvp_reg_init_17 = {
	1024,
	0x0006, 0x0210, 0x0250, 0x0053, 0x1003, 0x0021, 0x0321, 0x0324, 0x0000,
	0xfc, 0x3a, 0xf1,
	{ 0x39, 0x38, 0x38 }, { 0xf3, 0xf3, 0xf2 }
};

static struct imstt_regvals tvp_reg_init_18 = {
	1152,
  	0x0009, 0x0011, 0x059, 0x5b, 0x0003, 0x0031, 0x0397, 0x039a, 0x0000, 
	0xfd, 0x3a, 0xf1,
	{ 0x39, 0x38, 0x38 }, { 0xf3, 0xf3, 0xf2 }
};

static struct imstt_regvals tvp_reg_init_19 = {
	1280,
	0x0009, 0x0016, 0x0066, 0x0069, 0x0003, 0x0027, 0x03e7, 0x03e8, 0x03e7,
	0xf7, 0x36, 0xf0,
	{ 0x38, 0x38, 0x38 }, { 0xf3, 0xf2, 0xf1 }
};

static struct imstt_regvals tvp_reg_init_20 = {
	1280,
	0x0009, 0x0018, 0x0068, 0x006a, 0x0003, 0x0029, 0x0429, 0x042a, 0x0000,
	0xf0, 0x2d, 0xf0,
	{ 0x38, 0x38, 0x38 }, { 0xf3, 0xf2, 0xf1 }
};

/*
 * PCI driver prototypes
 */
static int imsttfb_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void imsttfb_remove(struct pci_dev *pdev);

static __u32
getclkMHz (struct fb_info_imstt *p)
{
	__u32 clk_m, clk_n, clk_p;

	clk_m = p->init.pclk_m;
	clk_n = p->init.pclk_n;
	clk_p = p->init.pclk_p;

	return 20 * (clk_m + 1) / ((clk_n + 1) * (clk_p ? 2 * clk_p : 1));
}

static void
setclkMHz (struct fb_info_imstt *p, __u32 MHz)
{
	__u32 clk_m, clk_n, clk_p, x, stage, spilled;

	clk_m = clk_n = clk_p = 0;
	stage = spilled = 0;
	for (;;) {
		switch (stage) {
			case 0:
				clk_m++;
				break;
			case 1:
				clk_n++;
				break;
		}
		x = 20 * (clk_m + 1) / ((clk_n + 1) * (clk_p ? 2 * clk_p : 1));
		if (x == MHz)
			break;
		if (x > MHz) {
			spilled = 1;
			stage = 1;
		} else if (spilled && x < MHz) {
			stage = 0;
		}
	}

	p->init.pclk_m = clk_m;
	p->init.pclk_n = clk_n;
	p->init.pclk_p = clk_p;
}

static struct imstt_regvals *
compute_imstt_regvals_ibm (struct fb_info_imstt *p, int xres, int yres)
{
	struct imstt_regvals *init = &p->init;
	__u32 MHz, hes, heb, veb, htp, vtp;

	switch (xres) {
		case 640:
			hes = 0x0008; heb = 0x0012; veb = 0x002a; htp = 10; vtp = 2;
			MHz = 30 /* .25 */ ;
			break;
		case 832:
			hes = 0x0005; heb = 0x0020; veb = 0x0028; htp = 8; vtp = 3;
			MHz = 57 /* .27_ */ ;
			break;
		case 1024:
			hes = 0x000a; heb = 0x001c; veb = 0x0020; htp = 8; vtp = 3;
			MHz = 80;
			break;
		case 1152:
			hes = 0x0012; heb = 0x0022; veb = 0x0031; htp = 4; vtp = 3;
			MHz = 101 /* .6_ */ ;
			break;
		case 1280:
			hes = 0x0012; heb = 0x002f; veb = 0x0029; htp = 4; vtp = 1;
			MHz = yres == 960 ? 126 : 135;
			break;
		case 1600:
			hes = 0x0018; heb = 0x0040; veb = 0x002a; htp = 4; vtp = 3;
			MHz = 200;
			break;
		default:
			return 0;
	}

	setclkMHz(p, MHz);

	init->hes = hes;
	init->heb = heb;
	init->hsb = init->heb + (xres >> 3);
	init->ht = init->hsb + htp;
	init->ves = 0x0003;
	init->veb = veb;
	init->vsb = init->veb + yres;
	init->vt = init->vsb + vtp;
	init->vil = init->vsb;

	init->pitch = xres;

	return init;
}

static struct imstt_regvals *
compute_imstt_regvals_tvp (struct fb_info_imstt *p, int xres, int yres)
{
	struct imstt_regvals *init;

	switch (xres) {
		case 512:
			init = &tvp_reg_init_2;
			break;
		case 640:
			init = &tvp_reg_init_6;
			break;
		case 800:
			init = &tvp_reg_init_12;
			break;
		case 832:
			init = &tvp_reg_init_13;
			break;
		case 1024:
			init = &tvp_reg_init_17;
			break;
		case 1152:
			init = &tvp_reg_init_18;
			break;
		case 1280:
			init = yres == 960 ? &tvp_reg_init_19 : &tvp_reg_init_20;
			break;
		default:
			return 0;
	}
	p->init = *init;

	return init;
}

static struct imstt_regvals *
compute_imstt_regvals (struct fb_info_imstt *p, u_int xres, u_int yres)
{
	if (p->ramdac == IBM)
		return compute_imstt_regvals_ibm(p, xres, yres);
	else
		return compute_imstt_regvals_tvp(p, xres, yres);
}

static void
set_imstt_regvals_ibm (struct fb_info_imstt *p, u_int bpp)
{
	struct imstt_regvals *init = &p->init;
	__u8 pformat = (bpp >> 3) + 2;

	p->cmap_regs[PIDXHI] = 0;		eieio();
	p->cmap_regs[PIDXLO] = PIXM0;		eieio();
	p->cmap_regs[PIDXDATA] = init->pclk_m;	eieio();
	p->cmap_regs[PIDXLO] = PIXN0;		eieio();
	p->cmap_regs[PIDXDATA] = init->pclk_n;	eieio();
	p->cmap_regs[PIDXLO] = PIXP0;		eieio();
	p->cmap_regs[PIDXDATA] = init->pclk_p;	eieio();
	p->cmap_regs[PIDXLO] = PIXC0;		eieio();
	p->cmap_regs[PIDXDATA] = 0x02;		eieio();

	p->cmap_regs[PIDXLO] = PIXFMT;		eieio();
	p->cmap_regs[PIDXDATA] = pformat;	eieio();
}

static void
set_imstt_regvals_tvp (struct fb_info_imstt *p, u_int bpp)
{
	struct imstt_regvals *init = &p->init;
	__u8 tcc, mxc, lckl_n, mic;
	__u8 mlc, lckl_p;

	switch (bpp) {
		case 8:
			tcc = 0x80;
			mxc = 0x4d;
			lckl_n = 0xc1;
			mlc = init->mlc[0];
			lckl_p = init->lckl_p[0];
			break;
		case 16:
			tcc = 0x44;
			mxc = 0x55;
			lckl_n = 0xe1;
			mlc = init->mlc[1];
			lckl_p = init->lckl_p[1];
			break;
		case 24:
			tcc = 0x5e;
			mxc = 0x5d;
			lckl_n = 0xf1;
			mlc = init->mlc[2];
			lckl_p = init->lckl_p[2];
			break;
		case 32:
			tcc = 0x46;
			mxc = 0x5d;
			lckl_n = 0xf1;
			mlc = init->mlc[2];
			lckl_p = init->lckl_p[2];
			break;
	}
	mic = 0x08;

	p->cmap_regs[TVPADDRW] = TVPIRPLA;	eieio();
	p->cmap_regs[TVPIDATA] = 0x00;		eieio();
	p->cmap_regs[TVPADDRW] = TVPIRPPD;	eieio();
	p->cmap_regs[TVPIDATA] = init->pclk_m;	eieio();
	p->cmap_regs[TVPADDRW] = TVPIRPPD;	eieio();
	p->cmap_regs[TVPIDATA] = init->pclk_n;	eieio();
	p->cmap_regs[TVPADDRW] = TVPIRPPD;	eieio();
	p->cmap_regs[TVPIDATA] = init->pclk_p;	eieio();

	p->cmap_regs[TVPADDRW] = TVPIRTCC;	eieio();
	p->cmap_regs[TVPIDATA] = tcc;		eieio();
	p->cmap_regs[TVPADDRW] = TVPIRMXC;	eieio();
	p->cmap_regs[TVPIDATA] = mxc;		eieio();
	p->cmap_regs[TVPADDRW] = TVPIRMIC;	eieio();
	p->cmap_regs[TVPIDATA] = mic;		eieio();

	p->cmap_regs[TVPADDRW] = TVPIRPLA;	eieio();
	p->cmap_regs[TVPIDATA] = 0x00;		eieio();
	p->cmap_regs[TVPADDRW] = TVPIRLPD;	eieio();
	p->cmap_regs[TVPIDATA] = lckl_n;	eieio();

	p->cmap_regs[TVPADDRW] = TVPIRPLA;	eieio();
	p->cmap_regs[TVPIDATA] = 0x15;		eieio();
	p->cmap_regs[TVPADDRW] = TVPIRMLC;	eieio();
	p->cmap_regs[TVPIDATA] = mlc;		eieio();

	p->cmap_regs[TVPADDRW] = TVPIRPLA;	eieio();
	p->cmap_regs[TVPIDATA] = 0x2a;		eieio();
	p->cmap_regs[TVPADDRW] = TVPIRLPD;	eieio();
	p->cmap_regs[TVPIDATA] = lckl_p;	eieio();
}

static void
set_imstt_regvals (struct fb_info_imstt *p, u_int bpp)
{
	struct imstt_regvals *init = &p->init;
	__u32 ctl, pitch, byteswap, scr;

	if (p->ramdac == IBM)
		set_imstt_regvals_ibm(p, bpp);
	else
		set_imstt_regvals_tvp(p, bpp);

  /*
   * From what I (jsk) can gather poking around with MacsBug,
   * bits 8 and 9 in the SCR register control endianness
   * correction (byte swapping).  These bits must be set according
   * to the color depth as follows:
   *     Color depth    Bit 9   Bit 8
   *     ==========     =====   =====
   *        8bpp          0       0
   *       16bpp          0       1
   *       32bpp          1       1
   */
	switch (bpp) {
		case 8:
			ctl = 0x17b1;
			pitch = init->pitch >> 2;
			byteswap = 0x000;
			break;
		case 16:
			ctl = 0x17b3;
			pitch = init->pitch >> 1;
			byteswap = 0x100;
			break;
		case 24:
			ctl = 0x17b9;
			pitch = init->pitch - (p->init.pitch >> 2);
			byteswap = 0x200;
			break;
		case 32:
			ctl = 0x17b5;
			pitch = init->pitch;
			byteswap = 0x300;
			break;
	}
	if (p->ramdac == TVP)
		ctl -= 0x30;

	out_le32(&p->dc_regs[HES], init->hes);
	out_le32(&p->dc_regs[HEB], init->heb);
	out_le32(&p->dc_regs[HSB], init->hsb);
	out_le32(&p->dc_regs[HT], init->ht);
	out_le32(&p->dc_regs[VES], init->ves);
	out_le32(&p->dc_regs[VEB], init->veb);
	out_le32(&p->dc_regs[VSB], init->vsb);
	out_le32(&p->dc_regs[VT], init->vt);
	out_le32(&p->dc_regs[VIL], init->vil);
	out_le32(&p->dc_regs[HCIV], 1);
	out_le32(&p->dc_regs[VCIV], 1);
	out_le32(&p->dc_regs[TCDR], 4);
	out_le32(&p->dc_regs[RRCIV], 1);
	out_le32(&p->dc_regs[RRSC], 0x980);
	out_le32(&p->dc_regs[RRCR], 0x11);

	if (p->ramdac == IBM) {
		out_le32(&p->dc_regs[HRIR], 0x0100);
		out_le32(&p->dc_regs[CMR], 0x00ff);
		out_le32(&p->dc_regs[SRGCTL], 0x0073);
	} else {
		out_le32(&p->dc_regs[HRIR], 0x0200);
		out_le32(&p->dc_regs[CMR], 0x01ff);
		out_le32(&p->dc_regs[SRGCTL], 0x0003);
	}

	switch (p->total_vram) {
		case 0x200000:
			scr = 0x059d | byteswap;
			break;
		/* case 0x400000:
		   case 0x800000: */
		default:
			pitch >>= 1;
			scr = 0x150dd | byteswap;
			break;
	}

	out_le32(&p->dc_regs[SCR], scr);
	out_le32(&p->dc_regs[SPR], pitch);
	out_le32(&p->dc_regs[STGCTL], ctl);
}

static inline void
set_offset (struct display *disp, struct fb_info_imstt *p)
{
	__u32 off = disp->var.yoffset * (disp->line_length >> 3)
		    + ((disp->var.xoffset * (disp->var.bits_per_pixel >> 3)) >> 3);
	out_le32(&p->dc_regs[SSR], off);
}

static inline void
set_555 (struct fb_info_imstt *p)
{
	if (p->ramdac == IBM) {
		p->cmap_regs[PIDXHI] = 0;	eieio();
		p->cmap_regs[PIDXLO] = BPP16;	eieio();
		p->cmap_regs[PIDXDATA] = 0x01;	eieio();
	} else {
		p->cmap_regs[TVPADDRW] = TVPIRTCC;	eieio();
		p->cmap_regs[TVPIDATA] = 0x44;		eieio();
	}
}

static inline void
set_565 (struct fb_info_imstt *p)
{
	if (p->ramdac == IBM) {
		p->cmap_regs[PIDXHI] = 0;	eieio();
		p->cmap_regs[PIDXLO] = BPP16;	eieio();
		p->cmap_regs[PIDXDATA] = 0x03;	eieio();
	} else {
		p->cmap_regs[TVPADDRW] = TVPIRTCC;	eieio();
		p->cmap_regs[TVPIDATA] = 0x45;		eieio();
	}
}

static void
imstt_set_cursor (struct fb_info_imstt *p, int on)
{
	struct imstt_cursor *c = &p->cursor;

	if (p->ramdac == IBM) {
		p->cmap_regs[PIDXHI] = 0;	eieio();
		if (!on) {
			p->cmap_regs[PIDXLO] = CURSCTL;	eieio();
			p->cmap_regs[PIDXDATA] = 0x00;	eieio();
		} else {
			p->cmap_regs[PIDXLO] = CURSXHI;		eieio();
			p->cmap_regs[PIDXDATA] = c->x >> 8;	eieio();
			p->cmap_regs[PIDXLO] = CURSXLO;		eieio();
			p->cmap_regs[PIDXDATA] = c->x & 0xff;	eieio();
			p->cmap_regs[PIDXLO] = CURSYHI;		eieio();
			p->cmap_regs[PIDXDATA] = c->y >> 8;	eieio();
			p->cmap_regs[PIDXLO] = CURSYLO;		eieio();
			p->cmap_regs[PIDXDATA] = c->y & 0xff;	eieio();
			p->cmap_regs[PIDXLO] = CURSCTL;		eieio();
			p->cmap_regs[PIDXDATA] = 0x02;		eieio();
		}
	} else {
		if (!on) {
			p->cmap_regs[TVPADDRW] = TVPIRICC;	eieio();
			p->cmap_regs[TVPIDATA] = 0x00;		eieio();
		} else {
			__u16 x = c->x + 0x40, y = c->y + 0x40;

			p->cmap_regs[TVPCXPOH] = x >> 8;	eieio();
			p->cmap_regs[TVPCXPOL] = x & 0xff;	eieio();
			p->cmap_regs[TVPCYPOH] = y >> 8;	eieio();
			p->cmap_regs[TVPCYPOL] = y & 0xff;	eieio();
			p->cmap_regs[TVPADDRW] = TVPIRICC;	eieio();
			p->cmap_regs[TVPIDATA] = 0x02;		eieio();
		}
	}
}

static void
imsttfbcon_cursor (struct display *disp, int mode, int x, int y)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)disp->fb_info;
	struct imstt_cursor *c = &p->cursor;

	x *= fontwidth(disp);
	y *= fontheight(disp);

	if (c->x == x && c->y == y && (mode == CM_ERASE) == !c->enable)
		return;

	c->enable = 0;
	if (c->on)
		imstt_set_cursor(p, 0);
	c->x = x - disp->var.xoffset;
	c->y = y - disp->var.yoffset;

	switch (mode) {
		case CM_ERASE:
			c->on = 0;
			break;
		case CM_DRAW:
		case CM_MOVE:
			if (c->on)
				imstt_set_cursor(p, c->on);
			else
				c->vbl_cnt = CURSOR_DRAW_DELAY;
			c->enable = 1;
			break;
	}
}

static int
imsttfbcon_set_font (struct display *disp, int width, int height)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)disp->fb_info;
	struct imstt_cursor *c = &p->cursor;
	u_int x, y;
	__u8 fgc;

	if (width > 32 || height > 32)
		return -EINVAL;

	c->height = height;
	c->width = width;

	fgc = ~attr_bgcol_ec(disp, disp->conp);

	if (p->ramdac == IBM) {
		p->cmap_regs[PIDXHI] = 1;	eieio();
		for (x = 0; x < 0x100; x++) {
			p->cmap_regs[PIDXLO] = x;	eieio();
			p->cmap_regs[PIDXDATA] = 0x00;	eieio();
		}
		p->cmap_regs[PIDXHI] = 1;	eieio();
		for (y = 0; y < height; y++)
			for (x = 0; x < width >> 2; x++) {
				p->cmap_regs[PIDXLO] = x + y * 8;	eieio();
				p->cmap_regs[PIDXDATA] = 0xff;		eieio();
			}
		p->cmap_regs[PIDXHI] = 0;	eieio();
		p->cmap_regs[PIDXLO] = CURS1R;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS1G;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS1B;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS2R;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS2G;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS2B;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS3R;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS3G;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
		p->cmap_regs[PIDXLO] = CURS3B;	eieio();
		p->cmap_regs[PIDXDATA] = fgc;	eieio();
	} else {
		p->cmap_regs[TVPADDRW] = TVPIRICC;	eieio();
		p->cmap_regs[TVPIDATA] &= 0x03;		eieio();
		p->cmap_regs[TVPADDRW] = 0;		eieio();
		for (x = 0; x < 0x200; x++) {
			p->cmap_regs[TVPCRDAT] = 0x00;	eieio();
		}
		for (x = 0; x < 0x200; x++) {
			p->cmap_regs[TVPCRDAT] = 0xff;	eieio();
		}
		p->cmap_regs[TVPADDRW] = TVPIRICC;	eieio();
		p->cmap_regs[TVPIDATA] &= 0x03;		eieio();
		for (y = 0; y < height; y++)
			for (x = 0; x < width >> 3; x++) {
				p->cmap_regs[TVPADDRW] = x + y * 8;	eieio();
				p->cmap_regs[TVPCRDAT] = 0xff;		eieio();
			}
		p->cmap_regs[TVPADDRW] = TVPIRICC;	eieio();
		p->cmap_regs[TVPIDATA] |= 0x08;		eieio();
		for (y = 0; y < height; y++)
			for (x = 0; x < width >> 3; x++) {
				p->cmap_regs[TVPADDRW] = x + y * 8;	eieio();
				p->cmap_regs[TVPCRDAT] = 0xff;		eieio();
			}
		p->cmap_regs[TVPCADRW] = 0x00;	eieio();
		for (x = 0; x < 12; x++) {
			p->cmap_regs[TVPCDATA] = fgc;	eieio();
		}
	}

	return 1;
}

static void
imstt_cursor_timer_handler (unsigned long dev_addr)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)dev_addr;
	struct imstt_cursor *c = &p->cursor;

	if (!c->enable)
		goto out;

	if (c->vbl_cnt && --c->vbl_cnt == 0) {
		c->on ^= 1;
		imstt_set_cursor(p, c->on);
		c->vbl_cnt = c->blink_rate;
	}

out:
	c->timer.expires = jiffies + (HZ / 50);
	add_timer(&c->timer);
}

static void __init 
imstt_cursor_init (struct fb_info_imstt *p)
{
	struct imstt_cursor *c = &p->cursor;

	imsttfbcon_set_font(&p->disp, fontwidth(&p->disp), fontheight(&p->disp));

	c->enable = 1;
	c->on = 1;
	c->x = c->y = 0;
	c->blink_rate = 0;
	c->vbl_cnt = CURSOR_DRAW_DELAY;

	if (curblink) {
		c->blink_rate = CURSOR_BLINK_RATE;
		init_timer(&c->timer);
		c->timer.expires = jiffies + (HZ / 50);
		c->timer.data = (unsigned long)p;
		c->timer.function = imstt_cursor_timer_handler;
		add_timer(&c->timer);
	}
}

static void
imsttfbcon_bmove (struct display *disp, int sy, int sx, int dy, int dx, int height, int width)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)disp->fb_info;
	__u32	Bpp, line_pitch,
		fb_offset_old, fb_offset_new,
		sp, dp_octl, cnt, bltctl;

	Bpp = disp->var.bits_per_pixel >> 3,

	sy *= fontheight(disp);
	sx *= fontwidth(disp);
	sx *= Bpp;
	dy *= fontheight(disp);
	dx *= fontwidth(disp);
	dx *= Bpp;
	height *= fontheight(disp);
	height--;
	width *= fontwidth(disp);
	width *= Bpp;
	width--;

	line_pitch = disp->line_length;
	bltctl = 0x05;
	sp = line_pitch << 16;
	cnt = height << 16;

	if (sy < dy) {
		sy += height;
		dy += height;
		sp |= -(line_pitch) & 0xffff;
		dp_octl = -(line_pitch) & 0xffff;
	} else {
		sp |= line_pitch;
		dp_octl = line_pitch;
	}
	if (sx < dx) {
		sx += width;
		dx += width;
		bltctl |= 0x80;
		cnt |= -(width) & 0xffff;
	} else {
		cnt |= width;
	}
	fb_offset_old = sy * line_pitch + sx;
	fb_offset_new = dy * line_pitch + dx;

	while(in_le32(&p->dc_regs[SSTATUS]) & 0x80);
	out_le32(&p->dc_regs[S1SA], fb_offset_old);
	out_le32(&p->dc_regs[SP], sp);
	out_le32(&p->dc_regs[DSA], fb_offset_new);
	out_le32(&p->dc_regs[CNT], cnt);
	out_le32(&p->dc_regs[DP_OCTL], dp_octl);
	out_le32(&p->dc_regs[BLTCTL], bltctl);
	while(in_le32(&p->dc_regs[SSTATUS]) & 0x80);
	while(in_le32(&p->dc_regs[SSTATUS]) & 0x40);
}

static void
imsttfbcon_clear (struct vc_data *conp, struct display *disp,
		  int sy, int sx, int height, int width)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)disp->fb_info;
	__u32 Bpp, line_pitch, bgc;

	bgc = attr_bgcol_ec(disp, conp);
	bgc |= (bgc << 8);
	bgc |= (bgc << 16);

	Bpp = disp->var.bits_per_pixel >> 3,
	line_pitch = disp->line_length;

	sy *= fontheight(disp);
	sy *= line_pitch;
	sx *= fontwidth(disp);
	sx *= Bpp;
	height *= fontheight(disp);
	height--;
	width *= fontwidth(disp);
	width *= Bpp;
	width--;

	while(in_le32(&p->dc_regs[SSTATUS]) & 0x80);
	out_le32(&p->dc_regs[DSA], sy + sx);
	out_le32(&p->dc_regs[CNT], (height << 16) | width);
	out_le32(&p->dc_regs[DP_OCTL], line_pitch);
	out_le32(&p->dc_regs[BI], 0xffffffff);
	out_le32(&p->dc_regs[MBC], 0xffffffff);
	out_le32(&p->dc_regs[CLR], bgc);
	out_le32(&p->dc_regs[BLTCTL], 0x840); /* 0x200000 */
	while(in_le32(&p->dc_regs[SSTATUS]) & 0x80);
	while(in_le32(&p->dc_regs[SSTATUS]) & 0x40);
}

static void
imsttfbcon_revc (struct display *disp, int sx, int sy)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)disp->fb_info;
	__u32 Bpp, line_pitch, height, width;

	Bpp = disp->var.bits_per_pixel >> 3,
	line_pitch = disp->line_length;

	height = fontheight(disp);
	width = fontwidth(disp) * Bpp;
	sy *= height;
	sy *= line_pitch;
	sx *= width;
	height--;
	width--;

	while(in_le32(&p->dc_regs[SSTATUS]) & 0x80);
	out_le32(&p->dc_regs[DSA], sy + sx);
	out_le32(&p->dc_regs[S1SA], sy + sx);
	out_le32(&p->dc_regs[CNT], (height << 16) | width);
	out_le32(&p->dc_regs[DP_OCTL], line_pitch);
	out_le32(&p->dc_regs[SP], line_pitch);
	out_le32(&p->dc_regs[BLTCTL], 0x40005);
	while(in_le32(&p->dc_regs[SSTATUS]) & 0x80);
	while(in_le32(&p->dc_regs[SSTATUS]) & 0x40);
}

#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_imstt8 = {
	setup:		fbcon_cfb8_setup,
	bmove:		imsttfbcon_bmove,
	clear:		imsttfbcon_clear,
	putc:		fbcon_cfb8_putc,
	putcs:		fbcon_cfb8_putcs,
	revc:		imsttfbcon_revc,
	cursor:		imsttfbcon_cursor,
	set_font:	imsttfbcon_set_font,
	clear_margins:	fbcon_cfb8_clear_margins,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB16
static struct display_switch fbcon_imstt16 = {
	setup:		fbcon_cfb16_setup,
	bmove:		imsttfbcon_bmove,
	clear:		imsttfbcon_clear,
	putc:		fbcon_cfb16_putc,
	putcs:		fbcon_cfb16_putcs,
	revc:		imsttfbcon_revc,
	cursor:		imsttfbcon_cursor,
	set_font:	imsttfbcon_set_font,
	clear_margins:	fbcon_cfb16_clear_margins,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB24
static struct display_switch fbcon_imstt24 = {
	setup:		fbcon_cfb24_setup,
	bmove:		imsttfbcon_bmove,
	clear:		imsttfbcon_clear,
	putc:		fbcon_cfb24_putc,
	putcs:		fbcon_cfb24_putcs,
	revc:		imsttfbcon_revc,
	cursor:		imsttfbcon_cursor,
	set_font:	imsttfbcon_set_font,
	clear_margins:	fbcon_cfb24_clear_margins,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB32
static struct display_switch fbcon_imstt32 = {
	setup:		fbcon_cfb32_setup,
	bmove:		imsttfbcon_bmove,
	clear:		imsttfbcon_clear,
	putc:		fbcon_cfb32_putc,
	putcs:		fbcon_cfb32_putcs,
	revc:		imsttfbcon_revc,
	cursor:		imsttfbcon_cursor,
	set_font:	imsttfbcon_set_font,
	clear_margins:	fbcon_cfb32_clear_margins,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif

#ifdef CONFIG_FB_COMPAT_XPMAC
#include <asm/vc_ioctl.h>

extern struct vc_mode display_info;
extern struct fb_info *console_fb_info;

static void
set_display_info (struct display *disp)
{
	display_info.width = disp->var.xres;
	display_info.height = disp->var.yres;
	display_info.depth = disp->var.bits_per_pixel;
	display_info.pitch = disp->line_length;

	switch (disp->var.xres) {
		case 512:
			display_info.mode = 2;
			break;
		case 640:
			display_info.mode = 6;
			break;
		case 800:
			display_info.mode = 12;
			break;
		case 832:
			display_info.mode = 13;
			break;
		case 1024:
			display_info.mode = 17;
			break;
		case 1152:
			display_info.mode = 18;
			break;
		case 1280:
			display_info.mode = disp->var.yres == 960 ? 19 : 20;
			break;
		default:
			display_info.mode = 0;
	}
}
#endif

static int
imsttfb_getcolreg (u_int regno, u_int *red, u_int *green,
		   u_int *blue, u_int *transp, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;

	if (regno > 255)
		return 1;
	*red = (p->palette[regno].red << 8) | p->palette[regno].red;
	*green = (p->palette[regno].green << 8) | p->palette[regno].green;
	*blue = (p->palette[regno].blue << 8) | p->palette[regno].blue;
	*transp = 0;

	return 0;
}

static int
imsttfb_setcolreg (u_int regno, u_int red, u_int green, u_int blue,
		   u_int transp, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	u_int bpp = fb_display[currcon].var.bits_per_pixel;

	if (regno > 255)
		return 1;

	red >>= 8;
	green >>= 8;
	blue >>= 8;

	p->palette[regno].red = red;
	p->palette[regno].green = green;
	p->palette[regno].blue = blue;

	/* PADDRW/PDATA are the same as TVPPADDRW/TVPPDATA */
	if (0 && bpp == 16)	/* screws up X */
		p->cmap_regs[PADDRW] = regno << 3;
	else
		p->cmap_regs[PADDRW] = regno;
	eieio();

	p->cmap_regs[PDATA] = red;	eieio();
	p->cmap_regs[PDATA] = green;	eieio();
	p->cmap_regs[PDATA] = blue;	eieio();

	if (regno < 16)
		switch (bpp) {
#ifdef FBCON_HAS_CFB16
			case 16:
				p->fbcon_cmap.cfb16[regno] = (regno << (fb_display[currcon].var.green.length == 5 ? 10 : 11)) | (regno << 5) | regno;
				break;
#endif
#ifdef FBCON_HAS_CFB24
			case 24:
				p->fbcon_cmap.cfb24[regno] = (regno << 16) | (regno << 8) | regno;
				break;
#endif
#ifdef FBCON_HAS_CFB32
			case 32: {
				int i = (regno << 8) | regno;
				p->fbcon_cmap.cfb32[regno] = (i << 16) | i;
				break;
			}
#endif
		}

	return 0;
}

static void
do_install_cmap (int con, struct fb_info *info)
{
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, imsttfb_setcolreg, info);
	else {
		u_int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		fb_set_cmap(fb_default_cmap(size), 1, imsttfb_setcolreg, info);
	}
}

static int
imsttfb_get_fix (struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	struct fb_var_screeninfo *var = &fb_display[con].var;

	*fix = p->fix;
	fix->visual = var->bits_per_pixel == 8 ? FB_VISUAL_PSEUDOCOLOR
					       : FB_VISUAL_DIRECTCOLOR;
	fix->line_length = var->xres * (var->bits_per_pixel >> 3);

	return 0;
}

static int
imsttfb_get_var (struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	*var = fb_display[con].var;

	return 0;
}

static void
set_dispsw (struct display *disp, struct fb_info_imstt *p)
{
	u_int accel = disp->var.accel_flags & FB_ACCELF_TEXT;

	if (disp->conp && disp->conp->vc_sw && disp->conp->vc_sw->con_cursor)
		disp->conp->vc_sw->con_cursor(disp->conp, CM_ERASE);

	p->dispsw = fbcon_dummy;
	disp->dispsw = &p->dispsw;
	disp->dispsw_data = 0;
	switch (disp->var.bits_per_pixel) {
		case 8:
			disp->var.red.offset = 0;
			disp->var.red.length = 8;
			disp->var.green.offset = 0;
			disp->var.green.length = 8;
			disp->var.blue.offset = 0;
			disp->var.blue.length = 8;
			disp->var.transp.offset = 0;
			disp->var.transp.length = 0;
#ifdef FBCON_HAS_CFB8
			p->dispsw = accel ? fbcon_imstt8 : fbcon_cfb8;
#endif
			break;
		case 16:	/* RGB 555 or 565 */
			if (disp->var.green.length != 6)
				disp->var.red.offset = 10;
			disp->var.red.length = 5;
			disp->var.green.offset = 5;
			if (disp->var.green.length != 6)
				disp->var.green.length = 5;
			disp->var.blue.offset = 0;
			disp->var.blue.length = 5;
			disp->var.transp.offset = 0;
			disp->var.transp.length = 0;
#ifdef FBCON_HAS_CFB16
			p->dispsw = accel ? fbcon_imstt16 : fbcon_cfb16;
			disp->dispsw_data = p->fbcon_cmap.cfb16;
#endif
			break;
		case 24:	/* RGB 888 */
			disp->var.red.offset = 16;
			disp->var.red.length = 8;
			disp->var.green.offset = 8;
			disp->var.green.length = 8;
			disp->var.blue.offset = 0;
			disp->var.blue.length = 8;
			disp->var.transp.offset = 0;
			disp->var.transp.length = 0;
#ifdef FBCON_HAS_CFB24
			p->dispsw = accel ? fbcon_imstt24 : fbcon_cfb24;
			disp->dispsw_data = p->fbcon_cmap.cfb24;
#endif
			break;
		case 32:	/* RGBA 8888 */
			disp->var.red.offset = 16;
			disp->var.red.length = 8;
			disp->var.green.offset = 8;
			disp->var.green.length = 8;
			disp->var.blue.offset = 0;
			disp->var.blue.length = 8;
			disp->var.transp.offset = 24;
			disp->var.transp.length = 8;
#ifdef FBCON_HAS_CFB32
			p->dispsw = accel ? fbcon_imstt32 : fbcon_cfb32;
			disp->dispsw_data = p->fbcon_cmap.cfb32;
#endif
			break;
	}

	if (accel && p->ramdac != IBM) {
		p->dispsw.cursor = 0;
		p->dispsw.set_font = 0;
	}

#ifdef CONFIG_FB_COMPAT_XPMAC
	set_display_info(disp);
#endif
}

static void
set_disp (struct display *disp, struct fb_info_imstt *p)
{
	u_int accel = disp->var.accel_flags & FB_ACCELF_TEXT;

	disp->fb_info = &p->info;

	set_dispsw(disp, p);

	disp->visual = disp->var.bits_per_pixel == 8 ? FB_VISUAL_PSEUDOCOLOR
					 	     : FB_VISUAL_DIRECTCOLOR;
	disp->screen_base = (__u8 *)p->frame_buffer;
	disp->visual = p->fix.visual;
	disp->type = p->fix.type;
	disp->type_aux = p->fix.type_aux;
	disp->line_length = disp->var.xres * (disp->var.bits_per_pixel >> 3);
	disp->can_soft_blank = 1;
	disp->inverse = inverse;
	disp->ypanstep = 1;
	disp->ywrapstep = 0;
	if (accel) {
		disp->scrollmode = SCROLL_YNOMOVE;
		if (disp->var.yres == disp->var.yres_virtual) {
			__u32 vram = (p->total_vram - (PAGE_SIZE << 2));
			disp->var.yres_virtual = ((vram << 3) / disp->var.bits_per_pixel) / disp->var.xres_virtual;
			if (disp->var.yres_virtual < disp->var.yres)
				disp->var.yres_virtual = disp->var.yres;
		}
	} else {
		disp->scrollmode = SCROLL_YREDRAW;
	}

	disp->var.activate = 0;
	disp->var.red.msb_right = 0;
	disp->var.green.msb_right = 0;
	disp->var.blue.msb_right = 0;
	disp->var.transp.msb_right = 0;
	disp->var.height = -1;
	disp->var.width = -1;
	disp->var.vmode = FB_VMODE_NONINTERLACED;
	disp->var.left_margin = disp->var.right_margin = 16;
	disp->var.upper_margin = disp->var.lower_margin = 16;
	disp->var.hsync_len = disp->var.vsync_len = 8;
}

static int
imsttfb_set_var (struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	struct display *disp;
	u_int oldbpp, oldxres, oldyres, oldgreenlen, oldaccel;

	disp = &fb_display[con];

	if ((var->bits_per_pixel != 8 && var->bits_per_pixel != 16
	    && var->bits_per_pixel != 24 && var->bits_per_pixel != 32)
	    || var->xres_virtual < var->xres || var->yres_virtual < var->yres
	    || var->nonstd
	    || (var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED)
		return -EINVAL;

	if ((var->xres * var->yres) * (var->bits_per_pixel >> 3) > p->total_vram
	    || (var->xres_virtual * var->yres_virtual) * (var->bits_per_pixel >> 3) > p->total_vram)
		return -EINVAL;

	if (!((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW))
		return 0;

	if (!compute_imstt_regvals(p, var->xres, var->yres))
		return -EINVAL;

	oldbpp = disp->var.bits_per_pixel;
	oldxres = disp->var.xres;
	oldyres = disp->var.yres;
	oldgreenlen = disp->var.green.length;
	oldaccel = disp->var.accel_flags;

	disp->var.bits_per_pixel = var->bits_per_pixel;
	disp->var.xres = var->xres;
	disp->var.yres = var->yres;
	disp->var.xres_virtual = var->xres_virtual;
	disp->var.yres_virtual = var->yres_virtual;
	disp->var.green.length = var->green.length;
	disp->var.accel_flags = var->accel_flags;

	set_disp(disp, p);

	if (info->changevar)
		(*info->changevar)(con);

	if (con == currcon) {
		if (oldgreenlen != disp->var.green.length) {
			if (disp->var.green.length == 6)
				set_565(p);
			else
				set_555(p);
		}
		if (oldxres != disp->var.xres || oldyres != disp->var.yres || oldbpp != disp->var.bits_per_pixel)
			set_imstt_regvals(p, disp->var.bits_per_pixel);
			
	}
	disp->var.pixclock = 1000000 / getclkMHz(p);

	if (oldbpp != disp->var.bits_per_pixel) {
		int err = fb_alloc_cmap(&disp->cmap, 0, 0);
		if (err)
			return err;
		do_install_cmap(con, info);
	}
	*var = disp->var;

	return 0;
}

static int
imsttfb_pan_display (struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	struct display *disp = &fb_display[con];

	if (var->xoffset + disp->var.xres > disp->var.xres_virtual
	    || var->yoffset + disp->var.yres > disp->var.yres_virtual)
		return -EINVAL;

	disp->var.xoffset = var->xoffset;
	disp->var.yoffset = var->yoffset;
	if (con == currcon)
		set_offset(disp, p);

	return 0;
}

static int
imsttfb_get_cmap (struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	if (con == currcon)	/* current console? */
		return fb_get_cmap(cmap, kspc, imsttfb_getcolreg, info);
	else if (fb_display[con].cmap.len)	/* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else {
		u_int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		fb_copy_cmap(fb_default_cmap(size), cmap, kspc ? 0 : 2);
	}

	return 0;
}

static int
imsttfb_set_cmap (struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
		if ((err = fb_alloc_cmap(&fb_display[con].cmap, size, 0)))
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, imsttfb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	return 0;
}

#define FBIMSTT_SETREG		0x545401
#define FBIMSTT_GETREG		0x545402
#define FBIMSTT_SETCMAPREG	0x545403
#define FBIMSTT_GETCMAPREG	0x545404
#define FBIMSTT_SETIDXREG	0x545405
#define FBIMSTT_GETIDXREG	0x545406

static int
imsttfb_ioctl (struct inode *inode, struct file *file, u_int cmd,
	       u_long arg, int con, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	__u8 idx[2];
	__u32 reg[2];

	switch (cmd) {
		case FBIMSTT_SETREG:
			if (copy_from_user(reg, (void *)arg, 8) || reg[0] > (0x1000 - sizeof(reg[0])) / sizeof(reg[0]))
				return -EFAULT;
			out_le32(&p->dc_regs[reg[0]], reg[1]);
			return 0;
		case FBIMSTT_GETREG:
			if (copy_from_user(reg, (void *)arg, 4) || reg[0] > (0x1000 - sizeof(reg[0])) / sizeof(reg[0]))
				return -EFAULT;
			reg[1] = in_le32(&p->dc_regs[reg[0]]);
			if (copy_to_user((void *)(arg + 4), &reg[1], 4))
				return -EFAULT;
			return 0;
		case FBIMSTT_SETCMAPREG:
			if (copy_from_user(reg, (void *)arg, 8) || reg[0] > (0x1000 - sizeof(reg[0])) / sizeof(reg[0]))
				return -EFAULT;
			out_le32(&((u_int *)p->cmap_regs)[reg[0]], reg[1]);
			return 0;
		case FBIMSTT_GETCMAPREG:
			if (copy_from_user(reg, (void *)arg, 4) || reg[0] > (0x1000 - sizeof(reg[0])) / sizeof(reg[0]))
				return -EFAULT;
			reg[1] = in_le32(&((u_int *)p->cmap_regs)[reg[0]]);
			if (copy_to_user((void *)(arg + 4), &reg[1], 4))
				return -EFAULT;
			return 0;
		case FBIMSTT_SETIDXREG:
			if (copy_from_user(idx, (void *)arg, 2))
				return -EFAULT;
			p->cmap_regs[PIDXHI] = 0;		eieio();
			p->cmap_regs[PIDXLO] = idx[0];		eieio();
			p->cmap_regs[PIDXDATA] = idx[1];	eieio();
			return 0;
		case FBIMSTT_GETIDXREG:
			if (copy_from_user(idx, (void *)arg, 1))
				return -EFAULT;
			p->cmap_regs[PIDXHI] = 0;		eieio();
			p->cmap_regs[PIDXLO] = idx[0];		eieio();
			idx[1] = p->cmap_regs[PIDXDATA];
			if (copy_to_user((void *)(arg + 1), &idx[1], 1))
				return -EFAULT;
			return 0;
		default:
			return -ENOIOCTLCMD;
	}
}

static struct pci_device_id imsttfb_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_IMS, PCI_DEVICE_ID_IMS_TT128,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, IBM },
	{ PCI_VENDOR_ID_IMS, PCI_DEVICE_ID_IMS_TT3D,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, TVP },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, imsttfb_pci_tbl);

static struct pci_driver imsttfb_pci_driver = {
	name:		"imsttfb",
	id_table:	imsttfb_pci_tbl,
	probe:		imsttfb_probe,
	remove:		__devexit_p(imsttfb_remove),
};

static struct fb_ops imsttfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	imsttfb_get_fix,
	fb_get_var:	imsttfb_get_var,
	fb_set_var:	imsttfb_set_var,
	fb_get_cmap:	imsttfb_get_cmap,
	fb_set_cmap:	imsttfb_set_cmap,
	fb_pan_display:	imsttfb_pan_display,
	fb_ioctl:	imsttfb_ioctl,
};

static int
imsttfbcon_switch (int con, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	struct display *old = &fb_display[currcon], *new = &fb_display[con];

	if (old->cmap.len)
		fb_get_cmap(&old->cmap, 1, imsttfb_getcolreg, info);

	if (old->conp && old->conp->vc_sw && old->conp->vc_sw->con_cursor)
		old->conp->vc_sw->con_cursor(old->conp, CM_ERASE);

	currcon = con;

	if (old->var.xres != new->var.xres
	    || old->var.yres != new->var.yres
	    || old->var.bits_per_pixel != new->var.bits_per_pixel
	    || old->var.green.length != new->var.green.length
	    || old->var.accel_flags != new->var.accel_flags) {
		set_dispsw(new, p);
		if (!compute_imstt_regvals(p, new->var.xres, new->var.yres))
			return -1;
		if (new->var.bits_per_pixel == 16) {
			if (new->var.green.length == 6)
				set_565(p);
			else
				set_555(p);
		}
		set_imstt_regvals(p, new->var.bits_per_pixel);
	}
	set_offset(new, p);

	imsttfbcon_set_font(new, fontwidth(new), fontheight(new));

	do_install_cmap(con, info);

	return 0;
}

static int
imsttfbcon_updatevar (int con, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	struct display *disp = &fb_display[con];

	if (con != currcon)
		goto out;

	if (p->ramdac == IBM)
		imsttfbcon_cursor(disp, CM_ERASE, p->cursor.x, p->cursor.y);

	set_offset(disp, p);

out:
	return 0;
}

static void
imsttfbcon_blank (int blank, struct fb_info *info)
{
	struct fb_info_imstt *p = (struct fb_info_imstt *)info;
	__u32 ctrl;

	ctrl = in_le32(&p->dc_regs[STGCTL]);
	if (blank > 0) {
		switch (blank - 1) {
			case VESA_NO_BLANKING:
			case VESA_POWERDOWN:
				ctrl &= ~0x00000380;
				if (p->ramdac == IBM) {
					p->cmap_regs[PIDXHI] = 0;	eieio();
					p->cmap_regs[PIDXLO] = MISCTL2;	eieio();
					p->cmap_regs[PIDXDATA] = 0x55;	eieio();
					p->cmap_regs[PIDXLO] = MISCTL1;	eieio();
					p->cmap_regs[PIDXDATA] = 0x11;	eieio();
					p->cmap_regs[PIDXLO] = SYNCCTL;	eieio();
					p->cmap_regs[PIDXDATA] = 0x0f;	eieio();
					p->cmap_regs[PIDXLO] = PWRMNGMT;eieio();
					p->cmap_regs[PIDXDATA] = 0x1f;	eieio();
					p->cmap_regs[PIDXLO] = CLKCTL;	eieio();
					p->cmap_regs[PIDXDATA] = 0xc0;
				}
				break;
			case VESA_VSYNC_SUSPEND:
				ctrl &= ~0x00000020;
				break;
			case VESA_HSYNC_SUSPEND:
				ctrl &= ~0x00000010;
				break;
		}
	} else {
		if (p->ramdac == IBM) {
			ctrl |= 0x000017b0;
			p->cmap_regs[PIDXHI] = 0;	eieio();
			p->cmap_regs[PIDXLO] = CLKCTL;	eieio();
			p->cmap_regs[PIDXDATA] = 0x01;	eieio();
			p->cmap_regs[PIDXLO] = PWRMNGMT;eieio();
			p->cmap_regs[PIDXDATA] = 0x00;	eieio();
			p->cmap_regs[PIDXLO] = SYNCCTL;	eieio();
			p->cmap_regs[PIDXDATA] = 0x00;	eieio();
			p->cmap_regs[PIDXLO] = MISCTL1;	eieio();
			p->cmap_regs[PIDXDATA] = 0x01;	eieio();
			p->cmap_regs[PIDXLO] = MISCTL2;	eieio();
			p->cmap_regs[PIDXDATA] = 0x45;	eieio();
		} else
			ctrl |= 0x00001780;
	}
	out_le32(&p->dc_regs[STGCTL], ctrl);
}

static void __init 
init_imstt(struct fb_info_imstt *p)
{
	__u32 i, tmp;
	__u32 *ip, *end;

	tmp = in_le32(&p->dc_regs[PRC]);
	if (p->ramdac == IBM)
		p->total_vram = (tmp & 0x0004) ? 0x400000 : 0x200000;
	else
		p->total_vram = 0x800000;

	ip = (__u32 *)p->frame_buffer;
	end = (__u32 *)(p->frame_buffer + p->total_vram);
	while (ip < end)
		*ip++ = 0;

	/* initialize the card */
	tmp = in_le32(&p->dc_regs[STGCTL]);
	out_le32(&p->dc_regs[STGCTL], tmp & ~0x1);
	out_le32(&p->dc_regs[SSR], 0);

	/* set default values for DAC registers */ 
	if (p->ramdac == IBM) {
		p->cmap_regs[PPMASK] = 0xff;	eieio();
		p->cmap_regs[PIDXHI] = 0;	eieio();
		for (i = 0; i < sizeof(ibm_initregs) / sizeof(*ibm_initregs); i++) {
			p->cmap_regs[PIDXLO] = ibm_initregs[i].addr;	eieio();
			p->cmap_regs[PIDXDATA] = ibm_initregs[i].value;	eieio();
		}
	} else {
		for (i = 0; i < sizeof(tvp_initregs) / sizeof(*tvp_initregs); i++) {
			p->cmap_regs[TVPADDRW] = tvp_initregs[i].addr;	eieio();
			p->cmap_regs[TVPIDATA] = tvp_initregs[i].value;	eieio();
		}
	}

#ifdef CONFIG_ALL_PPC
	{
		int vmode = init_vmode, cmode = init_cmode;

#ifdef CONFIG_NVRAM
		/* Attempt to read vmode/cmode from NVRAM */
		if (vmode == VMODE_NVRAM)
			vmode = nvram_read_byte(NV_VMODE);
		if (cmode == CMODE_NVRAM)
			cmode = nvram_read_byte(NV_CMODE);
#endif
		/* If we didn't get something from NVRAM, pick a
		 * sane default.
		 */
		if (vmode <= 0 || vmode > VMODE_MAX)
			vmode = VMODE_640_480_67;
		if (cmode < CMODE_8 || cmode > CMODE_32)
			cmode = CMODE_8;

		if (mac_vmode_to_var(vmode, cmode, &p->disp.var)) {
			p->disp.var.xres = p->disp.var.xres_virtual = INIT_XRES;
			p->disp.var.yres = p->disp.var.yres_virtual = INIT_YRES;
			p->disp.var.bits_per_pixel = INIT_BPP;
		}
	}
#else
	p->disp.var.xres = p->disp.var.xres_virtual = INIT_XRES;
	p->disp.var.yres = p->disp.var.yres_virtual = INIT_YRES;
	p->disp.var.bits_per_pixel = INIT_BPP;
#endif

	if ((p->disp.var.xres * p->disp.var.yres) * (p->disp.var.bits_per_pixel >> 3) > p->total_vram
	    || !(compute_imstt_regvals(p, p->disp.var.xres, p->disp.var.yres))) {
		printk("imsttfb: %ux%ux%u not supported\n", p->disp.var.xres, p->disp.var.yres, p->disp.var.bits_per_pixel);
		kfree(p);
		return;
	}

	sprintf(p->fix.id, "IMS TT (%s)", p->ramdac == IBM ? "IBM" : "TVP");
	p->fix.smem_start = p->frame_buffer_phys;
	p->fix.smem_len = p->total_vram;
	p->fix.mmio_start = p->dc_regs_phys;
	p->fix.mmio_len = 0x1000;
	p->fix.accel = FB_ACCEL_IMS_TWINTURBO;
	p->fix.type = FB_TYPE_PACKED_PIXELS;
	p->fix.visual = p->disp.var.bits_per_pixel == 8 ? FB_VISUAL_PSEUDOCOLOR
							: FB_VISUAL_DIRECTCOLOR;
	p->fix.line_length = p->disp.var.xres * (p->disp.var.bits_per_pixel >> 3);
	p->fix.xpanstep = 8;
	p->fix.ypanstep = 1;
	p->fix.ywrapstep = 0;

	p->disp.var.accel_flags = noaccel ? 0 : FB_ACCELF_TEXT;
	set_disp(&p->disp, p);

	if (!noaccel && p->ramdac == IBM)
		imstt_cursor_init(p);
	if (p->disp.var.green.length == 6)
		set_565(p);
	else
		set_555(p);
	set_imstt_regvals(p, p->disp.var.bits_per_pixel);

	p->disp.var.pixclock = 1000000 / getclkMHz(p);

	strcpy(p->info.modename, p->fix.id);
	strcpy(p->info.fontname, fontname);
	p->info.node = -1;
	p->info.fbops = &imsttfb_ops;
	p->info.disp = &p->disp;
	p->info.changevar = 0;
	p->info.switch_con = &imsttfbcon_switch;
	p->info.updatevar = &imsttfbcon_updatevar;
	p->info.blank = &imsttfbcon_blank;
	p->info.flags = FBINFO_FLAG_DEFAULT;

	for (i = 0; i < 16; i++) {
		u_int j = color_table[i];
		p->palette[i].red = default_red[j];
		p->palette[i].green = default_grn[j];
		p->palette[i].blue = default_blu[j];
	}

	if (register_framebuffer(&p->info) < 0) {
		kfree(p);
		return;
	}

	i = GET_FB_IDX(p->info.node);
	tmp = (in_le32(&p->dc_regs[SSTATUS]) & 0x0f00) >> 8;
	printk("fb%u: %s frame buffer; %uMB vram; chip version %u\n",
		i, p->fix.id, p->total_vram >> 20, tmp);

#ifdef CONFIG_FB_COMPAT_XPMAC
	strncpy(display_info.name, "IMS,tt128mb", sizeof(display_info.name));
	display_info.fb_address = p->frame_buffer_phys;
	display_info.cmap_adr_address = p->cmap_regs_phys + PADDRW;
	display_info.cmap_data_address = p->cmap_regs_phys + PDATA;
	display_info.disp_reg_address = p->dc_regs_phys;
	if (!console_fb_info)
		console_fb_info = &p->info;
#endif /* CONFIG_FB_COMPAT_XPMAC */
}

static int __devinit
imsttfb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct fb_info_imstt *p;
	unsigned long addr, size;

	addr = pci_resource_start (pdev, 0);
	size = pci_resource_len (pdev, 0);

	p = kmalloc(sizeof(struct fb_info_imstt), GFP_KERNEL);

	if (!p) {
		printk(KERN_ERR "imsttfb: Can't allocate memory\n");
		return -ENOMEM;
	}

	memset(p, 0, sizeof(struct fb_info_imstt));

	if (!request_mem_region(addr, size, "imsttfb")) {
		printk(KERN_ERR "imsttfb: Can't reserve memory region\n");
		kfree(p);
		return -ENODEV;
	}

	switch (pdev->device) {
		case PCI_DEVICE_ID_IMS_TT128: /* IMS,tt128mbA */
			p->ramdac = IBM;
			break;
		case PCI_DEVICE_ID_IMS_TT3D:  /* IMS,tt3d */
			p->ramdac = TVP;
			break;
		default:
			printk(KERN_INFO "imsttfb: Device 0x%lx unknown, "
					 "contact maintainer.\n", pdev->device);
			return -ENODEV;
	}

	p->frame_buffer_phys = addr;
	p->board_size = size;
	p->frame_buffer = (__u8 *)ioremap(addr, p->ramdac == IBM ? 0x400000 : 0x800000);
	p->dc_regs_phys = addr + 0x800000;
	p->dc_regs = (__u32 *)ioremap(addr + 0x800000, 0x1000);
	p->cmap_regs_phys = addr + 0x840000;
	p->cmap_regs = (__u8 *)ioremap(addr + 0x840000, 0x1000);

	init_imstt(p);

	pci_set_drvdata(pdev, p);

	return 0;
}

static void __devexit
imsttfb_remove(struct pci_dev *pdev)
{
	struct fb_info_imstt *p = pci_get_drvdata(pdev);

	unregister_framebuffer(&p->info);
	iounmap(p->cmap_regs);
	iounmap(p->dc_regs);
	iounmap(p->frame_buffer);
	release_mem_region(p->frame_buffer_phys, p->board_size);
	kfree(p);
}

#ifndef MODULE
int __init 
imsttfb_setup(char *options)
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
		} else if (!strncmp(this_opt, "noblink", 7)) {
			curblink = 0;
		} else if (!strncmp(this_opt, "noaccel", 7)) {
			noaccel = 1;
		} else if (!strncmp(this_opt, "inverse", 7)) {
			inverse = 1;
			fb_invert_cmaps();
		}
#if defined(CONFIG_PPC)
		else if (!strncmp(this_opt, "vmode:", 6)) {
			int vmode = simple_strtoul(this_opt+6, NULL, 0);
			if (vmode > 0 && vmode <= VMODE_MAX)
				init_vmode = vmode;
		} else if (!strncmp(this_opt, "cmode:", 6)) {
			int cmode = simple_strtoul(this_opt+6, NULL, 0);
			switch (cmode) {
				case CMODE_8:
				case 8:
					init_cmode = CMODE_8;
					break;
				case CMODE_16:
				case 15:
				case 16:
					init_cmode = CMODE_16;
					break;
				case CMODE_32:
				case 24:
				case 32:
					init_cmode = CMODE_32;
					break;
			}
		}
#endif
	}
	return 0;
}

#endif /* MODULE */

int __init imsttfb_init(void)
{
	return pci_module_init(&imsttfb_pci_driver);
}
 
static void __exit imsttfb_exit(void)
{
	pci_unregister_driver(&imsttfb_pci_driver);
}

#ifdef MODULE
MODULE_LICENSE("GPL");
module_init(imsttfb_init);
#endif
module_exit(imsttfb_exit);

