/*-
 * Copyright (c) 2004 Texas A&M University
 * All rights reserved.
 *
 * Developer: Wm. Daryl Hawkins
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
 * Intel ICH Watchdog Timer (WDT) driver
 *
 * Originally developed by Wm. Daryl Hawkins of Texas A&M
 * Heavily modified by <des@FreeBSD.org>
 *
 * This is a tricky one.  The ICH WDT can't be treated as a regular PCI
 * device as it's actually an integrated function of the ICH LPC interface
 * bridge.  Detection is also awkward, because we can only infer the
 * presence of the watchdog timer from the fact that the machine has an
 * ICH chipset, or, on ACPI 2.x systems, by the presence of the 'WDDT'
 * ACPI table (although this driver does not support the ACPI detection
 * method).
 *
 * There is one slight problem on non-ACPI or ACPI 1.x systems: we have no
 * way of knowing if the WDT is permanently disabled (either by the BIOS
 * or in hardware).
 *
 * The WDT is programmed through I/O registers in the ACPI I/O space.
 * Intel swears it's always at offset 0x60, so we use that.
 *
 * For details about the ICH WDT, see Intel Application Note AP-725
 * (document no. 292273-001).  The WDT is also described in the individual
 * chipset datasheets, e.g. Intel82801EB ICH5 / 82801ER ICH5R Datasheet
 * (document no. 252516-001) sections 9.10 and 9.11.
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

#include <dev/pci/pcivar.h>

#include <dev/ichwd/ichwd.h>

static struct ichwd_device ichwd_devices[] = {
	{ VENDORID_INTEL, DEVICEID_82801AA, "Intel 82801AA watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801AB, "Intel 82801AB watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801BA, "Intel 82801BA watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801BAM, "Intel 82801BAM watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801CA, "Intel 82801CA watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801CAM, "Intel 82801CAM watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801DB, "Intel 82801DB watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801DBM, "Intel 82801DBM watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801E, "Intel 82801E watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801EBR, "Intel 82801EB/ER watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_82801FBR, "Intel 82801FB/FR watchdog timer" },
	{ VENDORID_INTEL, DEVICEID_ICH5, "Intel ICH5 watchdog timer"},
	{ VENDORID_INTEL, DEVICEID_6300ESB, "Intel 6300ESB watchdog timer"},
	{ 0, 0, NULL },
};

static devclass_t ichwd_devclass;

#define ichwd_read_tco_1(sc, off) \
	bus_space_read_1((sc)->tco_bst, (sc)->tco_bsh, (off))
#define ichwd_read_tco_2(sc, off) \
	bus_space_read_2((sc)->tco_bst, (sc)->tco_bsh, (off))
#define ichwd_read_tco_4(sc, off) \
	bus_space_read_4((sc)->tco_bst, (sc)->tco_bsh, (off))

#define ichwd_write_tco_1(sc, off, val) \
	bus_space_write_1((sc)->tco_bst, (sc)->tco_bsh, (off), (val))
#define ichwd_write_tco_2(sc, off, val) \
	bus_space_write_2((sc)->tco_bst, (sc)->tco_bsh, (off), (val))
#define ichwd_write_tco_4(sc, off, val) \
	bus_space_write_4((sc)->tco_bst, (sc)->tco_bsh, (off), (val))

#define ichwd_read_smi_4(sc, off) \
	bus_space_read_4((sc)->smi_bst, (sc)->smi_bsh, (off))
#define ichwd_write_smi_4(sc, off, val) \
	bus_space_write_4((sc)->smi_bst, (sc)->smi_bsh, (off), (val))

static __inline void
ichwd_intr_enable(struct ichwd_softc *sc)
{
	ichwd_write_smi_4(sc, SMI_EN, ichwd_read_smi_4(sc, SMI_EN) & ~SMI_TCO_EN);
}

static __inline void
ichwd_intr_disable(struct ichwd_softc *sc)
{
	ichwd_write_smi_4(sc, SMI_EN, ichwd_read_smi_4(sc, SMI_EN) | SMI_TCO_EN);
}

static __inline void
ichwd_sts_reset(struct ichwd_softc *sc)
{
	ichwd_write_tco_2(sc, TCO1_STS, TCO_TIMEOUT);
	ichwd_write_tco_2(sc, TCO2_STS, TCO_BOOT_STS);
	ichwd_write_tco_2(sc, TCO2_STS, TCO_SECOND_TO_STS);
}

static __inline void
ichwd_tmr_enable(struct ichwd_softc *sc)
{
	uint16_t cnt;

	cnt = ichwd_read_tco_2(sc, TCO1_CNT) & TCO_CNT_PRESERVE;
	ichwd_write_tco_2(sc, TCO1_CNT, cnt & ~TCO_TMR_HALT);
	sc->active = 1;
	if (bootverbose)
		device_printf(sc->device, "timer enabled\n");
}

static __inline void
ichwd_tmr_disable(struct ichwd_softc *sc)
{
	uint16_t cnt;

	cnt = ichwd_read_tco_2(sc, TCO1_CNT) & TCO_CNT_PRESERVE;
	ichwd_write_tco_2(sc, TCO1_CNT, cnt | TCO_TMR_HALT);
	sc->active = 0;
	if (bootverbose)
		device_printf(sc->device, "timer disabled\n");
}

static __inline void
ichwd_tmr_reload(struct ichwd_softc *sc)
{
	ichwd_write_tco_1(sc, TCO_RLD, 1);
	if (bootverbose)
		device_printf(sc->device, "timer reloaded\n");
}

static __inline void
ichwd_tmr_set(struct ichwd_softc *sc, uint8_t timeout)
{
	ichwd_write_tco_1(sc, TCO_TMR, timeout);
	sc->timeout = timeout;
	if (bootverbose)
		device_printf(sc->device, "timeout set to %u ticks\n", timeout);
}

/*
 * Watchdog event handler.
 */
