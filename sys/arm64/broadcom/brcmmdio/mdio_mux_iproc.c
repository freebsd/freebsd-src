/*-
 * Copyright (c) 2019 Juniper Networks, Inc.
 * Copyright (c) 2019 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "mdio_if.h"

#define	REG_BASE_RID	0

#define	MDIO_RATE_ADJ_EXT_OFFSET	0x000
#define	MDIO_RATE_ADJ_INT_OFFSET	0x004
#define	MDIO_RATE_ADJ_DIVIDENT_SHIFT	16

#define	MDIO_SCAN_CTRL_OFFSET		0x008
#define	MDIO_SCAN_CTRL_OVRIDE_EXT_MSTR	28

#define	MDIO_PARAM_OFFSET		0x23c
#define	MDIO_PARAM_MIIM_CYCLE		29
#define	MDIO_PARAM_INTERNAL_SEL		25
#define	MDIO_PARAM_BUS_ID		22
#define	MDIO_PARAM_C45_SEL		21
#define	MDIO_PARAM_PHY_ID		16
#define	MDIO_PARAM_PHY_DATA		0

#define	MDIO_READ_OFFSET		0x240
#define	MDIO_READ_DATA_MASK		0xffff
#define	MDIO_ADDR_OFFSET		0x244

#define	MDIO_CTRL_OFFSET		0x248
#define	MDIO_CTRL_WRITE_OP		0x1
#define	MDIO_CTRL_READ_OP		0x2

#define	MDIO_STAT_OFFSET		0x24c
#define	MDIO_STAT_DONE			1

#define	BUS_MAX_ADDR			32
#define	EXT_BUS_START_ADDR		16

#define	MDIO_REG_ADDR_SPACE_SIZE	0x250

#define	MDIO_OPERATING_FREQUENCY	11000000
#define	MDIO_RATE_ADJ_DIVIDENT		1

#define	MII_ADDR_C45			(1<<30)

static int brcm_iproc_mdio_probe(device_t);
static int brcm_iproc_mdio_attach(device_t);
static int brcm_iproc_mdio_detach(device_t);

/* OFW bus interface */
struct brcm_mdio_ofw_devinfo {
	struct ofw_bus_devinfo	di_dinfo;
	struct resource_list	di_rl;
};

struct brcm_iproc_mdio_softc {
	struct simplebus_softc	sbus;
	device_t		dev;
	struct resource *	reg_base;
	uint32_t		clock_rate;
};

MALLOC_DEFINE(M_BRCM_IPROC_MDIO, "Broadcom IPROC MDIO",
    "Broadcom IPROC MDIO dynamic memory");

static int brcm_iproc_config(struct brcm_iproc_mdio_softc*);
static const struct ofw_bus_devinfo *
brcm_iproc_mdio_get_devinfo(device_t, device_t);
static int brcm_iproc_mdio_write_mux(device_t, int, int, int, int);
static int brcm_iproc_mdio_read_mux(device_t, int, int, int);

static device_method_t brcm_iproc_mdio_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		brcm_iproc_mdio_probe),
	DEVMETHOD(device_attach,	brcm_iproc_mdio_attach),
	DEVMETHOD(device_detach,	brcm_iproc_mdio_detach),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,		bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	brcm_iproc_mdio_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* MDIO interface */
	DEVMETHOD(mdio_writereg_mux,	brcm_iproc_mdio_write_mux),
	DEVMETHOD(mdio_readreg_mux,	brcm_iproc_mdio_read_mux),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(brcm_iproc_mdio, brcm_iproc_mdio_driver,
    brcm_iproc_mdio_fdt_methods, sizeof(struct brcm_iproc_mdio_softc));

