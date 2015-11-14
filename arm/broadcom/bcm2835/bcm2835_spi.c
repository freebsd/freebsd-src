/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2013 Luiz Otavio O Souza <loos@freebsd.org>
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include <arm/broadcom/bcm2835/bcm2835_gpio.h>
#include <arm/broadcom/bcm2835/bcm2835_spireg.h>
#include <arm/broadcom/bcm2835/bcm2835_spivar.h>

#include "spibus_if.h"

static void bcm_spi_intr(void *);

#ifdef	BCM_SPI_DEBUG
static void
bcm_spi_printr(device_t dev)
{
	struct bcm_spi_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	reg = BCM_SPI_READ(sc, SPI_CS);
	device_printf(dev, "CS=%b\n", reg,
	    "\20\1CS0\2CS1\3CPHA\4CPOL\7CSPOL"
	    "\10TA\11DMAEN\12INTD\13INTR\14ADCS\15REN\16LEN"
	    "\21DONE\22RXD\23TXD\24RXR\25RXF\26CSPOL0\27CSPOL1"
	    "\30CSPOL2\31DMA_LEN\32LEN_LONG");
	reg = BCM_SPI_READ(sc, SPI_CLK) & SPI_CLK_MASK;
	if (reg % 2)
		reg--;
	if (reg == 0)
		reg = 65536;
	device_printf(dev, "CLK=%uMhz/%d=%luhz\n",
	    SPI_CORE_CLK / 1000000, reg, SPI_CORE_CLK / reg);
	reg = BCM_SPI_READ(sc, SPI_DLEN) & SPI_DLEN_MASK;
	device_printf(dev, "DLEN=%d\n", reg);
	reg = BCM_SPI_READ(sc, SPI_LTOH) & SPI_LTOH_MASK;
	device_printf(dev, "LTOH=%d\n", reg);
	reg = BCM_SPI_READ(sc, SPI_DC);
	device_printf(dev, "DC=RPANIC=%#x RDREQ=%#x TPANIC=%#x TDREQ=%#x\n",
	    (reg & SPI_DC_RPANIC_MASK) >> SPI_DC_RPANIC_SHIFT,
	    (reg & SPI_DC_RDREQ_MASK) >> SPI_DC_RDREQ_SHIFT,
	    (reg & SPI_DC_TPANIC_MASK) >> SPI_DC_TPANIC_SHIFT,
	    (reg & SPI_DC_TDREQ_MASK) >> SPI_DC_TDREQ_SHIFT);
}
#endif

static void
bcm_spi_modifyreg(struct bcm_spi_softc *sc, uint32_t off, uint32_t mask,
	uint32_t value)
{
	uint32_t reg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	reg = BCM_SPI_READ(sc, off);
	reg &= ~mask;
	reg |= value;
	BCM_SPI_WRITE(sc, off, reg);
}

static int
bcm_spi_clock_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_spi_softc *sc;
	uint32_t clk;
	int error;

	sc = (struct bcm_spi_softc *)arg1;

	BCM_SPI_LOCK(sc);
	clk = BCM_SPI_READ(sc, SPI_CLK);
	BCM_SPI_UNLOCK(sc);
	clk &= 0xffff;
	if (clk == 0)
		clk = 65536;
	clk = SPI_CORE_CLK / clk;

	error = sysctl_handle_int(oidp, &clk, sizeof(clk), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	clk = SPI_CORE_CLK / clk;
	if (clk <= 1)
		clk = 2;
	else if (clk % 2)
		clk--;
	if (clk > 0xffff)
		clk = 0;
	BCM_SPI_LOCK(sc);
	BCM_SPI_WRITE(sc, SPI_CLK, clk);
	BCM_SPI_UNLOCK(sc);

	return (0);
}

