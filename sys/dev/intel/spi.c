/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include <dev/intel/spi.h>

/**
 *	Macros for driver mutex locking
 */
#define	INTELSPI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	INTELSPI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	INTELSPI_LOCK_INIT(_sc)		\
	mtx_init(&_sc->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    "intelspi", MTX_DEF)
#define	INTELSPI_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)
#define	INTELSPI_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define	INTELSPI_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_NOTOWNED)

#define INTELSPI_WRITE(_sc, _off, _val)		\
    bus_write_4((_sc)->sc_mem_res, (_off), (_val))
#define INTELSPI_READ(_sc, _off)			\
    bus_read_4((_sc)->sc_mem_res, (_off))

#define	INTELSPI_BUSY		0x1
#define	TX_FIFO_THRESHOLD	2
#define	RX_FIFO_THRESHOLD	2
#define	CLOCK_DIV_10MHZ		5
#define	DATA_SIZE_8BITS		8
#define	MAX_CLOCK_RATE		50000000

#define	CS_LOW		0
#define	CS_HIGH		1

#define	INTELSPI_SSPREG_SSCR0	 	0x0
#define	 SSCR0_SCR(n)				((((n) - 1) & 0xfff) << 8)
#define	 SSCR0_SSE				(1 << 7)
#define	 SSCR0_FRF_SPI				(0 << 4)
#define	 SSCR0_DSS(n)				(((n) - 1) << 0)
#define	INTELSPI_SSPREG_SSCR1	 	0x4
#define	 SSCR1_TINTE				(1 << 19)
#define	 SSCR1_RFT(n)				(((n) - 1) << 10)
#define	 SSCR1_RFT_MASK				(0xf << 10)
#define	 SSCR1_TFT(n)				(((n) - 1) << 6)
#define	 SSCR1_SPI_SPH				(1 << 4)
#define	 SSCR1_SPI_SPO				(1 << 3)
#define	 SSCR1_MODE_MASK				(SSCR1_SPI_SPO | SSCR1_SPI_SPH)
#define	 SSCR1_TIE				(1 << 1)
#define	 SSCR1_RIE				(1 << 0)
#define	INTELSPI_SSPREG_SSSR	 	0x8
#define	 SSSR_RFL_MASK				(0xf << 12)
#define	 SSSR_TFL_MASK				(0xf << 8)
#define	 SSSR_RNE				(1 << 3)
#define	 SSSR_TNF				(1 << 2)
#define	INTELSPI_SSPREG_SSITR	 	0xC
#define	INTELSPI_SSPREG_SSDR	 	0x10
#define	INTELSPI_SSPREG_SSTO	 	0x28
#define	INTELSPI_SSPREG_SSPSP	 	0x2C
#define	INTELSPI_SSPREG_SSTSA	 	0x30
#define	INTELSPI_SSPREG_SSRSA	 	0x34
#define	INTELSPI_SSPREG_SSTSS	 	0x38
#define	INTELSPI_SSPREG_SSACD	 	0x3C
#define	INTELSPI_SSPREG_ITF	 	0x40
#define	INTELSPI_SSPREG_SITF	 	0x44
#define	INTELSPI_SSPREG_SIRF	 	0x48
#define SPI_CS_CTRL(sc) \
	(intelspi_infos[sc->sc_vers].reg_lpss_base + \
	 intelspi_infos[sc->sc_vers].reg_cs_ctrl)
#define	 SPI_CS_CTRL_CS_MASK			(3)
#define	 SPI_CS_CTRL_SW_MODE			(1 << 0)
#define	 SPI_CS_CTRL_HW_MODE			(1 << 0)
#define	 SPI_CS_CTRL_CS_HIGH			(1 << 1)

static void	intelspi_intr(void *);

static int
intelspi_txfifo_full(struct intelspi_softc *sc)
{
	uint32_t sssr;

	INTELSPI_ASSERT_LOCKED(sc);

	sssr = INTELSPI_READ(sc, INTELSPI_SSPREG_SSSR);
	if (sssr & SSSR_TNF)
		return (0);

	return (1);
}

static int
intelspi_rxfifo_empty(struct intelspi_softc *sc)
{
	uint32_t sssr;

	INTELSPI_ASSERT_LOCKED(sc);

	sssr = INTELSPI_READ(sc, INTELSPI_SSPREG_SSSR);
	if (sssr & SSSR_RNE)
		return (0);
	else
		return (1);
}

