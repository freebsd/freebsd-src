/*-
 * Copyright (c) 2001 Andrew Miklic
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/stdarg.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/fbio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/pc/bios.h>
#include <machine/clock.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/pc/vesa.h>
#include <machine/resource.h>
#include <machine/rpb.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/gfb.h>
#include <dev/gfb/gfb_pci.h>

#include "opt_gfb.h"

struct gfb_softc *gfb_device_softcs[2][MAX_NUM_GFB_CARDS] = {
	{
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};

/*
   The following 9 variables exist only because we need statically
   allocated structures very early in boot to support gfb_configure()...
*/
struct gfb_softc console;
video_adapter_t console_adp;
struct gfb_conf console_gfbc;
u_char console_palette_red[256];
u_char console_palette_green[256];
u_char console_palette_blue[256];
u_char console_cursor_palette_red[3];
u_char console_cursor_palette_green[3];
u_char console_cursor_palette_blue[3];

extern struct gfb_font bold8x16;

/*****************************************************************************
 *
 * FB-generic functions
 *
 ****************************************************************************/

int
gfb_probe(int unit, video_adapter_t **adpp, void *arg, int flags)
{
	int error;

	/* Assume the best... */
	error = 0;

	if((*adpp = vid_get_adapter(vid_find_adapter((char *)arg, unit))) == NULL)
		error = ENODEV;
	else
		(*adpp)->va_flags |= V_ADP_PROBED;

	return(error);
}

int
gfb_init(int unit, video_adapter_t *adp, int flags)
{
	struct gfb_softc *sc;
	struct gfb_conf *gfbc;
	int error;

	/* Assume the best... */
	error = 0;

	if(!init_done(adp)) {
		sc = gfb_device_softcs[adp->va_model][unit];
		gfbc = sc->gfbc;

		/* Initialize the RAMDAC... */
		(*gfbc->ramdac_init)(sc);

		/* Initialize the palettes... */
		(*gfbc->ramdac_load_palette)(sc->adp, &sc->gfbc->palette);
		(*gfbc->ramdac_load_cursor_palette)(sc->adp,
		    &sc->gfbc->cursor_palette);

		/* Prepare the default font... */
		(*vidsw[adp->va_index]->load_font)(adp, 0, bold8x16.height,
		    bold8x16.data, 0, 256);
		adp->va_info.vi_cwidth = gfbc->fonts[0].width;
		adp->va_info.vi_cheight = gfbc->fonts[0].height;

		/*
		   Normalize vi_width and vi_height to be in terms of
		   on-screen characters, rather than pixels (*_init()
		   leaves them in terms of pixels...
		*/
		adp->va_info.vi_width /= adp->va_info.vi_cwidth;
		adp->va_info.vi_height /= adp->va_info.vi_cheight;

		/* Enable the default font... */
		(*vidsw[adp->va_index]->show_font)(adp, 0);

		/* Enable future font-loading... */
		adp->va_flags |= V_ADP_FONT;

		/* Flag this initialization for this adapter... */	
		adp->va_flags |= V_ADP_INITIALIZED;
	}
	return(error);
}

int
gfb_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	int error;

	/* Assume the best... */
	error = 0;

	/*
	   The info for GFB adapters does not depend on its mode,
	   so just copy it indiscriminantly (actually, we originally
	   checked the mode, but the current fb framework is somewhat
	   sloppily done in places, and assumes VGA in several places,
	   which makes such checks always fail for such GFBs as TGA)...
	*/
	bcopy(&adp->va_info, info, sizeof(video_info_t));
	return(error);
}

int
gfb_set_mode(video_adapter_t *adp, int mode)
{

	adp->va_mode = mode;
	return(0);
}

int
gfb_save_font(video_adapter_t *adp, int page, int fontsize, u_char *data,
    int ch, int count)
{
	struct gfb_softc *sc;
	int error;
	int i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* Check the font information... */
	if((sc->gfbc->fonts[page].height != fontsize) ||
	   (sc->gfbc->fonts[page].width != 8))
		error = EINVAL;
	else

		/*
		   Copy the character pixel array from our
		   very own private cache...
		*/
		for(i = ch; i < count * fontsize; i++)
			data[i] = adp->va_little_bitian ?
			    BIT_REVERSE(sc->gfbc->fonts[page].data[i]) :
			    sc->gfbc->fonts[page].data[i];

	return(error);
}

