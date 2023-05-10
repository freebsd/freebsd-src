/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for Qualcomm IPQ4018 clock and reset device */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/random.h>
#include <sys/stdatomic.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/hwreset/hwreset.h>

#include "clkdev_if.h"
#include "hwreset_if.h"

#include <dt-bindings/clock/qcom,gcc-ipq4019.h>

#include "qcom_gcc_ipq4018_var.h"


static int	qcom_gcc_ipq4018_modevent(module_t, int, void *);

static int	qcom_gcc_ipq4018_probe(device_t);
static int	qcom_gcc_ipq4018_attach(device_t);
static int	qcom_gcc_ipq4018_detach(device_t);

static int
qcom_gcc_ipq4018_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	case MOD_QUIESCE:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
qcom_gcc_ipq4018_probe(device_t dev)
{
	if (! ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "qcom,gcc-ipq4019") == 0)
		return (ENXIO);

	return (0);
}

static int
qcom_gcc_ipq4018_attach(device_t dev)
{
	struct qcom_gcc_ipq4018_softc *sc;

	sc = device_get_softc(dev);

	/* Found a compatible device! */
	sc->dev = dev;

	sc->reg_rid = 0;
	sc->reg = bus_alloc_resource_anywhere(dev, SYS_RES_MEMORY,
	    &sc->reg_rid, 0x60000, RF_ACTIVE);
	if (sc->reg == NULL) {
		device_printf(dev, "Couldn't allocate memory resource!\n");
		return (ENXIO);
	}

	device_set_desc(dev, "Qualcomm IPQ4018 Clock/Reset Controller");

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/*
	 * Register as a reset provider.
	 */
	hwreset_register_ofw_provider(dev);

	/*
	 * Setup and register as a clock provider.
	 */
	qcom_gcc_ipq4018_clock_setup(sc);

	return (0);
}

static int
qcom_gcc_ipq4018_detach(device_t dev)
{
	struct qcom_gcc_ipq4018_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * TBD - deregistering reset/clock resources.
	 */

	if (sc->reg != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    sc->reg_rid, sc->reg);
	}
	return (0);
}

static device_method_t qcom_gcc_ipq4018_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		qcom_gcc_ipq4018_probe),
	DEVMETHOD(device_attach,	qcom_gcc_ipq4018_attach),
	DEVMETHOD(device_detach,	qcom_gcc_ipq4018_detach),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	qcom_gcc_ipq4018_hwreset_assert),
	DEVMETHOD(hwreset_is_asserted,	qcom_gcc_ipq4018_hwreset_is_asserted),

	/* Clock interface */
	DEVMETHOD(clkdev_read_4,	qcom_gcc_ipq4018_clock_read),
	DEVMETHOD(clkdev_write_4,	qcom_gcc_ipq4018_clock_write),
	DEVMETHOD(clkdev_modify_4,	qcom_gcc_ipq4018_clock_modify),
	DEVMETHOD(clkdev_device_lock,	qcom_gcc_ipq4018_clock_lock),
	DEVMETHOD(clkdev_device_unlock,	qcom_gcc_ipq4018_clock_unlock),

	DEVMETHOD_END
};

static driver_t qcom_gcc_ipq4018_driver = {
	"qcom_gcc",
	qcom_gcc_ipq4018_methods,
	sizeof(struct qcom_gcc_ipq4018_softc)
};

EARLY_DRIVER_MODULE(qcom_gcc_ipq4018, simplebus, qcom_gcc_ipq4018_driver,
    qcom_gcc_ipq4018_modevent, NULL, BUS_PASS_CPU + BUS_PASS_ORDER_EARLY);
EARLY_DRIVER_MODULE(qcom_gcc_ipq4018, ofwbus, qcom_gcc_ipq4018_driver,
    qcom_gcc_ipq4018_modevent, NULL, BUS_PASS_CPU + BUS_PASS_ORDER_EARLY);
MODULE_VERSION(qcom_gcc_ipq4018, 1);
