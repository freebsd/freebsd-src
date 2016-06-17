/*
 *
 * Hardware accelerated Matrox Millennium I, II, Mystique, G100, G200 and G400
 *
 * (c) 1998-2001 Petr Vandrovec <vandrove@vc.cvut.cz>
 *
 * Version: 1.51 2001/06/18
 *
 * MTRR stuff: 1998 Tom Rini <trini@kernel.crashing.org>
 *
 * Contributors: "menion?" <menion@mindless.com>
 *                     Betatesting, fixes, ideas
 *
 *               "Kurt Garloff" <garloff@suse.de>
 *                     Betatesting, fixes, ideas, videomodes, videomodes timmings
 *
 *               "Tom Rini" <trini@kernel.crashing.org>
 *                     MTRR stuff, PPC cleanups, betatesting, fixes, ideas
 *
 *               "Bibek Sahu" <scorpio@dodds.net>
 *                     Access device through readb|w|l and write b|w|l
 *                     Extensive debugging stuff
 *
 *               "Daniel Haun" <haund@usa.net>
 *                     Testing, hardware cursor fixes
 *
 *               "Scott Wood" <sawst46+@pitt.edu>
 *                     Fixes
 *
 *               "Gerd Knorr" <kraxel@goldbach.isdn.cs.tu-berlin.de>
 *                     Betatesting
 *
 *               "Kelly French" <targon@hazmat.com>
 *               "Fernando Herrera" <fherrera@eurielec.etsit.upm.es>
 *                     Betatesting, bug reporting
 *
 *               "Pablo Bianucci" <pbian@pccp.com.ar>
 *                     Fixes, ideas, betatesting
 *
 *               "Inaky Perez Gonzalez" <inaky@peloncho.fis.ucm.es>
 *                     Fixes, enhandcements, ideas, betatesting
 *
 *               "Ryuichi Oikawa" <roikawa@rr.iiij4u.or.jp>
 *                     PPC betatesting, PPC support, backward compatibility
 *
 *               "Paul Womar" <Paul@pwomar.demon.co.uk>
 *               "Owen Waller" <O.Waller@ee.qub.ac.uk>
 *                     PPC betatesting
 *
 *               "Thomas Pornin" <pornin@bolet.ens.fr>
 *                     Alpha betatesting
 *
 *               "Pieter van Leuven" <pvl@iae.nl>
 *               "Ulf Jaenicke-Roessler" <ujr@physik.phy.tu-dresden.de>
 *                     G100 testing
 *
 *               "H. Peter Arvin" <hpa@transmeta.com>
 *                     Ideas
 *
 *               "Cort Dougan" <cort@cs.nmt.edu>
 *                     CHRP fixes and PReP cleanup
 *
 *               "Mark Vojkovich" <mvojkovi@ucsd.edu>
 *                     G400 support
 *
 * (following author is not in any relation with this code, but his code
 *  is included in this driver)
 *
 * Based on framebuffer driver for VBE 2.0 compliant graphic boards
 *     (c) 1998 Gerd Knorr <kraxel@cs.tu-berlin.de>
 *
 * (following author is not in any relation with this code, but his ideas
 *  were used when writting this driver)
 *
 *		 FreeVBE/AF (Matrox), "Shawn Hargreaves" <shawn@talula.demon.co.uk>
 *
 */

#include "matroxfb_accel.h"
#include "matroxfb_DAC1064.h"
#include "matroxfb_Ti3026.h"
#include "matroxfb_misc.h"

#define curr_ydstorg(x)	ACCESS_FBINFO2(x, curr.ydstorg.pixels)

#define mga_ydstlen(y,l) mga_outl(M_YDSTLEN | M_EXEC, ((y) << 16) | (l))

void matrox_cfbX_init(WPMINFO struct display* p) {
	u_int32_t maccess;
	u_int32_t mpitch;
	u_int32_t mopmode;

	DBG("matrox_cfbX_init")

	mpitch = p->var.xres_virtual;

	if (p->type == FB_TYPE_TEXT) {
		maccess = 0x00000000;
		mpitch = (mpitch >> 4) | 0x8000; /* set something */
		mopmode = M_OPMODE_8BPP;
	} else {
		switch (p->var.bits_per_pixel) {
		case 4:		maccess = 0x00000000;	/* accelerate as 8bpp video */
				mpitch = (mpitch >> 1) | 0x8000; /* disable linearization */
				mopmode = M_OPMODE_4BPP;
				break;
		case 8:		maccess = 0x00000000;
				mopmode = M_OPMODE_8BPP;
				break;
		case 16:	if (p->var.green.length == 5)
					maccess = 0xC0000001;
				else
					maccess = 0x40000001;
				mopmode = M_OPMODE_16BPP;
				break;
		case 24:	maccess = 0x00000003;
				mopmode = M_OPMODE_24BPP;
				break;
		case 32:	maccess = 0x00000002;
				mopmode = M_OPMODE_32BPP;
				break;
		default:	maccess = 0x00000000;
				mopmode = 0x00000000;
				break;	/* turn off acceleration!!! */
		}
	}
	mga_fifo(8);
	mga_outl(M_PITCH, mpitch);
	mga_outl(M_YDSTORG, curr_ydstorg(MINFO));
	if (ACCESS_FBINFO(capable.plnwt))
		mga_outl(M_PLNWT, -1);
	if (ACCESS_FBINFO(capable.srcorg)) {
		mga_outl(M_SRCORG, 0);
		mga_outl(M_DSTORG, 0);
	}
	mga_outl(M_OPMODE, mopmode);
	mga_outl(M_CXBNDRY, 0xFFFF0000);
	mga_outl(M_YTOP, 0);
	mga_outl(M_YBOT, 0x01FFFFFF);
	mga_outl(M_MACCESS, maccess);
	ACCESS_FBINFO(accel.m_dwg_rect) = M_DWG_TRAP | M_DWG_SOLID | M_DWG_ARZERO | M_DWG_SGNZERO | M_DWG_SHIFTZERO;
	if (isMilleniumII(MINFO)) ACCESS_FBINFO(accel.m_dwg_rect) |= M_DWG_TRANSC;
	ACCESS_FBINFO(accel.m_opmode) = mopmode;
}

