/*-
 * Copyright (c) 2018 Thomas Skibo <thomasskibo@yahoo.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "spibus_if.h"

static struct ofw_compat_data compat_data[] = {
	{"xlnx,zy7_spi",		1},
	{"xlnx,zynq-spi-1.0",		1},
	{"cdns,spi-r1p6",		1},
	{NULL,				0}
};

struct zy7_spi_softc {
	device_t		dev;
	device_t		child;
	struct mtx		sc_mtx;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*intrhandle;

	uint32_t		cfg_reg_shadow;
	uint32_t		spi_clock;
	uint32_t		ref_clock;
	unsigned int		spi_clk_real_freq;
	unsigned int		rx_overflows;
	unsigned int		tx_underflows;
	unsigned int		interrupts;
	unsigned int		stray_ints;
	struct spi_command	*cmd;
	int			tx_bytes;	/* tx_cmd_sz + tx_data_sz */
	int			tx_bytes_sent;
	int			rx_bytes;	/* rx_cmd_sz + rx_data_sz */
	int			rx_bytes_rcvd;
	int			busy;
};

#define ZY7_SPI_DEFAULT_SPI_CLOCK	50000000

#define SPI_SC_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	SPI_SC_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define SPI_SC_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	NULL, MTX_DEF)
#define SPI_SC_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->sc_mtx)
#define SPI_SC_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

#define RD4(sc, off)		(bus_read_4((sc)->mem_res, (off)))
#define WR4(sc, off, val)	(bus_write_4((sc)->mem_res, (off), (val)))

/*
 * SPI device registers.
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.12.1) December 6, 2017.  Xilinx doc UG585.
 */
#define ZY7_SPI_CONFIG_REG		0x0000
#define   ZY7_SPI_CONFIG_MODEFAIL_GEN_EN	(1 << 17)
#define   ZY7_SPI_CONFIG_MAN_STRT		(1 << 16)
#define   ZY7_SPI_CONFIG_MAN_STRT_EN		(1 << 15)
#define   ZY7_SPI_CONFIG_MAN_CS			(1 << 14)
#define   ZY7_SPI_CONFIG_CS_MASK		(0xf << 10)
#define   ZY7_SPI_CONFIG_CS(x)			((0xf ^ (1 << (x))) << 10)
#define   ZY7_SPI_CONFIG_PERI_SEL		(1 << 9)
#define   ZY7_SPI_CONFIG_REF_CLK		(1 << 8)
#define   ZY7_SPI_CONFIG_BAUD_RATE_DIV_MASK	(7 << 3)
#define   ZY7_SPI_CONFIG_BAUD_RATE_DIV_SHIFT	3
#define   ZY7_SPI_CONFIG_BAUD_RATE_DIV(x)	((x) << 3) /* divide by 2<<x */
#define   ZY7_SPI_CONFIG_CLK_PH			(1 << 2)   /* clock phase */
#define   ZY7_SPI_CONFIG_CLK_POL		(1 << 1)   /* clock polatiry */
#define   ZY7_SPI_CONFIG_MODE_SEL		(1 << 0)   /* master enable */

#define ZY7_SPI_INTR_STAT_REG		0x0004
#define ZY7_SPI_INTR_EN_REG		0x0008
#define ZY7_SPI_INTR_DIS_REG		0x000c
#define ZY7_SPI_INTR_MASK_REG		0x0010
#define   ZY7_SPI_INTR_TX_FIFO_UNDERFLOW	(1 << 6)
#define   ZY7_SPI_INTR_RX_FIFO_FULL		(1 << 5)
#define   ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY	(1 << 4)
#define   ZY7_SPI_INTR_TX_FIFO_FULL		(1 << 3)
#define   ZY7_SPI_INTR_TX_FIFO_NOT_FULL		(1 << 2)
#define   ZY7_SPI_INTR_MODE_FAULT		(1 << 1)
#define   ZY7_SPI_INTR_RX_OVERFLOW		(1 << 0)

