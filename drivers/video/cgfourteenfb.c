/* $Id: cgfourteenfb.c,v 1.11 2001/09/19 00:04:33 davem Exp $
 * cgfourteenfb.c: CGfourteen frame buffer driver
 *
 * Copyright (C) 1996,1998 Jakub Jelinek (jj@ultra.linux.cz)
 * Copyright (C) 1995 Miguel de Icaza (miguel@nuclecu.unam.mx)
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
#include <asm/pgtable.h>
#include <asm/uaccess.h>

#include <video/fbcon-cfb8.h>

#define CG14_MCR_INTENABLE_SHIFT	7
#define CG14_MCR_INTENABLE_MASK		0x80
#define CG14_MCR_VIDENABLE_SHIFT	6
#define CG14_MCR_VIDENABLE_MASK		0x40
#define CG14_MCR_PIXMODE_SHIFT		4
#define CG14_MCR_PIXMODE_MASK		0x30
#define CG14_MCR_TMR_SHIFT		2
#define CG14_MCR_TMR_MASK		0x0c
#define CG14_MCR_TMENABLE_SHIFT		1
#define CG14_MCR_TMENABLE_MASK		0x02
#define CG14_MCR_RESET_SHIFT		0
#define CG14_MCR_RESET_MASK		0x01
#define CG14_REV_REVISION_SHIFT		4
#define CG14_REV_REVISION_MASK		0xf0
#define CG14_REV_IMPL_SHIFT		0
#define CG14_REV_IMPL_MASK		0x0f
#define CG14_VBR_FRAMEBASE_SHIFT	12
#define CG14_VBR_FRAMEBASE_MASK		0x00fff000
#define CG14_VMCR1_SETUP_SHIFT		0
#define CG14_VMCR1_SETUP_MASK		0x000001ff
#define CG14_VMCR1_VCONFIG_SHIFT	9
#define CG14_VMCR1_VCONFIG_MASK		0x00000e00
#define CG14_VMCR2_REFRESH_SHIFT	0
#define CG14_VMCR2_REFRESH_MASK		0x00000001
#define CG14_VMCR2_TESTROWCNT_SHIFT	1
#define CG14_VMCR2_TESTROWCNT_MASK	0x00000002
#define CG14_VMCR2_FBCONFIG_SHIFT	2
#define CG14_VMCR2_FBCONFIG_MASK	0x0000000c
#define CG14_VCR_REFRESHREQ_SHIFT	0
#define CG14_VCR_REFRESHREQ_MASK	0x000003ff
#define CG14_VCR1_REFRESHENA_SHIFT	10
#define CG14_VCR1_REFRESHENA_MASK	0x00000400
#define CG14_VCA_CAD_SHIFT		0
#define CG14_VCA_CAD_MASK		0x000003ff
#define CG14_VCA_VERS_SHIFT		10
#define CG14_VCA_VERS_MASK		0x00000c00
#define CG14_VCA_RAMSPEED_SHIFT		12
#define CG14_VCA_RAMSPEED_MASK		0x00001000
#define CG14_VCA_8MB_SHIFT		13
#define CG14_VCA_8MB_MASK		0x00002000

#define CG14_MCR_PIXMODE_8		0
#define CG14_MCR_PIXMODE_16		2
#define CG14_MCR_PIXMODE_32		3

MODULE_LICENSE("GPL");

struct cg14_regs{
	volatile u8 mcr;	/* Master Control Reg */
	volatile u8 ppr;	/* Packed Pixel Reg */
	volatile u8 tms[2];	/* Test Mode Status Regs */
	volatile u8 msr;	/* Master Status Reg */
	volatile u8 fsr;	/* Fault Status Reg */
	volatile u8 rev;	/* Revision & Impl */
	volatile u8 ccr;	/* Clock Control Reg */
	volatile u32 tmr;	/* Test Mode Read Back */
	volatile u8 mod;	/* Monitor Operation Data Reg */
	volatile u8 acr;	/* Aux Control */
	u8 xxx0[6];
	volatile u16 hct;	/* Hor Counter */
	volatile u16 vct;	/* Vert Counter */
	volatile u16 hbs;	/* Hor Blank Start */
	volatile u16 hbc;	/* Hor Blank Clear */
	volatile u16 hss;	/* Hor Sync Start */
	volatile u16 hsc;	/* Hor Sync Clear */
	volatile u16 csc;	/* Composite Sync Clear */
	volatile u16 vbs;	/* Vert Blank Start */
	volatile u16 vbc;	/* Vert Blank Clear */
	volatile u16 vss;	/* Vert Sync Start */
	volatile u16 vsc;	/* Vert Sync Clear */
	volatile u16 xcs;
	volatile u16 xcc;
	volatile u16 fsa;	/* Fault Status Address */
	volatile u16 adr;	/* Address Registers */
	u8 xxx1[0xce];
	volatile u8 pcg[0x100]; /* Pixel Clock Generator */
	volatile u32 vbr;	/* Frame Base Row */
	volatile u32 vmcr;	/* VBC Master Control */
	volatile u32 vcr;	/* VBC refresh */
	volatile u32 vca;	/* VBC Config */
};