int
gfb_load_font(video_adapter_t *adp, int page, int fontsize, u_char *data,
    int ch, int count)
{
	struct gfb_softc *sc;
	int error;
	int i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* Copy the character pixel array into our very own private cache... */
	for(i = ch; i < count * fontsize; i++)
		sc->gfbc->fonts[page].data[i] = adp->va_little_bitian ?
		    BIT_REVERSE(data[i]) : data[i];

	/* Save the font information... */
	sc->gfbc->fonts[page].height = fontsize;
	sc->gfbc->fonts[page].width = 8;

	return(error);
}

int
gfb_show_font(video_adapter_t *adp, int page)
{
	struct gfb_softc *sc;
	int error;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* Normalize adapter values... */
	adp->va_info.vi_height *= adp->va_info.vi_cheight;
	adp->va_info.vi_width *= adp->va_info.vi_cwidth;

	/* Set the current font pixels... */
	sc->gfbc->font = sc->gfbc->fonts[page].data;

	/* Set the current font width... */
	adp->va_info.vi_cwidth = sc->gfbc->fonts[page].width;

	/* Set the current font height... */
	adp->va_info.vi_cheight = sc->gfbc->fonts[page].height;

	/* Recompute adapter values... */
	adp->va_info.vi_height /= adp->va_info.vi_cheight;
	adp->va_info.vi_width /= adp->va_info.vi_cwidth;

	return(error);
}

int
gfb_save_palette(video_adapter_t *adp, u_char *palette)
{
	struct gfb_softc *sc;
	int error;
	int i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

#if 0
	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_save_palette)
		error = sc->gfbc->ramdac_save_palette(adp, &sc->gfbc->palette);

	else
		/* Otherwise, use the built-in functionality... */
		error = sc->gfbc->builtin_save_palette(adp, &sc->gfbc->palette);
#endif

	for(i = 0; i < sc->gfbc->palette.count; i++) {
		palette[(3 * i)] = sc->gfbc->palette.red[i];
		palette[(3 * i) + 1] = sc->gfbc->palette.green[i];
		palette[(3 * i) + 2] = sc->gfbc->palette.blue[i];
	}
	return(error);
}

int
gfb_load_palette(video_adapter_t *adp, u_char *palette)
{
	struct gfb_softc *sc;
	int error;
	int i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	for(i = 0; i < sc->gfbc->palette.count; i++) {
		sc->gfbc->palette.red[i] = palette[(3 * i)];
		sc->gfbc->palette.green[i] = palette[(3 * i) + 1];
		sc->gfbc->palette.blue[i] = palette[(3 * i) + 2];
	}

	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_load_palette)
		error = sc->gfbc->ramdac_load_palette(adp, &sc->gfbc->palette);
	else
		/* Otherwise, use the built-in functionality... */
		error = sc->gfbc->builtin_load_palette(adp, &sc->gfbc->palette);

	return(error);
}

int
gfb_set_border(video_adapter_t *adp, int color)
{

	return(ENODEV);
}

int
gfb_save_state(video_adapter_t *adp, void *p, size_t size)
{
	int i;
	u_int32_t *regs;

	regs = (u_int32_t *)p;
	regs[0] = size;
	for(i = 1; i <= size; i++)
		regs[i] = READ_GFB_REGISTER(adp, i);
	return(0);
}

int
gfb_load_state(video_adapter_t *adp, void *p)
{
	size_t size;
	int i;
	u_int32_t *regs;

	regs = (u_int32_t *)p;
	size = regs[0];
	for(i = 1; i <= size; i++)
		WRITE_GFB_REGISTER(adp, i, regs[i]);
	return(0);
}

int
gfb_set_win_org(video_adapter_t *adp, off_t offset)
{

	adp->va_window_orig = offset;
	return(0);
}

int
gfb_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	struct gfb_softc *sc;
	int error;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_read_hw_cursor)
		error = sc->gfbc->ramdac_read_hw_cursor(adp, col, row);
	else
		/* Otherwise, use the built-in functionality... */
		error = sc->gfbc->builtin_read_hw_cursor(adp, col, row);

	return(error);
}

