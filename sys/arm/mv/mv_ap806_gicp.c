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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/bitset.h>
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

#include <arm/arm/gic_common.h>

#include <dt-bindings/interrupt-controller/irq.h>

#include "msi_if.h"
#include "pic_if.h"

#define	MV_AP806_GICP_MAX_NIRQS	207

MALLOC_DECLARE(M_GICP);
MALLOC_DEFINE(M_GICP, "gicp", "Marvell gicp driver");

struct mv_ap806_gicp_softc {
	device_t		dev;
	device_t		parent;
	struct resource		*res;

	ssize_t			spi_ranges_cnt;
	uint32_t		*spi_ranges;
	struct intr_map_data_fdt *parent_map_data;

	ssize_t			msi_bitmap_size; /* Nr of bits in the bitmap. */
	BITSET_DEFINE_VAR()     *msi_bitmap;
};

static struct ofw_compat_data compat_data[] = {
	{"marvell,ap806-gicp", 1},
	{NULL,             0}
};

#define	RD4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static msi_alloc_msi_t mv_ap806_gicp_alloc_msi;
static msi_release_msi_t mv_ap806_gicp_release_msi;
static msi_map_msi_t mv_ap806_gicp_map_msi;

static int
mv_ap806_gicp_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell GICP");
	return (BUS_PROBE_DEFAULT);
}

static int
mv_ap806_gicp_attach(device_t dev)
{
	struct mv_ap806_gicp_softc *sc;
	phandle_t node, xref, intr_parent;
	int i, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/* Look for our parent */
	if ((intr_parent = ofw_bus_find_iparent(node)) == 0) {
		device_printf(dev,
		     "Cannot find our parent interrupt controller\n");
		return (ENXIO);
	}
	if ((sc->parent = OF_device_from_xref(intr_parent)) == NULL) {
		device_printf(dev,
		     "cannot find parent interrupt controller device\n");
		return (ENXIO);
	}

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
        }

	sc->spi_ranges_cnt = OF_getencprop_alloc_multi(node, "marvell,spi-ranges",
	    sizeof(*sc->spi_ranges), (void **)&sc->spi_ranges);

	sc->msi_bitmap_size = 0;
	for (i = 0; i < sc->spi_ranges_cnt; i += 2)
		sc->msi_bitmap_size += sc->spi_ranges[i + 1];

	/*
	 * Create a bitmap of all MSIs that we have.
	 * Each has a correspoding SPI in the GIC.
	 * It will be used to dynamically allocate IRQs when requested.
	 */
	sc->msi_bitmap = BITSET_ALLOC(sc->msi_bitmap_size, M_GICP, M_WAITOK);
	BIT_FILL(sc->msi_bitmap_size, sc->msi_bitmap);	/* 1 - available, 0 - used. */

	xref = OF_xref_from_node(node);
	if (intr_pic_register(dev, xref) == NULL) {
		device_printf(dev, "Cannot register GICP\n");
		return (ENXIO);
	}
	/* Allocate GIC compatible mapping entry (3 cells) */
	sc->parent_map_data = (struct intr_map_data_fdt *)intr_alloc_map_data(
	    INTR_MAP_DATA_FDT, sizeof(struct intr_map_data_fdt) +
	    + 3 * sizeof(phandle_t), M_WAITOK | M_ZERO);
	OF_device_register_xref(xref, dev);

	return (0);
}

static int
mv_ap806_gicp_detach(device_t dev)
{

	return (EBUSY);
}

static uint32_t
mv_ap806_gicp_msi_to_spi(struct mv_ap806_gicp_softc *sc, int irq)
{
	int i;

	for (i = 0; i < sc->spi_ranges_cnt; i += 2) {
		if (irq < sc->spi_ranges[i + 1]) {
			irq += sc->spi_ranges[i];
			break;
		}
		irq -= sc->spi_ranges[i + 1];
	}

	return (irq - GIC_FIRST_SPI);
}

static uint32_t
mv_ap806_gicp_irq_to_msi(struct mv_ap806_gicp_softc *sc, int irq)
{
	int i;

	for (i = 0; i < sc->spi_ranges_cnt; i += 2) {
		if (irq >= sc->spi_ranges[i] &&
		    irq - sc->spi_ranges[i] < sc->spi_ranges[i + 1]) {
			irq -= sc->spi_ranges[i];
			break;
		}
	}

	return (irq);
}

static struct intr_map_data *
mv_ap806_gicp_convert_map_data(struct mv_ap806_gicp_softc *sc,
    struct intr_map_data *data)
{
	struct intr_map_data_fdt *daf;
	uint32_t irq_num;

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 2)
		return (NULL);

	irq_num = daf->cells[0];
	if (irq_num >= MV_AP806_GICP_MAX_NIRQS)
		return (NULL);

	/* Construct GIC compatible mapping. */
	sc->parent_map_data->ncells = 3;
	sc->parent_map_data->cells[0] = 0; /* SPI */
	sc->parent_map_data->cells[1] = mv_ap806_gicp_msi_to_spi(sc, irq_num);
	sc->parent_map_data->cells[2] = IRQ_TYPE_LEVEL_HIGH;

	return ((struct intr_map_data *)sc->parent_map_data);
}

