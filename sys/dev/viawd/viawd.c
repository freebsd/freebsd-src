/*-
 * Copyright (c) 2011 Fabien Thomas <fthomas@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/watchdog.h>

#include <isa/isavar.h>
#include <dev/pci/pcivar.h>

#include "viawd.h"

#define viawd_read_wd_4(sc, off) \
	bus_space_read_4((sc)->wd_bst, (sc)->wd_bsh, (off))
#define viawd_write_wd_4(sc, off, val) \
	bus_space_write_4((sc)->wd_bst, (sc)->wd_bsh, (off), (val))

static struct viawd_device viawd_devices[] = {
	{ DEVICEID_VT8251, "VIA VT8251 watchdog timer" },
	{ DEVICEID_CX700,  "VIA CX700 watchdog timer" },
	{ DEVICEID_VX800,  "VIA VX800 watchdog timer" },
	{ DEVICEID_VX855,  "VIA VX855 watchdog timer" },
	{ DEVICEID_VX900,  "VIA VX900 watchdog timer" },
	{ 0, NULL },
};

static devclass_t viawd_devclass;

static device_t
viawd_find(struct viawd_device **id_p)
{
	struct viawd_device *id;
	device_t sb_dev = NULL;

	/* Look for a supported VIA south bridge. */
	for (id = viawd_devices; id->desc != NULL; ++id)
		if ((sb_dev = pci_find_device(VENDORID_VIA, id->device)) != NULL)
			break;

	if (sb_dev == NULL)
		return (NULL);

	if (id_p != NULL)
		*id_p = id;

	return (sb_dev);
}

static void
viawd_tmr_state(struct viawd_softc *sc, int enable)
{
	uint32_t reg;

	reg = viawd_read_wd_4(sc, VIAWD_MEM_CTRL);
	if (enable)
		reg |= VIAWD_MEM_CTRL_TRIGGER | VIAWD_MEM_CTRL_ENABLE;
	else
		reg &= ~VIAWD_MEM_CTRL_ENABLE;
	viawd_write_wd_4(sc, VIAWD_MEM_CTRL, reg);
}

static void
viawd_tmr_set(struct viawd_softc *sc, unsigned int timeout)
{

	/* Keep value in range. */
	if (timeout < VIAWD_MEM_COUNT_MIN)
		timeout = VIAWD_MEM_COUNT_MIN;
	else if (timeout > VIAWD_MEM_COUNT_MAX)
		timeout = VIAWD_MEM_COUNT_MAX;

	viawd_write_wd_4(sc, VIAWD_MEM_COUNT, timeout);
	sc->timeout = timeout;
}

/*
 * Watchdog event handler - called by the framework to enable or disable
 * the watchdog or change the initial timeout value.
 */
static void
viawd_event(void *arg, unsigned int cmd, int *error)
{
	struct viawd_softc *sc = arg;
	unsigned int timeout;

	/* Convert from power-of-two-ns to second. */
	cmd &= WD_INTERVAL;
	timeout = ((uint64_t)1 << cmd) / 1000000000;
	if (cmd) {
		if (timeout != sc->timeout)
			viawd_tmr_set(sc, timeout);
		viawd_tmr_state(sc, 1);
		*error = 0;
	} else
		viawd_tmr_state(sc, 0);
}

static void
viawd_identify(driver_t *driver, device_t parent)
{
	device_t dev;
	device_t sb_dev;
	struct viawd_device *id_p;

	sb_dev = viawd_find(&id_p);
	if (sb_dev == NULL)
		return;

	/* Good, add child to bus. */
	if ((dev = device_find_child(parent, driver->name, 0)) == NULL)
		dev = BUS_ADD_CHILD(parent, 0, driver->name, 0);

	if (dev == NULL)
		return;

	device_set_desc_copy(dev, id_p->desc);
}

static int
viawd_probe(device_t dev)
{

	/* Do not claim some ISA PnP device by accident. */
	if (isa_get_logicalid(dev) != 0)
		return (ENXIO);
	return (0);
}

static int
viawd_attach(device_t dev)
{
	device_t sb_dev;
	struct viawd_softc *sc;
	struct viawd_device *id_p;
	uint32_t pmbase, reg;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sb_dev = viawd_find(&id_p);
	if (sb_dev == NULL) {
		device_printf(dev, "Can not find watchdog device.\n");
		goto fail;
	}
	sc->sb_dev = sb_dev;

	/* Get watchdog memory base. */
	pmbase = pci_read_config(sb_dev, VIAWD_CONFIG_BASE, 4);
	if (pmbase == 0) {
		device_printf(dev,
		    "Watchdog disabled in BIOS or hardware\n");
		goto fail;
	}

	/* Allocate I/O register space. */
	sc->wd_rid = 0;
	sc->wd_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->wd_rid,
	    pmbase, pmbase + VIAWD_MEM_LEN - 1, VIAWD_MEM_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->wd_res == NULL) {
		device_printf(dev, "Unable to map watchdog memory\n");
		goto fail;
	}
	sc->wd_bst = rman_get_bustag(sc->wd_res);
	sc->wd_bsh = rman_get_bushandle(sc->wd_res);

	/* Check if watchdog fired last boot. */
	reg = viawd_read_wd_4(sc, VIAWD_MEM_CTRL);
	if (reg & VIAWD_MEM_CTRL_FIRED) {
		device_printf(dev,
		    "ERROR: watchdog rebooted the system\n");
		/* Reset bit state. */
		viawd_write_wd_4(sc, VIAWD_MEM_CTRL, reg);
	}

	/* Register the watchdog event handler. */
	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, viawd_event, sc, 0);

	return (0);
fail:
	if (sc->wd_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->wd_rid, sc->wd_res);
	return (ENXIO);
}

static int
viawd_detach(device_t dev)
{
	struct viawd_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	/* Deregister event handler. */
	if (sc->ev_tag != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);
	sc->ev_tag = NULL;

	/*
	 * Do not stop the watchdog on shutdown if active but bump the
	 * timer to avoid spurious reset.
	 */
	reg = viawd_read_wd_4(sc, VIAWD_MEM_CTRL);
	if (reg & VIAWD_MEM_CTRL_ENABLE) {
		viawd_tmr_set(sc, VIAWD_TIMEOUT_SHUTDOWN);
		viawd_tmr_state(sc, 1);
		device_printf(dev,
		    "Keeping watchog alive during shutdown for %d seconds\n",
		    VIAWD_TIMEOUT_SHUTDOWN);
	}

	if (sc->wd_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    sc->wd_rid, sc->wd_res);

	return (0);
}

static device_method_t viawd_methods[] = {
	DEVMETHOD(device_identify, viawd_identify),
	DEVMETHOD(device_probe,	viawd_probe),
	DEVMETHOD(device_attach, viawd_attach),
	DEVMETHOD(device_detach, viawd_detach),
	DEVMETHOD(device_shutdown, viawd_detach),
	{0,0}
};

static driver_t viawd_driver = {
	"viawd",
	viawd_methods,
	sizeof(struct viawd_softc),
};

static int
viawd_modevent(module_t mode, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("viawd module loaded\n");
		break;
	case MOD_UNLOAD:
		printf("viawd module unloaded\n");
		break;
	case MOD_SHUTDOWN:
		printf("viawd module shutting down\n");
		break;
	}
	return (error);
}

DRIVER_MODULE(viawd, isa, viawd_driver, viawd_devclass, viawd_modevent, NULL);