int
gfb_set_hw_cursor(adp, col, row)
	video_adapter_t *adp;
	int col;
	int row;
{
	int error;
	struct gfb_softc *sc;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_set_hw_cursor)
		error = sc->gfbc->ramdac_set_hw_cursor(adp, col, row);

	/* Otherwise, use the built-in functionality... */
	else
		error = sc->gfbc->builtin_set_hw_cursor(adp, col, row);
	return(error);
}

int
gfb_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int cellsize, int blink)
{
	struct gfb_softc *sc;
	int error;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_set_hw_cursor_shape)
		error = sc->gfbc->ramdac_set_hw_cursor_shape(adp, base, height,
		    cellsize, blink);
	else
		/* Otherwise, use the built-in functionality... */
		error = sc->gfbc->builtin_set_hw_cursor_shape(adp, base,
		    height, cellsize, blink);

	return(error);
}

int
gfb_mmap(video_adapter_t *adp, vm_offset_t offset, int prot)
{
	int error;

	if(offset > adp->va_window_size - PAGE_SIZE)
		error = ENXIO;
#ifdef __i386__
	error = i386_btop(adp->va_info.vi_window + offset);
#elsif defined(__alpha__)
	error = alpha_btop(adp->va_info.vi_window + offset);
#else
	error = ENXIO;
#endif
	return(error);
}

int
gfb_ioctl(video_adapter_t *adp, u_long cmd, caddr_t arg)
{
	struct gfb_softc *sc;
	int error;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	switch (cmd) {
	case FBIOPUTCMAP:
		/* FALLTHROUGH */
	case FBIO_GETWINORG:
		/* FALLTHROUGH */
	case FBIO_SETWINORG:
		/* FALLTHROUGH */
	case FBIO_SETDISPSTART:
		/* FALLTHROUGH */
	case FBIO_SETLINEWIDTH:
		/* FALLTHROUGH */
	case FBIO_GETPALETTE:
		/* FALLTHROUGH */
	case FBIOGTYPE:
		/* FALLTHROUGH */
	case FBIOGETCMAP:
		/* FALLTHROUGH */
	default:
		error = fb_commonioctl(adp, cmd, arg); 
	}
	return(error);
}

int
gfb_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	int off;

	/*
	   Just traverse the buffer, one pixel span at a time, setting
	   each pixel to the block-color...
	*/
	for(off = (x * y); off < ((x + cx) * (y + cy)); off++)
		(*vidsw[adp->va_index]->putp)(adp, off, 0x000007ff, 0xffffffff,
		    sizeof(u_int32_t), 1, 0, 0);

	return(0);
}

int
gfb_bitblt(video_adapter_t *adp, ...)
{
	va_list args;
	vm_offset_t src, dst;
	int count, i;
	u_int32_t val;

	va_start(args, adp);

	src = (va_arg(args, vm_offset_t) + adp->va_window_orig) &
	    0x0000000000fffff8;
	dst = (va_arg(args, vm_offset_t) + adp->va_window_orig) &
	    0x0000000000fffff8;
	count = va_arg(args, int);
	for(i = 0; i < count; i++, src++, dst++) {
		val = READ_GFB_BUFFER(adp, src);
		WRITE_GFB_BUFFER(adp, dst, val);
	}
	va_end(args);
	return(0);
}

int
/*gfb_clear(video_adapter_t *adp, int n)*/
gfb_clear(video_adapter_t *adp)
	video_adapter_t *adp;
{
	int off;

#if 0
	if(n == 0)
		return(0);
#endif

	/*
	   Just traverse the buffer, one 2K-pixel span at a time, clearing
	   each pixel...
	*/
	/* for(off = 0; off < (n * adp->va_line_width); off += (2 KB)) */
	for(off = 0; off < adp->va_window_size; off++)
		(*vidsw[adp->va_index]->putp)(adp, off, 0x000007ff, 0xffffffff,
		    sizeof(u_int32_t), 1, 0, 0);

	return(0);
}

int
gfb_diag(video_adapter_t *adp, int level)
{
	video_info_t info;
	struct gfb_softc *sc;
	int error;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* Just dump everything we know about the adapter to the screen... */
	fb_dump_adp_info(sc->driver_name, adp, level);

	/* Try to get the info on this adapter... */
	if(!(error = (*vidsw[adp->va_index]->get_info)(adp,
	    adp->va_initial_mode, &info)))
		/*
		   Just dump everything we know about the adapter's mode
		   to the screen...
		*/
		fb_dump_mode_info(sc->driver_name, adp, &info, level);

	return(error);
}

