/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/bitset.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "msi_if.h"
#include "pic_if.h"

#define	MV_AP806_SEI_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	MV_AP806_SEI_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)
#define	MV_AP806_SEI_LOCK_INIT(_sc)	mtx_init(&_sc->mtx, 			\
	    device_get_nameunit(_sc->dev), "mv_ap806_sei", MTX_DEF)
#define	MV_AP806_SEI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->mtx);
#define	MV_AP806_SEI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->mtx, MA_OWNED);
#define	MV_AP806_SEI_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->mtx, MA_NOTOWNED);

#define GICP_SECR0		0x00
#define GICP_SECR1		0x04
#define GICP_SECR(i)		(0x00  + (((i)/32) * 0x4))
#define GICP_SECR_BIT(i)	((i) % 32)
#define GICP_SEMR0		0x20
#define GICP_SEMR1		0x24
#define GICP_SEMR(i)		(0x20  + (((i)/32) * 0x4))
#define GICP_SEMR_BIT(i)	((i) % 32)

#define	MV_AP806_SEI_AP_FIRST	0
#define	MV_AP806_SEI_AP_SIZE	21
#define	MV_AP806_SEI_CP_FIRST	21
#define	MV_AP806_SEI_CP_SIZE	43
#define	MV_AP806_SEI_MAX_NIRQS	(MV_AP806_SEI_AP_SIZE + MV_AP806_SEI_CP_SIZE)

#define	MV_AP806_SEI_SETSPI_OFFSET	0x30

BITSET_DEFINE(sei_msi_bitmap, MV_AP806_SEI_CP_SIZE);

struct mv_ap806_sei_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
};

struct mv_ap806_sei_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*irq_ih;
	struct mtx		mtx;

	struct mv_ap806_sei_irqsrc *isrcs;

	struct sei_msi_bitmap	msi_bitmap;
};

static struct ofw_compat_data compat_data[] = {
	{"marvell,ap806-sei", 1},
	{NULL,             0}
};

#define	RD4(sc, reg)		bus_read_4((sc)->mem_res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->mem_res, (reg), (val))

static msi_alloc_msi_t mv_ap806_sei_alloc_msi;
static msi_release_msi_t mv_ap806_sei_release_msi;
static msi_map_msi_t mv_ap806_sei_map_msi;

static inline void
mv_ap806_sei_isrc_mask(struct mv_ap806_sei_softc *sc,
     struct mv_ap806_sei_irqsrc *sisrc, uint32_t val)
{
	uint32_t tmp;
	int bit;

	bit = GICP_SEMR_BIT(sisrc->irq);
	MV_AP806_SEI_LOCK(sc);
	tmp = RD4(sc, GICP_SEMR(sisrc->irq));
	if (val != 0)
		tmp |= 1 << bit;
	else
		tmp &= ~(1 << bit);
	WR4(sc, GICP_SEMR(sisrc->irq), tmp);
	MV_AP806_SEI_UNLOCK(sc);
}

static inline void
mv_ap806_sei_isrc_eoi(struct mv_ap806_sei_softc *sc,
     struct mv_ap806_sei_irqsrc *sisrc)
{

	WR4(sc, GICP_SECR(sisrc->irq), GICP_SECR_BIT(sisrc->irq));
}

static void
mv_ap806_sei_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;
	mv_ap806_sei_isrc_mask(sc, sisrc, 0);
}

static void
mv_ap806_sei_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;
	mv_ap806_sei_isrc_mask(sc, sisrc, 1);
}

static int
mv_ap806_sei_map(device_t dev, struct intr_map_data *data, u_int *irqp)
{
	struct intr_map_data_fdt *daf;
	u_int irq;

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 1)
		return (EINVAL);

	if (daf->cells[0] < MV_AP806_SEI_AP_FIRST ||
	    daf->cells[0] >= MV_AP806_SEI_AP_FIRST + MV_AP806_SEI_AP_SIZE)
		return (EINVAL);

	irq = daf->cells[0];
	if (irqp != NULL)
		*irqp = irq;

	return(0);
}

