/*-
 * Copyright (C) 2012 Margarida Gouveia
 * Copyright (c) 2003 Peter Grehan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <powerpc/wii/wii_fbreg.h>
#include <powerpc/wii/wii_fbvar.h>

/*
 * Driver for the Nintendo Wii's framebuffer. Based on Linux's gcnfb.c.
 */

/*
 * Syscons glue.
 */
static int	wiifb_scprobe(device_t);
static int	wiifb_scattach(device_t);

static device_method_t wiifb_sc_methods[] = {
	DEVMETHOD(device_probe,		wiifb_scprobe),
	DEVMETHOD(device_attach,	wiifb_scattach),

	DEVMETHOD_END
};

static driver_t wiifb_sc_driver = {
	"wiifb",
	wiifb_sc_methods,
	sizeof(sc_softc_t),
};

static devclass_t sc_devclass;

DRIVER_MODULE(sc, wiibus, wiifb_sc_driver, sc_devclass, 0, 0);

static int
wiifb_scprobe(device_t dev)
{
	int error;

	device_set_desc(dev, "Nintendo Wii frambuffer");

	error = sc_probe_unit(device_get_unit(dev),
	    device_get_flags(dev) | SC_AUTODETECT_KBD);
	if (error != 0)
		return (error);

	/* This is a fake device, so make sure we added it ourselves */
	return (BUS_PROBE_NOWILDCARD);
}

static int
wiifb_scattach(device_t dev)
{

	return (sc_attach_unit(device_get_unit(dev),
	    device_get_flags(dev) | SC_AUTODETECT_KBD));
}

/*
 * Video driver routines and glue.
 */
static void			wiifb_reset_video(struct wiifb_softc *);
static void			wiifb_enable_interrupts(struct wiifb_softc *);
static void			wiifb_configure_tv_mode(struct wiifb_softc *);
static void			wiifb_setup_framebuffer(struct wiifb_softc *);
static int			wiifb_configure(int);
static vi_probe_t		wiifb_probe;
static vi_init_t		wiifb_init;
static vi_get_info_t		wiifb_get_info;
static vi_query_mode_t		wiifb_query_mode;
static vi_set_mode_t		wiifb_set_mode;
static vi_save_font_t		wiifb_save_font;
static vi_load_font_t		wiifb_load_font;
static vi_show_font_t		wiifb_show_font;
static vi_save_palette_t	wiifb_save_palette;
static vi_load_palette_t	wiifb_load_palette;
static vi_set_border_t		wiifb_set_border;
static vi_save_state_t		wiifb_save_state;
static vi_load_state_t		wiifb_load_state;
static vi_set_win_org_t		wiifb_set_win_org;
static vi_read_hw_cursor_t	wiifb_read_hw_cursor;
static vi_set_hw_cursor_t	wiifb_set_hw_cursor;
static vi_set_hw_cursor_shape_t	wiifb_set_hw_cursor_shape;
static vi_blank_display_t	wiifb_blank_display;
static vi_mmap_t		wiifb_mmap;
static vi_ioctl_t		wiifb_ioctl;
static vi_clear_t		wiifb_clear;
static vi_fill_rect_t		wiifb_fill_rect;
static vi_bitblt_t		wiifb_bitblt;
static vi_diag_t		wiifb_diag;
static vi_save_cursor_palette_t	wiifb_save_cursor_palette;
static vi_load_cursor_palette_t	wiifb_load_cursor_palette;
static vi_copy_t		wiifb_copy;
static vi_putp_t		wiifb_putp;
static vi_putc_t		wiifb_putc;
static vi_puts_t		wiifb_puts;
static vi_putm_t		wiifb_putm;

