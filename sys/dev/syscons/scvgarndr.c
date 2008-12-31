/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Sascha Wildner <saw@online.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/syscons/scvgarndr.c,v 1.21.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_syscons.h"
#include "opt_vga.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <machine/bus.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/vgareg.h>
#include <dev/syscons/syscons.h>

#include <isa/isareg.h>

#ifndef SC_RENDER_DEBUG
#define SC_RENDER_DEBUG		0
#endif

static vr_clear_t		vga_txtclear;
static vr_draw_border_t		vga_txtborder;
static vr_draw_t		vga_txtdraw;
static vr_set_cursor_t		vga_txtcursor_shape;
static vr_draw_cursor_t		vga_txtcursor;
static vr_blink_cursor_t	vga_txtblink;
#ifndef SC_NO_CUTPASTE
static vr_draw_mouse_t		vga_txtmouse;
#else
#define vga_txtmouse		(vr_draw_mouse_t *)vga_nop
#endif

#ifdef SC_PIXEL_MODE
static vr_init_t		vga_rndrinit;
static vr_clear_t		vga_pxlclear_direct;
static vr_clear_t		vga_pxlclear_planar;
static vr_draw_border_t		vga_pxlborder_direct;
static vr_draw_border_t		vga_pxlborder_planar;
static vr_draw_t		vga_egadraw;
static vr_draw_t		vga_vgadraw_direct;
static vr_draw_t		vga_vgadraw_planar;
static vr_set_cursor_t		vga_pxlcursor_shape;
static vr_draw_cursor_t		vga_pxlcursor_direct;
static vr_draw_cursor_t		vga_pxlcursor_planar;
static vr_blink_cursor_t	vga_pxlblink_direct;
static vr_blink_cursor_t	vga_pxlblink_planar;
#ifndef SC_NO_CUTPASTE
static vr_draw_mouse_t		vga_pxlmouse_direct;
static vr_draw_mouse_t		vga_pxlmouse_planar;
#else
#define vga_pxlmouse_direct	(vr_draw_mouse_t *)vga_nop
#define vga_pxlmouse_planar	(vr_draw_mouse_t *)vga_nop
#endif
#endif /* SC_PIXEL_MODE */

#ifndef SC_NO_MODE_CHANGE
static vr_draw_border_t		vga_grborder;
#endif

static void			vga_nop(scr_stat *scp);

static sc_rndr_sw_t txtrndrsw = {
	(vr_init_t *)vga_nop,
	vga_txtclear,
	vga_txtborder,
	vga_txtdraw,	
	vga_txtcursor_shape,
	vga_txtcursor,
	vga_txtblink,
	(vr_set_mouse_t *)vga_nop,
	vga_txtmouse,
};
RENDERER(mda, 0, txtrndrsw, vga_set);
RENDERER(cga, 0, txtrndrsw, vga_set);
RENDERER(ega, 0, txtrndrsw, vga_set);
RENDERER(vga, 0, txtrndrsw, vga_set);

#ifdef SC_PIXEL_MODE
static sc_rndr_sw_t egarndrsw = {
	(vr_init_t *)vga_nop,
	vga_pxlclear_planar,
	vga_pxlborder_planar,
	vga_egadraw,
	vga_pxlcursor_shape,
	vga_pxlcursor_planar,
	vga_pxlblink_planar,
	(vr_set_mouse_t *)vga_nop,
	vga_pxlmouse_planar,
};
RENDERER(ega, PIXEL_MODE, egarndrsw, vga_set);

static sc_rndr_sw_t vgarndrsw = {
	vga_rndrinit,
	(vr_clear_t *)vga_nop,
	(vr_draw_border_t *)vga_nop,
	(vr_draw_t *)vga_nop,
	vga_pxlcursor_shape,
	(vr_draw_cursor_t *)vga_nop,
	(vr_blink_cursor_t *)vga_nop,
	(vr_set_mouse_t *)vga_nop,
	(vr_draw_mouse_t *)vga_nop,
};
RENDERER(vga, PIXEL_MODE, vgarndrsw, vga_set);
#endif /* SC_PIXEL_MODE */

#ifndef SC_NO_MODE_CHANGE
static sc_rndr_sw_t grrndrsw = {
	(vr_init_t *)vga_nop,
	(vr_clear_t *)vga_nop,
	vga_grborder,
	(vr_draw_t *)vga_nop,
	(vr_set_cursor_t *)vga_nop,
	(vr_draw_cursor_t *)vga_nop,
	(vr_blink_cursor_t *)vga_nop,
	(vr_set_mouse_t *)vga_nop,
	(vr_draw_mouse_t *)vga_nop,
};
RENDERER(cga, GRAPHICS_MODE, grrndrsw, vga_set);
RENDERER(ega, GRAPHICS_MODE, grrndrsw, vga_set);
RENDERER(vga, GRAPHICS_MODE, grrndrsw, vga_set);
#endif /* SC_NO_MODE_CHANGE */

RENDERER_MODULE(vga, vga_set);

#ifndef SC_NO_CUTPASTE
#if !defined(SC_ALT_MOUSE_IMAGE) || defined(SC_PIXEL_MODE)
static u_short mouse_and_mask[16] = {
	0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00, 0xff80,
	0xfe00, 0x1e00, 0x1f00, 0x0f00, 0x0f00, 0x0000, 0x0000, 0x0000
};
static u_short mouse_or_mask[16] = {
	0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7c00, 0x7e00, 0x6800,
	0x0c00, 0x0c00, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000, 0x0000
};
#endif
#endif

