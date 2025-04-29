/*-
 * Copyright (c) 2018 Stormshield
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
 */

/*
 * Implementation of Primary to Sideband bridge (P2SB), the documentation is available here :
 * https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/c620-series-chipset-datasheet.pdf
 * section 36.9 P2SB Bridge.
 * This device exposes a 16MB memory block, this block is composed of 256 64KB blocks called ports.
 * The indexes of this array (target port ID) can be found on the Table 36-10 of the documentation.
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "p2sb.h"

#define PCI_PRODUCT_LEWISBURG_P2SB 0xa1a08086

#define P2SB_PORT2ADDRESS_SHIFT 16
#define P2SB_PORT_ADDRESS(port) ((uint32_t)port << P2SB_PORT2ADDRESS_SHIFT)

static const uint8_t lbg_communities[] = {
	0xAF, 0xAE, 0xAD, 0xAC, 0xAB, 0x11
};

/* The softc holds our per-instance data. */
struct p2sb_softc {
	device_t	dev;
	int rid;
	struct resource *res;
	struct intel_community *communities;
	int ncommunities;
	struct mtx mutex;
};

int
p2sb_get_port(device_t dev, int unit)
{

	if (unit >= nitems(lbg_communities))
		return (EINVAL);
	return (lbg_communities[unit]);
}

uint32_t
p2sb_port_read_4(device_t dev, uint8_t port, uint32_t reg)
{
	struct p2sb_softc *sc;

	KASSERT(reg < (1<<P2SB_PORT2ADDRESS_SHIFT), ("register out of port"));
	sc = device_get_softc(dev);
	return (bus_read_4(sc->res, P2SB_PORT_ADDRESS(port) + reg));
}

void
p2sb_port_write_4(device_t dev, uint8_t port, uint32_t reg, uint32_t val)
{
	struct p2sb_softc *sc;

	KASSERT(reg < (1<<P2SB_PORT2ADDRESS_SHIFT), ("register out of port"));
	sc = device_get_softc(dev);
	bus_write_4(sc->res, P2SB_PORT_ADDRESS(port) + reg, val);
}

void
p2sb_lock(device_t dev)
{
	struct p2sb_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock_spin(&sc->mutex);
}

void
p2sb_unlock(device_t dev)
{
	struct p2sb_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock_spin(&sc->mutex);
}


static int
p2sb_probe(device_t dev)
{

	if (pci_get_devid(dev) == PCI_PRODUCT_LEWISBURG_P2SB) {
		device_set_desc(dev, "Lewisburg P2SB");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

/* Attach function is only called if the probe is successful. */

static int
p2sb_attach(device_t dev)
{
	struct p2sb_softc *sc;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rid = PCIR_BAR(0);
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Could not allocate memory.\n");
		return (ENXIO);
	}
	mtx_init(&sc->mutex, device_get_nameunit(dev), NULL, MTX_SPIN);
	for (i = 0; i < nitems(lbg_communities); ++i)
		device_add_child(dev, "lbggpiocm", i);

	bus_attach_children(dev);
	return (0);
}

/* Detach device. */

static int
p2sb_detach(device_t dev)
{
	struct p2sb_softc *sc;
	int error;

	/* Teardown the state in our softc created in our attach routine. */
	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);

	sc = device_get_softc(dev);
	mtx_destroy(&sc->mutex);
	if (sc->res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
	return (0);
}

/* Called during system shutdown after sync. */

static int
p2sb_shutdown(device_t dev)
{

	return (0);
}

/*
 * Device suspend routine.
 */
static int
p2sb_suspend(device_t dev)
{

	return (0);
}

/*
 * Device resume routine.
 */
static int
p2sb_resume(device_t dev)
{

	return (0);
}

static device_method_t p2sb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		p2sb_probe),
	DEVMETHOD(device_attach,	p2sb_attach),
	DEVMETHOD(device_detach,	p2sb_detach),
	DEVMETHOD(device_shutdown,	p2sb_shutdown),
	DEVMETHOD(device_suspend,	p2sb_suspend),
	DEVMETHOD(device_resume,	p2sb_resume),

	DEVMETHOD_END
};

DEFINE_CLASS_0(p2sb, p2sb_driver, p2sb_methods, sizeof(struct p2sb_softc));
DRIVER_MODULE(p2sb, pci, p2sb_driver, 0, 0);
