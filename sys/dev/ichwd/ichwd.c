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
 *
 * ICH6/7/8 support by Takeharu KATO <takeharu1219@ybb.ne.jp>
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
	{ DEVICEID_82801AA,  "Intel 82801AA watchdog timer",    1 },
	{ DEVICEID_82801AB,  "Intel 82801AB watchdog timer",    1 },
	{ DEVICEID_82801BA,  "Intel 82801BA watchdog timer",    2 },
	{ DEVICEID_82801BAM, "Intel 82801BAM watchdog timer",   2 },
	{ DEVICEID_82801CA,  "Intel 82801CA watchdog timer",    3 },
	{ DEVICEID_82801CAM, "Intel 82801CAM watchdog timer",   3 },
	{ DEVICEID_82801DB,  "Intel 82801DB watchdog timer",    4 },
	{ DEVICEID_82801DBM, "Intel 82801DBM watchdog timer",   4 },
	{ DEVICEID_82801E,   "Intel 82801E watchdog timer",     5 },
	{ DEVICEID_82801EBR, "Intel 82801EB/ER watchdog timer", 5 },
	{ DEVICEID_6300ESB,  "Intel 6300ESB watchdog timer",    5 },
	{ DEVICEID_82801FBR, "Intel 82801FB/FR watchdog timer", 6 },
	{ DEVICEID_ICH6M,    "Intel ICH6M watchdog timer",      6 },
	{ DEVICEID_ICH6W,    "Intel ICH6W watchdog timer",      6 },
	{ DEVICEID_ICH7,     "Intel ICH7 watchdog timer",       7 },
	{ DEVICEID_ICH7M,    "Intel ICH7M watchdog timer",      7 },
	{ DEVICEID_ICH7MDH,  "Intel ICH7MDH watchdog timer",    7 },
	{ DEVICEID_ICH8,     "Intel ICH8 watchdog timer",       8 },
	{ DEVICEID_ICH8DH,   "Intel ICH8DH watchdog timer",     8 },
	{ DEVICEID_ICH8DO,   "Intel ICH8DO watchdog timer",     8 },
	{ 0, NULL, 0 },
};

static devclass_t ichwd_devclass;

#define ichwd_read_tco_1(sc, off) \
	bus_space_read_1((sc)->tco_bst, (sc)->tco_bsh, (off))
#define ichwd_read_tco_2(sc, off) \
	bus_space_read_2((sc)->tco_bst, (sc)->tco_bsh, (off))
#define ichwd_read_tco_4(sc, off) \
	bus_space_read_4((sc)->tco_bst, (sc)->tco_bsh, (off))
#define ichwd_read_smi_4(sc, off) \
	bus_space_read_4((sc)->smi_bst, (sc)->smi_bsh, (off))
#define ichwd_read_gcs_4(sc, off) \
	bus_space_read_4((sc)->gcs_bst, (sc)->gcs_bsh, (off))

#define ichwd_write_tco_1(sc, off, val) \
	bus_space_write_1((sc)->tco_bst, (sc)->tco_bsh, (off), (val))
#define ichwd_write_tco_2(sc, off, val) \
	bus_space_write_2((sc)->tco_bst, (sc)->tco_bsh, (off), (val))
#define ichwd_write_tco_4(sc, off, val) \
	bus_space_write_4((sc)->tco_bst, (sc)->tco_bsh, (off), (val))
#define ichwd_write_smi_4(sc, off, val) \
	bus_space_write_4((sc)->smi_bst, (sc)->smi_bsh, (off), (val))
#define ichwd_write_gcs_4(sc, off, val) \
	bus_space_write_4((sc)->gcs_bst, (sc)->gcs_bsh, (off), (val))

#define ichwd_verbose_printf(dev, ...) \
	do {						\
		if (bootverbose)			\
			device_printf(dev, __VA_ARGS__);\
	} while (0)

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
	ichwd_verbose_printf(sc->device, "timer enabled\n");
}

static __inline void
ichwd_tmr_disable(struct ichwd_softc *sc)
{
	uint16_t cnt;

	cnt = ichwd_read_tco_2(sc, TCO1_CNT) & TCO_CNT_PRESERVE;
	ichwd_write_tco_2(sc, TCO1_CNT, cnt | TCO_TMR_HALT);
	sc->active = 0;
	ichwd_verbose_printf(sc->device, "timer disabled\n");
}