static int
mv_ap806_sei_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct mv_ap806_sei_softc *sc;
	u_int irq;
	int rv;

	sc = device_get_softc(dev);
	rv = mv_ap806_sei_map(dev, data, &irq);
	if (rv == 0)
		*isrcp = &sc->isrcs[irq].isrc;

	return (rv);
}

static int
mv_ap806_sei_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;
	u_int irq;
	int rv;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;
	if (data == NULL)
		return (ENOTSUP);
	rv = mv_ap806_sei_map(dev, data, &irq);
	if (rv != 0)
		return (rv);
	if (irq != sisrc->irq)
		return (EINVAL);
	mv_ap806_sei_isrc_mask(sc, sisrc, 0);
	return (0);
}

static int
mv_ap806_sei_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;

	mv_ap806_sei_isrc_mask(sc, sisrc, 1);
	return (0);
}

static void
mv_ap806_sei_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;

	mv_ap806_sei_isrc_mask(sc, sisrc, 1);
	mv_ap806_sei_isrc_eoi(sc, sisrc);
}

static void
mv_ap806_sei_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;

	mv_ap806_sei_isrc_mask(sc, sisrc, 1);
}

static void
mv_ap806_sei_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sisrc;

	sc = device_get_softc(dev);
	sisrc = (struct mv_ap806_sei_irqsrc *)isrc;

	mv_ap806_sei_isrc_mask(sc, sisrc, 1);
	mv_ap806_sei_isrc_eoi(sc, sisrc);
}

/* ----------------------------------------------------------------------------
 *
 *		B u s    i n t e r f a c e
 */
static int
mv_ap806_sei_intr(void *arg)
{
	struct mv_ap806_sei_softc *sc;
	struct mv_ap806_sei_irqsrc *sirq;
	struct trapframe *tf;
	uint64_t cause;
	u_int irq;

	sc = (struct mv_ap806_sei_softc *)arg;
	tf = curthread->td_intr_frame;
	while (1) {
		cause = RD4(sc, GICP_SECR1);
		cause <<= 32;
		cause |= RD4(sc, GICP_SECR0);

		irq = ffsll(cause);
		if (irq == 0) break;
		irq--;
		sirq = &sc->isrcs[irq];
		if (intr_isrc_dispatch(&sirq->isrc, tf) != 0) {
			mv_ap806_sei_isrc_mask(sc, sirq, 0);
			mv_ap806_sei_isrc_eoi(sc, sirq);
			device_printf(sc->dev,
			    "Stray irq %u disabled\n", irq);
		}
	}

	return (FILTER_HANDLED);
}

static int
mv_ap806_sei_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell SEI");
	return (BUS_PROBE_DEFAULT);
}

