/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Nicolas Provost <dev@npsoft.fr>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Allwinner A20 Simple Framebuffer support
 *
 * The purpose of this driver is to keep the display set up by the initial
 * bootloader (u-boot's simple framebuffer) and to move the framebuffer area
 * to another address in memory which is usable.
 *
 * The device tree file must reflect the display resolution chosen by the boot-
 * loader (see dts/arm/olimex-som-evb.dts for an example), under the node
 * "allwinner,sun7i-a20-simplefb".
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/fbio.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/videomode/videomode.h>

#include "fb_if.h"

#define FB_ALIGN		0x1000

/* Display frontend */
#define DEFE_EN_REG		0x0000
#define DEFE_EN			(1 << 0)
#define DEFE_FRM_CTRL_REG	0x0001
#define DEFE_FRM_START		(1 << 16)
#define DEFE_FB0_REG		0x0020

/* Display backend */
#define	DEBE_REG_START		0x800
#define	DEBE_REG_END		0x1000
#define	DEBE_REG_WIDTH		4
#define	DEBE_MODCTL		0x800
#define	MODCTL_ITLMOD_EN	(1 << 28)
#define	MODCTL_OUT_SEL_MASK	(0x7 << 20)
#define	MODCTL_OUT_SEL(sel)	((sel) << 20)
#define	OUT_SEL_LCD		0
#define	MODCTL_LAY0_EN		(1 << 8)
#define	MODCTL_START_CTL	(1 << 1)
#define	MODCTL_EN		(1 << 0)
#define	DEBE_DISSIZE		0x808
#define	DIS_HEIGHT(h)		(((h) - 1) << 16)
#define	DIS_WIDTH(w)		(((w) - 1) << 0)
#define	DEBE_LAYSIZE0		0x810
#define	LAY_HEIGHT(h)		(((h) - 1) << 16)
#define	LAY_WIDTH(w)		(((w) - 1) << 0)
#define	DEBE_LAYCOOR0		0x820
#define	LAY_XCOOR(x)		((x) << 16)
#define	LAY_YCOOR(y)		((y) << 0)
#define	DEBE_LAYLINEWIDTH0	0x840
#define	DEBE_LAYFB_L32ADD0	0x850
#define	LAYFB_L32ADD(pa)	((pa) << 3)
#define	DEBE_LAYFB_H4ADD	0x860
#define	LAY0FB_H4ADD(pa)	((pa) >> 29)
#define	DEBE_REGBUFFCTL		0x870
#define	REGBUFFCTL_LOAD		(1 << 0)
#define	DEBE_ATTCTL1		0x8a0
#define	ATTCTL1_FBFMT(fmt)	((fmt) << 8)
#define	FBFMT_XRGB8888		9
#define	ATTCTL1_FBPS(ps)	((ps) << 0)
#define	FBPS_32BPP_ARGB		0

struct a20_simplefb_softc {
	device_t		dev;
	device_t		fbdev;
	struct resource		*res[2];
	struct fb_info		info;
};

static struct resource_spec a20_simplefb_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* DEFE0 */
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },	/* DEBE0 */
	{ -1, 0 }
};

#define	DEFE_READ(sc, reg)		bus_read_4((sc)->res[0], (reg))
#define	DEFE_WRITE(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))

#define	DEBE_READ(sc, reg)		bus_read_4((sc)->res[1], (reg))
#define	DEBE_WRITE(sc, reg, val)	bus_write_4((sc)->res[1], (reg), (val))

static int
a20_simplefb_allocfb(struct a20_simplefb_softc *sc)
{
	sc->info.fb_vbase = kmem_alloc_contig(sc->info.fb_size,
				M_NOWAIT | M_ZERO, 0, ~0,
				FB_ALIGN, 0, VM_MEMATTR_WRITE_COMBINING);
	if (sc->info.fb_vbase == 0) {
		device_printf(sc->dev, "failed to allocate FB memory\n");
		return (ENOMEM);
	}
	sc->info.fb_pbase = pmap_kextract(sc->info.fb_vbase);

	return (0);
}

static void
a20_simplefb_freefb(struct a20_simplefb_softc *sc)
{
	kmem_free(sc->info.fb_vbase, sc->info.fb_size);
}

static int
a20_simplefb_setup(struct a20_simplefb_softc *sc)
{
	/* Setup framebuffer address */
	DEFE_WRITE(sc, DEFE_FB0_REG, sc->info.fb_pbase);

	/* Point layer 0 to FB memory */
	DEBE_WRITE(sc, DEBE_LAYFB_L32ADD0,
			LAYFB_L32ADD(sc->info.fb_pbase));
	DEBE_WRITE(sc, DEBE_LAYFB_H4ADD,
			LAY0FB_H4ADD(sc->info.fb_pbase));

	return (0);
}

