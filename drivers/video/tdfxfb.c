/*
 *
 * tdfxfb.c
 *
 * Author: Hannu Mallat <hmallat@cc.hut.fi>
 *
 * Copyright © 1999 Hannu Mallat
 * All rights reserved
 *
 * Created      : Thu Sep 23 18:17:43 1999, hmallat
 * Last modified: Tue Nov  2 21:19:47 1999, hmallat
 *
 * Lots of the information here comes from the Daryll Strauss' Banshee 
 * patches to the XF86 server, and the rest comes from the 3dfx
 * Banshee specification. I'm very much indebted to Daryll for his
 * work on the X server.
 *
 * Voodoo3 support was contributed Harold Oga. Lots of additions
 * (proper acceleration, 24 bpp, hardware cursor) and bug fixes by Attila
 * Kesmarki. Thanks guys!
 * 
 * Voodoo1 and Voodoo2 support aren't relevant to this driver as they
 * behave very differently from the Voodoo3/4/5. For anyone wanting to
 * use frame buffer on the Voodoo1/2, see the sstfb driver (which is
 * located at http://www.sourceforge.net/projects/sstfb).
 *
 * While I _am_ grateful to 3Dfx for releasing the specs for Banshee,
 * I do wish the next version is a bit more complete. Without the XF86
 * patches I couldn't have gotten even this far... for instance, the
 * extensions to the VGA register set go completely unmentioned in the
 * spec! Also, lots of references are made to the 'SST core', but no
 * spec is publicly available, AFAIK.
 *
 * The structure of this driver comes pretty much from the Permedia
 * driver by Ilario Nardinocchi, which in turn is based on skeletonfb.
 * 
 * TODO:
 * - support for 16/32 bpp needs fixing (funky bootup penguin)
 * - multihead support (basically need to support an array of fb_infos)
 * - support other architectures (PPC, Alpha); does the fact that the VGA
 *   core can be accessed only thru I/O (not memory mapped) complicate
 *   things?
 *
 * Version history:
 *
 * 0.1.3 (released 1999-11-02) added Attila's panning support, code
 *			       reorg, hwcursor address page size alignment
 *                             (for mmaping both frame buffer and regs),
 *                             and my changes to get rid of hardcoded
 *                             VGA i/o register locations (uses PCI
 *                             configuration info now)
 * 0.1.2 (released 1999-10-19) added Attila Kesmarki's bug fixes and
 *                             improvements
 * 0.1.1 (released 1999-10-07) added Voodoo3 support by Harold Oga.
 * 0.1.0 (released 1999-10-06) initial version
 *
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
#include <linux/nvram.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>
#include <asm/io.h>
#include <linux/timer.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include <linux/spinlock.h>

#ifndef PCI_DEVICE_ID_3DFX_VOODOO5
#define PCI_DEVICE_ID_3DFX_VOODOO5	0x0009
#endif

/* membase0 register offsets */
#define STATUS		0x00
#define PCIINIT0	0x04
#define SIPMONITOR	0x08
#define LFBMEMORYCONFIG	0x0c
#define MISCINIT0	0x10
#define MISCINIT1	0x14
#define DRAMINIT0	0x18
#define DRAMINIT1	0x1c
#define AGPINIT		0x20
#define TMUGBEINIT	0x24
#define VGAINIT0	0x28
#define VGAINIT1	0x2c
#define DRAMCOMMAND	0x30
#define DRAMDATA	0x34
/* reserved             0x38 */
/* reserved             0x3c */
#define PLLCTRL0	0x40
#define PLLCTRL1	0x44
#define PLLCTRL2	0x48
#define DACMODE		0x4c
#define DACADDR		0x50
#define DACDATA		0x54
#define RGBMAXDELTA	0x58
#define VIDPROCCFG	0x5c
#define HWCURPATADDR	0x60
#define HWCURLOC	0x64
#define HWCURC0		0x68
#define HWCURC1		0x6c
#define VIDINFORMAT	0x70
#define VIDINSTATUS	0x74
#define VIDSERPARPORT	0x78
#define VIDINXDELTA	0x7c
#define VIDININITERR	0x80
#define VIDINYDELTA	0x84
#define VIDPIXBUFTHOLD	0x88
#define VIDCHRMIN	0x8c
#define VIDCHRMAX	0x90
#define VIDCURLIN	0x94
#define VIDSCREENSIZE	0x98
#define VIDOVRSTARTCRD	0x9c
#define VIDOVRENDCRD	0xa0
#define VIDOVRDUDX	0xa4
#define VIDOVRDUDXOFF	0xa8
#define VIDOVRDVDY	0xac
/*  ... */
#define VIDOVRDVDYOFF	0xe0
#define VIDDESKSTART	0xe4
#define VIDDESKSTRIDE	0xe8
#define VIDINADDR0	0xec
#define VIDINADDR1	0xf0
#define VIDINADDR2	0xf4
#define VIDINSTRIDE	0xf8
#define VIDCUROVRSTART	0xfc

#define INTCTRL		(0x00100000 + 0x04)
#define CLIP0MIN	(0x00100000 + 0x08)
#define CLIP0MAX	(0x00100000 + 0x0c)
#define DSTBASE		(0x00100000 + 0x10)
#define DSTFORMAT	(0x00100000 + 0x14)
#define SRCBASE		(0x00100000 + 0x34)
#define COMMANDEXTRA_2D	(0x00100000 + 0x38)
#define CLIP1MIN	(0x00100000 + 0x4c)
#define CLIP1MAX	(0x00100000 + 0x50)
#define SRCFORMAT	(0x00100000 + 0x54)
#define SRCSIZE		(0x00100000 + 0x58)
#define SRCXY		(0x00100000 + 0x5c)
#define COLORBACK	(0x00100000 + 0x60)
#define COLORFORE	(0x00100000 + 0x64)
#define DSTSIZE		(0x00100000 + 0x68)
#define DSTXY		(0x00100000 + 0x6c)
#define COMMAND_2D	(0x00100000 + 0x70)
#define LAUNCH_2D	(0x00100000 + 0x80)

#define COMMAND_3D	(0x00200000 + 0x120)

/* register bitfields (not all, only as needed) */

#define BIT(x) (1UL << (x))

/* COMMAND_2D reg. values */
#define ROP_COPY	0xcc     // src
#define ROP_INVERT      0x55     // NOT dst
#define ROP_XOR         0x66     // src XOR dst

#define AUTOINC_DSTX                    BIT(10)
#define AUTOINC_DSTY                    BIT(11)
#define COMMAND_2D_FILLRECT		0x05
#define COMMAND_2D_S2S_BITBLT		0x01      // screen to screen
#define COMMAND_2D_H2S_BITBLT           0x03       // host to screen


#define COMMAND_3D_NOP			0x00
#define STATUS_RETRACE			BIT(6)
#define STATUS_BUSY			BIT(9)
#define MISCINIT1_CLUT_INV		BIT(0)
#define MISCINIT1_2DBLOCK_DIS		BIT(15)
#define DRAMINIT0_SGRAM_NUM		BIT(26)
#define DRAMINIT0_SGRAM_TYPE		BIT(27)
#define DRAMINIT1_MEM_SDRAM		BIT(30)
#define VGAINIT0_VGA_DISABLE		BIT(0)
#define VGAINIT0_EXT_TIMING		BIT(1)
#define VGAINIT0_8BIT_DAC		BIT(2)
#define VGAINIT0_EXT_ENABLE		BIT(6)
#define VGAINIT0_WAKEUP_3C3		BIT(8)
#define VGAINIT0_LEGACY_DISABLE		BIT(9)
#define VGAINIT0_ALT_READBACK		BIT(10)
#define VGAINIT0_FAST_BLINK		BIT(11)
#define VGAINIT0_EXTSHIFTOUT		BIT(12)
#define VGAINIT0_DECODE_3C6		BIT(13)
#define VGAINIT0_SGRAM_HBLANK_DISABLE	BIT(22)
#define VGAINIT1_MASK			0x1fffff
#define VIDCFG_VIDPROC_ENABLE		BIT(0)
#define VIDCFG_CURS_X11			BIT(1)
#define VIDCFG_INTERLACE		BIT(3)
#define VIDCFG_HALF_MODE		BIT(4)
#define VIDCFG_DESK_ENABLE		BIT(7)
#define VIDCFG_CLUT_BYPASS		BIT(10)
#define VIDCFG_2X			BIT(26)
#define VIDCFG_HWCURSOR_ENABLE          BIT(27)
#define VIDCFG_PIXFMT_SHIFT		18
#define DACMODE_2X			BIT(0)

/* VGA rubbish, need to change this for multihead support */
#define MISC_W 	0x3c2
#define MISC_R 	0x3cc
#define SEQ_I 	0x3c4
#define SEQ_D	0x3c5
#define CRT_I	0x3d4
#define CRT_D	0x3d5
#define ATT_IW	0x3c0
#define IS1_R	0x3da
#define GRA_I	0x3ce
#define GRA_D	0x3cf

#ifndef FB_ACCEL_3DFX_BANSHEE 
#define FB_ACCEL_3DFX_BANSHEE 31
#endif

#define TDFXF_HSYNC_ACT_HIGH	0x01
#define TDFXF_HSYNC_ACT_LOW	0x02
#define TDFXF_VSYNC_ACT_HIGH	0x04
#define TDFXF_VSYNC_ACT_LOW	0x08
#define TDFXF_LINE_DOUBLE	0x10
#define TDFXF_VIDEO_ENABLE	0x20
#define TDFXF_INTERLACE		0x40

#define TDFXF_HSYNC_MASK	0x03
#define TDFXF_VSYNC_MASK	0x0c

//#define TDFXFB_DEBUG 
#ifdef TDFXFB_DEBUG
#define DPRINTK(a,b...) printk(KERN_DEBUG "fb: %s: " a, __FUNCTION__ , ## b)
#else
#define DPRINTK(a,b...)
#endif 

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

#define BANSHEE_MAX_PIXCLOCK 270000.0
#define VOODOO3_MAX_PIXCLOCK 300000.0
#define VOODOO5_MAX_PIXCLOCK 350000.0

struct banshee_reg {
  /* VGA rubbish */
  unsigned char att[21];
  unsigned char crt[25];
  unsigned char gra[ 9];
  unsigned char misc[1];
  unsigned char seq[ 5];

  /* Banshee extensions */
  unsigned char ext[2];
  unsigned long vidcfg;
  unsigned long vidpll;
  unsigned long mempll;
  unsigned long gfxpll;
  unsigned long dacmode;
  unsigned long vgainit0;
  unsigned long vgainit1;
  unsigned long screensize;
  unsigned long stride;
  unsigned long cursloc;
  unsigned long curspataddr;
  unsigned long cursc0;
  unsigned long cursc1;
  unsigned long startaddr;
  unsigned long clip0min;
  unsigned long clip0max;
  unsigned long clip1min;
  unsigned long clip1max;
  unsigned long srcbase;
  unsigned long dstbase;
  unsigned long miscinit0;
};

