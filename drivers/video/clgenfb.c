/*
 * drivers/video/clgenfb.c - driver for Cirrus Logic chipsets
 *
 * Copyright 1999-2001 Jeff Garzik <jgarzik@pobox.com>
 *
 * Contributors (thanks, all!)
 *
 *      Jeff Rugen:
 *      Major contributions;  Motorola PowerStack (PPC and PCI) support,
 *      GD54xx, 1280x1024 mode support, change MCLK based on VCLK.
 *
 *	Geert Uytterhoeven:
 *	Excellent code review.
 *
 *	Lars Hecking:
 *	Amiga updates and testing.
 *
 *	Cliff Matthews <ctm@ardi.com>:
 *	16bpp fix for CL-GD7548 (uses info from XFree86 4.2.0 source)
 *
 * Original clgenfb author:  Frank Neumann
 *
 * Based on retz3fb.c and clgen.c:
 *      Copyright (C) 1997 Jes Sorensen
 *      Copyright (C) 1996 Frank Neumann
 *
 ***************************************************************
 *
 * Format this code with GNU indent '-kr -i8 -pcs' options.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#define CLGEN_VERSION "1.9.9.1"

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
#include <asm/pgtable.h>

#ifdef CONFIG_ZORRO
#include <linux/zorro.h>
#endif
#ifdef CONFIG_PCI
#include <linux/pci.h>
#endif
#ifdef CONFIG_AMIGA
#include <asm/amigahw.h>
#endif
#ifdef CONFIG_ALL_PPC
#include <asm/processor.h>
#define isPReP (_machine == _MACH_prep)
#else
#define isPReP 0
#endif

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "clgenfb.h"
#include "vga.h"


/*****************************************************************
 *
 * debugging and utility macros
 *
 */

/* enable debug output? */
/* #define CLGEN_DEBUG 1 */

/* disable runtime assertions? */
/* #define CLGEN_NDEBUG */

