/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
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
 * Broadcom PCI-BHND Host Bridge.
 * 
 * This driver is used to "eat" PCI(e) cores operating in endpoint mode when
 * they're attached to a bhndb_pci driver on the host side.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include "bhnd_pcireg.h"

#include "bhnd_pcibvar.h"

static const struct bhnd_pcib_device {
	uint16_t	 vendor;
	uint16_t	 device;
	const char	*desc;
} bhnd_pcib_devs[] = {
	{ BHND_MFGID_BCM,	BHND_COREID_PCI,	"BHND Host-PCI bridge" },
	{ BHND_MFGID_BCM,	BHND_COREID_PCIE,	"BHND Host-PCI bridge (PCIe Gen1)" },
	{ BHND_MFGID_INVALID,	BHND_COREID_INVALID,	NULL }
};

static int
bhnd_pcib_probe(device_t dev)
{
	const struct bhnd_pcib_device *id;

	/* Ignore PCI cores configured in host bridge mode */
	if (bhnd_is_hostb_device(dev))
		return (ENXIO);

	for (id = bhnd_pcib_devs; id->device != BHND_COREID_INVALID; id++) {
		if (bhnd_get_vendor(dev) != id->vendor)
			continue;

		if (bhnd_get_device(dev) != id->device)
			continue;

		device_set_desc(dev, id->desc);
		return (BUS_PROBE_SPECIFIC);
	}

	return (ENXIO);
}

static int
bhnd_pcib_attach(device_t dev)
{
	return (ENXIO);
}

static int
bhnd_pcib_detach(device_t dev)
{
	return (ENXIO);
}

static int
bhnd_pcib_suspend(device_t dev)
{
	return (ENXIO);
}

static int
bhnd_pcib_resume(device_t dev)
{
	return (ENXIO);
}

static device_method_t bhnd_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bhnd_pcib_probe),
	DEVMETHOD(device_attach,	bhnd_pcib_attach),
	DEVMETHOD(device_detach,	bhnd_pcib_detach),
	DEVMETHOD(device_suspend,	bhnd_pcib_suspend),
	DEVMETHOD(device_resume,	bhnd_pcib_resume),
	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_pcib, bhnd_pcib_driver, bhnd_pcib_methods, sizeof(struct bhnd_pcib_softc));
DRIVER_MODULE(bhnd_pcib, bhnd, bhnd_pcib_driver, bhnd_hostb_devclass, 0, 0);

MODULE_VERSION(bhnd_pcib, 1);
MODULE_DEPEND(bhnd_pcib, pci, 1, 1, 1);
MODULE_DEPEND(bhnd_pcib, bhnd_pci_mdio, 1, 1, 1);