#ifdef SC_PIXEL_MODE
#define	VIDEO_MEMORY_POS(scp, pos, x) 					\
	scp->sc->adp->va_window +					\
	x * scp->xoff +							\
	scp->yoff * scp->font_size * scp->sc->adp->va_line_width +	\
	x * (pos % scp->xsize) +					\
	scp->font_size * scp->sc->adp->va_line_width * (pos / scp->xsize)

#define	vga_drawpxl(pos, color)						\
	switch (scp->sc->adp->va_info.vi_depth) {			\
		case 32:						\
		case 24:						\
			writel(pos, vga_palette32[color]);		\
			break;						\
		case 16:						\
			if (scp->sc->adp->va_info.vi_pixel_fsizes[1] == 5)\
				writew(pos, vga_palette15[color]);	\
			else						\
				writew(pos, vga_palette16[color]);	\
			break;						\
		case 15:						\
			writew(pos, vga_palette15[color]);		\
			break;						\
		}
	
static uint32_t vga_palette32[16] = {
	0x000000, 0x0000ad, 0x00ad00, 0x00adad,
	0xad0000, 0xad00ad, 0xad5200, 0xadadad,
	0x525252, 0x5252ff, 0x52ff52, 0x52ffff,
	0xff5252, 0xff52ff, 0xffff52, 0xffffff
};

static uint16_t vga_palette16[16] = {
	0x0000, 0x0016, 0x0560, 0x0576, 0xb000, 0xb016, 0xb2a0, 0xb576,
	0x52aa, 0x52bf, 0x57ea, 0x57ff, 0xfaaa, 0xfabf, 0xffea, 0xffff
};

static uint16_t vga_palette15[16] = {
	0x0000, 0x0016, 0x02c0, 0x02d6, 0x5800, 0x5816, 0x5940, 0x5ad6,
	0x294a, 0x295f, 0x2bea, 0x2bff, 0x7d4a, 0x7d5f, 0x7fea, 0x7fff
};

#ifndef SC_NO_CUTPASTE
static uint32_t mouse_buf32[256];
static uint16_t mouse_buf16[256];
#endif
#endif

static void
vga_nop(scr_stat *scp)
{
}

/* text mode renderer */

static void
vga_txtclear(scr_stat *scp, int c, int attr)
{
	sc_vtb_clear(&scp->scr, c, attr);
}

static void
vga_txtborder(scr_stat *scp, int color)
{
	(*vidsw[scp->sc->adapter]->set_border)(scp->sc->adp, color);
}

static void
vga_txtdraw(scr_stat *scp, int from, int count, int flip)
{
	vm_offset_t p;
	int c;
	int a;

	if (from + count > scp->xsize*scp->ysize)
		count = scp->xsize*scp->ysize - from;

	if (flip) {
		for (p = sc_vtb_pointer(&scp->scr, from); count-- > 0; ++from) {
			c = sc_vtb_getc(&scp->vtb, from);
			a = sc_vtb_geta(&scp->vtb, from);
			a = (a & 0x8800) | ((a & 0x7000) >> 4) 
				| ((a & 0x0700) << 4);
			p = sc_vtb_putchar(&scp->scr, p, c, a);
		}
	} else {
		sc_vtb_copy(&scp->vtb, from, &scp->scr, from, count);
	}
}

static void 
vga_txtcursor_shape(scr_stat *scp, int base, int height, int blink)
{
	if (base < 0 || base >= scp->font_size)
		return;
	/* the caller may set height <= 0 in order to disable the cursor */
#if 0
	scp->curs_attr.base = base;
	scp->curs_attr.height = height;
#endif
	(*vidsw[scp->sc->adapter]->set_hw_cursor_shape)(scp->sc->adp,
							base, height,
							scp->font_size, blink);
}

static void
draw_txtcharcursor(scr_stat *scp, int at, u_short c, u_short a, int flip)
{
	sc_softc_t *sc;

	sc = scp->sc;
	scp->cursor_saveunder_char = c;
	scp->cursor_saveunder_attr = a;

#ifndef SC_NO_FONT_LOADING
	if (scp->curs_attr.flags & CONS_CHAR_CURSOR) {
		unsigned char *font;
		int h;
		int i;

		if (scp->font_size < 14) {
			font = sc->font_8;
			h = 8;
		} else if (scp->font_size >= 16) {
			font = sc->font_16;
			h = 16;
		} else {
			font = sc->font_14;
			h = 14;
		}
		if (scp->curs_attr.base >= h)
			return;
		if (flip)
			a = (a & 0x8800)
				| ((a & 0x7000) >> 4) | ((a & 0x0700) << 4);
		bcopy(font + c*h, font + sc->cursor_char*h, h);
		font = font + sc->cursor_char*h;
		for (i = imax(h - scp->curs_attr.base - scp->curs_attr.height, 0);
			i < h - scp->curs_attr.base; ++i) {
			font[i] ^= 0xff;
		}
		/* XXX */
		(*vidsw[sc->adapter]->load_font)(sc->adp, 0, h, 8, font,
						 sc->cursor_char, 1);
		sc_vtb_putc(&scp->scr, at, sc->cursor_char, a);
	} else
#endif /* SC_NO_FONT_LOADING */
	{
		if ((a & 0x7000) == 0x7000) {
			a &= 0x8f00;
			if ((a & 0x0700) == 0)
				a |= 0x0700;
		} else {
			a |= 0x7000;
			if ((a & 0x0700) == 0x0700)
				a &= 0xf000;
		}
		if (flip)
			a = (a & 0x8800)
				| ((a & 0x7000) >> 4) | ((a & 0x0700) << 4);
		sc_vtb_putc(&scp->scr, at, c, a);
	}
}