/* debug output */
#ifdef CLGEN_DEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/* debugging assertions */
#ifndef CLGEN_NDEBUG
#define assert(expr) \
        if(!(expr)) { \
        printk( "Assertion failed! %s,%s,%s,line=%d\n",\
        #expr,__FILE__,__FUNCTION__,__LINE__); \
        }
#else
#define assert(expr)
#endif

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE  1
#define FALSE 0

#define MB_ (1024*1024)
#define KB_ (1024)

#define MAX_NUM_BOARDS 7


/*****************************************************************
 *
 * chipset information
 *
 */

/* board types */
typedef enum {
	BT_NONE = 0,
	BT_SD64,
	BT_PICCOLO,
	BT_PICASSO,
	BT_SPECTRUM,
	BT_PICASSO4,	/* GD5446 */
	BT_ALPINE,	/* GD543x/4x */
	BT_GD5480,
	BT_LAGUNA,	/* GD546x */
} clgen_board_t;


/*
 * per-board-type information, used for enumerating and abstracting
 * chip-specific information
 * NOTE: MUST be in the same order as clgen_board_t in order to
 * use direct indexing on this array
 * NOTE: '__initdata' cannot be used as some of this info
 * is required at runtime.  Maybe separate into an init-only and
 * a run-time table?
 */
static const struct clgen_board_info_rec {
	clgen_board_t btype;	/* chipset enum, not strictly necessary, as
				 * clgen_board_info[] is directly indexed
				 * by this value */
	char *name;		/* ASCII name of chipset */
	long maxclock;		/* maximum video clock */
	unsigned init_sr07 : 1;	/* init SR07 during init_vgachip() */
	unsigned init_sr1f : 1; /* write SR1F during init_vgachip() */
	unsigned scrn_start_bit19 : 1; /* construct bit 19 of screen start address */

	/* initial SR07 value, then for each mode */
	unsigned char sr07;
	unsigned char sr07_1bpp;
	unsigned char sr07_1bpp_mux;
	unsigned char sr07_8bpp;
	unsigned char sr07_8bpp_mux;

	unsigned char sr1f;	/* SR1F VGA initial register value */
} clgen_board_info[] = {
	{ BT_NONE, }, /* dummy record */
	{ BT_SD64,
		"CL SD64",
		140000,		/* the SD64/P4 have a higher max. videoclock */
		TRUE,
		TRUE,
		TRUE,
		0xF0,
		0xF0,
		0,		/* unused, does not multiplex */
		0xF1,
		0,		/* unused, does not multiplex */
		0x20 },
	{ BT_PICCOLO,
		"CL Piccolo",
		90000,
		TRUE,
		TRUE,
		FALSE,
		0x80,
		0x80,
		0,		/* unused, does not multiplex */
		0x81,
		0,		/* unused, does not multiplex */
		0x22 },
	{ BT_PICASSO,
		"CL Picasso",
		90000,
		TRUE,
		TRUE,
		FALSE,
		0x20,
		0x20,
		0,		/* unused, does not multiplex */
		0x21,
		0,		/* unused, does not multiplex */
		0x22 },
	{ BT_SPECTRUM,
		"CL Spectrum",
		90000,
		TRUE,
		TRUE,
		FALSE,
		0x80,
		0x80,
		0,		/* unused, does not multiplex */
		0x81,
		0,		/* unused, does not multiplex */
		0x22 },
	{ BT_PICASSO4,
		"CL Picasso4",
		140000,		/* the SD64/P4 have a higher max. videoclock */
		TRUE,
		FALSE,
		TRUE,
		0x20,
		0x20,
		0,		/* unused, does not multiplex */
		0x21,
		0,		/* unused, does not multiplex */
		0 },
	{ BT_ALPINE,
		"CL Alpine",
		110000,		/* 135100 for some, 85500 for others */
		TRUE,
		TRUE,
		TRUE,
		0xA0,
		0xA1,
		0xA7,
		0xA1,
		0xA7,
		0x1C },
	{ BT_GD5480,
		"CL GD5480",
		90000,
		TRUE,
		TRUE,
		TRUE,
		0x10,
		0x11,
		0,		/* unused, does not multiplex */
		0x11,
		0,		/* unused, does not multiplex */
		0x1C },
	{ BT_LAGUNA,
		"CL Laguna",
		135100,
		FALSE,
		FALSE,
		TRUE,
		0,		/* unused */
		0,		/* unused */
		0,		/* unused */
		0,		/* unused */
		0,		/* unused */
		0 },		/* unused */
};


#ifdef CONFIG_PCI
/* the list of PCI devices for which we probe, and the
 * order in which we do it */
static const struct {
	clgen_board_t btype;
	const char *nameOverride; /* XXX unused... for now */
	unsigned short device;
} clgen_pci_probe_list[] __initdata = {
	{ BT_ALPINE, NULL, PCI_DEVICE_ID_CIRRUS_5436 },
	{ BT_ALPINE, NULL, PCI_DEVICE_ID_CIRRUS_5434_8 },
	{ BT_ALPINE, NULL, PCI_DEVICE_ID_CIRRUS_5434_4 },
	{ BT_ALPINE, NULL, PCI_DEVICE_ID_CIRRUS_5430 }, /* GD-5440 has identical id */
	{ BT_ALPINE, NULL, PCI_DEVICE_ID_CIRRUS_7543 },
	{ BT_ALPINE, NULL, PCI_DEVICE_ID_CIRRUS_7548 },
	{ BT_GD5480, NULL, PCI_DEVICE_ID_CIRRUS_5480 }, /* MacPicasso probably */
	{ BT_PICASSO4, NULL, PCI_DEVICE_ID_CIRRUS_5446 }, /* Picasso 4 is a GD5446 */
	{ BT_LAGUNA, "CL Laguna", PCI_DEVICE_ID_CIRRUS_5462 },
	{ BT_LAGUNA, "CL Laguna 3D", PCI_DEVICE_ID_CIRRUS_5464 },
	{ BT_LAGUNA, "CL Laguna 3DA", PCI_DEVICE_ID_CIRRUS_5465 },
};
#endif /* CONFIG_PCI */


#ifdef CONFIG_ZORRO
static const struct {
	clgen_board_t btype;
	zorro_id id, id2;
	unsigned long size;
} clgen_zorro_probe_list[] __initdata = {
	{ BT_SD64,
		ZORRO_PROD_HELFRICH_SD64_RAM,
		ZORRO_PROD_HELFRICH_SD64_REG,
		0x400000 },
	{ BT_PICCOLO,
		ZORRO_PROD_HELFRICH_PICCOLO_RAM,
		ZORRO_PROD_HELFRICH_PICCOLO_REG,
		0x200000 },
	{ BT_PICASSO,
		ZORRO_PROD_VILLAGE_TRONIC_PICASSO_II_II_PLUS_RAM,
		ZORRO_PROD_VILLAGE_TRONIC_PICASSO_II_II_PLUS_REG,
		0x200000 },
	{ BT_SPECTRUM,
		ZORRO_PROD_GVP_EGS_28_24_SPECTRUM_RAM,
		ZORRO_PROD_GVP_EGS_28_24_SPECTRUM_REG,
		0x200000 },
	{ BT_PICASSO4,
		ZORRO_PROD_VILLAGE_TRONIC_PICASSO_IV_Z3,
		0,
		0x400000 },
};
#endif /* CONFIG_ZORRO */



struct clgenfb_par {
	struct fb_var_screeninfo var;

	__u32 line_length;	/* in BYTES! */
	__u32 visual;
	__u32 type;

	long freq;
	long nom;
	long den;
	long div;
	long multiplexing;
	long mclk;
	long divMCLK;

	long HorizRes;		/* The x resolution in pixel */
	long HorizTotal;
	long HorizDispEnd;
	long HorizBlankStart;
	long HorizBlankEnd;
	long HorizSyncStart;
	long HorizSyncEnd;

	long VertRes;		/* the physical y resolution in scanlines */
	long VertTotal;
	long VertDispEnd;
	long VertSyncStart;
	long VertSyncEnd;
	long VertBlankStart;
	long VertBlankEnd;
};



#ifdef CLGEN_DEBUG
typedef enum {
        CRT,
        SEQ
} clgen_dbg_reg_class_t;
#endif                          /* CLGEN_DEBUG */




/* info about board */
struct clgenfb_info {
	struct fb_info_gen gen;

	caddr_t fbmem;
	caddr_t regs;
	caddr_t mem;
	unsigned long size;
	clgen_board_t btype;
	int smallboard;
	unsigned char SFR;	/* Shadow of special function register */

	unsigned long fbmem_phys;
	unsigned long fbregs_phys;

	struct clgenfb_par currentmode;

	struct { u8 red, green, blue, pad; } palette[256];

	union {
#ifdef FBCON_HAS_CFB16
		u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB24
		u32 cfb24[16];
#endif
#ifdef FBCON_HAS_CFB32
		u32 cfb32[16];
#endif
	} fbcon_cmap;

#ifdef CONFIG_ZORRO
	unsigned long board_addr,
		      board_size;
#endif

#ifdef CONFIG_PCI
	struct pci_dev *pdev;
#define IS_7548(x) ((x)->pdev->device == PCI_DEVICE_ID_CIRRUS_7548)
#else
#define IS_7548(x) (FALSE)
#endif
};




static struct display disp;

static struct clgenfb_info boards[MAX_NUM_BOARDS];	/* the boards */

static unsigned clgen_def_mode = 1;
static int noaccel = 0;



/*
 *    Predefined Video Modes
 */

static const struct {
	const char *name;
	struct fb_var_screeninfo var;
} clgenfb_predefined[] __initdata =

{
	{"Autodetect",		/* autodetect mode */
	 {0}
	},

	{"640x480",		/* 640x480, 31.25 kHz, 60 Hz, 25 MHz PixClock */
	 {
		 640, 480, 640, 480, 0, 0, 8, 0,
		 {0, 8, 0},
		 {0, 8, 0},
		 {0, 8, 0},
		 {0, 0, 0},
	       0, 0, -1, -1, FB_ACCEL_NONE, 40000, 48, 16, 32, 8, 96, 4,
     FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
	 }
	},

	{"800x600",		/* 800x600, 48 kHz, 76 Hz, 50 MHz PixClock */
	 {
		 800, 600, 800, 600, 0, 0, 8, 0,
		 {0, 8, 0},
		 {0, 8, 0},
		 {0, 8, 0},
		 {0, 0, 0},
	       0, 0, -1, -1, FB_ACCEL_NONE, 20000, 128, 16, 24, 2, 96, 6,
     0, FB_VMODE_NONINTERLACED
	 }
	},

	/*
	   Modeline from XF86Config:
	   Mode "1024x768" 80  1024 1136 1340 1432  768 770 774 805
	 */
	{"1024x768",		/* 1024x768, 55.8 kHz, 70 Hz, 80 MHz PixClock */
		{
			1024, 768, 1024, 768, 0, 0, 8, 0,
			{0, 8, 0},
			{0, 8, 0},
			{0, 8, 0},
			{0, 0, 0},
	      0, 0, -1, -1, FB_ACCEL_NONE, 12500, 144, 32, 30, 2, 192, 6,
     0, FB_VMODE_NONINTERLACED
		}
	}
};

#define NUM_TOTAL_MODES    ARRAY_SIZE(clgenfb_predefined)
static struct fb_var_screeninfo clgenfb_default;

/*
 *    Frame Buffer Name
 */

static const char *clgenfb_name = "CLgen";

/****************************************************************************/
/**** BEGIN PROTOTYPES ******************************************************/


/*--- Interface used by the world ------------------------------------------*/
int clgenfb_init (void);
int clgenfb_setup (char *options);

static int clgenfb_open (struct fb_info *info, int user);
static int clgenfb_release (struct fb_info *info, int user);

/* function table of the above functions */
static struct fb_ops clgenfb_ops = {
	owner:		THIS_MODULE,
	fb_open:	clgenfb_open,
	fb_release:	clgenfb_release,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
};

/*--- Hardware Specific Routines -------------------------------------------*/
static void clgen_detect (void);
static int clgen_encode_fix (struct fb_fix_screeninfo *fix, const void *par,
			     struct fb_info_gen *info);
static int clgen_decode_var (const struct fb_var_screeninfo *var, void *par,
			     struct fb_info_gen *info);
static int clgen_encode_var (struct fb_var_screeninfo *var, const void *par,
			     struct fb_info_gen *info);
static void clgen_get_par (void *par, struct fb_info_gen *info);
static void clgen_set_par (const void *par, struct fb_info_gen *info);
static int clgen_getcolreg (unsigned regno, unsigned *red, unsigned *green,
			    unsigned *blue, unsigned *transp,
			    struct fb_info *info);
static int clgen_setcolreg (unsigned regno, unsigned red, unsigned green,
			    unsigned blue, unsigned transp,
			    struct fb_info *info);
static int clgen_pan_display (const struct fb_var_screeninfo *var,
			      struct fb_info_gen *info);
static int clgen_blank (int blank_mode, struct fb_info_gen *info);

static void clgen_set_disp (const void *par, struct display *disp,
			    struct fb_info_gen *info);

/* function table of the above functions */
static struct fbgen_hwswitch clgen_hwswitch =
{
	clgen_detect,
	clgen_encode_fix,
	clgen_decode_var,
	clgen_encode_var,
	clgen_get_par,
	clgen_set_par,
	clgen_getcolreg,
	clgen_setcolreg,
	clgen_pan_display,
	clgen_blank,
	clgen_set_disp
};

/* Text console acceleration */

#ifdef FBCON_HAS_CFB8
static void fbcon_clgen8_bmove (struct display *p, int sy, int sx,
				int dy, int dx, int height, int width);
static void fbcon_clgen8_clear (struct vc_data *conp, struct display *p,
				int sy, int sx, int height, int width);

static struct display_switch fbcon_clgen_8 = {
	setup:		fbcon_cfb8_setup,
	bmove:		fbcon_clgen8_bmove,
	clear:		fbcon_clgen8_clear,
	putc:		fbcon_cfb8_putc,
	putcs:		fbcon_cfb8_putcs,
	revc:		fbcon_cfb8_revc,
	clear_margins:	fbcon_cfb8_clear_margins,
	fontwidthmask:	FONTWIDTH (4) | FONTWIDTH (8) | FONTWIDTH (12) | FONTWIDTH (16)
};
#endif
#ifdef FBCON_HAS_CFB16
static void fbcon_clgen16_bmove (struct display *p, int sy, int sx,
				 int dy, int dx, int height, int width);
static void fbcon_clgen16_clear (struct vc_data *conp, struct display *p,
				 int sy, int sx, int height, int width);
static struct display_switch fbcon_clgen_16 = {
	setup:		fbcon_cfb16_setup,
	bmove:		fbcon_clgen16_bmove,
	clear:		fbcon_clgen16_clear,
	putc:		fbcon_cfb16_putc,
	putcs:		fbcon_cfb16_putcs,
	revc:		fbcon_cfb16_revc,
	clear_margins:	fbcon_cfb16_clear_margins,
	fontwidthmask:	FONTWIDTH (4) | FONTWIDTH (8) | FONTWIDTH (12) | FONTWIDTH (16)
};
#endif
#ifdef FBCON_HAS_CFB32
static void fbcon_clgen32_bmove (struct display *p, int sy, int sx,
				 int dy, int dx, int height, int width);
static void fbcon_clgen32_clear (struct vc_data *conp, struct display *p,
				 int sy, int sx, int height, int width);
static struct display_switch fbcon_clgen_32 = {
	setup:		fbcon_cfb32_setup,
	bmove:		fbcon_clgen32_bmove,
	clear:		fbcon_clgen32_clear,
	putc:		fbcon_cfb32_putc,
	putcs:		fbcon_cfb32_putcs,
	revc:		fbcon_cfb32_revc,
	clear_margins:	fbcon_cfb32_clear_margins,
	fontwidthmask:	FONTWIDTH (4) | FONTWIDTH (8) | FONTWIDTH (12) | FONTWIDTH (16)
};
#endif



/*--- Internal routines ----------------------------------------------------*/
static void init_vgachip (struct clgenfb_info *fb_info);
static void switch_monitor (struct clgenfb_info *fb_info, int on);
static void WGen (const struct clgenfb_info *fb_info,
		  int regnum, unsigned char val);
static unsigned char RGen (const struct clgenfb_info *fb_info, int regnum);
static void AttrOn (const struct clgenfb_info *fb_info);
static void WHDR (const struct clgenfb_info *fb_info, unsigned char val);
static void WSFR (struct clgenfb_info *fb_info, unsigned char val);
static void WSFR2 (struct clgenfb_info *fb_info, unsigned char val);
static void WClut (struct clgenfb_info *fb_info, unsigned char regnum, unsigned char red,
		   unsigned char green,
		   unsigned char blue);
#if 0
static void RClut (struct clgenfb_info *fb_info, unsigned char regnum, unsigned char *red,
		   unsigned char *green,
		   unsigned char *blue);
#endif
static void clgen_WaitBLT (caddr_t regbase);
static void clgen_BitBLT (caddr_t regbase, u_short curx, u_short cury,
			  u_short destx, u_short desty,
			  u_short width, u_short height,
			  u_short line_length);
static void clgen_RectFill (struct clgenfb_info *fb_info, u_short x, u_short y,
			    u_short width, u_short height,
			    u_char color, u_short line_length);

static void bestclock (long freq, long *best,
		       long *nom, long *den,
		       long *div, long maxfreq);

#ifdef CLGEN_DEBUG
static void clgen_dump (void);
static void clgen_dbg_reg_dump (caddr_t regbase);
static void clgen_dbg_print_regs (caddr_t regbase, clgen_dbg_reg_class_t reg_class,...);
static void clgen_dbg_print_byte (const char *name, unsigned char val);
#endif /* CLGEN_DEBUG */

/*** END   PROTOTYPES ********************************************************/
/*****************************************************************************/
/*** BEGIN Interface Used by the World ***************************************/

static int opencount = 0;

/*--- Open /dev/fbx ---------------------------------------------------------*/
static int clgenfb_open (struct fb_info *info, int user)
{
	if (opencount++ == 0)
		switch_monitor ((struct clgenfb_info *) info, 1);
	return 0;
}

/*--- Close /dev/fbx --------------------------------------------------------*/
static int clgenfb_release (struct fb_info *info, int user)
{
	if (--opencount == 0)
		switch_monitor ((struct clgenfb_info *) info, 0);
	return 0;
}

/**** END   Interface used by the World *************************************/
/****************************************************************************/
/**** BEGIN Hardware specific Routines **************************************/

static void clgen_detect (void)
{
	DPRINTK ("ENTER\n");
	DPRINTK ("EXIT\n");
}

static int clgen_encode_fix (struct fb_fix_screeninfo *fix, const void *par,
			     struct fb_info_gen *info)
{
	struct clgenfb_par *_par = (struct clgenfb_par *) par;
	struct clgenfb_info *_info = (struct clgenfb_info *) info;

	DPRINTK ("ENTER\n");

	memset (fix, 0, sizeof (struct fb_fix_screeninfo));
	strcpy (fix->id, clgenfb_name);

	if (_info->btype == BT_GD5480) {
		/* Select proper byte-swapping aperture */
		switch (_par->var.bits_per_pixel) {
		case 1:
		case 8:
			fix->smem_start = _info->fbmem_phys;
			break;
		case 16:
			fix->smem_start = _info->fbmem_phys + 1 * MB_;
			break;
		case 24:
		case 32:
			fix->smem_start = _info->fbmem_phys + 2 * MB_;
			break;
		}
	} else {
		fix->smem_start = _info->fbmem_phys;
	}

	/* monochrome: only 1 memory plane */
	/* 8 bit and above: Use whole memory area */
	fix->smem_len = _par->var.bits_per_pixel == 1 ? _info->size / 4
	    : _info->size;
	fix->type = _par->type;
	fix->type_aux = 0;
	fix->visual = _par->visual;
	fix->xpanstep = 1;
	fix->ypanstep = 1;
	fix->ywrapstep = 0;
	fix->line_length = _par->line_length;

	/* FIXME: map region at 0xB8000 if available, fill in here */
	fix->mmio_start = 0;
	fix->mmio_len = 0;
	fix->accel = FB_ACCEL_NONE;

	DPRINTK ("EXIT\n");
	return 0;
}



/* Get a good MCLK value */
static long clgen_get_mclk (long freq, int bpp, long *div)
{
	long mclk;

	assert (div != NULL);

	/* Calculate MCLK, in case VCLK is high enough to require > 50MHz.
	 * Assume a 64-bit data path for now.  The formula is:
	 * ((B * PCLK * 2)/W) * 1.2
	 * B = bytes per pixel, PCLK = pixclock, W = data width in bytes */
	mclk = ((bpp / 8) * freq * 2) / 4;
	mclk = (mclk * 12) / 10;
	if (mclk < 50000)
		mclk = 50000;
	DPRINTK ("Use MCLK of %ld kHz\n", mclk);

	/* Calculate value for SR1F.  Multiply by 2 so we can round up. */
	mclk = ((mclk * 16) / 14318);
	mclk = (mclk + 1) / 2;
	DPRINTK ("Set SR1F[5:0] to 0x%lx\n", mclk);

	/* Determine if we should use MCLK instead of VCLK, and if so, what we
	   * should divide it by to get VCLK */
	switch (freq) {
	case 24751 ... 25249:
		*div = 2;
		DPRINTK ("Using VCLK = MCLK/2\n");
		break;
	case 49501 ... 50499:
		*div = 1;
		DPRINTK ("Using VCLK = MCLK\n");
		break;
	default:
		*div = 0;
		break;
	}

	return mclk;
}

static int clgen_decode_var (const struct fb_var_screeninfo *var, void *par,
			     struct fb_info_gen *info)
{
	long freq;
	long maxclock;
	int xres, hfront, hsync, hback;
	int yres, vfront, vsync, vback;
	int nom, den;		/* translyting from pixels->bytes */
	int i;
	static struct {
		int xres, yres;
	} modes[] = { {
			1600, 1280
	}, {
		1280, 1024
	}, {
		1024, 768
	},
	{
		800, 600
	}, {
		640, 480
	}, {
		-1, -1
	}
	};

	struct clgenfb_par *_par = (struct clgenfb_par *) par;
	struct clgenfb_info *fb_info = (struct clgenfb_info *) info;

	assert (var != NULL);
	assert (par != NULL);
	assert (info != NULL);

	DPRINTK ("ENTER\n");

	DPRINTK ("Requested: %dx%dx%d\n", var->xres, var->yres, var->bits_per_pixel);
	DPRINTK ("  virtual: %dx%d\n", var->xres_virtual, var->yres_virtual);
	DPRINTK ("   offset: (%d,%d)\n", var->xoffset, var->yoffset);
	DPRINTK ("grayscale: %d\n", var->grayscale);

	memset (par, 0, sizeof (struct clgenfb_par));

	_par->var = *var;

	switch (var->bits_per_pixel) {
	case 1:
		nom = 4;
		den = 8;
		break;		/* 8 pixel per byte, only 1/4th of mem usable */
	case 2 ... 8:
		_par->var.bits_per_pixel = 8;
		nom = 1;
		den = 1;
		break;		/* 1 pixel == 1 byte */
	case 9 ... 16:
		_par->var.bits_per_pixel = 16;
		nom = 2;
		den = 1;
		break;		/* 2 bytes per pixel */
	case 17 ... 24:
		_par->var.bits_per_pixel = 24;
		nom = 3;
		den = 1;
		break;		/* 3 bytes per pixel */
	case 25 ... 32:
		_par->var.bits_per_pixel = 32;
		nom = 4;
		den = 1;
		break;		/* 4 bytes per pixel */
	default:
		printk ("clgen: mode %dx%dx%d rejected...color depth not supported.\n",
			var->xres, var->yres, var->bits_per_pixel);
		DPRINTK ("EXIT - EINVAL error\n");
		return -EINVAL;
	}

	if (_par->var.xres * nom / den * _par->var.yres > fb_info->size) {
		printk ("clgen: mode %dx%dx%d rejected...resolution too high to fit into video memory!\n",
			var->xres, var->yres, var->bits_per_pixel);
		DPRINTK ("EXIT - EINVAL error\n");
		return -EINVAL;
	}
	/* use highest possible virtual resolution */
	if (_par->var.xres_virtual == -1 &&
	    _par->var.yres_virtual == -1) {
		printk ("clgen: using maximum available virtual resolution\n");
		for (i = 0; modes[i].xres != -1; i++) {
			if (modes[i].xres * nom / den * modes[i].yres < fb_info->size / 2)
				break;
		}
		if (modes[i].xres == -1) {
			printk ("clgen: could not find a virtual resolution that fits into video memory!!\n");
			DPRINTK ("EXIT - EINVAL error\n");
			return -EINVAL;
		}
		_par->var.xres_virtual = modes[i].xres;
		_par->var.yres_virtual = modes[i].yres;

		printk ("clgen: virtual resolution set to maximum of %dx%d\n",
			_par->var.xres_virtual, _par->var.yres_virtual);
	} else if (_par->var.xres_virtual == -1) {
		/* FIXME: maximize X virtual resolution only */
	} else if (_par->var.yres_virtual == -1) {
		/* FIXME: maximize Y virtual resolution only */
	}
	if (_par->var.xoffset < 0)
		_par->var.xoffset = 0;
	if (_par->var.yoffset < 0)
		_par->var.yoffset = 0;

	/* truncate xoffset and yoffset to maximum if too high */
	if (_par->var.xoffset > _par->var.xres_virtual - _par->var.xres)
		_par->var.xoffset = _par->var.xres_virtual - _par->var.xres - 1;

	if (_par->var.yoffset > _par->var.yres_virtual - _par->var.yres)
		_par->var.yoffset = _par->var.yres_virtual - _par->var.yres - 1;

	switch (_par->var.bits_per_pixel) {
	case 1:
		_par->line_length = _par->var.xres_virtual / 8;
		_par->visual = FB_VISUAL_MONO10;
		break;

	case 8:
		_par->line_length = _par->var.xres_virtual;
		_par->visual = FB_VISUAL_PSEUDOCOLOR;
		_par->var.red.offset = 0;
		_par->var.red.length = 6;
		_par->var.green.offset = 0;
		_par->var.green.length = 6;
		_par->var.blue.offset = 0;
		_par->var.blue.length = 6;
		break;

	case 16:
		_par->line_length = _par->var.xres_virtual * 2;
		_par->visual = FB_VISUAL_DIRECTCOLOR;
		if(isPReP) {
			_par->var.red.offset = 2;
			_par->var.green.offset = -3;
			_par->var.blue.offset = 8;
		} else {
			_par->var.red.offset = 10;
			_par->var.green.offset = 5;
			_par->var.blue.offset = 0;
		}
		_par->var.red.length = 5;
		_par->var.green.length = 5;
		_par->var.blue.length = 5;
		break;

	case 24:
		_par->line_length = _par->var.xres_virtual * 3;
		_par->visual = FB_VISUAL_DIRECTCOLOR;
		if(isPReP) {
			_par->var.red.offset = 8;
			_par->var.green.offset = 16;
			_par->var.blue.offset = 24;
		} else {
			_par->var.red.offset = 16;
			_par->var.green.offset = 8;
			_par->var.blue.offset = 0;
		}
		_par->var.red.length = 8;
		_par->var.green.length = 8;
		_par->var.blue.length = 8;
		break;

	case 32:
		_par->line_length = _par->var.xres_virtual * 4;
		_par->visual = FB_VISUAL_DIRECTCOLOR;
		if(isPReP) {
			_par->var.red.offset = 8;
			_par->var.green.offset = 16;
			_par->var.blue.offset = 24;
		} else {
			_par->var.red.offset = 16;
			_par->var.green.offset = 8;
			_par->var.blue.offset = 0;
		}
		_par->var.red.length = 8;
		_par->var.green.length = 8;
		_par->var.blue.length = 8;
		break;

	default:
		DPRINTK("Unsupported bpp size: %d\n", _par->var.bits_per_pixel);
		assert (FALSE);
		/* should never occur */
		break;
	}

	_par->var.red.msb_right =
	    _par->var.green.msb_right =
	    _par->var.blue.msb_right =
	    _par->var.transp.offset =
	    _par->var.transp.length =
	    _par->var.transp.msb_right = 0;

	_par->type = FB_TYPE_PACKED_PIXELS;

	/* convert from ps to kHz */
	freq = 1000000000 / var->pixclock;

	DPRINTK ("desired pixclock: %ld kHz\n", freq);

	if (IS_7548(fb_info))
		maxclock = 80100;
	else
		maxclock = clgen_board_info[fb_info->btype].maxclock;
	_par->multiplexing = 0;

	/* If the frequency is greater than we can support, we might be able
	 * to use multiplexing for the video mode */
	if (freq > maxclock) {
		switch (fb_info->btype) {
		case BT_ALPINE:
		case BT_GD5480:
			_par->multiplexing = 1;
			break;

		default:
			printk (KERN_WARNING "clgen: ERROR: Frequency greater than maxclock (%ld kHz)\n", maxclock);
			DPRINTK ("EXIT - return -EINVAL\n");
			return -EINVAL;
		}
	}
#if 0
	/* TODO: If we have a 1MB 5434, we need to put ourselves in a mode where
	 * the VCLK is double the pixel clock. */
	switch (var->bits_per_pixel) {
	case 16:
	case 32:
		if (_par->HorizRes <= 800)
			freq /= 2;	/* Xbh has this type of clock for 32-bit */
		break;
	}
#endif

	bestclock (freq, &_par->freq, &_par->nom, &_par->den, &_par->div,
		   maxclock);
	_par->mclk = clgen_get_mclk (freq, _par->var.bits_per_pixel, &_par->divMCLK);

	xres = _par->var.xres;
	hfront = _par->var.right_margin;
	hsync = _par->var.hsync_len;
	hback = _par->var.left_margin;

	yres = _par->var.yres;
	vfront = _par->var.lower_margin;
	vsync = _par->var.vsync_len;
	vback = _par->var.upper_margin;

	if (_par->var.vmode & FB_VMODE_DOUBLE) {
		yres *= 2;
		vfront *= 2;
		vsync *= 2;
		vback *= 2;
	} else if (_par->var.vmode & FB_VMODE_INTERLACED) {
		yres = (yres + 1) / 2;
		vfront = (vfront + 1) / 2;
		vsync = (vsync + 1) / 2;
		vback = (vback + 1) / 2;
	}
	_par->HorizRes = xres;
	_par->HorizTotal = (xres + hfront + hsync + hback) / 8 - 5;
	_par->HorizDispEnd = xres / 8 - 1;
	_par->HorizBlankStart = xres / 8;
	_par->HorizBlankEnd = _par->HorizTotal + 5;	/* does not count with "-5" */
	_par->HorizSyncStart = (xres + hfront) / 8 + 1;
	_par->HorizSyncEnd = (xres + hfront + hsync) / 8 + 1;

	_par->VertRes = yres;
	_par->VertTotal = yres + vfront + vsync + vback - 2;
	_par->VertDispEnd = yres - 1;
	_par->VertBlankStart = yres;
	_par->VertBlankEnd = _par->VertTotal;
	_par->VertSyncStart = yres + vfront - 1;
	_par->VertSyncEnd = yres + vfront + vsync - 1;

	if (_par->VertRes >= 1024) {
		_par->VertTotal /= 2;
		_par->VertSyncStart /= 2;
		_par->VertSyncEnd /= 2;
		_par->VertDispEnd /= 2;
	}
	if (_par->multiplexing) {
		_par->HorizTotal /= 2;
		_par->HorizSyncStart /= 2;
		_par->HorizSyncEnd /= 2;
		_par->HorizDispEnd /= 2;
	}
	if (_par->VertRes >= 1280) {
		printk (KERN_WARNING "clgen: ERROR: VerticalTotal >= 1280; special treatment required! (TODO)\n");
		DPRINTK ("EXIT - EINVAL error\n");
		return -EINVAL;
	}
	DPRINTK ("EXIT\n");
	return 0;
}


static int clgen_encode_var (struct fb_var_screeninfo *var, const void *par,
			     struct fb_info_gen *info)
{
	DPRINTK ("ENTER\n");

	*var = ((struct clgenfb_par *) par)->var;

	DPRINTK ("EXIT\n");
	return 0;
}

/* get current video mode */
static void clgen_get_par (void *par, struct fb_info_gen *info)
{
	struct clgenfb_par *_par = (struct clgenfb_par *) par;
	struct clgenfb_info *_info = (struct clgenfb_info *) info;

	DPRINTK ("ENTER\n");

	*_par = _info->currentmode;

	DPRINTK ("EXIT\n");
}

static void clgen_set_mclk (const struct clgenfb_info *fb_info, int val, int div)
{
	assert (fb_info != NULL);

	if (div == 2) {
		/* VCLK = MCLK/2 */
		unsigned char old = vga_rseq (fb_info->regs, CL_SEQR1E);
		vga_wseq (fb_info->regs, CL_SEQR1E, old | 0x1);
		vga_wseq (fb_info->regs, CL_SEQR1F, 0x40 | (val & 0x3f));
	} else if (div == 1) {
		/* VCLK = MCLK */
		unsigned char old = vga_rseq (fb_info->regs, CL_SEQR1E);
		vga_wseq (fb_info->regs, CL_SEQR1E, old & ~0x1);
		vga_wseq (fb_info->regs, CL_SEQR1F, 0x40 | (val & 0x3f));
	} else {
		vga_wseq (fb_info->regs, CL_SEQR1F, val & 0x3f);
	}
}

/*************************************************************************
	clgen_set_par()

	actually writes the values for a new video mode into the hardware,
**************************************************************************/
static void clgen_set_par (const void *par, struct fb_info_gen *info)
{
	unsigned char tmp;
	int offset = 0;
	struct clgenfb_par *_par = (struct clgenfb_par *) par;
	struct clgenfb_info *fb_info = (struct clgenfb_info *) info;
	const struct clgen_board_info_rec *bi;

	DPRINTK ("ENTER\n");
	DPRINTK ("Requested mode: %dx%dx%d\n",
	       _par->var.xres, _par->var.yres, _par->var.bits_per_pixel);
	DPRINTK ("pixclock: %d\n", _par->var.pixclock);

	bi = &clgen_board_info[fb_info->btype];


	/* unlock register VGA_CRTC_H_TOTAL..CRT7 */
	vga_wcrt (fb_info->regs, VGA_CRTC_V_SYNC_END, 0x20);	/* previously: 0x00) */

	/* if debugging is enabled, all parameters get output before writing */
	DPRINTK ("CRT0: %ld\n", _par->HorizTotal);
	vga_wcrt (fb_info->regs, VGA_CRTC_H_TOTAL, _par->HorizTotal);

	DPRINTK ("CRT1: %ld\n", _par->HorizDispEnd);
	vga_wcrt (fb_info->regs, VGA_CRTC_H_DISP, _par->HorizDispEnd);

	DPRINTK ("CRT2: %ld\n", _par->HorizBlankStart);
	vga_wcrt (fb_info->regs, VGA_CRTC_H_BLANK_START, _par->HorizBlankStart);

	DPRINTK ("CRT3: 128+%ld\n", _par->HorizBlankEnd % 32);	/*  + 128: Compatible read */
	vga_wcrt (fb_info->regs, VGA_CRTC_H_BLANK_END, 128 + (_par->HorizBlankEnd % 32));

	DPRINTK ("CRT4: %ld\n", _par->HorizSyncStart);
	vga_wcrt (fb_info->regs, VGA_CRTC_H_SYNC_START, _par->HorizSyncStart);

	tmp = _par->HorizSyncEnd % 32;
	if (_par->HorizBlankEnd & 32)
		tmp += 128;
	DPRINTK ("CRT5: %d\n", tmp);
	vga_wcrt (fb_info->regs, VGA_CRTC_H_SYNC_END, tmp);

	DPRINTK ("CRT6: %ld\n", _par->VertTotal & 0xff);
	vga_wcrt (fb_info->regs, VGA_CRTC_V_TOTAL, (_par->VertTotal & 0xff));

	tmp = 16;		/* LineCompare bit #9 */
	if (_par->VertTotal & 256)
		tmp |= 1;
	if (_par->VertDispEnd & 256)
		tmp |= 2;
	if (_par->VertSyncStart & 256)
		tmp |= 4;
	if (_par->VertBlankStart & 256)
		tmp |= 8;
	if (_par->VertTotal & 512)
		tmp |= 32;
	if (_par->VertDispEnd & 512)
		tmp |= 64;
	if (_par->VertSyncStart & 512)
		tmp |= 128;
	DPRINTK ("CRT7: %d\n", tmp);
	vga_wcrt (fb_info->regs, VGA_CRTC_OVERFLOW, tmp);

	tmp = 0x40;		/* LineCompare bit #8 */
	if (_par->VertBlankStart & 512)
		tmp |= 0x20;
	if (_par->var.vmode & FB_VMODE_DOUBLE)
		tmp |= 0x80;
	DPRINTK ("CRT9: %d\n", tmp);
	vga_wcrt (fb_info->regs, VGA_CRTC_MAX_SCAN, tmp);

	DPRINTK ("CRT10: %ld\n", _par->VertSyncStart & 0xff);
	vga_wcrt (fb_info->regs, VGA_CRTC_V_SYNC_START, (_par->VertSyncStart & 0xff));

	DPRINTK ("CRT11: 64+32+%ld\n", _par->VertSyncEnd % 16);
	vga_wcrt (fb_info->regs, VGA_CRTC_V_SYNC_END, (_par->VertSyncEnd % 16 + 64 + 32));

	DPRINTK ("CRT12: %ld\n", _par->VertDispEnd & 0xff);
	vga_wcrt (fb_info->regs, VGA_CRTC_V_DISP_END, (_par->VertDispEnd & 0xff));

	DPRINTK ("CRT15: %ld\n", _par->VertBlankStart & 0xff);
	vga_wcrt (fb_info->regs, VGA_CRTC_V_BLANK_START, (_par->VertBlankStart & 0xff));

	DPRINTK ("CRT16: %ld\n", _par->VertBlankEnd & 0xff);
	vga_wcrt (fb_info->regs, VGA_CRTC_V_BLANK_END, (_par->VertBlankEnd & 0xff));

	DPRINTK ("CRT18: 0xff\n");
	vga_wcrt (fb_info->regs, VGA_CRTC_LINE_COMPARE, 0xff);

	tmp = 0;
	if (_par->var.vmode & FB_VMODE_INTERLACED)
		tmp |= 1;
	if (_par->HorizBlankEnd & 64)
		tmp |= 16;
	if (_par->HorizBlankEnd & 128)
		tmp |= 32;
	if (_par->VertBlankEnd & 256)
		tmp |= 64;
	if (_par->VertBlankEnd & 512)
		tmp |= 128;

	DPRINTK ("CRT1a: %d\n", tmp);
	vga_wcrt (fb_info->regs, CL_CRT1A, tmp);

	/* set VCLK0 */
	/* hardware RefClock: 14.31818 MHz */
	/* formula: VClk = (OSC * N) / (D * (1+P)) */
	/* Example: VClk = (14.31818 * 91) / (23 * (1+1)) = 28.325 MHz */

	vga_wseq (fb_info->regs, CL_SEQRB, _par->nom);
	tmp = _par->den << 1;
	if (_par->div != 0)
		tmp |= 1;

	if ((fb_info->btype == BT_SD64) ||
	    (fb_info->btype == BT_ALPINE) ||
	    (fb_info->btype == BT_GD5480))
		tmp |= 0x80;	/* 6 bit denom; ONLY 5434!!! (bugged me 10 days) */

	DPRINTK ("CL_SEQR1B: %ld\n", (long) tmp);
	vga_wseq (fb_info->regs, CL_SEQR1B, tmp);

	if (_par->VertRes >= 1024)
		/* 1280x1024 */
		vga_wcrt (fb_info->regs, VGA_CRTC_MODE, 0xc7);
	else
		/* mode control: VGA_CRTC_START_HI enable, ROTATE(?), 16bit
		 * address wrap, no compat. */
		vga_wcrt (fb_info->regs, VGA_CRTC_MODE, 0xc3);

/* HAEH?        vga_wcrt (fb_info->regs, VGA_CRTC_V_SYNC_END, 0x20);  * previously: 0x00  unlock VGA_CRTC_H_TOTAL..CRT7 */

	/* don't know if it would hurt to also program this if no interlaced */
	/* mode is used, but I feel better this way.. :-) */
	if (_par->var.vmode & FB_VMODE_INTERLACED)
		vga_wcrt (fb_info->regs, VGA_CRTC_REGS, _par->HorizTotal / 2);
	else
		vga_wcrt (fb_info->regs, VGA_CRTC_REGS, 0x00);	/* interlace control */

	vga_wseq (fb_info->regs, VGA_SEQ_CHARACTER_MAP, 0);

	/* adjust horizontal/vertical sync type (low/high) */
	tmp = 0x03;		/* enable display memory & CRTC I/O address for color mode */
	if (_par->var.sync & FB_SYNC_HOR_HIGH_ACT)
		tmp |= 0x40;
	if (_par->var.sync & FB_SYNC_VERT_HIGH_ACT)
		tmp |= 0x80;
	WGen (fb_info, VGA_MIS_W, tmp);

	vga_wcrt (fb_info->regs, VGA_CRTC_PRESET_ROW, 0);	/* Screen A Preset Row-Scan register */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_START, 0);	/* text cursor on and start line */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_END, 31);	/* text cursor end line */

	/******************************************************
	 *
	 * 1 bpp
	 *
	 */

	/* programming for different color depths */
	if (_par->var.bits_per_pixel == 1) {
		DPRINTK ("clgen: preparing for 1 bit deep display\n");
		vga_wgfx (fb_info->regs, VGA_GFX_MODE, 0);	/* mode register */

		/* SR07 */
		switch (fb_info->btype) {
		case BT_SD64:
		case BT_PICCOLO:
		case BT_PICASSO:
		case BT_SPECTRUM:
		case BT_PICASSO4:
		case BT_ALPINE:
		case BT_GD5480:
			DPRINTK (" (for GD54xx)\n");
			vga_wseq (fb_info->regs, CL_SEQR7,
				  _par->multiplexing ?
				  	bi->sr07_1bpp_mux : bi->sr07_1bpp);
			break;

		case BT_LAGUNA:
			DPRINTK (" (for GD546x)\n");
			vga_wseq (fb_info->regs, CL_SEQR7,
				vga_rseq (fb_info->regs, CL_SEQR7) & ~0x01);
			break;

		default:
			printk (KERN_WARNING "clgen: unknown Board\n");
			break;
		}

		/* Extended Sequencer Mode */
		switch (fb_info->btype) {
		case BT_SD64:
			/* setting the SEQRF on SD64 is not necessary (only during init) */
			DPRINTK ("(for SD64)\n");
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x1a);		/*  MCLK select */
			break;

		case BT_PICCOLO:
			DPRINTK ("(for Piccolo)\n");
/* ### ueberall 0x22? */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* ##vorher 1c MCLK select */
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* evtl d0 bei 1 bit? avoid FIFO underruns..? */
			break;

		case BT_PICASSO:
			DPRINTK ("(for Picasso)\n");
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* ##vorher 22 MCLK select */
			vga_wseq (fb_info->regs, CL_SEQRF, 0xd0);	/* ## vorher d0 avoid FIFO underruns..? */
			break;

		case BT_SPECTRUM:
			DPRINTK ("(for Spectrum)\n");
/* ### ueberall 0x22? */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* ##vorher 1c MCLK select */
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* evtl d0? avoid FIFO underruns..? */
			break;

		case BT_PICASSO4:
		case BT_ALPINE:
		case BT_GD5480:
		case BT_LAGUNA:
			DPRINTK (" (for GD54xx)\n");
			/* do nothing */
			break;

		default:
			printk (KERN_WARNING "clgen: unknown Board\n");
			break;
		}

		WGen (fb_info, VGA_PEL_MSK, 0x01);	/* pixel mask: pass-through for first plane */
		if (_par->multiplexing)
			WHDR (fb_info, 0x4a);	/* hidden dac reg: 1280x1024 */
		else
			WHDR (fb_info, 0);	/* hidden dac: nothing */
		vga_wseq (fb_info->regs, VGA_SEQ_MEMORY_MODE, 0x06);	/* memory mode: odd/even, ext. memory */
		vga_wseq (fb_info->regs, VGA_SEQ_PLANE_WRITE, 0x01);	/* plane mask: only write to first plane */
		offset = _par->var.xres_virtual / 16;
	}

	/******************************************************
	 *
	 * 8 bpp
	 *
	 */

	else if (_par->var.bits_per_pixel == 8) {
		DPRINTK ("clgen: preparing for 8 bit deep display\n");
		switch (fb_info->btype) {
		case BT_SD64:
		case BT_PICCOLO:
		case BT_PICASSO:
		case BT_SPECTRUM:
		case BT_PICASSO4:
		case BT_ALPINE:
		case BT_GD5480:
			DPRINTK (" (for GD54xx)\n");
			vga_wseq (fb_info->regs, CL_SEQR7,
				  _par->multiplexing ?
				  	bi->sr07_8bpp_mux : bi->sr07_8bpp);
			break;

		case BT_LAGUNA:
			DPRINTK (" (for GD546x)\n");
			vga_wseq (fb_info->regs, CL_SEQR7,
				vga_rseq (fb_info->regs, CL_SEQR7) | 0x01);
			break;

		default:
			printk (KERN_WARNING "clgen: unknown Board\n");
			break;
		}

		switch (fb_info->btype) {
		case BT_SD64:
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x1d);		/* MCLK select */
			break;

		case BT_PICCOLO:
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* ### vorher 1c MCLK select */
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			break;

		case BT_PICASSO:
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* ### vorher 1c MCLK select */
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			break;

		case BT_SPECTRUM:
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* ### vorher 1c MCLK select */
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			break;

		case BT_PICASSO4:
#ifdef CONFIG_ZORRO
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb8);	/* ### INCOMPLETE!! */
#endif
/*          vga_wseq (fb_info->regs, CL_SEQR1F, 0x1c); */
			break;

		case BT_ALPINE:
			DPRINTK (" (for GD543x)\n");
			clgen_set_mclk (fb_info, _par->mclk, _par->divMCLK);
			/* We already set SRF and SR1F */
			break;

		case BT_GD5480:
		case BT_LAGUNA:
			DPRINTK (" (for GD54xx)\n");
			/* do nothing */
			break;

		default:
			printk (KERN_WARNING "clgen: unknown Board\n");
			break;
		}

		vga_wgfx (fb_info->regs, VGA_GFX_MODE, 64);	/* mode register: 256 color mode */
		WGen (fb_info, VGA_PEL_MSK, 0xff);	/* pixel mask: pass-through all planes */
		if (_par->multiplexing)
			WHDR (fb_info, 0x4a);	/* hidden dac reg: 1280x1024 */
		else
			WHDR (fb_info, 0);	/* hidden dac: nothing */
		vga_wseq (fb_info->regs, VGA_SEQ_MEMORY_MODE, 0x0a);	/* memory mode: chain4, ext. memory */
		vga_wseq (fb_info->regs, VGA_SEQ_PLANE_WRITE, 0xff);	/* plane mask: enable writing to all 4 planes */
		offset = _par->var.xres_virtual / 8;
	}

	/******************************************************
	 *
	 * 16 bpp
	 *
	 */

	else if (_par->var.bits_per_pixel == 16) {
		DPRINTK ("clgen: preparing for 16 bit deep display\n");
		switch (fb_info->btype) {
		case BT_SD64:
			vga_wseq (fb_info->regs, CL_SEQR7, 0xf7);	/* Extended Sequencer Mode: 256c col. mode */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x1e);		/* MCLK select */
			break;

		case BT_PICCOLO:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x87);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* MCLK select */
			break;

		case BT_PICASSO:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x27);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* MCLK select */
			break;

		case BT_SPECTRUM:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x87);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* MCLK select */
			break;

		case BT_PICASSO4:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x27);
