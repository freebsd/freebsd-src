/*-
 * Copyright (c) 2003 Jake Burkholder.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <machine/bus.h>
#include <machine/ofw_upa.h>

#include <sys/rman.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/gallant12x22.h>
#include <dev/syscons/syscons.h>

#include <dev/ofw/openfirm.h>

#include <sparc64/creator/creator.h>

static int creator_configure(int flags);

static vi_probe_t creator_probe;
static vi_init_t creator_init;
static vi_get_info_t creator_get_info;
static vi_query_mode_t creator_query_mode;
static vi_set_mode_t creator_set_mode;
static vi_save_font_t creator_save_font;
static vi_load_font_t creator_load_font;
static vi_show_font_t creator_show_font;
static vi_save_palette_t creator_save_palette;
static vi_load_palette_t creator_load_palette;
static vi_set_border_t creator_set_border;
static vi_save_state_t creator_save_state;
static vi_load_state_t creator_load_state;
static vi_set_win_org_t creator_set_win_org;
static vi_read_hw_cursor_t creator_read_hw_cursor;
static vi_set_hw_cursor_t creator_set_hw_cursor;
static vi_set_hw_cursor_shape_t creator_set_hw_cursor_shape;
static vi_blank_display_t creator_blank_display;
static vi_mmap_t creator_mmap;
static vi_ioctl_t creator_ioctl;
static vi_clear_t creator_clear;
static vi_fill_rect_t creator_fill_rect;
static vi_bitblt_t creator_bitblt;
static vi_diag_t creator_diag;
static vi_save_cursor_palette_t creator_save_cursor_palette;
static vi_load_cursor_palette_t creator_load_cursor_palette;
static vi_copy_t creator_copy;
static vi_putp_t creator_putp;
static vi_putc_t creator_putc;
static vi_puts_t creator_puts;
static vi_putm_t creator_putm;

static void creator_ras_fifo_wait(struct creator_softc *sc, int n);
static void creator_ras_setbg(struct creator_softc *sc, int bg);
static void creator_ras_setfg(struct creator_softc *sc, int fg);
static void creator_ras_wait(struct creator_softc *sc);

static video_switch_t creatorvidsw = {
	.probe			= creator_probe,
	.init			= creator_init,
	.get_info		= creator_get_info,
	.query_mode		= creator_query_mode,
	.set_mode		= creator_set_mode,
	.save_font		= creator_save_font,
	.load_font		= creator_load_font,
	.show_font		= creator_show_font,
	.save_palette		= creator_save_palette,
	.load_palette		= creator_load_palette,
	.set_border		= creator_set_border,
	.save_state		= creator_save_state,
	.load_state		= creator_load_state,
	.set_win_org		= creator_set_win_org,
	.read_hw_cursor		= creator_read_hw_cursor,
	.set_hw_cursor		= creator_set_hw_cursor,
	.set_hw_cursor_shape	= creator_set_hw_cursor_shape,
	.blank_display		= creator_blank_display,
	.mmap			= creator_mmap,
	.ioctl			= creator_ioctl,
	.clear			= creator_clear,
	.fill_rect		= creator_fill_rect,
	.bitblt			= creator_bitblt,
	NULL,						/* XXX brain damage */
	NULL,						/* XXX brain damage */
	.diag			= creator_diag,
	.save_cursor_palette	= creator_save_cursor_palette,
	.load_cursor_palette	= creator_load_cursor_palette,
	.copy			= creator_copy,
	.putp			= creator_putp,
	.putc			= creator_putc,
	.puts			= creator_puts,
	.putm			= creator_putm
};

VIDEO_DRIVER(creator, creatorvidsw, creator_configure);

extern sc_rndr_sw_t txtrndrsw;
RENDERER(creator, 0, txtrndrsw, gfb_set);

RENDERER_MODULE(creator, gfb_set);

extern struct bus_space_tag nexus_bustag;