EXPORT_SYMBOL(matrox_cfbX_init);

static void matrox_cfbX_bmove(struct display* p, int sy, int sx, int dy, int dx, int height, int width) {
	int pixx = p->var.xres_virtual, start, end;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG("matrox_cfbX_bmove")

	CRITBEGIN

	sx *= fontwidth(p);
	dx *= fontwidth(p);
	width *= fontwidth(p);
	height *= fontheight(p);
	sy *= fontheight(p);
	dy *= fontheight(p);
	if ((dy < sy) || ((dy == sy) && (dx <= sx))) {
		mga_fifo(2);
		mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SHIFTZERO | M_DWG_SGNZERO |
			 M_DWG_BFCOL | M_DWG_REPLACE);
		mga_outl(M_AR5, pixx);
		width--;
		start = sy*pixx+sx+curr_ydstorg(MINFO);
		end = start+width;
	} else {
		mga_fifo(3);
		mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SHIFTZERO | M_DWG_BFCOL | M_DWG_REPLACE);
		mga_outl(M_SGN, 5);
		mga_outl(M_AR5, -pixx);
		width--;
		end = (sy+height-1)*pixx+sx+curr_ydstorg(MINFO);
		start = end+width;
		dy += height-1;
	}
	mga_fifo(4);
	mga_outl(M_AR0, end);
	mga_outl(M_AR3, start);
	mga_outl(M_FXBNDRY, ((dx+width)<<16) | dx);
	mga_ydstlen(dy, height);
	WaitTillIdle();

	CRITEND
}

#ifdef FBCON_HAS_CFB4
static void matrox_cfb4_bmove(struct display* p, int sy, int sx, int dy, int dx, int height, int width) {
	int pixx, start, end;
	CRITFLAGS
	MINFO_FROM_DISP(p);
	/* both (sx or dx or width) and fontwidth() are odd, so their multiply is
	   also odd, that means that we cannot use acceleration */

	DBG("matrox_cfb4_bmove")

	CRITBEGIN

	if ((sx | dx | width) & fontwidth(p) & 1) {
		fbcon_cfb4_bmove(p, sy, sx, dy, dx, height, width);
		return;
	}
	sx *= fontwidth(p);
	dx *= fontwidth(p);
	width *= fontwidth(p);
	height *= fontheight(p);
	sy *= fontheight(p);
	dy *= fontheight(p);
	pixx = p->var.xres_virtual >> 1;
	sx >>= 1;
	dx >>= 1;
	width >>= 1;
	if ((dy < sy) || ((dy == sy) && (dx <= sx))) {
		mga_fifo(2);
		mga_outl(M_AR5, pixx);
		mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SHIFTZERO | M_DWG_SGNZERO |
			M_DWG_BFCOL | M_DWG_REPLACE);
		width--;
		start = sy*pixx+sx+curr_ydstorg(MINFO);
		end = start+width;
	} else {
		mga_fifo(3);
		mga_outl(M_SGN, 5);
		mga_outl(M_AR5, -pixx);
		mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SHIFTZERO | M_DWG_BFCOL | M_DWG_REPLACE);
		width--;
		end = (sy+height-1)*pixx+sx+curr_ydstorg(MINFO);
		start = end+width;
		dy += height-1;
	}
	mga_fifo(5);
	mga_outl(M_AR0, end);
	mga_outl(M_AR3, start);
	mga_outl(M_FXBNDRY, ((dx+width)<<16) | dx);
	mga_outl(M_YDST, dy*pixx >> 5);
	mga_outl(M_LEN | M_EXEC, height);
	WaitTillIdle();

	CRITEND
}
#endif

static void matroxfb_accel_clear(WPMINFO u_int32_t color, int sy, int sx, int height,
		int width) {
	CRITFLAGS

	DBG("matroxfb_accel_clear")

	CRITBEGIN

	mga_fifo(5);
	mga_outl(M_DWGCTL, ACCESS_FBINFO(accel.m_dwg_rect) | M_DWG_REPLACE);
	mga_outl(M_FCOL, color);
	mga_outl(M_FXBNDRY, ((sx + width) << 16) | sx);
	mga_ydstlen(sy, height);
	WaitTillIdle();

	CRITEND
}

static void matrox_cfbX_clear(u_int32_t color, struct display* p, int sy, int sx, int height, int width) {

	DBG("matrox_cfbX_clear")

	matroxfb_accel_clear(PMXINFO(p) color, sy * fontheight(p), sx * fontwidth(p),
			     height * fontheight(p), width * fontwidth(p));
}

