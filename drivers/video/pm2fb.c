/*
 * Permedia2 framebuffer driver.
 * Copyright (c) 1998-1999 Ilario Nardinocchi (nardinoc@CS.UniBO.IT)
 * Copyright (c) 1999 Jakub Jelinek (jakub@redhat.com)
 * Based on linux/drivers/video/skeletonfb.c by Geert Uytterhoeven.
 * --------------------------------------------------------------------------
 * $Id: pm2fb.c,v 1.163 1999/02/21 14:06:49 illo Exp $
 * --------------------------------------------------------------------------
 * TODO multiple boards support
 * --------------------------------------------------------------------------
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
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
#include <linux/console.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>
#include "pm2fb.h"
#include "cvisionppc.h"
#ifdef __sparc__
#include <asm/pbm.h>
#include <asm/fbio.h>
#endif

#if !defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
#error	"The endianness of the target host has not been defined."
#endif

#if defined(__BIG_ENDIAN) && !defined(__sparc__)
#define PM2FB_BE_APERTURE
#endif

/* Need to debug this some more */
#undef PM2FB_HW_CURSOR

#if defined(CONFIG_FB_PM2_PCI) && !defined(CONFIG_PCI)
#undef CONFIG_FB_PM2_PCI
#warning "support for Permedia2 PCI boards with no generic PCI support!"
#endif

#undef PM2FB_MASTER_DEBUG
#ifdef PM2FB_MASTER_DEBUG
#define DPRINTK(a,b...)	printk(KERN_DEBUG "pm2fb: %s: " a, __FUNCTION__ , ## b)
#else
#define DPRINTK(a,b...)
#endif 

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

/*
 * The _DEFINITIVE_ memory mapping/unmapping functions.
 * This is due to the fact that they're changing soooo often...
 */
#define MMAP(a,b)	ioremap((unsigned long )(a), b)
#define UNMAP(a,b)	iounmap(a)

/*
 * The _DEFINITIVE_ memory i/o barrier functions.
 * This is due to the fact that they're changing soooo often...
 */
#define DEFW()		wmb()
#define DEFR()		rmb()
#define DEFRW()		mb()

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

struct pm2fb_par {
	u32 pixclock;		/* pixclock in KHz */
	u32 width;		/* width of virtual screen */
	u32 height;		/* height of virtual screen */
	u32 hsstart;		/* horiz. sync start */
	u32 hsend;		/* horiz. sync end */
	u32 hbend;		/* horiz. blank end (also gate end) */
	u32 htotal;		/* total width (w/ sync & blank) */
	u32 vsstart;		/* vert. sync start */
	u32 vsend;		/* vert. sync end */
	u32 vbend;		/* vert. blank end */
	u32 vtotal;		/* total height (w/ sync & blank) */
	u32 stride;		/* screen stride */
	u32 base;		/* screen base (xoffset+yoffset) */
	u32 depth;		/* screen depth (8, 16, 24 or 32) */
	u32 video;		/* video control (hsync,vsync) */
};

#define OPTF_OLD_MEM		(1L<<0)
#define OPTF_YPAN		(1L<<1)
#define OPTF_VIRTUAL		(1L<<2)
#define OPTF_USER		(1L<<3)
static struct {
	char font[40];
	u32 flags;
	struct pm2fb_par user_mode;
} pm2fb_options =
#ifdef __sparc__
	/* For some reason Raptor is not happy with the low-end mode */
	{"\0", 0L, {31499,640,480,4,20,50,209,0,3,20,499,80,0,8,121}};
#else
	{"\0", 0L, {25174,640,480,4,28,40,199,9,11,45,524,80,0,8,121}};
#endif

static char curblink __initdata = 1;

static struct {
	char name[16];
	struct pm2fb_par par;
} user_mode[] __initdata = {
	{"640x480-60",
		{25174,640,480,4,28,40,199,9,11,45,524,80,0,8,121}},
	{"640x480-72",
		{31199,640,480,6,16,48,207,8,10,39,518,80,0,8,121}},
	{"640x480-75",
		{31499,640,480,4,20,50,209,0,3,20,499,80,0,8,121}},
	{"640x480-90",
		{39909,640,480,8,18,48,207,24,38,53,532,80,0,8,121}},
	{"640x480-100",
		{44899,640,480,8,40,52,211,21,33,51,530,80,0,8,121}},
	{"800x600-56",
		{35999,800,600,6,24,56,255,0,2,25,624,100,0,8,41}},
	{"800x600-60",
		{40000,800,600,10,42,64,263,0,4,28,627,100,0,8,41}},
	{"800x600-70",
		{44899,800,600,6,42,52,251,8,20,36,635,100,0,8,105}},
	{"800x600-72",
		{50000,800,600,14,44,60,259,36,42,66,665,100,0,8,41}},
	{"800x600-75",
		{49497,800,600,4,24,64,263,0,3,25,624,100,0,8,41}},
	{"800x600-90",
		{56637,800,600,2,18,48,247,7,18,35,634,100,0,8,41}},
	{"800x600-100",
		{67499,800,600,0,16,70,269,6,10,25,624,100,0,8,41}},
	{"1024x768-60",
		{64998,1024,768,6,40,80,335,2,8,38,805,128,0,8,121}},
	{"1024x768-70",
		{74996,1024,768,6,40,76,331,2,8,38,805,128,0,8,121}},
	{"1024x768-72",
		{74996,1024,768,6,40,66,321,2,8,38,805,128,0,8,121}},
	{"1024x768-75",
		{78932,1024,768,4,28,72,327,0,3,32,799,128,0,8,41}},
	{"1024x768-90",
		{100000,1024,768,0,24,72,327,20,35,77,844,128,0,8,121}},
	{"1024x768-100",
		{109998,1024,768,0,22,92,347,0,7,24,791,128,0,8,121}},
	{"1024x768-illo",
		{120322,1024,768,12,48,120,375,3,7,32,799,128,0,8,41}},
	{"1152x864-60",
		{80000,1152,864,16,44,76,363,5,10,52,915,144,0,8,41}},
	{"1152x864-70",
		{100000,1152,864,10,48,90,377,12,23,81,944,144,0,8,41}},
	{"1152x864-75",
		{109998,1152,864,6,42,78,365,44,52,138,1001,144,0,8,41}},
	{"1152x864-80",
		{109998,1152,864,4,32,72,359,29,36,94,957,144,0,8,41}},
	{"1280x1024-60",
		{107991,1280,1024,12,40,102,421,0,3,42,1065,160,0,8,41}},
	{"1280x1024-70",
		{125992,1280,1024,20,48,102,421,0,5,42,1065,160,0,8,41}},
	{"1280x1024-74",
		{134989,1280,1024,8,44,108,427,0,29,40,1063,160,0,8,41}},
	{"1280x1024-75",
		{134989,1280,1024,4,40,102,421,0,3,42,1065,160,0,8,41}},
	{"1600x1200-60",
		{155981,1600,1200,8,48,112,511,9,17,70,1269,200,0,8,121}},
	{"1600x1200-66",
		{171998,1600,1200,10,44,120,519,2,5,53,1252,200,0,8,121}},
	{"1600x1200-76",
		{197980,1600,1200,10,44,120,519,2,7,50,1249,200,0,8,121}},
	{"\0", },
};

#ifdef CONFIG_FB_PM2_PCI
struct pm2pci_par {
	u32 mem_config;
	u32 mem_control;
	u32 boot_address;
	struct pci_dev* dev;
};
#endif

#define DEFAULT_CURSOR_BLINK_RATE       (20)
#define CURSOR_DRAW_DELAY               (2)

struct pm2_cursor {
    int	enable;
    int on;
    int vbl_cnt;
    int blink_rate;
    struct {
        u16 x, y;
    } pos, hot, size;
    u8 color[6];
    u8 bits[8][64];
    u8 mask[8][64];
    struct timer_list *timer;
};

static const char permedia2_name[16]="Permedia2";

static struct pm2fb_info {
	struct fb_info_gen gen;
	int board;			/* Permedia2 board index (see
					   board_table[] below) */
	pm2type_t type;
	struct {
		unsigned long  fb_base;	/* physical framebuffer memory base */
		u32 fb_size;		/* framebuffer memory size */
		unsigned long  rg_base;	/* physical register memory base */
		unsigned long  p_fb;	/* physical address of frame buffer */
		unsigned char* v_fb;	/* virtual address of frame buffer */
		unsigned long  p_regs;	/* physical address of registers
					   region, must be rg_base or
					   rg_base+PM2_REGS_SIZE depending on
					   the host endianness */
		unsigned char* v_regs;	/* virtual address of p_regs */
	} regions;
	union {				/* here, the per-board par structs */
#ifdef CONFIG_FB_PM2_CVPPC
		struct cvppc_par cvppc;	/* CVisionPPC data */
#endif
#ifdef CONFIG_FB_PM2_PCI
		struct pm2pci_par pci;	/* Permedia2 PCI boards data */
#endif
	} board_par;
	struct pm2fb_par current_par;	/* displayed screen */
	int current_par_valid;
	u32 memclock;			/* memclock (set by the per-board
					   		init routine) */
	struct display disp;
	struct {
		u8 transp;
		u8 red;
		u8 green;
		u8 blue;
	} palette[256];
	union {
#ifdef FBCON_HAS_CFB16
		u16 cmap16[16];
#endif
#ifdef FBCON_HAS_CFB24
		u32 cmap24[16];
#endif
#ifdef FBCON_HAS_CFB32
		u32 cmap32[16];
#endif
	} cmap;
	struct pm2_cursor *cursor;
} fb_info;

#ifdef CONFIG_FB_PM2_CVPPC
static int cvppc_detect(struct pm2fb_info*);
static void cvppc_init(struct pm2fb_info*);
#endif

#ifdef CONFIG_FB_PM2_PCI
static int pm2pci_detect(struct pm2fb_info*);
static void pm2pci_init(struct pm2fb_info*);
#endif

#ifdef PM2FB_HW_CURSOR
static void pm2fb_cursor(struct display *p, int mode, int x, int y);
static int pm2fb_set_font(struct display *d, int width, int height);
static struct pm2_cursor *pm2_init_cursor(struct pm2fb_info *fb);
static void pm2v_set_cursor_color(struct pm2fb_info *fb, u8 *red, u8 *green, u8 *blue);
static void pm2v_set_cursor_shape(struct pm2fb_info *fb);
static u8 cursor_color_map[2] = { 0, 0xff };
#else
#define pm2fb_cursor NULL
#define pm2fb_set_font NULL
#endif

