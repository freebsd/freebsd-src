/* $Id: creatorfb.c,v 1.37 2001/10/16 05:44:44 davem Exp $
 * creatorfb.c: Creator/Creator3D frame buffer driver
 *
 * Copyright (C) 1997,1998,1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

#include <linux/module.h>
#include <linux/sched.h>
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
#include <linux/init.h>
#include <linux/selection.h>

#include <video/sbusfb.h>

#include <asm/upa.h>

#define	FFB_SFB8R_VOFF		0x00000000
#define	FFB_SFB8G_VOFF		0x00400000
#define	FFB_SFB8B_VOFF		0x00800000
#define	FFB_SFB8X_VOFF		0x00c00000
#define	FFB_SFB32_VOFF		0x01000000
#define	FFB_SFB64_VOFF		0x02000000
#define	FFB_FBC_REGS_VOFF	0x04000000
#define	FFB_BM_FBC_REGS_VOFF	0x04002000
#define	FFB_DFB8R_VOFF		0x04004000
#define	FFB_DFB8G_VOFF		0x04404000
#define	FFB_DFB8B_VOFF		0x04804000
#define	FFB_DFB8X_VOFF		0x04c04000
#define	FFB_DFB24_VOFF		0x05004000
#define	FFB_DFB32_VOFF		0x06004000
#define	FFB_DFB422A_VOFF	0x07004000	/* DFB 422 mode write to A */
#define	FFB_DFB422AD_VOFF	0x07804000	/* DFB 422 mode with line doubling */
#define	FFB_DFB24B_VOFF		0x08004000	/* DFB 24bit mode write to B */
#define	FFB_DFB422B_VOFF	0x09004000	/* DFB 422 mode write to B */
#define	FFB_DFB422BD_VOFF	0x09804000	/* DFB 422 mode with line doubling */
#define	FFB_SFB16Z_VOFF		0x0a004000	/* 16bit mode Z planes */
#define	FFB_SFB8Z_VOFF		0x0a404000	/* 8bit mode Z planes */
#define	FFB_SFB422_VOFF		0x0ac04000	/* SFB 422 mode write to A/B */
#define	FFB_SFB422D_VOFF	0x0b404000	/* SFB 422 mode with line doubling */
#define	FFB_FBC_KREGS_VOFF	0x0bc04000
#define	FFB_DAC_VOFF		0x0bc06000
#define	FFB_PROM_VOFF		0x0bc08000
#define	FFB_EXP_VOFF		0x0bc18000

#define	FFB_SFB8R_POFF		0x04000000UL
#define	FFB_SFB8G_POFF		0x04400000UL
#define	FFB_SFB8B_POFF		0x04800000UL
#define	FFB_SFB8X_POFF		0x04c00000UL
#define	FFB_SFB32_POFF		0x05000000UL
#define	FFB_SFB64_POFF		0x06000000UL
#define	FFB_FBC_REGS_POFF	0x00600000UL
#define	FFB_BM_FBC_REGS_POFF	0x00600000UL
#define	FFB_DFB8R_POFF		0x01000000UL
#define	FFB_DFB8G_POFF		0x01400000UL
#define	FFB_DFB8B_POFF		0x01800000UL
#define	FFB_DFB8X_POFF		0x01c00000UL
#define	FFB_DFB24_POFF		0x02000000UL
#define	FFB_DFB32_POFF		0x03000000UL
#define	FFB_FBC_KREGS_POFF	0x00610000UL
#define	FFB_DAC_POFF		0x00400000UL
#define	FFB_PROM_POFF		0x00000000UL
#define	FFB_EXP_POFF		0x00200000UL
#define FFB_DFB422A_POFF	0x09000000UL
#define FFB_DFB422AD_POFF	0x09800000UL
#define FFB_DFB24B_POFF		0x0a000000UL
#define FFB_DFB422B_POFF	0x0b000000UL
#define FFB_DFB422BD_POFF	0x0b800000UL
#define FFB_SFB16Z_POFF		0x0c800000UL
#define FFB_SFB8Z_POFF		0x0c000000UL
#define FFB_SFB422_POFF		0x0d000000UL
#define FFB_SFB422D_POFF	0x0d800000UL

/* Draw operations */
#define FFB_DRAWOP_DOT		0x00
#define FFB_DRAWOP_AADOT	0x01
#define FFB_DRAWOP_BRLINECAP	0x02
#define FFB_DRAWOP_BRLINEOPEN	0x03
#define FFB_DRAWOP_DDLINE	0x04
#define FFB_DRAWOP_AALINE	0x05
#define FFB_DRAWOP_TRIANGLE	0x06
#define FFB_DRAWOP_POLYGON	0x07
#define FFB_DRAWOP_RECTANGLE	0x08
#define FFB_DRAWOP_FASTFILL	0x09
#define FFB_DRAWOP_BCOPY	0x0a
#define FFB_DRAWOP_VSCROLL	0x0b