#ifdef FBCON_HAS_CFB4
static void matrox_cfb4_clear(struct vc_data* conp, struct display* p, int sy, int sx, int height, int width) {
	u_int32_t bgx;
	int whattodo;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG("matrox_cfb4_clear")

	CRITBEGIN

	whattodo = 0;
	bgx = attr_bgcol_ec(p, conp);
	bgx |= bgx << 4;
	bgx |= bgx << 8;
	bgx |= bgx << 16;
	sy *= fontheight(p);
	sx *= fontwidth(p);
	height *= fontheight(p);
	width *= fontwidth(p);
	if (sx & 1) {
		sx ++;
		if (!width) return;
		width --;
		whattodo = 1;
	}
	if (width & 1) {
		whattodo |= 2;
	}
	width >>= 1;
	sx >>= 1;
	if (width) {
		mga_fifo(5);
		mga_outl(M_DWGCTL, ACCESS_FBINFO(accel.m_dwg_rect) | M_DWG_REPLACE2);
		mga_outl(M_FCOL, bgx);
		mga_outl(M_FXBNDRY, ((sx + width) << 16) | sx);
		mga_outl(M_YDST, sy * p->var.xres_virtual >> 6);
		mga_outl(M_LEN | M_EXEC, height);
		WaitTillIdle();
	}
	if (whattodo) {
		u_int32_t step = p->var.xres_virtual >> 1;
		vaddr_t vbase = ACCESS_FBINFO(video.vbase);
		if (whattodo & 1) {
			unsigned int uaddr = sy * step + sx - 1;
			u_int32_t loop;
			u_int8_t bgx2 = bgx & 0xF0;
			for (loop = height; loop > 0; loop --) {
				mga_writeb(vbase, uaddr, (mga_readb(vbase, uaddr) & 0x0F) | bgx2);
				uaddr += step;
			}
		}
		if (whattodo & 2) {
			unsigned int uaddr = sy * step + sx + width;
			u_int32_t loop;
			u_int8_t bgx2 = bgx & 0x0F;
			for (loop = height; loop > 0; loop --) {
				mga_writeb(vbase, uaddr, (mga_readb(vbase, uaddr) & 0xF0) | bgx2);
				uaddr += step;
			}
		}
	}

	CRITEND
}
#endif

#ifdef FBCON_HAS_CFB8
static void matrox_cfb8_clear(struct vc_data* conp, struct display* p, int sy, int sx, int height, int width) {
	u_int32_t bgx;

	DBG("matrox_cfb8_clear")

	bgx = attr_bgcol_ec(p, conp);
	bgx |= bgx << 8;
	bgx |= bgx << 16;
	matrox_cfbX_clear(bgx, p, sy, sx, height, width);
}
#endif

