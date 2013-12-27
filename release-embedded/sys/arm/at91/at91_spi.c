/*-
 * Copyright (c) 2006 M. Warner Losh.
 * Copyright (c) 2011-2012 Ian Lepore.
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
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sx.h>

#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_spireg.h>
#include <arm/at91/at91_pdcreg.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "spibus_if.h"

struct at91_spi_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	bus_dma_tag_t dmatag;		/* bus dma tag for transfers */
	bus_dmamap_t map[4];		/* Maps for the transaction */
	struct sx xfer_mtx;		/* Enforce one transfer at a time */
	uint32_t xfer_done;		/* interrupt<->mainthread signaling */
};

#define CS_TO_MR(cs)	((~(1 << (cs)) & 0x0f) << 16)

static inline uint32_t
RD4(struct at91_spi_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct at91_spi_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

/* bus entry points */
static int at91_spi_attach(device_t dev);
static int at91_spi_detach(device_t dev);
static int at91_spi_probe(device_t dev);
static int at91_spi_transfer(device_t dev, device_t child,
    struct spi_command *cmd);

/* helper routines */
static void at91_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs,
    int error);
static int at91_spi_activate(device_t dev);
static void at91_spi_deactivate(device_t dev);
static void at91_spi_intr(void *arg);

static int
at91_spi_probe(device_t dev)
{

	device_set_desc(dev, "AT91 SPI");
	return (0);
}

static int
at91_spi_attach(device_t dev)
{
	struct at91_spi_softc *sc;
	int err;
	uint32_t csr;

	sc = device_get_softc(dev);

	sc->dev = dev;
	sx_init(&sc->xfer_mtx, device_get_nameunit(dev));

	/*
	 * Allocate resources.
	 */
	err = at91_spi_activate(dev);
	if (err)
		goto out;

	/*
	 * Set up the hardware.
	 */

	WR4(sc, SPI_CR, SPI_CR_SWRST);
	/* "Software Reset must be Written Twice" erratum */
	WR4(sc, SPI_CR, SPI_CR_SWRST);
	WR4(sc, SPI_IDR, 0xffffffff);

	WR4(sc, SPI_MR, (0xf << 24) | SPI_MR_MSTR | SPI_MR_MODFDIS |
	    CS_TO_MR(0));

	/*
	 * For now, run the bus at the slowest speed possible as otherwise we
	 * may encounter data corruption on transmit as seen with ETHERNUT5
	 * and AT45DB321D even though both board and slave device can take
	 * more.
	 * This also serves as a work-around for the "NPCSx rises if no data
	 * data is to be transmitted" erratum.  The ideal workaround for the
	 * latter is to take the chip select control away from the peripheral
	 * and manage it directly as a GPIO line.  The easy solution is to
	 * slow down the bus so dramatically that it just never gets starved
	 * as may be seen when the OCHI controller is running and consuming
	 * memory and APB bandwidth.
	 * Also, currently we lack a way for lettting both the board and the
	 * slave devices take their maximum supported SPI clocks into account.
	 * Also, we hard-wire SPI mode to 3.
	 */
	csr = SPI_CSR_CPOL | (4 << 16) | (0xff << 8);
	WR4(sc, SPI_CSR0, csr);
	WR4(sc, SPI_CSR1, csr);
	WR4(sc, SPI_CSR2, csr);
	WR4(sc, SPI_CSR3, csr);

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
	int err, i, rid;

	sc = device_get_softc(dev);
	err = ENOMEM;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto out;

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL)
		goto out;
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, at91_spi_intr, sc, &sc->intrhand);
	if (err != 0)
		goto out;

	err = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, 2048, 1,
	    2048, BUS_DMA_ALLOCNOW, NULL, NULL, &sc->dmatag);
	if (err != 0)
		goto out;

	for (i = 0; i < 4; i++) {
		err = bus_dmamap_create(sc->dmatag, 0,  &sc->map[i]);
		if (err != 0)
			goto out;
	}
out:
	if (err != 0)
		at91_spi_deactivate(dev);
	return (err);
}

static void
at91_spi_deactivate(device_t dev)
{
	struct at91_spi_softc *sc;
	int i;

	sc = device_get_softc(dev);
	bus_generic_detach(dev);

	for (i = 0; i < 4; i++)
		if (sc->map[i])
			bus_dmamap_destroy(sc->dmatag, sc->map[i]);

	if (sc->dmatag)
		bus_dma_tag_destroy(sc->dmatag);

	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = NULL;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = NULL;

	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = NULL;
}

static void
at91_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs __unused,
    int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
at91_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct at91_spi_softc *sc;
	bus_addr_t addr;
	int err, i, j, mode[4], cs;

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz,
	    ("%s: TX/RX command sizes should be equal", __func__));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz,
	    ("%s: TX/RX data sizes should be equal", __func__));

	/* get the proper chip select */
	spibus_get_cs(child, &cs);

	sc = device_get_softc(dev);
	i = 0;

	sx_xlock(&sc->xfer_mtx);

	/*
	 * Disable transfers while we set things up.
	 */
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTDIS | PDC_PTCR_RXTDIS);

	/*
	 * PSCDEC = 0 has a range of 0..3 for chip select.  We
	 * don't support PSCDEC = 1 which has a range of 0..15.
	 */
	if (cs < 0 || cs > 3) {
		device_printf(dev,
		    "Invalid chip select %d requested by %s\n", cs,
		    device_get_nameunit(child));
		err = EINVAL;
		goto out;
	}