/*
 * Table of the supported Permedia2 based boards.
 * Three hooks are defined for each board:
 * detect(): should return 1 if the related board has been detected, 0
 *           otherwise. It should also fill the fields 'regions.fb_base',
 *           'regions.fb_size', 'regions.rg_base' and 'memclock' in the
 *           passed pm2fb_info structure.
 * init(): called immediately after the reset of the Permedia2 chip.
 *         It should reset the memory controller if needed (the MClk
 *         is set shortly afterwards by the caller).
 * cleanup(): called after the driver has been unregistered.
 *
 * the init and cleanup pointers can be NULL.
 */
static const struct {
	int (*detect)(struct pm2fb_info*);
	void (*init)(struct pm2fb_info*);
	void (*cleanup)(struct pm2fb_info*);
	char name[32];
} board_table[] = {
#ifdef CONFIG_FB_PM2_PCI
	{ pm2pci_detect, pm2pci_init, NULL, "Permedia2 PCI board" },
#endif
#ifdef CONFIG_FB_PM2_CVPPC
	{ cvppc_detect, cvppc_init, NULL, "CVisionPPC/BVisionPPC" },
#endif
	{ NULL, }
};

/*
 * partial products for the supported horizontal resolutions.
 */
#define PACKPP(p0,p1,p2)	(((p2)<<6)|((p1)<<3)|(p0))
static const struct {
	u16 width;
	u16 pp;
} pp_table[] = {
	{ 32,	PACKPP(1, 0, 0) }, { 64,	PACKPP(1, 1, 0) },
	{ 96,	PACKPP(1, 1, 1) }, { 128,	PACKPP(2, 1, 1) },
	{ 160,	PACKPP(2, 2, 1) }, { 192,	PACKPP(2, 2, 2) },
	{ 224,	PACKPP(3, 2, 1) }, { 256,	PACKPP(3, 2, 2) },
	{ 288,	PACKPP(3, 3, 1) }, { 320,	PACKPP(3, 3, 2) },
	{ 384,	PACKPP(3, 3, 3) }, { 416,	PACKPP(4, 3, 1) },
	{ 448,	PACKPP(4, 3, 2) }, { 512,	PACKPP(4, 3, 3) },
	{ 544,	PACKPP(4, 4, 1) }, { 576,	PACKPP(4, 4, 2) },
	{ 640,	PACKPP(4, 4, 3) }, { 768,	PACKPP(4, 4, 4) },
	{ 800,	PACKPP(5, 4, 1) }, { 832,	PACKPP(5, 4, 2) },
	{ 896,	PACKPP(5, 4, 3) }, { 1024,	PACKPP(5, 4, 4) },
	{ 1056,	PACKPP(5, 5, 1) }, { 1088,	PACKPP(5, 5, 2) },
	{ 1152,	PACKPP(5, 5, 3) }, { 1280,	PACKPP(5, 5, 4) },
	{ 1536,	PACKPP(5, 5, 5) }, { 1568,	PACKPP(6, 5, 1) },
	{ 1600,	PACKPP(6, 5, 2) }, { 1664,	PACKPP(6, 5, 3) },
	{ 1792,	PACKPP(6, 5, 4) }, { 2048,	PACKPP(6, 5, 5) },
	{ 0,	0 } };

static void pm2fb_detect(void);
static int pm2fb_encode_fix(struct fb_fix_screeninfo* fix,
				const void* par, struct fb_info_gen* info);
static int pm2fb_decode_var(const struct fb_var_screeninfo* var,
					void* par, struct fb_info_gen* info);
static int pm2fb_encode_var(struct fb_var_screeninfo* var,
				const void* par, struct fb_info_gen* info);
static void pm2fb_get_par(void* par, struct fb_info_gen* info);
static void pm2fb_set_par(const void* par, struct fb_info_gen* info);
static int pm2fb_getcolreg(unsigned regno,
			unsigned* red, unsigned* green, unsigned* blue,
				unsigned* transp, struct fb_info* info);
static int pm2fb_setcolreg(unsigned regno,
			unsigned red, unsigned green, unsigned blue,
				unsigned transp, struct fb_info* info);
static int pm2fb_blank(int blank_mode, struct fb_info_gen* info);
static int pm2fb_pan_display(const struct fb_var_screeninfo* var,
					struct fb_info_gen* info);
static void pm2fb_set_disp(const void* par, struct display* disp,
						struct fb_info_gen* info);

static struct fbgen_hwswitch pm2fb_hwswitch={
	pm2fb_detect, pm2fb_encode_fix, pm2fb_decode_var,
	pm2fb_encode_var, pm2fb_get_par, pm2fb_set_par,
	pm2fb_getcolreg, pm2fb_setcolreg, pm2fb_pan_display,
	pm2fb_blank, pm2fb_set_disp
};

static struct fb_ops pm2fb_ops={
	owner:		THIS_MODULE,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
};

/***************************************************************************
 * Begin of Permedia2 specific functions
 ***************************************************************************/

inline static u32 RD32(unsigned char* base, s32 off) {

	return readl(base+off);
}

inline static void WR32(unsigned char* base, s32 off, u32 v) {

	writel(v, base+off);
}

inline static u32 pm2_RD(struct pm2fb_info* p, s32 off) {

	return RD32(p->regions.v_regs, off);
}

inline static void pm2_WR(struct pm2fb_info* p, s32 off, u32 v) {

	WR32(p->regions.v_regs, off, v);
}

inline static u32 pm2_RDAC_RD(struct pm2fb_info* p, s32 idx) {

	int index = PM2R_RD_INDEXED_DATA;
	switch (p->type) {
	case PM2_TYPE_PERMEDIA2:
		pm2_WR(p, PM2R_RD_PALETTE_WRITE_ADDRESS, idx);
		break;
	case PM2_TYPE_PERMEDIA2V:
		pm2_WR(p, PM2VR_RD_INDEX_LOW, idx & 0xff);
		index = PM2VR_RD_INDEXED_DATA;
		break;
	}	
	DEFRW();
	return pm2_RD(p, index);
}

inline static void pm2_RDAC_WR(struct pm2fb_info* p, s32 idx,
						u32 v) {

	int index = PM2R_RD_INDEXED_DATA;
	switch (p->type) {
	case PM2_TYPE_PERMEDIA2:
		pm2_WR(p, PM2R_RD_PALETTE_WRITE_ADDRESS, idx);
		break;
	case PM2_TYPE_PERMEDIA2V:
		pm2_WR(p, PM2VR_RD_INDEX_LOW, idx & 0xff);
		index = PM2VR_RD_INDEXED_DATA;
		break;
	}	
	DEFRW();
	pm2_WR(p, index, v);
}

inline static u32 pm2v_RDAC_RD(struct pm2fb_info* p, s32 idx) {

	pm2_WR(p, PM2VR_RD_INDEX_LOW, idx & 0xff);
	DEFRW();
	return pm2_RD(p, PM2VR_RD_INDEXED_DATA);
}

inline static void pm2v_RDAC_WR(struct pm2fb_info* p, s32 idx,
						u32 v) {

	pm2_WR(p, PM2VR_RD_INDEX_LOW, idx & 0xff);
	DEFRW();
	pm2_WR(p, PM2VR_RD_INDEXED_DATA, v);
}

#ifdef CONFIG_FB_PM2_FIFO_DISCONNECT
#define WAIT_FIFO(p,a)
#else
inline static void WAIT_FIFO(struct pm2fb_info* p, u32 a) {

	while(pm2_RD(p, PM2R_IN_FIFO_SPACE)<a);
	DEFRW();
}
#endif

static u32 partprod(u32 xres) {
	int i;

	for (i=0; pp_table[i].width && pp_table[i].width!=xres; i++);
	if (!pp_table[i].width)
		DPRINTK("invalid width %u\n", xres);
	return pp_table[i].pp;
}

static u32 to3264(u32 timing, int bpp, int is64) {

	switch (bpp) {
		case 8:
			timing=timing>>(2+is64);
			break;
		case 16:
			timing=timing>>(1+is64);
			break;
		case 24:
			timing=(timing*3)>>(2+is64);
			break;
		case 32:
			if (is64)
				timing=timing>>1;
			break;
	}
	return timing;
}

static u32 from3264(u32 timing, int bpp, int is64) {

	switch (bpp) {
		case 8:
			timing=timing<<(2+is64);
			break;
		case 16:
			timing=timing<<(1+is64);
			break;
		case 24:
			timing=(timing<<(2+is64))/3;
			break;
		case 32:
			if (is64)
				timing=timing<<1;
			break;
	}
	return timing;
}

static void pm2_mnp(u32 clk, unsigned char* mm, unsigned char* nn,
		unsigned char* pp) {
	unsigned char m;
	unsigned char n;
	unsigned char p;
	u32 f;
	s32 curr;
	s32 delta=100000;

	*mm=*nn=*pp=0;
	for (n=2; n<15; n++) {
		for (m=2; m; m++) {
			f=PM2_REFERENCE_CLOCK*m/n;
			if (f>=150000 && f<=300000) {
				for (p=0; p<5; p++, f>>=1) {
					curr=clk>f?clk-f:f-clk;
					if (curr<delta) {
						delta=curr;
						*mm=m;
						*nn=n;
						*pp=p;
					}
				}
			}
		}
	}
}

static void pm2v_mnp(u32 clk, unsigned char* mm, unsigned char* nn,
		unsigned char* pp) {
	unsigned char m;
	unsigned char n;
	unsigned char p;
	u32 f;
	s32 delta=1000;

	*mm=*nn=*pp=0;
	for (n=1; n; n++) {
		for (m=1; m; m++) {
			for (p=0; p<2; p++) {
				f=PM2_REFERENCE_CLOCK*n/(m * (1<<(p+1)));
				if (clk>f-delta && clk<f+delta) {
					delta=clk>f?clk-f:f-clk;
					*mm=m;
					*nn=n;
					*pp=p;
				}
			}
		}
	}
}

static void wait_pm2(struct pm2fb_info* i) {

	WAIT_FIFO(i, 1);
	pm2_WR(i, PM2R_SYNC, 0);
	DEFRW();
	do {
		while (pm2_RD(i, PM2R_OUT_FIFO_WORDS)==0);
		DEFR();
	} while (pm2_RD(i, PM2R_OUT_FIFO)!=PM2TAG(PM2R_SYNC));
}

