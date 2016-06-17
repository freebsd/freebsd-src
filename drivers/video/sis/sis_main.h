/*
 * SiS 300/630/730/540/315/550/650/651/M650/661FX/M661FX/740/741/330/760
 * frame buffer driver for Linux kernels 2.4.x and 2.5.x
 *
 * Copyright (C) 2001-2004 Thomas Winischhofer, Vienna, Austria.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef _SISFB_MAIN
#define _SISFB_MAIN

#include "vstruct.h"

/* ------------------- Constant Definitions ------------------------- */

#define AGPOFF     /* default is turn off AGP */

#define SISFAIL(x) do { printk(x "\n"); return -EINVAL; } while(0)

#define VER_MAJOR                 1
#define VER_MINOR                 6
#define VER_LEVEL                 32

#include "sis.h"

/* To be included in pci_ids.h */
#ifndef PCI_DEVICE_ID_SI_650_VGA
#define PCI_DEVICE_ID_SI_650_VGA  0x6325
#endif
#ifndef PCI_DEVICE_ID_SI_650
#define PCI_DEVICE_ID_SI_650      0x0650
#endif
#ifndef PCI_DEVICE_ID_SI_740
#define PCI_DEVICE_ID_SI_740      0x0740
#endif
#ifndef PCI_DEVICE_ID_SI_330
#define PCI_DEVICE_ID_SI_330      0x0330
#endif
#ifndef PCI_DEVICE_ID_SI_660_VGA
#define PCI_DEVICE_ID_SI_660_VGA  0x6330
#endif
#ifndef PCI_DEVICE_ID_SI_660
#define PCI_DEVICE_ID_SI_660      0x0661
#endif
#ifndef PCI_DEVICE_ID_SI_741
#define PCI_DEVICE_ID_SI_741      0x0741
#endif
#ifndef PCI_DEVICE_ID_SI_660
#define PCI_DEVICE_ID_SI_660      0x0660
#endif
#ifndef PCI_DEVICE_ID_SI_760
#define PCI_DEVICE_ID_SI_760      0x0760
#endif

/* To be included in fb.h */
#ifndef FB_ACCEL_SIS_GLAMOUR_2
#define FB_ACCEL_SIS_GLAMOUR_2  40	/* SiS 315, 65x, 740, 661, 741  */
#endif
#ifndef FB_ACCEL_SIS_XABRE
#define FB_ACCEL_SIS_XABRE      41	/* SiS 330 ("Xabre"), 760 	*/
#endif

#define MAX_ROM_SCAN              0x10000

#define HW_CURSOR_CAP             0x80
#define TURBO_QUEUE_CAP           0x40
#define AGP_CMD_QUEUE_CAP         0x20
#define VM_CMD_QUEUE_CAP          0x10
#define MMIO_CMD_QUEUE_CAP        0x08

/* For 300 series */
#ifdef CONFIG_FB_SIS_300
#define TURBO_QUEUE_AREA_SIZE     0x80000 /* 512K */
#endif

/* For 315/Xabre series */
#ifdef CONFIG_FB_SIS_315
#define COMMAND_QUEUE_AREA_SIZE   0x80000 /* 512K */
#define COMMAND_QUEUE_THRESHOLD   0x1F
#endif

#define HW_CURSOR_AREA_SIZE_315   0x4000  /* 16K */
#define HW_CURSOR_AREA_SIZE_300   0x1000  /* 4K */

#define OH_ALLOC_SIZE             4000
#define SENTINEL                  0x7fffffff

#define SEQ_ADR                   0x14
#define SEQ_DATA                  0x15
#define DAC_ADR                   0x18
#define DAC_DATA                  0x19
#define CRTC_ADR                  0x24
#define CRTC_DATA                 0x25
#define DAC2_ADR                  (0x16-0x30)
#define DAC2_DATA                 (0x17-0x30)
#define VB_PART1_ADR              (0x04-0x30)
#define VB_PART1_DATA             (0x05-0x30)
#define VB_PART2_ADR              (0x10-0x30)
#define VB_PART2_DATA             (0x11-0x30)
#define VB_PART3_ADR              (0x12-0x30)
#define VB_PART3_DATA             (0x13-0x30)
#define VB_PART4_ADR              (0x14-0x30)
#define VB_PART4_DATA             (0x15-0x30)

#define SISSR			  SiS_Pr.SiS_P3c4
#define SISCR                     SiS_Pr.SiS_P3d4
#define SISDACA                   SiS_Pr.SiS_P3c8
#define SISDACD                   SiS_Pr.SiS_P3c9
#define SISPART1                  SiS_Pr.SiS_Part1Port
#define SISPART2                  SiS_Pr.SiS_Part2Port
#define SISPART3                  SiS_Pr.SiS_Part3Port
#define SISPART4                  SiS_Pr.SiS_Part4Port
#define SISPART5                  SiS_Pr.SiS_Part5Port
#define SISDAC2A                  SISPART5
#define SISDAC2D                  (SISPART5 + 1)
#define SISMISCR                  (SiS_Pr.RelIO + 0x1c)
#define SISMISCW                  SiS_Pr.SiS_P3c2
#define SISINPSTAT		  (SiS_Pr.RelIO + 0x2a)
#define SISPEL			  SiS_Pr.SiS_P3c6

#define IND_SIS_PASSWORD          0x05  /* SRs */
#define IND_SIS_COLOR_MODE        0x06
#define IND_SIS_RAMDAC_CONTROL    0x07
#define IND_SIS_DRAM_SIZE         0x14
#define IND_SIS_SCRATCH_REG_16    0x16
#define IND_SIS_SCRATCH_REG_17    0x17
#define IND_SIS_SCRATCH_REG_1A    0x1A
#define IND_SIS_MODULE_ENABLE     0x1E
#define IND_SIS_PCI_ADDRESS_SET   0x20
#define IND_SIS_TURBOQUEUE_ADR    0x26
#define IND_SIS_TURBOQUEUE_SET    0x27
#define IND_SIS_POWER_ON_TRAP     0x38
#define IND_SIS_POWER_ON_TRAP2    0x39
#define IND_SIS_CMDQUEUE_SET      0x26
#define IND_SIS_CMDQUEUE_THRESHOLD  0x27

#define IND_SIS_SCRATCH_REG_CR30  0x30  /* CRs */
#define IND_SIS_SCRATCH_REG_CR31  0x31
#define IND_SIS_SCRATCH_REG_CR32  0x32
#define IND_SIS_SCRATCH_REG_CR33  0x33
#define IND_SIS_LCD_PANEL         0x36
#define IND_SIS_SCRATCH_REG_CR37  0x37
#define IND_SIS_AGP_IO_PAD        0x48

#define IND_BRI_DRAM_STATUS       0x63 /* PCI config memory size offset */

#define MMIO_QUEUE_PHYBASE        0x85C0
#define MMIO_QUEUE_WRITEPORT      0x85C4
#define MMIO_QUEUE_READPORT       0x85C8

#define SIS_CRT2_WENABLE_300 0x24
#define SIS_CRT2_WENABLE_315 0x2F

#define SIS_PASSWORD              0x86  /* SR05 */
#define SIS_INTERLACED_MODE       0x20  /* SR06 */
#define SIS_8BPP_COLOR_MODE       0x0
#define SIS_15BPP_COLOR_MODE      0x1 
#define SIS_16BPP_COLOR_MODE      0x2 
#define SIS_32BPP_COLOR_MODE      0x4 
#define SIS_DRAM_SIZE_MASK        0x3F  /* 300/630/730 SR14 */
#define SIS_DRAM_SIZE_1MB         0x00
#define SIS_DRAM_SIZE_2MB         0x01
#define SIS_DRAM_SIZE_4MB         0x03
#define SIS_DRAM_SIZE_8MB         0x07
#define SIS_DRAM_SIZE_16MB        0x0F
#define SIS_DRAM_SIZE_32MB        0x1F
#define SIS_DRAM_SIZE_64MB        0x3F
#define SIS_DATA_BUS_MASK         0xC0
#define SIS_DATA_BUS_32           0x00
#define SIS_DATA_BUS_64           0x01
#define SIS_DATA_BUS_128          0x02

#define SIS315_DATA_BUS_MASK      0x02
#define SIS315_DATA_BUS_64        0x00
#define SIS315_DATA_BUS_128       0x01
#define SIS315_DUAL_CHANNEL_MASK  0x0C
#define SIS315_SINGLE_CHANNEL_1_RANK  	0x00
#define SIS315_SINGLE_CHANNEL_2_RANK  	0x01
#define SIS315_ASYM_DDR		  	0x02
#define SIS315_DUAL_CHANNEL_1_RANK    	0x03

#define SIS_SCRATCH_REG_1A_MASK   0x10

