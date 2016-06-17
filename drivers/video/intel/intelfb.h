#ifndef _INTELFB_H
#define _INTELFB_H

/* $DHD: intelfb/intelfb.h,v 1.38 2003/02/06 17:29:41 dawes Exp $ */
/* $TG$ */


/*** Version/name ***/
#define INTELFB_VERSION			"0.7.7"
#define INTELFB_MODULE_NAME		"intelfb"
#define SUPPORTED_CHIPSETS		"830M/845G/852GM/855GM/865G"


/*** Debug/feature defines ***/

#ifndef DEBUG
#define DEBUG				0
#endif

#ifndef VERBOSE
#define VERBOSE				1
#endif

#ifndef REGDUMP
#define REGDUMP				0
#endif

#ifndef DETECT_VGA_CLASS_ONLY
#define DETECT_VGA_CLASS_ONLY		1
#endif

#ifndef FIXED_MODE
#define FIXED_MODE			0
#endif

#ifndef ALLOCATE_FOR_PANNING
#define ALLOCATE_FOR_PANNING		0
#endif

#ifndef BAILOUT_EARLY
#define BAILOUT_EARLY			0
#endif

#ifndef TEST_MODE_TO_HW
#define TEST_MODE_TO_HW			0
#endif

#ifndef USE_SYNC_PAGE
#define USE_SYNC_PAGE			1
#endif

#ifndef MOBILE_HW_CURSOR
#define MOBILE_HW_CURSOR		0
#endif

#ifndef PREFERRED_MODE
#define PREFERRED_MODE			"1024x768-16@60"
#endif

/*** hw-related values ***/

/* PCI ids for supported devices */
#define PCI_DEVICE_ID_INTEL_830M	0x3577
#define PCI_DEVICE_ID_INTEL_845G	0x2562
#define PCI_DEVICE_ID_INTEL_85XGM	0x3582
#define PCI_DEVICE_ID_INTEL_865G	0x2572

/* Size of MMIO region */
#define INTEL_REG_SIZE			0x80000

#define STRIDE_ALIGNMENT		16

#define PAT_ROP_GXCOPY                  0xf0
#define PAT_ROP_GXXOR                   0x5a

#define PALETTE_8_ENTRIES		256


/*** Macros ***/

/* basic arithmetic */
#define KB(x)			((x) * 1024)
#define MB(x)			((x) * 1024 * 1024)
#define BtoKB(x)		((x) / 1024)
#define BtoMB(x)		((x) / 1024 / 1024)

#define ROUND_UP_TO(x, y)	(((x) + (y) - 1) / (y) * (y))
#define ROUND_DOWN_TO(x, y)	((x) / (y) * (y))
#define ROUND_UP_TO_PAGE(x)	ROUND_UP_TO((x), PAGE_SIZE)
#define ROUND_DOWN_TO_PAGE(x)	ROUND_DOWN_TO((x), PAGE_SIZE)

/* messages */
#define PFX			INTELFB_MODULE_NAME ": "

#define ERR_MSG(fmt, args...)	printk(KERN_ERR PFX fmt, ## args)
#define WRN_MSG(fmt, args...)	printk(KERN_WARNING PFX fmt, ## args)
#define NOT_MSG(fmt, args...)	printk(KERN_NOTICE PFX fmt, ## args)
#define INF_MSG(fmt, args...)	printk(KERN_INFO PFX fmt, ## args)
#if DEBUG
#define DBG_MSG(fmt, args...)	printk(KERN_DEBUG PFX fmt, ## args)
#else
#define DBG_MSG(fmt, args...)	while (0) printk(fmt, ## args)
#endif

/* get commonly used pointers */
#define GET_DINFO(info)		(struct intelfb_info *)(info)
#define GET_DISP(info, con)	((con) < 0) ? (info)->disp : &fb_display[con]

/* module parameters */
#define INTELFB_INT_PARAM(name, default, desc)				\
	static int name = default;					\
	MODULE_PARM(name, "i");						\
	MODULE_PARM_DESC(name, desc);

#define INTELFB_STR_PARAM(name, default, desc)				\
	static const char *name = default;				\
	MODULE_PARM(name, "s");						\
	MODULE_PARM_DESC(name, desc);