static video_switch_t wiifbvidsw = {
	.probe			= wiifb_probe,
	.init			= wiifb_init,
	.get_info		= wiifb_get_info,
	.query_mode		= wiifb_query_mode,
	.set_mode		= wiifb_set_mode,
	.save_font		= wiifb_save_font,
	.load_font		= wiifb_load_font,
	.show_font		= wiifb_show_font,
	.save_palette		= wiifb_save_palette,
	.load_palette		= wiifb_load_palette,
	.set_border		= wiifb_set_border,
	.save_state		= wiifb_save_state,
	.load_state		= wiifb_load_state,
	.set_win_org		= wiifb_set_win_org,
	.read_hw_cursor		= wiifb_read_hw_cursor,
	.set_hw_cursor		= wiifb_set_hw_cursor,
	.set_hw_cursor_shape	= wiifb_set_hw_cursor_shape,
	.blank_display		= wiifb_blank_display,
	.mmap			= wiifb_mmap,
	.ioctl			= wiifb_ioctl,
	.clear			= wiifb_clear,
	.fill_rect		= wiifb_fill_rect,
	.bitblt			= wiifb_bitblt,
	.diag			= wiifb_diag,
	.save_cursor_palette	= wiifb_save_cursor_palette,
	.load_cursor_palette	= wiifb_load_cursor_palette,
	.copy			= wiifb_copy,
	.putp			= wiifb_putp,
	.putc			= wiifb_putc,
	.puts			= wiifb_puts,
	.putm			= wiifb_putm,
};

VIDEO_DRIVER(wiifb, wiifbvidsw, wiifb_configure);

extern sc_rndr_sw_t txtrndrsw;
RENDERER(wiifb, 0, txtrndrsw, gfb_set);
RENDERER_MODULE(wiifb, gfb_set);

static struct wiifb_softc wiifb_softc;
static uint16_t wiifb_static_window[ROW*COL];
extern u_char dflt_font_8[];

/*
 * Map the syscons colors to YUY2 (Y'UV422).
 * Some colours are an approximation.
 *
 * The Wii has a 16 bit pixel, so each 32 bit DWORD encodes
 * two pixels.  The upper 16 bits is for pixel 0 (left hand pixel
 * in a pair), the lower 16 bits is for pixel 1. 
 *
 * For now, we're going to ignore that entirely and just use the
 * lower 16 bits for each pixel. We'll take the upper value into
 * account later.
 */
static uint32_t wiifb_cmap[16] = {
	0x00800080,	/* Black */
	0x1dff1d6b,	/* Blue */
	0x4b554b4a,	/* Green */
	0x80808080,	/* Cyan */
	0x4c544cff,	/* Red */
	0x3aaa34b5,	/* Magenta */
	0x7140718a,	/* Brown */
	0xff80ff80,	/* White */
	0x80808080,	/* Gray */
	0xc399c36a,	/* Bright Blue */
	0xd076d074,	/* Bright Green */
	0x80808080,	/* Bright Cyan */
	0x4c544cff,	/* Bright Red */
	0x3aaa34b5,	/* Bright Magenta */
	0xe100e194,	/* Bright Yellow */
	0xff80ff80	/* Bright White */
};

static struct wiifb_mode_desc wiifb_modes[] = {
	[WIIFB_MODE_NTSC_480i] = {
		"NTSC 480i",
		640, 480,
		525,
		WIIFB_MODE_FLAG_INTERLACED,
	},
	[WIIFB_MODE_NTSC_480p] = {
		"NTSC 480p",
		640, 480,
		525,
		WIIFB_MODE_FLAG_PROGRESSIVE,
	},
	[WIIFB_MODE_PAL_576i] = {
		"PAL 576i (50Hz)",
		640, 574,
		625,
		WIIFB_MODE_FLAG_INTERLACED,
	},
	[WIIFB_MODE_PAL_480i] = {
		"PAL 480i (60Hz)",
		640, 480,
		525,
		WIIFB_MODE_FLAG_INTERLACED,
	},
	[WIIFB_MODE_PAL_480p] = {
		"PAL 480p",
		640, 480,
		525,
		WIIFB_MODE_FLAG_PROGRESSIVE,
	},
};

static const uint32_t wiifb_filter_coeft[] = {
	0x1ae771f0, 0x0db4a574, 0x00c1188e, 0xc4c0cbe2, 0xfcecdecf,
	0x13130f08, 0x00080c0f
};

static __inline int
wiifb_background(uint8_t attr)
{

	return (attr >> 4);
}

static __inline int
wiifb_foreground(uint8_t attr)
{

	return (attr & 0x0f);
}

