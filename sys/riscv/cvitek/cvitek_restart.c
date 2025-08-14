/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Bojan Novković <bnovkov@FreeBSD.org>
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
 * Driver for cvitek's poweroff/restart controller.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/reboot.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#define RTC_CTRL0_UNLOCK	0x4
#define  RTC_CTRL0_UNLOCK_KEY	0xAB18

#define RTC_CTRL0			0x8
#define  RTC_CTRL0_RESERVED_MASK	0xFFFF0800
#define  RTC_CTRL0_REQ_SHUTDOWN		0x01
#define  RTC_CTRL0_REQ_POWERCYCLE	0x08
#define  RTC_CTRL0_REQ_WARM_RESET	0x10

#define RTC_BASE_OFFSET		0x1000
#define RTC_EN_SHDN_REQ		(RTC_BASE_OFFSET + 0xC0)
#define RTC_EN_PWR_CYC_REQ	(RTC_BASE_OFFSET + 0xC8)
#define RTC_EN_WARM_RST_REQ	(RTC_BASE_OFFSET + 0xCC)

struct cvitek_restart_softc {
	int reg_rid;
	struct resource *reg;
	eventhandler_tag tag;
};

static void
cvitek_restart_shutdown_final(device_t dev, int howto)
{
	struct cvitek_restart_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);
	val = RTC_CTRL0_RESERVED_MASK;
	if ((howto & RB_POWEROFF) != 0)
		val |= RTC_CTRL0_REQ_SHUTDOWN;
	else if ((howto & RB_POWERCYCLE) != 0)
		val |= RTC_CTRL0_REQ_POWERCYCLE;
	else
		val |= RTC_CTRL0_REQ_WARM_RESET;

	/* Unlock writes to 'rtc_ctrl0'. */
	bus_write_4(sc->reg, RTC_CTRL0_UNLOCK, RTC_CTRL0_UNLOCK_KEY);
	bus_write_4(sc->reg, RTC_CTRL0, val);
	DELAY(1000);

	device_printf(dev, "Poweroff request failed\n");
}

static int
cvitek_restart_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "cvitek,restart")) {
		device_set_desc(dev, "Cvitek restart controller");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
cvitek_restart_attach(device_t dev)
{
	struct cvitek_restart_softc *sc;

	sc = device_get_softc(dev);
	sc->reg = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->reg_rid,
	    RF_ACTIVE);
	if (sc->reg == NULL) {
		device_printf(dev, "can't map RTC regs\n");
		return (ENXIO);
	}

	/* Enable requests for various poweroff methods. */
	bus_write_4(sc->reg, RTC_EN_SHDN_REQ, 0x1);
	bus_write_4(sc->reg, RTC_EN_PWR_CYC_REQ, 0x1);
	bus_write_4(sc->reg, RTC_EN_WARM_RST_REQ, 0x1);

	sc->tag = EVENTHANDLER_REGISTER(shutdown_final,
	    cvitek_restart_shutdown_final, dev, SHUTDOWN_PRI_LAST);

	return (0);
}

static int
cvitek_restart_detach(device_t dev)
{
	struct cvitek_restart_softc *sc;

	sc = device_get_softc(dev);
	if (sc->reg == NULL)
		return (0);

	bus_write_4(sc->reg, RTC_EN_SHDN_REQ, 0x0);
	bus_write_4(sc->reg, RTC_EN_PWR_CYC_REQ, 0x0);
	bus_write_4(sc->reg, RTC_EN_WARM_RST_REQ, 0x0);

	bus_release_resource(dev, SYS_RES_MEMORY, sc->reg_rid, sc->reg);
	EVENTHANDLER_DEREGISTER(shutdown_final, sc->tag);

	return (0);
}

static device_method_t cvitek_restart_methods[] = {
	DEVMETHOD(device_probe,		cvitek_restart_probe),
	DEVMETHOD(device_attach,	cvitek_restart_attach),
	DEVMETHOD(device_detach,	cvitek_restart_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(cvitek_restart, cvitek_restart_driver, cvitek_restart_methods,
    sizeof(struct cvitek_restart_softc));
DRIVER_MODULE(cvitek_restart, simplebus, cvitek_restart_driver, NULL, NULL);