static __inline void
ichwd_tmr_reload(struct ichwd_softc *sc)
{
	if (sc->ich_version <= 5)
		ichwd_write_tco_1(sc, TCO_RLD, 1);
	else
		ichwd_write_tco_2(sc, TCO_RLD, 1);

	ichwd_verbose_printf(sc->device, "timer reloaded\n");
}

static __inline void
ichwd_tmr_set(struct ichwd_softc *sc, unsigned int timeout)
{

	/*
	 * If the datasheets are to be believed, the minimum value
	 * actually varies from chipset to chipset - 4 for ICH5 and 2 for
	 * all other chipsets.  I suspect this is a bug in the ICH5
	 * datasheet and that the minimum is uniformly 2, but I'd rather
	 * err on the side of caution.
	 */
	if (timeout < 4)
		timeout = 4;

	if (sc->ich_version <= 5) {
		uint8_t tmr_val8 = ichwd_read_tco_1(sc, TCO_TMR1);

		tmr_val8 &= 0xc0;
		if (timeout > 0xbf)
			timeout = 0xbf;
		tmr_val8 |= timeout;
		ichwd_write_tco_1(sc, TCO_TMR1, tmr_val8);
	} else {
		uint16_t tmr_val16 = ichwd_read_tco_2(sc, TCO_TMR2);

		tmr_val16 &= 0xfc00;
		if (timeout > 0x0bff)
			timeout = 0x0bff;
		tmr_val16 |= timeout;
		ichwd_write_tco_2(sc, TCO_TMR2, tmr_val16);
	}

	sc->timeout = timeout;

	ichwd_verbose_printf(sc->device, "timeout set to %u ticks\n", timeout);
}

static __inline int
ichwd_clear_noreboot(struct ichwd_softc *sc)
{
	uint32_t status;
	int rc = 0;

	/* try to clear the NO_REBOOT bit */
	if (sc->ich_version <= 5) {
		status = pci_read_config(sc->ich, ICH_GEN_STA, 1);
		status &= ~ICH_GEN_STA_NO_REBOOT;
		pci_write_config(sc->ich, ICH_GEN_STA, status, 1);
		status = pci_read_config(sc->ich, ICH_GEN_STA, 1);
		if (status & ICH_GEN_STA_NO_REBOOT)
			rc = EIO;
	} else {
		status = ichwd_read_gcs_4(sc, 0);
		status &= ~ICH_GCS_NO_REBOOT;
		ichwd_write_gcs_4(sc, 0, status);
		status = ichwd_read_gcs_4(sc, 0);
		if (status & ICH_GCS_NO_REBOOT)
			rc = EIO;
	}

	if (rc)
		device_printf(sc->device,
		    "ICH WDT present but disabled in BIOS or hardware\n");

	return (rc);
}

/*
 * Watchdog event handler.
 */
static void
ichwd_event(void *arg, unsigned int cmd, int *error)
{
	struct ichwd_softc *sc = arg;
	unsigned int timeout;

	/* convert from power-of-two-ns to WDT ticks */
	cmd &= WD_INTERVAL;
	timeout = ((uint64_t)1 << cmd) / ICHWD_TICK;
	if (cmd) {
		if (timeout != sc->timeout) {
			if (!sc->active)
				ichwd_tmr_enable(sc);
			ichwd_tmr_set(sc, timeout);
		}
		ichwd_tmr_reload(sc);
		*error = 0;
	} else {
		if (sc->active)
			ichwd_tmr_disable(sc);
	}
}

static device_t
ichwd_find_ich_lpc_bridge(struct ichwd_device **id_p)
{
	struct ichwd_device *id;
	device_t ich = NULL;

	/* look for an ICH LPC interface bridge */
	for (id = ichwd_devices; id->desc != NULL; ++id)
		if ((ich = pci_find_device(VENDORID_INTEL, id->device)) != NULL)
			break;

	if (ich == NULL)
		return (NULL);

	ichwd_verbose_printf(ich, "found ICH%d or equivalent chipset: %s\n",
	    id->version, id->desc);

	if (id_p)
		*id_p = id;

	return (ich);
}

