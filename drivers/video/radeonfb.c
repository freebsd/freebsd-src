/*
 *	drivers/video/radeonfb.c
 *	framebuffer driver for ATI Radeon chipset video boards
 *
 *	Copyright 2000	Ani Joshi <ajoshi@kernel.crashing.org>
 *
 *
 *	ChangeLog:
 *	2000-08-03	initial version 0.0.1
 *	2000-09-10	more bug fixes, public release 0.0.5
 *	2001-02-19	mode bug fixes, 0.0.7
 *	2001-07-05	fixed scrolling issues, engine initialization,
 *			and minor mode tweaking, 0.0.9
 *	2001-09-07	Radeon VE support, Nick Kurshev
 *			blanking, pan_display, and cmap fixes, 0.1.0
 *	2001-10-10	Radeon 7500 and 8500 support, and experimental
 *			flat panel support, 0.1.1
 *	2001-11-17	Radeon M6 (ppc) support, Daniel Berlin, 0.1.2
 *	2001-11-18	DFP fixes, Kevin Hendricks, 0.1.3
 *	2001-11-29	more cmap, backlight fixes, Benjamin Herrenschmidt
 *	2002-01-18	DFP panel detection via BIOS, Michael Clark, 0.1.4
 *	2002-06-02	console switching, mode set fixes, accel fixes
 *	2002-06-03	MTRR support, Peter Horton, 0.1.5
 *	2002-09-21	rv250, r300, m9 initial support,
 *			added mirror option, 0.1.6
 *	2003-04-10	accel engine fixes, 0.1.7
 *	2003-04-12	Mac PowerBook sleep fixes, Benjamin Herrenschmidt,
 *			0.1.8
 *
 * Other change (--BenH)
 * 
 * 	2003-01-01	- Tweaks for PLL on some iBooks
 * 			- Fix SURFACE_CNTL usage on r9000	
 *      2003-03-23	- Added new Power Management code from ATI
 *      		- Added default PLL values for r300 from lkml
 *      		- Fix mirror ioctl result code (that ioctl still need some
 *      		  rework to actually use the second head)
 *      2003-03-26	- Never set TMDS_PLL_EN, it seem to break more than
 *                        just old r300's
 *      2003-04-02	- Got final word from ATI, TMDS_PLL_EN has to be flipped
 *      		  depending if we are dealing with an "RV" card or not
 *      		- Comsetic changes to sleep code, make it a bit more robust
 *      		  hopefully
 *      		- Fix 800x600-8 mode accel (Daniel Mantione)
 *      		- Fix scaling on LCDs (not yet preserving aspect ratio though)
 *      		- Properly set scroll mode to SCROLL_YREDRAW when accel
 *      		  is disabled from fbset
 *      		- Add some more radeon PCI IDs & default PLL values
 *      2003-04-05	- Update the code that retreive the panel infos from the
 *                        BIOS to match what XFree is doing
 *                      - Avoid a divide by 0 when failing to retreive those infos
 *      2003-04-07	- Fix the M6 video RAM workaround
 *      		- Some bits in the PM code were flipped, fix that.
 *      		- RB2D_DSTCACHE_MODE shouldn't be cleared on r300 (and
 *      		  maybe not on others according to a comment in XFree, but
 *      		  we keep working code for now).
 *      		- Use ROP3_S for rectangle fill
 *      2003-04-10      - Re-change the pitch workaround. We now align the pitch
 *      		  when accel is enabled for a given mode, and we don't when
 *      		  accel is disabled. That should properly deal with all cases
 *      		  and allows us to remove the "special case" accel code
 *      		- Bring in XFree workaround to not write the same value to
 *      		  the PLL (can cause blanking of some panels)
 *      		- Bring in some of Peter Horton fixes (accel reset, cleanups)
 *      		  still some more to get in though...
 *      		- Back to use of ROP3_P for rectangle fill (hrm...)
 *      2003-04-11	- Properly reset accel engine on each console switch so
 *      		  we work around switching from XFree leaving it in a weird
 *      		  state. Also extend the comparison of values causing us to
 *      		  reload the mode on console switch.
 *      2003-04-30	- For BIOS returned LCD infos, assume high sync polarity
 *      2003-07-08	- Fix an oops during boot related to disp not beeing initialized
 *      		  when modedb called us back for set_var. Remove bogus refs to
 *      		  RADEON_PM chip, this is really a mach64 chip, not a Radeon.
 *      		  Add some DFP blanking support
 *      2003-07-11	- Merged with Ani's 0.1.8 version
 *      
 *	Special thanks to ATI DevRel team for their hardware donations,
 *	and for spending the time to fix the power management code !
 *	
 *	Note: This driver in in bad need of beeing completely re-organized.
 *	      My long term plans, if I ever get enough time for that, is
 *	      to split the actual mode setting code so it can properly 
 *	      work on any head, the probe code, which will be stuffed with
 *	      OF parsing on PPC and i2c fallback (look at what XFree does)
 *	      and the PM code ought to be in a separate file. --BenH.
 *
 *
 *      Known Bugs:
 *      
 *       - Incompatible with ATI FireGL drivers. They are playing with things
 *         like MC_FB_LOCATION behind our back. Not much we can do. This is
 *         becoming a real problem as DRI is also playing with those and the
 *         GATOS CVS as well in a different way.
 *         We should really define _once for all_ the way we want those setup
 *         and do it the same way everywhere or we won't be able to keep
 *         compatibility with radeonfb.
 *         IMHO, the proper setup is what radeon_fixup_apertures() does on
 *         PPC when SET_MC_FB_FROM_APERTURE is defined (not the case currently
 *         because of compatiblity problems with DRI). This is, I think, also
 *         what GATOS does. We shall ask ATI what they do in the FireGL drivers
 *       - We don't preserve aspect ratio on scaled modes on LCDs yet
 *       - The way we retreive the BIOS informations probably doesn't work with
 *         anything but the primary card since we need a "live" BIOS image in
 *         memory to find the tables configured by the BIOS during POST stage.
 */


#define RADEON_VERSION	"0.1.8-ben"


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
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#ifdef CONFIG_ALL_PPC
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <video/macmodes.h>

#ifdef CONFIG_NVRAM
#include <linux/nvram.h>
#endif

#ifdef CONFIG_PMAC_BACKLIGHT
#include <asm/backlight.h>
#endif

#ifdef CONFIG_BOOTX_TEXT
#include <asm/btext.h>
#endif

#ifdef CONFIG_ADB_PMU
#include <linux/adb.h>
#include <linux/pmu.h>
#endif

#endif /* CONFIG_ALL_PPC */

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <video/fbcon.h> 
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "radeon.h"

#include <linux/radeonfb.h>


#define DEBUG	0

#if DEBUG
#define RTRACE		printk
#else
#define RTRACE		if(0) printk
#endif



enum radeon_chips {
	RADEON_QD,
	RADEON_QE,
	RADEON_QF,
	RADEON_QG,
	RADEON_QY,
	RADEON_QZ,
	RADEON_LW,
	RADEON_LX,
	RADEON_LY,
	RADEON_LZ,
	RADEON_QL,
	RADEON_QN,
	RADEON_QO,
	RADEON_Ql,
	RADEON_BB,
	RADEON_QM,
	RADEON_QW,
	RADEON_QX,
	RADEON_Id,
	RADEON_Ie,
	RADEON_If,
	RADEON_Ig,
	RADEON_Y_,
	RADEON_Ya,
	RADEON_Yd,
	RADEON_Ld,
	RADEON_Le,
	RADEON_Lf,
	RADEON_Lg,
	RADEON_ND,
	RADEON_NE,
	RADEON_NF,
	RADEON_NG,
	RADEON_AE,
	RADEON_AF,
	RADEON_AD,
	RADEON_NH,
	RADEON_NI,
	RADEON_AP,
	RADEON_AR,
};

enum radeon_arch {
	RADEON_R100,
	RADEON_M6,
	RADEON_RV100,
	RADEON_R200,
	RADEON_M7,
	RADEON_RV200,
	RADEON_M9,
	RADEON_RV250,
	RADEON_RV280,
	RADEON_R300,
	RADEON_R350,
	RADEON_RV350,
};

static struct radeon_chip_info {
	const char *name;
	unsigned char arch;
} radeon_chip_info[] __devinitdata = {
	{ "QD", RADEON_R100 },
	{ "QE", RADEON_R100 },
	{ "QF", RADEON_R100 },
	{ "QG", RADEON_R100 },
	{ "VE QY", RADEON_RV100 },
	{ "VE QZ", RADEON_RV100 },
	{ "M7 LW", RADEON_M7 },
	{ "M7 LX", RADEON_M7 },
	{ "M6 LY", RADEON_M6 },
	{ "M6 LZ", RADEON_M6 },
	{ "8500 QL", RADEON_R200 },
	{ "8500 QN", RADEON_R200 },
	{ "8500 QO", RADEON_R200 },
	{ "8500 Ql", RADEON_R200 },
	{ "8500 BB", RADEON_R200 },
	{ "9100 QM", RADEON_R200 },
	{ "7500 QW", RADEON_RV200 },
	{ "7500 QX", RADEON_RV200 },
	{ "9000 Id", RADEON_RV250 },
	{ "9000 Ie", RADEON_RV250 },
	{ "9000 If", RADEON_RV250 },
	{ "9000 Ig", RADEON_RV250 },
	{ "9200 Y", RADEON_RV280 },
	{ "9200 Ya", RADEON_RV280 },
	{ "9200 Yd", RADEON_RV280 },
	{ "M9 Ld", RADEON_M9 },
	{ "M9 Le", RADEON_M9 },
	{ "M9 Lf", RADEON_M9 },
	{ "M9 Lg", RADEON_M9 },
	{ "9700 ND", RADEON_R300 },
	{ "9700 NE", RADEON_R300 },
	{ "9700 NF", RADEON_R350 },
	{ "9700 NG", RADEON_R350 },
	{ "9700 AE", RADEON_R300 },
	{ "9700 AF", RADEON_R300 },
	{ "9500 AD", RADEON_R300 },
	{ "9800 NH", RADEON_R350 },
	{ "9800 NI", RADEON_R350 },
	{ "9600 AP", RADEON_RV350 },
	{ "9600 AR", RADEON_RV350 },
};


enum radeon_montype
{
	MT_NONE,
	MT_CRT,		/* CRT */
	MT_LCD,		/* LCD */
	MT_DFP,		/* DVI */
	MT_CTV,		/* composite TV */
	MT_STV		/* S-Video out */
};


static struct pci_device_id radeonfb_pci_table[] __devinitdata = {
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QD, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QD},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QE},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QF},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QG, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QG},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QY, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QY},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QZ, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QZ},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LW, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LW},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LX, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LX},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LY, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LY},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LZ, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LZ},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QL, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QL},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QN, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QN},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QO, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QO},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ql, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Ql},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_BB, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_BB},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QM, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QM},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QW, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QW},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QX, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QX},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Id, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Id},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ie, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Ie},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_If, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_If},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ig, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Ig},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ld, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Ld},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Le, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Le},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Lf, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Lf},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Lg, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Lg},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_ND, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_ND},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_NE},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_NF},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NG, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_NG},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_AE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_AE},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_AF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_AF},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NH, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_NH},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NI, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_NI},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Y_, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Y_},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ya, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Ya},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Yd, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_Yd},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_AD, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_AD},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_AP, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_AP},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_AR, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_AR},
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, radeonfb_pci_table);


typedef struct {
	u16 reg;
	u32 val;
} reg_val;


/* these common regs are cleared before mode setting so they do not
 * interfere with anything
 */
reg_val common_regs[] = {
	{ OVR_CLR, 0 },	
	{ OVR_WID_LEFT_RIGHT, 0 },
	{ OVR_WID_TOP_BOTTOM, 0 },
	{ OV0_SCALE_CNTL, 0 },
	{ SUBPIC_CNTL, 0 },
	{ VIPH_CONTROL, 0 },
	{ I2C_CNTL_1, 0 },
	{ GEN_INT_CNTL, 0 },
	{ CAP0_TRIG_CNTL, 0 },
};

reg_val common_regs_m6[] = {
	{ OVR_CLR, 0 },
	{ OVR_WID_LEFT_RIGHT, 0 },
	{ OVR_WID_TOP_BOTTOM, 0 },
	{ OV0_SCALE_CNTL, 0 },
	{ SUBPIC_CNTL, 0 },
	{ GEN_INT_CNTL, 0 },
	{ CAP0_TRIG_CNTL, 0 }
};


#define COMMON_REGS_SIZE = (sizeof(common_regs)/sizeof(common_regs[0]))
#define COMMON_REGS_M6_SIZE = (sizeof(common_regs)/sizeof(common_regs_m6[0]))

typedef struct {
        u8 clock_chip_type;
        u8 struct_size;
        u8 accelerator_entry;
        u8 VGA_entry;
        u16 VGA_table_offset;
        u16 POST_table_offset;
        u16 XCLK;
        u16 MCLK;
        u8 num_PLL_blocks;
        u8 size_PLL_blocks;
        u16 PCLK_ref_freq;
        u16 PCLK_ref_divider;
        u32 PCLK_min_freq;
        u32 PCLK_max_freq;
        u16 MCLK_ref_freq;
        u16 MCLK_ref_divider;
        u32 MCLK_min_freq;
        u32 MCLK_max_freq;
        u16 XCLK_ref_freq;
        u16 XCLK_ref_divider;
        u32 XCLK_min_freq;
        u32 XCLK_max_freq;
} __attribute__ ((packed)) PLL_BLOCK;


struct pll_info {
	int ppll_max;
	int ppll_min;
	int xclk;
	int ref_div;
	int ref_clk;
};


struct ram_info {
	int ml;
	int mb;
	int trcd;
	int trp;
	int twr;
	int cl;
	int tr2w;
	int loop_latency;
	int rloop;
};


struct radeon_regs {
	/* CRTC regs */
	u32 crtc_h_total_disp;
	u32 crtc_h_sync_strt_wid;
	u32 crtc_v_total_disp;
	u32 crtc_v_sync_strt_wid;
	u32 crtc_pitch;
	u32 crtc_gen_cntl;
	u32 crtc_ext_cntl;
	u32 crtc_more_cntl;
	u32 dac_cntl;

	u32 flags;
	u32 pix_clock;
	int xres, yres;

	/* DDA regs */
	u32 dda_config;
	u32 dda_on_off;

	/* PLL regs */
	u32 ppll_div_3;
	u32 ppll_ref_div;

	/* Flat panel regs */
	u32 fp_crtc_h_total_disp;
	u32 fp_crtc_v_total_disp;
	u32 fp_gen_cntl;
	u32 fp_h_sync_strt_wid;
	u32 fp_horz_stretch;
	u32 fp_panel_cntl;
	u32 fp_v_sync_strt_wid;
	u32 fp_vert_stretch;
	u32 lvds_gen_cntl;
	u32 lvds_pll_cntl;
	u32 tmds_crc;
	u32 tmds_transmitter_cntl;

	/* Dynamic clock control */
	u32 vclk_ecp_cntl;

	/* Endian control */
	u32 surface_cntl;
};


struct radeonfb_info {
	struct fb_info info;

	struct radeon_regs state;
	struct radeon_regs init_state;

	char name[16];
	char ram_type[12];

	unsigned long mmio_base_phys;
	unsigned long fb_base_phys;

	unsigned long mmio_base;
	unsigned long fb_base;

	unsigned long fb_local_base;

	struct pci_dev *pdev;

	unsigned char *EDID;
	unsigned char *bios_seg;

	struct display disp;
	int currcon;
	struct display *currcon_display;

	struct { u8 red, green, blue, pad; } palette[256];

	short chipset;
	unsigned char arch;
	int video_ram;
	u8 rev;
	int pitch, bpp, depth;
	int xres, yres, pixclock;
	int xres_virtual, yres_virtual;

	int use_default_var;
	int got_dfpinfo;

	int hasCRTC2;
	int crtDisp_type;
	int dviDisp_type;

	int panel_xres, panel_yres;
	int clock;
	int hOver_plus, hSync_width, hblank;
	int vOver_plus, vSync_width, vblank;
	int hAct_high, vAct_high, interlaced;
	int synct, misc;

	u32 dp_gui_master_cntl;

	struct pll_info pll;
	int pll_output_freq, post_div, fb_div;

	struct ram_info ram;

	int mtrr_hdl;

#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB32)
        union {
#if defined(FBCON_HAS_CFB16)
                u_int16_t cfb16[16];
#endif
#if defined(FBCON_HAS_CFB24)
                u_int32_t cfb24[16];
#endif  
#if defined(FBCON_HAS_CFB32)
                u_int32_t cfb32[16];
#endif  
        } con_cmap;
#endif  

#ifdef CONFIG_PMAC_PBOOK
	int pm_reg;
	u32 save_regs[64];
	u32 mdll, mdll2;
#endif

	int asleep;

	struct radeonfb_info *next;
};


static struct fb_var_screeninfo radeonfb_default_var = {
        640, 480, 640, 480, 0, 0, 8, 0,
        {0, 6, 0}, {0, 6, 0}, {0, 6, 0}, {0, 0, 0},
        0, 0, -1, -1, 0, 39721, 40, 24, 32, 11, 96, 2,
        0, FB_VMODE_NONINTERLACED
};


/*
 * IO macros
 */

#define INREG8(addr)		readb((rinfo->mmio_base)+addr)
#define OUTREG8(addr,val)	writeb(val, (rinfo->mmio_base)+addr)
#define INREG(addr)		readl((rinfo->mmio_base)+addr)
#define OUTREG(addr,val)	writel(val, (rinfo->mmio_base)+addr)

#define OUTPLL(addr,val)						\
	do {								\
		OUTREG8(CLOCK_CNTL_INDEX, (addr & 0x0000003f) | 0x00000080); \
		OUTREG(CLOCK_CNTL_DATA, val);				\
	} while (0)							\

#define OUTPLLP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INPLL(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTPLL(addr, _tmp);					\
	} while (0)

#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)


static __inline__ u32 _INPLL(struct radeonfb_info *rinfo, u32 addr)
{
	OUTREG8(CLOCK_CNTL_INDEX, addr & 0x0000003f);
	return (INREG(CLOCK_CNTL_DATA));
}

#define INPLL(addr)		_INPLL(rinfo, addr)

#define PRIMARY_MONITOR(rinfo)	((rinfo->dviDisp_type != MT_NONE) &&	\
				 (rinfo->dviDisp_type != MT_STV) &&	\
				 (rinfo->dviDisp_type != MT_CTV) ?	\
				 rinfo->dviDisp_type : rinfo->crtDisp_type)

static char *GET_MON_NAME(int type)
{
	char *pret = NULL;

	switch (type) {
		case MT_NONE:
			pret = "no";
			break;
		case MT_CRT:
			pret = "CRT";
			break;
		case MT_DFP:
			pret = "DFP";
			break;
		case MT_LCD:
			pret = "LCD";
			break;
		case MT_CTV:
			pret = "CTV";
			break;
		case MT_STV:
			pret = "STV";
			break;
	}

	return pret;
}


/*
 * 2D engine routines
 */

static __inline__ void radeon_engine_flush (struct radeonfb_info *rinfo)
{
	int i;

	/* initiate flush */
	OUTREGP(RB2D_DSTCACHE_CTLSTAT, RB2D_DC_FLUSH_ALL,
	        ~RB2D_DC_FLUSH_ALL);

	for (i=0; i < 2000000; i++) {
		if (!(INREG(RB2D_DSTCACHE_CTLSTAT) & RB2D_DC_BUSY))
			break;
	}
}


static __inline__ void _radeon_fifo_wait (struct radeonfb_info *rinfo, int entries)
{
	int i;

	for (i=0; i<2000000; i++)
		if ((INREG(RBBM_STATUS) & 0x7f) >= entries)
			return;
}


static __inline__ void _radeon_engine_idle (struct radeonfb_info *rinfo)
{
	int i;

	/* ensure FIFO is empty before waiting for idle */
	_radeon_fifo_wait (rinfo, 64);

	for (i=0; i<2000000; i++) {
		if (((INREG(RBBM_STATUS) & GUI_ACTIVE)) == 0) {
			radeon_engine_flush (rinfo);
			return;
		}
	}
}


#define radeon_engine_idle()		_radeon_engine_idle(rinfo)
#define radeon_fifo_wait(entries)	_radeon_fifo_wait(rinfo,entries)



/*
 * helper routines
 */

static __inline__ u32 radeon_get_dstbpp(u16 depth)
{
	switch (depth) {
		case 8:
			return DST_8BPP;
		case 15:
			return DST_15BPP;
		case 16:
			return DST_16BPP;
		case 32:
			return DST_32BPP;
		default:
			return 0;
	}
}


static inline int var_to_depth(const struct fb_var_screeninfo *var)
{
	if (var->bits_per_pixel != 16)
		return var->bits_per_pixel;
	return (var->green.length == 6) ? 16 : 15;
}


