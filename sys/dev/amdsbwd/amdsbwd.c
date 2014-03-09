/*-
 * Copyright (c) 2009 Andriy Gapon <avg@FreeBSD.org>
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
 * This is a driver for watchdog timer present in AMD SB600/SB7xx/SB8xx
 * southbridges.
 * Please see the following specifications for the descriptions of the
 * registers and flags:
 * - AMD SB600 Register Reference Guide, Public Version,  Rev. 3.03 (SB600 RRG)
 *   http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/46155_sb600_rrg_pub_3.03.pdf
 * - AMD SB700/710/750 Register Reference Guide (RRG)
 *   http://developer.amd.com/assets/43009_sb7xx_rrg_pub_1.00.pdf
 * - AMD SB700/710/750 Register Programming Requirements (RPR)
 *   http://developer.amd.com/assets/42413_sb7xx_rpr_pub_1.00.pdf
 * - AMD SB800-Series Southbridges Register Reference Guide (RRG)
 *   http://support.amd.com/us/Embedded_TechDocs/45482.pdf
 * Please see the following for Watchdog Resource Table specification:
 * - Watchdog Timer Hardware Requirements for Windows Server 2003 (WDRT)
 *   http://www.microsoft.com/whdc/system/sysinternals/watchdog.mspx
 * AMD SB600/SB7xx/SB8xx watchdog hardware seems to conform to the above
 * specifications, but the table hasn't been spotted in the wild yet.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/watchdog.h>

#include <dev/pci/pcivar.h>
#include <isa/isavar.h>

/* SB7xx RRG 2.3.3.1.1. */
#define	AMDSB_PMIO_INDEX		0xcd6
#define	AMDSB_PMIO_DATA			(PMIO_INDEX + 1)
#define	AMDSB_PMIO_WIDTH		2
/* SB7xx RRG 2.3.3.2. */
#define	AMDSB_PM_RESET_STATUS0		0x44
#define	AMDSB_PM_RESET_STATUS1		0x45
#define		AMDSB_WD_RST_STS	0x02
/* SB7xx RRG 2.3.3.2, RPR 2.36. */
#define	AMDSB_PM_WDT_CTRL		0x69
#define		AMDSB_WDT_DISABLE	0x01
#define		AMDSB_WDT_RES_MASK	(0x02 | 0x04)
#define		AMDSB_WDT_RES_32US	0x00
#define		AMDSB_WDT_RES_10MS	0x02
#define		AMDSB_WDT_RES_100MS	0x04
#define		AMDSB_WDT_RES_1S	0x06
#define	AMDSB_PM_WDT_BASE_LSB		0x6c
#define	AMDSB_PM_WDT_BASE_MSB		0x6f
/* SB8xx RRG 2.3.3. */
#define	AMDSB8_PM_WDT_EN		0x48
#define		AMDSB8_WDT_DEC_EN	0x01
#define		AMDSB8_WDT_DISABLE	0x02
#define	AMDSB8_PM_WDT_CTRL		0x4c
#define		AMDSB8_WDT_32KHZ	0x00
#define		AMDSB8_WDT_1HZ		0x03
#define		AMDSB8_WDT_RES_MASK	0x03
#define	AMDSB8_PM_RESET_STATUS0		0xC0
#define	AMDSB8_PM_RESET_STATUS1		0xC1
#define		AMDSB8_WD_RST_STS	0x20
/* SB7xx RRG 2.3.4, WDRT. */
#define	AMDSB_WD_CTRL			0x00
#define		AMDSB_WD_RUN		0x01
#define		AMDSB_WD_FIRED		0x02
#define		AMDSB_WD_SHUTDOWN	0x04
#define		AMDSB_WD_DISABLE	0x08
#define		AMDSB_WD_RESERVED	0x70
#define		AMDSB_WD_RELOAD		0x80
#define	AMDSB_WD_COUNT			0x04
#define		AMDSB_WD_COUNT_MASK	0xffff
#define	AMDSB_WDIO_REG_WIDTH		4
/* WDRT */
#define	MAXCOUNT_MIN_VALUE		511
/* SB7xx RRG 2.3.1.1, SB600 RRG 2.3.1.1, SB8xx RRG 2.3.1.  */
#define	AMDSB_SMBUS_DEVID		0x43851002
#define	AMDSB8_SMBUS_REVID		0x40

#define	amdsbwd_verbose_printf(dev, ...)	\
	do {						\
		if (bootverbose)			\
			device_printf(dev, __VA_ARGS__);\
	} while (0)