#define ZY7_SPI_EN_REG			0x0014
#define   ZY7_SPI_ENABLE		(1 << 0)

#define ZY7_SPI_DELAY_CTRL_REG		0x0018
#define   ZY7_SPI_DELAY_CTRL_BTWN_MASK		(0xff << 16)
#define   ZY7_SPI_DELAY_CTRL_BTWN_SHIFT		16
#define   ZY7_SPI_DELAY_CTRL_AFTER_MASK		(0xff << 8)
#define   ZY7_SPI_DELAY_CTRL_AFTER_SHIFT	8
#define   ZY7_SPI_DELAY_CTRL_INIT_MASK		(0xff << 0)
#define   ZY7_SPI_DELAY_CTRL_INIT_SHIFT		0

#define ZY7_SPI_TX_DATA_REG		0x001c
#define ZY7_SPI_RX_DATA_REG		0x0020

#define ZY7_SPI_SLV_IDLE_COUNT_REG	0x0024

#define ZY7_SPI_TX_THRESH_REG		0x0028
#define ZY7_SPI_RX_THRESH_REG		0x002c

/* Fill hardware fifo with command and data bytes. */
static void
zy7_spi_write_fifo(struct zy7_spi_softc *sc, int nbytes)
{
	uint8_t byte;

	while (nbytes > 0) {
		if (sc->tx_bytes_sent < sc->cmd->tx_cmd_sz)
			/* Writing command. */
			byte = *((uint8_t *)sc->cmd->tx_cmd +
				 sc->tx_bytes_sent);
		else
			/* Writing data. */
			byte = *((uint8_t *)sc->cmd->tx_data +
				 (sc->tx_bytes_sent - sc->cmd->tx_cmd_sz));

		WR4(sc, ZY7_SPI_TX_DATA_REG, (uint32_t)byte);

		sc->tx_bytes_sent++;
		nbytes--;
	}
}

/* Read hardware fifo data into command response and data buffers. */
static void
zy7_spi_read_fifo(struct zy7_spi_softc *sc)
{
	uint8_t byte;

	do {
		byte = RD4(sc, ZY7_SPI_RX_DATA_REG) & 0xff;

		if (sc->rx_bytes_rcvd < sc->cmd->rx_cmd_sz)
			/* Reading command. */
			*((uint8_t *)sc->cmd->rx_cmd + sc->rx_bytes_rcvd) =
			    byte;
		else
			/* Reading data. */
			*((uint8_t *)sc->cmd->rx_data +
			    (sc->rx_bytes_rcvd - sc->cmd->rx_cmd_sz)) =
			    byte;

		sc->rx_bytes_rcvd++;

	} while (sc->rx_bytes_rcvd < sc->rx_bytes &&
	    (RD4(sc, ZY7_SPI_INTR_STAT_REG) &
		ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY) != 0);
}

/* End a transfer early by draining rx fifo and disabling interrupts. */
static void
zy7_spi_abort_transfer(struct zy7_spi_softc *sc)
{
	/* Drain receive fifo. */
	while ((RD4(sc, ZY7_SPI_INTR_STAT_REG) &
		ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY) != 0)
		(void)RD4(sc, ZY7_SPI_RX_DATA_REG);

	/* Shut down interrupts. */
	WR4(sc, ZY7_SPI_INTR_DIS_REG,
	    ZY7_SPI_INTR_RX_OVERFLOW |
	    ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY |
	    ZY7_SPI_INTR_TX_FIFO_NOT_FULL);
}

