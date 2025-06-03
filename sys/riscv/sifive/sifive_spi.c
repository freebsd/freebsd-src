/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Axiado Corporation
 * All rights reserved.
 *
 * This software was developed in part by Philip Paeps and Kristof Provost
 * under contract for Axiado Corporation.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/clk/clk.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "spibus_if.h"

#if 1
#define	DBGPRINT(dev, fmt, args...) \
	device_printf(dev, "%s: " fmt "\n", __func__, ## args)
#else
#define	DBGPRINT(dev, fmt, args...)
#endif

static struct resource_spec sfspi_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	RESOURCE_SPEC_END
};

struct sfspi_softc {
	device_t		dev;
	device_t		parent;

	struct mtx		mtx;

	struct resource		*res;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;

	void			*ih;

	clk_t			clk;
	uint64_t		freq;
	uint32_t		cs_max;
};

#define	SFSPI_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	SFSPI_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	SFSPI_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED);
#define	SFSPI_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED);

/*
 * Register offsets.
 * From Sifive-Unleashed-FU540-C000-v1.0.pdf page 101.
 */
#define	SFSPI_REG_SCKDIV	0x00 /* Serial clock divisor */
#define	SFSPI_REG_SCKMODE	0x04 /* Serial clock mode */
#define	SFSPI_REG_CSID		0x10 /* Chip select ID */
#define	SFSPI_REG_CSDEF		0x14 /* Chip select default */
#define	SFSPI_REG_CSMODE	0x18 /* Chip select mode */
#define	SFSPI_REG_DELAY0	0x28 /* Delay control 0 */
#define	SFSPI_REG_DELAY1	0x2C /* Delay control 1 */
#define	SFSPI_REG_FMT		0x40 /* Frame format */
#define	SFSPI_REG_TXDATA	0x48 /* Tx FIFO data */
#define	SFSPI_REG_RXDATA	0x4C /* Rx FIFO data */
#define	SFSPI_REG_TXMARK	0x50 /* Tx FIFO watermark */
#define	SFSPI_REG_RXMARK	0x54 /* Rx FIFO watermark */
#define	SFSPI_REG_FCTRL		0x60 /* SPI flash interface control* */
#define	SFSPI_REG_FFMT		0x64 /* SPI flash instruction format* */
#define	SFSPI_REG_IE		0x70 /* SPI interrupt enable */
#define	SFSPI_REG_IP		0x74 /* SPI interrupt pending */

#define	SFSPI_SCKDIV_MASK	0xfff

#define	SFSPI_CSDEF_ALL		((1 << sc->cs_max)-1)

#define	SFSPI_CSMODE_AUTO	0x0U
#define	SFSPI_CSMODE_HOLD	0x2U
#define	SFSPI_CSMODE_OFF	0x3U

#define	SFSPI_TXDATA_DATA_MASK	0xff
#define	SFSPI_TXDATA_FULL	(1 << 31)

#define	SFSPI_RXDATA_DATA_MASK	0xff
#define	SFSPI_RXDATA_EMPTY	(1 << 31)

#define	SFSPI_SCKMODE_PHA	(1 << 0)
#define	SFSPI_SCKMODE_POL	(1 << 1)

#define	SFSPI_FMT_PROTO_SINGLE	0x0U
#define	SFSPI_FMT_PROTO_DUAL	0x1U
#define	SFSPI_FMT_PROTO_QUAD	0x2U
#define	SFSPI_FMT_PROTO_MASK	0x3U
#define	SFSPI_FMT_ENDIAN	(1 << 2)
#define	SFSPI_FMT_DIR		(1 << 3)
#define	SFSPI_FMT_LEN(x)	((uint32_t)(x) << 16)
#define	SFSPI_FMT_LEN_MASK	(0xfU << 16)

#define	SFSPI_FIFO_DEPTH	8

#define	SFSPI_READ(_sc, _reg)		\
    bus_space_read_4((_sc)->bst, (_sc)->bsh, (_reg))
#define	SFSPI_WRITE(_sc, _reg, _val)	\
    bus_space_write_4((_sc)->bst, (_sc)->bsh, (_reg), (_val))