struct tdfxfb_par {
  u32 pixclock;

  u32 baseline;

  u32 width;
  u32 height;
  u32 width_virt;
  u32 height_virt;
  u32 lpitch; /* line pitch, in bytes */
  u32 ppitch; /* pixel pitch, in bits */
  u32 bpp;    

  u32 hdispend;
  u32 hsyncsta;
  u32 hsyncend;
  u32 htotal;

  u32 vdispend;
  u32 vsyncsta;
  u32 vsyncend;
  u32 vtotal;

  u32 video;
  u32 accel_flags;
  u32 cmap_len;
};

struct fb_info_tdfx {
  struct fb_info fb_info;

  u16 dev;
  u32 max_pixclock;

  unsigned long regbase_phys;
  void *regbase_virt;
  unsigned long regbase_size;
  unsigned long bufbase_phys;
  void *bufbase_virt;
  unsigned long bufbase_size;
  unsigned long iobase;

  struct { unsigned red, green, blue, pad; } palette[256];
  struct tdfxfb_par default_par;
  struct tdfxfb_par current_par;
  struct display disp;
#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)  
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
#endif
  struct { 
     int type;             
     int state;             
     int w,u,d;             
     int x,y,redraw;
     unsigned long enable,disable;    
     unsigned long cursorimage;       
     struct timer_list timer;
  } cursor;
 
  spinlock_t DAClock; 
#ifdef CONFIG_MTRR
  int mtrr_idx;
#endif
};

/*
 *  Frame buffer device API
 */
static int tdfxfb_get_fix(struct fb_fix_screeninfo* fix, 
			  int con,
			  struct fb_info* fb);
static int tdfxfb_get_var(struct fb_var_screeninfo* var, 
			  int con,
			  struct fb_info* fb);
static int tdfxfb_set_var(struct fb_var_screeninfo* var,
			  int con,
			  struct fb_info* fb);
static int tdfxfb_pan_display(struct fb_var_screeninfo* var, 
			      int con,
			      struct fb_info* fb);
static int tdfxfb_get_cmap(struct fb_cmap *cmap, 
			   int kspc, 
			   int con,
			   struct fb_info* info);
static int tdfxfb_set_cmap(struct fb_cmap* cmap, 
			   int kspc, 
			   int con,
			   struct fb_info* info);

/*
 *  Interface to the low level console driver
 */
static int  tdfxfb_switch_con(int con, 
			      struct fb_info* fb);
static int  tdfxfb_updatevar(int con, 
			     struct fb_info* fb);
static void tdfxfb_blank(int blank, 
			 struct fb_info* fb);

/*
 *  Internal routines
 */
static void tdfxfb_set_par(struct tdfxfb_par* par,
			   struct fb_info_tdfx* 
			   info);
static int  tdfxfb_decode_var(const struct fb_var_screeninfo *var,
			      struct tdfxfb_par *par,
			      const struct fb_info_tdfx *info);
static int  tdfxfb_encode_var(struct fb_var_screeninfo* var,
			      const struct tdfxfb_par* par,
			      const struct fb_info_tdfx* info);
static int  tdfxfb_encode_fix(struct fb_fix_screeninfo* fix,
			      const struct tdfxfb_par* par,
			      const struct fb_info_tdfx* info);
static void tdfxfb_set_dispsw(struct display* disp, 
			      struct fb_info_tdfx* info,
			      int bpp, 
			      int accel);
static int  tdfxfb_getcolreg(u_int regno,
			     u_int* red, 
			     u_int* green, 
			     u_int* blue,
			     u_int* transp, 
			     struct fb_info* fb);
static int  tdfxfb_setcolreg(u_int regno, 
			     u_int red, 
			     u_int green, 
			     u_int blue,
			     u_int transp, 
			     struct fb_info* fb);
static void  tdfxfb_install_cmap(struct display *d, 
				 struct fb_info *info);

static void tdfxfb_hwcursor_init(void);
static void tdfxfb_createcursorshape(struct display* p);
static void tdfxfb_createcursor(struct display * p);  

/*
 * do_xxx: Hardware-specific functions
 */
static void  do_pan_var(struct fb_var_screeninfo* var, struct fb_info_tdfx *i);
static void  do_flashcursor(unsigned long ptr);
static void  do_bitblt(u32 curx, u32 cury, u32 dstx,u32 dsty, 
		      u32 width, u32 height,u32 stride,u32 bpp);
static void  do_fillrect(u32 x, u32 y, u32 w,u32 h, 
			u32 color,u32 stride,u32 bpp,u32 rop);
static void  do_putc(u32 fgx, u32 bgx,struct display *p,
			int c, int yy,int xx);
static void  do_putcs(u32 fgx, u32 bgx,struct display *p,
		     const unsigned short *s,int count, int yy,int xx);
static u32 do_calc_pll(int freq, int* freq_out);
static void  do_write_regs(struct banshee_reg* reg);
static unsigned long do_lfb_size(void);

/*
 *  Interface used by the world
 */
int tdfxfb_init(void);
void tdfxfb_setup(char *options, 
		  int *ints);

/*
 * PCI driver prototypes
 */
static int tdfxfb_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void tdfxfb_remove(struct pci_dev *pdev);

static int currcon = 0;

static struct fb_ops tdfxfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	tdfxfb_get_fix,
	fb_get_var:	tdfxfb_get_var,
	fb_set_var:	tdfxfb_set_var,
	fb_get_cmap:	tdfxfb_get_cmap,
	fb_set_cmap:	tdfxfb_set_cmap,
	fb_pan_display:	tdfxfb_pan_display,
};

static struct pci_device_id tdfxfb_id_table[] __devinitdata = {
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_BANSHEE,
	  PCI_ANY_ID, PCI_ANY_ID, PCI_BASE_CLASS_DISPLAY << 16,
	  0xff0000, 0 },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO3,
	  PCI_ANY_ID, PCI_ANY_ID, PCI_BASE_CLASS_DISPLAY << 16,
	  0xff0000, 0 },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO5,
	  PCI_ANY_ID, PCI_ANY_ID, PCI_BASE_CLASS_DISPLAY << 16,
	  0xff0000, 0 },
	{ 0, }
};

static struct pci_driver tdfxfb_driver = {
	name:		"tdfxfb",
	id_table:	tdfxfb_id_table,
	probe:		tdfxfb_probe,
	remove:		__devexit_p(tdfxfb_remove),
};

MODULE_DEVICE_TABLE(pci, tdfxfb_id_table);

struct mode {
  char* name;
  struct fb_var_screeninfo var;
} mode;