#define SIS_ENABLE_2D             0x40  /* SR1E */

#define SIS_MEM_MAP_IO_ENABLE     0x01  /* SR20 */
#define SIS_PCI_ADDR_ENABLE       0x80

#define SIS_AGP_CMDQUEUE_ENABLE   0x80  /* 315/650/740 SR26 */
#define SIS_VRAM_CMDQUEUE_ENABLE  0x40
#define SIS_MMIO_CMD_ENABLE       0x20
#define SIS_CMD_QUEUE_SIZE_512k   0x00
#define SIS_CMD_QUEUE_SIZE_1M     0x04
#define SIS_CMD_QUEUE_SIZE_2M     0x08
#define SIS_CMD_QUEUE_SIZE_4M     0x0C
#define SIS_CMD_QUEUE_RESET       0x01
#define SIS_CMD_AUTO_CORR	  0x02

#define SIS_SIMULTANEOUS_VIEW_ENABLE  0x01  /* CR30 */
#define SIS_MODE_SELECT_CRT2      0x02
#define SIS_VB_OUTPUT_COMPOSITE   0x04
#define SIS_VB_OUTPUT_SVIDEO      0x08
#define SIS_VB_OUTPUT_SCART       0x10
#define SIS_VB_OUTPUT_LCD         0x20
#define SIS_VB_OUTPUT_CRT2        0x40
#define SIS_VB_OUTPUT_HIVISION    0x80

#define SIS_VB_OUTPUT_DISABLE     0x20  /* CR31 */
#define SIS_DRIVER_MODE           0x40

#define SIS_VB_COMPOSITE          0x01  /* CR32 */
#define SIS_VB_SVIDEO             0x02
#define SIS_VB_SCART              0x04
#define SIS_VB_LCD                0x08
#define SIS_VB_CRT2               0x10
#define SIS_CRT1                  0x20
#define SIS_VB_HIVISION           0x40
#define SIS_VB_DVI                0x80
#define SIS_VB_TV                 (SIS_VB_COMPOSITE | SIS_VB_SVIDEO | \
                                   SIS_VB_SCART | SIS_VB_HIVISION)

#define SIS_EXTERNAL_CHIP_MASK    	   0x0E  /* CR37 (< SiS 660) */
#define SIS_EXTERNAL_CHIP_SIS301           0x01  /* in CR37 << 1 ! */
#define SIS_EXTERNAL_CHIP_LVDS             0x02  /* in CR37 << 1 ! */
#define SIS_EXTERNAL_CHIP_TRUMPION         0x03  /* in CR37 << 1 ! */
#define SIS_EXTERNAL_CHIP_LVDS_CHRONTEL    0x04  /* in CR37 << 1 ! */
#define SIS_EXTERNAL_CHIP_CHRONTEL         0x05  /* in CR37 << 1 ! */
#define SIS310_EXTERNAL_CHIP_LVDS          0x02  /* in CR37 << 1 ! */
#define SIS310_EXTERNAL_CHIP_LVDS_CHRONTEL 0x03  /* in CR37 << 1 ! */

#define SIS_AGP_2X                0x20  /* CR48 */

#define BRI_DRAM_SIZE_MASK        0x70  /* PCI bridge config data */
#define BRI_DRAM_SIZE_2MB         0x00
#define BRI_DRAM_SIZE_4MB         0x01
#define BRI_DRAM_SIZE_8MB         0x02
#define BRI_DRAM_SIZE_16MB        0x03
#define BRI_DRAM_SIZE_32MB        0x04
#define BRI_DRAM_SIZE_64MB        0x05

#define HW_DEVICE_EXTENSION	  SIS_HW_INFO
#define PHW_DEVICE_EXTENSION      PSIS_HW_INFO

#define SR_BUFFER_SIZE            5
#define CR_BUFFER_SIZE            5

/* entries for disp_state - deprecated as of 1.6.02 */
#define DISPTYPE_CRT1       0x00000008L
#define DISPTYPE_CRT2       0x00000004L
#define DISPTYPE_LCD        0x00000002L
#define DISPTYPE_TV         0x00000001L
#define DISPTYPE_DISP1      DISPTYPE_CRT1
#define DISPTYPE_DISP2      (DISPTYPE_CRT2 | DISPTYPE_LCD | DISPTYPE_TV)
#define DISPMODE_SINGLE	    0x00000020L
#define DISPMODE_MIRROR	    0x00000010L
#define DISPMODE_DUALVIEW   0x00000040L

/* Deprecated as of 1.6.02 - use vbflags instead */
#define HASVB_NONE      	0x00
#define HASVB_301       	0x01
#define HASVB_LVDS      	0x02
#define HASVB_TRUMPION  	0x04
#define HASVB_LVDS_CHRONTEL	0x10
#define HASVB_302       	0x20
#define HASVB_303       	0x40
#define HASVB_CHRONTEL  	0x80

/* Useful macros */
#define inSISREG(base)          inb(base)
#define outSISREG(base,val)     outb(val,base)
#define orSISREG(base,val)      do { \
                                  unsigned char __Temp = inb(base); \
                                  outSISREG(base, __Temp | (val)); \
                                } while (0)
#define andSISREG(base,val)     do { \
                                  unsigned char __Temp = inb(base); \
                                  outSISREG(base, __Temp & (val)); \
                                } while (0)
#define inSISIDXREG(base,idx,var)   do { \
                                      outb(idx,base); var=inb((base)+1); \
                                    } while (0)
#define outSISIDXREG(base,idx,val)  do { \
                                      outb(idx,base); outb((val),(base)+1); \
                                    } while (0)
#define orSISIDXREG(base,idx,val)   do { \
                                      unsigned char __Temp; \
                                      outb(idx,base);   \
                                      __Temp = inb((base)+1)|(val); \
                                      outSISIDXREG(base,idx,__Temp); \
                                    } while (0)
#define andSISIDXREG(base,idx,and)  do { \
                                      unsigned char __Temp; \
                                      outb(idx,base);   \
                                      __Temp = inb((base)+1)&(and); \
                                      outSISIDXREG(base,idx,__Temp); \
                                    } while (0)