static void
sfspi_tx(struct sfspi_softc *sc, uint8_t *buf, uint32_t bufsiz)
{
	uint32_t val;
	uint8_t *p, *end;

	KASSERT(buf != NULL, ("TX buffer cannot be NULL"));

	end = buf + bufsiz;
	for (p = buf; p < end; p++) {
		do {
			val = SFSPI_READ(sc, SFSPI_REG_TXDATA);
		} while (val & SFSPI_TXDATA_FULL);
		val = *p;
		SFSPI_WRITE(sc, SFSPI_REG_TXDATA, val);
	}
}

static void
sfspi_rx(struct sfspi_softc *sc, uint8_t *buf, uint32_t bufsiz)
{
	uint32_t val;
	uint8_t *p, *end;

	KASSERT(buf != NULL, ("RX buffer cannot be NULL"));
	KASSERT(bufsiz <= SFSPI_FIFO_DEPTH,
	    ("Cannot receive more than %d bytes at a time\n",
	    SFSPI_FIFO_DEPTH));

	end = buf + bufsiz;
	for (p = buf; p < end; p++) {
		do {
			val = SFSPI_READ(sc, SFSPI_REG_RXDATA);
		} while (val & SFSPI_RXDATA_EMPTY);
		*p = val & SFSPI_RXDATA_DATA_MASK;
	};
}

static int
sfspi_xfer_buf(struct sfspi_softc *sc, uint8_t *rxbuf, uint8_t *txbuf,
    uint32_t txlen, uint32_t rxlen)
{
	uint32_t bytes;

	KASSERT(txlen == rxlen, ("TX and RX lengths must be equal"));
	KASSERT(rxbuf != NULL, ("RX buffer cannot be NULL"));
	KASSERT(txbuf != NULL, ("TX buffer cannot be NULL"));

	while (txlen) {
		bytes = (txlen > SFSPI_FIFO_DEPTH) ? SFSPI_FIFO_DEPTH : txlen;
		sfspi_tx(sc, txbuf, bytes);
		txbuf += bytes;
		sfspi_rx(sc, rxbuf, bytes);
		rxbuf += bytes;
		txlen -= bytes;
	}

	return (0);
}

static int
sfspi_setup(struct sfspi_softc *sc, uint32_t cs, uint32_t mode,
    uint32_t freq)
{
	uint32_t csmode, fmt, sckdiv, sckmode;

	SFSPI_ASSERT_LOCKED(sc);

	/*
	 * Fsck = Fin / 2 * (div + 1)
	 * -> div = Fin / (2 * Fsck) - 1
	 */
	sckdiv = (howmany(sc->freq >> 1, freq) - 1) & SFSPI_SCKDIV_MASK;
	SFSPI_WRITE(sc, SFSPI_REG_SCKDIV, sckdiv);

	switch (mode) {
	case SPIBUS_MODE_NONE:
		sckmode = 0;
		break;
	case SPIBUS_MODE_CPHA:
		sckmode = SFSPI_SCKMODE_PHA;
		break;
	case SPIBUS_MODE_CPOL:
		sckmode = SFSPI_SCKMODE_POL;
		break;
	case SPIBUS_MODE_CPOL_CPHA:
		sckmode = SFSPI_SCKMODE_PHA | SFSPI_SCKMODE_POL;
		break;
	default:
		return (EINVAL);
	}
	SFSPI_WRITE(sc, SFSPI_REG_SCKMODE, sckmode);

	csmode = SFSPI_CSMODE_HOLD;
	if (cs & SPIBUS_CS_HIGH)
		csmode = SFSPI_CSMODE_AUTO;
	SFSPI_WRITE(sc, SFSPI_REG_CSMODE, csmode);

	SFSPI_WRITE(sc, SFSPI_REG_CSID, cs & ~SPIBUS_CS_HIGH);

	fmt = SFSPI_FMT_PROTO_SINGLE | SFSPI_FMT_LEN(8);
	SFSPI_WRITE(sc, SFSPI_REG_FMT, fmt);

	return (0);
}

