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
 * BHNDB PCI SPROM driver.
 * 
 * Provides support for early PCI bridge cores that vend SPROM CSRs
 * via PCI configuration space.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/cores/pci/bhnd_pci_hostbvar.h>
#include <dev/bhnd/nvram/bhnd_spromvar.h>

#include "bhnd_nvram_if.h"
#include "bhndb_pcireg.h"
#include "bhndb_pcivar.h"

struct bhndb_pci_sprom_softc {
	device_t		 dev;
	struct bhnd_resource	*sprom_res;	/**< SPROM resource */
	int			 sprom_rid;	/**< SPROM RID */
	struct bhnd_sprom	 shadow;	/**< SPROM shadow */
	struct mtx		 mtx;		/**< SPROM shadow mutex */
};

#define	SPROM_LOCK_INIT(sc) \
	mtx_init(&(sc)->mtx, device_get_nameunit((sc)->dev), \
	    "BHND PCI SPROM lock", MTX_DEF)
#define	SPROM_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	SPROM_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	SPROM_LOCK_ASSERT(sc, what)	mtx_assert(&(sc)->mtx, what)
#define	SPROM_LOCK_DESTROY(sc)		mtx_destroy(&(sc)->mtx)

static int
bhndb_pci_sprom_probe(device_t dev)
{
	device_t	bridge, bus;

	/* Our parent must be a PCI-BHND bridge with an attached bhnd bus */
	bridge = device_get_parent(dev);
	if (device_get_driver(bridge) != &bhndb_pci_driver)
		return (ENXIO);
	
	bus = device_find_child(bridge, devclass_get_name(bhnd_devclass), 0);
	if (bus == NULL)
		return (ENXIO);

	/* Found */
	device_set_desc(dev, "PCI-BHNDB SPROM/OTP");
	if (!bootverbose)
		device_quiet(dev);

	/* Refuse wildcard attachments */
	return (BUS_PROBE_NOWILDCARD);
}

static int
bhndb_pci_sprom_attach(device_t dev)
{
	struct bhndb_pci_sprom_softc	*sc;
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
	if ((error = bhnd_sprom_init(&sc->shadow, sc->sprom_res, 0))) {
		device_printf(dev, "unrecognized SPROM format\n");
		goto failed;
	}

	/* Initialize mutex */
	SPROM_LOCK_INIT(sc);

	return (0);
	
failed:
	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->sprom_rid,
	    sc->sprom_res);
	return (error);
}

static int
bhndb_pci_sprom_resume(device_t dev)
{
	return (0);
}

static int
bhndb_pci_sprom_suspend(device_t dev)
{
	return (0);
}

static int
bhndb_pci_sprom_detach(device_t dev)
{
	struct bhndb_pci_sprom_softc	*sc;
	
	sc = device_get_softc(dev);

	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->sprom_rid,
	    sc->sprom_res);
	bhnd_sprom_fini(&sc->shadow);
	SPROM_LOCK_DESTROY(sc);

	return (0);
}

static int
bhndb_pci_sprom_getvar(device_t dev, const char *name, void *buf, size_t *len)
{
	struct bhndb_pci_sprom_softc	*sc;
	int				 error;

	sc = device_get_softc(dev);

	SPROM_LOCK(sc);
	error = bhnd_sprom_getvar(&sc->shadow, name, buf, len);
	SPROM_UNLOCK(sc);

	return (error);
}

static int
bhndb_pci_sprom_setvar(device_t dev, const char *name, const void *buf,
    size_t len)
{
	struct bhndb_pci_sprom_softc	*sc;
	int				 error;

	sc = device_get_softc(dev);

	SPROM_LOCK(sc);
	error = bhnd_sprom_setvar(&sc->shadow, name, buf, len);
	SPROM_UNLOCK(sc);

	return (error);
}

static device_method_t bhndb_pci_sprom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bhndb_pci_sprom_probe),
	DEVMETHOD(device_attach,		bhndb_pci_sprom_attach),
	DEVMETHOD(device_resume,		bhndb_pci_sprom_resume),
	DEVMETHOD(device_suspend,		bhndb_pci_sprom_suspend),
	DEVMETHOD(device_detach,		bhndb_pci_sprom_detach),

	/* NVRAM interface */
	DEVMETHOD(bhnd_nvram_getvar,		bhndb_pci_sprom_getvar),
	DEVMETHOD(bhnd_nvram_setvar,		bhndb_pci_sprom_setvar),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_nvram, bhndb_pci_sprom_driver, bhndb_pci_sprom_methods, sizeof(struct bhndb_pci_sprom_softc));

DRIVER_MODULE(bhndb_pci_sprom, bhndb, bhndb_pci_sprom_driver, bhnd_nvram_devclass, NULL, NULL);
MODULE_DEPEND(bhndb_pci_sprom, bhnd, 1, 1, 1);
MODULE_VERSION(bhndb_pci_sprom, 1);