#define setSISIDXREG(base,idx,and,or)   do { \
                                          unsigned char __Temp; \
                                          outb(idx,base);   \
                                          __Temp = (inb((base)+1)&(and))|(or); \
                                          outSISIDXREG(base,idx,__Temp); \
                                        } while (0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/* Offscreen layout */
typedef struct _SIS_GLYINFO {
	unsigned char ch;
	int fontwidth;
	int fontheight;
	u8 gmask[72];
	int ngmask;
} SIS_GLYINFO;

static char sisfb_fontname[40];
#endif

typedef struct _SIS_OH {
	struct _SIS_OH *poh_next;
	struct _SIS_OH *poh_prev;
	unsigned long offset;
	unsigned long size;
} SIS_OH;

typedef struct _SIS_OHALLOC {
	struct _SIS_OHALLOC *poha_next;
	SIS_OH aoh[1];
} SIS_OHALLOC;

typedef struct _SIS_HEAP {
	SIS_OH oh_free;
	SIS_OH oh_used;
	SIS_OH *poh_freelist;
	SIS_OHALLOC *poha_chain;
	unsigned long max_freesize;
} SIS_HEAP;

/* Fbcon stuff */
static struct fb_info *sis_fb_info;

static struct fb_var_screeninfo default_var = {
	.xres            = 0,
	.yres            = 0,
	.xres_virtual    = 0,
	.yres_virtual    = 0,
	.xoffset         = 0,
	.yoffset         = 0,
	.bits_per_pixel  = 0,
	.grayscale       = 0,
	.red             = {0, 8, 0},
	.green           = {0, 8, 0},
	.blue            = {0, 8, 0},
	.transp          = {0, 0, 0},
	.nonstd          = 0,
	.activate        = FB_ACTIVATE_NOW,
	.height          = -1,
	.width           = -1,
	.accel_flags     = 0,
	.pixclock        = 0,
	.left_margin     = 0,
	.right_margin    = 0,
	.upper_margin    = 0,
	.lower_margin    = 0,
	.hsync_len       = 0,
	.vsync_len       = 0,
	.sync            = 0,
	.vmode           = FB_VMODE_NONINTERLACED,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	.reserved        = {0, 0, 0, 0, 0, 0}
#endif
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct fb_fix_screeninfo sisfb_fix = {
	.id		= "SiS",
	.type		= FB_TYPE_PACKED_PIXELS,
	.xpanstep	= 0,
	.ypanstep	= 1,
};
static char myid[40];
static u32 pseudo_palette[17];
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static struct display sis_disp;

static struct display_switch sisfb_sw;	

static struct {
	u16 blue, green, red, pad;
} sis_palette[256];

static union {
#ifdef FBCON_HAS_CFB16
	u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB32
	u32 cfb32[16];
#endif
} sis_fbcon_cmap;

static int sisfb_inverse = 0;
static int currcon = 0;
#endif

static int sisfb_off = 0;
static int sisfb_crt1off = 0;
static int sisfb_forcecrt1 = -1;
static int sisfb_crt2type  = -1;	/* CRT2 type (for overriding autodetection) */
static int sisfb_tvplug    = -1;	/* Tv plug type (for overriding autodetection) */
static int sisfb_userom = 1;
static int sisfb_useoem = -1;
static int sisfb_parm_rate = -1;
static int sisfb_mem = 0;
static int sisfb_pdc = 0xff;
static int sisfb_pdca = 0xff;
static int sisfb_ypan = -1;
static int sisfb_max = -1;
static int sisfb_nocrt2rate = 0;
static int sisfb_dstn = 0;
static int sisfb_fstn = 0;
int 	   sisfb_accel = -1;
int 	   sisfb_queuemode = -1; 	/* Use MMIO queue mode by default (315 series only) */
#if !defined(__i386__) && !defined(__x86_64__)
static int sisfb_resetcard = 0;
static int sisfb_videoram = 0;
#endif

/* data for sis hardware ("par") */
struct video_info ivideo;

/* For ioctl SISFB_GET_INFO */
sisfb_info sisfbinfo;

/* Hardware info; contains data on hardware */
SIS_HW_INFO sishw_ext;

/* SiS private structure */
SiS_Private  SiS_Pr;

typedef enum _SIS_CMDTYPE {
	MMIO_CMD = 0,
	AGP_CMD_QUEUE,
	VM_CMD_QUEUE,
} SIS_CMDTYPE;

/* List of supported chips */
static struct sisfb_chip_info {
        int 		chip;
	VGA_ENGINE 	vgaengine;
	int 		hwcursor_size;
	int		CRT2_write_enable;
	const char 	*chip_name;
} sisfb_chip_info[] __devinitdata = {
	{ SIS_300,    SIS_300_VGA, HW_CURSOR_AREA_SIZE_300 * 2, SIS_CRT2_WENABLE_300, "SiS 300/305" },
	{ SIS_540,    SIS_300_VGA, HW_CURSOR_AREA_SIZE_300 * 2, SIS_CRT2_WENABLE_300, "SiS 540" },
	{ SIS_630,    SIS_300_VGA, HW_CURSOR_AREA_SIZE_300 * 2, SIS_CRT2_WENABLE_300, "SiS 630" },
	{ SIS_315H,   SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 315H" },
	{ SIS_315,    SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 315" },
	{ SIS_315PRO, SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 315PRO" },
	{ SIS_550,    SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 55x" },
	{ SIS_650,    SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 65x" },
	{ SIS_330,    SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 330" },
	{ SIS_660,    SIS_315_VGA, HW_CURSOR_AREA_SIZE_315 * 4, SIS_CRT2_WENABLE_315, "SiS 660" },
};

static struct pci_device_id __devinitdata sisfb_pci_table[] = {
#ifdef CONFIG_FB_SIS_300
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_300,     PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_540_VGA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_630_VGA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
#endif
#ifdef CONFIG_FB_SIS_315
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_315H,    PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_315,     PCI_ANY_ID, PCI_ANY_ID, 0, 0, 4},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_315PRO,  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 5},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_550_VGA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 6},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_650_VGA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 7},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_330,     PCI_ANY_ID, PCI_ANY_ID, 0, 0, 8},
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_660_VGA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 9},
#endif
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, sisfb_pci_table);

#define MD_SIS300 1
#define MD_SIS315 2

/* Mode table */
/* NOT const - will be patched for 1280x768 mode number chaos reasons */
struct _sisbios_mode {
	char name[15];
	u8 mode_no;
	u16 vesa_mode_no_1;  /* "SiS defined" VESA mode number */
	u16 vesa_mode_no_2;  /* Real VESA mode numbers */
	u16 xres;
	u16 yres;
	u16 bpp;
	u16 rate_idx;
	u16 cols;
	u16 rows;
	u8  chipset;
} sisbios_mode[] = {
#define MODE_INDEX_NONE           0  /* index for mode=none */
	{"none",         0xff, 0x0000, 0x0000,    0,    0,  0, 0,   0,  0, MD_SIS300|MD_SIS315},
	{"320x200x8",    0x59, 0x0138, 0x0000,  320,  200,  8, 1,  40, 12, MD_SIS300|MD_SIS315},
	{"320x200x16",   0x41, 0x010e, 0x0000,  320,  200, 16, 1,  40, 12, MD_SIS300|MD_SIS315},
	{"320x200x24",   0x4f, 0x0000, 0x0000,  320,  200, 32, 1,  40, 12, MD_SIS300|MD_SIS315},  /* TW: That's for people who mix up color- and fb depth */
	{"320x200x32",   0x4f, 0x0000, 0x0000,  320,  200, 32, 1,  40, 12, MD_SIS300|MD_SIS315},
	{"320x240x8",    0x50, 0x0132, 0x0000,  320,  240,  8, 1,  40, 15, MD_SIS300|MD_SIS315},
	{"320x240x16",   0x56, 0x0135, 0x0000,  320,  240, 16, 1,  40, 15, MD_SIS300|MD_SIS315},
	{"320x240x24",   0x53, 0x0000, 0x0000,  320,  240, 32, 1,  40, 15, MD_SIS300|MD_SIS315},
	{"320x240x32",   0x53, 0x0000, 0x0000,  320,  240, 32, 1,  40, 15, MD_SIS300|MD_SIS315},
	{"320x240x8",    0x5a, 0x0132, 0x0000,  320,  480,  8, 1,  40, 30,           MD_SIS315},  /* TW: FSTN */
	{"320x240x16",   0x5b, 0x0135, 0x0000,  320,  480, 16, 1,  40, 30,           MD_SIS315},  /* TW: FSTN */
	{"400x300x8",    0x51, 0x0133, 0x0000,  400,  300,  8, 1,  50, 18, MD_SIS300|MD_SIS315},
	{"400x300x16",   0x57, 0x0136, 0x0000,  400,  300, 16, 1,  50, 18, MD_SIS300|MD_SIS315},
	{"400x300x24",   0x54, 0x0000, 0x0000,  400,  300, 32, 1,  50, 18, MD_SIS300|MD_SIS315},
	{"400x300x32",   0x54, 0x0000, 0x0000,  400,  300, 32, 1,  50, 18, MD_SIS300|MD_SIS315},
	{"512x384x8",    0x52, 0x0000, 0x0000,  512,  384,  8, 1,  64, 24, MD_SIS300|MD_SIS315},
	{"512x384x16",   0x58, 0x0000, 0x0000,  512,  384, 16, 1,  64, 24, MD_SIS300|MD_SIS315},
	{"512x384x24",   0x5c, 0x0000, 0x0000,  512,  384, 32, 1,  64, 24, MD_SIS300|MD_SIS315},
	{"512x384x32",   0x5c, 0x0000, 0x0000,  512,  384, 32, 1,  64, 24, MD_SIS300|MD_SIS315},
	{"640x400x8",    0x2f, 0x0000, 0x0000,  640,  400,  8, 1,  80, 25, MD_SIS300|MD_SIS315},
	{"640x400x16",   0x5d, 0x0000, 0x0000,  640,  400, 16, 1,  80, 25, MD_SIS300|MD_SIS315},
	{"640x400x24",   0x5e, 0x0000, 0x0000,  640,  400, 32, 1,  80, 25, MD_SIS300|MD_SIS315},
	{"640x400x32",   0x5e, 0x0000, 0x0000,  640,  400, 32, 1,  80, 25, MD_SIS300|MD_SIS315},
	{"640x480x8",    0x2e, 0x0101, 0x0101,  640,  480,  8, 1,  80, 30, MD_SIS300|MD_SIS315},
	{"640x480x16",   0x44, 0x0111, 0x0111,  640,  480, 16, 1,  80, 30, MD_SIS300|MD_SIS315},
	{"640x480x24",   0x62, 0x013a, 0x0112,  640,  480, 32, 1,  80, 30, MD_SIS300|MD_SIS315},
	{"640x480x32",   0x62, 0x013a, 0x0112,  640,  480, 32, 1,  80, 30, MD_SIS300|MD_SIS315},
	{"720x480x8",    0x31, 0x0000, 0x0000,  720,  480,  8, 1,  90, 30, MD_SIS300|MD_SIS315},
	{"720x480x16",   0x33, 0x0000, 0x0000,  720,  480, 16, 1,  90, 30, MD_SIS300|MD_SIS315},
	{"720x480x24",   0x35, 0x0000, 0x0000,  720,  480, 32, 1,  90, 30, MD_SIS300|MD_SIS315},
	{"720x480x32",   0x35, 0x0000, 0x0000,  720,  480, 32, 1,  90, 30, MD_SIS300|MD_SIS315},
	{"720x576x8",    0x32, 0x0000, 0x0000,  720,  576,  8, 1,  90, 36, MD_SIS300|MD_SIS315},
	{"720x576x16",   0x34, 0x0000, 0x0000,  720,  576, 16, 1,  90, 36, MD_SIS300|MD_SIS315},
	{"720x576x24",   0x36, 0x0000, 0x0000,  720,  576, 32, 1,  90, 36, MD_SIS300|MD_SIS315},
	{"720x576x32",   0x36, 0x0000, 0x0000,  720,  576, 32, 1,  90, 36, MD_SIS300|MD_SIS315},
	{"768x576x8",    0x5f, 0x0000, 0x0000,  768,  576,  8, 1,  96, 36, MD_SIS300|MD_SIS315},
	{"768x576x16",   0x60, 0x0000, 0x0000,  768,  576, 16, 1,  96, 36, MD_SIS300|MD_SIS315},
	{"768x576x24",   0x61, 0x0000, 0x0000,  768,  576, 32, 1,  96, 36, MD_SIS300|MD_SIS315},
	{"768x576x32",   0x61, 0x0000, 0x0000,  768,  576, 32, 1,  96, 36, MD_SIS300|MD_SIS315},
	{"800x480x8",    0x70, 0x0000, 0x0000,  800,  480,  8, 1, 100, 30, MD_SIS300|MD_SIS315},
	{"800x480x16",   0x7a, 0x0000, 0x0000,  800,  480, 16, 1, 100, 30, MD_SIS300|MD_SIS315},
	{"800x480x24",   0x76, 0x0000, 0x0000,  800,  480, 32, 1, 100, 30, MD_SIS300|MD_SIS315},
	{"800x480x32",   0x76, 0x0000, 0x0000,  800,  480, 32, 1, 100, 30, MD_SIS300|MD_SIS315},
#define DEFAULT_MODE              43 /* index for 800x600x8 */
#define DEFAULT_LCDMODE           43 /* index for 800x600x8 */
#define DEFAULT_TVMODE            43 /* index for 800x600x8 */
	{"800x600x8",    0x30, 0x0103, 0x0103,  800,  600,  8, 2, 100, 37, MD_SIS300|MD_SIS315},
	{"800x600x16",   0x47, 0x0114, 0x0114,  800,  600, 16, 2, 100, 37, MD_SIS300|MD_SIS315},
	{"800x600x24",   0x63, 0x013b, 0x0115,  800,  600, 32, 2, 100, 37, MD_SIS300|MD_SIS315},
	{"800x600x32",   0x63, 0x013b, 0x0115,  800,  600, 32, 2, 100, 37, MD_SIS300|MD_SIS315},
	{"848x480x8",    0x39, 0x0000, 0x0000,  848,  480,  8, 2, 106, 30, MD_SIS300|MD_SIS315},
	{"848x480x16",   0x3b, 0x0000, 0x0000,  848,  480, 16, 2, 106, 30, MD_SIS300|MD_SIS315},
	{"848x480x24",   0x3e, 0x0000, 0x0000,  848,  480, 32, 2, 106, 30, MD_SIS300|MD_SIS315},
	{"848x480x32",   0x3e, 0x0000, 0x0000,  848,  480, 32, 2, 106, 30, MD_SIS300|MD_SIS315},
	{"856x480x8",    0x3f, 0x0000, 0x0000,  856,  480,  8, 2, 107, 30, MD_SIS300|MD_SIS315},
	{"856x480x16",   0x42, 0x0000, 0x0000,  856,  480, 16, 2, 107, 30, MD_SIS300|MD_SIS315},
	{"856x480x24",   0x45, 0x0000, 0x0000,  856,  480, 32, 2, 107, 30, MD_SIS300|MD_SIS315},
	{"856x480x32",   0x45, 0x0000, 0x0000,  856,  480, 32, 2, 107, 30, MD_SIS300|MD_SIS315},
	{"1024x576x8",   0x71, 0x0000, 0x0000, 1024,  576,  8, 1, 128, 36, MD_SIS300|MD_SIS315},
	{"1024x576x16",  0x74, 0x0000, 0x0000, 1024,  576, 16, 1, 128, 36, MD_SIS300|MD_SIS315},
	{"1024x576x24",  0x77, 0x0000, 0x0000, 1024,  576, 32, 1, 128, 36, MD_SIS300|MD_SIS315},
	{"1024x576x32",  0x77, 0x0000, 0x0000, 1024,  576, 32, 1, 128, 36, MD_SIS300|MD_SIS315},
	{"1024x600x8",   0x20, 0x0000, 0x0000, 1024,  600,  8, 1, 128, 37, MD_SIS300          },
	{"1024x600x16",  0x21, 0x0000, 0x0000, 1024,  600, 16, 1, 128, 37, MD_SIS300          },
	{"1024x600x24",  0x22, 0x0000, 0x0000, 1024,  600, 32, 1, 128, 37, MD_SIS300          },
	{"1024x600x32",  0x22, 0x0000, 0x0000, 1024,  600, 32, 1, 128, 37, MD_SIS300          },
	{"1024x768x8",   0x38, 0x0105, 0x0105, 1024,  768,  8, 2, 128, 48, MD_SIS300|MD_SIS315},
	{"1024x768x16",  0x4a, 0x0117, 0x0117, 1024,  768, 16, 2, 128, 48, MD_SIS300|MD_SIS315},
	{"1024x768x24",  0x64, 0x013c, 0x0118, 1024,  768, 32, 2, 128, 48, MD_SIS300|MD_SIS315},
	{"1024x768x32",  0x64, 0x013c, 0x0118, 1024,  768, 32, 2, 128, 48, MD_SIS300|MD_SIS315},
	{"1152x768x8",   0x23, 0x0000, 0x0000, 1152,  768,  8, 1, 144, 48, MD_SIS300          },
	{"1152x768x16",  0x24, 0x0000, 0x0000, 1152,  768, 16, 1, 144, 48, MD_SIS300          },
	{"1152x768x24",  0x25, 0x0000, 0x0000, 1152,  768, 32, 1, 144, 48, MD_SIS300          },
	{"1152x768x32",  0x25, 0x0000, 0x0000, 1152,  768, 32, 1, 144, 48, MD_SIS300          },
	{"1152x864x8",   0x29, 0x0000, 0x0000, 1152,  864,  8, 1, 144, 54, MD_SIS300|MD_SIS315},
	{"1152x864x16",  0x2a, 0x0000, 0x0000, 1152,  864, 16, 1, 144, 54, MD_SIS300|MD_SIS315},
	{"1152x864x24",  0x2b, 0x0000, 0x0000, 1152,  864, 32, 1, 144, 54, MD_SIS300|MD_SIS315},
	{"1152x864x32",  0x2b, 0x0000, 0x0000, 1152,  864, 32, 1, 144, 54, MD_SIS300|MD_SIS315},
	{"1280x720x8",   0x79, 0x0000, 0x0000, 1280,  720,  8, 1, 160, 45, MD_SIS300|MD_SIS315},
	{"1280x720x16",  0x75, 0x0000, 0x0000, 1280,  720, 16, 1, 160, 45, MD_SIS300|MD_SIS315},
	{"1280x720x24",  0x78, 0x0000, 0x0000, 1280,  720, 32, 1, 160, 45, MD_SIS300|MD_SIS315},
	{"1280x720x32",  0x78, 0x0000, 0x0000, 1280,  720, 32, 1, 160, 45, MD_SIS300|MD_SIS315},
#define MODEINDEX_1280x768 79
	{"1280x768x8",   0x23, 0x0000, 0x0000, 1280,  768,  8, 1, 160, 48, MD_SIS300|MD_SIS315},
	{"1280x768x16",  0x24, 0x0000, 0x0000, 1280,  768, 16, 1, 160, 48, MD_SIS300|MD_SIS315},
	{"1280x768x24",  0x25, 0x0000, 0x0000, 1280,  768, 32, 1, 160, 48, MD_SIS300|MD_SIS315},
	{"1280x768x32",  0x25, 0x0000, 0x0000, 1280,  768, 32, 1, 160, 48, MD_SIS300|MD_SIS315},
	{"1280x800x8",   0x14, 0x0000, 0x0000, 1280,  800,  8, 1, 160, 50,           MD_SIS315},
	{"1280x800x16",  0x15, 0x0000, 0x0000, 1280,  800, 16, 1, 160, 50,           MD_SIS315},
	{"1280x800x24",  0x16, 0x0000, 0x0000, 1280,  800, 32, 1, 160, 50,           MD_SIS315},
	{"1280x800x32",  0x16, 0x0000, 0x0000, 1280,  800, 32, 1, 160, 50,           MD_SIS315},
	{"1280x960x8",   0x7c, 0x0000, 0x0000, 1280,  960,  8, 1, 160, 60, MD_SIS300|MD_SIS315},
	{"1280x960x16",  0x7d, 0x0000, 0x0000, 1280,  960, 16, 1, 160, 60, MD_SIS300|MD_SIS315},
	{"1280x960x24",  0x7e, 0x0000, 0x0000, 1280,  960, 32, 1, 160, 60, MD_SIS300|MD_SIS315},
	{"1280x960x32",  0x7e, 0x0000, 0x0000, 1280,  960, 32, 1, 160, 60, MD_SIS300|MD_SIS315},
	{"1280x1024x8",  0x3a, 0x0107, 0x0107, 1280, 1024,  8, 2, 160, 64, MD_SIS300|MD_SIS315},
	{"1280x1024x16", 0x4d, 0x011a, 0x011a, 1280, 1024, 16, 2, 160, 64, MD_SIS300|MD_SIS315},
	{"1280x1024x24", 0x65, 0x013d, 0x011b, 1280, 1024, 32, 2, 160, 64, MD_SIS300|MD_SIS315},
	{"1280x1024x32", 0x65, 0x013d, 0x011b, 1280, 1024, 32, 2, 160, 64, MD_SIS300|MD_SIS315},
	{"1360x768x8",   0x48, 0x0000, 0x0000, 1360,  768,  8, 1, 170, 48, MD_SIS300|MD_SIS315},
	{"1360x768x16",  0x4b, 0x0000, 0x0000, 1360,  768, 16, 1, 170, 48, MD_SIS300|MD_SIS315},
	{"1360x768x24",  0x4e, 0x0000, 0x0000, 1360,  768, 32, 1, 170, 48, MD_SIS300|MD_SIS315},
	{"1360x768x32",  0x4e, 0x0000, 0x0000, 1360,  768, 32, 1, 170, 48, MD_SIS300|MD_SIS315},
	{"1360x1024x8",  0x67, 0x0000, 0x0000, 1360, 1024,  8, 1, 170, 64, MD_SIS300          },
	{"1360x1024x16", 0x6f, 0x0000, 0x0000, 1360, 1024, 16, 1, 170, 64, MD_SIS300          },
	{"1360x1024x24", 0x72, 0x0000, 0x0000, 1360, 1024, 32, 1, 170, 64, MD_SIS300          },
	{"1360x1024x32", 0x72, 0x0000, 0x0000, 1360, 1024, 32, 1, 170, 64, MD_SIS300          },
	{"1400x1050x8",  0x26, 0x0000, 0x0000, 1400, 1050,  8, 1, 175, 65,           MD_SIS315},
	{"1400x1050x16", 0x27, 0x0000, 0x0000, 1400, 1050, 16, 1, 175, 65,           MD_SIS315},
	{"1400x1050x24", 0x28, 0x0000, 0x0000, 1400, 1050, 32, 1, 175, 65,           MD_SIS315},
	{"1400x1050x32", 0x28, 0x0000, 0x0000, 1400, 1050, 32, 1, 175, 65,           MD_SIS315},
	{"1600x1200x8",  0x3c, 0x0130, 0x011c, 1600, 1200,  8, 1, 200, 75, MD_SIS300|MD_SIS315},
	{"1600x1200x16", 0x3d, 0x0131, 0x011e, 1600, 1200, 16, 1, 200, 75, MD_SIS300|MD_SIS315},
	{"1600x1200x24", 0x66, 0x013e, 0x011f, 1600, 1200, 32, 1, 200, 75, MD_SIS300|MD_SIS315},
	{"1600x1200x32", 0x66, 0x013e, 0x011f, 1600, 1200, 32, 1, 200, 75, MD_SIS300|MD_SIS315},
	{"1680x1050x8",  0x17, 0x0000, 0x0000, 1680, 1050,  8, 1, 210, 65,           MD_SIS315},
	{"1680x1050x16", 0x18, 0x0000, 0x0000, 1680, 1050, 16, 1, 210, 65,           MD_SIS315},
	{"1680x1050x24", 0x19, 0x0000, 0x0000, 1680, 1050, 32, 1, 210, 65,           MD_SIS315},
	{"1680x1050x32", 0x19, 0x0000, 0x0000, 1680, 1050, 32, 1, 210, 65,           MD_SIS315},
	{"1920x1440x8",  0x68, 0x013f, 0x0000, 1920, 1440,  8, 1, 240, 75, MD_SIS300|MD_SIS315},
	{"1920x1440x16", 0x69, 0x0140, 0x0000, 1920, 1440, 16, 1, 240, 75, MD_SIS300|MD_SIS315},
	{"1920x1440x24", 0x6b, 0x0141, 0x0000, 1920, 1440, 32, 1, 240, 75, MD_SIS300|MD_SIS315},
	{"1920x1440x32", 0x6b, 0x0141, 0x0000, 1920, 1440, 32, 1, 240, 75, MD_SIS300|MD_SIS315},
	{"2048x1536x8",  0x6c, 0x0000, 0x0000, 2048, 1536,  8, 1, 256, 96,           MD_SIS315},
	{"2048x1536x16", 0x6d, 0x0000, 0x0000, 2048, 1536, 16, 1, 256, 96,           MD_SIS315},
	{"2048x1536x24", 0x6e, 0x0000, 0x0000, 2048, 1536, 32, 1, 256, 96,           MD_SIS315},
	{"2048x1536x32", 0x6e, 0x0000, 0x0000, 2048, 1536, 32, 1, 256, 96,           MD_SIS315},
	{"\0", 0x00, 0, 0, 0, 0, 0, 0, 0}
};

/* mode-related variables */
#ifdef MODULE
int sisfb_mode_idx = MODE_INDEX_NONE;  /* Don't use a mode by default if we are a module */
#else
int sisfb_mode_idx = -1;               /* Use a default mode if we are inside the kernel */
#endif

/* CR36 evaluation */
const USHORT sis300paneltype[] =
    { LCD_UNKNOWN,   LCD_800x600,   LCD_1024x768,  LCD_1280x1024,
      LCD_1280x960,  LCD_640x480,   LCD_1024x600,  LCD_1152x768,
      LCD_1024x768,  LCD_1024x768,  LCD_1024x768,  LCD_1024x768,
      LCD_1024x768,  LCD_1024x768,  LCD_320x480,   LCD_1024x768 };

const USHORT sis310paneltype[] =
    { LCD_UNKNOWN,   LCD_800x600,   LCD_1024x768,  LCD_1280x1024,
      LCD_640x480,   LCD_1024x600,  LCD_1152x864,  LCD_1280x960,
      LCD_1152x768,  LCD_1400x1050, LCD_1280x768,  LCD_1600x1200,
      LCD_640x480_2, LCD_640x480_3, LCD_320x480,   LCD_1024x768 };

const USHORT sis661paneltype[] =
    { LCD_UNKNOWN,   LCD_800x600,   LCD_1024x768,  LCD_1280x1024,
      LCD_640x480,   LCD_1024x600,  LCD_1152x864,  LCD_1280x960,
      LCD_1152x768,  LCD_1400x1050, LCD_1280x768,  LCD_1600x1200,
      LCD_1280x800,  LCD_1680x1050, LCD_1280x720,  LCD_1024x768 };

#define FL_550_DSTN 0x01
#define FL_550_FSTN 0x02

static struct _sis_crt2type {
	char name[10];
	int type_no;
	int tvplug_no;
	unsigned short flags;
} sis_crt2type[] __initdata = {
	{"NONE", 	0, 		-1,        0},
	{"LCD",  	CRT2_LCD, 	-1,        0},
	{"TV",   	CRT2_TV, 	-1,        0},
	{"VGA",  	CRT2_VGA, 	-1,        0},
	{"SVIDEO", 	CRT2_TV, 	TV_SVIDEO, 0},
	{"COMPOSITE", 	CRT2_TV, 	TV_AVIDEO, 0},
	{"SCART", 	CRT2_TV, 	TV_SCART,  0},
	{"DSTN",        CRT2_LCD,       -1,        FL_550_DSTN},
	{"FSTN",        CRT2_LCD,       -1,        FL_550_FSTN},
	{"\0",  	-1, 		-1,        0}
};

/* Queue mode selection for 310 series */
static struct _sis_queuemode {
	char name[6];
	int type_no;
} sis_queuemode[] __initdata = {
	{"AGP",  	AGP_CMD_QUEUE},
	{"VRAM", 	VM_CMD_QUEUE},
	{"MMIO", 	MMIO_CMD},
	{"\0",   	-1}
};

/* TV standard */
static struct _sis_tvtype {
	char name[6];
	int type_no;
} sis_tvtype[] __initdata = {
	{"PAL",  	TV_PAL},
	{"NTSC", 	TV_NTSC},
	{"\0",   	-1}
};

static const struct _sis_vrate {
	u16 idx;
	u16 xres;
	u16 yres;
	u16 refresh;
	BOOLEAN SiS730valid32bpp;
} sisfb_vrate[] = {
	{1,  320,  200,  70,  TRUE},
	{1,  320,  240,  60,  TRUE},
	{1,  320,  480,  60,  TRUE},
	{1,  400,  300,  60,  TRUE},
	{1,  512,  384,  60,  TRUE},
	{1,  640,  400,  72,  TRUE},
	{1,  640,  480,  60,  TRUE}, {2,  640,  480,  72,  TRUE}, {3,  640,  480,  75,  TRUE},
	{4,  640,  480,  85,  TRUE}, {5,  640,  480, 100,  TRUE}, {6,  640,  480, 120,  TRUE},
	{7,  640,  480, 160,  TRUE}, {8,  640,  480, 200,  TRUE},
	{1,  720,  480,  60,  TRUE},
	{1,  720,  576,  58,  TRUE},
	{1,  768,  576,  58,  TRUE},
	{1,  800,  480,  60,  TRUE}, {2,  800,  480,  75,  TRUE}, {3,  800,  480,  85,  TRUE},
	{1,  800,  600,  56,  TRUE}, {2,  800,  600,  60,  TRUE}, {3,  800,  600,  72,  TRUE},
	{4,  800,  600,  75,  TRUE}, {5,  800,  600,  85,  TRUE}, {6,  800,  600, 105,  TRUE},
	{7,  800,  600, 120,  TRUE}, {8,  800,  600, 160,  TRUE},
	{1,  848,  480,  39,  TRUE}, {2,  848,  480,  60,  TRUE},
	{1,  856,  480,  39,  TRUE}, {2,  856,  480,  60,  TRUE},
	{1, 1024,  576,  60,  TRUE}, {2, 1024,  576,  75,  TRUE}, {3, 1024,  576,  85,  TRUE},
	{1, 1024,  600,  60,  TRUE},
	{1, 1024,  768,  43,  TRUE}, {2, 1024,  768,  60,  TRUE}, {3, 1024,  768,  70, FALSE},
	{4, 1024,  768,  75, FALSE}, {5, 1024,  768,  85,  TRUE}, {6, 1024,  768, 100,  TRUE},
	{7, 1024,  768, 120,  TRUE},
	{1, 1152,  768,  60,  TRUE},
	{1, 1152,  864,  75,  TRUE}, {2, 1152,  864,  84,  TRUE},
	{1, 1280,  720,  60,  TRUE}, {2, 1280,  720,  75,  TRUE}, {3, 1280,  720,  85,  TRUE},
	{1, 1280,  768,  60,  TRUE},
	{1, 1280,  800,  60,  TRUE},
	{1, 1280,  960,  60,  TRUE}, {2, 1280,  960,  85,  TRUE},
	{1, 1280, 1024,  43,  TRUE}, {2, 1280, 1024,  60,  TRUE}, {3, 1280, 1024,  75,  TRUE},
	{4, 1280, 1024,  85,  TRUE},
	{1, 1360,  768,  60,  TRUE},
	{1, 1360, 1024,  59,  TRUE},
	{1, 1400, 1050,  60,  TRUE}, {2, 1400, 1050,  75,  TRUE},
	{1, 1600, 1200,  60,  TRUE}, {2, 1600, 1200,  65,  TRUE}, {3, 1600, 1200,  70,  TRUE},
	{4, 1600, 1200,  75,  TRUE}, {5, 1600, 1200,  85,  TRUE}, {6, 1600, 1200, 100,  TRUE},
	{7, 1600, 1200, 120,  TRUE},
	{1, 1680, 1050,  60,  TRUE},
	{1, 1920, 1440,  60,  TRUE}, {2, 1920, 1440,  65,  TRUE}, {3, 1920, 1440,  70,  TRUE},
	{4, 1920, 1440,  75,  TRUE}, {5, 1920, 1440,  85,  TRUE}, {6, 1920, 1440, 100,  TRUE},
	{1, 2048, 1536,  60,  TRUE}, {2, 2048, 1536,  65,  TRUE}, {3, 2048, 1536,  70,  TRUE},
	{4, 2048, 1536,  75,  TRUE}, {5, 2048, 1536,  85,  TRUE},
	{0,    0,    0,   0, FALSE}
};

static struct sisfb_monitor {
	u16 hmin;
	u16 hmax;
	u16 vmin;
	u16 vmax;
	u32 dclockmax;
	u8  feature;
	BOOLEAN datavalid;
} sisfb_thismonitor;

static const struct _sisfbddcsmodes {
	u32 mask;
	u16 h;
	u16 v;
	u32 d;
} sisfb_ddcsmodes[] = {
	{ 0x10000, 67, 75, 108000},
	{ 0x08000, 48, 72,  50000},
	{ 0x04000, 46, 75,  49500},
	{ 0x01000, 35, 43,  44900},
	{ 0x00800, 48, 60,  65000},
	{ 0x00400, 56, 70,  75000},
	{ 0x00200, 60, 75,  78800},
	{ 0x00100, 80, 75, 135000},
	{ 0x00020, 31, 60,  25200},
	{ 0x00008, 38, 72,  31500},
	{ 0x00004, 37, 75,  31500},
	{ 0x00002, 35, 56,  36000},
	{ 0x00001, 38, 60,  40000}
};

static const struct _sisfbddcfmodes {
	u16 x;
	u16 y;
	u16 v;
	u16 h;
	u32 d;
} sisfb_ddcfmodes[] = {
       { 1280, 1024, 85, 92, 157500},
       { 1600, 1200, 60, 75, 162000},
       { 1600, 1200, 65, 82, 175500},
       { 1600, 1200, 70, 88, 189000},
       { 1600, 1200, 75, 94, 202500},
       { 1600, 1200, 85, 107,229500},
       { 1920, 1440, 60, 90, 234000},
       { 1920, 1440, 75, 113,297000}
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static u8 sisfb_lastrates[128];
#endif

#ifdef CONFIG_FB_SIS_300
static struct _chswtable {
    int subsysVendor;
    int subsysCard;
    char *vendorName;
    char *cardName;
} mychswtable[] __devinitdata = {
        { 0x1631, 0x1002, "Mitachi", "0x1002" },
	{ 0x1071, 0x7521, "Mitac"  , "7521P"  },
	{ 0,      0,      ""       , ""       }
};
#endif

static struct _customttable {
    unsigned short chipID;
    char *biosversion;
    char *biosdate;
    unsigned long bioschksum;
    unsigned short biosFootprintAddr[5];
    unsigned char biosFootprintData[5];
    unsigned short pcisubsysvendor;
    unsigned short pcisubsyscard;
    char *vendorName;
    char *cardName;
    unsigned long SpecialID;
    char *optionName;
} mycustomttable[] __devinitdata = {
	{ SIS_630, "2.00.07", "09/27/2002-13:38:25",
	  0x3240A8,
	  { 0x220, 0x227, 0x228, 0x229, 0x0ee },
	  {  0x01,  0xe3,  0x9a,  0x6a,  0xef },
	  0x1039, 0x6300,
	  "Barco", "iQ R200L/300/400", CUT_BARCO1366, "BARCO_1366"
	},
	{ SIS_630, "2.00.07", "09/27/2002-13:38:25",
	  0x323FBD,
	  { 0x220, 0x227, 0x228, 0x229, 0x0ee },
	  {  0x00,  0x5a,  0x64,  0x41,  0xef },
	  0x1039, 0x6300,
	  "Barco", "iQ G200L/300/400/500", CUT_BARCO1024, "BARCO_1024"
	},
	{ SIS_650, "", "",
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x0e11, 0x083c,
	  "Inventec (Compaq)", "3017cl/3045US", CUT_COMPAQ12802, "COMPAQ_1280"
	},
	{ SIS_650, "", "",
	  0,
	  { 0x00c, 0, 0, 0, 0 },
	  { 'e'  , 0, 0, 0, 0 },
	  0x1558, 0x0287,
	  "Clevo", "L285/L287 (Version 1)", CUT_CLEVO1024, "CLEVO_L28X_1"
	},
	{ SIS_650, "", "",
	  0,
	  { 0x00c, 0, 0, 0, 0 },
	  { 'y'  , 0, 0, 0, 0 },
	  0x1558, 0x0287,
	  "Clevo", "L285/L287 (Version 2)", CUT_CLEVO10242, "CLEVO_L28X_2"
	},
	{ SIS_650, "", "",
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1558, 0x0400,  /* possibly 401 and 402 as well; not panelsize specific (?) */
	  "Clevo", "D400S/D410S/D400H/D410H", CUT_CLEVO1400, "CLEVO_D4X0"
	},
	{ SIS_650, "", "",
	  0,	/* Shift LCD in LCD-via-CRT1 mode */
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1558, 0x2263,
	  "Clevo", "D22ES/D27ES", CUT_UNIWILL1024, "CLEVO_D2X0ES"
	},
	{ SIS_650, "", "",
	  0,	/* Shift LCD in LCD-via-CRT1 mode */
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1734, 0x101f,
	  "Uniwill", "N243S9", CUT_UNIWILL1024, "UNIWILL_N243S9"
	},
	{ SIS_650, "", "",
	  0,	/* Shift LCD in LCD-via-CRT1 mode */
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1584, 0x5103,
	  "Uniwill", "N35BS1", CUT_UNIWILL10242, "UNIWILL_N35BS1"
	},
	{ SIS_650, "1.09.2c", "",  /* Other versions, too? */
	  0,	/* Shift LCD in LCD-via-CRT1 mode */
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1019, 0x0f05,
	  "ECS", "A928", CUT_UNIWILL1024, "ECS_A928"
	},
	{ SIS_740, "1.11.27a", "",
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1043, 0x1612,
	  "Asus", "L3000D/L3500D", CUT_ASUSL3000D, "ASUS_L3X00"
	},
	{ SIS_650, "1.10.9k", "",
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1025, 0x0028,
	  "Acer", "Aspire 1700", CUT_ACER1280, "ACER_ASPIRE1700"
	},
	{ SIS_650, "1.10.7w", "",
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x14c0, 0x0012,
	  "Compal", "??? (V1)", CUT_COMPAL1400_1, "COMPAL_1400_1"
	},
	{ SIS_650, "1.10.7x", "",
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x14c0, 0x0012,
	  "Compal", "??? (V2)", CUT_COMPAL1400_2, "COMPAL_1400_2"
	},
	{ SIS_650, "1.10.8o", "",
	  0,	/* For EMI (unknown) */
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1043, 0x1612,
	  "Asus", "A2H (V1)", CUT_ASUSA2H_1, "ASUS_A2H_1"
	},
	{ SIS_650, "1.10.8q", "",
	  0,	/* For EMI */
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0x1043, 0x1612,
	  "Asus", "A2H (V2)", CUT_ASUSA2H_2, "ASUS_A2H_2"
	},
	{ 4321, "", "",			/* never autodetected */
	  0,
	  { 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0 },
	  0, 0,
	  "Generic", "LVDS/Parallel 848x480", CUT_PANEL848, "PANEL848x480"
	},
	{ 0, "", "",
	  0,
	  { 0, 0, 0, 0 },
	  { 0, 0, 0, 0 },
	  0, 0,
	  "", "", CUT_NONE, ""
	}
};



static unsigned long sisfb_heap_start;
static unsigned long sisfb_heap_end;
static unsigned long sisfb_heap_size;
static SIS_HEAP      sisfb_heap;

static const struct _sis_TV_filter {
	u8 filter[9][4];
} sis_TV_filter[] = {
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_0 */
	   {0x00,0xE0,0x10,0x60},
	   {0x00,0xEE,0x10,0x44},
	   {0x00,0xF4,0x10,0x38},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0x00,0x00,0x10,0x20},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_1 */
	   {0x00,0xE0,0x10,0x60},
	   {0x00,0xEE,0x10,0x44},
	   {0x00,0xF4,0x10,0x38},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0x00,0x00,0x10,0x20},
	   {0x00,0x04,0x10,0x18},
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_2 */
	   {0xF5,0xEE,0x1B,0x44},
	   {0xF8,0xF4,0x18,0x38},
	   {0xEB,0x04,0x25,0x18},
	   {0xF1,0x05,0x1F,0x16},
	   {0xF6,0x06,0x1A,0x14},
	   {0xFA,0x06,0x16,0x14},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_3 */
	   {0xF1,0x04,0x1F,0x18},
	   {0xEE,0x0D,0x22,0x06},
	   {0xF7,0x06,0x19,0x14},
	   {0xF4,0x0B,0x1C,0x0A},
	   {0xFA,0x07,0x16,0x12},
	   {0xF9,0x0A,0x17,0x0C},
	   {0x00,0x07,0x10,0x12}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_4 - 320 */
	   {0x00,0xE0,0x10,0x60},
	   {0x00,0xEE,0x10,0x44},
	   {0x00,0xF4,0x10,0x38},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0x00,0x00,0x10,0x20},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_5 - 640 */
	   {0xF5,0xEE,0x1B,0x44},
	   {0xF8,0xF4,0x18,0x38},
	   {0xEB,0x04,0x25,0x18},
	   {0xF1,0x05,0x1F,0x16},
	   {0xF6,0x06,0x1A,0x14},
	   {0xFA,0x06,0x16,0x14},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_6 - 720 */
	   {0xEB,0x04,0x25,0x18},
	   {0xE7,0x0E,0x29,0x04},
	   {0xEE,0x0C,0x22,0x08},
	   {0xF6,0x0B,0x1A,0x0A},
	   {0xF9,0x0A,0x17,0x0C},
	   {0xFC,0x0A,0x14,0x0C},
	   {0x00,0x08,0x10,0x10}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* NTSCFilter_7 - 800 */
	   {0xEC,0x02,0x24,0x1C},
	   {0xF2,0x04,0x1E,0x18},
	   {0xEB,0x15,0x25,0xF6},
	   {0xF4,0x10,0x1C,0x00},
	   {0xF8,0x0F,0x18,0x02},
	   {0x00,0x04,0x10,0x18},
	   {0x01,0x06,0x0F,0x14}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_0 */
	   {0x00,0xE0,0x10,0x60},
	   {0x00,0xEE,0x10,0x44},
	   {0x00,0xF4,0x10,0x38},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0x00,0x00,0x10,0x20},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_1 */
	   {0x00,0xE0,0x10,0x60},
	   {0x00,0xEE,0x10,0x44},
	   {0x00,0xF4,0x10,0x38},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0x00,0x00,0x10,0x20},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_2 */
	   {0xF5,0xEE,0x1B,0x44},
	   {0xF8,0xF4,0x18,0x38},
	   {0xF1,0xF7,0x01,0x32},
	   {0xF5,0xFB,0x1B,0x2A},
	   {0xF9,0xFF,0x17,0x22},
	   {0xFB,0x01,0x15,0x1E},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_3 */
	   {0xF5,0xFB,0x1B,0x2A},
	   {0xEE,0xFE,0x22,0x24},
	   {0xF3,0x00,0x1D,0x20},
	   {0xF9,0x03,0x17,0x1A},
	   {0xFB,0x02,0x14,0x1E},
	   {0xFB,0x04,0x15,0x18},
	   {0x00,0x06,0x10,0x14}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_4 - 320 */
	   {0x00,0xE0,0x10,0x60},
	   {0x00,0xEE,0x10,0x44},
	   {0x00,0xF4,0x10,0x38},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0x00,0x00,0x10,0x20},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_5 - 640 */
	   {0xF5,0xEE,0x1B,0x44},
	   {0xF8,0xF4,0x18,0x38},
	   {0xF1,0xF7,0x1F,0x32},
	   {0xF5,0xFB,0x1B,0x2A},
	   {0xF9,0xFF,0x17,0x22},
	   {0xFB,0x01,0x15,0x1E},
	   {0x00,0x04,0x10,0x18}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_6 - 720 */
	   {0xF5,0xEE,0x1B,0x2A},
	   {0xEE,0xFE,0x22,0x24},
	   {0xF3,0x00,0x1D,0x20},
	   {0xF9,0x03,0x17,0x1A},
	   {0xFB,0x02,0x14,0x1E},
	   {0xFB,0x04,0x15,0x18},
	   {0x00,0x06,0x10,0x14}, 
	   {0xFF,0xFF,0xFF,0xFF} }},
	{ {{0x00,0x00,0x00,0x40},  /* PALFilter_7 - 800 */
	   {0xF5,0xEE,0x1B,0x44},
	   {0xF8,0xF4,0x18,0x38},
	   {0xFC,0xFB,0x14,0x2A},
	   {0xEB,0x05,0x25,0x16},
	   {0xF1,0x05,0x1F,0x16},
	   {0xFA,0x07,0x16,0x12},
	   {0x00,0x07,0x10,0x12}, 
	   {0xFF,0xFF,0xFF,0xFF} }}
};

