/*-
 * Copyright (c) 2005, 2006 Rink Springer <rink@il.fontys.nl>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This is the syscon(4)-ized version of the Xbox Frame Buffer driver. It
 * supports about all features required, such as mouse support.
 *
 * A lot of functions that are not useful to us have not been implemented.
 * It appears that some functions are never called, but these implementations
 * are here nevertheless.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm_param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/consio.h>
#include <sys/limits.h>
#include <sys/tty.h>
#include <sys/kbio.h>
#include <sys/fbio.h>
#include <dev/kbd/kbdreg.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bus.h>
#include <machine/xbox.h>
#include <x86/legacyvar.h>
#include <dev/fb/fbreg.h>
#include <dev/fb/gfb.h>
#include <dev/syscons/syscons.h>

struct xboxfb_softc {
	video_adapter_t	sc_va;

	/* screen height (pixels) */
	uint32_t sc_height;

	/* screen width (pixels) */
	uint32_t sc_width;

	/* pointer to the actual XBOX video memory */
	char* sc_framebuffer;

	/* pointer to the font used */
	const struct gfb_font* sc_font;
};

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

#define XBOXFB_DRIVER_NAME "xboxsc"

extern const struct gfb_font bold8x16;

static vi_probe_t xboxfb_probe;
static vi_init_t xboxfb_init;
static vi_get_info_t xboxfb_get_info;
static vi_query_mode_t xboxfb_query_mode;
static vi_set_mode_t xboxfb_set_mode;
static vi_save_font_t xboxfb_save_font;
static vi_load_font_t xboxfb_load_font;
static vi_show_font_t xboxfb_show_font;
static vi_save_palette_t xboxfb_save_palette;
static vi_load_palette_t xboxfb_load_palette;
static vi_set_border_t xboxfb_set_border;
static vi_save_state_t xboxfb_save_state;
static vi_load_state_t xboxfb_load_state;
static vi_set_win_org_t xboxfb_set_win_org;
static vi_read_hw_cursor_t xboxfb_read_hw_cursor;
static vi_set_hw_cursor_t xboxfb_set_hw_cursor;
static vi_set_hw_cursor_shape_t xboxfb_set_hw_cursor_shape;
static vi_blank_display_t xboxfb_blank_display;
static vi_mmap_t xboxfb_mmap;
static vi_ioctl_t xboxfb_ioctl;
static vi_clear_t xboxfb_clear;
static vi_fill_rect_t xboxfb_fill_rect;
static vi_bitblt_t xboxfb_bitblt;
static vi_diag_t xboxfb_diag;
static vi_save_cursor_palette_t xboxfb_save_cursor_palette;
static vi_load_cursor_palette_t xboxfb_load_cursor_palette;
static vi_copy_t xboxfb_copy;
static vi_putp_t xboxfb_putp;
static vi_putc_t xboxfb_putc;
static vi_puts_t xboxfb_puts;
static vi_putm_t xboxfb_putm;

static video_switch_t xboxvidsw = {
	.probe                = xboxfb_probe,
	.init                 = xboxfb_init,
	.get_info             = xboxfb_get_info,
	.query_mode           = xboxfb_query_mode,
	.set_mode             = xboxfb_set_mode,
	.save_font            = xboxfb_save_font,
	.load_font            = xboxfb_load_font,
	.show_font            = xboxfb_show_font,
	.save_palette         = xboxfb_save_palette,
	.load_palette         = xboxfb_load_palette,
	.set_border           = xboxfb_set_border,
	.save_state           = xboxfb_save_state,
	.load_state           = xboxfb_load_state,
	.set_win_org          = xboxfb_set_win_org,
	.read_hw_cursor       = xboxfb_read_hw_cursor,
	.set_hw_cursor        = xboxfb_set_hw_cursor,
	.set_hw_cursor_shape  = xboxfb_set_hw_cursor_shape,
	.blank_display        = xboxfb_blank_display,
	.mmap                 = xboxfb_mmap,
	.ioctl                = xboxfb_ioctl,
	.clear                = xboxfb_clear,
	.fill_rect            = xboxfb_fill_rect,
	.bitblt               = xboxfb_bitblt,
	NULL,
	NULL,
	.diag                 = xboxfb_diag,
	.save_cursor_palette  = xboxfb_save_cursor_palette,
	.load_cursor_palette  = xboxfb_load_cursor_palette,
	.copy                 = xboxfb_copy,
	.putp                 = xboxfb_putp,
	.putc                 = xboxfb_putc,
	.puts                 = xboxfb_puts,
	.putm                 = xboxfb_putm
};