#define	C(r, g, b)	((b << 16) | (g << 8) | (r))
static int cmap[] = {
	C(0x00, 0x00, 0x00),		/* black */
	C(0x00, 0x00, 0xff),		/* blue */
	C(0x00, 0xff, 0x00),		/* green */
	C(0x00, 0xc0, 0xc0),		/* cyan */
	C(0xff, 0x00, 0x00),		/* red */
	C(0xc0, 0x00, 0xc0),		/* magenta */
	C(0xc0, 0xc0, 0x00),		/* brown */
	C(0xc0, 0xc0, 0xc0),		/* light grey */
	C(0x80, 0x80, 0x80),		/* dark grey */
	C(0x80, 0x80, 0xff),		/* light blue */
	C(0x80, 0xff, 0x80),		/* light green */
	C(0x80, 0xff, 0xff),		/* light cyan */
	C(0xff, 0x80, 0x80),		/* light red */
	C(0xff, 0x80, 0xff),		/* light magenta */
	C(0xff, 0xff, 0x80),		/* yellow */
	C(0xff, 0xff, 0xff),		/* white */
};

#define	TODO	printf("%s: unimplemented\n", __func__)

static struct creator_softc creator_softc;

static int
creator_configure(int flags)
{
	struct upa_regs reg[FFB_NREG];
	struct creator_softc *sc;
	phandle_t chosen;
	phandle_t stdout;
	phandle_t child;
	char buf[32];
	int i;

	sc = &creator_softc;
	for (child = OF_child(OF_peer(0)); child != 0;
	    child = OF_peer(child)) {
		OF_getprop(child, "name", buf, sizeof(buf));
		if  (strcmp(buf, "SUNW,ffb") == 0 ||
		     strcmp(buf, "SUNW,afb") == 0)
			break;
	}
	if (child == 0)
		return (0);

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	if (child == stdout)
		sc->sc_console = 1;

	OF_getprop(child, "reg", reg, sizeof(reg));
	for (i = 0; i < FFB_NREG; i++) {
		sc->sc_bt[i] = &nexus_bustag;
		sc->sc_bh[i] = UPA_REG_PHYS(reg + i);
	}
	OF_getprop(child, "height", &sc->sc_height, sizeof(sc->sc_height));
	OF_getprop(child, "width", &sc->sc_width, sizeof(sc->sc_width));

	creator_init(0, &sc->sc_va, 0);

	return (0);
}

static int
creator_probe(int unit, video_adapter_t **adp, void *arg, int flags)
{
	TODO;
	return (0);
}

static u_char creator_mouse_pointer[64][8] __aligned(8) = {
	{ 0x00, 0x00, },	/* ............ */
	{ 0x80, 0x00, },	/* *........... */
	{ 0xc0, 0x00, },	/* **.......... */
	{ 0xe0, 0x00, },	/* ***......... */
	{ 0xf0, 0x00, },	/* ****........ */
	{ 0xf8, 0x00, },	/* *****....... */
	{ 0xfc, 0x00, },	/* ******...... */
	{ 0xfe, 0x00, },	/* *******..... */
	{ 0xff, 0x00, },	/* ********.... */
	{ 0xff, 0x80, },	/* *********... */
	{ 0xfc, 0xc0, },	/* ******..**.. */
	{ 0xdc, 0x00, },	/* **.***...... */
	{ 0x8e, 0x00, },	/* *...***..... */
	{ 0x0e, 0x00, },	/* ....***..... */
	{ 0x07, 0x00, },	/* .....***.... */
	{ 0x04, 0x00, },	/* .....*...... */
	{ 0x00, 0x00, },	/* ............ */
	{ 0x00, 0x00, },	/* ............ */
	{ 0x00, 0x00, },	/* ............ */
	{ 0x00, 0x00, },	/* ............ */
	{ 0x00, 0x00, },	/* ............ */
	{ 0x00, 0x00, },	/* ............ */
};