struct amdsbwd_softc {
	device_t		dev;
	eventhandler_tag	ev_tag;
	struct resource		*res_ctrl;
	struct resource		*res_count;
	int			rid_ctrl;
	int			rid_count;
	int			ms_per_tick;
	int			max_ticks;
	int			active;
	unsigned int		timeout;
};

static void	amdsbwd_identify(driver_t *driver, device_t parent);
static int	amdsbwd_probe(device_t dev);
static int	amdsbwd_attach(device_t dev);
static int	amdsbwd_detach(device_t dev);

static device_method_t amdsbwd_methods[] = {
	DEVMETHOD(device_identify,	amdsbwd_identify),
	DEVMETHOD(device_probe,		amdsbwd_probe),
	DEVMETHOD(device_attach,	amdsbwd_attach),
	DEVMETHOD(device_detach,	amdsbwd_detach),
#if 0
	DEVMETHOD(device_shutdown,	amdsbwd_detach),
#endif
	DEVMETHOD_END
};

static devclass_t	amdsbwd_devclass;
static driver_t		amdsbwd_driver = {
	"amdsbwd",
	amdsbwd_methods,
	sizeof(struct amdsbwd_softc)
};

DRIVER_MODULE(amdsbwd, isa, amdsbwd_driver, amdsbwd_devclass, NULL, NULL);


static uint8_t
pmio_read(struct resource *res, uint8_t reg)
{
	bus_write_1(res, 0, reg);	/* Index */
	return (bus_read_1(res, 1));	/* Data */
}

static void
pmio_write(struct resource *res, uint8_t reg, uint8_t val)
{
	bus_write_1(res, 0, reg);	/* Index */
	bus_write_1(res, 1, val);	/* Data */
}

static uint32_t
wdctrl_read(struct amdsbwd_softc *sc)
{
	return (bus_read_4(sc->res_ctrl, 0));
}

static void
wdctrl_write(struct amdsbwd_softc *sc, uint32_t val)
{
	bus_write_4(sc->res_ctrl, 0, val);
}

static __unused uint32_t
wdcount_read(struct amdsbwd_softc *sc)
{
	return (bus_read_4(sc->res_count, 0));
}

static void
wdcount_write(struct amdsbwd_softc *sc, uint32_t val)
{
	bus_write_4(sc->res_count, 0, val);
}

static void
amdsbwd_tmr_enable(struct amdsbwd_softc *sc)
{
	uint32_t val;

	val = wdctrl_read(sc);
	val |= AMDSB_WD_RUN;
	wdctrl_write(sc, val);
	sc->active = 1;
	amdsbwd_verbose_printf(sc->dev, "timer enabled\n");
}

static void
amdsbwd_tmr_disable(struct amdsbwd_softc *sc)
{
	uint32_t val;

	val = wdctrl_read(sc);
	val &= ~AMDSB_WD_RUN;
	wdctrl_write(sc, val);
	sc->active = 0;
	amdsbwd_verbose_printf(sc->dev, "timer disabled\n");
}

static void
amdsbwd_tmr_reload(struct amdsbwd_softc *sc)
{
	uint32_t val;

	val = wdctrl_read(sc);
	val |= AMDSB_WD_RELOAD;
	wdctrl_write(sc, val);
}

static void
amdsbwd_tmr_set(struct amdsbwd_softc *sc, uint16_t timeout)
{

	timeout &= AMDSB_WD_COUNT_MASK;
	wdcount_write(sc, timeout);
	sc->timeout = timeout;
	amdsbwd_verbose_printf(sc->dev, "timeout set to %u ticks\n", timeout);
}

static void
amdsbwd_event(void *arg, unsigned int cmd, int *error)
{
	struct amdsbwd_softc *sc = arg;
	unsigned int timeout;

	/* convert from power-of-two-ns to WDT ticks */
	cmd &= WD_INTERVAL;
	if (cmd < WD_TO_1SEC)
		cmd = 0;
	if (cmd) {
		timeout = ((uint64_t)1 << (cmd - WD_TO_1MS)) / sc->ms_per_tick;
		if (timeout > sc->max_ticks)
			timeout = sc->max_ticks;
		if (timeout != sc->timeout) {
			amdsbwd_tmr_set(sc, timeout);
			if (!sc->active)
				amdsbwd_tmr_enable(sc);
		}
		amdsbwd_tmr_reload(sc);
		*error = 0;
	} else {
		if (sc->active)
			amdsbwd_tmr_disable(sc);
	}
}

