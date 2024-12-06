/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 BusyTech
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

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/* Registers */
#define DWWDT_CR		0x00
#define DWWDT_CR_WDT_EN		(1 << 0)
#define DWWDT_CR_RESP_MODE	(1 << 1)
#define DWWDT_TORR		0x04
#define DWWDT_CCVR		0x08
#define DWWDT_CRR		0x0C
#define DWWDT_CRR_KICK		0x76
#define DWWDT_STAT		0x10
#define DWWDT_STAT_STATUS	0x01
#define DWWDT_EOI		0x14

#define DWWDT_READ4(sc, reg) 		bus_read_4((sc)->sc_mem_res, (reg))
#define DWWDT_WRITE4(sc, reg, val)	\
	bus_write_4((sc)->sc_mem_res, (reg), (val))

/*
 * 47 = 16 (timeout shift of dwwdt) + 30 (1s ~= 2 ** 30ns) + 1
 * (pre-restart delay)
 */
#define DWWDT_EXP_OFFSET	47

struct dwwdt_softc {
	device_t		 sc_dev;
	struct resource		*sc_mem_res;
	struct resource		*sc_irq_res;
	void			*sc_intr_cookie;
	clk_t			 sc_clk;
	uint64_t		 sc_clk_freq;
	eventhandler_tag	 sc_evtag;
	int 			 sc_mem_rid;
	int			 sc_irq_rid;
	enum {
		DWWDT_STOPPED,
		DWWDT_RUNNING,
	}			 sc_status;
};

static struct ofw_compat_data compat_data[] = {
	{ "snps,dw-wdt",		1 },
	{ NULL,				0 }
};

SYSCTL_NODE(_dev, OID_AUTO, dwwdt, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Synopsys Designware watchdog timer");

/* Setting this to true disables full restart mode. */
static bool dwwdt_prevent_restart = false;
SYSCTL_BOOL(_dev_dwwdt, OID_AUTO, prevent_restart, CTLFLAG_RW | CTLFLAG_MPSAFE,
    &dwwdt_prevent_restart, 0, "Disable system reset on timeout");

static bool dwwdt_debug_enabled = false;
SYSCTL_BOOL(_dev_dwwdt, OID_AUTO, debug, CTLFLAG_RW | CTLFLAG_MPSAFE,
    &dwwdt_debug_enabled, 0, "Enable debug mode");

static bool dwwdt_panic_first = true;
SYSCTL_BOOL(_dev_dwwdt, OID_AUTO, panic_first, CTLFLAG_RW | CTLFLAG_MPSAFE,
    &dwwdt_panic_first, 0,
    "Try to panic on timeout, reset on another timeout");

static int dwwdt_probe(device_t);
static int dwwdt_attach(device_t);
static int dwwdt_detach(device_t);
static int dwwdt_shutdown(device_t);

static void dwwdt_intr(void *);
static void dwwdt_event(void *, unsigned int, int *);

/* Helpers */
static inline void dwwdt_start(struct dwwdt_softc *sc);
static inline bool dwwdt_started(const struct dwwdt_softc *sc);
static inline void dwwdt_stop(struct dwwdt_softc *sc);
static inline void dwwdt_set_timeout(const struct dwwdt_softc *sc, int val);

static void dwwdt_debug(device_t);

static void
dwwdt_debug(device_t dev)
{
	/*
	 * Reading from EOI may clear interrupt flag.
	 */
	const struct dwwdt_softc *sc = device_get_softc(dev);

	device_printf(dev, "Registers dump: \n");
	device_printf(dev, "  CR:   %08x\n", DWWDT_READ4(sc, DWWDT_CR));
	device_printf(dev, "  CCVR: %08x\n", DWWDT_READ4(sc, DWWDT_CCVR));
	device_printf(dev, "  CRR:  %08x\n", DWWDT_READ4(sc, DWWDT_CRR));
	device_printf(dev, "  STAT: %08x\n", DWWDT_READ4(sc, DWWDT_STAT));

	device_printf(dev, "Clock: %s\n", clk_get_name(sc->sc_clk));
	device_printf(dev, "  FREQ: %lu\n", sc->sc_clk_freq);
}

static inline bool
dwwdt_started(const struct dwwdt_softc *sc)
{

	/* CR_WDT_E bit can be clear only by full CPU reset. */
	return ((DWWDT_READ4(sc, DWWDT_CR) & DWWDT_CR_WDT_EN) != 0);
}

static void inline
dwwdt_start(struct dwwdt_softc *sc)
{
	uint32_t val;

	/* Enable watchdog */
	val = DWWDT_READ4(sc, DWWDT_CR);
	val |= DWWDT_CR_WDT_EN | DWWDT_CR_RESP_MODE;
	DWWDT_WRITE4(sc, DWWDT_CR, val);
	sc->sc_status = DWWDT_RUNNING;
}

static void inline
dwwdt_stop(struct dwwdt_softc *sc)
{

	sc->sc_status = DWWDT_STOPPED;
	dwwdt_set_timeout(sc, 0x0f);
}

static void inline
dwwdt_set_timeout(const struct dwwdt_softc *sc, int val)
{

	DWWDT_WRITE4(sc, DWWDT_TORR, val);
	DWWDT_WRITE4(sc, DWWDT_CRR, DWWDT_CRR_KICK);
}

static void
dwwdt_intr(void *arg)
{
	struct dwwdt_softc *sc = arg;

	KASSERT((DWWDT_READ4(sc, DWWDT_STAT) & DWWDT_STAT_STATUS) != 0,
	    ("Missing interrupt status bit?"));

	if (dwwdt_prevent_restart || sc->sc_status == DWWDT_STOPPED) {
		/*
		 * Confirm interrupt reception. Restart counter.
		 * This also emulates stopping watchdog.
		 */
		(void)DWWDT_READ4(sc, DWWDT_EOI);
		return;
	}

	if (dwwdt_panic_first)
		panic("dwwdt pre-timeout interrupt");
}

static void
dwwdt_event(void *arg, unsigned int cmd, int *error)
{
	struct dwwdt_softc *sc = arg;
	const int exponent = flsl(sc->sc_clk_freq);
	int timeout;
	int val;

	timeout = cmd & WD_INTERVAL;
	val = MAX(0, timeout + exponent - DWWDT_EXP_OFFSET + 1);

	dwwdt_stop(sc);
	if (cmd == 0 || val > 0x0f) {
		/*
		 * Set maximum time between interrupts and Leave watchdog
		 * disabled.
		 */
		return;
	}

	dwwdt_set_timeout(sc, val);
	dwwdt_start(sc);
	*error = 0;

	if (dwwdt_debug_enabled)
		dwwdt_debug(sc->sc_dev);
}

static int
dwwdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Synopsys Designware watchdog timer");
	return (BUS_PROBE_DEFAULT);
}