static int xboxfb_configure(int flags);
VIDEO_DRIVER(xboxsc, xboxvidsw, xboxfb_configure);

static vr_init_t xbr_init;
static vr_clear_t xbr_clear;
static vr_draw_border_t xbr_draw_border;
static vr_draw_t xbr_draw;
static vr_set_cursor_t xbr_set_cursor;
static vr_draw_cursor_t xbr_draw_cursor;
static vr_blink_cursor_t xbr_blink_cursor;
static vr_set_mouse_t xbr_set_mouse;
static vr_draw_mouse_t xbr_draw_mouse;

/*
 * We use our own renderer; this is because we must emulate a hardware
 * cursor.
 */
static sc_rndr_sw_t xboxrend = {
	xbr_init,
	xbr_clear,
	xbr_draw_border,
	xbr_draw,
	xbr_set_cursor,
	xbr_draw_cursor,
	xbr_blink_cursor,
	xbr_set_mouse,
	xbr_draw_mouse
};
RENDERER(xboxsc, 0, xboxrend, gfb_set);

static struct xboxfb_softc xboxfb_sc;

/* color mappings, from dev/fb/creator.c */
static const uint32_t cmap[] = {
	0x00000000,			/* black */
	0x000000ff,			/* blue */
	0x0000ff00,			/* green */
	0x0000c0c0,			/* cyan */
	0x00ff0000,			/* red */
	0x00c000c0,			/* magenta */
	0x00c0c000,			/* brown */
	0x00c0c0c0,			/* light grey */
	0x00808080,			/* dark grey */
	0x008080ff,			/* light blue */
	0x0080ff80,			/* light green */
	0x0080ffff,			/* light cyan */
	0x00ff8080,			/* light red */
	0x00ff80ff,			/* light magenta */
	0x00ffff80,			/* yellow */
	0x00ffffff			/* white */
};

/* mouse pointer from dev/syscons/scgfbrndr.c */
static u_char mouse_pointer[16] = {
        0x00, 0x40, 0x60, 0x70, 0x78, 0x7c, 0x7e, 0x68,
        0x0c, 0x0c, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00
};

static int
xboxfb_init(int unit, video_adapter_t* adp, int flags)
{
	struct xboxfb_softc* sc = &xboxfb_sc;
	video_info_t* vi;
	int i;
	int* iptr;

	vi = &adp->va_info;

	vid_init_struct (adp, XBOXFB_DRIVER_NAME, -1, unit);
	sc->sc_height = SCREEN_HEIGHT;
	sc->sc_width = SCREEN_WIDTH;
	sc->sc_font = &bold8x16;
	if (!(adp->va_flags & V_ADP_INITIALIZED)) {
		/*
		 * We must make a mapping from video framebuffer memory
		 * to real. This is very crude:  we map the entire
		 * videomemory to PAGE_SIZE! Since our kernel lives at
		 * it's relocated address range (0xc0xxxxxx), it won't
		 * care.
		 *
		 * We use address PAGE_SIZE and up so we can still trap
		 * NULL pointers.  Once the real init is called, the
		 * mapping will be done via the OS and stored in a more
		 * sensible location ... but since we're not fully
		 * initialized, this is our only way to go :-(
		 */
		for (i = 0; i < (XBOX_FB_SIZE / PAGE_SIZE); i++) {
			pmap_kenter (((i + 1) * PAGE_SIZE), XBOX_FB_START + (i * PAGE_SIZE));
		}
		pmap_kenter ((i + 1) * PAGE_SIZE, XBOX_FB_START_PTR - XBOX_FB_START_PTR % PAGE_SIZE);
		sc->sc_framebuffer = (char*)PAGE_SIZE;

		/* ensure the framebuffer is where we want it to be */
		*(uint32_t*)((i + 1) * PAGE_SIZE + XBOX_FB_START_PTR % PAGE_SIZE) = XBOX_FB_START;

		/* clear the screen */
		iptr = (uint32_t*)sc->sc_framebuffer;
		for (i = 0; i < sc->sc_height * sc->sc_width; i++)
			*iptr++ = cmap[0];

		/* don't ever do this again! */
		adp->va_flags |= V_ADP_INITIALIZED;
	}

	vi->vi_mode = M_TEXT_80x25;
	vi->vi_cwidth = sc->sc_font->width;
	vi->vi_cheight = sc->sc_font->height;
	vi->vi_height = (sc->sc_height / vi->vi_cheight);
	vi->vi_width = (sc->sc_width / vi->vi_cwidth);
	vi->vi_flags = V_INFO_COLOR | V_INFO_LINEAR;
	vi->vi_mem_model = V_INFO_MM_DIRECT;

	adp->va_flags |= V_ADP_COLOR;

	if (vid_register(adp) < 0)
		return (ENXIO);

	adp->va_flags |= V_ADP_REGISTERED;

	return 0;
}

