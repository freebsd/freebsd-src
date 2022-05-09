/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/simple_mfd.h>

device_t simple_mfd_add_device(device_t dev, phandle_t node, u_int order,
    const char *name, int unit, struct simplebus_devinfo *di);
struct simplebus_devinfo *simple_mfd_setup_dinfo(device_t dev, phandle_t node,
    struct simplebus_devinfo *di);

#include "syscon_if.h"
#include <dev/extres/syscon/syscon.h>

MALLOC_DECLARE(M_SYSCON);

static uint32_t simple_mfd_syscon_read_4(struct syscon *syscon,
    bus_size_t offset);
static int simple_mfd_syscon_write_4(struct syscon *syscon, bus_size_t offset,
    uint32_t val);
static int simple_mfd_syscon_modify_4(struct syscon *syscon, bus_size_t offset,
    uint32_t clear_bits, uint32_t set_bits);

#define	SYSCON_LOCK(_sc)		mtx_lock_spin(&(_sc)->mtx)
#define	SYSCON_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->mtx)
#define	SYSCON_LOCK_INIT(_sc)		mtx_init(&(_sc)->mtx,	\
    device_get_nameunit((_sc)->dev), "syscon", MTX_SPIN)
#define	SYSCON_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->mtx);
#define	SYSCON_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED);
#define	SYSCON_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_NOTOWNED);

static syscon_method_t simple_mfd_syscon_methods[] = {
	SYSCONMETHOD(syscon_unlocked_read_4,	simple_mfd_syscon_read_4),
	SYSCONMETHOD(syscon_unlocked_write_4,	simple_mfd_syscon_write_4),
	SYSCONMETHOD(syscon_unlocked_modify_4,	simple_mfd_syscon_modify_4),

	SYSCONMETHOD_END
};
DEFINE_CLASS_1(simple_mfd_syscon, simple_mfd_syscon_class,
    simple_mfd_syscon_methods, 0, syscon_class);

static uint32_t
simple_mfd_syscon_read_4(struct syscon *syscon, bus_size_t offset)
{
	struct simple_mfd_softc *sc;
	uint32_t val;

	sc = device_get_softc(syscon->pdev);
	SYSCON_ASSERT_LOCKED(sc);;
	val = bus_read_4(sc->mem_res, offset);
	return (val);
}

static int
simple_mfd_syscon_write_4(struct syscon *syscon, bus_size_t offset,
    uint32_t val)
{
	struct simple_mfd_softc *sc;

	sc = device_get_softc(syscon->pdev);
	SYSCON_ASSERT_LOCKED(sc);
	bus_write_4(sc->mem_res, offset, val);
	return (0);
}

static int
simple_mfd_syscon_modify_4(struct syscon *syscon, bus_size_t offset,
    uint32_t clear_bits, uint32_t set_bits)
{
	struct simple_mfd_softc *sc;
	uint32_t val;

	sc = device_get_softc(syscon->pdev);
	SYSCON_ASSERT_LOCKED(sc);
	val = bus_read_4(sc->mem_res, offset);
	val &= ~clear_bits;
	val |= set_bits;
	bus_write_4(sc->mem_res, offset, val);
	return (0);
}

static int
simple_mfd_syscon_get_handle(device_t dev, struct syscon **syscon)
{
	struct simple_mfd_softc *sc;

	sc = device_get_softc(dev);
	*syscon = sc->syscon;
	if (*syscon == NULL)
		return (ENODEV);
	return (0);
}

static void
simple_mfd_syscon_lock(device_t dev)
{
	struct simple_mfd_softc *sc;

	sc = device_get_softc(dev);
	SYSCON_LOCK(sc);
}

static void
simple_mfd_syscon_unlock(device_t dev)
{
	struct simple_mfd_softc *sc;

	sc = device_get_softc(dev);
	SYSCON_UNLOCK(sc);
}

static int
simple_mfd_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "simple-mfd"))
		return (ENXIO);

	device_set_desc(dev, "Simple MFD (Multi-Functions Device)");

	return (BUS_PROBE_GENERIC);
}