/* Pixel processor control */
/* Force WID */
#define FFB_PPC_FW_DISABLE	0x800000
#define FFB_PPC_FW_ENABLE	0xc00000
/* Auxiliary clip */
#define FFB_PPC_ACE_DISABLE	0x040000
#define FFB_PPC_ACE_AUX_SUB	0x080000
#define FFB_PPC_ACE_AUX_ADD	0x0c0000
/* Depth cue */
#define FFB_PPC_DCE_DISABLE	0x020000
#define FFB_PPC_DCE_ENABLE	0x030000
/* Alpha blend */
#define FFB_PPC_ABE_DISABLE	0x008000
#define FFB_PPC_ABE_ENABLE	0x00c000
/* View clip */
#define FFB_PPC_VCE_DISABLE	0x001000
#define FFB_PPC_VCE_2D		0x002000
#define FFB_PPC_VCE_3D		0x003000
/* Area pattern */
#define FFB_PPC_APE_DISABLE	0x000800
#define FFB_PPC_APE_ENABLE	0x000c00
/* Transparent background */
#define FFB_PPC_TBE_OPAQUE	0x000200
#define FFB_PPC_TBE_TRANSPARENT	0x000300
/* Z source */
#define FFB_PPC_ZS_VAR		0x000080
#define FFB_PPC_ZS_CONST	0x0000c0
/* Y source */
#define FFB_PPC_YS_VAR		0x000020
#define FFB_PPC_YS_CONST	0x000030
/* X source */
#define FFB_PPC_XS_WID		0x000004
#define FFB_PPC_XS_VAR		0x000008
#define FFB_PPC_XS_CONST	0x00000c
/* Color (BGR) source */
#define FFB_PPC_CS_VAR		0x000002
#define FFB_PPC_CS_CONST	0x000003

#define FFB_ROP_NEW                  0x83

#define FFB_UCSR_FIFO_MASK     0x00000fff
#define FFB_UCSR_FB_BUSY       0x01000000
#define FFB_UCSR_RP_BUSY       0x02000000
#define FFB_UCSR_ALL_BUSY      (FFB_UCSR_RP_BUSY|FFB_UCSR_FB_BUSY)
#define FFB_UCSR_READ_ERR      0x40000000
#define FFB_UCSR_FIFO_OVFL     0x80000000
#define FFB_UCSR_ALL_ERRORS    (FFB_UCSR_READ_ERR|FFB_UCSR_FIFO_OVFL)

struct ffb_fbc {
	/* Next vertex registers */
	u32		xxx1[3];
	volatile u32	alpha;
	volatile u32	red;
	volatile u32	green;
	volatile u32	blue;
	volatile u32	depth;
	volatile u32	y;
	volatile u32	x;
	u32		xxx2[2];
	volatile u32	ryf;
	volatile u32	rxf;
	u32		xxx3[2];
	
	volatile u32	dmyf;
	volatile u32	dmxf;
	u32		xxx4[2];
	volatile u32	ebyi;
	volatile u32	ebxi;
	u32		xxx5[2];
	volatile u32	by;
	volatile u32	bx;
	u32		dy;
	u32		dx;
	volatile u32	bh;
	volatile u32	bw;
	u32		xxx6[2];
	
	u32		xxx7[32];
	
	/* Setup unit vertex state register */
	volatile u32	suvtx;
	u32		xxx8[63];
	
	/* Control registers */
	volatile u32	ppc;
	volatile u32	wid;
	volatile u32	fg;
	volatile u32	bg;
	volatile u32	consty;
	volatile u32	constz;
	volatile u32	xclip;
	volatile u32	dcss;
	volatile u32	vclipmin;
	volatile u32	vclipmax;
	volatile u32	vclipzmin;
	volatile u32	vclipzmax;
	volatile u32	dcsf;
	volatile u32	dcsb;
	volatile u32	dczf;
	volatile u32	dczb;
	
	u32		xxx9;
	volatile u32	blendc;
	volatile u32	blendc1;
	volatile u32	blendc2;
	volatile u32	fbramitc;
	volatile u32	fbc;
	volatile u32	rop;
	volatile u32	cmp;
	volatile u32	matchab;
	volatile u32	matchc;
	volatile u32	magnab;
	volatile u32	magnc;
	volatile u32	fbcfg0;
	volatile u32	fbcfg1;
	volatile u32	fbcfg2;
	volatile u32	fbcfg3;
	