static void
wiifb_reset_video(struct wiifb_softc *sc)
{
	struct wiifb_dispcfg dc;

	wiifb_dispcfg_read(sc, &dc);
	dc.dc_reset = 1;
	wiifb_dispcfg_write(sc, &dc);
	dc.dc_reset = 0;
	wiifb_dispcfg_write(sc, &dc);
}

static void
wiifb_enable_interrupts(struct wiifb_softc *sc)
{
	struct wiifb_dispint di;

#ifdef notyet
	/*
	 * Display Interrupt 0
	 */
	di.di_htiming = 1;
	di.di_vtiming = 1;
	di.di_enable = 1;
	di.di_irq    = 1;
	wiifb_dispint_write(sc, 0, &di);

	/*
	 * Display Interrupt 1
	 */
	di.di_htiming = sc->sc_format == WIIFB_FORMAT_PAL ? 433 : 430;
	di.di_vtiming = sc->sc_mode->fd_lines;
	di.di_enable = 1;
	di.di_irq    = 1;
	if (sc->sc_mode->fd_flags & WIIFB_MODE_FLAG_INTERLACED)
		di.di_vtiming /= 2;
	wiifb_dispint_write(sc, 1, &di);

	/*
	 * Display Interrupts 2 and 3 are not used.
	 */
	memset(&di, 0, sizeof(di));
	wiifb_dispint_write(sc, 2, &di);
	wiifb_dispint_write(sc, 3, &di);
#else
	memset(&di, 0, sizeof(di));
	wiifb_dispint_write(sc, 0, &di);
	wiifb_dispint_write(sc, 1, &di);
	wiifb_dispint_write(sc, 2, &di);
	wiifb_dispint_write(sc, 3, &di);
#endif
}

/*
 * Reference gcnfb.c for an in depth explanation.
 * XXX only works with NTSC.
 */