/*          vga_wseq (fb_info->regs, CL_SEQR1F, 0x1c);  */
			break;

		case BT_ALPINE:
			DPRINTK (" (for GD543x)\n");
			if (IS_7548(fb_info)) {
				vga_wseq (fb_info->regs, CL_SEQR7, 
					  (vga_rseq (fb_info->regs, CL_SEQR7) & 0xE0)
					  | 0x17);
				WHDR (fb_info, 0xC1);
			} else {
				if (_par->HorizRes >= 1024)
					vga_wseq (fb_info->regs, CL_SEQR7, 0xa7);
				else
					vga_wseq (fb_info->regs, CL_SEQR7, 0xa3);
			}	
			clgen_set_mclk (fb_info, _par->mclk, _par->divMCLK);
			break;

		case BT_GD5480:
			DPRINTK (" (for GD5480)\n");
			vga_wseq (fb_info->regs, CL_SEQR7, 0x17);
			/* We already set SRF and SR1F */
			break;

		case BT_LAGUNA:
			DPRINTK (" (for GD546x)\n");
			vga_wseq (fb_info->regs, CL_SEQR7,
				vga_rseq (fb_info->regs, CL_SEQR7) & ~0x01);
			break;

		default:
			printk (KERN_WARNING "CLGEN: unknown Board\n");
			break;
		}

		vga_wgfx (fb_info->regs, VGA_GFX_MODE, 64);	/* mode register: 256 color mode */
		WGen (fb_info, VGA_PEL_MSK, 0xff);	/* pixel mask: pass-through all planes */