/* 2.3.x kernels have a fb mode database, so supply only one backup default */
struct mode default_mode[] = {
  { "640x480-8@60", /* @ 60 Hz */
    {
      640, 480, 640, 1024, 0, 0, 8, 0,
      {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
      0, FB_ACTIVATE_NOW, -1, -1, FB_ACCELF_TEXT, 
      39722, 40, 24, 32, 11, 96, 2,
      0, FB_VMODE_NONINTERLACED
    }
  }
};

static struct fb_info_tdfx fb_info;

static int  noaccel = 0;
static int  nopan   = 0;
static int  nowrap  = 1;      // not implemented (yet)
static int  inverse = 0;
#ifdef CONFIG_MTRR
static int  nomtrr = 0;
#endif
static int  nohwcursor = 0;
static char __initdata fontname[40] = { 0 };
static char *mode_option __initdata = NULL;

/* ------------------------------------------------------------------------- 
 *                      Hardware-specific funcions
 * ------------------------------------------------------------------------- */

#ifdef VGA_REG_IO 
static inline  u8 vga_inb(u32 reg) { return inb(reg); }
static inline u16 vga_inw(u32 reg) { return inw(reg); }
static inline u16 vga_inl(u32 reg) { return inl(reg); }

static inline void vga_outb(u32 reg,  u8 val) { outb(val, reg); }
static inline void vga_outw(u32 reg, u16 val) { outw(val, reg); }
static inline void vga_outl(u32 reg, u32 val) { outl(val, reg); }
#else
static inline  u8 vga_inb(u32 reg) { 
  return inb(fb_info.iobase + reg - 0x300); 
}
static inline u16 vga_inw(u32 reg) { 
  return inw(fb_info.iobase + reg - 0x300); 
}
static inline u16 vga_inl(u32 reg) { 
  return inl(fb_info.iobase + reg - 0x300); 
}

static inline void vga_outb(u32 reg,  u8 val) { 
  outb(val, fb_info.iobase + reg - 0x300); 
}
static inline void vga_outw(u32 reg, u16 val) { 
  outw(val, fb_info.iobase + reg - 0x300); 
}
static inline void vga_outl(u32 reg, u32 val) { 
  outl(val, fb_info.iobase + reg - 0x300); 
}
#endif

static inline void gra_outb(u32 idx, u8 val) {
  vga_outb(GRA_I, idx); vga_outb(GRA_D, val);
}

static inline u8 gra_inb(u32 idx) {
  vga_outb(GRA_I, idx); return vga_inb(GRA_D);
}

static inline void seq_outb(u32 idx, u8 val) {
  vga_outb(SEQ_I, idx); vga_outb(SEQ_D, val);
}

static inline u8 seq_inb(u32 idx) {
  vga_outb(SEQ_I, idx); return vga_inb(SEQ_D);
}

static inline void crt_outb(u32 idx, u8 val) {
  vga_outb(CRT_I, idx); vga_outb(CRT_D, val);
}

static inline u8 crt_inb(u32 idx) {
  vga_outb(CRT_I, idx); return vga_inb(CRT_D);
}

static inline void att_outb(u32 idx, u8 val) {
  unsigned char tmp;
  tmp = vga_inb(IS1_R);
  vga_outb(ATT_IW, idx);
  vga_outb(ATT_IW, val);
}

static inline u8 att_inb(u32 idx) {
  unsigned char tmp;
  tmp = vga_inb(IS1_R);
  vga_outb(ATT_IW, idx);
  return vga_inb(ATT_IW);
}

static inline void vga_disable_video(void) {
  unsigned char s;
  s = seq_inb(0x01) | 0x20;
  seq_outb(0x00, 0x01);
  seq_outb(0x01, s);
  seq_outb(0x00, 0x03);
}

static inline void vga_enable_video(void) {
  unsigned char s;
  s = seq_inb(0x01) & 0xdf;
  seq_outb(0x00, 0x01);
  seq_outb(0x01, s);
  seq_outb(0x00, 0x03);
}

static inline void vga_disable_palette(void) {
  vga_inb(IS1_R);
  vga_outb(ATT_IW, 0x00);
}

static inline void vga_enable_palette(void) {
  vga_inb(IS1_R);
  vga_outb(ATT_IW, 0x20);
}

static inline u32 tdfx_inl(unsigned int reg) {
  return readl(fb_info.regbase_virt + reg);
}

static inline void tdfx_outl(unsigned int reg, u32 val) {
  writel(val, fb_info.regbase_virt + reg);
}

static inline void banshee_make_room(int size) {
  while((tdfx_inl(STATUS) & 0x1f) < size);
}
 
static inline void banshee_wait_idle(void) {
  int i = 0;

  banshee_make_room(1);
  tdfx_outl(COMMAND_3D, COMMAND_3D_NOP);

  while(1) {
    i = (tdfx_inl(STATUS) & STATUS_BUSY) ? 0 : i + 1;
    if(i == 3) break;
  }
}
/*
 * Set the color of a palette entry in 8bpp mode 
 */
static inline void do_setpalentry(unsigned regno, u32 c) {  
   banshee_make_room(2); tdfx_outl(DACADDR,  regno); tdfx_outl(DACDATA,  c); }

/* 
 * Set the starting position of the visible screen to var->yoffset 
 */
static void do_pan_var(struct fb_var_screeninfo* var, struct fb_info_tdfx *i)
{
    u32 addr;
    addr = var->yoffset*i->current_par.lpitch;
    banshee_make_room(1);
    tdfx_outl(VIDDESKSTART, addr);
}
   
/*
 * Invert the hardware cursor image (timerfunc)  
 */
static void do_flashcursor(unsigned long ptr)
{
   struct fb_info_tdfx* i=(struct fb_info_tdfx *)ptr;
   unsigned long flags;

   spin_lock_irqsave(&i->DAClock, flags);
   banshee_make_room(1);
   tdfx_outl( VIDPROCCFG, tdfx_inl(VIDPROCCFG) ^ VIDCFG_HWCURSOR_ENABLE );
   i->cursor.timer.expires=jiffies+HZ/2;
   add_timer(&i->cursor.timer);
   spin_unlock_irqrestore(&i->DAClock, flags);
}

/*
 * FillRect 2D command (solidfill or invert (via ROP_XOR))   
 */
static void do_fillrect(u32 x, u32 y, u32 w, u32 h, 
			u32 color, u32 stride, u32 bpp, u32 rop) {

   u32 fmt= stride | ((bpp+((bpp==8) ? 0 : 8)) << 13); 

   banshee_make_room(5);
   tdfx_outl(DSTFORMAT, fmt);
   tdfx_outl(COLORFORE, color);
   tdfx_outl(COMMAND_2D, COMMAND_2D_FILLRECT | (rop << 24));
   tdfx_outl(DSTSIZE,    w | (h << 16));
   tdfx_outl(LAUNCH_2D,  x | (y << 16));
   banshee_wait_idle();
}

/*
 * Screen-to-Screen BitBlt 2D command (for the bmove fb op.) 
 */

static void do_bitblt(u32 curx, 
			   u32 cury, 
			   u32 dstx,
			   u32 dsty, 
			   u32 width, 
			   u32 height,
			   u32 stride,
			   u32 bpp) {

   u32 blitcmd = COMMAND_2D_S2S_BITBLT | (ROP_COPY << 24);
   u32 fmt= stride | ((bpp+((bpp==8) ? 0 : 8)) << 13); 
   
   if (curx <= dstx) {
     //-X 
     blitcmd |= BIT(14);
     curx += width-1;
     dstx += width-1;
   }
   if (cury <= dsty) {
     //-Y  
     blitcmd |= BIT(15);
     cury += height-1;
     dsty += height-1;
   }
   
   banshee_make_room(6);

   tdfx_outl(SRCFORMAT, fmt);
   tdfx_outl(DSTFORMAT, fmt);
   tdfx_outl(COMMAND_2D, blitcmd); 
   tdfx_outl(DSTSIZE,   width | (height << 16));
   tdfx_outl(DSTXY,     dstx | (dsty << 16));
   tdfx_outl(LAUNCH_2D, curx | (cury << 16)); 
   banshee_wait_idle();
}

static void do_putc(u32 fgx, u32 bgx,
			 struct display *p,
			 int c, int yy,int xx)
{   
   int i;
   int stride=fb_info.current_par.lpitch;
   u32 bpp=fb_info.current_par.bpp;
   int fw=(fontwidth(p)+7)>>3;
   u8 *chardata=p->fontdata+(c&p->charmask)*fontheight(p)*fw;
   u32 fmt= stride | ((bpp+((bpp==8) ? 0 : 8)) << 13); 
   
   xx *= fontwidth(p);
   yy *= fontheight(p);

   banshee_make_room(8+((fontheight(p)*fw+3)>>2) );
   tdfx_outl(COLORFORE, fgx);
   tdfx_outl(COLORBACK, bgx);
   tdfx_outl(SRCXY,     0);
   tdfx_outl(DSTXY,     xx | (yy << 16));
   tdfx_outl(COMMAND_2D, COMMAND_2D_H2S_BITBLT | (ROP_COPY << 24));
#ifdef __BIG_ENDIAN
   tdfx_outl(SRCFORMAT, 0x400000 | BIT(20) );   
#else
   tdfx_outl(SRCFORMAT, 0x400000);
#endif
   tdfx_outl(DSTFORMAT, fmt);
   tdfx_outl(DSTSIZE,   fontwidth(p) | (fontheight(p) << 16));
   i=fontheight(p);
   switch (fw) {
    case 1:
     while (i>=4) {
         tdfx_outl(LAUNCH_2D,*(u32*)chardata);
	 chardata+=4;
	 i-=4;
     }
     switch (i) {
      case 0: break;
      case 1:  tdfx_outl(LAUNCH_2D,*chardata); break;
      case 2:  tdfx_outl(LAUNCH_2D,*(u16*)chardata); break;
      case 3:  tdfx_outl(LAUNCH_2D,*(u16*)chardata | ((chardata[3]) << 24)); break;
     }
     break;
   case 2:
     while (i>=2) {
         tdfx_outl(LAUNCH_2D,*(u32*)chardata);
	 chardata+=4;
	 i-=2;
     }
     if (i) tdfx_outl(LAUNCH_2D,*(u16*)chardata); 
     break;
   default:
     // Is there a font with width more that 16 pixels ?
     for (i=fontheight(p);i>0;i--) {
	 tdfx_outl(LAUNCH_2D,*(u32*)chardata);
	 chardata+=4;
     }
     break;
   }
   banshee_wait_idle();
}

static void do_putcs(u32 fgx, u32 bgx,
			  struct display *p,
			  const unsigned short *s,
			  int count, int yy,int xx)
{   
   int i;
   int stride=fb_info.current_par.lpitch;
   u32 bpp=fb_info.current_par.bpp;
   int fw=(fontwidth(p)+7)>>3;
   int w=fontwidth(p);
   int h=fontheight(p);
   int regsneed=1+((h*fw+3)>>2);
   u32 fmt= stride | ((bpp+((bpp==8) ? 0 : 8)) << 13); 

   xx *= w;
   yy = (yy*h) << 16;
   banshee_make_room(8);

   tdfx_outl(COMMAND_3D, COMMAND_3D_NOP);
   tdfx_outl(COLORFORE, fgx);
   tdfx_outl(COLORBACK, bgx);
#ifdef __BIG_ENDIAN
   tdfx_outl(SRCFORMAT, 0x400000 | BIT(20) );   
#else
   tdfx_outl(SRCFORMAT, 0x400000);
#endif   
   tdfx_outl(DSTFORMAT, fmt);
   tdfx_outl(DSTSIZE, w | (h << 16));
   tdfx_outl(SRCXY,     0);
   tdfx_outl(COMMAND_2D, COMMAND_2D_H2S_BITBLT | (ROP_COPY << 24));
   
   while (count--) {
      u8 *chardata=p->fontdata+(scr_readw(s++) & p->charmask)*h*fw;
   
      banshee_make_room(regsneed);
      tdfx_outl(DSTXY, xx | yy);
      xx+=w;
      
      i=h;
      switch (fw) {
       case 1:
        while (i>=4) {
           tdfx_outl(LAUNCH_2D,*(u32*)chardata);
	   chardata+=4;
	   i-=4;
        }
        switch (i) {
          case 0: break;
          case 1:  tdfx_outl(LAUNCH_2D,*chardata); break;
          case 2:  tdfx_outl(LAUNCH_2D,*(u16*)chardata); break;
          case 3:  tdfx_outl(LAUNCH_2D,*(u16*)chardata | ((chardata[3]) << 24)); break;
        }
        break;
       case 2:
        while (i>=2) {
         tdfx_outl(LAUNCH_2D,*(u32*)chardata);
	 chardata+=4;
	 i-=2;
        }
        if (i) tdfx_outl(LAUNCH_2D,*(u16*)chardata); 
        break;
       default:
       // Is there a font with width more that 16 pixels ?
        for (;i>0;i--) {
	  tdfx_outl(LAUNCH_2D,*(u32*)chardata);
	  chardata+=4;
        }
        break;
     }
   }
   banshee_wait_idle();
}

static u32 do_calc_pll(int freq, int* freq_out) {
  int m, n, k, best_m, best_n, best_k, f_cur, best_error;
  int fref = 14318;
  
  /* this really could be done with more intelligence --
     255*63*4 = 64260 iterations is silly */
  best_error = freq;
  best_n = best_m = best_k = 0;
  for(n = 1; n < 256; n++) {
    for(m = 1; m < 64; m++) {
      for(k = 0; k < 4; k++) {
	f_cur = fref*(n + 2)/(m + 2)/(1 << k);
	if(abs(f_cur - freq) < best_error) {
	  best_error = abs(f_cur-freq);
	  best_n = n;
	  best_m = m;
	  best_k = k;
	}
      }
    }
  }
  n = best_n;
  m = best_m;
  k = best_k;
  *freq_out = fref*(n + 2)/(m + 2)/(1 << k);

  return (n << 8) | (m << 2) | k;
}

static void do_write_regs(struct banshee_reg* reg) {
  int i;

  banshee_wait_idle();

  tdfx_outl(MISCINIT1, tdfx_inl(MISCINIT1) | 0x01);

  crt_outb(0x11, crt_inb(0x11) & 0x7f); /* CRT unprotect */

  banshee_make_room(3);
  tdfx_outl(VGAINIT1,      reg->vgainit1 &  0x001FFFFF);
  tdfx_outl(VIDPROCCFG,    reg->vidcfg   & ~0x00000001);
#if 0
  tdfx_outl(PLLCTRL1,      reg->mempll);
  tdfx_outl(PLLCTRL2,      reg->gfxpll);
#endif
  tdfx_outl(PLLCTRL0,      reg->vidpll);

  vga_outb(MISC_W, reg->misc[0x00] | 0x01);

  for(i = 0; i < 5; i++)
    seq_outb(i, reg->seq[i]);

  for(i = 0; i < 25; i++)
    crt_outb(i, reg->crt[i]);

  for(i = 0; i < 9; i++)
    gra_outb(i, reg->gra[i]);

  for(i = 0; i < 21; i++)
    att_outb(i, reg->att[i]);

  crt_outb(0x1a, reg->ext[0]);
  crt_outb(0x1b, reg->ext[1]);

  vga_enable_palette();
  vga_enable_video();

  banshee_make_room(11);
  tdfx_outl(VGAINIT0,      reg->vgainit0);
  tdfx_outl(DACMODE,       reg->dacmode);
  tdfx_outl(VIDDESKSTRIDE, reg->stride);
  if (nohwcursor) {
     tdfx_outl(HWCURPATADDR,  0);
  } else {
     tdfx_outl(HWCURPATADDR,  reg->curspataddr);
     tdfx_outl(HWCURC0,       reg->cursc0);
     tdfx_outl(HWCURC1,       reg->cursc1);
     tdfx_outl(HWCURLOC,      reg->cursloc);
  }
   
  tdfx_outl(VIDSCREENSIZE, reg->screensize);
  tdfx_outl(VIDDESKSTART,  reg->startaddr);
  tdfx_outl(VIDPROCCFG,    reg->vidcfg);
  tdfx_outl(VGAINIT1,      reg->vgainit1);  
  tdfx_outl(MISCINIT0,	   reg->miscinit0);

  banshee_make_room(8);
  tdfx_outl(SRCBASE,         reg->srcbase);
  tdfx_outl(DSTBASE,         reg->dstbase);
  tdfx_outl(COMMANDEXTRA_2D, 0);
  tdfx_outl(CLIP0MIN,        0);
  tdfx_outl(CLIP0MAX,        0x0fff0fff);
  tdfx_outl(CLIP1MIN,        0);
  tdfx_outl(CLIP1MAX,        0x0fff0fff);
  tdfx_outl(SRCXY, 0);

  banshee_wait_idle();
}

static unsigned long do_lfb_size(void) {
  u32 draminit0 = 0;
  u32 draminit1 = 0;
  u32 miscinit1 = 0;
  u32 lfbsize   = 0;
  int sgram_p     = 0;

  draminit0 = tdfx_inl(DRAMINIT0);  
  draminit1 = tdfx_inl(DRAMINIT1);

  if ((fb_info.dev == PCI_DEVICE_ID_3DFX_BANSHEE) ||
      (fb_info.dev == PCI_DEVICE_ID_3DFX_VOODOO3)) {
    sgram_p = (draminit1 & DRAMINIT1_MEM_SDRAM) ? 0 : 1;
  
    lfbsize = sgram_p ?
      (((draminit0 & DRAMINIT0_SGRAM_NUM)  ? 2 : 1) * 
       ((draminit0 & DRAMINIT0_SGRAM_TYPE) ? 8 : 4) * 1024 * 1024) :
      16 * 1024 * 1024;
  } else {
    /* Voodoo4/5 */
    u32 chips, psize, banks;

    chips = ((draminit0 & (1 << 26)) == 0) ? 4 : 8;
    psize = 1 << ((draminit0 & 0x38000000) >> 28);
    banks = ((draminit0 & (1 << 30)) == 0) ? 2 : 4;
    lfbsize = chips * psize * banks;
    lfbsize <<= 20;
  }

  /* disable block writes for SDRAM (why?) */
  miscinit1 = tdfx_inl(MISCINIT1);
  miscinit1 |= sgram_p ? 0 : MISCINIT1_2DBLOCK_DIS;
  miscinit1 |= MISCINIT1_CLUT_INV;

  banshee_make_room(1); 
  tdfx_outl(MISCINIT1, miscinit1);

  return lfbsize;
}

/* ------------------------------------------------------------------------- 
 *              Hardware independent part, interface to the world
 * ------------------------------------------------------------------------- */

#define tdfx_cfb24_putc  tdfx_cfb32_putc
#define tdfx_cfb24_putcs tdfx_cfb32_putcs
#define tdfx_cfb24_clear tdfx_cfb32_clear

static void tdfx_cfbX_clear_margins(struct vc_data* conp, struct display* p,
				    int bottom_only)
{
   unsigned int cw=fontwidth(p);
   unsigned int ch=fontheight(p);
   unsigned int rw=p->var.xres % cw;   // it be in a non-standard mode or not?
   unsigned int bh=p->var.yres % ch;
   unsigned int rs=p->var.xres - rw;
   unsigned int bs=p->var.yres - bh;

   if (!bottom_only && rw) { 
      do_fillrect( p->var.xoffset+rs, 0, 
		  rw, p->var.yres_virtual, 0, 
		  fb_info.current_par.lpitch,
		  fb_info.current_par.bpp, ROP_COPY);
   }
   
   if (bh) { 
      do_fillrect( p->var.xoffset, p->var.yoffset+bs, 
		  rs, bh, 0, 
		  fb_info.current_par.lpitch,
		  fb_info.current_par.bpp, ROP_COPY);
   }
}
static void tdfx_cfbX_bmove(struct display* p, 
				int sy, 
				int sx, 
				int dy,
				int dx, 
				int height, 
				int width) {
   do_bitblt(fontwidth(p)*sx,
		 fontheight(p)*sy, 
		 fontwidth(p)*dx,
		 fontheight(p)*dy, 
		 fontwidth(p)*width, 
		 fontheight(p)*height, 
		 fb_info.current_par.lpitch, 
		 fb_info.current_par.bpp);
}
static void tdfx_cfb8_putc(struct vc_data* conp,
			       struct display* p,
			       int c, int yy,int xx)
{   
   u32 fgx,bgx;
   fgx=attr_fgcol(p, c);
   bgx=attr_bgcol(p, c);
   do_putc( fgx,bgx,p,c,yy,xx );
}

static void tdfx_cfb16_putc(struct vc_data* conp,
			       struct display* p,
			       int c, int yy,int xx)
{   
   u32 fgx,bgx;
   fgx=((u16*)p->dispsw_data)[attr_fgcol(p,c)];
   bgx=((u16*)p->dispsw_data)[attr_bgcol(p,c)];
   do_putc( fgx,bgx,p,c,yy,xx );
}

static void tdfx_cfb32_putc(struct vc_data* conp,
			       struct display* p,
			       int c, int yy,int xx)
{   
   u32 fgx,bgx;
   fgx=((u32*)p->dispsw_data)[attr_fgcol(p,c)];
   bgx=((u32*)p->dispsw_data)[attr_bgcol(p,c)];
   do_putc( fgx,bgx,p,c,yy,xx );
}
static void tdfx_cfb8_putcs(struct vc_data* conp,
			    struct display* p,
			    const unsigned short *s,int count,int yy,int xx)
{
   u16 c = scr_readw(s);
   u32 fgx = attr_fgcol(p, c);
   u32 bgx = attr_bgcol(p, c);
   do_putcs( fgx,bgx,p,s,count,yy,xx );
}
static void tdfx_cfb16_putcs(struct vc_data* conp,
			    struct display* p,
			    const unsigned short *s,int count,int yy,int xx)
{
   u16 c = scr_readw(s);
   u32 fgx = ((u16*)p->dispsw_data)[attr_fgcol(p, c)];
   u32 bgx = ((u16*)p->dispsw_data)[attr_bgcol(p, c)];
   do_putcs( fgx,bgx,p,s,count,yy,xx );
}
static void tdfx_cfb32_putcs(struct vc_data* conp,
			    struct display* p,
			    const unsigned short *s,int count,int yy,int xx)
{
   u16 c = scr_readw(s);
   u32 fgx = ((u32*)p->dispsw_data)[attr_fgcol(p, c)];
   u32 bgx = ((u32*)p->dispsw_data)[attr_bgcol(p, c)];
   do_putcs( fgx,bgx,p,s,count,yy,xx );
}

static void tdfx_cfb8_clear(struct vc_data* conp, 
				struct display* p, 
				int sy,
				int sx, 
				int height, 
				int width) {
  u32 bg;

  bg = attr_bgcol_ec(p,conp);
  do_fillrect(fontwidth(p)*sx,
		   fontheight(p)*sy,
		   fontwidth(p)*width, 
		   fontheight(p)*height,
		   bg, 
		   fb_info.current_par.lpitch, 
		   fb_info.current_par.bpp,ROP_COPY);
}

static void tdfx_cfb16_clear(struct vc_data* conp, 
				struct display* p, 
				int sy,
				int sx, 
				int height, 
				int width) {
  u32 bg;

  bg = ((u16*)p->dispsw_data)[attr_bgcol_ec(p,conp)];
  do_fillrect(fontwidth(p)*sx,
		   fontheight(p)*sy,
		   fontwidth(p)*width, 
		   fontheight(p)*height,
		   bg, 
		   fb_info.current_par.lpitch, 
		   fb_info.current_par.bpp,ROP_COPY);
}

static void tdfx_cfb32_clear(struct vc_data* conp, 
				struct display* p, 
				int sy,
				int sx, 
				int height, 
				int width) {
  u32 bg;

  bg = ((u32*)p->dispsw_data)[attr_bgcol_ec(p,conp)];
  do_fillrect(fontwidth(p)*sx,
		   fontheight(p)*sy,
		   fontwidth(p)*width, 
		   fontheight(p)*height,
		   bg, 
		   fb_info.current_par.lpitch, 
		   fb_info.current_par.bpp,ROP_COPY);
}
static void tdfx_cfbX_revc(struct display *p, int xx, int yy)
{
   int bpp=fb_info.current_par.bpp;
   
   do_fillrect( xx * fontwidth(p), yy * fontheight(p), 
	        fontwidth(p), fontheight(p), 
	        (bpp==8) ? 0x0f : 0xffffffff, 
	        fb_info.current_par.lpitch, bpp, ROP_XOR);
   
}
static void tdfx_cfbX_cursor(struct display *p, int mode, int x, int y) 
{
   unsigned long flags;
   int tip;
   struct fb_info_tdfx *info=(struct fb_info_tdfx *)p->fb_info;
     
   tip=p->conp->vc_cursor_type & CUR_HWMASK;
   if (mode==CM_ERASE) {
	if (info->cursor.state != CM_ERASE) {
	     spin_lock_irqsave(&info->DAClock,flags);
	     info->cursor.state=CM_ERASE;
	     del_timer(&(info->cursor.timer));
	     tdfx_outl(VIDPROCCFG,info->cursor.disable); 
	     spin_unlock_irqrestore(&info->DAClock,flags);
	}
	return;
   }
   if ((p->conp->vc_cursor_type & CUR_HWMASK) != info->cursor.type)
	 tdfxfb_createcursor(p);
   x *= fontwidth(p);
   y *= fontheight(p);
   y -= p->var.yoffset;
   spin_lock_irqsave(&info->DAClock,flags);
   if ((x!=info->cursor.x) ||
      (y!=info->cursor.y) ||
      (info->cursor.redraw)) {
          info->cursor.x=x;
	  info->cursor.y=y;
	  info->cursor.redraw=0;
	  x += 63;
	  y += 63;    
          banshee_make_room(2);
	  tdfx_outl(VIDPROCCFG, info->cursor.disable);
	  tdfx_outl(HWCURLOC, (y << 16) + x);
	  /* fix cursor color - XFree86 forgets to restore it properly */
	  tdfx_outl(HWCURC0, 0);
	  tdfx_outl(HWCURC1, 0xffffff);
   }
   info->cursor.state = CM_DRAW;
   mod_timer(&info->cursor.timer,jiffies+HZ/2);
   banshee_make_room(1);
   tdfx_outl(VIDPROCCFG, info->cursor.enable);
   spin_unlock_irqrestore(&info->DAClock,flags);
   return;     
}
#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_banshee8 = {
   setup:		fbcon_cfb8_setup, 
   bmove:		tdfx_cfbX_bmove, 
   clear:		tdfx_cfb8_clear, 
   putc:		tdfx_cfb8_putc,
   putcs:		tdfx_cfb8_putcs, 
   revc:		tdfx_cfbX_revc,   
   cursor:		tdfx_cfbX_cursor, 
   clear_margins:	tdfx_cfbX_clear_margins,
   fontwidthmask:	FONTWIDTHRANGE(8, 12)
};
#endif
#ifdef FBCON_HAS_CFB16
static struct display_switch fbcon_banshee16 = {
   setup:		fbcon_cfb16_setup, 
   bmove:		tdfx_cfbX_bmove, 
   clear:		tdfx_cfb16_clear, 
   putc:		tdfx_cfb16_putc,
   putcs:		tdfx_cfb16_putcs, 
   revc:		tdfx_cfbX_revc, 
   cursor:		tdfx_cfbX_cursor, 
   clear_margins:	tdfx_cfbX_clear_margins,
   fontwidthmask:	FONTWIDTHRANGE(8, 12)
};
#endif
#ifdef FBCON_HAS_CFB24
static struct display_switch fbcon_banshee24 = {
   setup:		fbcon_cfb24_setup, 
   bmove:		tdfx_cfbX_bmove, 
   clear:		tdfx_cfb24_clear, 
   putc:		tdfx_cfb24_putc,
   putcs:		tdfx_cfb24_putcs, 
   revc:		tdfx_cfbX_revc, 
   cursor:		tdfx_cfbX_cursor, 
   clear_margins:	tdfx_cfbX_clear_margins,
   fontwidthmask:	FONTWIDTHRANGE(8, 12)
};
#endif
#ifdef FBCON_HAS_CFB32
static struct display_switch fbcon_banshee32 = {
   setup:		fbcon_cfb32_setup, 
   bmove:		tdfx_cfbX_bmove, 
   clear:		tdfx_cfb32_clear, 
   putc:		tdfx_cfb32_putc,
   putcs:		tdfx_cfb32_putcs, 
   revc:		tdfx_cfbX_revc, 
   cursor:		tdfx_cfbX_cursor, 
   clear_margins:	tdfx_cfbX_clear_margins,
   fontwidthmask:	FONTWIDTHRANGE(8, 12)
};
#endif

