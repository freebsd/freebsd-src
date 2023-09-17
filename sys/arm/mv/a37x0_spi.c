/*-
 * Copyright (c) 2018, 2019 Rubicon Communications, LLC (Netgate)
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
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "spibus_if.h"

struct a37x0_spi_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res;
	struct resource		*sc_irq_res;
	struct spi_command	*sc_cmd;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	uint32_t		sc_len;
	uint32_t		sc_maxfreq;
	uint32_t		sc_read;
	uint32_t		sc_flags;
	uint32_t		sc_written;
	void			*sc_intrhand;
};

#define	A37X0_SPI_WRITE(_sc, _off, _val)		\
    bus_space_write_4((_sc)->sc_bst, (_sc)->sc_bsh, (_off), (_val))
#define	A37X0_SPI_READ(_sc, _off)			\
    bus_space_read_4((_sc)->sc_bst, (_sc)->sc_bsh, (_off))
#define	A37X0_SPI_LOCK(_sc)	mtx_lock(&(_sc)->sc_mtx)
#define	A37X0_SPI_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)

#define	A37X0_SPI_BUSY			(1 << 0)
/*
 * While the A3700 utils from Marvell usually sets the QSF clock to 200MHz,
 * there is no guarantee that it is correct without the proper clock framework
 * to retrieve the actual TBG and PLL settings.
 */
#define	A37X0_SPI_CLOCK			200000000	/* QSF Clock 200MHz */

#define	A37X0_SPI_CONTROL		0x0
#define	 A37X0_SPI_CS_SHIFT		16
#define	 A37X0_SPI_CS_MASK		(0xf << A37X0_SPI_CS_SHIFT)
#define	A37X0_SPI_CONF			0x4
#define	 A37X0_SPI_WFIFO_THRS_SHIFT	28
#define	 A37X0_SPI_RFIFO_THRS_SHIFT	24
#define	 A37X0_SPI_AUTO_CS_EN		(1 << 20)
#define	 A37X0_SPI_DMA_WR_EN		(1 << 19)
#define	 A37X0_SPI_DMA_RD_EN		(1 << 18)
#define	 A37X0_SPI_FIFO_MODE		(1 << 17)
#define	 A37X0_SPI_SRST			(1 << 16)
#define	 A37X0_SPI_XFER_START		(1 << 15)
#define	 A37X0_SPI_XFER_STOP		(1 << 14)
#define	 A37X0_SPI_INSTR_PIN		(1 << 13)
#define	 A37X0_SPI_ADDR_PIN		(1 << 12)
#define	 A37X0_SPI_DATA_PIN_MASK	0x3
#define	 A37X0_SPI_DATA_PIN_SHIFT	10
#define	 A37X0_SPI_FIFO_FLUSH		(1 << 9)
#define	 A37X0_SPI_RW_EN		(1 << 8)
#define	 A37X0_SPI_CLK_POL		(1 << 7)
#define	 A37X0_SPI_CLK_PHASE		(1 << 6)
#define	 A37X0_SPI_BYTE_LEN		(1 << 5)
#define	 A37X0_SPI_PSC_MASK		0x1f
#define	A37X0_SPI_DATA_OUT		0x8
#define	A37X0_SPI_DATA_IN		0xc
#define	A37X0_SPI_INTR_STAT		0x28
#define	A37X0_SPI_INTR_MASK		0x2c
#define	 A37X0_SPI_RDY			(1 << 1)
#define	 A37X0_SPI_XFER_DONE		(1 << 0)

static struct ofw_compat_data compat_data[] = {
	{ "marvell,armada-3700-spi",	1 },
	{ NULL, 0 }
};

static void a37x0_spi_intr(void *);

static int
a37x0_spi_wait(struct a37x0_spi_softc *sc, int timeout, uint32_t reg,
    uint32_t mask)
{
	int i;

	for (i = 0; i < timeout; i++) {
		if ((A37X0_SPI_READ(sc, reg) & mask) == 0)
			return (0);
		DELAY(100);
	}

	return (ETIMEDOUT);
}

static int
a37x0_spi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "Armada 37x0 SPI controller");

	return (BUS_PROBE_DEFAULT);
}