#ifdef CONFIG_PCI
		WHDR (fb_info, 0xc0);	/* Copy Xbh */
#elif defined(CONFIG_ZORRO)
		/* FIXME: CONFIG_PCI and CONFIG_ZORRO may be defined both */
		WHDR (fb_info, 0xa0);	/* hidden dac reg: nothing special */
#endif
		vga_wseq (fb_info->regs, VGA_SEQ_MEMORY_MODE, 0x0a);	/* memory mode: chain4, ext. memory */
		vga_wseq (fb_info->regs, VGA_SEQ_PLANE_WRITE, 0xff);	/* plane mask: enable writing to all 4 planes */
		offset = _par->var.xres_virtual / 4;
	}

	/******************************************************
	 *
	 * 32 bpp
	 *
	 */

	else if (_par->var.bits_per_pixel == 32) {
		DPRINTK ("clgen: preparing for 24/32 bit deep display\n");
		switch (fb_info->btype) {
		case BT_SD64:
			vga_wseq (fb_info->regs, CL_SEQR7, 0xf9);	/* Extended Sequencer Mode: 256c col. mode */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x1e);		/* MCLK select */
			break;

		case BT_PICCOLO:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x85);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* MCLK select */
			break;

		case BT_PICASSO:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x25);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* MCLK select */
			break;

		case BT_SPECTRUM:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x85);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);	/* Fast Page-Mode writes */
			vga_wseq (fb_info->regs, CL_SEQR1F, 0x22);		/* MCLK select */
			break;

		case BT_PICASSO4:
			vga_wseq (fb_info->regs, CL_SEQR7, 0x25);
/*          vga_wseq (fb_info->regs, CL_SEQR1F, 0x1c);  */
			break;

		case BT_ALPINE:
			DPRINTK (" (for GD543x)\n");
			vga_wseq (fb_info->regs, CL_SEQR7, 0xa9);
			clgen_set_mclk (fb_info, _par->mclk, _par->divMCLK);
			break;

		case BT_GD5480:
			DPRINTK (" (for GD5480)\n");
			vga_wseq (fb_info->regs, CL_SEQR7, 0x19);
			/* We already set SRF and SR1F */
			break;

		case BT_LAGUNA:
			DPRINTK (" (for GD546x)\n");
			vga_wseq (fb_info->regs, CL_SEQR7,
				vga_rseq (fb_info->regs, CL_SEQR7) & ~0x01);
			break;

		default:
			printk (KERN_WARNING "clgen: unknown Board\n");
			break;
		}

		vga_wgfx (fb_info->regs, VGA_GFX_MODE, 64);	/* mode register: 256 color mode */
		WGen (fb_info, VGA_PEL_MSK, 0xff);	/* pixel mask: pass-through all planes */
		WHDR (fb_info, 0xc5);	/* hidden dac reg: 8-8-8 mode (24 or 32) */
		vga_wseq (fb_info->regs, VGA_SEQ_MEMORY_MODE, 0x0a);	/* memory mode: chain4, ext. memory */
		vga_wseq (fb_info->regs, VGA_SEQ_PLANE_WRITE, 0xff);	/* plane mask: enable writing to all 4 planes */
		offset = _par->var.xres_virtual / 4;
	}

	/******************************************************
	 *
	 * unknown/unsupported bpp
	 *
	 */

	else {
		printk (KERN_ERR "clgen: What's this?? requested color depth == %d.\n",
			_par->var.bits_per_pixel);
	}

	if (IS_7548(fb_info)) {
		vga_wseq (fb_info->regs, CL_SEQR2D, 
			vga_rseq (fb_info->regs, CL_SEQR2D) | 0xC0);
	}

	vga_wcrt (fb_info->regs, VGA_CRTC_OFFSET, offset & 0xff);
	tmp = 0x22;
	if (offset & 0x100)
		tmp |= 0x10;	/* offset overflow bit */

	vga_wcrt (fb_info->regs, CL_CRT1B, tmp);	/* screen start addr #16-18, fastpagemode cycles */

	if (fb_info->btype == BT_SD64 ||
	    fb_info->btype == BT_PICASSO4 ||
	    fb_info->btype == BT_ALPINE ||
	    fb_info->btype == BT_GD5480)
		vga_wcrt (fb_info->regs, CL_CRT1D, 0x00);	/* screen start address bit 19 */

	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_HI, 0);	/* text cursor location high */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_LO, 0);	/* text cursor location low */
	vga_wcrt (fb_info->regs, VGA_CRTC_UNDERLINE, 0);	/* underline row scanline = at very bottom */

	vga_wattr (fb_info->regs, VGA_ATC_MODE, 1);	/* controller mode */
	vga_wattr (fb_info->regs, VGA_ATC_OVERSCAN, 0);		/* overscan (border) color */
	vga_wattr (fb_info->regs, VGA_ATC_PLANE_ENABLE, 15);	/* color plane enable */
	vga_wattr (fb_info->regs, CL_AR33, 0);	/* pixel panning */
	vga_wattr (fb_info->regs, VGA_ATC_COLOR_PAGE, 0);	/* color select */

	/* [ EGS: SetOffset(); ] */
	/* From SetOffset(): Turn on VideoEnable bit in Attribute controller */
	AttrOn (fb_info);

	vga_wgfx (fb_info->regs, VGA_GFX_SR_VALUE, 0);	/* set/reset register */
	vga_wgfx (fb_info->regs, VGA_GFX_SR_ENABLE, 0);		/* set/reset enable */
	vga_wgfx (fb_info->regs, VGA_GFX_COMPARE_VALUE, 0);	/* color compare */
	vga_wgfx (fb_info->regs, VGA_GFX_DATA_ROTATE, 0);	/* data rotate */
	vga_wgfx (fb_info->regs, VGA_GFX_PLANE_READ, 0);	/* read map select */
	vga_wgfx (fb_info->regs, VGA_GFX_MISC, 1);	/* miscellaneous register */
	vga_wgfx (fb_info->regs, VGA_GFX_COMPARE_MASK, 15);	/* color don't care */
	vga_wgfx (fb_info->regs, VGA_GFX_BIT_MASK, 255);	/* bit mask */

	vga_wseq (fb_info->regs, CL_SEQR12, 0x0);	/* graphics cursor attributes: nothing special */

	/* finally, turn on everything - turn off "FullBandwidth" bit */
	/* also, set "DotClock%2" bit where requested */
	tmp = 0x01;

/*** FB_VMODE_CLOCK_HALVE in linux/fb.h not defined anymore ?
    if (var->vmode & FB_VMODE_CLOCK_HALVE)
	tmp |= 0x08;
*/

	vga_wseq (fb_info->regs, VGA_SEQ_CLOCK_MODE, tmp);
	DPRINTK ("CL_SEQR1: %d\n", tmp);

	fb_info->currentmode = *_par;

	DPRINTK ("virtual offset: (%d,%d)\n", _par->var.xoffset, _par->var.yoffset);
	/* pan to requested offset */
	clgen_pan_display (&fb_info->currentmode.var, (struct fb_info_gen *) fb_info);

#ifdef CLGEN_DEBUG
	clgen_dump ();
#endif

	DPRINTK ("EXIT\n");
	return;
}


static int clgen_getcolreg (unsigned regno, unsigned *red, unsigned *green,
			    unsigned *blue, unsigned *transp,
			    struct fb_info *info)
{
    struct clgenfb_info *fb_info = (struct clgenfb_info *)info;

    if (regno > 255)
	return 1;
    *red = fb_info->palette[regno].red;
    *green = fb_info->palette[regno].green;
    *blue = fb_info->palette[regno].blue;
    *transp = 0;
    return 0;
}


static int clgen_setcolreg (unsigned regno, unsigned red, unsigned green,
			    unsigned blue, unsigned transp,
			    struct fb_info *info)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) info;

	if (regno > 255)
		return -EINVAL;

#ifdef FBCON_HAS_CFB8
	switch (fb_info->currentmode.var.bits_per_pixel) {
	case 8:
		/* "transparent" stuff is completely ignored. */
		WClut (fb_info, regno, red >> 10, green >> 10, blue >> 10);
		break;
	default:
		/* do nothing */
		break;
	}
#endif	/* FBCON_HAS_CFB8 */

	fb_info->palette[regno].red = red;
	fb_info->palette[regno].green = green;
	fb_info->palette[regno].blue = blue;

	if (regno >= 16)
		return 0;

	switch (fb_info->currentmode.var.bits_per_pixel) {

#ifdef FBCON_HAS_CFB16
	case 16:
		assert (regno < 16);
		if(isPReP) {
			fb_info->fbcon_cmap.cfb16[regno] =
			    ((red & 0xf800) >> 9) |
			    ((green & 0xf800) >> 14) |
			    ((green & 0xf800) << 2) |
			    ((blue & 0xf800) >> 3);
		} else {
			fb_info->fbcon_cmap.cfb16[regno] =
			    ((red & 0xf800) >> 1) |
			    ((green & 0xf800) >> 6) |
			    ((blue & 0xf800) >> 11);
		}
#endif /* FBCON_HAS_CFB16 */

#ifdef FBCON_HAS_CFB24
	case 24:
		assert (regno < 16);
		fb_info->fbcon_cmap.cfb24[regno] =
			(red   << fb_info->currentmode.var.red.offset)   |
			(green << fb_info->currentmode.var.green.offset) |
			(blue  << fb_info->currentmode.var.blue.offset);
		break;
#endif /* FBCON_HAS_CFB24 */

#ifdef FBCON_HAS_CFB32
	case 32:
		assert (regno < 16);
		if(isPReP) {
			fb_info->fbcon_cmap.cfb32[regno] =
			    ((red & 0xff00)) |
			    ((green & 0xff00) << 8) |
			    ((blue & 0xff00) << 16);
		} else {
			fb_info->fbcon_cmap.cfb32[regno] =
			    ((red & 0xff00) << 8) |
			    ((green & 0xff00)) |
			    ((blue & 0xff00) >> 8);
		}
		break;
#endif /* FBCON_HAS_CFB32 */
	default:
		/* do nothing */
		break;
	}

	return 0;
}

/*************************************************************************
	clgen_pan_display()

	performs display panning - provided hardware permits this
**************************************************************************/
static int clgen_pan_display (const struct fb_var_screeninfo *var,
			      struct fb_info_gen *info)
{
	int xoffset = 0;
	int yoffset = 0;
	unsigned long base;
	unsigned char tmp = 0, tmp2 = 0, xpix;
	struct clgenfb_info *fb_info = (struct clgenfb_info *) info;

	DPRINTK ("ENTER\n");

	/* no range checks for xoffset and yoffset,   */
	/* as fbgen_pan_display has already done this */

	fb_info->currentmode.var.xoffset = var->xoffset;
	fb_info->currentmode.var.yoffset = var->yoffset;

	xoffset = var->xoffset * fb_info->currentmode.var.bits_per_pixel / 8;
	yoffset = var->yoffset;

	base = yoffset * fb_info->currentmode.line_length + xoffset;

