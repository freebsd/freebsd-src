/*-
 * Copyright (c) 2003 Peter Grehan
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/sc_machdep.h>
#include <machine/platform.h>
#include <machine/pmap.h>

#include <sys/rman.h>

#include <dev/fb/fbreg.h>
#include <dev/syscons/syscons.h>

#include "ps3-hvcall.h"

#define PS3FB_SIZE (4*1024*1024)

#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET	0x0100
#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC		0x0101
#define  L1GPU_DISPLAY_SYNC_HSYNC			1
#define  L1GPU_DISPLAY_SYNC_VSYNC			2
#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP		0x0102

extern u_char dflt_font_16[];
extern u_char dflt_font_14[];
extern u_char dflt_font_8[];

static int ps3fb_configure(int flags);
void ps3fb_remap(void);

static vi_probe_t ps3fb_probe;
static vi_init_t ps3fb_init;
static vi_get_info_t ps3fb_get_info;
static vi_query_mode_t ps3fb_query_mode;
static vi_set_mode_t ps3fb_set_mode;
static vi_save_font_t ps3fb_save_font;
static vi_load_font_t ps3fb_load_font;
static vi_show_font_t ps3fb_show_font;
static vi_save_palette_t ps3fb_save_palette;
static vi_load_palette_t ps3fb_load_palette;
static vi_set_border_t ps3fb_set_border;
static vi_save_state_t ps3fb_save_state;
static vi_load_state_t ps3fb_load_state;
static vi_set_win_org_t ps3fb_set_win_org;
static vi_read_hw_cursor_t ps3fb_read_hw_cursor;
static vi_set_hw_cursor_t ps3fb_set_hw_cursor;
static vi_set_hw_cursor_shape_t ps3fb_set_hw_cursor_shape;
static vi_blank_display_t ps3fb_blank_display;
static vi_mmap_t ps3fb_mmap;
static vi_ioctl_t ps3fb_ioctl;
static vi_clear_t ps3fb_clear;
static vi_fill_rect_t ps3fb_fill_rect;
static vi_bitblt_t ps3fb_bitblt;
static vi_diag_t ps3fb_diag;
static vi_save_cursor_palette_t ps3fb_save_cursor_palette;
static vi_load_cursor_palette_t ps3fb_load_cursor_palette;
static vi_copy_t ps3fb_copy;
static vi_putp_t ps3fb_putp;
static vi_putc_t ps3fb_putc;
static vi_puts_t ps3fb_puts;
static vi_putm_t ps3fb_putm;

struct ps3fb_softc {
	video_adapter_t sc_va;
	struct cdev 	*sc_si;
	int		sc_console;

	intptr_t	sc_addr;
	int		sc_height;
	int		sc_width;
	int		sc_stride;
	int		sc_ncol;
	int		sc_nrow;

	int		sc_xmargin;
	int		sc_ymargin;

	u_char		*sc_font;
	int		sc_font_height;
};

static video_switch_t ps3fbvidsw = {
	.probe			= ps3fb_probe,
	.init			= ps3fb_init,
	.get_info		= ps3fb_get_info,
	.query_mode		= ps3fb_query_mode,
	.set_mode		= ps3fb_set_mode,
	.save_font		= ps3fb_save_font,
	.load_font		= ps3fb_load_font,
	.show_font		= ps3fb_show_font,
	.save_palette		= ps3fb_save_palette,
	.load_palette		= ps3fb_load_palette,
	.set_border		= ps3fb_set_border,
	.save_state		= ps3fb_save_state,
	.load_state		= ps3fb_load_state,
	.set_win_org		= ps3fb_set_win_org,
	.read_hw_cursor		= ps3fb_read_hw_cursor,
	.set_hw_cursor		= ps3fb_set_hw_cursor,
	.set_hw_cursor_shape	= ps3fb_set_hw_cursor_shape,
	.blank_display		= ps3fb_blank_display,
	.mmap			= ps3fb_mmap,
	.ioctl			= ps3fb_ioctl,
	.clear			= ps3fb_clear,
	.fill_rect		= ps3fb_fill_rect,
	.bitblt			= ps3fb_bitblt,
	.diag			= ps3fb_diag,
	.save_cursor_palette	= ps3fb_save_cursor_palette,
	.load_cursor_palette	= ps3fb_load_cursor_palette,
	.copy			= ps3fb_copy,
	.putp			= ps3fb_putp,
	.putc			= ps3fb_putc,
	.puts			= ps3fb_puts,
	.putm			= ps3fb_putm,
};

VIDEO_DRIVER(ps3fb, ps3fbvidsw, ps3fb_configure);

extern sc_rndr_sw_t txtrndrsw;
RENDERER(ps3fb, 0, txtrndrsw, gfb_set);

RENDERER_MODULE(ps3fb, gfb_set);

/*
 * Define the iso6429-1983 colormap
 */
