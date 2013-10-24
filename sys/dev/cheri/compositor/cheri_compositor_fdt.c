/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
 * Copyright (c) 2013 Philip Withnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
__FBSDID("$FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor_fdt.c 245380 2013-05-08 20:54:00Z pwithnall $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/cheri/compositor/cheri_compositor_internal.h>

/* Based on sys/dev/terasic/mtl/terasic_mtl_fdt.c by Robert N. M. Watson. */

static int
cheri_compositor_fdt_probe(device_t dev)
{
	if (ofw_bus_is_compatible(dev, "sri-cambridge,compositor")) {
		device_set_desc(dev, "CHERI Compositor");
		return (BUS_PROBE_DEFAULT);
	}
        return (ENXIO);
}

static int
cheri_compositor_fdt_attach(device_t dev)
{
	struct cheri_compositor_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->compositor_dev = dev;
	sc->compositor_unit = device_get_unit(dev);

	/*
	 * FDT allows multiple memory resources to be defined for a device;
	 * query the control registers first, followed by the Avalon sampler for
	 * bus usage statistics, then the client frame buffer region.
	 * However, we need to sanity-check that they are page-aligned and
	 * page-sized, so we may still abort.
	 */
	sc->compositor_reg_rid = 0;
	sc->compositor_reg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->compositor_reg_rid, RF_ACTIVE);
	if (sc->compositor_reg_res == NULL) {
		device_printf(dev, "couldn't map register memory\n");
		error = ENXIO;
		goto error;
	}
	if (rman_get_start(sc->compositor_reg_res) % PAGE_SIZE != 0) {
		device_printf(dev, "improper register address\n");
		error = ENXIO;
		goto error;
	}
	if (rman_get_size(sc->compositor_reg_res) % PAGE_SIZE != 0) {
		device_printf(dev, "improper register size\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->compositor_dev, "registers at mem %p-%p\n",
            (void *)rman_get_start(sc->compositor_reg_res),
	    (void *)(rman_get_start(sc->compositor_reg_res) +
	      rman_get_size(sc->compositor_reg_res)));

	/* Note: The sampler isn't mmap()pable, so its registers don't have to
	 * be page-aligned. */
	sc->compositor_sampler_rid = 2;
	sc->compositor_sampler_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->compositor_sampler_rid, RF_ACTIVE);
	if (sc->compositor_sampler_res == NULL) {
		device_printf(dev, "couldn't map sampler memory\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->compositor_dev, "Avalon sampler at mem %p-%p\n",
            (void *)rman_get_start(sc->compositor_sampler_res),
	    (void *)(rman_get_start(sc->compositor_sampler_res) +
	      rman_get_size(sc->compositor_sampler_res)));

	sc->compositor_cfb_rid = 1;
	sc->compositor_cfb_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->compositor_cfb_rid, RF_ACTIVE);
	if (sc->compositor_cfb_res == NULL) {
		device_printf(dev, "couldn't map CFB memory\n");
		error = ENXIO;
		goto error;
	}
	if (rman_get_start(sc->compositor_cfb_res) % PAGE_SIZE != 0) {
		device_printf(dev, "improper CFB address\n");
		error = ENXIO;
		goto error;
	}
	if (rman_get_size(sc->compositor_cfb_res) % PAGE_SIZE != 0) {
		device_printf(dev, "improper CFB size\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->compositor_dev, "CFB at mem %p-%p\n",
            (void *)rman_get_start(sc->compositor_cfb_res),
	    (void *)(rman_get_start(sc->compositor_cfb_res) +
	      rman_get_size(sc->compositor_cfb_res)));

	error = cheri_compositor_attach(sc);
	if (error == 0)
		return (0);
error:
	if (sc->compositor_cfb_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->compositor_cfb_rid,
		    sc->compositor_cfb_res);
	if (sc->compositor_sampler_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->compositor_sampler_rid,
		    sc->compositor_sampler_res);
	if (sc->compositor_reg_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->compositor_reg_rid,
		    sc->compositor_reg_res);
	return (error);
}

static int
cheri_compositor_fdt_detach(device_t dev)
{
	struct cheri_compositor_softc *sc;

	sc = device_get_softc(dev);
	cheri_compositor_detach(sc);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->compositor_cfb_rid,
	    sc->compositor_cfb_res);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->compositor_sampler_rid,
	    sc->compositor_sampler_res);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->compositor_reg_rid,
	    sc->compositor_reg_res);
	return (0);
}

static device_method_t cheri_compositor_fdt_methods[] = {
	DEVMETHOD(device_probe, cheri_compositor_fdt_probe),
	DEVMETHOD(device_attach, cheri_compositor_fdt_attach),
	DEVMETHOD(device_detach, cheri_compositor_fdt_detach),
	{ 0, 0 }
};

static driver_t cheri_compositor_fdt_driver = {
	"cheri_compositor",
	cheri_compositor_fdt_methods,
	sizeof(struct cheri_compositor_softc),
};

DRIVER_MODULE(cheri_compositor, simplebus, cheri_compositor_fdt_driver,
    cheri_compositor_devclass, 0, 0);
