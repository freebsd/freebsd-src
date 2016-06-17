/*
 *
 * Hardware accelerated Matrox Millennium I, II, Mystique, G100, G200, G400 and G450.
 *
 * (c) 1998-2002 Petr Vandrovec <vandrove@vc.cvut.cz>
 *
 * Portions Copyright (c) 2001 Matrox Graphics Inc.
 *
 * Version: 1.64 2002/06/10
 *
 */

#include "matroxfb_maven.h"
#include "matroxfb_crtc2.h"
#include "matroxfb_misc.h"
#include "matroxfb_DAC1064.h"
#include <linux/matroxfb.h>
#include <asm/uaccess.h>

/* **************************************************** */

static int mem = 8192;

MODULE_PARM(mem, "i");
MODULE_PARM_DESC(mem, "Memory size reserved for dualhead (default=8MB)");

/* **************************************************** */

static int matroxfb_dh_getcolreg(unsigned regno, unsigned *red, unsigned *green,
		unsigned *blue, unsigned *transp, struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	if (regno >= 16)
		return 1;
	*red = m2info->palette[regno].red;
	*blue = m2info->palette[regno].blue;
	*green = m2info->palette[regno].green;
	*transp = m2info->palette[regno].transp;
	return 0;
#undef m2info
}

static int matroxfb_dh_setcolreg(unsigned regno, unsigned red, unsigned green,
		unsigned blue, unsigned transp, struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct display* p;

	if (regno >= 16)
		return 1;
	m2info->palette[regno].red = red;
	m2info->palette[regno].blue = blue;
	m2info->palette[regno].green = green;
	m2info->palette[regno].transp = transp;
	p = m2info->currcon_display;
	if (p->var.grayscale) {
		/* gray = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
	}
	red = CNVT_TOHW(red, p->var.red.length);
	green = CNVT_TOHW(green, p->var.green.length);
	blue = CNVT_TOHW(blue, p->var.blue.length);
	transp = CNVT_TOHW(transp, p->var.transp.length);

	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
		case 16:
			m2info->cmap.cfb16[regno] =
				(red << p->var.red.offset)     |
				(green << p->var.green.offset) |
				(blue << p->var.blue.offset)   |
				(transp << p->var.transp.offset);
			break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:
			m2info->cmap.cfb32[regno] =
				(red << p->var.red.offset)     |
				(green << p->var.green.offset) |
				(blue << p->var.blue.offset)   |
				(transp << p->var.transp.offset);
			break;
#endif
	}
	return 0;
#undef m2info
}

static inline void my_install_cmap(struct matroxfb_dh_fb_info* m2info)
{
	/* Do not touch this code if you do not understand what it does! */
	/* Never try to use do_install_cmap() instead. It is crap. */
	struct fb_cmap* cmap = &m2info->currcon_display->cmap;
	
	if (cmap->len)
		fb_set_cmap(cmap, 1, matroxfb_dh_setcolreg, &m2info->fbcon);
	else
		fb_set_cmap(fb_default_cmap(16), 1, matroxfb_dh_setcolreg, &m2info->fbcon);
}


static void matroxfb_dh_restore(struct matroxfb_dh_fb_info* m2info,
		struct my_timming* mt,
		struct display* p,
		int mode,
		unsigned int pos) {
	struct matrox_crtc2 c2;
	MINFO_FROM(m2info->primary_dev);

	switch (mode) {
		case 15:
			c2.ctl = 0x00200000;
			break;
		case 16:
			c2.ctl = 0x00400000;
			break;
/*		case 32: */
		default:
			c2.ctl = 0x00800000;
			break;
	}
	c2.ctl |= 0x00000001;	/* enable CRTC2 */
	c2.datactl = 0;
	if (ACCESS_FBINFO(outputs[1]).src == MATROXFB_SRC_CRTC2) {
		if (ACCESS_FBINFO(devflags.g450dac)) {
			c2.ctl |= 0x00000006; /* source from secondary pixel PLL */
			/* no vidrst when in monitor mode */
			if (ACCESS_FBINFO(outputs[1]).mode != MATROXFB_OUTPUT_MODE_MONITOR) {
				c2.ctl |=  0xC0001000; /* Enable H/V vidrst */
			}
		} else {
			c2.ctl |= 0x00000002; /* source from VDOCLK */
			c2.ctl |= 0xC0000000; /* enable vvidrst & hvidrst */
			/* MGA TVO is our clock source */
		}
	} else if (ACCESS_FBINFO(outputs[0]).src == MATROXFB_SRC_CRTC2) {
		c2.ctl |= 0x00000004; /* source from pixclock */
		/* PIXPLL is our clock source */
	}
	if (ACCESS_FBINFO(outputs[0]).src == MATROXFB_SRC_CRTC2) {
		c2.ctl |= 0x00100000;	/* connect CRTC2 to DAC */
	}
	if (mt->interlaced) {
		c2.ctl |= 0x02000000;	/* interlaced, second field is bigger, as G450 apparently ignores it */
		mt->VDisplay >>= 1;
		mt->VSyncStart >>= 1;
		mt->VSyncEnd >>= 1;
		mt->VTotal >>= 1;
	}
	if ((mt->HTotal & 7) == 2) {
		c2.datactl |= 0x00000010;
		mt->HTotal &= ~7;
	}
	c2.ctl |= 0x10000000;	/* 0x10000000 is VIDRST polarity */
	c2.hparam = ((mt->HDisplay - 8) << 16) | (mt->HTotal - 8);
	c2.hsync = ((mt->HSyncEnd - 8) << 16) | (mt->HSyncStart - 8);
	c2.vparam = ((mt->VDisplay - 1) << 16) | (mt->VTotal - 1);
	c2.vsync = ((mt->VSyncEnd - 1) << 16) | (mt->VSyncStart - 1);
	c2.preload = ((mt->VSyncStart) << 16) | (mt->HSyncStart);
	mga_outl(0x3C14, c2.hparam);
	mga_outl(0x3C18, c2.hsync);
	mga_outl(0x3C1C, c2.vparam);
	mga_outl(0x3C20, c2.vsync);
	mga_outl(0x3C24, c2.preload);
	{
		u_int32_t linelen = p->var.xres_virtual * (p->var.bits_per_pixel >> 3);
		if (c2.ctl & 0x02000000) {
			/* field #0 is smaller, so... */
			mga_outl(0x3C2C, pos);			/* field #1 vmemory start */
			mga_outl(0x3C28, pos + linelen);	/* field #0 vmemory start */
			linelen <<= 1;
			m2info->interlaced = 1;
		} else {
			mga_outl(0x3C28, pos);		/* vmemory start */
			m2info->interlaced = 0;
		}
		mga_outl(0x3C40, linelen);
	}
	mga_outl(0x3C4C, c2.datactl);		/* data control */
	if (c2.ctl & 0x02000000) {
		int i;

		mga_outl(0x3C10, c2.ctl & ~0x02000000);
                for (i = 0; i < 2; i++) {
                        unsigned int nl;
                        unsigned int lastl = 0;

                        while ((nl = mga_inl(0x3C48) & 0xFFF) >= lastl) {
                                lastl = nl;
                        }
                }
	}
        mga_outl(0x3C10, c2.ctl);
	ACCESS_FBINFO(hw).crtc2.ctl = c2.ctl;
	{
		u_int32_t tmp;

		tmp = mt->VDisplay << 16;		/* line compare */
		if (mt->sync & FB_SYNC_HOR_HIGH_ACT)
			tmp |= 0x00000100;
		if (mt->sync & FB_SYNC_VERT_HIGH_ACT)
			tmp |= 0x00000200;
		mga_outl(0x3C44, tmp);
	}
}

static void matroxfb_dh_disable(struct matroxfb_dh_fb_info* m2info) {
	MINFO_FROM(m2info->primary_dev);

	mga_outl(0x3C10, 0x00000004);	/* disable CRTC2, CRTC1->DAC1, PLL as clock source */
	ACCESS_FBINFO(hw).crtc2.ctl = 0x00000004;
}

static void matroxfb_dh_cfbX_init(struct matroxfb_dh_fb_info* m2info,
		struct display* p) {
	/* no acceleration for secondary head... */
}

static void matroxfb_dh_pan_var(struct matroxfb_dh_fb_info* m2info,
		struct fb_var_screeninfo* var) {
	unsigned int pos;
	unsigned int linelen;
	unsigned int pixelsize;
	MINFO_FROM(m2info->primary_dev);

	pixelsize = var->bits_per_pixel >> 3;
	linelen = var->xres_virtual * pixelsize;
	pos = var->yoffset * linelen + var->xoffset * pixelsize;
	pos += m2info->video.offbase;
	if (m2info->interlaced) {
		mga_outl(0x3C2C, pos);
		mga_outl(0x3C28, pos + linelen);
	} else {
		mga_outl(0x3C28, pos);
	}
}

static int matroxfb_dh_decode_var(struct matroxfb_dh_fb_info* m2info,
		struct display* p,
		struct fb_var_screeninfo* var,
		int *visual,
		int *video_cmap_len,
		int *mode) {
	unsigned int mask;
	unsigned int memlen;
	unsigned int vramlen;

	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
		case 16:	mask = 0x1F;
				break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:	mask = 0x0F;
				break;
#endif
		default:	return -EINVAL;
	}
	vramlen = m2info->video.len_usable;
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;
	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	var->xres_virtual = (var->xres_virtual + mask) & ~mask;
	if (var->yres_virtual > 32767)
		return -EINVAL;
	memlen = var->xres_virtual * var->yres_virtual * (var->bits_per_pixel >> 3);
	if (memlen > vramlen)
		return -EINVAL;
	if (var->xoffset + var->xres > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yoffset + var->yres > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	var->xres &= ~7;
	var->left_margin &= ~7;
	var->right_margin &= ~7;
	var->hsync_len &= ~7;

	*mode = var->bits_per_pixel;
	if (var->bits_per_pixel == 16) {
		if (var->green.length == 5) {
			var->red.offset = 10;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 5;
			var->blue.offset = 0;
			var->blue.length = 5;
			var->transp.offset = 15;
			var->transp.length = 1;
			*mode = 15;
		} else {
			var->red.offset = 11;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 0;
			var->blue.length = 5;
			var->transp.offset = 0;
			var->transp.length = 0;
		}
	} else {
			var->red.offset = 16;
			var->red.length = 8;
			var->green.offset = 8;
			var->green.length = 8;
			var->blue.offset = 0;
			var->blue.length = 8;
			var->transp.offset = 24;
			var->transp.length = 8;
	}
	*visual = FB_VISUAL_TRUECOLOR;
	*video_cmap_len = 16;
	return 0;
}

static void initMatroxDH(struct matroxfb_dh_fb_info* m2info, struct display* p) {
	switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
		case 16:
			p->dispsw_data = m2info->cmap.cfb16;
			p->dispsw = &fbcon_cfb16;
			break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:
			p->dispsw_data = m2info->cmap.cfb32;
			p->dispsw = &fbcon_cfb32;
			break;
#endif
		default:
			p->dispsw_data = NULL;
			p->dispsw = &fbcon_dummy;
			break;
	}
}