static void
wiifb_configure_tv_mode(struct wiifb_softc *sc)
{
	struct wiifb_vtiming vt;
	struct wiifb_hscaling hs;
	struct wiifb_htiming0 ht0;
	struct wiifb_htiming1 ht1;
	struct wiifb_vtimingodd vto;
	struct wiifb_vtimingeven vte;
	struct wiifb_burstblankodd bbo;
	struct wiifb_burstblankeven bbe;
	struct wiifb_picconf pc;
	struct wiifb_mode_desc *mode = sc->sc_mode;
	unsigned int height = mode->fd_height;
	unsigned int width = mode->fd_width;
	unsigned int eqpulse, interlacebias, shift;
	const unsigned int maxwidth = 714;
	unsigned int hblanking = maxwidth - width;
	unsigned int hmargin = hblanking / 2;
	unsigned int A = 20 + hmargin, C = 60 + hblanking - hmargin;
	unsigned int maxheight = 484;
	unsigned int P = 2 * (20 - 10 + 1);
	unsigned int Q = 1;
	unsigned int vblanking = maxheight - height;
	unsigned int vmargin = vblanking / 2;
	unsigned int prb = vmargin;
	unsigned int psb = vblanking - vmargin;
	int i;

	/*
	 * Vertical timing.
	 */
	if (mode->fd_flags & WIIFB_MODE_FLAG_INTERLACED) {
		vt.vt_actvideo = height / 2;
		interlacebias = 1;
		shift = 0;
	} else {
		vt.vt_actvideo = height;
		interlacebias = 0;
		shift = 1;
	}
	/* Lines of equalization */
	if (mode->fd_lines == 625)
		eqpulse = 2 * 2.5;
	else
		eqpulse = 2 * 3;
	vt.vt_eqpulse = eqpulse << shift;
	wiifb_vtiming_write(sc, &vt);

	/*
	 * Horizontal timings.
	 */
	ht0.ht0_hlinew = 858 / 2;
	ht1.ht1_hsyncw = 64;
	ht0.ht0_hcolourstart = 71;
	ht0.ht0_hcolourend = 71 + 34;
	ht1.ht1_hblankstart = (858 / 2) - A;
	ht1.ht1_hblankend = 64 + C;
	wiifb_htiming0_write(sc, &ht0);
	wiifb_htiming1_write(sc, &ht1);

	/*
	 * Vertical timing odd/even.
	 */
	if (vmargin & 1) {
		vto.vto_preb = (P + interlacebias + prb) << shift;
		vto.vto_postb = (Q - interlacebias + psb) << shift;
		vte.vte_preb = (P + prb) << shift;
		vte.vte_postb = (Q - psb) << shift;
	} else {
		/* XXX if this isn't 0, it doesn't work? */
		prb = 0;
		psb = 0;
		vte.vte_preb = (P + interlacebias + prb) << shift;
		vte.vte_postb = (Q - interlacebias + psb) << shift;
		vto.vto_preb = (P + prb) << shift;
		vto.vto_postb = (Q - psb) << shift;
	}
	wiifb_vtimingodd_write(sc, &vto);
	wiifb_vtimingeven_write(sc, &vte);

	/*
	 * Burst blanking odd/even interval.
	 */
	bbo.bbo_bs1 = 2 * (18 - 7 + 1);
	bbe.bbe_bs2 = bbo.bbo_bs3 = bbe.bbe_bs4 = bbo.bbo_bs1;
	bbo.bbo_be1 = 2 * (525 - 7 + 1);
	bbe.bbe_be2 = bbo.bbo_be3 = bbe.bbe_be4 = bbo.bbo_be1;
	wiifb_burstblankodd_write(sc, &bbo);
	wiifb_burstblankeven_write(sc, &bbe);

	/*
	 * Picture configuration.
	 */ 
	pc.pc_strides = (mode->fd_width * 2) / 32;
	if (mode->fd_flags & WIIFB_MODE_FLAG_INTERLACED)
		pc.pc_strides *= 2;
	pc.pc_reads = (mode->fd_width * 2) / 32;
	wiifb_picconf_write(sc, &pc);

	/*
	 * Horizontal scaling disabled.
	 */
	hs.hs_enable = 0;
	hs.hs_step = 256;
	wiifb_hscaling_write(sc, &hs);

	/*
	 * Filter coeficient table.
	 */
	for (i = 0; i < 7; i++)
		wiifb_filtcoeft_write(sc, i, wiifb_filter_coeft[i]);

	/*
	 * Anti alias.
	 */
	wiifb_antialias_write(sc, 0x00ff0000);

	/*
	 * Video clock.
	 */
	wiifb_videoclk_write(sc, 
	    mode->fd_flags & WIIFB_MODE_FLAG_INTERLACED ? 0 : 1);

	/*
	 * Disable horizontal scaling width.
	 */
	wiifb_hscalingw_write(sc, mode->fd_width);

	/*
	 * DEBUG mode borders. Not used.
	 */
	wiifb_hborderend_write(sc, 0);
	wiifb_hborderstart_write(sc, 0);

	/*
	 * XXX unknown registers.
	 */
	wiifb_unknown1_write(sc, 0x00ff);
	wiifb_unknown2_write(sc, 0x00ff00ff);
	wiifb_unknown3_write(sc, 0x00ff00ff);
}

static void
wiifb_setup_framebuffer(struct wiifb_softc *sc)
{
	intptr_t addr = sc->sc_fb_addr;
	struct wiifb_topfieldbasel tfbl;
	struct wiifb_bottomfieldbasel bfbl;
	struct wiifb_topfieldbaser tfbr;
	struct wiifb_bottomfieldbaser bfbr;

	tfbl.tfbl_fbaddr     = addr >> 5;
	tfbl.tfbl_xoffset    = (addr / 2) & 0xf;
	tfbl.tfbl_pageoffbit = 1;
	wiifb_topfieldbasel_write(sc, &tfbl);

	if (sc->sc_mode->fd_flags & WIIFB_MODE_FLAG_INTERLACED)
		addr += sc->sc_mode->fd_width * 2;
	bfbl.bfbl_fbaddr     = addr >> 5;
	bfbl.bfbl_xoffset    = (addr / 2) & 0xf;
	bfbl.bfbl_pageoffbit = 1;
	wiifb_bottomfieldbasel_write(sc, &bfbl);

	/*
	 * Only used used for 3D.
	 */
	memset(&tfbr, 0, sizeof(tfbr));
	memset(&bfbr, 0, sizeof(bfbr));
	wiifb_topfieldbaser_write(sc, &tfbr);
	wiifb_bottomfieldbaser_write(sc, &bfbr);
}