static void pm2_set_memclock(struct pm2fb_info* info, u32 clk) {
	int i;
	unsigned char m, n, p;

	pm2_mnp(clk, &m, &n, &p);
	WAIT_FIFO(info, 10);
	pm2_RDAC_WR(info, PM2I_RD_MEMORY_CLOCK_3, 6);
	DEFW();
	pm2_RDAC_WR(info, PM2I_RD_MEMORY_CLOCK_1, m);
	pm2_RDAC_WR(info, PM2I_RD_MEMORY_CLOCK_2, n);
	DEFW();
	pm2_RDAC_WR(info, PM2I_RD_MEMORY_CLOCK_3, 8|p);
	DEFW();
	pm2_RDAC_RD(info, PM2I_RD_MEMORY_CLOCK_STATUS);
	DEFR();
	for (i=256; i &&
		!(pm2_RD(info, PM2R_RD_INDEXED_DATA)&PM2F_PLL_LOCKED); i--);
}

static void pm2_set_pixclock(struct pm2fb_info* info, u32 clk) {
	int i;
	unsigned char m, n, p;

	switch (info->type) {
	case PM2_TYPE_PERMEDIA2:
		pm2_mnp(clk, &m, &n, &p);
		WAIT_FIFO(info, 10);
		pm2_RDAC_WR(info, PM2I_RD_PIXEL_CLOCK_A3, 0);
		DEFW();
		pm2_RDAC_WR(info, PM2I_RD_PIXEL_CLOCK_A1, m);
		pm2_RDAC_WR(info, PM2I_RD_PIXEL_CLOCK_A2, n);
		DEFW();
		pm2_RDAC_WR(info, PM2I_RD_PIXEL_CLOCK_A3, 8|p);
		DEFW();
		pm2_RDAC_RD(info, PM2I_RD_PIXEL_CLOCK_STATUS);
		DEFR();
		for (i=256; i &&
		     !(pm2_RD(info, PM2R_RD_INDEXED_DATA)&PM2F_PLL_LOCKED); i--);
		break;
	case PM2_TYPE_PERMEDIA2V:
		pm2v_mnp(clk/2, &m, &n, &p);
		WAIT_FIFO(info, 8);
		pm2_WR(info, PM2VR_RD_INDEX_HIGH, PM2VI_RD_CLK0_PRESCALE >> 8);
		pm2v_RDAC_WR(info, PM2VI_RD_CLK0_PRESCALE, m);
		pm2v_RDAC_WR(info, PM2VI_RD_CLK0_FEEDBACK, n);
		pm2v_RDAC_WR(info, PM2VI_RD_CLK0_POSTSCALE, p);
		pm2_WR(info, PM2VR_RD_INDEX_HIGH, 0);
		break;
	}
}

static void clear_palette(struct pm2fb_info* p) {
	int i=256;

	WAIT_FIFO(p, 1);
	pm2_WR(p, PM2R_RD_PALETTE_WRITE_ADDRESS, 0);
	DEFW();
	while (i--) {
		WAIT_FIFO(p, 3);
		pm2_WR(p, PM2R_RD_PALETTE_DATA, 0);
		pm2_WR(p, PM2R_RD_PALETTE_DATA, 0);
		pm2_WR(p, PM2R_RD_PALETTE_DATA, 0);
	}
}

static void set_color(struct pm2fb_info* p, unsigned char regno,
			unsigned char r, unsigned char g, unsigned char b) {

	WAIT_FIFO(p, 4);
	pm2_WR(p, PM2R_RD_PALETTE_WRITE_ADDRESS, regno);
	DEFW();
	pm2_WR(p, PM2R_RD_PALETTE_DATA, r);
	DEFW();
	pm2_WR(p, PM2R_RD_PALETTE_DATA, g);
	DEFW();
	pm2_WR(p, PM2R_RD_PALETTE_DATA, b);
}

static void set_aperture(struct pm2fb_info* i, struct pm2fb_par* p) {

	WAIT_FIFO(i, 2);
#ifdef __LITTLE_ENDIAN
	pm2_WR(i, PM2R_APERTURE_ONE, 0);
	pm2_WR(i, PM2R_APERTURE_TWO, 0);
#else
	switch (p->depth) {
		case 8:
		case 24:
			pm2_WR(i, PM2R_APERTURE_ONE, 0);
			pm2_WR(i, PM2R_APERTURE_TWO, 1);
			break;
		case 16:
			pm2_WR(i, PM2R_APERTURE_ONE, 2);
			pm2_WR(i, PM2R_APERTURE_TWO, 1);
			break;
		case 32:
			pm2_WR(i, PM2R_APERTURE_ONE, 1);
			pm2_WR(i, PM2R_APERTURE_TWO, 1);
			break;
	}
#endif
}

static void set_screen(struct pm2fb_info* i, struct pm2fb_par* p) {
	u32 clrmode=0;
	u32 txtmap=0;
	u32 pixsize=0;
	u32 clrformat=0;
	u32 xres;
	u32 video, tmp;

	if (i->type == PM2_TYPE_PERMEDIA2V) {
		WAIT_FIFO(i, 1);
		pm2_WR(i, PM2VR_RD_INDEX_HIGH, 0);
	}
	xres=(p->width+31)&~31;
	set_aperture(i, p);
	DEFRW();
	WAIT_FIFO(i, 27);
	pm2_RDAC_WR(i, PM2I_RD_COLOR_KEY_CONTROL, p->depth==8?0:
						PM2F_COLOR_KEY_TEST_OFF);
	switch (p->depth) {
		case 8:
			pm2_WR(i, PM2R_FB_READ_PIXEL, 0);
			clrformat=0x0e;
			break;
		case 16:
			pm2_WR(i, PM2R_FB_READ_PIXEL, 1);
			clrmode=PM2F_RD_TRUECOLOR|0x06;
			txtmap=PM2F_TEXTEL_SIZE_16;
			pixsize=1;
			clrformat=0x70;
			break;
		case 32:
			pm2_WR(i, PM2R_FB_READ_PIXEL, 2);
			clrmode=PM2F_RD_TRUECOLOR|0x08;
			txtmap=PM2F_TEXTEL_SIZE_32;
			pixsize=2;
			clrformat=0x20;
			break;
		case 24:
			pm2_WR(i, PM2R_FB_READ_PIXEL, 4);
			clrmode=PM2F_RD_TRUECOLOR|0x09;
			txtmap=PM2F_TEXTEL_SIZE_24;
			pixsize=4;
			clrformat=0x20;
			break;
	}
	pm2_WR(i, PM2R_SCREEN_SIZE, (p->height<<16)|p->width);
	pm2_WR(i, PM2R_SCISSOR_MODE, PM2F_SCREEN_SCISSOR_ENABLE);
	pm2_WR(i, PM2R_FB_WRITE_MODE, PM2F_FB_WRITE_ENABLE);
	pm2_WR(i, PM2R_FB_READ_MODE, partprod(xres));
	pm2_WR(i, PM2R_LB_READ_MODE, partprod(xres));
	pm2_WR(i, PM2R_TEXTURE_MAP_FORMAT, txtmap|partprod(xres));
	pm2_WR(i, PM2R_H_TOTAL, p->htotal);
	pm2_WR(i, PM2R_HS_START, p->hsstart);
	pm2_WR(i, PM2R_HS_END, p->hsend);
	pm2_WR(i, PM2R_HG_END, p->hbend);
	pm2_WR(i, PM2R_HB_END, p->hbend);
	pm2_WR(i, PM2R_V_TOTAL, p->vtotal);
	pm2_WR(i, PM2R_VS_START, p->vsstart);
	pm2_WR(i, PM2R_VS_END, p->vsend);
	pm2_WR(i, PM2R_VB_END, p->vbend);
	pm2_WR(i, PM2R_SCREEN_STRIDE, p->stride);
	DEFW();
	pm2_WR(i, PM2R_SCREEN_BASE, p->base);
	/* HW cursor needs /VSYNC for recognizing vert retrace */
	video=p->video & ~(PM2F_HSYNC_ACT_LOW|PM2F_VSYNC_ACT_LOW);
	video|=PM2F_HSYNC_ACT_HIGH|PM2F_VSYNC_ACT_HIGH;
	switch (i->type) {
	case PM2_TYPE_PERMEDIA2:
		tmp = PM2F_RD_PALETTE_WIDTH_8;
		pm2_RDAC_WR(i, PM2I_RD_COLOR_MODE, PM2F_RD_COLOR_MODE_RGB|
						   PM2F_RD_GUI_ACTIVE|clrmode);
		if ((p->video & PM2F_HSYNC_ACT_LOW) == PM2F_HSYNC_ACT_LOW)
			tmp |= 4; /* invert hsync */
		if ((p->video & PM2F_HSYNC_ACT_LOW) == PM2F_HSYNC_ACT_LOW)
			tmp |= 8; /* invert vsync */
		pm2_RDAC_WR(i, PM2I_RD_MISC_CONTROL, tmp);
		break;
	case PM2_TYPE_PERMEDIA2V:
		tmp = 0;
		pm2v_RDAC_WR(i, PM2VI_RD_PIXEL_SIZE, pixsize);
		pm2v_RDAC_WR(i, PM2VI_RD_COLOR_FORMAT, clrformat);
		if ((p->video & PM2F_HSYNC_ACT_LOW) == PM2F_HSYNC_ACT_LOW)
			tmp |= 1; /* invert hsync */
		if ((p->video & PM2F_HSYNC_ACT_LOW) == PM2F_HSYNC_ACT_LOW)
			tmp |= 4; /* invert vsync */
		pm2v_RDAC_WR(i, PM2VI_RD_SYNC_CONTROL, tmp);
		pm2v_RDAC_WR(i, PM2VI_RD_MISC_CONTROL, 1);
		break;
	}
	pm2_WR(i, PM2R_VIDEO_CONTROL, video);
	pm2_set_pixclock(i, p->pixclock);
};

/*
 * copy with packed pixels (8/16bpp only).
 */
static void pm2fb_pp_copy(struct pm2fb_info* i, s32 xsrc, s32 ysrc,
					s32 x, s32 y, s32 w, s32 h) {
	s32 scale=i->current_par.depth==8?2:1;
	s32 offset;

	if (!w || !h)
		return;
	WAIT_FIFO(i, 7);
	pm2_WR(i, PM2R_CONFIG,	PM2F_CONFIG_FB_WRITE_ENABLE|
				PM2F_CONFIG_FB_PACKED_DATA|
				PM2F_CONFIG_FB_READ_SOURCE_ENABLE);
	pm2_WR(i, PM2R_FB_PIXEL_OFFSET, 0);
	pm2_WR(i, PM2R_FB_SOURCE_DELTA,	((ysrc-y)&0xfff)<<16|
						((xsrc-x)&0xfff));
	offset=(x&0x3)-(xsrc&0x3);
	pm2_WR(i, PM2R_RECTANGLE_ORIGIN, (y<<16)|(x>>scale));
	pm2_WR(i, PM2R_RECTANGLE_SIZE, (h<<16)|((w+7)>>scale));
	pm2_WR(i, PM2R_PACKED_DATA_LIMITS, (offset<<29)|(x<<16)|(x+w));
	DEFW();
	pm2_WR(i, PM2R_RENDER,	PM2F_RENDER_RECTANGLE|
				(x<xsrc?PM2F_INCREASE_X:0)|
				(y<ysrc?PM2F_INCREASE_Y:0));
	wait_pm2(i);
}