static int matroxfb_dh_open(struct fb_info* info, int user) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	MINFO_FROM(m2info->primary_dev);

	if (MINFO) {
		int err;

 		if (ACCESS_FBINFO(dead)) {
 			return -ENXIO;
 		}
		err = ACCESS_FBINFO(fbcon.fbops)->fb_open(&ACCESS_FBINFO(fbcon), user);
		if (err) {
			return err;
		}
	}
	return 0;
#undef m2info
}

static int matroxfb_dh_release(struct fb_info* info, int user) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	int err = 0;
	MINFO_FROM(m2info->primary_dev);

	if (MINFO) {
		err = ACCESS_FBINFO(fbcon.fbops)->fb_release(&ACCESS_FBINFO(fbcon), user);
	}
	return err;
#undef m2info
}

static int matroxfb_dh_get_fix(struct fb_fix_screeninfo* fix, int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct display* p;

	if (con >= 0)
		p = fb_display + con;
	else
		p = m2info->fbcon.disp;

	memset(fix, 0, sizeof(*fix));
	strcpy(fix->id, "MATROX DH");

	fix->smem_start = m2info->video.base;
	fix->smem_len = m2info->video.len_usable;
	fix->type = p->type;
	fix->type_aux = p->type_aux;
	fix->visual = p->visual;
	fix->xpanstep = 8;	/* TBD */
	fix->ypanstep = 1;
	fix->ywrapstep = 0;
	fix->line_length = p->line_length;
	fix->mmio_start = m2info->mmio.base;
	fix->mmio_len = m2info->mmio.len;
	fix->accel = 0;		/* no accel... */
	return 0;
#undef m2info
}