static void _radeon_engine_reset(struct radeonfb_info *rinfo)
{
	u32 clock_cntl_index, mclk_cntl, rbbm_soft_reset;
	u32 host_path_cntl;

	radeon_engine_flush (rinfo);

    	/* Some ASICs have bugs with dynamic-on feature, which are  
     	 * ASIC-version dependent, so we force all blocks on for now
     	 * -- from XFree86
     	 * We don't do that on macs, things just work here with dynamic
     	 * clocking... --BenH
	 */
#ifdef CONFIG_ALL_PPC
	if (_machine != _MACH_Pmac && rinfo->hasCRTC2)
#else
	if (rinfo->hasCRTC2)
#endif	
	{
		u32 tmp;

		tmp = INPLL(SCLK_CNTL);
		OUTPLL(SCLK_CNTL, ((tmp & ~DYN_STOP_LAT_MASK) |
				   CP_MAX_DYN_STOP_LAT |
				   SCLK_FORCEON_MASK));

		if (rinfo->arch == RADEON_RV200)
		{
			tmp = INPLL(SCLK_MORE_CNTL);
			OUTPLL(SCLK_MORE_CNTL, tmp | SCLK_MORE_FORCEON);
		}
	}

	clock_cntl_index = INREG(CLOCK_CNTL_INDEX);
	mclk_cntl = INPLL(MCLK_CNTL);

	OUTPLL(MCLK_CNTL, (mclk_cntl |
			   FORCEON_MCLKA |
			   FORCEON_MCLKB |
			   FORCEON_YCLKA |
			   FORCEON_YCLKB |
			   FORCEON_MC |
			   FORCEON_AIC));

	host_path_cntl = INREG(HOST_PATH_CNTL);
	rbbm_soft_reset = INREG(RBBM_SOFT_RESET);

	if (rinfo->arch == RADEON_R300) {
		u32 tmp;

		OUTREG(RBBM_SOFT_RESET, (rbbm_soft_reset |
					 SOFT_RESET_CP |
					 SOFT_RESET_HI |
					 SOFT_RESET_E2));
		INREG(RBBM_SOFT_RESET);
		OUTREG(RBBM_SOFT_RESET, 0);
		tmp = INREG(RB2D_DSTCACHE_MODE);
		OUTREG(RB2D_DSTCACHE_MODE, tmp | (1 << 17)); /* FIXME */
	} else {
		OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset |
					SOFT_RESET_CP |
					SOFT_RESET_HI |
					SOFT_RESET_SE |
					SOFT_RESET_RE |
					SOFT_RESET_PP |
					SOFT_RESET_E2 |
					SOFT_RESET_RB);
		INREG(RBBM_SOFT_RESET);
		OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset & (u32)
					~(SOFT_RESET_CP |
					  SOFT_RESET_HI |
					  SOFT_RESET_SE |
					  SOFT_RESET_RE |
					  SOFT_RESET_PP |
					  SOFT_RESET_E2 |
					  SOFT_RESET_RB));
		INREG(RBBM_SOFT_RESET);
	}

	OUTREG(HOST_PATH_CNTL, host_path_cntl | HDP_SOFT_RESET);
	INREG(HOST_PATH_CNTL);
	OUTREG(HOST_PATH_CNTL, host_path_cntl);

	if (rinfo->arch != RADEON_R300)
		OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset);

	OUTREG(CLOCK_CNTL_INDEX, clock_cntl_index);
	OUTPLL(MCLK_CNTL, mclk_cntl);

	return;
}

#define radeon_engine_reset()		_radeon_engine_reset(rinfo)


static __inline__ int round_div(int num, int den)
{
        return (num + (den / 2)) / den;
}

static __inline__ int min_bits_req(int val)
{
        int bits_req = 0;
                
        if (val == 0)
                bits_req = 1;
                        
        while (val) {
                val >>= 1;
                bits_req++;
        }       

        return (bits_req);
}

static __inline__ int _max(int val1, int val2)
{
        if (val1 >= val2)
                return val1;
        else
                return val2;
}                       



/*
 * globals
 */
        
static char fontname[40] __initdata;
static char *mode_option __initdata;
static char noaccel = 0;
static char mirror = 0;
static int panel_yres __initdata = 0;
static char force_dfp __initdata = 0;
static char force_crt __initdata = 0;
static char force_nolcd __initdata = 0;
static struct radeonfb_info *board_list = NULL;
static char nomtrr __initdata = 0;

#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_radeon8;
#endif

#ifdef FBCON_HAS_CFB16
static struct display_switch fbcon_radeon16;
#endif

#ifdef FBCON_HAS_CFB32
static struct display_switch fbcon_radeon32;
#endif


/*
 * prototypes
 */

static int radeonfb_get_fix (struct fb_fix_screeninfo *fix, int con,
                             struct fb_info *info);
static int radeonfb_get_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info);
static int radeonfb_set_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info);
static int radeonfb_get_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info);
static int radeonfb_set_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info);
static int radeonfb_pan_display (struct fb_var_screeninfo *var, int con,
                                 struct fb_info *info);
static int radeonfb_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                           unsigned long arg, int con, struct fb_info *info);
static int radeonfb_switch (int con, struct fb_info *info);
static int radeonfb_updatevar (int con, struct fb_info *info);
static void radeonfb_blank (int blank, struct fb_info *info);
static int radeon_get_cmap_len (const struct fb_var_screeninfo *var);
static int radeon_getcolreg (unsigned regno, unsigned *red, unsigned *green,
                             unsigned *blue, unsigned *transp,
                             struct fb_info *info);
static int radeon_setcolreg (unsigned regno, unsigned red, unsigned green,
                             unsigned blue, unsigned transp, struct fb_info *info);
static void radeon_set_dispsw (struct radeonfb_info *rinfo, struct display *disp);
static void radeon_save_state (struct radeonfb_info *rinfo,
                               struct radeon_regs *save);
static int radeon_engine_init (struct radeonfb_info *rinfo);
static void radeon_load_video_mode (struct radeonfb_info *rinfo,
                                    struct fb_var_screeninfo *mode);
static void radeon_write_mode (struct radeonfb_info *rinfo,
                               struct radeon_regs *mode);
static int __devinit radeon_set_fbinfo (struct radeonfb_info *rinfo);
static int __devinit radeon_init_disp (struct radeonfb_info *rinfo);
static int radeonfb_pci_register (struct pci_dev *pdev,
                                 const struct pci_device_id *ent);
static void __devexit radeonfb_pci_unregister (struct pci_dev *pdev);
static int radeon_do_set_var (struct fb_var_screeninfo *var, int con,
                             int real, struct fb_info *info);

#ifdef CONFIG_PMAC_PBOOK
static int radeon_sleep_notify(struct pmu_sleep_notifier *self, int when);
static struct pmu_sleep_notifier radeon_sleep_notifier = {
	radeon_sleep_notify, SLEEP_LEVEL_VIDEO,
};
#endif /* CONFIG_PMAC_PBOOK */
#ifdef CONFIG_PMAC_BACKLIGHT
static int radeon_set_backlight_enable(int on, int level, void *data);
static int radeon_set_backlight_level(int level, void *data);
static struct backlight_controller radeon_backlight_controller = {
	radeon_set_backlight_enable,
	radeon_set_backlight_level
};
static void OUTMC( struct radeonfb_info *rinfo, u8 indx, u32 value);
static u32 INMC(struct radeonfb_info *rinfo, u8 indx);
static void radeon_pm_disable_dynamic_mode(struct radeonfb_info *rinfo);
static void radeon_pm_enable_dynamic_mode(struct radeonfb_info *rinfo);
static void radeon_pm_yclk_mclk_sync(struct radeonfb_info *rinfo);
static void radeon_pm_program_mode_reg(struct radeonfb_info *rinfo, u16 value, u8 delay_required);
static void radeon_pm_enable_dll(struct radeonfb_info *rinfo);
static void radeon_pm_full_reset_sdram(struct radeonfb_info *rinfo);
#endif /* CONFIG_PMAC_BACKLIGHT */

static struct fb_ops radeon_fb_ops = {
	fb_get_fix:		radeonfb_get_fix,
	fb_get_var:		radeonfb_get_var,
	fb_set_var:		radeonfb_set_var,
	fb_get_cmap:		radeonfb_get_cmap,
	fb_set_cmap:		radeonfb_set_cmap,
	fb_pan_display:		radeonfb_pan_display,
	fb_ioctl:		radeonfb_ioctl,
};


static struct pci_driver radeonfb_driver = {
	name:		"radeonfb",
	id_table:	radeonfb_pci_table,
	probe:		radeonfb_pci_register,
	remove:		__devexit_p(radeonfb_pci_unregister),
};


int __init radeonfb_init (void)
{
	return pci_module_init (&radeonfb_driver);
}


void __exit radeonfb_exit (void)
{
	pci_unregister_driver (&radeonfb_driver);
}