static int
bcm_spi_cs_bit_proc(SYSCTL_HANDLER_ARGS, uint32_t bit)
{
	struct bcm_spi_softc *sc;
	uint32_t reg;
	int error;

	sc = (struct bcm_spi_softc *)arg1;
	BCM_SPI_LOCK(sc);
	reg = BCM_SPI_READ(sc, SPI_CS);
	BCM_SPI_UNLOCK(sc);
	reg = (reg & bit) ? 1 : 0;

	error = sysctl_handle_int(oidp, &reg, sizeof(reg), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (reg)
		reg = bit;
	BCM_SPI_LOCK(sc);
	bcm_spi_modifyreg(sc, SPI_CS, bit, reg);
	BCM_SPI_UNLOCK(sc);

	return (0);
}

static int
bcm_spi_cpol_proc(SYSCTL_HANDLER_ARGS)
{

	return (bcm_spi_cs_bit_proc(oidp, arg1, arg2, req, SPI_CS_CPOL));
}

static int
bcm_spi_cpha_proc(SYSCTL_HANDLER_ARGS)
{

	return (bcm_spi_cs_bit_proc(oidp, arg1, arg2, req, SPI_CS_CPHA));
}

static int
bcm_spi_cspol0_proc(SYSCTL_HANDLER_ARGS)
{

	return (bcm_spi_cs_bit_proc(oidp, arg1, arg2, req, SPI_CS_CSPOL0));
}

static int
bcm_spi_cspol1_proc(SYSCTL_HANDLER_ARGS)
{

	return (bcm_spi_cs_bit_proc(oidp, arg1, arg2, req, SPI_CS_CSPOL1));
}

static void
bcm_spi_sysctl_init(struct bcm_spi_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	/*
	 * Add system sysctl tree/handlers.
	 */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree_node = device_get_sysctl_tree(sc->sc_dev);
	tree = SYSCTL_CHILDREN(tree_node);
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "clock",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_spi_clock_proc, "IU", "SPI BUS clock frequency");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "cpol",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_spi_cpol_proc, "IU", "SPI BUS clock polarity");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "cpha",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_spi_cpha_proc, "IU", "SPI BUS clock phase");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "cspol0",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_spi_cspol0_proc, "IU", "SPI BUS chip select 0 polarity");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "cspol1",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_spi_cspol1_proc, "IU", "SPI BUS chip select 1 polarity");
}

static int
bcm_spi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-spi"))
		return (ENXIO);

	device_set_desc(dev, "BCM2708/2835 SPI controller");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm_spi_attach(device_t dev)
{
	struct bcm_spi_softc *sc;
	device_t gpio;
	int i, rid;

	if (device_get_unit(dev) != 0) {
		device_printf(dev, "only one SPI controller supported\n");
		return (ENXIO);
	}

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	/* Configure the GPIO pins to ALT0 function to enable SPI the pins. */
	gpio = devclass_get_device(devclass_find("gpio"), 0);
	if (!gpio) {
		device_printf(dev, "cannot find gpio0\n");
		return (ENXIO);
	}
	for (i = 0; i < nitems(bcm_spi_pins); i++)
		bcm_gpio_set_alternate(gpio, bcm_spi_pins[i], BCM_GPIO_ALT0);

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

	/* Hook up our interrupt handler. */
	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, bcm_spi_intr, sc, &sc->sc_intrhand)) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot setup the interrupt handler\n");
		return (ENXIO);
	}

	mtx_init(&sc->sc_mtx, "bcm_spi", NULL, MTX_DEF);

	/* Add sysctl nodes. */
	bcm_spi_sysctl_init(sc);

#ifdef	BCM_SPI_DEBUG
	bcm_spi_printr(dev);
#endif

	/*
	 * Enable the SPI controller.  Clear the rx and tx FIFO.
	 * Defaults to SPI mode 0.
	 */
	BCM_SPI_WRITE(sc, SPI_CS, SPI_CS_CLEAR_RXFIFO | SPI_CS_CLEAR_TXFIFO);

	/* Set the SPI clock to 500Khz. */
	BCM_SPI_WRITE(sc, SPI_CLK, SPI_CORE_CLK / 500000);

#ifdef	BCM_SPI_DEBUG
	bcm_spi_printr(dev);
#endif

	device_add_child(dev, "spibus", -1);

	return (bus_generic_attach(dev));
}

