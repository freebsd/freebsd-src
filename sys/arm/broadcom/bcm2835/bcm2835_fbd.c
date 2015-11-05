/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fb/fbreg.h>
#include <dev/vt/vt.h>
#include <dev/vt/colors/vt_termcolors.h>

#include <arm/broadcom/bcm2835/bcm2835_mbox_prop.h>

#include "fb_if.h"
#include "mbox_if.h"

#define	FB_DEPTH		24

struct bcmsc_softc {
	struct fb_info 		*info;
};

static int bcm_fb_probe(device_t);
static int bcm_fb_attach(device_t);

static int
bcm_fb_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-fb"))
		return (ENXIO);

	device_set_desc(dev, "BCM2835 VT framebuffer driver");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm_fb_attach(device_t dev)
{
	char bootargs[2048], *n, *p, *v;
	device_t fbd;
	int fbswap, err;
	phandle_t chosen;
	struct bcm2835_fb_config fb;
	struct bcmsc_softc *sc;
	struct fb_info *info;

	sc = device_get_softc(dev);
	memset(&fb, 0, sizeof(fb));
	if (bcm2835_mbox_fb_get_w_h(&fb) != 0)
		return (ENXIO);
	fb.bpp = FB_DEPTH;
	if ((err = bcm2835_mbox_fb_init(&fb)) != 0) {
		device_printf(dev, "bcm2835_mbox_fb_init failed, err=%d\n", err);
		return (ENXIO);
	}

	info = malloc(sizeof(struct fb_info), M_DEVBUF, M_WAITOK | M_ZERO);
	info->fb_name = device_get_nameunit(dev);
	info->fb_vbase = (intptr_t)pmap_mapdev(fb.base, fb.size);
	info->fb_pbase = fb.base;
	info->fb_size = fb.size;
	info->fb_bpp = info->fb_depth = fb.bpp;
	info->fb_stride = fb.pitch;
	info->fb_width = fb.xres;
	info->fb_height = fb.yres;
	sc->info = info;

	/* Newer firmware versions needs an inverted color palette. */
	fbswap = 0;
	chosen = OF_finddevice("/chosen");
	if (chosen != 0 &&
	    OF_getprop(chosen, "bootargs", &bootargs, sizeof(bootargs)) > 0) {
		p = bootargs;
		while ((v = strsep(&p, " ")) != NULL) {
			if (*v == '\0')
				continue;
			n = strsep(&v, "=");
			if (strcmp(n, "bcm2708_fb.fbswap") == 0 && v != NULL)
				if (*v == '1')
					fbswap = 1;
                }
        }
	if (fbswap) {
		switch (info->fb_bpp) {
		case 24:
			vt_generate_cons_palette(info->fb_cmap,
			    COLOR_FORMAT_RGB, 0xff, 0, 0xff, 8, 0xff, 16);
			info->fb_cmsize = 16;
			break;
		case 32:
			vt_generate_cons_palette(info->fb_cmap,
			    COLOR_FORMAT_RGB, 0xff, 16, 0xff, 8, 0xff, 0);
			info->fb_cmsize = 16;
			break;
		}
	}

	fbd = device_add_child(dev, "fbd", device_get_unit(dev));
	if (fbd == NULL) {
		device_printf(dev, "Failed to add fbd child\n");
		free(info, M_DEVBUF);
		return (ENXIO);
	} else if (device_probe_and_attach(fbd) != 0) {
		device_printf(dev, "Failed to attach fbd device\n");
		device_delete_child(dev, fbd);
		free(info, M_DEVBUF);
		return (ENXIO);
	}

	device_printf(dev, "%dx%d(%dx%d@%d,%d) %dbpp\n", fb.xres, fb.yres,
	    fb.vxres, fb.vyres, fb.xoffset, fb.yoffset, fb.bpp);
	device_printf(dev,
	    "fbswap: %d, pitch %d, base 0x%08x, screen_size %d\n",
	    fbswap, fb.pitch, fb.base, fb.size);

	return (0);
}

static struct fb_info *
bcm_fb_helper_getinfo(device_t dev)
{
	struct bcmsc_softc *sc;

	sc = device_get_softc(dev);

	return (sc->info);
}

static device_method_t bcm_fb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_fb_probe),
	DEVMETHOD(device_attach,	bcm_fb_attach),

	/* Framebuffer service methods */
	DEVMETHOD(fb_getinfo,		bcm_fb_helper_getinfo),

	DEVMETHOD_END
};

static devclass_t bcm_fb_devclass;

static driver_t bcm_fb_driver = {
	"fb",
	bcm_fb_methods,
	sizeof(struct bcmsc_softc),
};

DRIVER_MODULE(bcm2835fb, ofwbus, bcm_fb_driver, bcm_fb_devclass, 0, 0);
