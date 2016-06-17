/* $Id: bwtwofb.c,v 1.15 2001/09/19 00:04:33 davem Exp $
 * bwtwofb.c: BWtwo frame buffer driver
 *
 * Copyright (C) 1998 Jakub Jelinek   (jj@ultra.linux.cz)
 * Copyright (C) 1996 Miguel de Icaza (miguel@nuclecu.unam.mx)
 * Copyright (C) 1997 Eddie C. Dost   (ecd@skynet.be)
 * Copyright (C) 1998 Pavel Machek    (pavel@ucw.cz)
 */

#include <linux/config.h>
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
#if !defined(__sparc_v9__) && !defined(__mc68000__)
#include <asm/sun4paddr.h>
#endif

#include <video/fbcon-mfb.h>

/* OBio addresses for the bwtwo registers */
#define BWTWO_REGISTER_OFFSET 0x400000

struct bw2_regs {
	struct bt_regs		bt;
	volatile u8		control;
	volatile u8		status;
	volatile u8		cursor_start;
	volatile u8		cursor_end;
	volatile u8		h_blank_start;
	volatile u8		h_blank_end;
	volatile u8		h_sync_start;
	volatile u8		h_sync_end;
	volatile u8		comp_sync_end;
	volatile u8		v_blank_start_high;
	volatile u8		v_blank_start_low;
	volatile u8		v_blank_end;
	volatile u8		v_sync_start;
	volatile u8		v_sync_end;
	volatile u8		xfer_holdoff_start;
	volatile u8		xfer_holdoff_end;
};

/* Status Register Constants */
#define BWTWO_SR_RES_MASK	0x70
#define BWTWO_SR_1600_1280	0x50
#define BWTWO_SR_1152_900_76_A	0x40
#define BWTWO_SR_1152_900_76_B	0x60
#define BWTWO_SR_ID_MASK	0x0f
#define BWTWO_SR_ID_MONO	0x02
#define BWTWO_SR_ID_MONO_ECL	0x03
#define BWTWO_SR_ID_MSYNC	0x04
#define BWTWO_SR_ID_NOCONN	0x0a

/* Control Register Constants */
#define BWTWO_CTL_ENABLE_INTS   0x80
#define BWTWO_CTL_ENABLE_VIDEO  0x40
#define BWTWO_CTL_ENABLE_TIMING 0x20
#define BWTWO_CTL_ENABLE_CURCMP 0x10
#define BWTWO_CTL_XTAL_MASK     0x0C
#define BWTWO_CTL_DIVISOR_MASK  0x03

/* Status Register Constants */
#define BWTWO_STAT_PENDING_INT  0x80
#define BWTWO_STAT_MSENSE_MASK  0x70
#define BWTWO_STAT_ID_MASK      0x0f

static struct sbus_mmap_map bw2_mmap_map[] = {
	{ 0,			0,			SBUS_MMAP_FBSIZE(1) },
	{ 0,			0,			0		    }
};

static void bw2_blank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u8 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readb(&fb->s.bw2.regs->control);
	tmp &= ~BWTWO_CTL_ENABLE_VIDEO;
	sbus_writeb(tmp, &fb->s.bw2.regs->control);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void bw2_unblank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u8 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readb(&fb->s.bw2.regs->control);
	tmp |= BWTWO_CTL_ENABLE_VIDEO;
	sbus_writeb(tmp, &fb->s.bw2.regs->control);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void bw2_margins (struct fb_info_sbusfb *fb, struct display *p,
			 int x_margin, int y_margin)
{
	p->screen_base += (y_margin - fb->y_margin) *
		p->line_length + ((x_margin - fb->x_margin) >> 3);
}

static u8 bw2regs_1600[] __initdata = {
	0x14, 0x8b,	0x15, 0x28,	0x16, 0x03,	0x17, 0x13,
	0x18, 0x7b,	0x19, 0x05,	0x1a, 0x34,	0x1b, 0x2e,
	0x1c, 0x00,	0x1d, 0x0a,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x21,	0
};

static u8 bw2regs_ecl[] __initdata = {
	0x14, 0x65,	0x15, 0x1e,	0x16, 0x04,	0x17, 0x0c,
	0x18, 0x5e,	0x19, 0x03,	0x1a, 0xa7,	0x1b, 0x23,
	0x1c, 0x00,	0x1d, 0x08,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,	0
};

static u8 bw2regs_analog[] __initdata = {
	0x14, 0xbb,	0x15, 0x2b,	0x16, 0x03,	0x17, 0x13,
	0x18, 0xb0,	0x19, 0x03,	0x1a, 0xa6,	0x1b, 0x22,
	0x1c, 0x01,	0x1d, 0x05,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,	0
};