/*
 * block operation. copy=0: rectangle fill, copy=1: rectangle copy.
 */
static void pm2fb_block_op(struct pm2fb_info* i, int copy,
					s32 xsrc, s32 ysrc,
					s32 x, s32 y, s32 w, s32 h,
					u32 color) {

	if (!w || !h)
		return;
	WAIT_FIFO(i, 6);
	pm2_WR(i, PM2R_CONFIG,	PM2F_CONFIG_FB_WRITE_ENABLE|
				PM2F_CONFIG_FB_READ_SOURCE_ENABLE);
	pm2_WR(i, PM2R_FB_PIXEL_OFFSET, 0);
	if (copy)
		pm2_WR(i, PM2R_FB_SOURCE_DELTA,	((ysrc-y)&0xfff)<<16|
							((xsrc-x)&0xfff));
	else
		pm2_WR(i, PM2R_FB_BLOCK_COLOR, color);
	pm2_WR(i, PM2R_RECTANGLE_ORIGIN, (y<<16)|x);
	pm2_WR(i, PM2R_RECTANGLE_SIZE, (h<<16)|w);
	DEFW();
	pm2_WR(i, PM2R_RENDER,	PM2F_RENDER_RECTANGLE|
				(x<xsrc?PM2F_INCREASE_X:0)|
				(y<ysrc?PM2F_INCREASE_Y:0)|
				(copy?0:PM2F_RENDER_FASTFILL));
	wait_pm2(i);
}

/***************************************************************************
 * Begin of generic initialization functions
 ***************************************************************************/

static void pm2fb_reset(struct pm2fb_info* p) {

	if (p->type == PM2_TYPE_PERMEDIA2V)
		pm2_WR(p, PM2VR_RD_INDEX_HIGH, 0);
	pm2_WR(p, PM2R_RESET_STATUS, 0);
	DEFRW();
	while (pm2_RD(p, PM2R_RESET_STATUS)&PM2F_BEING_RESET);
	DEFRW();
#ifdef CONFIG_FB_PM2_FIFO_DISCONNECT
	DPRINTK("FIFO disconnect enabled\n");
	pm2_WR(p, PM2R_FIFO_DISCON, 1);
	DEFRW();
#endif
	if (board_table[p->board].init)
		board_table[p->board].init(p);
	WAIT_FIFO(p, 48);
	pm2_WR(p, PM2R_CHIP_CONFIG, pm2_RD(p, PM2R_CHIP_CONFIG)&
					~(PM2F_VGA_ENABLE|PM2F_VGA_FIXED));
	pm2_WR(p, PM2R_BYPASS_WRITE_MASK, ~(0L));
	pm2_WR(p, PM2R_FRAMEBUFFER_WRITE_MASK, ~(0L));
	pm2_WR(p, PM2R_FIFO_CONTROL, 0);
	pm2_WR(p, PM2R_FILTER_MODE, PM2F_SYNCHRONIZATION);
	pm2_WR(p, PM2R_APERTURE_ONE, 0);
	pm2_WR(p, PM2R_APERTURE_TWO, 0);
	pm2_WR(p, PM2R_LB_READ_FORMAT, 0);
	pm2_WR(p, PM2R_LB_WRITE_FORMAT, 0); 
	pm2_WR(p, PM2R_LB_READ_MODE, 0);
	pm2_WR(p, PM2R_LB_SOURCE_OFFSET, 0);
	pm2_WR(p, PM2R_FB_SOURCE_OFFSET, 0);
	pm2_WR(p, PM2R_FB_PIXEL_OFFSET, 0);
	pm2_WR(p, PM2R_WINDOW_ORIGIN, 0);
	pm2_WR(p, PM2R_FB_WINDOW_BASE, 0);
	pm2_WR(p, PM2R_LB_WINDOW_BASE, 0);
	pm2_WR(p, PM2R_FB_SOFT_WRITE_MASK, ~(0L));
	pm2_WR(p, PM2R_FB_HARD_WRITE_MASK, ~(0L));
	pm2_WR(p, PM2R_FB_READ_PIXEL, 0);
	pm2_WR(p, PM2R_DITHER_MODE, 0);
	pm2_WR(p, PM2R_AREA_STIPPLE_MODE, 0);
	pm2_WR(p, PM2R_DEPTH_MODE, 0);
	pm2_WR(p, PM2R_STENCIL_MODE, 0);
	pm2_WR(p, PM2R_TEXTURE_ADDRESS_MODE, 0);
	pm2_WR(p, PM2R_TEXTURE_READ_MODE, 0);
	pm2_WR(p, PM2R_TEXEL_LUT_MODE, 0);
	pm2_WR(p, PM2R_YUV_MODE, 0);
	pm2_WR(p, PM2R_COLOR_DDA_MODE, 0);
	pm2_WR(p, PM2R_TEXTURE_COLOR_MODE, 0);
	pm2_WR(p, PM2R_FOG_MODE, 0);
	pm2_WR(p, PM2R_ALPHA_BLEND_MODE, 0);
	pm2_WR(p, PM2R_LOGICAL_OP_MODE, 0);
	pm2_WR(p, PM2R_STATISTICS_MODE, 0);
	pm2_WR(p, PM2R_SCISSOR_MODE, 0);
	switch (p->type) {
	case PM2_TYPE_PERMEDIA2:
		pm2_RDAC_WR(p, PM2I_RD_MODE_CONTROL, 0); /* no overlay */
		pm2_RDAC_WR(p, PM2I_RD_CURSOR_CONTROL, 0);
		pm2_RDAC_WR(p, PM2I_RD_MISC_CONTROL, PM2F_RD_PALETTE_WIDTH_8);
		break;
	case PM2_TYPE_PERMEDIA2V:
		pm2v_RDAC_WR(p, PM2VI_RD_MISC_CONTROL, 1); /* 8bit */
		break;
	}
	pm2_RDAC_WR(p, PM2I_RD_COLOR_KEY_CONTROL, 0);
	pm2_RDAC_WR(p, PM2I_RD_OVERLAY_KEY, 0);
	pm2_RDAC_WR(p, PM2I_RD_RED_KEY, 0);
	pm2_RDAC_WR(p, PM2I_RD_GREEN_KEY, 0);
	pm2_RDAC_WR(p, PM2I_RD_BLUE_KEY, 0);
	clear_palette(p);
	if (p->memclock)
		pm2_set_memclock(p, p->memclock);
}

static int __init pm2fb_conf(struct pm2fb_info* p){

	for (p->board=0; board_table[p->board].detect &&
			!(board_table[p->board].detect(p)); p->board++);
	if (!board_table[p->board].detect) {
		DPRINTK("no board found.\n");
		return 0;
	}
	DPRINTK("found board: %s\n", board_table[p->board].name);

	p->regions.p_fb=p->regions.fb_base;
	if (!request_mem_region(p->regions.p_fb, p->regions.fb_size,
		    		"pm2fb")) {
		printk (KERN_ERR "pm2fb: cannot reserve fb memory, abort\n");
		return 0;
	}
	p->regions.v_fb=MMAP(p->regions.p_fb, p->regions.fb_size);

#ifndef PM2FB_BE_APERTURE
	p->regions.p_regs=p->regions.rg_base;
#else
	p->regions.p_regs=p->regions.rg_base+PM2_REGS_SIZE;
#endif
	if (!request_mem_region(p->regions.p_regs, PM2_REGS_SIZE, "pm2fb")) {
		printk (KERN_ERR "pm2fb: cannot reserve mmio memory, abort\n");
		UNMAP(p->regions.v_fb, p->regions.fb_size);
		return 0;
	}
	p->regions.v_regs=MMAP(p->regions.p_regs, PM2_REGS_SIZE);

#ifdef PM2FB_HW_CURSOR
	p->cursor = pm2_init_cursor(p);
#endif
	return 1;
}

/***************************************************************************
 * Begin of per-board initialization functions
 ***************************************************************************/

/*
 * Phase5 CvisionPPC/BVisionPPC
 */
#ifdef CONFIG_FB_PM2_CVPPC
static int cvppc_PCI_init(struct cvppc_par* p) {
	extern u32 powerup_PCI_present;

	if (!powerup_PCI_present) {
		DPRINTK("no PCI bridge detected\n");
		return 0;
	}
	if (!(p->pci_config=MMAP(CVPPC_PCI_CONFIG, 256))) {
		DPRINTK("unable to map PCI config region\n");
		return 0;
	}
	if (RD32(p->pci_config, PCI_VENDOR_ID)!=
			((PCI_DEVICE_ID_TI_TVP4020<<16)|PCI_VENDOR_ID_TI)) {
		DPRINTK("bad vendorID/deviceID\n");
		return 0;
	}
	if (!(p->pci_bridge=MMAP(CSPPC_PCI_BRIDGE, 256))) {
		DPRINTK("unable to map PCI bridge\n");
		return 0;
	}
	WR32(p->pci_bridge, CSPPC_BRIDGE_ENDIAN, CSPPCF_BRIDGE_BIG_ENDIAN);
	DEFW();
	if (pm2fb_options.flags & OPTF_OLD_MEM)
		WR32(p->pci_config, PCI_CACHE_LINE_SIZE, 0xff00);
	WR32(p->pci_config, PCI_BASE_ADDRESS_0, CVPPC_REGS_REGION);
	WR32(p->pci_config, PCI_BASE_ADDRESS_1, CVPPC_FB_APERTURE_ONE);
	WR32(p->pci_config, PCI_BASE_ADDRESS_2, CVPPC_FB_APERTURE_TWO);
	WR32(p->pci_config, PCI_ROM_ADDRESS, CVPPC_ROM_ADDRESS);
	DEFW();
	WR32(p->pci_config, PCI_COMMAND, 0xef000000 |
						PCI_COMMAND_IO |
						PCI_COMMAND_MEMORY |
						PCI_COMMAND_MASTER);
	return 1;
}

static int __init cvppc_detect(struct pm2fb_info* p) {

	if (!cvppc_PCI_init(&p->board_par.cvppc))
		return 0;
	p->type = PM2_TYPE_PERMEDIA2;
	p->regions.fb_base=CVPPC_FB_APERTURE_ONE;
	p->regions.fb_size=CVPPC_FB_SIZE;
	p->regions.rg_base=CVPPC_REGS_REGION;
	p->memclock=CVPPC_MEMCLOCK;
	return 1;
}

