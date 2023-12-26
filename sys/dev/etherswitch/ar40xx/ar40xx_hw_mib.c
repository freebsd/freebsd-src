/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
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
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mdio/mdio.h>
#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_mib.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"


#define MIB_DESC(_s , _o, _n)   \
	{		       \
		.size = (_s),   \
		.offset = (_o), \
		.name = (_n),   \
	}

static const struct ar40xx_mib_desc ar40xx_mibs[] = {
	MIB_DESC(1, AR40XX_STATS_RXBROAD, "RxBroad"),
	MIB_DESC(1, AR40XX_STATS_RXPAUSE, "RxPause"),
	MIB_DESC(1, AR40XX_STATS_RXMULTI, "RxMulti"),
	MIB_DESC(1, AR40XX_STATS_RXFCSERR, "RxFcsErr"),
	MIB_DESC(1, AR40XX_STATS_RXALIGNERR, "RxAlignErr"),
	MIB_DESC(1, AR40XX_STATS_RXRUNT, "RxRunt"),
	MIB_DESC(1, AR40XX_STATS_RXFRAGMENT, "RxFragment"),
	MIB_DESC(1, AR40XX_STATS_RX64BYTE, "Rx64Byte"),
	MIB_DESC(1, AR40XX_STATS_RX128BYTE, "Rx128Byte"),
	MIB_DESC(1, AR40XX_STATS_RX256BYTE, "Rx256Byte"),
	MIB_DESC(1, AR40XX_STATS_RX512BYTE, "Rx512Byte"),
	MIB_DESC(1, AR40XX_STATS_RX1024BYTE, "Rx1024Byte"),
	MIB_DESC(1, AR40XX_STATS_RX1518BYTE, "Rx1518Byte"),
	MIB_DESC(1, AR40XX_STATS_RXMAXBYTE, "RxMaxByte"),
	MIB_DESC(1, AR40XX_STATS_RXTOOLONG, "RxTooLong"),
	MIB_DESC(2, AR40XX_STATS_RXGOODBYTE, "RxGoodByte"),
	MIB_DESC(2, AR40XX_STATS_RXBADBYTE, "RxBadByte"),
	MIB_DESC(1, AR40XX_STATS_RXOVERFLOW, "RxOverFlow"),
	MIB_DESC(1, AR40XX_STATS_FILTERED, "Filtered"),
	MIB_DESC(1, AR40XX_STATS_TXBROAD, "TxBroad"),
	MIB_DESC(1, AR40XX_STATS_TXPAUSE, "TxPause"),
	MIB_DESC(1, AR40XX_STATS_TXMULTI, "TxMulti"),
	MIB_DESC(1, AR40XX_STATS_TXUNDERRUN, "TxUnderRun"),
	MIB_DESC(1, AR40XX_STATS_TX64BYTE, "Tx64Byte"),
	MIB_DESC(1, AR40XX_STATS_TX128BYTE, "Tx128Byte"),
	MIB_DESC(1, AR40XX_STATS_TX256BYTE, "Tx256Byte"),
	MIB_DESC(1, AR40XX_STATS_TX512BYTE, "Tx512Byte"),
	MIB_DESC(1, AR40XX_STATS_TX1024BYTE, "Tx1024Byte"),
	MIB_DESC(1, AR40XX_STATS_TX1518BYTE, "Tx1518Byte"),
	MIB_DESC(1, AR40XX_STATS_TXMAXBYTE, "TxMaxByte"),
	MIB_DESC(1, AR40XX_STATS_TXOVERSIZE, "TxOverSize"),
	MIB_DESC(2, AR40XX_STATS_TXBYTE, "TxByte"),
	MIB_DESC(1, AR40XX_STATS_TXCOLLISION, "TxCollision"),
	MIB_DESC(1, AR40XX_STATS_TXABORTCOL, "TxAbortCol"),
	MIB_DESC(1, AR40XX_STATS_TXMULTICOL, "TxMultiCol"),
	MIB_DESC(1, AR40XX_STATS_TXSINGLECOL, "TxSingleCol"),
	MIB_DESC(1, AR40XX_STATS_TXEXCDEFER, "TxExcDefer"),
	MIB_DESC(1, AR40XX_STATS_TXDEFER, "TxDefer"),
	MIB_DESC(1, AR40XX_STATS_TXLATECOL, "TxLateCol"),
};


int
ar40xx_hw_mib_op(struct ar40xx_softc *sc, uint32_t op)
{
	uint32_t reg;
	int ret;

	AR40XX_LOCK_ASSERT(sc);

	/* Trigger capturing statistics on all ports */
	AR40XX_REG_BARRIER_READ(sc);
	reg = AR40XX_REG_READ(sc, AR40XX_REG_MIB_FUNC);
	reg &= ~AR40XX_MIB_FUNC;
	reg |= (op << AR40XX_MIB_FUNC_S);
	AR40XX_REG_WRITE(sc, AR40XX_REG_MIB_FUNC, reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	/* Now wait */
	ret = ar40xx_hw_wait_bit(sc, AR40XX_REG_MIB_FUNC,
	    AR40XX_MIB_BUSY, 0);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "%s: ERROR: timeout waiting for MIB load\n",
		    __func__);
	}

	return ret;
}

int
ar40xx_hw_mib_capture(struct ar40xx_softc *sc)
{
	int ret;

	ret = ar40xx_hw_mib_op(sc, AR40XX_MIB_FUNC_CAPTURE);
	return (ret);
}

int
ar40xx_hw_mib_flush(struct ar40xx_softc *sc)
{
	int ret;

	ret = ar40xx_hw_mib_op(sc, AR40XX_MIB_FUNC_FLUSH);
	return (ret);
}

int
ar40xx_hw_mib_fetch(struct ar40xx_softc *sc, int port)
{
	uint64_t val;
	uint32_t base, reg;
	int i;

	base = AR40XX_REG_PORT_STATS_START
	    + (AR40XX_REG_PORT_STATS_LEN * port);

	/* For now just print them out, we'll store them later */
	AR40XX_REG_BARRIER_READ(sc);
	for (i = 0; i < nitems(ar40xx_mibs); i++) {
		val = 0;

		val = AR40XX_REG_READ(sc, base + ar40xx_mibs[i].offset);
		if (ar40xx_mibs[i].size == 2) {
			reg = AR40XX_REG_READ(sc, base + ar40xx_mibs[i].offset + 4);
			val |= ((uint64_t) reg << 32);
		}

		device_printf(sc->sc_dev, "%s[%d] = %llu\n", ar40xx_mibs[i].name, port, val);
	}

	return (0);
}