static int
simple_mfd_attach(device_t dev)
{
	struct simple_mfd_softc *sc;
	phandle_t node, child;
	device_t cdev;
	int rid;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	sc->dev = dev;
	rid = 0;

	/* Parse address-cells and size-cells from the parent node as a fallback */
	if (OF_getencprop(node, "#address-cells", &sc->sc.acells,
	    sizeof(sc->sc.acells)) == -1) {
		if (OF_getencprop(OF_parent(node), "#address-cells", &sc->sc.acells,
		    sizeof(sc->sc.acells)) == -1) {
			sc->sc.acells = 2;
		}
	}
	if (OF_getencprop(node, "#size-cells", &sc->sc.scells,
	    sizeof(sc->sc.scells)) == -1) {
		if (OF_getencprop(OF_parent(node), "#size-cells", &sc->sc.scells,
		    sizeof(sc->sc.scells)) == -1) {
			sc->sc.scells = 1;
		}
	}

	/* If the node has a ranges prop, parse it so children mapping will be done correctly */
	if (OF_hasprop(node, "ranges")) {
		if (simplebus_fill_ranges(node, &sc->sc) < 0) {
			device_printf(dev, "could not get ranges\n");
			return (ENXIO);
		}
	}

	/* Attach child devices */
	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		cdev = simple_mfd_add_device(dev, child, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}

	if (ofw_bus_is_compatible(dev, "syscon")) {
		sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
		    RF_ACTIVE);
		if (sc->mem_res == NULL) {
			device_printf(dev,
			    "Cannot allocate memory resource\n");
			return (ENXIO);
		}

		SYSCON_LOCK_INIT(sc);
		sc->syscon = syscon_create_ofw_node(dev,
		    &simple_mfd_syscon_class, ofw_bus_get_node(dev));
		if (sc->syscon == NULL) {
			device_printf(dev,
			    "Failed to create/register syscon\n");
			return (ENXIO);
		}
	}
	return (bus_generic_attach(dev));
}

static int
simple_mfd_detach(device_t dev)
{
	struct simple_mfd_softc *sc;

	sc = device_get_softc(dev);
	if (ofw_bus_is_compatible(dev, "syscon")) {
		if (sc->syscon != NULL) {
			syscon_unregister(sc->syscon);
			free(sc->syscon, M_SYSCON);
		}

		SYSCON_LOCK_DESTROY(sc);

		if (sc->mem_res != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY, 0,
			    sc->mem_res);
	}
	return (0);
}

struct simplebus_devinfo *
simple_mfd_setup_dinfo(device_t dev, phandle_t node,
    struct simplebus_devinfo *di)
{
	struct simplebus_softc *sc;
	struct simplebus_devinfo *ndi;

	sc = device_get_softc(dev);
	if (di == NULL)
		ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	else
		ndi = di;
	if (ofw_bus_gen_setup_devinfo(&ndi->obdinfo, node) != 0) {
		if (di == NULL)
			free(ndi, M_DEVBUF);
		return (NULL);
	}

	/* reg resources is from the parent but interrupts is on the node itself */
	resource_list_init(&ndi->rl);
	ofw_bus_reg_to_rl(dev, OF_parent(node), sc->acells, sc->scells, &ndi->rl);
	ofw_bus_intr_to_rl(dev, node, &ndi->rl, NULL);

	return (ndi);
}

device_t
simple_mfd_add_device(device_t dev, phandle_t node, u_int order,
    const char *name, int unit, struct simplebus_devinfo *di)
{
	struct simplebus_devinfo *ndi;
	device_t cdev;

	if ((ndi = simple_mfd_setup_dinfo(dev, node, di)) == NULL)
		return (NULL);
	cdev = device_add_child_ordered(dev, order, name, unit);
	if (cdev == NULL) {
		device_printf(dev, "<%s>: device_add_child failed\n",
		    ndi->obdinfo.obd_name);
		resource_list_free(&ndi->rl);
		ofw_bus_gen_destroy_devinfo(&ndi->obdinfo);
		if (di == NULL)
			free(ndi, M_DEVBUF);
		return (NULL);
	}
	device_set_ivars(cdev, ndi);

	return(cdev);
}

static device_method_t simple_mfd_methods[] = {
	/* syscon interface */
	DEVMETHOD(syscon_get_handle,	simple_mfd_syscon_get_handle),
	DEVMETHOD(syscon_device_lock,	simple_mfd_syscon_lock),
	DEVMETHOD(syscon_device_unlock,	simple_mfd_syscon_unlock),

	/* Device interface */
	DEVMETHOD(device_probe,		simple_mfd_probe),
	DEVMETHOD(device_attach,	simple_mfd_attach),
	DEVMETHOD(device_detach,	simple_mfd_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(simple_mfd, simple_mfd_driver, simple_mfd_methods,
  sizeof(struct simple_mfd_softc), simplebus_driver);

EARLY_DRIVER_MODULE(simple_mfd, simplebus, simple_mfd_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
MODULE_VERSION(simple_mfd, 1);