	u32		ppcfg;
	volatile u32	pick;
	volatile u32	fillmode;
	volatile u32	fbramwac;
	volatile u32	pmask;
	volatile u32	xpmask;
	volatile u32	ypmask;
	volatile u32	zpmask;
	volatile u32	clip0min;
	volatile u32	clip0max;
	volatile u32	clip1min;
	volatile u32	clip1max;
	volatile u32	clip2min;
	volatile u32	clip2max;
	volatile u32	clip3min;
	volatile u32	clip3max;
	
	/* New 3dRAM III support regs */
	volatile u32	rawblend2;
	volatile u32	rawpreblend;
	volatile u32	rawstencil;
	volatile u32	rawstencilctl;
	volatile u32	threedram1;
	volatile u32	threedram2;
	volatile u32	passin;
	volatile u32	rawclrdepth;
	volatile u32	rawpmask;
	volatile u32	rawcsrc;
	volatile u32	rawmatch;
	volatile u32	rawmagn;
	volatile u32	rawropblend;
	volatile u32	rawcmp;
	volatile u32	rawwac;
	volatile u32	fbramid;
	
	volatile u32	drawop;
	u32		xxx10[2];
	volatile u32	fontlpat;
	u32		xxx11;
	volatile u32	fontxy;
	volatile u32	fontw;
	volatile u32	fontinc;
	volatile u32	font;
	u32		xxx12[3];
	volatile u32	blend2;
	volatile u32	preblend;
	volatile u32	stencil;
	volatile u32	stencilctl;

	u32		xxx13[4];	
	volatile u32	dcss1;
	volatile u32	dcss2;
	volatile u32	dcss3;
	volatile u32	widpmask;
	volatile u32	dcs2;
	volatile u32	dcs3;
	volatile u32	dcs4;
	u32		xxx14;
	volatile u32	dcd2;
	volatile u32	dcd3;
	volatile u32	dcd4;
	u32		xxx15;
	
	volatile u32	pattern[32];
	
	u32		xxx16[256];
	
	volatile u32	devid;
	u32		xxx17[63];
	
	volatile u32	ucsr;
	u32		xxx18[31];
	
	volatile u32	mer;
};

static __inline__ void FFBFifo(struct fb_info_sbusfb *fb, int n)
{
	struct ffb_fbc *fbc;
	int cache = fb->s.ffb.fifo_cache;

	if (cache - n < 0) {
		fbc = fb->s.ffb.fbc;
		do {	cache = (upa_readl(&fbc->ucsr) & FFB_UCSR_FIFO_MASK) - 8;
		} while (cache - n < 0);
	}
	fb->s.ffb.fifo_cache = cache - n;
}

static __inline__ void FFBWait(struct ffb_fbc *ffb)
{
	int limit = 10000;

	do {
		if ((upa_readl(&ffb->ucsr) & FFB_UCSR_ALL_BUSY) == 0)
			break;
		if ((upa_readl(&ffb->ucsr) & FFB_UCSR_ALL_ERRORS) != 0) {
			upa_writel(FFB_UCSR_ALL_ERRORS, &ffb->ucsr);
		}
	} while(--limit > 0);
}

struct ffb_dac {
	volatile u32	type;
	volatile u32	value;
	volatile u32	type2;
	volatile u32	value2;
};