	if (fb_info->currentmode.var.bits_per_pixel == 1) {
		/* base is already correct */
		xpix = (unsigned char) (var->xoffset % 8);
	} else {
		base /= 4;
		xpix = (unsigned char) ((xoffset % 4) * 2);
	}

	/* lower 8 + 8 bits of screen start address */
	vga_wcrt (fb_info->regs, VGA_CRTC_START_LO, (unsigned char) (base & 0xff));
	vga_wcrt (fb_info->regs, VGA_CRTC_START_HI, (unsigned char) (base >> 8));

	/* construct bits 16, 17 and 18 of screen start address */
	if (base & 0x10000)
		tmp |= 0x01;
	if (base & 0x20000)
		tmp |= 0x04;
	if (base & 0x40000)
		tmp |= 0x08;

	tmp2 = (vga_rcrt (fb_info->regs, CL_CRT1B) & 0xf2) | tmp;	/* 0xf2 is %11110010, exclude tmp bits */
	vga_wcrt (fb_info->regs, CL_CRT1B, tmp2);

	/* construct bit 19 of screen start address */
	if (clgen_board_info[fb_info->btype].scrn_start_bit19) {
		tmp2 = 0;
		if (base & 0x80000)
			tmp2 = 0x80;
		vga_wcrt (fb_info->regs, CL_CRT1D, tmp2);
	}

	/* write pixel panning value to AR33; this does not quite work in 8bpp */
	/* ### Piccolo..? Will this work? */
	if (fb_info->currentmode.var.bits_per_pixel == 1)
		vga_wattr (fb_info->regs, CL_AR33, xpix);


	DPRINTK ("EXIT\n");
	return (0);
}


static int clgen_blank (int blank_mode, struct fb_info_gen *info)
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
	unsigned char val;
	static int current_mode = 0;
	struct clgenfb_info *fb_info = (struct clgenfb_info *) info;

	DPRINTK ("ENTER, blank mode = %d\n", blank_mode);

	if (current_mode == blank_mode) {
		DPRINTK ("EXIT, returning 0\n");
		return 0;
	}

	/* Undo current */
	switch (current_mode) {
	case 0:		/* Screen is normal */
		break;
	case 1:		/* Screen is blanked */
		val = vga_rseq (fb_info->regs, VGA_SEQ_CLOCK_MODE);
		vga_wseq (fb_info->regs, VGA_SEQ_CLOCK_MODE, val & 0xdf);	/* clear "FullBandwidth" bit */
		break;
	case 2:		/* vsync suspended */
	case 3:		/* hsync suspended */
	case 4:		/* sceen is powered down */
		vga_wgfx (fb_info->regs, CL_GRE, 0x00);
		break;
	default:
		DPRINTK ("EXIT, returning 1\n");
		return 1;
	}

	/* set new */
	switch (blank_mode) {
	case 0:		/* Unblank screen */
		break;
	case 1:		/* Blank screen */
		val = vga_rseq (fb_info->regs, VGA_SEQ_CLOCK_MODE);
		vga_wseq (fb_info->regs, VGA_SEQ_CLOCK_MODE, val | 0x20);	/* set "FullBandwidth" bit */
		break;
	case 2:		/* suspend vsync */
		vga_wgfx (fb_info->regs, CL_GRE, 0x04);
		break;
	case 3:		/* suspend hsync */
		vga_wgfx (fb_info->regs, CL_GRE, 0x02);
		break;
	case 4:		/* powerdown */
		vga_wgfx (fb_info->regs, CL_GRE, 0x06);
		break;
	default:
		DPRINTK ("EXIT, returning 1\n");
		return 1;
	}

	current_mode = blank_mode;
	DPRINTK ("EXIT, returning 0\n");
	return 0;
}
/**** END   Hardware specific Routines **************************************/
/****************************************************************************/
/**** BEGIN Internal Routines ***********************************************/

static void __init init_vgachip (struct clgenfb_info *fb_info)
{
	const struct clgen_board_info_rec *bi;

	DPRINTK ("ENTER\n");

	assert (fb_info != NULL);

	bi = &clgen_board_info[fb_info->btype];

	/* reset board globally */
	switch (fb_info->btype) {
	case BT_PICCOLO:
		WSFR (fb_info, 0x01);
		udelay (500);
		WSFR (fb_info, 0x51);
		udelay (500);
		break;
	case BT_PICASSO:
		WSFR2 (fb_info, 0xff);
		udelay (500);
		break;
	case BT_SD64:
	case BT_SPECTRUM:
		WSFR (fb_info, 0x1f);
		udelay (500);
		WSFR (fb_info, 0x4f);
		udelay (500);
		break;
	case BT_PICASSO4:
		vga_wcrt (fb_info->regs, CL_CRT51, 0x00);	/* disable flickerfixer */
		mdelay (100);
		vga_wgfx (fb_info->regs, CL_GR2F, 0x00);	/* from Klaus' NetBSD driver: */
		vga_wgfx (fb_info->regs, CL_GR33, 0x00);	/* put blitter into 542x compat */
		vga_wgfx (fb_info->regs, CL_GR31, 0x00);	/* mode */
		break;

	case BT_GD5480:
		vga_wgfx (fb_info->regs, CL_GR2F, 0x00);	/* from Klaus' NetBSD driver: */
		break;

	case BT_ALPINE:
		/* Nothing to do to reset the board. */
		break;

	default:
		printk (KERN_ERR "clgen: Warning: Unknown board type\n");
		break;
	}

	assert (fb_info->size > 0); /* make sure RAM size set by this point */

	/* assume it's a "large memory" board (2/4 MB) */
	fb_info->smallboard = FALSE;

	/* the P4 is not fully initialized here; I rely on it having been */
	/* inited under AmigaOS already, which seems to work just fine    */
	/* (Klaus advised to do it this way)                              */

	if (fb_info->btype != BT_PICASSO4) {
		WGen (fb_info, CL_VSSM, 0x10);	/* EGS: 0x16 */
		WGen (fb_info, CL_POS102, 0x01);
		WGen (fb_info, CL_VSSM, 0x08);	/* EGS: 0x0e */

		if (fb_info->btype != BT_SD64)
			WGen (fb_info, CL_VSSM2, 0x01);

		vga_wseq (fb_info->regs, CL_SEQR0, 0x03);	/* reset sequencer logic */

		vga_wseq (fb_info->regs, VGA_SEQ_CLOCK_MODE, 0x21);	/* FullBandwidth (video off) and 8/9 dot clock */
		WGen (fb_info, VGA_MIS_W, 0xc1);	/* polarity (-/-), disable access to display memory, VGA_CRTC_START_HI base address: color */

/*      vga_wgfx (fb_info->regs, CL_GRA, 0xce);    "magic cookie" - doesn't make any sense to me.. */
		vga_wseq (fb_info->regs, CL_SEQR6, 0x12);	/* unlock all extension registers */

		vga_wgfx (fb_info->regs, CL_GR31, 0x04);	/* reset blitter */

		switch (fb_info->btype) {
		case BT_GD5480:
			vga_wseq (fb_info->regs, CL_SEQRF, 0x98);
			break;
		case BT_ALPINE:
			break;
		case BT_SD64:
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb8);
			break;
		default:
			vga_wseq (fb_info->regs, CL_SEQR16, 0x0f);
			vga_wseq (fb_info->regs, CL_SEQRF, 0xb0);
			break;
		}
	}
	vga_wseq (fb_info->regs, VGA_SEQ_PLANE_WRITE, 0xff);	/* plane mask: nothing */
	vga_wseq (fb_info->regs, VGA_SEQ_CHARACTER_MAP, 0x00);	/* character map select: doesn't even matter in gx mode */
	vga_wseq (fb_info->regs, VGA_SEQ_MEMORY_MODE, 0x0e);	/* memory mode: chain-4, no odd/even, ext. memory */

	/* controller-internal base address of video memory */
	if (bi->init_sr07)
		vga_wseq (fb_info->regs, CL_SEQR7, bi->sr07);

	/*  vga_wseq (fb_info->regs, CL_SEQR8, 0x00); *//* EEPROM control: shouldn't be necessary to write to this at all.. */

	vga_wseq (fb_info->regs, CL_SEQR10, 0x00);		/* graphics cursor X position (incomplete; position gives rem. 3 bits */
	vga_wseq (fb_info->regs, CL_SEQR11, 0x00);		/* graphics cursor Y position (..."... ) */
	vga_wseq (fb_info->regs, CL_SEQR12, 0x00);		/* graphics cursor attributes */
	vga_wseq (fb_info->regs, CL_SEQR13, 0x00);		/* graphics cursor pattern address */

	/* writing these on a P4 might give problems..  */
	if (fb_info->btype != BT_PICASSO4) {
		vga_wseq (fb_info->regs, CL_SEQR17, 0x00);		/* configuration readback and ext. color */
		vga_wseq (fb_info->regs, CL_SEQR18, 0x02);		/* signature generator */
	}

	/* MCLK select etc. */
	if (bi->init_sr1f)
		vga_wseq (fb_info->regs, CL_SEQR1F, bi->sr1f);

	vga_wcrt (fb_info->regs, VGA_CRTC_PRESET_ROW, 0x00);	/* Screen A preset row scan: none */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_START, 0x20);	/* Text cursor start: disable text cursor */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_END, 0x00);	/* Text cursor end: - */
	vga_wcrt (fb_info->regs, VGA_CRTC_START_HI, 0x00);	/* Screen start address high: 0 */
	vga_wcrt (fb_info->regs, VGA_CRTC_START_LO, 0x00);	/* Screen start address low: 0 */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_HI, 0x00);	/* text cursor location high: 0 */
	vga_wcrt (fb_info->regs, VGA_CRTC_CURSOR_LO, 0x00);	/* text cursor location low: 0 */

	vga_wcrt (fb_info->regs, VGA_CRTC_UNDERLINE, 0x00);	/* Underline Row scanline: - */
	vga_wcrt (fb_info->regs, VGA_CRTC_MODE, 0xc3);	/* mode control: timing enable, byte mode, no compat modes */
	vga_wcrt (fb_info->regs, VGA_CRTC_LINE_COMPARE, 0x00);	/* Line Compare: not needed */
	/* ### add 0x40 for text modes with > 30 MHz pixclock */
	vga_wcrt (fb_info->regs, CL_CRT1B, 0x02);	/* ext. display controls: ext.adr. wrap */

	vga_wgfx (fb_info->regs, VGA_GFX_SR_VALUE, 0x00);	/* Set/Reset registes: - */
	vga_wgfx (fb_info->regs, VGA_GFX_SR_ENABLE, 0x00);	/* Set/Reset enable: - */
	vga_wgfx (fb_info->regs, VGA_GFX_COMPARE_VALUE, 0x00);	/* Color Compare: - */
	vga_wgfx (fb_info->regs, VGA_GFX_DATA_ROTATE, 0x00);	/* Data Rotate: - */
	vga_wgfx (fb_info->regs, VGA_GFX_PLANE_READ, 0x00);	/* Read Map Select: - */
	vga_wgfx (fb_info->regs, VGA_GFX_MODE, 0x00);	/* Mode: conf. for 16/4/2 color mode, no odd/even, read/write mode 0 */
	vga_wgfx (fb_info->regs, VGA_GFX_MISC, 0x01);	/* Miscellaneous: memory map base address, graphics mode */
	vga_wgfx (fb_info->regs, VGA_GFX_COMPARE_MASK, 0x0f);	/* Color Don't care: involve all planes */
	vga_wgfx (fb_info->regs, VGA_GFX_BIT_MASK, 0xff);	/* Bit Mask: no mask at all */
	if (fb_info->btype == BT_ALPINE)
		vga_wgfx (fb_info->regs, CL_GRB, 0x20);	/* (5434 can't have bit 3 set for bitblt) */
	else
		vga_wgfx (fb_info->regs, CL_GRB, 0x28);	/* Graphics controller mode extensions: finer granularity, 8byte data latches */

	vga_wgfx (fb_info->regs, CL_GRC, 0xff);	/* Color Key compare: - */
	vga_wgfx (fb_info->regs, CL_GRD, 0x00);	/* Color Key compare mask: - */
	vga_wgfx (fb_info->regs, CL_GRE, 0x00);	/* Miscellaneous control: - */
	/*  vga_wgfx (fb_info->regs, CL_GR10, 0x00); *//* Background color byte 1: - */
/*  vga_wgfx (fb_info->regs, CL_GR11, 0x00); */

	vga_wattr (fb_info->regs, VGA_ATC_PALETTE0, 0x00);	/* Attribute Controller palette registers: "identity mapping" */
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE1, 0x01);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE2, 0x02);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE3, 0x03);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE4, 0x04);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE5, 0x05);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE6, 0x06);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE7, 0x07);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE8, 0x08);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTE9, 0x09);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTEA, 0x0a);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTEB, 0x0b);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTEC, 0x0c);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTED, 0x0d);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTEE, 0x0e);
	vga_wattr (fb_info->regs, VGA_ATC_PALETTEF, 0x0f);

	vga_wattr (fb_info->regs, VGA_ATC_MODE, 0x01);	/* Attribute Controller mode: graphics mode */
	vga_wattr (fb_info->regs, VGA_ATC_OVERSCAN, 0x00);	/* Overscan color reg.: reg. 0 */
	vga_wattr (fb_info->regs, VGA_ATC_PLANE_ENABLE, 0x0f);	/* Color Plane enable: Enable all 4 planes */
/* ###  vga_wattr (fb_info->regs, CL_AR33, 0x00); * Pixel Panning: - */
	vga_wattr (fb_info->regs, VGA_ATC_COLOR_PAGE, 0x00);	/* Color Select: - */

	WGen (fb_info, VGA_PEL_MSK, 0xff);	/* Pixel mask: no mask */

	if (fb_info->btype != BT_ALPINE && fb_info->btype != BT_GD5480)
		WGen (fb_info, VGA_MIS_W, 0xc3);	/* polarity (-/-), enable display mem, VGA_CRTC_START_HI i/o base = color */

	vga_wgfx (fb_info->regs, CL_GR31, 0x04);	/* BLT Start/status: Blitter reset */
	vga_wgfx (fb_info->regs, CL_GR31, 0x00);	/* - " -           : "end-of-reset" */

	/* CLUT setup */
	WClut (fb_info, 0, 0x00, 0x00, 0x00);	/* background: black */
	WClut (fb_info, 1, 0x3f, 0x3f, 0x3f);	/* foreground: white */
	WClut (fb_info, 2, 0x00, 0x20, 0x00);
	WClut (fb_info, 3, 0x00, 0x20, 0x20);
	WClut (fb_info, 4, 0x20, 0x00, 0x00);
	WClut (fb_info, 5, 0x20, 0x00, 0x20);
	WClut (fb_info, 6, 0x20, 0x10, 0x00);
	WClut (fb_info, 7, 0x20, 0x20, 0x20);
	WClut (fb_info, 8, 0x10, 0x10, 0x10);
	WClut (fb_info, 9, 0x10, 0x10, 0x30);
	WClut (fb_info, 10, 0x10, 0x30, 0x10);
	WClut (fb_info, 11, 0x10, 0x30, 0x30);
	WClut (fb_info, 12, 0x30, 0x10, 0x10);
	WClut (fb_info, 13, 0x30, 0x10, 0x30);
	WClut (fb_info, 14, 0x30, 0x30, 0x10);
	WClut (fb_info, 15, 0x30, 0x30, 0x30);

	/* the rest a grey ramp */
	{
		int i;

		for (i = 16; i < 256; i++)
			WClut (fb_info, i, i >> 2, i >> 2, i >> 2);
	}


	/* misc... */
	WHDR (fb_info, 0);	/* Hidden DAC register: - */

	printk (KERN_INFO "clgen: This board has %ld bytes of DRAM memory\n", fb_info->size);
	DPRINTK ("EXIT\n");
	return;
}