#ifdef FBCON_HAS_CFB16
static void matrox_cfb16_clear(struct vc_data* conp, struct display* p, int sy, int sx, int height, int width) {
	u_int32_t bgx;

	DBG("matrox_cfb16_clear")

	bgx = ((u_int16_t*)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	matrox_cfbX_clear((bgx << 16) | bgx, p, sy, sx, height, width);
}
#endif

#if defined(FBCON_HAS_CFB32) || defined(FBCON_HAS_CFB24)
static void matrox_cfb32_clear(struct vc_data* conp, struct display* p, int sy, int sx, int height, int width) {
	u_int32_t bgx;

	DBG("matrox_cfb32_clear")

	bgx = ((u_int32_t*)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	matrox_cfbX_clear(bgx, p, sy, sx, height, width);
}
#endif

static void matrox_cfbX_fastputc(u_int32_t fgx, u_int32_t bgx, struct display* p, int c, int yy, int xx) {
	unsigned int charcell;
	unsigned int ar3;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	charcell = fontwidth(p) * fontheight(p);
	yy *= fontheight(p);
	xx *= fontwidth(p);

	CRITBEGIN

	mga_fifo(8);
	mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SGNZERO | M_DWG_SHIFTZERO | M_DWG_BMONOWF | M_DWG_LINEAR | M_DWG_REPLACE);

	mga_outl(M_FCOL, fgx);
	mga_outl(M_BCOL, bgx);
	mga_outl(M_FXBNDRY, ((xx + fontwidth(p) - 1) << 16) | xx);
	ar3 = ACCESS_FBINFO(fastfont.mgabase) + (c & p->charmask) * charcell;
	mga_outl(M_AR3, ar3);
	mga_outl(M_AR0, (ar3 + charcell - 1) & 0x0003FFFF);
	mga_ydstlen(yy, fontheight(p));
	WaitTillIdle();

	CRITEND
}

static void matrox_cfbX_putc(u_int32_t fgx, u_int32_t bgx, struct display* p, int c, int yy, int xx) {
	u_int32_t ar0;
	u_int32_t step;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matrox_cfbX_putc");

	yy *= fontheight(p);
	xx *= fontwidth(p);

	CRITBEGIN

#ifdef __BIG_ENDIAN
	WaitTillIdle();
	mga_outl(M_OPMODE, M_OPMODE_8BPP);
#else
	mga_fifo(7);
#endif
	ar0 = fontwidth(p) - 1;
	mga_outl(M_FXBNDRY, ((xx+ar0)<<16) | xx);
	if (fontwidth(p) <= 8)
		step = 1;
	else if (fontwidth(p) <= 16)
		step = 2;
	else
		step = 4;
	if (fontwidth(p) == step << 3) {
		size_t charcell = fontheight(p)*step;
		/* TODO: Align charcell to 4B for BE */
		mga_outl(M_DWGCTL, M_DWG_ILOAD | M_DWG_SGNZERO | M_DWG_SHIFTZERO | M_DWG_BMONOWF | M_DWG_LINEAR | M_DWG_REPLACE);
		mga_outl(M_FCOL, fgx);
		mga_outl(M_BCOL, bgx);
		mga_outl(M_AR3, 0);
		mga_outl(M_AR0, fontheight(p)*fontwidth(p)-1);
		mga_ydstlen(yy, fontheight(p));
		mga_memcpy_toio(ACCESS_FBINFO(mmio.vbase), 0, p->fontdata+(c&p->charmask)*charcell, charcell);
	} else {
		u8* chardata = p->fontdata+(c&p->charmask)*fontheight(p)*step;
		int i;

		mga_outl(M_DWGCTL, M_DWG_ILOAD | M_DWG_SGNZERO | M_DWG_SHIFTZERO | M_DWG_BMONOWF | M_DWG_REPLACE);
		mga_outl(M_FCOL, fgx);
		mga_outl(M_BCOL, bgx);
		mga_outl(M_AR5, 0);
		mga_outl(M_AR3, 0);
		mga_outl(M_AR0, ar0);
		mga_ydstlen(yy, fontheight(p));

		switch (step) {
		case 1:
			for (i = fontheight(p); i > 0; i--) {
#ifdef __LITTLE_ENDIAN
				mga_outl(0, *chardata++);
#else
				mga_outl(0, (*chardata++) << 24);
#endif
			}
			break;
		case 2:
			for (i = fontheight(p); i > 0; i--) {
#ifdef __LITTLE_ENDIAN
				mga_outl(0, *(u_int16_t*)chardata);
#else
				mga_outl(0, (*(u_int16_t*)chardata) << 16);
#endif
				chardata += 2;
			}
			break;
		case 4:
			mga_memcpy_toio(ACCESS_FBINFO(mmio.vbase), 0, chardata, fontheight(p) * 4);
			break;
		}
	}
	WaitTillIdle();
#ifdef __BIG_ENDIAN
	mga_outl(M_OPMODE, ACCESS_FBINFO(accel.m_opmode));
#endif
	CRITEND
}

#ifdef FBCON_HAS_CFB8
static void matrox_cfb8_putc(struct vc_data* conp, struct display* p, int c, int yy, int xx) {
	u_int32_t fgx, bgx;
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfb8_putc");

	fgx = attr_fgcol(p, c);
	bgx = attr_bgcol(p, c);
	fgx |= (fgx << 8);
	fgx |= (fgx << 16);
	bgx |= (bgx << 8);
	bgx |= (bgx << 16);
	ACCESS_FBINFO(curr.putc)(fgx, bgx, p, c, yy, xx);
}
#endif

#ifdef FBCON_HAS_CFB16
static void matrox_cfb16_putc(struct vc_data* conp, struct display* p, int c, int yy, int xx) {
	u_int32_t fgx, bgx;
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfb16_putc");

	fgx = ((u_int16_t*)p->dispsw_data)[attr_fgcol(p, c)];
	bgx = ((u_int16_t*)p->dispsw_data)[attr_bgcol(p, c)];
	fgx |= (fgx << 16);
	bgx |= (bgx << 16);
	ACCESS_FBINFO(curr.putc)(fgx, bgx, p, c, yy, xx);
}
#endif

#if defined(FBCON_HAS_CFB32) || defined(FBCON_HAS_CFB24)
static void matrox_cfb32_putc(struct vc_data* conp, struct display* p, int c, int yy, int xx) {
	u_int32_t fgx, bgx;
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfb32_putc");

	fgx = ((u_int32_t*)p->dispsw_data)[attr_fgcol(p, c)];
	bgx = ((u_int32_t*)p->dispsw_data)[attr_bgcol(p, c)];
	ACCESS_FBINFO(curr.putc)(fgx, bgx, p, c, yy, xx);
}
#endif

static void matrox_cfbX_fastputcs(u_int32_t fgx, u_int32_t bgx, struct display* p, const unsigned short* s, int count, int yy, int xx) {
	unsigned int charcell;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	yy *= fontheight(p);
	xx *= fontwidth(p);
	charcell = fontwidth(p) * fontheight(p);

	CRITBEGIN

	mga_fifo(3);
	mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SGNZERO | M_DWG_SHIFTZERO | M_DWG_BMONOWF | M_DWG_LINEAR | M_DWG_REPLACE);
	mga_outl(M_FCOL, fgx);
	mga_outl(M_BCOL, bgx);
	while (count--) {
		u_int32_t ar3 = ACCESS_FBINFO(fastfont.mgabase) + (scr_readw(s++) & p->charmask)*charcell;

		mga_fifo(4);
		mga_outl(M_FXBNDRY, ((xx + fontwidth(p) - 1) << 16) | xx);
		mga_outl(M_AR3, ar3);
		mga_outl(M_AR0, (ar3 + charcell - 1) & 0x0003FFFF);
		mga_ydstlen(yy, fontheight(p));
		xx += fontwidth(p);
	}
	WaitTillIdle();

	CRITEND
}

