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
#include <dev/etherswitch/ar40xx/ar40xx_hw_atu.h>
#include <dev/etherswitch/ar40xx/ar40xx_debug.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

int
ar40xx_hw_atu_wait_busy(struct ar40xx_softc *sc)
{
	int ret;

	ret = ar40xx_hw_wait_bit(sc, AR40XX_REG_ATU_FUNC,
	    AR40XX_ATU_FUNC_BUSY, 0);
	return (ret);
}

int
ar40xx_hw_atu_flush_all(struct ar40xx_softc *sc)
{
	int ret;

	AR40XX_LOCK_ASSERT(sc);

	AR40XX_DPRINTF(sc, AR40XX_DBG_ATU_OP, "%s: called\n", __func__);
	ret = ar40xx_hw_atu_wait_busy(sc);
	if (ret != 0)
		return (ret);

	AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_FUNC,
	    AR40XX_ATU_FUNC_OP_FLUSH
	    | AR40XX_ATU_FUNC_BUSY);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (ret);
}

int
ar40xx_hw_atu_flush_port(struct ar40xx_softc *sc, int port)
{
	uint32_t val;
	int ret;

	AR40XX_LOCK_ASSERT(sc);

	AR40XX_DPRINTF(sc, AR40XX_DBG_ATU_OP, "%s: called, port=%d\n",
	     __func__, port);

	if (port >= AR40XX_NUM_PORTS) {
		return (EINVAL);
	}

	ret = ar40xx_hw_atu_wait_busy(sc);
	if (ret != 0)
		return (ret);

	val = AR40XX_ATU_FUNC_OP_FLUSH_UNICAST;
	val |= (port << AR40XX_ATU_FUNC_PORT_NUM_S)
	    & AR40XX_ATU_FUNC_PORT_NUM;

	AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_FUNC,
	    val | AR40XX_ATU_FUNC_BUSY);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

int
ar40xx_hw_atu_fetch_entry(struct ar40xx_softc *sc, etherswitch_atu_entry_t *e,
    int atu_fetch_op)
{
	uint32_t ret0, ret1, ret2, val;
	int ret;

	AR40XX_LOCK_ASSERT(sc);

	switch (atu_fetch_op) {
	case 0:
		/* Initialise things for the first fetch */

		AR40XX_DPRINTF(sc, AR40XX_DBG_ATU_OP,
		    "%s: initializing\n", __func__);

		ret = ar40xx_hw_atu_wait_busy(sc);
		if (ret != 0)
			return (ret);

		AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_FUNC,
		    AR40XX_ATU_FUNC_OP_GET_NEXT);
		AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_DATA0, 0);
		AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_DATA1, 0);
		AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_DATA2, 0);
		AR40XX_REG_BARRIER_WRITE(sc);

		return (0);
	case 1:
		AR40XX_DPRINTF(sc, AR40XX_DBG_ATU_OP,
		    "%s: reading next\n", __func__);
		/*
		 * Attempt to read the next address entry; don't modify what
		 * is there in these registers as its used for the next fetch
		 */
		ret = ar40xx_hw_atu_wait_busy(sc);
		if (ret != 0)
			return (ret);

		/* Begin the next read event; not modifying anything */
		AR40XX_REG_BARRIER_READ(sc);
		val = AR40XX_REG_READ(sc, AR40XX_REG_ATU_FUNC);
		val |= AR40XX_ATU_FUNC_BUSY;
		AR40XX_REG_WRITE(sc, AR40XX_REG_ATU_FUNC, val);
		AR40XX_REG_BARRIER_WRITE(sc);

		/* Wait for it to complete */
		ret = ar40xx_hw_atu_wait_busy(sc);
		if (ret != 0)
			return (ret);

		/* Fetch the ethernet address and ATU status */
		AR40XX_REG_BARRIER_READ(sc);
		ret0 = AR40XX_REG_READ(sc, AR40XX_REG_ATU_DATA0);
		ret1 = AR40XX_REG_READ(sc, AR40XX_REG_ATU_DATA1);
		ret2 = AR40XX_REG_READ(sc, AR40XX_REG_ATU_DATA2);

		/* If the status is zero, then we're done */
		if (MS(ret2, AR40XX_ATU_FUNC_DATA2_STATUS) == 0)
			return (ENOENT);

		/* MAC address */
		e->es_macaddr[5] = MS(ret0, AR40XX_ATU_DATA0_MAC_ADDR3);
		e->es_macaddr[4] = MS(ret0, AR40XX_ATU_DATA0_MAC_ADDR2);
		e->es_macaddr[3] = MS(ret0, AR40XX_ATU_DATA0_MAC_ADDR1);
		e->es_macaddr[2] = MS(ret0, AR40XX_ATU_DATA0_MAC_ADDR0);
		e->es_macaddr[0] = MS(ret1, AR40XX_ATU_DATA1_MAC_ADDR5);
		e->es_macaddr[1] = MS(ret1, AR40XX_ATU_DATA1_MAC_ADDR4);

		/* Bitmask of ports this entry is for */
		e->es_portmask = MS(ret1, AR40XX_ATU_DATA1_DEST_PORT);

		/* TODO: other flags that are interesting */

		AR40XX_DPRINTF(sc, AR40XX_DBG_ATU_OP,
		    "%s: MAC %6D portmask 0x%08x\n",
		    __func__,
		    e->es_macaddr, ":", e->es_portmask);
		return (0);
	default:
		return (EINVAL);
	}
	return (EINVAL);
}
