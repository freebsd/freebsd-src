/*
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Reference: Intel 6300ESB Controller Hub Datasheet Section 16
 */

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/watchdog.h>

#include <dev/pci/pcireg.h>

#include <dev/ichwd/ichwd.h>
#include <dev/ichwd/i6300esbwd.h>

#include <x86/pci_cfgreg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

struct i6300esbwd_softc {
	device_t dev;
	int res_id;
	struct resource *res;
	eventhandler_tag ev_tag;
	bool locked;
};

static const struct i6300esbwd_pci_id {
	uint16_t id;
	const char *name;
} i6300esbwd_pci_devices[] = {
	{ DEVICEID_6300ESB_2, "6300ESB Watchdog Timer" },
};

static uint16_t __unused
i6300esbwd_cfg_read(struct i6300esbwd_softc *sc)
{
	return (pci_read_config(sc->dev, WDT_CONFIG_REG, 2));
}

static void
i6300esbwd_cfg_write(struct i6300esbwd_softc *sc, uint16_t val)
{
	pci_write_config(sc->dev, WDT_CONFIG_REG, val, 2);
}

static uint8_t
i6300esbwd_lock_read(struct i6300esbwd_softc *sc)
{
	return (pci_read_config(sc->dev, WDT_LOCK_REG, 1));
}

static void
i6300esbwd_lock_write(struct i6300esbwd_softc *sc, uint8_t val)
{
	pci_write_config(sc->dev, WDT_LOCK_REG, val, 1);
}

/*
 * According to Intel 6300ESB I/O Controller Hub Datasheet 16.5.2,
 * the resource should be unlocked before modifing any registers.
 * The way to unlock is by write 0x80, 0x86 to the reload register.
 */
static void
i6300esbwd_unlock_res(struct i6300esbwd_softc *sc)
{
	bus_write_2(sc->res, WDT_RELOAD_REG, WDT_UNLOCK_SEQ_1_VAL);
	bus_write_2(sc->res, WDT_RELOAD_REG, WDT_UNLOCK_SEQ_2_VAL);
}

static int
i6300esbwd_sysctl_locked(SYSCTL_HANDLER_ARGS)
{
	struct i6300esbwd_softc *sc = (struct i6300esbwd_softc *)arg1;
	int error;
	int result;

	result = sc->locked;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1 && !sc->locked) {
		i6300esbwd_lock_write(sc, i6300esbwd_lock_read(sc) | WDT_LOCK);
		sc->locked = true;
	}

	return (0);
}

static void
i6300esbwd_event(void *arg, unsigned int cmd, int *error)
{
	struct i6300esbwd_softc *sc = arg;
	uint32_t timeout;
	uint16_t regval;

	cmd &= WD_INTERVAL;
	if (cmd != 0 &&
	    (cmd < WD_TO_1MS || (cmd - WD_TO_1MS) >= WDT_PRELOAD_BIT)) {
		*error = EINVAL;
		return;
	}
	timeout = 1 << (cmd - WD_TO_1MS);

	/* reset the timer to prevent timeout a timeout is about to occur */
	i6300esbwd_unlock_res(sc);
	bus_write_2(sc->res, WDT_RELOAD_REG, WDT_RELOAD);

	if (!cmd) {
		/*
		 * when the lock is enabled, we are unable to overwrite LOCK
		 * register
		 */
		if (sc->locked)
			*error = EPERM;
		else
			i6300esbwd_lock_write(sc,
			    i6300esbwd_lock_read(sc) & ~WDT_ENABLE);
		return;
	}

	i6300esbwd_unlock_res(sc);
	bus_write_4(sc->res, WDT_PRELOAD_1_REG, timeout);

	i6300esbwd_unlock_res(sc);
	bus_write_4(sc->res, WDT_PRELOAD_2_REG, timeout);

	i6300esbwd_unlock_res(sc);
	bus_write_2(sc->res, WDT_RELOAD_REG, WDT_RELOAD);

	if (!sc->locked) {
		i6300esbwd_lock_write(sc, WDT_ENABLE);
		regval = i6300esbwd_lock_read(sc);
		sc->locked = regval & WDT_LOCK;
	}
}

static int
i6300esbwd_probe(device_t dev)
{
	const struct i6300esbwd_pci_id *pci_id;
	uint16_t pci_dev_id;
	int err = ENXIO;

	if (pci_get_vendor(dev) != VENDORID_INTEL)
		goto end;

	pci_dev_id = pci_get_device(dev);
	for (pci_id = i6300esbwd_pci_devices;
	    pci_id < i6300esbwd_pci_devices + nitems(i6300esbwd_pci_devices);
	    ++pci_id) {
		if (pci_id->id == pci_dev_id) {
			device_set_desc(dev, pci_id->name);
			err = BUS_PROBE_DEFAULT;
			break;
		}
	}

end:
	return (err);
}

static int
i6300esbwd_attach(device_t dev)
{
	struct i6300esbwd_softc *sc = device_get_softc(dev);
	uint16_t regval;

	sc->dev = dev;
	sc->res_id = PCIR_BAR(0);
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->res_id,
	    RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "unable to map memory region\n");
		return (ENXIO);
	}

	i6300esbwd_cfg_write(sc, WDT_INT_TYPE_DISABLED_VAL);
	regval = i6300esbwd_lock_read(sc);
	if (regval & WDT_LOCK)
		sc->locked = true;
	else {
		sc->locked = false;
		i6300esbwd_lock_write(sc, WDT_TOUT_CNF_WT_MODE);
	}

	i6300esbwd_unlock_res(sc);
	bus_write_2(sc->res, WDT_RELOAD_REG, WDT_RELOAD | WDT_TIMEOUT);

	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, i6300esbwd_event, sc,
	    0);

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "locked",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    i6300esbwd_sysctl_locked, "I",
	    "Lock the timer so that we cannot disable it");

	return (0);
}

static int
i6300esbwd_detach(device_t dev)
{
	struct i6300esbwd_softc *sc = device_get_softc(dev);

	if (sc->ev_tag)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);

	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->res_id, sc->res);

	return (0);
}

static device_method_t i6300esbwd_methods[] = {
	DEVMETHOD(device_probe, i6300esbwd_probe),
	DEVMETHOD(device_attach, i6300esbwd_attach),
	DEVMETHOD(device_detach, i6300esbwd_detach),
	DEVMETHOD(device_shutdown, i6300esbwd_detach),
	DEVMETHOD_END
};

static driver_t i6300esbwd_driver = {
	"i6300esbwd",
	i6300esbwd_methods,
	sizeof(struct i6300esbwd_softc),
};

DRIVER_MODULE(i6300esbwd, pci, i6300esbwd_driver, NULL, NULL);
