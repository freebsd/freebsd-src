/*-
 * Copyright (c) 2012 Robert N. M. Watson
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
__FBSDID("$FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor_nexus.c 245380 2013-05-08 20:55:00Z pwithnall $");

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

#include <dev/cheri/compositor/cheri_compositor_internal.h>

/* Based on sys/dev/terasic/mtl/terasic_mtl_nexus.c by Robert N. M. Watson. */

static int
cheri_compositor_nexus_probe(device_t dev)
{
	device_set_desc(dev, "CHERI Compositor");
	return (BUS_PROBE_NOWILDCARD);
}

static int
cheri_compositor_nexus_attach(device_t dev)
{
	struct cheri_compositor_softc *sc;
	u_long cfb_maddr, sampler_maddr, reg_maddr;
	u_long cfb_msize, sampler_msize, reg_msize;
	int error;

	sc = device_get_softc(dev);
	sc->compositor_dev = dev;
	sc->compositor_unit = device_get_unit(dev);

	/*
	 * Query non-standard hints to find the locations of our two memory
	 * regions.  Enforce certain alignment and size requirements.
	 */
	if (resource_long_value(device_get_name(dev), device_get_unit(dev),
	    "reg_maddr", &reg_maddr) != 0 || (reg_maddr % PAGE_SIZE != 0)) {
		device_printf(dev, "improper register address\n");
		return (ENXIO);
	}
	if (resource_long_value(device_get_name(dev), device_get_unit(dev),
	    "reg_msize", &reg_msize) != 0 || (reg_msize % PAGE_SIZE != 0)) {
		device_printf(dev, "improper register size\n");
		return (ENXIO);
	}
	if (resource_long_value(device_get_name(dev), device_get_unit(dev),
	    "sampler_maddr", &sampler_maddr) != 0 ||
	    (sampler_maddr % PAGE_SIZE != 0)) {
		device_printf(dev, "improper sampler address\n");
		return (ENXIO);
	}
	if (resource_long_value(device_get_name(dev), device_get_unit(dev),
	    "sampler_msize", &sampler_msize) != 0 ||
	    (sampler_msize % PAGE_SIZE != 0)) {
		device_printf(dev, "improper sampler size\n");
		return (ENXIO);
	}
	if (resource_long_value(device_get_name(dev), device_get_unit(dev),
	    "cfb_maddr", &cfb_maddr) != 0 ||
	    (cfb_maddr % PAGE_SIZE != 0)) {
		device_printf(dev, "improper CFB address\n");
		return (ENXIO);
	}
	if (resource_long_value(device_get_name(dev), device_get_unit(dev),
	    "cfb_msize", &cfb_msize) != 0 ||
	    (cfb_msize % PAGE_SIZE != 0)) {
		device_printf(dev, "improper CFB size\n");
		return (ENXIO);
	}

	/*
	 * Allocate resources.
	 */
	sc->compositor_reg_rid = 0;
	sc->compositor_reg_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &sc->compositor_reg_rid, reg_maddr, reg_maddr + reg_msize - 1,
	    reg_msize, RF_ACTIVE);
	if (sc->compositor_reg_res == NULL) {
		device_printf(dev, "couldn't map register memory\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->compositor_dev, "registers at mem %p-%p\n",
	    (void *)reg_maddr, (void *)(reg_maddr + reg_msize));

	sc->compositor_sampler_rid = 0;
	sc->compositor_sampler_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &sc->compositor_sampler_rid, sampler_maddr,
	    sampler_maddr + sampler_msize - 1, sampler_msize, RF_ACTIVE);
	if (sc->compositor_sampler_res == NULL) {
		device_printf(dev, "couldn't map sampler memory\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->compositor_dev, "Avalon sampler at mem %p-%p\n",
	    (void *)sampler_maddr, (void *)(sampler_maddr + sampler_msize));

	sc->compositor_cfb_rid = 0;
	sc->compositor_cfb_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &sc->compositor_cfb_rid, cfb_maddr, cfb_maddr + cfb_msize - 1,
	    cfb_msize, RF_ACTIVE);
	if (sc->compositor_cfb_res == NULL) {
		device_printf(dev, "couldn't map CFB memory\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->compositor_dev, "CFB at mem %p-%p\n",
	    (void *)cfb_maddr, (void *)(cfb_maddr + cfb_msize));
	
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
cheri_compositor_nexus_detach(device_t dev)
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

static device_method_t cheri_compositor_nexus_methods[] = {
	DEVMETHOD(device_probe, cheri_compositor_nexus_probe),
	DEVMETHOD(device_attach, cheri_compositor_nexus_attach),
	DEVMETHOD(device_detach, cheri_compositor_nexus_detach),
	{ 0, 0 }
};

static driver_t cheri_compositor_nexus_driver = {
	"cheri_compositor",
	cheri_compositor_nexus_methods,
	sizeof(struct cheri_compositor_softc),
};

DRIVER_MODULE(cheri_compositor, nexus, cheri_compositor_nexus_driver,
    cheri_compositor_devclass, 0, 0);