static int
creator_init(int unit, video_adapter_t *adp, int flags)
{
	struct creator_softc *sc;
	phandle_t options;
	video_info_t *vi;
	char buf[32];
	cell_t col;
	cell_t row;
	int i, j;

	sc = (struct creator_softc *)adp;
	vi = &adp->va_info;

	vid_init_struct(adp, "creator", -1, unit);

	options = OF_finddevice("/options");
	OF_getprop(options, "screen-#rows", buf, sizeof(buf));
	vi->vi_height = strtol(buf, NULL, 10);
	OF_getprop(options, "screen-#columns", buf, sizeof(buf));
	vi->vi_width = strtol(buf, NULL, 10);
	vi->vi_cwidth = 12;
	vi->vi_cheight = 22;

	sc->sc_font = gallant12x22_data;
	sc->sc_xmargin = (sc->sc_width - (vi->vi_width * vi->vi_cwidth)) / 2;
	sc->sc_ymargin = (sc->sc_height - (vi->vi_height * vi->vi_cheight)) / 2;

	sc->sc_bg_cache = -1;
	sc->sc_fg_cache = -1;

	creator_ras_wait(sc);
	sc->sc_fifo_cache = 0;
	creator_ras_fifo_wait(sc, 2);

	FFB_WRITE(sc, FFB_FBC, FFB_FBC_PPC, FBC_PPC_VCE_DIS |
	    FBC_PPC_TBE_OPAQUE | FBC_PPC_APE_DIS | FBC_PPC_CS_CONST);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FBC, FFB_FBC_WB_A | FFB_FBC_RB_A |
	    FFB_FBC_SB_BOTH | FFB_FBC_XE_OFF | FFB_FBC_RGBE_MASK);

	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, 0x8000);
	sc->sc_dac = (FFB_READ(sc, FFB_DAC, FFB_DAC_VALUE) >> 0x1c);

	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, 0x102);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 0xffffff);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 0x0);

	for (i = 0; i < 2; i++) {
		FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, i ? 0x0 : 0x80);
		for (j = 0; j < 64; j++) {
			FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2,
			    *(uint32_t *)(&creator_mouse_pointer[j][0]));
			FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 
			    *(uint32_t *)(&creator_mouse_pointer[j][4]));
		}
	}

	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, 0x100);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 0x0);

	if (sc->sc_console) {
		col = NULL;
		row = NULL;
		OF_interpret("stdout @ is my-self addr line# addr column# ",
		    2, &col, &row);
		if (col != NULL && row != NULL) {
			sc->sc_colp = (int *)(col + 4);
			sc->sc_rowp = (int *)(row + 4);
		}
	} else {
		creator_blank_display(&sc->sc_va, V_DISPLAY_ON);
	}

	creator_set_mode(&sc->sc_va, 0);

	vid_register(&sc->sc_va);

	return (0);
}

static int
creator_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	bcopy(&adp->va_info, info, sizeof(*info));
	return (0);
}

static int
creator_query_mode(video_adapter_t *adp, video_info_t *info)
{
	TODO;
	return (0);
}

static int
creator_set_mode(video_adapter_t *adp, int mode)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	creator_ras_fifo_wait(sc, 4);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_ROP, FBC_ROP_NEW);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_DRAWOP, FBC_DRAWOP_RECTANGLE);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_PMASK, 0xffffffff);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONTINC, 0x10000);

	creator_ras_setbg(sc, 0x0);
	creator_ras_setfg(sc, 0xffffff);

	creator_ras_fifo_wait(sc, 4);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BX, 0);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BY, 0);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BH, sc->sc_height);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BW, sc->sc_width);

	creator_ras_wait(sc);

	return (0);
}

static int
creator_save_font(video_adapter_t *adp, int page, int size, u_char *data,
    int c, int count)
{
	TODO;
	return (0);
}

static int
creator_load_font(video_adapter_t *adp, int page, int size, u_char *data,
    int c, int count)
{
	TODO;
	return (0);
}

static int
creator_show_font(video_adapter_t *adp, int page)
{
	TODO;
	return (0);
}

static int
creator_save_palette(video_adapter_t *adp, u_char *palette)
{
	/* TODO; */
	return (0);
}

static int
creator_load_palette(video_adapter_t *adp, u_char *palette)
{
	/* TODO; */
	return (0);
}

static int
creator_set_border(video_adapter_t *adp, int border)
{
	/* TODO; */
	return (0);
}

static int
creator_save_state(video_adapter_t *adp, void *p, size_t size)
{
	TODO;
	return (0);
}

static int
creator_load_state(video_adapter_t *adp, void *p)
{
	TODO;
	return (0);
}

static int
creator_set_win_org(video_adapter_t *adp, off_t offset)
{
	TODO;
	return (0);
}

static int
creator_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	if (sc->sc_colp != NULL && sc->sc_rowp != NULL) {
		*col = *sc->sc_colp;
		*row = *sc->sc_rowp;
	} else {
		*col = 0;
		*row = 0;
	}
	return (0);
}

static int
creator_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	if (sc->sc_colp != NULL && sc->sc_rowp != NULL) {
		*sc->sc_colp = col;
		*sc->sc_rowp = row;
	}
	return (0);
}