int
gfb_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

#if 0
	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_save_cursor_palette)
		error = sc->gfbc->ramdac_save_cursor_palette(adp,
		    &sc->gfbc->cursor_palette);

	else
		/* Otherwise, use the built-in functionality... */
		error = sc->gfbc->builtin_save_cursor_palette(adp,
		    &sc->gfbc->cursor_palette);
#endif

	for(i = 0; i < sc->gfbc->cursor_palette.count; i++) {
		palette[(3 * i)] = sc->gfbc->cursor_palette.red[i];
		palette[(3 * i) + 1] = sc->gfbc->cursor_palette.green[i];
		palette[(3 * i) + 2] = sc->gfbc->cursor_palette.blue[i];
	}
	return(error);
}

int
gfb_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	for(i = 0; i < sc->gfbc->cursor_palette.count; i++) {
		sc->gfbc->cursor_palette.red[i] = palette[(3 * i)];
		sc->gfbc->cursor_palette.green[i] = palette[(3 * i) + 1];
		sc->gfbc->cursor_palette.blue[i] = palette[(3 * i) + 2];
	}

	/* If we have a RAMDAC-specific counterpart, use it... */
	if(sc->gfbc->ramdac_load_cursor_palette)
		error = sc->gfbc->ramdac_load_cursor_palette(adp,
		    &sc->gfbc->cursor_palette);
	else
		/* Otherwise, use the built-in functionality... */
		error = sc->gfbc->builtin_load_cursor_palette(adp,
		    &sc->gfbc->cursor_palette);

	return(error);
}

int
gfb_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{
	int error, num_pixels;

	if(n == 0)
		return(0);
	num_pixels = adp->va_info.vi_cheight * adp->va_line_width;
	error = (*vidsw[adp->va_index]->bitblt)(adp, src * num_pixels,
	    dst * num_pixels, n * num_pixels);
	return(error);
}