#define CG14_CCR_ENABLE	0x04
#define CG14_CCR_SELECT 0x02	/* HW/Full screen */

struct cg14_cursor {
	volatile u32 cpl0[32];	/* Enable plane 0 */
	volatile u32 cpl1[32];  /* Color selection plane */
	volatile u8 ccr;	/* Cursor Control Reg */
	u8 xxx0[3];
	volatile u16 cursx;	/* Cursor x,y position */
	volatile u16 cursy;	/* Cursor x,y position */
	volatile u32 color0;
	volatile u32 color1;
	u32 xxx1[0x1bc];
	volatile u32 cpl0i[32];	/* Enable plane 0 autoinc */
	volatile u32 cpl1i[32]; /* Color selection autoinc */
};

struct cg14_dac {
	volatile u8 addr;	/* Address Register */
	u8 xxx0[255];
	volatile u8 glut;	/* Gamma table */
	u8 xxx1[255];
	volatile u8 select;	/* Register Select */
	u8 xxx2[255];
	volatile u8 mode;	/* Mode Register */
};

struct cg14_xlut{
	volatile u8 x_xlut [256];
	volatile u8 x_xlutd [256];
	u8 xxx0[0x600];
	volatile u8 x_xlut_inc [256];
	volatile u8 x_xlutd_inc [256];
};

/* Color look up table (clut) */
/* Each one of these arrays hold the color lookup table (for 256
 * colors) for each MDI page (I assume then there should be 4 MDI
 * pages, I still wonder what they are.  I have seen NeXTStep split
 * the screen in four parts, while operating in 24 bits mode.  Each
 * integer holds 4 values: alpha value (transparency channel, thanks
 * go to John Stone (johns@umr.edu) from OpenBSD), red, green and blue
 *
 * I currently use the clut instead of the Xlut
 */
struct cg14_clut {
	u32 c_clut [256];
	u32 c_clutd [256];    /* i wonder what the 'd' is for */
	u32 c_clut_inc [256];
	u32 c_clutd_inc [256];
};

static struct sbus_mmap_map cg14_mmap_map[] __initdata = {
	{ CG14_REGS,		0x80000000,		0x1000		    },
	{ CG14_XLUT,		0x80003000,		0x1000		    },
	{ CG14_CLUT1,		0x80004000,		0x1000		    },
	{ CG14_CLUT2,		0x80005000,		0x1000		    },
	{ CG14_CLUT3,		0x80006000,		0x1000		    },
	{ CG3_MMAP_OFFSET - 
	  0x7000,		0x80000000,		0x7000		    },
	{ CG3_MMAP_OFFSET,	0x00000000,		SBUS_MMAP_FBSIZE(1) },
	{ MDI_CURSOR_MAP,	0x80001000,		0x1000		    },
	{ MDI_CHUNKY_BGR_MAP,	0x01000000,		0x400000	    },
	{ MDI_PLANAR_X16_MAP,	0x02000000,		0x200000	    },
	{ MDI_PLANAR_C16_MAP,	0x02800000,		0x200000	    },
	{ MDI_PLANAR_X32_MAP,	0x03000000,		0x100000	    },
	{ MDI_PLANAR_B32_MAP,	0x03400000,		0x100000	    },
	{ MDI_PLANAR_G32_MAP,	0x03800000,		0x100000	    },
	{ MDI_PLANAR_R32_MAP,	0x03c00000,		0x100000	    },
	{ 0,			0,			0		    }
};

