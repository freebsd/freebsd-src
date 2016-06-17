/* $Id: leofb.c,v 1.14 2001/10/16 05:44:44 davem Exp $
 * leofb.c: Leo (ZX) 24/8bit frame buffer driver
 *
 * Copyright (C) 1996-1999 Jakub Jelinek (jj@ultra.linux.cz)
 * Copyright (C) 1997 Michal Rehacek (Michal.Rehacek@st.mff.cuni.cz)
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

#define LEO_OFF_LC_SS0_KRN	0x00200000UL
#define LEO_OFF_LC_SS0_USR	0x00201000UL
#define LEO_OFF_LC_SS1_KRN	0x01200000UL
#define LEO_OFF_LC_SS1_USR	0x01201000UL
#define LEO_OFF_LD_SS0		0x00400000UL
#define LEO_OFF_LD_SS1		0x01400000UL
#define LEO_OFF_LD_GBL		0x00401000UL
#define LEO_OFF_LX_KRN		0x00600000UL
#define LEO_OFF_LX_CURSOR	0x00601000UL
#define LEO_OFF_SS0		0x00800000UL
#define LEO_OFF_SS1		0x01800000UL
#define LEO_OFF_UNK		0x00602000UL
#define LEO_OFF_UNK2		0x00000000UL

#define LEO_CUR_ENABLE		0x00000080
#define LEO_CUR_UPDATE		0x00000030
#define LEO_CUR_PROGRESS	0x00000006
#define LEO_CUR_UPDATECMAP	0x00000003

#define LEO_CUR_TYPE_MASK	0x00000000
#define LEO_CUR_TYPE_IMAGE	0x00000020
#define LEO_CUR_TYPE_CMAP	0x00000050

struct leo_cursor {
	u8		xxx0[16];
	volatile u32	cur_type;
	volatile u32	cur_misc;
	volatile u32	cur_cursxy;
	volatile u32	cur_data;
};

#define LEO_KRN_TYPE_CLUT0	0x00001000
#define LEO_KRN_TYPE_CLUT1	0x00001001
#define LEO_KRN_TYPE_CLUT2	0x00001002
#define LEO_KRN_TYPE_WID	0x00001003
#define LEO_KRN_TYPE_UNK	0x00001006
#define LEO_KRN_TYPE_VIDEO	0x00002003
#define LEO_KRN_TYPE_CLUTDATA	0x00004000
#define LEO_KRN_CSR_ENABLE	0x00000008
#define LEO_KRN_CSR_PROGRESS	0x00000004
#define LEO_KRN_CSR_UNK		0x00000002
#define LEO_KRN_CSR_UNK2	0x00000001

struct leo_lx_krn {
	volatile u32	krn_type;
	volatile u32	krn_csr;
	volatile u32	krn_value;
};

struct leo_lc_ss0_krn {
	volatile u32 	misc;
	u8		xxx0[0x800-4];
	volatile u32	rev;
};

struct leo_lc_ss0_usr {
	volatile u32	csr;
	volatile u32	addrspace;
	volatile u32 	fontmsk;
	volatile u32	fontt;
	volatile u32	extent;
	volatile u32	src;
	u32		dst;
	volatile u32	copy;
	volatile u32	fill;
};

struct leo_lc_ss1_krn {
	u8	unknown;
};

struct leo_lc_ss1_usr {
	u8	unknown;
};

struct leo_ld {
	u8		xxx0[0xe00];
	volatile u32	csr;
	volatile u32	wid;
	volatile u32	wmask;
	volatile u32	widclip;
	volatile u32	vclipmin;
	volatile u32	vclipmax;
	volatile u32	pickmin;	/* SS1 only */
	volatile u32	pickmax;	/* SS1 only */
	volatile u32	fg;
	volatile u32	bg;
	volatile u32	src;		/* Copy/Scroll (SS0 only) */
	volatile u32	dst;		/* Copy/Scroll/Fill (SS0 only) */
	volatile u32	extent;		/* Copy/Scroll/Fill size (SS0 only) */
	u32		xxx1[3];
	volatile u32	setsem;		/* SS1 only */
	volatile u32	clrsem;		/* SS1 only */
	volatile u32	clrpick;	/* SS1 only */
	volatile u32	clrdat;		/* SS1 only */
	volatile u32	alpha;		/* SS1 only */
	u8		xxx2[0x2c];
	volatile u32	winbg;
	volatile u32	planemask;
	volatile u32	rop;
	volatile u32	z;
	volatile u32	dczf;		/* SS1 only */
	volatile u32	dczb;		/* SS1 only */
	volatile u32	dcs;		/* SS1 only */
	volatile u32	dczs;		/* SS1 only */
	volatile u32	pickfb;		/* SS1 only */
	volatile u32	pickbb;		/* SS1 only */
	volatile u32	dcfc;		/* SS1 only */
	volatile u32	forcecol;	/* SS1 only */
	volatile u32	door[8];	/* SS1 only */
	volatile u32	pick[5];	/* SS1 only */
};

