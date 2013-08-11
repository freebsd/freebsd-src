/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <sys/kdb.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fb/fbreg.h>
#include <dev/syscons/syscons.h>

#include <arm/broadcom/bcm2835/bcm2835_mbox.h>
#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

#include "mbox_if.h"

#define	BCMFB_FONT_HEIGHT	16

struct argb {
	uint8_t		a;
	uint8_t		r;
	uint8_t		g;
	uint8_t		b;
};

static struct argb bcmfb_palette[16] = {
	{0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0xaa},
	{0x00, 0x00, 0xaa, 0x00},
	{0x00, 0x00, 0xaa, 0xaa},
	{0x00, 0xaa, 0x00, 0x00},
	{0x00, 0xaa, 0x00, 0xaa},
	{0x00, 0xaa, 0x55, 0x00},
	{0x00, 0xaa, 0xaa, 0xaa},
	{0x00, 0x55, 0x55, 0x55},
	{0x00, 0x55, 0x55, 0xff},
	{0x00, 0x55, 0xff, 0x55},
	{0x00, 0x55, 0xff, 0xff},
	{0x00, 0xff, 0x55, 0x55},
	{0x00, 0xff, 0x55, 0xff},
	{0x00, 0xff, 0xff, 0x55},
	{0x00, 0xff, 0xff, 0xff}
};

/* mouse pointer from dev/syscons/scgfbrndr.c */
static u_char mouse_pointer[16] = {
        0x00, 0x40, 0x60, 0x70, 0x78, 0x7c, 0x7e, 0x68,
        0x0c, 0x0c, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00
};

#define FB_WIDTH		640
#define FB_HEIGHT		480
#define FB_DEPTH		24

struct bcm_fb_config {
	uint32_t	xres;
	uint32_t	yres;
	uint32_t	vxres;
	uint32_t	vyres;
	uint32_t	pitch;
	uint32_t	bpp;
	uint32_t	xoffset;
	uint32_t	yoffset;
	/* Filled by videocore */
	uint32_t	base;
	uint32_t	screen_size;
};

struct bcmsc_softc {
	device_t		dev;
	struct cdev *		cdev;
	struct mtx		mtx;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	struct bcm_fb_config*	fb_config;
	bus_addr_t		fb_config_phys;
	struct intr_config_hook	init_hook;

};

struct video_adapter_softc {
	/* Videoadpater part */
	video_adapter_t	va;
	int		console;

	intptr_t	fb_addr;
	intptr_t	fb_paddr;
	unsigned int	fb_size;

	unsigned int	height;
	unsigned int	width;
	unsigned int	depth;
	unsigned int	stride;

	unsigned int	xmargin;
	unsigned int	ymargin;

	unsigned char	*font;
	int		initialized;
};

static struct bcmsc_softc *bcmsc_softc;
static struct video_adapter_softc va_softc;

#define	bcm_fb_lock(_sc)	mtx_lock(&(_sc)->mtx)
#define	bcm_fb_unlock(_sc)	mtx_unlock(&(_sc)->mtx)
#define	bcm_fb_lock_assert(sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

static int bcm_fb_probe(device_t);
static int bcm_fb_attach(device_t);
static void bcm_fb_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err);
static void bcmfb_update_margins(video_adapter_t *adp);
static int bcmfb_configure(int);