/*
 * Look for an ICH LPC interface bridge.  If one is found, register an
 * ichwd device.  There can be only one.
 */
static void
ichwd_identify(driver_t *driver, device_t parent)
{
	struct ichwd_device *id_p;
	device_t ich = NULL;
	device_t dev;
	uint32_t rcba;
	int rc;

	ich = ichwd_find_ich_lpc_bridge(&id_p);
	if (ich == NULL)
		return;

	/* good, add child to bus */
	if ((dev = device_find_child(parent, driver->name, 0)) == NULL)
		dev = BUS_ADD_CHILD(parent, 0, driver->name, 0);

	if (dev == NULL)
		return;

	device_set_desc_copy(dev, id_p->desc);

	if (id_p->version >= 6) {
		/* get RCBA (root complex base address) */
		rcba = pci_read_config(ich, ICH_RCBA, 4);
		rc = bus_set_resource(ich, SYS_RES_MEMORY, 0,
		    (rcba & 0xffffc000) + ICH_GCS_OFFSET, ICH_GCS_SIZE);
		if (rc)
			ichwd_verbose_printf(dev,
			    "Can not set memory resource for RCBA\n");
	}
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
	struct ichwd_device *id_p;
	device_t ich;
	unsigned int pmbase = 0;

	sc = device_get_softc(dev);
	sc->device = dev;

	ich = ichwd_find_ich_lpc_bridge(&id_p);
	if (ich == NULL) {
		device_printf(sc->device, "Can not find ICH device.\n");
		goto fail;
	}
	sc->ich = ich;
	sc->ich_version = id_p->version;

	/* get ACPI base address */
	pmbase = pci_read_config(ich, ICH_PMBASE, 2) & ICH_PMBASE_MASK;
	if (pmbase == 0) {
		device_printf(dev, "ICH PMBASE register is empty\n");
		goto fail;
	}

	/* allocate I/O register space */
	sc->smi_rid = 0;
	sc->smi_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->smi_rid,
	    pmbase + SMI_BASE, pmbase + SMI_BASE + SMI_LEN - 1, SMI_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->smi_res == NULL) {
		device_printf(dev, "unable to reserve SMI registers\n");
		goto fail;
	}
	sc->smi_bst = rman_get_bustag(sc->smi_res);
	sc->smi_bsh = rman_get_bushandle(sc->smi_res);

	sc->tco_rid = 1;
	sc->tco_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->tco_rid,
	    pmbase + TCO_BASE, pmbase + TCO_BASE + TCO_LEN - 1, TCO_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->tco_res == NULL) {
		device_printf(dev, "unable to reserve TCO registers\n");
		goto fail;
	}
	sc->tco_bst = rman_get_bustag(sc->tco_res);
	sc->tco_bsh = rman_get_bushandle(sc->tco_res);

	sc->gcs_rid = 0;
	if (sc->ich_version >= 6) {
		sc->gcs_res = bus_alloc_resource_any(ich, SYS_RES_MEMORY,
		    &sc->gcs_rid, RF_ACTIVE|RF_SHAREABLE);
		if (sc->gcs_res == NULL) {
			device_printf(dev, "unable to reserve GCS registers\n");
			goto fail;
		}
		sc->gcs_bst = rman_get_bustag(sc->gcs_res);
		sc->gcs_bsh = rman_get_bushandle(sc->gcs_res);
	} else {
		sc->gcs_res = 0;
		sc->gcs_bst = 0;
		sc->gcs_bsh = 0;
	}

	if (ichwd_clear_noreboot(sc) != 0)
		goto fail;

	device_printf(dev, "%s (ICH%d or equivalent)\n",
	    device_get_desc(dev), sc->ich_version);

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
	if (sc->gcs_res != NULL)
		bus_release_resource(ich, SYS_RES_MEMORY,
		    sc->gcs_rid, sc->gcs_res);

	return (ENXIO);
}

static int
ichwd_detach(device_t dev)
{
	struct ichwd_softc *sc;
	device_t ich = NULL;

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

	/* deallocate memory resource */
	ich = ichwd_find_ich_lpc_bridge(NULL);
	if (sc->gcs_res && ich)
		bus_release_resource(ich, SYS_RES_MEMORY, sc->gcs_rid, sc->gcs_res);

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