static void matrox_cfbX_putcs(u_int32_t fgx, u_int32_t bgx, struct display* p, const unsigned short* s, int count, int yy, int xx) {
	u_int32_t step;
	u_int32_t ydstlen;
	u_int32_t xlen;
	u_int32_t ar0;
	u_int32_t charcell;
	u_int32_t fxbndry;
	vaddr_t mmio;
	int easy;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfbX_putcs");

	yy *= fontheight(p);
	xx *= fontwidth(p);
	if (fontwidth(p) <= 8)
		step = 1;
	else if (fontwidth(p) <= 16)
		step = 2;
	else
		step = 4;
	charcell = fontheight(p)*step;
	xlen = (charcell + 3) & ~3;
	ydstlen = (yy << 16) | fontheight(p);
	if (fontwidth(p) == step << 3) {
		ar0 = fontheight(p)*fontwidth(p) - 1;
		easy = 1;
	} else {
		ar0 = fontwidth(p) - 1;
		easy = 0;
	}

	CRITBEGIN

#ifdef __BIG_ENDIAN
	WaitTillIdle();
	mga_outl(M_OPMODE, M_OPMODE_8BPP);
#else
	mga_fifo(3);
#endif
	if (easy)
		mga_outl(M_DWGCTL, M_DWG_ILOAD | M_DWG_SGNZERO | M_DWG_SHIFTZERO | M_DWG_BMONOWF | M_DWG_LINEAR | M_DWG_REPLACE);
	else
		mga_outl(M_DWGCTL, M_DWG_ILOAD | M_DWG_SGNZERO | M_DWG_SHIFTZERO | M_DWG_BMONOWF | M_DWG_REPLACE);
	mga_outl(M_FCOL, fgx);
	mga_outl(M_BCOL, bgx);
	fxbndry = ((xx + fontwidth(p) - 1) << 16) | xx;
	mmio = ACCESS_FBINFO(mmio.vbase);
	while (count--) {
		u_int8_t* chardata = p->fontdata + (scr_readw(s++) & p->charmask)*charcell;

		mga_fifo(6);
		mga_writel(mmio, M_FXBNDRY, fxbndry);
		mga_writel(mmio, M_AR0, ar0);
		mga_writel(mmio, M_AR3, 0);
		if (easy) {
			mga_writel(mmio, M_YDSTLEN | M_EXEC, ydstlen);
			mga_memcpy_toio(mmio, 0, chardata, xlen);
		} else {
			mga_writel(mmio, M_AR5, 0);
			mga_writel(mmio, M_YDSTLEN | M_EXEC, ydstlen);
			switch (step) {
				case 1:	{
						u_int8_t* charend = chardata + charcell;
						for (; chardata != charend; chardata++) {
#ifdef __LITTLE_ENDIAN
							mga_writel(mmio, 0, *chardata);
#else
							mga_writel(mmio, 0, (*chardata) << 24);
#endif
						}
					}
					break;
				case 2:	{
						u_int8_t* charend = chardata + charcell;
						for (; chardata != charend; chardata += 2) {
#ifdef __LITTLE_ENDIAN
							mga_writel(mmio, 0, *(u_int16_t*)chardata);
#else
							mga_writel(mmio, 0, (*(u_int16_t*)chardata) << 16);
#endif
						}
					}
					break;
				default:
					mga_memcpy_toio(mmio, 0, chardata, charcell);
					break;
			}
		}
		fxbndry += fontwidth(p) + (fontwidth(p) << 16);
	}
	WaitTillIdle();
#ifdef __BIG_ENDIAN
	mga_outl(M_OPMODE, ACCESS_FBINFO(accel.m_opmode));
#endif
	CRITEND
}

#ifdef FBCON_HAS_CFB8
static void matrox_cfb8_putcs(struct vc_data* conp, struct display* p, const unsigned short* s, int count, int yy, int xx) {
	u_int16_t c;
	u_int32_t fgx, bgx;
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfb8_putcs");

	c = scr_readw(s);
	fgx = attr_fgcol(p, c);
	bgx = attr_bgcol(p, c);
	fgx |= (fgx << 8);
	fgx |= (fgx << 16);
	bgx |= (bgx << 8);
	bgx |= (bgx << 16);
	ACCESS_FBINFO(curr.putcs)(fgx, bgx, p, s, count, yy, xx);
}
#endif

#ifdef FBCON_HAS_CFB16
static void matrox_cfb16_putcs(struct vc_data* conp, struct display* p, const unsigned short* s, int count, int yy, int xx) {
	u_int16_t c;
	u_int32_t fgx, bgx;
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfb16_putcs");

	c = scr_readw(s);
	fgx = ((u_int16_t*)p->dispsw_data)[attr_fgcol(p, c)];
	bgx = ((u_int16_t*)p->dispsw_data)[attr_bgcol(p, c)];
	fgx |= (fgx << 16);
	bgx |= (bgx << 16);
	ACCESS_FBINFO(curr.putcs)(fgx, bgx, p, s, count, yy, xx);
}
#endif

#if defined(FBCON_HAS_CFB32) || defined(FBCON_HAS_CFB24)
static void matrox_cfb32_putcs(struct vc_data* conp, struct display* p, const unsigned short* s, int count, int yy, int xx) {
	u_int16_t c;
	u_int32_t fgx, bgx;
	MINFO_FROM_DISP(p);

	DBG_HEAVY("matroxfb_cfb32_putcs");

	c = scr_readw(s);
	fgx = ((u_int32_t*)p->dispsw_data)[attr_fgcol(p, c)];
	bgx = ((u_int32_t*)p->dispsw_data)[attr_bgcol(p, c)];
	ACCESS_FBINFO(curr.putcs)(fgx, bgx, p, s, count, yy, xx);
}
#endif

#ifdef FBCON_HAS_CFB4
static void matrox_cfb4_revc(struct display* p, int xx, int yy) {
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG_LOOP("matroxfb_cfb4_revc");

	if (fontwidth(p) & 1) {
		fbcon_cfb4_revc(p, xx, yy);
		return;
	}
	yy *= fontheight(p);
	xx *= fontwidth(p);
	xx |= (xx + fontwidth(p)) << 16;
	xx >>= 1;

	CRITBEGIN

	mga_fifo(5);
	mga_outl(M_DWGCTL, ACCESS_FBINFO(accel.m_dwg_rect) | M_DWG_XOR);
	mga_outl(M_FCOL, 0xFFFFFFFF);
	mga_outl(M_FXBNDRY, xx);
	mga_outl(M_YDST, yy * p->var.xres_virtual >> 6);
	mga_outl(M_LEN | M_EXEC, fontheight(p));
	WaitTillIdle();

	CRITEND
}
#endif

#ifdef FBCON_HAS_CFB8
static void matrox_cfb8_revc(struct display* p, int xx, int yy) {
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG_LOOP("matrox_cfb8_revc")

	yy *= fontheight(p);
	xx *= fontwidth(p);

	CRITBEGIN

	mga_fifo(4);
	mga_outl(M_DWGCTL, ACCESS_FBINFO(accel.m_dwg_rect) | M_DWG_XOR);
	mga_outl(M_FCOL, 0x0F0F0F0F);
	mga_outl(M_FXBNDRY, ((xx + fontwidth(p)) << 16) | xx);
	mga_ydstlen(yy, fontheight(p));
	WaitTillIdle();

	CRITEND
}
#endif

