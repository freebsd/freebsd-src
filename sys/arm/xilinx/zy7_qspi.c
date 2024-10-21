/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/cdefs.h>
/*
 * This is a driver for the Quad-SPI Flash Controller in the Xilinx
 * Zynq-7000 SoC.
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

#include <dev/flash/mx25lreg.h>

#include "spibus_if.h"

static struct ofw_compat_data compat_data[] = {
	{"xlnx,zy7_qspi",		1},
	{"xlnx,zynq-qspi-1.0",		1},
	{NULL,				0}
};

struct zy7_qspi_softc {
	device_t		dev;
	device_t		child;
	struct mtx		sc_mtx;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*intrhandle;

	uint32_t		cfg_reg_shadow;
	uint32_t		lqspi_cfg_shadow;
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
	int			is_dual;
	int			is_stacked;
	int			is_dio;
};

#define ZY7_QSPI_DEFAULT_SPI_CLOCK	50000000

#define QSPI_SC_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	QSPI_SC_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define QSPI_SC_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	NULL, MTX_DEF)
#define QSPI_SC_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->sc_mtx)
#define QSPI_SC_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

#define RD4(sc, off)		(bus_read_4((sc)->mem_res, (off)))
#define WR4(sc, off, val)	(bus_write_4((sc)->mem_res, (off), (val)))

/*
 * QSPI device registers.
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.12.2) July 1, 2018.  Xilinx doc UG585.
 */
#define ZY7_QSPI_CONFIG_REG		0x0000
#define   ZY7_QSPI_CONFIG_IFMODE		(1U << 31)
#define   ZY7_QSPI_CONFIG_ENDIAN		(1 << 26)
#define   ZY7_QSPI_CONFIG_HOLDB_DR		(1 << 19)
#define   ZY7_QSPI_CONFIG_RSVD1			(1 << 17) /* must be 1 */
#define   ZY7_QSPI_CONFIG_MANSTRT		(1 << 16)
#define   ZY7_QSPI_CONFIG_MANSTRTEN		(1 << 15)
#define   ZY7_QSPI_CONFIG_SSFORCE		(1 << 14)
#define   ZY7_QSPI_CONFIG_PCS			(1 << 10)
#define   ZY7_QSPI_CONFIG_REF_CLK		(1 << 8)
#define   ZY7_QSPI_CONFIG_FIFO_WIDTH_MASK	(3 << 6)
#define   ZY7_QSPI_CONFIG_FIFO_WIDTH32		(3 << 6)
#define   ZY7_QSPI_CONFIG_BAUD_RATE_DIV_MASK	(7 << 3)
#define   ZY7_QSPI_CONFIG_BAUD_RATE_DIV_SHIFT	3
#define   ZY7_QSPI_CONFIG_BAUD_RATE_DIV(x)	((x) << 3) /* divide by 2<<x */
#define   ZY7_QSPI_CONFIG_CLK_PH		(1 << 2)   /* clock phase */
#define   ZY7_QSPI_CONFIG_CLK_POL		(1 << 1)   /* clock polarity */
#define   ZY7_QSPI_CONFIG_MODE_SEL		(1 << 0)   /* master enable */

#define ZY7_QSPI_INTR_STAT_REG		0x0004
#define ZY7_QSPI_INTR_EN_REG		0x0008
#define ZY7_QSPI_INTR_DIS_REG		0x000c
#define ZY7_QSPI_INTR_MASK_REG		0x0010
#define   ZY7_QSPI_INTR_TX_FIFO_UNDERFLOW	(1 << 6)
#define   ZY7_QSPI_INTR_RX_FIFO_FULL		(1 << 5)
#define   ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY	(1 << 4)
#define   ZY7_QSPI_INTR_TX_FIFO_FULL		(1 << 3)
#define   ZY7_QSPI_INTR_TX_FIFO_NOT_FULL	(1 << 2)
#define   ZY7_QSPI_INTR_RX_OVERFLOW		(1 << 0)

#define ZY7_QSPI_EN_REG			0x0014
#define   ZY7_SPI_ENABLE			1