static u8 bw2regs_76hz[] __initdata = {
	0x14, 0xb7,	0x15, 0x27,	0x16, 0x03,	0x17, 0x0f,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xae,	0x1b, 0x2a,
	0x1c, 0x01,	0x1d, 0x09,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x24,	0
};

static u8 bw2regs_66hz[] __initdata = {
	0x14, 0xbb,	0x15, 0x2b,	0x16, 0x04,	0x17, 0x14,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xa8,	0x1b, 0x24,
	0x1c, 0x01,	0x1d, 0x05,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,	0
};

static char idstring[60] __initdata = { 0 };

char __init *bwtwofb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
#ifdef CONFIG_SUN4
	unsigned long phys = sun4_bwtwo_physaddr;
	struct resource res;
#else
	unsigned long phys = fb->sbdp->reg_addrs[0].phys_addr;
#endif
	struct resource *resp;
	unsigned int vaddr;

#ifndef FBCON_HAS_MFB
	return NULL;
#endif

#ifdef CONFIG_SUN4
	res.start = phys;
	res.end = res.start + BWTWO_REGISTER_OFFSET + sizeof(struct bw2_regs) - 1;
	res.flags = IORESOURCE_IO | (fb->iospace & 0xff);
	resp = &res;
#else
	resp = &fb->sbdp->resource[0];
#endif
	if (!fb->s.bw2.regs) {
		fb->s.bw2.regs = (struct bw2_regs *)
			sbus_ioremap(resp, BWTWO_REGISTER_OFFSET,
				     sizeof(struct bw2_regs), "bw2 regs");
		if ((!ARCH_SUN4) && (!prom_getbool(fb->prom_node, "width"))) {
			/* Ugh, broken PROM didn't initialize us.
			 * Let's deal with this ourselves.
			 */
			u8 status, mon;
			u8 *p;
			int sizechange = 0;

			status = sbus_readb(&fb->s.bw2.regs->status);
			mon = status & BWTWO_SR_RES_MASK;
			switch (status & BWTWO_SR_ID_MASK) {
				case BWTWO_SR_ID_MONO_ECL:
					if (mon == BWTWO_SR_1600_1280) {
						p = bw2regs_1600;
						fb->type.fb_width = 1600;
						fb->type.fb_height = 1280;
						sizechange = 1;
					} else
						p = bw2regs_ecl;
					break;
				case BWTWO_SR_ID_MONO:
					p = bw2regs_analog;
					break;
				case BWTWO_SR_ID_MSYNC:
					if (mon == BWTWO_SR_1152_900_76_A ||
					    mon == BWTWO_SR_1152_900_76_B)
						p = bw2regs_76hz;
					else
						p = bw2regs_66hz;
					break;
				case BWTWO_SR_ID_NOCONN:
					return NULL;
				default:
#ifndef CONFIG_FB_SUN3
					prom_printf("bw2: can't handle SR %02x\n",
						    status);
					prom_halt();
#endif					
					return NULL; /* fool gcc. */
			}
			for ( ; *p; p += 2) {
				u8 *regp = &((u8 *)fb->s.bw2.regs)[p[0]];
				sbus_writeb(p[1], regp);
			}
		}
	}

	strcpy(fb->info.modename, "BWtwo");
	strcpy(fix->id, "BWtwo");
	fix->line_length = fb->var.xres_virtual >> 3;
	fix->accel = FB_ACCEL_SUN_BWTWO;
	
	disp->scrollmode = SCROLL_YREDRAW;
	disp->inverse = 1;
	if (!disp->screen_base) {
		disp->screen_base = (char *)
			sbus_ioremap(resp, 0, type->fb_size, "bw2 ram");
	}
	disp->screen_base += fix->line_length * fb->y_margin + (fb->x_margin >> 3);
	fb->dispsw = fbcon_mfb;
	fix->visual = FB_VISUAL_MONO01;

#ifndef CONFIG_SUN4
	fb->blank = bw2_blank;
	fb->unblank = bw2_unblank;

	prom_getproperty(fb->sbdp->prom_node, "address",
			 (char *)&vaddr, sizeof(vaddr));
	fb->physbase = __get_phys((unsigned long)vaddr);

#endif
	fb->margins = bw2_margins;
	fb->mmap_map = bw2_mmap_map;

#ifdef __sparc_v9__
	sprintf(idstring, "bwtwo at %016lx", phys);
#else	
	sprintf(idstring, "bwtwo at %x.%08lx", fb->iospace, phys);
#endif
	
	return idstring;
}

MODULE_LICENSE("GPL");