static struct {
	uint8_t	red;
	uint8_t	green;
	uint8_t	blue;
} ps3fb_cmap[16] = {		/*  #     R    G    B   Color */
				/*  -     -    -    -   ----- */
	{ 0x00, 0x00, 0x00 },	/*  0     0    0    0   Black */
	{ 0x00, 0x00, 0xaa },	/*  1     0    0  2/3   Blue  */
	{ 0x00, 0xaa, 0x00 },	/*  2     0  2/3    0   Green */
	{ 0x00, 0xaa, 0xaa },	/*  3     0  2/3  2/3   Cyan  */
	{ 0xaa, 0x00, 0x00 },	/*  4   2/3    0    0   Red   */
	{ 0xaa, 0x00, 0xaa },	/*  5   2/3    0  2/3   Magenta */
	{ 0xaa, 0x55, 0x00 },	/*  6   2/3  1/3    0   Brown */
	{ 0xaa, 0xaa, 0xaa },	/*  7   2/3  2/3  2/3   White */
        { 0x55, 0x55, 0x55 },	/*  8   1/3  1/3  1/3   Gray  */
	{ 0x55, 0x55, 0xff },	/*  9   1/3  1/3    1   Bright Blue  */
	{ 0x55, 0xff, 0x55 },	/* 10   1/3    1  1/3   Bright Green */
	{ 0x55, 0xff, 0xff },	/* 11   1/3    1    1   Bright Cyan  */
	{ 0xff, 0x55, 0x55 },	/* 12     1  1/3  1/3   Bright Red   */
	{ 0xff, 0x55, 0xff },	/* 13     1  1/3    1   Bright Magenta */
	{ 0xff, 0xff, 0x80 },	/* 14     1    1  1/3   Bright Yellow */
	{ 0xff, 0xff, 0xff }	/* 15     1    1    1   Bright White */
};

#define	TODO	printf("%s: unimplemented\n", __func__)

static u_int16_t ps3fb_static_window[ROW*COL];

static struct ps3fb_softc ps3fb_softc;

static __inline int
ps3fb_background(uint8_t attr)
{
	return (attr >> 4);
}

static __inline int
ps3fb_foreground(uint8_t attr)
{
	return (attr & 0x0f);
}

static u_int
ps3fb_pix32(int attr)
{
	u_int retval;

	retval = (ps3fb_cmap[attr].red  << 16) |
		(ps3fb_cmap[attr].green << 8) |
		ps3fb_cmap[attr].blue;

	return (retval);
}

static int
ps3fb_configure(int flags)
{
	struct ps3fb_softc *sc;
	int disable;
	char compatible[64];
#if 0
	phandle_t root;
#endif
	static int done = 0;

	disable = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disable);
	if (disable != 0)
		return (0);

	if (done != 0)
		return (0);
	done = 1;

	sc = &ps3fb_softc;

#if 0
	root = OF_finddevice("/");
	if (OF_getprop(root, "compatible", compatible, sizeof(compatible)) <= 0)
                return (0);
	
	if (strncmp(compatible, "sony,ps3", sizeof(compatible)) != 0)
		return (0);
#else
	TUNABLE_STR_FETCH("hw.platform", compatible, sizeof(compatible));
	if (strcmp(compatible, "ps3") != 0)
		return (0);