static void
a20_simplefb_enable(struct a20_simplefb_softc *sc, int onoff)
{
	uint32_t val;

	/* Enable or disable DEFE/DEBE */
	if (onoff == 0) {
		val = DEBE_READ(sc, DEBE_MODCTL);
		DEBE_WRITE(sc, DEBE_MODCTL, val & ~MODCTL_EN);
		val = DEFE_READ(sc, DEFE_EN_REG);
		DEFE_WRITE(sc, DEFE_EN_REG, val & ~DEFE_EN);
	}
	else {
		val = DEBE_READ(sc, DEBE_MODCTL);
		DEBE_WRITE(sc, DEBE_MODCTL, val | MODCTL_EN);
		DEFE_WRITE(sc, DEFE_EN_REG, DEFE_EN);

		/* Commit settings */
		DEBE_WRITE(sc, DEBE_REGBUFFCTL, REGBUFFCTL_LOAD);

		/* Start DEBE */
		val = DEBE_READ(sc, DEBE_MODCTL);
		val |= MODCTL_START_CTL;
		DEBE_WRITE(sc, DEBE_MODCTL, val);
	}
}

static int
a20_simplefb_configure(struct a20_simplefb_softc *sc)
{
	int error;

	/* Disable graphic stack */
	a20_simplefb_enable(sc, 0);

	/* Detach the old FB device */
	if (sc->fbdev != NULL) {
		device_delete_child(sc->dev, sc->fbdev);
		sc->fbdev = NULL;
	}

	/* Allocate the FB */
	if (sc->info.fb_vbase == 0) {
		error = a20_simplefb_allocfb(sc);
		if (error != 0)
			return (error);
	}

	/* Setup framebuffer in backend/frontend */
	error = a20_simplefb_setup(sc);
	if (error != 0)
		return (error);

	/* Attach the new framebuffer device */
	sc->fbdev = device_add_child(sc->dev, "fbd",
					device_get_unit(sc->dev));
	if (sc->fbdev == NULL)
		return (ENOENT);
	
	//sc->info.fb_name = device_get_nameunit(sc->fbdev);
	error = device_probe_and_attach(sc->fbdev);
	if (error != 0)
		return (error);

	/* Enable graphic stack */
	a20_simplefb_enable(sc, 1);

	return (0);
}

static int
a20_simplefb_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun7i-a20-simplefb"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner A20 Simple Framebuffer");
	return (BUS_PROBE_DEFAULT);
}

static int
a20_simplefb_attach(device_t dev)
{
	struct a20_simplefb_softc *sc;
	pcell_t height, width, stride, depth;
	phandle_t node;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (OF_getencprop(node, "height", &height, sizeof(height)) == -1 ||
		OF_getencprop(node, "width", &width, sizeof(width)) == -1 ||
		OF_getencprop(node, "stride", &stride, sizeof(stride)) == -1 ||
		OF_getencprop(node, "depth", &depth, sizeof(depth)) == -1) {
		device_printf(dev, "missing information in device tree\n");
		return (ENXIO);
	}

	sc->info.fb_size = stride * height;
	sc->info.fb_bpp = sc->info.fb_depth = depth;
	sc->info.fb_stride = stride;
	sc->info.fb_width = width;
	sc->info.fb_height = height;
	sc->dev = dev;

	if (bus_alloc_resources(dev, a20_simplefb_spec, sc->res)) {
		device_printf(dev, "cannot allocate resources\n");
		return (ENXIO);
	}

	if (a20_simplefb_configure(sc)) {
		device_printf(dev, "failed to configure simplefb\n");
		return (ENXIO);
	}

	return (0);
}

static struct fb_info *
a20_simplefb_fb_getinfo(device_t dev)
{
	struct a20_simplefb_softc *sc;

	sc = device_get_softc(dev);

	return (&sc->info);
}

static device_method_t a20_simplefb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a20_simplefb_probe),
	DEVMETHOD(device_attach,	a20_simplefb_attach),

	/* FB interface */
	DEVMETHOD(fb_getinfo,		a20_simplefb_fb_getinfo),

	DEVMETHOD_END
};

static driver_t a20_simplefb_driver = {
	"fb",
	a20_simplefb_methods,
	sizeof(struct a20_simplefb_softc),
};

static devclass_t a20_simplefb_devclass;

DRIVER_MODULE(fb, simplebus, a20_simplefb_driver, a20_simplefb_devclass, 0, 0);