static int
xboxfb_probe(int unit, video_adapter_t** adp, void* arg, int flags)
{
	return 0;
}

static int
xboxfb_configure(int flags)
{
	struct xboxfb_softc* sc = &xboxfb_sc;

	/* Don't init the framebuffer on non-XBOX-es */
	if (!arch_i386_is_xbox)
		return 0;

	/*
	 * If we do only a probe, we are in such an early boot stadium
	 * that we cannot yet do a 'clean' initialization.
	 */
	if (flags & VIO_PROBE_ONLY) {
		xboxfb_init(0, &sc->sc_va, 0);
		return 1;
	}

	/* Do a clean mapping of the framebuffer memory */
	sc->sc_framebuffer = pmap_mapdev (XBOX_FB_START, XBOX_FB_SIZE);
	return 1;
}

static void
sc_identify(driver_t* driver, device_t parent)
{
	BUS_ADD_CHILD(parent, INT_MAX, SC_DRIVER_NAME, 0);
}

static int
sc_probe(device_t dev)
{
	device_set_desc(dev, "XBox System console");
	return (sc_probe_unit(device_get_unit(dev), device_get_flags(dev) | SC_AUTODETECT_KBD));
}

static int sc_attach(device_t dev)
{
	return (sc_attach_unit(device_get_unit(dev), device_get_flags(dev) | SC_AUTODETECT_KBD));
}

static device_method_t sc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	sc_identify),
	DEVMETHOD(device_probe,		sc_probe),
	DEVMETHOD(device_attach,	sc_attach),
	{ 0, 0 }
};

static driver_t xboxfb_sc_driver = {
	SC_DRIVER_NAME,
	sc_methods,
	sizeof(sc_softc_t)
};

static devclass_t sc_devclass;

DRIVER_MODULE(sc, legacy, xboxfb_sc_driver, sc_devclass, 0, 0);

static void
xbr_init(scr_stat* scp)
{
}

static void
xbr_clear(scr_stat* scp, int c, int attr)
{
}

static void
xbr_draw_border(scr_stat* scp, int color)
{
}

static void
xbr_draw(scr_stat* scp, int from, int count, int flip)
{
	video_adapter_t* adp = scp->sc->adp;
	int i, c, a;

	if (!flip) {
		/* Normal printing */
		vidd_puts(adp, from, (uint16_t*)sc_vtb_pointer(&scp->vtb, from), count);
	} else {	
		/* This is for selections and such: invert the color attribute */
		for (i = count; i-- > 0; ++from) {
			c = sc_vtb_getc(&scp->vtb, from);
			a = sc_vtb_geta(&scp->vtb, from) >> 8;
			vidd_putc(adp, from, c, (a >> 4) | ((a & 0xf) << 4));
		}
	}
}

static void
xbr_set_cursor(scr_stat* scp, int base, int height, int blink)
{
}

static void
xbr_draw_cursor(scr_stat* scp, int at, int blink, int on, int flip)
{
	struct xboxfb_softc* sc = &xboxfb_sc;
	video_adapter_t* adp = scp->sc->adp;
	uint32_t* ptri = (uint32_t*)sc->sc_framebuffer;
	int row, col, i, j;

	if (scp->curs_attr.height <= 0)
		return;

	/* calculate the coordinates in the video buffer */
	row = (at / adp->va_info.vi_width) * adp->va_info.vi_cheight;
	col = (at % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	ptri += (row * sc->sc_width) + col;

	/* our cursor consists of simply inverting the char under it */
	for (i = 0; i < adp->va_info.vi_cheight; i++) {
		for (j = 0; j < adp->va_info.vi_cwidth; j++) {
			*ptri++ ^= 0x00FFFFFF;
		}
		ptri += (sc->sc_width - adp->va_info.vi_cwidth);
	}
}

static void
xbr_blink_cursor(scr_stat* scp, int at, int flip)
{
}

static void
xbr_set_mouse(scr_stat* scp)
{
}

static void
xbr_draw_mouse(scr_stat* scp, int x, int y, int on)
{
	vidd_putm(scp->sc->adp, x, y, mouse_pointer, 0xffffffff, 16, 8);

}

static int
xboxfb_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	bcopy(&adp->va_info, info, sizeof(*info));
	return (0);
}