static void
bcm_fb_init(void *arg)
{
	struct bcmsc_softc *sc = arg;
	struct video_adapter_softc *va_sc = &va_softc;
	int err;
	volatile struct bcm_fb_config*	fb_config = sc->fb_config;
	phandle_t node;
	pcell_t cell;
	device_t mbox;

	node = ofw_bus_get_node(sc->dev);

	fb_config->xres = 0;
	fb_config->yres = 0;
	fb_config->bpp = 0;

	if ((OF_getprop(node, "broadcom,width", &cell, sizeof(cell))) > 0)
		fb_config->xres = (int)fdt32_to_cpu(cell);
	if (fb_config->xres == 0)
		fb_config->xres = FB_WIDTH;

	if ((OF_getprop(node, "broadcom,height", &cell, sizeof(cell))) > 0)
		fb_config->yres = (uint32_t)fdt32_to_cpu(cell);
	if (fb_config->yres == 0)
		fb_config->yres = FB_HEIGHT;

	if ((OF_getprop(node, "broadcom,depth", &cell, sizeof(cell))) > 0)
		fb_config->bpp = (uint32_t)fdt32_to_cpu(cell);
	if (fb_config->bpp == 0)
		fb_config->bpp = FB_DEPTH;

	fb_config->vxres = 0;
	fb_config->vyres = 0;
	fb_config->xoffset = 0;
	fb_config->yoffset = 0;
	fb_config->base = 0;
	fb_config->pitch = 0;
	fb_config->screen_size = 0;

	bus_dmamap_sync(sc->dma_tag, sc->dma_map,
		BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	mbox = devclass_get_device(devclass_find("mbox"), 0);
	if (mbox) {
		MBOX_WRITE(mbox, BCM2835_MBOX_CHAN_FB, sc->fb_config_phys);
		MBOX_READ(mbox, BCM2835_MBOX_CHAN_FB, &err);
	}
	bus_dmamap_sync(sc->dma_tag, sc->dma_map,
		BUS_DMASYNC_POSTREAD);

	if (fb_config->base != 0) {
		device_printf(sc->dev, "%dx%d(%dx%d@%d,%d) %dbpp\n", 
			fb_config->xres, fb_config->yres,
			fb_config->vxres, fb_config->vyres,
			fb_config->xoffset, fb_config->yoffset,
			fb_config->bpp);


		device_printf(sc->dev, "pitch %d, base 0x%08x, screen_size %d\n", 
			fb_config->pitch, fb_config->base,
			fb_config->screen_size);

		va_sc->fb_addr = (intptr_t)pmap_mapdev(fb_config->base, fb_config->screen_size);
		va_sc->fb_paddr = fb_config->base;
		va_sc->fb_size = fb_config->screen_size;
		va_sc->depth = fb_config->bpp;
		va_sc->stride = fb_config->pitch;

		va_sc->width = fb_config->xres;
		va_sc->height = fb_config->yres;
		bcmfb_update_margins(&va_sc->va);
	}
	else {
		device_printf(sc->dev, "Failed to set framebuffer info\n");
		return;
	}

	config_intrhook_disestablish(&sc->init_hook);
}

static int
bcm_fb_probe(device_t dev)
{
	int error = 0;

	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-fb"))
		return (ENXIO);

	device_set_desc(dev, "BCM2835 framebuffer device");

	error = sc_probe_unit(device_get_unit(dev), 
	    device_get_flags(dev) | SC_AUTODETECT_KBD);
	if (error != 0)
		return (error);


	return (BUS_PROBE_DEFAULT);
}

static int
bcm_fb_attach(device_t dev)
{
	struct bcmsc_softc *sc = device_get_softc(dev);
	int dma_size = sizeof(struct bcm_fb_config);
	int err;

	if (bcmsc_softc)
		return (ENXIO);

	bcmsc_softc = sc;

	sc->dev = dev;
	mtx_init(&sc->mtx, "bcm2835fb", "fb", MTX_DEF);

	err = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),
	    PAGE_SIZE, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    dma_size, 1,		/* maxsize, nsegments */
	    dma_size, 0,		/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->dma_tag);

	err = bus_dmamem_alloc(sc->dma_tag, (void **)&sc->fb_config,
	    0, &sc->dma_map);
	if (err) {
		device_printf(dev, "cannot allocate framebuffer\n");
		goto fail;
	}

	err = bus_dmamap_load(sc->dma_tag, sc->dma_map, sc->fb_config,
	    dma_size, bcm_fb_dmamap_cb, &sc->fb_config_phys, BUS_DMA_NOWAIT);

	if (err) {
		device_printf(dev, "cannot load DMA map\n");
		goto fail;
	}

	err = (sc_attach_unit(device_get_unit(dev),
	    device_get_flags(dev) | SC_AUTODETECT_KBD));

	if (err) {
		device_printf(dev, "failed to attach syscons\n");
		goto fail;
	}

	/* 
	 * We have to wait until interrupts are enabled. 
	 * Mailbox relies on it to get data from VideoCore
	 */
        sc->init_hook.ich_func = bcm_fb_init;
        sc->init_hook.ich_arg = sc;

        if (config_intrhook_establish(&sc->init_hook) != 0) {
		device_printf(dev, "failed to establish intrhook\n");
                return (ENOMEM);
	}

	return (0);