static struct sbus_mmap_map ffb_mmap_map[] = {
	{ FFB_SFB8R_VOFF,	FFB_SFB8R_POFF,		0x0400000 },
	{ FFB_SFB8G_VOFF,	FFB_SFB8G_POFF,		0x0400000 },
	{ FFB_SFB8B_VOFF,	FFB_SFB8B_POFF,		0x0400000 },
	{ FFB_SFB8X_VOFF,	FFB_SFB8X_POFF,		0x0400000 },
	{ FFB_SFB32_VOFF,	FFB_SFB32_POFF,		0x1000000 },
	{ FFB_SFB64_VOFF,	FFB_SFB64_POFF,		0x2000000 },
	{ FFB_FBC_REGS_VOFF,	FFB_FBC_REGS_POFF,	0x0002000 },
	{ FFB_BM_FBC_REGS_VOFF,	FFB_BM_FBC_REGS_POFF,	0x0002000 },
	{ FFB_DFB8R_VOFF,	FFB_DFB8R_POFF,		0x0400000 },
	{ FFB_DFB8G_VOFF,	FFB_DFB8G_POFF,		0x0400000 },
	{ FFB_DFB8B_VOFF,	FFB_DFB8B_POFF,		0x0400000 },
	{ FFB_DFB8X_VOFF,	FFB_DFB8X_POFF,		0x0400000 },
	{ FFB_DFB24_VOFF,	FFB_DFB24_POFF,		0x1000000 },
	{ FFB_DFB32_VOFF,	FFB_DFB32_POFF,		0x1000000 },
	{ FFB_FBC_KREGS_VOFF,	FFB_FBC_KREGS_POFF,	0x0002000 },
	{ FFB_DAC_VOFF,		FFB_DAC_POFF,		0x0002000 },
	{ FFB_PROM_VOFF,	FFB_PROM_POFF,		0x0010000 },
	{ FFB_EXP_VOFF,		FFB_EXP_POFF,		0x0002000 },
	{ FFB_DFB422A_VOFF,	FFB_DFB422A_POFF,	0x0800000 },
	{ FFB_DFB422AD_VOFF,	FFB_DFB422AD_POFF,	0x0800000 },
	{ FFB_DFB24B_VOFF,	FFB_DFB24B_POFF,	0x1000000 },
	{ FFB_DFB422B_VOFF,	FFB_DFB422B_POFF,	0x0800000 },
	{ FFB_DFB422BD_VOFF,	FFB_DFB422BD_POFF,	0x0800000 },
	{ FFB_SFB16Z_VOFF,	FFB_SFB16Z_POFF,	0x0800000 },
	{ FFB_SFB8Z_VOFF,	FFB_SFB8Z_POFF,		0x0800000 },
	{ FFB_SFB422_VOFF,	FFB_SFB422_POFF,	0x0800000 },
	{ FFB_SFB422D_VOFF,	FFB_SFB422D_POFF,	0x0800000 },
	{ 0,			0,			0	  }
};

static void ffb_setup(struct display *p)
{
	p->next_line = 8192;
	p->next_plane = 0;
}