static int
bcm_spi_detach(device_t dev)
{
	struct bcm_spi_softc *sc;

	bus_generic_detach(dev);

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

static void
bcm_spi_fill_fifo(struct bcm_spi_softc *sc)
{
	struct spi_command *cmd;
	uint32_t cs, written;
	uint8_t *data;

	cmd = sc->sc_cmd;
	cs = BCM_SPI_READ(sc, SPI_CS) & (SPI_CS_TA | SPI_CS_TXD);
	while (sc->sc_written < sc->sc_len &&
	    cs == (SPI_CS_TA | SPI_CS_TXD)) {
		data = (uint8_t *)cmd->tx_cmd;
		written = sc->sc_written++;
		if (written >= cmd->tx_cmd_sz) {
			data = (uint8_t *)cmd->tx_data;
			written -= cmd->tx_cmd_sz;
		}
		BCM_SPI_WRITE(sc, SPI_FIFO, data[written]);
		cs = BCM_SPI_READ(sc, SPI_CS) & (SPI_CS_TA | SPI_CS_TXD);
	}
}

static void
bcm_spi_drain_fifo(struct bcm_spi_softc *sc)
{
	struct spi_command *cmd;
	uint32_t cs, read;
	uint8_t *data;

	cmd = sc->sc_cmd;
	cs = BCM_SPI_READ(sc, SPI_CS) & SPI_CS_RXD;
	while (sc->sc_read < sc->sc_len && cs == SPI_CS_RXD) {
		data = (uint8_t *)cmd->rx_cmd;
		read = sc->sc_read++;
		if (read >= cmd->rx_cmd_sz) {
			data = (uint8_t *)cmd->rx_data;
			read -= cmd->rx_cmd_sz;
		}
		data[read] = BCM_SPI_READ(sc, SPI_FIFO) & 0xff;
		cs = BCM_SPI_READ(sc, SPI_CS) & SPI_CS_RXD;
	}
}

static void
bcm_spi_intr(void *arg)
{
	struct bcm_spi_softc *sc;

	sc = (struct bcm_spi_softc *)arg;
	BCM_SPI_LOCK(sc);

	/* Filter stray interrupts. */
	if ((sc->sc_flags & BCM_SPI_BUSY) == 0) {
		BCM_SPI_UNLOCK(sc);
		return;
	}

	/* TX - Fill up the FIFO. */
	bcm_spi_fill_fifo(sc);

	/* RX - Drain the FIFO. */
	bcm_spi_drain_fifo(sc);

	/* Check for end of transfer. */
	if (sc->sc_written == sc->sc_len && sc->sc_read == sc->sc_len) {
		/* Disable interrupts and the SPI engine. */
		bcm_spi_modifyreg(sc, SPI_CS,
		    SPI_CS_TA | SPI_CS_INTR | SPI_CS_INTD, 0);
		wakeup(sc->sc_dev);
	}

	BCM_SPI_UNLOCK(sc);
}

static int
bcm_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct bcm_spi_softc *sc;
	int cs, err;

	sc = device_get_softc(dev);

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz, 
	    ("TX/RX command sizes should be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz, 
	    ("TX/RX data sizes should be equal"));

	/* Get the proper chip select for this child. */
	spibus_get_cs(child, &cs);
	if (cs < 0 || cs > 2) {
		device_printf(dev,
		    "Invalid chip select %d requested by %s\n", cs,
		    device_get_nameunit(child));
		return (EINVAL);
	}

	BCM_SPI_LOCK(sc);

	/* If the controller is in use wait until it is available. */
	while (sc->sc_flags & BCM_SPI_BUSY)
		mtx_sleep(dev, &sc->sc_mtx, 0, "bcm_spi", 0);

	/* Now we have control over SPI controller. */
	sc->sc_flags = BCM_SPI_BUSY;

	/* Clear the FIFO. */
	bcm_spi_modifyreg(sc, SPI_CS,
	    SPI_CS_CLEAR_RXFIFO | SPI_CS_CLEAR_TXFIFO,
	    SPI_CS_CLEAR_RXFIFO | SPI_CS_CLEAR_TXFIFO);

	/* Save a pointer to the SPI command. */
	sc->sc_cmd = cmd;
	sc->sc_read = 0;
	sc->sc_written = 0;
	sc->sc_len = cmd->tx_cmd_sz + cmd->tx_data_sz;

	/*
	 * Set the CS for this transaction, enable interrupts and announce
	 * we're ready to tx.  This will kick off the first interrupt.
	 */
	bcm_spi_modifyreg(sc, SPI_CS,
	    SPI_CS_MASK | SPI_CS_TA | SPI_CS_INTR | SPI_CS_INTD,
	    cs | SPI_CS_TA | SPI_CS_INTR | SPI_CS_INTD);

	/* Wait for the transaction to complete. */
	err = mtx_sleep(dev, &sc->sc_mtx, 0, "bcm_spi", hz * 2);

	/* Make sure the SPI engine and interrupts are disabled. */
	bcm_spi_modifyreg(sc, SPI_CS, SPI_CS_TA | SPI_CS_INTR | SPI_CS_INTD, 0);

	/* Release the controller and wakeup the next thread waiting for it. */
	sc->sc_flags = 0;
	wakeup_one(dev);
	BCM_SPI_UNLOCK(sc);

	/*
	 * Check for transfer timeout.  The SPI controller doesn't
	 * return errors.
	 */
	if (err == EWOULDBLOCK) {
		device_printf(sc->sc_dev, "SPI error\n");
		err = EIO;
	}

	return (err);
}

static phandle_t
bcm_spi_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the SPI bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t bcm_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_spi_probe),
	DEVMETHOD(device_attach,	bcm_spi_attach),
	DEVMETHOD(device_detach,	bcm_spi_detach),

	/* SPI interface */
	DEVMETHOD(spibus_transfer,	bcm_spi_transfer),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	bcm_spi_get_node),

	DEVMETHOD_END
};

static devclass_t bcm_spi_devclass;

static driver_t bcm_spi_driver = {
	"spi",
	bcm_spi_methods,
	sizeof(struct bcm_spi_softc),
};

DRIVER_MODULE(bcm2835_spi, simplebus, bcm_spi_driver, bcm_spi_devclass, 0, 0);