static int           filter = -1;
static unsigned char filter_tb;

/* ---------------------- Prototypes ------------------------- */

/* Interface used by the world */
#ifndef MODULE
int             sisfb_setup(char *options);
#endif

/* Interface to the low level console driver */
int             sisfb_init(void);

/* fbdev routines */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static int      sisfb_get_fix(struct fb_fix_screeninfo *fix, 
			      int con,
			      struct fb_info *info);
static int      sisfb_get_var(struct fb_var_screeninfo *var, 
			      int con,
			      struct fb_info *info);
static int      sisfb_set_var(struct fb_var_screeninfo *var, 
			      int con,
			      struct fb_info *info);
static void     sisfb_crtc_to_var(struct fb_var_screeninfo *var);			      
static int      sisfb_get_cmap(struct fb_cmap *cmap, 
			       int kspc, 
			       int con,
			       struct fb_info *info);
static int      sisfb_set_cmap(struct fb_cmap *cmap, 
			       int kspc, 
			       int con,
			       struct fb_info *info);			
static int      sisfb_update_var(int con, 
				 struct fb_info *info);
static int      sisfb_switch(int con, 
			     struct fb_info *info);
static void     sisfb_blank(int blank, 
			    struct fb_info *info);
static void     sisfb_set_disp(int con, 
			       struct fb_var_screeninfo *var, 
                               struct fb_info *info);
