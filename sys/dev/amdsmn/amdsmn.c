/*-
 * Copyright (c) 2017 Conrad Meyer <cem@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the AMD Family 17h CPU System Management Network.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/pci/pcivar.h>
#include <x86/pci_cfgreg.h>

#include <dev/amdsmn/amdsmn.h>

#define	SMN_ADDR_REG	0x60
#define	SMN_DATA_REG	0x64

struct amdsmn_softc {
	struct mtx smn_lock;
};

static struct pciid {
	uint32_t	device_id;
} amdsmn_ids[] = {
	{ 0x14501022 },
};

/*
 * Device methods.
 */
static void 	amdsmn_identify(driver_t *driver, device_t parent);
static int	amdsmn_probe(device_t dev);
static int	amdsmn_attach(device_t dev);
static int	amdsmn_detach(device_t dev);

static device_method_t amdsmn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	amdsmn_identify),
	DEVMETHOD(device_probe,		amdsmn_probe),
	DEVMETHOD(device_attach,	amdsmn_attach),
	DEVMETHOD(device_detach,	amdsmn_detach),
	DEVMETHOD_END
};

static driver_t amdsmn_driver = {
	"amdsmn",
	amdsmn_methods,
	sizeof(struct amdsmn_softc),
};

static devclass_t amdsmn_devclass;
DRIVER_MODULE(amdsmn, hostb, amdsmn_driver, amdsmn_devclass, NULL, NULL);
MODULE_VERSION(amdsmn, 1);
MODULE_PNP_INFO("W32:vendor/device", pci, amdsmn, amdsmn_ids,
    nitems(amdsmn_ids));

static bool
amdsmn_match(device_t parent)
{
	uint32_t devid;
	size_t i;

	devid = pci_get_devid(parent);
	for (i = 0; i < nitems(amdsmn_ids); i++)
		if (amdsmn_ids[i].device_id == devid)
			return (true);
	return (false);
}

static void
amdsmn_identify(driver_t *driver, device_t parent)
{
	device_t child;

	/* Make sure we're not being doubly invoked. */
	if (device_find_child(parent, "amdsmn", -1) != NULL)
		return;
	if (!amdsmn_match(parent))
		return;

	child = device_add_child(parent, "amdsmn", -1);
	if (child == NULL)
		device_printf(parent, "add amdsmn child failed\n");
}

static int
amdsmn_probe(device_t dev)
{
	uint32_t family;

	if (resource_disabled("amdsmn", 0))
		return (ENXIO);
	if (!amdsmn_match(device_get_parent(dev)))
		return (ENXIO);

	family = CPUID_TO_FAMILY(cpu_id);

	switch (family) {
	case 0x17:
		break;
	default:
		return (ENXIO);
	}
	device_set_desc(dev, "AMD Family 17h System Management Network");

	return (BUS_PROBE_GENERIC);
}

static int
amdsmn_attach(device_t dev)
{
	struct amdsmn_softc *sc = device_get_softc(dev);

	mtx_init(&sc->smn_lock, "SMN mtx", "SMN", MTX_DEF);
	return (0);
}

int
amdsmn_detach(device_t dev)
{
	struct amdsmn_softc *sc = device_get_softc(dev);

	mtx_destroy(&sc->smn_lock);
	return (0);
}

int
amdsmn_read(device_t dev, uint32_t addr, uint32_t *value)
{
	struct amdsmn_softc *sc = device_get_softc(dev);
	device_t parent;

	parent = device_get_parent(dev);

	mtx_lock(&sc->smn_lock);
	pci_write_config(parent, SMN_ADDR_REG, addr, 4);
	*value = pci_read_config(parent, SMN_DATA_REG, 4);
	mtx_unlock(&sc->smn_lock);

	return (0);
}

int
amdsmn_write(device_t dev, uint32_t addr, uint32_t value)
{
	struct amdsmn_softc *sc = device_get_softc(dev);
	device_t parent;

	parent = device_get_parent(dev);

	mtx_lock(&sc->smn_lock);
	pci_write_config(parent, SMN_ADDR_REG, addr, 4);
	pci_write_config(parent, SMN_DATA_REG, value, 4);
	mtx_unlock(&sc->smn_lock);

	return (0);
}
