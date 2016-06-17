/* $id: p9100fb.c,v 1.4 1999/08/18 10:55:01 shadow Exp $
 * p9100fb.c: P9100 frame buffer driver
 *
 * Copyright 1999 Derrick J Brashear (shadow@dementia.org)
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

#include "p9100.h"

static struct sbus_mmap_map p9100_mmap_map[] = {
#if 0 /* For now, play we're a dumb color fb */
  { P9100_CTL_OFF, 0x38000000, 0x2000 },
  { P9100_CMD_OFF, 0x38002000, 0x2000 },
  { P9100_FB_OFF, 0x38800000, 0x200000 },
  { CG3_MMAP_OFFSET, 0x38800000, SBUS_MMAP_FBSIZE(1) },
#else
  { CG3_MMAP_OFFSET, 0x0, SBUS_MMAP_FBSIZE(1) },
#endif
  { 0, 0, 0 }
};

#define _READCTL(member, out) \
{ \
  struct p9100_ctrl *actual; \
  actual = (struct p9100_ctrl *)fb->s.p9100.ctrl; \
  out = sbus_readl(&actual-> ## member ); \
}

#define READCTL(member, out) \
{ \
  struct p9100_ctrl *enab, *actual; \
  actual = (struct p9100_ctrl *)fb->s.p9100.ctrl; \
  enab = (struct p9100_ctrl *)fb->s.p9100.fbmem; \
  out = sbus_readl(&enab-> ## member ); \
  out = sbus_readl(&actual-> ## member ); \
}

#define WRITECTL(member, val) \
{ \
  u32 __writetmp; \
  struct p9100_ctrl *enab, *actual; \
  actual = (struct p9100_ctrl *)fb->s.p9100.ctrl; \
  enab = (struct p9100_ctrl *)fb->s.p9100.fbmem; \
  __writetmp = sbus_readl(&enab-> ## member ); \
  sbus_writel(val, &actual-> ## member ); \
}

static void p9100_loadcmap (struct fb_info_sbusfb *fb, struct display *p, int index, int count)
{
	unsigned long flags;
	u32 tmp;
	int i;

	spin_lock_irqsave(&fb->lock, flags);

	_READCTL(pwrup_cfg, tmp);
	WRITECTL(ramdac_cmap_wridx, (index << 16));

	for (i = index; count--; i++){
		_READCTL(pwrup_cfg, tmp);
		WRITECTL(ramdac_palette_data, (fb->color_map CM(i,0) << 16));
		_READCTL(pwrup_cfg, tmp);
		WRITECTL(ramdac_palette_data, (fb->color_map CM(i,1) << 16));
		_READCTL(pwrup_cfg, tmp);
		WRITECTL(ramdac_palette_data, (fb->color_map CM(i,2) << 16));
	}

	spin_unlock_irqrestore(&fb->lock, flags);
}

static void p9100_blank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&fb->lock, flags);
	READCTL(vid_screenpaint_timectl1, val);
	val &= ~ SCREENPAINT_TIMECTL1_ENABLE_VIDEO;
	WRITECTL(vid_screenpaint_timectl1, val);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void p9100_unblank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&fb->lock, flags);
	READCTL(vid_screenpaint_timectl1, val);
	val |= SCREENPAINT_TIMECTL1_ENABLE_VIDEO;
	WRITECTL(vid_screenpaint_timectl1, val);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void p9100_margins (struct fb_info_sbusfb *fb, struct display *p, int x_margin, int y_margin)
{
  p->screen_base += (y_margin - fb->y_margin) * p->line_length + 
    (x_margin - fb->x_margin);
}

static char idstring[60] __initdata = { 0 };

char * __init p9100fb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
	struct sbus_dev *sdev = fb->sbdp;
	unsigned long phys = sdev->reg_addrs[2].phys_addr;
	int tmp;

#ifndef FBCON_HAS_CFB8
	return NULL;
#endif

	/* Control regs: fb->sbdp->reg_addrs[0].phys_addr 
	 * Command regs: fb->sbdp->reg_addrs[1].phys_addr 
	 * Frame buffer: fb->sbdp->reg_addrs[2].phys_addr 
         */

	if (!fb->s.p9100.ctrl) {
		fb->s.p9100.ctrl = (struct p9100_ctrl *)
			sbus_ioremap(&sdev->resource[0], 0,
				     sdev->reg_addrs[0].reg_size, "p9100 ctrl");
	}

	strcpy(fb->info.modename, "p9100");
	strcpy(fix->id, "p9100");
	fix->accel = FB_ACCEL_SUN_CGTHREE;
	fix->line_length = fb->var.xres_virtual;

	disp->scrollmode = SCROLL_YREDRAW;
	if (!disp->screen_base)
		disp->screen_base = (char *)
			sbus_ioremap(&sdev->resource[2], 0,
				     type->fb_size, "p9100 ram");
	fb->s.p9100.fbmem = (volatile u32 *)disp->screen_base;
	disp->screen_base += fix->line_length * fb->y_margin + fb->x_margin;

	READCTL(sys_config, tmp);
        switch ((tmp >> SYS_CONFIG_PIXELSIZE_SHIFT) & 7) {
	case 7: 
	  type->fb_depth = 24; 
	  break;
	case 5: 
	  type->fb_depth = 32;
	  break;
	case 3: 
	  type->fb_depth = 16; 
	  break;
	case 2: 
	  type->fb_depth = 8; 
	  break;
	default: 
	  printk("p9100: screen depth unknown: 0x%x", tmp);
	  return NULL;
        }

	fb->dispsw = fbcon_cfb8;

	fb->margins = p9100_margins;
	fb->loadcmap = p9100_loadcmap;
	fb->blank = p9100_blank;
	fb->unblank = p9100_unblank;
	
	fb->physbase = phys;
	fb->mmap_map = p9100_mmap_map;
	
	sprintf(idstring, "%s at 0x%x", "p9100", 
		(unsigned int)disp->screen_base);

	return idstring;
}