static int      sis_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			      unsigned *blue, unsigned *transp,
			      struct fb_info *fb_info);
static void     sisfb_do_install_cmap(int con, 
                                      struct fb_info *info);
static void     sis_get_glyph(struct fb_info *info, 
                              SIS_GLYINFO *gly);
static int 	sisfb_mmap(struct fb_info *info, struct file *file,
		           struct vm_area_struct *vma);	
static int      sisfb_ioctl(struct inode *inode, struct file *file,
		       	    unsigned int cmd, unsigned long arg, int con,
		       	    struct fb_info *info);		      
#endif			

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int      sisfb_set_par(struct fb_info *info);
static int      sisfb_blank(int blank, 
                            struct fb_info *info);			
static int 	sisfb_mmap(struct fb_info *info, struct file *file,
		           struct vm_area_struct *vma);			    
extern void     fbcon_sis_fillrect(struct fb_info *info, 
                                   const struct fb_fillrect *rect);
extern void     fbcon_sis_copyarea(struct fb_info *info, 
                                   const struct fb_copyarea *area);
extern int      fbcon_sis_sync(struct fb_info *info);
static int      sisfb_ioctl(struct inode *inode, 
	 		    struct file *file,
		       	    unsigned int cmd, 
			    unsigned long arg, 
		       	    struct fb_info *info);