EARLY_DRIVER_MODULE(brcm_iproc_mdio, ofwbus, brcm_iproc_mdio_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(brcm_iproc_mdio, simplebus, brcm_iproc_mdio_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

static struct ofw_compat_data mdio_compat_data[] = {
	{"brcm,mdio-mux-iproc",	true},
	{NULL,			false}
};

static int
brcm_iproc_switch(struct brcm_iproc_mdio_softc *sc, int child)
{
	uint32_t param, bus_id;
	uint32_t bus_dir;

	/* select bus and its properties */
	bus_dir = (child < EXT_BUS_START_ADDR);
	bus_id = bus_dir ? child : (child - EXT_BUS_START_ADDR);

	param = (bus_dir ? 1 : 0) << MDIO_PARAM_INTERNAL_SEL;
	param |= (bus_id << MDIO_PARAM_BUS_ID);

	bus_write_4(sc->reg_base, MDIO_PARAM_OFFSET, param);

	return (0);
}

static int
iproc_mdio_wait_for_idle(struct brcm_iproc_mdio_softc *sc, uint32_t result)
{
	unsigned int timeout = 1000; /* loop for 1s */
	uint32_t val;

	do {
		val = bus_read_4(sc->reg_base, MDIO_STAT_OFFSET);
		if ((val & MDIO_STAT_DONE) == result)
			return (0);

		pause("BRCM MDIO SLEEP", 1000 / hz);
	} while (timeout--);

	return (ETIMEDOUT);
}

/* start_miim_ops- Program and start MDIO transaction over mdio bus.
 * @base: Base address
 * @phyid: phyid of the selected bus.
 * @reg: register offset to be read/written.
 * @val :0 if read op else value to be written in @reg;
 * @op: Operation that need to be carried out.
 *      MDIO_CTRL_READ_OP: Read transaction.
 *      MDIO_CTRL_WRITE_OP: Write transaction.
 *
 * Return value: Successful Read operation returns read reg values and write
 *      operation returns 0. Failure operation returns negative error code.
 */
static int
brcm_iproc_mdio_op(struct brcm_iproc_mdio_softc *sc,
	uint16_t phyid, uint32_t reg, uint32_t val, uint32_t op)
{
	uint32_t param;
	int ret;

	bus_write_4(sc->reg_base, MDIO_CTRL_OFFSET, 0);
	bus_read_4(sc->reg_base, MDIO_STAT_OFFSET);
	ret = iproc_mdio_wait_for_idle(sc, 0);
	if (ret)
		goto err;

	param = bus_read_4(sc->reg_base, MDIO_PARAM_OFFSET);
	param |= phyid << MDIO_PARAM_PHY_ID;
	param |= val << MDIO_PARAM_PHY_DATA;
	if (reg & MII_ADDR_C45)
		param |= (1 << MDIO_PARAM_C45_SEL);

	bus_write_4(sc->reg_base, MDIO_PARAM_OFFSET, param);

	bus_write_4(sc->reg_base, MDIO_ADDR_OFFSET, reg);

	bus_write_4(sc->reg_base, MDIO_CTRL_OFFSET, op);

	ret = iproc_mdio_wait_for_idle(sc, 1);
	if (ret)
		goto err;

	if (op == MDIO_CTRL_READ_OP)
		ret = bus_read_4(sc->reg_base, MDIO_READ_OFFSET) & MDIO_READ_DATA_MASK;
err:
	return ret;
}

static int
brcm_iproc_config(struct brcm_iproc_mdio_softc *sc)
{
	uint32_t divisor;
	uint32_t val;

	/* Disable external mdio master access */
	val = bus_read_4(sc->reg_base, MDIO_SCAN_CTRL_OFFSET);
	val |= 1 << MDIO_SCAN_CTRL_OVRIDE_EXT_MSTR;
	bus_write_4(sc->reg_base, MDIO_SCAN_CTRL_OFFSET, val);

	if (sc->clock_rate) {
		/* use rate adjust regs to derrive the mdio's operating
		 * frequency from the specified core clock
		 */
		divisor = sc->clock_rate / MDIO_OPERATING_FREQUENCY;
		divisor = divisor / (MDIO_RATE_ADJ_DIVIDENT + 1);
		val = divisor;
		val |= MDIO_RATE_ADJ_DIVIDENT << MDIO_RATE_ADJ_DIVIDENT_SHIFT;
		bus_write_4(sc->reg_base, MDIO_RATE_ADJ_EXT_OFFSET, val);
		bus_write_4(sc->reg_base, MDIO_RATE_ADJ_INT_OFFSET, val);
	}

	return (0);
}

static int
brcm_iproc_mdio_write_mux(device_t dev, int bus, int phy, int reg, int val)
{
	struct brcm_iproc_mdio_softc *sc;

	sc = device_get_softc(dev);

	if (brcm_iproc_switch(sc, bus) != 0) {
		device_printf(dev, "Failed to set BUS MUX\n");
		return (EINVAL);
	}

	return (brcm_iproc_mdio_op(sc, phy, reg, val, MDIO_CTRL_WRITE_OP));
}

static int
brcm_iproc_mdio_read_mux(device_t dev, int bus, int phy, int reg)
{
	struct brcm_iproc_mdio_softc *sc;

	sc = device_get_softc(dev);

	if (brcm_iproc_switch(sc, bus) != 0) {
		device_printf(dev, "Failed to set BUS MUX\n");
		return (EINVAL);
	}

	return (brcm_iproc_mdio_op(sc, phy, reg, 0, MDIO_CTRL_READ_OP));
}

static int
brcm_iproc_mdio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_search_compatible(dev, mdio_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Broadcom MDIO MUX driver");
	return (BUS_PROBE_DEFAULT);
}

static int
brcm_iproc_mdio_attach(device_t dev)
{
	struct brcm_iproc_mdio_softc *sc;
	phandle_t node, parent;
	struct brcm_mdio_ofw_devinfo *di;
	int rid;
	device_t child;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate memory resources */
	rid = REG_BASE_RID;
	sc->reg_base = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->reg_base == NULL) {
		device_printf(dev, "Could not allocate memory\n");
		return (ENXIO);
	}

	/* Configure MDIO controlled */
	if (brcm_iproc_config(sc) < 0) {
		device_printf(dev, "Unable to initialize IPROC MDIO\n");
		goto error;
	}

	parent = ofw_bus_get_node(dev);
	simplebus_init(dev, parent);

	/* Iterate through all bus subordinates */
	for (node = OF_child(parent); node > 0; node = OF_peer(node)) {
		/* Allocate and populate devinfo. */
		di = malloc(sizeof(*di), M_BRCM_IPROC_MDIO, M_WAITOK | M_ZERO);
		if (ofw_bus_gen_setup_devinfo(&di->di_dinfo, node) != 0) {
			free(di, M_BRCM_IPROC_MDIO);
			continue;
		}

		/* Initialize and populate resource list. */
		resource_list_init(&di->di_rl);
		ofw_bus_reg_to_rl(dev, node, sc->sbus.acells, sc->sbus.scells,
		    &di->di_rl);
		ofw_bus_intr_to_rl(dev, node, &di->di_rl, NULL);

		/* Add newbus device for this FDT node */
		child = device_add_child(dev, NULL, -1);
		if (child == NULL) {
			printf("Failed to add child\n");
			resource_list_free(&di->di_rl);
			ofw_bus_gen_destroy_devinfo(&di->di_dinfo);
			free(di, M_BRCM_IPROC_MDIO);
			continue;
		}

		device_set_ivars(child, di);
	}

	/*
	 * Register device to this node/xref.
	 * Thanks to that we will be able to retrieve device_t structure
	 * while holding only node reference acquired from FDT.
	 */
	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (bus_generic_attach(dev));

error:
	brcm_iproc_mdio_detach(dev);
	return (ENXIO);
}

static const struct ofw_bus_devinfo *
brcm_iproc_mdio_get_devinfo(device_t bus __unused, device_t child)
{
	struct brcm_mdio_ofw_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_dinfo);
}

static int
brcm_iproc_mdio_detach(device_t dev)
{
	struct brcm_iproc_mdio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->reg_base != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, REG_BASE_RID,
		    sc->reg_base);
	}

	return (0);
}