static int
mv_ap806_gicp_activate_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);
	data = mv_ap806_gicp_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	return (PIC_ACTIVATE_INTR(sc->parent, isrc, res, data));
}

static void
mv_ap806_gicp_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	PIC_ENABLE_INTR(sc->parent, isrc);
}

static void
mv_ap806_gicp_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	PIC_DISABLE_INTR(sc->parent, isrc);
}

static int
mv_ap806_gicp_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{

	panic("%s: MSI interface has to be used to map an interrupt.\n",
	    __func__);
}

static int
mv_ap806_gicp_deactivate_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	data = mv_ap806_gicp_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	return (PIC_DEACTIVATE_INTR(sc->parent, isrc, res, data));
}

static int
mv_ap806_gicp_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);
	data = mv_ap806_gicp_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	return (PIC_SETUP_INTR(sc->parent, isrc, res, data));
}

static int
mv_ap806_gicp_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);
	data = mv_ap806_gicp_convert_map_data(sc, data);
	if (data == NULL)
		return (EINVAL);

	return (PIC_TEARDOWN_INTR(sc->parent, isrc, res, data));
}

static void
mv_ap806_gicp_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	PIC_PRE_ITHREAD(sc->parent, isrc);
}

static void
mv_ap806_gicp_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	PIC_POST_ITHREAD(sc->parent, isrc);
}

static void
mv_ap806_gicp_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	PIC_POST_FILTER(sc->parent, isrc);
}

static int
mv_ap806_gicp_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct mv_ap806_gicp_softc *sc;
	int i, ret, vector;

	sc = device_get_softc(dev);

	for (i = 0; i < count; i++) {
		/*
		 * Find first available vector represented by first set bit
		 * in the bitmap. BIT_FFS starts the count from 1, 0 means
		 * that nothing was found.
		 */
		vector = BIT_FFS(sc->msi_bitmap_size, sc->msi_bitmap);
		if (vector == 0) {
			ret = ENOMEM;
			i--;
			goto fail;
		}
		vector--;
		BIT_CLR(sc->msi_bitmap_size, vector, sc->msi_bitmap);

		/* Create GIC compatible SPI interrupt description. */
		sc->parent_map_data->ncells = 3;
		sc->parent_map_data->cells[0] = 0;	/* SPI */
		sc->parent_map_data->cells[1] = mv_ap806_gicp_msi_to_spi(sc, vector);
		sc->parent_map_data->cells[2] = IRQ_TYPE_LEVEL_HIGH;

		ret = PIC_MAP_INTR(sc->parent,
		    (struct intr_map_data *)sc->parent_map_data,
		    &srcs[i]);
		if (ret != 0)
			goto fail;

		srcs[i]->isrc_dev = dev;
	}

	return (0);
fail:
	mv_ap806_gicp_release_msi(dev, child, i + 1, srcs);
	return (ret);
}

static int
mv_ap806_gicp_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **srcs)
{
	struct mv_ap806_gicp_softc *sc;
	int i;

	sc = device_get_softc(dev);

	for (i = 0; i < count; i++) {
		BIT_SET(sc->msi_bitmap_size,
		    mv_ap806_gicp_irq_to_msi(sc, srcs[i]->isrc_irq),
		    sc->msi_bitmap);
	}

	return (0);
}

static int
mv_ap806_gicp_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct mv_ap806_gicp_softc *sc;

	sc = device_get_softc(dev);

	*addr = rman_get_start(sc->res);
	*data = mv_ap806_gicp_irq_to_msi(sc, isrc->isrc_irq);

	return (0);
}

static device_method_t mv_ap806_gicp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mv_ap806_gicp_probe),
	DEVMETHOD(device_attach,	mv_ap806_gicp_attach),
	DEVMETHOD(device_detach,	mv_ap806_gicp_detach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_activate_intr,	mv_ap806_gicp_activate_intr),
	DEVMETHOD(pic_disable_intr,	mv_ap806_gicp_disable_intr),
	DEVMETHOD(pic_enable_intr,	mv_ap806_gicp_enable_intr),
	DEVMETHOD(pic_map_intr,		mv_ap806_gicp_map_intr),
	DEVMETHOD(pic_deactivate_intr,	mv_ap806_gicp_deactivate_intr),
	DEVMETHOD(pic_setup_intr,	mv_ap806_gicp_setup_intr),
	DEVMETHOD(pic_teardown_intr,	mv_ap806_gicp_teardown_intr),
	DEVMETHOD(pic_post_filter,	mv_ap806_gicp_post_filter),
	DEVMETHOD(pic_post_ithread,	mv_ap806_gicp_post_ithread),
	DEVMETHOD(pic_pre_ithread,	mv_ap806_gicp_pre_ithread),

	/* MSI interface */
	DEVMETHOD(msi_alloc_msi,	mv_ap806_gicp_alloc_msi),
	DEVMETHOD(msi_release_msi,	mv_ap806_gicp_release_msi),
	DEVMETHOD(msi_map_msi,		mv_ap806_gicp_map_msi),

	DEVMETHOD_END
};

static driver_t mv_ap806_gicp_driver = {
	"mv_ap806_gicp",
	mv_ap806_gicp_methods,
	sizeof(struct mv_ap806_gicp_softc),
};

EARLY_DRIVER_MODULE(mv_ap806_gicp, simplebus, mv_ap806_gicp_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