static int
wiifb_configure(int flags)
{
	struct wiifb_softc *sc;
	struct wiifb_dispcfg dc;
	int progressive;

	sc = &wiifb_softc;
	if (sc->sc_initialized) {
		/* XXX We should instead use bus_space */
		sc->sc_fb_addr = (intptr_t)pmap_mapdev(WIIFB_FB_ADDR, WIIFB_FB_LEN);
		sc->sc_reg_addr = (intptr_t)pmap_mapdev(WIIFB_REG_ADDR, WIIFB_REG_LEN);
		return 0;
	}

	sc->sc_console = 1;

	sc->sc_fb_addr = WIIFB_FB_ADDR;
	sc->sc_fb_size = WIIFB_FB_LEN;

	sc->sc_reg_addr = WIIFB_REG_ADDR;
	sc->sc_reg_size = WIIFB_REG_LEN;

	wiifb_reset_video(sc);
	wiifb_dispcfg_read(sc, &dc);
	sc->sc_format = dc.dc_format;
	sc->sc_component = wiifb_component_enabled(sc);
	progressive = dc.dc_noninterlaced;
	switch (sc->sc_format) {
	case WIIFB_FORMAT_MPAL:
	case WIIFB_FORMAT_DEBUG:
	case WIIFB_FORMAT_NTSC:
		sc->sc_mode = progressive ?
		    &wiifb_modes[WIIFB_MODE_NTSC_480p] :
		    &wiifb_modes[WIIFB_MODE_NTSC_480i];
		break;
	case WIIFB_FORMAT_PAL:
		sc->sc_mode = progressive ?
		    &wiifb_modes[WIIFB_MODE_PAL_480p] :
		    &wiifb_modes[WIIFB_MODE_PAL_480i];
		break;
	}
	sc->sc_height = sc->sc_mode->fd_height;
	sc->sc_width = sc->sc_mode->fd_width;
	/* Usually we multiply by 4, but I think this looks better. */
	sc->sc_stride = sc->sc_width * 2;

	wiifb_init(0, &sc->sc_va, 0);

	sc->sc_initialized = 1;

	return (0);
}

static int
wiifb_probe(int unit, video_adapter_t **adp, void *arg, int flags)
{

	return (0);
}

static int
wiifb_init(int unit, video_adapter_t *adp, int flags)
{
	struct wiifb_softc *sc;
	video_info_t *vi;

	sc = (struct wiifb_softc *)adp;
	vi = &adp->va_info;

	vid_init_struct(adp, "wiifb", -1, unit);

	sc->sc_font = dflt_font_8;
	vi->vi_cheight = WIIFB_FONT_HEIGHT;
	vi->vi_width = sc->sc_width/8;
	vi->vi_height = sc->sc_height/vi->vi_cheight;
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

	adp->va_window = (vm_offset_t) wiifb_static_window;
	/* XXX no colour support */
	adp->va_flags |= V_ADP_FONT | /*V_ADP_COLOR |*/ V_ADP_MODECHANGE;

	vid_register(&sc->sc_va);

	wiifb_configure_tv_mode(sc);
	wiifb_setup_framebuffer(sc);
	wiifb_enable_interrupts(sc);
	wiifb_clear(adp);

	return (0);
}

static int
wiifb_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{

	bcopy(&adp->va_info, info, sizeof(*info));
	return (0);
}

static int
wiifb_query_mode(video_adapter_t *adp, video_info_t *info)
{

	return (0);
}

static int
wiifb_set_mode(video_adapter_t *adp, int mode)
{

	return (0);
}

static int
wiifb_save_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{

	return (0);
}

static int
wiifb_load_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	struct wiifb_softc *sc = (struct wiifb_softc *)adp;

	sc->sc_font = data;

	return (0);
}

static int
wiifb_show_font(video_adapter_t *adp, int page)
{

	return (0);
}

static int
wiifb_save_palette(video_adapter_t *adp, u_char *palette)
{

	return (0);
}