static void
vga_txtcursor(scr_stat *scp, int at, int blink, int on, int flip)
{
	video_adapter_t *adp;
	int cursor_attr;

	if (scp->curs_attr.height <= 0)	/* the text cursor is disabled */
		return;

	adp = scp->sc->adp;
	if (blink) {
		scp->status |= VR_CURSOR_BLINK;
		if (on) {
			scp->status |= VR_CURSOR_ON;
			(*vidsw[adp->va_index]->set_hw_cursor)(adp,
							       at%scp->xsize,
							       at/scp->xsize); 
		} else {
			if (scp->status & VR_CURSOR_ON)
				(*vidsw[adp->va_index]->set_hw_cursor)(adp,
								       -1, -1);
			scp->status &= ~VR_CURSOR_ON;
		}
	} else {
		scp->status &= ~VR_CURSOR_BLINK;
		if (on) {
			scp->status |= VR_CURSOR_ON;
			draw_txtcharcursor(scp, at,
					   sc_vtb_getc(&scp->scr, at),
					   sc_vtb_geta(&scp->scr, at),
					   flip);
		} else {
			cursor_attr = scp->cursor_saveunder_attr;
			if (flip)
				cursor_attr = (cursor_attr & 0x8800)
					| ((cursor_attr & 0x7000) >> 4)
					| ((cursor_attr & 0x0700) << 4);
			if (scp->status & VR_CURSOR_ON)
				sc_vtb_putc(&scp->scr, at,
					    scp->cursor_saveunder_char,
					    cursor_attr);
			scp->status &= ~VR_CURSOR_ON;
		}
	}
}

static void
vga_txtblink(scr_stat *scp, int at, int flip)
{
}

#ifndef SC_NO_CUTPASTE

static void
draw_txtmouse(scr_stat *scp, int x, int y)
{
#ifndef SC_ALT_MOUSE_IMAGE
    if (ISMOUSEAVAIL(scp->sc->adp->va_flags)) {
	u_char font_buf[128];
	u_short cursor[32];
	u_char c;
	int pos;
	int xoffset, yoffset;
	int crtc_addr;
	int i;

	/* prepare mousepointer char's bitmaps */
	pos = (y/scp->font_size - scp->yoff)*scp->xsize + x/8 - scp->xoff;
	bcopy(scp->font + sc_vtb_getc(&scp->scr, pos)*scp->font_size,
	      &font_buf[0], scp->font_size);
	bcopy(scp->font + sc_vtb_getc(&scp->scr, pos + 1)*scp->font_size,
	      &font_buf[32], scp->font_size);
	bcopy(scp->font 
		 + sc_vtb_getc(&scp->scr, pos + scp->xsize)*scp->font_size,
	      &font_buf[64], scp->font_size);
	bcopy(scp->font
		 + sc_vtb_getc(&scp->scr, pos + scp->xsize + 1)*scp->font_size,
	      &font_buf[96], scp->font_size);
	for (i = 0; i < scp->font_size; ++i) {
		cursor[i] = font_buf[i]<<8 | font_buf[i+32];
		cursor[i + scp->font_size] = font_buf[i+64]<<8 | font_buf[i+96];
	}

	/* now and-or in the mousepointer image */
	xoffset = x%8;
	yoffset = y%scp->font_size;
	for (i = 0; i < 16; ++i) {
		cursor[i + yoffset] =
	    		(cursor[i + yoffset] & ~(mouse_and_mask[i] >> xoffset))
	    		| (mouse_or_mask[i] >> xoffset);
	}
	for (i = 0; i < scp->font_size; ++i) {
		font_buf[i] = (cursor[i] & 0xff00) >> 8;
		font_buf[i + 32] = cursor[i] & 0xff;
		font_buf[i + 64] = (cursor[i + scp->font_size] & 0xff00) >> 8;
		font_buf[i + 96] = cursor[i + scp->font_size] & 0xff;
	}

#if 1
	/* wait for vertical retrace to avoid jitter on some videocards */
	crtc_addr = scp->sc->adp->va_crtc_addr;
	while (!(inb(crtc_addr + 6) & 0x08)) /* idle */ ;
#endif
	c = scp->sc->mouse_char;
	(*vidsw[scp->sc->adapter]->load_font)(scp->sc->adp, 0, 32, 8, font_buf,
					      c, 4); 

	sc_vtb_putc(&scp->scr, pos, c, sc_vtb_geta(&scp->scr, pos));
	/* FIXME: may be out of range! */
	sc_vtb_putc(&scp->scr, pos + scp->xsize, c + 2,
		    sc_vtb_geta(&scp->scr, pos + scp->xsize));
	if (x < (scp->xsize - 1)*8) {
		sc_vtb_putc(&scp->scr, pos + 1, c + 1,
			    sc_vtb_geta(&scp->scr, pos + 1));
		sc_vtb_putc(&scp->scr, pos + scp->xsize + 1, c + 3,
			    sc_vtb_geta(&scp->scr, pos + scp->xsize + 1));
	}
    } else
#endif /* SC_ALT_MOUSE_IMAGE */
    {
	/* Red, magenta and brown are mapped to green to to keep it readable */
	static const int col_conv[16] = {
		6, 6, 6, 6, 2, 2, 2, 6, 14, 14, 14, 14, 10, 10, 10, 14
	};
	int pos;
	int color;
	int a;

	pos = (y/scp->font_size - scp->yoff)*scp->xsize + x/8 - scp->xoff;
	a = sc_vtb_geta(&scp->scr, pos);
	if (scp->sc->adp->va_flags & V_ADP_COLOR)
		color = (col_conv[(a & 0xf000) >> 12] << 12)
			| ((a & 0x0f00) | 0x0800);
	else
		color = ((a & 0xf000) >> 4) | ((a & 0x0f00) << 4);
	sc_vtb_putc(&scp->scr, pos, sc_vtb_getc(&scp->scr, pos), color);
    }
}

