/*-
 * Copyright (c) 2001 Tsubai Masanari.
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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/broadcom/bcm2835/bcm2835_gpio.h>
#include <arm/broadcom/bcm2835/bcm2835_bscreg.h>
#include <arm/broadcom/bcm2835/bcm2835_bscvar.h>

#include "iicbus_if.h"

static void bcm_bsc_intr(void *);

static void
bcm_bsc_modifyreg(struct bcm_bsc_softc *sc, uint32_t off, uint32_t mask,
	uint32_t value)
{
	uint32_t reg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);        
	reg = BCM_BSC_READ(sc, off);
	reg &= ~mask;
	reg |= value;
	BCM_BSC_WRITE(sc, off, reg);
}

static int
bcm_bsc_clock_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_bsc_softc *sc;
	uint32_t clk;
	int error;

	sc = (struct bcm_bsc_softc *)arg1;

	BCM_BSC_LOCK(sc);
	clk = BCM_BSC_READ(sc, BCM_BSC_CLOCK);
	BCM_BSC_UNLOCK(sc);
	clk &= 0xffff;
	if (clk == 0)
		clk = 32768;
	clk = BCM_BSC_CORE_CLK / clk;
	error = sysctl_handle_int(oidp, &clk, sizeof(clk), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	clk = BCM_BSC_CORE_CLK / clk;
	if (clk % 2)
		clk--;
	if (clk > 0xffff)
		clk = 0xffff;
	BCM_BSC_LOCK(sc);
	BCM_BSC_WRITE(sc, BCM_BSC_CLOCK, clk);
	BCM_BSC_UNLOCK(sc);

	return (0);
}

static int
bcm_bsc_clkt_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_bsc_softc *sc;
	uint32_t clkt;
	int error;

	sc = (struct bcm_bsc_softc *)arg1;

	BCM_BSC_LOCK(sc);
	clkt = BCM_BSC_READ(sc, BCM_BSC_CLKT);
	BCM_BSC_UNLOCK(sc);
	clkt &= 0xffff;
	error = sysctl_handle_int(oidp, &clkt, sizeof(clkt), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	BCM_BSC_LOCK(sc);
	BCM_BSC_WRITE(sc, BCM_BSC_CLKT, clkt & 0xffff);
	BCM_BSC_UNLOCK(sc);

	return (0);
}

static int
bcm_bsc_fall_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_bsc_softc *sc;
	uint32_t clk, reg;
	int error;

	sc = (struct bcm_bsc_softc *)arg1;

	BCM_BSC_LOCK(sc);
	reg = BCM_BSC_READ(sc, BCM_BSC_DELAY);
	BCM_BSC_UNLOCK(sc);
	reg >>= 16;
	error = sysctl_handle_int(oidp, &reg, sizeof(reg), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	BCM_BSC_LOCK(sc);
	clk = BCM_BSC_READ(sc, BCM_BSC_CLOCK);
	clk = BCM_BSC_CORE_CLK / clk;
	if (reg > clk / 2)
		reg = clk / 2 - 1;
	bcm_bsc_modifyreg(sc, BCM_BSC_DELAY, 0xffff0000, reg << 16);
	BCM_BSC_UNLOCK(sc);

	return (0);
}

static int
bcm_bsc_rise_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_bsc_softc *sc;
	uint32_t clk, reg;
	int error;

	sc = (struct bcm_bsc_softc *)arg1;

	BCM_BSC_LOCK(sc);
	reg = BCM_BSC_READ(sc, BCM_BSC_DELAY);
	BCM_BSC_UNLOCK(sc);
	reg &= 0xffff;
	error = sysctl_handle_int(oidp, &reg, sizeof(reg), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	BCM_BSC_LOCK(sc);
	clk = BCM_BSC_READ(sc, BCM_BSC_CLOCK);
	clk = BCM_BSC_CORE_CLK / clk;
	if (reg > clk / 2)
		reg = clk / 2 - 1;
	bcm_bsc_modifyreg(sc, BCM_BSC_DELAY, 0xffff, reg);
	BCM_BSC_UNLOCK(sc);

	return (0);
}

static void
bcm_bsc_sysctl_init(struct bcm_bsc_softc *sc)
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
	    bcm_bsc_clock_proc, "IU", "I2C BUS clock frequency");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "clock_stretch",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_bsc_clkt_proc, "IU", "I2C BUS clock stretch timeout");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "fall_edge_delay",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_bsc_fall_proc, "IU", "I2C BUS falling edge delay");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "rise_edge_delay",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    bcm_bsc_rise_proc, "IU", "I2C BUS rising edge delay");
}

static void
bcm_bsc_reset(struct bcm_bsc_softc *sc)
{

	/* Clear pending interrupts. */
	BCM_BSC_WRITE(sc, BCM_BSC_STATUS, BCM_BSC_STATUS_CLKT |
	    BCM_BSC_STATUS_ERR | BCM_BSC_STATUS_DONE);
	/* Clear the FIFO. */
	bcm_bsc_modifyreg(sc, BCM_BSC_CTRL, BCM_BSC_CTRL_CLEAR0,
	    BCM_BSC_CTRL_CLEAR0);
}