#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)
static void matrox_cfbX_revc(struct display* p, int xx, int yy) {
	CRITFLAGS
	MINFO_FROM_DISP(p);

	DBG_LOOP("matrox_cfbX_revc")

	yy *= fontheight(p);
	xx *= fontwidth(p);

	CRITBEGIN

	mga_fifo(4);
	mga_outl(M_DWGCTL, ACCESS_FBINFO(accel.m_dwg_rect) | M_DWG_XOR);
	mga_outl(M_FCOL, 0xFFFFFFFF);
	mga_outl(M_FXBNDRY, ((xx + fontwidth(p)) << 16) | xx);
	mga_ydstlen(yy, fontheight(p));
	WaitTillIdle();

	CRITEND
}
#endif

static void matrox_cfbX_clear_margins(struct vc_data* conp, struct display* p, int bottom_only) {
	unsigned int bottom_height, right_width;
	unsigned int bottom_start, right_start;
	unsigned int cell_h, cell_w;

	DBG("matrox_cfbX_clear_margins")

	cell_w = fontwidth(p);
	if (!cell_w) return;	/* PARANOID */
	right_width = p->var.xres % cell_w;
	right_start = p->var.xres - right_width;
	if (!bottom_only && right_width) {
		/* clear whole right margin, not only visible portion */
		matroxfb_accel_clear(     PMXINFO(p)
			     /* color */  0x00000000,
			     /* y */      0,
			     /* x */      p->var.xoffset + right_start,
			     /* height */ p->var.yres_virtual,
			     /* width */  right_width);
	}
	cell_h = fontheight(p);
	if (!cell_h) return;	/* PARANOID */
	bottom_height = p->var.yres % cell_h;
	if (bottom_height) {
		bottom_start = p->var.yres - bottom_height;
		matroxfb_accel_clear(		  PMXINFO(p)
				     /* color */  0x00000000,
				     /* y */	  p->var.yoffset + bottom_start,
				     /* x */	  p->var.xoffset,
				     /* height */ bottom_height,
				     /* width */  right_start);
	}
}

static void matrox_text_setup(struct display* p) {
	MINFO_FROM_DISP(p);

	p->next_line = p->line_length ? p->line_length : ((p->var.xres_virtual / (fontwidth(p)?fontwidth(p):8)) * ACCESS_FBINFO(devflags.textstep));
	p->next_plane = 0;
}

static void matrox_text_bmove(struct display* p, int sy, int sx, int dy, int dx,
		int height, int width) {
	unsigned int srcoff;
	unsigned int dstoff;
	unsigned int step;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	CRITBEGIN

	step = ACCESS_FBINFO(devflags.textstep);
	srcoff = (sy * p->next_line) + (sx * step);
	dstoff = (dy * p->next_line) + (dx * step);
	if (dstoff < srcoff) {
		while (height > 0) {
			int i;
			for (i = width; i > 0; dstoff += step, srcoff += step, i--)
				mga_writew(ACCESS_FBINFO(video.vbase), dstoff, mga_readw(ACCESS_FBINFO(video.vbase), srcoff));
			height--;
			dstoff += p->next_line - width * step;
			srcoff += p->next_line - width * step;
		}
	} else {
		unsigned int off;

		off = (height - 1) * p->next_line + (width - 1) * step;
		srcoff += off;
		dstoff += off;
		while (height > 0) {
			int i;
			for (i = width; i > 0; dstoff -= step, srcoff -= step, i--)
				mga_writew(ACCESS_FBINFO(video.vbase), dstoff, mga_readw(ACCESS_FBINFO(video.vbase), srcoff));
			dstoff -= p->next_line - width * step;
			srcoff -= p->next_line - width * step;
			height--;
		}
	}
	CRITEND
}

static void matrox_text_clear(struct vc_data* conp, struct display* p, int sy, int sx,
		int height, int width) {
	unsigned int offs;
	unsigned int val;
	unsigned int step;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	step = ACCESS_FBINFO(devflags.textstep);
	offs = sy * p->next_line + sx * step;
	val = ntohs((attr_bgcol(p, conp->vc_video_erase_char) << 4) | attr_fgcol(p, conp->vc_video_erase_char) | (' ' << 8));

	CRITBEGIN

	while (height > 0) {
		int i;
		for (i = width; i > 0; offs += step, i--)
			mga_writew(ACCESS_FBINFO(video.vbase), offs, val);
		offs += p->next_line - width * step;
		height--;
	}
	CRITEND
}

static void matrox_text_putc(struct vc_data* conp, struct display* p, int c, int yy, int xx) {
	unsigned int offs;
	unsigned int chr;
	unsigned int step;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	step = ACCESS_FBINFO(devflags.textstep);
	offs = yy * p->next_line + xx * step;
	chr = attr_fgcol(p,c) | (attr_bgcol(p,c) << 4) | ((c & p->charmask) << 8);
	if (chr & 0x10000) chr |= 0x08;

	CRITBEGIN

	mga_writew(ACCESS_FBINFO(video.vbase), offs, ntohs(chr));

	CRITEND
}

static void matrox_text_putcs(struct vc_data* conp, struct display* p, const unsigned short* s,
		int count, int yy, int xx) {
	unsigned int offs;
	unsigned int attr;
	unsigned int step;
	u_int16_t c;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	step = ACCESS_FBINFO(devflags.textstep);
	offs = yy * p->next_line + xx * step;
	c = scr_readw(s);
	attr = attr_fgcol(p, c) | (attr_bgcol(p, c) << 4);

	CRITBEGIN

	while (count-- > 0) {
		unsigned int chr = ((scr_readw(s++)) & p->charmask) << 8;
		if (chr & 0x10000) chr ^= 0x10008;
		mga_writew(ACCESS_FBINFO(video.vbase), offs, ntohs(attr|chr));
		offs += step;
	}

	CRITEND
}

