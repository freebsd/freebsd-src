/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
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
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/irq.h>

#include "pic_if.h"
#include "msi_if.h"

#define	ICU_TYPE_NSR		1
#define	ICU_TYPE_SEI		2

#define	ICU_GRP_NSR		0x0
#define	ICU_GRP_SR		0x1
#define	ICU_GRP_SEI		0x4
#define	ICU_GRP_REI		0x5

#define	ICU_SETSPI_NSR_AL	0x10
#define	ICU_SETSPI_NSR_AH	0x14
#define	ICU_CLRSPI_NSR_AL	0x18
#define	ICU_CLRSPI_NSR_AH	0x1c
#define	ICU_SETSPI_SEI_AL	0x50
#define	ICU_SETSPI_SEI_AH	0x54
#define	ICU_INT_CFG(x)	(0x100 + (x) * 4)
#define	 ICU_INT_ENABLE		(1 << 24)
#define	 ICU_INT_EDGE		(1 << 28)
#define	 ICU_INT_GROUP_SHIFT	29
#define	 ICU_INT_MASK		0x3ff

#define	ICU_INT_SATA0		109
#define	ICU_INT_SATA1		107

#define	MV_CP110_ICU_MAX_NIRQS	207

#define	MV_CP110_ICU_CLRSPI_OFFSET	0x8

struct mv_cp110_icu_softc {
	device_t		dev;
	device_t		parent;
	struct resource		*res;
	struct intr_map_data_fdt *parent_map_data;
	bool			initialized;
	int			type;
};

static struct resource_spec mv_cp110_icu_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

static struct ofw_compat_data compat_data[] = {
	{"marvell,cp110-icu-nsr",	ICU_TYPE_NSR},
	{"marvell,cp110-icu-sei",	ICU_TYPE_SEI},
	{NULL,				0}
};

#define	RD4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int
mv_cp110_icu_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell Interrupt Consolidation Unit");
	return (BUS_PROBE_DEFAULT);
}

static int
mv_cp110_icu_attach(device_t dev)
{
	struct mv_cp110_icu_softc *sc;
	phandle_t node, msi_parent;
	uint32_t reg, icu_grp;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	sc->type = (int)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	sc->initialized = false;

	if (OF_getencprop(node, "msi-parent", &msi_parent,
	    sizeof(phandle_t)) <= 0) {
		device_printf(dev, "cannot find msi-parent property\n");
		return (ENXIO);
	}

	if ((sc->parent = OF_device_from_xref(msi_parent)) == NULL) {
		device_printf(dev, "cannot find msi-parent device\n");
		return (ENXIO);
	}
	if (bus_alloc_resources(dev, mv_cp110_icu_res_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	if (intr_pic_register(dev, OF_xref_from_node(node)) == NULL) {
		device_printf(dev, "Cannot register ICU\n");
		goto fail;
	}

	/* Allocate GICP/SEI compatible mapping entry (2 cells) */
	sc->parent_map_data = (struct intr_map_data_fdt *)intr_alloc_map_data(
	    INTR_MAP_DATA_FDT, sizeof(struct intr_map_data_fdt) +
	    + 3 * sizeof(phandle_t), M_WAITOK | M_ZERO);

	/* Clear any previous mapping done by firmware. */
	for (i = 0; i < MV_CP110_ICU_MAX_NIRQS; i++) {
		reg = RD4(sc, ICU_INT_CFG(i));
		icu_grp = reg >> ICU_INT_GROUP_SHIFT;

		if (icu_grp == ICU_GRP_NSR || icu_grp == ICU_GRP_SEI)
			WR4(sc, ICU_INT_CFG(i), 0);
	}

	return (0);

fail:
	bus_release_resources(dev, mv_cp110_icu_res_spec, &sc->res);
	return (ENXIO);
}

static struct intr_map_data *
mv_cp110_icu_convert_map_data(struct mv_cp110_icu_softc *sc, struct intr_map_data *data)
{
	struct intr_map_data_fdt *daf;
	uint32_t reg, irq_no, irq_type;

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 2)
		return (NULL);

	irq_no = daf->cells[0];
	if (irq_no >= MV_CP110_ICU_MAX_NIRQS)
		return (NULL);

	irq_type = daf->cells[1];
	if (irq_type != IRQ_TYPE_LEVEL_HIGH &&
	    irq_type != IRQ_TYPE_EDGE_RISING)
		return (NULL);

	/* ICU -> GICP/SEI mapping is set in mv_cp110_icu_map_intr. */
	reg = RD4(sc, ICU_INT_CFG(irq_no));

	/* Construct GICP compatible mapping. */
	sc->parent_map_data->ncells = 2;
	sc->parent_map_data->cells[0] = reg & ICU_INT_MASK;
	sc->parent_map_data->cells[1] = irq_type;

	return ((struct intr_map_data *)sc->parent_map_data);
}

static int
mv_cp110_icu_detach(device_t dev)
{

	return (EBUSY);
}

static int
mv_cp110_icu_activate_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);
	data = mv_cp110_icu_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);
	return (PIC_ACTIVATE_INTR(sc->parent, isrc, res, data));
}

