/*-
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Power management - simple driver to handle reset and give access to
 * memory space region for other drivers through prcm driver.
 * Documentation/devicetree/binding/arm/omap/prm-inst.txt
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_prm.h>

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

/* relative to prcm address range */
#define TI_PRM_PER_RSTCTRL	0xC00

struct ti_prm_softc {
	device_t		dev;
	uint8_t			type;
	bool			has_reset;
};

/* Device */
#define TI_OMAP_PRM_INST	10

#define TI_AM3_PRM_INST		5
#define TI_AM4_PRM_INST		4
#define TI_OMAP4_PRM_INST	3
#define TI_OMAP5_PRM_INST	2
#define TI_DRA7_PRM_INST	1
#define TI_END			0

static struct ofw_compat_data compat_data[] = {
	{ "ti,am3-prm-inst",	TI_AM3_PRM_INST },
	{ "ti,am4-prm-inst",	TI_AM4_PRM_INST },
	{ "ti,omap4-prm-inst",	TI_OMAP4_PRM_INST },
	{ "ti,omap5-prm-inst",	TI_OMAP5_PRM_INST },
	{ "ti,dra7-prm-inst",	TI_DRA7_PRM_INST },
	{ NULL,		TI_END }
};

static struct ofw_compat_data required_data[] = {
	{ "ti,omap-prm-inst", TI_OMAP_PRM_INST },
	{ NULL, TI_END }
};

/* device interface */
static int
ti_prm_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, required_data)->ocd_data == 0)
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI OMAP Power Management");
	return(BUS_PROBE_DEFAULT);
}

static int
ti_prm_attach(device_t dev)
{
	struct ti_prm_softc *sc;
	phandle_t node;

 	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	node = ofw_bus_get_node(sc->dev);

	if (OF_hasprop(node, "#reset-cells")) {
		sc->has_reset = true;
	} else
		sc->has_reset = false;

	/* Make device visible for other drivers */
	OF_device_register_xref(OF_xref_from_node(node), sc->dev);

	return (0);
}

static int
ti_prm_detach(device_t dev) {
	return (EBUSY);
}

int
ti_prm_reset(device_t dev)
{
	struct ti_prm_softc *sc;
	int err;

	sc = device_get_softc(dev);
	if (sc->has_reset == false)
		return 1;

	err = ti_prm_modify_4(dev, TI_PRM_PER_RSTCTRL, 0x2, 0x00);
	return (err);
}

int
ti_prm_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	device_t parent;

	parent = device_get_parent(dev);
	DPRINTF(dev, "offset=%lx write %x\n", addr, val);
	ti_prcm_device_lock(parent);
	ti_prcm_write_4(parent, addr, val);
	ti_prcm_device_unlock(parent);
	return (0);
}

int
ti_prm_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	device_t parent;

	parent = device_get_parent(dev);

	ti_prcm_device_lock(parent);
	ti_prcm_read_4(parent, addr, val);
	ti_prcm_device_unlock(parent);
	DPRINTF(dev, "offset=%lx Read %x\n", addr, *val);
	return (0);
}

int
ti_prm_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	device_t parent;

	parent = device_get_parent(dev);

	ti_prcm_device_lock(parent);
	ti_prcm_modify_4(parent, addr, clr, set);
	ti_prcm_device_unlock(parent);
	DPRINTF(dev, "offset=%lx (clr %x set %x)\n", addr, clr, set);

	return (0);
}

static device_method_t ti_prm_methods[] = {
	DEVMETHOD(device_probe,		ti_prm_probe),
	DEVMETHOD(device_attach,	ti_prm_attach),
	DEVMETHOD(device_detach,	ti_prm_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ti_prm, ti_prm_driver, ti_prm_methods,
    sizeof(struct ti_prm_softc), simplebus_driver);

EARLY_DRIVER_MODULE(ti_prm, simplebus, ti_prm_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_prm, 1);
MODULE_DEPEND(ti_prm, ti_sysc, 1, 1, 1);
