/* $Id: cgsixfb.c,v 1.26 2001/10/16 05:44:44 davem Exp $
 * cgsixfb.c: CGsix (GX,GXplus) frame buffer driver
 *
 * Copyright (C) 1996,1998 Jakub Jelinek (jj@ultra.linux.cz)
 * Copyright (C) 1996 Miguel de Icaza (miguel@nuclecu.unam.mx)
 * Copyright (C) 1996 Eddie C. Dost (ecd@skynet.be)
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
#include <asm/io.h>

/* Offset of interesting structures in the OBIO space */
/*
 * Brooktree is the video dac and is funny to program on the cg6.
 * (it's even funnier on the cg3)
 * The FBC could be the frame buffer control
 * The FHC could is the frame buffer hardware control.
 */
#define CG6_ROM_OFFSET       0x0UL
#define CG6_BROOKTREE_OFFSET 0x200000UL
#define CG6_DHC_OFFSET       0x240000UL
#define CG6_ALT_OFFSET       0x280000UL
#define CG6_FHC_OFFSET       0x300000UL
#define CG6_THC_OFFSET       0x301000UL
#define CG6_FBC_OFFSET       0x700000UL
#define CG6_TEC_OFFSET       0x701000UL
#define CG6_RAM_OFFSET       0x800000UL

/* FHC definitions */
#define CG6_FHC_FBID_SHIFT           24
#define CG6_FHC_FBID_MASK            255
#define CG6_FHC_REV_SHIFT            20
#define CG6_FHC_REV_MASK             15
#define CG6_FHC_FROP_DISABLE         (1 << 19)
#define CG6_FHC_ROW_DISABLE          (1 << 18)
#define CG6_FHC_SRC_DISABLE          (1 << 17)
#define CG6_FHC_DST_DISABLE          (1 << 16)
#define CG6_FHC_RESET                (1 << 15)
#define CG6_FHC_LITTLE_ENDIAN        (1 << 13)
#define CG6_FHC_RES_MASK             (3 << 11)
#define CG6_FHC_1024                 (0 << 11)
#define CG6_FHC_1152                 (1 << 11)
#define CG6_FHC_1280                 (2 << 11)
#define CG6_FHC_1600                 (3 << 11)
#define CG6_FHC_CPU_MASK             (3 << 9)
#define CG6_FHC_CPU_SPARC            (0 << 9)
#define CG6_FHC_CPU_68020            (1 << 9)
#define CG6_FHC_CPU_386              (2 << 9)
#define CG6_FHC_TEST		     (1 << 8)
#define CG6_FHC_TEST_X_SHIFT	     4
#define CG6_FHC_TEST_X_MASK	     15
#define CG6_FHC_TEST_Y_SHIFT	     0
#define CG6_FHC_TEST_Y_MASK	     15

/* FBC mode definitions */
#define CG6_FBC_BLIT_IGNORE		0x00000000
#define CG6_FBC_BLIT_NOSRC		0x00100000
#define CG6_FBC_BLIT_SRC		0x00200000
#define CG6_FBC_BLIT_ILLEGAL		0x00300000
#define CG6_FBC_BLIT_MASK		0x00300000

#define CG6_FBC_VBLANK			0x00080000

#define CG6_FBC_MODE_IGNORE		0x00000000
#define CG6_FBC_MODE_COLOR8		0x00020000
#define CG6_FBC_MODE_COLOR1		0x00040000
#define CG6_FBC_MODE_HRMONO		0x00060000
#define CG6_FBC_MODE_MASK		0x00060000

#define CG6_FBC_DRAW_IGNORE		0x00000000
#define CG6_FBC_DRAW_RENDER		0x00008000
#define CG6_FBC_DRAW_PICK		0x00010000
#define CG6_FBC_DRAW_ILLEGAL		0x00018000
#define CG6_FBC_DRAW_MASK		0x00018000