static int
dwwdt_attach(device_t dev)
{
	struct dwwdt_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "cannot allocate memory resource\n");
		goto err_no_mem;
	}

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->sc_irq_rid, RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "cannot allocate ireq resource\n");
		goto err_no_irq;
	}

	sc->sc_intr_cookie = NULL;
	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_MPSAFE | INTR_TYPE_MISC,
	    NULL, dwwdt_intr, sc, &sc->sc_intr_cookie) != 0) {
		device_printf(dev, "cannot setup interrupt routine\n");
		goto err_no_intr;
	}

	if (clk_get_by_ofw_index(dev, 0, 0, &sc->sc_clk) != 0) {
		device_printf(dev, "cannot find clock\n");
		goto err_no_clock;
	}

	if (clk_enable(sc->sc_clk) != 0) {
		device_printf(dev, "cannot enable clock\n");
		goto err_no_freq;
	}

	if (clk_get_freq(sc->sc_clk, &sc->sc_clk_freq) != 0) {
		device_printf(dev, "cannot get clock frequency\n");
		goto err_no_freq;
	}

	if (sc->sc_clk_freq == 0UL)
		goto err_no_freq;

	sc->sc_evtag = EVENTHANDLER_REGISTER(watchdog_list, dwwdt_event, sc, 0);
	sc->sc_status = DWWDT_STOPPED;

	bus_attach_children(dev);
	return (0);

err_no_freq:
	clk_release(sc->sc_clk);
err_no_clock:
	bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intr_cookie);
err_no_intr:
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid, sc->sc_irq_res);
err_no_irq:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
	    sc->sc_mem_res);
err_no_mem:
	return (ENXIO);
}

static int
dwwdt_detach(device_t dev)
{
	struct dwwdt_softc *sc = device_get_softc(dev);
	int error;

	if (dwwdt_started(sc)) {
		/*
		 * Once started it cannot be stopped. Prevent module unload
		 * instead.
		 */
		return (EBUSY);
	}

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);

	EVENTHANDLER_DEREGISTER(watchdog_list, sc->sc_evtag);
	sc->sc_evtag = NULL;

	if (sc->sc_clk != NULL)
		clk_release(sc->sc_clk);

	if (sc->sc_intr_cookie != NULL)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intr_cookie);

	if (sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid,
		    sc->sc_irq_res);
	}

	if (sc->sc_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem_res);
	}

	return (0);
}

static int
dwwdt_shutdown(device_t dev)
{
	struct dwwdt_softc *sc;

	sc = device_get_softc(dev);

	/* Prevent restarts during shutdown. */
	dwwdt_prevent_restart = true;
	dwwdt_stop(sc);
	return (bus_generic_shutdown(dev));
}

static device_method_t dwwdt_methods[] = {
	DEVMETHOD(device_probe, dwwdt_probe),
	DEVMETHOD(device_attach, dwwdt_attach),
	DEVMETHOD(device_detach, dwwdt_detach),
	DEVMETHOD(device_shutdown, dwwdt_shutdown),

	{0, 0}
};

static driver_t dwwdt_driver = {
	"dwwdt",
	dwwdt_methods,
	sizeof(struct dwwdt_softc),
};

DRIVER_MODULE(dwwdt, simplebus, dwwdt_driver, NULL, NULL);
MODULE_VERSION(dwwdt, 1);
OFWBUS_PNP_INFO(compat_data);
