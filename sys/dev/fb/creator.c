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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/consio.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/ofw_upa.h>
#include <machine/sc_machdep.h>

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

static void creator_cursor_enable(struct creator_softc *sc, int onoff);
static void creator_cursor_install(struct creator_softc *sc);

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
static const int cmap[] = {
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

static const u_char creator_mouse_pointer[64][8] __aligned(8) = {
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

static struct creator_softc creator_softc;

static inline void creator_ras_fifo_wait(struct creator_softc *sc, int n);
static inline void creator_ras_setfontinc(struct creator_softc *sc, int fontinc);
static inline void creator_ras_setfontw(struct creator_softc *sc, int fontw);
static inline void creator_ras_setbg(struct creator_softc *sc, int bg);
static inline void creator_ras_setfg(struct creator_softc *sc, int fg);
static inline void creator_ras_setpmask(struct creator_softc *sc, int pmask);
static inline void creator_ras_wait(struct creator_softc *sc);

static inline void
creator_ras_wait(struct creator_softc *sc)
{
	int ucsr;
	int r;

	for (;;) {
		ucsr = FFB_READ(sc, FFB_FBC, FFB_FBC_UCSR);
		if ((ucsr & (FBC_UCSR_FB_BUSY | FBC_UCSR_RP_BUSY)) == 0)
			break;
		r = ucsr & (FBC_UCSR_READ_ERR | FBC_UCSR_FIFO_OVFL);
		if (r != 0)
			FFB_WRITE(sc, FFB_FBC, FFB_FBC_UCSR, r);
	}
}

static inline void
creator_ras_fifo_wait(struct creator_softc *sc, int n)
{
	int cache;

	cache = sc->sc_fifo_cache;
	while (cache < n)
		cache = (FFB_READ(sc, FFB_FBC, FFB_FBC_UCSR) &
		    FBC_UCSR_FIFO_MASK) - 8;
	sc->sc_fifo_cache = cache - n;
}

static inline void
creator_ras_setfontinc(struct creator_softc *sc, int fontinc)
{

	if (fontinc == sc->sc_fontinc_cache)
		return;
	sc->sc_fontinc_cache = fontinc;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONTINC, fontinc);
	creator_ras_wait(sc);
}

static inline void
creator_ras_setfontw(struct creator_softc *sc, int fontw)
{

	if (fontw == sc->sc_fontw_cache)
		return;
	sc->sc_fontw_cache = fontw;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONTW, fontw);
	creator_ras_wait(sc);
}

static inline void
creator_ras_setbg(struct creator_softc *sc, int bg)
{

	if (bg == sc->sc_bg_cache)
		return;
	sc->sc_bg_cache = bg;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BG, bg);
	creator_ras_wait(sc);
}

static inline void
creator_ras_setfg(struct creator_softc *sc, int fg)
{

	if (fg == sc->sc_fg_cache)
		return;
	sc->sc_fg_cache = fg;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FG, fg);
	creator_ras_wait(sc);
}

static inline void
creator_ras_setpmask(struct creator_softc *sc, int pmask)
{

	if (pmask == sc->sc_pmask_cache)
		return;
	sc->sc_pmask_cache = pmask;
	creator_ras_fifo_wait(sc, 1);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_PMASK, pmask);
	creator_ras_wait(sc);
}

static int
creator_configure(int flags)
{
	struct upa_regs reg[FFB_NREG];
	struct creator_softc *sc;
	phandle_t chosen;
	phandle_t output;
	ihandle_t stdout;
	char buf[32];
	int i;

	/*
	 * For the high-level console probing return the number of
	 * registered adapters.
	 */
	if (!(flags & VIO_PROBE_ONLY)) {
		for (i = 0; vid_find_adapter(CREATOR_DRIVER_NAME, i) >= 0; i++)
			;
		return (i);
	}

	/* Low-level console probing and initialization. */

	sc = &creator_softc;
	if (sc->sc_va.va_flags & V_ADP_REGISTERED)
		goto found;

	if ((chosen = OF_finddevice("/chosen")) == -1)
		return (0);
	if (OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1)
		return (0);
	if ((output = OF_instance_to_package(stdout)) == -1)
		return (0);
	if (OF_getprop(output, "name", buf, sizeof(buf)) == -1)
		return (0);
	if (strcmp(buf, "SUNW,ffb") == 0 || strcmp(buf, "SUNW,afb") == 0) {
		sc->sc_flags = CREATOR_CONSOLE;
		if (strcmp(buf, "SUNW,afb") == 0)
			sc->sc_flags |= CREATOR_AFB;
		sc->sc_node = output;
	} else
		return (0);

	if (OF_getprop(output, "reg", reg, sizeof(reg)) == -1)
		return (0);
	for (i = 0; i < FFB_NREG; i++) {
		sc->sc_bt[i] = &nexus_bustag;
		sc->sc_bh[i] = UPA_REG_PHYS(reg + i);
	}

	if (creator_init(0, &sc->sc_va, 0) < 0)
		return (0);

 found:
	/* Return number of found adapters. */
	return (1);
}