static void switch_monitor (struct clgenfb_info *fb_info, int on)
{
#ifdef CONFIG_ZORRO /* only works on Zorro boards */
	static int IsOn = 0;	/* XXX not ok for multiple boards */

	DPRINTK ("ENTER\n");

	if (fb_info->btype == BT_PICASSO4)
		return;		/* nothing to switch */
	if (fb_info->btype == BT_ALPINE)
		return;		/* nothing to switch */
	if (fb_info->btype == BT_GD5480)
		return;		/* nothing to switch */
	if (fb_info->btype == BT_PICASSO) {
		if ((on && !IsOn) || (!on && IsOn))
			WSFR (fb_info, 0xff);

		DPRINTK ("EXIT\n");
		return;
	}
	if (on) {
		switch (fb_info->btype) {
		case BT_SD64:
			WSFR (fb_info, fb_info->SFR | 0x21);
			break;
		case BT_PICCOLO:
			WSFR (fb_info, fb_info->SFR | 0x28);
			break;
		case BT_SPECTRUM:
			WSFR (fb_info, 0x6f);
			break;
		default: /* do nothing */ break;
		}
	} else {
		switch (fb_info->btype) {
		case BT_SD64:
			WSFR (fb_info, fb_info->SFR & 0xde);
			break;
		case BT_PICCOLO:
			WSFR (fb_info, fb_info->SFR & 0xd7);
			break;
		case BT_SPECTRUM:
			WSFR (fb_info, 0x4f);
			break;
		default: /* do nothing */ break;
		}
	}

	DPRINTK ("EXIT\n");
#endif /* CONFIG_ZORRO */
}

static void clgen_set_disp (const void *par, struct display *disp,
			    struct fb_info_gen *info)
{
	struct clgenfb_par *_par = (struct clgenfb_par *) par;
	struct clgenfb_info *fb_info = (struct clgenfb_info *) info;
	int accel_text;

	DPRINTK ("ENTER\n");

	assert (_par != NULL);
	assert (fb_info != NULL);

	accel_text = _par->var.accel_flags & FB_ACCELF_TEXT;

	printk ("Cirrus Logic video mode: ");
	disp->screen_base = (char *) fb_info->fbmem;
	switch (_par->var.bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1:
		printk ("monochrome\n");
		if (fb_info->btype == BT_GD5480)
			disp->screen_base = (char *) fb_info->fbmem;
		disp->dispsw = &fbcon_mfb;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		printk ("8 bit color depth\n");
		if (fb_info->btype == BT_GD5480)
			disp->screen_base = (char *) fb_info->fbmem;
		if (accel_text)
			disp->dispsw = &fbcon_clgen_8;
		else
			disp->dispsw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		printk ("16 bit color depth\n");
		if (accel_text)
			disp->dispsw = &fbcon_clgen_16;
		else
			disp->dispsw = &fbcon_cfb16;
		if (fb_info->btype == BT_GD5480)
			disp->screen_base = (char *) fb_info->fbmem + 1 * MB_;
		disp->dispsw_data = fb_info->fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		printk ("24 bit color depth\n");
		disp->dispsw = &fbcon_cfb24;
		if (fb_info->btype == BT_GD5480)
			disp->screen_base = (char *) fb_info->fbmem + 2 * MB_;
		disp->dispsw_data = fb_info->fbcon_cmap.cfb24;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		printk ("32 bit color depth\n");
		if (accel_text)
			disp->dispsw = &fbcon_clgen_32;
		else
			disp->dispsw = &fbcon_cfb32;
		if (fb_info->btype == BT_GD5480)
			disp->screen_base = (char *) fb_info->fbmem + 2 * MB_;
		disp->dispsw_data = fb_info->fbcon_cmap.cfb32;
		break;
#endif

	default:
		printk ("unsupported color depth\n");
		disp->dispsw = &fbcon_dummy;
		disp->dispsw_data = NULL;
		break;
	}

	DPRINTK ("EXIT\n");
}

#ifdef FBCON_HAS_CFB8
static void fbcon_clgen8_bmove (struct display *p, int sy, int sx,
				int dy, int dx, int height, int width)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) p->fb_info;

	DPRINTK ("ENTER\n");

	sx *= fontwidth (p);
	sy *= fontheight (p);
	dx *= fontwidth (p);
	dy *= fontheight (p);
	width *= fontwidth (p);
	height *= fontheight (p);

	clgen_BitBLT (fb_info->regs, (unsigned short) sx, (unsigned short) sy,
		      (unsigned short) dx, (unsigned short) dy,
		      (unsigned short) width, (unsigned short) height,
		      fb_info->currentmode.line_length);

	DPRINTK ("EXIT\n");
}

static void fbcon_clgen8_clear (struct vc_data *conp, struct display *p,
				int sy, int sx, int height, int width)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) p->fb_info;
	unsigned short col;

	DPRINTK ("ENTER\n");

	sx *= fontwidth (p);
	sy *= fontheight (p);
	width *= fontwidth (p);
	height *= fontheight (p);

	col = attr_bgcol_ec (p, conp);
	col &= 0xff;

	clgen_RectFill (fb_info, (unsigned short) sx, (unsigned short) sy,
			(unsigned short) width, (unsigned short) height,
			col, fb_info->currentmode.line_length);

	DPRINTK ("EXIT\n");
}

#endif

#ifdef FBCON_HAS_CFB16
static void fbcon_clgen16_bmove (struct display *p, int sy, int sx,
				 int dy, int dx, int height, int width)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) p->fb_info;

	DPRINTK ("ENTER\n");

	sx *= fontwidth (p) * 2;	/* 2 bytes/pixel */
	sy *= fontheight (p);
	dx *= fontwidth (p) * 2;	/* 2 bytes/pixel */
	dy *= fontheight (p);
	width *= fontwidth (p) * 2;	/* 2 bytes/pixel */
	height *= fontheight (p);

	clgen_BitBLT (fb_info->regs, (unsigned short) sx, (unsigned short) sy,
		      (unsigned short) dx, (unsigned short) dy,
		      (unsigned short) width, (unsigned short) height,
		      fb_info->currentmode.line_length);

	DPRINTK ("EXIT\n");
}

static void fbcon_clgen16_clear (struct vc_data *conp, struct display *p,
				 int sy, int sx, int height, int width)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) p->fb_info;
	unsigned short col;

	DPRINTK ("ENTER\n");

	sx *= fontwidth (p) * 2;	/* 2 bytes/pixel */
	sy *= fontheight (p);
	width *= fontwidth (p) * 2;	/* 2 bytes/pixel? */
	height *= fontheight (p);

	col = attr_bgcol_ec (p, conp);
	col &= 0xff;

	clgen_RectFill (fb_info, (unsigned short) sx, (unsigned short) sy,
			(unsigned short) width, (unsigned short) height,
			col, fb_info->currentmode.line_length);

	DPRINTK ("EXIT\n");
}

#endif

#ifdef FBCON_HAS_CFB32
static void fbcon_clgen32_bmove (struct display *p, int sy, int sx,
				 int dy, int dx, int height, int width)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) p->fb_info;

	DPRINTK ("ENTER\n");

	sx *= fontwidth (p) * 4;	/* 4 bytes/pixel */
	sy *= fontheight (p);
	dx *= fontwidth (p) * 4;	/* 4 bytes/pixel */
	dy *= fontheight (p);
	width *= fontwidth (p) * 4;	/* 4 bytes/pixel */
	height *= fontheight (p);

	clgen_BitBLT (fb_info->regs, (unsigned short) sx, (unsigned short) sy,
		      (unsigned short) dx, (unsigned short) dy,
		      (unsigned short) width, (unsigned short) height,
		      fb_info->currentmode.line_length);

	DPRINTK ("EXIT\n");
}

static void fbcon_clgen32_clear (struct vc_data *conp, struct display *p,
				 int sy, int sx, int height, int width)
{
	struct clgenfb_info *fb_info = (struct clgenfb_info *) p->fb_info;

	unsigned short col;

	DPRINTK ("ENTER\n");

	sx *= fontwidth (p) * 4;	/* 4 bytes/pixel */
	sy *= fontheight (p);
	width *= fontwidth (p) * 4;	/* 4 bytes/pixel? */
	height *= fontheight (p);

	col = attr_bgcol_ec (p, conp);
	col &= 0xff;

	clgen_RectFill (fb_info, (unsigned short) sx, (unsigned short) sy,
			(unsigned short) width, (unsigned short) height,
			col, fb_info->currentmode.line_length);

	DPRINTK ("EXIT\n");
}

#endif				/* FBCON_HAS_CFB32 */




#ifdef CONFIG_ALL_PPC
#define PREP_VIDEO_BASE ((volatile unsigned long) 0xC0000000)
#define PREP_IO_BASE    ((volatile unsigned char *) 0x80000000)
static void __init get_prep_addrs (unsigned long *display, unsigned long *registers)
{
	DPRINTK ("ENTER\n");

	*display = PREP_VIDEO_BASE;
	*registers = (unsigned long) PREP_IO_BASE;

	DPRINTK ("EXIT\n");
}

#endif				/* CONFIG_ALL_PPC */




#ifdef CONFIG_PCI
static int release_io_ports = 0;

/* Pulled the logic from XFree86 Cirrus driver to get the memory size,
 * based on the DRAM bandwidth bit and DRAM bank switching bit.  This
 * works with 1MB, 2MB and 4MB configurations (which the Motorola boards
 * seem to have. */
static unsigned int __init clgen_get_memsize (caddr_t regbase)
{
	unsigned long mem;
	unsigned char SRF;

	DPRINTK ("ENTER\n");

	SRF = vga_rseq (regbase, CL_SEQRF);
	switch ((SRF & 0x18)) {
	    case 0x08: mem = 512 * 1024; break;
	    case 0x10: mem = 1024 * 1024; break;
		/* 64-bit DRAM data bus width; assume 2MB. Also indicates 2MB memory
		   * on the 5430. */
	    case 0x18: mem = 2048 * 1024; break;
	    default: printk ("CLgenfb: Unknown memory size!\n");
		mem = 1024 * 1024;
	}
	if (SRF & 0x80) {
		/* If DRAM bank switching is enabled, there must be twice as much
		   * memory installed. (4MB on the 5434) */
		mem *= 2;
	}
	/* TODO: Handling of GD5446/5480 (see XF86 sources ...) */
	return mem;

	DPRINTK ("EXIT\n");
}



static struct pci_dev * __init clgen_pci_dev_get (clgen_board_t *btype)
{
	struct pci_dev *pdev;
	int i;

	DPRINTK ("ENTER\n");

	for (i = 0; i < ARRAY_SIZE(clgen_pci_probe_list); i++) {
		pdev = NULL;
		while ((pdev = pci_find_device (PCI_VENDOR_ID_CIRRUS,
				clgen_pci_probe_list[i].device, pdev)) != NULL) {
			if (pci_enable_device(pdev) == 0) {
				*btype = clgen_pci_probe_list[i].btype;
				DPRINTK ("EXIT, returning pdev=%p\n", pdev);
				return pdev;
			}
		}
	}

	DPRINTK ("EXIT, returning NULL\n");
	return NULL;
}




static void __init get_pci_addrs (const struct pci_dev *pdev,
			   unsigned long *display, unsigned long *registers)
{
	assert (pdev != NULL);
	assert (display != NULL);
	assert (registers != NULL);

	DPRINTK ("ENTER\n");

	*display = 0;
	*registers = 0;

	/* This is a best-guess for now */

	if (pci_resource_flags(pdev, 0) & IORESOURCE_IO) {
		*display = pci_resource_start(pdev, 1);
		*registers = pci_resource_start(pdev, 0);
	} else {
		*display = pci_resource_start(pdev, 0);
		*registers = pci_resource_start(pdev, 1);
	}

	assert (*display != 0);

	DPRINTK ("EXIT\n");
}


static void __exit clgen_pci_unmap (struct clgenfb_info *info)
{
	iounmap (info->fbmem);
	release_mem_region(info->fbmem_phys, info->size);

#if 0 /* if system didn't claim this region, we would... */
	release_mem_region(0xA0000, 65535);
#endif

	if (release_io_ports)
		release_region(0x3C0, 32);
}


static int __init clgen_pci_setup (struct clgenfb_info *info,
				   clgen_board_t *btype)
{
	struct pci_dev *pdev;
	unsigned long board_addr, board_size;

	DPRINTK ("ENTER\n");

	pdev = clgen_pci_dev_get (btype);
	if (!pdev) {
		printk (KERN_INFO "clgenfb: couldn't find Cirrus Logic PCI device\n");
		DPRINTK ("EXIT, returning 1\n");
		return 1;
	}
	DPRINTK (" Found PCI device, base address 0 is 0x%lx, btype set to %d\n",
		 pdev->resource[0].start, *btype);
	DPRINTK (" base address 1 is 0x%lx\n", pdev->resource[1].start);

	info->pdev = pdev;

	if(isPReP) {
		/* Xbh does this, though 0 seems to be the init value */
		pcibios_write_config_dword (0, pdev->devfn, PCI_BASE_ADDRESS_0,
			0x00000000);

#ifdef CONFIG_ALL_PPC
		get_prep_addrs (&board_addr, &info->fbregs_phys);
#endif
	} else {
		DPRINTK ("Attempt to get PCI info for Cirrus Graphics Card\n");
		get_pci_addrs (pdev, &board_addr, &info->fbregs_phys);
	}

	DPRINTK ("Board address: 0x%lx, register address: 0x%lx\n", board_addr, info->fbregs_phys);

	if(isPReP) {
		/* PReP dies if we ioremap the IO registers, but it works w/out... */
		info->regs = (char *) info->fbregs_phys;
	} else
		info->regs = 0;		/* FIXME: this forces VGA.  alternatives? */

	if (*btype == BT_GD5480) {
		board_size = 32 * MB_;
	} else {
		board_size = clgen_get_memsize (info->regs);
	}

	if (!request_mem_region(board_addr, board_size, "clgenfb")) {
		printk(KERN_ERR "clgen: cannot reserve region 0x%lx, abort\n",
		       board_addr);
		return -1;
	}
#if 0 /* if the system didn't claim this region, we would... */
	if (!request_mem_region(0xA0000, 65535, "clgenfb")) {
		printk(KERN_ERR "clgen: cannot reserve region 0x%lx, abort\n",
		       0xA0000L);
		release_mem_region(board_addr, board_size);
		return -1;
	}
#endif
	if (request_region(0x3C0, 32, "clgenfb"))
		release_io_ports = 1;

	info->fbmem = ioremap (board_addr, board_size);
	info->fbmem_phys = board_addr;
	info->size = board_size;

	printk (" RAM (%lu kB) at 0x%lx, ", info->size / KB_, board_addr);

	printk ("Cirrus Logic chipset on PCI bus\n");

	DPRINTK ("EXIT, returning 0\n");
	return 0;
}
#endif				/* CONFIG_PCI */




#ifdef CONFIG_ZORRO
static int __init clgen_zorro_find (struct zorro_dev **z_o,
				    struct zorro_dev **z2_o,
				    clgen_board_t *btype, unsigned long *size)
{
	struct zorro_dev *z = NULL;
	int i;

	assert (z_o != NULL);
	assert (btype != NULL);

	for (i = 0; i < ARRAY_SIZE(clgen_zorro_probe_list); i++)
		if ((z = zorro_find_device(clgen_zorro_probe_list[i].id, NULL)))
			break;

	if (z) {
		*z_o = z;
		if (clgen_zorro_probe_list[i].id2)
			*z2_o = zorro_find_device(clgen_zorro_probe_list[i].id2, NULL);
		else
			*z2_o = NULL;

		*btype = clgen_zorro_probe_list[i].btype;
		*size = clgen_zorro_probe_list[i].size;

		printk (KERN_INFO "clgen: %s board detected; ",
			clgen_board_info[*btype].name);

		return 0;
	}

	printk (KERN_NOTICE "clgen: no supported board found.\n");
	return -1;
}