/* ------------------------------------------------------------------------- */

static void tdfxfb_set_par(struct tdfxfb_par* par,
			   struct fb_info_tdfx*     info) {
  struct fb_info_tdfx* i = (struct fb_info_tdfx*)info;
  struct banshee_reg reg;
  u32 cpp;
  u32 hd, hs, he, ht, hbs, hbe;
  u32 vd, vs, ve, vt, vbs, vbe;
  u32 wd;
  int fout;
  int freq;
   
  memset(&reg, 0, sizeof(reg));

  cpp = (par->bpp + 7)/8;
  
  reg.vidcfg = 
    VIDCFG_VIDPROC_ENABLE |
    VIDCFG_DESK_ENABLE    |
    VIDCFG_CURS_X11 |
    ((cpp - 1) << VIDCFG_PIXFMT_SHIFT) |
    (cpp != 1 ? VIDCFG_CLUT_BYPASS : 0);

  /* PLL settings */
  freq = par->pixclock;

  reg.dacmode = 0;
  reg.vidcfg  &= ~VIDCFG_2X;

  if(freq > i->max_pixclock/2) {
    freq = freq > i->max_pixclock ? i->max_pixclock : freq;
    reg.dacmode |= DACMODE_2X;
    reg.vidcfg  |= VIDCFG_2X;
    par->hdispend >>= 1;
    par->hsyncsta >>= 1;
    par->hsyncend >>= 1;
    par->htotal   >>= 1;
  }
  wd = (par->hdispend >> 3) - 1;

  hd  = (par->hdispend >> 3) - 1;
  hs  = (par->hsyncsta >> 3) - 1;
  he  = (par->hsyncend >> 3) - 1;
  ht  = (par->htotal   >> 3) - 1;
  hbs = hd;
  hbe = ht;

  if (par->video & TDFXF_LINE_DOUBLE) {
    vd = (par->vdispend << 1) - 1;
    vs = (par->vsyncsta << 1) - 1;
    ve = (par->vsyncend << 1) - 1;
    vt = (par->vtotal   << 1) - 2;
  } else {
    vd = par->vdispend - 1;
    vs = par->vsyncsta - 1;
    ve = par->vsyncend - 1;
    vt = par->vtotal   - 2;
  }
  vbs = vd;
  vbe = vt;
  
  /* this is all pretty standard VGA register stuffing */
  reg.misc[0x00] = 
    0x0f |
    (par->hdispend < 400 ? 0xa0 :
     par->hdispend < 480 ? 0x60 :
     par->hdispend < 768 ? 0xe0 : 0x20);
     
  reg.gra[0x00] = 0x00;
  reg.gra[0x01] = 0x00;
  reg.gra[0x02] = 0x00;
  reg.gra[0x03] = 0x00;
  reg.gra[0x04] = 0x00;
  reg.gra[0x05] = 0x40;
  reg.gra[0x06] = 0x05;
  reg.gra[0x07] = 0x0f;
  reg.gra[0x08] = 0xff;

  reg.att[0x00] = 0x00;
  reg.att[0x01] = 0x01;
  reg.att[0x02] = 0x02;
  reg.att[0x03] = 0x03;
  reg.att[0x04] = 0x04;
  reg.att[0x05] = 0x05;
  reg.att[0x06] = 0x06;
  reg.att[0x07] = 0x07;
  reg.att[0x08] = 0x08;
  reg.att[0x09] = 0x09;
  reg.att[0x0a] = 0x0a;
  reg.att[0x0b] = 0x0b;
  reg.att[0x0c] = 0x0c;
  reg.att[0x0d] = 0x0d;
  reg.att[0x0e] = 0x0e;
  reg.att[0x0f] = 0x0f;
  reg.att[0x10] = 0x41;
  reg.att[0x11] = 0x00;
  reg.att[0x12] = 0x0f;
  reg.att[0x13] = 0x00;
  reg.att[0x14] = 0x00;

  reg.seq[0x00] = 0x03;
  reg.seq[0x01] = 0x01; /* fixme: clkdiv2? */
  reg.seq[0x02] = 0x0f;
  reg.seq[0x03] = 0x00;
  reg.seq[0x04] = 0x0e;

  reg.crt[0x00] = ht - 4;
  reg.crt[0x01] = hd;
  reg.crt[0x02] = hbs;
  reg.crt[0x03] = 0x80 | (hbe & 0x1f);
  reg.crt[0x04] = hs;
  reg.crt[0x05] = ((hbe & 0x20) << 2) | (he & 0x1f);
  reg.crt[0x06] = vt;
  reg.crt[0x07] = 
    ((vs & 0x200) >> 2) |
    ((vd & 0x200) >> 3) |
    ((vt & 0x200) >> 4) |
    0x10 |
    ((vbs & 0x100) >> 5) |
    ((vs  & 0x100) >> 6) |
    ((vd  & 0x100) >> 7) |
    ((vt  & 0x100) >> 8);
  reg.crt[0x08] = 0x00;
  reg.crt[0x09] = 
    0x40 |
    ((vbs & 0x200) >> 4);
  reg.crt[0x0a] = 0x00;
  reg.crt[0x0b] = 0x00;
  reg.crt[0x0c] = 0x00;
  reg.crt[0x0d] = 0x00;
  reg.crt[0x0e] = 0x00;
  reg.crt[0x0f] = 0x00;
  reg.crt[0x10] = vs;
  reg.crt[0x11] = (ve & 0x0f) | 0x20;
  reg.crt[0x12] = vd;
  reg.crt[0x13] = wd;
  reg.crt[0x14] = 0x00;
  reg.crt[0x15] = vbs;
  reg.crt[0x16] = vbe + 1; 
  reg.crt[0x17] = 0xc3;
  reg.crt[0x18] = 0xff;
  
  /* Banshee's nonvga stuff */
  reg.ext[0x00] = (((ht  & 0x100) >> 8) | 
		   ((hd  & 0x100) >> 6) |
		   ((hbs & 0x100) >> 4) |
		   ((hbe &  0x40) >> 1) |
		   ((hs  & 0x100) >> 2) |
		   ((he  &  0x20) << 2)); 
  reg.ext[0x01] = (((vt  & 0x400) >> 10) |
		   ((vd  & 0x400) >>  8) | 
		   ((vbs & 0x400) >>  6) |
		   ((vbe & 0x400) >>  4));
  
  reg.vgainit0 = 
    VGAINIT0_8BIT_DAC     |
    VGAINIT0_EXT_ENABLE   |
    VGAINIT0_WAKEUP_3C3   |
    VGAINIT0_ALT_READBACK |
    VGAINIT0_EXTSHIFTOUT;
  reg.vgainit1 = tdfx_inl(VGAINIT1) & 0x1fffff;

  reg.stride    = par->width*cpp;
  reg.cursloc   = 0;
   
  reg.cursc0    = 0; 
  reg.cursc1    = 0xffffff;
   
  reg.curspataddr = fb_info.cursor.cursorimage;   
  
  reg.startaddr = par->baseline*reg.stride;
  reg.srcbase   = reg.startaddr;
  reg.dstbase   = reg.startaddr;

  reg.vidpll = do_calc_pll(freq, &fout);
#if 0
  reg.mempll = do_calc_pll(..., &fout);
  reg.gfxpll = do_calc_pll(..., &fout);
#endif

  if (par->video & TDFXF_LINE_DOUBLE) {
    reg.screensize = par->width | (par->height << 13);
    reg.vidcfg |= VIDCFG_HALF_MODE;
    reg.crt[0x09] |= 0x80;
  } else {
    reg.screensize = par->width | (par->height << 12);
    reg.vidcfg &= ~VIDCFG_HALF_MODE;
  }
  if (par->video & TDFXF_INTERLACE)
    reg.vidcfg |= VIDCFG_INTERLACE;

  fb_info.cursor.enable=reg.vidcfg | VIDCFG_HWCURSOR_ENABLE;
  fb_info.cursor.disable=reg.vidcfg;
   
  reg.miscinit0 = tdfx_inl(MISCINIT0);

#if defined(__BIG_ENDIAN)
  switch (par->bpp) {
    case 8:
    case 24:
      reg.miscinit0 &= ~(1 << 30);
      reg.miscinit0 &= ~(1 << 31);
      break;
    case 16:
      reg.miscinit0 |= (1 << 30);
      reg.miscinit0 |= (1 << 31);
      break;
    case 32:
      reg.miscinit0 |= (1 << 30);
      reg.miscinit0 &= ~(1 << 31);
      break;
  }
#endif

  do_write_regs(&reg);
  if (reg.vidcfg & VIDCFG_2X) {
    par->hdispend <<= 1;
    par->hsyncsta <<= 1;
    par->hsyncend <<= 1;
    par->htotal   <<= 1;
  }
  i->current_par = *par;
}

