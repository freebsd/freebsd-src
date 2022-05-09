/*-
 * Copyright (c) 2013 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_sysc.h>

#include <dev/extres/syscon/syscon.h>
#include "syscon_if.h"

#include "am335x_pwm.h"
#include "am335x_scm.h"

#define	PWMSS_IDVER		0x00
#define	PWMSS_SYSCONFIG		0x04
#define	PWMSS_CLKCONFIG		0x08
#define		CLKCONFIG_EPWMCLK_EN	(1 << 8)
#define	PWMSS_CLKSTATUS		0x0C

/* TRM chapter 2 memory map table 2-3 + VER register location */
#define PWMSS_REV_0		0x0000
#define PWMSS_REV_1		0x2000
#define PWMSS_REV_2		0x4000

static device_probe_t am335x_pwmss_probe;
static device_attach_t am335x_pwmss_attach;
static device_detach_t am335x_pwmss_detach;

struct am335x_pwmss_softc {
	struct simplebus_softc	sc_simplebus;
	device_t		sc_dev;
	struct syscon           *syscon;
};

static device_method_t am335x_pwmss_methods[] = {
	DEVMETHOD(device_probe,		am335x_pwmss_probe),
	DEVMETHOD(device_attach,	am335x_pwmss_attach),
	DEVMETHOD(device_detach,	am335x_pwmss_detach),

	DEVMETHOD_END
};

static int
am335x_pwmss_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "ti,am33xx-pwmss"))
		return (ENXIO);

	device_set_desc(dev, "AM335x PWM");

	return (BUS_PROBE_DEFAULT);
}

static int
am335x_pwmss_attach(device_t dev)
{
	struct am335x_pwmss_softc *sc;
	uint32_t reg, id;
	uint64_t rev_address;
	phandle_t node, opp_table;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	/* FIXME: For now; Go and kidnap syscon from opp-table */
	opp_table = OF_finddevice("/opp-table");
	if (opp_table == -1) {
		device_printf(dev, "Cant find /opp-table\n");
		return (ENXIO);
	}
	if (!OF_hasprop(opp_table, "syscon")) {
		device_printf(dev, "/opp-table doesnt have required syscon property\n");
		return (ENXIO);
	}
	if (syscon_get_by_ofw_property(dev, opp_table, "syscon", &sc->syscon) != 0) {
		device_printf(dev, "Failed to get syscon\n");
		return (ENXIO);
	}

	ti_sysc_clock_enable(device_get_parent(dev));

	rev_address = ti_sysc_get_rev_address(device_get_parent(dev));
	switch (rev_address) {
	case PWMSS_REV_0:
		id = 0;
		break;
	case PWMSS_REV_1:
		id = 1;
		break;
	case PWMSS_REV_2:
		id = 2;
		break;
	}

	reg = SYSCON_READ_4(sc->syscon, SCM_PWMSS_CTRL);
	reg |= (1 << id);
	SYSCON_WRITE_4(sc->syscon, SCM_PWMSS_CTRL, reg);

	node = ofw_bus_get_node(dev);

	if (node == -1)
		return (ENXIO);

	simplebus_init(dev, node);

	/*
	 * Allow devices to identify.
	 */
	bus_generic_probe(dev);

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node))
		simplebus_add_device(dev, node, 0, NULL, -1, NULL);

	return (bus_generic_attach(dev));
}

static int
am335x_pwmss_detach(device_t dev)
{

	return (0);
}

DEFINE_CLASS_1(am335x_pwmss, am335x_pwmss_driver, am335x_pwmss_methods,
    sizeof(struct am335x_pwmss_softc), simplebus_driver);
DRIVER_MODULE(am335x_pwmss, simplebus, am335x_pwmss_driver, 0, 0);
MODULE_VERSION(am335x_pwmss, 1);
MODULE_DEPEND(am335x_pwmss, ti_sysc, 1, 1, 1);
