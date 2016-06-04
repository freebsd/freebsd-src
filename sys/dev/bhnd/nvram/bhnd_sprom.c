/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * BHND SPROM driver.
 * 
 * Abstract driver for memory-mapped SPROM devices.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include "bhnd_nvram_if.h"

#include "bhnd_spromvar.h"

#define	SPROM_LOCK_INIT(sc) \
	mtx_init(&(sc)->mtx, device_get_nameunit((sc)->dev), \
	    "BHND SPROM lock", MTX_DEF)
#define	SPROM_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	SPROM_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	SPROM_LOCK_ASSERT(sc, what)	mtx_assert(&(sc)->mtx, what)
#define	SPROM_LOCK_DESTROY(sc)		mtx_destroy(&(sc)->mtx)

/**
 * Default bhnd sprom driver implementation of DEVICE_PROBE().
 */
int
bhnd_sprom_probe(device_t dev)
{
	/* Quiet by default */
	if (!bootverbose)
		device_quiet(dev);
	device_set_desc(dev, "SPROM/OTP");

	/* Refuse wildcard attachments */
	return (BUS_PROBE_NOWILDCARD);
}

/* Default DEVICE_ATTACH() implementation; assumes a zero offset to the
 * SPROM data */
static int
bhnd_sprom_attach_meth(device_t dev)
{
	return (bhnd_sprom_attach(dev, 0));
}

/**
 * BHND SPROM device attach.
 * 
 * This should be called from DEVICE_ATTACH() with the @p offset to the
 * SPROM data.
 * 
 * Assumes SPROM is mapped via SYS_RES_MEMORY resource with RID 0.
 * 
 * @param dev BHND SPROM device.
 * @param offset Offset to the SPROM data.
 */
int
bhnd_sprom_attach(device_t dev, bus_size_t offset)
{
	struct bhnd_sprom_softc	*sc;
	int				 error;
	
	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate SPROM resource */
	sc->sprom_rid = 0;
	sc->sprom_res = bhnd_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sprom_rid, RF_ACTIVE);
	if (sc->sprom_res == NULL) {
		device_printf(dev, "failed to allocate resources\n");
		return (ENXIO);
	}

	/* Initialize SPROM shadow */
	if ((error = bhnd_sprom_init(&sc->shadow, sc->sprom_res, offset)))
		goto failed;

	/* Initialize mutex */
	SPROM_LOCK_INIT(sc);

	return (0);
	
failed:
	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->sprom_rid,
	    sc->sprom_res);
	return (error);
}

/**
 * Default bhnd_sprom implementation of DEVICE_RESUME().
 */
int
bhnd_sprom_resume(device_t dev)
{
	return (0);
}

/**
 * Default bhnd sprom driver implementation of DEVICE_SUSPEND().
 */
int
bhnd_sprom_suspend(device_t dev)
{
	return (0);
}

/**
 * Default bhnd sprom driver implementation of DEVICE_DETACH().
 */
int
bhnd_sprom_detach(device_t dev)
{
	struct bhnd_sprom_softc	*sc;
	
	sc = device_get_softc(dev);

	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->sprom_rid,
	    sc->sprom_res);
	bhnd_sprom_fini(&sc->shadow);
	SPROM_LOCK_DESTROY(sc);

	return (0);
}

/**
 * Default bhnd sprom driver implementation of BHND_NVRAM_GETVAR().
 */
static int
bhnd_sprom_getvar_meth(device_t dev, const char *name, void *buf, size_t *len)
{
	struct bhnd_sprom_softc	*sc;
	int				 error;

	sc = device_get_softc(dev);

	SPROM_LOCK(sc);
	error = bhnd_sprom_getvar(&sc->shadow, name, buf, len);
	SPROM_UNLOCK(sc);

	return (error);
}

/**
 * Default bhnd sprom driver implementation of BHND_NVRAM_SETVAR().
 */
static int
bhnd_sprom_setvar_meth(device_t dev, const char *name, const void *buf,
    size_t len)
{
	struct bhnd_sprom_softc	*sc;
	int				 error;

	sc = device_get_softc(dev);

	SPROM_LOCK(sc);
	error = bhnd_sprom_setvar(&sc->shadow, name, buf, len);
	SPROM_UNLOCK(sc);

	return (error);
}

static device_method_t bhnd_sprom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bhnd_sprom_probe),
	DEVMETHOD(device_attach,		bhnd_sprom_attach_meth),
	DEVMETHOD(device_resume,		bhnd_sprom_resume),
	DEVMETHOD(device_suspend,		bhnd_sprom_suspend),
	DEVMETHOD(device_detach,		bhnd_sprom_detach),

	/* NVRAM interface */
	DEVMETHOD(bhnd_nvram_getvar,		bhnd_sprom_getvar_meth),
	DEVMETHOD(bhnd_nvram_setvar,		bhnd_sprom_setvar_meth),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_nvram, bhnd_sprom_driver, bhnd_sprom_methods, sizeof(struct bhnd_sprom_softc));
MODULE_VERSION(bhnd_sprom, 1);