static int tdfxfb_decode_var(const struct fb_var_screeninfo* var,
			     struct tdfxfb_par*              par,
			     const struct fb_info_tdfx*      info) {
  struct fb_info_tdfx* i = (struct fb_info_tdfx*)info;

  if(var->bits_per_pixel != 8  &&
     var->bits_per_pixel != 16 &&
     var->bits_per_pixel != 24 &&
     var->bits_per_pixel != 32) {
    DPRINTK("depth not supported: %u\n", var->bits_per_pixel);
    return -EINVAL;
  }

  if(var->xoffset) {
    DPRINTK("xoffset not supported\n");
    return -EINVAL;
  }

  if(var->xres != var->xres_virtual) {
    DPRINTK("virtual x resolution != physical x resolution not supported\n");
    return -EINVAL;
  }

  if(var->yres > var->yres_virtual) {
    DPRINTK("virtual y resolution < physical y resolution not possible\n");
    return -EINVAL;
  }

  /* Banshee doesn't support interlace, but Voodoo4 and probably Voodoo3 do. */
  if(((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED)
     && (i->dev == PCI_DEVICE_ID_3DFX_BANSHEE)) {
    DPRINTK("interlace not supported on Banshee\n");
    return -EINVAL;
  }

  memset(par, 0, sizeof(struct tdfxfb_par));

  switch(i->dev) {
  case PCI_DEVICE_ID_3DFX_BANSHEE:
  case PCI_DEVICE_ID_3DFX_VOODOO3:
  case PCI_DEVICE_ID_3DFX_VOODOO5:
    par->width       = (var->xres + 15) & ~15; /* could sometimes be 8 */
    par->width_virt  = par->width;
    par->height      = var->yres;
    par->height_virt = var->yres_virtual;
    par->bpp         = var->bits_per_pixel;
    par->ppitch      = var->bits_per_pixel;
    par->lpitch      = par->width* ((par->ppitch+7)>>3);
    par->cmap_len    = (par->bpp == 8) ? 256 : 16;
     
    par->baseline = 0;

    if(par->width < 320 || par->width > 2048) {
      DPRINTK("width not supported: %u\n", par->width);
      return -EINVAL;
    }
    if(par->height < 200 || par->height > 2048) {
      DPRINTK("height not supported: %u\n", par->height);
      return -EINVAL;
    }
    if(par->lpitch*par->height_virt > i->bufbase_size) {
      DPRINTK("no memory for screen (%ux%ux%u)\n",
	      par->width, par->height_virt, par->bpp);
      return -EINVAL;
    }
    par->pixclock = PICOS2KHZ(var->pixclock);
    if(par->pixclock > i->max_pixclock) {
      DPRINTK("pixclock too high (%uKHz)\n", par->pixclock);
      return -EINVAL;
    }

    par->hdispend = var->xres;
    par->hsyncsta = par->hdispend + var->right_margin;
    par->hsyncend = par->hsyncsta + var->hsync_len;
    par->htotal   = par->hsyncend + var->left_margin;

    par->vdispend = var->yres;
    par->vsyncsta = par->vdispend + var->lower_margin;
    par->vsyncend = par->vsyncsta + var->vsync_len;
    par->vtotal   = par->vsyncend + var->upper_margin;

    if(var->sync & FB_SYNC_HOR_HIGH_ACT)
      par->video |= TDFXF_HSYNC_ACT_HIGH;
    else
      par->video |= TDFXF_HSYNC_ACT_LOW;
    if(var->sync & FB_SYNC_VERT_HIGH_ACT)
      par->video |= TDFXF_VSYNC_ACT_HIGH;
    else
      par->video |= TDFXF_VSYNC_ACT_LOW;
    if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE)
      par->video |= TDFXF_LINE_DOUBLE;
    else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED)
      par->video |= TDFXF_INTERLACE;
    if(var->activate == FB_ACTIVATE_NOW)
      par->video |= TDFXF_VIDEO_ENABLE;
  }

  if(var->accel_flags & FB_ACCELF_TEXT)
    par->accel_flags = FB_ACCELF_TEXT;
  else
    par->accel_flags = 0;

  return 0;
}

