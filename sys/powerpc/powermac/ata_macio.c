/*-
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

/*
 * Mac-io ATA controller
 */
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/ata.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

#include <dev/ofw/ofw_bus.h>

#include "ata_dbdma.h"

/*
 * Offset to control registers from base
 */
#define ATA_MACIO_ALTOFFSET	0x160

/*
 * Define the gap between registers
 */
#define ATA_MACIO_REGGAP	16

/*
 * Whether or not to bind to the DBDMA IRQ
 */
#define USE_DBDMA_IRQ		0

/*
 * Timing register
 */
#define ATA_MACIO_TIMINGREG	0x200

#define ATA_TIME_TO_TICK(rev,time) howmany(time, (rev == 4) ? 15 : 30)
#define PIO_REC_OFFSET 4
#define PIO_REC_MIN 1
#define PIO_ACT_MIN 1
#define DMA_REC_OFFSET 1
#define DMA_REC_MIN 1
#define DMA_ACT_MIN 1

struct ide_timings {
	int cycle;      /* minimum cycle time [ns] */
	int active;     /* minimum command active time [ns] */
};

struct ide_timings pio_timings[5] = {
	{ 600, 180 },	/* PIO 0 */
	{ 390, 150 },	/* PIO 1 */
	{ 240, 105 },	/* PIO 2 */
	{ 180,  90 },	/* PIO 3 */
	{ 120,  75 }	/* PIO 4 */
};

static const struct ide_timings dma_timings[3] = {
	{ 480, 240 },	/* WDMA 0 */
	{ 165,  90 },	/* WDMA 1 */
	{ 120,  75 }	/* WDMA 2 */
};

static const struct ide_timings udma_timings[5] = {
        { 120, 180 },	/* UDMA 0 */
        {  90, 150 },	/* UDMA 1 */
        {  60, 120 },	/* UDMA 2 */
        {  45,  90 },	/* UDMA 3 */
        {  30,  90 }	/* UDMA 4 */
};

/*
 * Define the macio ata bus attachment.
 */
static  int  ata_macio_probe(device_t dev);
static  void ata_macio_setmode(device_t parent, device_t dev);
static  int  ata_macio_attach(device_t dev);
static  int  ata_macio_begin_transaction(struct ata_request *request);

static device_method_t ata_macio_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,		ata_macio_probe),
	DEVMETHOD(device_attach,        ata_macio_attach),

	/* ATA interface */
	DEVMETHOD(ata_setmode,		ata_macio_setmode),
	{ 0, 0 }
};

struct ata_macio_softc {
	struct ata_dbdma_channel sc_ch;

	int rev;
	int max_mode;
	struct resource *sc_mem;

	uint32_t udmaconf[2];
	uint32_t wdmaconf[2];
	uint32_t pioconf[2];
};

static driver_t ata_macio_driver = {
	"ata",
	ata_macio_methods,
	sizeof(struct ata_macio_softc),
};

DRIVER_MODULE(ata, macio, ata_macio_driver, ata_devclass, 0, 0);
MODULE_DEPEND(ata, ata, 1, 1, 1);

static int
ata_macio_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);
	const char *name = ofw_bus_get_name(dev);
	struct ata_macio_softc *sc;
	struct ata_channel *ch;
	int rid, i;

	if (strcmp(type, "ata") != 0 &&
	    strcmp(type, "ide") != 0)
		return (ENXIO);

	sc = device_get_softc(dev);
	bzero(sc, sizeof(struct ata_macio_softc));
	ch = &sc->sc_ch.sc_ch;

	if (strcmp(name,"ata-4") == 0) {
		sc->rev = 4;
		sc->max_mode = ATA_UDMA4;
	} else {
		sc->rev = 3;
		sc->max_mode = ATA_WDMA2;
	}

	rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "could not allocate memory\n");
		return (ENXIO);
	}

	/*
	 * Set up the resource vectors
	 */
	for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
		ch->r_io[i].res = sc->sc_mem;
		ch->r_io[i].offset = i * ATA_MACIO_REGGAP;
	}
	ch->r_io[ATA_CONTROL].res = sc->sc_mem;
	ch->r_io[ATA_CONTROL].offset = ATA_MACIO_ALTOFFSET;
	ata_default_registers(dev);

	ch->unit = 0;
	ch->flags |= ATA_USE_16BIT;
	ata_generic_hw(dev);

	return (ata_probe(dev));
}