static int
wiifb_load_palette(video_adapter_t *adp, u_char *palette)
{

	return (0);
}

static int
wiifb_set_border(video_adapter_t *adp, int border)
{

	return (wiifb_blank_display(adp, border));
}

static int
wiifb_save_state(video_adapter_t *adp, void *p, size_t size)
{

	return (0);
}

static int
wiifb_load_state(video_adapter_t *adp, void *p)
{

	return (0);
}

static int
wiifb_set_win_org(video_adapter_t *adp, off_t offset)
{

	return (0);
}

static int
wiifb_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{

	*col = *row = 0;

	return (0);
}

static int
wiifb_set_hw_cursor(video_adapter_t *adp, int col, int row)
{

	return (0);
}

static int
wiifb_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{

	return (0);
}

static int
wiifb_blank_display(video_adapter_t *adp, int mode)
{
	struct wiifb_softc *sc = (struct wiifb_softc *)adp;
	uint32_t *p;

	for (p = (uint32_t *)sc->sc_fb_addr;
	    p < (uint32_t *)(sc->sc_fb_addr + sc->sc_fb_size);
	    p++)
		*p = wiifb_cmap[wiifb_background(SC_NORM_ATTR)];

	return (0);
}

static int
wiifb_mmap(video_adapter_t *adp, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct wiifb_softc *sc;

	sc = (struct wiifb_softc *)adp;

	/*
	 * This might be a legacy VGA mem request: if so, just point it at the
	 * framebuffer, since it shouldn't be touched
	 */
	if (offset < sc->sc_stride*sc->sc_height) {
		*paddr = sc->sc_fb_addr + offset;
		return (0);
	}

	return (EINVAL);
}

static int
wiifb_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{

	return (0);
}

static int
wiifb_clear(video_adapter_t *adp)
{

	return (wiifb_blank_display(adp, 0));
}

static int
wiifb_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{

	return (0);
}

static int
wiifb_bitblt(video_adapter_t *adp, ...)
{

	return (0);
}

static int
wiifb_diag(video_adapter_t *adp, int level)
{

	return (0);
}

static int
wiifb_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{

	return (0);
}

static int
wiifb_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{

	return (0);
}

static int
wiifb_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{

	return (0);
}

static int
wiifb_putp(video_adapter_t *adp, vm_offset_t off, uint32_t p, uint32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{

	return (0);
}

static int
wiifb_putc(video_adapter_t *adp, vm_offset_t off, uint8_t c, uint8_t a)
{
	struct wiifb_softc *sc;
	int row;
	int col;
	int i, j, k;
	uint32_t *addr;
	u_char *p;
	uint32_t fg, bg;
	unsigned long pixel[2];

	sc = (struct wiifb_softc *)adp;
	row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
	col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth / 2;
	p = sc->sc_font + c*WIIFB_FONT_HEIGHT;
	addr = (uint32_t *)sc->sc_fb_addr
	    + (row + sc->sc_ymargin)*(sc->sc_stride/4)
	    + col + sc->sc_xmargin;

	bg = wiifb_cmap[wiifb_background(a)];
	fg = wiifb_cmap[wiifb_foreground(a)];

	for (i = 0; i < WIIFB_FONT_HEIGHT; i++) {
		for (j = 0, k = 7; j < 4; j++, k--) {
			if ((p[i] & (1 << k)) == 0)
				pixel[0] = bg;
			else
				pixel[0] = fg;
			k--;	
			if ((p[i] & (1 << k)) == 0)
				pixel[1] = bg;
			else
				pixel[1] = fg;

			addr[j] = (pixel[0] & 0xffff00ff) |
			          (pixel[1] & 0x0000ff00);
		}
		addr += (sc->sc_stride/4);
	}

        return (0);
}

static int
wiifb_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i;

	for (i = 0; i < len; i++) 
		wiifb_putc(adp, off + i, s[i] & 0xff, (s[i] & 0xff00) >> 8);

	return (0);
}

static int
wiifb_putm(video_adapter_t *adp, int x, int y, uint8_t *pixel_image,
    uint32_t pixel_mask, int size, int width)
{
	
	return (0);
}