fail:
	return (ENXIO);
}


static void
bcm_fb_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	bus_addr_t *addr;

	if (err)
		return;

	addr = (bus_addr_t*)arg;
	*addr = PHYS_TO_VCBUS(segs[0].ds_addr);
}

static device_method_t bcm_fb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_fb_probe),
	DEVMETHOD(device_attach,	bcm_fb_attach),

	{ 0, 0 }
};

static devclass_t bcm_fb_devclass;

static driver_t bcm_fb_driver = {
	"fb",
	bcm_fb_methods,
	sizeof(struct bcmsc_softc),
};

DRIVER_MODULE(bcm2835fb, fdtbus, bcm_fb_driver, bcm_fb_devclass, 0, 0);

/*
 * Video driver routines and glue.
 */
static vi_probe_t		bcmfb_probe;
static vi_init_t		bcmfb_init;
static vi_get_info_t		bcmfb_get_info;
static vi_query_mode_t		bcmfb_query_mode;
static vi_set_mode_t		bcmfb_set_mode;
static vi_save_font_t		bcmfb_save_font;
static vi_load_font_t		bcmfb_load_font;
static vi_show_font_t		bcmfb_show_font;
static vi_save_palette_t	bcmfb_save_palette;
static vi_load_palette_t	bcmfb_load_palette;
static vi_set_border_t		bcmfb_set_border;
static vi_save_state_t		bcmfb_save_state;
static vi_load_state_t		bcmfb_load_state;
static vi_set_win_org_t		bcmfb_set_win_org;
static vi_read_hw_cursor_t	bcmfb_read_hw_cursor;
static vi_set_hw_cursor_t	bcmfb_set_hw_cursor;
static vi_set_hw_cursor_shape_t	bcmfb_set_hw_cursor_shape;
static vi_blank_display_t	bcmfb_blank_display;
static vi_mmap_t		bcmfb_mmap;
static vi_ioctl_t		bcmfb_ioctl;
static vi_clear_t		bcmfb_clear;
static vi_fill_rect_t		bcmfb_fill_rect;
static vi_bitblt_t		bcmfb_bitblt;
static vi_diag_t		bcmfb_diag;
static vi_save_cursor_palette_t	bcmfb_save_cursor_palette;
static vi_load_cursor_palette_t	bcmfb_load_cursor_palette;
static vi_copy_t		bcmfb_copy;
static vi_putp_t		bcmfb_putp;
static vi_putc_t		bcmfb_putc;
static vi_puts_t		bcmfb_puts;
static vi_putm_t		bcmfb_putm;

static video_switch_t bcmfbvidsw = {
	.probe			= bcmfb_probe,
	.init			= bcmfb_init,
	.get_info		= bcmfb_get_info,
	.query_mode		= bcmfb_query_mode,
	.set_mode		= bcmfb_set_mode,
	.save_font		= bcmfb_save_font,
	.load_font		= bcmfb_load_font,
	.show_font		= bcmfb_show_font,
	.save_palette		= bcmfb_save_palette,
	.load_palette		= bcmfb_load_palette,
	.set_border		= bcmfb_set_border,
	.save_state		= bcmfb_save_state,
	.load_state		= bcmfb_load_state,
	.set_win_org		= bcmfb_set_win_org,
	.read_hw_cursor		= bcmfb_read_hw_cursor,
	.set_hw_cursor		= bcmfb_set_hw_cursor,
	.set_hw_cursor_shape	= bcmfb_set_hw_cursor_shape,
	.blank_display		= bcmfb_blank_display,
	.mmap			= bcmfb_mmap,
	.ioctl			= bcmfb_ioctl,
	.clear			= bcmfb_clear,
	.fill_rect		= bcmfb_fill_rect,
	.bitblt			= bcmfb_bitblt,
	.diag			= bcmfb_diag,
	.save_cursor_palette	= bcmfb_save_cursor_palette,
	.load_cursor_palette	= bcmfb_load_cursor_palette,
	.copy			= bcmfb_copy,
	.putp			= bcmfb_putp,
	.putc			= bcmfb_putc,
	.puts			= bcmfb_puts,
	.putm			= bcmfb_putm,
};

VIDEO_DRIVER(bcmfb, bcmfbvidsw, bcmfb_configure);