int __init radeonfb_setup (char *options)
{
        char *this_opt;

        if (!options || !*options)
                return 0;
 
	while ((this_opt = strsep (&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
                if (!strncmp (this_opt, "font:", 5)) {
                        char *p;
                        int i;
        
                        p = this_opt + 5;
                        for (i=0; i<sizeof (fontname) - 1; i++)
                                if (!*p || *p == ' ' || *p == ',')
                                        break;
                        memcpy(fontname, this_opt + 5, i);
                } else if (!strncmp(this_opt, "noaccel", 7)) {
			noaccel = 1;
                } else if (!strncmp(this_opt, "mirror", 6)) {
			mirror = 1;
		} else if (!strncmp(this_opt, "dfp", 3)) {
			force_dfp = 1;
			force_nolcd = 1;
		} else if (!strncmp(this_opt, "crt", 3)) {
			force_crt = 1;
			force_nolcd = 1;
		} else if (!strncmp(this_opt, "nolcd", 5)) {
			force_nolcd = 1;
		} else if (!strncmp(this_opt, "panel_yres:", 11)) {
			panel_yres = simple_strtoul((this_opt+11), NULL, 0);
		} else if (!strncmp(this_opt, "nomtrr", 6)) {
			nomtrr = 1;
                } else
			mode_option = this_opt;
        }

	return 0;
}

#ifdef MODULE
module_init(radeonfb_init);
module_exit(radeonfb_exit);
#endif


MODULE_AUTHOR("Ani Joshi");
MODULE_DESCRIPTION("framebuffer driver for ATI Radeon chipset");
MODULE_LICENSE("GPL");

MODULE_PARM(noaccel, "i");
MODULE_PARM_DESC(noaccel, "Disable (1) or enable (0) the usage of the 2d accel engine");
MODULE_PARM(force_dfp, "i");
MODULE_PARM_DESC(force_dfp,"Force (1) the usage of a digital flat panel");
MODULE_PARM(force_crt, "i");
MODULE_PARM_DESC(force_crt,"Force (1) the usage of a CRT monitor");
MODULE_PARM(force_nolcd, "i");
MODULE_PARM_DESC(force_nolcd,"Avoid (1) the usage of a digital flat panel");

static unsigned char *radeon_find_rom(struct radeonfb_info *rinfo)
{       
#if defined(__i386__)
	/* I simplified this code as we used to miss the signatures in
	 * a lot of case. It's now closer to XFree, we just don't check
	 * for signatures at all... Something better will have to be done
	 * later obviously
	 */
        u32  segstart;
        unsigned char *rom_base;
                                                
        for(segstart=0x000c0000; segstart<0x000f0000; segstart+=0x00001000) {
                rom_base = (char *)ioremap(segstart, 0x1000);
                if ((*rom_base == 0x55) && (((*(rom_base + 1)) & 0xff) == 0xaa))
	                return rom_base;
                iounmap(rom_base);
        }
#endif          
        return NULL;
}

#ifdef CONFIG_ALL_PPC
static int radeon_read_OF (struct radeonfb_info *rinfo)
{
	struct device_node *dp;
	unsigned int *xtal;

	dp = pci_device_to_OF_node(rinfo->pdev);
	if (dp == NULL)
		return 0;

	xtal = (unsigned int *) get_property(dp, "ATY,RefCLK", 0);
	if ((xtal == NULL) || (*xtal == 0))
		return 0;

	rinfo->pll.ref_clk = *xtal / 10;

	return 1;
}
#endif	

static void radeon_get_pllinfo(struct radeonfb_info *rinfo, char *bios_seg)
{
        void *bios_header;
        void *header_ptr;
        u16 bios_header_offset, pll_info_offset;
        PLL_BLOCK pll;

	if (bios_seg) {
	        bios_header = bios_seg + 0x48L;
       		header_ptr  = bios_header;
        
        	bios_header_offset = readw(header_ptr);
	        bios_header = bios_seg + bios_header_offset;
        	bios_header += 0x30;
        
        	header_ptr = bios_header;
        	pll_info_offset = readw(header_ptr);
        	header_ptr = bios_seg + pll_info_offset;
        
        	memcpy_fromio(&pll, header_ptr, 50);
        
        	/* Consider ref clock to be sane between 1000 and 5000,
        	 * just in case we tapped the wrong BIOS...
        	 */
		if (pll.PCLK_ref_freq < 1000 || pll.PCLK_ref_freq > 5000)
			goto use_defaults;

        	rinfo->pll.xclk = (u32)pll.XCLK;
        	rinfo->pll.ref_clk = (u32)pll.PCLK_ref_freq;
        	rinfo->pll.ref_div = (u32)pll.PCLK_ref_divider;
        	rinfo->pll.ppll_min = pll.PCLK_min_freq;
        	rinfo->pll.ppll_max = pll.PCLK_max_freq;

		printk("radeonfb: ref_clk=%d, ref_div=%d, xclk=%d from BIOS\n",
			rinfo->pll.ref_clk, rinfo->pll.ref_div, rinfo->pll.xclk);
	} else {
#ifdef CONFIG_ALL_PPC
		if (radeon_read_OF(rinfo)) {
			unsigned int tmp, Nx, M, ref_div, xclk;

			tmp = INPLL(M_SPLL_REF_FB_DIV);
			ref_div = INPLL(PPLL_REF_DIV) & 0x3ff;

			Nx = (tmp & 0xff00) >> 8;
			M = (tmp & 0xff);
			xclk = ((((2 * Nx * rinfo->pll.ref_clk) + (M)) /
				(2 * M)));

			rinfo->pll.xclk = xclk;
			rinfo->pll.ref_div = ref_div;
			rinfo->pll.ppll_min = 12000;
			rinfo->pll.ppll_max = 35000;

			printk("radeonfb: ref_clk=%d, ref_div=%d, xclk=%d from OF\n",
				rinfo->pll.ref_clk, rinfo->pll.ref_div, rinfo->pll.xclk);

			return;
		}
#endif
use_defaults:
		/* No BIOS or BIOS not found, use defaults
		 * 
		 * NOTE: Those defaults settings are rather "randomly" picked from
		 * informations we found so far, but we would really need some
		 * better mecanism to get them. Recent XFree can +/- probe for
		 * the proper clocks.
		 */
		switch (rinfo->arch) {
			case RADEON_RV200:
				rinfo->pll.ppll_max = 35000;
				rinfo->pll.ppll_min = 12000;
				rinfo->pll.xclk = 23000;
				rinfo->pll.ref_div = 12;
				rinfo->pll.ref_clk = 2700;
				break;
			case RADEON_R200:
				rinfo->pll.ppll_max = 35000;
				rinfo->pll.ppll_min = 12000;
				rinfo->pll.xclk = 27500;
				rinfo->pll.ref_div = 12;
				rinfo->pll.ref_clk = 2700;
				break;
			case RADEON_RV250:
				rinfo->pll.ppll_max = 35000;
				rinfo->pll.ppll_min = 12000;
				rinfo->pll.xclk = 25000;
				rinfo->pll.ref_div = 12;
				rinfo->pll.ref_clk = 2700;
				break;
			case RADEON_R300:
				rinfo->pll.ppll_max = 40000;
				rinfo->pll.ppll_min = 20000;
				rinfo->pll.xclk = 27000;
				rinfo->pll.ref_div = 12;
				rinfo->pll.ref_clk = 2700;
				break;
			case RADEON_R100:
			default:
				rinfo->pll.ppll_max = 35000;
				rinfo->pll.ppll_min = 12000;
				rinfo->pll.xclk = 16600;
				rinfo->pll.ref_div = 67;
				rinfo->pll.ref_clk = 2700;
				break;
		}

		printk("radeonfb: ref_clk=%d, ref_div=%d, xclk=%d defaults\n",
			rinfo->pll.ref_clk, rinfo->pll.ref_div, rinfo->pll.xclk);
	}
}


static void radeon_get_moninfo (struct radeonfb_info *rinfo)
{
	u32 tmp = INREG(RADEON_BIOS_4_SCRATCH);

	if (force_dfp) {
		printk("radeonfb: forcing DFP\n");
		rinfo->dviDisp_type = MT_DFP;
		return;
	} else if (force_crt) {
		printk("radeonfb: forcing CRT\n");
		rinfo->dviDisp_type = MT_NONE;
		rinfo->crtDisp_type = MT_CRT;
		return;
	}


	if (rinfo->hasCRTC2 && tmp) {
		/* primary DVI port */
		if (tmp & 0x08)
			rinfo->dviDisp_type = MT_DFP;
		else if (tmp & 0x4)
			rinfo->dviDisp_type = MT_LCD;
		else if (tmp & 0x200)
			rinfo->dviDisp_type = MT_CRT;
		else if (tmp & 0x10)
			rinfo->dviDisp_type = MT_CTV;
		else if (tmp & 0x20)
			rinfo->dviDisp_type = MT_STV;

		/* secondary CRT port */
		if (tmp & 0x2)
			rinfo->crtDisp_type = MT_CRT;
		else if (tmp & 0x800)
			rinfo->crtDisp_type = MT_DFP;
		else if (tmp & 0x400)
			rinfo->crtDisp_type = MT_LCD;
		else if (tmp & 0x1000)
			rinfo->crtDisp_type = MT_CTV;
		else if (tmp & 0x2000)
			rinfo->crtDisp_type = MT_STV;
	} else {
		rinfo->dviDisp_type = MT_NONE;

		tmp = INREG(FP_GEN_CNTL);

		if (tmp & FP_EN_TMDS)
			rinfo->crtDisp_type = MT_DFP;
		else
			rinfo->crtDisp_type = MT_CRT;
	}
}

#ifdef CONFIG_ALL_PPC
static int radeon_get_EDID_OF(struct radeonfb_info *rinfo)
{
        struct device_node *dp;
        unsigned char *pedid = NULL;
        static char *propnames[] = { "DFP,EDID", "LCD,EDID", "EDID", "EDID1", NULL };
        int i;  

        dp = pci_device_to_OF_node(rinfo->pdev);
        while (dp != NULL) {
                for (i = 0; propnames[i] != NULL; ++i) {
                        pedid = (unsigned char *)
                                get_property(dp, propnames[i], NULL);
                        if (pedid != NULL) {
                                rinfo->EDID = pedid;
                                return 1;
                        }
                }
                dp = dp->child;
        }
        return 0;
}
#endif /* CONFIG_ALL_PPC */

static void radeon_get_EDID(struct radeonfb_info *rinfo)
{
#ifdef CONFIG_ALL_PPC
	if (!radeon_get_EDID_OF(rinfo))
		RTRACE("radeonfb: could not retrieve EDID from OF\n");
#else
	/* XXX use other methods later */
#endif
}

#ifdef CONFIG_ALL_PPC
#undef SET_MC_FB_FROM_APERTURE
static void
radeon_fixup_apertures(struct radeonfb_info *rinfo)
{
	u32 save_crtc_gen_cntl, save_crtc2_gen_cntl;
	u32 save_crtc_ext_cntl;
	u32 aper_base, aper_size;
	u32 agp_base;

	/* First, we disable display to avoid interfering */
	if (rinfo->hasCRTC2) {
		save_crtc2_gen_cntl = INREG(CRTC2_GEN_CNTL);
		OUTREG(CRTC2_GEN_CNTL, save_crtc2_gen_cntl | CRTC2_DISP_REQ_EN_B);
	}
	save_crtc_gen_cntl = INREG(CRTC_GEN_CNTL);
	save_crtc_ext_cntl = INREG(CRTC_EXT_CNTL);
	
	OUTREG(CRTC_EXT_CNTL, save_crtc_ext_cntl | CRTC_DISPLAY_DIS);
	OUTREG(CRTC_GEN_CNTL, save_crtc_gen_cntl | CRTC_DISP_REQ_EN_B);
	mdelay(100);

	aper_base = INREG(CONFIG_APER_0_BASE);
	aper_size = INREG(CONFIG_APER_SIZE);

#ifdef SET_MC_FB_FROM_APERTURE
	/* Set framebuffer to be at the same address as set in PCI BAR */
	OUTREG(MC_FB_LOCATION, 
		((aper_base + aper_size - 1) & 0xffff0000) | (aper_base >> 16));
	rinfo->fb_local_base = aper_base;
#else
	OUTREG(MC_FB_LOCATION, 0x7fff0000);
	rinfo->fb_local_base = 0;
#endif
	agp_base = aper_base + aper_size;
	if (agp_base & 0xf0000000)
		agp_base = (aper_base | 0x0fffffff) + 1;

	/* Set AGP to be just after the framebuffer on a 256Mb boundary. This
	 * assumes the FB isn't mapped to 0xf0000000 or above, but this is
	 * always the case on PPCs afaik.
	 */
#ifdef SET_MC_FB_FROM_APERTURE
	OUTREG(MC_AGP_LOCATION, 0xffff0000 | (agp_base >> 16));
#else
	OUTREG(MC_AGP_LOCATION, 0xffffe000);
#endif

	/* Fixup the display base addresses & engine offsets while we
	 * are at it as well
	 */
#ifdef SET_MC_FB_FROM_APERTURE
	OUTREG(DISPLAY_BASE_ADDR, aper_base);
	if (rinfo->hasCRTC2)
		OUTREG(CRTC2_DISPLAY_BASE_ADDR, aper_base);
#else
	OUTREG(DISPLAY_BASE_ADDR, 0);
	if (rinfo->hasCRTC2)
		OUTREG(CRTC2_DISPLAY_BASE_ADDR, 0);
#endif
	mdelay(100);

	/* Restore display settings */
	OUTREG(CRTC_GEN_CNTL, save_crtc_gen_cntl);
	OUTREG(CRTC_EXT_CNTL, save_crtc_ext_cntl);
	if (rinfo->hasCRTC2)
		OUTREG(CRTC2_GEN_CNTL, save_crtc2_gen_cntl);	

#if 0
	printk("aper_base: %08x MC_FB_LOC to: %08x, MC_AGP_LOC to: %08x\n",
		aper_base,
		((aper_base + aper_size - 1) & 0xffff0000) | (aper_base >> 16),
		0xffff0000 | (agp_base >> 16));
#endif
}
#endif /* CONFIG_ALL_PPC */

static int radeon_dfp_parse_EDID(struct radeonfb_info *rinfo)
{
	unsigned char *block = rinfo->EDID;

	if (!block)
		return 0;

	/* jump to the detailed timing block section */
	block += 54;

	rinfo->clock = (block[0] + (block[1] << 8));
	rinfo->panel_xres = (block[2] + ((block[4] & 0xf0) << 4));
	rinfo->hblank = (block[3] + ((block[4] & 0x0f) << 8));
	rinfo->panel_yres = (block[5] + ((block[7] & 0xf0) << 4));
	rinfo->vblank = (block[6] + ((block[7] & 0x0f) << 8));
	rinfo->hOver_plus = (block[8] + ((block[11] & 0xc0) << 2));
	rinfo->hSync_width = (block[9] + ((block[11] & 0x30) << 4));
	rinfo->vOver_plus = ((block[10] >> 4) + ((block[11] & 0x0c) << 2));
	rinfo->vSync_width = ((block[10] & 0x0f) + ((block[11] & 0x03) << 4));
	rinfo->interlaced = ((block[17] & 0x80) >> 7);
	rinfo->synct = ((block[17] & 0x18) >> 3);
	rinfo->misc = ((block[17] & 0x06) >> 1);
	rinfo->hAct_high = rinfo->vAct_high = 0;
	if (rinfo->synct == 3) {
		if (rinfo->misc & 2)
			rinfo->hAct_high = 1;
		if (rinfo->misc & 1)
			rinfo->vAct_high = 1;
	}

	RTRACE("hOver_plus = %d\t hSync_width = %d\n", rinfo->hOver_plus,
		rinfo->hSync_width);
	RTRACE("vOver_plus = %d\t vSync_width = %d\n", rinfo->vOver_plus,
		rinfo->vSync_width);
	RTRACE("hblank = %d\t vblank = %d\n", rinfo->hblank,
		rinfo->vblank);
	RTRACE("sync = %d\n", rinfo->synct);
	RTRACE("misc = %d\n", rinfo->misc);
	RTRACE("clock = %d\n", rinfo->clock);
	printk("radeonfb: detected DFP panel size from EDID: %dx%d\n",
		rinfo->panel_xres, rinfo->panel_yres);

	rinfo->got_dfpinfo = 1;

	return 1;
}

static void radeon_update_default_var(struct radeonfb_info *rinfo)
{
	struct fb_var_screeninfo *var = &radeonfb_default_var;

        /*
         * Update default var to match the lcd monitor's native resolution
         */
	var->xres = rinfo->panel_xres;
	var->yres = rinfo->panel_yres;
	var->xres_virtual = rinfo->panel_xres;
	var->yres_virtual = rinfo->panel_yres;
	var->xoffset = var->yoffset = 0;
	var->bits_per_pixel = 8;
	var->pixclock = 100000000 / rinfo->clock;
	var->left_margin = (rinfo->hblank - rinfo->hOver_plus - rinfo->hSync_width);
	var->right_margin = rinfo->hOver_plus;
	var->upper_margin = (rinfo->vblank - rinfo->vOver_plus - rinfo->vSync_width);
	var->lower_margin = rinfo->vOver_plus;
	var->hsync_len = rinfo->hSync_width;
	var->vsync_len = rinfo->vSync_width;
	var->sync = 0;
	if (rinfo->hAct_high)
		var->sync |= FB_SYNC_HOR_HIGH_ACT;
	if (rinfo->vAct_high)
		var->sync |= FB_SYNC_VERT_HIGH_ACT;

	var->vmode = 0;
	if (rinfo->interlaced)
		var->vmode |= FB_VMODE_INTERLACED;

	rinfo->use_default_var = 1;
}

/* Copied from XFree86 4.3 --BenH */
static int
radeon_get_dfpinfo_BIOS(struct radeonfb_info *rinfo, unsigned char *fpbiosstart)
{
	unsigned char *tmp;
	unsigned short offset;

	offset = readw(fpbiosstart + 0x34);
	if (offset != 0)
		offset = readw(rinfo->bios_seg + offset + 2);
	if (offset == 0) {
		printk("radeonfb: Failed to detect DFP panel info using BIOS\n");
		return 0;
	}
	tmp = rinfo->bios_seg + offset;

	/* This is an EDID block */
	rinfo->clock = readw(tmp);
	rinfo->panel_xres = (readb(tmp + 2) + ((readb(tmp + 4) & 0xf0) << 4));
	rinfo->hblank = (readb(tmp + 3) + ((readb(tmp + 4) & 0x0f) << 8));
	rinfo->panel_yres = (readb(tmp + 5) + ((readb(tmp + 7) & 0xf0) << 4));
	rinfo->vblank = (readb(tmp + 6) + ((readb(tmp + 7) & 0x0f) << 8));
	rinfo->hOver_plus = (readb(tmp + 8) + ((readb(tmp + 11) & 0xc0) << 2));
	rinfo->hSync_width = (readb(tmp + 9) + ((readb(tmp + 11) & 0x30) << 4));
	rinfo->vOver_plus = ((readb(tmp + 10) >> 4) + ((readb(tmp + 11) & 0x0c) << 2));
	rinfo->vSync_width = ((readb(tmp + 10) & 0x0f) + ((readb(tmp + 11) & 0x03) << 4));
	rinfo->interlaced = ((readb(tmp + 17) & 0x80) >> 7);
	rinfo->synct = ((readb(tmp + 17) & 0x18) >> 3);
	rinfo->misc = ((readb(tmp + 17) & 0x06) >> 1);
	rinfo->hAct_high = rinfo->vAct_high = 0;
	if (rinfo->synct == 3) {
		if (rinfo->misc & 2)
			rinfo->hAct_high = 1;
		if (rinfo->misc & 1)
			rinfo->vAct_high = 1;
	}

	printk("radeonfb: detected DFP panel size from BIOS: %dx%d\n",
		rinfo->panel_xres, rinfo->panel_yres);

	rinfo->got_dfpinfo = 1;
	return 1;
}


static int
radeon_get_lcdinfo_BIOS(struct radeonfb_info *rinfo, unsigned char *fpbiosstart)
{
	unsigned char *tmp, *tmp0;
	unsigned char stmp[30];
	unsigned short offset;
	int i;

	offset = readw(fpbiosstart + 0x40);
	if (offset == 0) {
		printk("radeonfb: Failed to detect LCD panel info using BIOS\n");
		return 0;
	}
	tmp = rinfo->bios_seg + offset;

	for(i=0; i<24; i++)
		stmp[i] = readb(tmp+i+1);
	stmp[24] = 0;
	printk("radeonfb: panel ID string: %s\n", stmp);
	rinfo->panel_xres = readw(tmp + 25);
	rinfo->panel_yres = readw(tmp + 27);
	printk("radeonfb: detected LCD panel size from BIOS: %dx%d\n",
		rinfo->panel_xres, rinfo->panel_yres);

	for(i=0; i<20; i++) {
		tmp0 = rinfo->bios_seg + readw(tmp+64+i*2);
		if (tmp0 == 0)
			break;
		if ((readw(tmp0) == rinfo->panel_xres) &&
		    (readw(tmp0+2) == rinfo->panel_yres)) {
			rinfo->hblank = (readw(tmp0+17) - readw(tmp0+19)) * 8;
			rinfo->hOver_plus = ((readw(tmp0+21) - readw(tmp0+19) -1) * 8) & 0x7fff;
			rinfo->hSync_width = readb(tmp0+23) * 8;
			rinfo->vblank = readw(tmp0+24) - readw(tmp0+26);
			rinfo->vOver_plus = (readw(tmp0+28) & 0x7ff) - readw(tmp0+26);
			rinfo->vSync_width = (readw(tmp0+28) & 0xf800) >> 11;
			rinfo->clock = readw(tmp0+9);
                        /* We don't know that the H/V sync active level should be
                           make the same assumptions as XFree does - High Active */
                        rinfo->vAct_high=1;
                        rinfo->hAct_high=1;
			rinfo->got_dfpinfo = 1;
			return 1;
		}
	}
	return 0;
}

static int radeon_get_panelinfo_BIOS(struct radeonfb_info *rinfo)
{
	unsigned char *fpbiosstart;

	if (!rinfo->bios_seg)
		return 0;

	if (!(fpbiosstart = rinfo->bios_seg + readw(rinfo->bios_seg + 0x48))) {
		printk("radeonfb: Failed to detect DFP panel info using BIOS\n");
		return 0;
	}

	if (rinfo->dviDisp_type == MT_LCD)
		return radeon_get_lcdinfo_BIOS(rinfo, fpbiosstart);
	else if (rinfo->dviDisp_type == MT_DFP)
		return radeon_get_dfpinfo_BIOS(rinfo, fpbiosstart);

	return 0;
}



static int radeon_get_dfpinfo (struct radeonfb_info *rinfo)
{
	unsigned int tmp;
	unsigned short a, b;

	if (radeon_get_panelinfo_BIOS(rinfo))
		radeon_update_default_var(rinfo);

	if (radeon_dfp_parse_EDID(rinfo))
		radeon_update_default_var(rinfo);

	if (!rinfo->got_dfpinfo) {
		/*
		 * it seems all else has failed now and we
		 * resort to probing registers for our DFP info
	         */
		if (panel_yres) {
			rinfo->panel_yres = panel_yres;
		} else {
			tmp = INREG(FP_VERT_STRETCH);
			tmp &= 0x00fff000;
			rinfo->panel_yres = (unsigned short)(tmp >> 0x0c) + 1;
		}

		switch (rinfo->panel_yres) {
			case 480:
				rinfo->panel_xres = 640;
				break;
			case 600:
				rinfo->panel_xres = 800;
				break;
			case 768:
				rinfo->panel_xres = 1024;
				break;
			case 1024:
				rinfo->panel_xres = 1280;
				break;
			case 1050:
				rinfo->panel_xres = 1400;
				break;
			case 1200:
				rinfo->panel_xres = 1600;
				break;
			default:
				printk("radeonfb: Failed to detect DFP panel size\n");
				return 0;
		}

		printk("radeonfb: detected DFP panel size from registers: %dx%d\n",
			rinfo->panel_xres, rinfo->panel_yres);

		tmp = INREG(FP_CRTC_H_TOTAL_DISP);
		a = (tmp & FP_CRTC_H_TOTAL_MASK) + 4;
		b = (tmp & 0x01ff0000) >> FP_CRTC_H_DISP_SHIFT;
		rinfo->hblank = (a - b + 1) * 8;

		tmp = INREG(FP_H_SYNC_STRT_WID);
		rinfo->hOver_plus = (unsigned short) ((tmp & FP_H_SYNC_STRT_CHAR_MASK) >>
					FP_H_SYNC_STRT_CHAR_SHIFT) - b - 1;
		rinfo->hOver_plus *= 8;
		rinfo->hSync_width = (unsigned short) ((tmp & FP_H_SYNC_WID_MASK) >>
					FP_H_SYNC_WID_SHIFT);
		rinfo->hSync_width *= 8;
		tmp = INREG(FP_CRTC_V_TOTAL_DISP);
		a = (tmp & FP_CRTC_V_TOTAL_MASK) + 1;
		b = (tmp & FP_CRTC_V_DISP_MASK) >> FP_CRTC_V_DISP_SHIFT;
		rinfo->vblank = a - b /* + 24 */ ;

		tmp = INREG(FP_V_SYNC_STRT_WID);
		rinfo->vOver_plus = (unsigned short) (tmp & FP_V_SYNC_STRT_MASK)
					- b + 1;
		rinfo->vSync_width = (unsigned short) ((tmp & FP_V_SYNC_WID_MASK) >>
					FP_V_SYNC_WID_SHIFT);

		/* XXX */
		/* We should calculate the pixclock as well here... --BenH.
		 */

		return 1;
	}

	return 1;
}

static int radeonfb_pci_register (struct pci_dev *pdev,
				  const struct pci_device_id *ent)
{
	struct radeonfb_info *rinfo;
	struct radeon_chip_info *rci = &radeon_chip_info[ent->driver_data];
	u32 tmp;
	int i, j;

	RTRACE("radeonfb_pci_register BEGIN\n");

	rinfo = kmalloc (sizeof (struct radeonfb_info), GFP_KERNEL);
	if (!rinfo) {
		printk ("radeonfb: could not allocate memory\n");
		return -ENODEV;
	}

	memset (rinfo, 0, sizeof (struct radeonfb_info));

	rinfo->pdev = pdev;
	strncpy(rinfo->name, rci->name, 16);
	rinfo->arch = rci->arch;

	/* enable device */
	{
		int err;

		if ((err = pci_enable_device(pdev))) {
			printk("radeonfb: cannot enable device\n");
			kfree (rinfo);
			return -ENODEV;
		}
	}

	/* set base addrs */
	rinfo->fb_base_phys = pci_resource_start (pdev, 0);
	rinfo->mmio_base_phys = pci_resource_start (pdev, 2);

	/* request the mem regions */
	if (!request_mem_region (rinfo->fb_base_phys,
				 pci_resource_len(pdev, 0), "radeonfb")) {
		printk ("radeonfb: cannot reserve FB region\n");
		kfree (rinfo);
		return -ENODEV;
	}

	if (!request_mem_region (rinfo->mmio_base_phys,
				 pci_resource_len(pdev, 2), "radeonfb")) {
		printk ("radeonfb: cannot reserve MMIO region\n");
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	/* map the regions */
	rinfo->mmio_base = (unsigned long) ioremap (rinfo->mmio_base_phys,
				    		    RADEON_REGSIZE);
	if (!rinfo->mmio_base) {
		printk ("radeonfb: cannot map MMIO\n");
		release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	rinfo->chipset = pdev->device;

	switch (rinfo->arch) {
		case RADEON_R100:
			rinfo->hasCRTC2 = 0;
			break;
		default:
			/* all the rest have it */
			rinfo->hasCRTC2 = 1;
			break;
	}

	if (mirror)
		printk("radeonfb: mirroring display to CRT\n");

	/* framebuffer size */
	tmp = INREG(CONFIG_MEMSIZE);

	/* mem size is bits [28:0], mask off the rest */
	rinfo->video_ram = tmp & CONFIG_MEMSIZE_MASK;

	/* ram type */
	tmp = INREG(MEM_SDRAM_MODE_REG);
	switch ((MEM_CFG_TYPE & tmp) >> 30) {
		case 0:
			/* SDR SGRAM (2:1) */
			strcpy(rinfo->ram_type, "SDR SGRAM");
			rinfo->ram.ml = 4;
			rinfo->ram.mb = 4;
			rinfo->ram.trcd = 1;
			rinfo->ram.trp = 2;
			rinfo->ram.twr = 1;
			rinfo->ram.cl = 2;
			rinfo->ram.loop_latency = 16;
			rinfo->ram.rloop = 16;
	
			break;
		case 1:
			/* DDR SGRAM */
			strcpy(rinfo->ram_type, "DDR SGRAM");
			rinfo->ram.ml = 4;
			rinfo->ram.mb = 4;
			rinfo->ram.trcd = 3;
			rinfo->ram.trp = 3;
			rinfo->ram.twr = 2;
			rinfo->ram.cl = 3;
			rinfo->ram.tr2w = 1;
			rinfo->ram.loop_latency = 16;
			rinfo->ram.rloop = 16;

			break;
		default:
			/* 64-bit SDR SGRAM */
			strcpy(rinfo->ram_type, "SDR SGRAM 64");
			rinfo->ram.ml = 4;
			rinfo->ram.mb = 8;
			rinfo->ram.trcd = 3;
			rinfo->ram.trp = 3;
			rinfo->ram.twr = 1;
			rinfo->ram.cl = 3;
			rinfo->ram.tr2w = 1;
			rinfo->ram.loop_latency = 17;
			rinfo->ram.rloop = 17;

			break;
	}

	rinfo->bios_seg = radeon_find_rom(rinfo);
	radeon_get_pllinfo(rinfo, rinfo->bios_seg);

	/*
	 * Hack to get around some busted production M6's
	 * reporting no ram
	 */
	if (rinfo->video_ram == 0) {
		switch (pdev->device) {
			case PCI_DEVICE_ID_ATI_RADEON_LY:
			case PCI_DEVICE_ID_ATI_RADEON_LZ:
				rinfo->video_ram = 8192 * 1024;
				break;
			default:
				break;
		}
	}


	RTRACE("radeonfb: probed %s %dk videoram\n", (rinfo->ram_type), (rinfo->video_ram/1024));

	RTRACE("BIOS 4 scratch = %x\n", INREG(RADEON_BIOS_4_SCRATCH));
	RTRACE("FP_GEN_CNTL: %x, FP2_GEN_CNTL: %x\n",
		INREG(FP_GEN_CNTL), INREG(FP2_GEN_CNTL));
	RTRACE("TMDS_TRANSMITTER_CNTL: %x, TMDS_CNTL: %x, LVDS_GEN_CNTL: %x\n",
		INREG(TMDS_TRANSMITTER_CNTL), INREG(TMDS_CNTL), INREG(LVDS_GEN_CNTL));
	RTRACE("DAC_CNTL: %x, DAC_CNTL2: %x, CRTC_GEN_CNTL: %x\n",
		INREG(DAC_CNTL), INREG(DAC_CNTL2), INREG(CRTC_GEN_CNTL));


#if !defined(__powerpc__)
	radeon_get_moninfo(rinfo);
#else
	switch (rinfo->arch) {
		case RADEON_M6:
		case RADEON_M7:
		case RADEON_M9:
			/* If forced to no-LCD, we shut down the backlight */
			if (force_nolcd) {
#ifdef CONFIG_PMAC_BACKLIGHT
				radeon_set_backlight_enable(0, BACKLIGHT_OFF, rinfo);
#endif
			} else {
				rinfo->dviDisp_type = MT_LCD;
				break;
			}
			/* Fall through */
		default:
			radeon_get_moninfo(rinfo);
			break;
	}
#endif

	radeon_get_EDID(rinfo);

	if ((rinfo->dviDisp_type == MT_DFP) || (rinfo->dviDisp_type == MT_LCD) ||
	    (rinfo->crtDisp_type == MT_DFP)) {
		if (!radeon_get_dfpinfo(rinfo)) {
			iounmap ((void*)rinfo->mmio_base);
			release_mem_region (rinfo->mmio_base_phys,
					    pci_resource_len(pdev, 2));
			release_mem_region (rinfo->fb_base_phys,
					    pci_resource_len(pdev, 0));
			kfree (rinfo);
			return -ENODEV;
		}
	}

	rinfo->fb_base = (unsigned long) ioremap (rinfo->fb_base_phys,
				  		  rinfo->video_ram);
	if (!rinfo->fb_base) {
		printk ("radeonfb: cannot map FB\n");
		iounmap ((void*)rinfo->mmio_base);
		release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	/* currcon not yet configured, will be set by first switch */
	rinfo->currcon = -1;

	/* On PPC, the firmware sets up a memory mapping that tends
	 * to cause lockups when enabling the engine. We reconfigure
	 * the card internal memory mappings properly
	 */
#ifdef CONFIG_ALL_PPC
	radeon_fixup_apertures(rinfo);
#else	
	rinfo->fb_local_base = INREG(MC_FB_LOCATION) << 16;
#endif /* CONFIG_ALL_PPC */

	/* save current mode regs before we switch into the new one
	 * so we can restore this upon __exit
	 */
	radeon_save_state (rinfo, &rinfo->init_state);

	/* init palette */
	for (i=0; i<16; i++) {
		j = color_table[i];
		rinfo->palette[i].red = default_red[j];
		rinfo->palette[i].green = default_grn[j];
		rinfo->palette[i].blue = default_blu[j];
	}

	pci_set_drvdata(pdev, rinfo);
	rinfo->next = board_list;
	board_list = rinfo;

	/* set all the vital stuff */
	radeon_set_fbinfo (rinfo);

	if (register_framebuffer ((struct fb_info *) rinfo) < 0) {
		printk ("radeonfb: could not register framebuffer\n");
		iounmap ((void*)rinfo->fb_base);
		iounmap ((void*)rinfo->mmio_base);
		release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

#ifdef CONFIG_MTRR
	rinfo->mtrr_hdl = nomtrr ? -1 : mtrr_add(rinfo->fb_base_phys,
						 rinfo->video_ram,
						 MTRR_TYPE_WRCOMB, 1);
#endif


#ifdef CONFIG_PMAC_BACKLIGHT
	if (rinfo->dviDisp_type == MT_LCD)
		register_backlight_controller(&radeon_backlight_controller,
					      rinfo, "ati");
#endif

	printk ("radeonfb: ATI Radeon %s %s %d MB\n", rinfo->name, rinfo->ram_type,
		(rinfo->video_ram/(1024*1024)));

	if (rinfo->hasCRTC2) {
		printk("radeonfb: DVI port %s monitor connected\n",
			GET_MON_NAME(rinfo->dviDisp_type));
		printk("radeonfb: CRT port %s monitor connected\n",
			GET_MON_NAME(rinfo->crtDisp_type));
	} else {
		printk("radeonfb: CRT port %s monitor connected\n",
			GET_MON_NAME(rinfo->crtDisp_type));
	}

#ifdef CONFIG_PMAC_PBOOK
	if (rinfo->arch == RADEON_M6 || 
	    rinfo->arch == RADEON_M7 || 
	    rinfo->arch == RADEON_M9) {
               /* Find PM registers in config space */
               rinfo->pm_reg = pci_find_capability(pdev, PCI_CAP_ID_PM);

               /* Enable dynamic PM of chip clocks */
               radeon_pm_enable_dynamic_mode(rinfo);

               /* Register sleep callbacks */
               pmu_register_sleep_notifier(&radeon_sleep_notifier);
               printk("radeonfb: Power Management enabled for Mobility chipsets\n");
       }
#endif

	RTRACE("radeonfb_pci_register END\n");

	return 0;
}



static void __devexit radeonfb_pci_unregister (struct pci_dev *pdev)
{
        struct radeonfb_info *rinfo = pci_get_drvdata(pdev);
 
        if (!rinfo)
                return;
 
	/* restore original state */
        radeon_write_mode (rinfo, &rinfo->init_state);
 
#ifdef CONFIG_MTRR
	if (rinfo->mtrr_hdl >= 0)
		mtrr_del(rinfo->mtrr_hdl, 0, 0);
#endif

        unregister_framebuffer ((struct fb_info *) rinfo);
                
        iounmap ((void*)rinfo->mmio_base);
        iounmap ((void*)rinfo->fb_base);
 
	release_mem_region (rinfo->mmio_base_phys,
			    pci_resource_len(pdev, 2));
	release_mem_region (rinfo->fb_base_phys,
			    pci_resource_len(pdev, 0));
        
        kfree (rinfo);
}


static int radeon_engine_init (struct radeonfb_info *rinfo)
{
	unsigned long temp;

	/* disable 3D engine */
	OUTREG(RB3D_CNTL, 0);

	radeon_engine_reset ();

	radeon_fifo_wait (1);
	if (rinfo->arch != RADEON_R300)
		OUTREG(RB2D_DSTCACHE_MODE, 0);

	radeon_fifo_wait (3);
	/* We re-read MC_FB_LOCATION from card as it can have been
	 * modified by XFree drivers (ouch !)
	 */
	rinfo->fb_local_base = INREG(MC_FB_LOCATION) << 16;

	OUTREG(DEFAULT_PITCH_OFFSET, (rinfo->pitch << 0x16) |
				     (rinfo->fb_local_base >> 10));
	OUTREG(DST_PITCH_OFFSET, (rinfo->pitch << 0x16) | (rinfo->fb_local_base >> 10));
	OUTREG(SRC_PITCH_OFFSET, (rinfo->pitch << 0x16) | (rinfo->fb_local_base >> 10));

	radeon_fifo_wait (1);
#if defined(__BIG_ENDIAN)
	OUTREGP(DP_DATATYPE, HOST_BIG_ENDIAN_EN, ~HOST_BIG_ENDIAN_EN);
#else
	OUTREGP(DP_DATATYPE, 0, ~HOST_BIG_ENDIAN_EN);
#endif
	radeon_fifo_wait (1);
	OUTREG(DEFAULT_SC_BOTTOM_RIGHT, (DEFAULT_SC_RIGHT_MAX |
					 DEFAULT_SC_BOTTOM_MAX));

	temp = radeon_get_dstbpp(rinfo->depth);
	rinfo->dp_gui_master_cntl = ((temp << 8) | GMC_CLR_CMP_CNTL_DIS);

	radeon_fifo_wait (1);
	OUTREG(DP_GUI_MASTER_CNTL, (rinfo->dp_gui_master_cntl |
				    GMC_BRUSH_SOLID_COLOR |
				    GMC_SRC_DATATYPE_COLOR));

	radeon_fifo_wait (7);

	/* clear line drawing regs */
	OUTREG(DST_LINE_START, 0);
	OUTREG(DST_LINE_END, 0);

	/* set brush color regs */
	OUTREG(DP_BRUSH_FRGD_CLR, 0xffffffff);
	OUTREG(DP_BRUSH_BKGD_CLR, 0x00000000);

	/* set source color regs */
	OUTREG(DP_SRC_FRGD_CLR, 0xffffffff);
	OUTREG(DP_SRC_BKGD_CLR, 0x00000000);

	/* default write mask */
	OUTREG(DP_WRITE_MSK, 0xffffffff);

	radeon_engine_idle ();

	return 0;
}



static int __devinit radeon_set_fbinfo (struct radeonfb_info *rinfo)
{
	struct fb_info *info;

	info = &rinfo->info;

	strcpy (info->modename, rinfo->name);
        info->node = -1;
        info->flags = FBINFO_FLAG_DEFAULT;
        info->fbops = &radeon_fb_ops;
        info->display_fg = NULL;
        strncpy (info->fontname, fontname, sizeof (info->fontname));
        info->fontname[sizeof (info->fontname) - 1] = 0;
        info->changevar = NULL;
        info->switch_con = radeonfb_switch;
        info->updatevar = radeonfb_updatevar;
        info->blank = radeonfb_blank;

        if (radeon_init_disp (rinfo) < 0)
                return -1;   

        return 0;
}



static int __devinit radeon_init_disp (struct radeonfb_info *rinfo)
{
        struct fb_info *info;
        struct display *disp;

        info = &rinfo->info;
        disp = &rinfo->disp;
        
        disp->var = radeonfb_default_var;

	/* We must initialize disp before calling fb_find_mode, as the later
	 * will cause an implicit call to radeonfb_set_var that could crash
	 * if disp is NULL
	 */
        info->disp = disp;
        rinfo->currcon_display = disp;

#ifndef MODULE
        if (mode_option)
                fb_find_mode (&disp->var, &rinfo->info, mode_option,
                              NULL, 0, NULL, 8);
        else
#endif
	if (!rinfo->use_default_var)
                fb_find_mode (&disp->var, &rinfo->info, "640x480-8@60",
                              NULL, 0, NULL, 0);

	disp->var.accel_flags |= FB_ACCELF_TEXT;

	/* Do we need that below ? ... */
	rinfo->depth = var_to_depth(&disp->var);
	rinfo->bpp = disp->var.bits_per_pixel;

	/* Apply that dawn mode ! */
	radeon_do_set_var(&disp->var, -1, 0, info);

	return 0;
}


static void radeon_set_dispsw (struct radeonfb_info *rinfo, struct display *disp)

{
        int accel;  
                
        accel = disp->var.accel_flags & FB_ACCELF_TEXT;
                
        disp->dispsw_data = NULL;
        
        disp->screen_base = (char*)rinfo->fb_base;
        disp->type = FB_TYPE_PACKED_PIXELS;
        disp->type_aux = 0;
        disp->ypanstep = 1;
        disp->ywrapstep = 0;
        disp->can_soft_blank = 1;
        disp->inverse = 0;

        switch (disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
                case 8:
                        disp->visual = FB_VISUAL_PSEUDOCOLOR;
                        disp->dispsw = accel ? &fbcon_radeon8 : &fbcon_cfb8;
                        if (accel)
                        	disp->line_length = (disp->var.xres_virtual + 0x3f) & ~0x3f;
                        else
                        	disp->line_length = disp->var.xres_virtual;
                        break;
#endif /* FBCON_HAS_CFB8 */

#ifdef FBCON_HAS_CFB16
                case 16:
                        disp->dispsw = accel ? &fbcon_radeon16 : &fbcon_cfb16;
                        disp->dispsw_data = &rinfo->con_cmap.cfb16;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        if (accel)
				disp->line_length = (disp->var.xres_virtual * 2 + 0x3f) & ~0x3f;
			else
				disp->line_length = disp->var.xres_virtual * 2;
                        break;
#endif  /* FBCON_HAS_CFB16 */

#ifdef FBCON_HAS_CFB24       
                case 24:
                        disp->dispsw = &fbcon_cfb24;
                        disp->dispsw_data = &rinfo->con_cmap.cfb24;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        if (accel)
				disp->line_length = (disp->var.xres_virtual * 3 + 0x3f) & ~0x3f;
			else
				disp->line_length = disp->var.xres_virtual * 3;
                        break;
#endif /* FBCON_HAS_CFB24 */

#ifdef FBCON_HAS_CFB32
                case 32:
                        disp->dispsw = accel ? &fbcon_radeon32 : &fbcon_cfb32;
                        disp->dispsw_data = &rinfo->con_cmap.cfb32;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        if (accel)
				disp->line_length = (disp->var.xres_virtual * 4 + 0x3f) & ~0x3f;
			else
				disp->line_length = disp->var.xres_virtual * 4;
                        break;   
#endif /* FBCON_HAS_CFB32 */

                default:
                        printk ("radeonfb: setting fbcon_dummy renderer\n");
                        disp->dispsw = &fbcon_dummy;
        }
                
        return;
}                        


static void do_install_cmap(int con, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
                
        if (con != rinfo->currcon)
                return;
                
        if (fb_display[con].cmap.len)
                fb_set_cmap(&fb_display[con].cmap, 1, radeon_setcolreg, info);
        else {
		int size = radeon_get_cmap_len(&fb_display[con].var);
                fb_set_cmap(fb_default_cmap(size), 1, radeon_setcolreg, info);
        }
}



static int radeonfb_do_maximize(struct radeonfb_info *rinfo,
                                struct fb_var_screeninfo *var,
                                struct fb_var_screeninfo *v,
                                int nom, int den)
{
        static struct {
                int xres, yres;
        } modes[] = {
                {1600, 1280},
                {1280, 1024},
                {1024, 768},
                {800, 600},
                {640, 480},
                {-1, -1}
        };
        int i;
                
        /* use highest possible virtual resolution */
        if (v->xres_virtual == -1 && v->yres_virtual == -1) {
                printk("radeonfb: using max available virtual resolution\n");
                for (i=0; modes[i].xres != -1; i++) {
                        if (modes[i].xres * nom / den * modes[i].yres <
                            rinfo->video_ram / 2)
                                break;
                }
                if (modes[i].xres == -1) {
                        printk("radeonfb: could not find virtual resolution that fits into video memory!\n");
                        return -EINVAL;
                }
                v->xres_virtual = modes[i].xres;  
                v->yres_virtual = modes[i].yres;
                
                printk("radeonfb: virtual resolution set to max of %dx%d\n",
                        v->xres_virtual, v->yres_virtual);
        } else if (v->xres_virtual == -1) {
                v->xres_virtual = (rinfo->video_ram * den /   
                                (nom * v->yres_virtual * 2)) & ~15;
        } else if (v->yres_virtual == -1) {
                v->xres_virtual = (v->xres_virtual + 15) & ~15;
                v->yres_virtual = rinfo->video_ram * den /
                        (nom * v->xres_virtual *2);
        } else {
                if (v->xres_virtual * nom / den * v->yres_virtual >
                        rinfo->video_ram) {
                        return -EINVAL;
                }
        }
                
        if (v->xres_virtual * nom / den >= 8192) {
                v->xres_virtual = 8192 * den / nom - 16;
        }       
        
        if (v->xres_virtual < v->xres)
                return -EINVAL;
                
        if (v->yres_virtual < v->yres)
                return -EINVAL;
                                
        return 0;
}
                        


/*
 * fb ops
 */

static int radeonfb_get_fix (struct fb_fix_screeninfo *fix, int con,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;  
        
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];

        memset (fix, 0, sizeof (struct fb_fix_screeninfo));
	sprintf (fix->id, "ATI Radeon %s", rinfo->name);
        
        fix->smem_start = rinfo->fb_base_phys;
        fix->smem_len = rinfo->video_ram;

        fix->type = disp->type;
        fix->type_aux = disp->type_aux;
        fix->visual = disp->visual;

        fix->xpanstep = 8;
        fix->ypanstep = 1;
        fix->ywrapstep = 0;
        
        fix->line_length = disp->line_length;
 
        fix->mmio_start = rinfo->mmio_base_phys;
        fix->mmio_len = RADEON_REGSIZE;
	if (noaccel)
	        fix->accel = FB_ACCEL_NONE;
	else
		fix->accel = FB_ACCEL_ATI_RADEON;
        
        return 0;
}



static int radeonfb_get_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        
        *var = (con < 0) ? rinfo->disp.var : fb_display[con].var;
        
        return 0;
}


static int radeon_do_set_var (struct fb_var_screeninfo *var, int con,
                             int real, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
        struct fb_var_screeninfo v;
        int nom, den, accel;
        unsigned chgvar = 1;

        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];

        accel = (noaccel == 0) && ((var->accel_flags & FB_ACCELF_TEXT) != 0);

        if (con >= 0) {
                chgvar = ((disp->var.xres != var->xres) ||
                          (disp->var.yres != var->yres) ||
                          (disp->var.xres_virtual != var->xres_virtual) ||
                          (disp->var.yres_virtual != var->yres_virtual) ||
                          (disp->var.bits_per_pixel != var->bits_per_pixel) ||
                          memcmp (&disp->var.red, &var->red, sizeof (var->red)) ||
                          memcmp (&disp->var.green, &var->green, sizeof (var->green)) ||
                          memcmp (&disp->var.blue, &var->blue, sizeof (var->blue)) ||
                          var->accel_flags != disp->var.accel_flags);
        }

try_again:

        memcpy (&v, var, sizeof (v));

        switch (v.bits_per_pixel) {
		case 0 ... 8:
			v.bits_per_pixel = 8;
			break;
		case 9 ... 16:
			v.bits_per_pixel = 16;
			break;
		case 17 ... 24:
			v.bits_per_pixel = 24;
			break;
		case 25 ... 32:
			v.bits_per_pixel = 32;
			break;
		default:
			return -EINVAL;
	}

	switch (var_to_depth(&v)) {
#ifdef FBCON_HAS_CFB8
                case 8:
                        nom = den = 1;
                        if (accel)
                        	disp->line_length = (v.xres_virtual + 0x3f) & ~0x3f;
                        else
                        	disp->line_length = v.xres_virtual;
                        disp->visual = FB_VISUAL_PSEUDOCOLOR; 
                        v.red.offset = v.green.offset = v.blue.offset = 0;
                        v.red.length = v.green.length = v.blue.length = 8;
                        v.transp.offset = v.transp.length = 0;
                        break;
#endif
                        
#ifdef FBCON_HAS_CFB16
		case 15:
			nom = 2;
			den = 1;
			if (accel)
                        	disp->line_length = (v.xres_virtual * 2 + 0x3f) & ~0x3f;
			else
                        	disp->line_length = v.xres_virtual * 2;
			disp->visual = FB_VISUAL_DIRECTCOLOR;
			v.red.offset = 10;
			v.green.offset = 5;
			v.red.offset = 0;
			v.red.length = v.green.length = v.blue.length = 5;
			v.transp.offset = v.transp.length = 0;
			break;
                case 16:
                        nom = 2;
                        den = 1;
			if (accel)
                        	disp->line_length = (v.xres_virtual * 2 + 0x3f) & ~0x3f;
			else
                        	disp->line_length = v.xres_virtual * 2;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        v.red.offset = 11;
                        v.green.offset = 5;
                        v.blue.offset = 0;
                        v.red.length = 5;
                        v.green.length = 6;
                        v.blue.length = 5;
                        v.transp.offset = v.transp.length = 0;
                        break;  
#endif
                        
#ifdef FBCON_HAS_CFB24
                case 24:
                        nom = 4;
                        den = 1;
			if (accel)
                        	disp->line_length = (v.xres_virtual * 3 + 0x3f) & ~0x3f;
			else
                        	disp->line_length = v.xres_virtual * 3;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        v.red.offset = 16;
                        v.green.offset = 8;
                        v.blue.offset = 0;
                        v.red.length = v.blue.length = v.green.length = 8;
                        v.transp.offset = v.transp.length = 0;
                        break;
#endif
#ifdef FBCON_HAS_CFB32
                case 32:
                        nom = 4;
                        den = 1;
			if (accel)
                        	disp->line_length = (v.xres_virtual * 4 + 0x3f) & ~0x3f;
			else
                        	disp->line_length = v.xres_virtual * 4;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        v.red.offset = 16;
                        v.green.offset = 8;
                        v.blue.offset = 0;
                        v.red.length = v.blue.length = v.green.length = 8;
                        v.transp.offset = 24;
                        v.transp.length = 8;
                        break;
#endif
                default:
                        printk ("radeonfb: mode %dx%dx%d rejected, color depth invalid\n",
                                var->xres, var->yres, var->bits_per_pixel);
                        return -EINVAL;
        }

        if (radeonfb_do_maximize(rinfo, var, &v, nom, den) < 0)
                return -EINVAL;  
                
        if (v.xoffset < 0)
                v.xoffset = 0;
        if (v.yoffset < 0)
                v.yoffset = 0;
         
        if (v.xoffset > v.xres_virtual - v.xres)
                v.xoffset = v.xres_virtual - v.xres - 1;
                        
        if (v.yoffset > v.yres_virtual - v.yres)
                v.yoffset = v.yres_virtual - v.yres - 1;
         
        v.red.msb_right = v.green.msb_right = v.blue.msb_right =
                          v.transp.offset = v.transp.length =
                          v.transp.msb_right = 0;
                        
        switch (v.activate & FB_ACTIVATE_MASK) {
                case FB_ACTIVATE_TEST:
                        return 0;
                case FB_ACTIVATE_NXTOPEN:
                case FB_ACTIVATE_NOW:
                        break;
                default:
                        return -EINVAL;
        }

        /* Set real accel */
        if (accel)
	        v.accel_flags |= FB_ACCELF_TEXT;
        else
	        v.accel_flags &= ~FB_ACCELF_TEXT;
        
        memcpy (&disp->var, &v, sizeof (v));

        if (real)
        	radeon_load_video_mode (rinfo, &v);
	if (accel && real) {
		if (radeon_engine_init(rinfo) < 0) {
                        var->accel_flags &= ~FB_ACCELF_TEXT;
			goto try_again;
		}
	}

        if (chgvar) {     
	        radeon_set_dispsw(rinfo, disp);

                if (!accel)
                        disp->scrollmode = SCROLL_YREDRAW;
                else
                        disp->scrollmode = 0;
                
                if (info && info->changevar && con >= 0)
                        info->changevar(con);
        }

        if (real)        
        	do_install_cmap(con, info);
  
        return 0;
}

static int radeonfb_set_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info)
{
	return radeon_do_set_var(var, con, 1, info);
}


static int radeonfb_get_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
                
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];
        
        if (con == rinfo->currcon) {
                int rc = fb_get_cmap (cmap, kspc, radeon_getcolreg, info);
                return rc;
        } else if (disp->cmap.len)
                fb_copy_cmap (&disp->cmap, cmap, kspc ? 0 : 2);
        else
                fb_copy_cmap (fb_default_cmap (radeon_get_cmap_len (&disp->var)),
                              cmap, kspc ? 0 : 2);
                        
        return 0;
}