static void cvppc_init(struct pm2fb_info* p) {

	WAIT_FIFO(p, 3);
	pm2_WR(p, PM2R_MEM_CONTROL, 0);
	pm2_WR(p, PM2R_BOOT_ADDRESS, 0x30);
	DEFW();
	if (pm2fb_options.flags & OPTF_OLD_MEM)
		pm2_WR(p, PM2R_MEM_CONFIG, CVPPC_MEM_CONFIG_OLD);
	else
		pm2_WR(p, PM2R_MEM_CONFIG, CVPPC_MEM_CONFIG_NEW);
}
#endif /* CONFIG_FB_PM2_CVPPC */

/*
 * Generic PCI detection routines
 */
#ifdef CONFIG_FB_PM2_PCI
struct {
	unsigned short vendor, device;
	char *name;
	pm2type_t type;
} pm2pci_cards[] __initdata = {
{ PCI_VENDOR_ID_TI, PCI_DEVICE_ID_TI_TVP4020, "Texas Instruments TVP4020", PM2_TYPE_PERMEDIA2 },
{ PCI_VENDOR_ID_3DLABS, PCI_DEVICE_ID_3DLABS_PERMEDIA2, "3dLabs Permedia 2", PM2_TYPE_PERMEDIA2 },
{ PCI_VENDOR_ID_3DLABS, PCI_DEVICE_ID_3DLABS_PERMEDIA2V, "3dLabs Permedia 2v", PM2_TYPE_PERMEDIA2V },
{ 0, 0 }
};

static int __init pm2pci_detect(struct pm2fb_info* p) {
	struct pm2pci_par* pci=&p->board_par.pci;
	struct pci_dev* dev;
	int i;
	unsigned char* m;
#ifdef __sparc__
	struct pcidev_cookie *pcp;
#endif

	memset(pci, 0, sizeof(struct pm2pci_par));
	if (!pci_present()) {
		DPRINTK("no PCI bus found.\n");
		return 0;
	}
	DPRINTK("scanning PCI bus for known chipsets...\n");

	pci_for_each_dev(dev) {
		for (i = 0; pm2pci_cards[i].vendor; i++)
			if (pm2pci_cards[i].vendor == dev->vendor &&
			    pm2pci_cards[i].device == dev->device) {
				pci->dev = dev;
				p->type = pm2pci_cards[i].type;
				DPRINTK("... found %s\n", pm2pci_cards[i].name);
				break;
			}
		if (pci->dev)
			break;
	}
	if (!pci->dev) {
		DPRINTK("no PCI board found.\n");
		return 0;
	}
	DPRINTK("PCI board @%08lx %08lx %08lx rom %08lx\n",
			pci->dev->resource[0].start,
			pci->dev->resource[1].start,
			pci->dev->resource[2].start,
			pci->dev->resource[PCI_ROM_RESOURCE].start);
#ifdef __sparc__
	p->regions.rg_base= pci->dev->resource[0].start;
	p->regions.fb_base= pci->dev->resource[1].start;
	pcp = pci->dev->sysdata;
	/* If the user has not asked for a particular mode, lets guess */
	if (pcp->prom_node && !(pm2fb_options.flags & OPTF_USER)) {
		char timing[256], *q, *r;
		unsigned long w, h;
		int i;
		prom_getstring(pcp->prom_node, "timing-numbers", timing, 256);
		/* FIXME: Find out what the actual pixclock is and other values as well */
		if (timing[0]) {
			w = simple_strtoul(timing, &q, 0);
			h = 0;
			if (q == timing) w = 0;
			if (w) {
				for (i = 0; i < 3; i++) {
					for (r = q; *r && (*r < '0' || *r > '9'); r++);
					simple_strtoul(r, &q, 0);
					if (r == q) break;
				}
				if (i < 3) w = 0;
			}
			if (w) {
				for (r = q; *r && (*r < '0' || *r > '9'); r++);
				h = simple_strtoul(r, &q, 0);
				if (r == q) w = 0;
			}
			if (w == 640 && h == 480) w = 0;
			if (w) {
				for (i=0; user_mode[i].name[0] &&
					  (w != user_mode[i].par.width ||
					   h != user_mode[i].par.height); i++);
				if (user_mode[i].name[0])
					memcpy(&p->current_par, &user_mode[i].par, sizeof(user_mode[i].par));
			}
		}
	}
#else
	if (pm2fb_options.flags & OPTF_VIRTUAL) {
		p->regions.rg_base = __pa(pci_resource_start(pci->dev, 0));
		p->regions.fb_base = __pa(pci_resource_start(pci->dev, 1));
	}
	else {
		p->regions.rg_base = pci_resource_start(pci->dev, 0);
		p->regions.fb_base = pci_resource_start(pci->dev, 1);
	}
#endif
#ifdef PM2FB_BE_APERTURE
	p->regions.rg_base += PM2_REGS_SIZE;
#endif
	if ((m=MMAP(p->regions.rg_base, PM2_REGS_SIZE))) {
		pci->mem_control=RD32(m, PM2R_MEM_CONTROL);
		pci->boot_address=RD32(m, PM2R_BOOT_ADDRESS);
		pci->mem_config=RD32(m, PM2R_MEM_CONFIG);
		switch (pci->mem_config & PM2F_MEM_CONFIG_RAM_MASK) {
			case PM2F_MEM_BANKS_1:
				p->regions.fb_size=0x200000;
				break;
			case PM2F_MEM_BANKS_2:
				p->regions.fb_size=0x400000;
				break;
			case PM2F_MEM_BANKS_3:
				p->regions.fb_size=0x600000;
				break;
			case PM2F_MEM_BANKS_4:
				p->regions.fb_size=0x800000;
				break;
		}
		p->memclock=CVPPC_MEMCLOCK;
		UNMAP(m, PM2_REGS_SIZE);
		return 1;
	}
	DPRINTK("MMAP() failed.\n");
	return 0;
}

static void pm2pci_init(struct pm2fb_info* p) {
	struct pm2pci_par* pci=&p->board_par.pci;

	WAIT_FIFO(p, 3);
	pm2_WR(p, PM2R_MEM_CONTROL, pci->mem_control);
	pm2_WR(p, PM2R_BOOT_ADDRESS, pci->boot_address);
	DEFW();
	pm2_WR(p, PM2R_MEM_CONFIG, pci->mem_config);
}
#endif /* CONFIG_FB_PM2_PCI */

/***************************************************************************
 * Console hw acceleration
 ***************************************************************************/


static int pm2fb_blank(int blank_mode, struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;
	u32 video;

	if (!i->current_par_valid)
		return 1;
	video=i->current_par.video;
	if (blank_mode>0) {
		switch (blank_mode-1) {
			case VESA_NO_BLANKING:		/* FIXME */
				video=video&~(PM2F_VIDEO_ENABLE);
				break;
			case VESA_HSYNC_SUSPEND:
				video=video&~(PM2F_HSYNC_MASK|
						PM2F_BLANK_LOW);
				break;
			case VESA_VSYNC_SUSPEND:
				video=video&~(PM2F_VSYNC_MASK|
						PM2F_BLANK_LOW);
				break;
			case VESA_POWERDOWN:
				video=video&~(PM2F_VSYNC_MASK|
						PM2F_HSYNC_MASK|
						PM2F_BLANK_LOW);
				break;
		}
	}
	WAIT_FIFO(i, 1);
	pm2_WR(i, PM2R_VIDEO_CONTROL, video);
	return 0;
}

static int pm2fb_pan_display(const struct fb_var_screeninfo* var,
					struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;

	if (!i->current_par_valid)
		return -EINVAL;
	i->current_par.base=to3264(var->yoffset*i->current_par.width+
				var->xoffset, i->current_par.depth, 1);
	WAIT_FIFO(i, 1);
	pm2_WR(i, PM2R_SCREEN_BASE, i->current_par.base);
	return 0;
}

static void pm2fb_pp_bmove(struct display* p, int sy, int sx,
				int dy, int dx, int height, int width) {

	if (fontwidthlog(p)) {
		sx=sx<<fontwidthlog(p);
		dx=dx<<fontwidthlog(p);
		width=width<<fontwidthlog(p);
	}
	else {
		sx=sx*fontwidth(p);
		dx=dx*fontwidth(p);
		width=width*fontwidth(p);
	}
	sy=sy*fontheight(p);
	dy=dy*fontheight(p);
	height=height*fontheight(p);
	pm2fb_pp_copy((struct pm2fb_info* )p->fb_info, sx, sy, dx,
							dy, width, height);
}

static void pm2fb_bmove(struct display* p, int sy, int sx,
				int dy, int dx, int height, int width) {

	if (fontwidthlog(p)) {
		sx=sx<<fontwidthlog(p);
		dx=dx<<fontwidthlog(p);
		width=width<<fontwidthlog(p);
	}
	else {
		sx=sx*fontwidth(p);
		dx=dx*fontwidth(p);
		width=width*fontwidth(p);
	}
	sy=sy*fontheight(p);
	dy=dy*fontheight(p);
	height=height*fontheight(p);
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 1, sx, sy, dx, dy,
							width, height, 0);
}

#ifdef FBCON_HAS_CFB8
static void pm2fb_clear8(struct vc_data* conp, struct display* p,
				int sy, int sx, int height, int width) {
	u32 c;

	sx=sx*fontwidth(p);
	width=width*fontwidth(p);
	sy=sy*fontheight(p);
	height=height*fontheight(p);
	c=attr_bgcol_ec(p, conp);
	c|=c<<8;
	c|=c<<16;
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0, sx, sy,
							width, height, c);
}

static void pm2fb_clear_margins8(struct vc_data* conp, struct display* p,
							int bottom_only) {
	u32 c;
	u32 sx;
	u32 sy;

	c=attr_bgcol_ec(p, conp);
	c|=c<<8;
	c|=c<<16;
	sx=conp->vc_cols*fontwidth(p);
	sy=conp->vc_rows*fontheight(p);
	if (!bottom_only)
		pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0,
			sx, 0, (p->var.xres-sx), p->var.yres_virtual, c);
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0,
				0, p->var.yoffset+sy, sx, p->var.yres-sy, c);
}

