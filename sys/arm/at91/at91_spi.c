/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <arm/at91/at91_spireg.h>
#include <arm/at91/at91_pdcreg.h>

#include <dev/spibus/spi.h>
#include "spibus_if.h"

struct at91_spi_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	bus_dma_tag_t dmatag;		/* bus dma tag for mbufs */
	bus_dmamap_t map[4];		/* Maps for the transaction */
	int rxdone;
};

static inline uint32_t
RD4(struct at91_spi_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_spi_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

/* bus entry points */
static int at91_spi_probe(device_t dev);
static int at91_spi_attach(device_t dev);
static int at91_spi_detach(device_t dev);

/* helper routines */
static int at91_spi_activate(device_t dev);
static void at91_spi_deactivate(device_t dev);
static void at91_spi_intr(void *arg);

static int
at91_spi_probe(device_t dev)
{
	device_set_desc(dev, "SPI");
	return (0);
}

static int
at91_spi_attach(device_t dev)
{
	struct at91_spi_softc *sc = device_get_softc(dev);
	int err, i;

	sc->dev = dev;
	err = at91_spi_activate(dev);
	if (err)
		goto out;

	/*
	 * Allocate DMA tags and maps
	 */
	err = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, 2058, 1,
	    2048, BUS_DMA_ALLOCNOW, NULL, NULL, &sc->dmatag);
	if (err != 0)
		goto out;
	for (i = 0; i < 4; i++) {
		err = bus_dmamap_create(sc->dmatag, 0,  &sc->map[i]);
		if (err != 0)
			goto out;
	}

	// reset the SPI
	WR4(sc, SPI_CR, SPI_CR_SWRST);
	WR4(sc, SPI_IDR, 0xffffffff);

	WR4(sc, SPI_MR, (0xf << 24) | SPI_MR_MSTR | SPI_MR_MODFDIS |
	    (0xE << 16));

	WR4(sc, SPI_CSR0, SPI_CSR_CPOL | (4 << 16) | (2 << 8));
	WR4(sc, SPI_CR, SPI_CR_SPIEN);

	WR4(sc, PDC_PTCR, PDC_PTCR_TXTDIS);
	WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS);
	WR4(sc, PDC_RNPR, 0);
	WR4(sc, PDC_RNCR, 0);
	WR4(sc, PDC_TNPR, 0);
	WR4(sc, PDC_TNCR, 0);
	WR4(sc, PDC_RPR, 0);
	WR4(sc, PDC_RCR, 0);
	WR4(sc, PDC_TPR, 0);
	WR4(sc, PDC_TCR, 0);
	RD4(sc, SPI_RDR);
	RD4(sc, SPI_SR);

	device_add_child(dev, "spibus", -1);
	bus_generic_attach(dev);
out:
	if (err)
		at91_spi_deactivate(dev);
	return (err);
}

static int
at91_spi_detach(device_t dev)
{
	return (EBUSY);	/* XXX */
}

static int
at91_spi_activate(device_t dev)
{
	struct at91_spi_softc *sc;
	int rid, err = ENOMEM;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL)
		goto errout;
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, at91_spi_intr, sc, &sc->intrhand);
	if (err != 0)
		goto errout;
	return (0);
errout:
	at91_spi_deactivate(dev);
	return (err);
}

static void
at91_spi_deactivate(device_t dev)
{
	struct at91_spi_softc *sc;

	sc = device_get_softc(dev);
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = 0;
	return;
}

static void
at91_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
at91_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct at91_spi_softc *sc;
	int i, j, rxdone, err, mode[4];
	bus_addr_t addr;

	sc = device_get_softc(dev);
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTDIS | PDC_PTCR_RXTDIS);
	i = 0;
	if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->tx_cmd,
	    cmd->tx_cmd_sz, at91_getaddr, &addr, 0) != 0)
		goto out;
	WR4(sc, PDC_TPR, addr);
	WR4(sc, PDC_TCR, cmd->tx_cmd_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREWRITE);
	mode[i++] = BUS_DMASYNC_POSTWRITE;
	if (cmd->tx_data_sz > 0) {
		if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->tx_data,
			cmd->tx_data_sz, at91_getaddr, &addr, 0) != 0)
			goto out;
		WR4(sc, PDC_TNPR, addr);
		WR4(sc, PDC_TNCR, cmd->tx_data_sz);
		bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREWRITE);
		mode[i++] = BUS_DMASYNC_POSTWRITE;
	}
	if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->rx_cmd,
	    cmd->tx_cmd_sz, at91_getaddr, &addr, 0) != 0)
		goto out;
	WR4(sc, PDC_RPR, addr);
	WR4(sc, PDC_RCR, cmd->tx_cmd_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREREAD);
	mode[i++] = BUS_DMASYNC_POSTREAD;
	if (cmd->rx_data_sz > 0) {
		if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->rx_data,
			cmd->tx_data_sz, at91_getaddr, &addr, 0) != 0)
			goto out;
		WR4(sc, PDC_RNPR, addr);
		WR4(sc, PDC_RNCR, cmd->rx_data_sz);
		bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREREAD);
		mode[i++] = BUS_DMASYNC_POSTREAD;
	}
	WR4(sc, SPI_IER, SPI_SR_ENDRX);
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTEN | PDC_PTCR_RXTEN);

	rxdone = sc->rxdone;
	do {
		err = tsleep(&sc->rxdone, PCATCH | PZERO, "spi", hz);
	} while (rxdone == sc->rxdone && err != EINTR);
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTDIS | PDC_PTCR_RXTDIS);
	if (err == 0) {
		for (j = 0; j < i; j++) 
			bus_dmamap_sync(sc->dmatag, sc->map[j], mode[j]);
	}
	for (j = 0; j < i; j++)
		bus_dmamap_unload(sc->dmatag, sc->map[j]);
	return (err);
out:
	for (j = 0; j < i; j++)
		bus_dmamap_unload(sc->dmatag, sc->map[j]);
	return (EIO);
}

static void
at91_spi_intr(void *arg)
{
	struct at91_spi_softc *sc = (struct at91_spi_softc*)arg;
	uint32_t sr;

	sr = RD4(sc, SPI_SR) & RD4(sc, SPI_IMR);
	if (sr & SPI_SR_ENDRX) {
		sc->rxdone++;
		WR4(sc, SPI_IDR, SPI_SR_ENDRX);
		wakeup(&sc->rxdone);
	}
	if (sr & ~SPI_SR_ENDRX) {
		device_printf(sc->dev, "Unexpected ISR %#x\n", sr);
		WR4(sc, SPI_IDR, sr & ~SPI_SR_ENDRX);
	}
}

static devclass_t at91_spi_devclass;

static device_method_t at91_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_spi_probe),
	DEVMETHOD(device_attach,	at91_spi_attach),
	DEVMETHOD(device_detach,	at91_spi_detach),

	/* spibus interface */
	DEVMETHOD(spibus_transfer,	at91_spi_transfer),
	{ 0, 0 }
};

static driver_t at91_spi_driver = {
	"spi",
	at91_spi_methods,
	sizeof(struct at91_spi_softc),
};

DRIVER_MODULE(at91_spi, atmelarm, at91_spi_driver, at91_spi_devclass, 0, 0);