static vr_init_t bcmrend_init;
static vr_clear_t bcmrend_clear;
static vr_draw_border_t bcmrend_draw_border;
static vr_draw_t bcmrend_draw;
static vr_set_cursor_t bcmrend_set_cursor;
static vr_draw_cursor_t bcmrend_draw_cursor;
static vr_blink_cursor_t bcmrend_blink_cursor;
static vr_set_mouse_t bcmrend_set_mouse;
static vr_draw_mouse_t bcmrend_draw_mouse;

/*
 * We use our own renderer; this is because we must emulate a hardware
 * cursor.
 */
static sc_rndr_sw_t bcmrend = {
	bcmrend_init,
	bcmrend_clear,
	bcmrend_draw_border,
	bcmrend_draw,
	bcmrend_set_cursor,
	bcmrend_draw_cursor,
	bcmrend_blink_cursor,
	bcmrend_set_mouse,
	bcmrend_draw_mouse
};

RENDERER(bcmfb, 0, bcmrend, gfb_set);
RENDERER_MODULE(bcmfb, gfb_set);

static void
bcmrend_init(scr_stat* scp)
{
}

static void
bcmrend_clear(scr_stat* scp, int c, int attr)
{
}

static void
bcmrend_draw_border(scr_stat* scp, int color)
{
}

static void
bcmrend_draw(scr_stat* scp, int from, int count, int flip)
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
bcmrend_set_cursor(scr_stat* scp, int base, int height, int blink)
{
}

static void
bcmrend_draw_cursor(scr_stat* scp, int off, int blink, int on, int flip)
{
	video_adapter_t* adp = scp->sc->adp;
	struct video_adapter_softc *sc;
	int row, col;
	uint8_t *addr;
	int i, j, bytes;

	sc = (struct video_adapter_softc *)adp;

	if (scp->curs_attr.height <= 0)
		return;

	if (sc->fb_addr == 0)
		return;

	if (off >= adp->va_info.vi_width * adp->va_info.vi_height)
		return;

	/* calculate the coordinates in the video buffer */
	row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
	col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;

	addr = (uint8_t *)sc->fb_addr
	    + (row + sc->ymargin)*(sc->stride)
	    + (sc->depth/8) * (col + sc->xmargin);

	bytes = sc->depth/8;

	/* our cursor consists of simply inverting the char under it */
	for (i = 0; i < adp->va_info.vi_cheight; i++) {
		for (j = 0; j < adp->va_info.vi_cwidth; j++) {
			switch (sc->depth) {
			case 32:
			case 24:
				addr[bytes*j + 2] ^= 0xff;
				/* FALLTHROUGH */
			case 16:
				addr[bytes*j + 1] ^= 0xff;
				addr[bytes*j] ^= 0xff;
				break;
			default:
				break;
			}
		}

		addr += sc->stride;
	}
}

static void
bcmrend_blink_cursor(scr_stat* scp, int at, int flip)
{
}

static void
bcmrend_set_mouse(scr_stat* scp)
{
}

static void
bcmrend_draw_mouse(scr_stat* scp, int x, int y, int on)
{
	vidd_putm(scp->sc->adp, x, y, mouse_pointer, 0xffffffff, 16, 8);
}

static uint16_t bcmfb_static_window[ROW*COL];
extern u_char dflt_font_16[];

/*
 * Update videoadapter settings after changing resolution
 */
static void
bcmfb_update_margins(video_adapter_t *adp)
{
	struct video_adapter_softc *sc;
	video_info_t *vi;

	sc = (struct video_adapter_softc *)adp;
	vi = &adp->va_info;

	sc->xmargin = (sc->width - (vi->vi_width * vi->vi_cwidth)) / 2;
	sc->ymargin = (sc->height - (vi->vi_height * vi->vi_cheight))/2;
}