static int
creator_probe(int unit, video_adapter_t **adpp, void *arg, int flags)
{

	return (0);
}

static int
creator_init(int unit, video_adapter_t *adp, int flags)
{
	struct creator_softc *sc;
	phandle_t options;
	video_info_t *vi;
	char buf[32];

	sc = (struct creator_softc *)adp;
	vi = &adp->va_info;

	vid_init_struct(adp, CREATOR_DRIVER_NAME, -1, unit);

 	if (OF_getprop(sc->sc_node, "height", &sc->sc_height,
	    sizeof(sc->sc_height)) == -1)
		return (ENXIO);
	if (OF_getprop(sc->sc_node, "width", &sc->sc_width,
	    sizeof(sc->sc_width)) == -1)
		return (ENXIO);
	if ((options = OF_finddevice("/options")) == -1)
		return (ENXIO);
	if (OF_getprop(options, "screen-#rows", buf, sizeof(buf)) == -1)
		return (ENXIO);
	vi->vi_height = strtol(buf, NULL, 10);
	if (OF_getprop(options, "screen-#columns", buf, sizeof(buf)) == -1)
		return (ENXIO);
	vi->vi_width = strtol(buf, NULL, 10);
	vi->vi_cwidth = 12;
	vi->vi_cheight = 22;
	vi->vi_flags = V_INFO_COLOR;
	vi->vi_mem_model = V_INFO_MM_OTHER;

	sc->sc_font = gallant12x22_data;
	sc->sc_xmargin = (sc->sc_width - (vi->vi_width * vi->vi_cwidth)) / 2;
	sc->sc_ymargin = (sc->sc_height - (vi->vi_height * vi->vi_cheight)) / 2;

	creator_set_mode(adp, 0);

	if (!(sc->sc_flags & CREATOR_AFB)) {
		FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, FFB_DAC_CFG_DID);
		if (((FFB_READ(sc, FFB_DAC, FFB_DAC_VALUE) &
		    FFB_DAC_CFG_DID_PNUM) >> 12) != 0x236e) {
		    	sc->sc_flags |= CREATOR_PAC1;
			FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, FFB_DAC_CFG_UCTRL);
			if (((FFB_READ(sc, FFB_DAC, FFB_DAC_VALUE) &
			    FFB_DAC_UCTRL_MANREV) >> 8) <= 2)
				sc->sc_flags |= CREATOR_CURINV;
		}
	}

	creator_blank_display(adp, V_DISPLAY_ON);
	creator_clear(adp);

	/*
	 * Setting V_ADP_MODECHANGE serves as hack so creator_set_mode()
	 * (which will invalidate our caches and restore our settings) is
	 * called when the X server shuts down. Otherwise screen corruption
	 * happens most of the time.
	 */
	adp->va_flags |= V_ADP_COLOR | V_ADP_MODECHANGE | V_ADP_BORDER |
	    V_ADP_INITIALIZED;
	if (vid_register(adp) < 0)
		return (ENXIO);
	adp->va_flags |= V_ADP_REGISTERED;

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

	return (ENODEV);
}