static int radeonfb_set_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
        unsigned int cmap_len;
                
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];
  
        cmap_len = radeon_get_cmap_len (&disp->var);
        if (disp->cmap.len != cmap_len) {
                int err = fb_alloc_cmap (&disp->cmap, cmap_len, 0);
                if (err)
                        return err;
        }
 
        if (con == rinfo->currcon) {
                int rc = fb_set_cmap (cmap, kspc, radeon_setcolreg, info);
                return rc;
        } else
                fb_copy_cmap (cmap, &disp->cmap, kspc ? 0 : 1);
        
        return 0;
}               



static int radeonfb_pan_display (struct fb_var_screeninfo *var, int con,
                                 struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
                        
        if (((var->xoffset + var->xres) > var->xres_virtual)
            || ((var->yoffset + var->yres) > var->yres_virtual))
               return -EINVAL;
                        
        if (rinfo->asleep)
                return 0;

        OUTREG(CRTC_OFFSET, ((var->yoffset * var->xres_virtual + var->xoffset)
                             * var->bits_per_pixel / 8) & ~7);
                
        return 0;
}


static int radeonfb_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                           unsigned long arg, int con, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
	unsigned int tmp;
	u32 value = 0;
	int rc;

	switch (cmd) {
		/*
		 * TODO:  set mirror accordingly for non-Mobility chipsets with 2 CRTC's
		 */
		case FBIO_RADEON_SET_MIRROR:
			switch (rinfo->arch) {
				case RADEON_M6:
				case RADEON_M7:
				case RADEON_M9:
					break;
				default:
					return -EINVAL;
			}

			rc = get_user(value, (__u32*)arg);

			if (rc)
				return rc;

			if (value & 0x01) {
				tmp = INREG(LVDS_GEN_CNTL);

				tmp |= (LVDS_ON | LVDS_BLON);
			} else {
				tmp = INREG(LVDS_GEN_CNTL);

				tmp &= ~(LVDS_ON | LVDS_BLON);
			}

			OUTREG(LVDS_GEN_CNTL, tmp);

			if (value & 0x02) {
				tmp = INREG(CRTC_EXT_CNTL);
				tmp |= CRTC_CRT_ON;

				mirror = 1;
			} else {
				tmp = INREG(CRTC_EXT_CNTL);
				tmp &= ~CRTC_CRT_ON;

				mirror = 0;
			}

			OUTREG(CRTC_EXT_CNTL, tmp);

			return 0;

		case FBIO_RADEON_GET_MIRROR:
			switch (rinfo->arch) {
				case RADEON_M6:
				case RADEON_M7:
				case RADEON_M9:
					break;
				default:
					return -EINVAL;
			}

			tmp = INREG(LVDS_GEN_CNTL);
			if ((LVDS_ON | LVDS_BLON) & tmp)
				value |= 0x01;

			tmp = INREG(CRTC_EXT_CNTL);
			if (CRTC_CRT_ON & tmp)
				value |= 0x02;

			return put_user(value, (__u32*)arg);
	}

	return -EINVAL;
}


static int radeonfb_switch (int con, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp, *old_disp;
        struct fb_cmap *cmap;
        int switchmode = 0;
        
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];
        old_disp = rinfo->currcon_display;
                
        if (rinfo->currcon >= 0) {
                cmap = &(rinfo->currcon_display->cmap);
                if (cmap->len)
                        fb_get_cmap (cmap, 1, radeon_getcolreg, info);
        }   

        if ((old_disp == NULL) || ((disp->var.xres != old_disp->var.xres) ||
            (disp->var.yres != old_disp->var.yres) ||
            (disp->var.xres_virtual != old_disp->var.xres_virtual) ||
            (disp->var.yres_virtual != old_disp->var.yres_virtual) ||
            (disp->var.bits_per_pixel != old_disp->var.bits_per_pixel) ||
            memcmp (&disp->var.red, &old_disp->var.red, sizeof (old_disp->var.red)) ||
            memcmp (&disp->var.green, &old_disp->var.green, sizeof (old_disp->var.green)) ||
            memcmp (&disp->var.blue, &old_disp->var.blue, sizeof (old_disp->var.blue)) ||
            old_disp->var.accel_flags != disp->var.accel_flags))
        	switchmode = 1;                  

	if (rinfo->currcon == -1)
		switchmode = 1;

try_again:
	rinfo->currcon = con;
	rinfo->currcon_display = disp;
	disp->var.activate = FB_ACTIVATE_NOW;
        
        if (switchmode) {
                radeonfb_set_var (&disp->var, con, info);
                do_install_cmap(con, info);
        } else {
		if (radeon_engine_init(rinfo) < 0) {
                        disp->var.accel_flags &= ~FB_ACCELF_TEXT;
                        switchmode = 1;
			goto try_again;
		}
        }

        radeon_set_dispsw (rinfo, disp);

        return 0;
}