static void cg14_loadcmap (struct fb_info_sbusfb *fb, struct display *p,
			   int index, int count)
{
	struct cg14_clut *clut = fb->s.cg14.clut;
	unsigned long flags;
	        
	spin_lock_irqsave(&fb->lock, flags);
	for (; count--; index++) {
		u32 val;

		val = ((fb->color_map CM(index,2) << 16) |
		       (fb->color_map CM(index,1) << 8) |
		       (fb->color_map CM(index,0)));
		sbus_writel(val, &clut->c_clut[index]);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg14_margins (struct fb_info_sbusfb *fb, struct display *p,
			  int x_margin, int y_margin)
{
	p->screen_base += (y_margin - fb->y_margin) *
		p->line_length + (x_margin - fb->x_margin);
}

static void cg14_setcursormap (struct fb_info_sbusfb *fb, u8 *red, u8 *green, u8 *blue)
{
	struct cg14_cursor *cur = fb->s.cg14.cursor;
	unsigned long flags;
	
	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(((red[0]) | (green[0] << 8) | (blue[0] << 16)), &cur->color0);
	sbus_writel(((red[1]) | (green[1] << 8) | (blue[1] << 16)), &cur->color1);
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Set cursor shape */
static void cg14_setcurshape (struct fb_info_sbusfb *fb)
{
	struct cg14_cursor *cur = fb->s.cg14.cursor;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&fb->lock, flags);
	for (i = 0; i < 32; i++){
		sbus_writel(fb->cursor.bits[0][i], &cur->cpl0[i]);
		sbus_writel(fb->cursor.bits[1][i], &cur->cpl1[i]);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Load cursor information */
static void cg14_setcursor (struct fb_info_sbusfb *fb)
{
	struct cg_cursor *c = &fb->cursor;
	struct cg14_cursor *cur = fb->s.cg14.cursor;
	unsigned long flags;
                
	spin_lock_irqsave(&fb->lock, flags);
	if (c->enable) {
		u8 tmp = sbus_readb(&cur->ccr);

		tmp |= CG14_CCR_ENABLE;
		sbus_writeb(tmp, &cur->ccr);
	} else {
		u8 tmp = sbus_readb(&cur->ccr);

		tmp &= ~CG14_CCR_ENABLE;
		sbus_writeb(tmp, &cur->ccr);
	}
	sbus_writew(((c->cpos.fbx - c->chot.fbx) & 0xfff), &cur->cursx);
	sbus_writew(((c->cpos.fby - c->chot.fby) & 0xfff), &cur->cursy);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg14_switch_from_graph (struct fb_info_sbusfb *fb)
{
	unsigned long flags;

	spin_lock_irqsave(&fb->lock, flags);

	/* Set the 8-bpp mode */
	if (fb->open && fb->mmaped){
		volatile char *mcr = &fb->s.cg14.regs->mcr;
		char tmp;
	                                
		fb->s.cg14.mode = 8;
		tmp = sbus_readb(mcr);
		tmp &= ~(CG14_MCR_PIXMODE_MASK);
		sbus_writeb(tmp, mcr);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void cg14_reset (struct fb_info_sbusfb *fb)
{
	volatile char *mcr = &fb->s.cg14.regs->mcr;
	unsigned long flags;
	char tmp;
	        
	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readb(mcr);
	tmp &= ~(CG14_MCR_PIXMODE_MASK);
	sbus_writeb(tmp, mcr);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static int cg14_ioctl (struct fb_info_sbusfb *fb, unsigned int cmd, unsigned long arg)
{
	volatile char *mcr = &fb->s.cg14.regs->mcr;
	struct mdi_cfginfo *mdii;
	unsigned long flags;
	int mode, ret = 0;
	char tmp;
	        
	switch (cmd) {
	case MDI_RESET:
		spin_lock_irqsave(&fb->lock, flags);
		tmp = sbus_readb(mcr);
		tmp &= ~CG14_MCR_PIXMODE_MASK;
		sbus_writeb(tmp, mcr);
		spin_unlock_irqrestore(&fb->lock, flags);
		break;
	case MDI_GET_CFGINFO:
		mdii = (struct mdi_cfginfo *)arg;
		if (put_user(FBTYPE_MDICOLOR, &mdii->mdi_type) ||
		    __put_user(fb->type.fb_height, &mdii->mdi_height) ||
		    __put_user(fb->type.fb_width, &mdii->mdi_width) ||
		    __put_user(fb->s.cg14.mode, &mdii->mdi_mode) ||
		    __put_user(72, &mdii->mdi_pixfreq) || /* FIXME */
		    __put_user(fb->s.cg14.ramsize, &mdii->mdi_size))
			return -EFAULT;
		break;
	case MDI_SET_PIXELMODE:
		if (get_user(mode, (int *)arg))
			return -EFAULT;

		spin_lock_irqsave(&fb->lock, flags);
		tmp = sbus_readb(mcr);
		switch (mode){
		case MDI_32_PIX:
			tmp = (tmp & ~CG14_MCR_PIXMODE_MASK) |
				(CG14_MCR_PIXMODE_32 << CG14_MCR_PIXMODE_SHIFT);
			break;
		case MDI_16_PIX:
			tmp = (tmp & ~CG14_MCR_PIXMODE_MASK) | 0x20;
			break;
		case MDI_8_PIX:
			tmp = (tmp & ~CG14_MCR_PIXMODE_MASK);
			break;
		default:
			ret = -ENOSYS;
			break;
		};
		if (ret == 0) {
			sbus_writeb(tmp, mcr);
			fb->s.cg14.mode = mode;
		}
		spin_unlock_irqrestore(&fb->lock, flags);
		break;
	default:
		ret = -EINVAL;
	};

	return ret;
}

static unsigned long __init get_phys(unsigned long addr)
{
	return __get_phys(addr);
}

static int __init get_iospace(unsigned long addr)
{
	return __get_iospace(addr);
}

static char idstring[60] __initdata = { 0 };

char __init *cgfourteenfb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
	unsigned long rphys, phys;
	u32 bases[6];
	int is_8mb, i;

#ifndef FBCON_HAS_CFB8
	return NULL;
#endif
	prom_getproperty (fb->prom_node, "address", (char *) &bases[0], 8);
	if (!bases[0]) {
		printk("cg14 not mmaped\n");
		return NULL;
	}
	if (get_iospace(bases[0]) != get_iospace(bases[1])) {
		printk("Ugh. cg14 iospaces don't match\n");
		return NULL;
	}
	fb->physbase = phys = get_phys(bases[1]);
	rphys = get_phys(bases[0]);
	fb->iospace = get_iospace(bases[0]);
	fb->s.cg14.regs = (struct cg14_regs *)(unsigned long)bases[0];
	fb->s.cg14.clut = (void *)((unsigned long)bases[0]+CG14_CLUT1);
	fb->s.cg14.cursor = (void *)((unsigned long)bases[0]+CG14_CURSORREGS);
	disp->screen_base = (char *)bases[1];
	
	/* CG14_VCA_8MB_MASK is not correctly set on the 501-2482
	 * VSIMM, so we read the memory size from the PROM
	 */
	prom_getproperty(fb->prom_node, "reg", (char *) &bases[0], 24);
	is_8mb = bases[5] == 0x800000;

	fb->mmap_map = kmalloc(sizeof(cg14_mmap_map), GFP_KERNEL);
	if (!fb->mmap_map)
		return NULL;

	for (i = 0; ; i++) {
		fb->mmap_map[i].voff = cg14_mmap_map[i].voff;
		fb->mmap_map[i].poff = (cg14_mmap_map[i].poff & 0x80000000) ?
				       (cg14_mmap_map[i].poff & 0x7fffffff) + rphys - phys :
				       cg14_mmap_map[i].poff;
		fb->mmap_map[i].size = cg14_mmap_map[i].size;
		if (is_8mb && fb->mmap_map[i].size >= 0x100000 &&
		    fb->mmap_map[i].size <= 0x400000)
			fb->mmap_map[i].size <<= 1;
		if (!cg14_mmap_map[i].size)
			break;
	}

	strcpy(fb->info.modename, "CGfourteen");
	strcpy(fix->id, "CGfourteen");
	fix->line_length = fb->var.xres_virtual;
	fix->accel = FB_ACCEL_SUN_CG14;
	
	disp->scrollmode = SCROLL_YREDRAW;
	disp->screen_base += fix->line_length * fb->y_margin + fb->x_margin;
	fb->dispsw = fbcon_cfb8;
	
	type->fb_depth = 24;
	fb->emulations[1] = FBTYPE_SUN3COLOR;

	fb->margins = cg14_margins;
	fb->loadcmap = cg14_loadcmap;
	fb->setcursor = cg14_setcursor;
	fb->setcursormap = cg14_setcursormap;
	fb->setcurshape = cg14_setcurshape;
	fb->reset = cg14_reset;
	fb->switch_from_graph = cg14_switch_from_graph;
	fb->ioctl = cg14_ioctl;

	fb->s.cg14.mode = 8;
	fb->s.cg14.ramsize = (is_8mb) ? 0x800000 : 0x400000;
	
	cg14_reset(fb);
	
	sprintf(idstring, "cgfourteen at %x.%08lx, %dMB, rev=%d, impl=%d", fb->iospace, phys,
		is_8mb ? 8 : 4, fb->s.cg14.regs->rev >> 4, fb->s.cg14.regs->rev & 0xf);
	
	return idstring;
}