#ifdef SPI_CHIP_SELECT_HIGH_SUPPORT
	/*
	 * The AT91RM9200 couldn't do CS high for CS 0.  Other chips can, but we
	 * don't support that yet, or other spi modes.
	 */
	if (at91_is_rm92() && cs == 0 &&
	    (cmd->flags & SPI_CHIP_SELECT_HIGH) != 0) {
		device_printf(dev,
		    "Invalid chip select high requested by %s for cs 0.\n",
		    device_get_nameunit(child));
		err = EINVAL;
		goto out;
	}
#endif
	err = (RD4(sc, SPI_MR) & ~0x000f0000) | CS_TO_MR(cs);
	WR4(sc, SPI_MR, err);

	/*
	 * Set up the TX side of the transfer.
	 */
	if ((err = bus_dmamap_load(sc->dmatag, sc->map[i], cmd->tx_cmd,
	    cmd->tx_cmd_sz, at91_getaddr, &addr, 0)) != 0)
		goto out;
	WR4(sc, PDC_TPR, addr);
	WR4(sc, PDC_TCR, cmd->tx_cmd_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREWRITE);
	mode[i++] = BUS_DMASYNC_POSTWRITE;
	if (cmd->tx_data_sz > 0) {
		if ((err = bus_dmamap_load(sc->dmatag, sc->map[i],
		    cmd->tx_data, cmd->tx_data_sz, at91_getaddr, &addr, 0)) !=
		    0)
			goto out;
		WR4(sc, PDC_TNPR, addr);
		WR4(sc, PDC_TNCR, cmd->tx_data_sz);
		bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREWRITE);
		mode[i++] = BUS_DMASYNC_POSTWRITE;
	}

	/*
	 * Set up the RX side of the transfer.
	 */
	if ((err = bus_dmamap_load(sc->dmatag, sc->map[i], cmd->rx_cmd,
	    cmd->rx_cmd_sz, at91_getaddr, &addr, 0)) != 0)
		goto out;
	WR4(sc, PDC_RPR, addr);
	WR4(sc, PDC_RCR, cmd->rx_cmd_sz);
	bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREREAD);
	mode[i++] = BUS_DMASYNC_POSTREAD;
	if (cmd->rx_data_sz > 0) {
		if ((err = bus_dmamap_load(sc->dmatag, sc->map[i],
		    cmd->rx_data, cmd->rx_data_sz, at91_getaddr, &addr, 0)) !=
		    0)
			goto out;
		WR4(sc, PDC_RNPR, addr);
		WR4(sc, PDC_RNCR, cmd->rx_data_sz);
		bus_dmamap_sync(sc->dmatag, sc->map[i], BUS_DMASYNC_PREREAD);
		mode[i++] = BUS_DMASYNC_POSTREAD;
	}

	/*
	 * Start the transfer, wait for it to complete.
	 */
	sc->xfer_done = 0;
	WR4(sc, SPI_IER, SPI_SR_RXBUFF);
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTEN | PDC_PTCR_RXTEN);
	do
		err = tsleep(&sc->xfer_done, PCATCH | PZERO, "at91_spi", hz);
	while (sc->xfer_done == 0 && err != EINTR);

	/*
	 * Stop the transfer and clean things up.
	 */
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTDIS | PDC_PTCR_RXTDIS);
	if (err == 0)
		for (j = 0; j < i; j++)
			bus_dmamap_sync(sc->dmatag, sc->map[j], mode[j]);
out:
	for (j = 0; j < i; j++)
		bus_dmamap_unload(sc->dmatag, sc->map[j]);

	sx_xunlock(&sc->xfer_mtx);

	return (err);
}

static void
at91_spi_intr(void *arg)
{
	struct at91_spi_softc *sc;
	uint32_t sr;

	sc = (struct at91_spi_softc*)arg;

	sr = RD4(sc, SPI_SR) & RD4(sc, SPI_IMR);
	if ((sr & SPI_SR_RXBUFF) != 0) {
		sc->xfer_done = 1;
		WR4(sc, SPI_IDR, SPI_SR_RXBUFF);
		wakeup(&sc->xfer_done);
	}
	if ((sr & ~SPI_SR_RXBUFF) != 0) {
		device_printf(sc->dev, "Unexpected ISR %#x\n", sr);
		WR4(sc, SPI_IDR, sr & ~SPI_SR_RXBUFF);
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

	DEVMETHOD_END
};

static driver_t at91_spi_driver = {
	"spi",
	at91_spi_methods,
	sizeof(struct at91_spi_softc),
};

DRIVER_MODULE(at91_spi, atmelarm, at91_spi_driver, at91_spi_devclass, NULL,
    NULL);