static void
mv_cp110_icu_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_cp110_icu_softc *sc;
	sc = device_get_softc(dev);

	PIC_ENABLE_INTR(sc->parent, isrc);
}

static void
mv_cp110_icu_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);

	PIC_DISABLE_INTR(sc->parent, isrc);
}

static void
mv_cp110_icu_init(struct mv_cp110_icu_softc *sc, uint64_t addr)
{

	if (sc->initialized)
		return;

	switch (sc->type) {
	case ICU_TYPE_NSR:
		WR4(sc, ICU_SETSPI_NSR_AL, addr & UINT32_MAX);
		WR4(sc, ICU_SETSPI_NSR_AH, (addr >> 32) & UINT32_MAX);
		addr += MV_CP110_ICU_CLRSPI_OFFSET;
		WR4(sc, ICU_CLRSPI_NSR_AL, addr & UINT32_MAX);
		WR4(sc, ICU_CLRSPI_NSR_AH, (addr >> 32) & UINT32_MAX);
		break;
	case ICU_TYPE_SEI:
		WR4(sc, ICU_SETSPI_SEI_AL, addr & UINT32_MAX);
		WR4(sc, ICU_SETSPI_SEI_AH, (addr >> 32) & UINT32_MAX);
		break;
	default:
		panic("Unkown ICU type.");
	}

	sc->initialized = true;
}

static int
mv_cp110_icu_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct mv_cp110_icu_softc *sc;
	struct intr_map_data_fdt *daf;
	uint32_t vector, irq_no, irq_type;
	uint64_t addr;
	int ret;

	sc = device_get_softc(dev);

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	/* Parse original */
	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 2)
		return (EINVAL);

	irq_no = daf->cells[0];
	if (irq_no >= MV_CP110_ICU_MAX_NIRQS)
		return (EINVAL);

	irq_type = daf->cells[1];
	if (irq_type != IRQ_TYPE_LEVEL_HIGH &&
	    irq_type != IRQ_TYPE_EDGE_RISING)
		return (EINVAL);

	/*
	 * Allocate MSI vector.
	 * We don't use intr_alloc_msi wrapper, since it registers a new irq
	 * in the kernel. In our case irq was already added by the ofw code.
	 */
	ret = MSI_ALLOC_MSI(sc->parent, dev, 1, 1, NULL, isrcp);
	if (ret != 0)
		return (ret);

	ret = MSI_MAP_MSI(sc->parent, dev, *isrcp, &addr, &vector);
	if (ret != 0)
		goto fail;

	mv_cp110_icu_init(sc, addr);
	vector |= ICU_INT_ENABLE;

	if (sc->type == ICU_TYPE_NSR)
		vector |= ICU_GRP_NSR << ICU_INT_GROUP_SHIFT;
	else
		vector |= ICU_GRP_SEI << ICU_INT_GROUP_SHIFT;

	if (irq_type & IRQ_TYPE_EDGE_BOTH)
		vector |= ICU_INT_EDGE;

	WR4(sc, ICU_INT_CFG(irq_no), vector);

	/*
	 * SATA controller has two ports, each gets its own interrupt.
	 * The problem is that only one irq is described in dts.
	 * Also ahci_generic driver supports only one irq per controller.
	 * As a workaround map both interrupts when one of them is allocated.
	 * This allows us to use both SATA ports.
	 */
	if (irq_no == ICU_INT_SATA0)
		WR4(sc, ICU_INT_CFG(ICU_INT_SATA1), vector);
	if (irq_no == ICU_INT_SATA1)
		WR4(sc, ICU_INT_CFG(ICU_INT_SATA0), vector);

	(*isrcp)->isrc_dev = sc->dev;
	return (ret);