#endif

	sc->sc_console = 1;

	/* XXX: get from HV repository */
	sc->sc_height = 480;
	sc->sc_width = 720;
	sc->sc_stride = sc->sc_width*4;

	/*
	 * The loader puts the FB at 0x10000000, so use that for now.
	 */

	sc->sc_addr = 0x10000000;
	ps3fb_init(0, &sc->sc_va, 0);

	return (0);
}

void
ps3fb_remap(void)
{
	vm_offset_t va, fb_paddr;
	uint64_t fbhandle, fbcontext;

	lv1_gpu_close();
	lv1_gpu_open(0);

	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET,
	    0,0,0,0);
	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET,
	    0,0,1,0);
	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC,
	    0,L1GPU_DISPLAY_SYNC_VSYNC,0,0);
	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC,
	    1,L1GPU_DISPLAY_SYNC_VSYNC,0,0);
	lv1_gpu_memory_allocate(PS3FB_SIZE, 0, 0, 0, 0, &fbhandle, &fb_paddr);
	lv1_gpu_context_allocate(fbhandle, 0, &fbcontext);

	lv1_gpu_context_attribute(fbcontext,
	    L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, 0, 0, 0, 0);
	lv1_gpu_context_attribute(fbcontext,
	    L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, 1, 0, 0, 0);

	for (va = 0; va < PS3FB_SIZE; va += PAGE_SIZE)
		pmap_kenter_attr(0x10000000 + va, fb_paddr + va,
		    VM_MEMATTR_WRITE_COMBINING); 
}

static int
ps3fb_probe(int unit, video_adapter_t **adp, void *arg, int flags)
{
	TODO;
	return (0);
}

static int
ps3fb_init(int unit, video_adapter_t *adp, int flags)
{
	struct ps3fb_softc *sc;
	video_info_t *vi;
	int cxborder, cyborder;
	int font_height;

	sc = (struct ps3fb_softc *)adp;
	vi = &adp->va_info;

	vid_init_struct(adp, "ps3fb", -1, unit);

	/* The default font size can be overridden by loader */
	font_height = 16;
	TUNABLE_INT_FETCH("hw.syscons.fsize", &font_height);
	if (font_height == 8) {
		sc->sc_font = dflt_font_8;
		sc->sc_font_height = 8;
	} else if (font_height == 14) {
		sc->sc_font = dflt_font_14;
		sc->sc_font_height = 14;
	} else {
		/* default is 8x16 */
		sc->sc_font = dflt_font_16;
		sc->sc_font_height = 16;
	}

	/* The user can set a border in chars - default is 1 char width */
	cxborder = 8;
	cyborder = 2;
	TUNABLE_INT_FETCH("hw.syscons.xborder", &cxborder);
	TUNABLE_INT_FETCH("hw.syscons.yborder", &cyborder);

	vi->vi_cheight = sc->sc_font_height;
	vi->vi_width = sc->sc_width/8 - 2*cxborder;
	vi->vi_height = sc->sc_height/sc->sc_font_height - 2*cyborder;
	vi->vi_cwidth = 8;

	/*
	 * Clamp width/height to syscons maximums
	 */
	if (vi->vi_width > COL)
		vi->vi_width = COL;
	if (vi->vi_height > ROW)
		vi->vi_height = ROW;

	sc->sc_xmargin = (sc->sc_width - (vi->vi_width * vi->vi_cwidth)) / 2;
	sc->sc_ymargin = (sc->sc_height - (vi->vi_height * vi->vi_cheight))/2;

	/*
	 * Avoid huge amounts of conditional code in syscons by
	 * defining a dummy h/w text display buffer.
	 */
	adp->va_window = (vm_offset_t) ps3fb_static_window;

	/*
	 * Enable future font-loading and flag color support, as well as 
	 * adding V_ADP_MODECHANGE so that we ps3fb_set_mode() gets called
	 * when the X server shuts down. This enables us to get the console
	 * back when X disappears.
	 */
	adp->va_flags |= V_ADP_FONT | V_ADP_COLOR | V_ADP_MODECHANGE;

	ps3fb_set_mode(&sc->sc_va, 0);
	vid_register(&sc->sc_va);

	return (0);
}