static void __exit clgen_zorro_unmap (struct clgenfb_info *info)
{
	release_mem_region(info->board_addr, info->board_size);

	if (info->btype == BT_PICASSO4) {
		iounmap ((void *)info->board_addr);
		iounmap ((void *)info->fbmem_phys);
	} else {
		if (info->board_addr > 0x01000000)
			iounmap ((void *)info->board_addr);
	}
}


static int __init clgen_zorro_setup (struct clgenfb_info *info,
				     clgen_board_t *btype)
{
	struct zorro_dev *z = NULL, *z2 = NULL;
	unsigned long board_addr, board_size, size;

	assert (info != NULL);
	assert (btype != NULL);

	if (clgen_zorro_find (&z, &z2, btype, &size))
		return -1;

	assert (z > 0);
	assert (z2 >= 0);
	assert (*btype != BT_NONE);

	info->board_addr = board_addr = z->resource.start;
	info->board_size = board_size = z->resource.end-z->resource.start+1;
	info->size = size;

	if (!request_mem_region(board_addr, board_size, "clgenfb")) {
		printk(KERN_ERR "clgen: cannot reserve region 0x%lx, abort\n",
		       board_addr);
		return -1;
	}

	printk (" RAM (%lu MB) at $%lx, ", board_size / MB_, board_addr);

	if (*btype == BT_PICASSO4) {
		printk (" REG at $%lx\n", board_addr + 0x600000);

		/* To be precise, for the P4 this is not the */
		/* begin of the board, but the begin of RAM. */
		/* for P4, map in its address space in 2 chunks (### TEST! ) */
		/* (note the ugly hardcoded 16M number) */
		info->regs = ioremap (board_addr, 16777216);
		DPRINTK ("clgen: Virtual address for board set to: $%p\n", info->regs);
		info->regs += 0x600000;
		info->fbregs_phys = board_addr + 0x600000;

		info->fbmem_phys = board_addr + 16777216;
		info->fbmem = ioremap (info->fbmem_phys, 16777216);
	} else {
		printk (" REG at $%lx\n", (unsigned long) z2->resource.start);

		info->fbmem_phys = board_addr;
		if (board_addr > 0x01000000)
			info->fbmem = ioremap (board_addr, board_size);
		else
			info->fbmem = (caddr_t) ZTWO_VADDR (board_addr);

		/* set address for REG area of board */
		info->regs = (caddr_t) ZTWO_VADDR (z2->resource.start);
		info->fbregs_phys = z2->resource.start;

		DPRINTK ("clgen: Virtual address for board set to: $%p\n", info->regs);
	}

	printk (KERN_INFO "Cirrus Logic chipset on Zorro bus\n");

	return 0;
}
#endif /* CONFIG_ZORRO */



/********************************************************************/
/* clgenfb_init() - master initialization function                  */
/********************************************************************/
int __init clgenfb_init(void)
{
	int err, j, k;

	clgen_board_t btype = BT_NONE;
	struct clgenfb_info *fb_info = NULL;

	DPRINTK ("ENTER\n");

	printk (KERN_INFO "clgen: Driver for Cirrus Logic based graphic boards, v" CLGEN_VERSION "\n");

	fb_info = &boards[0];	/* FIXME support multiple boards ... */

#ifdef CONFIG_PCI
	if (clgen_pci_setup (fb_info, &btype)) { /* Also does OF setup */
		DPRINTK ("EXIT, returning -ENXIO\n");
		return -ENXIO;
	}

#elif defined(CONFIG_ZORRO)
	/* FIXME: CONFIG_PCI and CONFIG_ZORRO may both be defined */
	if (clgen_zorro_setup (fb_info, &btype)) {
		DPRINTK ("EXIT, returning -ENXIO\n");
		return -ENXIO;
	}

#else
#error This driver requires Zorro or PCI bus.
#endif				/* !CONFIG_PCI, !CONFIG_ZORRO */

	/* sanity checks */
	assert (btype != BT_NONE);
	assert (btype == clgen_board_info[btype].btype);

	fb_info->btype = btype;

	DPRINTK ("clgen: (RAM start set to: 0x%p)\n", fb_info->fbmem);

	if (noaccel)
	{
		printk("clgen: disabling text acceleration support\n");
#ifdef FBCON_HAS_CFB8
		fbcon_clgen_8.bmove = fbcon_cfb8_bmove;
		fbcon_clgen_8.clear = fbcon_cfb8_clear;
#endif
#ifdef FBCON_HAS_CFB16
		fbcon_clgen_16.bmove = fbcon_cfb16_bmove;
		fbcon_clgen_16.clear = fbcon_cfb16_clear;
#endif
#ifdef FBCON_HAS_CFB32
		fbcon_clgen_32.bmove = fbcon_cfb32_bmove;
		fbcon_clgen_32.clear = fbcon_cfb32_clear;
#endif
	}

	init_vgachip (fb_info);

	/* set up a few more things, register framebuffer driver etc */
	fb_info->gen.parsize = sizeof (struct clgenfb_par);
	fb_info->gen.fbhw = &clgen_hwswitch;

	strncpy (fb_info->gen.info.modename, clgen_board_info[btype].name,
		 sizeof (fb_info->gen.info.modename));
	fb_info->gen.info.modename [sizeof (fb_info->gen.info.modename) - 1] = 0;

	fb_info->gen.info.node = -1;
	fb_info->gen.info.fbops = &clgenfb_ops;
	fb_info->gen.info.disp = &disp;
	fb_info->gen.info.changevar = NULL;
	fb_info->gen.info.switch_con = &fbgen_switch;
	fb_info->gen.info.updatevar = &fbgen_update_var;
	fb_info->gen.info.blank = &fbgen_blank;
	fb_info->gen.info.flags = FBINFO_FLAG_DEFAULT;

	for (j = 0; j < 256; j++) {
		if (j < 16) {
			k = color_table[j];
			fb_info->palette[j].red = default_red[k];
			fb_info->palette[j].green = default_grn[k];
			fb_info->palette[j].blue = default_blu[k];
		} else {
			fb_info->palette[j].red =
			fb_info->palette[j].green =
			fb_info->palette[j].blue = j;
		}
	}

	/* now that we know the board has been registered n' stuff, we */
	/* can finally initialize it to a default mode */
	clgenfb_default = clgenfb_predefined[clgen_def_mode].var;
	clgenfb_default.activate = FB_ACTIVATE_NOW;
	clgenfb_default.yres_virtual = 480 * 3;		/* for fast scrolling (YPAN-Mode) */
	err = fbgen_do_set_var (&clgenfb_default, 1, &fb_info->gen);

	if (err) {
		DPRINTK ("EXIT, returning -EINVAL\n");
		return -EINVAL;
	}

	disp.var = clgenfb_default;
	fbgen_set_disp (-1, &fb_info->gen);
	fbgen_install_cmap (0, &fb_info->gen);

	err = register_framebuffer (&fb_info->gen.info);
	if (err) {
		printk (KERN_ERR "clgen: ERROR - could not register fb device; err = %d!\n", err);
		DPRINTK ("EXIT, returning -EINVAL\n");
		return -EINVAL;
	}
	DPRINTK ("EXIT, returning 0\n");
	return 0;
}



    /*
     *  Cleanup (only needed for module)
     */
static void __exit clgenfb_cleanup (struct clgenfb_info *info)
{
	DPRINTK ("ENTER\n");

#ifdef CONFIG_ZORRO
	switch_monitor (info, 0);

	clgen_zorro_unmap (info);
#else
	clgen_pci_unmap (info);
#endif				/* CONFIG_ZORRO */

	unregister_framebuffer ((struct fb_info *) info);
	printk ("Framebuffer unregistered\n");

	DPRINTK ("EXIT\n");
}


#ifndef MODULE
int __init clgenfb_setup(char *options) {
	char *this_opt, s[32];
	int i;

	DPRINTK ("ENTER\n");

	if (!options || !*options)
		return 0;

	for (this_opt = strtok (options, ","); this_opt != NULL;
	     this_opt = strtok (NULL, ",")) {
		if (!*this_opt) continue;

		DPRINTK("clgenfb_setup: option '%s'\n", this_opt);

		for (i = 0; i < NUM_TOTAL_MODES; i++) {
			sprintf (s, "mode:%s", clgenfb_predefined[i].name);
			if (strcmp (this_opt, s) == 0)
				clgen_def_mode = i;
		}
		if (!strcmp(this_opt, "noaccel"))
			noaccel = 1;
	}
	return 0;
}
#endif


    /*
     *  Modularization
     */

MODULE_AUTHOR("Copyright 1999,2000 Jeff Garzik <jgarzik@pobox.com>");
MODULE_DESCRIPTION("Accelerated FBDev driver for Cirrus Logic chips");
MODULE_LICENSE("GPL");

static void __exit clgenfb_exit (void)
{
	DPRINTK ("ENTER\n");

	clgenfb_cleanup (&boards[0]);	/* FIXME: support multiple boards */

	DPRINTK ("EXIT\n");
}

#ifdef MODULE
module_init(clgenfb_init);
#endif
module_exit(clgenfb_exit);


/**********************************************************************/
/* about the following functions - I have used the same names for the */
/* functions as Markus Wild did in his Retina driver for NetBSD as    */
/* they just made sense for this purpose. Apart from that, I wrote    */
/* these functions myself.                                            */
/**********************************************************************/

/*** WGen() - write into one of the external/general registers ***/
static void WGen (const struct clgenfb_info *fb_info,
		  int regnum, unsigned char val)
{
	unsigned long regofs = 0;

	if (fb_info->btype == BT_PICASSO) {
		/* Picasso II specific hack */
/*              if (regnum == VGA_PEL_IR || regnum == VGA_PEL_D || regnum == CL_VSSM2) */
		if (regnum == VGA_PEL_IR || regnum == VGA_PEL_D)
			regofs = 0xfff;
	}

	vga_w (fb_info->regs, regofs + regnum, val);
}

/*** RGen() - read out one of the external/general registers ***/
static unsigned char RGen (const struct clgenfb_info *fb_info, int regnum)
{
	unsigned long regofs = 0;

	if (fb_info->btype == BT_PICASSO) {
		/* Picasso II specific hack */
/*              if (regnum == VGA_PEL_IR || regnum == VGA_PEL_D || regnum == CL_VSSM2) */
		if (regnum == VGA_PEL_IR || regnum == VGA_PEL_D)
			regofs = 0xfff;
	}

	return vga_r (fb_info->regs, regofs + regnum);
}

/*** AttrOn() - turn on VideoEnable for Attribute controller ***/
static void AttrOn (const struct clgenfb_info *fb_info)
{
	assert (fb_info != NULL);

	DPRINTK ("ENTER\n");

	if (vga_rcrt (fb_info->regs, CL_CRT24) & 0x80) {
		/* if we're just in "write value" mode, write back the */
		/* same value as before to not modify anything */
		vga_w (fb_info->regs, VGA_ATT_IW,
		       vga_r (fb_info->regs, VGA_ATT_R));
	}
	/* turn on video bit */
/*      vga_w (fb_info->regs, VGA_ATT_IW, 0x20); */
	vga_w (fb_info->regs, VGA_ATT_IW, 0x33);

	/* dummy write on Reg0 to be on "write index" mode next time */
	vga_w (fb_info->regs, VGA_ATT_IW, 0x00);

	DPRINTK ("EXIT\n");
}

/*** WHDR() - write into the Hidden DAC register ***/
/* as the HDR is the only extension register that requires special treatment
 * (the other extension registers are accessible just like the "ordinary"
 * registers of their functional group) here is a specialized routine for
 * accessing the HDR
 */
static void WHDR (const struct clgenfb_info *fb_info, unsigned char val)
{
	unsigned char dummy;

	if (fb_info->btype == BT_PICASSO) {
		/* Klaus' hint for correct access to HDR on some boards */
		/* first write 0 to pixel mask (3c6) */
		WGen (fb_info, VGA_PEL_MSK, 0x00);
		udelay (200);
		/* next read dummy from pixel address (3c8) */
		dummy = RGen (fb_info, VGA_PEL_IW);
		udelay (200);
	}
	/* now do the usual stuff to access the HDR */

	dummy = RGen (fb_info, VGA_PEL_MSK);
	udelay (200);
	dummy = RGen (fb_info, VGA_PEL_MSK);
	udelay (200);
	dummy = RGen (fb_info, VGA_PEL_MSK);
	udelay (200);
	dummy = RGen (fb_info, VGA_PEL_MSK);
	udelay (200);

	WGen (fb_info, VGA_PEL_MSK, val);
	udelay (200);

	if (fb_info->btype == BT_PICASSO) {
		/* now first reset HDR access counter */
		dummy = RGen (fb_info, VGA_PEL_IW);
		udelay (200);

		/* and at the end, restore the mask value */
		/* ## is this mask always 0xff? */
		WGen (fb_info, VGA_PEL_MSK, 0xff);
		udelay (200);
	}
}


/*** WSFR() - write to the "special function register" (SFR) ***/
static void WSFR (struct clgenfb_info *fb_info, unsigned char val)
{
#ifdef CONFIG_ZORRO
	assert (fb_info->regs != NULL);
	fb_info->SFR = val;
	z_writeb (val, fb_info->regs + 0x8000);
#endif
}

/* The Picasso has a second register for switching the monitor bit */
static void WSFR2 (struct clgenfb_info *fb_info, unsigned char val)
{
#ifdef CONFIG_ZORRO
	/* writing an arbitrary value to this one causes the monitor switcher */
	/* to flip to Amiga display */
	assert (fb_info->regs != NULL);
	fb_info->SFR = val;
	z_writeb (val, fb_info->regs + 0x9000);
#endif
}


/*** WClut - set CLUT entry (range: 0..63) ***/
static void WClut (struct clgenfb_info *fb_info, unsigned char regnum, unsigned char red,
	    unsigned char green, unsigned char blue)
{
	unsigned int data = VGA_PEL_D;

	/* address write mode register is not translated.. */
	vga_w (fb_info->regs, VGA_PEL_IW, regnum);

	if (fb_info->btype == BT_PICASSO || fb_info->btype == BT_PICASSO4 ||
	    fb_info->btype == BT_ALPINE || fb_info->btype == BT_GD5480) {
		/* but DAC data register IS, at least for Picasso II */
		if (fb_info->btype == BT_PICASSO)
			data += 0xfff;
		vga_w (fb_info->regs, data, red);
		vga_w (fb_info->regs, data, green);
		vga_w (fb_info->regs, data, blue);
	} else {
		vga_w (fb_info->regs, data, blue);
		vga_w (fb_info->regs, data, green);
		vga_w (fb_info->regs, data, red);
	}
}


#if 0
/*** RClut - read CLUT entry (range 0..63) ***/
static void RClut (struct clgenfb_info *fb_info, unsigned char regnum, unsigned char *red,
	    unsigned char *green, unsigned char *blue)
{
	unsigned int data = VGA_PEL_D;

	vga_w (fb_info->regs, VGA_PEL_IR, regnum);

	if (fb_info->btype == BT_PICASSO || fb_info->btype == BT_PICASSO4 ||
	    fb_info->btype == BT_ALPINE || fb_info->btype == BT_GD5480) {
		if (fb_info->btype == BT_PICASSO)
			data += 0xfff;
		*red = vga_r (fb_info->regs, data);
		*green = vga_r (fb_info->regs, data);
		*blue = vga_r (fb_info->regs, data);
	} else {
		*blue = vga_r (fb_info->regs, data);
		*green = vga_r (fb_info->regs, data);
		*red = vga_r (fb_info->regs, data);
	}
}
#endif