static int matroxfb_dh_get_var(struct fb_var_screeninfo* var, int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	if (con < 0)
		*var = m2info->fbcon.disp->var;
	else
		*var = fb_display[con].var;
	return 0;
#undef m2info
}

static int matroxfb_dh_set_var(struct fb_var_screeninfo* var, int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct display* p;
	int chgvar;
	int visual;
	int cmap_len;
	int mode;
	int err;
	MINFO_FROM(m2info->primary_dev);

	if (con < 0)
		p = m2info->fbcon.disp;
	else
		p = fb_display + con;
	if ((err = matroxfb_dh_decode_var(m2info, p, var, &visual, &cmap_len, &mode)) != 0)
		return err;
	switch (var->activate & FB_ACTIVATE_MASK) {
		case FB_ACTIVATE_TEST:	return 0;
		case FB_ACTIVATE_NXTOPEN:
		case FB_ACTIVATE_NOW:	break;
		default:		return -EINVAL;
	}
	if (con >= 0) {
		chgvar = (p->var.xres != var->xres) ||
			(p->var.yres != var->yres) ||
			(p->var.xres_virtual != var->xres_virtual) ||
			(p->var.yres_virtual != var->yres_virtual) ||
			(p->var.bits_per_pixel != var->bits_per_pixel) ||
			memcmp(&p->var.red, &var->red, sizeof(var->red)) ||
			memcmp(&p->var.green, &var->green, sizeof(var->green)) ||
			memcmp(&p->var.blue, &var->blue, sizeof(var->blue));
	} else
		chgvar = 0;
	p->var = *var;
	/* cmap */
	p->screen_base = m2info->fbcon.screen_base = vaddr_va(m2info->video.vbase);
	p->visual = visual;
	p->ypanstep = 1;
	p->ywrapstep = 0;
	p->type = FB_TYPE_PACKED_PIXELS;
	p->type_aux = 0;
	p->next_line = p->line_length = (var->xres_virtual * var->bits_per_pixel) >> 3;
	p->can_soft_blank = 0;
	p->inverse = 0;	/* TBD */
	initMatroxDH(m2info, p);
	if (chgvar && info && info->changevar)
		info->changevar(con);
	if (con == m2info->currcon) {
		struct my_timming mt;
		unsigned int pos;
		int out;
		int cnt;

		matroxfb_var2my(var, &mt);
		mt.crtc = MATROXFB_SRC_CRTC2;
		/* CRTC2 delay */
		mt.delay = 34;

		pos = (var->yoffset * var->xres_virtual + var->xoffset) * var->bits_per_pixel >> 3;
		pos += m2info->video.offbase;
		cnt = 0;
		down_read(&ACCESS_FBINFO(altout).lock);
		for (out = 0; out < MATROXFB_MAX_OUTPUTS; out++) {
			if (ACCESS_FBINFO(outputs[out]).src == MATROXFB_SRC_CRTC2) {
				cnt++;
				if (ACCESS_FBINFO(outputs[out]).output->compute) {
					ACCESS_FBINFO(outputs[out]).output->compute(ACCESS_FBINFO(outputs[out]).data, &mt);
				}
			}
		}
		ACCESS_FBINFO(crtc2).pixclock = mt.pixclock;
		ACCESS_FBINFO(crtc2).mnp = mt.mnp;
		up_read(&ACCESS_FBINFO(altout).lock);
		if (cnt) {
			matroxfb_dh_restore(m2info, &mt, p, mode, pos);
		} else {
			matroxfb_dh_disable(m2info);
		}
		DAC1064_global_init(PMINFO2);
		DAC1064_global_restore(PMINFO2);
		down_read(&ACCESS_FBINFO(altout).lock);
		for (out = 0; out < MATROXFB_MAX_OUTPUTS; out++) {
			if (ACCESS_FBINFO(outputs[out]).src == MATROXFB_SRC_CRTC2 &&
			    ACCESS_FBINFO(outputs[out]).output->program) {
				ACCESS_FBINFO(outputs[out]).output->program(ACCESS_FBINFO(outputs[out]).data);
			}
		}
		for (out = 0; out < MATROXFB_MAX_OUTPUTS; out++) {
			if (ACCESS_FBINFO(outputs[out]).src == MATROXFB_SRC_CRTC2 &&
			    ACCESS_FBINFO(outputs[out]).output->start) {
				ACCESS_FBINFO(outputs[out]).output->start(ACCESS_FBINFO(outputs[out]).data);
			}
		}
		up_read(&ACCESS_FBINFO(altout).lock);
		matroxfb_dh_cfbX_init(m2info, p);
		my_install_cmap(m2info);
	}
	return 0;
#undef m2info
}

