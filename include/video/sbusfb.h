#include <linux/timer.h>
#include <asm/sbus.h>
#include <asm/oplib.h>
#include <asm/fbio.h>

#include <video/fbcon.h>

struct bt_regs {
	volatile unsigned int addr;           /* address register */
	volatile unsigned int color_map;      /* color map */
	volatile unsigned int control;        /* control register */
	volatile unsigned int cursor;         /* cursor map register */
};

struct fb_info_creator {
	struct ffb_fbc *fbc;
	struct ffb_dac *dac;
	int xy_margin;
	int fifo_cache;
	u64 yx_margin;
	int fg_cache;
	int bg_cache;
	int dac_rev;
};
struct fb_info_cgsix {
	struct bt_regs *bt;
	struct cg6_fbc *fbc;
	struct cg6_thc *thc;
	struct cg6_tec *tec;
	volatile u32 *fhc;
};
struct fb_info_bwtwo {
	struct bw2_regs *regs;
};
struct fb_info_cgthree {
	struct cg3_regs *regs;
};
struct fb_info_tcx {
	struct bt_regs *bt;
	struct tcx_thc *thc;
	struct tcx_tec *tec;
	u32 *cplane;
};
struct fb_info_leo {
	struct leo_lx_krn *lx_krn;
	struct leo_lc_ss0_usr *lc_ss0_usr;
	struct leo_ld_ss0 *ld_ss0;
	struct leo_ld_ss1 *ld_ss1;
	struct leo_cursor *cursor;
	unsigned int extent;
};
struct fb_info_cgfourteen {
	struct cg14_regs *regs;
	struct cg14_cursor *cursor;
	struct cg14_clut *clut;
	int ramsize;
	int mode;
};
struct fb_info_p9100 {
	struct p9100_ctrl *ctrl;
	volatile u32 *fbmem;
};

struct cg_cursor {
	char	enable;         /* cursor is enabled */
	char	mode;		/* cursor mode */
	struct	fbcurpos cpos;  /* position */
	struct	fbcurpos chot;  /* hot-spot */
	struct	fbcurpos size;  /* size of mask & image fields */
	struct	fbcurpos hwsize; /* hw max size */
	int	bits[2][128];   /* space for mask & image bits */
	char	color [6];      /* cursor colors */
	struct	timer_list timer; /* cursor timer */
	int	blink_rate;	/* cursor blink rate */
};

struct sbus_mmap_map {
	unsigned long voff;
	unsigned long poff;
	unsigned long size;
};

#define SBUS_MMAP_FBSIZE(n) (-n)
#define SBUS_MMAP_EMPTY	0x80000000

struct fb_info_sbusfb {
	struct fb_info info;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct display disp;
	struct display_switch dispsw;
	struct fbtype type;
	struct sbus_dev *sbdp;
	spinlock_t lock;
	int prom_node, prom_parent;
	union {
		struct fb_info_creator ffb;
		struct fb_info_cgsix cg6;
		struct fb_info_bwtwo bw2;
		struct fb_info_cgthree cg3;
		struct fb_info_tcx tcx;
		struct fb_info_leo leo;
		struct fb_info_cgfourteen cg14;
		struct fb_info_p9100 p9100;
	} s;
	unsigned char *color_map;
	struct cg_cursor cursor;
	unsigned char open;
	unsigned char mmaped;
	unsigned char blanked;
	int x_margin;
	int y_margin;
	int vtconsole;
	int consolecnt;
	int graphmode;
	int emulations[4];
	struct sbus_mmap_map *mmap_map;
	unsigned long physbase;
	int iospace;
	/* Methods */
	void (*setup)(struct display *);
	void (*setcursor)(struct fb_info_sbusfb *);
	void (*setcurshape)(struct fb_info_sbusfb *);
	void (*setcursormap)(struct fb_info_sbusfb *, unsigned char *, unsigned char *, unsigned char *);
	void (*loadcmap)(struct fb_info_sbusfb *, struct display *, int, int);
	void (*blank)(struct fb_info_sbusfb *);
	void (*unblank)(struct fb_info_sbusfb *);
	void (*margins)(struct fb_info_sbusfb *, struct display *, int, int);
	void (*reset)(struct fb_info_sbusfb *);
	void (*fill)(struct fb_info_sbusfb *, struct display *, int, int, unsigned short *);
	void (*switch_from_graph)(struct fb_info_sbusfb *);
	void (*restore_palette)(struct fb_info_sbusfb *);
	int (*ioctl)(struct fb_info_sbusfb *, unsigned int, unsigned long);
};

extern char *creatorfb_init(struct fb_info_sbusfb *);
extern char *cgsixfb_init(struct fb_info_sbusfb *);
extern char *cgthreefb_init(struct fb_info_sbusfb *);
extern char *tcxfb_init(struct fb_info_sbusfb *);
extern char *leofb_init(struct fb_info_sbusfb *);
extern char *bwtwofb_init(struct fb_info_sbusfb *);
extern char *cgfourteenfb_init(struct fb_info_sbusfb *);
extern char *p9100fb_init(struct fb_info_sbusfb *);

#define sbusfbinfod(disp) ((struct fb_info_sbusfb *)(disp->fb_info))
#define sbusfbinfo(info) ((struct fb_info_sbusfb *)(info))
#define CM(i, j) [3*(i)+(j)]

#define SBUSFBINIT_SIZECHANGE ((char *)-1)