fail:
	if (*isrcp != NULL)
		MSI_RELEASE_MSI(sc->parent, dev, 1, isrcp);

	return (ret);
}

static int
mv_cp110_icu_deactivate_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_cp110_icu_softc *sc;
	struct intr_map_data_fdt *daf;
	int irq_no, ret;

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	sc = device_get_softc(dev);
	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 2)
		return (EINVAL);

	irq_no = daf->cells[0];
	data = mv_cp110_icu_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	/* Clear the mapping. */
	WR4(sc, ICU_INT_CFG(irq_no), 0);

	ret = PIC_DEACTIVATE_INTR(sc->parent, isrc, res, data);
	if (ret != 0)
		return (ret);

	return (MSI_RELEASE_MSI(sc->parent, dev, 1, &isrc));
}

static int
mv_cp110_icu_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);
	data = mv_cp110_icu_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	return (PIC_SETUP_INTR(sc->parent, isrc, res, data));
}

static int
mv_cp110_icu_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);
	data = mv_cp110_icu_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	return (PIC_TEARDOWN_INTR(sc->parent, isrc, res, data));
}

static void
mv_cp110_icu_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);

	PIC_PRE_ITHREAD(sc->parent, isrc);
}

static void
mv_cp110_icu_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);

	PIC_POST_ITHREAD(sc->parent, isrc);
}

static void
mv_cp110_icu_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_cp110_icu_softc *sc;

	sc = device_get_softc(dev);

	PIC_POST_FILTER(sc->parent, isrc);
}

static device_method_t mv_cp110_icu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mv_cp110_icu_probe),
	DEVMETHOD(device_attach,	mv_cp110_icu_attach),
	DEVMETHOD(device_detach,	mv_cp110_icu_detach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_activate_intr,	mv_cp110_icu_activate_intr),
	DEVMETHOD(pic_disable_intr,	mv_cp110_icu_disable_intr),
	DEVMETHOD(pic_enable_intr,	mv_cp110_icu_enable_intr),
	DEVMETHOD(pic_map_intr,		mv_cp110_icu_map_intr),
	DEVMETHOD(pic_deactivate_intr,	mv_cp110_icu_deactivate_intr),
	DEVMETHOD(pic_setup_intr,	mv_cp110_icu_setup_intr),
	DEVMETHOD(pic_teardown_intr,	mv_cp110_icu_teardown_intr),
	DEVMETHOD(pic_post_filter,	mv_cp110_icu_post_filter),
	DEVMETHOD(pic_post_ithread,	mv_cp110_icu_post_ithread),
	DEVMETHOD(pic_pre_ithread,	mv_cp110_icu_pre_ithread),

	DEVMETHOD_END
};

static driver_t mv_cp110_icu_driver = {
	"mv_cp110_icu",
	mv_cp110_icu_methods,
	sizeof(struct mv_cp110_icu_softc),
};

EARLY_DRIVER_MODULE(mv_cp110_icu, mv_cp110_icu_bus, mv_cp110_icu_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