static int
creator_set_mode(video_adapter_t *adp, int mode)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	sc->sc_bg_cache = -1;
	sc->sc_fg_cache = -1;
	sc->sc_fontinc_cache = -1;
	sc->sc_fontw_cache = -1;
	sc->sc_pmask_cache = -1;

	creator_ras_wait(sc);
	sc->sc_fifo_cache = 0;
	creator_ras_fifo_wait(sc, 2);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_PPC, FBC_PPC_VCE_DIS |
	    FBC_PPC_TBE_OPAQUE | FBC_PPC_APE_DIS | FBC_PPC_CS_CONST);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FBC, FFB_FBC_WB_A | FFB_FBC_RB_A |
	    FFB_FBC_SB_BOTH | FFB_FBC_XE_OFF | FFB_FBC_RGBE_MASK);
	return (0);
}

static int
creator_save_font(video_adapter_t *adp, int page, int size, u_char *data,
    int c, int count)
{

	return (ENODEV);
}

static int
creator_load_font(video_adapter_t *adp, int page, int size, u_char *data,
    int c, int count)
{

	return (ENODEV);
}

static int
creator_show_font(video_adapter_t *adp, int page)
{

	return (ENODEV);
}

static int
creator_save_palette(video_adapter_t *adp, u_char *palette)
{

	return (ENODEV);
}

static int
creator_load_palette(video_adapter_t *adp, u_char *palette)
{

	return (ENODEV);
}

static int
creator_set_border(video_adapter_t *adp, int border)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	creator_fill_rect(adp, border, 0, 0, sc->sc_width, sc->sc_ymargin);
	creator_fill_rect(adp, border, 0, sc->sc_height - sc->sc_ymargin,
	    sc->sc_width, sc->sc_ymargin);
	creator_fill_rect(adp, border, 0, 0, sc->sc_xmargin, sc->sc_height);
	creator_fill_rect(adp, border, sc->sc_width - sc->sc_xmargin, 0,
	    sc->sc_xmargin, sc->sc_height);
	return (0);
}

static int
creator_save_state(video_adapter_t *adp, void *p, size_t size)
{

	return (ENODEV);
}

static int
creator_load_state(video_adapter_t *adp, void *p)
{

	return (ENODEV);
}

static int
creator_set_win_org(video_adapter_t *adp, off_t offset)
{

	return (ENODEV);
}

static int
creator_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{

	*col = 0;
	*row = 0;
	return (0);
}

static int
creator_set_hw_cursor(video_adapter_t *adp, int col, int row)
{

	return (ENODEV);
}

static int
creator_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{

	return (ENODEV);
}

static int
creator_blank_display(video_adapter_t *adp, int mode)
{
	struct creator_softc *sc;
	uint32_t v;
	int i;

	sc = (struct creator_softc *)adp;
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, FFB_DAC_CFG_TGEN);
	v = FFB_READ(sc, FFB_DAC, FFB_DAC_VALUE);
	switch (mode) {
	case V_DISPLAY_ON:
		v &= ~(FFB_DAC_CFG_TGEN_VSD | FFB_DAC_CFG_TGEN_HSD);
		v |= FFB_DAC_CFG_TGEN_VIDE;
		break;
	case V_DISPLAY_BLANK:
		v |= (FFB_DAC_CFG_TGEN_VSD | FFB_DAC_CFG_TGEN_HSD);
		v &= ~FFB_DAC_CFG_TGEN_VIDE;
		break;
	case V_DISPLAY_STAND_BY:
		v &= ~FFB_DAC_CFG_TGEN_VSD;
		v &= ~FFB_DAC_CFG_TGEN_VIDE;
		break;
	case V_DISPLAY_SUSPEND:
		v |=  FFB_DAC_CFG_TGEN_VSD;
		v &= ~FFB_DAC_CFG_TGEN_HSD;
		v &= ~FFB_DAC_CFG_TGEN_VIDE;
		break;
	}
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, FFB_DAC_CFG_TGEN);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE, v);
	for (i = 0; i < 10; i++) {
		FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE, FFB_DAC_CFG_TGEN);
		(void)FFB_READ(sc, FFB_DAC, FFB_DAC_VALUE);
	}
	return (0);
}

static int
creator_mmap(video_adapter_t *adp, vm_offset_t offset, vm_paddr_t *paddr,
    int prot)
{

	return (EINVAL);
}