static int radeonfb_updatevar (int con, struct fb_info *info)
{
        int rc;
                
        rc = (con < 0) ? -EINVAL : radeonfb_pan_display (&fb_display[con].var,
                                                         con, info);
        
        return rc;
}

static void radeonfb_blank (int blank, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        u32 val = INREG(CRTC_EXT_CNTL);
	u32 val_lvds = INREG(LVDS_GEN_CNTL);
	u32 val_dfp = INREG(FP_GEN_CNTL);

	if (rinfo->asleep)
		return;
		
#ifdef CONFIG_PMAC_BACKLIGHT
	if (rinfo->dviDisp_type == MT_LCD && _machine == _MACH_Pmac) {
		set_backlight_enable(!blank);
		return;
	}
#endif
                        
        /* reset it */
        val &= ~(CRTC_DISPLAY_DIS | CRTC_HSYNC_DIS |
                 CRTC_VSYNC_DIS);
	val_lvds &= ~(LVDS_DISPLAY_DIS);
	val_dfp |= FP_FPON | FP_TMDS_EN;

        switch (blank) {
                case VESA_NO_BLANKING:
                        break;
                case VESA_VSYNC_SUSPEND:
                        val |= (CRTC_DISPLAY_DIS | CRTC_VSYNC_DIS);
                        break;
                case VESA_HSYNC_SUSPEND:
                        val |= (CRTC_DISPLAY_DIS | CRTC_HSYNC_DIS);
                        break;
                case VESA_POWERDOWN:
                        val |= (CRTC_DISPLAY_DIS | CRTC_VSYNC_DIS | 
                                CRTC_HSYNC_DIS);
			val_lvds |= (LVDS_DISPLAY_DIS);
			val_dfp &= ~(FP_FPON | FP_TMDS_EN);
                        break;
        }

	switch (rinfo->dviDisp_type) {
		case MT_LCD:
			OUTREG(LVDS_GEN_CNTL, val_lvds);
			break;
		case MT_DFP:
			OUTREG(FP_GEN_CNTL, val_dfp);
			break;
		case MT_CRT:
		default:
		        OUTREG(CRTC_EXT_CNTL, val);
			break;
	}
}


static int radeon_get_cmap_len (const struct fb_var_screeninfo *var)
{
        int rc = 256;            /* reasonable default */
        
        switch (var_to_depth(var)) {
                case 15:
                        rc = 32;
                        break;
		case 16:
			rc = 64;
			break;
        }
                
        return rc;
}


static int radeon_getcolreg (unsigned regno, unsigned *red, unsigned *green,
                             unsigned *blue, unsigned *transp,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
	
	if (regno > 255)
		return 1;
     
 	*red = (rinfo->palette[regno].red<<8) | rinfo->palette[regno].red; 
    	*green = (rinfo->palette[regno].green<<8) | rinfo->palette[regno].green;
    	*blue = (rinfo->palette[regno].blue<<8) | rinfo->palette[regno].blue;
    	*transp = 0;

	return 0;
}                            



static int radeon_setcolreg (unsigned regno, unsigned red, unsigned green,
                             unsigned blue, unsigned transp, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
	u32 pindex;

	if (regno > 255)
		return 1;

	red >>= 8;
	green >>= 8;
	blue >>= 8;
	rinfo->palette[regno].red = red;
	rinfo->palette[regno].green = green;
	rinfo->palette[regno].blue = blue;

        /* default */
        pindex = regno;

        if (!rinfo->asleep) {
        	u32 dac_cntl2, vclk_cntl;
        	
		vclk_cntl = INPLL(VCLK_ECP_CNTL);
		OUTPLL(VCLK_ECP_CNTL, vclk_cntl & ~PIXCLK_DAC_ALWAYS_ONb);

		/* Make sure we are on first palette */
		if (rinfo->hasCRTC2) {
			dac_cntl2 = INREG(DAC_CNTL2);
			dac_cntl2 &= ~DAC2_PALETTE_ACCESS_CNTL;
			OUTREG(DAC_CNTL2, dac_cntl2);
		}

		if (rinfo->bpp == 16) {
			pindex = regno * 8;

			if (rinfo->depth == 16 && regno > 63)
				return 1;
			if (rinfo->depth == 15 && regno > 31)
				return 1;

			/* For 565, the green component is mixed one order below */
			if (rinfo->depth == 16) {
		                OUTREG8(PALETTE_INDEX, pindex>>1);
	       	         	OUTREG(PALETTE_DATA, (rinfo->palette[regno>>1].red << 16) |
	                        	(green << 8) | (rinfo->palette[regno>>1].blue));
	                	green = rinfo->palette[regno<<1].green;
	        	}
		}

		if (rinfo->depth != 16 || regno < 32) {
			OUTREG8(PALETTE_INDEX, pindex);
			OUTREG(PALETTE_DATA, (red << 16) | (green << 8) | blue);
		}

		OUTPLL(VCLK_ECP_CNTL, vclk_cntl);
	}
 	if (regno < 16) {
        	switch (rinfo->depth) {
#ifdef FBCON_HAS_CFB16
		        case 15:
        			rinfo->con_cmap.cfb16[regno] = (regno << 10) | (regno << 5) |
				                       	 	  regno;   
			        break;
		        case 16:
        			rinfo->con_cmap.cfb16[regno] = (regno << 11) | (regno << 5) |
				                       	 	  regno;   
			        break;
#endif
#ifdef FBCON_HAS_CFB24   
                        case 24:
                                rinfo->con_cmap.cfb24[regno] = (regno << 16) | (regno << 8) | regno;
                                break;
#endif
#ifdef FBCON_HAS_CFB32
	        	case 32: {
            			u32 i;    
   
  		       		i = (regno << 8) | regno;
            			rinfo->con_cmap.cfb32[regno] = (i << 16) | i;
		        	break;
        		}
#endif
                }
        }
        return 0;

}


static void radeon_save_state (struct radeonfb_info *rinfo,
                               struct radeon_regs *save)
{
	/* CRTC regs */
	save->crtc_gen_cntl = INREG(CRTC_GEN_CNTL);
	save->crtc_ext_cntl = INREG(CRTC_EXT_CNTL);
	save->crtc_more_cntl = INREG(CRTC_MORE_CNTL);
	save->dac_cntl = INREG(DAC_CNTL);
        save->crtc_h_total_disp = INREG(CRTC_H_TOTAL_DISP);
        save->crtc_h_sync_strt_wid = INREG(CRTC_H_SYNC_STRT_WID);
        save->crtc_v_total_disp = INREG(CRTC_V_TOTAL_DISP);
        save->crtc_v_sync_strt_wid = INREG(CRTC_V_SYNC_STRT_WID);
	save->crtc_pitch = INREG(CRTC_PITCH);
#if defined(__BIG_ENDIAN)
	save->surface_cntl = INREG(SURFACE_CNTL);
#endif

	/* FP regs */
	save->fp_crtc_h_total_disp = INREG(FP_CRTC_H_TOTAL_DISP);
	save->fp_crtc_v_total_disp = INREG(FP_CRTC_V_TOTAL_DISP);
	save->fp_gen_cntl = INREG(FP_GEN_CNTL);
	save->fp_h_sync_strt_wid = INREG(FP_H_SYNC_STRT_WID);
	save->fp_horz_stretch = INREG(FP_HORZ_STRETCH);
	save->fp_v_sync_strt_wid = INREG(FP_V_SYNC_STRT_WID);
	save->fp_vert_stretch = INREG(FP_VERT_STRETCH);
	save->lvds_gen_cntl = INREG(LVDS_GEN_CNTL);
	save->lvds_pll_cntl = INREG(LVDS_PLL_CNTL);
	save->tmds_crc = INREG(TMDS_CRC);
	save->tmds_transmitter_cntl = INREG(TMDS_TRANSMITTER_CNTL);
	save->vclk_ecp_cntl = INPLL(VCLK_ECP_CNTL);
}


static void radeon_calc_pll_regs(struct radeonfb_info *rinfo, struct radeon_regs *regs, unsigned long freq)
{
	const struct {
		int divider;
		int bitvalue;
	} *post_div,
	  post_divs[] = {
		{ 1,  0 },
		{ 2,  1 },
		{ 4,  2 },
		{ 8,  3 },
		{ 3,  4 },
		{ 16, 5 },
		{ 6,  6 },
		{ 12, 7 },
		{ 0,  0 },
	};

	if (freq > rinfo->pll.ppll_max)
		freq = rinfo->pll.ppll_max;
	if (freq*12 < rinfo->pll.ppll_min)
		freq = rinfo->pll.ppll_min / 12;


	for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
		rinfo->pll_output_freq = post_div->divider * freq;
		if (rinfo->pll_output_freq >= rinfo->pll.ppll_min  &&
		    rinfo->pll_output_freq <= rinfo->pll.ppll_max)
			break;
	}

	/* Why do we have those in rinfo at this point ? --BenH */
	rinfo->post_div = post_div->divider;
	rinfo->fb_div = round_div(rinfo->pll.ref_div*rinfo->pll_output_freq,
				  rinfo->pll.ref_clk);
	regs->ppll_ref_div = rinfo->pll.ref_div;
	regs->ppll_div_3 = rinfo->fb_div | (post_div->bitvalue << 16);

#ifdef CONFIG_ALL_PPC
	/* Gross hack for iBook with M7 until I find out a proper fix */
	if (machine_is_compatible("PowerBook4,3") && rinfo->arch == RADEON_M7)
		regs->ppll_div_3 = 0x000600ad;
#endif /* CONFIG_ALL_PPC */

	RTRACE("post div = 0x%x\n", rinfo->post_div);
	RTRACE("fb_div = 0x%x\n", rinfo->fb_div);
	RTRACE("ppll_div_3 = 0x%x\n", regs->ppll_div_3);
}

static void radeon_load_video_mode (struct radeonfb_info *rinfo,
                                    struct fb_var_screeninfo *mode)
{
	struct radeon_regs newmode;
	int hTotal, vTotal, hSyncStart, hSyncEnd,
	    hSyncPol, vSyncStart, vSyncEnd, vSyncPol, cSync;
	u8 hsync_adj_tab[] = {0, 0x12, 9, 9, 6, 5};
	u8 hsync_fudge_fp[] = {2, 2, 0, 0, 5, 5};
	u32 sync, h_sync_pol, v_sync_pol, dotClock, pixClock;
	unsigned int freq;
#if 0
        unsigned int xclk_freq, vclk_freq;
        int xclk_per_trans, xclk_per_trans_precise;
        int useable_precision, roff, ron;
        int min_bits;
#endif
	int format = 0;
	int hsync_start, hsync_fudge, bytpp, hsync_wid, vsync_wid;
	int primary_mon = PRIMARY_MONITOR(rinfo);
	int depth = var_to_depth(mode);
        int accel = mode->accel_flags & FB_ACCELF_TEXT;

	rinfo->xres = mode->xres;
	rinfo->yres = mode->yres;
	rinfo->xres_virtual = mode->xres_virtual;
	rinfo->yres_virtual = mode->yres_virtual;
	rinfo->pixclock = mode->pixclock;

	hSyncStart = mode->xres + mode->right_margin;
	hSyncEnd = hSyncStart + mode->hsync_len;
	hTotal = hSyncEnd + mode->left_margin;

	vSyncStart = mode->yres + mode->lower_margin;
	vSyncEnd = vSyncStart + mode->vsync_len;
	vTotal = vSyncEnd + mode->upper_margin;
	pixClock = mode->pixclock;
	
	if ((primary_mon == MT_DFP) || (primary_mon == MT_LCD)) {
                /* Force the native video mode of the LCD monitor.
                 * This is complicated, because when the hardware
                 * stretcher is used, the extra pixels are not counted
                 * in the horizontal timing parameters. So:
                 *   - For the visible part of the display, we use the
                 *     requested paramters.
                 *   - For the invisible part of the display, we use the
                 *     parameters of the native video mode.
                 */
		if (rinfo->panel_xres < mode->xres)
			rinfo->xres = mode->xres = rinfo->panel_xres;
		if (rinfo->panel_yres < mode->yres)
			rinfo->yres = mode->yres = rinfo->panel_yres;

		hTotal = mode->xres + rinfo->hblank;
		hSyncStart = mode->xres + rinfo->hOver_plus;
		hSyncEnd = hSyncStart + rinfo->hSync_width;

		vTotal = mode->yres + rinfo->vblank;
		vSyncStart = mode->yres + rinfo->vOver_plus;
		vSyncEnd = vSyncStart + rinfo->vSync_width;

		/* If we know the LCD clock, we shall use it, unfortunately,
		 * we may not know it until we have proper EDID probing
		 */
		if (rinfo->clock)
			pixClock = 100000000 / rinfo->clock;
	}
	dotClock = 1000000000 / pixClock;
	freq = dotClock / 10; /* x100 */

	sync = mode->sync;
	h_sync_pol = sync & FB_SYNC_HOR_HIGH_ACT ? 0 : 1;
	v_sync_pol = sync & FB_SYNC_VERT_HIGH_ACT ? 0 : 1;

	RTRACE("hStart = %d, hEnd = %d, hTotal = %d\n",
		hSyncStart, hSyncEnd, hTotal);
	RTRACE("vStart = %d, vEnd = %d, vTotal = %d\n",
		vSyncStart, vSyncEnd, vTotal);

	hsync_wid = (hSyncEnd - hSyncStart) / 8;
	vsync_wid = vSyncEnd - vSyncStart;
	if (hsync_wid == 0)
		hsync_wid = 1;
	else if (hsync_wid > 0x3f)	/* max */
		hsync_wid = 0x3f;

	if (vsync_wid == 0)
		vsync_wid = 1;
	else if (vsync_wid > 0x1f)	/* max */
		vsync_wid = 0x1f;

	hSyncPol = mode->sync & FB_SYNC_HOR_HIGH_ACT ? 0 : 1;
	vSyncPol = mode->sync & FB_SYNC_VERT_HIGH_ACT ? 0 : 1;

	cSync = mode->sync & FB_SYNC_COMP_HIGH_ACT ? (1 << 4) : 0;

	format = radeon_get_dstbpp(depth);
	bytpp = mode->bits_per_pixel >> 3;

	if ((primary_mon == MT_DFP) || (primary_mon == MT_LCD))
		hsync_fudge = hsync_fudge_fp[format-1];
	else
		hsync_fudge = hsync_adj_tab[format-1];

	hsync_start = hSyncStart - 8 + hsync_fudge;

	newmode.crtc_gen_cntl = CRTC_EXT_DISP_EN | CRTC_EN |
				(format << 8);

	/* Clear auto-center etc... Maybe we can actually use these
	 * later, when I implement a scaling mode that keep
	 * aspect ratio
	 */
	newmode.crtc_more_cntl = rinfo->init_state.crtc_more_cntl;
	newmode.crtc_more_cntl &= 0xfffffff0;
	
	if ((primary_mon == MT_DFP) || (primary_mon == MT_LCD)) {
		newmode.crtc_ext_cntl = VGA_ATI_LINEAR | XCRT_CNT_EN;
		if (mirror)
			newmode.crtc_ext_cntl |= CRTC_CRT_ON;

		newmode.crtc_gen_cntl &= ~(CRTC_DBL_SCAN_EN |
					   CRTC_INTERLACE_EN);
	} else {
		newmode.crtc_ext_cntl = VGA_ATI_LINEAR | XCRT_CNT_EN |
					CRTC_CRT_ON;
	}

	newmode.dac_cntl = /* INREG(DAC_CNTL) | */ DAC_MASK_ALL | DAC_VGA_ADR_EN |
			   DAC_8BIT_EN;

	newmode.crtc_h_total_disp = ((((hTotal / 8) - 1) & 0x3ff) |
				     (((mode->xres / 8) - 1) << 16));

	newmode.crtc_h_sync_strt_wid = ((hsync_start & 0x1fff) |
					(hsync_wid << 16) | (h_sync_pol << 23));

	newmode.crtc_v_total_disp = ((vTotal - 1) & 0xffff) |
				    ((mode->yres - 1) << 16);

	newmode.crtc_v_sync_strt_wid = (((vSyncStart - 1) & 0xfff) |
					 (vsync_wid << 16) | (v_sync_pol  << 23));

	/* We first calculate the engine pitch (we always calculate it as this value may
	 * be used elsewhere, like when setting us the btext engine
	 */
	rinfo->pitch = ((mode->xres_virtual * ((mode->bits_per_pixel + 1) / 8) + 0x3f)
 				& ~(0x3f)) >> 6;
	if (accel)
		/* Then, re-multiply it to get the CRTC pitch */
		newmode.crtc_pitch = (rinfo->pitch << 3) / ((mode->bits_per_pixel + 1) / 8);
	else
		newmode.crtc_pitch = (mode->xres_virtual >> 3);

	newmode.crtc_pitch |= (newmode.crtc_pitch << 16);

	/*
	 * It looks like recent chips have a problem with SURFACE_CNTL,
	 * setting SURF_TRANSLATION_DIS completely disables the
	 * swapper as well, so we leave it unset now.
	 */
	newmode.surface_cntl = 0;

#if defined(__BIG_ENDIAN)
	/* Setup swapping on both apertures, though we currently
	 * only use aperture 0, enabling swapper on aperture 1
	 * won't harm
	 */
	switch (mode->bits_per_pixel) {
		case 16:
			newmode.surface_cntl |= NONSURF_AP0_SWP_16BPP;
			newmode.surface_cntl |= NONSURF_AP1_SWP_16BPP;
			break;
		case 24:	
		case 32:
			newmode.surface_cntl |= NONSURF_AP0_SWP_32BPP;
			newmode.surface_cntl |= NONSURF_AP1_SWP_32BPP;
			break;
	}
#endif

	RTRACE("h_total_disp = 0x%x\t   hsync_strt_wid = 0x%x\n",
		newmode.crtc_h_total_disp, newmode.crtc_h_sync_strt_wid);
	RTRACE("v_total_disp = 0x%x\t   vsync_strt_wid = 0x%x\n",
		newmode.crtc_v_total_disp, newmode.crtc_v_sync_strt_wid);

	newmode.xres = mode->xres;
	newmode.yres = mode->yres;

	rinfo->bpp = mode->bits_per_pixel;
	rinfo->depth = depth;

	RTRACE("pixclock = %lu\n", (unsigned long)pixClock);
	RTRACE("freq = %lu\n", (unsigned long)freq);
	radeon_calc_pll_regs(rinfo, &newmode, freq);

	newmode.vclk_ecp_cntl = rinfo->init_state.vclk_ecp_cntl;

#if 0
	/* DDA */
	/* XXX: Figure out if there is really a DDA on radeons ! I think there
	 * isn't actually...
	 */
	vclk_freq = round_div(rinfo->pll.ref_clk * rinfo->fb_div,
			      rinfo->pll.ref_div * rinfo->post_div);
	xclk_freq = rinfo->pll.xclk;

	xclk_per_trans = round_div(xclk_freq * 128, vclk_freq * mode->bits_per_pixel);

	min_bits = min_bits_req(xclk_per_trans);
	useable_precision = min_bits + 1;

	xclk_per_trans_precise = round_div((xclk_freq * 128) << (11 - useable_precision),
					   vclk_freq * mode->bits_per_pixel);

	ron = (4 * rinfo->ram.mb + 3 * _max(rinfo->ram.trcd - 2, 0) +
	       2 * rinfo->ram.trp + rinfo->ram.twr + rinfo->ram.cl + rinfo->ram.tr2w +
	       xclk_per_trans) << (11 - useable_precision);
	roff = xclk_per_trans_precise * (32 - 4);

	RTRACE("ron = %d, roff = %d\n", ron, roff);
	RTRACE("vclk_freq = %d, per = %d\n", vclk_freq, xclk_per_trans_precise);

	if ((ron + rinfo->ram.rloop) >= roff) {
		printk("radeonfb: error ron out of range\n");
		return;
	}

	newmode.dda_config = (xclk_per_trans_precise |
			      (useable_precision << 16) |
			      (rinfo->ram.rloop << 20));
	newmode.dda_on_off = (ron << 16) | roff;
#endif