static int
a37x0_spi_attach(device_t dev)
{
	int err, rid;
	pcell_t maxfreq;
	struct a37x0_spi_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->sc_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	/* Make sure that no CS is asserted. */
	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONTROL);
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONTROL, reg & ~A37X0_SPI_CS_MASK);

	/* Reset FIFO. */
	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONF);
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONF, reg | A37X0_SPI_FIFO_FLUSH);
	err = a37x0_spi_wait(sc, 20, A37X0_SPI_CONF, A37X0_SPI_FIFO_FLUSH);
	if (err != 0) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot flush the controller fifo.\n");
		return (ENXIO);
	}

	/* Reset the Controller. */
	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONF);
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONF, reg | A37X0_SPI_SRST);
	DELAY(1000);
	/* Enable the single byte IO, disable FIFO. */
	reg &= ~(A37X0_SPI_FIFO_MODE | A37X0_SPI_BYTE_LEN);
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONF, reg);

	/* Disable and clear interrupts. */
	A37X0_SPI_WRITE(sc, A37X0_SPI_INTR_MASK, 0);
	reg = A37X0_SPI_READ(sc, A37X0_SPI_INTR_STAT);
	A37X0_SPI_WRITE(sc, A37X0_SPI_INTR_STAT, reg);

	/* Hook up our interrupt handler. */
	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, a37x0_spi_intr, sc, &sc->sc_intrhand)) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot setup the interrupt handler\n");
		return (ENXIO);
	}

	mtx_init(&sc->sc_mtx, "a37x0_spi", NULL, MTX_DEF);

	/* Read the controller max-frequency. */
	if (OF_getencprop(ofw_bus_get_node(dev), "spi-max-frequency", &maxfreq,
	    sizeof(maxfreq)) == -1)
		maxfreq = 0;
	sc->sc_maxfreq = maxfreq;

	device_add_child(dev, "spibus", -1);

	/* Probe and attach the spibus when interrupts are available. */
	return (bus_delayed_attach_children(dev));
}

static int
a37x0_spi_detach(device_t dev)
{
	int err;
	struct a37x0_spi_softc *sc;

	if ((err = device_delete_children(dev)) != 0)
		return (err);
	sc = device_get_softc(dev);
	mtx_destroy(&sc->sc_mtx);
	if (sc->sc_intrhand)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);

	return (0);
}

static __inline void
a37x0_spi_rx_byte(struct a37x0_spi_softc *sc)
{
	struct spi_command *cmd;
	uint32_t read;
	uint8_t *p;

	if (sc->sc_read == sc->sc_len)
		return;
	cmd = sc->sc_cmd;
	p = (uint8_t *)cmd->rx_cmd;
	read = sc->sc_read++;
	if (read >= cmd->rx_cmd_sz) {
		p = (uint8_t *)cmd->rx_data;
		read -= cmd->rx_cmd_sz;
	}
	p[read] = A37X0_SPI_READ(sc, A37X0_SPI_DATA_IN) & 0xff;
}

static __inline void
a37x0_spi_tx_byte(struct a37x0_spi_softc *sc)
{
	struct spi_command *cmd;
	uint32_t written;
	uint8_t *p;

	if (sc->sc_written == sc->sc_len)
		return;
	cmd = sc->sc_cmd;
	p = (uint8_t *)cmd->tx_cmd;
	written = sc->sc_written++;
	if (written >= cmd->tx_cmd_sz) {
		p = (uint8_t *)cmd->tx_data;
		written -= cmd->tx_cmd_sz;
	}
	A37X0_SPI_WRITE(sc, A37X0_SPI_DATA_OUT, p[written]);
}

static __inline void
a37x0_spi_set_clock(struct a37x0_spi_softc *sc, uint32_t clock)
{
	uint32_t psc, reg;

	if (sc->sc_maxfreq > 0 && clock > sc->sc_maxfreq)
		clock = sc->sc_maxfreq;
	psc = A37X0_SPI_CLOCK / clock;
	if ((A37X0_SPI_CLOCK % clock) > 0)
		psc++;
	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONF);
	reg &= ~A37X0_SPI_PSC_MASK;
	reg |= psc & A37X0_SPI_PSC_MASK;
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONF, reg);
}

static __inline void
a37x0_spi_set_pins(struct a37x0_spi_softc *sc, uint32_t npins)
{
	uint32_t reg;

	/* Sets single, dual or quad SPI mode. */
	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONF);
	reg &= ~(A37X0_SPI_DATA_PIN_MASK << A37X0_SPI_DATA_PIN_SHIFT);
	reg |= (npins / 2) << A37X0_SPI_DATA_PIN_SHIFT;
	reg |= A37X0_SPI_INSTR_PIN | A37X0_SPI_ADDR_PIN;
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONF, reg);
}

static __inline void
a37x0_spi_set_mode(struct a37x0_spi_softc *sc, uint32_t mode)
{
	uint32_t reg;

	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONF);
	switch (mode) {
	case 0:
		reg &= ~(A37X0_SPI_CLK_PHASE | A37X0_SPI_CLK_POL);
		break;
	case 1:
		reg &= ~A37X0_SPI_CLK_POL;
		reg |= A37X0_SPI_CLK_PHASE;
		break;
	case 2:
		reg &= ~A37X0_SPI_CLK_PHASE;
		reg |= A37X0_SPI_CLK_POL;
		break;
	case 3:
		reg |= (A37X0_SPI_CLK_PHASE | A37X0_SPI_CLK_POL);
		break;
	}
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONF, reg);
}