static void
intelspi_fill_tx_fifo(struct intelspi_softc *sc)
{
	struct spi_command *cmd;
	uint32_t written;
	uint8_t *data;

	INTELSPI_ASSERT_LOCKED(sc);

	cmd = sc->sc_cmd;
	while (sc->sc_written < sc->sc_len &&
	    !intelspi_txfifo_full(sc)) {
		data = (uint8_t *)cmd->tx_cmd;
		written = sc->sc_written++;

		if (written >= cmd->tx_cmd_sz) {
			data = (uint8_t *)cmd->tx_data;
			written -= cmd->tx_cmd_sz;
		}
		INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSDR, data[written]);
	}
}

static void
intelspi_drain_rx_fifo(struct intelspi_softc *sc)
{
	struct spi_command *cmd;
	uint32_t  read;
	uint8_t *data;

	INTELSPI_ASSERT_LOCKED(sc);

	cmd = sc->sc_cmd;
	while (sc->sc_read < sc->sc_len &&
	    !intelspi_rxfifo_empty(sc)) {
		data = (uint8_t *)cmd->rx_cmd;
		read = sc->sc_read++;
		if (read >= cmd->rx_cmd_sz) {
			data = (uint8_t *)cmd->rx_data;
			read -= cmd->rx_cmd_sz;
		}
		data[read] = INTELSPI_READ(sc, INTELSPI_SSPREG_SSDR) & 0xff;
	}
}

static int
intelspi_transaction_done(struct intelspi_softc *sc)
{
	int txfifo_empty;
	uint32_t sssr;

	INTELSPI_ASSERT_LOCKED(sc);

	if (sc->sc_written != sc->sc_len ||
	    sc->sc_read != sc->sc_len)
		return (0);

	sssr = INTELSPI_READ(sc, INTELSPI_SSPREG_SSSR);
	txfifo_empty = ((sssr & SSSR_TFL_MASK) == 0) &&
		(sssr & SSSR_TNF);

	if (txfifo_empty && !(sssr & SSSR_RNE))
		return (1);

	return (0);
}

static int
intelspi_transact(struct intelspi_softc *sc)
{

	INTELSPI_ASSERT_LOCKED(sc);

	/* TX - Fill up the FIFO. */
	intelspi_fill_tx_fifo(sc);

	/* RX - Drain the FIFO. */
	intelspi_drain_rx_fifo(sc);

	/* Check for end of transfer. */
	return intelspi_transaction_done(sc);
}

static void
intelspi_intr(void *arg)
{
	struct intelspi_softc *sc;
	uint32_t reg;

	sc = (struct intelspi_softc *)arg;

	INTELSPI_LOCK(sc);
	if ((sc->sc_flags & INTELSPI_BUSY) == 0) {
		INTELSPI_UNLOCK(sc);
		return;
	}

	/* Check if SSP if off */
	reg = INTELSPI_READ(sc, INTELSPI_SSPREG_SSSR);
	if (reg == 0xffffffffU) {
		INTELSPI_UNLOCK(sc);
		return;
	}

	/* Check for end of transfer. */
	if (intelspi_transact(sc)) {
		/* Disable interrupts */
		reg = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR1);
		reg &= ~(SSCR1_TIE | SSCR1_RIE | SSCR1_TINTE);
		INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR1, reg);
		wakeup(sc->sc_dev);
	}

	INTELSPI_UNLOCK(sc);
}

static void
intelspi_init(struct intelspi_softc *sc)
{
	uint32_t reg;

	INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR0, 0);

	/* Manual CS control */
	reg = INTELSPI_READ(sc, SPI_CS_CTRL(sc));
	reg &= ~(SPI_CS_CTRL_CS_MASK);
	reg |= (SPI_CS_CTRL_SW_MODE | SPI_CS_CTRL_CS_HIGH);
	INTELSPI_WRITE(sc, SPI_CS_CTRL(sc), reg);

	/* Set TX/RX FIFO IRQ threshold levels */
	reg = SSCR1_TFT(TX_FIFO_THRESHOLD) | SSCR1_RFT(RX_FIFO_THRESHOLD);
	INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR1, reg);

	reg = SSCR0_SCR(CLOCK_DIV_10MHZ);
	/* Put SSP in SPI mode */
	reg |= SSCR0_FRF_SPI;
	/* Data size */
	reg |= SSCR0_DSS(DATA_SIZE_8BITS);
	/* Enable SSP */
	reg |= SSCR0_SSE;
	INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR0, reg);
}