static int
sfspi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct sfspi_softc *sc;
	uint32_t clock, cs, csdef, mode;
	int err;

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz,
	    ("TX and RX command sizes must be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz,
	    ("TX and RX data sizes must be equal"));

	sc = device_get_softc(dev);
	spibus_get_cs(child, &cs);
	spibus_get_clock(child, &clock);
	spibus_get_mode(child, &mode);

	if (cs > sc->cs_max) {
		device_printf(sc->dev, "Invalid chip select %u\n", cs);
		return (EINVAL);
	}

	SFSPI_LOCK(sc);
	device_busy(sc->dev);

	err = sfspi_setup(sc, cs, mode, clock);
	if (err != 0) {
		SFSPI_UNLOCK(sc);
		return (err);
	}

	err = 0;
	if (cmd->tx_cmd_sz > 0)
		err = sfspi_xfer_buf(sc, cmd->rx_cmd, cmd->tx_cmd,
		    cmd->tx_cmd_sz, cmd->rx_cmd_sz);
	if (cmd->tx_data_sz > 0 && err == 0)
		err = sfspi_xfer_buf(sc, cmd->rx_data, cmd->tx_data,
		    cmd->tx_data_sz, cmd->rx_data_sz);

	/* Deassert chip select. */
	csdef = SFSPI_CSDEF_ALL & ~(1 << cs);
	SFSPI_WRITE(sc, SFSPI_REG_CSDEF, csdef);
	SFSPI_WRITE(sc, SFSPI_REG_CSDEF, SFSPI_CSDEF_ALL);

	device_unbusy(sc->dev);
	SFSPI_UNLOCK(sc);

	return (err);
}

static int
sfspi_attach(device_t dev)
{
	struct sfspi_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	error = bus_alloc_resources(dev, sfspi_spec, &sc->res);
	if (error) {
		device_printf(dev, "Couldn't allocate resources\n");
		goto fail;
	}
	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	error = clk_get_by_ofw_index(dev, 0, 0, &sc->clk);
	if (error) {
		device_printf(dev, "Couldn't allocate clock: %d\n", error);
		goto fail;
	}
	error = clk_enable(sc->clk);
	if (error) {
		device_printf(dev, "Couldn't enable clock: %d\n", error);
		goto fail;
	}

	error = clk_get_freq(sc->clk, &sc->freq);
	if (error) {
		device_printf(sc->dev, "Couldn't get frequency: %d\n", error);
		goto fail;
	}

	/*
	 * From Sifive-Unleashed-FU540-C000-v1.0.pdf page 103:
	 * csdef is cs_width bits wide and all ones on reset.
	 */
	sc->cs_max = SFSPI_READ(sc, SFSPI_REG_CSDEF);

	/*
	 * We don't support the direct-mapped flash interface.
	 * Disable it.
	 */
	SFSPI_WRITE(sc, SFSPI_REG_FCTRL, 0x0);

	/* Probe and attach the spibus when interrupts are available. */
	sc->parent = device_add_child(dev, "spibus", DEVICE_UNIT_ANY);
	bus_delayed_attach_children(dev);

	return (0);

fail:
	bus_release_resources(dev, sfspi_spec, &sc->res);
	mtx_destroy(&sc->mtx);
	return (error);
}

static int
sfspi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "sifive,spi0"))
		return (ENXIO);

	device_set_desc(dev, "SiFive SPI controller");

	return (BUS_PROBE_DEFAULT);
}

static phandle_t
sfspi_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static device_method_t sfspi_methods[] = {
	DEVMETHOD(device_probe,		sfspi_probe),
	DEVMETHOD(device_attach,	sfspi_attach),

	DEVMETHOD(spibus_transfer,	sfspi_transfer),

	DEVMETHOD(ofw_bus_get_node,	sfspi_get_node),

	DEVMETHOD_END
};

static driver_t sfspi_driver = {
	"sifive_spi",
	sfspi_methods,
	sizeof(struct sfspi_softc)
};

DRIVER_MODULE(sifive_spi, simplebus, sfspi_driver, 0, 0);
DRIVER_MODULE(ofw_spibus, sifive_spi, ofw_spibus_driver, 0, 0);
MODULE_DEPEND(sifive_spi, ofw_spibus, 1, 1, 1);