static void ffb_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct ffb_fbc *fbc = fb->s.ffb.fbc;
	unsigned long flags;
	u64 yx, hw;
	int fg;
	
	spin_lock_irqsave(&fb->lock, flags);
	fg = ((u32 *)p->dispsw_data)[attr_bgcol_ec(p,conp)];
	if (fg != fb->s.ffb.fg_cache) {
		FFBFifo(fb, 5);
		upa_writel(fg, &fbc->fg);
		fb->s.ffb.fg_cache = fg;
	} else
		FFBFifo(fb, 4);

	if (fontheightlog(p)) {
		yx = (u64)sy << (fontheightlog(p) + 32); hw = (u64)height << (fontheightlog(p) + 32);
	} else {
		yx = (u64)(sy * fontheight(p)) << 32; hw = (u64)(height * fontheight(p)) << 32;
	}
	if (fontwidthlog(p)) {
		yx += sx << fontwidthlog(p); hw += width << fontwidthlog(p);
	} else {
		yx += sx * fontwidth(p); hw += width * fontwidth(p);
	}
	upa_writeq(yx + fb->s.ffb.yx_margin, &fbc->by);
	upa_writeq(hw, &fbc->bh);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void ffb_fill(struct fb_info_sbusfb *fb, struct display *p, int s,
		     int count, unsigned short *boxes)
{
	register struct ffb_fbc *fbc = fb->s.ffb.fbc;
	unsigned long flags;
	int fg;

	spin_lock_irqsave(&fb->lock, flags);
	fg = ((u32 *)p->dispsw_data)[attr_bgcol(p,s)];
	if (fg != fb->s.ffb.fg_cache) {
		FFBFifo(fb, 1);
		upa_writel(fg, &fbc->fg);
		fb->s.ffb.fg_cache = fg;
	}
	while (count-- > 0) {
		FFBFifo(fb, 4);
		upa_writel(boxes[1], &fbc->by);
		upa_writel(boxes[0], &fbc->bx);
		upa_writel(boxes[3] - boxes[1], &fbc->bh);
		upa_writel(boxes[2] - boxes[0], &fbc->bw);
		boxes += 4;
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void ffb_putc(struct vc_data *conp, struct display *p, int c, int yy, int xx)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct ffb_fbc *fbc = fb->s.ffb.fbc;
	unsigned long flags;
	int i, xy;
	u8 *fd;
	u64 fgbg;

	spin_lock_irqsave(&fb->lock, flags);
	if (fontheightlog(p)) {
		xy = (yy << (16 + fontheightlog(p)));
		i = ((c & p->charmask) << fontheightlog(p));
	} else {
		xy = ((yy * fontheight(p)) << 16);
		i = (c & p->charmask) * fontheight(p);
	}
	if (fontwidth(p) <= 8)
		fd = p->fontdata + i;
	else
		fd = p->fontdata + (i << 1);
	if (fontwidthlog(p))
		xy += (xx << fontwidthlog(p)) + fb->s.ffb.xy_margin;
	else
		xy += (xx * fontwidth(p)) + fb->s.ffb.xy_margin;
	fgbg = (((u64)(((u32 *)p->dispsw_data)[attr_fgcol(p,c)])) << 32) |
	       ((u32 *)p->dispsw_data)[attr_bgcol(p,c)];
	if (fgbg != *(u64 *)&fb->s.ffb.fg_cache) {
		FFBFifo(fb, 2);
		upa_writeq(fgbg, &fbc->fg);
		*(u64 *)&fb->s.ffb.fg_cache = fgbg;
	}
	FFBFifo(fb, 2 + fontheight(p));
	upa_writel(xy, &fbc->fontxy);
	upa_writel(fontwidth(p), &fbc->fontw);
	if (fontwidth(p) <= 8) {
		for (i = 0; i < fontheight(p); i++) {
			u32 val = *fd++ << 24;

			upa_writel(val, &fbc->font);
		}
	} else {
		for (i = 0; i < fontheight(p); i++) {
			u32 val = *(u16 *)fd << 16;

			upa_writel(val, &fbc->font);
			fd += 2;
		}
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void ffb_putcs(struct vc_data *conp, struct display *p, const unsigned short *s,
		      int count, int yy, int xx)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct ffb_fbc *fbc = fb->s.ffb.fbc;
	unsigned long flags;
	int i, xy;
	u8 *fd1, *fd2, *fd3, *fd4;
	u16 c;
	u64 fgbg;

	spin_lock_irqsave(&fb->lock, flags);
	c = scr_readw(s);
	fgbg = (((u64)(((u32 *)p->dispsw_data)[attr_fgcol(p, c)])) << 32) |
	       ((u32 *)p->dispsw_data)[attr_bgcol(p, c)];
	if (fgbg != *(u64 *)&fb->s.ffb.fg_cache) {
		FFBFifo(fb, 2);
		upa_writeq(fgbg, &fbc->fg);
		*(u64 *)&fb->s.ffb.fg_cache = fgbg;
	}
	xy = fb->s.ffb.xy_margin;
	if (fontwidthlog(p))
		xy += (xx << fontwidthlog(p));
	else
		xy += xx * fontwidth(p);
	if (fontheightlog(p))
		xy += (yy << (16 + fontheightlog(p)));
	else
		xy += ((yy * fontheight(p)) << 16);
	if (fontwidth(p) <= 8) {
		while (count >= 4) {
			count -= 4;
			FFBFifo(fb, 2 + fontheight(p));
			upa_writel(4 * fontwidth(p), &fbc->fontw);
			upa_writel(xy, &fbc->fontxy);
			if (fontheightlog(p)) {
				fd1 = p->fontdata + ((scr_readw(s++) & p->charmask) << fontheightlog(p));
				fd2 = p->fontdata + ((scr_readw(s++) & p->charmask) << fontheightlog(p));
				fd3 = p->fontdata + ((scr_readw(s++) & p->charmask) << fontheightlog(p));
				fd4 = p->fontdata + ((scr_readw(s++) & p->charmask) << fontheightlog(p));
			} else {
				fd1 = p->fontdata + ((scr_readw(s++) & p->charmask) * fontheight(p));
				fd2 = p->fontdata + ((scr_readw(s++) & p->charmask) * fontheight(p));
				fd3 = p->fontdata + ((scr_readw(s++) & p->charmask) * fontheight(p));
				fd4 = p->fontdata + ((scr_readw(s++) & p->charmask) * fontheight(p));
			}
			if (fontwidth(p) == 8) {
				for (i = 0; i < fontheight(p); i++) {
					u32 val;

					val = ((u32)*fd4++) | ((((u32)*fd3++) | ((((u32)*fd2++) | (((u32)*fd1++) 
						<< 8)) << 8)) << 8);
					upa_writel(val, &fbc->font);
				}
				xy += 32;
			} else {
				for (i = 0; i < fontheight(p); i++) {
					u32 val = (((u32)*fd4++) | ((((u32)*fd3++) | ((((u32)*fd2++) | (((u32)*fd1++) 
						<< fontwidth(p))) << fontwidth(p))) << fontwidth(p))) << (24 - 3 * fontwidth(p));
					upa_writel(val, &fbc->font);
				}
				xy += 4 * fontwidth(p);
			}
		}
	} else {
		while (count >= 2) {
			count -= 2;
			FFBFifo(fb, 2 + fontheight(p));
			upa_writel(2 * fontwidth(p), &fbc->fontw);
			upa_writel(xy, &fbc->fontxy);
			if (fontheightlog(p)) {
				fd1 = p->fontdata + ((scr_readw(s++) & p->charmask) << (fontheightlog(p) + 1));
				fd2 = p->fontdata + ((scr_readw(s++) & p->charmask) << (fontheightlog(p) + 1));
			} else {
				fd1 = p->fontdata + (((scr_readw(s++) & p->charmask) * fontheight(p)) << 1);
				fd2 = p->fontdata + (((scr_readw(s++) & p->charmask) * fontheight(p)) << 1);
			}
			for (i = 0; i < fontheight(p); i++) {
				u32 val = ((((u32)*(u16 *)fd1) << fontwidth(p)) | ((u32)*(u16 *)fd2)) << (16 - fontwidth(p));

				upa_writel(val, &fbc->font);
				fd1 += 2; fd2 += 2;
			}
			xy += 2 * fontwidth(p);
		}
	}
	while (count) {
		count--;
		FFBFifo(fb, 2 + fontheight(p));
		upa_writel(fontwidth(p), &fbc->fontw);
		upa_writel(xy, &fbc->fontxy);
		if (fontheightlog(p))
			i = ((scr_readw(s++) & p->charmask) << fontheightlog(p));
		else
			i = ((scr_readw(s++) & p->charmask) * fontheight(p));
		if (fontwidth(p) <= 8) {
			fd1 = p->fontdata + i;
			for (i = 0; i < fontheight(p); i++) {
				u32 val = *fd1++ << 24;

				upa_writel(val, &fbc->font);
			}
		} else {
			fd1 = p->fontdata + (i << 1);
			for (i = 0; i < fontheight(p); i++) {
				u32 val = *(u16 *)fd1 << 16;

				upa_writel(val, &fbc->font);
				fd1 += 2;
			}
		}
		xy += fontwidth(p);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void ffb_revc(struct display *p, int xx, int yy)
{
	/* Not used if hw cursor */
}

#if 0
static void ffb_blank(struct fb_info_sbusfb *fb)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	upa_writel(0x6000, &dac->type);
	tmp = (upa_readl(&dac->value) & ~0x1);
	upa_writel(0x6000, &dac->type);
	upa_writel(tmp, &dac->value);
	spin_unlock_irqrestore(&fb->lock, flags);
}
#endif

static void ffb_unblank(struct fb_info_sbusfb *fb)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	upa_writel(0x6000, &dac->type);
	tmp = (upa_readl(&dac->value) | 0x1);
	upa_writel(0x6000, &dac->type);
	upa_writel(tmp, &dac->value);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void ffb_loadcmap (struct fb_info_sbusfb *fb, struct display *p, int index, int count)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	unsigned long flags;
	int i, j = count;
	
	spin_lock_irqsave(&fb->lock, flags);
	upa_writel(0x2000 | index, &dac->type);
	for (i = index; j--; i++) {
		u32 val;

		/* Feed the colors in :)) */
		val = ((fb->color_map CM(i,0))) |
			((fb->color_map CM(i,1)) << 8) |
			((fb->color_map CM(i,2)) << 16);
		upa_writel(val, &dac->value);
	}
	if (!p)
		goto out;
	for (i = index, j = count; i < 16 && j--; i++)
		((u32 *)p->dispsw_data)[i] = ((fb->color_map CM(i,0))) |
			      		     ((fb->color_map CM(i,1)) << 8) |
					     ((fb->color_map CM(i,2)) << 16);
out:
	spin_unlock_irqrestore(&fb->lock, flags);
}

static struct display_switch ffb_dispsw __initdata = {
	setup:		ffb_setup,
	bmove:		fbcon_redraw_bmove,
	clear:		ffb_clear,
	putc:		ffb_putc,
	putcs:		ffb_putcs,
	revc:		ffb_revc, 
	fontwidthmask:	FONTWIDTHRANGE(1,16) /* Allow fontwidths up to 16 */
};

static void ffb_margins (struct fb_info_sbusfb *fb, struct display *p, int x_margin, int y_margin)
{
	register struct ffb_fbc *fbc = fb->s.ffb.fbc;
	unsigned long flags;

	spin_lock_irqsave(&fb->lock, flags);
	fb->s.ffb.xy_margin = (y_margin << 16) + x_margin;
	fb->s.ffb.yx_margin = (((u64)y_margin) << 32) + x_margin;
	p->screen_base += 8192 * (y_margin - fb->y_margin) + 4 * (x_margin - fb->x_margin);
	FFBWait(fbc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static __inline__ void __ffb_curs_enable (struct fb_info_sbusfb *fb, int enable)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	u32 val;

	upa_writel(0x100, &dac->type2);
	if (fb->s.ffb.dac_rev <= 2) {
		val = enable ? 3 : 0;
	} else {
		val = enable ? 0 : 3;
	}
	upa_writel(val, &dac->value2);
}

static void ffb_setcursormap (struct fb_info_sbusfb *fb, u8 *red, u8 *green, u8 *blue)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	unsigned long flags;
	
	spin_lock_irqsave(&fb->lock, flags);
	__ffb_curs_enable (fb, 0);
	upa_writel(0x102, &dac->type2);
	upa_writel((red[0] | (green[0]<<8) | (blue[0]<<16)), &dac->value2);
	upa_writel((red[1] | (green[1]<<8) | (blue[1]<<16)), &dac->value2);
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Set cursor shape */
static void ffb_setcurshape (struct fb_info_sbusfb *fb)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	unsigned long flags;
	int i, j;

	spin_lock_irqsave(&fb->lock, flags);
	__ffb_curs_enable (fb, 0);
	for (j = 0; j < 2; j++) {
		u32 val = j ? 0 : 0x80;

		upa_writel(val, &dac->type2);
		for (i = 0; i < 0x40; i++) {
			if (fb->cursor.size.fbx <= 32) {
				upa_writel(fb->cursor.bits [j][i], &dac->value2);
				upa_writel(0, &dac->value2);
			} else {
				upa_writel(fb->cursor.bits [j][2*i], &dac->value2);
				upa_writel(fb->cursor.bits [j][2*i+1], &dac->value2);
			}
		}
	}	
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Load cursor information */
static void ffb_setcursor (struct fb_info_sbusfb *fb)
{
	struct ffb_dac *dac = fb->s.ffb.dac;
	struct cg_cursor *c = &fb->cursor;
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&fb->lock, flags);
	upa_writel(0x104, &dac->type2);
	/* Should this be just 0x7ff?? 
	   Should I do some margin handling and setcurshape in that case? */
	val = (((c->cpos.fby - c->chot.fby) & 0xffff) << 16)
		|((c->cpos.fbx - c->chot.fbx) & 0xffff);
	upa_writel(val, &dac->value2);
	__ffb_curs_enable (fb, fb->cursor.enable);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void ffb_switch_from_graph (struct fb_info_sbusfb *fb)
{
	register struct ffb_fbc *fbc = fb->s.ffb.fbc;
	unsigned long flags;

	spin_lock_irqsave(&fb->lock, flags);
	FFBWait(fbc);
	fb->s.ffb.fifo_cache = 0;
	FFBFifo(fb, 8);
	upa_writel(FFB_PPC_VCE_DISABLE|FFB_PPC_TBE_OPAQUE|
		   FFB_PPC_APE_DISABLE|FFB_PPC_CS_CONST,
		   &fbc->ppc);
	upa_writel(0x2000707f, &fbc->fbc);
	upa_writel(FFB_ROP_NEW, &fbc->rop);
	upa_writel(FFB_DRAWOP_RECTANGLE, &fbc->drawop);
	upa_writel(0xffffffff, &fbc->pmask);
	upa_writel(0x10000, &fbc->fontinc);
	upa_writel(fb->s.ffb.fg_cache, &fbc->fg);
	upa_writel(fb->s.ffb.bg_cache, &fbc->bg);
	FFBWait(fbc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static int __init ffb_rasterimg (struct fb_info *info, int start)
{
	ffb_switch_from_graph (sbusfbinfo(info));
	return 0;
}

static char idstring[60] __initdata = { 0 };

static int __init creator_apply_upa_parent_ranges(int parent, struct linux_prom64_registers *regs)
{
	struct linux_prom64_ranges ranges[PROMREG_MAX];
	char name[128];
	int len, i;

	prom_getproperty(parent, "name", name, sizeof(name));
	if (strcmp(name, "upa") != 0)
		return 0;

	len = prom_getproperty(parent, "ranges", (void *) ranges, sizeof(ranges));
	if (len <= 0)
		return 1;

	len /= sizeof(struct linux_prom64_ranges);
	for (i = 0; i < len; i++) {
		struct linux_prom64_ranges *rng = &ranges[i];
		u64 phys_addr = regs->phys_addr;

		if (phys_addr >= rng->ot_child_base &&
		    phys_addr < (rng->ot_child_base + rng->or_size)) {
			regs->phys_addr -= rng->ot_child_base;
			regs->phys_addr += rng->ot_parent_base;
			return 0;
		}
	}

	return 1;
}

char __init *creatorfb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
	struct linux_prom64_registers regs[2*PROMREG_MAX];
	int i, afb = 0;
	unsigned int btype;
	char name[64];
	struct fb_ops *fbops;

	if (prom_getproperty(fb->prom_node, "reg", (void *) regs, sizeof(regs)) <= 0)
		return NULL;

	if (creator_apply_upa_parent_ranges(fb->prom_parent, &regs[0]))
		return NULL;
		
	disp->dispsw_data = (void *)kmalloc(16 * sizeof(u32), GFP_KERNEL);
	if (disp->dispsw_data == NULL)
		return NULL;
	memset(disp->dispsw_data, 0, 16 * sizeof(u32));

	fbops = kmalloc(sizeof(*fbops), GFP_KERNEL);
	if (fbops == NULL) {
		kfree(disp->dispsw_data);
		return NULL;
	}
	
	*fbops = *fb->info.fbops;
	fbops->fb_rasterimg = ffb_rasterimg;
	fb->info.fbops = fbops;

	prom_getstring(fb->prom_node, "name", name, sizeof(name));
	if (!strcmp(name, "SUNW,afb"))
		afb = 1;
		
	btype = prom_getintdefault(fb->prom_node, "board_type", 0);
		
	strcpy(fb->info.modename, "Creator");
	if (!afb) {
		if ((btype & 7) == 3)
		    strcpy(fix->id, "Creator 3D");
		else
		    strcpy(fix->id, "Creator");
	} else
		strcpy(fix->id, "Elite 3D");
	
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->line_length = 8192;
	fix->accel = FB_ACCEL_SUN_CREATOR;
	
	var->bits_per_pixel = 32;
	var->green.offset = 8;
	var->blue.offset = 16;
	var->accel_flags = FB_ACCELF_TEXT;
	
	disp->scrollmode = SCROLL_YREDRAW;
	disp->screen_base = (char *)__va(regs[0].phys_addr) + FFB_DFB24_POFF + 8192 * fb->y_margin + 4 * fb->x_margin;
	fb->s.ffb.xy_margin = (fb->y_margin << 16) + fb->x_margin;
	fb->s.ffb.yx_margin = (((u64)fb->y_margin) << 32) + fb->x_margin;
	fb->s.ffb.fbc = (struct ffb_fbc *)(regs[0].phys_addr + FFB_FBC_REGS_POFF);
	fb->s.ffb.dac = (struct ffb_dac *)(regs[0].phys_addr + FFB_DAC_POFF);
	fb->dispsw = ffb_dispsw;

	fb->margins = ffb_margins;
	fb->loadcmap = ffb_loadcmap;
	fb->setcursor = ffb_setcursor;
	fb->setcursormap = ffb_setcursormap;
	fb->setcurshape = ffb_setcurshape;
	fb->switch_from_graph = ffb_switch_from_graph;
	fb->fill = ffb_fill;
#if 0
	/* XXX Can't enable this for now, I've seen cases
	 * XXX where the VC was blanked, and Xsun24 was started
	 * XXX via a remote login, the sunfb code did not
	 * XXX unblank creator when it was mmap'd for some
	 * XXX reason, investigate later... -DaveM
	 */
	fb->blank = ffb_blank;
	fb->unblank = ffb_unblank;
#endif
	
	/* If there are any read errors or fifo overflow conditions,
	 * clear them now.
	 */
	if((upa_readl(&fb->s.ffb.fbc->ucsr) & FFB_UCSR_ALL_ERRORS) != 0)
		upa_writel(FFB_UCSR_ALL_ERRORS, &fb->s.ffb.fbc->ucsr);

	ffb_switch_from_graph(fb);
	
	fb->physbase = regs[0].phys_addr;
	fb->mmap_map = ffb_mmap_map;
	
	fb->cursor.hwsize.fbx = 64;
	fb->cursor.hwsize.fby = 64;
	
	type->fb_depth = 24;
	
	upa_writel(0x8000, &fb->s.ffb.dac->type);
	fb->s.ffb.dac_rev = (upa_readl(&fb->s.ffb.dac->value) >> 0x1c);
	                
	i = prom_getintdefault (fb->prom_node, "board_type", 8);

	sprintf(idstring, "%s at %016lx type %d DAC %d",
		fix->id, regs[0].phys_addr, i, fb->s.ffb.dac_rev);
	
	/* Elite3D has different DAC revision numbering, and no DAC revisions
	   have the reversed meaning of cursor enable */
	if (afb)
		fb->s.ffb.dac_rev = 10;
	
	/* Unblank it just to be sure.  When there are multiple
	 * FFB/AFB cards in the system, or it is not the OBP
	 * chosen console, it will have video outputs off in
	 * the DAC.
	 */
	ffb_unblank(fb);

	return idstring;
}

MODULE_LICENSE("GPL");