int
gfb_putp(video_adapter_t *adp, vm_offset_t off, u_int32_t p, u_int32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{
	int i, j, k, num_shifts;
	u_int32_t _p, val[32];

	if(bpp < 1)
		return(-1);

	/*
	    If we don't display bits right-to-left (little-bitian?),
	    then perform a bit-swap on p...
    	*/
	if(bit_ltor) {
		num_shifts = 8 * size;
		for(i = 0, _p = 0; i < num_shifts; i++, p >>= 1) {
			_p <<= 1;
			_p |= (p & 0x00000001);
		}
	} else
		_p = p;

	switch(bpp) {
	/* Accelerate the simplest cases... */
	case 1:
		if((a & 0x00000001) == 0)
			val[0] = 0;
		else if(size <= 0)
			val[0] = 0;
		else if(size == 1)
			val[0] = _p & 0x000000ff;
		else if(size == 2)
			val[0] = _p & 0x0000ffff;
		else if(size == 3)
			val[0] = _p & 0x00ffffff;
		else if(size == 4)
			val[0] = _p & 0xffffffff;
		break;

	/* Only do the following if we are not a simple case... */
	case 8:
		if(size > 0) {
			a &= 0x000000ff;
			val[0] = 0;
			if(_p & 0x00000001) val[0] |= (a);
			if(_p & 0x00000002) val[0] |= (a << 8);
			if(_p & 0x00000004) val[0] |= (a << 16);
			if(_p & 0x00000008) val[0] |= (a << 24);
			val[1] = 0;
			if(_p & 0x00000010) val[1] |= (a);
			if(_p & 0x00000020) val[1] |= (a << 8);
			if(_p & 0x00000040) val[1] |= (a << 16);
			if(_p & 0x00000080) val[1] |= (a << 24);
		}
		if(size > 1) {
			val[2] = 0;
			if(_p & 0x00000100) val[2] |= (a);
			if(_p & 0x00000200) val[2] |= (a << 8);
			if(_p & 0x00000400) val[2] |= (a << 16);
			if(_p & 0x00000800) val[2] |= (a << 24);
			val[3] = 0;
			if(_p & 0x00001000) val[3] |= (a);
			if(_p & 0x00002000) val[3] |= (a << 8);
			if(_p & 0x00004000) val[3] |= (a << 16);
			if(_p & 0x00008000) val[3] |= (a << 24);
		}	
		if(size > 2) {
			val[4] = 0;
			if(_p & 0x00010000) val[4] |= (a);
			if(_p & 0x00020000) val[4] |= (a << 8);
			if(_p & 0x00040000) val[4] |= (a << 16);
			if(_p & 0x00080000) val[4] |= (a << 24);
			val[5] = 0;
			if(_p & 0x00100000) val[5] |= (a);
			if(_p & 0x00200000) val[5] |= (a << 8);
			if(_p & 0x00400000) val[5] |= (a << 16);
			if(_p & 0x00800080) val[5] |= (a << 24);
		}
		if(size > 3) {
			val[6] = 0;
			if(_p & 0x01000000) val[6] |= (a);
			if(_p & 0x02000000) val[6] |= (a << 8);
			if(_p & 0x04000000) val[6] |= (a << 16);
			if(_p & 0x08000000) val[6] |= (a << 24);
			val[7] = 0;
			if(_p & 0x10000000) val[7] |= (a);
			if(_p & 0x20000000) val[7] |= (a << 8);
			if(_p & 0x40000000) val[7] |= (a << 16);
			if(_p & 0x80000000) val[7] |= (a << 24);
		}
		break;
	case 16:
		if(size > 0) {
			a &= 0x0000ffff;
			if(_p & 0x00000001) val[0] |= (a);
			if(_p & 0x00000002) val[0] |= (a << 16);
			if(_p & 0x00000004) val[1] |= (a);
			if(_p & 0x00000008) val[1] |= (a << 16);
			if(_p & 0x00000010) val[2] |= (a);
			if(_p & 0x00000020) val[2] |= (a << 16);
			if(_p & 0x00000040) val[3] |= (a);
			if(_p & 0x00000080) val[3] |= (a << 16);
		}
		if(size > 1) {
			if(_p & 0x00000100) val[4] |= (a);
			if(_p & 0x00000200) val[4] |= (a << 16);
			if(_p & 0x00000400) val[5] |= (a);
			if(_p & 0x00000800) val[5] |= (a << 16);
			if(_p & 0x00001000) val[6] |= (a);
			if(_p & 0x00002000) val[6] |= (a << 16);
			if(_p & 0x00004000) val[7] |= (a);
			if(_p & 0x00008000) val[7] |= (a << 16);
		}
		if(size > 2) {
			if(_p & 0x00010000) val[8] |= (a);
			if(_p & 0x00020000) val[8] |= (a << 16);
			if(_p & 0x00040000) val[9] |= (a);
			if(_p & 0x00080000) val[9] |= (a << 16);
			if(_p & 0x00100000) val[10] |= (a);
			if(_p & 0x00200000) val[10] |= (a << 16);
			if(_p & 0x00400000) val[11] |= (a);
			if(_p & 0x00800000) val[11] |= (a << 16);
		}
		if(size > 3) {
			if(_p & 0x01000000) val[12] |= (a);
			if(_p & 0x02000000) val[12] |= (a << 16);
			if(_p & 0x04000000) val[13] |= (a);
			if(_p & 0x08000000) val[13] |= (a << 16);
			if(_p & 0x10000000) val[14] |= (a);
			if(_p & 0x20000000) val[14] |= (a << 16);
			if(_p & 0x40000000) val[15] |= (a);
			if(_p & 0x80000000) val[15] |= (a << 16);
		}
		break;
	case 32:
		if(size > 0) {
			a &= 0xffffffff;
			if(_p & 0x00000001) val[0] = (a);
			if(_p & 0x00000002) val[1] = (a);
			if(_p & 0x00000004) val[2] = (a);
			if(_p & 0x00000008) val[3] = (a);
			if(_p & 0x00000010) val[4] = (a);
			if(_p & 0x00000020) val[5] = (a);
			if(_p & 0x00000040) val[6] = (a);
			if(_p & 0x00000080) val[7] = (a);
		}
		if(size > 1) {
			if(_p & 0x00000100) val[8] = (a);
			if(_p & 0x00000200) val[9] = (a);
			if(_p & 0x00000400) val[10] = (a);
			if(_p & 0x00000800) val[11] = (a);
			if(_p & 0x00001000) val[12] = (a);
			if(_p & 0x00002000) val[13] = (a);
			if(_p & 0x00004000) val[14] = (a);
			if(_p & 0x00008000) val[15] = (a);
		}
		if(size > 2) {
			if(_p & 0x00010000) val[16] = (a);
			if(_p & 0x00020000) val[17] = (a);
			if(_p & 0x00040000) val[18] = (a);
			if(_p & 0x00080000) val[19] = (a);
			if(_p & 0x00100000) val[20] = (a);
			if(_p & 0x00200000) val[21] = (a);
			if(_p & 0x00400000) val[22] = (a);
			if(_p & 0x00800000) val[23] = (a);
		}
		if(size > 3) {
			if(_p & 0x01000000) val[24] = (a);
			if(_p & 0x02000000) val[25] = (a);
			if(_p & 0x04000000) val[26] = (a);
			if(_p & 0x08000000) val[27] = (a);
			if(_p & 0x10000000) val[28] = (a);
			if(_p & 0x20000000) val[29] = (a);
			if(_p & 0x40000000) val[30] = (a);
			if(_p & 0x80000000) val[31] = (a);
		}
		break;
	default:
		break;
	}
	j = (bpp == 1) ? 1 : bpp * size / sizeof(u_int32_t);

	/*
	    If we don't display bytes right-to-left (little-endian),
	    then perform a byte-swap on p (we don't have to swap if
	    bpp == 1 and val[0] == 0)...
	*/
	if((byte_ltor) && (j > 1) && (val[j] != 0)) {
		for(i = 0; i < (j - i); i++) {
			_p = val[j - i];
			val[j - i] = val[i];
			val[i] = _p;
		}
		for(i = 0; i < j; i++) {
			_p = val[i];
			for(k = 0, val[i] = 0; k < sizeof(u_int32_t);
			    k++, _p >>= 8) {
				val[i] <<= 8;
				val[i] |= (_p & 0xff);
			}
		}
	}

	for(i = 0; i < j; i++) {
		/* Write the pixel-row... */
		WRITE_GFB_BUFFER(adp, (off + i), val[i]);
	}
	return(0);
}

int
gfb_putc(video_adapter_t *adp, vm_offset_t off, u_int8_t c, u_int8_t a)
{
	vm_offset_t poff;
	struct gfb_softc *sc;
	int i, pixel_size;
	u_int row, col;
	u_int8_t *pixel;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];
	pixel_size = adp->va_info.vi_depth / 8;

	/* Get the start of the array of pixels rows for this character... */
	pixel = sc->gfbc->font + (c * adp->va_info.vi_cheight);

	/* Calculate the new cursor position... */
	row = off / adp->va_info.vi_width;
	col = off % adp->va_info.vi_width;

	/* Iterate over all the pixel rows for this character... */
	for(i = 0; i < adp->va_info.vi_cheight; i++) {
		/* Get the address of the character's pixel-row... */
		poff = ((col * adp->va_info.vi_cwidth * pixel_size) +
		    (((row * adp->va_info.vi_cheight) + i) *
		    adp->va_line_width)) / sizeof(u_int32_t);

		/* Now display the current pixel row... */
		(*vidsw[adp->va_index]->putp)(adp, poff, pixel[i], a,
		    sizeof(u_int8_t), 1, 1, 0);
	}
	return(0);
}

int
gfb_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	struct gfb_softc *sc;
	int i;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* If the string in empty, just return now... */
	if(len == 0)
		return(0);

	for(i = 0; i < len; i++)
		(*vidsw[adp->va_index]->putc)(adp, off + i, s[i] & 0x00ff,
		    (s[i] & 0xff00) >> 8);

	return(0);
}

int
gfb_putm(video_adapter_t *adp, int x, int y, u_int8_t *pixel_image,
    u_int32_t pixel_mask, int size)
{
	vm_offset_t poff;
	int i, pixel_size;

	pixel_size = adp->va_info.vi_depth / 8;

	/* Iterate over all the pixel rows for the mouse pointer... */
	for(i = 0; i < size; i++) {
		/* Get the address of the mouse pointer's pixel-row... */
		poff = ((x * pixel_size) + ((y + i) * adp->va_line_width)) /
		    sizeof(u_int32_t);
		/* Now display the current pixel-row... */
		(*vidsw[adp->va_index]->putp)(adp, poff, pixel_image[i],
		    pixel_mask, sizeof(u_int8_t), 1, 1, 0);
	}

	return(0);
}

int
gfb_error(void)
{

	return(0);
}