static void
remove_txtmouse(scr_stat *scp, int x, int y)
{
}

static void 
vga_txtmouse(scr_stat *scp, int x, int y, int on)
{
	if (on)
		draw_txtmouse(scp, x, y);
	else
		remove_txtmouse(scp, x, y);
}

#endif /* SC_NO_CUTPASTE */

#ifdef SC_PIXEL_MODE

/* pixel (raster text) mode renderer */

static void
vga_rndrinit(scr_stat *scp)
{
	if (scp->sc->adp->va_info.vi_mem_model == V_INFO_MM_PLANAR) {
		scp->rndr->clear = vga_pxlclear_planar;
		scp->rndr->draw_border = vga_pxlborder_planar;
		scp->rndr->draw = vga_vgadraw_planar;
		scp->rndr->draw_cursor = vga_pxlcursor_planar;
		scp->rndr->blink_cursor = vga_pxlblink_planar;
		scp->rndr->draw_mouse = vga_pxlmouse_planar;
	} else if (scp->sc->adp->va_info.vi_mem_model == V_INFO_MM_DIRECT) {
		scp->rndr->clear = vga_pxlclear_direct;
		scp->rndr->draw_border = vga_pxlborder_direct;
		scp->rndr->draw = vga_vgadraw_direct;
		scp->rndr->draw_cursor = vga_pxlcursor_direct;
		scp->rndr->blink_cursor = vga_pxlblink_direct;
		scp->rndr->draw_mouse = vga_pxlmouse_direct;
	}
}

static void
vga_pxlclear_direct(scr_stat *scp, int c, int attr)
{
	vm_offset_t p;
	int line_width;
	int pixel_size;
	int lines;
	int i;

	line_width = scp->sc->adp->va_line_width;
	pixel_size = scp->sc->adp->va_info.vi_pixel_size;
	lines = scp->ysize * scp->font_size; 
	p = scp->sc->adp->va_window +
	    line_width * scp->yoff * scp->font_size +
	    scp->xoff * 8 * pixel_size;

	for (i = 0; i < lines; ++i) {
		bzero_io((void *)p, scp->xsize * 8 * pixel_size);
		p += line_width;
	}
}