static void
a37x0_spi_intr(void *arg)
{
	struct a37x0_spi_softc *sc;
	uint32_t status;

	sc = (struct a37x0_spi_softc *)arg;
	A37X0_SPI_LOCK(sc);

	/* Filter stray interrupts. */
	if ((sc->sc_flags & A37X0_SPI_BUSY) == 0) {
		A37X0_SPI_UNLOCK(sc);
		return;
	}

	status = A37X0_SPI_READ(sc, A37X0_SPI_INTR_STAT);
	if (status & A37X0_SPI_XFER_DONE)
		a37x0_spi_rx_byte(sc);

	/* Clear the interrupt status. */
	A37X0_SPI_WRITE(sc, A37X0_SPI_INTR_STAT, status);

	/* Check for end of transfer. */
	if (sc->sc_written == sc->sc_len && sc->sc_read == sc->sc_len)
		wakeup(sc->sc_dev);
	else
		a37x0_spi_tx_byte(sc);

	A37X0_SPI_UNLOCK(sc);
}

static int
a37x0_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	int timeout;
	struct a37x0_spi_softc *sc;
	uint32_t clock, cs, mode, reg;

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz,
	    ("TX/RX command sizes should be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz,
	    ("TX/RX data sizes should be equal"));

	/* Get the proper data for this child. */
	spibus_get_cs(child, &cs);
	cs &= ~SPIBUS_CS_HIGH;
	if (cs > 3) {
		device_printf(dev,
		    "Invalid CS %d requested by %s\n", cs,
		    device_get_nameunit(child));
		return (EINVAL);
	}
	spibus_get_clock(child, &clock);
	if (clock == 0) {
		device_printf(dev,
		    "Invalid clock %uHz requested by %s\n", clock,
		    device_get_nameunit(child));
		return (EINVAL);
	}
	spibus_get_mode(child, &mode);
	if (mode > 3) {
		device_printf(dev,
		    "Invalid mode %u requested by %s\n", mode,
		    device_get_nameunit(child));
		return (EINVAL);
	}

	sc = device_get_softc(dev);
	A37X0_SPI_LOCK(sc);

	/* Wait until the controller is free. */
	while (sc->sc_flags & A37X0_SPI_BUSY)
		mtx_sleep(dev, &sc->sc_mtx, 0, "a37x0_spi", 0);

	/* Now we have control over SPI controller. */
	sc->sc_flags = A37X0_SPI_BUSY;

	/* Set transfer mode and clock. */
	a37x0_spi_set_mode(sc, mode);
	a37x0_spi_set_pins(sc, 1);
	a37x0_spi_set_clock(sc, clock);

	/* Set CS. */
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONTROL, 1 << (A37X0_SPI_CS_SHIFT + cs));

	/* Save a pointer to the SPI command. */
	sc->sc_cmd = cmd;
	sc->sc_read = 0;
	sc->sc_written = 0;
	sc->sc_len = cmd->tx_cmd_sz + cmd->tx_data_sz;

	/* Clear interrupts. */
	reg = A37X0_SPI_READ(sc, A37X0_SPI_INTR_STAT);
	A37X0_SPI_WRITE(sc, A37X0_SPI_INTR_STAT, reg);

	while ((sc->sc_len - sc->sc_written) > 0) {
		/*
		 * Write to start the transmission and read the byte
		 * back when ready.
		 */
		a37x0_spi_tx_byte(sc);
		timeout = 1000;
		while (--timeout > 0) {
			reg = A37X0_SPI_READ(sc, A37X0_SPI_CONTROL);
			if (reg & A37X0_SPI_XFER_DONE)
				break;
			DELAY(1);
		}
		if (timeout == 0)
			break;
		a37x0_spi_rx_byte(sc);
	}

	/* Stop the controller. */
	reg = A37X0_SPI_READ(sc, A37X0_SPI_CONTROL);
	A37X0_SPI_WRITE(sc, A37X0_SPI_CONTROL, reg & ~A37X0_SPI_CS_MASK);
	A37X0_SPI_WRITE(sc, A37X0_SPI_INTR_MASK, 0);

	/* Release the controller and wakeup the next thread waiting for it. */
	sc->sc_flags = 0;
	wakeup_one(dev);
	A37X0_SPI_UNLOCK(sc);

	return ((timeout == 0) ? EIO : 0);
}

static phandle_t
a37x0_spi_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static device_method_t a37x0_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a37x0_spi_probe),
	DEVMETHOD(device_attach,	a37x0_spi_attach),
	DEVMETHOD(device_detach,	a37x0_spi_detach),

	/* SPI interface */
	DEVMETHOD(spibus_transfer,	a37x0_spi_transfer),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	a37x0_spi_get_node),

	DEVMETHOD_END
};

static driver_t a37x0_spi_driver = {
	"spi",
	a37x0_spi_methods,
	sizeof(struct a37x0_spi_softc),
};

DRIVER_MODULE(a37x0_spi, simplebus, a37x0_spi_driver, 0, 0);
