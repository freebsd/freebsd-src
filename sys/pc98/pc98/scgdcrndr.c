/*-
 * Copyright (c) 1999 FreeBSD(98) Porting Team.
 * All rights reserved.
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
 * $FreeBSD$
 */

#include "opt_syscons.h"
#include "opt_gdc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <dev/fb/fbreg.h>
#include <dev/syscons/syscons.h>

#ifndef SC_RENDER_DEBUG
#define SC_RENDER_DEBUG		0
#endif

static vr_clear_t		gdc_txtclear;
static vr_draw_border_t		gdc_txtborder;
static vr_draw_t		gdc_txtdraw;
static vr_set_cursor_t		gdc_txtcursor_shape;
static vr_draw_cursor_t		gdc_txtcursor;
#ifndef SC_NO_CUTPASTE
static vr_draw_mouse_t		gdc_txtmouse;
#else
#define gdc_txtmouse		(vr_draw_mouse_t *)gdc_nop
#endif

#ifndef SC_NO_MODE_CHANGE
static vr_draw_border_t		gdc_grborder;
#endif

static void			gdc_nop(scr_stat *scp, ...);

static sc_rndr_sw_t txtrndrsw = {
	gdc_txtclear,
	gdc_txtborder,
	gdc_txtdraw,	
	gdc_txtcursor_shape,
	gdc_txtcursor,
	(vr_blink_cursor_t *)gdc_nop,
	(vr_set_mouse_t *)gdc_nop,
	gdc_txtmouse,
};
RENDERER(gdc, 0, txtrndrsw, gdc_set);

#ifndef SC_NO_MODE_CHANGE
static sc_rndr_sw_t grrndrsw = {
	(vr_clear_t *)gdc_nop,
	gdc_grborder,
	(vr_draw_t *)gdc_nop,
	(vr_set_cursor_t *)gdc_nop,
	(vr_draw_cursor_t *)gdc_nop,
	(vr_blink_cursor_t *)gdc_nop,
	(vr_set_mouse_t *)gdc_nop,
	(vr_draw_mouse_t *)gdc_nop,
};
RENDERER(gdc, GRAPHICS_MODE, grrndrsw, gdc_set);
#endif /* SC_NO_MODE_CHANGE */

RENDERER_MODULE(gdc, gdc_set);

static void
gdc_nop(scr_stat *scp, ...)
{
}

/* text mode renderer */

static void
gdc_txtclear(scr_stat *scp, int c, int attr)
{
	sc_vtb_clear(&scp->scr, c, attr);
}

static void
gdc_txtborder(scr_stat *scp, int color)
{
	(*vidsw[scp->sc->adapter]->set_border)(scp->sc->adp, color);
}

static void
gdc_txtdraw(scr_stat *scp, int from, int count, int flip)
{
	vm_offset_t p;
	int c;
	int a;

	if (from + count > scp->xsize*scp->ysize)
		count = scp->xsize*scp->ysize - from;

	if (flip) {
		p = sc_vtb_pointer(&scp->scr, from);
		for (; count-- > 0; ++from) {
			c = sc_vtb_getc(&scp->vtb, from);
			a = sc_vtb_geta(&scp->vtb, from) ^ 0x0800;
			p = sc_vtb_putchar(&scp->scr, p, c, a);
		}
	} else {
		sc_vtb_copy(&scp->vtb, from, &scp->scr, from, count);
	}
}

static void
gdc_txtcursor_shape(scr_stat *scp, int base, int height, int blink)
{
	if (base < 0 || base >= scp->font_size)
		return;
	/* the caller may set height <= 0 in order to disable the cursor */
	(*vidsw[scp->sc->adapter]->set_hw_cursor_shape)(scp->sc->adp,
							base, height,
							scp->font_size, blink);
}

static void
gdc_txtcursor(scr_stat *scp, int at, int blink, int on, int flip)
{
	if (on) {
		scp->status |= VR_CURSOR_ON;
		(*vidsw[scp->sc->adapter]->set_hw_cursor)(scp->sc->adp,
						 at%scp->xsize, at/scp->xsize); 
	} else {
		if (scp->status & VR_CURSOR_ON)
			(*vidsw[scp->sc->adapter]->set_hw_cursor)(scp->sc->adp,
								  -1, -1);
		scp->status &= ~VR_CURSOR_ON;
	}
}

#ifndef SC_NO_CUTPASTE

static void
draw_txtmouse(scr_stat *scp, int x, int y)
{
	int at;

	at = (y/scp->font_size - scp->yoff)*scp->xsize + x/8 - scp->xoff;
	sc_vtb_putc(&scp->scr, at,
		    sc_vtb_getc(&scp->scr, at),
		    sc_vtb_geta(&scp->vtb, at) ^ 0x0800);
}

static void
remove_txtmouse(scr_stat *scp, int x, int y)
{
}

static void 
gdc_txtmouse(scr_stat *scp, int x, int y, int on)
{
	if (on)
		draw_txtmouse(scp, x, y);
	else
		remove_txtmouse(scp, x, y);
}

#endif /* SC_NO_CUTPASTE */

#ifndef SC_NO_MODE_CHANGE

/* graphics mode renderer */

static void
gdc_grborder(scr_stat *scp, int color)
{
	(*vidsw[scp->sc->adapter]->set_border)(scp->sc->adp, color);
}

#endif /* SC_NO_MODE_CHANGE */