static int matroxfb_dh_get_cmap(struct fb_cmap* cmap, int kspc, int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct display* dsp;

	if (con < 0)
		dsp = m2info->fbcon.disp;
	else
		dsp = fb_display + con;
	if (con == m2info->currcon)
		return fb_get_cmap(cmap, kspc, matroxfb_dh_getcolreg, info);
	else if (dsp->cmap.len)
		fb_copy_cmap(&dsp->cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(16), cmap, kspc ? 0 : 2);
	return 0;
#undef m2info
}

static int matroxfb_dh_set_cmap(struct fb_cmap* cmap, int kspc, int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct display* dsp;

	if (con < 0)
		dsp = m2info->fbcon.disp;
	else
		dsp = fb_display + con;
	if (dsp->cmap.len != 16) {
		int err;

		err = fb_alloc_cmap(&dsp->cmap, 16, 0);
		if (err)
			return err;
	}
	if (con == m2info->currcon)
		return fb_set_cmap(cmap, kspc, matroxfb_dh_setcolreg, info);
	else
		fb_copy_cmap(cmap, &dsp->cmap, kspc ? 0 : 1);
	return 0;
#undef m2info
}

static int matroxfb_dh_pan_display(struct fb_var_screeninfo* var, int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct display* dsp;

	if (con < 0)
		dsp = m2info->fbcon.disp;
	else
		dsp = fb_display + con;
	if (var->xoffset + dsp->var.xres > dsp->var.xres_virtual ||
	    var->yoffset + dsp->var.yres > dsp->var.yres_virtual)
		return -EINVAL;
	if (con == m2info->currcon)
		matroxfb_dh_pan_var(m2info, var);
	dsp->var.xoffset = var->xoffset;
	dsp->var.yoffset = var->yoffset;
	return 0;
#undef m2info
}

static int matroxfb_dh_switch(int con, struct fb_info* info);

static int matroxfb_dh_get_vblank(const struct matroxfb_dh_fb_info* m2info, struct fb_vblank* vblank) {
	MINFO_FROM(m2info->primary_dev);

	matroxfb_enable_irq(PMINFO 0);
	memset(vblank, 0, sizeof(*vblank));
	vblank->flags = FB_VBLANK_HAVE_VCOUNT | FB_VBLANK_HAVE_VBLANK;
	/* mask out reserved bits + field number (odd/even) */
	vblank->vcount = mga_inl(0x3C48) & 0x000007FF;
	/* compatibility stuff */
	if (vblank->vcount >= m2info->currcon_display->var.yres)
		vblank->flags |= FB_VBLANK_VBLANKING;
	if (test_bit(0, &ACCESS_FBINFO(irq_flags))) {
		vblank->flags |= FB_VBLANK_HAVE_COUNT;
		/* Only one writer, aligned int value...
		   it should work without lock and without atomic_t */
		vblank->count = ACCESS_FBINFO(crtc2).vsync.cnt;
	}
	return 0;
}

static int matroxfb_dh_ioctl(struct inode* inode,
		struct file* file,
		unsigned int cmd,
		unsigned long arg,
		int con,
		struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	MINFO_FROM(m2info->primary_dev);

	DBG("matroxfb_crtc2_ioctl")

	switch (cmd) {
		case FBIOGET_VBLANK:
			{
				struct fb_vblank vblank;
				int err;

				err = matroxfb_dh_get_vblank(m2info, &vblank);
				if (err)
					return err;
				if (copy_to_user((struct fb_vblank*)arg, &vblank, sizeof(vblank)))
					return -EFAULT;
				return 0;
			}
		case FBIO_WAITFORVSYNC:
			{
				u_int32_t crt;

				if (get_user(crt, (u_int32_t *)arg))
					return -EFAULT;

				if (crt != 0)
					return -ENODEV;
				return matroxfb_wait_for_sync(PMINFO 1);
			}
		case MATROXFB_SET_OUTPUT_MODE:
		case MATROXFB_GET_OUTPUT_MODE:
		case MATROXFB_GET_ALL_OUTPUTS:
			{
				return ACCESS_FBINFO(fbcon.fbops)->fb_ioctl(inode, file, cmd, arg, con, &ACCESS_FBINFO(fbcon));
			}
		case MATROXFB_SET_OUTPUT_CONNECTION:
			{
				u_int32_t tmp;
				int out;
				int changes;

				if (get_user(tmp, (u_int32_t*)arg))
					return -EFAULT;
				for (out = 0; out < 32; out++) {
					if (tmp & (1 << out)) {
						if (out >= MATROXFB_MAX_OUTPUTS)
							return -ENXIO;
						if (!ACCESS_FBINFO(outputs[out]).output)
							return -ENXIO;
						switch (ACCESS_FBINFO(outputs[out]).src) {
							case MATROXFB_SRC_NONE:
							case MATROXFB_SRC_CRTC2:
								break;
							default:
								return -EBUSY;
						}
					}
				}
				if (ACCESS_FBINFO(devflags.panellink)) {
					if (tmp & MATROXFB_OUTPUT_CONN_DFP)
						return -EINVAL;
					if ((ACCESS_FBINFO(outputs[2]).src == MATROXFB_SRC_CRTC1) && tmp)
						return -EBUSY;
				}
				changes = 0;
				for (out = 0; out < MATROXFB_MAX_OUTPUTS; out++) {
					if (tmp & (1 << out)) {
						if (ACCESS_FBINFO(outputs[out]).src != MATROXFB_SRC_CRTC2) {
							changes = 1;
							ACCESS_FBINFO(outputs[out]).src = MATROXFB_SRC_CRTC2;
						}
					} else if (ACCESS_FBINFO(outputs[out]).src == MATROXFB_SRC_CRTC2) {
						changes = 1;
						ACCESS_FBINFO(outputs[out]).src = MATROXFB_SRC_NONE;
					}
				}
				if (!changes)
					return 0;
				matroxfb_dh_switch(m2info->currcon, info);
				return 0;
			}
		case MATROXFB_GET_OUTPUT_CONNECTION:
			{
				u_int32_t conn = 0;
				int out;

				for (out = 0; out < MATROXFB_MAX_OUTPUTS; out++) {
					if (ACCESS_FBINFO(outputs[out]).src == MATROXFB_SRC_CRTC2) {
						conn |= 1 << out;
					}
				}
				if (put_user(conn, (u_int32_t*)arg))
					return -EFAULT;
				return 0;
			}
		case MATROXFB_GET_AVAILABLE_OUTPUTS:
			{
				u_int32_t tmp = 0;
				int out;

				for (out = 0; out < MATROXFB_MAX_OUTPUTS; out++) {
					if (ACCESS_FBINFO(outputs[out]).output) {
						switch (ACCESS_FBINFO(outputs[out]).src) {
							case MATROXFB_SRC_NONE:
							case MATROXFB_SRC_CRTC2:
								tmp |= 1 << out;
								break;
						}
					}
				}
				if (ACCESS_FBINFO(devflags.panellink)) {
					tmp &= ~MATROXFB_OUTPUT_CONN_DFP;
					if (ACCESS_FBINFO(outputs[2]).src == MATROXFB_SRC_CRTC1) {
						tmp = 0;
					}
				}
				if (put_user(tmp, (u_int32_t*)arg))
					return -EFAULT;
				return 0;
			}
	}
	return -EINVAL;
#undef m2info
}

static int matroxfb_dh_blank(int blank, struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	switch (blank) {
		case 1:
		case 2:
		case 3:
		case 4:
		default:;
	}
	/* do something... */
	return 0;
#undef m2info
}

static void matroxfb_dh_blank24(int blank, struct fb_info* info) {
	matroxfb_dh_blank(blank, info);
}

static struct fb_ops matroxfb_dh_ops = {
	owner:		THIS_MODULE,
	fb_open:	matroxfb_dh_open,
	fb_release:	matroxfb_dh_release,
	fb_get_fix:	matroxfb_dh_get_fix,
	fb_get_var:	matroxfb_dh_get_var,
	fb_set_var:	matroxfb_dh_set_var,
	fb_get_cmap:	matroxfb_dh_get_cmap,
	fb_set_cmap:	matroxfb_dh_set_cmap,
	fb_pan_display:	matroxfb_dh_pan_display,
	fb_ioctl:	matroxfb_dh_ioctl,
};

static int matroxfb_dh_switch(int con, struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	struct fb_cmap* cmap;
	struct display* p;

	if (m2info->currcon >= 0) {
		cmap = &m2info->currcon_display->cmap;
		if (cmap->len) {
			fb_get_cmap(cmap, 1, matroxfb_dh_getcolreg, info);
		}
	}
	m2info->currcon = con;
	if (con < 0)
		p = m2info->fbcon.disp;
	else
		p = fb_display + con;
	m2info->currcon_display = p;
	p->var.activate = FB_ACTIVATE_NOW;
	matroxfb_dh_set_var(&p->var, con, info);
	return 0;
#undef m2info
}

static int matroxfb_dh_updatevar(int con, struct fb_info* info) {
#define m2info (list_entry(info, struct matroxfb_dh_fb_info, fbcon))
	matroxfb_dh_pan_var(m2info, &fb_display[con].var);
	return 0;
#undef m2info
}

static struct fb_var_screeninfo matroxfb_dh_defined = {
		640,480,640,480,/* W,H, virtual W,H */
		0,0,		/* offset */
		32,		/* depth */
		0,		/* gray */
		{0,0,0},	/* R */
		{0,0,0},	/* G */
		{0,0,0},	/* B */
		{0,0,0},	/* alpha */
		0,		/* nonstd */
		FB_ACTIVATE_NOW,
		-1,-1,		/* display size */
		0,		/* accel flags */
		39721L,48L,16L,33L,10L,
		96L,2,0,	/* no sync info */
		FB_VMODE_NONINTERLACED,
		{0,0,0,0,0,0}
};

static int matroxfb_dh_regit(CPMINFO struct matroxfb_dh_fb_info* m2info) {
#define minfo (m2info->primary_dev)
	struct display* d;
	void* oldcrtc2;

	d = kmalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		return -ENOMEM;
	}

	memset(d, 0, sizeof(*d));

	strcpy(m2info->fbcon.modename, "MATROX CRTC2");
	m2info->fbcon.changevar = NULL;
	m2info->fbcon.node = NODEV;
	m2info->fbcon.fbops = &matroxfb_dh_ops;
	m2info->fbcon.disp = d;
	m2info->fbcon.switch_con = &matroxfb_dh_switch;
	m2info->fbcon.updatevar = &matroxfb_dh_updatevar;
	m2info->fbcon.blank = &matroxfb_dh_blank24;
	m2info->fbcon.flags = FBINFO_FLAG_DEFAULT;
	m2info->currcon = -1;
	m2info->currcon_display = d;

	if (mem < 64)
		mem *= 1024;
	if (mem < 64*1024)
		mem *= 1024;
	mem &= ~0x00000FFF;	/* PAGE_MASK? */
	if (ACCESS_FBINFO(video.len_usable) + mem <= ACCESS_FBINFO(video.len))
		m2info->video.offbase = ACCESS_FBINFO(video.len) - mem;
	else if (ACCESS_FBINFO(video.len) < mem) {
		kfree(d);
		return -ENOMEM;
	} else { /* check yres on first head... */
		m2info->video.borrowed = mem;
		ACCESS_FBINFO(video.len_usable) -= mem;
		m2info->video.offbase = ACCESS_FBINFO(video.len_usable);
	}
	m2info->video.base = ACCESS_FBINFO(video.base) + m2info->video.offbase;
	m2info->video.len = m2info->video.len_usable = m2info->video.len_maximum = mem;
	m2info->video.vbase.vaddr = vaddr_va(ACCESS_FBINFO(video.vbase)) + m2info->video.offbase;
	m2info->mmio.base = ACCESS_FBINFO(mmio.base);
	m2info->mmio.vbase = ACCESS_FBINFO(mmio.vbase);
	m2info->mmio.len = ACCESS_FBINFO(mmio.len);

	/*
	 *  If we have unused output, connect CRTC2 to it...
	 */
	if (ACCESS_FBINFO(outputs[1]).output &&
	    ACCESS_FBINFO(outputs[1]).src == MATROXFB_SRC_NONE &&
	    ACCESS_FBINFO(outputs[2]).src == MATROXFB_SRC_NONE) {
		ACCESS_FBINFO(outputs[1]).src = MATROXFB_SRC_CRTC2;
	}

	matroxfb_dh_set_var(&matroxfb_dh_defined, -2, &m2info->fbcon);
	if (register_framebuffer(&m2info->fbcon)) {
		kfree(d);
		return -ENXIO;
	}
	if (m2info->currcon < 0) {
		matroxfb_dh_set_var(&matroxfb_dh_defined, -1, &m2info->fbcon);
	}
	down_write(&ACCESS_FBINFO(crtc2.lock));
	oldcrtc2 = ACCESS_FBINFO(crtc2.info);
	ACCESS_FBINFO(crtc2.info) = m2info;
	up_write(&ACCESS_FBINFO(crtc2.lock));
	if (oldcrtc2) {
		printk(KERN_ERR "matroxfb_crtc2: Internal consistency check failed: crtc2 already present: %p\n",
			oldcrtc2);
	}
	return 0;
#undef minfo
}

/* ************************** */

static int matroxfb_dh_registerfb(struct matroxfb_dh_fb_info* m2info) {
#define minfo (m2info->primary_dev)
	if (matroxfb_dh_regit(PMINFO m2info)) {
		printk(KERN_ERR "matroxfb_crtc2: secondary head failed to register\n");
		return -1;
	}
	printk(KERN_INFO "matroxfb_crtc2: secondary head of fb%u was registered as fb%u\n",
		GET_FB_IDX(ACCESS_FBINFO(fbcon.node)), GET_FB_IDX(m2info->fbcon.node));
	m2info->fbcon_registered = 1;
	return 0;
#undef minfo
}

static void matroxfb_dh_deregisterfb(struct matroxfb_dh_fb_info* m2info) {
#define minfo (m2info->primary_dev)
	if (m2info->fbcon_registered) {
		int id;
		struct matroxfb_dh_fb_info* crtc2;

		down_write(&ACCESS_FBINFO(crtc2.lock));
		crtc2 = ACCESS_FBINFO(crtc2.info);
		if (crtc2 == m2info)
			ACCESS_FBINFO(crtc2.info) = NULL;
		up_write(&ACCESS_FBINFO(crtc2.lock));
		if (crtc2 != m2info) {
			printk(KERN_ERR "matroxfb_crtc2: Internal consistency check failed: crtc2 mismatch at unload: %p != %p\n",
				crtc2, m2info);
			printk(KERN_ERR "matroxfb_crtc2: Expect kernel crash after module unload.\n");
			return;
		}
		id = GET_FB_IDX(m2info->fbcon.node);
		unregister_framebuffer(&m2info->fbcon);
		kfree(m2info->fbcon.disp);
		/* return memory back to primary head */
		ACCESS_FBINFO(video.len_usable) += m2info->video.borrowed;
		printk(KERN_INFO "matroxfb_crtc2: fb%u unregistered\n", id);
		m2info->fbcon_registered = 0;
	}
#undef minfo
}

static void* matroxfb_crtc2_probe(struct matrox_fb_info* minfo) {
	struct matroxfb_dh_fb_info* m2info;

	/* hardware is CRTC2 incapable... */
	if (!ACCESS_FBINFO(devflags.crtc2))
		return NULL;
	m2info = (struct matroxfb_dh_fb_info*)kmalloc(sizeof(*m2info), GFP_KERNEL);
	if (!m2info) {
		printk(KERN_ERR "matroxfb_crtc2: Not enough memory for CRTC2 control structs\n");
		return NULL;
	}
	memset(m2info, 0, sizeof(*m2info));
	m2info->primary_dev = MINFO;
	if (matroxfb_dh_registerfb(m2info)) {
		kfree(m2info);
		printk(KERN_ERR "matroxfb_crtc2: CRTC2 framebuffer failed to register\n");
		return NULL;
	}
	return m2info;
}

static void matroxfb_crtc2_remove(struct matrox_fb_info* minfo, void* crtc2) {
	matroxfb_dh_deregisterfb(crtc2);
	kfree(crtc2);
}

static struct matroxfb_driver crtc2 = {
		name:	"Matrox G400 CRTC2",
		probe:	matroxfb_crtc2_probe,
		remove:	matroxfb_crtc2_remove };

static int matroxfb_crtc2_init(void) {
	matroxfb_register_driver(&crtc2);
	return 0;
}

static void matroxfb_crtc2_exit(void) {
	matroxfb_unregister_driver(&crtc2);
}

MODULE_AUTHOR("(c) 1999-2002 Petr Vandrovec <vandrove@vc.cvut.cz>");
MODULE_DESCRIPTION("Matrox G400 CRTC2 driver");
MODULE_LICENSE("GPL");
module_init(matroxfb_crtc2_init);
module_exit(matroxfb_crtc2_exit);
/* we do not have __setup() yet */