/*******************************************************************
	clgen_WaitBLT()

	Wait for the BitBLT engine to complete a possible earlier job
*********************************************************************/

/* FIXME: use interrupts instead */
static inline void clgen_WaitBLT (caddr_t regbase)
{
	/* now busy-wait until we're done */
	while (vga_rgfx (regbase, CL_GR31) & 0x08)
		/* do nothing */ ;
}

/*******************************************************************
	clgen_BitBLT()

	perform accelerated "scrolling"
********************************************************************/

static void clgen_BitBLT (caddr_t regbase, u_short curx, u_short cury, u_short destx, u_short desty,
		   u_short width, u_short height, u_short line_length)
{
	u_short nwidth, nheight;
	u_long nsrc, ndest;
	u_char bltmode;

	DPRINTK ("ENTER\n");

	nwidth = width - 1;
	nheight = height - 1;

	bltmode = 0x00;
	/* if source adr < dest addr, do the Blt backwards */
	if (cury <= desty) {
		if (cury == desty) {
			/* if src and dest are on the same line, check x */
			if (curx < destx)
				bltmode |= 0x01;
		} else
			bltmode |= 0x01;
	}
	if (!bltmode) {
		/* standard case: forward blitting */
		nsrc = (cury * line_length) + curx;
		ndest = (desty * line_length) + destx;
	} else {
		/* this means start addresses are at the end, counting backwards */
		nsrc = cury * line_length + curx + nheight * line_length + nwidth;
		ndest = desty * line_length + destx + nheight * line_length + nwidth;
	}

        clgen_WaitBLT(regbase);

	/*
	   run-down of registers to be programmed:
	   destination pitch
	   source pitch
	   BLT width/height
	   source start
	   destination start
	   BLT mode
	   BLT ROP
	   VGA_GFX_SR_VALUE / VGA_GFX_SR_ENABLE: "fill color"
	   start/stop
	 */

	/* pitch: set to line_length */
	vga_wgfx (regbase, CL_GR24, line_length & 0xff);	/* dest pitch low */
	vga_wgfx (regbase, CL_GR25, (line_length >> 8));	/* dest pitch hi */
	vga_wgfx (regbase, CL_GR26, line_length & 0xff);	/* source pitch low */
	vga_wgfx (regbase, CL_GR27, (line_length >> 8));	/* source pitch hi */

	/* BLT width: actual number of pixels - 1 */
	vga_wgfx (regbase, CL_GR20, nwidth & 0xff);	/* BLT width low */
	vga_wgfx (regbase, CL_GR21, (nwidth >> 8));	/* BLT width hi */

	/* BLT height: actual number of lines -1 */
	vga_wgfx (regbase, CL_GR22, nheight & 0xff);	/* BLT height low */
	vga_wgfx (regbase, CL_GR23, (nheight >> 8));	/* BLT width hi */

	/* BLT destination */
	vga_wgfx (regbase, CL_GR28, (u_char) (ndest & 0xff));	/* BLT dest low */
	vga_wgfx (regbase, CL_GR29, (u_char) (ndest >> 8));	/* BLT dest mid */
	vga_wgfx (regbase, CL_GR2A, (u_char) (ndest >> 16));	/* BLT dest hi */

	/* BLT source */
	vga_wgfx (regbase, CL_GR2C, (u_char) (nsrc & 0xff));	/* BLT src low */
	vga_wgfx (regbase, CL_GR2D, (u_char) (nsrc >> 8));		/* BLT src mid */
	vga_wgfx (regbase, CL_GR2E, (u_char) (nsrc >> 16));	/* BLT src hi */

	/* BLT mode */
	vga_wgfx (regbase, CL_GR30, bltmode);	/* BLT mode */

	/* BLT ROP: SrcCopy */
	vga_wgfx (regbase, CL_GR32, 0x0d);		/* BLT ROP */

	/* and finally: GO! */
	vga_wgfx (regbase, CL_GR31, 0x02);		/* BLT Start/status */

	DPRINTK ("EXIT\n");
}


/*******************************************************************
	clgen_RectFill()

	perform accelerated rectangle fill
********************************************************************/

static void clgen_RectFill (struct clgenfb_info *fb_info,
		     u_short x, u_short y, u_short width, u_short height,
		     u_char color, u_short line_length)
{
	u_short nwidth, nheight;
	u_long ndest;
	u_char op;

	DPRINTK ("ENTER\n");

	nwidth = width - 1;
	nheight = height - 1;

	ndest = (y * line_length) + x;

        clgen_WaitBLT(fb_info->regs);

	/* pitch: set to line_length */
	vga_wgfx (fb_info->regs, CL_GR24, line_length & 0xff);	/* dest pitch low */
	vga_wgfx (fb_info->regs, CL_GR25, (line_length >> 8));	/* dest pitch hi */
	vga_wgfx (fb_info->regs, CL_GR26, line_length & 0xff);	/* source pitch low */
	vga_wgfx (fb_info->regs, CL_GR27, (line_length >> 8));	/* source pitch hi */

	/* BLT width: actual number of pixels - 1 */
	vga_wgfx (fb_info->regs, CL_GR20, nwidth & 0xff);	/* BLT width low */
	vga_wgfx (fb_info->regs, CL_GR21, (nwidth >> 8));	/* BLT width hi */

	/* BLT height: actual number of lines -1 */
	vga_wgfx (fb_info->regs, CL_GR22, nheight & 0xff);		/* BLT height low */
	vga_wgfx (fb_info->regs, CL_GR23, (nheight >> 8));		/* BLT width hi */

	/* BLT destination */
	vga_wgfx (fb_info->regs, CL_GR28, (u_char) (ndest & 0xff));	/* BLT dest low */
	vga_wgfx (fb_info->regs, CL_GR29, (u_char) (ndest >> 8));	/* BLT dest mid */
	vga_wgfx (fb_info->regs, CL_GR2A, (u_char) (ndest >> 16));		/* BLT dest hi */

	/* BLT source: set to 0 (is a dummy here anyway) */
	vga_wgfx (fb_info->regs, CL_GR2C, 0x00);	/* BLT src low */
	vga_wgfx (fb_info->regs, CL_GR2D, 0x00);	/* BLT src mid */
	vga_wgfx (fb_info->regs, CL_GR2E, 0x00);	/* BLT src hi */

	/* This is a ColorExpand Blt, using the */
	/* same color for foreground and background */
	vga_wgfx (fb_info->regs, VGA_GFX_SR_VALUE, color);	/* foreground color */
	vga_wgfx (fb_info->regs, VGA_GFX_SR_ENABLE, color);	/* background color */

	op = 0xc0;
	if (fb_info->currentmode.var.bits_per_pixel == 16) {
		vga_wgfx (fb_info->regs, CL_GR10, color);	/* foreground color */
		vga_wgfx (fb_info->regs, CL_GR11, color);	/* background color */
		op = 0x50;
		op = 0xd0;
	} else if (fb_info->currentmode.var.bits_per_pixel == 32) {
		vga_wgfx (fb_info->regs, CL_GR10, color);	/* foreground color */
		vga_wgfx (fb_info->regs, CL_GR11, color);	/* background color */
		vga_wgfx (fb_info->regs, CL_GR12, color);	/* foreground color */
		vga_wgfx (fb_info->regs, CL_GR13, color);	/* background color */
		vga_wgfx (fb_info->regs, CL_GR14, 0);	/* foreground color */
		vga_wgfx (fb_info->regs, CL_GR15, 0);	/* background color */
		op = 0x50;
		op = 0xf0;
	}
	/* BLT mode: color expand, Enable 8x8 copy (faster?) */
	vga_wgfx (fb_info->regs, CL_GR30, op);	/* BLT mode */

	/* BLT ROP: SrcCopy */
	vga_wgfx (fb_info->regs, CL_GR32, 0x0d);	/* BLT ROP */

	/* and finally: GO! */
	vga_wgfx (fb_info->regs, CL_GR31, 0x02);	/* BLT Start/status */

	DPRINTK ("EXIT\n");
}


/**************************************************************************
 * bestclock() - determine closest possible clock lower(?) than the
 * desired pixel clock
 **************************************************************************/
static void bestclock (long freq, long *best, long *nom,
		       long *den, long *div, long maxfreq)
{
	long n, h, d, f;

	assert (best != NULL);
	assert (nom != NULL);
	assert (den != NULL);
	assert (div != NULL);
	assert (maxfreq > 0);

	*nom = 0;
	*den = 0;
	*div = 0;

	DPRINTK ("ENTER\n");

	if (freq < 8000)
		freq = 8000;

	if (freq > maxfreq)
		freq = maxfreq;

	*best = 0;
	f = freq * 10;

	for (n = 32; n < 128; n++) {
		d = (143181 * n) / f;
		if ((d >= 7) && (d <= 63)) {
			if (d > 31)
				d = (d / 2) * 2;
			h = (14318 * n) / d;
			if (abs (h - freq) < abs (*best - freq)) {
				*best = h;
				*nom = n;
				if (d < 32) {
					*den = d;
					*div = 0;
				} else {
					*den = d / 2;
					*div = 1;
				}
			}
		}
		d = ((143181 * n) + f - 1) / f;
		if ((d >= 7) && (d <= 63)) {
			if (d > 31)
				d = (d / 2) * 2;
			h = (14318 * n) / d;
			if (abs (h - freq) < abs (*best - freq)) {
				*best = h;
				*nom = n;
				if (d < 32) {
					*den = d;
					*div = 0;
				} else {
					*den = d / 2;
					*div = 1;
				}
			}
		}
	}

	DPRINTK ("Best possible values for given frequency:\n");
	DPRINTK ("        best: %ld kHz  nom: %ld  den: %ld  div: %ld\n",
		 freq, *nom, *den, *div);

	DPRINTK ("EXIT\n");
}


/* -------------------------------------------------------------------------
 *
 * debugging functions
 *
 * -------------------------------------------------------------------------
 */

#ifdef CLGEN_DEBUG

/**
 * clgen_dbg_print_byte
 * @name: name associated with byte value to be displayed
 * @val: byte value to be displayed
 *
 * DESCRIPTION:
 * Display an indented string, along with a hexidecimal byte value, and
 * its decoded bits.  Bits 7 through 0 are listed in left-to-right
 * order.
 */

static
void clgen_dbg_print_byte (const char *name, unsigned char val)
{
	DPRINTK ("%8s = 0x%02X (bits 7-0: %c%c%c%c%c%c%c%c)\n",
		 name, val,
		 val & 0x80 ? '1' : '0',
		 val & 0x40 ? '1' : '0',
		 val & 0x20 ? '1' : '0',
		 val & 0x10 ? '1' : '0',
		 val & 0x08 ? '1' : '0',
		 val & 0x04 ? '1' : '0',
		 val & 0x02 ? '1' : '0',
		 val & 0x01 ? '1' : '0');
}


/**
 * clgen_dbg_print_regs
 * @base: If using newmmio, the newmmio base address, otherwise %NULL
 * @reg_class: type of registers to read: %CRT, or %SEQ
 *
 * DESCRIPTION:
 * Dumps the given list of VGA CRTC registers.  If @base is %NULL,
 * old-style I/O ports are queried for information, otherwise MMIO is
 * used at the given @base address to query the information.
 */

static
void clgen_dbg_print_regs (caddr_t regbase, clgen_dbg_reg_class_t reg_class,...)
{
	va_list list;
	unsigned char val = 0;
	unsigned reg;
	char *name;

	va_start (list, reg_class);

	name = va_arg (list, char *);
	while (name != NULL) {
		reg = va_arg (list, int);

		switch (reg_class) {
		case CRT:
			val = vga_rcrt (regbase, (unsigned char) reg);
			break;
		case SEQ:
			val = vga_rseq (regbase, (unsigned char) reg);
			break;
		default:
			/* should never occur */
			assert (FALSE);
			break;
		}

		clgen_dbg_print_byte (name, val);

		name = va_arg (list, char *);
	}

	va_end (list);
}


/**
 * clgen_dump
 * @clgeninfo:
 *
 * DESCRIPTION:
 */

static
void clgen_dump (void)
{
	clgen_dbg_reg_dump (NULL);
}


/**
 * clgen_dbg_reg_dump
 * @base: If using newmmio, the newmmio base address, otherwise %NULL
 *
 * DESCRIPTION:
 * Dumps a list of interesting VGA and CLGEN registers.  If @base is %NULL,
 * old-style I/O ports are queried for information, otherwise MMIO is
 * used at the given @base address to query the information.
 */

static
void clgen_dbg_reg_dump (caddr_t regbase)
{
	DPRINTK ("CLGEN VGA CRTC register dump:\n");

	clgen_dbg_print_regs (regbase, CRT,
			   "CR00", 0x00,
			   "CR01", 0x01,
			   "CR02", 0x02,
			   "CR03", 0x03,
			   "CR04", 0x04,
			   "CR05", 0x05,
			   "CR06", 0x06,
			   "CR07", 0x07,
			   "CR08", 0x08,
			   "CR09", 0x09,
			   "CR0A", 0x0A,
			   "CR0B", 0x0B,
			   "CR0C", 0x0C,
			   "CR0D", 0x0D,
			   "CR0E", 0x0E,
			   "CR0F", 0x0F,
			   "CR10", 0x10,
			   "CR11", 0x11,
			   "CR12", 0x12,
			   "CR13", 0x13,
			   "CR14", 0x14,
			   "CR15", 0x15,
			   "CR16", 0x16,
			   "CR17", 0x17,
			   "CR18", 0x18,
			   "CR22", 0x22,
			   "CR24", 0x24,
			   "CR26", 0x26,
			   "CR2D", 0x2D,
			   "CR2E", 0x2E,
			   "CR2F", 0x2F,
			   "CR30", 0x30,
			   "CR31", 0x31,
			   "CR32", 0x32,
			   "CR33", 0x33,
			   "CR34", 0x34,
			   "CR35", 0x35,
			   "CR36", 0x36,
			   "CR37", 0x37,
			   "CR38", 0x38,
			   "CR39", 0x39,
			   "CR3A", 0x3A,
			   "CR3B", 0x3B,
			   "CR3C", 0x3C,
			   "CR3D", 0x3D,
			   "CR3E", 0x3E,
			   "CR3F", 0x3F,
			   NULL);

	DPRINTK ("\n");

	DPRINTK ("CLGEN VGA SEQ register dump:\n");

	clgen_dbg_print_regs (regbase, SEQ,
			   "SR00", 0x00,
			   "SR01", 0x01,
			   "SR02", 0x02,
			   "SR03", 0x03,
			   "SR04", 0x04,
			   "SR08", 0x08,
			   "SR09", 0x09,
			   "SR0A", 0x0A,
			   "SR0B", 0x0B,
			   "SR0D", 0x0D,
			   "SR10", 0x10,
			   "SR11", 0x11,
			   "SR12", 0x12,
			   "SR13", 0x13,
			   "SR14", 0x14,
			   "SR15", 0x15,
			   "SR16", 0x16,
			   "SR17", 0x17,
			   "SR18", 0x18,
			   "SR19", 0x19,
			   "SR1A", 0x1A,
			   "SR1B", 0x1B,
			   "SR1C", 0x1C,
			   "SR1D", 0x1D,
			   "SR1E", 0x1E,
			   "SR1F", 0x1F,
			   NULL);

	DPRINTK ("\n");
}

#endif				/* CLGEN_DEBUG */