static int
bcm_bsc_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-bsc"))
		return (ENXIO);

	device_set_desc(dev, "BCM2708/2835 BSC controller");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm_bsc_attach(device_t dev)
{
	struct bcm_bsc_softc *sc;
	unsigned long start;
	device_t gpio;
	int i, rid;

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

	/* Check the unit we are attaching by its base address. */
	start = rman_get_start(sc->sc_mem_res);
	for (i = 0; i < nitems(bcm_bsc_pins); i++) {
		if (bcm_bsc_pins[i].start == start)
			break;
	}
	if (i == nitems(bcm_bsc_pins)) {
		device_printf(dev, "only bsc0 and bsc1 are supported\n");
		return (ENXIO);
	}

	/*
	 * Configure the GPIO pins to ALT0 function to enable BSC control
	 * over the pins.
	 */
	gpio = devclass_get_device(devclass_find("gpio"), 0);
	if (!gpio) {
		device_printf(dev, "cannot find gpio0\n");
		return (ENXIO);
	}
	bcm_gpio_set_alternate(gpio, bcm_bsc_pins[i].sda, BCM_GPIO_ALT0);
	bcm_gpio_set_alternate(gpio, bcm_bsc_pins[i].scl, BCM_GPIO_ALT0);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	/* Hook up our interrupt handler. */
	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, bcm_bsc_intr, sc, &sc->sc_intrhand)) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot setup the interrupt handler\n");
		return (ENXIO);
	}

	mtx_init(&sc->sc_mtx, "bcm_bsc", NULL, MTX_DEF);

	bcm_bsc_sysctl_init(sc);

	/* Enable the BSC controller.  Flush the FIFO. */
	BCM_BSC_LOCK(sc);
	BCM_BSC_WRITE(sc, BCM_BSC_CTRL, BCM_BSC_CTRL_I2CEN);
	bcm_bsc_reset(sc);
	BCM_BSC_UNLOCK(sc);

	device_add_child(dev, "iicbus", -1);

	return (bus_generic_attach(dev));
}