static void
ichwd_event(void *arg, unsigned int cmd, int *error)
{
	struct ichwd_softc *sc = arg;
	unsigned int timeout;


	/* disable / enable */
	if (!(cmd & WD_ACTIVE)) {
		if (sc->active)
			ichwd_tmr_disable(sc);
		*error = 0;
		return;
	}
	if (!sc->active)
		ichwd_tmr_enable(sc);

	cmd &= WD_INTERVAL;
	/* convert from power-of-to-ns to WDT ticks */
	if (cmd >= 64) {
		*error = EINVAL;
		return;
	}
	timeout = ((uint64_t)1 << cmd) / ICHWD_TICK;
	if (timeout < ICHWD_MIN_TIMEOUT || timeout > ICHWD_MAX_TIMEOUT) {
		*error = EINVAL;
		return;
	}

	/* set new initial value */
	if (timeout != sc->timeout)
		ichwd_tmr_set(sc, timeout);

	/* reload */
	ichwd_tmr_reload(sc);

	*error = 0;
	return;
}

static unsigned int pmbase = 0;

/*
 * Look for an ICH LPC interface bridge.  If one is found, register an
 * ichwd device.  There can be only one.
 */
static void
ichwd_identify(driver_t *driver, device_t parent)
{
	struct ichwd_device *id;
	device_t ich = NULL;
	device_t dev;

	/* look for an ICH LPC interface bridge */
	for (id = ichwd_devices; id->desc != NULL; ++id)
		if ((ich = pci_find_device(id->vendor, id->device)) != NULL)
			break;
	if (ich == NULL)
		return;

	if (bootverbose)
		printf("%s(): found ICH chipset: %s\n", __func__, id->desc);

	/* get for ACPI base address */
	pmbase = pci_read_config(ich, ICH_PMBASE, 2) & ICH_PMBASE_MASK;
	if (pmbase == 0) {
		if (bootverbose)
			printf("%s(): ICH PMBASE register is empty\n",
			    __func__);
		return;
	}

	/* try to clear the NO_REBOOT bit */
	pci_write_config(ich, ICH_GEN_STA, 0x00, 1);
	if (pci_read_config(ich, ICH_GEN_STA, 1) & ICH_GEN_STA_NO_REBOOT) {
		if (bootverbose)
			printf("%s(): ICH WDT present but disabled\n",
			    __func__);
		return;
	}

	/* good, add child to bus */
	if ((dev = device_find_child(parent, driver->name, 0)) == NULL)
		dev = BUS_ADD_CHILD(parent, 0, driver->name, 0);

	if (dev != NULL)
		device_set_desc_copy(dev, id->desc);
}