static int
ps3fb_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	bcopy(&adp->va_info, info, sizeof(*info));
	return (0);
}

static int
ps3fb_query_mode(video_adapter_t *adp, video_info_t *info)
{
	TODO;
	return (0);
}

static int
ps3fb_set_mode(video_adapter_t *adp, int mode)
{
	struct ps3fb_softc *sc;

	sc = (struct ps3fb_softc *)adp;

	/* XXX: no real mode setting at the moment */

	ps3fb_blank_display(&sc->sc_va, V_DISPLAY_ON);

	return (0);
}

static int
ps3fb_save_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	TODO;
	return (0);
}

static int
ps3fb_load_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	struct ps3fb_softc *sc;

	sc = (struct ps3fb_softc *)adp;

	/*
	 * syscons code has already determined that current width/height
	 * are unchanged for this new font
	 */
	sc->sc_font = data;
	return (0);
}

static int
ps3fb_show_font(video_adapter_t *adp, int page)
{

	return (0);
}

static int
ps3fb_save_palette(video_adapter_t *adp, u_char *palette)
{
	/* TODO; */
	return (0);
}

static int
ps3fb_load_palette(video_adapter_t *adp, u_char *palette)
{
	/* TODO; */
	return (0);
}

static int
ps3fb_set_border(video_adapter_t *adp, int border)
{
	/* XXX Be lazy for now and blank entire screen */
	return (ps3fb_blank_display(adp, border));
}

static int
ps3fb_save_state(video_adapter_t *adp, void *p, size_t size)
{
	TODO;
	return (0);
}

static int
ps3fb_load_state(video_adapter_t *adp, void *p)
{
	TODO;
	return (0);
}

static int
ps3fb_set_win_org(video_adapter_t *adp, off_t offset)
{
	TODO;
	return (0);
}

static int
ps3fb_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	*col = 0;
	*row = 0;

	return (0);
}

static int
ps3fb_set_hw_cursor(video_adapter_t *adp, int col, int row)
{

	return (0);
}

static int
ps3fb_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{
	return (0);
}

static int
ps3fb_blank_display(video_adapter_t *adp, int mode)
{
	struct ps3fb_softc *sc;
	int i;
	uint32_t *addr;

	sc = (struct ps3fb_softc *)adp;
	addr = (uint32_t *) sc->sc_addr;

	for (i = 0; i < (sc->sc_stride/4)*sc->sc_height; i++)
		*(addr + i) = ps3fb_pix32(ps3fb_background(SC_NORM_ATTR));

	return (0);
}

static int
ps3fb_mmap(video_adapter_t *adp, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct ps3fb_softc *sc;

	sc = (struct ps3fb_softc *)adp;

	/*
	 * This might be a legacy VGA mem request: if so, just point it at the
	 * framebuffer, since it shouldn't be touched
	 */
	if (offset < sc->sc_stride*sc->sc_height) {
		*paddr = sc->sc_addr + offset;
		return (0);
	}

	return (EINVAL);
}

static int
ps3fb_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{

	return (0);
}

static int
ps3fb_clear(video_adapter_t *adp)
{
	TODO;
	return (0);
}

static int
ps3fb_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	TODO;
	return (0);
}

static int
ps3fb_bitblt(video_adapter_t *adp, ...)
{
	TODO;
	return (0);
}

static int
ps3fb_diag(video_adapter_t *adp, int level)
{
	TODO;
	return (0);
}

static int
ps3fb_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	TODO;
	return (0);
}

static int
ps3fb_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	TODO;
	return (0);
}

static int
ps3fb_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{
	TODO;
	return (0);
}