extern int	sisfb_mode_rate_to_dclock(SiS_Private *SiS_Pr, 
			      PSIS_HW_INFO HwDeviceExtension,
			      unsigned char modeno, unsigned char rateindex);	
extern int      sisfb_mode_rate_to_ddata(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceExtension,
			 unsigned char modeno, unsigned char rateindex,
			 unsigned int *left_margin, unsigned int *right_margin, 
			 unsigned int *upper_margin, unsigned int *lower_margin,
			 unsigned int *hsync_len, unsigned int *vsync_len,
			 unsigned int *sync, unsigned int *vmode);
#endif
			
static int      sisfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			      struct fb_info *info);

/* Internal 2D accelerator functions */
extern int      sisfb_initaccel(void);
extern void     sisfb_syncaccel(void);

/* Internal general routines */
static void     sisfb_search_mode(char *name, BOOLEAN quiet);
static int      sisfb_validate_mode(int modeindex, unsigned long vbflags);
static u8       sisfb_search_refresh_rate(unsigned int rate, int index);
static int      sisfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			unsigned blue, unsigned transp,
			struct fb_info *fb_info);
static int      sisfb_do_set_var(struct fb_var_screeninfo *var, int isactive,
		      	struct fb_info *info);
static void     sisfb_pre_setmode(void);
static void     sisfb_post_setmode(void);