#define ZY7_QSPI_DELAY_REG		0x0018
#define   ZY7_QSPI_DELAY_NSS_MASK		(0xffU << 24)
#define   ZY7_QSPI_DELAY_NSS_SHIFT		24
#define   ZY7_QSPI_DELAY_NSS(x)			((x) << 24)
#define   ZY7_QSPI_DELAY_BTWN_MASK		(0xff << 16)
#define   ZY7_QSPI_DELAY_BTWN_SHIFT		16
#define   ZY7_QSPI_DELAY_BTWN(x)		((x) << 16)
#define   ZY7_QSPI_DELAY_AFTER_MASK		(0xff << 8)
#define   ZY7_QSPI_DELAY_AFTER_SHIFT		8
#define   ZY7_QSPI_DELAY_AFTER(x)		((x) << 8)
#define   ZY7_QSPI_DELAY_INIT_MASK		0xff
#define   ZY7_QSPI_DELAY_INIT_SHIFT		0
#define   ZY7_QSPI_DELAY_INIT(x)		(x)

#define ZY7_QSPI_TXD0_REG		0x001c
#define ZY7_QSPI_RX_DATA_REG		0x0020

#define ZY7_QSPI_SLV_IDLE_CT_REG	0x0024
#define   ZY7_QSPI_SLV_IDLE_CT_MASK		0xff

#define ZY7_QSPI_TX_THRESH_REG		0x0028
#define ZY7_QSPI_RX_THRESH_REG		0x002c

#define ZY7_QSPI_GPIO_REG		0x0030
#define   ZY7_QSPI_GPIO_WP_N			1

#define ZY7_QSPI_LPBK_DLY_ADJ_REG	0x0038
#define   ZY7_QSPI_LPBK_DLY_ADJ_LPBK_SEL	(1 << 8)
#define   ZY7_QSPI_LPBK_DLY_ADJ_LPBK_PH		(1 << 7)
#define   ZY7_QSPI_LPBK_DLY_ADJ_USE_LPBK	(1 << 5)
#define   ZY7_QSPI_LPBK_DLY_ADJ_DLY1_MASK	(3 << 3)
#define   ZY7_QSPI_LPBK_DLY_ADJ_DLY1_SHIFT	3
#define   ZY7_QSPI_LPBK_DLY_ADJ_DLY1(x)		((x) << 3)
#define   ZY7_QSPI_LPBK_DLY_ADJ_DLY0_MASK	7
#define   ZY7_QSPI_LPBK_DLY_ADJ_DLY0_SHIFT	0
#define   ZY7_QSPI_LPBK_DLY_ADJ_DLY0(x)		(x)

#define ZY7_QSPI_TXD1_REG		0x0080
#define ZY7_QSPI_TXD2_REG		0x0084
#define ZY7_QSPI_TXD3_REG		0x0088

#define ZY7_QSPI_LQSPI_CFG_REG		0x00a0
#define   ZY7_QSPI_LQSPI_CFG_LINEAR		(1U << 31)
#define   ZY7_QSPI_LQSPI_CFG_TWO_MEM		(1 << 30)
#define   ZY7_QSPI_LQSPI_CFG_SEP_BUS		(1 << 29)
#define   ZY7_QSPI_LQSPI_CFG_U_PAGE		(1 << 28)
#define   ZY7_QSPI_LQSPI_CFG_MODE_EN		(1 << 25)
#define   ZY7_QSPI_LQSPI_CFG_MODE_ON		(1 << 24)
#define   ZY7_QSPI_LQSPI_CFG_MODE_BITS_MASK	(0xff << 16)
#define   ZY7_QSPI_LQSPI_CFG_MODE_BITS_SHIFT	16
#define   ZY7_QSPI_LQSPI_CFG_MODE_BITS(x)	((x) << 16)
#define   ZY7_QSPI_LQSPI_CFG_DUMMY_BYTES_MASK	(7 << 8)
#define   ZY7_QSPI_LQSPI_CFG_DUMMY_BYTES_SHIFT	8
#define   ZY7_QSPI_LQSPI_CFG_DUMMY_BYTES(x)	((x) << 8)
#define   ZY7_QSPI_LQSPI_CFG_INST_CODE_MASK	0xff
#define   ZY7_QSPI_LQSPI_CFG_INST_CODE_SHIFT	0
#define   ZY7_QSPI_LQSPI_CFG_INST_CODE(x)	(x)

#define ZY7_QSPI_LQSPI_STS_REG		0x00a4
#define   ZY7_QSPI_LQSPI_STS_D_FSM_ERR		(1 << 2)
#define   ZY7_QSPI_LQSPI_STS_WR_RECVD		(1 << 1)