static int
bcmfb_configure(int flags)
{
	struct video_adapter_softc *va_sc;

	va_sc = &va_softc;
	phandle_t display, root;
	pcell_t cell;

	if (va_sc->initialized)
		return (0);

	va_sc->width = 0;
	va_sc->height = 0;

	/*
	 * It seems there is no way to let syscons framework know
	 * that framebuffer resolution has changed. So just try
	 * to fetch data from FDT and go with defaults if failed
	 */
	root = OF_finddevice("/");
	if ((root != 0) && 
	    (display = fdt_find_compatible(root, "broadcom,bcm2835-fb", 1))) {
		if ((OF_getprop(display, "broadcom,width", 
		    &cell, sizeof(cell))) > 0)
			va_sc->width = (int)fdt32_to_cpu(cell);

		if ((OF_getprop(display, "broadcom,height", 
		    &cell, sizeof(cell))) > 0)
			va_sc->height = (int)fdt32_to_cpu(cell);
	}

	if (va_sc->width == 0)
		va_sc->width = FB_WIDTH;
	if (va_sc->height == 0)
		va_sc->height = FB_HEIGHT;

	bcmfb_init(0, &va_sc->va, 0);

	va_sc->initialized = 1;

	return (0);
}

static int
bcmfb_probe(int unit, video_adapter_t **adp, void *arg, int flags)
{

	return (0);
}

static int
bcmfb_init(int unit, video_adapter_t *adp, int flags)
{
	struct video_adapter_softc *sc;
	video_info_t *vi;

	sc = (struct video_adapter_softc *)adp;
	vi = &adp->va_info;

	vid_init_struct(adp, "bcmfb", -1, unit);

	sc->font = dflt_font_16;
	vi->vi_cheight = BCMFB_FONT_HEIGHT;
	vi->vi_cwidth = 8;

	vi->vi_width = sc->width/8;
	vi->vi_height = sc->height/vi->vi_cheight;

	/*
	 * Clamp width/height to syscons maximums
	 */
	if (vi->vi_width > COL)
		vi->vi_width = COL;
	if (vi->vi_height > ROW)
		vi->vi_height = ROW;

	sc->xmargin = (sc->width - (vi->vi_width * vi->vi_cwidth)) / 2;
	sc->ymargin = (sc->height - (vi->vi_height * vi->vi_cheight))/2;


	adp->va_window = (vm_offset_t) bcmfb_static_window;
	adp->va_flags |= V_ADP_FONT /* | V_ADP_COLOR | V_ADP_MODECHANGE */;

	vid_register(&sc->va);

	return (0);
}

static int
bcmfb_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	bcopy(&adp->va_info, info, sizeof(*info));
	return (0);
}

static int
bcmfb_query_mode(video_adapter_t *adp, video_info_t *info)
{
	return (0);
}

static int
bcmfb_set_mode(video_adapter_t *adp, int mode)
{
	return (0);
}

static int
bcmfb_save_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	return (0);
}

static int
bcmfb_load_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	struct video_adapter_softc *sc = (struct video_adapter_softc *)adp;

	sc->font = data;

	return (0);
}

static int
bcmfb_show_font(video_adapter_t *adp, int page)
{
	return (0);
}

static int
bcmfb_save_palette(video_adapter_t *adp, u_char *palette)
{
	return (0);
}

static int
bcmfb_load_palette(video_adapter_t *adp, u_char *palette)
{
	return (0);
}

static int
bcmfb_set_border(video_adapter_t *adp, int border)
{
	return (bcmfb_blank_display(adp, border));
}

static int
bcmfb_save_state(video_adapter_t *adp, void *p, size_t size)
{
	return (0);
}

static int
bcmfb_load_state(video_adapter_t *adp, void *p)
{
	return (0);
}

static int
bcmfb_set_win_org(video_adapter_t *adp, off_t offset)
{
	return (0);
}

static int
bcmfb_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	*col = *row = 0;

	return (0);
}

static int
bcmfb_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	return (0);
}

static int
bcmfb_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{
	return (0);
}

static int
bcmfb_blank_display(video_adapter_t *adp, int mode)
{

	struct video_adapter_softc *sc;

	sc = (struct video_adapter_softc *)adp;
	if (sc && sc->fb_addr)
		memset((void*)sc->fb_addr, 0, sc->fb_size);

	return (0);
}

static int
bcmfb_mmap(video_adapter_t *adp, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct video_adapter_softc *sc;

	sc = (struct video_adapter_softc *)adp;

	/*
	 * This might be a legacy VGA mem request: if so, just point it at the
	 * framebuffer, since it shouldn't be touched
	 */
	if (offset < sc->stride*sc->height) {
		*paddr = sc->fb_paddr + offset;
		return (0);
	}

	return (EINVAL);
}