static struct display_switch pm2_cfb8 = {
	setup:		fbcon_cfb8_setup,
	bmove:		pm2fb_pp_bmove,
#ifdef __alpha__
	/* Not sure why, but this works and the other does not. */
	/* Also, perhaps we need a separate routine to wait for the
	   blitter to stop before doing this? */
	/* In addition, maybe we need to do this for 16 and 32 bit depths? */
	clear:		fbcon_cfb8_clear,
#else
	clear:		pm2fb_clear8,
#endif
	putc:		fbcon_cfb8_putc,
	putcs:		fbcon_cfb8_putcs,
	revc:		fbcon_cfb8_revc,
	cursor:		pm2fb_cursor,
	set_font:	pm2fb_set_font,
	clear_margins:	pm2fb_clear_margins8,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16) };
#endif /* FBCON_HAS_CFB8 */

#ifdef FBCON_HAS_CFB16
static void pm2fb_clear16(struct vc_data* conp, struct display* p,
				int sy, int sx, int height, int width) {
	u32 c;

	sx=sx*fontwidth(p);
	width=width*fontwidth(p);
	sy=sy*fontheight(p);
	height=height*fontheight(p);
	c=((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	c|=c<<16;
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0, sx, sy,
							width, height, c);
}

static void pm2fb_clear_margins16(struct vc_data* conp, struct display* p,
							int bottom_only) {
	u32 c;
	u32 sx;
	u32 sy;

	c = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	c|=c<<16;
	sx=conp->vc_cols*fontwidth(p);
	sy=conp->vc_rows*fontheight(p);
	if (!bottom_only)
		pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0,
			sx, 0, (p->var.xres-sx), p->var.yres_virtual, c);
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0,
				0, p->var.yoffset+sy, sx, p->var.yres-sy, c);
}