static void
intelspi_set_cs(struct intelspi_softc *sc, int level)
{
	uint32_t reg;

	reg = INTELSPI_READ(sc, SPI_CS_CTRL(sc));
	reg &= ~(SPI_CS_CTRL_CS_MASK);
	reg |= SPI_CS_CTRL_SW_MODE;

	if (level == CS_HIGH)
		reg |= SPI_CS_CTRL_CS_HIGH;

	INTELSPI_WRITE(sc, SPI_CS_CTRL(sc), reg);
}

int
intelspi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct intelspi_softc *sc;
	int err, poll_limit;
	uint32_t sscr0, sscr1, mode, clock, cs_delay;
	bool restart = false;

	sc = device_get_softc(dev);
	err = 0;

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz, 
	    ("TX/RX command sizes should be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz, 
	    ("TX/RX data sizes should be equal"));

	INTELSPI_LOCK(sc);

	/* If the controller is in use wait until it is available. */
	while (sc->sc_flags & INTELSPI_BUSY) {
		if ((cmd->flags & SPI_FLAG_NO_SLEEP) == SPI_FLAG_NO_SLEEP) {
			INTELSPI_UNLOCK(sc);
			return (EBUSY);
		}
		err = mtx_sleep(dev, &sc->sc_mtx, 0, "intelspi", 0);
		if (err == EINTR) {
			INTELSPI_UNLOCK(sc);
			return (err);
		}
	}

	/* Now we have control over SPI controller. */
	sc->sc_flags = INTELSPI_BUSY;

	/* Configure the clock rate and SPI mode. */
	spibus_get_clock(child, &clock);
	spibus_get_mode(child, &mode);

	if (clock != sc->sc_clock || mode != sc->sc_mode) {
		sscr0 = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR0);
		sscr0 &= ~SSCR0_SSE;
		INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR0, sscr0);
		restart = true;
	}

	if (clock != sc->sc_clock) {
		sscr0 = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR0);
		sscr0 &= ~SSCR0_SCR(0xfff);
		if (clock == 0)
			sscr0 |= SSCR0_SCR(CLOCK_DIV_10MHZ);
		else
			sscr0 |= SSCR0_SCR(howmany(MAX_CLOCK_RATE, min(MAX_CLOCK_RATE, clock)));
		INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR0, sscr0);
		sc->sc_clock = clock;
	}

	if (mode != sc->sc_mode) {
		sscr1 = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR1);
		sscr1 &= ~SSCR1_MODE_MASK;
		if (mode & SPIBUS_MODE_CPHA)
			sscr1 |= SSCR1_SPI_SPH;
		if (mode & SPIBUS_MODE_CPOL)
			sscr1 |= SSCR1_SPI_SPO;
		INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR1, sscr1);
		sc->sc_mode = mode;
	}

	if (restart) {
		sscr0 = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR0);
		sscr0 |= SSCR0_SSE;
		INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR0, sscr0);
	}

	/* Save a pointer to the SPI command. */
	sc->sc_cmd = cmd;
	sc->sc_read = 0;
	sc->sc_written = 0;
	sc->sc_len = cmd->tx_cmd_sz + cmd->tx_data_sz;

	/* Enable CS */
	intelspi_set_cs(sc, CS_LOW);

	/* Wait the CS delay */
	spibus_get_cs_delay(child, &cs_delay);
	DELAY(cs_delay);

	/* Transfer as much as possible to FIFOs */
	if ((cmd->flags & SPI_FLAG_NO_SLEEP) == SPI_FLAG_NO_SLEEP) {
		/* We cannot wait with mtx_sleep if we're called from e.g. an ithread */
		poll_limit = 2000;
		while (!intelspi_transact(sc) && poll_limit-- > 0)
			DELAY(1000);
		if (poll_limit == 0) {
			device_printf(dev, "polling was stuck, transaction not finished\n");
			err = EIO;
		}
	} else {
		if (!intelspi_transact(sc)) {
			/* If FIFO is not large enough - enable interrupts */
			sscr1 = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR1);
			sscr1 |= (SSCR1_TIE | SSCR1_RIE | SSCR1_TINTE);
			INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR1, sscr1);

			/* and wait for transaction to complete */
			err = mtx_sleep(dev, &sc->sc_mtx, 0, "intelspi", hz * 2);
		}
	}

	/* De-assert CS */
	if ((cmd->flags & SPI_FLAG_KEEP_CS) == 0)
		intelspi_set_cs(sc, CS_HIGH);

	/* Clear transaction details */
	sc->sc_cmd = NULL;
	sc->sc_read = 0;
	sc->sc_written = 0;
	sc->sc_len = 0;

	/* Make sure the SPI engine and interrupts are disabled. */
	sscr1 = INTELSPI_READ(sc, INTELSPI_SSPREG_SSCR1);
	sscr1 &= ~(SSCR1_TIE | SSCR1_RIE | SSCR1_TINTE);
	INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR1, sscr1);

	/* Release the controller and wakeup the next thread waiting for it. */
	sc->sc_flags = 0;
	wakeup_one(dev);
	INTELSPI_UNLOCK(sc);

	/*
	 * Check for transfer timeout.  The SPI controller doesn't
	 * return errors.
	 */
	if (err == EWOULDBLOCK) {
		device_printf(sc->sc_dev, "transfer timeout\n");
		err = EIO;
	}

	return (err);
}