static void
amdsbwd_identify(driver_t *driver, device_t parent)
{
	device_t		child;
	device_t		smb_dev;

	if (resource_disabled("amdsbwd", 0))
		return;
	if (device_find_child(parent, "amdsbwd", -1) != NULL)
		return;

	/*
	 * Try to identify SB600/SB7xx by PCI Device ID of SMBus device
	 * that should be present at bus 0, device 20, function 0.
	 */
	smb_dev = pci_find_bsf(0, 20, 0);
	if (smb_dev == NULL)
		return;
	if (pci_get_devid(smb_dev) != AMDSB_SMBUS_DEVID)
		return;

	child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "amdsbwd", -1);
	if (child == NULL)
		device_printf(parent, "add amdsbwd child failed\n");
}


static void
amdsbwd_probe_sb7xx(device_t dev, struct resource *pmres, uint32_t *addr)
{
	uint32_t	val;
	int		i;

	/* Report cause of previous reset for user's convenience. */
	val = pmio_read(pmres, AMDSB_PM_RESET_STATUS0);
	if (val != 0)
		amdsbwd_verbose_printf(dev, "ResetStatus0 = %#04x\n", val);
	val = pmio_read(pmres, AMDSB_PM_RESET_STATUS1);
	if (val != 0)
		amdsbwd_verbose_printf(dev, "ResetStatus1 = %#04x\n", val);
	if ((val & AMDSB_WD_RST_STS) != 0)
		device_printf(dev, "Previous Reset was caused by Watchdog\n");

	/* Find base address of memory mapped WDT registers. */
	for (*addr = 0, i = 0; i < 4; i++) {
		*addr <<= 8;
		*addr |= pmio_read(pmres, AMDSB_PM_WDT_BASE_MSB - i);
	}
	/* Set watchdog timer tick to 1s. */
	val = pmio_read(pmres, AMDSB_PM_WDT_CTRL);
	val &= ~AMDSB_WDT_RES_MASK;
	val |= AMDSB_WDT_RES_10MS;
	pmio_write(pmres, AMDSB_PM_WDT_CTRL, val);

	/* Enable watchdog device (in stopped state). */
	val = pmio_read(pmres, AMDSB_PM_WDT_CTRL);
	val &= ~AMDSB_WDT_DISABLE;
	pmio_write(pmres, AMDSB_PM_WDT_CTRL, val);

	/*
	 * XXX TODO: Ensure that watchdog decode is enabled
	 * (register 0x41, bit 3).
	 */
	device_set_desc(dev, "AMD SB600/SB7xx Watchdog Timer");
}

static void
amdsbwd_probe_sb8xx(device_t dev, struct resource *pmres, uint32_t *addr)
{
	uint32_t	val;
	int		i;

	/* Report cause of previous reset for user's convenience. */
	val = pmio_read(pmres, AMDSB8_PM_RESET_STATUS0);
	if (val != 0)
		amdsbwd_verbose_printf(dev, "ResetStatus0 = %#04x\n", val);
	val = pmio_read(pmres, AMDSB8_PM_RESET_STATUS1);
	if (val != 0)
		amdsbwd_verbose_printf(dev, "ResetStatus1 = %#04x\n", val);
	if ((val & AMDSB8_WD_RST_STS) != 0)
		device_printf(dev, "Previous Reset was caused by Watchdog\n");

	/* Find base address of memory mapped WDT registers. */
	for (*addr = 0, i = 0; i < 4; i++) {
		*addr <<= 8;
		*addr |= pmio_read(pmres, AMDSB8_PM_WDT_EN + 3 - i);
	}
	*addr &= ~0x07u;

	/* Set watchdog timer tick to 1s. */
	val = pmio_read(pmres, AMDSB8_PM_WDT_CTRL);
	val &= ~AMDSB8_WDT_RES_MASK;
	val |= AMDSB8_WDT_1HZ;
	pmio_write(pmres, AMDSB8_PM_WDT_CTRL, val);
#ifdef AMDSBWD_DEBUG
	val = pmio_read(pmres, AMDSB8_PM_WDT_CTRL);
	amdsbwd_verbose_printf(dev, "AMDSB8_PM_WDT_CTRL value = %#02x\n", val);
#endif

	/*
	 * Enable watchdog device (in stopped state)
	 * and decoding of its address.
	 */
	val = pmio_read(pmres, AMDSB8_PM_WDT_EN);
	val &= ~AMDSB8_WDT_DISABLE;
	val |= AMDSB8_WDT_DEC_EN;
	pmio_write(pmres, AMDSB8_PM_WDT_EN, val);
#ifdef AMDSBWD_DEBUG
	val = pmio_read(pmres, AMDSB8_PM_WDT_EN);
	device_printf(dev, "AMDSB8_PM_WDT_EN value = %#02x\n", val);
#endif
	device_set_desc(dev, "AMD SB8xx Watchdog Timer");
}

