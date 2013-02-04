/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
#include <sys/watchdog.h>

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

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>


struct lpc_fb_dmamap_arg {
	bus_addr_t		lf_dma_busaddr;
};

struct lpc_lcd_config {
	int			lc_xres;
	int			lc_yres;
	int			lc_bpp;
	uint32_t		lc_pixelclock;
	int			lc_left_margin;
	int			lc_right_margin;
	int			lc_upper_margin;
	int			lc_lower_margin;
	int			lc_hsync_len;
	int			lc_vsync_len;
};

struct lpc_fb_softc {
	device_t		lf_dev;
	struct cdev *		lf_cdev;
	struct mtx		lf_mtx;
	struct resource *	lf_mem_res;
	struct resource *	lf_irq_res;
	bus_space_tag_t		lf_bst;
	bus_space_handle_t	lf_bsh;
	void *			lf_intrhand;
	bus_dma_tag_t		lf_dma_tag;
	bus_dmamap_t		lf_dma_map;
	void *			lf_buffer;
	bus_addr_t		lf_buffer_phys;
	bus_size_t		lf_buffer_size;
	struct lpc_lcd_config	lf_lcd_config;
	int			lf_initialized;
	int			lf_opened;
};

extern void ssd1289_configure(void);

#define	lpc_fb_lock(_sc)	mtx_lock(&(_sc)->lf_mtx)
#define	lpc_fb_unlock(_sc)	mtx_unlock(&(_sc)->lf_mtx)
#define	lpc_fb_lock_assert(sc)	mtx_assert(&(_sc)->lf_mtx, MA_OWNED)

#define	lpc_fb_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->lf_bst, (_sc)->lf_bsh, (_reg))
#define	lpc_fb_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->lf_bst, (_sc)->lf_bsh, (_reg), (_val))



static int lpc_fb_probe(device_t);
static int lpc_fb_attach(device_t);
static void lpc_fb_intr(void *);
static void lpc_fb_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err);

static int lpc_fb_fdt_read(phandle_t, const char *, uint32_t *);
static int lpc_fb_read_lcd_config(phandle_t, struct lpc_lcd_config *);

static int lpc_fb_open(struct cdev *, int, int, struct thread *);
static int lpc_fb_close(struct cdev *, int, int, struct thread *);
static int lpc_fb_ioctl(struct cdev *, u_long, caddr_t, int, struct thread *);
static int lpc_fb_mmap(struct cdev *, vm_ooffset_t, vm_paddr_t *, int, vm_memattr_t *);

static void lpc_fb_blank(struct lpc_fb_softc *);

static struct cdevsw lpc_fb_cdevsw = {
	.d_open		= lpc_fb_open,
	.d_close	= lpc_fb_close,
	.d_ioctl	= lpc_fb_ioctl,
	.d_mmap		= lpc_fb_mmap,
	.d_name		= "lpcfb",
	.d_version	= D_VERSION,
};

