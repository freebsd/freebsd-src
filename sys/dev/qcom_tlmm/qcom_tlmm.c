/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>
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
 * This is a pinmux/gpio controller for the Qualcomm IPQ/MSM/Snapdragon SoCs.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/gpio/gpiobusvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>

#include "qcom_tlmm_var.h"
#include "qcom_tlmm_pin.h"
#include "qcom_tlmm_debug.h"

#include "gpio_if.h"

#define	DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
	    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN)

/* TODO: put in a header file */
extern	void qcom_tlmm_ipq4018_attach(struct qcom_tlmm_softc *sc);

struct qcom_tlmm_chipset_list {
	qcom_tlmm_chipset_t id;
	const char *ofw_str;
	const char *desc_str;
	void (*attach_func)(struct qcom_tlmm_softc *);
};

static struct qcom_tlmm_chipset_list qcom_tlmm_chipsets[] = {
	{ QCOM_TLMM_CHIPSET_IPQ4018, "qcom,ipq4019-pinctrl",
	    "Qualcomm Atheros TLMM IPQ4018/IPQ4019 GPIO/Pinmux driver",
	    qcom_tlmm_ipq4018_attach },
	{ 0, NULL, NULL, NULL },
};

static int
qcom_tlmm_probe(device_t dev)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	struct qcom_tlmm_chipset_list *ql;
	int i;

	if (! ofw_bus_status_okay(dev))
		return (ENXIO);

	for (i = 0; qcom_tlmm_chipsets[i].id != 0; i++) {
		ql = &qcom_tlmm_chipsets[i];
		device_printf(dev, "%s: checking %s\n", __func__, ql->ofw_str);
		if (ofw_bus_is_compatible(dev, ql->ofw_str) == 1) {
			sc->sc_chipset = ql->id;
			sc->sc_attach_func = ql->attach_func;
			device_set_desc(dev, ql->desc_str);
			return (0);
		}
	}

	return (ENXIO);
}

static int
qcom_tlmm_detach(device_t dev)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->gpio_mtx), ("gpio mutex not initialized"));

	gpiobus_detach_bus(dev);
	if (sc->gpio_ih)
		bus_teardown_intr(dev, sc->gpio_irq_res, sc->gpio_ih);
	if (sc->gpio_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->gpio_irq_rid,
		    sc->gpio_irq_res);
	if (sc->gpio_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->gpio_mem_rid,
		    sc->gpio_mem_res);
	if (sc->gpio_pins)
		free(sc->gpio_pins, M_DEVBUF);
	mtx_destroy(&sc->gpio_mtx);

	return(0);
}

static int
qcom_tlmm_attach(device_t dev)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	int i;

	KASSERT((device_get_unit(dev) == 0),
	    ("qcom_tlmm: Only one gpio module supported"));

	mtx_init(&sc->gpio_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Map control/status registers. */
	sc->gpio_mem_rid = 0;
	sc->gpio_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->gpio_mem_rid, RF_ACTIVE);

	if (sc->gpio_mem_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		qcom_tlmm_detach(dev);
		return (ENXIO);
	}

	if ((sc->gpio_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->gpio_irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		qcom_tlmm_detach(dev);
		return (ENXIO);
	}

	if ((bus_setup_intr(dev, sc->gpio_irq_res, INTR_TYPE_MISC,
	    qcom_tlmm_filter, qcom_tlmm_intr, sc, &sc->gpio_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		qcom_tlmm_detach(dev);
		return (ENXIO);
	}

	sc->dev = dev;
	sc->sc_debug = 0;

	/* Call platform specific attach function */
	sc->sc_attach_func(sc);

	qcom_tlmm_debug_sysctl_attach(sc);

	/* Allocate local pin state for all of our pins */
	sc->gpio_pins = malloc(sizeof(*sc->gpio_pins) * sc->gpio_npins,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* Note: direct map between gpio pin and gpio_pin[] entry */
	for (i = 0; i < sc->gpio_npins; i++) {
		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME,
		    "gpio%d", i);
		sc->gpio_pins[i].gp_pin = i;
		sc->gpio_pins[i].gp_caps = DEFAULT_CAPS;
		(void) qcom_tlmm_pin_getflags(dev, i,
		    &sc->gpio_pins[i].gp_flags);
	}

	fdt_pinctrl_register(dev, NULL);
	fdt_pinctrl_configure_by_name(dev, "default");

	sc->busdev = gpiobus_add_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "%s: failed to attach bus\n", __func__);
		qcom_tlmm_detach(dev);
		return (ENXIO);
	}
	bus_attach_children(dev);

	return (0);
}

static device_method_t qcom_tlmm_methods[] = {
	/* Driver */
	DEVMETHOD(device_probe, qcom_tlmm_probe),
	DEVMETHOD(device_attach, qcom_tlmm_attach),
	DEVMETHOD(device_detach, qcom_tlmm_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, qcom_tlmm_get_bus),
	DEVMETHOD(gpio_pin_max, qcom_tlmm_pin_max),
	DEVMETHOD(gpio_pin_getname, qcom_tlmm_pin_getname),
	DEVMETHOD(gpio_pin_getflags, qcom_tlmm_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, qcom_tlmm_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, qcom_tlmm_pin_setflags),
	DEVMETHOD(gpio_pin_get, qcom_tlmm_pin_get),
	DEVMETHOD(gpio_pin_set, qcom_tlmm_pin_set),
	DEVMETHOD(gpio_pin_toggle, qcom_tlmm_pin_toggle),

	/* OFW */
	DEVMETHOD(ofw_bus_get_node, qcom_tlmm_pin_get_node),

	/* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure, qcom_tlmm_pinctrl_configure),

	{0, 0},
};

static driver_t qcom_tlmm_driver = {
	"gpio",
	qcom_tlmm_methods,
	sizeof(struct qcom_tlmm_softc),
};

EARLY_DRIVER_MODULE(qcom_tlmm, simplebus, qcom_tlmm_driver,
    NULL, NULL, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
EARLY_DRIVER_MODULE(qcom_tlmm, ofwbus, qcom_tlmm_driver,
    NULL, NULL, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_VERSION(qcom_tlmm, 1);