static int
ps3fb_putp(video_adapter_t *adp, vm_offset_t off, uint32_t p, uint32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{
	TODO;
	return (0);
}

static int
ps3fb_putc(video_adapter_t *adp, vm_offset_t off, uint8_t c, uint8_t a)
{
	struct ps3fb_softc *sc;
	int row;
	int col;
	int i, j, k;
	uint32_t *addr;
	u_char *p;

	sc = (struct ps3fb_softc *)adp;
        row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
        col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	p = sc->sc_font + c*sc->sc_font_height;
	addr = (uint32_t *)sc->sc_addr
		+ (row + sc->sc_ymargin)*(sc->sc_stride/4)
		+ col + sc->sc_xmargin;

	for (i = 0; i < sc->sc_font_height; i++) {
		for (j = 0, k = 7; j < 8; j++, k--) {
			if ((p[i] & (1 << k)) == 0)
				*(addr + j) = ps3fb_pix32(ps3fb_background(a));
			else
				*(addr + j) = ps3fb_pix32(ps3fb_foreground(a));
		}
		addr += (sc->sc_stride/4);
	}

	return (0);
}

static int
ps3fb_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		ps3fb_putc(adp, off + i, s[i] & 0xff, (s[i] & 0xff00) >> 8);
	}
	return (0);
}

static int
ps3fb_putm(video_adapter_t *adp, int x, int y, uint8_t *pixel_image,
    uint32_t pixel_mask, int size, int width)
{
	struct ps3fb_softc *sc;
	int i, j, k;
	uint32_t fg, bg;
	uint32_t *addr;

	sc = (struct ps3fb_softc *)adp;
	addr = (uint32_t *)sc->sc_addr
		+ (y + sc->sc_ymargin)*(sc->sc_stride/4)
		+ x + sc->sc_xmargin;

	fg = ps3fb_pix32(ps3fb_foreground(SC_NORM_ATTR));
	bg = ps3fb_pix32(ps3fb_background(SC_NORM_ATTR));

	for (i = 0; i < size && i+y < sc->sc_height - 2*sc->sc_ymargin; i++) {
		for (j = 0, k = width; j < 8; j++, k--) {
			if (x + j >= sc->sc_width - 2*sc->sc_xmargin)
				continue;

			if (pixel_image[i] & (1 << k))
				*(addr + j) = (*(addr + j) == fg) ? bg : fg;
		}
		addr += (sc->sc_stride/4);
	}

	return (0);
}

/*
 * Define the syscons nexus device attachment
 */
static void
ps3fb_scidentify(driver_t *driver, device_t parent)
{
	device_t child;

	/*
	 * Add with a priority guaranteed to make it last on
	 * the device list
	 */
	if (strcmp(installed_platform(), "ps3") == 0)
		child = BUS_ADD_CHILD(parent, INT_MAX, SC_DRIVER_NAME, 0);
}

static int
ps3fb_scprobe(device_t dev)
{
	int error;

	if (strcmp(installed_platform(), "ps3") != 0)
		return (ENXIO);

	device_set_desc(dev, "System console");

	error = sc_probe_unit(device_get_unit(dev), 
	    device_get_flags(dev) | SC_AUTODETECT_KBD);
	if (error != 0)
		return (error);

	/* This is a fake device, so make sure we added it ourselves */
	return (BUS_PROBE_NOWILDCARD);
}

static int
ps3fb_scattach(device_t dev)
{
	return (sc_attach_unit(device_get_unit(dev),
	    device_get_flags(dev) | SC_AUTODETECT_KBD));
}

static device_method_t ps3fb_sc_methods[] = {
  	DEVMETHOD(device_identify,	ps3fb_scidentify),
	DEVMETHOD(device_probe,		ps3fb_scprobe),
	DEVMETHOD(device_attach,	ps3fb_scattach),
	{ 0, 0 }
};

static driver_t ps3fb_sc_driver = {
	SC_DRIVER_NAME,
	ps3fb_sc_methods,
	sizeof(sc_softc_t),
};

static devclass_t	sc_devclass;

DRIVER_MODULE(sc, nexus, ps3fb_sc_driver, sc_devclass, 0, 0);

/*
 * Define a stub keyboard driver in case one hasn't been
 * compiled into the kernel
 */
#include <sys/kbio.h>
#include <dev/kbd/kbdreg.h>

static int dummy_kbd_configure(int flags);

keyboard_switch_t ps3dummysw;

static int
dummy_kbd_configure(int flags)
{

	return (0);
}
KEYBOARD_DRIVER(ps3dummy, ps3dummysw, dummy_kbd_configure);