static struct display_switch pm2_cfb16 = {
	setup:		fbcon_cfb16_setup,
	bmove:		pm2fb_pp_bmove,
	clear:		pm2fb_clear16,
	putc:		fbcon_cfb16_putc,
	putcs:		fbcon_cfb16_putcs,
	revc:		fbcon_cfb16_revc,
	cursor:		pm2fb_cursor,
	set_font:	pm2fb_set_font,
	clear_margins:	pm2fb_clear_margins16,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif /* FBCON_HAS_CFB16 */

#ifdef FBCON_HAS_CFB24
/*
 * fast fill for 24bpp works only when red==green==blue
 */
static void pm2fb_clear24(struct vc_data* conp, struct display* p,
				int sy, int sx, int height, int width) {
	struct pm2fb_info* i=(struct pm2fb_info* )p->fb_info;
	u32 c;

	c=attr_bgcol_ec(p, conp);
	if (		i->palette[c].red==i->palette[c].green &&
			i->palette[c].green==i->palette[c].blue) {
		c=((u32 *)p->dispsw_data)[c];
		c|=(c&0xff0000)<<8;
		sx=sx*fontwidth(p);
		width=width*fontwidth(p);
		sy=sy*fontheight(p);
		height=height*fontheight(p);
		pm2fb_block_op(i, 0, 0, 0, sx, sy, width, height, c);
	}
	else
		fbcon_cfb24_clear(conp, p, sy, sx, height, width);

}

static void pm2fb_clear_margins24(struct vc_data* conp, struct display* p,
							int bottom_only) {
	struct pm2fb_info* i=(struct pm2fb_info* )p->fb_info;
	u32 c;
	u32 sx;
	u32 sy;

	c=attr_bgcol_ec(p, conp);
	if (		i->palette[c].red==i->palette[c].green &&
			i->palette[c].green==i->palette[c].blue) {
		c=((u32 *)p->dispsw_data)[c];
		c|=(c&0xff0000)<<8;
		sx=conp->vc_cols*fontwidth(p);
		sy=conp->vc_rows*fontheight(p);
		if (!bottom_only)
		pm2fb_block_op(i, 0, 0, 0, sx, 0, (p->var.xres-sx),
							p->var.yres_virtual, c);
		pm2fb_block_op(i, 0, 0, 0, 0, p->var.yoffset+sy,
						sx, p->var.yres-sy, c);
	}
	else
		fbcon_cfb24_clear_margins(conp, p, bottom_only);

}

static struct display_switch pm2_cfb24 = {
	setup:		fbcon_cfb24_setup,
	bmove:		pm2fb_bmove,
	clear:		pm2fb_clear24,
	putc:		fbcon_cfb24_putc,
	putcs:		fbcon_cfb24_putcs,
	revc:		fbcon_cfb24_revc,
	cursor:		pm2fb_cursor,
	set_font:	pm2fb_set_font,
	clear_margins:	pm2fb_clear_margins24,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif /* FBCON_HAS_CFB24 */

#ifdef FBCON_HAS_CFB32
static void pm2fb_clear32(struct vc_data* conp, struct display* p,
				int sy, int sx, int height, int width) {
	u32 c;

	sx=sx*fontwidth(p);
	width=width*fontwidth(p);
	sy=sy*fontheight(p);
	height=height*fontheight(p);
	c=((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0, sx, sy,
							width, height, c);
}

static void pm2fb_clear_margins32(struct vc_data* conp, struct display* p,
							int bottom_only) {
	u32 c;
	u32 sx;
	u32 sy;

	c = ((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	sx=conp->vc_cols*fontwidth(p);
	sy=conp->vc_rows*fontheight(p);
	if (!bottom_only)
		pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0,
			sx, 0, (p->var.xres-sx), p->var.yres_virtual, c);
	pm2fb_block_op((struct pm2fb_info* )p->fb_info, 0, 0, 0,
				0, p->var.yoffset+sy, sx, p->var.yres-sy, c);
}

static struct display_switch pm2_cfb32 = {
	setup:		fbcon_cfb32_setup,
	bmove:		pm2fb_bmove,
	clear:		pm2fb_clear32,
	putc:		fbcon_cfb32_putc,
	putcs:		fbcon_cfb32_putcs,
	revc:		fbcon_cfb32_revc,
	cursor:		pm2fb_cursor,
	set_font:	pm2fb_set_font,
	clear_margins:	pm2fb_clear_margins32,
	fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif /* FBCON_HAS_CFB32 */

/***************************************************************************
 * Framebuffer functions
 ***************************************************************************/

static void pm2fb_detect(void) {}

static int pm2fb_encode_fix(struct fb_fix_screeninfo* fix,
			const void* par, struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;
	struct pm2fb_par* p=(struct pm2fb_par* )par;

	strcpy(fix->id, permedia2_name);
	fix->smem_start=i->regions.p_fb;
	fix->smem_len=i->regions.fb_size;
	fix->mmio_start=i->regions.p_regs;
	fix->mmio_len=PM2_REGS_SIZE;
	fix->accel=FB_ACCEL_3DLABS_PERMEDIA2;
	fix->type=FB_TYPE_PACKED_PIXELS;
	fix->visual=p->depth==8?FB_VISUAL_PSEUDOCOLOR:FB_VISUAL_TRUECOLOR;
	if (i->current_par_valid)
		fix->line_length=i->current_par.width*(i->current_par.depth/8);
	else
		fix->line_length=0;
	fix->xpanstep=p->depth==24?8:64/p->depth;
	fix->ypanstep=1;
	fix->ywrapstep=0;
	return 0;
}

#ifdef PM2FB_MASTER_DEBUG
static void pm2fb_display_var(const struct fb_var_screeninfo* var) {

	printk( KERN_DEBUG
"- struct fb_var_screeninfo ---------------------------------------------------\n");
	printk( KERN_DEBUG
		"resolution: %ux%ux%u (virtual %ux%u+%u+%u)\n",
			var->xres, var->yres, var->bits_per_pixel,
			var->xres_virtual, var->yres_virtual,
			var->xoffset, var->yoffset);
	printk( KERN_DEBUG
		"color: %c%c "
		"R(%u,%u,%u), G(%u,%u,%u), B(%u,%u,%u), T(%u,%u,%u)\n",
			var->grayscale?'G':'C', var->nonstd?'N':'S',
			var->red.offset, var->red.length, var->red.msb_right,
			var->green.offset, var->green.length, var->green.msb_right,
			var->blue.offset, var->blue.length, var->blue.msb_right,
			var->transp.offset, var->transp.length,
			var->transp.msb_right);
	printk( KERN_DEBUG
		"timings: %ups (%u,%u)-(%u,%u)+%u+%u\n",
		var->pixclock,
		var->left_margin, var->upper_margin, var->right_margin,
		var->lower_margin, var->hsync_len, var->vsync_len);
	printk(	KERN_DEBUG
		"activate %08x accel_flags %08x sync %08x vmode %08x\n",
		var->activate, var->accel_flags, var->sync, var->vmode);
	printk(	KERN_DEBUG
"------------------------------------------------------------------------------\n");
}

#define pm2fb_decode_var pm2fb_wrapped_decode_var
#endif

static int pm2fb_decode_var(const struct fb_var_screeninfo* var,
				void* par, struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;
	struct pm2fb_par p;
	u32 xres;
	int data64;

	memset(&p, 0, sizeof(struct pm2fb_par));
	p.width=(var->xres_virtual+7)&~7;
	p.height=var->yres_virtual;
	p.depth=(var->bits_per_pixel+7)&~7;
	p.depth=p.depth>32?32:p.depth;
	data64=p.depth>8 || i->type == PM2_TYPE_PERMEDIA2V;
	xres=(var->xres+31)&~31;
	if (p.width<xres+var->xoffset)
		p.width=xres+var->xoffset;
	if (p.height<var->yres+var->yoffset)
		p.height=var->yres+var->yoffset;
	if (!partprod(xres)) {
		DPRINTK("width not supported: %u\n", xres);
		return -EINVAL;
	}
	if (p.width>2047) {
		DPRINTK("virtual width not supported: %u\n", p.width);
		return -EINVAL;
	}
	if (var->yres<200) {
		DPRINTK("height not supported: %u\n",
						(u32 )var->yres);
		return -EINVAL;
	}
	if (p.height<200 || p.height>2047) {
		DPRINTK("virtual height not supported: %u\n", p.height);
		return -EINVAL;
	}
	if (p.depth>32) {
		DPRINTK("depth not supported: %u\n", p.depth);
		return -EINVAL;
	}
	if (p.width*p.height*p.depth/8>i->regions.fb_size) {
		DPRINTK("no memory for screen (%ux%ux%u)\n",
						p.width, p.height, p.depth);
		return -EINVAL;
	}
	p.pixclock=PICOS2KHZ(var->pixclock);
	if (p.pixclock>PM2_MAX_PIXCLOCK) {
		DPRINTK("pixclock too high (%uKHz)\n", p.pixclock);
		return -EINVAL;
	}
	p.hsstart=to3264(var->right_margin, p.depth, data64);
	p.hsend=p.hsstart+to3264(var->hsync_len, p.depth, data64);
	p.hbend=p.hsend+to3264(var->left_margin, p.depth, data64);
	p.htotal=to3264(xres, p.depth, data64)+p.hbend-1;
	p.vsstart=var->lower_margin?var->lower_margin-1:0;	/* FIXME! */
	p.vsend=var->lower_margin+var->vsync_len-1;
	p.vbend=var->lower_margin+var->vsync_len+var->upper_margin;
	p.vtotal=var->yres+p.vbend-1;
	p.stride=to3264(p.width, p.depth, 1);
	p.base=to3264(var->yoffset*xres+var->xoffset, p.depth, 1);
	if (data64)
		p.video|=PM2F_DATA_64_ENABLE;
	if (var->sync & FB_SYNC_HOR_HIGH_ACT)
		p.video|=PM2F_HSYNC_ACT_HIGH;
	else
		p.video|=PM2F_HSYNC_ACT_LOW;
	if (var->sync & FB_SYNC_VERT_HIGH_ACT)
		p.video|=PM2F_VSYNC_ACT_HIGH;
	else
		p.video|=PM2F_VSYNC_ACT_LOW;
	if ((var->vmode & FB_VMODE_MASK)==FB_VMODE_INTERLACED) {
		DPRINTK("interlaced not supported\n");
		return -EINVAL;
	}
	if ((var->vmode & FB_VMODE_MASK)==FB_VMODE_DOUBLE)
		p.video|=PM2F_LINE_DOUBLE;
	if (var->activate==FB_ACTIVATE_NOW)
		p.video|=PM2F_VIDEO_ENABLE;
	*((struct pm2fb_par* )par)=p;
	return 0;
}

#ifdef PM2FB_MASTER_DEBUG
#undef pm2fb_decode_var

static int pm2fb_decode_var(const struct fb_var_screeninfo* var,
				void* par, struct fb_info_gen* info) {
	int result;

	result=pm2fb_wrapped_decode_var(var, par, info);
	pm2fb_display_var(var);
	return result;
}
#endif

static int pm2fb_encode_var(struct fb_var_screeninfo* var,
				const void* par, struct fb_info_gen* info) {
	struct pm2fb_par* p=(struct pm2fb_par* )par;
	struct fb_var_screeninfo v;
	u32 base;

	memset(&v, 0, sizeof(struct fb_var_screeninfo));
	v.xres_virtual=p->width;
	v.yres_virtual=p->height;
	v.xres=(p->htotal+1)-p->hbend;
	v.yres=(p->vtotal+1)-p->vbend;
	v.right_margin=p->hsstart;
	v.hsync_len=p->hsend-p->hsstart;
	v.left_margin=p->hbend-p->hsend;
	v.lower_margin=p->vsstart+1;
	v.vsync_len=p->vsend-v.lower_margin+1;
	v.upper_margin=p->vbend-v.lower_margin-v.vsync_len;
	v.bits_per_pixel=p->depth;
	if (p->video & PM2F_DATA_64_ENABLE) {
		v.xres=v.xres<<1;
		v.right_margin=v.right_margin<<1;
		v.hsync_len=v.hsync_len<<1;
		v.left_margin=v.left_margin<<1;
	}
	switch (p->depth) {
		case 8:
			v.red.length=v.green.length=v.blue.length=8;
			v.xres=v.xres<<2;
			v.right_margin=v.right_margin<<2;
			v.hsync_len=v.hsync_len<<2;
			v.left_margin=v.left_margin<<2;
			break;
		case 16:
			v.red.offset=11;
			v.red.length=5;
			v.green.offset=5;
			v.green.length=6;
			v.blue.length=5;
			v.xres=v.xres<<1;
			v.right_margin=v.right_margin<<1;
			v.hsync_len=v.hsync_len<<1;
			v.left_margin=v.left_margin<<1;
			break;
		case 32:
			v.transp.offset=24;
			v.red.offset=16;
			v.green.offset=8;
			v.red.length=v.green.length=v.blue.length=
							v.transp.length=8;
			break;
		case 24:
			v.blue.offset=16;
			v.green.offset=8;
			v.red.length=v.green.length=v.blue.length=8;
			v.xres=(v.xres<<2)/3;
			v.right_margin=(v.right_margin<<2)/3;
			v.hsync_len=(v.hsync_len<<2)/3;
			v.left_margin=(v.left_margin<<2)/3;
			break;
	}
	base=from3264(p->base, p->depth, 1);
	v.xoffset=base%v.xres;
	v.yoffset=base/v.xres;
	v.height=v.width=-1;
	v.pixclock=KHZ2PICOS(p->pixclock);
	if ((p->video & PM2F_HSYNC_MASK)==PM2F_HSYNC_ACT_HIGH)
		v.sync|=FB_SYNC_HOR_HIGH_ACT;
	if ((p->video & PM2F_VSYNC_MASK)==PM2F_VSYNC_ACT_HIGH)
		v.sync|=FB_SYNC_VERT_HIGH_ACT;
	if (p->video & PM2F_LINE_DOUBLE)
		v.vmode=FB_VMODE_DOUBLE;
	*var=v;
	return 0;
}

static void set_user_mode(struct pm2fb_info* i) {

	if (pm2fb_options.flags & OPTF_YPAN) {
		int h = i->current_par.height;
		i->current_par.height=i->regions.fb_size/
			(i->current_par.width*i->current_par.depth/8);
		i->current_par.height=MIN(i->current_par.height,2047);
		i->current_par.height=MAX(i->current_par.height,h);
	}
}

static void pm2fb_get_par(void* par, struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;
	
	if (!i->current_par_valid) {
		set_user_mode(i);
		pm2fb_reset(i);
		set_screen(i, &i->current_par);
		i->current_par_valid=1;
	}
	*((struct pm2fb_par* )par)=i->current_par;
}

static void pm2fb_set_par(const void* par, struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;
	struct pm2fb_par* p;

	p=(struct pm2fb_par* )par;
	if (i->current_par_valid) {
		i->current_par.base=p->base;
		if (!memcmp(p, &i->current_par, sizeof(struct pm2fb_par))) {
			WAIT_FIFO(i, 1);
			pm2_WR(i, PM2R_SCREEN_BASE, p->base);
			return;
		}
	}
	set_screen(i, p);
	i->current_par=*p;
	i->current_par_valid=1;
#ifdef PM2FB_HW_CURSOR	
	if (i->cursor) {
		pm2v_set_cursor_color(i, cursor_color_map, cursor_color_map, cursor_color_map);
		pm2v_set_cursor_shape(i);
	}
#endif
}

static int pm2fb_getcolreg(unsigned regno,
			unsigned* red, unsigned* green, unsigned* blue,
				unsigned* transp, struct fb_info* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;

	if (regno<256) {
		*red=i->palette[regno].red<<8|i->palette[regno].red;
		*green=i->palette[regno].green<<8|i->palette[regno].green;
		*blue=i->palette[regno].blue<<8|i->palette[regno].blue;
		*transp=i->palette[regno].transp<<8|i->palette[regno].transp;
	}
	return regno>255;
}

static int pm2fb_setcolreg(unsigned regno,
			unsigned red, unsigned green, unsigned blue,
				unsigned transp, struct fb_info* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;

	if (regno<16) {
		switch (i->current_par.depth) {
#ifdef FBCON_HAS_CFB8
			case 8:
				break;
#endif
#ifdef FBCON_HAS_CFB16
			case 16:
				i->cmap.cmap16[regno]=
					((u32 )red & 0xf800) |
					(((u32 )green & 0xfc00)>>5) |
					(((u32 )blue & 0xf800)>>11);
				break;
#endif
#ifdef FBCON_HAS_CFB24
			case 24:
				i->cmap.cmap24[regno]=
					(((u32 )blue & 0xff00) << 8) |
					((u32 )green & 0xff00) |
					(((u32 )red & 0xff00) >> 8);
				break;
#endif
#ifdef FBCON_HAS_CFB32
			case 32:
	   			i->cmap.cmap32[regno]=
					(((u32 )transp & 0xff00) << 16) |
		    			(((u32 )red & 0xff00) << 8) |
					(((u32 )green & 0xff00)) |
			 		(((u32 )blue & 0xff00) >> 8);
				break;
#endif
			default:
				DPRINTK("bad depth %u\n",
						i->current_par.depth);
				break;
		}
	}
	if (regno<256) {
		i->palette[regno].red=red >> 8;
		i->palette[regno].green=green >> 8;
		i->palette[regno].blue=blue >> 8;
		i->palette[regno].transp=transp >> 8;
		if (i->current_par.depth==8)
			set_color(i, regno, red>>8, green>>8, blue>>8);
	}
	return regno>255;
}

static void pm2fb_set_disp(const void* par, struct display* disp,
						   struct fb_info_gen* info) {
	struct pm2fb_info* i=(struct pm2fb_info* )info;
	unsigned long flags;
	unsigned long depth;

	save_flags(flags);
	cli();
	disp->screen_base = i->regions.v_fb;
	switch (depth=((struct pm2fb_par* )par)->depth) {
#ifdef FBCON_HAS_CFB8
		case 8:
			disp->dispsw=&pm2_cfb8;
			break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:
			disp->dispsw=&pm2_cfb16;
			disp->dispsw_data=i->cmap.cmap16;
			break;
#endif
#ifdef FBCON_HAS_CFB24
		case 24:
			disp->dispsw=&pm2_cfb24;
			disp->dispsw_data=i->cmap.cmap24;
			break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:
			disp->dispsw=&pm2_cfb32;
			disp->dispsw_data=i->cmap.cmap32;
			break;
#endif
		default:
			disp->dispsw=&fbcon_dummy;
			break;
	}
	restore_flags(flags);
}

#ifdef PM2FB_HW_CURSOR
/***************************************************************************
 * Hardware cursor support
 ***************************************************************************/
 
static u8 cursor_bits_lookup[16] = {
	0x00, 0x40, 0x10, 0x50, 0x04, 0x44, 0x14, 0x54,
	0x01, 0x41, 0x11, 0x51, 0x05, 0x45, 0x15, 0x55
};

static u8 cursor_mask_lookup[16] = {
	0x00, 0x80, 0x20, 0xa0, 0x08, 0x88, 0x28, 0xa8,
	0x02, 0x82, 0x22, 0xa2, 0x0a, 0x8a, 0x2a, 0xaa
};

static void pm2v_set_cursor_color(struct pm2fb_info *fb, u8 *red, u8 *green, u8 *blue)
{
	struct pm2_cursor *c = fb->cursor;
	int i;

	for (i = 0; i < 2; i++) {
		c->color[3*i] = red[i];
		c->color[3*i+1] = green[i];
		c->color[3*i+2] = blue[i];
	}

	WAIT_FIFO(fb, 14);
	pm2_WR(fb, PM2VR_RD_INDEX_HIGH, PM2VI_RD_CURSOR_PALETTE >> 8);
	for (i = 0; i < 6; i++)
		pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PALETTE+i, c->color[i]);
	pm2_WR(fb, PM2VR_RD_INDEX_HIGH, 0);
}

static void pm2v_set_cursor_shape(struct pm2fb_info *fb)
{
	struct pm2_cursor *c = fb->cursor;
	u8 m, b;
	int i, x, y;

	WAIT_FIFO(fb, 1);
	pm2_WR(fb, PM2VR_RD_INDEX_HIGH, PM2VI_RD_CURSOR_PATTERN >> 8);
	for (y = 0, i = 0; y < c->size.y; y++) {
		WAIT_FIFO(fb, 32);
		for (x = 0; x < c->size.x >> 3; x++) {
			m = c->mask[x][y];
			b = c->bits[x][y];
			pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PATTERN + i,
				     cursor_mask_lookup[m >> 4] |
				     cursor_bits_lookup[(b & m) >> 4]);
			pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PATTERN + i + 1,
				     cursor_mask_lookup[m & 0x0f] |
				     cursor_bits_lookup[(b & m) & 0x0f]);
			i+=2;
		}
		for ( ; x < 8; x++) {
			pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PATTERN + i, 0);
			pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PATTERN + i + 1, 0);
			i+=2;
		}
	}
	for (; y < 64; y++) {
		WAIT_FIFO(fb, 32);
		for (x = 0; x < 8; x++) {
			pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PATTERN + i, 0);
			pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_PATTERN + i + 1, 0);
			i+=2;
		}
	}
	WAIT_FIFO(fb, 1);
	pm2_WR(fb, PM2VR_RD_INDEX_HIGH, 0);
}

