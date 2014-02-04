/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

#include "spibus_if.h"

struct lpc_spi_softc
{
	device_t		ls_dev;
	struct resource *	ls_mem_res;
	struct resource *	ls_irq_res;
	bus_space_tag_t		ls_bst;
	bus_space_handle_t	ls_bsh;
};

static int lpc_spi_probe(device_t);
static int lpc_spi_attach(device_t);
static int lpc_spi_detach(device_t);
static int lpc_spi_transfer(device_t, device_t, struct spi_command *);

#define	lpc_spi_read_4(_sc, _reg)		\
    bus_space_read_4(_sc->ls_bst, _sc->ls_bsh, _reg)
#define	lpc_spi_write_4(_sc, _reg, _val)	\
    bus_space_write_4(_sc->ls_bst, _sc->ls_bsh, _reg, _val)

static int
lpc_spi_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "lpc,spi"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 PL022 SPI/SSP controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_spi_attach(device_t dev)
{
	struct lpc_spi_softc *sc = device_get_softc(dev);
	int rid;

	sc->ls_dev = dev;

	rid = 0;
	sc->ls_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->ls_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->ls_bst = rman_get_bustag(sc->ls_mem_res);
	sc->ls_bsh = rman_get_bushandle(sc->ls_mem_res);

	rid = 0;
	sc->ls_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->ls_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	bus_space_write_4(sc->ls_bst, 0xd0028100, 0, (1<<12)|(1<<10)|(1<<9)|(1<<8)|(1<<6)|(1<<5)); 
	lpc_pwr_write(dev, LPC_CLKPWR_SSP_CTRL, LPC_CLKPWR_SSP_CTRL_SSP0EN);
	lpc_spi_write_4(sc, LPC_SSP_CR0, LPC_SSP_CR0_DSS(8));
	lpc_spi_write_4(sc, LPC_SSP_CR1, LPC_SSP_CR1_SSE);
	lpc_spi_write_4(sc, LPC_SSP_CPSR, 128);

	device_add_child(dev, "spibus", 0);
	return (bus_generic_attach(dev));
}

static int
lpc_spi_detach(device_t dev)
{
	return (EBUSY);
}

static int
lpc_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct lpc_spi_softc *sc = device_get_softc(dev);
	struct spibus_ivar *devi = SPIBUS_IVAR(child);
	uint8_t *in_buf, *out_buf;
	int i;

	/* Set CS active */
	lpc_gpio_set_state(child, devi->cs, 0);

	/* Wait for FIFO to be ready */
	while ((lpc_spi_read_4(sc, LPC_SSP_SR) & LPC_SSP_SR_TNF) == 0);

	/* Command */
	in_buf = cmd->rx_cmd;
	out_buf = cmd->tx_cmd;
	for (i = 0; i < cmd->tx_cmd_sz; i++) {
		lpc_spi_write_4(sc, LPC_SSP_DR, out_buf[i]);
		in_buf[i] = lpc_spi_read_4(sc, LPC_SSP_DR);
	}

	/* Data */
	in_buf = cmd->rx_data;
	out_buf = cmd->tx_data;
	for (i = 0; i < cmd->tx_data_sz; i++) {
		lpc_spi_write_4(sc, LPC_SSP_DR, out_buf[i]);
		in_buf[i] = lpc_spi_read_4(sc, LPC_SSP_DR);
	}

	/* Set CS inactive */
	lpc_gpio_set_state(child, devi->cs, 1);

	return (0);
}

static device_method_t lpc_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_spi_probe),
	DEVMETHOD(device_attach,	lpc_spi_attach),
	DEVMETHOD(device_detach,	lpc_spi_detach),

	/* SPI interface */
	DEVMETHOD(spibus_transfer,	lpc_spi_transfer),

	{ 0, 0 }
};

static devclass_t lpc_spi_devclass;

static driver_t lpc_spi_driver = {
	"spi",
	lpc_spi_methods,
	sizeof(struct lpc_spi_softc),
};

DRIVER_MODULE(lpcspi, simplebus, lpc_spi_driver, lpc_spi_devclass, 0, 0);