static int
amdsbwd_probe(device_t dev)
{
	struct resource		*res;
	device_t		smb_dev;
	uint32_t		addr;
	int			rid;
	int			rc;

	/* Do not claim some ISA PnP device by accident. */
	if (isa_get_logicalid(dev) != 0)
		return (ENXIO);

	rc = bus_set_resource(dev, SYS_RES_IOPORT, 0, AMDSB_PMIO_INDEX,
	    AMDSB_PMIO_WIDTH);
	if (rc != 0) {
		device_printf(dev, "bus_set_resource for IO failed\n");
		return (ENXIO);
	}
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0ul, ~0ul,
	    AMDSB_PMIO_WIDTH, RF_ACTIVE | RF_SHAREABLE);
	if (res == NULL) {
		device_printf(dev, "bus_alloc_resource for IO failed\n");
		return (ENXIO);
	}

	smb_dev = pci_find_bsf(0, 20, 0);
	KASSERT(smb_dev != NULL, ("can't find SMBus PCI device\n"));
	if (pci_get_revid(smb_dev) < AMDSB8_SMBUS_REVID)
		amdsbwd_probe_sb7xx(dev, res, &addr);
	else
		amdsbwd_probe_sb8xx(dev, res, &addr);

	bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
	bus_delete_resource(dev, SYS_RES_IOPORT, rid);

	amdsbwd_verbose_printf(dev, "memory base address = %#010x\n", addr);
	rc = bus_set_resource(dev, SYS_RES_MEMORY, 0, addr + AMDSB_WD_CTRL,
	    AMDSB_WDIO_REG_WIDTH);
	if (rc != 0) {
		device_printf(dev, "bus_set_resource for control failed\n");
		return (ENXIO);
	}
	rc = bus_set_resource(dev, SYS_RES_MEMORY, 1, addr + AMDSB_WD_COUNT,
	    AMDSB_WDIO_REG_WIDTH);
	if (rc != 0) {
		device_printf(dev, "bus_set_resource for count failed\n");
		return (ENXIO);
	}

	return (0);
}

static int
amdsbwd_attach_sb(device_t dev, struct amdsbwd_softc *sc)
{
	device_t	smb_dev;

	sc->max_ticks = UINT16_MAX;
	sc->rid_ctrl = 0;
	sc->rid_count = 1;

	smb_dev = pci_find_bsf(0, 20, 0);
	KASSERT(smb_dev != NULL, ("can't find SMBus PCI device\n"));
	if (pci_get_revid(smb_dev) < AMDSB8_SMBUS_REVID)
		sc->ms_per_tick = 10;
	else
		sc->ms_per_tick = 1000;

	sc->res_ctrl = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->rid_ctrl, RF_ACTIVE);
	if (sc->res_ctrl == NULL) {
		device_printf(dev, "bus_alloc_resource for ctrl failed\n");
		return (ENXIO);
	}
	sc->res_count = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->rid_count, RF_ACTIVE);
	if (sc->res_count == NULL) {
		device_printf(dev, "bus_alloc_resource for count failed\n");
		return (ENXIO);
	}
	return (0);
}

static int
amdsbwd_attach(device_t dev)
{
	struct amdsbwd_softc	*sc;
	int			rc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rc = amdsbwd_attach_sb(dev, sc);
	if (rc != 0)
		goto fail;

#ifdef AMDSBWD_DEBUG
	device_printf(dev, "wd ctrl = %#04x\n", wdctrl_read(sc));
	device_printf(dev, "wd count = %#04x\n", wdcount_read(sc));
#endif

	/* Setup initial state of Watchdog Control. */
	wdctrl_write(sc, AMDSB_WD_FIRED);

	if (wdctrl_read(sc) & AMDSB_WD_DISABLE) {
		device_printf(dev, "watchdog hardware is disabled\n");
		goto fail;
	}

	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, amdsbwd_event, sc,
	    EVENTHANDLER_PRI_ANY);

	return (0);

fail:
	amdsbwd_detach(dev);
	return (ENXIO);
}

static int
amdsbwd_detach(device_t dev)
{
	struct amdsbwd_softc *sc;

	sc = device_get_softc(dev);
	if (sc->ev_tag != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);

	if (sc->active)
		amdsbwd_tmr_disable(sc);

	if (sc->res_ctrl != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid_ctrl,
		    sc->res_ctrl);

	if (sc->res_count != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid_count,
		    sc->res_count);

	return (0);
}

