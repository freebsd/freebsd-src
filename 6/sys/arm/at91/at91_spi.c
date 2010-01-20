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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
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
	struct mtx sc_mtx;		/* basically a perimeter lock */
	bus_dma_tag_t dmatag;		/* bus dma tag for mbufs */
	bus_dmamap_t map[4];		/* Maps for the transaction */
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

#define AT91_SPI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_SPI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT91_SPI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "spi", MTX_DEF)
#define AT91_SPI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT91_SPI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT91_SPI_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static devclass_t at91_spi_devclass;

/* bus entry points */

static int at91_spi_probe(device_t dev);
static int at91_spi_attach(device_t dev);
static int at91_spi_detach(device_t dev);

/* helper routines */
static int at91_spi_activate(device_t dev);
static void at91_spi_deactivate(device_t dev);

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

	AT91_SPI_LOCK_INIT(sc);

	/*
	 * Allocate DMA tags and maps
	 */
	err = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, 2058, 1, 2048, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->dmatag);
	if (err != 0)
		goto out;
	for (i = 0; i < 4; i++) {
		err = bus_dmamap_create(sc->dmatag, 0,  &sc->map[i]);
		if (err != 0)
			goto out;
	}

	// reset the SPI
	WR4(sc, SPI_CR, SPI_CR_SWRST);

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
	WR4(sc, PDC_PTCR, PDC_PTCR_RXTEN);
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTEN);
	RD4(sc, SPI_RDR);
	RD4(sc, SPI_SR);

	device_add_child(dev, "spibus", -1);
	bus_generic_attach(dev);
out:;
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
	int rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	return (0);
errout:
	at91_spi_deactivate(dev);
	return (ENOMEM);
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
	int i;
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
	i++;
	if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->tx_data,
	    cmd->tx_data_sz, at91_getaddr, &addr, 0) != 0)
		goto out;
	WR4(sc, PDC_TNPR, addr);
	WR4(sc, PDC_TNCR, cmd->tx_cmd_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREWRITE);
	i++;
	if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->rx_cmd,
	    cmd->tx_cmd_sz, at91_getaddr, &addr, 0) != 0)
		goto out;
	WR4(sc, PDC_RPR, addr);
	WR4(sc, PDC_RCR, cmd->tx_cmd_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREREAD);
	i++;
	if (bus_dmamap_load(sc->dmatag, sc->map[i], cmd->rx_data,
	    cmd->tx_data_sz, at91_getaddr, &addr, 0) != 0)
		goto out;
	WR4(sc, PDC_RNPR, addr);
	WR4(sc, PDC_RNCR, cmd->tx_data_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREREAD);

	WR4(sc, PDC_PTCR, PDC_PTCR_TXTEN | PDC_PTCR_RXTEN);

	// wait for completion
	// XXX should be done as an ISR of some sort.
	while (RD4(sc, SPI_SR) & SPI_SR_ENDRX)
		DELAY(700);

	// Sync the buffers after the DMA is done, and unload them.
	bus_dmamap_sync(sc->dmatag, sc->map[0], BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->dmatag, sc->map[1], BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->dmatag, sc->map[2], BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->dmatag, sc->map[3], BUS_DMASYNC_POSTREAD);
	for (i = 0; i < 4; i++)
		bus_dmamap_unload(sc->dmatag, sc->map[i]);
	return (0);
out:;
	while (i-- > 0)
		bus_dmamap_unload(sc->dmatag, sc->map[i]);
	return (EIO);
}

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
	"at91_spi",
	at91_spi_methods,
	sizeof(struct at91_spi_softc),
};

DRIVER_MODULE(at91_spi, atmelarm, at91_spi_driver, at91_spi_devclass, 0, 0);