static int tdfxfb_encode_var(struct fb_var_screeninfo* var,
			    const struct tdfxfb_par* par,
			    const struct fb_info_tdfx* info) {
  struct fb_var_screeninfo v;

  memset(&v, 0, sizeof(struct fb_var_screeninfo));
  v.xres_virtual   = par->width_virt;
  v.yres_virtual   = par->height_virt;
  v.xres           = par->width;
  v.yres           = par->height;
  v.right_margin   = par->hsyncsta - par->hdispend;
  v.hsync_len      = par->hsyncend - par->hsyncsta;
  v.left_margin    = par->htotal   - par->hsyncend;
  v.lower_margin   = par->vsyncsta - par->vdispend;
  v.vsync_len      = par->vsyncend - par->vsyncsta;
  v.upper_margin   = par->vtotal   - par->vsyncend;
  v.bits_per_pixel = par->bpp;
  switch(par->bpp) {
  case 8:
    v.red.length = v.green.length = v.blue.length = 8;
    break;
  case 16:
    v.red.offset   = 11;
    v.red.length   = 5;
    v.green.offset = 5;
    v.green.length = 6;
    v.blue.offset  = 0;
    v.blue.length  = 5;
    break;
  case 24:
  case 32:
    v.red.offset   = 16;
    v.green.offset = 8;
    v.blue.offset  = 0;
    v.red.length = v.green.length = v.blue.length = 8;
    break;
  }
  v.height = v.width = -1;
  v.pixclock = KHZ2PICOS(par->pixclock);
  if((par->video & TDFXF_HSYNC_MASK) == TDFXF_HSYNC_ACT_HIGH)
    v.sync |= FB_SYNC_HOR_HIGH_ACT;
  if((par->video & TDFXF_VSYNC_MASK) == TDFXF_VSYNC_ACT_HIGH)
    v.sync |= FB_SYNC_VERT_HIGH_ACT;
  if(par->video & TDFXF_LINE_DOUBLE)
    v.vmode = FB_VMODE_DOUBLE;
  else if(par->video & TDFXF_INTERLACE)
    v.vmode = FB_VMODE_INTERLACED;
  *var = v;
  return 0;
}

static int tdfxfb_encode_fix(struct fb_fix_screeninfo*  fix,
			     const struct tdfxfb_par*   par,
			     const struct fb_info_tdfx* info) {
  memset(fix, 0, sizeof(struct fb_fix_screeninfo));

  switch(info->dev) {
  case PCI_DEVICE_ID_3DFX_BANSHEE:
    strcpy(fix->id, "3Dfx Banshee");
    break;
  case PCI_DEVICE_ID_3DFX_VOODOO3:
    strcpy(fix->id, "3Dfx Voodoo3");
    break;
  case PCI_DEVICE_ID_3DFX_VOODOO5:
    strcpy(fix->id, "3Dfx Voodoo5");
    break;
  default:
    return -EINVAL;
  }

  fix->smem_start  = info->bufbase_phys;
  fix->smem_len    = info->bufbase_size;
  fix->mmio_start  = info->regbase_phys;
  fix->mmio_len    = info->regbase_size;
  fix->accel       = FB_ACCEL_3DFX_BANSHEE;
  fix->type        = FB_TYPE_PACKED_PIXELS;
  fix->type_aux    = 0;
  fix->line_length = par->lpitch;
  fix->visual      = (par->bpp == 8) 
                     ? FB_VISUAL_PSEUDOCOLOR
                     : FB_VISUAL_TRUECOLOR;

  fix->xpanstep    = 0; 
  fix->ypanstep    = nopan ? 0 : 1;
  fix->ywrapstep   = nowrap ? 0 : 1;

  return 0;
}

static int tdfxfb_get_fix(struct fb_fix_screeninfo *fix, 
			  int con,
			  struct fb_info *fb) {
  const struct fb_info_tdfx *info = (struct fb_info_tdfx*)fb;
  struct tdfxfb_par par;

  if(con == -1)
    par = info->default_par;
  else
    tdfxfb_decode_var(&fb_display[con].var, &par, info);
  tdfxfb_encode_fix(fix, &par, info);
  return 0;
}

static int tdfxfb_get_var(struct fb_var_screeninfo *var, 
			  int con,
			  struct fb_info *fb) {
  const struct fb_info_tdfx *info = (struct fb_info_tdfx*)fb;

  if(con == -1)
    tdfxfb_encode_var(var, &info->default_par, info);
  else
    *var = fb_display[con].var;
  return 0;
}
 
static void tdfxfb_set_dispsw(struct display *disp, 
			      struct fb_info_tdfx *info,
			      int bpp, 
			      int accel) {

  if (disp->dispsw && disp->conp) 
     fb_con.con_cursor(disp->conp, CM_ERASE);
  switch(bpp) {
#ifdef FBCON_HAS_CFB8
  case 8:
    disp->dispsw = noaccel ? &fbcon_cfb8 : &fbcon_banshee8;
    if (nohwcursor) fbcon_banshee8.cursor = NULL;
    break;
#endif
#ifdef FBCON_HAS_CFB16
  case 16:
    disp->dispsw = noaccel ? &fbcon_cfb16 : &fbcon_banshee16;
    disp->dispsw_data = info->fbcon_cmap.cfb16;
    if (nohwcursor) fbcon_banshee16.cursor = NULL;
    break;
#endif
#ifdef FBCON_HAS_CFB24
  case 24:
    disp->dispsw = noaccel ? &fbcon_cfb24 : &fbcon_banshee24; 
    disp->dispsw_data = info->fbcon_cmap.cfb24;
    if (nohwcursor) fbcon_banshee24.cursor = NULL;
    break;
#endif
#ifdef FBCON_HAS_CFB32
  case 32:
    disp->dispsw = noaccel ? &fbcon_cfb32 : &fbcon_banshee32;
    disp->dispsw_data = info->fbcon_cmap.cfb32;
    if (nohwcursor) fbcon_banshee32.cursor = NULL;
    break;
#endif
  default:
    disp->dispsw = &fbcon_dummy;
  }
   
}