static int
lpc_fb_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "lpc,fb"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 framebuffer device");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_fb_attach(device_t dev)
{
	struct lpc_fb_softc *sc = device_get_softc(dev);
	struct lpc_fb_dmamap_arg ctx;
	phandle_t node;
	int mode, rid, err = 0;

	sc->lf_dev = dev;
	mtx_init(&sc->lf_mtx, "lpcfb", "fb", MTX_DEF);

	rid = 0;
	sc->lf_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->lf_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->lf_bst = rman_get_bustag(sc->lf_mem_res);
	sc->lf_bsh = rman_get_bushandle(sc->lf_mem_res);

	rid = 0;
	sc->lf_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->lf_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lf_mem_res);
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->lf_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, lpc_fb_intr, sc, &sc->lf_intrhand))
	{
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lf_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 1, sc->lf_irq_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}

	node = ofw_bus_get_node(dev);

	err = lpc_fb_read_lcd_config(node, &sc->lf_lcd_config);
	if (err) {
		device_printf(dev, "cannot read LCD configuration\n");
		goto fail;
	}

	sc->lf_buffer_size = sc->lf_lcd_config.lc_xres * 
	    sc->lf_lcd_config.lc_yres *
	    (sc->lf_lcd_config.lc_bpp == 24 ? 3 : 2);

	device_printf(dev, "%dx%d LCD, %d bits per pixel, %dkHz pixel clock\n",
	    sc->lf_lcd_config.lc_xres, sc->lf_lcd_config.lc_yres,
	    sc->lf_lcd_config.lc_bpp, sc->lf_lcd_config.lc_pixelclock / 1000);

	err = bus_dma_tag_create(
	    bus_get_dma_tag(sc->lf_dev),
	    4, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    sc->lf_buffer_size, 1,	/* maxsize, nsegments */
	    sc->lf_buffer_size, 0,	/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lf_dma_tag);

	err = bus_dmamem_alloc(sc->lf_dma_tag, (void **)&sc->lf_buffer,
	    0, &sc->lf_dma_map);
	if (err) {
		device_printf(dev, "cannot allocate framebuffer\n");
		goto fail;
	}

	err = bus_dmamap_load(sc->lf_dma_tag, sc->lf_dma_map, sc->lf_buffer,
	    sc->lf_buffer_size, lpc_fb_dmamap_cb, &ctx, BUS_DMA_NOWAIT);
	if (err) {
		device_printf(dev, "cannot load DMA map\n");
		goto fail;
	}

	switch (sc->lf_lcd_config.lc_bpp) {
	case 12:
		mode = LPC_CLKPWR_LCDCLK_CTRL_MODE_12;
		break;
	case 15:
		mode = LPC_CLKPWR_LCDCLK_CTRL_MODE_15;
		break;
	case 16:
		mode = LPC_CLKPWR_LCDCLK_CTRL_MODE_16;
		break;
	case 24:
		mode = LPC_CLKPWR_LCDCLK_CTRL_MODE_24;
		break;
	default:
		panic("unsupported bpp");
	}

	lpc_pwr_write(sc->lf_dev, LPC_CLKPWR_LCDCLK_CTRL,
	    LPC_CLKPWR_LCDCLK_CTRL_MODE(mode) |
	    LPC_CLKPWR_LCDCLK_CTRL_HCLKEN);

	sc->lf_buffer_phys = ctx.lf_dma_busaddr;
	sc->lf_cdev = make_dev(&lpc_fb_cdevsw, 0, UID_ROOT, GID_WHEEL,
	    0600, "lpcfb");

	sc->lf_cdev->si_drv1 = sc;

	return (0);
fail:
	return (ENXIO);
}

static void
lpc_fb_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	struct lpc_fb_dmamap_arg *ctx;

	if (err)
		return;

	ctx = (struct lpc_fb_dmamap_arg *)arg;
	ctx->lf_dma_busaddr = segs[0].ds_addr;
}

static void
lpc_fb_intr(void *arg)
{
}

static int
lpc_fb_fdt_read(phandle_t node, const char *name, uint32_t *ret)
{
	if (OF_getprop(node, name, ret, sizeof(uint32_t)) <= 0)
		return (ENOENT);

	*ret = fdt32_to_cpu(*ret);
	return (0);
}

static int
lpc_fb_read_lcd_config(phandle_t node, struct lpc_lcd_config *cfg)
{
	if (lpc_fb_fdt_read(node, "horizontal-resolution", &cfg->lc_xres))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "vertical-resolution", &cfg->lc_yres))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "bits-per-pixel", &cfg->lc_bpp))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "pixel-clock", &cfg->lc_pixelclock))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "left-margin", &cfg->lc_left_margin))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "right-margin", &cfg->lc_right_margin))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "upper-margin", &cfg->lc_upper_margin))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "lower-margin", &cfg->lc_lower_margin))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "hsync-len", &cfg->lc_hsync_len))
		return (ENXIO);

	if (lpc_fb_fdt_read(node, "vsync-len", &cfg->lc_vsync_len))
		return (ENXIO);

	return (0);
}