static int
creator_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{
	return (0);
}

static int
creator_blank_display(video_adapter_t *adp, int mode)
{
	struct creator_softc *sc;
	int v;

	sc = (struct creator_softc *)adp;
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, 0x6000);
	v = FFB_READ(sc, FFB_DAC, FFB_DAC_VALUE);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, 0x6000);
	if (mode == V_DISPLAY_ON)
		v |= 0x1;
	else
		v &= ~0x1;
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE, v);
	return (0);
}

static int
creator_mmap(video_adapter_t *adp, vm_offset_t offset, vm_paddr_t *paddr,
    int prot)
{
	TODO;
	return (0);
}

static int
creator_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{
	TODO;
	return (0);
}

static int
creator_clear(video_adapter_t *adp)
{
	TODO;
	return (0);
}

static int
creator_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	TODO;
	return (0);
}

static int
creator_bitblt(video_adapter_t *adp, ...)
{
	TODO;
	return (0);
}

static int
creator_diag(video_adapter_t *adp, int level)
{
	TODO;
	return (0);
}

static int
creator_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	TODO;
	return (0);
}

static int
creator_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	TODO;
	return (0);
}

static int
creator_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{
	TODO;
	return (0);
}

static int
creator_putp(video_adapter_t *adp, vm_offset_t off, u_int32_t p, u_int32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{
	TODO;
	return (0);
}

static int
creator_putc(video_adapter_t *adp, vm_offset_t off, u_int8_t c, u_int8_t a)
{
	struct creator_softc *sc;
	uint16_t *p;
	int row;
	int col;
	int i;

	sc = (struct creator_softc *)adp;
	row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
	col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	p = (uint16_t *)sc->sc_font + (c * adp->va_info.vi_cheight);
	creator_ras_setfg(sc, cmap[a & 0xf]);
	creator_ras_setbg(sc, cmap[(a >> 4) & 0xf]);
	creator_ras_fifo_wait(sc, 2 + adp->va_info.vi_cheight);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONTXY,
	    ((row + sc->sc_ymargin) << 16) | (col + sc->sc_xmargin));
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONTW, adp->va_info.vi_cwidth);
	for (i = 0; i < adp->va_info.vi_cheight; i++) {
		FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONT, *p++ << 16);
	}
	return (0);
}

static int
creator_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		(*vidsw[adp->va_index]->putc)(adp, off + i, s[i] & 0xff,
		    (s[i] & 0xff00) >> 8);
	}
	return (0);
}

static int
creator_putm(video_adapter_t *adp, int x, int y, u_int8_t *pixel_image,
    u_int32_t pixel_mask, int size)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, 0x104);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2,
	    ((y + sc->sc_ymargin) << 16) | (x + sc->sc_xmargin));
	return (0);
}

static void
creator_ras_fifo_wait(struct creator_softc *sc, int n)
{
	int cache;

	cache = sc->sc_fifo_cache;
	while (cache < n) {
		cache = (FFB_READ(sc, FFB_FBC, FFB_FBC_UCSR) &
		    FBC_UCSR_FIFO_MASK) - 8;
	}
	sc->sc_fifo_cache = cache - n;
}

static void
creator_ras_setbg(struct creator_softc *sc, int bg)
{

	if (bg == sc->sc_bg_cache)
		return;
	sc->sc_bg_cache = bg;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BG, bg);
	creator_ras_wait(sc);
}

static void
creator_ras_setfg(struct creator_softc *sc, int fg)
{

	if (fg == sc->sc_fg_cache)
		return;
	sc->sc_fg_cache = fg;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FG, fg);
	creator_ras_wait(sc);
}

static void
creator_ras_wait(struct creator_softc *sc)
{
	int ucsr;
	int r;

	for (;;) {
		ucsr = FFB_READ(sc, FFB_FBC, FFB_FBC_UCSR);
		if ((ucsr & (FBC_UCSR_FB_BUSY|FBC_UCSR_RP_BUSY)) == 0)
			break;
		r = ucsr & (FBC_UCSR_READ_ERR | FBC_UCSR_FIFO_OVFL);
		if (r != 0) {
			FFB_WRITE(sc, FFB_FBC, FFB_FBC_UCSR, r);
		}
	}
}