static void
zy7_spi_intr(void *arg)
{
	struct zy7_spi_softc *sc = (struct zy7_spi_softc *)arg;
	uint32_t istatus;

	SPI_SC_LOCK(sc);

	sc->interrupts++;

	istatus = RD4(sc, ZY7_SPI_INTR_STAT_REG);

	/* Stray interrupts can happen if a transfer gets interrupted. */
	if (!sc->busy) {
		sc->stray_ints++;
		SPI_SC_UNLOCK(sc);
		return;
	}

	if ((istatus & ZY7_SPI_INTR_RX_OVERFLOW) != 0) {
		device_printf(sc->dev, "rx fifo overflow!\n");
		sc->rx_overflows++;

		/* Clear status bit. */
		WR4(sc, ZY7_SPI_INTR_STAT_REG,
		    ZY7_SPI_INTR_RX_OVERFLOW);
	}

	/* Empty receive fifo before any more transmit data is sent. */
	if (sc->rx_bytes_rcvd < sc->rx_bytes &&
	    (istatus & ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY) != 0) {
		zy7_spi_read_fifo(sc);
		if (sc->rx_bytes_rcvd == sc->rx_bytes)
			/* Disable receive interrupts. */
			WR4(sc, ZY7_SPI_INTR_DIS_REG,
			    ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY |
			    ZY7_SPI_INTR_RX_OVERFLOW);
	}

	/* Count tx underflows.  They probably shouldn't happen. */
	if ((istatus & ZY7_SPI_INTR_TX_FIFO_UNDERFLOW) != 0) {
		sc->tx_underflows++;

		/* Clear status bit. */
		WR4(sc, ZY7_SPI_INTR_STAT_REG,
		    ZY7_SPI_INTR_TX_FIFO_UNDERFLOW);
	}

	/* Fill transmit fifo. */
	if (sc->tx_bytes_sent < sc->tx_bytes &&
	    (istatus & ZY7_SPI_INTR_TX_FIFO_NOT_FULL) != 0) {
		zy7_spi_write_fifo(sc, MIN(96, sc->tx_bytes -
			sc->tx_bytes_sent));

		if (sc->tx_bytes_sent == sc->tx_bytes) {
			/* Disable transmit FIFO interrupt, enable receive
			 * FIFO interrupt.
			 */
			WR4(sc, ZY7_SPI_INTR_DIS_REG,
			    ZY7_SPI_INTR_TX_FIFO_NOT_FULL);
			WR4(sc, ZY7_SPI_INTR_EN_REG,
			    ZY7_SPI_INTR_RX_FIFO_NOT_EMPTY);
		}
	}

	/* Finished with transfer? */
	if (sc->tx_bytes_sent == sc->tx_bytes &&
	    sc->rx_bytes_rcvd == sc->rx_bytes) {
		/* De-assert CS. */
		sc->cfg_reg_shadow &=
		    ~(ZY7_SPI_CONFIG_CLK_PH | ZY7_SPI_CONFIG_CLK_POL);
		sc->cfg_reg_shadow |= ZY7_SPI_CONFIG_CS_MASK;
		WR4(sc, ZY7_SPI_CONFIG_REG, sc->cfg_reg_shadow);

		wakeup(sc->dev);
	}

	SPI_SC_UNLOCK(sc);
}

/* Initialize hardware. */
static int
zy7_spi_init_hw(struct zy7_spi_softc *sc)
{
	uint32_t baud_div;

	/* Find best clock divider. Divide by 2 not supported. */
	baud_div = 1;
	while ((sc->ref_clock >> (baud_div + 1)) > sc->spi_clock &&
	    baud_div < 8)
		baud_div++;
	if (baud_div >= 8) {
		device_printf(sc->dev, "cannot configure clock divider: ref=%d"
		    " spi=%d.\n", sc->ref_clock, sc->spi_clock);
		return (EINVAL);
	}
	sc->spi_clk_real_freq = sc->ref_clock >> (baud_div + 1);

	/* Set up configuration register. */
	sc->cfg_reg_shadow =
	    ZY7_SPI_CONFIG_MAN_CS |
	    ZY7_SPI_CONFIG_CS_MASK |
	    ZY7_SPI_CONFIG_BAUD_RATE_DIV(baud_div) |
	    ZY7_SPI_CONFIG_MODE_SEL;
	WR4(sc, ZY7_SPI_CONFIG_REG, sc->cfg_reg_shadow);

	/* Set thresholds. */
	WR4(sc, ZY7_SPI_TX_THRESH_REG, 32);
	WR4(sc, ZY7_SPI_RX_THRESH_REG, 1);

	/* Clear and disable all interrupts. */
	WR4(sc, ZY7_SPI_INTR_STAT_REG, ~0);
	WR4(sc, ZY7_SPI_INTR_DIS_REG, ~0);

	/* Enable SPI. */
	WR4(sc, ZY7_SPI_EN_REG, ZY7_SPI_ENABLE);

	return (0);
}