static int
xboxfb_query_mode(video_adapter_t *adp, video_info_t *info)
{
	return (ENODEV);
}

static int
xboxfb_set_mode(video_adapter_t *adp, int mode)
{
	return (0);
}

static int
xboxfb_save_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	return (ENODEV);
}

static int
xboxfb_load_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	return (ENODEV);
}

static int
xboxfb_show_font(video_adapter_t *adp, int page)
{
	return (ENODEV);
}

static int
xboxfb_save_palette(video_adapter_t *adp, u_char *palette)
{
	return (ENODEV);
}

static int
xboxfb_load_palette(video_adapter_t *adp, u_char *palette)
{
	return (ENODEV);
}

static int
xboxfb_set_border(video_adapter_t *adp, int border)
{
	return (0);
}

static int
xboxfb_save_state(video_adapter_t *adp, void *p, size_t size)
{
	return (ENODEV);
}

static int
xboxfb_load_state(video_adapter_t *adp, void *p)
{
	return (ENODEV);
}

static int
xboxfb_set_win_org(video_adapter_t *adp, off_t offset)
{
	return (ENODEV);
}

static int
xboxfb_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	*col = 0;
	*row = 0;
	return (0);
}

static int
xboxfb_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	return (ENODEV);
}

static int
xboxfb_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{
	return (ENODEV);
}

static int
xboxfb_blank_display(video_adapter_t *adp, int mode)
{
	return (0);
}

static int
xboxfb_mmap(video_adapter_t *adp, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	return (EINVAL);
}

static int
xboxfb_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{
	return (fb_commonioctl(adp, cmd, data));
}

static int
xboxfb_clear(video_adapter_t *adp)
{
	return (0);
}

static int
xboxfb_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	return (0);
}

static int
xboxfb_bitblt(video_adapter_t *adp, ...)
{
	return (ENODEV);
}

static int
xboxfb_diag(video_adapter_t *adp, int level)
{
	video_info_t info;

	fb_dump_adp_info(adp->va_name, adp, level);
	xboxfb_get_info(adp, 0, &info);
	fb_dump_mode_info(adp->va_name, adp, &info, level);
	return (0);
}

static int
xboxfb_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	return (ENODEV);
}

static int
xboxfb_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	return (ENODEV);
}

static int
xboxfb_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{
	return (ENODEV);
}

static int
xboxfb_putp(video_adapter_t *adp, vm_offset_t off, u_int32_t p, u_int32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{
	return (ENODEV);
}

static int
xboxfb_putc(video_adapter_t *adp, vm_offset_t off, u_int8_t c, u_int8_t a)
{
	int row, col;
	int i, j;
	struct xboxfb_softc* sc = &xboxfb_sc;
	uint32_t* ptri = (uint32_t*)sc->sc_framebuffer;
	const uint8_t* fontdata;
	uint32_t clr;
	uint8_t mask;

	/* calculate the position in the frame buffer */
	row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
	col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	fontdata = &sc->sc_font->data[c * adp->va_info.vi_cheight];
	ptri += (row * sc->sc_width) + col;

	/* Place the character on the screen, pixel by pixel */
	for (j = 0; j < adp->va_info.vi_cheight; j++) {
		mask = 0x80;
		for (i = 0; i < adp->va_info.vi_cwidth; i++) {
			clr = (*fontdata & mask) ? cmap[a & 0xf] : cmap[(a >> 4) & 0xf];
			*ptri++ = clr;
			mask >>= 1;
		}
		ptri += (sc->sc_width - adp->va_info.vi_cwidth);
		fontdata++;
	}
	return (0);
}

static int
xboxfb_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		vidd_putc(adp, off + i, s[i] & 0xff, (s[i] & 0xff00) >> 8);
	}
	return (0);
}

static int
xboxfb_putm(video_adapter_t *adp, int x, int y, u_int8_t *pixel_image,
    u_int32_t pixel_mask, int size, int width)
{
	struct xboxfb_softc* sc = &xboxfb_sc;
	uint32_t* ptri = (uint32_t*)sc->sc_framebuffer;
	int i, j;	

	if (x < 0 || y < 0 || x + width > sc->sc_width || y + (2 * size) > sc->sc_height)
		return 0;

	ptri += (y * sc->sc_width) + x;

	/* plot the mousecursor wherever the user wants it */
	for (j = 0; j < size; j++) {
		for (i = width; i > 0; i--) {
			if (pixel_image[j] & (1 << i))
				*ptri = cmap[0xf];
			ptri++;
		}
		ptri += (sc->sc_width - width);
	}
	return (0);
}
