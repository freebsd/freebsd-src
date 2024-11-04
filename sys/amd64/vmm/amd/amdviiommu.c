/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Ka Ho Ng
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "amdvi_priv.h"
#include "ivhd_if.h"

struct amdviiommu_softc {
	struct resource *event_res;	/* Event interrupt resource. */
	void   		*event_tag;	/* Event interrupt tag. */
	int		event_rid;
};

static int	amdviiommu_probe(device_t);
static int	amdviiommu_attach(device_t);
static int	amdviiommu_detach(device_t);
static int	ivhd_setup_intr(device_t, driver_intr_t, void *,
		    const char *);
static int	ivhd_teardown_intr(device_t);

static device_method_t amdviiommu_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,			amdviiommu_probe),
	DEVMETHOD(device_attach,		amdviiommu_attach),
	DEVMETHOD(device_detach,		amdviiommu_detach),
	DEVMETHOD(ivhd_setup_intr,		ivhd_setup_intr),
	DEVMETHOD(ivhd_teardown_intr,		ivhd_teardown_intr),
	DEVMETHOD_END
};
static driver_t amdviiommu_driver = {
	"amdviiommu",
	amdviiommu_methods,
	sizeof(struct amdviiommu_softc),
};

static int
amdviiommu_probe(device_t dev)
{
	int error;
	int capoff;

	/*
	 * Check base class and sub-class
	 */
	if (pci_get_class(dev) != PCIC_BASEPERIPH ||
	    pci_get_subclass(dev) != PCIS_BASEPERIPH_IOMMU)
		return (ENXIO);

	/*
	 * A IOMMU capability block carries a 0Fh capid.
	 */
	error = pci_find_cap(dev, PCIY_SECDEV, &capoff);
	if (error)
		return (ENXIO);

	/*
	 * bit [18:16] == 011b indicates the capability block is IOMMU
	 * capability block. If the field is not set to 011b, bail out.
	 */
	if ((pci_read_config(dev, capoff + 2, 2) & 0x7) != 0x3)
		return (ENXIO);

	return (BUS_PROBE_SPECIFIC);
}

static int
amdviiommu_attach(device_t dev)
{

	device_set_desc(dev, "AMD-Vi/IOMMU PCI function");
	return (0);
}

static int
amdviiommu_detach(device_t dev)
{

	return (0);
}

static int
ivhd_setup_intr(device_t dev, driver_intr_t handler, void *arg,
    const char *desc)
{
	struct amdviiommu_softc *sc;
	int error, msicnt;

	sc = device_get_softc(dev);
	msicnt = 1;
	if (sc->event_res != NULL)
		panic("%s is called without intr teardown", __func__);
	sc->event_rid = 1;

	error = pci_alloc_msi(dev, &msicnt);
	if (error) {
		device_printf(dev, "Couldn't find event MSI IRQ resource.\n");
		return (ENOENT);
	}

	sc->event_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->event_rid, RF_ACTIVE);
	if (sc->event_res == NULL) {
		device_printf(dev, "Unable to allocate event INTR resource.\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->event_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, handler, arg, &sc->event_tag);
	if (error) {
		device_printf(dev, "Fail to setup event intr\n");
		goto fail;
	}

	bus_describe_intr(dev, sc->event_res, sc->event_tag, "%s", desc);
	return (0);

fail:
	ivhd_teardown_intr(dev);
	return (error);
}

static int
ivhd_teardown_intr(device_t dev)
{
	struct amdviiommu_softc *sc;

	sc = device_get_softc(dev);

	if (sc->event_tag != NULL) {
		bus_teardown_intr(dev, sc->event_res, sc->event_tag);
		sc->event_tag = NULL;
	}
	if (sc->event_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->event_rid,
		    sc->event_res);
		sc->event_res = NULL;
	}
	pci_release_msi(dev);
	return (0);
}

/* This driver has to be loaded before ivhd */
DRIVER_MODULE(amdviiommu, pci, amdviiommu_driver, 0, 0);
MODULE_DEPEND(amdviiommu, pci, 1, 1, 1);