int
intelspi_attach(device_t dev)
{
	struct intelspi_softc	*sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	INTELSPI_LOCK_INIT(sc);

	sc->sc_mem_res = bus_alloc_resource_any(sc->sc_dev,
	    SYS_RES_MEMORY, &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "can't allocate memory resource\n");
		goto error;
	}

	sc->sc_irq_res = bus_alloc_resource_any(sc->sc_dev,
	    SYS_RES_IRQ, &sc->sc_irq_rid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "can't allocate IRQ resource\n");
		goto error;
	}

	/* Hook up our interrupt handler. */
	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, intelspi_intr, sc, &sc->sc_irq_ih)) {
		device_printf(dev, "cannot setup the interrupt handler\n");
		goto error;
	}

	intelspi_init(sc);

	device_add_child(dev, "spibus", -1);

	return (bus_delayed_attach_children(dev));

error:
	INTELSPI_LOCK_DESTROY(sc);

	if (sc->sc_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_mem_rid, sc->sc_mem_res);

	if (sc->sc_irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_irq_rid, sc->sc_irq_res);

	return (ENXIO);
}

int
intelspi_detach(device_t dev)
{
	struct intelspi_softc	*sc;

	sc = device_get_softc(dev);

	INTELSPI_LOCK_DESTROY(sc);

	if (sc->sc_irq_ih)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_ih);

	if (sc->sc_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_mem_rid, sc->sc_mem_res);

	if (sc->sc_irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_irq_rid, sc->sc_irq_res);

	return (bus_generic_detach(dev));
}

int
intelspi_suspend(device_t dev)
{
	struct intelspi_softc        *sc;
	int err, i;

	sc = device_get_softc(dev);

	err = bus_generic_suspend(dev);
	if (err)
		return (err);

	for (i = 0; i < 9; i++) {
		unsigned long offset = i * sizeof(uint32_t);
		sc->sc_regs[i] = INTELSPI_READ(sc,
		    intelspi_infos[sc->sc_vers].reg_lpss_base + offset);
	}

	/* Shutdown just in case */
	INTELSPI_WRITE(sc, INTELSPI_SSPREG_SSCR0, 0);

	return (0);
}

int
intelspi_resume(device_t dev)
{
	struct intelspi_softc   *sc;
	int i;

	sc = device_get_softc(dev);

	for (i = 0; i < 9; i++) {
		unsigned long offset = i * sizeof(uint32_t);
		INTELSPI_WRITE(sc,
		    intelspi_infos[sc->sc_vers].reg_lpss_base + offset,
		    sc->sc_regs[i]);
	}

	intelspi_init(sc);

	/* Ensure the next transfer would reconfigure these */
	sc->sc_clock = 0;
	sc->sc_mode = 0;

	return (bus_generic_resume(dev));
}