static int tdfxfb_set_var(struct fb_var_screeninfo *var, 
			  int con,
			  struct fb_info *fb) {
   struct fb_info_tdfx *info = (struct fb_info_tdfx*)fb;
   struct tdfxfb_par par;
   struct display *display;
   int oldxres, oldyres, oldvxres, oldvyres, oldbpp, oldaccel, accel, err;
   int activate = var->activate;
   int j,k;
   
   if(con >= 0)
     display = &fb_display[con];
   else
     display = fb->disp;	/* used during initialization */
   
   if((err = tdfxfb_decode_var(var, &par, info)))
     return err;
   
   tdfxfb_encode_var(var, &par, info);
   
   if((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
      oldxres  = display->var.xres;
      oldyres  = display->var.yres;
      oldvxres = display->var.xres_virtual;
      oldvyres = display->var.yres_virtual;
      oldbpp   = display->var.bits_per_pixel;
      oldaccel = display->var.accel_flags;
      display->var = *var;
      if(con < 0                         ||
	 oldxres  != var->xres           || 
	 oldyres  != var->yres           ||
	 oldvxres != var->xres_virtual   || 
	 oldvyres != var->yres_virtual   ||
	 oldbpp   != var->bits_per_pixel || 
	 oldaccel != var->accel_flags) {
	 struct fb_fix_screeninfo fix;
	 
	 tdfxfb_encode_fix(&fix, &par, info);
	 display->screen_base    = info->bufbase_virt;
	 display->visual         = fix.visual;
	 display->type           = fix.type;
	 display->type_aux       = fix.type_aux;
	 display->ypanstep       = fix.ypanstep;
	 display->ywrapstep      = fix.ywrapstep;
	 display->line_length    = fix.line_length;
	 display->next_line      = fix.line_length;
	 display->can_soft_blank = 1;
	 display->inverse        = inverse;
	 accel = var->accel_flags & FB_ACCELF_TEXT;
	 tdfxfb_set_dispsw(display, info, par.bpp, accel);
	 
	 if(nopan) display->scrollmode = SCROLL_YREDRAW;
	
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
      if(!info->fb_info.display_fg ||
	 info->fb_info.display_fg->vc_num == con ||
	 con < 0)
	tdfxfb_set_par(&par, info);
      if (!nohwcursor) 
	if (display && display->conp)
	  tdfxfb_createcursor( display );
      info->cursor.redraw=1;
      if(oldbpp != var->bits_per_pixel || con < 0) {
	 if((err = fb_alloc_cmap(&display->cmap, 0, 0)))
	   return err;
	 tdfxfb_install_cmap(display, &(info->fb_info));
      }
   }
  
   return 0;
}

static int tdfxfb_pan_display(struct fb_var_screeninfo* var, 
			      int con,
			      struct fb_info* fb) {
  struct fb_info_tdfx* i = (struct fb_info_tdfx*)fb;

  if(nopan)                return -EINVAL;
  if(var->xoffset)         return -EINVAL;
  if(var->yoffset > var->yres_virtual)   return -EINVAL;
  if(nowrap && 
     (var->yoffset + var->yres > var->yres_virtual)) return -EINVAL;
 
  if (con==currcon)
    do_pan_var(var,i);
   
  fb_display[con].var.xoffset=var->xoffset;
  fb_display[con].var.yoffset=var->yoffset; 
  return 0;
}

static int tdfxfb_get_cmap(struct fb_cmap *cmap, 
			   int kspc, 
			   int con,
			   struct fb_info *fb) {

   struct fb_info_tdfx* i = (struct fb_info_tdfx*)fb;
   struct display *d=(con<0) ? fb->disp : fb_display + con;
   
   if(con == currcon) {
      /* current console? */
      return fb_get_cmap(cmap, kspc, tdfxfb_getcolreg, fb);
   } else if(d->cmap.len) {
      /* non default colormap? */
      fb_copy_cmap(&d->cmap, cmap, kspc ? 0 : 2);
   } else {
      fb_copy_cmap(fb_default_cmap(i->current_par.cmap_len), cmap, kspc ? 0 : 2);
   }
   return 0;
}

static int tdfxfb_set_cmap(struct fb_cmap *cmap, 
			   int kspc, 
			   int con,
			   struct fb_info *fb) {
   struct display *d=(con<0) ? fb->disp : fb_display + con;
   struct fb_info_tdfx *i = (struct fb_info_tdfx*)fb;

   int cmap_len= (i->current_par.bpp == 8) ? 256 : 16;
   if (d->cmap.len!=cmap_len) {
      int err;
      if((err = fb_alloc_cmap(&d->cmap, cmap_len, 0)))
	return err;
   }
   if(con == currcon) {
      /* current console? */
      return fb_set_cmap(cmap, kspc, tdfxfb_setcolreg, fb);
   } else {
      fb_copy_cmap(cmap, &d->cmap, kspc ? 0 : 1);
   }
   return 0;
}

/**
 * 	tdfxfb_probe - Device Initializiation
 * 	
 * 	@pdev:	PCI Device to initialize
 * 	@id:	PCI Device ID
 *
 * 	Initializes and allocates resources for PCI device @pdev.
 *
 */
static int __devinit tdfxfb_probe(struct pci_dev *pdev,
				  const struct pci_device_id *id)
{
	struct fb_var_screeninfo var;
	char *name = NULL;

	fb_info.dev = pdev->device;
	
	switch (pdev->device) {
		case PCI_DEVICE_ID_3DFX_BANSHEE:
			fb_info.max_pixclock = BANSHEE_MAX_PIXCLOCK;
			name = "Banshee";
			break;
		case PCI_DEVICE_ID_3DFX_VOODOO3:
			fb_info.max_pixclock = VOODOO3_MAX_PIXCLOCK;
			name = "Voodoo3";
			break;
		case PCI_DEVICE_ID_3DFX_VOODOO5:
			fb_info.max_pixclock = VOODOO5_MAX_PIXCLOCK;
			name = "Voodoo5";
			break;
	}
	
        if (pci_enable_device(pdev)) 
        {
                printk(KERN_WARNING "fb: Unable to enable %s PCI device.\n", name);
                return -ENXIO;
        }

	fb_info.regbase_phys = pci_resource_start(pdev, 0);
	fb_info.regbase_size = 1 << 24;
	fb_info.regbase_virt = ioremap_nocache(fb_info.regbase_phys, 1 << 24);
	
	if (!fb_info.regbase_virt) {
		printk(KERN_WARNING "fb: Can't remap %s register area.\n", name);
		return -ENXIO;
	}
      
	fb_info.bufbase_phys = pci_resource_start (pdev, 1);
	
	if (!(fb_info.bufbase_size = do_lfb_size())) {
		iounmap(fb_info.regbase_virt);
		printk(KERN_WARNING "fb: Can't count %s memory.\n", name);
		return -ENXIO;
	}
	
	fb_info.bufbase_virt = ioremap_nocache(fb_info.bufbase_phys,
					       fb_info.bufbase_size);
					       
	if (!fb_info.regbase_virt) {
		printk(KERN_WARNING "fb: Can't remap %s framebuffer.\n", name);
		iounmap(fb_info.regbase_virt);
		return -ENXIO;
	}

	fb_info.iobase = pci_resource_start (pdev, 2);
      
        if (!fb_info.iobase) {
	        printk(KERN_WARNING "fb: Can't access %s I/O ports.\n", name);
		iounmap(fb_info.regbase_virt);
		iounmap(fb_info.bufbase_virt);
                return -ENXIO;
	}
   
	printk("fb: %s memory = %ldK\n", name, fb_info.bufbase_size >> 10);

#ifdef CONFIG_MTRR
	if (!nomtrr) {
		fb_info.mtrr_idx = mtrr_add(fb_info.bufbase_phys,
					    fb_info.bufbase_size,
					    MTRR_TYPE_WRCOMB, 1);
		printk(KERN_INFO "fb: MTRR's turned on\n");
	}
#endif

	/* clear framebuffer memory */
	memset_io(fb_info.bufbase_virt, 0, fb_info.bufbase_size);
	currcon = -1;

	if (!nohwcursor)
		tdfxfb_hwcursor_init();
       
	init_timer(&fb_info.cursor.timer);
	fb_info.cursor.timer.function = do_flashcursor; 
	fb_info.cursor.timer.data = (unsigned long)(&fb_info);
	fb_info.cursor.state = CM_ERASE;
	spin_lock_init(&fb_info.DAClock);
       
	strcpy(fb_info.fb_info.modename, "3Dfx "); 
	strcat(fb_info.fb_info.modename, name);
	fb_info.fb_info.changevar  = NULL;
	fb_info.fb_info.node       = -1;
	fb_info.fb_info.fbops      = &tdfxfb_ops;
	fb_info.fb_info.disp       = &fb_info.disp;
	strcpy(fb_info.fb_info.fontname, fontname);
	fb_info.fb_info.switch_con = &tdfxfb_switch_con;
	fb_info.fb_info.updatevar  = &tdfxfb_updatevar;
	fb_info.fb_info.blank      = &tdfxfb_blank;
	fb_info.fb_info.flags      = FBINFO_FLAG_DEFAULT;
      
	memset(&var, 0, sizeof(var));
	
	if (!mode_option || !fb_find_mode(&var, &fb_info.fb_info,
					  mode_option, NULL, 0, NULL, 8))
		var = default_mode[0].var;

	noaccel ? (var.accel_flags &= ~FB_ACCELF_TEXT) :
		  (var.accel_flags |=  FB_ACCELF_TEXT) ;

	if (tdfxfb_decode_var(&var, &fb_info.default_par, &fb_info)) {
		/* 
		 * ugh -- can't use the mode from the mode db. (or command
		 * line), so try the default
		 */

		printk(KERN_NOTICE "tdfxfb: can't decode the supplied video mode, using default\n");

		var = default_mode[0].var;

		noaccel ? (var.accel_flags &= ~FB_ACCELF_TEXT) :
			  (var.accel_flags |=  FB_ACCELF_TEXT) ;

		if (tdfxfb_decode_var(&var, &fb_info.default_par, &fb_info)) {
			/* this is getting really bad!... */
			printk(KERN_WARNING "tdfxfb: can't decode default video mode\n");
			return -ENXIO;
		}
	}

	fb_info.disp.screen_base = fb_info.bufbase_virt;
	fb_info.disp.var         = var;
      
	if (tdfxfb_set_var(&var, -1, &fb_info.fb_info)) {
		printk(KERN_WARNING "tdfxfb: can't set default video mode\n");
		return -ENXIO;
	}

	if (register_framebuffer(&fb_info.fb_info) < 0) {
		printk(KERN_WARNING "tdfxfb: can't register framebuffer\n");
		return -ENXIO;
	}

	printk(KERN_INFO "fb%d: %s frame buffer device\n", 
	     GET_FB_IDX(fb_info.fb_info.node), fb_info.fb_info.modename);
      
  	return 0;
}

/**
 *	tdfxfb_remove - Device removal
 *
 * 	@pdev:	PCI Device to cleanup
 *
 *	Releases all resources allocated during the course of the driver's
 *	lifetime for the PCI device @pdev.
 *
 */
static void __devexit tdfxfb_remove(struct pci_dev *pdev)
{
	unregister_framebuffer(&fb_info.fb_info);
	del_timer_sync(&fb_info.cursor.timer);

#ifdef CONFIG_MTRR
       if (!nomtrr) {
          mtrr_del(fb_info.mtrr_idx, fb_info.bufbase_phys, fb_info.bufbase_size);
	    printk("fb: MTRR's turned off\n");
       }
#endif

	iounmap(fb_info.regbase_virt);
	iounmap(fb_info.bufbase_virt);
}

int __init tdfxfb_init(void)
{
	return pci_module_init(&tdfxfb_driver);
}

static void __exit tdfxfb_exit(void)
{
	pci_unregister_driver(&tdfxfb_driver);
}

MODULE_AUTHOR("Hannu Mallat <hmallat@cc.hut.fi>");
MODULE_DESCRIPTION("3Dfx framebuffer device driver");
MODULE_LICENSE("GPL");


#ifdef MODULE
module_init(tdfxfb_init);
#endif
module_exit(tdfxfb_exit);


#ifndef MODULE
void tdfxfb_setup(char *options, 
		  int *ints) {
  char* this_opt;

  if(!options || !*options)
    return;

  while((this_opt = strsep(&options, ",")) != NULL) {
    if(!*this_opt)
      continue;
    if(!strcmp(this_opt, "inverse")) {
      inverse = 1;
      fb_invert_cmaps();
    } else if(!strcmp(this_opt, "noaccel")) {
      noaccel = nopan = nowrap = nohwcursor = 1; 
    } else if(!strcmp(this_opt, "nopan")) {
      nopan = 1;
    } else if(!strcmp(this_opt, "nowrap")) {
      nowrap = 1;
    } else if (!strcmp(this_opt, "nohwcursor")) {
      nohwcursor = 1;
#ifdef CONFIG_MTRR
    } else if (!strcmp(this_opt, "nomtrr")) {
      nomtrr = 1;
#endif
    } else if (!strncmp(this_opt, "font:", 5)) {
      strncpy(fontname, this_opt + 5, 40);
    } else {
      mode_option = this_opt;
    }
  } 
}
#endif

static int tdfxfb_switch_con(int con, 
			     struct fb_info *fb) {
   struct fb_info_tdfx *info = (struct fb_info_tdfx*)fb;
   struct tdfxfb_par par;
   int old_con = currcon;
   int set_par = 1;

   /* Do we have to save the colormap? */
   if (currcon>=0)
     if(fb_display[currcon].cmap.len)
       fb_get_cmap(&fb_display[currcon].cmap, 1, tdfxfb_getcolreg, fb);
   
   currcon = con;
   fb_display[currcon].var.activate = FB_ACTIVATE_NOW; 
   tdfxfb_decode_var(&fb_display[con].var, &par, info);
   if (old_con>=0 && vt_cons[old_con]->vc_mode!=KD_GRAPHICS) {
     /* check if we have to change video registers */
     struct tdfxfb_par old_par;
     tdfxfb_decode_var(&fb_display[old_con].var, &old_par, info);
     if (!memcmp(&par,&old_par,sizeof(par)))
	set_par = 0;	/* avoid flicker */
   }
   if (set_par)
     tdfxfb_set_par(&par, info);

   if (fb_display[con].dispsw && fb_display[con].conp)
     fb_con.con_cursor(fb_display[con].conp, CM_ERASE);
   
   del_timer(&(info->cursor.timer));
   fb_info.cursor.state=CM_ERASE; 
   
   if (!nohwcursor) 
     if (fb_display[con].conp)
       tdfxfb_createcursor( &fb_display[con] );
   
   info->cursor.redraw=1;
   
   tdfxfb_set_dispsw(&fb_display[con], 
		     info, 
		     par.bpp,
		     par.accel_flags & FB_ACCELF_TEXT);
   
   tdfxfb_install_cmap(&fb_display[con], fb);
   tdfxfb_updatevar(con, fb);
   
   return 1;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */
static void tdfxfb_blank(int blank, 
			 struct fb_info *fb) {
  u32 dacmode, state = 0, vgablank = 0;

  dacmode = tdfx_inl(DACMODE);

  switch(blank) {
  case 0: /* Screen: On; HSync: On, VSync: On */    
    state    = 0;
    vgablank = 0;
    break;
  case 1: /* Screen: Off; HSync: On, VSync: On */
    state    = 0;
    vgablank = 1;
    break;
  case 2: /* Screen: Off; HSync: On, VSync: Off */
    state    = BIT(3);
    vgablank = 1;
    break;
  case 3: /* Screen: Off; HSync: Off, VSync: On */
    state    = BIT(1);
    vgablank = 1;
    break;
  case 4: /* Screen: Off; HSync: Off, VSync: Off */
    state    = BIT(1) | BIT(3);
    vgablank = 1;
    break;
  }

  dacmode &= ~(BIT(1) | BIT(3));
  dacmode |= state;
  banshee_make_room(1); 
  tdfx_outl(DACMODE, dacmode);
  if(vgablank) 
    vga_disable_video();
  else
    vga_enable_video();

  return;
}

static int  tdfxfb_updatevar(int con, 
			     struct fb_info* fb) {

   struct fb_info_tdfx* i = (struct fb_info_tdfx*)fb;
   if ((con==currcon) && (!nopan)) 
     do_pan_var(&fb_display[con].var,i);
   return 0;
}

static int tdfxfb_getcolreg(unsigned        regno, 
			    unsigned*       red, 
			    unsigned*       green,
			    unsigned*       blue, 
			    unsigned*       transp,
			    struct fb_info* fb) {
   struct fb_info_tdfx* i = (struct fb_info_tdfx*)fb;

   if (regno > i->current_par.cmap_len) return 1;
   
   *red    = i->palette[regno].red; 
   *green  = i->palette[regno].green; 
   *blue   = i->palette[regno].blue; 
   *transp = 0;
   
   return 0;
}

static int tdfxfb_setcolreg(unsigned        regno, 
			    unsigned        red, 
			    unsigned        green,
			    unsigned        blue, 
			    unsigned        transp,
			    struct fb_info* info) {
   struct fb_info_tdfx* i = (struct fb_info_tdfx*)info;
#ifdef FBCON_HAS_CFB8   
   u32 rgbcol;
#endif
   if (regno >= i->current_par.cmap_len) return 1;
   
   i->palette[regno].red    = red;
   i->palette[regno].green  = green;
   i->palette[regno].blue   = blue;
   
   switch(i->current_par.bpp) {
#ifdef FBCON_HAS_CFB8
    case 8:
      rgbcol=(((u32)red   & 0xff00) << 8) |
	(((u32)green & 0xff00) << 0) |
	(((u32)blue  & 0xff00) >> 8);
      do_setpalentry(regno,rgbcol);
      break;
#endif
#ifdef FBCON_HAS_CFB16
    case 16:
      i->fbcon_cmap.cfb16[regno] =
	(((u32)red   & 0xf800) >> 0) |
	(((u32)green & 0xfc00) >> 5) |
	(((u32)blue  & 0xf800) >> 11);
	 break;
#endif
#ifdef FBCON_HAS_CFB24
    case 24:
      i->fbcon_cmap.cfb24[regno] =
	(((u32)red & 0xff00) << 8) |
	(((u32)green & 0xff00) << 0) |
	(((u32)blue & 0xff00) >> 8);
      break;
#endif
#ifdef FBCON_HAS_CFB32
    case 32:
      i->fbcon_cmap.cfb32[regno] =
	(((u32)red   & 0xff00) << 8) |
	(((u32)green & 0xff00) << 0) |
	(((u32)blue  & 0xff00) >> 8);
      break;
#endif
    default:
      DPRINTK("bad depth %u\n", i->current_par.bpp);
      break;
   }
   return 0;
}

static void tdfxfb_install_cmap(struct display *d,struct fb_info *info) 
{
   struct fb_info_tdfx* i = (struct fb_info_tdfx*)info;

   if(d->cmap.len) {
      fb_set_cmap(&(d->cmap), 1, tdfxfb_setcolreg, info);
   } else {
      fb_set_cmap(fb_default_cmap(i->current_par.cmap_len), 1, 
		  tdfxfb_setcolreg, info);
   }
}

static void tdfxfb_createcursorshape(struct display* p) 
{
   unsigned int h,cu,cd;
   
   h=fontheight(p);
   cd=h;
   if (cd >= 10) cd --; 
   fb_info.cursor.type=p->conp->vc_cursor_type & CUR_HWMASK;
   switch (fb_info.cursor.type) {
      case CUR_NONE: 
	cu=cd; 
	break;
      case CUR_UNDERLINE: 
	cu=cd - 2; 
	break;
      case CUR_LOWER_THIRD: 
	cu=(h * 2) / 3; 
	break;
      case CUR_LOWER_HALF: 
	cu=h / 2; 
	break;
      case CUR_TWO_THIRDS: 
	cu=h / 3; 
	break;
      case CUR_BLOCK:
      default:
	cu=0;
	cd = h;
	break;
   }
   fb_info.cursor.w=fontwidth(p);
   fb_info.cursor.u=cu;
   fb_info.cursor.d=cd;
}
   
static void tdfxfb_createcursor(struct display *p)
{
   u8 *cursorbase;
   u32 xline;
   unsigned int i;
   unsigned int h,to;

   tdfxfb_createcursorshape(p);
   xline = (~0) << (32 - fb_info.cursor.w);

#ifdef __LITTLE_ENDIAN
   xline = swab32(xline);
#else
   switch (p->var.bits_per_pixel) {
      case 8:
      case 24:
         xline = swab32(xline);
         break;
      case 16:
         xline = ((xline & 0xff000000 ) >> 16 )
               | ((xline & 0x00ff0000 ) >> 16 )
               | ((xline & 0x0000ff00 ) << 16 )
               | ((xline & 0x000000ff ) << 16 );
         break;
      case 32:
         break;
   }
#endif

   cursorbase=(u8*)fb_info.bufbase_virt;
   h=fb_info.cursor.cursorimage;     
   
   to=fb_info.cursor.u;
   for (i = 0; i < to; i++) {
	writel(0, cursorbase+h);
	writel(0, cursorbase+h+4);
	writel(~0, cursorbase+h+8);
	writel(~0, cursorbase+h+12);
	h += 16;
   }
   
   to = fb_info.cursor.d;
   
   for (; i < to; i++) {
	writel(xline, cursorbase+h);
	writel(0, cursorbase+h+4);
	writel(~0, cursorbase+h+8);
	writel(~0, cursorbase+h+12);
	h += 16;
   }
   
   for (; i < 64; i++) {
	writel(0, cursorbase+h);
	writel(0, cursorbase+h+4);
	writel(~0, cursorbase+h+8);
	writel(~0, cursorbase+h+12);
	h += 16;
   }
}
   
static void tdfxfb_hwcursor_init(void)
{
   unsigned int start;
   start = (fb_info.bufbase_size-1024) & (PAGE_MASK << 1);
   	/* even page boundary - on Voodoo4 4500 bottom 48 lines
	 * contained trash when just page boundary was used... */
   fb_info.bufbase_size=start; 
   fb_info.cursor.cursorimage=fb_info.bufbase_size;
   printk("tdfxfb: reserving 1024 bytes for the hwcursor at %p\n",
	  fb_info.regbase_virt+fb_info.cursor.cursorimage);
}

 