static int
mv_ap806_sei_attach(device_t dev)
{
	struct mv_ap806_sei_softc *sc;
	phandle_t xref, node;
	uint32_t irq;
	const char *name;
	int rv, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	MV_AP806_SEI_LOCK_INIT(sc);

	/* Allocate resources. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		rv = ENXIO;
		goto fail;
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		rv = ENXIO;
		goto fail;
	}

	/* Mask all interrupts) */
	WR4(sc, GICP_SEMR0, 0xFFFFFFFF);
	WR4(sc, GICP_SEMR1, 0xFFFFFFFF);

	/* Create all interrupt sources */
	sc->isrcs = malloc(sizeof(*sc->isrcs) * MV_AP806_SEI_MAX_NIRQS,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	name = device_get_nameunit(sc->dev);
	for (irq = 0; irq < MV_AP806_SEI_MAX_NIRQS; irq++) {
		sc->isrcs[irq].irq = irq;
		rv = intr_isrc_register(&sc->isrcs[irq].isrc,
		    sc->dev, 0, "%s,%u", name, irq);
		if (rv != 0)
			goto fail; /* XXX deregister ISRCs */
	}
	xref = OF_xref_from_node(node);
	if (intr_pic_register(dev, xref) == NULL) {
		device_printf(dev, "Cannot register SEI\n");
		rv = ENXIO;
		goto fail;
	}
	if (bus_setup_intr(dev, sc->irq_res,INTR_TYPE_MISC | INTR_MPSAFE,
	    mv_ap806_sei_intr, NULL, sc, &sc->irq_ih)) {
		device_printf(dev,
		    "Unable to register interrupt handler\n");
		rv = ENXIO;
		goto fail;
	}

	/*
	 * Bitmap of all IRQs.
	 * 1 - available, 0 - used.
	 */
	BIT_FILL(MV_AP806_SEI_CP_SIZE, &sc->msi_bitmap);

	OF_device_register_xref(xref, dev);
	return (0);

fail:
	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	MV_AP806_SEI_LOCK_DESTROY(sc);
	return (ENXIO);
}

static int
mv_ap806_sei_detach(device_t dev)
{

	return (EBUSY);
}

static int
mv_ap806_sei_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct mv_ap806_sei_softc *sc;
	int i, ret = 0, vector;

	sc = device_get_softc(dev);

	for (i = 0; i < count; i++) {
		/*
		 * Find first available MSI vector represented by first set bit
		 * in the bitmap. BIT_FFS starts the count from 1,
		 * 0 means that nothing was found.
		 */
		vector = BIT_FFS_AT(MV_AP806_SEI_CP_SIZE, &sc->msi_bitmap, 0);
		if (vector == 0) {
			ret = ENOMEM;
			i--;
			goto fail;
		}

		vector--;
		BIT_CLR(MV_AP806_SEI_CP_SIZE, vector, &sc->msi_bitmap);
		vector += MV_AP806_SEI_CP_FIRST;

		srcs[i] = &sc->isrcs[vector].isrc;
	}

	return (ret);
fail:
	mv_ap806_sei_release_msi(dev, child, i + 1, srcs);
	return (ret);
}

static int
mv_ap806_sei_release_msi(device_t dev, device_t child, int count, struct intr_irqsrc **srcs)
{
	struct mv_ap806_sei_softc *sc;
	int i;

	sc = device_get_softc(dev);

	for (i = 0; i < count; i++) {
		BIT_SET(MV_AP806_SEI_CP_SIZE,
		    srcs[i]->isrc_irq - MV_AP806_SEI_CP_FIRST,
		    &sc->msi_bitmap);
	}

	return (0);
}

static int
mv_ap806_sei_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct mv_ap806_sei_softc *sc;

	sc = device_get_softc(dev);

	*addr = rman_get_start(sc->mem_res) + MV_AP806_SEI_SETSPI_OFFSET;
	*data = isrc->isrc_irq;

	return (0);
}

static device_method_t mv_ap806_sei_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mv_ap806_sei_probe),
	DEVMETHOD(device_attach,	mv_ap806_sei_attach),
	DEVMETHOD(device_detach,	mv_ap806_sei_detach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	mv_ap806_sei_disable_intr),
	DEVMETHOD(pic_enable_intr,	mv_ap806_sei_enable_intr),
	DEVMETHOD(pic_map_intr,		mv_ap806_sei_map_intr),
	DEVMETHOD(pic_setup_intr,	mv_ap806_sei_setup_intr),
	DEVMETHOD(pic_teardown_intr,	mv_ap806_sei_teardown_intr),
	DEVMETHOD(pic_post_filter,	mv_ap806_sei_post_filter),
	DEVMETHOD(pic_post_ithread,	mv_ap806_sei_post_ithread),
	DEVMETHOD(pic_pre_ithread,	mv_ap806_sei_pre_ithread),

	/* MSI interface */
	DEVMETHOD(msi_alloc_msi,	mv_ap806_sei_alloc_msi),
	DEVMETHOD(msi_release_msi,	mv_ap806_sei_release_msi),
	DEVMETHOD(msi_map_msi,		mv_ap806_sei_map_msi),

	DEVMETHOD_END
};

static driver_t mv_ap806_sei_driver = {
	"mv_ap806_sei",
	mv_ap806_sei_methods,
	sizeof(struct mv_ap806_sei_softc),
};

EARLY_DRIVER_MODULE(mv_ap806_sei, simplebus, mv_ap806_sei_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