static int
ata_macio_attach(device_t dev)
{
	struct ata_macio_softc *sc = device_get_softc(dev);
	uint32_t timingreg;

#if USE_DBDMA_IRQ
	int dbdma_irq_rid = 1;
	struct resource *dbdma_irq;
	void *cookie;
#endif

	/* Init DMA engine */

	sc->sc_ch.dbdma_rid = 1;
	sc->sc_ch.dbdma_regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 
	    &sc->sc_ch.dbdma_rid, RF_ACTIVE); 

	ata_dbdma_dmainit(dev);

	/* Configure initial timings */
	timingreg = bus_read_4(sc->sc_mem, ATA_MACIO_TIMINGREG);
	if (sc->rev == 4) {
		sc->udmaconf[0] = sc->udmaconf[1] = timingreg & 0x1ff00000;
		sc->wdmaconf[0] = sc->wdmaconf[1] = timingreg & 0x001ffc00;
		sc->pioconf[0]  = sc->pioconf[1]  = timingreg & 0x000003ff;
	} else {
		sc->udmaconf[0] = sc->udmaconf[1] = 0;
		sc->wdmaconf[0] = sc->wdmaconf[1] = timingreg & 0xfffff800;
		sc->pioconf[0]  = sc->pioconf[1]  = timingreg & 0x000007ff;
	}

#if USE_DBDMA_IRQ
	/* Bind to DBDMA interrupt as well */

	if ((dbdma_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &dbdma_irq_rid, RF_SHAREABLE | RF_ACTIVE)) != NULL) {
		bus_setup_intr(dev, dbdma_irq, ATA_INTR_FLAGS, NULL,
			(driver_intr_t *)ata_interrupt, sc,&cookie);
	} 
#endif

	/* Set begin_transaction */
	sc->sc_ch.sc_ch.hw.begin_transaction = ata_macio_begin_transaction;

	return ata_attach(dev);
}

static void
ata_macio_setmode(device_t parent, device_t dev)
{
	struct ata_device *atadev = device_get_softc(dev);
	struct ata_macio_softc *sc = device_get_softc(parent);
	int mode = atadev->mode;

	int min_cycle = 0, min_active = 0;
        int cycle_tick = 0, act_tick = 0, inact_tick = 0, half_tick;

	mode = ata_limit_mode(dev, mode, sc->max_mode);

	if (ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
		return;

	atadev->mode = mode;

	if ((mode & ATA_DMA_MASK) == ATA_UDMA0) {
		min_cycle = udma_timings[mode & ATA_MODE_MASK].cycle;
		min_active = udma_timings[mode & ATA_MODE_MASK].active;
	
		cycle_tick = ATA_TIME_TO_TICK(sc->rev,min_cycle);
		act_tick = ATA_TIME_TO_TICK(sc->rev,min_active);

		/* mask: 0x1ff00000 */
		sc->udmaconf[atadev->unit] =
		    (cycle_tick << 21) | (act_tick << 25) | 0x100000;
	} else if ((mode & ATA_DMA_MASK) == ATA_WDMA0) {
		min_cycle = dma_timings[mode & ATA_MODE_MASK].cycle;
		min_active = dma_timings[mode & ATA_MODE_MASK].active;
	
		cycle_tick = ATA_TIME_TO_TICK(sc->rev,min_cycle);
		act_tick = ATA_TIME_TO_TICK(sc->rev,min_active);

		if (sc->rev == 4) {
			inact_tick = cycle_tick - act_tick;
			/* mask: 0x001ffc00 */
			sc->wdmaconf[atadev->unit] = 
			    (act_tick << 10) | (inact_tick << 15);
		} else {
			inact_tick = cycle_tick - act_tick - DMA_REC_OFFSET;
			if (inact_tick < DMA_REC_MIN)
				inact_tick = DMA_REC_MIN;
			half_tick = 0;  /* XXX */

			/* mask: 0xfffff800 */
			sc->wdmaconf[atadev->unit] = (half_tick << 21) 
			    | (inact_tick << 16) | (act_tick << 11);
		}
	} else {
		min_cycle = 
		    pio_timings[(mode & ATA_MODE_MASK) - ATA_PIO0].cycle;
		min_active = 
		    pio_timings[(mode & ATA_MODE_MASK) - ATA_PIO0].active;
	
		cycle_tick = ATA_TIME_TO_TICK(sc->rev,min_cycle);
		act_tick = ATA_TIME_TO_TICK(sc->rev,min_active);

		if (sc->rev == 4) {
			inact_tick = cycle_tick - act_tick;

			/* mask: 0x000003ff */
			sc->pioconf[atadev->unit] =
			    (inact_tick << 5) | act_tick;
		} else {
			if (act_tick < PIO_ACT_MIN)
				act_tick = PIO_ACT_MIN;

			inact_tick = cycle_tick - act_tick - PIO_REC_OFFSET;
			if (inact_tick < PIO_REC_MIN)
				inact_tick = PIO_REC_MIN;

			/* mask: 0x000007ff */
			sc->pioconf[atadev->unit] = 
			    (inact_tick << 5) | act_tick;
		}
	}
}

static int
ata_macio_begin_transaction(struct ata_request *request)
{
	struct ata_device *atadev = device_get_softc(request->dev);
	struct ata_macio_softc *sc = device_get_softc(request->parent);

	bus_write_4(sc->sc_mem, ATA_MACIO_TIMINGREG, 
	    sc->udmaconf[atadev->unit] | sc->wdmaconf[atadev->unit] 
	    | sc->pioconf[atadev->unit]); 

	return ata_begin_transaction(request);
}