static int
creator_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{
	struct creator_softc *sc;
	struct fbcursor *fbc;
	struct fbtype *fb;

	sc = (struct creator_softc *)adp;
	switch (cmd) {
	case FBIOGTYPE:
		fb = (struct fbtype *)data;
		fb->fb_type = FBTYPE_CREATOR;
		fb->fb_height = sc->sc_height;
		fb->fb_width = sc->sc_width;
		fb->fb_depth = fb->fb_cmsize = fb->fb_size = 0;
		break;
	case FBIOSCURSOR:
		fbc = (struct fbcursor *)data;
		if (fbc->set & FB_CUR_SETCUR && fbc->enable == 0) {
			creator_cursor_enable(sc, 0);
			sc->sc_flags &= ~CREATOR_CUREN;
		} else
			return (ENODEV);
		break;
		break;
	default:
		return (fb_commonioctl(adp, cmd, data));
	}
	return (0);
}

static int
creator_clear(video_adapter_t *adp)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	creator_fill_rect(adp, (SC_NORM_ATTR >> 4) & 0xf, 0, 0, sc->sc_width,
	    sc->sc_height);
	return (0);
}

static int
creator_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	struct creator_softc *sc;

	sc = (struct creator_softc *)adp;
	creator_ras_setpmask(sc, 0xffffffff);
	creator_ras_fifo_wait(sc, 2);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_ROP, FBC_ROP_NEW);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_DRAWOP, FBC_DRAWOP_RECTANGLE);
	creator_ras_setfg(sc, cmap[val & 0xf]);
	/*
	 * Note that at least the Elite3D cards are sensitive to the order
	 * of operations here.
	 */
	creator_ras_fifo_wait(sc, 4);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BY, y);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BX, x);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BH, cy);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_BW, cx);
	creator_ras_wait(sc);
	return (0);
}

static int
creator_bitblt(video_adapter_t *adp, ...)
{

	return (ENODEV);
}

static int
creator_diag(video_adapter_t *adp, int level)
{
	video_info_t info;

	fb_dump_adp_info(adp->va_name, adp, level);
	creator_get_info(adp, 0, &info);
	fb_dump_mode_info(adp->va_name, adp, &info, level);
	return (0);
}

static int
creator_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{

	return (ENODEV);
}

static int
creator_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{

	return (ENODEV);
}

static int
creator_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{

	return (ENODEV);
}

static int
creator_putp(video_adapter_t *adp, vm_offset_t off, u_int32_t p, u_int32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{

	return (ENODEV);
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
	creator_ras_fifo_wait(sc, 1 + adp->va_info.vi_cheight);
	FFB_WRITE(sc, FFB_FBC, FFB_FBC_FONTXY,
	    ((row + sc->sc_ymargin) << 16) | (col + sc->sc_xmargin));
	creator_ras_setfontw(sc, adp->va_info.vi_cwidth);
	creator_ras_setfontinc(sc, 0x10000);
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
	if (!(sc->sc_flags & CREATOR_CUREN)) {
		creator_cursor_install(sc);
		creator_cursor_enable(sc, 1);
		sc->sc_flags |= CREATOR_CUREN;
	}
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, FFB_DAC_CUR_POS);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2,
	    ((y + sc->sc_ymargin) << 16) | (x + sc->sc_xmargin));
	return (0);
}

static void
creator_cursor_enable(struct creator_softc *sc, int onoff)
{
	int v;

	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, FFB_DAC_CUR_CTRL);
	if (sc->sc_flags & CREATOR_CURINV)
		v = onoff ? FFB_DAC_CUR_CTRL_P0 | FFB_DAC_CUR_CTRL_P1 : 0;
	else
		v = onoff ? 0 : FFB_DAC_CUR_CTRL_P0 | FFB_DAC_CUR_CTRL_P1;
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, v);
}

static void
creator_cursor_install(struct creator_softc *sc)
{
	int i, j;

	creator_cursor_enable(sc, 0);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, FFB_DAC_CUR_COLOR1);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 0xffffff);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 0x0);
	for (i = 0; i < 2; i++) {
		FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2,
		    i ? FFB_DAC_CUR_BITMAP_P0 : FFB_DAC_CUR_BITMAP_P1);
		for (j = 0; j < 64; j++) {
			FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2,
			    *(const uint32_t *)(&creator_mouse_pointer[j][0]));
			FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2, 
			    *(const uint32_t *)(&creator_mouse_pointer[j][4]));
		}
	}
}