static char *   sis_find_rom(struct pci_dev *pdev);
static BOOLEAN  sisfb_CheckVBRetrace(void);
static BOOLEAN  sisfbcheckvretracecrt2(void);
static BOOLEAN  sisfbcheckvretracecrt1(void);
static BOOLEAN  sisfb_bridgeisslave(void);
static void     sisfb_detect_VB_connect(void);
static void     sisfb_get_VB_type(void);

static void     sisfb_handle_ddc(struct sisfb_monitor *monitor, int crtno);
static BOOLEAN  sisfb_interpret_edid(struct sisfb_monitor *monitor, unsigned char *buffer);

/* SiS-specific Export functions */
void            sis_dispinfo(struct ap_data *rec);
void            sis_malloc(struct sis_memreq *req);
void            sis_free(unsigned long base);

/* Chipset-dependent internal routines */
#ifdef CONFIG_FB_SIS_300
static int      sisfb_get_dram_size_300(void);
#endif
#ifdef CONFIG_FB_SIS_315
static int      sisfb_get_dram_size_315(void);
#endif

/* Internal heap routines */
static int      sisfb_heap_init(void);
static SIS_OH   *sisfb_poh_new_node(void);
static SIS_OH   *sisfb_poh_allocate(unsigned long size);
static void     sisfb_delete_node(SIS_OH *poh);
static void     sisfb_insert_node(SIS_OH *pohList, SIS_OH *poh);
static SIS_OH   *sisfb_poh_free(unsigned long base);
static void     sisfb_free_node(SIS_OH *poh);