static void pm2v_set_cursor(struct pm2fb_info *fb, int on)
{
	struct pm2_cursor *c = fb->cursor;
	int x = c->pos.x;

	if (!on) x = 4000;
	WAIT_FIFO(fb, 14);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_X_LOW, x & 0xff);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_X_HIGH, (x >> 8) & 0x0f);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_Y_LOW, c->pos.y & 0xff);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_Y_HIGH, (c->pos.y >> 8) & 0x0f);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_X_HOT, c->hot.x & 0x3f);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_Y_HOT, c->hot.y & 0x3f);
	pm2v_RDAC_WR(fb, PM2VI_RD_CURSOR_MODE, 0x11);
}

static void pm2_cursor_timer_handler(unsigned long dev_addr)
{
	struct pm2fb_info *fb = (struct pm2fb_info *)dev_addr;

	if (!fb->cursor->enable)
		goto out;

	if (fb->cursor->vbl_cnt && --fb->cursor->vbl_cnt == 0) {
		fb->cursor->on ^= 1;
		pm2v_set_cursor(fb, fb->cursor->on);
		fb->cursor->vbl_cnt = fb->cursor->blink_rate;
	}

out:
	fb->cursor->timer->expires = jiffies + (HZ / 50);
	add_timer(fb->cursor->timer);
}

static void pm2fb_cursor(struct display *p, int mode, int x, int y)
{
	struct pm2fb_info *fb = (struct pm2fb_info *)p->fb_info;
	struct pm2_cursor *c = fb->cursor;

	if (!c) return;

	x *= fontwidth(p);
	y *= fontheight(p);
	if (c->pos.x == x && c->pos.y == y && (mode == CM_ERASE) == !c->enable)
		return;

	c->enable = 0;
	if (c->on)
		pm2v_set_cursor(fb, 0);
	c->pos.x = x;
	c->pos.y = y;

	switch (mode) {
	case CM_ERASE:
		c->on = 0;
		break;

	case CM_DRAW:
	case CM_MOVE:
		if (c->on)
			pm2v_set_cursor(fb, 1);
		else
			c->vbl_cnt = CURSOR_DRAW_DELAY;
		c->enable = 1;
		break;
	}
}

static struct pm2_cursor * __init pm2_init_cursor(struct pm2fb_info *fb)
{
	struct pm2_cursor *cursor;

	if (fb->type != PM2_TYPE_PERMEDIA2V)
		return 0; /* FIXME: Support hw cursor everywhere */

	cursor = kmalloc(sizeof(struct pm2_cursor), GFP_ATOMIC);
	if (!cursor)
		return 0;
	memset(cursor, 0, sizeof(*cursor));

	cursor->timer = kmalloc(sizeof(*cursor->timer), GFP_KERNEL);
	if (!cursor->timer) {
		kfree(cursor);
		return 0;
	}
	memset(cursor->timer, 0, sizeof(*cursor->timer));

	cursor->blink_rate = DEFAULT_CURSOR_BLINK_RATE;

	if (curblink) {
		init_timer(cursor->timer);
		cursor->timer->expires = jiffies + (HZ / 50);
		cursor->timer->data = (unsigned long)fb;
		cursor->timer->function = pm2_cursor_timer_handler;
		add_timer(cursor->timer);
	}

	return cursor;
}

static int pm2fb_set_font(struct display *d, int width, int height)
{
	struct pm2fb_info *fb = (struct pm2fb_info *)d->fb_info;
	struct pm2_cursor *c = fb->cursor;
	int i, j;

	if (c) {
		if (!width || !height) {
			width = 8;
			height = 16;
		}

		c->hot.x = 0;
		c->hot.y = 0;
		c->size.x = width;
		c->size.y = height;

		memset(c->bits, 0xff, sizeof(c->bits));
		memset(c->mask, 0, sizeof(c->mask));

		for (i = 0, j = width; j >= 0; j -= 8, i++) {
			c->mask[i][height-2] = (j >= 8) ? 0xff : (0xff << (8 - j));
			c->mask[i][height-1] = (j >= 8) ? 0xff : (0xff << (8 - j));
		}

		pm2v_set_cursor_color(fb, cursor_color_map, cursor_color_map, cursor_color_map);
		pm2v_set_cursor_shape(fb);
	}
	return 1;
}
#endif /* PM2FB_HW_CURSOR */

/***************************************************************************
 * Begin of public functions
 ***************************************************************************/

#ifdef MODULE
static void pm2fb_cleanup(void) {
	struct pm2fb_info* i = &fb_info;

	unregister_framebuffer((struct fb_info *)i);
	pm2fb_reset(i);

	UNMAP(i->regions.v_fb, i->regions.fb_size);
	release_mem_region(i->regions.p_fb, i->regions.fb_size);

	UNMAP(i->regions.v_regs, PM2_REGS_SIZE);
	release_mem_region(i->regions.p_regs, PM2_REGS_SIZE);

	if (board_table[i->board].cleanup)
		board_table[i->board].cleanup(i);
}
#endif /* MODULE */

int __init pm2fb_init(void){

	MOD_INC_USE_COUNT;
	memset(&fb_info, 0, sizeof(fb_info));
	memcpy(&fb_info.current_par, &pm2fb_options.user_mode, sizeof(fb_info.current_par));
	if (!pm2fb_conf(&fb_info)) {
		MOD_DEC_USE_COUNT;
		return -ENXIO;
	}
	pm2fb_reset(&fb_info);
	fb_info.disp.scrollmode=SCROLL_YNOMOVE;
	fb_info.gen.parsize=sizeof(struct pm2fb_par);
	fb_info.gen.fbhw=&pm2fb_hwswitch;
	strcpy(fb_info.gen.info.modename, permedia2_name);
	fb_info.gen.info.flags=FBINFO_FLAG_DEFAULT;
	fb_info.gen.info.fbops=&pm2fb_ops;
	fb_info.gen.info.disp=&fb_info.disp;
	strcpy(fb_info.gen.info.fontname, pm2fb_options.font);
	fb_info.gen.info.switch_con=&fbgen_switch;
	fb_info.gen.info.updatevar=&fbgen_update_var;
	fb_info.gen.info.blank=&fbgen_blank;
	fbgen_get_var(&fb_info.disp.var, -1, &fb_info.gen.info);
	fbgen_do_set_var(&fb_info.disp.var, 1, &fb_info.gen);
	fbgen_set_disp(-1, &fb_info.gen);
	fbgen_install_cmap(0, &fb_info.gen);
	if (register_framebuffer(&fb_info.gen.info)<0) {
		printk(KERN_ERR "pm2fb: unable to register.\n");
		MOD_DEC_USE_COUNT;
		return -EINVAL;
	}
	printk(KERN_INFO "fb%d: %s (%s), using %uK of video memory.\n",
				GET_FB_IDX(fb_info.gen.info.node),
				board_table[fb_info.board].name,
				permedia2_name,
				(u32 )(fb_info.regions.fb_size>>10));
	return 0;
}

static void __init pm2fb_mode_setup(char* options){
	int i;

	for (i=0; user_mode[i].name[0] &&
		strcmp(options, user_mode[i].name); i++);
	if (user_mode[i].name[0]) {
		memcpy(&pm2fb_options.user_mode, &user_mode[i].par,
					sizeof(pm2fb_options.user_mode));
		pm2fb_options.flags |= OPTF_USER;
	}
}

static void __init pm2fb_font_setup(char* options){

	strncpy(pm2fb_options.font, options, sizeof(pm2fb_options.font));
	pm2fb_options.font[sizeof(pm2fb_options.font)-1]='\0';
}

int __init pm2fb_setup(char* options){
	char* next;

	while (options) {
		if ((next=strchr(options, ',')))
			*(next++)='\0';
		if (!strncmp(options, "font:", 5))
			pm2fb_font_setup(options+5);
		else if (!strncmp(options, "mode:", 5))
			pm2fb_mode_setup(options+5);
		else if (!strcmp(options, "ypan"))
			pm2fb_options.flags |= OPTF_YPAN;
		else if (!strcmp(options, "oldmem"))
			pm2fb_options.flags |= OPTF_OLD_MEM;
		else if (!strcmp(options, "virtual"))
			pm2fb_options.flags |= OPTF_VIRTUAL;
		else if (!strcmp(options, "noblink"))
			curblink = 0;
		options=next;
	}
	return 0;
}

/***************************************************************************
 * Begin of module functions
 ***************************************************************************/

#ifdef MODULE

MODULE_LICENSE("GPL");

static char *mode = NULL;

MODULE_PARM(mode, "s");

int __init init_module(void) {

	if (mode) pm2fb_mode_setup(mode);
	return pm2fb_init();
}

void cleanup_module(void) {

	pm2fb_cleanup();
}
#endif /* MODULE */

/***************************************************************************
 * That's all folks!
 ***************************************************************************/