#define CG6_FBC_BWRITE0_IGNORE		0x00000000
#define CG6_FBC_BWRITE0_ENABLE		0x00002000
#define CG6_FBC_BWRITE0_DISABLE		0x00004000
#define CG6_FBC_BWRITE0_ILLEGAL		0x00006000
#define CG6_FBC_BWRITE0_MASK		0x00006000

#define CG6_FBC_BWRITE1_IGNORE		0x00000000
#define CG6_FBC_BWRITE1_ENABLE		0x00000800
#define CG6_FBC_BWRITE1_DISABLE		0x00001000
#define CG6_FBC_BWRITE1_ILLEGAL		0x00001800
#define CG6_FBC_BWRITE1_MASK		0x00001800

#define CG6_FBC_BREAD_IGNORE		0x00000000
#define CG6_FBC_BREAD_0			0x00000200
#define CG6_FBC_BREAD_1			0x00000400
#define CG6_FBC_BREAD_ILLEGAL		0x00000600
#define CG6_FBC_BREAD_MASK		0x00000600

#define CG6_FBC_BDISP_IGNORE		0x00000000
#define CG6_FBC_BDISP_0			0x00000080
#define CG6_FBC_BDISP_1			0x00000100
#define CG6_FBC_BDISP_ILLEGAL		0x00000180
#define CG6_FBC_BDISP_MASK		0x00000180

#define CG6_FBC_INDEX_MOD		0x00000040
#define CG6_FBC_INDEX_MASK		0x00000030

/* THC definitions */
#define CG6_THC_MISC_REV_SHIFT       16
#define CG6_THC_MISC_REV_MASK        15
#define CG6_THC_MISC_RESET           (1 << 12)
#define CG6_THC_MISC_VIDEO           (1 << 10)
#define CG6_THC_MISC_SYNC            (1 << 9)
#define CG6_THC_MISC_VSYNC           (1 << 8)
#define CG6_THC_MISC_SYNC_ENAB       (1 << 7)
#define CG6_THC_MISC_CURS_RES        (1 << 6)
#define CG6_THC_MISC_INT_ENAB        (1 << 5)
#define CG6_THC_MISC_INT             (1 << 4)
#define CG6_THC_MISC_INIT            0x9f

MODULE_LICENSE("GPL");

/* The contents are unknown */
struct cg6_tec {
	volatile int tec_matrix;
	volatile int tec_clip;
	volatile int tec_vdc;
};

struct cg6_thc {
        uint thc_pad0[512];
	volatile uint thc_hs;		/* hsync timing */
	volatile uint thc_hsdvs;
	volatile uint thc_hd;
	volatile uint thc_vs;		/* vsync timing */
	volatile uint thc_vd;
	volatile uint thc_refresh;
	volatile uint thc_misc;
	uint thc_pad1[56];
	volatile uint thc_cursxy;	/* cursor x,y position (16 bits each) */
	volatile uint thc_cursmask[32];	/* cursor mask bits */
	volatile uint thc_cursbits[32];	/* what to show where mask enabled */
};

struct cg6_fbc {
	u32		xxx0[1];
	volatile u32	mode;
	volatile u32	clip;
	u32		xxx1[1];	    
	volatile u32	s;
	volatile u32	draw;
	volatile u32	blit;
	volatile u32	font;
	u32		xxx2[24];
	volatile u32	x0, y0, z0, color0;
	volatile u32	x1, y1, z1, color1;
	volatile u32	x2, y2, z2, color2;
	volatile u32	x3, y3, z3, color3;
	volatile u32	offx, offy;
	u32		xxx3[2];
	volatile u32	incx, incy;
	u32		xxx4[2];
	volatile u32	clipminx, clipminy;
	u32		xxx5[2];
	volatile u32	clipmaxx, clipmaxy;
	u32		xxx6[2];
	volatile u32	fg;
	volatile u32	bg;
	volatile u32	alu;
	volatile u32	pm;
	volatile u32	pixelm;
	u32		xxx7[2];
	volatile u32	patalign;
	volatile u32	pattern[8];
	u32		xxx8[432];
	volatile u32	apointx, apointy, apointz;
	u32		xxx9[1];
	volatile u32	rpointx, rpointy, rpointz;
	u32		xxx10[5];
	volatile u32	pointr, pointg, pointb, pointa;
	volatile u32	alinex, aliney, alinez;
	u32		xxx11[1];
	volatile u32	rlinex, rliney, rlinez;
	u32		xxx12[5];
	volatile u32	liner, lineg, lineb, linea;
	volatile u32	atrix, atriy, atriz;
	u32		xxx13[1];
	volatile u32	rtrix, rtriy, rtriz;
	u32		xxx14[5];
	volatile u32	trir, trig, trib, tria;
	volatile u32	aquadx, aquady, aquadz;
	u32		xxx15[1];
	volatile u32	rquadx, rquady, rquadz;
	u32		xxx16[5];
	volatile u32	quadr, quadg, quadb, quada;
	volatile u32	arectx, arecty, arectz;
	u32		xxx17[1];
	volatile u32	rrectx, rrecty, rrectz;
	u32		xxx18[5];
	volatile u32	rectr, rectg, rectb, recta;
};

static struct sbus_mmap_map cg6_mmap_map[] = {
	{ CG6_FBC,		CG6_FBC_OFFSET,		PAGE_SIZE 		},
	{ CG6_TEC,		CG6_TEC_OFFSET,		PAGE_SIZE 		},
	{ CG6_BTREGS,		CG6_BROOKTREE_OFFSET,	PAGE_SIZE 		},
	{ CG6_FHC,		CG6_FHC_OFFSET,		PAGE_SIZE 		},
	{ CG6_THC,		CG6_THC_OFFSET,		PAGE_SIZE 		},
	{ CG6_ROM,		CG6_ROM_OFFSET,		0x10000   		},
	{ CG6_RAM,		CG6_RAM_OFFSET,		SBUS_MMAP_FBSIZE(1)  	},
	{ CG6_DHC,		CG6_DHC_OFFSET,		0x40000   		},
	{ 0,			0,			0	  		}
};

static void cg6_setup(struct display *p)
{
	p->next_line = sbusfbinfo(p->fb_info)->var.xres_virtual;
	p->next_plane = 0;
}

static void cg6_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct cg6_fbc *fbc = fb->s.cg6.fbc;
	unsigned long flags;
	int x, y, w, h;
	int i;
	
	spin_lock_irqsave(&fb->lock, flags);
	do {
		i = sbus_readl(&fbc->s);
	} while (i & 0x10000000);
	sbus_writel(attr_bgcol_ec(p,conp), &fbc->fg);
	sbus_writel(attr_bgcol_ec(p,conp), &fbc->bg);
	sbus_writel(~0, &fbc->pixelm);
	sbus_writel(0xea80ff00, &fbc->alu);
	sbus_writel(0, &fbc->s);
	sbus_writel(0, &fbc->clip);
	sbus_writel(~0, &fbc->pm);

        if (fontheightlog(p)) {
		y = sy << fontheightlog(p); h = height << fontheightlog(p);
	} else {
		y = sy * fontheight(p); h = height * fontheight(p);
	}
	if (fontwidthlog(p)) {
		x = sx << fontwidthlog(p); w = width << fontwidthlog(p);
	} else {
		x = sx * fontwidth(p); w = width * fontwidth(p);
	}
	sbus_writel(y + fb->y_margin, &fbc->arecty);
	sbus_writel(x + fb->x_margin, &fbc->arectx);
	sbus_writel(y + fb->y_margin + h, &fbc->arecty);
	sbus_writel(x + fb->x_margin + w, &fbc->arectx);
	do {
		i = sbus_readl(&fbc->draw);
	} while (i < 0 && (i & 0x20000000));
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_fill(struct fb_info_sbusfb *fb, struct display *p, int s,
		     int count, unsigned short *boxes)
{
	int i;
	register struct cg6_fbc *fbc = fb->s.cg6.fbc;
	unsigned long flags;
	
	spin_lock_irqsave(&fb->lock, flags);
	do {
		i = sbus_readl(&fbc->s);
	} while (i & 0x10000000);
	sbus_writel(attr_bgcol(p,s), &fbc->fg);
	sbus_writel(attr_bgcol(p,s), &fbc->bg);
	sbus_writel(~0, &fbc->pixelm);
	sbus_writel(0xea80ff00, &fbc->alu);
	sbus_writel(0, &fbc->s);
	sbus_writel(0, &fbc->clip);
	sbus_writel(~0, &fbc->pm);
	while (count-- > 0) {
		sbus_writel(boxes[1], &fbc->arecty);
		sbus_writel(boxes[0], &fbc->arectx);
		sbus_writel(boxes[3], &fbc->arecty);
		sbus_writel(boxes[2], &fbc->arectx);
		boxes += 4;
		do {
			i = sbus_readl(&fbc->draw);
		} while (i < 0 && (i & 0x20000000));
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_putc(struct vc_data *conp, struct display *p, int c, int yy, int xx)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct cg6_fbc *fbc = fb->s.cg6.fbc;
	unsigned long flags;
	int i, x, y;
	u8 *fd;

	spin_lock_irqsave(&fb->lock, flags);
	if (fontheightlog(p)) {
		y = fb->y_margin + (yy << fontheightlog(p));
		i = ((c & p->charmask) << fontheightlog(p));
	} else {
		y = fb->y_margin + (yy * fontheight(p));
		i = (c & p->charmask) * fontheight(p);
	}
	if (fontwidth(p) <= 8)
		fd = p->fontdata + i;
	else
		fd = p->fontdata + (i << 1);
	if (fontwidthlog(p))
		x = fb->x_margin + (xx << fontwidthlog(p));
	else
		x = fb->x_margin + (xx * fontwidth(p));
	do {
		i = sbus_readl(&fbc->s);
	} while (i & 0x10000000);
	sbus_writel(attr_fgcol(p,c), &fbc->fg);
	sbus_writel(attr_bgcol(p,c), &fbc->bg);
	sbus_writel(0x140000, &fbc->mode);
	sbus_writel(0xe880fc30, &fbc->alu);
	sbus_writel(~0, &fbc->pixelm);
	sbus_writel(0, &fbc->s);
	sbus_writel(0, &fbc->clip);
	sbus_writel(0xff, &fbc->pm);
	sbus_writel(0, &fbc->incx);
	sbus_writel(1, &fbc->incy);
	sbus_writel(x, &fbc->x0);
	sbus_writel(x + fontwidth(p) - 1, &fbc->x1);
	sbus_writel(y, &fbc->y0);
	if (fontwidth(p) <= 8) {
		for (i = 0; i < fontheight(p); i++) {
			u32 val = *fd++ << 24;
			sbus_writel(val, &fbc->font);
		}
	} else {
		for (i = 0; i < fontheight(p); i++) {
			u32 val = *(u16 *)fd << 16;

			sbus_writel(val, &fbc->font);
			fd += 2;
		}
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_putcs(struct vc_data *conp, struct display *p, const unsigned short *s,
		      int count, int yy, int xx)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct cg6_fbc *fbc = fb->s.cg6.fbc;
	unsigned long flags;
	int i, x, y;
	u8 *fd1, *fd2, *fd3, *fd4;
	u16 c;

	spin_lock_irqsave(&fb->lock, flags);
	do {
		i = sbus_readl(&fbc->s);
	} while (i & 0x10000000);
	c = scr_readw(s);
	sbus_writel(attr_fgcol(p, c), &fbc->fg);
	sbus_writel(attr_bgcol(p, c), &fbc->bg);
	sbus_writel(0x140000, &fbc->mode);
	sbus_writel(0xe880fc30, &fbc->alu);
	sbus_writel(~0, &fbc->pixelm);
	sbus_writel(0, &fbc->s);
	sbus_writel(0, &fbc->clip);
	sbus_writel(0xff, &fbc->pm);
	x = fb->x_margin;
	y = fb->y_margin;
	if (fontwidthlog(p))
		x += (xx << fontwidthlog(p));
	else
		x += xx * fontwidth(p);
	if (fontheightlog(p))
		y += (yy << fontheightlog(p));
	else
		y += (yy * fontheight(p));
	if (fontwidth(p) <= 8) {
		while (count >= 4) {
			count -= 4;
			sbus_writel(0, &fbc->incx);
			sbus_writel(1, &fbc->incy);
			sbus_writel(x, &fbc->x0);
			sbus_writel((x += 4 * fontwidth(p)) - 1, &fbc->x1);
			sbus_writel(y, &fbc->y0);
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
					u32 val = ((u32)*fd4++) |
						((((u32)*fd3++) |
						  ((((u32)*fd2++) |
						    (((u32)*fd1++)
						     << 8)) << 8)) << 8);
					sbus_writel(val, &fbc->font);
				}
			} else {
				for (i = 0; i < fontheight(p); i++) {
					u32 val = (((u32)*fd4++) |
						   ((((u32)*fd3++) |
						     ((((u32)*fd2++) |
						       (((u32)*fd1++) 
							<< fontwidth(p))) <<
						      fontwidth(p))) <<
						    fontwidth(p))) <<
						(24 - 3 * fontwidth(p));
					sbus_writel(val, &fbc->font);
				}
			}
		}
	} else {
		while (count >= 2) {
			count -= 2;
			sbus_writel(0, &fbc->incx);
			sbus_writel(1, &fbc->incy);
			sbus_writel(x, &fbc->x0);
			sbus_writel((x += 2 * fontwidth(p)) - 1, &fbc->x1);
			sbus_writel(y, &fbc->y0);
			if (fontheightlog(p)) {
				fd1 = p->fontdata + ((scr_readw(s++) & p->charmask) << (fontheightlog(p) + 1));
				fd2 = p->fontdata + ((scr_readw(s++) & p->charmask) << (fontheightlog(p) + 1));
			} else {
				fd1 = p->fontdata + (((scr_readw(s++) & p->charmask) * fontheight(p)) << 1);
				fd2 = p->fontdata + (((scr_readw(s++) & p->charmask) * fontheight(p)) << 1);
			}
			for (i = 0; i < fontheight(p); i++) {
				u32 val = ((((u32)*(u16 *)fd1) << fontwidth(p)) |
					   ((u32)*(u16 *)fd2)) << (16 - fontwidth(p));
				sbus_writel(val, &fbc->font);
				fd1 += 2; fd2 += 2;
			}
		}
	}
	while (count) {
		count--;
		sbus_writel(0, &fbc->incx);
		sbus_writel(1, &fbc->incy);
		sbus_writel(x, &fbc->x0);
		sbus_writel((x += fontwidth(p)) - 1, &fbc->x1);
		sbus_writel(y, &fbc->y0);
		if (fontheightlog(p))
			i = ((scr_readw(s++) & p->charmask) << fontheightlog(p));
		else
			i = ((scr_readw(s++) & p->charmask) * fontheight(p));
		if (fontwidth(p) <= 8) {
			fd1 = p->fontdata + i;
			for (i = 0; i < fontheight(p); i++) {
				u32 val = *fd1++ << 24;
				sbus_writel(val, &fbc->font);
			}
		} else {
			fd1 = p->fontdata + (i << 1);
			for (i = 0; i < fontheight(p); i++) {
				u32 val = *(u16 *)fd1 << 16;
				sbus_writel(val, &fbc->font);
				fd1 += 2;
			}
		}
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_revc(struct display *p, int xx, int yy)
{
	/* Not used if hw cursor */
}

static void cg6_loadcmap (struct fb_info_sbusfb *fb, struct display *p, int index, int count)
{
	struct bt_regs *bt = fb->s.cg6.bt;
	unsigned long flags;
	int i;
                
	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(index << 24, &bt->addr);
	for (i = index; count--; i++){
		sbus_writel(fb->color_map CM(i,0) << 24,
			    &bt->color_map);
		sbus_writel(fb->color_map CM(i,1) << 24,
			    &bt->color_map);
		sbus_writel(fb->color_map CM(i,2) << 24,
			    &bt->color_map);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_restore_palette (struct fb_info_sbusfb *fb)
{
	struct bt_regs *bt = fb->s.cg6.bt;
	unsigned long flags;
                
	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(0, &bt->addr);
	sbus_writel(0xffffffff, &bt->color_map);
	sbus_writel(0xffffffff, &bt->color_map);
	sbus_writel(0xffffffff, &bt->color_map);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static struct display_switch cg6_dispsw __initdata = {
	setup:		cg6_setup,
	bmove:		fbcon_redraw_bmove,
	clear:		cg6_clear,
	putc:		cg6_putc,
	putcs:		cg6_putcs,
	revc:		cg6_revc, 
	fontwidthmask:	FONTWIDTHRANGE(1,16) /* Allow fontwidths up to 16 */
};

static void cg6_setcursormap (struct fb_info_sbusfb *fb, u8 *red, u8 *green, u8 *blue)
{
        struct bt_regs *bt = fb->s.cg6.bt;
	unsigned long flags;
        
	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(1 << 24, &bt->addr);
	sbus_writel(red[0] << 24, &bt->cursor);
	sbus_writel(green[0] << 24, &bt->cursor);
	sbus_writel(blue[0] << 24, &bt->cursor);
	sbus_writel(3 << 24, &bt->addr);
	sbus_writel(red[1] << 24, &bt->cursor);
	sbus_writel(green[1] << 24, &bt->cursor);
	sbus_writel(blue[1] << 24, &bt->cursor);
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Set cursor shape */
static void cg6_setcurshape (struct fb_info_sbusfb *fb)
{
	struct cg6_thc *thc = fb->s.cg6.thc;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&fb->lock, flags);
	for (i = 0; i < 32; i++) {
		sbus_writel(fb->cursor.bits[0][i],
			    &thc->thc_cursmask [i]);
		sbus_writel(fb->cursor.bits[1][i],
			    &thc->thc_cursbits [i]);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Load cursor information */
static void cg6_setcursor (struct fb_info_sbusfb *fb)
{
	unsigned int v;
	unsigned long flags;
	struct cg_cursor *c = &fb->cursor;

	spin_lock_irqsave(&fb->lock, flags);
	if (c->enable)
		v = ((c->cpos.fbx - c->chot.fbx) << 16)
		    |((c->cpos.fby - c->chot.fby) & 0xffff);
	else
		/* Magic constant to turn off the cursor */
		v = ((65536-32) << 16) | (65536-32);
	sbus_writel(v, &fb->s.cg6.thc->thc_cursxy);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_blank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readl(&fb->s.cg6.thc->thc_misc);
	tmp &= ~CG6_THC_MISC_VIDEO;
	sbus_writel(tmp, &fb->s.cg6.thc->thc_misc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_unblank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readl(&fb->s.cg6.thc->thc_misc);
	tmp |= CG6_THC_MISC_VIDEO;
	sbus_writel(tmp, &fb->s.cg6.thc->thc_misc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_reset (struct fb_info_sbusfb *fb)
{
	unsigned int rev, conf;
	struct cg6_tec *tec = fb->s.cg6.tec;
	struct cg6_fbc *fbc = fb->s.cg6.fbc;
	unsigned long flags;
	u32 mode, tmp;
	int i;
	
	spin_lock_irqsave(&fb->lock, flags);

	/* Turn off stuff in the Transform Engine. */
	sbus_writel(0, &tec->tec_matrix);
	sbus_writel(0, &tec->tec_clip);
	sbus_writel(0, &tec->tec_vdc);

	/* Take care of bugs in old revisions. */
	rev = (sbus_readl(fb->s.cg6.fhc) >> CG6_FHC_REV_SHIFT) & CG6_FHC_REV_MASK;
	if (rev < 5) {
		conf = (sbus_readl(fb->s.cg6.fhc) & CG6_FHC_RES_MASK) |
			CG6_FHC_CPU_68020 | CG6_FHC_TEST |
			(11 << CG6_FHC_TEST_X_SHIFT) |
			(11 << CG6_FHC_TEST_Y_SHIFT);
		if (rev < 2)
			conf |= CG6_FHC_DST_DISABLE;
		sbus_writel(conf, fb->s.cg6.fhc);
	}

	/* Set things in the FBC. Bad things appear to happen if we do
	 * back to back store/loads on the mode register, so copy it
	 * out instead. */
	mode = sbus_readl(&fbc->mode);
	do {
		i = sbus_readl(&fbc->s);
	} while (i & 0x10000000);
	mode &= ~(CG6_FBC_BLIT_MASK | CG6_FBC_MODE_MASK |
		       CG6_FBC_DRAW_MASK | CG6_FBC_BWRITE0_MASK |
		       CG6_FBC_BWRITE1_MASK | CG6_FBC_BREAD_MASK |
		       CG6_FBC_BDISP_MASK);
	mode |= (CG6_FBC_BLIT_SRC | CG6_FBC_MODE_COLOR8 |
		      CG6_FBC_DRAW_RENDER | CG6_FBC_BWRITE0_ENABLE |
		      CG6_FBC_BWRITE1_DISABLE | CG6_FBC_BREAD_0 |
		      CG6_FBC_BDISP_0);
	sbus_writel(mode, &fbc->mode);

	sbus_writel(0, &fbc->clip);
	sbus_writel(0, &fbc->offx);
	sbus_writel(0, &fbc->offy);
	sbus_writel(0, &fbc->clipminx);
	sbus_writel(0, &fbc->clipminy);
	sbus_writel(fb->type.fb_width - 1, &fbc->clipmaxx);
	sbus_writel(fb->type.fb_height - 1, &fbc->clipmaxy);

	/* Enable cursor in Brooktree DAC. */
	sbus_writel(0x06 << 24, &fb->s.cg6.bt->addr);
	tmp = sbus_readl(&fb->s.cg6.bt->control);
	tmp |= 0x03 << 24;
	sbus_writel(tmp, &fb->s.cg6.bt->control);

	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg6_margins (struct fb_info_sbusfb *fb, struct display *p, int x_margin, int y_margin)
{
	p->screen_base += (y_margin - fb->y_margin) *
		p->line_length + (x_margin - fb->x_margin);
}

static int __init cg6_rasterimg (struct fb_info *info, int start)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	register struct cg6_fbc *fbc = fb->s.cg6.fbc;
	int i;
	
	do {
		i = sbus_readl(&fbc->s);
	} while (i & 0x10000000);
	return 0;
}

static char idstring[70] __initdata = { 0 };

char __init *cgsixfb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
	struct sbus_dev *sdev = fb->sbdp;
	unsigned long phys = sdev->reg_addrs[0].phys_addr;
	u32 conf;
	char *p;
	char *cardtype;
	struct bt_regs *bt;
	struct fb_ops *fbops;

	fbops = kmalloc(sizeof(*fbops), GFP_KERNEL);
	if (fbops == NULL)
		return NULL;
	
	*fbops = *fb->info.fbops;
	fbops->fb_rasterimg = cg6_rasterimg;
	fb->info.fbops = fbops;
	
	if (prom_getbool (fb->prom_node, "dblbuf")) {
		type->fb_size *= 4;
		fix->smem_len *= 4;
	}

	fix->line_length = fb->var.xres_virtual;
	fix->accel = FB_ACCEL_SUN_CGSIX;
	
	var->accel_flags = FB_ACCELF_TEXT;
	
	disp->scrollmode = SCROLL_YREDRAW;
	if (!disp->screen_base) {
		disp->screen_base = (char *)
			sbus_ioremap(&sdev->resource[0], CG6_RAM_OFFSET,
				     type->fb_size, "cgsix ram");
	}
	disp->screen_base += fix->line_length * fb->y_margin + fb->x_margin;
	fb->s.cg6.fbc = (struct cg6_fbc *)
		sbus_ioremap(&sdev->resource[0], CG6_FBC_OFFSET,
			     4096, "cgsix fbc");
	fb->s.cg6.tec = (struct cg6_tec *)
		sbus_ioremap(&sdev->resource[0], CG6_TEC_OFFSET,
			     sizeof(struct cg6_tec), "cgsix tec");
	fb->s.cg6.thc = (struct cg6_thc *)
		sbus_ioremap(&sdev->resource[0], CG6_THC_OFFSET,
			     sizeof(struct cg6_thc), "cgsix thc");
	fb->s.cg6.bt = bt = (struct bt_regs *)
		sbus_ioremap(&sdev->resource[0], CG6_BROOKTREE_OFFSET,
			     sizeof(struct bt_regs), "cgsix dac");
	fb->s.cg6.fhc = (u32 *)
		sbus_ioremap(&sdev->resource[0], CG6_FHC_OFFSET,
			     sizeof(u32), "cgsix fhc");
#if 0
	prom_printf("CG6: RES[%016lx:%016lx:%016lx]\n",
		    sdev->resource[0].start,
		    sdev->resource[0].end,
		    sdev->resource[0].flags);
	prom_printf("CG6: fbc(%p) tec(%p) thc(%p) bt(%p) fhc(%p)\n",
		    fb->s.cg6.fbc,
		    fb->s.cg6.tec,
		    fb->s.cg6.thc,
		    fb->s.cg6.bt,
		    fb->s.cg6.fhc);
	prom_halt();
#endif
	fb->dispsw = cg6_dispsw;

	fb->margins = cg6_margins;
	fb->loadcmap = cg6_loadcmap;
	fb->setcursor = cg6_setcursor;
	fb->setcursormap = cg6_setcursormap;
	fb->setcurshape = cg6_setcurshape;
	fb->restore_palette = cg6_restore_palette;
	fb->fill = cg6_fill;
	fb->blank = cg6_blank;
	fb->unblank = cg6_unblank;
	fb->reset = cg6_reset;
	
	fb->physbase = phys;
	fb->mmap_map = cg6_mmap_map;
	
	/* Initialize Brooktree DAC */
	sbus_writel(0x04 << 24, &bt->addr);         /* color planes */
	sbus_writel(0xff << 24, &bt->control);
	sbus_writel(0x05 << 24, &bt->addr);
	sbus_writel(0x00 << 24, &bt->control);
	sbus_writel(0x06 << 24, &bt->addr);         /* overlay plane */
	sbus_writel(0x73 << 24, &bt->control);
	sbus_writel(0x07 << 24, &bt->addr);
	sbus_writel(0x00 << 24, &bt->control);
	
	conf = sbus_readl(fb->s.cg6.fhc);
	switch(conf & CG6_FHC_CPU_MASK) {
	case CG6_FHC_CPU_SPARC: p = "sparc"; break;
	case CG6_FHC_CPU_68020: p = "68020"; break;
	default: p = "i386"; break;
	}

	if (((conf >> CG6_FHC_REV_SHIFT) & CG6_FHC_REV_MASK) >= 11) {
		if (fix->smem_len <= 0x100000) {
			cardtype = "TGX";
		} else {
			cardtype = "TGX+";
		}
	} else {
		if (fix->smem_len <= 0x100000) {
			cardtype = "GX";
		} else {
			cardtype = "GX+";
		}
	}
	                                                                        
	sprintf(idstring, 
#ifdef __sparc_v9__
		    "cgsix at %016lx TEC Rev %x CPU %s Rev %x [%s]", phys,
#else	
		    "cgsix at %x.%08lx TEC Rev %x CPU %s Rev %x [%s]",
		    fb->iospace, phys, 
#endif
		    ((sbus_readl(&fb->s.cg6.thc->thc_misc) >> CG6_THC_MISC_REV_SHIFT) &
		     CG6_THC_MISC_REV_MASK),
		    p, (conf >> CG6_FHC_REV_SHIFT) & CG6_FHC_REV_MASK, cardtype);

	sprintf(fb->info.modename, "CGsix [%s]", cardtype);
	sprintf(fix->id, "CGsix [%s]", cardtype);
		    
	cg6_reset(fb);
		    
	return idstring;
}