static int
bcm_bsc_detach(device_t dev)
{
	struct bcm_bsc_softc *sc;

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
bcm_bsc_intr(void *arg)
{
	struct bcm_bsc_softc *sc;
	uint32_t status;

	sc = (struct bcm_bsc_softc *)arg;

	BCM_BSC_LOCK(sc);

	/* The I2C interrupt is shared among all the BSC controllers. */
	if ((sc->sc_flags & BCM_I2C_BUSY) == 0) {
		BCM_BSC_UNLOCK(sc);
		return;
	}

	status = BCM_BSC_READ(sc, BCM_BSC_STATUS);

	/* Check for errors. */
	if (status & (BCM_BSC_STATUS_CLKT | BCM_BSC_STATUS_ERR)) {
		/* Disable interrupts. */
		BCM_BSC_WRITE(sc, BCM_BSC_CTRL, BCM_BSC_CTRL_I2CEN);
		sc->sc_flags |= BCM_I2C_ERROR;
		bcm_bsc_reset(sc);
		wakeup(sc->sc_dev);
		BCM_BSC_UNLOCK(sc);
		return;
	}

	if (sc->sc_flags & BCM_I2C_READ) {
		while (sc->sc_resid > 0 && (status & BCM_BSC_STATUS_RXD)) {
			*sc->sc_data++ = BCM_BSC_READ(sc, BCM_BSC_DATA);
			sc->sc_resid--;
			status = BCM_BSC_READ(sc, BCM_BSC_STATUS);
		}
	} else {
		while (sc->sc_resid > 0 && (status & BCM_BSC_STATUS_TXD)) {
			BCM_BSC_WRITE(sc, BCM_BSC_DATA, *sc->sc_data++);
			sc->sc_resid--;
			status = BCM_BSC_READ(sc, BCM_BSC_STATUS);
		}
	}

	if (status & BCM_BSC_STATUS_DONE) {
		/* Disable interrupts. */
		BCM_BSC_WRITE(sc, BCM_BSC_CTRL, BCM_BSC_CTRL_I2CEN);
		bcm_bsc_reset(sc);
		wakeup(sc->sc_dev);
	}

	BCM_BSC_UNLOCK(sc);
}

static int
bcm_bsc_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct bcm_bsc_softc *sc;
	uint32_t intr, read, status;
	int i, err;

	sc = device_get_softc(dev);
	BCM_BSC_LOCK(sc);

	/* If the controller is busy wait until it is available. */
	while (sc->sc_flags & BCM_I2C_BUSY)
		mtx_sleep(dev, &sc->sc_mtx, 0, "bcm_bsc", 0);

	/* Now we have control over the BSC controller. */
	sc->sc_flags = BCM_I2C_BUSY;

	/* Clear the FIFO and the pending interrupts. */
	bcm_bsc_reset(sc);

	err = 0;
	for (i = 0; i < nmsgs; i++) {

		/* Write the slave address. */
		BCM_BSC_WRITE(sc, BCM_BSC_SLAVE, msgs[i].slave);

		/* Write the data length. */
		BCM_BSC_WRITE(sc, BCM_BSC_DLEN, msgs[i].len);

		sc->sc_data = msgs[i].buf;
		sc->sc_resid = msgs[i].len;
		if ((msgs[i].flags & IIC_M_RD) == 0) {
			/* Fill up the TX FIFO. */
			status = BCM_BSC_READ(sc, BCM_BSC_STATUS);
			while (sc->sc_resid > 0 &&
			    (status & BCM_BSC_STATUS_TXD)) {
				BCM_BSC_WRITE(sc, BCM_BSC_DATA, *sc->sc_data);
				sc->sc_data++;
				sc->sc_resid--;
				status = BCM_BSC_READ(sc, BCM_BSC_STATUS);
			}
			read = 0;
			intr = BCM_BSC_CTRL_INTT;
			sc->sc_flags &= ~BCM_I2C_READ;
		} else {
			sc->sc_flags |= BCM_I2C_READ;
			read = BCM_BSC_CTRL_READ;
			intr = BCM_BSC_CTRL_INTR;
		}
		intr |= BCM_BSC_CTRL_INTD;

		/* Start the transfer. */
		BCM_BSC_WRITE(sc, BCM_BSC_CTRL, BCM_BSC_CTRL_I2CEN |
		    BCM_BSC_CTRL_ST | read | intr);

		/* Wait for the transaction to complete. */
		err = mtx_sleep(dev, &sc->sc_mtx, 0, "bcm_bsc", hz);

		/* Check if we have a timeout or an I2C error. */
		if ((sc->sc_flags & BCM_I2C_ERROR) || err == EWOULDBLOCK) {
			device_printf(sc->sc_dev, "I2C error\n");
			err = EIO;
			break;
		}
	}

	/* Clean the controller flags. */
	sc->sc_flags = 0;

	BCM_BSC_UNLOCK(sc);

	return (err);
}

static phandle_t
bcm_bsc_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the I2C bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t bcm_bsc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_bsc_probe),
	DEVMETHOD(device_attach,	bcm_bsc_attach),
	DEVMETHOD(device_detach,	bcm_bsc_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	iicbus_null_callback),
	DEVMETHOD(iicbus_transfer,	bcm_bsc_transfer),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	bcm_bsc_get_node),

	DEVMETHOD_END
};

static devclass_t bcm_bsc_devclass;

static driver_t bcm_bsc_driver = {
	"iichb",
	bcm_bsc_methods,
	sizeof(struct bcm_bsc_softc),
};

DRIVER_MODULE(iicbus, bcm2835_bsc, iicbus_driver, iicbus_devclass, 0, 0);
DRIVER_MODULE(bcm2835_bsc, simplebus, bcm_bsc_driver, bcm_bsc_devclass, 0, 0);