static void
zy7_spi_add_sysctls(device_t dev)
{
	struct zy7_spi_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *child;

	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "spi_clk_real_freq", CTLFLAG_RD,
	    &sc->spi_clk_real_freq, 0, "SPI clock real frequency");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "rx_overflows", CTLFLAG_RD,
	    &sc->rx_overflows, 0, "RX FIFO overflow events");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "tx_underflows", CTLFLAG_RD,
	    &sc->tx_underflows, 0, "TX FIFO underflow events");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "interrupts", CTLFLAG_RD,
	    &sc->interrupts, 0, "interrupt calls");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "stray_ints", CTLFLAG_RD,
	    &sc->stray_ints, 0, "stray interrupts");
}

static int
zy7_spi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Zynq SPI Controller");

	return (BUS_PROBE_DEFAULT);
}

static int zy7_spi_detach(device_t);

static int
zy7_spi_attach(device_t dev)
{
	struct zy7_spi_softc *sc;
	int rid, err;
	phandle_t node;
	pcell_t cell;

	sc = device_get_softc(dev);
	sc->dev = dev;

	SPI_SC_LOCK_INIT(sc);

	/* Get ref-clock and spi-clock properties. */
	node = ofw_bus_get_node(dev);
	if (OF_getprop(node, "ref-clock", &cell, sizeof(cell)) > 0)
		sc->ref_clock = fdt32_to_cpu(cell);
	else {
		device_printf(dev, "must have ref-clock property\n");
		return (ENXIO);
	}
	if (OF_getprop(node, "spi-clock", &cell, sizeof(cell)) > 0)
		sc->spi_clock = fdt32_to_cpu(cell);
	else
		sc->spi_clock = ZY7_SPI_DEFAULT_SPI_CLOCK;

	/* Get memory resource. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		zy7_spi_detach(dev);
		return (ENOMEM);
	}

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate IRQ resource.\n");
		zy7_spi_detach(dev);
		return (ENOMEM);
	}

	/* Activate the interrupt. */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, zy7_spi_intr, sc, &sc->intrhandle);
	if (err) {
		device_printf(dev, "could not setup IRQ.\n");
		zy7_spi_detach(dev);
		return (err);
	}

	/* Configure the device. */
	err = zy7_spi_init_hw(sc);
	if (err) {
		zy7_spi_detach(dev);
		return (err);
	}

	sc->child = device_add_child(dev, "spibus", -1);

	zy7_spi_add_sysctls(dev);

	/* Attach spibus driver as a child later when interrupts work. */
	config_intrhook_oneshot((ich_func_t)bus_generic_attach, dev);

	return (0);
}

static int
zy7_spi_detach(device_t dev)
{
	struct zy7_spi_softc *sc = device_get_softc(dev);

	if (device_is_attached(dev))
		bus_generic_detach(dev);

	/* Delete child bus. */
	if (sc->child)
		device_delete_child(dev, sc->child);

	/* Disable hardware. */
	if (sc->mem_res != NULL) {
		/* Disable SPI. */
		WR4(sc, ZY7_SPI_EN_REG, 0);

		/* Clear and disable all interrupts. */
		WR4(sc, ZY7_SPI_INTR_STAT_REG, ~0);
		WR4(sc, ZY7_SPI_INTR_DIS_REG, ~0);
	}

	/* Teardown and release interrupt. */
	if (sc->irq_res != NULL) {
		if (sc->intrhandle)
			bus_teardown_intr(dev, sc->irq_res, sc->intrhandle);
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	}

	/* Release memory resource. */
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);

	SPI_SC_LOCK_DESTROY(sc);

	return (0);
}