/* misc macros */
#define TEXT_ACCEL(d, v)						\
	((d)->accel && (d)->ring_active &&				\
	 ((v)->accel_flags & FB_ACCELF_TEXT))

#define USE_DRAWGLYPH(d)						\
	((d)->chipset == INTEL_865G)

#define NOACCEL_CHIPSET(d)						\
	((d)->chipset != INTEL_865G)

#ifndef LockPage
#define LockPage(page)		set_bit(PG_locked, &(page)->flags)
#endif
#ifndef UnlockPage
#define UnlockPage(page)	unlock_page(page)
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#define FB_SET_CMAP(a, b, c, d) fb_set_cmap(a, b, d)
#else
#define FB_SET_CMAP(a, b, c, d) fb_set_cmap(a, b, c, d)
#endif


/*** Driver paramters ***/

#define RINGBUFFER_SIZE		KB(64)
#define HW_CURSOR_SIZE		KB(4)


/*** Data Types ***/

/* supported chipsets */
enum intel_chips {
	INTEL_830M,
	INTEL_845G,
	INTEL_85XGM,
	INTEL_852GM,
	INTEL_852GME,
	INTEL_855GM,
	INTEL_855GME,
	INTEL_865G
};

struct intelfb_hwstate {
	u32 vga0_divisor;
	u32 vga1_divisor;
	u32 vga_pd;
	u32 dpll_a;
	u32 dpll_b;
	u32 fpa0;
	u32 fpa1;
	u32 fpb0;
	u32 fpb1;
	u32 palette_a[PALETTE_8_ENTRIES];
	u32 palette_b[PALETTE_8_ENTRIES];
	u32 htotal_a;
	u32 hblank_a;
	u32 hsync_a;
	u32 vtotal_a;
	u32 vblank_a;
	u32 vsync_a;
	u32 src_size_a;
	u32 bclrpat_a;
	u32 htotal_b;
	u32 hblank_b;
	u32 hsync_b;
	u32 vtotal_b;
	u32 vblank_b;
	u32 vsync_b;
	u32 src_size_b;
	u32 bclrpat_b;
	u32 adpa;
	u32 dvoa;
	u32 dvob;
	u32 dvoc;
	u32 dvoa_srcdim;
	u32 dvob_srcdim;
	u32 dvoc_srcdim;
	u32 lvds;
	u32 pipe_a_conf;
	u32 pipe_b_conf;
	u32 disp_arb;
	u32 cursor_a_control;
	u32 cursor_b_control;
	u32 cursor_a_base;
	u32 cursor_b_base;
	u32 cursor_size;
	u32 disp_a_ctrl;
	u32 disp_b_ctrl;
	u32 disp_a_base;
	u32 disp_b_base;
	u32 cursor_a_palette[4];
	u32 cursor_b_palette[4];
	u32 disp_a_stride;
	u32 disp_b_stride;
	u32 vgacntrl;
	u32 add_id;
	u32 swf0x[7];
	u32 swf1x[7];
	u32 swf3x[3];
	u32 fence[8];
	u32 instpm;
	u32 mem_mode;
	u32 fw_blc_0;
	u32 fw_blc_1;
};

struct intelfb_info {
	struct fb_info info;

	const char *name;

	u32 fb_base_phys;
	u32 mmio_base_phys;

	u32 fb_base;
	u32 mmio_base;

	u32 fb_offset;

	struct pci_dev *pdev;

	/* Ring buffer */
	u32 ring_base_phys;
	u32 ring_base;
	u32 ring_size;
	u32 ring_head;
	u32 ring_tail;
	u32 ring_tail_mask;
	u32 ring_space;

	/* HW cursor */
	u32 cursor_base_phys;
	u32 cursor_offset;
	u32 cursor_base;
	u32 cursor_size;
	u32 cursor_page_virt;
	u32 cursor_base_real;

#if USE_SYNC_PAGE
	/* 2D synchronisation */
	u32 syncpage_virt;
	u32 syncpage_phys;
#endif

	struct display disp;
	int currcon;
	struct display *currcon_display;

	struct { u8 red, green, blue, pad; } palette[256];