static int
ichwd_probe(device_t dev)
{
	(void)dev;
	return (0);
}

static int
ichwd_attach(device_t dev)
{
	struct ichwd_softc *sc;

	sc = device_get_softc(dev);
	sc->device = dev;

	if (pmbase == 0) {
		printf("Not found\n");
	}

	/* allocate I/O register space */
	sc->smi_rid = 0;
	sc->smi_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->smi_rid,
	    pmbase + SMI_BASE, ~0ul, SMI_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->smi_res == NULL) {
		device_printf(dev, "unable to reserve SMI registers\n");
		goto fail;
	}
	sc->smi_bst = rman_get_bustag(sc->smi_res);
	sc->smi_bsh = rman_get_bushandle(sc->smi_res);

	sc->tco_rid = 1;
	sc->tco_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->tco_rid,
	    pmbase + TCO_BASE, ~0ul, TCO_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->tco_res == NULL) {
		device_printf(dev, "unable to reserve TCO registers\n");
		goto fail;
	}
	sc->tco_bst = rman_get_bustag(sc->tco_res);
	sc->tco_bsh = rman_get_bushandle(sc->tco_res);
	/* reset the watchdog status registers */

	ichwd_sts_reset(sc);

	/* make sure the WDT starts out inactive */
	ichwd_tmr_disable(sc);

	/* register the watchdog event handler */
	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, ichwd_event, sc, 0);

	/* enable watchdog timeout interrupts */
	ichwd_intr_enable(sc);

	return (0);
 fail:
	sc = device_get_softc(dev);
	if (sc->tco_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->tco_rid, sc->tco_res);
	if (sc->smi_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->smi_rid, sc->smi_res);
	return (ENXIO);
}

static int
ichwd_detach(device_t dev)
{
	struct ichwd_softc *sc;

	device_printf(dev, "detaching\n");

	sc = device_get_softc(dev);

	/* halt the watchdog timer */
	if (sc->active)
		ichwd_tmr_disable(sc);

	/* disable watchdog timeout interrupts */
	ichwd_intr_disable(sc);

	/* deregister event handler */
	if (sc->ev_tag != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);
	sc->ev_tag = NULL;

	/* reset the watchdog status registers */
	ichwd_sts_reset(sc);

	/* deallocate I/O register space */
	bus_release_resource(dev, SYS_RES_IOPORT, sc->tco_rid, sc->tco_res);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->smi_rid, sc->smi_res);

	return (0);
}

static device_method_t ichwd_methods[] = {
	DEVMETHOD(device_identify, ichwd_identify),
	DEVMETHOD(device_probe,	ichwd_probe),
	DEVMETHOD(device_attach, ichwd_attach),
	DEVMETHOD(device_detach, ichwd_detach),
	DEVMETHOD(device_shutdown, ichwd_detach),
	{0,0}
};

static driver_t ichwd_driver = {
	"ichwd",
	ichwd_methods,
	sizeof(struct ichwd_softc),
};

static int
ichwd_modevent(module_t mode, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("ichwd module loaded\n");
		break;
	case MOD_UNLOAD:
		printf("ichwd module unloaded\n");
		break;
	case MOD_SHUTDOWN:
		printf("ichwd module shutting down\n");
		break;
	}
	return (error);
}

DRIVER_MODULE(ichwd, isa, ichwd_driver, ichwd_devclass, ichwd_modevent, NULL);