static phandle_t
zy7_spi_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static int
zy7_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct zy7_spi_softc *sc = device_get_softc(dev);
	uint32_t cs;
	uint32_t mode;
	int err = 0;

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz,
	    ("TX/RX command sizes should be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz,
	    ("TX/RX data sizes should be equal"));

	/* Get chip select and mode for this child. */
	spibus_get_cs(child, &cs);
	cs &= ~SPIBUS_CS_HIGH;
	if (cs > 2) {
		device_printf(dev, "Invalid chip select %d requested by %s",
		    cs, device_get_nameunit(child));
		return (EINVAL);
	}
	spibus_get_mode(child, &mode);

	SPI_SC_LOCK(sc);

	/* Wait for controller available. */
	while (sc->busy != 0) {
		err = mtx_sleep(dev, &sc->sc_mtx, 0, "zspi0", 0);
		if (err) {
			SPI_SC_UNLOCK(sc);
			return (err);
		}
	}

	/* Start transfer. */
	sc->busy = 1;
	sc->cmd = cmd;
	sc->tx_bytes = sc->cmd->tx_cmd_sz + sc->cmd->tx_data_sz;
	sc->tx_bytes_sent = 0;
	sc->rx_bytes = sc->cmd->rx_cmd_sz + sc->cmd->rx_data_sz;
	sc->rx_bytes_rcvd = 0;

	/* Enable interrupts.  zy7_spi_intr() will handle transfer. */
	WR4(sc, ZY7_SPI_INTR_EN_REG,
	    ZY7_SPI_INTR_TX_FIFO_NOT_FULL |
	    ZY7_SPI_INTR_RX_OVERFLOW);

	/* Handle polarity and phase. */
	if (mode == SPIBUS_MODE_CPHA || mode == SPIBUS_MODE_CPOL_CPHA)
		sc->cfg_reg_shadow |= ZY7_SPI_CONFIG_CLK_PH;
	if (mode == SPIBUS_MODE_CPOL || mode == SPIBUS_MODE_CPOL_CPHA)
		sc->cfg_reg_shadow |= ZY7_SPI_CONFIG_CLK_POL;

	/* Assert CS. */
	sc->cfg_reg_shadow &= ~ZY7_SPI_CONFIG_CS_MASK;
	sc->cfg_reg_shadow |= ZY7_SPI_CONFIG_CS(cs);
	WR4(sc, ZY7_SPI_CONFIG_REG, sc->cfg_reg_shadow);

	/* Wait for completion. */
	err = mtx_sleep(dev, &sc->sc_mtx, 0, "zspi1", hz * 2);
	if (err)
		zy7_spi_abort_transfer(sc);

	/* Release controller. */
	sc->busy = 0;
	wakeup_one(dev);

	SPI_SC_UNLOCK(sc);

	return (err);
}

static device_method_t zy7_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		zy7_spi_probe),
	DEVMETHOD(device_attach,	zy7_spi_attach),
	DEVMETHOD(device_detach,	zy7_spi_detach),

	/* SPI interface */
	DEVMETHOD(spibus_transfer,	zy7_spi_transfer),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	zy7_spi_get_node),

	DEVMETHOD_END
};

static driver_t zy7_spi_driver = {
	"zy7_spi",
	zy7_spi_methods,
	sizeof(struct zy7_spi_softc),
};

DRIVER_MODULE(zy7_spi, simplebus, zy7_spi_driver, 0, 0);
DRIVER_MODULE(ofw_spibus, zy7_spi, ofw_spibus_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
MODULE_DEPEND(zy7_spi, ofw_spibus, 1, 1, 1);