	if ((primary_mon == MT_DFP) || (primary_mon == MT_LCD)) {
		unsigned int hRatio, vRatio;

		if (mode->xres > rinfo->panel_xres)
			mode->xres = rinfo->panel_xres;
		if (mode->yres > rinfo->panel_yres)
			mode->yres = rinfo->panel_yres;

		newmode.fp_horz_stretch = (((rinfo->panel_xres / 8) - 1)
					   << HORZ_PANEL_SHIFT);
		newmode.fp_vert_stretch = ((rinfo->panel_yres - 1)
					   << VERT_PANEL_SHIFT);

		if (mode->xres != rinfo->panel_xres) {
			hRatio = round_div(mode->xres * HORZ_STRETCH_RATIO_MAX,
					   rinfo->panel_xres);
			newmode.fp_horz_stretch = (((((unsigned long)hRatio) & HORZ_STRETCH_RATIO_MASK)) |
						   (newmode.fp_horz_stretch &
						    (HORZ_PANEL_SIZE | HORZ_FP_LOOP_STRETCH |
						     HORZ_AUTO_RATIO_INC)));
			newmode.fp_horz_stretch |= (HORZ_STRETCH_BLEND |
						    HORZ_STRETCH_ENABLE);
		}
		newmode.fp_horz_stretch &= ~HORZ_AUTO_RATIO;

		if (mode->yres != rinfo->panel_yres) {
			vRatio = round_div(mode->yres * VERT_STRETCH_RATIO_MAX,
					   rinfo->panel_yres);
			newmode.fp_vert_stretch = (((((unsigned long)vRatio) & VERT_STRETCH_RATIO_MASK)) |
						   (newmode.fp_vert_stretch &
						   (VERT_PANEL_SIZE | VERT_STRETCH_RESERVED)));
			newmode.fp_vert_stretch |= (VERT_STRETCH_BLEND |
						    VERT_STRETCH_ENABLE);
		}
		newmode.fp_vert_stretch &= ~VERT_AUTO_RATIO_EN;

		newmode.fp_gen_cntl = (rinfo->init_state.fp_gen_cntl & (u32)
				       ~(FP_SEL_CRTC2 |
					 FP_RMX_HVSYNC_CONTROL_EN |
					 FP_DFP_SYNC_SEL |
					 FP_CRT_SYNC_SEL |
					 FP_CRTC_LOCK_8DOT |
					 FP_USE_SHADOW_EN |
					 FP_CRTC_USE_SHADOW_VEND |
					 FP_CRT_SYNC_ALT));

		newmode.fp_gen_cntl |= (FP_CRTC_DONT_SHADOW_VPAR |
					FP_CRTC_DONT_SHADOW_HEND);

		newmode.lvds_gen_cntl = rinfo->init_state.lvds_gen_cntl;
		newmode.lvds_pll_cntl = rinfo->init_state.lvds_pll_cntl;
		newmode.tmds_crc = rinfo->init_state.tmds_crc;
		newmode.tmds_transmitter_cntl = rinfo->init_state.tmds_transmitter_cntl;

		if (primary_mon == MT_LCD) {
			newmode.lvds_gen_cntl |= (LVDS_ON | LVDS_BLON);
			newmode.fp_gen_cntl &= ~(FP_FPON | FP_TMDS_EN);
		} else {
			/* DFP */
			newmode.fp_gen_cntl |= (FP_FPON | FP_TMDS_EN);
			newmode.tmds_transmitter_cntl = (TMDS_RAN_PAT_RST | TMDS_ICHCSEL) &
							 ~(TMDS_PLLRST);
			/* TMDS_PLL_EN bit is reversed on RV (and mobility) chips */
			if (rinfo->arch == RADEON_R100 ||
			    rinfo->arch == RADEON_R200 ||
			    rinfo->arch == RADEON_R300 ||
			    rinfo->arch == RADEON_R350)
				newmode.tmds_transmitter_cntl &= ~TMDS_PLL_EN;
			else
				newmode.tmds_transmitter_cntl |= TMDS_PLL_EN;

			newmode.crtc_ext_cntl &= ~CRTC_CRT_ON;
		}

		newmode.fp_crtc_h_total_disp = (((rinfo->hblank / 8) & 0x3ff) |
				(((mode->xres / 8) - 1) << 16));
		newmode.fp_crtc_v_total_disp = (rinfo->vblank & 0xffff) |
				((mode->yres - 1) << 16);
		newmode.fp_h_sync_strt_wid = ((rinfo->hOver_plus & 0x1fff) |
				(hsync_wid << 16) | (h_sync_pol << 23));
		newmode.fp_v_sync_strt_wid = ((rinfo->vOver_plus & 0xfff) |
				(vsync_wid << 16) | (v_sync_pol  << 23));
	}

	/* do it! */
	if (!rinfo->asleep)
		radeon_write_mode (rinfo, &newmode);

#if defined(CONFIG_BOOTX_TEXT)
	btext_update_display(rinfo->fb_base_phys, mode->xres, mode->yres,
			     rinfo->depth, rinfo->pitch*64);
#endif

	return;
}


static void radeon_write_pll_regs(struct radeonfb_info *rinfo, struct radeon_regs *mode)
{
	/* Workaround from XFree */
	if (rinfo->arch < RADEON_R300) {
	        /* A temporal workaround for the occational blanking on certain laptop panels. 
	           This appears to related to the PLL divider registers (fail to lock?).  
		   It occurs even when all dividers are the same with their old settings.  
	           In this case we really don't need to fiddle with PLL registers. 
	           By doing this we can avoid the blanking problem with some panels.
	        */
		if ((mode->ppll_ref_div == (INPLL(PPLL_REF_DIV) & PPLL_REF_DIV_MASK)) &&
		    (mode->ppll_div_3 == (INPLL(PPLL_DIV_3) &
		    		(PPLL_POST3_DIV_MASK | PPLL_FB3_DIV_MASK))))
            		return;
	}

	while ((INREG(CLOCK_CNTL_INDEX) & PPLL_DIV_SEL_MASK) !=
	       PPLL_DIV_SEL_MASK) {
		OUTREGP(CLOCK_CNTL_INDEX, PPLL_DIV_SEL_MASK, 0xffff);
	}

	OUTPLLP(PPLL_CNTL, PPLL_RESET, 0xffff);

	while ((INPLL(PPLL_REF_DIV) & PPLL_REF_DIV_MASK) !=
	       (mode->ppll_ref_div & PPLL_REF_DIV_MASK)) {
		OUTPLLP(PPLL_REF_DIV, mode->ppll_ref_div, ~PPLL_REF_DIV_MASK);
	}

	while ((INPLL(PPLL_DIV_3) & PPLL_FB3_DIV_MASK) !=
	       (mode->ppll_div_3 & PPLL_FB3_DIV_MASK)) {
		OUTPLLP(PPLL_DIV_3, mode->ppll_div_3, ~PPLL_FB3_DIV_MASK);
	}

	while ((INPLL(PPLL_DIV_3) & PPLL_POST3_DIV_MASK) !=
	       (mode->ppll_div_3 & PPLL_POST3_DIV_MASK)) {
		OUTPLLP(PPLL_DIV_3, mode->ppll_div_3, ~PPLL_POST3_DIV_MASK);
	}

	OUTPLL(HTOTAL_CNTL, 0);

	OUTPLLP(PPLL_CNTL, 0, ~PPLL_RESET);
}

static void radeon_write_mode (struct radeonfb_info *rinfo,
                               struct radeon_regs *mode)
{
	int i;
	int primary_mon = PRIMARY_MONITOR(rinfo);

	radeonfb_blank(VESA_POWERDOWN, (struct fb_info *)rinfo);

        if (rinfo->arch == RADEON_M6) {
		for (i=0; i<8; i++)
			OUTREG(common_regs_m6[i].reg, common_regs_m6[i].val);
	} else {
		for (i=0; i<9; i++)
			OUTREG(common_regs[i].reg, common_regs[i].val);
	}

	OUTREG(CRTC_GEN_CNTL, mode->crtc_gen_cntl);
	OUTREGP(CRTC_EXT_CNTL, mode->crtc_ext_cntl,
		CRTC_HSYNC_DIS | CRTC_VSYNC_DIS | CRTC_DISPLAY_DIS);
	OUTREG(CRTC_MORE_CNTL, mode->crtc_more_cntl);
	OUTREGP(DAC_CNTL, mode->dac_cntl, DAC_RANGE_CNTL | DAC_BLANKING);
	OUTREG(CRTC_H_TOTAL_DISP, mode->crtc_h_total_disp);
	OUTREG(CRTC_H_SYNC_STRT_WID, mode->crtc_h_sync_strt_wid);
	OUTREG(CRTC_V_TOTAL_DISP, mode->crtc_v_total_disp);
	OUTREG(CRTC_V_SYNC_STRT_WID, mode->crtc_v_sync_strt_wid);
	OUTREG(CRTC_OFFSET, 0);
	OUTREG(CRTC_OFFSET_CNTL, 0);
	OUTREG(CRTC_PITCH, mode->crtc_pitch);
	OUTREG(SURFACE_CNTL, mode->surface_cntl);

	radeon_write_pll_regs(rinfo, mode);

#if 0
	/* Those don't seem to actually exist in radeon's, despite some drivers still
	 * apparently trying to fill them, including some ATI sample codes ...
	 * Can someone confirm what's up ? --BenH.
	 */
	OUTREG(DDA_CONFIG, mode->dda_config);
	OUTREG(DDA_ON_OFF, mode->dda_on_off);
#endif

	if ((primary_mon == MT_DFP) || (primary_mon == MT_LCD)) {
		OUTREG(FP_CRTC_H_TOTAL_DISP, mode->fp_crtc_h_total_disp);
		OUTREG(FP_CRTC_V_TOTAL_DISP, mode->fp_crtc_v_total_disp);
		OUTREG(FP_H_SYNC_STRT_WID, mode->fp_h_sync_strt_wid);
		OUTREG(FP_V_SYNC_STRT_WID, mode->fp_v_sync_strt_wid);
		OUTREG(FP_HORZ_STRETCH, mode->fp_horz_stretch);
		OUTREG(FP_VERT_STRETCH, mode->fp_vert_stretch);
		OUTREG(FP_GEN_CNTL, mode->fp_gen_cntl);
		OUTREG(TMDS_CRC, mode->tmds_crc);
		OUTREG(TMDS_TRANSMITTER_CNTL, mode->tmds_transmitter_cntl);

		if (primary_mon == MT_LCD) {
			unsigned int tmp = INREG(LVDS_GEN_CNTL);

			mode->lvds_gen_cntl &= ~LVDS_STATE_MASK;
			mode->lvds_gen_cntl |= (rinfo->init_state.lvds_gen_cntl & LVDS_STATE_MASK);

			if ((tmp & (LVDS_ON | LVDS_BLON)) ==
			    (mode->lvds_gen_cntl & (LVDS_ON | LVDS_BLON))) {
				OUTREG(LVDS_GEN_CNTL, mode->lvds_gen_cntl);
			} else {
				if (mode->lvds_gen_cntl & (LVDS_ON | LVDS_BLON)) {
					udelay(1000);
					OUTREG(LVDS_GEN_CNTL, mode->lvds_gen_cntl);
				} else {
					OUTREG(LVDS_GEN_CNTL, mode->lvds_gen_cntl |
					       LVDS_BLON);
					udelay(1000);
					OUTREG(LVDS_GEN_CNTL, mode->lvds_gen_cntl);
				}
			}
		}
	}

	radeonfb_blank(VESA_NO_BLANKING, (struct fb_info *)rinfo);

	OUTPLL(VCLK_ECP_CNTL, mode->vclk_ecp_cntl);

	return;
}


#ifdef CONFIG_PMAC_BACKLIGHT

/* TODO: Dbl check these tables, we don't go up to full ON backlight
 * in these, possibly because we noticed MacOS doesn't, but I'd prefer
 * having some more official numbers from ATI
 */
static int backlight_conv_m6[] = {
        0xff, 0xc0, 0xb5, 0xaa, 0x9f, 0x94, 0x89, 0x7e,
        0x73, 0x68, 0x5d, 0x52, 0x47, 0x3c, 0x31, 0x24
};
static int backlight_conv_m7[] = {
        0x00, 0x3f, 0x4a, 0x55, 0x60, 0x6b, 0x76, 0x81,
        0x8c, 0x97, 0xa2, 0xad, 0xb8, 0xc3, 0xce, 0xd9
};
                
/* We turn off the LCD completely instead of just dimming the backlight.
 * This provides some greater power saving and the display is useless
 * without backlight anyway.
 */


static int radeon_set_backlight_enable(int on, int level, void *data)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)data;
	unsigned int lvds_gen_cntl = INREG(LVDS_GEN_CNTL);
	int* conv_table;

	/* Pardon me for that hack... maybe some day we can figure
	 * out in what direction backlight should work on a given
	 * panel ?
	 */
	if ((rinfo->arch == RADEON_M7 || rinfo->arch == RADEON_M9)
		&& !machine_is_compatible("PowerBook4,3"))
		conv_table = backlight_conv_m7;
	else
		conv_table = backlight_conv_m6;

	lvds_gen_cntl |= (LVDS_BL_MOD_EN | LVDS_BLON);
	if (on && (level > BACKLIGHT_OFF)) {
		lvds_gen_cntl |= LVDS_DIGON;
		if ((lvds_gen_cntl & LVDS_ON) == 0) {
			lvds_gen_cntl &= ~LVDS_BLON;
			OUTREG(LVDS_GEN_CNTL, lvds_gen_cntl);
			(void)INREG(LVDS_GEN_CNTL);
			mdelay(10);
			lvds_gen_cntl |= LVDS_BLON;
			OUTREG(LVDS_GEN_CNTL, lvds_gen_cntl);
		}
		lvds_gen_cntl &= ~LVDS_BL_MOD_LEVEL_MASK;
		lvds_gen_cntl |= (conv_table[level] <<
				  LVDS_BL_MOD_LEVEL_SHIFT);
		lvds_gen_cntl |= (LVDS_ON | LVDS_EN);
		lvds_gen_cntl &= ~LVDS_DISPLAY_DIS;
	} else {
		lvds_gen_cntl &= ~LVDS_BL_MOD_LEVEL_MASK;
		lvds_gen_cntl |= (conv_table[0] <<
				  LVDS_BL_MOD_LEVEL_SHIFT);
		lvds_gen_cntl |= LVDS_DISPLAY_DIS;
		OUTREG(LVDS_GEN_CNTL, lvds_gen_cntl);
		udelay(10);
		lvds_gen_cntl &= ~(LVDS_ON | LVDS_EN | LVDS_BLON | LVDS_DIGON);
	}

	OUTREG(LVDS_GEN_CNTL, lvds_gen_cntl);
	rinfo->init_state.lvds_gen_cntl &= ~LVDS_STATE_MASK;
	rinfo->init_state.lvds_gen_cntl |= (lvds_gen_cntl & LVDS_STATE_MASK);

	return 0;
}

static int radeon_set_backlight_level(int level, void *data)
{
	return radeon_set_backlight_enable(1, level, data);
}
#endif /* CONFIG_PMAC_BACKLIGHT */


#ifdef CONFIG_PMAC_PBOOK

/*
 * Radeon M6, M7 and M9 Power Management code. This code currently
 * only supports the mobile chips in D2 mode, that is typically what
 * is used on Apple laptops, it's based from some informations provided by ATI
 * along with hours of tracing of MacOS drivers.
 * 
 * New version of this code almost totally rewritten by ATI, many thanks
 * for their support.
 */

static void OUTMC( struct radeonfb_info *rinfo, u8 indx, u32 value)
{
	OUTREG( MC_IND_INDEX, indx | MC_IND_INDEX__MC_IND_WR_EN);	
	OUTREG( MC_IND_DATA, value);		
}

static u32 INMC(struct radeonfb_info *rinfo, u8 indx)
{
	OUTREG( MC_IND_INDEX, indx);					
	return INREG( MC_IND_DATA);
}

static void radeon_pm_save_regs(struct radeonfb_info *rinfo)
{
	rinfo->save_regs[0] = INPLL(PLL_PWRMGT_CNTL);
	rinfo->save_regs[1] = INPLL(CLK_PWRMGT_CNTL);
	rinfo->save_regs[2] = INPLL(MCLK_CNTL);
	rinfo->save_regs[3] = INPLL(SCLK_CNTL);
	rinfo->save_regs[4] = INPLL(CLK_PIN_CNTL);
	rinfo->save_regs[5] = INPLL(VCLK_ECP_CNTL);
	rinfo->save_regs[6] = INPLL(PIXCLKS_CNTL);
	rinfo->save_regs[7] = INPLL(MCLK_MISC);
	rinfo->save_regs[8] = INPLL(P2PLL_CNTL);
	
	rinfo->save_regs[9] = INREG(DISP_MISC_CNTL);
	rinfo->save_regs[10] = INREG(DISP_PWR_MAN);
	rinfo->save_regs[11] = INREG(LVDS_GEN_CNTL);
	rinfo->save_regs[12] = INREG(LVDS_PLL_CNTL);
	rinfo->save_regs[13] = INREG(TV_DAC_CNTL);
	rinfo->save_regs[14] = INREG(BUS_CNTL1);
	rinfo->save_regs[15] = INREG(CRTC_OFFSET_CNTL);
	rinfo->save_regs[16] = INREG(AGP_CNTL);
	rinfo->save_regs[17] = (INREG(CRTC_GEN_CNTL) & 0xfdffffff) | 0x04000000;
	rinfo->save_regs[18] = (INREG(CRTC2_GEN_CNTL) & 0xfdffffff) | 0x04000000;
	rinfo->save_regs[19] = INREG(GPIOPAD_A);
	rinfo->save_regs[20] = INREG(GPIOPAD_EN);
	rinfo->save_regs[21] = INREG(GPIOPAD_MASK);
	rinfo->save_regs[22] = INREG(ZV_LCDPAD_A);
	rinfo->save_regs[23] = INREG(ZV_LCDPAD_EN);
	rinfo->save_regs[24] = INREG(ZV_LCDPAD_MASK);
	rinfo->save_regs[25] = INREG(GPIO_VGA_DDC);
	rinfo->save_regs[26] = INREG(GPIO_DVI_DDC);
	rinfo->save_regs[27] = INREG(GPIO_MONID);
	rinfo->save_regs[28] = INREG(GPIO_CRT2_DDC);

	rinfo->save_regs[29] = INREG(SURFACE_CNTL);
	rinfo->save_regs[30] = INREG(MC_FB_LOCATION);
	rinfo->save_regs[31] = INREG(DISPLAY_BASE_ADDR);
	rinfo->save_regs[32] = INREG(MC_AGP_LOCATION);
	rinfo->save_regs[33] = INREG(CRTC2_DISPLAY_BASE_ADDR);
}

static void radeon_pm_restore_regs(struct radeonfb_info *rinfo)
{
	OUTPLL(P2PLL_CNTL, rinfo->save_regs[8] & 0xFFFFFFFE); /* First */
	
	OUTPLL(PLL_PWRMGT_CNTL, rinfo->save_regs[0]);
	OUTPLL(CLK_PWRMGT_CNTL, rinfo->save_regs[1]);
	OUTPLL(MCLK_CNTL, rinfo->save_regs[2]);
	OUTPLL(SCLK_CNTL, rinfo->save_regs[3]);
	OUTPLL(CLK_PIN_CNTL, rinfo->save_regs[4]);
	OUTPLL(VCLK_ECP_CNTL, rinfo->save_regs[5]);
	OUTPLL(PIXCLKS_CNTL, rinfo->save_regs[6]);
	OUTPLL(MCLK_MISC, rinfo->save_regs[7]);
	
	OUTREG(SURFACE_CNTL, rinfo->save_regs[29]);
	OUTREG(MC_FB_LOCATION, rinfo->save_regs[30]);
	OUTREG(DISPLAY_BASE_ADDR, rinfo->save_regs[31]);
	OUTREG(MC_AGP_LOCATION, rinfo->save_regs[32]);
	OUTREG(CRTC2_DISPLAY_BASE_ADDR, rinfo->save_regs[33]);

	OUTREG(DISP_MISC_CNTL, rinfo->save_regs[9]);
	OUTREG(DISP_PWR_MAN, rinfo->save_regs[10]);
	OUTREG(LVDS_GEN_CNTL, rinfo->save_regs[11]);
	OUTREG(LVDS_PLL_CNTL,rinfo->save_regs[12]);
	OUTREG(TV_DAC_CNTL, rinfo->save_regs[13]);
	OUTREG(BUS_CNTL1, rinfo->save_regs[14]);
	OUTREG(CRTC_OFFSET_CNTL, rinfo->save_regs[15]);
	OUTREG(AGP_CNTL, rinfo->save_regs[16]);
	OUTREG(CRTC_GEN_CNTL, rinfo->save_regs[17]);
	OUTREG(CRTC2_GEN_CNTL, rinfo->save_regs[18]);

	// wait VBL before that one  ?
	OUTPLL(P2PLL_CNTL, rinfo->save_regs[8]);
	
	OUTREG(GPIOPAD_A, rinfo->save_regs[19]);
	OUTREG(GPIOPAD_EN, rinfo->save_regs[20]);
	OUTREG(GPIOPAD_MASK, rinfo->save_regs[21]);
	OUTREG(ZV_LCDPAD_A, rinfo->save_regs[22]);
	OUTREG(ZV_LCDPAD_EN, rinfo->save_regs[23]);
	OUTREG(ZV_LCDPAD_MASK, rinfo->save_regs[24]);
	OUTREG(GPIO_VGA_DDC, rinfo->save_regs[25]);
	OUTREG(GPIO_DVI_DDC, rinfo->save_regs[26]);
	OUTREG(GPIO_MONID, rinfo->save_regs[27]);
	OUTREG(GPIO_CRT2_DDC, rinfo->save_regs[28]);
}

static void radeon_pm_disable_iopad(struct radeonfb_info *rinfo)
{		
	OUTREG(GPIOPAD_MASK, 0x0001ffff);
	OUTREG(GPIOPAD_EN, 0x00000400);
	OUTREG(GPIOPAD_A, 0x00000000);		
        OUTREG(ZV_LCDPAD_MASK, 0x00000000);
        OUTREG(ZV_LCDPAD_EN, 0x00000000);
      	OUTREG(ZV_LCDPAD_A, 0x00000000); 	
	OUTREG(GPIO_VGA_DDC, 0x00030000);
	OUTREG(GPIO_DVI_DDC, 0x00000000);
	OUTREG(GPIO_MONID, 0x00030000);
	OUTREG(GPIO_CRT2_DDC, 0x00000000);
}

static void radeon_pm_program_v2clk(struct radeonfb_info *rinfo)
{
	/* Set v2clk to 65MHz */
  	OUTPLL(pllPIXCLKS_CNTL,
  		INPLL(pllPIXCLKS_CNTL) & ~PIXCLKS_CNTL__PIX2CLK_SRC_SEL_MASK);
	 
  	OUTPLL(pllP2PLL_REF_DIV, 0x0000000c);
	OUTPLL(pllP2PLL_CNTL, 0x0000bf00);
	OUTPLL(pllP2PLL_DIV_0, 0x00020074 | P2PLL_DIV_0__P2PLL_ATOMIC_UPDATE_W);
	
	OUTPLL(pllP2PLL_CNTL, INPLL(pllP2PLL_CNTL) & ~P2PLL_CNTL__P2PLL_SLEEP);
	mdelay(1);

	OUTPLL(pllP2PLL_CNTL, INPLL(pllP2PLL_CNTL) & ~P2PLL_CNTL__P2PLL_RESET); 	
	mdelay( 1);

  	OUTPLL(pllPIXCLKS_CNTL,
  		(INPLL(pllPIXCLKS_CNTL) & ~PIXCLKS_CNTL__PIX2CLK_SRC_SEL_MASK)
  		| (0x03 << PIXCLKS_CNTL__PIX2CLK_SRC_SEL__SHIFT));
	mdelay( 1);	
}