	int pci_chipset;
	int chipset;
	int mobile;

	int video_ram;
	int aperture_size;
	int stolen_size;
	int bpp, depth;
	u32 visual;

	int xres, yres, pitch;
	int pixclock;

	int pipe;

	int accel;
	int hwcursor;
	int fixed_mode;

	int ring_active;

#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB32)
	union {
#if defined(FBCON_HAS_CFB16)
		u_int16_t cfb16[16];
#endif
#if defined(FBCON_HAS_CFB32)
		u_int32_t cfb32[16];
#endif
	} con_cmap;
#endif

	int vc_mode;

	struct intelfb_hwstate save_state;

	int registered;

	int initial_vga;
	struct fb_var_screeninfo initial_var;
	u32 initial_fb_base;
	u32 initial_video_ram;
	u32 initial_pitch;

	struct {
		int type;
		int state;
		int on;
		int enabled;
		int blanked;
		int w, u, d;
		int x, y, redraw;
		unsigned long cursorimage;
		struct timer_list timer;
	} cursor;
	spinlock_t DAClock;
};


/*** Functions ***/

/* intelfb.c */
extern int intelfb_var_to_depth(const struct fb_var_screeninfo *var);
extern void intelfb_create_cursor_shape(struct intelfb_info *dinfo,
					struct display *disp);

/* intelfbhw.c */

extern int intelfbhw_get_chipset(struct pci_dev *pdev, const char **name,
				 int *chipset, int *mobile);
extern int intelfbhw_get_memory(struct pci_dev *pdev, int *aperture_size,
				int *stolen_size);
extern const char *intelfbhw_check_non_crt(struct intelfb_info *dinfo);
extern int intelfbhw_validate_mode(struct intelfb_info *dinfo, int con,
				   struct fb_var_screeninfo *var);
extern int intelfbhw_pan_display(struct fb_var_screeninfo *var, int con,
				 struct fb_info *info);
extern void intelfbhw_do_blank(int blank, struct fb_info *info);
extern void intelfbhw_setcolreg(struct intelfb_info *dinfo, unsigned regno,
				unsigned red, unsigned green, unsigned blue,
				unsigned transp);
extern int intelfbhw_read_hw_state(struct intelfb_info *dinfo,
				   struct intelfb_hwstate *hw, int flag);
extern void intelfbhw_print_hw_state(struct intelfb_info *dinfo,
				     struct intelfb_hwstate *hw);
extern int intelfbhw_mode_to_hw(struct intelfb_info *dinfo,
				struct intelfb_hwstate *hw,
				struct fb_var_screeninfo *var);
extern int intelfbhw_program_mode(struct intelfb_info *dinfo,
				  const struct intelfb_hwstate *hw, int blank);
extern void intelfbhw_do_sync(struct intelfb_info *dinfo);
extern void intelfbhw_2d_stop(struct intelfb_info *dinfo);
extern void intelfbhw_2d_start(struct intelfb_info *dinfo);
extern void intelfbhw_do_fillrect(struct intelfb_info *dinfo, u32 x, u32 y,
				  u32 w, u32 h, u32 color, u32 pitch, u32 bpp,
				  u32 rop);
extern void intelfbhw_do_bitblt(struct intelfb_info *dinfo, u32 curx, u32 cury,
				u32 dstx, u32 dsty, u32 w, u32 h, u32 pitch,
				u32 bpp);
extern int intelfbhw_do_drawglyph(struct intelfb_info *dinfo, u32 fg, u32 bg,
				  u32 w, u32 h, u8* cdat, u32 x, u32 y,
				  u32 pitch, u32 bpp);
extern void intelfbhw_cursor_init(struct intelfb_info *dinfo);
extern void intelfbhw_cursor_hide(struct intelfb_info *dinfo);
extern void intelfbhw_cursor_show(struct intelfb_info *dinfo);
extern void intelfbhw_cursor_setpos(struct intelfb_info *dinfo, int x, int y);
extern void intelfbhw_cursor_setcolor(struct intelfb_info *dinfo, u32 bg,
				      u32 fg);
extern void intelfbhw_cursor_load(struct intelfb_info *dinfo,
				  struct display *disp);


#endif /* _INTELFB_H */