#define ZY7_QSPI_MOD_ID_REG		0x00fc

static int zy7_qspi_detach(device_t);

/* Fill hardware fifo with command and data bytes. */
static void
zy7_qspi_write_fifo(struct zy7_qspi_softc *sc, int nbytes)
{
	int n, nvalid;
	uint32_t data;

	while (nbytes > 0) {
		nvalid = MIN(4, nbytes);
		data = 0xffffffff;

		/*
		 * A hardware bug forces us to wait until the tx fifo is
		 * empty before writing partial words.  We'll come back
		 * next tx interrupt.
		 */
		if (nvalid < 4 && (RD4(sc, ZY7_QSPI_INTR_STAT_REG) &
		    ZY7_QSPI_INTR_TX_FIFO_NOT_FULL) == 0)
			return;

		if (sc->tx_bytes_sent < sc->cmd->tx_cmd_sz) {
			/* Writing command. */
			n = MIN(nvalid, sc->cmd->tx_cmd_sz -
			    sc->tx_bytes_sent);
			memcpy(&data, (uint8_t *)sc->cmd->tx_cmd +
			    sc->tx_bytes_sent, n);

			if (nvalid > n) {
				/* Writing start of data. */
				memcpy((uint8_t *)&data + n,
				    sc->cmd->tx_data, nvalid - n);
			}
		} else
			/* Writing data. */
			memcpy(&data, (uint8_t *)sc->cmd->tx_data +
			    (sc->tx_bytes_sent - sc->cmd->tx_cmd_sz), nvalid);

		switch (nvalid) {
		case 1:
			WR4(sc, ZY7_QSPI_TXD1_REG, data);
			break;
		case 2:
			WR4(sc, ZY7_QSPI_TXD2_REG, data);
			break;
		case 3:
			WR4(sc, ZY7_QSPI_TXD3_REG, data);
			break;
		case 4:
			WR4(sc, ZY7_QSPI_TXD0_REG, data);
			break;
		}

		sc->tx_bytes_sent += nvalid;
		nbytes -= nvalid;
	}
}

/* Read hardware fifo data into command response and data buffers. */
static void
zy7_qspi_read_fifo(struct zy7_qspi_softc *sc)
{
	int n, nbytes;
	uint32_t data;

	do {
		data = RD4(sc, ZY7_QSPI_RX_DATA_REG);
		nbytes = MIN(4, sc->rx_bytes - sc->rx_bytes_rcvd);

		/*
		 * Last word in non-word-multiple transfer is packed
		 * non-intuitively.
		 */
		if (nbytes < 4)
			data >>= 8 * (4 - nbytes);

		if (sc->rx_bytes_rcvd < sc->cmd->rx_cmd_sz) {
			/* Reading command. */
			n = MIN(nbytes, sc->cmd->rx_cmd_sz -
			    sc->rx_bytes_rcvd);
			memcpy((uint8_t *)sc->cmd->rx_cmd + sc->rx_bytes_rcvd,
			    &data, n);
			sc->rx_bytes_rcvd += n;
			nbytes -= n;
			data >>= 8 * n;
		}

		if (nbytes > 0) {
			/* Reading data. */
			memcpy((uint8_t *)sc->cmd->rx_data +
			    (sc->rx_bytes_rcvd - sc->cmd->rx_cmd_sz),
			    &data, nbytes);
			sc->rx_bytes_rcvd += nbytes;
		}

	} while (sc->rx_bytes_rcvd < sc->rx_bytes &&
		 (RD4(sc, ZY7_QSPI_INTR_STAT_REG) &
		  ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY) != 0);
}

/* End a transfer early by draining rx fifo and disabling interrupts. */
static void
zy7_qspi_abort_transfer(struct zy7_qspi_softc *sc)
{
	/* Drain receive fifo. */
	while ((RD4(sc, ZY7_QSPI_INTR_STAT_REG) &
		ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY) != 0)
		(void)RD4(sc, ZY7_QSPI_RX_DATA_REG);

	/* Shut down interrupts. */
	WR4(sc, ZY7_QSPI_INTR_DIS_REG,
	    ZY7_QSPI_INTR_RX_OVERFLOW |
	    ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY |
	    ZY7_QSPI_INTR_TX_FIFO_NOT_FULL);
}

static void
zy7_qspi_intr(void *arg)
{
	struct zy7_qspi_softc *sc = (struct zy7_qspi_softc *)arg;
	uint32_t istatus;

	QSPI_SC_LOCK(sc);

	sc->interrupts++;

	istatus = RD4(sc, ZY7_QSPI_INTR_STAT_REG);

	/* Stray interrupts can happen if a transfer gets interrupted. */
	if (!sc->busy) {
		sc->stray_ints++;
		QSPI_SC_UNLOCK(sc);
		return;
	}

	if ((istatus & ZY7_QSPI_INTR_RX_OVERFLOW) != 0) {
		device_printf(sc->dev, "rx fifo overflow!\n");
		sc->rx_overflows++;

		/* Clear status bit. */
		WR4(sc, ZY7_QSPI_INTR_STAT_REG,
		    ZY7_QSPI_INTR_RX_OVERFLOW);
	}

	/* Empty receive fifo before any more transmit data is sent. */
	if (sc->rx_bytes_rcvd < sc->rx_bytes &&
	    (istatus & ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY) != 0) {
		zy7_qspi_read_fifo(sc);
		if (sc->rx_bytes_rcvd == sc->rx_bytes)
			/* Disable receive interrupts. */
			WR4(sc, ZY7_QSPI_INTR_DIS_REG,
			    ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY |
			    ZY7_QSPI_INTR_RX_OVERFLOW);
	}

	/*
	 * Transmit underflows aren't really a bug because a hardware
	 * bug forces us to allow the tx fifo to go empty between full
	 * and partial fifo writes.  Why bother counting?
	 */
	if ((istatus & ZY7_QSPI_INTR_TX_FIFO_UNDERFLOW) != 0) {
		sc->tx_underflows++;

		/* Clear status bit. */
		WR4(sc, ZY7_QSPI_INTR_STAT_REG,
		    ZY7_QSPI_INTR_TX_FIFO_UNDERFLOW);
	}

	/* Fill transmit fifo. */
	if (sc->tx_bytes_sent < sc->tx_bytes &&
	    (istatus & ZY7_QSPI_INTR_TX_FIFO_NOT_FULL) != 0) {
		zy7_qspi_write_fifo(sc, MIN(240, sc->tx_bytes -
			sc->tx_bytes_sent));

		if (sc->tx_bytes_sent == sc->tx_bytes) {
			/*
			 * Disable transmit FIFO interrupt, enable receive
			 * FIFO interrupt.
			 */
			WR4(sc, ZY7_QSPI_INTR_DIS_REG,
			    ZY7_QSPI_INTR_TX_FIFO_NOT_FULL);
			WR4(sc, ZY7_QSPI_INTR_EN_REG,
			    ZY7_QSPI_INTR_RX_FIFO_NOT_EMPTY);
		}
	}

	/* Finished with transfer? */
	if (sc->tx_bytes_sent == sc->tx_bytes &&
	    sc->rx_bytes_rcvd == sc->rx_bytes) {
		/* De-assert CS. */
		sc->cfg_reg_shadow |= ZY7_QSPI_CONFIG_PCS;
		WR4(sc, ZY7_QSPI_CONFIG_REG, sc->cfg_reg_shadow);

		wakeup(sc->dev);
	}

	QSPI_SC_UNLOCK(sc);
}

/* Initialize hardware. */
static int
zy7_qspi_init_hw(struct zy7_qspi_softc *sc)
{
	uint32_t baud_div;

	/* Configure LQSPI Config register.  Disable linear mode. */
	sc->lqspi_cfg_shadow = RD4(sc, ZY7_QSPI_LQSPI_CFG_REG);
	sc->lqspi_cfg_shadow &= ~(ZY7_QSPI_LQSPI_CFG_LINEAR |
				  ZY7_QSPI_LQSPI_CFG_TWO_MEM |
				  ZY7_QSPI_LQSPI_CFG_SEP_BUS);
	if (sc->is_dual) {
		sc->lqspi_cfg_shadow |= ZY7_QSPI_LQSPI_CFG_TWO_MEM;
		if (sc->is_stacked) {
			sc->lqspi_cfg_shadow &=
			    ~ZY7_QSPI_LQSPI_CFG_INST_CODE_MASK;
			sc->lqspi_cfg_shadow |=
			    ZY7_QSPI_LQSPI_CFG_INST_CODE(sc->is_dio ?
				CMD_READ_DUAL_IO : CMD_READ_QUAD_OUTPUT);
		} else
			sc->lqspi_cfg_shadow |= ZY7_QSPI_LQSPI_CFG_SEP_BUS;
	}
	WR4(sc, ZY7_QSPI_LQSPI_CFG_REG, sc->lqspi_cfg_shadow);

	/* Find best clock divider. */
	baud_div = 0;
	while ((sc->ref_clock >> (baud_div + 1)) > sc->spi_clock &&
	       baud_div < 8)
		baud_div++;
	if (baud_div >= 8) {
		device_printf(sc->dev, "cannot configure clock divider: ref=%d"
		    " spi=%d.\n", sc->ref_clock, sc->spi_clock);
		return (EINVAL);
	}
	sc->spi_clk_real_freq = sc->ref_clock >> (baud_div + 1);

	/*
	 * If divider is 2 (the max speed), use internal loopback master
	 * clock for read data.  (See section 12.3.1 in ref man.)
	 */
	if (baud_div == 0)
		WR4(sc, ZY7_QSPI_LPBK_DLY_ADJ_REG,
		    ZY7_QSPI_LPBK_DLY_ADJ_USE_LPBK |
		    ZY7_QSPI_LPBK_DLY_ADJ_DLY1(0) |
		    ZY7_QSPI_LPBK_DLY_ADJ_DLY0(0));
	else
		WR4(sc, ZY7_QSPI_LPBK_DLY_ADJ_REG, 0);

	/* Set up configuration register. */
	sc->cfg_reg_shadow =
		ZY7_QSPI_CONFIG_IFMODE |
		ZY7_QSPI_CONFIG_HOLDB_DR |
		ZY7_QSPI_CONFIG_RSVD1 |
		ZY7_QSPI_CONFIG_SSFORCE |
		ZY7_QSPI_CONFIG_PCS |
		ZY7_QSPI_CONFIG_FIFO_WIDTH32 |
		ZY7_QSPI_CONFIG_BAUD_RATE_DIV(baud_div) |
		ZY7_QSPI_CONFIG_MODE_SEL;
	WR4(sc, ZY7_QSPI_CONFIG_REG, sc->cfg_reg_shadow);

	/*
	 * Set thresholds.  We must use 1 for tx threshold because there
	 * is no fifo empty flag and we need one to implement a bug
	 * workaround.
	 */
	WR4(sc, ZY7_QSPI_TX_THRESH_REG, 1);
	WR4(sc, ZY7_QSPI_RX_THRESH_REG, 1);

	/* Clear and disable all interrupts. */
	WR4(sc, ZY7_QSPI_INTR_STAT_REG, ~0);
	WR4(sc, ZY7_QSPI_INTR_DIS_REG, ~0);

	/* Enable SPI. */
	WR4(sc, ZY7_QSPI_EN_REG, ZY7_SPI_ENABLE);

	return (0);
}

static void
zy7_qspi_add_sysctls(device_t dev)
{
	struct zy7_qspi_softc *sc = device_get_softc(dev);
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
zy7_qspi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Zynq Quad-SPI Flash Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
zy7_qspi_attach(device_t dev)
{
	struct zy7_qspi_softc *sc;
	int rid, err;
	phandle_t node;
	pcell_t cell;

	sc = device_get_softc(dev);
	sc->dev = dev;

	QSPI_SC_LOCK_INIT(sc);

	/* Get ref-clock, spi-clock, and other properties. */
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
		sc->spi_clock = ZY7_QSPI_DEFAULT_SPI_CLOCK;
	if (OF_getprop(node, "is-stacked", &cell, sizeof(cell)) > 0 &&
	    fdt32_to_cpu(cell) != 0) {
		sc->is_dual = 1;
		sc->is_stacked = 1;
	} else if (OF_getprop(node, "is-dual", &cell, sizeof(cell)) > 0 &&
		   fdt32_to_cpu(cell) != 0)
		sc->is_dual = 1;
	if (OF_getprop(node, "is-dio", &cell, sizeof(cell)) > 0 &&
	    fdt32_to_cpu(cell) != 0)
		sc->is_dio = 1;

	/* Get memory resource. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		zy7_qspi_detach(dev);
		return (ENOMEM);
	}

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate IRQ resource.\n");
		zy7_qspi_detach(dev);
		return (ENOMEM);
	}

	/* Activate the interrupt. */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, zy7_qspi_intr, sc, &sc->intrhandle);
	if (err) {
		device_printf(dev, "could not setup IRQ.\n");
		zy7_qspi_detach(dev);
		return (err);
	}

	/* Configure the device. */
	err = zy7_qspi_init_hw(sc);
	if (err) {
		zy7_qspi_detach(dev);
		return (err);
	}

	sc->child = device_add_child(dev, "spibus", DEVICE_UNIT_ANY);

	zy7_qspi_add_sysctls(dev);

	/* Attach spibus driver as a child later when interrupts work. */
	bus_delayed_attach_children(dev);

	return (0);
}

static int
zy7_qspi_detach(device_t dev)
{
	struct zy7_qspi_softc *sc = device_get_softc(dev);

	if (device_is_attached(dev))
		bus_generic_detach(dev);

	/* Delete child bus. */
	if (sc->child)
		device_delete_child(dev, sc->child);

	/* Disable hardware. */
	if (sc->mem_res != NULL) {
		/* Disable SPI. */
		WR4(sc, ZY7_QSPI_EN_REG, 0);

		/* Clear and disable all interrupts. */
		WR4(sc, ZY7_QSPI_INTR_STAT_REG, ~0);
		WR4(sc, ZY7_QSPI_INTR_DIS_REG, ~0);
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

	QSPI_SC_LOCK_DESTROY(sc);

	return (0);
}

static phandle_t
zy7_qspi_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static int
zy7_qspi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct zy7_qspi_softc *sc = device_get_softc(dev);
	int err = 0;

	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz,
	    ("TX/RX command sizes should be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz,
	    ("TX/RX data sizes should be equal"));

	if (sc->is_dual && cmd->tx_data_sz % 2 != 0) {
		device_printf(dev, "driver does not support odd byte data "
		    "transfers in dual mode. (sz=%d)\n", cmd->tx_data_sz);
		return (EINVAL);
	}

	QSPI_SC_LOCK(sc);

	/* Wait for controller available. */
	while (sc->busy != 0) {
		err = mtx_sleep(dev, &sc->sc_mtx, 0, "zqspi0", 0);
		if (err) {
			QSPI_SC_UNLOCK(sc);
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

	/* Enable interrupts.  zy7_qspi_intr() will handle transfer. */
	WR4(sc, ZY7_QSPI_INTR_EN_REG,
	    ZY7_QSPI_INTR_TX_FIFO_NOT_FULL |
	    ZY7_QSPI_INTR_RX_OVERFLOW);

#ifdef SPI_XFER_U_PAGE	/* XXX: future support for stacked memories. */
	if (sc->is_stacked) {
		if ((cmd->flags & SPI_XFER_U_PAGE) != 0)
			sc->lqspi_cfg_shadow |= ZY7_QSPI_LQSPI_CFG_U_PAGE;
		else
			sc->lqspi_cfg_shadow &= ~ZY7_QSPI_LQSPI_CFG_U_PAGE;
		WR4(sc, ZY7_QSPI_LQSPI_CFG_REG, sc->lqspi_cfg_shadow);
	}
#endif

	/* Assert CS. */
	sc->cfg_reg_shadow &= ~ZY7_QSPI_CONFIG_PCS;
	WR4(sc, ZY7_QSPI_CONFIG_REG, sc->cfg_reg_shadow);

	/* Wait for completion. */
	err = mtx_sleep(dev, &sc->sc_mtx, 0, "zqspi1", hz * 2);
	if (err)
		zy7_qspi_abort_transfer(sc);

	/* Release controller. */
	sc->busy = 0;
	wakeup_one(dev);

	QSPI_SC_UNLOCK(sc);

	return (err);
}

static device_method_t zy7_qspi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		zy7_qspi_probe),
	DEVMETHOD(device_attach,	zy7_qspi_attach),
	DEVMETHOD(device_detach,	zy7_qspi_detach),

	/* SPI interface */
	DEVMETHOD(spibus_transfer,	zy7_qspi_transfer),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	zy7_qspi_get_node),

	DEVMETHOD_END
};

static driver_t zy7_qspi_driver = {
	"zy7_qspi",
	zy7_qspi_methods,
	sizeof(struct zy7_qspi_softc),
};

DRIVER_MODULE(zy7_qspi, simplebus, zy7_qspi_driver, 0, 0);
DRIVER_MODULE(ofw_spibus, zy7_qspi, ofw_spibus_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
MODULE_DEPEND(zy7_qspi, ofw_spibus, 1, 1, 1);