#define LEO_SS1_MISC_ENABLE	0x00000001
#define LEO_SS1_MISC_STEREO	0x00000002
struct leo_ld_ss1 {
	u8		xxx0[0xef4];
	volatile u32	ss1_misc;
};

struct leo_ld_gbl {
	u8	unknown;
};

static struct sbus_mmap_map leo_mmap_map[] = {
	{ LEO_SS0_MAP,		LEO_OFF_SS0,		0x800000	},
	{ LEO_LC_SS0_USR_MAP,	LEO_OFF_LC_SS0_USR,	0x1000		},
	{ LEO_LD_SS0_MAP,	LEO_OFF_LD_SS0,		0x1000		},
	{ LEO_LX_CURSOR_MAP,	LEO_OFF_LX_CURSOR,	0x1000		},
	{ LEO_SS1_MAP,		LEO_OFF_SS1,		0x800000	},
	{ LEO_LC_SS1_USR_MAP,	LEO_OFF_LC_SS1_USR,	0x1000		},
	{ LEO_LD_SS1_MAP,	LEO_OFF_LD_SS1,		0x1000		},
	{ LEO_UNK_MAP,		LEO_OFF_UNK,		0x1000		},
	{ LEO_LX_KRN_MAP,	LEO_OFF_LX_KRN,		0x1000		},
	{ LEO_LC_SS0_KRN_MAP,	LEO_OFF_LC_SS0_KRN,	0x1000		},
	{ LEO_LC_SS1_KRN_MAP,	LEO_OFF_LC_SS1_KRN,	0x1000		},
	{ LEO_LD_GBL_MAP,	LEO_OFF_LD_GBL,		0x1000		},
	{ LEO_UNK2_MAP,		LEO_OFF_UNK2,		0x100000	},
	{ 0,			0,			0	  	}
};

static void leo_setup(struct display *p)
{
	p->next_line = 8192;
	p->next_plane = 0;
}