static void radeon_pm_low_current(struct radeonfb_info *rinfo)
{
	u32 reg;

	reg  = INREG(BUS_CNTL1);
	reg &= ~BUS_CNTL1_MOBILE_PLATFORM_SEL_MASK;
	reg |= BUS_CNTL1_AGPCLK_VALID | (1<<BUS_CNTL1_MOBILE_PLATFORM_SEL_SHIFT);
	OUTREG(BUS_CNTL1, reg);
	
	reg  = INPLL(PLL_PWRMGT_CNTL);
	reg |= PLL_PWRMGT_CNTL_SPLL_TURNOFF | PLL_PWRMGT_CNTL_PPLL_TURNOFF |
		PLL_PWRMGT_CNTL_P2PLL_TURNOFF | PLL_PWRMGT_CNTL_TVPLL_TURNOFF;
	reg &= ~PLL_PWRMGT_CNTL_SU_MCLK_USE_BCLK;
	reg &= ~PLL_PWRMGT_CNTL_MOBILE_SU;
	OUTPLL(PLL_PWRMGT_CNTL, reg);
	
	reg  = INREG(TV_DAC_CNTL);
	reg &= ~(TV_DAC_CNTL_BGADJ_MASK |TV_DAC_CNTL_DACADJ_MASK);
	reg |=TV_DAC_CNTL_BGSLEEP | TV_DAC_CNTL_RDACPD | TV_DAC_CNTL_GDACPD |
		TV_DAC_CNTL_BDACPD |
		(8<<TV_DAC_CNTL_BGADJ__SHIFT) | (8<<TV_DAC_CNTL_DACADJ__SHIFT);
	OUTREG(TV_DAC_CNTL, reg);
	
	reg  = INREG(TMDS_TRANSMITTER_CNTL);
	reg &= ~(TMDS_PLL_EN | TMDS_PLLRST);
	OUTREG(TMDS_TRANSMITTER_CNTL, reg);

	reg = INREG(DAC_CNTL);
	reg &= ~DAC_CMP_EN;
	OUTREG(DAC_CNTL, reg);

	reg = INREG(DAC_CNTL2);
	reg &= ~DAC2_CMP_EN;
	OUTREG(DAC_CNTL2, reg);
	
	reg  = INREG(TV_DAC_CNTL);
	reg &= ~TV_DAC_CNTL_DETECT;
	OUTREG(TV_DAC_CNTL, reg);
}

static void radeon_pm_setup_for_suspend(struct radeonfb_info *rinfo)
{

	u32 sclk_cntl, mclk_cntl, sclk_more_cntl;

	u32 pll_pwrmgt_cntl;
	u32 clk_pwrmgt_cntl;
	u32 clk_pin_cntl;
	u32 vclk_ecp_cntl; 
	u32 pixclks_cntl;
	u32 disp_mis_cntl;
	u32 disp_pwr_man;

	
	/* Force Core Clocks */
	sclk_cntl = INPLL( pllSCLK_CNTL_M6);
	sclk_cntl |= 	SCLK_CNTL_M6__IDCT_MAX_DYN_STOP_LAT|
			SCLK_CNTL_M6__VIP_MAX_DYN_STOP_LAT|
			SCLK_CNTL_M6__RE_MAX_DYN_STOP_LAT|
			SCLK_CNTL_M6__PB_MAX_DYN_STOP_LAT|
			SCLK_CNTL_M6__TAM_MAX_DYN_STOP_LAT|
			SCLK_CNTL_M6__TDM_MAX_DYN_STOP_LAT|
			SCLK_CNTL_M6__RB_MAX_DYN_STOP_LAT|
			
			SCLK_CNTL_M6__FORCE_DISP2|
			SCLK_CNTL_M6__FORCE_CP|
			SCLK_CNTL_M6__FORCE_HDP|
			SCLK_CNTL_M6__FORCE_DISP1|
			SCLK_CNTL_M6__FORCE_TOP|
			SCLK_CNTL_M6__FORCE_E2|
			SCLK_CNTL_M6__FORCE_SE|
			SCLK_CNTL_M6__FORCE_IDCT|
			SCLK_CNTL_M6__FORCE_VIP|
			
			SCLK_CNTL_M6__FORCE_RE|
			SCLK_CNTL_M6__FORCE_PB|
			SCLK_CNTL_M6__FORCE_TAM|
			SCLK_CNTL_M6__FORCE_TDM|
			SCLK_CNTL_M6__FORCE_RB|
			SCLK_CNTL_M6__FORCE_TV_SCLK|
			SCLK_CNTL_M6__FORCE_SUBPIC|
			SCLK_CNTL_M6__FORCE_OV0;

	OUTPLL( pllSCLK_CNTL_M6, sclk_cntl);

	sclk_more_cntl = INPLL(pllSCLK_MORE_CNTL);
	sclk_more_cntl |= 	SCLK_MORE_CNTL__FORCE_DISPREGS |
				SCLK_MORE_CNTL__FORCE_MC_GUI |
				SCLK_MORE_CNTL__FORCE_MC_HOST;

	OUTPLL(pllSCLK_MORE_CNTL, sclk_more_cntl);		

	
	mclk_cntl = INPLL( pllMCLK_CNTL_M6);
	mclk_cntl &= ~(	MCLK_CNTL_M6__FORCE_MCLKA |  
			MCLK_CNTL_M6__FORCE_MCLKB |
			MCLK_CNTL_M6__FORCE_YCLKA | 
			MCLK_CNTL_M6__FORCE_YCLKB | 
			MCLK_CNTL_M6__FORCE_MC
		      );	
    	OUTPLL( pllMCLK_CNTL_M6, mclk_cntl);
	
	/* Force Display clocks	*/
	vclk_ecp_cntl = INPLL( pllVCLK_ECP_CNTL);
	vclk_ecp_cntl &= ~(VCLK_ECP_CNTL__PIXCLK_ALWAYS_ONb |VCLK_ECP_CNTL__PIXCLK_DAC_ALWAYS_ONb);
	vclk_ecp_cntl |= VCLK_ECP_CNTL__ECP_FORCE_ON;
	OUTPLL( pllVCLK_ECP_CNTL, vclk_ecp_cntl);
	
	
	pixclks_cntl = INPLL( pllPIXCLKS_CNTL);
	pixclks_cntl &= ~(	PIXCLKS_CNTL__PIXCLK_GV_ALWAYS_ONb | 
				PIXCLKS_CNTL__PIXCLK_BLEND_ALWAYS_ONb|
				PIXCLKS_CNTL__PIXCLK_DIG_TMDS_ALWAYS_ONb |
				PIXCLKS_CNTL__PIXCLK_LVDS_ALWAYS_ONb|
				PIXCLKS_CNTL__PIXCLK_TMDS_ALWAYS_ONb|
				PIXCLKS_CNTL__PIX2CLK_ALWAYS_ONb|
				PIXCLKS_CNTL__PIX2CLK_DAC_ALWAYS_ONb);
						
 	OUTPLL( pllPIXCLKS_CNTL, pixclks_cntl);



	/* Enable System power management */
	pll_pwrmgt_cntl = INPLL( pllPLL_PWRMGT_CNTL);
	
	pll_pwrmgt_cntl |= 	PLL_PWRMGT_CNTL__SPLL_TURNOFF |
				PLL_PWRMGT_CNTL__MPLL_TURNOFF|
				PLL_PWRMGT_CNTL__PPLL_TURNOFF|
				PLL_PWRMGT_CNTL__P2PLL_TURNOFF|
				PLL_PWRMGT_CNTL__TVPLL_TURNOFF;
						
	OUTPLL( pllPLL_PWRMGT_CNTL, pll_pwrmgt_cntl);
	
	clk_pwrmgt_cntl	 = INPLL( pllCLK_PWRMGT_CNTL_M6);
	
	clk_pwrmgt_cntl &= ~(	CLK_PWRMGT_CNTL_M6__MPLL_PWRMGT_OFF|
				CLK_PWRMGT_CNTL_M6__SPLL_PWRMGT_OFF|
				CLK_PWRMGT_CNTL_M6__PPLL_PWRMGT_OFF|
				CLK_PWRMGT_CNTL_M6__P2PLL_PWRMGT_OFF|
				CLK_PWRMGT_CNTL_M6__MCLK_TURNOFF|
				CLK_PWRMGT_CNTL_M6__SCLK_TURNOFF|
				CLK_PWRMGT_CNTL_M6__PCLK_TURNOFF|
				CLK_PWRMGT_CNTL_M6__P2CLK_TURNOFF|
				CLK_PWRMGT_CNTL_M6__TVPLL_PWRMGT_OFF|
				CLK_PWRMGT_CNTL_M6__GLOBAL_PMAN_EN|
				CLK_PWRMGT_CNTL_M6__ENGINE_DYNCLK_MODE|
				CLK_PWRMGT_CNTL_M6__ACTIVE_HILO_LAT_MASK|
				CLK_PWRMGT_CNTL_M6__CG_NO1_DEBUG_MASK			
			);
						
	clk_pwrmgt_cntl |= CLK_PWRMGT_CNTL_M6__GLOBAL_PMAN_EN | CLK_PWRMGT_CNTL_M6__DISP_PM;
	
	OUTPLL( pllCLK_PWRMGT_CNTL_M6, clk_pwrmgt_cntl);	
	
	clk_pin_cntl = INPLL( pllCLK_PIN_CNTL);
	
	clk_pin_cntl &= ~CLK_PIN_CNTL__ACCESS_REGS_IN_SUSPEND;
	OUTPLL( pllMCLK_MISC, INPLL( pllMCLK_MISC) | MCLK_MISC__EN_MCLK_TRISTATE_IN_SUSPEND);	
	
	/* AGP PLL control */
	OUTREG(BUS_CNTL1, INREG(BUS_CNTL1) |  BUS_CNTL1__AGPCLK_VALID);

	OUTREG(BUS_CNTL1,
		(INREG(BUS_CNTL1) & ~BUS_CNTL1__MOBILE_PLATFORM_SEL_MASK)
		| (2<<BUS_CNTL1__MOBILE_PLATFORM_SEL__SHIFT));	// 440BX
	OUTREG(CRTC_OFFSET_CNTL, (INREG(CRTC_OFFSET_CNTL) & ~CRTC_OFFSET_CNTL__CRTC_STEREO_SYNC_OUT_EN));
	
	clk_pin_cntl &= ~CLK_PIN_CNTL__CG_CLK_TO_OUTPIN;
	clk_pin_cntl |= CLK_PIN_CNTL__XTALIN_ALWAYS_ONb;	
	OUTPLL( pllCLK_PIN_CNTL, clk_pin_cntl);

	/* Solano2M */
	OUTREG(AGP_CNTL,
		(INREG(AGP_CNTL) & ~(AGP_CNTL__MAX_IDLE_CLK_MASK))
		| (0x20<<AGP_CNTL__MAX_IDLE_CLK__SHIFT));

	/* ACPI mode */
	OUTPLL( pllPLL_PWRMGT_CNTL, INPLL( pllPLL_PWRMGT_CNTL) & ~PLL_PWRMGT_CNTL__PM_MODE_SEL);					


	disp_mis_cntl = INREG(DISP_MISC_CNTL);
	
	disp_mis_cntl &= ~(	DISP_MISC_CNTL__SOFT_RESET_GRPH_PP | 
				DISP_MISC_CNTL__SOFT_RESET_SUBPIC_PP | 
				DISP_MISC_CNTL__SOFT_RESET_OV0_PP |
				DISP_MISC_CNTL__SOFT_RESET_GRPH_SCLK|
				DISP_MISC_CNTL__SOFT_RESET_SUBPIC_SCLK|
				DISP_MISC_CNTL__SOFT_RESET_OV0_SCLK|
				DISP_MISC_CNTL__SOFT_RESET_GRPH2_PP|
				DISP_MISC_CNTL__SOFT_RESET_GRPH2_SCLK|
				DISP_MISC_CNTL__SOFT_RESET_LVDS|
				DISP_MISC_CNTL__SOFT_RESET_TMDS|
				DISP_MISC_CNTL__SOFT_RESET_DIG_TMDS|
				DISP_MISC_CNTL__SOFT_RESET_TV);
	
	OUTREG(DISP_MISC_CNTL, disp_mis_cntl);					
						
	disp_pwr_man = INREG(DISP_PWR_MAN);
	
	disp_pwr_man &= ~(	DISP_PWR_MAN__DISP_PWR_MAN_D3_CRTC_EN	| 
						DISP_PWR_MAN__DISP2_PWR_MAN_D3_CRTC2_EN |
						DISP_PWR_MAN__DISP_PWR_MAN_DPMS_MASK|		
						DISP_PWR_MAN__DISP_D3_RST|
						DISP_PWR_MAN__DISP_D3_REG_RST
					);
	
	disp_pwr_man |= DISP_PWR_MAN__DISP_D3_GRPH_RST|
					DISP_PWR_MAN__DISP_D3_SUBPIC_RST|
					DISP_PWR_MAN__DISP_D3_OV0_RST|
					DISP_PWR_MAN__DISP_D1D2_GRPH_RST|
					DISP_PWR_MAN__DISP_D1D2_SUBPIC_RST|
					DISP_PWR_MAN__DISP_D1D2_OV0_RST|
					DISP_PWR_MAN__DIG_TMDS_ENABLE_RST|
					DISP_PWR_MAN__TV_ENABLE_RST| 
//					DISP_PWR_MAN__AUTO_PWRUP_EN|
					0;
	
	OUTREG(DISP_PWR_MAN, disp_pwr_man);					
							
	clk_pwrmgt_cntl = INPLL( pllCLK_PWRMGT_CNTL_M6);
	pll_pwrmgt_cntl = INPLL( pllPLL_PWRMGT_CNTL) ;
	clk_pin_cntl 	= INPLL( pllCLK_PIN_CNTL);
	disp_pwr_man	= INREG(DISP_PWR_MAN);
		
	
	/* D2 */
	clk_pwrmgt_cntl |= CLK_PWRMGT_CNTL_M6__DISP_PM;
	pll_pwrmgt_cntl |= PLL_PWRMGT_CNTL__MOBILE_SU | PLL_PWRMGT_CNTL__SU_SCLK_USE_BCLK;
	clk_pin_cntl	|= CLK_PIN_CNTL__XTALIN_ALWAYS_ONb;
	disp_pwr_man 	&= ~(DISP_PWR_MAN__DISP_PWR_MAN_D3_CRTC_EN_MASK | DISP_PWR_MAN__DISP2_PWR_MAN_D3_CRTC2_EN_MASK);							
						

	OUTPLL( pllCLK_PWRMGT_CNTL_M6, clk_pwrmgt_cntl);
	OUTPLL( pllPLL_PWRMGT_CNTL, pll_pwrmgt_cntl);
	OUTPLL( pllCLK_PIN_CNTL, clk_pin_cntl);
	OUTREG(DISP_PWR_MAN, disp_pwr_man);

	/* disable display request & disable display */
	OUTREG( CRTC_GEN_CNTL, (INREG( CRTC_GEN_CNTL) & ~CRTC_GEN_CNTL__CRTC_EN) | CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B);
	OUTREG( CRTC2_GEN_CNTL, (INREG( CRTC2_GEN_CNTL) & ~CRTC2_GEN_CNTL__CRTC2_EN) | CRTC2_GEN_CNTL__CRTC2_DISP_REQ_EN_B);

	mdelay(17);				   

}

static void radeon_pm_disable_dynamic_mode(struct radeonfb_info *rinfo)
{

	u32 sclk_cntl;
	u32 mclk_cntl;
	u32 sclk_more_cntl;
	
	u32 vclk_ecp_cntl;
	u32 pixclks_cntl;

	/* Mobility chips only */
	if ((rinfo->arch != RADEON_M6) && (rinfo->arch != RADEON_M7) && (rinfo->arch != RADEON_M9))
		return;
	
	/* Force Core Clocks */
	sclk_cntl = INPLL( pllSCLK_CNTL_M6);
	sclk_cntl |= 	SCLK_CNTL_M6__FORCE_CP|
			SCLK_CNTL_M6__FORCE_HDP|
			SCLK_CNTL_M6__FORCE_DISP1|
			SCLK_CNTL_M6__FORCE_DISP2|
			SCLK_CNTL_M6__FORCE_TOP|
			SCLK_CNTL_M6__FORCE_E2|
			SCLK_CNTL_M6__FORCE_SE|
			SCLK_CNTL_M6__FORCE_IDCT|
			SCLK_CNTL_M6__FORCE_VIP|
			SCLK_CNTL_M6__FORCE_RE|
			SCLK_CNTL_M6__FORCE_PB|
			SCLK_CNTL_M6__FORCE_TAM|
			SCLK_CNTL_M6__FORCE_TDM|
			SCLK_CNTL_M6__FORCE_RB|
			SCLK_CNTL_M6__FORCE_TV_SCLK|
			SCLK_CNTL_M6__FORCE_SUBPIC|
			SCLK_CNTL_M6__FORCE_OV0;
    	OUTPLL( pllSCLK_CNTL_M6, sclk_cntl);
	
	
	
	sclk_more_cntl = INPLL(pllSCLK_MORE_CNTL);
	sclk_more_cntl |= 	SCLK_MORE_CNTL__FORCE_DISPREGS|
				SCLK_MORE_CNTL__FORCE_MC_GUI|
				SCLK_MORE_CNTL__FORCE_MC_HOST;	
	OUTPLL(pllSCLK_MORE_CNTL, sclk_more_cntl);
	
	/* Force Display clocks	*/
	vclk_ecp_cntl = INPLL( pllVCLK_ECP_CNTL);
	vclk_ecp_cntl &= ~(	VCLK_ECP_CNTL__PIXCLK_ALWAYS_ONb |
			 	VCLK_ECP_CNTL__PIXCLK_DAC_ALWAYS_ONb);

	OUTPLL( pllVCLK_ECP_CNTL, vclk_ecp_cntl);
	
	pixclks_cntl = INPLL( pllPIXCLKS_CNTL);
	pixclks_cntl &= ~(	PIXCLKS_CNTL__PIXCLK_GV_ALWAYS_ONb |
			 	PIXCLKS_CNTL__PIXCLK_BLEND_ALWAYS_ONb|
				PIXCLKS_CNTL__PIXCLK_DIG_TMDS_ALWAYS_ONb |
				PIXCLKS_CNTL__PIXCLK_LVDS_ALWAYS_ONb|
				PIXCLKS_CNTL__PIXCLK_TMDS_ALWAYS_ONb|
				PIXCLKS_CNTL__PIX2CLK_ALWAYS_ONb|
				PIXCLKS_CNTL__PIX2CLK_DAC_ALWAYS_ONb);
						
 	OUTPLL( pllPIXCLKS_CNTL, pixclks_cntl);

	/* Force Memory Clocks */
	mclk_cntl  = INPLL( pllMCLK_CNTL_M6);
	mclk_cntl |= 	MCLK_CNTL_M6__FORCE_MCLKA|  
			MCLK_CNTL_M6__FORCE_MCLKB|	
			MCLK_CNTL_M6__FORCE_YCLKA|
			MCLK_CNTL_M6__FORCE_YCLKB;
			
    	OUTPLL( pllMCLK_CNTL_M6, mclk_cntl);
}

static void radeon_pm_enable_dynamic_mode(struct radeonfb_info *rinfo)
{
	u32 clk_pwrmgt_cntl;
	u32 sclk_cntl;
	u32 sclk_more_cntl;
	u32 clk_pin_cntl;
	u32 pixclks_cntl;
	u32 vclk_ecp_cntl;
	u32 mclk_cntl;
	u32 mclk_misc;

	/* Mobility chips only */
	if ((rinfo->arch != RADEON_M6) && (rinfo->arch != RADEON_M7) && (rinfo->arch != RADEON_M9))
		return;
	
	/* Set Latencies */
	clk_pwrmgt_cntl = INPLL( pllCLK_PWRMGT_CNTL_M6);
	
	clk_pwrmgt_cntl &= ~(	 CLK_PWRMGT_CNTL_M6__ENGINE_DYNCLK_MODE_MASK|
				 CLK_PWRMGT_CNTL_M6__ACTIVE_HILO_LAT_MASK|
				 CLK_PWRMGT_CNTL_M6__DISP_DYN_STOP_LAT_MASK|
				 CLK_PWRMGT_CNTL_M6__DYN_STOP_MODE_MASK);
	/* Mode 1 */
	clk_pwrmgt_cntl = 	CLK_PWRMGT_CNTL_M6__MC_CH_MODE|
				CLK_PWRMGT_CNTL_M6__ENGINE_DYNCLK_MODE | 
				(1<<CLK_PWRMGT_CNTL_M6__ACTIVE_HILO_LAT__SHIFT) |
				(0<<CLK_PWRMGT_CNTL_M6__DISP_DYN_STOP_LAT__SHIFT)|
				(0<<CLK_PWRMGT_CNTL_M6__DYN_STOP_MODE__SHIFT);

	OUTPLL( pllCLK_PWRMGT_CNTL_M6, clk_pwrmgt_cntl);
						

	clk_pin_cntl = INPLL( pllCLK_PIN_CNTL);
	clk_pin_cntl |= CLK_PIN_CNTL__SCLK_DYN_START_CNTL;
	 
	OUTPLL( pllCLK_PIN_CNTL, clk_pin_cntl);

	/* Enable Dyanmic mode for SCLK */

	sclk_cntl = INPLL( pllSCLK_CNTL_M6);	
	sclk_cntl &= SCLK_CNTL_M6__SCLK_SRC_SEL_MASK;
	sclk_cntl |= SCLK_CNTL_M6__FORCE_VIP;		

	OUTPLL( pllSCLK_CNTL_M6, sclk_cntl);


	sclk_more_cntl = INPLL(pllSCLK_MORE_CNTL);
	sclk_more_cntl &= ~(SCLK_MORE_CNTL__FORCE_DISPREGS);
				                    
	OUTPLL(pllSCLK_MORE_CNTL, sclk_more_cntl);

	
	/* Enable Dynamic mode for PIXCLK & PIX2CLK */

	pixclks_cntl = INPLL( pllPIXCLKS_CNTL);
	
	pixclks_cntl|=  PIXCLKS_CNTL__PIX2CLK_ALWAYS_ONb | 
			PIXCLKS_CNTL__PIX2CLK_DAC_ALWAYS_ONb|
			PIXCLKS_CNTL__PIXCLK_BLEND_ALWAYS_ONb|
			PIXCLKS_CNTL__PIXCLK_GV_ALWAYS_ONb|
			PIXCLKS_CNTL__PIXCLK_DIG_TMDS_ALWAYS_ONb|
			PIXCLKS_CNTL__PIXCLK_LVDS_ALWAYS_ONb|
			PIXCLKS_CNTL__PIXCLK_TMDS_ALWAYS_ONb;

	OUTPLL( pllPIXCLKS_CNTL, pixclks_cntl);
		
		
	vclk_ecp_cntl = INPLL( pllVCLK_ECP_CNTL);
	
	vclk_ecp_cntl|=  VCLK_ECP_CNTL__PIXCLK_ALWAYS_ONb | 
			 VCLK_ECP_CNTL__PIXCLK_DAC_ALWAYS_ONb;

	OUTPLL( pllVCLK_ECP_CNTL, vclk_ecp_cntl);


	/* Enable Dynamic mode for MCLK	*/
	mclk_cntl = INPLL( pllMCLK_CNTL_M6);
	mclk_cntl &= ~(	MCLK_CNTL_M6__FORCE_MCLKA |  
			MCLK_CNTL_M6__FORCE_MCLKB |
			MCLK_CNTL_M6__FORCE_YCLKA |
			MCLK_CNTL_M6__FORCE_YCLKB );
    	OUTPLL( pllMCLK_CNTL_M6, mclk_cntl);

	mclk_misc = INPLL(pllMCLK_MISC);
	mclk_misc |= 	MCLK_MISC__MC_MCLK_MAX_DYN_STOP_LAT|
			MCLK_MISC__IO_MCLK_MAX_DYN_STOP_LAT|
			MCLK_MISC__MC_MCLK_DYN_ENABLE|
			MCLK_MISC__IO_MCLK_DYN_ENABLE;	
	
	OUTPLL(pllMCLK_MISC, mclk_misc);
}