static void matrox_text_revc(struct display* p, int xx, int yy) {
	unsigned int offs;
	unsigned int step;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	step = ACCESS_FBINFO(devflags.textstep);
	offs = yy * p->next_line + xx * step + 1;

	CRITBEGIN

	mga_writeb(ACCESS_FBINFO(video.vbase), offs, mga_readb(ACCESS_FBINFO(video.vbase), offs) ^ 0x77);

	CRITEND
}

static void matrox_text_createcursor(WPMINFO struct display* p) {
	CRITFLAGS

	if (ACCESS_FBINFO(currcon_display) != p)
		return;

	matroxfb_createcursorshape(PMINFO p, 0);

	CRITBEGIN

	mga_setr(M_CRTC_INDEX, 0x0A, ACCESS_FBINFO(cursor.u));
	mga_setr(M_CRTC_INDEX, 0x0B, ACCESS_FBINFO(cursor.d) - 1);

	CRITEND
}

static void matrox_text_cursor(struct display* p, int mode, int x, int y) {
	unsigned int pos;
	CRITFLAGS
	MINFO_FROM_DISP(p);

	if (ACCESS_FBINFO(currcon_display) != p)
		return;

	if (mode == CM_ERASE) {
		if (ACCESS_FBINFO(cursor.state) != CM_ERASE) {

			CRITBEGIN

			mga_setr(M_CRTC_INDEX, 0x0A, 0x20);

			CRITEND

			ACCESS_FBINFO(cursor.state) = CM_ERASE;
		}
		return;
	}
	if ((p->conp->vc_cursor_type & CUR_HWMASK) != ACCESS_FBINFO(cursor.type))
		matrox_text_createcursor(PMINFO p);

	/* DO NOT CHECK cursor.x != x because of matroxfb_vgaHWinit moves cursor to 0,0 */
	ACCESS_FBINFO(cursor.x) = x;
	ACCESS_FBINFO(cursor.y) = y;
	pos = p->next_line / ACCESS_FBINFO(devflags.textstep) * y + x;

	CRITBEGIN

	mga_setr(M_CRTC_INDEX, 0x0F, pos);
	mga_setr(M_CRTC_INDEX, 0x0E, pos >> 8);

	mga_setr(M_CRTC_INDEX, 0x0A, ACCESS_FBINFO(cursor.u));

	CRITEND

	ACCESS_FBINFO(cursor.state) = CM_DRAW;
}

void matrox_text_round(CPMINFO struct fb_var_screeninfo* var, struct display* p) {
	unsigned hf;
	unsigned vf;
	unsigned vxres;
	unsigned ych;

	hf = fontwidth(p);
	if (!hf) hf = 8;
	/* do not touch xres */
	vxres = (var->xres_virtual + hf - 1) / hf;
	if (vxres >= 256)
		vxres = 255;
	if (vxres < 16)
		vxres = 16;
	vxres = (vxres + 1) & ~1;	/* must be even */
	vf = fontheight(p);
	if (!vf) vf = 16;
	if (var->yres < var->yres_virtual) {
		ych = ACCESS_FBINFO(devflags.textvram) / vxres;
		var->yres_virtual = ych * vf;
	} else
		ych = var->yres_virtual / vf;
	if (vxres * ych > ACCESS_FBINFO(devflags.textvram)) {
		ych = ACCESS_FBINFO(devflags.textvram) / vxres;
		var->yres_virtual = ych * vf;
	}
	var->xres_virtual = vxres * hf;
}

EXPORT_SYMBOL(matrox_text_round);

static int matrox_text_setfont(struct display* p, int width, int height) {
	DBG("matrox_text_setfont");

	if (p) {
		MINFO_FROM_DISP(p);

		matrox_text_round(PMINFO &p->var, p);
		p->next_line = p->line_length = ((p->var.xres_virtual / (fontwidth(p)?fontwidth(p):8)) * ACCESS_FBINFO(devflags.textstep));

		if (p->conp)
			matrox_text_createcursor(PMINFO p);
	}
	return 0;
}

#define matrox_cfb16_revc matrox_cfbX_revc
#define matrox_cfb24_revc matrox_cfbX_revc
#define matrox_cfb32_revc matrox_cfbX_revc

#define matrox_cfb24_clear matrox_cfb32_clear
#define matrox_cfb24_putc matrox_cfb32_putc
#define matrox_cfb24_putcs matrox_cfb32_putcs

#ifdef FBCON_HAS_VGATEXT
static struct display_switch matroxfb_text = {
	setup:		matrox_text_setup,
	bmove:		matrox_text_bmove,
	clear:		matrox_text_clear,
	putc:		matrox_text_putc,
	putcs:		matrox_text_putcs,
	revc:		matrox_text_revc,
	cursor:		matrox_text_cursor,
	set_font:	matrox_text_setfont,
	fontwidthmask:	FONTWIDTH(8)|FONTWIDTH(9)
};
#endif

#ifdef FBCON_HAS_CFB4
static struct display_switch matroxfb_cfb4 = {
	setup:		fbcon_cfb4_setup,
	bmove:		matrox_cfb4_bmove,
	clear:		matrox_cfb4_clear,
	putc:		fbcon_cfb4_putc,
	putcs:		fbcon_cfb4_putcs,
	revc:		matrox_cfb4_revc,
	fontwidthmask:	FONTWIDTH(8) /* fix, fix, fix it */
};
#endif

#ifdef FBCON_HAS_CFB8
static struct display_switch matroxfb_cfb8 = {
	setup:		fbcon_cfb8_setup,
	bmove:		matrox_cfbX_bmove,
	clear:		matrox_cfb8_clear,
	putc:		matrox_cfb8_putc,
	putcs:		matrox_cfb8_putcs,
	revc:		matrox_cfb8_revc,
	clear_margins:	matrox_cfbX_clear_margins,
	fontwidthmask:	~1 /* FONTWIDTHS */
};
#endif