static void leo_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct leo_lc_ss0_usr *us = fb->s.leo.lc_ss0_usr;
	register struct leo_ld *ss = (struct leo_ld *) fb->s.leo.ld_ss0;
	unsigned long flags;
	int x, y, w, h;
	int i;

	spin_lock_irqsave(&fb->lock, flags);
	do {
		i = sbus_readl(&us->csr);
	} while (i & 0x20000000);
	sbus_writel((attr_bgcol_ec(p,conp)<<24), &ss->fg);
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
	sbus_writel((w - 1) | ((h - 1) << 11), &us->extent);
	sbus_writel((x + fb->x_margin) | ((y + fb->y_margin) << 11) | 0x80000000,
		    &us->fill);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_fill(struct fb_info_sbusfb *fb, struct display *p, int s,
		     int count, unsigned short *boxes)
{
	int i;
	register struct leo_lc_ss0_usr *us = fb->s.leo.lc_ss0_usr;
	register struct leo_ld *ss = (struct leo_ld *) fb->s.leo.ld_ss0;
	unsigned long flags;
	
	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel((attr_bgcol(p,s)<<24), &ss->fg);
	while (count-- > 0) {
		do {
			i = sbus_readl(&us->csr);
		} while (i & 0x20000000);
		sbus_writel((boxes[2] - boxes[0] - 1) | 
			    ((boxes[3] - boxes[1] - 1) << 11),
			    &us->extent);
		sbus_writel(boxes[0] | (boxes[1] << 11) | 0x80000000,
			    &us->fill);
		boxes += 4;
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_putc(struct vc_data *conp, struct display *p, int c, int yy, int xx)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct leo_lc_ss0_usr *us = fb->s.leo.lc_ss0_usr;
	register struct leo_ld *ss = (struct leo_ld *) fb->s.leo.ld_ss0;
	unsigned long flags;
	int i, x, y;
	u8 *fd;
	u32 *u;

	spin_lock_irqsave(&fb->lock, flags);
	if (fontheightlog(p)) {
		y = yy << (fontheightlog(p) + 11);
		i = (c & p->charmask) << fontheightlog(p);
	} else {
		y = (yy * fontheight(p)) << 11;
		i = (c & p->charmask) * fontheight(p);
	}
	if (fontwidth(p) <= 8)
		fd = p->fontdata + i;
	else
		fd = p->fontdata + (i << 1);
	if (fontwidthlog(p))
		x = xx << fontwidthlog(p);
	else
		x = xx * fontwidth(p);
	do {
		i = sbus_readl(&us->csr);
	} while (i & 0x20000000);
	sbus_writel(attr_fgcol(p,c) << 24, &ss->fg);
	sbus_writel(attr_bgcol(p,c) << 24, &ss->bg);
	sbus_writel(0xFFFFFFFF<<(32-fontwidth(p)),
		    &us->fontmsk);
	u = ((u32 *)p->screen_base) + y + x;
	if (fontwidth(p) <= 8) {
		for (i = 0; i < fontheight(p); i++, u += 2048) {
			u32 val = *fd++ << 24;

			sbus_writel(val, u);
		}
	} else {
		for (i = 0; i < fontheight(p); i++, u += 2048) {
			u32 val = *(u16 *)fd << 16;

			sbus_writel(val, u);
			fd += 2;
		}
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_putcs(struct vc_data *conp, struct display *p, const unsigned short *s,
		      int count, int yy, int xx)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)p->fb_info;
	register struct leo_lc_ss0_usr *us = fb->s.leo.lc_ss0_usr;
	register struct leo_ld *ss = (struct leo_ld *) fb->s.leo.ld_ss0;
	unsigned long flags;
	int i, x, y;
	u8 *fd1, *fd2, *fd3, *fd4;
	u16 c;
	u32 *u;

	spin_lock_irqsave(&fb->lock, flags);
	do {
		i = sbus_readl(&us->csr);
	} while (i & 0x20000000);
	c = scr_readw(s);
	sbus_writel(attr_fgcol(p, c) << 24, &ss->fg);
	sbus_writel(attr_bgcol(p, c) << 24, &ss->bg);
	sbus_writel(0xFFFFFFFF<<(32-fontwidth(p)), &us->fontmsk);
	if (fontwidthlog(p))
		x = (xx << fontwidthlog(p));
	else
		x = xx * fontwidth(p);
	if (fontheightlog(p))
		y = yy << (fontheightlog(p) + 11);
	else
		y = (yy * fontheight(p)) << 11;
	u = ((u32 *)p->screen_base) + y + x;
	if (fontwidth(p) <= 8) {
		sbus_writel(0xFFFFFFFF<<(32-4*fontwidth(p)), &us->fontmsk);
		x = 4*fontwidth(p) - fontheight(p)*2048;
		while (count >= 4) {
			count -= 4;
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
				for (i = 0; i < fontheight(p); i++, u += 2048) {
					u32 val;

					val = ((u32)*fd4++) | ((((u32)*fd3++) | ((((u32)*fd2++) | (((u32)*fd1++) 
						<< 8)) << 8)) << 8);
					sbus_writel(val, u);
				}
				u += x;
			} else {
				for (i = 0; i < fontheight(p); i++, u += 2048) {
					u32 val;

					val = (((u32)*fd4++) | ((((u32)*fd3++) | ((((u32)*fd2++) | (((u32)*fd1++) 
						<< fontwidth(p))) << fontwidth(p))) << fontwidth(p))) << (24 - 3 * fontwidth(p));
					sbus_writel(val, u);
				}
				u += x;
			}
		}
	} else {
		sbus_writel(0xFFFFFFFF<<(32-2*fontwidth(p)), &us->fontmsk);
		x = 2*fontwidth(p) - fontheight(p)*2048;
		while (count >= 2) {
			count -= 2;
			if (fontheightlog(p)) {
				fd1 = p->fontdata + ((scr_readw(s++) & p->charmask) << (fontheightlog(p) + 1));
				fd2 = p->fontdata + ((scr_readw(s++) & p->charmask) << (fontheightlog(p) + 1));
			} else {
				fd1 = p->fontdata + (((scr_readw(s++) & p->charmask) * fontheight(p)) << 1);
				fd2 = p->fontdata + (((scr_readw(s++) & p->charmask) * fontheight(p)) << 1);
			}
			for (i = 0; i < fontheight(p); i++, u += 2048) {
				u32 val;

				val = ((((u32)*(u16 *)fd1) << fontwidth(p)) | ((u32)*(u16 *)fd2)) << (16 - fontwidth(p));
				sbus_writel(val, u);
				fd1 += 2; fd2 += 2;
			}
			u += x;
		}
	}
	sbus_writel(0xFFFFFFFF<<(32-fontwidth(p)), &us->fontmsk);
	x = fontwidth(p) - fontheight(p)*2048;
	while (count) {
		count--;
		if (fontheightlog(p))
			i = ((scr_readw(s++) & p->charmask) << fontheightlog(p));
		else
			i = ((scr_readw(s++) & p->charmask) * fontheight(p));
		if (fontwidth(p) <= 8) {
			fd1 = p->fontdata + i;
			for (i = 0; i < fontheight(p); i++, u += 2048) {
				u32 val = *fd1++ << 24;

				sbus_writel(val, u);
			}
		} else {
			fd1 = p->fontdata + (i << 1);
			for (i = 0; i < fontheight(p); i++, u += 2048) {
				u32 val = *(u16 *)fd1 << 16;

				sbus_writel(val, u);
				fd1 += 2;
			}
		}
		u += x;
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_revc(struct display *p, int xx, int yy)
{
	/* Not used if hw cursor */
}

static int leo_wait (struct leo_lx_krn *lx_krn)
{
	int i;
	
	for (i = 0; (sbus_readl(&lx_krn->krn_csr) & LEO_KRN_CSR_PROGRESS) && i < 300000; i++)
		udelay (1); /* Busy wait at most 0.3 sec */
	if (i == 300000)
		return -EFAULT; /* Timed out - should we print some message? */
	return 0;
}

static void leo_loadcmap (struct fb_info_sbusfb *fb, struct display *p, int index, int count)
{
        struct leo_lx_krn *lx_krn = fb->s.leo.lx_krn;
	unsigned long flags;
	u32 tmp;
	int i;
	
	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(LEO_KRN_TYPE_CLUT0, &lx_krn->krn_type);
	i = leo_wait (lx_krn);
	if (i)
		goto out;
	sbus_writel(LEO_KRN_TYPE_CLUTDATA, &lx_krn->krn_type);
	for (i = 0; i < 256; i++) {
		u32 val;

		val = fb->color_map CM(i,0) |
			(fb->color_map CM(i,1) << 8) |
			(fb->color_map CM(i,2) << 16);

		sbus_writel(val, &lx_krn->krn_value); /* Throw colors there :)) */
	}
	sbus_writel(LEO_KRN_TYPE_CLUT0, &lx_krn->krn_type);
	tmp = sbus_readl(&lx_krn->krn_csr);
	tmp |= (LEO_KRN_CSR_UNK|LEO_KRN_CSR_UNK2);
	sbus_writel(tmp, &lx_krn->krn_csr);
out:
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_restore_palette (struct fb_info_sbusfb *fb)
{
	u32 tmp;
	unsigned long flags;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readl(&fb->s.leo.ld_ss1->ss1_misc);
	tmp &= ~(LEO_SS1_MISC_ENABLE);
	sbus_writel(tmp, &fb->s.leo.ld_ss1->ss1_misc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static struct display_switch leo_dispsw __initdata = {
	setup:		leo_setup,
	bmove:		fbcon_redraw_bmove,
	clear:		leo_clear,
	putc:		leo_putc,
	putcs:		leo_putcs,
	revc:		leo_revc, 
	fontwidthmask:	FONTWIDTHRANGE(1,16) /* Allow fontwidths up to 16 */
};

static void leo_setcursormap (struct fb_info_sbusfb *fb, u8 *red, u8 *green, u8 *blue)
{
        struct leo_cursor *l = fb->s.leo.cursor;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&fb->lock, flags);
	for (i = 0; (sbus_readl(&l->cur_misc) & LEO_CUR_PROGRESS) && i < 300000; i++)
		udelay (1); /* Busy wait at most 0.3 sec */
	if (i == 300000)
		goto out; /* Timed out - should we print some message? */
	sbus_writel(LEO_CUR_TYPE_CMAP, &l->cur_type);
	sbus_writel((red[0] | (green[0]<<8) | (blue[0]<<16)), &l->cur_data);
	sbus_writel((red[1] | (green[1]<<8) | (blue[1]<<16)), &l->cur_data);
	sbus_writel(LEO_CUR_UPDATECMAP, &l->cur_misc);
out:
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Set cursor shape */
static void leo_setcurshape (struct fb_info_sbusfb *fb)
{
	int i, j, k;
	u32 m, n, mask;
	struct leo_cursor *l = fb->s.leo.cursor;
	u32 tmp;
	unsigned long flags;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readl(&l->cur_misc);
	tmp &= ~LEO_CUR_ENABLE;
	sbus_writel(tmp, &l->cur_misc);
	for (k = 0; k < 2; k ++) {
		sbus_writel((k * LEO_CUR_TYPE_IMAGE), &l->cur_type);
		for (i = 0; i < 32; i++) {
			mask = 0;
			m = fb->cursor.bits[k][i];
			/* mask = m with reversed bit order */
			for (j = 0, n = 1; j < 32; j++, n <<= 1)
				if (m & n)
					mask |= (0x80000000 >> j);
			sbus_writel(mask, &l->cur_data);
		}
	}
	tmp = sbus_readl(&l->cur_misc);
	tmp |= LEO_CUR_ENABLE;
	sbus_writel(tmp, &l->cur_misc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

/* Load cursor information */
static void leo_setcursor (struct fb_info_sbusfb *fb)
{
	struct cg_cursor *c = &fb->cursor;
	struct leo_cursor *l = fb->s.leo.cursor;
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	tmp = sbus_readl(&l->cur_misc);
	tmp &= ~LEO_CUR_ENABLE;
	sbus_writel(tmp, &l->cur_misc);

	sbus_writel(((c->cpos.fbx - c->chot.fbx) & 0x7ff) |
		    (((c->cpos.fby - c->chot.fby) & 0x7ff) << 11),
		    &l->cur_cursxy);

	tmp = sbus_readl(&l->cur_misc);
	tmp |= LEO_CUR_UPDATE;
	if (c->enable)
		tmp |= LEO_CUR_ENABLE;
	sbus_writel(tmp, &l->cur_misc);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_blank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(LEO_KRN_TYPE_VIDEO, &fb->s.leo.lx_krn->krn_type);

	tmp = sbus_readl(&fb->s.leo.lx_krn->krn_csr);
	tmp &= ~LEO_KRN_CSR_ENABLE;
	sbus_writel(tmp, &fb->s.leo.lx_krn->krn_csr);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static void leo_unblank (struct fb_info_sbusfb *fb)
{
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(LEO_KRN_TYPE_VIDEO, &fb->s.leo.lx_krn->krn_type);

	tmp = sbus_readl(&fb->s.leo.lx_krn->krn_csr);
	if (!(tmp & LEO_KRN_CSR_ENABLE)) {
		tmp |= LEO_KRN_CSR_ENABLE;
		sbus_writel(tmp, &fb->s.leo.lx_krn->krn_csr);
	}
	spin_unlock_irqrestore(&fb->lock, flags);
}

static int __init
leo_wid_put (struct fb_info_sbusfb *fb, struct fb_wid_list *wl)
{
	struct leo_lx_krn *lx_krn = fb->s.leo.lx_krn;
	struct fb_wid_item *wi;
	int i, j;

	sbus_writel(LEO_KRN_TYPE_WID, &lx_krn->krn_type);
	i = leo_wait (lx_krn);
	if (i)
		return i;
	for (i = 0, wi = wl->wl_list; i < wl->wl_count; i++, wi++) {
		switch (wi->wi_type) {
		case FB_WID_DBL_8: j = (wi->wi_index & 0xf) + 0x40; break;
		case FB_WID_DBL_24: j = wi->wi_index & 0x3f; break;
		default: return -EINVAL;
		}
		sbus_writel(0x5800 + j, &lx_krn->krn_type);
		sbus_writel(wi->wi_values[0], &lx_krn->krn_value);
	}
	sbus_writel(LEO_KRN_TYPE_WID, &lx_krn->krn_type);
	sbus_writel(3, &lx_krn->krn_csr);
	return 0;
}

static void leo_margins (struct fb_info_sbusfb *fb, struct display *p, int x_margin, int y_margin)
{
	p->screen_base += 8192 * (y_margin - fb->y_margin) + 4 * (x_margin - fb->x_margin);
}

static void leo_switch_from_graph (struct fb_info_sbusfb *fb)
{
	register struct leo_lc_ss0_usr *us = fb->s.leo.lc_ss0_usr;
	register struct leo_ld *ss = (struct leo_ld *) fb->s.leo.ld_ss0;
	unsigned long flags;

	spin_lock_irqsave(&fb->lock, flags);
	sbus_writel(0xffffffff, &ss->wid);
	sbus_writel(0xffff, &ss->wmask);
	sbus_writel(0, &ss->vclipmin);
	sbus_writel(fb->s.leo.extent, &ss->vclipmax);
	sbus_writel(0xff000000, &ss->planemask);
	sbus_writel(0x310850, &ss->rop);
	sbus_writel(0, &ss->widclip);
	sbus_writel(4, &us->addrspace);
	sbus_writel(0, &us->fontt);
	spin_unlock_irqrestore(&fb->lock, flags);
}

static int __init leo_rasterimg (struct fb_info *info, int start)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	register struct leo_lc_ss0_usr *us = fb->s.leo.lc_ss0_usr;
	register struct leo_ld *ss = (struct leo_ld *) fb->s.leo.ld_ss0;

	if (start) {
		sbus_writel(1, &ss->wid);
		sbus_writel(0xffffff, &ss->planemask); 
		sbus_writel(0x310b90, &ss->rop);
		sbus_writel(0, &us->addrspace);
	} else {
		sbus_writel(0xffffffff, &ss->wid);
		sbus_writel(0xff000000, &ss->planemask);
		sbus_writel(0x310850, &ss->rop);
		sbus_writel(4, &us->addrspace);
	}
	return 0;
}

static char idstring[40] __initdata = { 0 };

char * __init leofb_init(struct fb_info_sbusfb *fb)
{
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	struct display *disp = &fb->disp;
	struct fbtype *type = &fb->type;
	struct sbus_dev *sdev = fb->sbdp;
	unsigned long phys = sdev->reg_addrs[0].phys_addr;
	struct fb_wid_item wi;
	struct fb_wid_list wl;
	int i;
	register struct leo_lc_ss0_usr *us;
	register struct leo_ld *ss;
	struct fb_ops *fbops;
	u32 tmp;

	strcpy(fb->info.modename, "Leo");
		
	strcpy(fix->id, "Leo");
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->line_length = 8192;
	fix->accel = FB_ACCEL_SUN_LEO;
	
	var->bits_per_pixel = 32;
	var->green.offset = 8;
	var->blue.offset = 16;
	var->accel_flags = FB_ACCELF_TEXT;
	
	fbops = kmalloc(sizeof(*fbops), GFP_KERNEL);
	if (fbops == NULL)
		return NULL;
	
	*fbops = *fb->info.fbops;
	fbops->fb_rasterimg = leo_rasterimg;
	fb->info.fbops = fbops;
	
	disp->scrollmode = SCROLL_YREDRAW;
	if (!disp->screen_base) {
		disp->screen_base = (char *)
			sbus_ioremap(&sdev->resource[0], LEO_OFF_SS0,
				     0x800000, "leo ram");
	}
	disp->screen_base += 8192 * fb->y_margin + 4 * fb->x_margin;
	us = fb->s.leo.lc_ss0_usr = (struct leo_lc_ss0_usr *)
		sbus_ioremap(&sdev->resource[0], LEO_OFF_LC_SS0_USR,
			     0x1000, "leolc ss0usr");
	fb->s.leo.ld_ss0 = (struct leo_ld_ss0 *)
		sbus_ioremap(&sdev->resource[0], LEO_OFF_LD_SS0,
			     0x1000, "leold ss0");
	ss = (struct leo_ld *) fb->s.leo.ld_ss0;
	fb->s.leo.ld_ss1 = (struct leo_ld_ss1 *)
		sbus_ioremap(&sdev->resource[0], LEO_OFF_LD_SS1,
			     0x1000, "leold ss1");
	fb->s.leo.lx_krn = (struct leo_lx_krn *)
		sbus_ioremap(&sdev->resource[0], LEO_OFF_LX_KRN,
			     0x1000, "leolx krn");
	fb->s.leo.cursor = (struct leo_cursor *)
		sbus_ioremap(&sdev->resource[0], LEO_OFF_LX_CURSOR,
			     sizeof(struct leo_cursor), "leolx cursor");
	fb->dispsw = leo_dispsw;
	
	fb->s.leo.extent = (type->fb_width-1) | ((type->fb_height-1) << 16);

	wl.wl_count = 1;
	wl.wl_list = &wi;
	wi.wi_type = FB_WID_DBL_8;
	wi.wi_index = 0;
	wi.wi_values [0] = 0x2c0;
	leo_wid_put (fb, &wl);
	wi.wi_index = 1;
	wi.wi_values [0] = 0x30;
	leo_wid_put (fb, &wl);
	wi.wi_index = 2;
	wi.wi_values [0] = 0x20;
	leo_wid_put (fb, &wl);
	wi.wi_type = FB_WID_DBL_24;
	wi.wi_index = 1;
	wi.wi_values [0] = 0x30;
	leo_wid_put (fb, &wl);

	tmp = sbus_readl(&fb->s.leo.ld_ss1->ss1_misc);
	tmp |= LEO_SS1_MISC_ENABLE;
	sbus_writel(tmp, &fb->s.leo.ld_ss1->ss1_misc);

	sbus_writel(0xffffffff, &ss->wid);
	sbus_writel(0xffff, &ss->wmask);
	sbus_writel(0, &ss->vclipmin);
	sbus_writel(fb->s.leo.extent, &ss->vclipmax);
	sbus_writel(0, &ss->fg);
	sbus_writel(0xff000000, &ss->planemask);
	sbus_writel(0x310850, &ss->rop);
	sbus_writel(0, &ss->widclip);
	sbus_writel((type->fb_width-1) | ((type->fb_height-1) << 11), &us->extent);
	sbus_writel(4, &us->addrspace);
	sbus_writel(0x80000000, &us->fill);
	sbus_writel(0, &us->fontt);
	do {
		i = sbus_readl(&us->csr);
	} while (i & 0x20000000);

	fb->margins = leo_margins;
	fb->loadcmap = leo_loadcmap;
	fb->setcursor = leo_setcursor;
	fb->setcursormap = leo_setcursormap;
	fb->setcurshape = leo_setcurshape;
	fb->restore_palette = leo_restore_palette;
	fb->switch_from_graph = leo_switch_from_graph;
	fb->fill = leo_fill;
	fb->blank = leo_blank;
	fb->unblank = leo_unblank;
	
	fb->physbase = phys;
	fb->mmap_map = leo_mmap_map;
	
#ifdef __sparc_v9__
	sprintf(idstring, "leo at %016lx", phys);
#else	
	sprintf(idstring, "leo at %x.%08lx", fb->iospace, phys);
#endif
		    
	return idstring;
}

MODULE_LICENSE("GPL");