/* Internal routines to access PCI configuration space */
BOOLEAN         sisfb_query_VGA_config_space(PSIS_HW_INFO psishw_ext,
	          	unsigned long offset, unsigned long set, unsigned long *value);
BOOLEAN         sisfb_query_north_bridge_space(PSIS_HW_INFO psishw_ext,
	         	unsigned long offset, unsigned long set, unsigned long *value);

/* Sensing routines */
static void     SiS_Sense30x(void);
static int      SISDoSense(int tempbl, int tempbh, int tempcl, int tempch);
static void     SiS_SenseCh(void);

/* Routines from init.c/init301.c */
extern USHORT   SiS_GetModeID(int VGAEngine, ULONG VBFlags, int HDisplay, int VDisplay, int Depth, BOOLEAN FSTN);
extern USHORT   SiS_GetModeID_LCD(int VGAEngine, ULONG VBFlags, int HDisplay, int VDisplay, int Depth,
                                  BOOLEAN FSTN, USHORT CustomT, int LCDwith, int LCDheight);
extern USHORT   SiS_GetModeID_TV(int VGAEngine, ULONG VBFlags, int HDisplay, int VDisplay, int Depth);
extern USHORT   SiS_GetModeID_VGA2(int VGAEngine, ULONG VBFlags, int HDisplay, int VDisplay, int Depth);

extern void 	SiSRegInit(SiS_Private *SiS_Pr, SISIOADDRESS BaseAddr);
extern BOOLEAN  SiSSetMode(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceInfo, USHORT ModeNo);
extern void     SiS_SetEnableDstn(SiS_Private *SiS_Pr, int enable);
extern void     SiS_SetEnableFstn(SiS_Private *SiS_Pr, int enable);

extern BOOLEAN  SiSDetermineROMLayout661(SiS_Private *SiS_Pr, PSIS_HW_INFO HwInfo);

extern BOOLEAN  sisfb_gettotalfrommode(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceExtension,
		       unsigned char modeno, int *htotal, int *vtotal, unsigned char rateindex);

/* Chrontel TV functions */
extern USHORT 	SiS_GetCH700x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH700x(SiS_Private *SiS_Pr, USHORT tempbx);
extern USHORT 	SiS_GetCH701x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH701x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void     SiS_SetCH70xxANDOR(SiS_Private *SiS_Pr, USHORT tempax,USHORT tempbh);
extern void     SiS_DDC2Delay(SiS_Private *SiS_Pr, USHORT delaytime);
extern void     SiS_SetChrontelGPIO(SiS_Private *SiS_Pr, USHORT myvbinfo);
extern USHORT   SiS_HandleDDC(SiS_Private *SiS_Pr, unsigned long VBFlags, int VGAEngine,
		              USHORT adaptnum, USHORT DDCdatatype, unsigned char *buffer);
extern USHORT   SiS_ReadDDC1Bit(SiS_Private *SiS_Pr);			      
extern void 	SiS_Chrontel701xBLOn(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceInfo);
extern void 	SiS_Chrontel701xBLOff(SiS_Private *SiS_Pr);
extern void 	SiS_SiS30xBLOn(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceInfo);
extern void 	SiS_SiS30xBLOff(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceInfo);
#endif