#ifdef FBCON_HAS_CFB16
static struct display_switch matroxfb_cfb16 = {
	setup:		fbcon_cfb16_setup,
	bmove:		matrox_cfbX_bmove,
	clear:		matrox_cfb16_clear,
	putc:		matrox_cfb16_putc,
	putcs:		matrox_cfb16_putcs,
	revc:		matrox_cfb16_revc,
	clear_margins:	matrox_cfbX_clear_margins,
	fontwidthmask:	~1 /* FONTWIDTHS */
};
#endif

#ifdef FBCON_HAS_CFB24
static struct display_switch matroxfb_cfb24 = {
	setup:		fbcon_cfb24_setup,
	bmove:		matrox_cfbX_bmove,
	clear:		matrox_cfb24_clear,
	putc:		matrox_cfb24_putc,
	putcs:		matrox_cfb24_putcs,
	revc:		matrox_cfb24_revc,
	clear_margins:	matrox_cfbX_clear_margins,
	fontwidthmask:	~1 /* FONTWIDTHS */ /* TODO: and what about non-aligned access on BE? I think that there are no in my code */
};
#endif

#ifdef FBCON_HAS_CFB32
static struct display_switch matroxfb_cfb32 = {
	setup:		fbcon_cfb32_setup,
	bmove:		matrox_cfbX_bmove,
	clear:		matrox_cfb32_clear,
	putc:		matrox_cfb32_putc,
	putcs:		matrox_cfb32_putcs,
	revc:		matrox_cfb32_revc,
	clear_margins:	matrox_cfbX_clear_margins,
	fontwidthmask:	~1 /* FONTWIDTHS */
};
#endif

void initMatrox(WPMINFO struct display* p) {
	struct display_switch *swtmp;

	DBG("initMatrox")

	if (ACCESS_FBINFO(currcon_display) != p)
		return;
	if (p->dispsw && p->conp)
		fb_con.con_cursor(p->conp, CM_ERASE);
	p->dispsw_data = NULL;
	if ((p->var.accel_flags & FB_ACCELF_TEXT) != FB_ACCELF_TEXT) {
		if (p->type == FB_TYPE_TEXT) {
			swtmp = &matroxfb_text;
		} else {
			switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
			case 4:
				swtmp = &fbcon_cfb4;
				break;
#endif
#ifdef FBCON_HAS_CFB8
			case 8:
				swtmp = &fbcon_cfb8;
				break;
#endif
#ifdef FBCON_HAS_CFB16
			case 16:
				p->dispsw_data = &ACCESS_FBINFO(cmap.cfb16);
				swtmp = &fbcon_cfb16;
				break;
#endif
#ifdef FBCON_HAS_CFB24
			case 24:
				p->dispsw_data = &ACCESS_FBINFO(cmap.cfb24);
				swtmp = &fbcon_cfb24;
				break;
#endif
#ifdef FBCON_HAS_CFB32
			case 32:
				p->dispsw_data = &ACCESS_FBINFO(cmap.cfb32);
				swtmp = &fbcon_cfb32;
				break;
#endif
			default:
				p->dispsw = &fbcon_dummy;
				return;
			}
		}
		dprintk(KERN_INFO "matroxfb: acceleration disabled\n");
	} else if (p->type == FB_TYPE_TEXT) {
		swtmp = &matroxfb_text;
	} else {
		switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
		case 4:
			swtmp = &matroxfb_cfb4;
			break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
			swtmp = &matroxfb_cfb8;
			break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:
			p->dispsw_data = &ACCESS_FBINFO(cmap.cfb16);
			swtmp = &matroxfb_cfb16;
			break;
#endif
#ifdef FBCON_HAS_CFB24
		case 24:
			p->dispsw_data = &ACCESS_FBINFO(cmap.cfb24);
			swtmp = &matroxfb_cfb24;
			break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:
			p->dispsw_data = &ACCESS_FBINFO(cmap.cfb32);
			swtmp = &matroxfb_cfb32;
			break;
#endif
		default:
			p->dispsw = &fbcon_dummy;
			return;
		}
	}
	memcpy(&ACCESS_FBINFO(dispsw), swtmp, sizeof(ACCESS_FBINFO(dispsw)));
	p->dispsw = &ACCESS_FBINFO(dispsw);
	if ((p->type != FB_TYPE_TEXT) && ACCESS_FBINFO(devflags.hwcursor)) {
		ACCESS_FBINFO(hw_switch)->selhwcursor(PMINFO p);
	}
}

EXPORT_SYMBOL(initMatrox);

void matrox_init_putc(WPMINFO struct display* p, void (*dac_createcursor)(WPMINFO struct display* p)) {
	int i;

	if (p && p->conp) {
		if (p->type == FB_TYPE_TEXT) {
			matrox_text_createcursor(PMINFO p);
			matrox_text_loadfont(PMINFO p);
			i = 0;
		} else {
			dac_createcursor(PMINFO p);
			i = matroxfb_fastfont_tryset(PMINFO p);
		}
	} else
		i = 0;
	if (i) {
		ACCESS_FBINFO(curr.putc) = matrox_cfbX_fastputc;
		ACCESS_FBINFO(curr.putcs) = matrox_cfbX_fastputcs;
	} else {
		ACCESS_FBINFO(curr.putc) = matrox_cfbX_putc;
		ACCESS_FBINFO(curr.putcs) = matrox_cfbX_putcs;
	}
}

EXPORT_SYMBOL(matrox_init_putc);

MODULE_LICENSE("GPL");