static void radeon_pm_yclk_mclk_sync(struct radeonfb_info *rinfo)
{
	u32 mc_chp_io_cntl_a1, mc_chp_io_cntl_b1;

	mc_chp_io_cntl_a1 = INMC( rinfo, ixMC_CHP_IO_CNTL_A1) & ~MC_CHP_IO_CNTL_A1__MEM_SYNC_ENA_MASK;
	mc_chp_io_cntl_b1 = INMC( rinfo, ixMC_CHP_IO_CNTL_B1) & ~MC_CHP_IO_CNTL_B1__MEM_SYNC_ENB_MASK;

	OUTMC( rinfo, ixMC_CHP_IO_CNTL_A1, mc_chp_io_cntl_a1 | (1<<MC_CHP_IO_CNTL_A1__MEM_SYNC_ENA__SHIFT));
	OUTMC( rinfo, ixMC_CHP_IO_CNTL_B1, mc_chp_io_cntl_b1 | (1<<MC_CHP_IO_CNTL_B1__MEM_SYNC_ENB__SHIFT));

	/* Wassup ? This doesn't seem to be defined, let's hope we are ok this way --BenH */
#ifdef MCLK_YCLK_SYNC_ENABLE
	mc_chp_io_cntl_a1 |= (2<<MC_CHP_IO_CNTL_A1__MEM_SYNC_ENA__SHIFT);
	mc_chp_io_cntl_b1 |= (2<<MC_CHP_IO_CNTL_B1__MEM_SYNC_ENB__SHIFT);
#endif

	OUTMC( rinfo, ixMC_CHP_IO_CNTL_A1, mc_chp_io_cntl_a1);
	OUTMC( rinfo, ixMC_CHP_IO_CNTL_B1, mc_chp_io_cntl_b1);

	mdelay( 1);
}

static void radeon_pm_program_mode_reg(struct radeonfb_info *rinfo, u16 value, u8 delay_required)
{  
	u32 mem_sdram_mode;

	mem_sdram_mode  = INREG( MEM_SDRAM_MODE_REG);

	mem_sdram_mode &= ~MEM_SDRAM_MODE_REG__MEM_MODE_REG_MASK;
	mem_sdram_mode |= (value<<MEM_SDRAM_MODE_REG__MEM_MODE_REG__SHIFT) | MEM_SDRAM_MODE_REG__MEM_CFG_TYPE;
	OUTREG( MEM_SDRAM_MODE_REG, mem_sdram_mode);

	mem_sdram_mode |=  MEM_SDRAM_MODE_REG__MEM_SDRAM_RESET;
	OUTREG( MEM_SDRAM_MODE_REG, mem_sdram_mode);

	mem_sdram_mode &= ~MEM_SDRAM_MODE_REG__MEM_SDRAM_RESET;
	OUTREG( MEM_SDRAM_MODE_REG, mem_sdram_mode);

	if (delay_required == 1)
		while( (INREG( MC_STATUS) & (MC_STATUS__MEM_PWRUP_COMPL_A | MC_STATUS__MEM_PWRUP_COMPL_B) ) == 0 )
			{ }; 	
}


static void radeon_pm_enable_dll(struct radeonfb_info *rinfo)
{  
#define DLL_RESET_DELAY 	5
#define DLL_SLEEP_DELAY		1

	u32 DLL_CKO_Value = INPLL(pllMDLL_CKO)   | MDLL_CKO__MCKOA_SLEEP |  MDLL_CKO__MCKOA_RESET;
	u32 DLL_CKA_Value = INPLL(pllMDLL_RDCKA) | MDLL_RDCKA__MRDCKA0_SLEEP | MDLL_RDCKA__MRDCKA1_SLEEP | MDLL_RDCKA__MRDCKA0_RESET | MDLL_RDCKA__MRDCKA1_RESET;
	u32 DLL_CKB_Value = INPLL(pllMDLL_RDCKB) | MDLL_RDCKB__MRDCKB0_SLEEP | MDLL_RDCKB__MRDCKB1_SLEEP | MDLL_RDCKB__MRDCKB0_RESET | MDLL_RDCKB__MRDCKB1_RESET;

	/* Setting up the DLL range for write */
	OUTPLL(pllMDLL_CKO,   	DLL_CKO_Value);
	OUTPLL(pllMDLL_RDCKA,  	DLL_CKA_Value);
	OUTPLL(pllMDLL_RDCKB,	DLL_CKB_Value);

	mdelay( DLL_RESET_DELAY);

	/* Channel A */

	/* Power Up */
	DLL_CKO_Value &= ~(MDLL_CKO__MCKOA_SLEEP );
	OUTPLL(pllMDLL_CKO,   	DLL_CKO_Value);
	mdelay( DLL_SLEEP_DELAY);  		
   
	DLL_CKO_Value &= ~(MDLL_CKO__MCKOA_RESET );
	OUTPLL(pllMDLL_CKO,	DLL_CKO_Value);
	mdelay( DLL_RESET_DELAY);  		

	/* Power Up */
	DLL_CKA_Value &= ~(MDLL_RDCKA__MRDCKA0_SLEEP );
	OUTPLL(pllMDLL_RDCKA,  	DLL_CKA_Value);
	mdelay( DLL_SLEEP_DELAY);  		

	DLL_CKA_Value &= ~(MDLL_RDCKA__MRDCKA0_RESET );
	OUTPLL(pllMDLL_RDCKA,	DLL_CKA_Value);
	mdelay( DLL_RESET_DELAY);  		

	/* Power Up */
	DLL_CKA_Value &= ~(MDLL_RDCKA__MRDCKA1_SLEEP);
	OUTPLL(pllMDLL_RDCKA,	DLL_CKA_Value);
	mdelay( DLL_SLEEP_DELAY);  		

	DLL_CKA_Value &= ~(MDLL_RDCKA__MRDCKA1_RESET);
	OUTPLL(pllMDLL_RDCKA,	DLL_CKA_Value);
	mdelay( DLL_RESET_DELAY);  		


	/* Channel B */

	/* Power Up */
	DLL_CKO_Value &= ~(MDLL_CKO__MCKOB_SLEEP );
	OUTPLL(pllMDLL_CKO,   	DLL_CKO_Value);
	mdelay( DLL_SLEEP_DELAY);  		
   
	DLL_CKO_Value &= ~(MDLL_CKO__MCKOB_RESET );
	OUTPLL(pllMDLL_CKO,   	DLL_CKO_Value);
	mdelay( DLL_RESET_DELAY);  		

	/* Power Up */
	DLL_CKB_Value &= ~(MDLL_RDCKB__MRDCKB0_SLEEP);
	OUTPLL(pllMDLL_RDCKB,   DLL_CKB_Value);
	mdelay( DLL_SLEEP_DELAY);  		

	DLL_CKB_Value &= ~(MDLL_RDCKB__MRDCKB0_RESET);
	OUTPLL(pllMDLL_RDCKB,   DLL_CKB_Value);
	mdelay( DLL_RESET_DELAY);  		

	/* Power Up */
	DLL_CKB_Value &= ~(MDLL_RDCKB__MRDCKB1_SLEEP);
	OUTPLL(pllMDLL_RDCKB,   DLL_CKB_Value);
	mdelay( DLL_SLEEP_DELAY);  		

	DLL_CKB_Value &= ~(MDLL_RDCKB__MRDCKB1_RESET);
	OUTPLL(pllMDLL_RDCKB,   DLL_CKB_Value);
	mdelay( DLL_RESET_DELAY);  		

#undef DLL_RESET_DELAY 
#undef DLL_SLEEP_DELAY
}

static void radeon_pm_full_reset_sdram(struct radeonfb_info *rinfo)
{
	u32 crtcGenCntl, crtcGenCntl2, memRefreshCntl, crtc_more_cntl, fp_gen_cntl, fp2_gen_cntl;
 
	crtcGenCntl  = INREG( CRTC_GEN_CNTL);
	crtcGenCntl2 = INREG( CRTC2_GEN_CNTL);

	memRefreshCntl 	= INREG( MEM_REFRESH_CNTL);
	crtc_more_cntl 	= INREG( CRTC_MORE_CNTL);
	fp_gen_cntl 	= INREG( FP_GEN_CNTL);
	fp2_gen_cntl 	= INREG( FP2_GEN_CNTL);
 

	OUTREG( CRTC_MORE_CNTL, 	0);
	OUTREG( FP_GEN_CNTL, 	0);
	OUTREG( FP2_GEN_CNTL, 	0);
 
	OUTREG( CRTC_GEN_CNTL,  (crtcGenCntl | CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B) );
	OUTREG( CRTC2_GEN_CNTL, (crtcGenCntl2 | CRTC2_GEN_CNTL__CRTC2_DISP_REQ_EN_B) );
  
	/* Disable refresh */
	OUTREG( MEM_REFRESH_CNTL, memRefreshCntl | MEM_REFRESH_CNTL__MEM_REFRESH_DIS);
 
	/* Reset memory */
	OUTREG( MEM_SDRAM_MODE_REG,
		INREG( MEM_SDRAM_MODE_REG) & ~MEM_SDRAM_MODE_REG__MC_INIT_COMPLETE); // Init  Not Complete

	/* DLL */
	radeon_pm_enable_dll(rinfo);

	// MLCK /YCLK sync 
	radeon_pm_yclk_mclk_sync(rinfo);

	if ((rinfo->arch == RADEON_M6) || (rinfo->arch == RADEON_M7) || (rinfo->arch == RADEON_M9)) {
		radeon_pm_program_mode_reg(rinfo, 0x2000, 1);   
		radeon_pm_program_mode_reg(rinfo, 0x2001, 1);   
		radeon_pm_program_mode_reg(rinfo, 0x2002, 1);   
		radeon_pm_program_mode_reg(rinfo, 0x0132, 1);   
		radeon_pm_program_mode_reg(rinfo, 0x0032, 1); 
	}	

	OUTREG( MEM_SDRAM_MODE_REG,
		INREG( MEM_SDRAM_MODE_REG) |  MEM_SDRAM_MODE_REG__MC_INIT_COMPLETE); // Init Complete

	OUTREG( MEM_REFRESH_CNTL, 	memRefreshCntl);

	OUTREG( CRTC_GEN_CNTL, 		crtcGenCntl);
	OUTREG( CRTC2_GEN_CNTL, 	crtcGenCntl2);
	OUTREG( FP_GEN_CNTL, 		fp_gen_cntl);
	OUTREG( FP2_GEN_CNTL, 		fp2_gen_cntl);

	OUTREG( CRTC_MORE_CNTL, 	crtc_more_cntl);

	mdelay( 15);
}


static void radeon_set_suspend(struct radeonfb_info *rinfo, int suspend)
{
	u16 pwr_cmd;

	if (!rinfo->pm_reg)
		return;

	/* Set the chip into appropriate suspend mode (we use D2,
	 * D3 would require a compete re-initialization of the chip,
	 * including PCI config registers, clocks, AGP conf, ...)
	 */
	if (suspend) {
		/* Disable dynamic power management of clocks for the
		 * duration of the suspend/resume process
		 */
		radeon_pm_disable_dynamic_mode(rinfo);
		/* Save some registers */
		radeon_pm_save_regs(rinfo);

		/* Prepare mobility chips for suspend
		 */
		if (rinfo->arch == RADEON_M6 || rinfo->arch == RADEON_M7 || rinfo->arch == RADEON_M9) {
			/* Program V2CLK */
			radeon_pm_program_v2clk(rinfo);
		
			/* Disable IO PADs */
			radeon_pm_disable_iopad(rinfo);

			/* Set low current */
			radeon_pm_low_current(rinfo);

			/* Prepare chip for power management */
			radeon_pm_setup_for_suspend(rinfo);

			/* Reset the MDLL */
			OUTPLL( pllMDLL_CKO, INPLL( pllMDLL_CKO) | MDLL_CKO__MCKOA_RESET | MDLL_CKO__MCKOB_RESET);
		}

		/* Switch PCI power managment to D2. */
		for (;;) {
			pci_read_config_word(
				rinfo->pdev, rinfo->pm_reg+PCI_PM_CTRL,
				&pwr_cmd);
			if (pwr_cmd & 2)
				break;			
			pci_write_config_word(
				rinfo->pdev, rinfo->pm_reg+PCI_PM_CTRL,
				(pwr_cmd & ~PCI_PM_CTRL_STATE_MASK) | 2);
			mdelay(500);
		}
	} else {
		/* Switch back PCI powermanagment to D0 */
		mdelay(200);
		pci_write_config_word(rinfo->pdev, rinfo->pm_reg+PCI_PM_CTRL, 0);
		mdelay(500);

		/* Reset the SDRAM controller */
		if (rinfo->arch == RADEON_M6 || rinfo->arch == RADEON_M7 || rinfo->arch == RADEON_M9)
			radeon_pm_full_reset_sdram(rinfo);
		
		/* Restore some registers */
		radeon_pm_restore_regs(rinfo);
		radeon_pm_enable_dynamic_mode(rinfo);
		mdelay(10);
	}
}

/*
 * Save the contents of the framebuffer when we go to sleep,
 * and restore it when we wake up again.
 */

static int radeon_sleep_notify(struct pmu_sleep_notifier *self, int when)
{
	struct radeonfb_info *rinfo;

	for (rinfo = board_list; rinfo != NULL; rinfo = rinfo->next) {
		struct fb_fix_screeninfo fix;
		int nb;
	        struct display *disp;  

        	disp = (rinfo->currcon < 0) ? rinfo->info.disp : &fb_display[rinfo->currcon];

		switch (rinfo->arch) {
			case RADEON_M6:
			case RADEON_M7:
			case RADEON_M9:
				break;
			default:
				return PBOOK_SLEEP_REFUSE;
		}

		radeonfb_get_fix(&fix, fg_console, (struct fb_info *)rinfo);
		nb = fb_display[fg_console].var.yres * fix.line_length;

		switch (when) {
			case PBOOK_SLEEP_NOW:
				acquire_console_sem();
				disp->dispsw = &fbcon_dummy;

				if (!noaccel) {
					/* Make sure engine is idle and reset */
					radeon_engine_idle();
					radeon_engine_reset();
					radeon_engine_idle();
				}

				/* Blank display and LCD */
				radeonfb_blank(VESA_POWERDOWN+1,
					       (struct fb_info *)rinfo);

				/* Sleep */
				rinfo->asleep = 1;
				radeon_set_suspend(rinfo, 1);
				release_console_sem();
				
				break;
			case PBOOK_WAKE:
				acquire_console_sem();
				/* Wakeup chip*/
				radeon_set_suspend(rinfo, 0);

				/* Re-set mode */
				rinfo->asleep = 0;
				radeon_load_video_mode(rinfo, &disp->var);
				if ((disp->var.accel_flags & FB_ACCELF_TEXT) && !noaccel)
					radeon_engine_init(rinfo);
				do_install_cmap(rinfo->currcon < 0 ? 0 : rinfo->currcon,
						(struct fb_info *)rinfo);
				/* Allow fbdev to tap us again */
				radeon_set_dispsw(rinfo, disp);

				/* Unblank screen */
				radeonfb_blank(0, (struct fb_info *)rinfo);
				release_console_sem();
				break;
		}
	}

	return PBOOK_SLEEP_OK;
}

#endif /* CONFIG_PMAC_PBOOK */

/*
 * text console acceleration
 */


static void fbcon_radeon_bmove(struct display *p, int srcy, int srcx,
			       int dsty, int dstx, int height, int width)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)(p->fb_info);
	u32 dp_cntl = DST_LAST_PEL;
	u32 dp_cntl_save = 0;

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	dstx *= fontwidth(p);
	dsty *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	if (srcy < dsty) {
		srcy += height;
		dsty += height;
	} else
		dp_cntl |= DST_Y_TOP_TO_BOTTOM;

	if (srcx < dstx) {
		srcx += width;
		dstx += width;
	} else
		dp_cntl |= DST_X_LEFT_TO_RIGHT;

	dp_cntl_save = INREG(DP_CNTL);

	radeon_fifo_wait(6);
	OUTREG(DP_GUI_MASTER_CNTL, (rinfo->dp_gui_master_cntl |
				    GMC_BRUSH_NONE |
				    GMC_SRC_DATATYPE_COLOR |
				    ROP3_S |
				    DP_SRC_SOURCE_MEMORY));
	OUTREG(DP_WRITE_MSK, 0xffffffff);
	OUTREG(DP_CNTL, dp_cntl);
	OUTREG(SRC_Y_X, (srcy << 16) | srcx);
	OUTREG(DST_Y_X, (dsty << 16) | dstx);
	OUTREG(DST_HEIGHT_WIDTH, (height << 16) | width);

	radeon_fifo_wait(1);
	OUTREG(DP_CNTL, dp_cntl_save);

	radeon_engine_idle();
}



static void radeon_rectfill(struct radeonfb_info *rinfo,
			    int dsty, int dstx,
			    int height, int width,
			    u32 clr)
{
	radeon_fifo_wait(6);
	OUTREG(DP_GUI_MASTER_CNTL, (rinfo->dp_gui_master_cntl |
				    GMC_BRUSH_SOLID_COLOR |
				    GMC_SRC_DATATYPE_COLOR |
				    ROP3_P));
	OUTREG(DP_BRUSH_FRGD_CLR, clr);
	OUTREG(DP_WRITE_MSK, 0xffffffff);
	OUTREG(DP_CNTL, (DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM));
	OUTREG(DST_Y_X, (dsty << 16) | dstx);
	OUTREG(DST_WIDTH_HEIGHT, (width << 16) | height);

	radeon_engine_idle();
}



#ifdef FBCON_HAS_CFB8

static void fbcon_radeon8_clear(struct vc_data *conp, struct display *p,
			       int srcy, int srcx, int height, int width)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)(p->fb_info);
	u32 clr;

	clr = attr_bgcol_ec(p, conp);
	clr |= (clr << 8);
	clr |= (clr << 16);

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	radeon_rectfill(rinfo, srcy, srcx, height, width, clr);
}



static struct display_switch fbcon_radeon8 = {
	setup:			fbcon_cfb8_setup,
	bmove:			fbcon_radeon_bmove,
	clear:			fbcon_radeon8_clear,
	putc:			fbcon_cfb8_putc,
	putcs:			fbcon_cfb8_putcs,
	revc:			fbcon_cfb8_revc,
	clear_margins:		fbcon_cfb8_clear_margins,
	fontwidthmask:		FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif

#ifdef FBCON_HAS_CFB16


static void fbcon_radeon16_clear(struct vc_data *conp, struct display *p,
			       int srcy, int srcx, int height, int width)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)(p->fb_info);
	u32 clr;

	clr = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	clr |= (clr << 16);

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	radeon_rectfill(rinfo, srcy, srcx, height, width, clr);
}



static struct display_switch fbcon_radeon16 = {
	setup:			fbcon_cfb16_setup,
	bmove:			fbcon_radeon_bmove,
	clear:			fbcon_radeon16_clear,
	putc:			fbcon_cfb16_putc,
	putcs:			fbcon_cfb16_putcs,
	revc:			fbcon_cfb16_revc,
	clear_margins:		fbcon_cfb16_clear_margins,
	fontwidthmask:		FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif

#ifdef FBCON_HAS_CFB32


static void fbcon_radeon32_clear(struct vc_data *conp, struct display *p,
			       int srcy, int srcx, int height, int width)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)(p->fb_info);
	u32 clr;

	clr = ((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	radeon_rectfill(rinfo, srcy, srcx, height, width, clr);
}



static struct display_switch fbcon_radeon32 = {
	setup:			fbcon_cfb32_setup,
	bmove:			fbcon_radeon_bmove,
	clear:			fbcon_radeon32_clear,
	putc:			fbcon_cfb32_putc,
	putcs:			fbcon_cfb32_putcs,
	revc:			fbcon_cfb32_revc,
	clear_margins:		fbcon_cfb32_clear_margins,
	fontwidthmask:		FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif

