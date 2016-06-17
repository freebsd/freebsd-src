/* $Id: cgthreefb.c,v 1.11 2001/09/19 00:04:33 davem Exp $
 * cgthreefb.c: CGthree frame buffer driver
 *
 * Copyright (C) 1996,1998 Jakub Jelinek (jj@ultra.linux.cz)
 * Copyright (C) 1996 Miguel de Icaza (miguel@nuclecu.unam.mx)
 * Copyright (C) 1997 Eddie C. Dost (ecd@skynet.be)
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

#include <video/fbcon-cfb8.h>

/* Control Register Constants */
#define CG3_CR_ENABLE_INTS      0x80
#define CG3_CR_ENABLE_VIDEO     0x40
#define CG3_CR_ENABLE_TIMING    0x20
#define CG3_CR_ENABLE_CURCMP    0x10
#define CG3_CR_XTAL_MASK        0x0c
#define CG3_CR_DIVISOR_MASK     0x03

/* Status Register Constants */
#define CG3_SR_PENDING_INT      0x80
#define CG3_SR_RES_MASK         0x70
#define CG3_SR_1152_900_76_A    0x40
#define CG3_SR_1152_900_76_B    0x60
#define CG3_SR_ID_MASK          0x0f
#define CG3_SR_ID_COLOR         0x01
#define CG3_SR_ID_MONO          0x02
#define CG3_SR_ID_MONO_ECL      0x03

MODULE_LICENSE("GPL");

enum cg3_type {
	CG3_AT_66HZ = 0,
	CG3_AT_76HZ,
	CG3_RDI
};

struct cg3_regs {
	struct bt_regs	cmap;
	volatile u8	control;
	volatile u8	status;
	volatile u8	cursor_start;
	volatile u8	cursor_end;
	volatile u8	h_blank_start;
	volatile u8	h_blank_end;
	volatile u8	h_sync_start;
	volatile u8	h_sync_end;
	volatile u8	comp_sync_end;
	volatile u8	v_blank_start_high;
	volatile u8	v_blank_start_low;
	volatile u8	v_blank_end;
	volatile u8	v_sync_start;
	volatile u8	v_sync_end;
	volatile u8	xfer_holdoff_start;
	volatile u8	xfer_holdoff_end;
};

/* Offset of interesting structures in the OBIO space */
#define CG3_REGS_OFFSET	     0x400000UL
#define CG3_RAM_OFFSET	     0x800000UL

static struct sbus_mmap_map cg3_mmap_map[] = {
	{ CG3_MMAP_OFFSET,	CG3_RAM_OFFSET,		SBUS_MMAP_FBSIZE(1) },
	{ 0,			0,			0		    }
};

/* The cg3 palette is loaded with 4 color values at each time  */
/* so you end up with: (rgb)(r), (gb)(rg), (b)(rgb), and so on */

#define D4M3(x) ((((x)>>2)<<1) + ((x)>>2))      /* (x/4)*3 */
#define D4M4(x) ((x)&~0x3)                      /* (x/4)*4 */

static void cg3_loadcmap (struct fb_info_sbusfb *fb, struct display *p, int index, int count)
{
	struct bt_regs *bt = &fb->s.cg3.regs->cmap;
	unsigned long flags;
	u32 *i;
	volatile u8 *regp;
	int steps;
	        
	spin_lock_irqsave(&fb->lock, flags);

	i = (((u32 *)fb->color_map) + D4M3(index));
	steps = D4M3(index+count-1) - D4M3(index)+3;
	                        
	regp = (volatile u8 *)&bt->addr;
	sbus_writeb(D4M4(index), regp);
	while (steps--) {
		u32 val = *i++;
		sbus_writel(val, &bt->color_map);
	}

	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg3_blank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u8 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readb(&fb->s.cg3.regs->control);
	tmp &= ~CG3_CR_ENABLE_VIDEO;
	sbus_writeb(tmp, &fb->s.cg3.regs->control);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg3_unblank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u8 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readb(&fb->s.cg3.regs->control);
	tmp |= CG3_CR_ENABLE_VIDEO;
	sbus_writeb(tmp, &fb->s.cg3.regs->control);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg3_margins (struct fb_info_sbusfb *fb, struct display *p,
			 int x_margin, int y_margin)
{
	p->screen_base += (y_margin - fb->y_margin) *
		p->line_length + (x_margin - fb->x_margin);
}

static u8 cg3regvals_66hz[] __initdata = {	/* 1152 x 900, 66 Hz */
	0x14, 0xbb,	0x15, 0x2b,	0x16, 0x04,	0x17, 0x14,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xa8,	0x1b, 0x24,
	0x1c, 0x01,	0x1d, 0x05,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,	0
};

static u8 cg3regvals_76hz[] __initdata = {	/* 1152 x 900, 76 Hz */
	0x14, 0xb7,	0x15, 0x27,	0x16, 0x03,	0x17, 0x0f,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xae,	0x1b, 0x2a,
	0x1c, 0x01,	0x1d, 0x09,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x24,	0
};

static u8 cg3regvals_rdi[] __initdata = {	/* 640 x 480, cgRDI */
	0x14, 0x70,	0x15, 0x20,	0x16, 0x08,	0x17, 0x10,
	0x18, 0x06,	0x19, 0x02,	0x1a, 0x31,	0x1b, 0x51,
	0x1c, 0x06,	0x1d, 0x0c,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x22,	0
};

static u8 *cg3_regvals[] __initdata = {
	cg3regvals_66hz, cg3regvals_76hz, cg3regvals_rdi
};

static u_char cg3_dacvals[] __initdata = {
	4, 0xff,	5, 0x00,	6, 0x70,	7, 0x00,	0
};

static char idstring[60] __initdata = { 0 };

char __init *cgthreefb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
	struct sbus_dev *sdev = fb->sbdp;
	unsigned long phys = sdev->reg_addrs[0].phys_addr;
	int cgRDI = strstr(fb->sbdp->prom_name, "cgRDI") != NULL;

#ifndef FBCON_HAS_CFB8
	return NULL;
#endif

	if (!fb->s.cg3.regs) {
		fb->s.cg3.regs = (struct cg3_regs *)
			sbus_ioremap(&sdev->resource[0], CG3_REGS_OFFSET,
				     sizeof(struct cg3_regs), "cg3 regs");
		if (cgRDI) {
			char buffer[40];
			char *p;
			int ww, hh;
		
			*buffer = 0;
			prom_getstring (fb->prom_node, "params", buffer, sizeof(buffer));
			if (*buffer) {
				ww = simple_strtoul (buffer, &p, 10);
				if (ww && *p == 'x') {
					hh = simple_strtoul (p + 1, &p, 10);
					if (hh && *p == '-') {
						if (type->fb_width != ww || type->fb_height != hh) {
							type->fb_width = ww;
							type->fb_height = hh;
							return SBUSFBINIT_SIZECHANGE;
						}
					}
				}
			}
		}
	}

	strcpy(fb->info.modename, "CGthree");
	strcpy(fix->id, "CGthree");
	fix->line_length = fb->var.xres_virtual;
	fix->accel = FB_ACCEL_SUN_CGTHREE;
	
	disp->scrollmode = SCROLL_YREDRAW;
	if (!disp->screen_base) {
		disp->screen_base = (char *)
			sbus_ioremap(&sdev->resource[0], CG3_RAM_OFFSET,
				     type->fb_size, "cg3 ram");
	}
	disp->screen_base += fix->line_length * fb->y_margin + fb->x_margin;
	fb->dispsw = fbcon_cfb8;

	fb->margins = cg3_margins;
	fb->loadcmap = cg3_loadcmap;
	fb->blank = cg3_blank;
	fb->unblank = cg3_unblank;
	
	fb->physbase = phys;
	fb->mmap_map = cg3_mmap_map;
	
#ifdef __sparc_v9__	
	sprintf(idstring, "%s at %016lx", cgRDI ? "cgRDI" : "cgthree", phys);
#else
	sprintf(idstring, "%s at %x.%08lx", cgRDI ? "cgRDI" : "cgthree", fb->iospace, phys);
#endif
	
	if (!prom_getbool(fb->prom_node, "width")) {
		/* Ugh, broken PROM didn't initialize us.
		 * Let's deal with this ourselves.
		 */
		enum cg3_type type;
		u8 *p;

		if (cgRDI)
			type = CG3_RDI;
		else {
			u8 status = sbus_readb(&fb->s.cg3.regs->status), mon;
			if ((status & CG3_SR_ID_MASK) == CG3_SR_ID_COLOR) {
				mon = status & CG3_SR_RES_MASK;
				if (mon == CG3_SR_1152_900_76_A ||
				    mon == CG3_SR_1152_900_76_B)
					type = CG3_AT_76HZ;
				else
					type = CG3_AT_66HZ;
			} else {
				prom_printf("cgthree: can't handle SR %02x\n",
					    status);
				prom_halt();
				return NULL; /* fool gcc. */
			}
		}

		for (p = cg3_regvals[type]; *p; p += 2) {
			u8 *regp = &((u8 *)fb->s.cg3.regs)[p[0]];
			sbus_writeb(p[1], regp);
		}
		for (p = cg3_dacvals; *p; p += 2) {
			volatile u8 *regp;

			regp = (volatile u8 *)&fb->s.cg3.regs->cmap.addr;
			sbus_writeb(p[0], regp);
			regp = (volatile u8 *)&fb->s.cg3.regs->cmap.control;
			sbus_writeb(p[1], regp);
		}
	}

	return idstring;
}