static void
vga_pxlclear_planar(scr_stat *scp, int c, int attr)
{
	vm_offset_t p;
	int line_width;
	int lines;
	int i;

	/* XXX: we are just filling the screen with the background color... */
	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
	outw(GDCIDX, ((attr & 0xf000) >> 4) | 0x00); /* set/reset */
	line_width = scp->sc->adp->va_line_width;
	lines = scp->ysize*scp->font_size; 
	p = scp->sc->adp->va_window + line_width*scp->yoff*scp->font_size
		+ scp->xoff;
	for (i = 0; i < lines; ++i) {
		bzero_io((void *)p, scp->xsize);
		p += line_width;
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
}

static void
vga_pxlborder_direct(scr_stat *scp, int color)
{
	vm_offset_t s;
	vm_offset_t e;
	vm_offset_t f;
	int line_width;
	int pixel_size;
	int x;
	int y;
	int i;

	line_width = scp->sc->adp->va_line_width;
	pixel_size = scp->sc->adp->va_info.vi_pixel_size;

	if (scp->yoff > 0) {
		s = scp->sc->adp->va_window;
		e = s + line_width * scp->yoff * scp->font_size;

		for (f = s; f < e; f += pixel_size)
			vga_drawpxl(f, color);
	}

	y = (scp->yoff + scp->ysize) * scp->font_size;

	if (scp->ypixel > y) {
		s = scp->sc->adp->va_window + line_width * y;
		e = s + line_width * (scp->ypixel - y);

		for (f = s; f < e; f += pixel_size)
			vga_drawpxl(f, color);
	}

	y = scp->yoff * scp->font_size;
	x = scp->xpixel / 8 - scp->xoff - scp->xsize;

	for (i = 0; i < scp->ysize * scp->font_size; ++i) {
		if (scp->xoff > 0) {
			s = scp->sc->adp->va_window + line_width * (y + i);
			e = s + scp->xoff * 8 * pixel_size;

			for (f = s; f < e; f += pixel_size)
				vga_drawpxl(f, color);
		}

		if (x > 0) {
			s = scp->sc->adp->va_window + line_width * (y + i) +
			    scp->xoff * 8 * pixel_size +
			    scp->xsize * 8 * pixel_size;
			e = s + x * 8 * pixel_size;

			for (f = s; f < e; f += pixel_size)
				vga_drawpxl(f, color);
		}
	}
}

static void
vga_pxlborder_planar(scr_stat *scp, int color)
{
	vm_offset_t p;
	int line_width;
	int x;
	int y;
	int i;

	(*vidsw[scp->sc->adapter]->set_border)(scp->sc->adp, color);

	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
	outw(GDCIDX, (color << 8) | 0x00);	/* set/reset */
	line_width = scp->sc->adp->va_line_width;
	p = scp->sc->adp->va_window;
	if (scp->yoff > 0)
		bzero_io((void *)p, line_width*scp->yoff*scp->font_size);
	y = (scp->yoff + scp->ysize)*scp->font_size;
	if (scp->ypixel > y)
		bzero_io((void *)(p + line_width*y), line_width*(scp->ypixel - y));
	y = scp->yoff*scp->font_size;
	x = scp->xpixel/8 - scp->xoff - scp->xsize;
	for (i = 0; i < scp->ysize*scp->font_size; ++i) {
		if (scp->xoff > 0)
			bzero_io((void *)(p + line_width*(y + i)), scp->xoff);
		if (x > 0)
			bzero_io((void *)(p + line_width*(y + i)
				     + scp->xoff + scp->xsize), x);
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
}

static void 
vga_egadraw(scr_stat *scp, int from, int count, int flip)
{
	vm_offset_t d;
	vm_offset_t e;
	u_char *f;
	u_short bg;
	u_short col1, col2;
	int line_width;
	int i, j;
	int a;
	u_char c;

	line_width = scp->sc->adp->va_line_width;

	d = VIDEO_MEMORY_POS(scp, from, 1);

	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	bg = -1;
	if (from + count > scp->xsize*scp->ysize)
		count = scp->xsize*scp->ysize - from;
	for (i = from; count-- > 0; ++i) {
		a = sc_vtb_geta(&scp->vtb, i);
		if (flip) {
			col1 = ((a & 0x7000) >> 4) | (a & 0x0800);
			col2 = ((a & 0x8000) >> 4) | (a & 0x0700);
		} else {
			col1 = (a & 0x0f00);
			col2 = (a & 0xf000) >> 4;
		}
		/* set background color in EGA/VGA latch */
		if (bg != col2) {
			bg = col2;
			outw(GDCIDX, bg | 0x00);	/* set/reset */
			outw(GDCIDX, 0xff08);		/* bit mask */
			writeb(d, 0);
			c = readb(d);	/* set bg color in the latch */
		}
		/* foreground color */
		outw(GDCIDX, col1 | 0x00);		/* set/reset */
		e = d;
		f = &(scp->font[sc_vtb_getc(&scp->vtb, i)*scp->font_size]);
		for (j = 0; j < scp->font_size; ++j, ++f) {
			outw(GDCIDX, (*f << 8) | 0x08);	/* bit mask */
	        	writeb(e, 0);
			e += line_width;
		}
		++d;
		if ((i % scp->xsize) == scp->xsize - 1)
			d += scp->xoff*2 
				 + (scp->font_size - 1)*line_width;
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
}

static void
vga_vgadraw_direct(scr_stat *scp, int from, int count, int flip)
{
	vm_offset_t d = 0;
	vm_offset_t e;
	u_char *f;
	u_short col1, col2, color;
	int line_width, pixel_size;
	int i, j, k;
	int a;

	line_width = scp->sc->adp->va_line_width;
	pixel_size = scp->sc->adp->va_info.vi_pixel_size;

	d = VIDEO_MEMORY_POS(scp, from, 8 * pixel_size);

	if (from + count > scp->xsize * scp->ysize)
		count = scp->xsize * scp->ysize - from;

	for (i = from; count-- > 0; ++i) {
		a = sc_vtb_geta(&scp->vtb, i);

		if (flip) {
			col1 = (((a & 0x7000) >> 4) | (a & 0x0800)) >> 8;
			col2 = (((a & 0x8000) >> 4) | (a & 0x0700)) >> 8;
		} else {
			col1 = (a & 0x0f00) >> 8;
			col2 = (a & 0xf000) >> 12;
		}

		e = d;
		f = &(scp->font[sc_vtb_getc(&scp->vtb, i) * scp->font_size]);

		for (j = 0; j < scp->font_size; ++j, ++f) {
			for (k = 0; k < 8; ++k) {
				color = *f & (1 << (7 - k)) ? col1 : col2;
				vga_drawpxl(e + pixel_size * k, color);
			}

			e += line_width;
		}

		d += 8 * pixel_size;

		if ((i % scp->xsize) == scp->xsize - 1)
			d += scp->xoff * 16 * pixel_size +
			     (scp->font_size - 1) * line_width;
	}
}

static void
vga_vgadraw_planar(scr_stat *scp, int from, int count, int flip)
{
	vm_offset_t d;
	vm_offset_t e;
	u_char *f;
	u_short bg;
	u_short col1, col2;
	int line_width;
	int i, j;
	int a;
	u_char c;

	d = VIDEO_MEMORY_POS(scp, from, 1);

	line_width = scp->sc->adp->va_line_width;

	outw(GDCIDX, 0x0305);		/* read mode 0, write mode 3 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
	bg = -1;
	if (from + count > scp->xsize*scp->ysize)
		count = scp->xsize*scp->ysize - from;
	for (i = from; count-- > 0; ++i) {
		a = sc_vtb_geta(&scp->vtb, i);
		if (flip) {
			col1 = ((a & 0x7000) >> 4) | (a & 0x0800);
			col2 = ((a & 0x8000) >> 4) | (a & 0x0700);
		} else {
			col1 = (a & 0x0f00);
			col2 = (a & 0xf000) >> 4;
		}
		/* set background color in EGA/VGA latch */
		if (bg != col2) {
			bg = col2;
			outw(GDCIDX, 0x0005);	/* read mode 0, write mode 0 */
			outw(GDCIDX, bg | 0x00); /* set/reset */
			writeb(d, 0);
			c = readb(d);		/* set bg color in the latch */
			outw(GDCIDX, 0x0305);	/* read mode 0, write mode 3 */
		}
		/* foreground color */
		outw(GDCIDX, col1 | 0x00);	/* set/reset */
		e = d;
		f = &(scp->font[sc_vtb_getc(&scp->vtb, i)*scp->font_size]);
		for (j = 0; j < scp->font_size; ++j, ++f) {
	        	writeb(e, *f);
			e += line_width;
		}
		++d;
		if ((i % scp->xsize) == scp->xsize - 1)
			d += scp->xoff*2 
				 + (scp->font_size - 1)*line_width;
	}
	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
}

static void 
vga_pxlcursor_shape(scr_stat *scp, int base, int height, int blink)
{
	if (base < 0 || base >= scp->font_size)
		return;
	/* the caller may set height <= 0 in order to disable the cursor */
#if 0
	scp->curs_attr.base = base;
	scp->curs_attr.height = height;
#endif
}

static void 
draw_pxlcursor_direct(scr_stat *scp, int at, int on, int flip)
{
	vm_offset_t d = 0;
	u_char *f;
	int line_width, pixel_size;
	int height;
	int col1, col2, color;
	int a;
	int i, j;

	line_width = scp->sc->adp->va_line_width;
	pixel_size = scp->sc->adp->va_info.vi_pixel_size;

	d = VIDEO_MEMORY_POS(scp, at, 8 * pixel_size) +
	    (scp->font_size - scp->curs_attr.base - 1) * line_width;

	a = sc_vtb_geta(&scp->vtb, at);

	if (flip) {
		col1 = ((on) ? (a & 0x0f00) : ((a & 0xf000) >> 4)) >> 8;
		col2 = ((on) ? ((a & 0xf000) >> 4) : (a & 0x0f00)) >> 8;
	} else {
		col1 = ((on) ? ((a & 0xf000) >> 4) : (a & 0x0f00)) >> 8;
		col2 = ((on) ? (a & 0x0f00) : ((a & 0xf000) >> 4)) >> 8;
	}

	f = &(scp->font[sc_vtb_getc(&scp->vtb, at) * scp->font_size +
	      scp->font_size - scp->curs_attr.base - 1]);

	height = imin(scp->curs_attr.height, scp->font_size);

	for (i = 0; i < height; ++i, --f) {
		for (j = 0; j < 8; ++j) {
			color = *f & (1 << (7 - j)) ? col1 : col2;
			vga_drawpxl(d + pixel_size * j, color);
		}

		d -= line_width;
	}
}

static void 
draw_pxlcursor_planar(scr_stat *scp, int at, int on, int flip)
{
	vm_offset_t d;
	u_char *f;
	int line_width;
	int height;
	int col;
	int a;
	int i;
	u_char c;

	line_width = scp->sc->adp->va_line_width;

	d = VIDEO_MEMORY_POS(scp, at, 1) +
	    (scp->font_size - scp->curs_attr.base - 1) * line_width;

	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	/* set background color in EGA/VGA latch */
	a = sc_vtb_geta(&scp->vtb, at);
	if (flip)
		col = (on) ? ((a & 0xf000) >> 4) : (a & 0x0f00);
	else
		col = (on) ? (a & 0x0f00) : ((a & 0xf000) >> 4);
	outw(GDCIDX, col | 0x00);	/* set/reset */
	outw(GDCIDX, 0xff08);		/* bit mask */
	writeb(d, 0);
	c = readb(d);			/* set bg color in the latch */
	/* foreground color */
	if (flip)
		col = (on) ? (a & 0x0f00) : ((a & 0xf000) >> 4);
	else
		col = (on) ? ((a & 0xf000) >> 4) : (a & 0x0f00);
	outw(GDCIDX, col | 0x00);	/* set/reset */
	f = &(scp->font[sc_vtb_getc(&scp->vtb, at)*scp->font_size
		+ scp->font_size - scp->curs_attr.base - 1]);
	height = imin(scp->curs_attr.height, scp->font_size);
	for (i = 0; i < height; ++i, --f) {
		outw(GDCIDX, (*f << 8) | 0x08);	/* bit mask */
	       	writeb(d, 0);
		d -= line_width;
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
}

static int pxlblinkrate = 0;

static void 
vga_pxlcursor_direct(scr_stat *scp, int at, int blink, int on, int flip)
{
	if (scp->curs_attr.height <= 0)	/* the text cursor is disabled */
		return;

	if (on) {
		if (!blink) {
			scp->status |= VR_CURSOR_ON;
			draw_pxlcursor_direct(scp, at, on, flip);
		} else if (++pxlblinkrate & 4) {
			pxlblinkrate = 0;
			scp->status ^= VR_CURSOR_ON;
			draw_pxlcursor_direct(scp, at,
					      scp->status & VR_CURSOR_ON,
					      flip);
		}
	} else {
		if (scp->status & VR_CURSOR_ON)
			draw_pxlcursor_direct(scp, at, on, flip);
		scp->status &= ~VR_CURSOR_ON;
	}
	if (blink)
		scp->status |= VR_CURSOR_BLINK;
	else
		scp->status &= ~VR_CURSOR_BLINK;
}

static void 
vga_pxlcursor_planar(scr_stat *scp, int at, int blink, int on, int flip)
{
	if (scp->curs_attr.height <= 0)	/* the text cursor is disabled */
		return;

	if (on) {
		if (!blink) {
			scp->status |= VR_CURSOR_ON;
			draw_pxlcursor_planar(scp, at, on, flip);
		} else if (++pxlblinkrate & 4) {
			pxlblinkrate = 0;
			scp->status ^= VR_CURSOR_ON;
			draw_pxlcursor_planar(scp, at,
					      scp->status & VR_CURSOR_ON,
					      flip);
		}
	} else {
		if (scp->status & VR_CURSOR_ON)
			draw_pxlcursor_planar(scp, at, on, flip);
		scp->status &= ~VR_CURSOR_ON;
	}
	if (blink)
		scp->status |= VR_CURSOR_BLINK;
	else
		scp->status &= ~VR_CURSOR_BLINK;
}

static void
vga_pxlblink_direct(scr_stat *scp, int at, int flip)
{
	if (!(scp->status & VR_CURSOR_BLINK))
		return;
	if (!(++pxlblinkrate & 4))
		return;
	pxlblinkrate = 0;
	scp->status ^= VR_CURSOR_ON;
	draw_pxlcursor_direct(scp, at, scp->status & VR_CURSOR_ON, flip);
}

static void
vga_pxlblink_planar(scr_stat *scp, int at, int flip)
{
	if (!(scp->status & VR_CURSOR_BLINK))
		return;
	if (!(++pxlblinkrate & 4))
		return;
	pxlblinkrate = 0;
	scp->status ^= VR_CURSOR_ON;
	draw_pxlcursor_planar(scp, at, scp->status & VR_CURSOR_ON, flip);
}

#ifndef SC_NO_CUTPASTE

static void
draw_pxlmouse_planar(scr_stat *scp, int x, int y)
{
	vm_offset_t p;
	int line_width;
	int xoff, yoff;
	int ymax;
	u_short m;
	int i, j;

	line_width = scp->sc->adp->va_line_width;
	xoff = (x - scp->xoff*8)%8;
	yoff = y - (y/line_width)*line_width;
	ymax = imin(y + 16, scp->ypixel);

	outw(GDCIDX, 0x0805);		/* read mode 1, write mode 0 */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
	outw(GDCIDX, 0x0002);		/* color compare */
	outw(GDCIDX, 0x0007);		/* color don't care */
	outw(GDCIDX, 0xff08);		/* bit mask */
	outw(GDCIDX, 0x0803);		/* data rotate/function select (and) */
	p = scp->sc->adp->va_window + line_width*y + x/8;
	if (x < scp->xpixel - 8) {
		for (i = y, j = 0; i < ymax; ++i, ++j) {
			m = ~(mouse_and_mask[j] >> xoff);
#if defined(__i386__) || defined(__amd64__)
			*(u_char *)p &= m >> 8;
			*(u_char *)(p + 1) &= m;
#else
			writeb(p, readb(p) & (m >> 8));
			writeb(p + 1, readb(p + 1) & (m >> 8));
#endif
			p += line_width;
		}
	} else {
		xoff += 8;
		for (i = y, j = 0; i < ymax; ++i, ++j) {
			m = ~(mouse_and_mask[j] >> xoff);
#if defined(__i386__) || defined(__amd64__)
			*(u_char *)p &= m;
#else
			writeb(p, readb(p) & (m >> 8));
#endif
			p += line_width;
		}
	}
	outw(GDCIDX, 0x1003);		/* data rotate/function select (or) */
	p = scp->sc->adp->va_window + line_width*y + x/8;
	if (x < scp->xpixel - 8) {
		for (i = y, j = 0; i < ymax; ++i, ++j) {
			m = mouse_or_mask[j] >> xoff;
#if defined(__i386__) || defined(__amd64__)
			*(u_char *)p &= m >> 8;
			*(u_char *)(p + 1) &= m;
#else
			writeb(p, readb(p) & (m >> 8));
			writeb(p + 1, readb(p + 1) & (m >> 8));
#endif
			p += line_width;
		}
	} else {
		for (i = y, j = 0; i < ymax; ++i, ++j) {
			m = mouse_or_mask[j] >> xoff;
#if defined(__i386__) || defined(__amd64__)
			*(u_char *)p &= m;
#else
			writeb(p, readb(p) & (m >> 8));
#endif
			p += line_width;
		}
	}
	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
}

static void
remove_pxlmouse_planar(scr_stat *scp, int x, int y)
{
	vm_offset_t p;
	int col, row;
	int pos;
	int line_width;
	int ymax;
	int i;

	/* erase the mouse cursor image */
	col = x/8 - scp->xoff;
	row = y/scp->font_size - scp->yoff;
	pos = row*scp->xsize + col;
	i = (col < scp->xsize - 1) ? 2 : 1;
	(*scp->rndr->draw)(scp, pos, i, FALSE);
	if (row < scp->ysize - 1)
		(*scp->rndr->draw)(scp, pos + scp->xsize, i, FALSE);

	/* paint border if necessary */
	line_width = scp->sc->adp->va_line_width;
	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
	outw(GDCIDX, (scp->border << 8) | 0x00);	/* set/reset */
	if (row == scp->ysize - 1) {
		i = (scp->ysize + scp->yoff)*scp->font_size;
		ymax = imin(i + scp->font_size, scp->ypixel);
		p = scp->sc->adp->va_window + i*line_width + scp->xoff + col;
		if (col < scp->xsize - 1) {
			for (; i < ymax; ++i) {
				writeb(p, 0);
				writeb(p + 1, 0);
				p += line_width;
			}
		} else {
			for (; i < ymax; ++i) {
				writeb(p, 0);
				p += line_width;
			}
		}
	}
	if ((col == scp->xsize - 1) && (scp->xoff > 0)) {
		i = (row + scp->yoff)*scp->font_size;
		ymax = imin(i + scp->font_size*2, scp->ypixel);
		p = scp->sc->adp->va_window + i*line_width
			+ scp->xoff + scp->xsize;
		for (; i < ymax; ++i) {
			writeb(p, 0);
			p += line_width;
		}
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
}

static void 
vga_pxlmouse_direct(scr_stat *scp, int x, int y, int on)
{
	vm_offset_t p;
	int line_width, pixel_size;
	int xend, yend;
	static int x_old = 0, xend_old = 0;
	static int y_old = 0, yend_old = 0;
	int i, j;
	uint32_t *u32;
	uint16_t *u16;
	int bpp;

	if (!on)
		return;

	bpp = scp->sc->adp->va_info.vi_depth;

	if ((bpp == 16) && (scp->sc->adp->va_info.vi_pixel_fsizes[1] == 5))
		bpp = 15;

	line_width = scp->sc->adp->va_line_width;
	pixel_size = scp->sc->adp->va_info.vi_pixel_size;

	xend = imin(x + 16, scp->xpixel);
	yend = imin(y + 16, scp->ypixel);

	p = scp->sc->adp->va_window + y_old * line_width + x_old * pixel_size;

	for (i = 0; i < (yend_old - y_old); i++) {
		for (j = (xend_old - x_old - 1); j >= 0; j--) {
			switch (bpp) {
			case 32:
				u32 = (uint32_t*)(p + j * pixel_size);
				writel(u32, mouse_buf32[i * 16 + j]);
				break;
			case 16:
				/* FALLTHROUGH */
			case 15:
				u16 = (uint16_t*)(p + j * pixel_size);
				writew(u16, mouse_buf16[i * 16 + j]);
				break;
			}
		}

		p += line_width;
	}

	p = scp->sc->adp->va_window + y * line_width + x * pixel_size;

	for (i = 0; i < (yend - y); i++) {
		for (j = (xend - x - 1); j >= 0; j--) {
			switch (bpp) {
			case 32:
				u32 = (uint32_t*)(p + j * pixel_size);
				mouse_buf32[i * 16 + j] = *u32;
				if (mouse_or_mask[i] & (1 << (15 - j)))
					writel(u32, vga_palette32[15]);
				else if (mouse_and_mask[i] & (1 << (15 - j)))
					writel(u32, 0);
				break;
			case 16:
				u16 = (uint16_t*)(p + j * pixel_size);
				mouse_buf16[i * 16 + j] = *u16;
				if (mouse_or_mask[i] & (1 << (15 - j)))
					writew(u16, vga_palette16[15]);
				else if (mouse_and_mask[i] & (1 << (15 - j)))
					writew(u16, 0);
				break;
			case 15:
				u16 = (uint16_t*)(p  + j * pixel_size);
				mouse_buf16[i * 16 + j] = *u16;
				if (mouse_or_mask[i] & (1 << (15 - j)))
					writew(u16, vga_palette15[15]);
				else if (mouse_and_mask[i] & (1 << (15 - j)))
					writew(u16, 0);
				break;
			}
		}

		p += line_width;
	}

	x_old = x;
	y_old = y;
	xend_old = xend;
	yend_old = yend;
}

static void 
vga_pxlmouse_planar(scr_stat *scp, int x, int y, int on)
{
	if (on)
		draw_pxlmouse_planar(scp, x, y);
	else
		remove_pxlmouse_planar(scp, x, y);
}

#endif /* SC_NO_CUTPASTE */
#endif /* SC_PIXEL_MODE */

#ifndef SC_NO_MODE_CHANGE

/* graphics mode renderer */

static void
vga_grborder(scr_stat *scp, int color)
{
	(*vidsw[scp->sc->adapter]->set_border)(scp->sc->adp, color);
}

#endif