static void
lpc_fb_setup(struct lpc_fb_softc *sc)
{
	struct lpc_lcd_config *cfg = &sc->lf_lcd_config;
	uint32_t bpp;

	lpc_fb_write_4(sc, LPC_LCD_TIMH,
	    LPC_LCD_TIMH_PPL(cfg->lc_xres) |
	    LPC_LCD_TIMH_HSW(cfg->lc_hsync_len - 1) |
	    LPC_LCD_TIMH_HFP(cfg->lc_right_margin - 1) |
	    LPC_LCD_TIMH_HBP(cfg->lc_left_margin - 1));

	lpc_fb_write_4(sc, LPC_LCD_TIMV,
	    LPC_LCD_TIMV_LPP(cfg->lc_yres - 1) |
	    LPC_LCD_TIMV_VSW(cfg->lc_vsync_len - 1) |
	    LPC_LCD_TIMV_VFP(cfg->lc_lower_margin) |
	    LPC_LCD_TIMV_VBP(cfg->lc_upper_margin));

	/* XXX LPC_LCD_POL_PCD_LO */
	lpc_fb_write_4(sc, LPC_LCD_POL,
	    LPC_LCD_POL_IHS | LPC_LCD_POL_IVS |
	    LPC_LCD_POL_CPL(cfg->lc_xres - 1) |
	    LPC_LCD_POL_PCD_LO(4));
	
	lpc_fb_write_4(sc, LPC_LCD_UPBASE, sc->lf_buffer_phys);
	
	switch (cfg->lc_bpp) {
	case 1:
		bpp = LPC_LCD_CTRL_BPP1;
		break;
	case 2:
		bpp = LPC_LCD_CTRL_BPP2;
		break;
	case 4:
		bpp = LPC_LCD_CTRL_BPP4;
		break;
	case 8:
		bpp = LPC_LCD_CTRL_BPP8;
		break;
	case 12:
		bpp = LPC_LCD_CTRL_BPP12_444;
		break;
	case 15:
		bpp = LPC_LCD_CTRL_BPP16;
		break;
	case 16:
		bpp = LPC_LCD_CTRL_BPP16_565;
		break;
	case 24:
		bpp = LPC_LCD_CTRL_BPP24;
		break;
	default:
		panic("LCD unknown bpp: %d", cfg->lc_bpp);
	}

	lpc_fb_write_4(sc, LPC_LCD_CTRL,
	    LPC_LCD_CTRL_LCDVCOMP(1) |
	    LPC_LCD_CTRL_LCDPWR |
	    LPC_LCD_CTRL_BGR |
	    LPC_LCD_CTRL_LCDTFT |
	    LPC_LCD_CTRL_LCDBPP(bpp) |
	    LPC_LCD_CTRL_LCDEN);
}


static int
lpc_fb_open(struct cdev *cdev, int oflags, int devtype, struct thread *td)
{
	struct lpc_fb_softc *sc = cdev->si_drv1;

	lpc_fb_lock(sc);

	if (sc->lf_opened)
		return (EBUSY);

	sc->lf_opened = 1;

	lpc_fb_unlock(sc);

	if (!sc->lf_initialized) {
		ssd1289_configure();
		lpc_fb_setup(sc);
		lpc_fb_blank(sc);
		sc->lf_initialized = 1;
	}

	return (0);
}

static int
lpc_fb_close(struct cdev *cdev, int fflag, int devtype, struct thread *td)
{	
	struct lpc_fb_softc *sc = cdev->si_drv1;

	lpc_fb_lock(sc);
	sc->lf_opened = 0;
	lpc_fb_unlock(sc);

	return (0);
}

static int
lpc_fb_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int x,
    struct thread *td)
{
	
	return (EINVAL);
}

static int
lpc_fb_mmap(struct cdev *cdev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	struct lpc_fb_softc *sc = cdev->si_drv1;

	*paddr = (vm_paddr_t)(sc->lf_buffer_phys + offset);
	return (0);
}

static void
lpc_fb_blank(struct lpc_fb_softc *sc)
{
	memset(sc->lf_buffer, 0xffff, sc->lf_buffer_size);
}

static device_method_t lpc_fb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_fb_probe),
	DEVMETHOD(device_attach,	lpc_fb_attach),

	{ 0, 0 }
};

static devclass_t lpc_fb_devclass;

static driver_t lpc_fb_driver = {
	"lpcfb",
	lpc_fb_methods,
	sizeof(struct lpc_fb_softc),
};

DRIVER_MODULE(lpcfb, simplebus, lpc_fb_driver, lpc_fb_devclass, 0, 0);