static int
bcmfb_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{
	struct video_adapter_softc *sc;
	struct fbtype *fb;

	sc = (struct video_adapter_softc *)adp;

	switch (cmd) {
	case FBIOGTYPE:
		fb = (struct fbtype *)data;
		fb->fb_type = FBTYPE_PCIMISC;
		fb->fb_height = sc->height;
		fb->fb_width = sc->width;
		fb->fb_depth = sc->depth;
		if (sc->depth <= 1 || sc->depth > 8)
			fb->fb_cmsize = 0;
		else
			fb->fb_cmsize = 1 << sc->depth;
		fb->fb_size = sc->fb_size;
		break;
	default:
		return (fb_commonioctl(adp, cmd, data));
	}

	return (0);
}

static int
bcmfb_clear(video_adapter_t *adp)
{

	return (bcmfb_blank_display(adp, 0));
}

static int
bcmfb_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{

	return (0);
}

static int
bcmfb_bitblt(video_adapter_t *adp, ...)
{

	return (0);
}

static int
bcmfb_diag(video_adapter_t *adp, int level)
{

	return (0);
}

static int
bcmfb_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{

	return (0);
}

static int
bcmfb_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{

	return (0);
}

static int
bcmfb_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{

	return (0);
}

static int
bcmfb_putp(video_adapter_t *adp, vm_offset_t off, uint32_t p, uint32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{

	return (0);
}

static int
bcmfb_putc(video_adapter_t *adp, vm_offset_t off, uint8_t c, uint8_t a)
{
	struct video_adapter_softc *sc;
	int row;
	int col;
	int i, j, k;
	uint8_t *addr;
	u_char *p;
	uint8_t fg, bg, color;
	uint16_t rgb;

	sc = (struct video_adapter_softc *)adp;

	if (sc->fb_addr == 0)
		return (0);

	row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
	col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	p = sc->font + c*BCMFB_FONT_HEIGHT;
	addr = (uint8_t *)sc->fb_addr
	    + (row + sc->ymargin)*(sc->stride)
	    + (sc->depth/8) * (col + sc->xmargin);

	fg = a & 0xf ;
	bg = (a >> 4) & 0xf;

	for (i = 0; i < BCMFB_FONT_HEIGHT; i++) {
		for (j = 0, k = 7; j < 8; j++, k--) {
			if ((p[i] & (1 << k)) == 0)
				color = bg;
			else
				color = fg;

			switch (sc->depth) {
			case 32:
				addr[4*j+0] = bcmfb_palette[color].r;
				addr[4*j+1] = bcmfb_palette[color].g;
				addr[4*j+2] = bcmfb_palette[color].b;
				addr[4*j+3] = bcmfb_palette[color].a;
				break;
			case 24:
				addr[3*j] = bcmfb_palette[color].r;
				addr[3*j+1] = bcmfb_palette[color].g;
				addr[3*j+2] = bcmfb_palette[color].b;
				break;
			case 16:
				rgb = (bcmfb_palette[color].r >> 3) << 11;
				rgb |= (bcmfb_palette[color].g >> 2) << 5;
				rgb |= (bcmfb_palette[color].b >> 3);
				addr[2*j] = rgb & 0xff;
				addr[2*j + 1] = (rgb >> 8) & 0xff;
			default:
				/* Not supported yet */
				break;
			}
		}

		addr += (sc->stride);
	}

        return (0);
}

static int
bcmfb_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i;

	for (i = 0; i < len; i++) 
		bcmfb_putc(adp, off + i, s[i] & 0xff, (s[i] & 0xff00) >> 8);

	return (0);
}

static int
bcmfb_putm(video_adapter_t *adp, int x, int y, uint8_t *pixel_image,
    uint32_t pixel_mask, int size, int width)
{

	return (0);
}

/*
 * Define a stub keyboard driver in case one hasn't been
 * compiled into the kernel
 */
#include <sys/kbio.h>
#include <dev/kbd/kbdreg.h>

static int dummy_kbd_configure(int flags);

keyboard_switch_t bcmdummysw;

static int
dummy_kbd_configure(int flags)
{

	return (0);
}
KEYBOARD_DRIVER(bcmdummy, bcmdummysw, dummy_kbd_configure);
