/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_vtu.h>
#include <dev/etherswitch/ar40xx/ar40xx_debug.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"


/*
 * Perform a VTU (vlan table unit) operation.
 */
int
ar40xx_hw_vtu_op(struct ar40xx_softc *sc, uint32_t op, uint32_t val)
{
	int ret;

	AR40XX_DPRINTF(sc, AR40XX_DBG_VTU_OP,
	    "%s: called; op=0x%08x, val=0x%08x\n",
	    __func__, op, val);

	ret = (ar40xx_hw_wait_bit(sc, AR40XX_REG_VTU_FUNC1,
                            AR40XX_VTU_FUNC1_BUSY, 0));
	if (ret != 0)
		return (ret);

	if ((op & AR40XX_VTU_FUNC1_OP) == AR40XX_VTU_FUNC1_OP_LOAD) {
		AR40XX_REG_WRITE(sc, AR40XX_REG_VTU_FUNC0, val);
		AR40XX_REG_BARRIER_WRITE(sc);
	}

	op |= AR40XX_VTU_FUNC1_BUSY;
	AR40XX_REG_WRITE(sc, AR40XX_REG_VTU_FUNC1, op);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Load in a VLAN table map / port configuration for the given
 * vlan ID.
 */
int
ar40xx_hw_vtu_load_vlan(struct ar40xx_softc *sc, uint32_t vid,
    uint32_t port_mask, uint32_t untagged_mask)
{

	uint32_t op, val, mode;
	int i, ret;

	AR40XX_DPRINTF(sc, AR40XX_DBG_VTU_OP,
	    "%s: called; vid=%d port_mask=0x%08x, untagged_mask=0x%08x\n",
	    __func__, vid, port_mask, untagged_mask);

	op = AR40XX_VTU_FUNC1_OP_LOAD | (vid << AR40XX_VTU_FUNC1_VID_S);
	val = AR40XX_VTU_FUNC0_VALID | AR40XX_VTU_FUNC0_IVL;
	for (i = 0; i < AR40XX_NUM_PORTS; i++) {
		if ((port_mask & (1U << i)) == 0)
			/* Not in the VLAN at all */
			mode = AR40XX_VTU_FUNC0_EG_MODE_NOT;
		else if (sc->sc_vlan.vlan == 0)
			/* VLAN mode disabled; keep the provided VLAN tag */
			mode = AR40XX_VTU_FUNC0_EG_MODE_KEEP;
		else if (untagged_mask & (1U << i))
			/* Port in the VLAN; is untagged */
			mode = AR40XX_VTU_FUNC0_EG_MODE_UNTAG;
		else
			/* Port is in the VLAN; is tagged */
			mode = AR40XX_VTU_FUNC0_EG_MODE_TAG;
		val |= mode << AR40XX_VTU_FUNC0_EG_MODE_S(i);
	}
	ret = ar40xx_hw_vtu_op(sc, op, val);

	return (ret);
}

/*
 * Flush all VLAN port entries.
 */
int
ar40xx_hw_vtu_flush(struct ar40xx_softc *sc)
{
	int ret;

	AR40XX_DPRINTF(sc, AR40XX_DBG_VTU_OP, "%s: called\n", __func__);

	ret = ar40xx_hw_vtu_op(sc, AR40XX_VTU_FUNC1_OP_FLUSH, 0);
	return (ret);
}

/*
 * Get the VLAN port map for the given vlan ID.
 */
int
ar40xx_hw_vtu_get_vlan(struct ar40xx_softc *sc, int vid, uint32_t *ports,
    uint32_t *untagged_ports)
{
	uint32_t op, reg, val;
	int i, r;

	op = AR40XX_VTU_FUNC1_OP_GET_ONE;

	/* Filter out any etherswitch VID flags; only grab the VLAN ID */
	vid &= ETHERSWITCH_VID_MASK;

	/* XXX TODO: the VTU here stores egress mode - keep, tag, untagged, none */
	op |= (vid << AR40XX_VTU_FUNC1_VID_S);
	r = ar40xx_hw_vtu_op(sc, op, 0);
	if (r != 0) {
		device_printf(sc->sc_dev, "%s: %d: op failed\n", __func__, vid);
		return (r);
	}

	AR40XX_REG_BARRIER_READ(sc);
	reg = AR40XX_REG_READ(sc, AR40XX_REG_VTU_FUNC0);

	*ports = 0;
	for (i = 0; i < AR40XX_NUM_PORTS; i++) {
		val = reg >> AR40XX_VTU_FUNC0_EG_MODE_S(i);
		val = val & 0x3;
		/* XXX KEEP (unmodified? For non-dot1q operation?) */
		if (val == AR40XX_VTU_FUNC0_EG_MODE_TAG) {
			*ports |= (1 << i);
		} else if (val == AR40XX_VTU_FUNC0_EG_MODE_UNTAG) {
			*ports |= (1 << i);
			*untagged_ports |= (1 << i);
		}
	}

	return (0);
}

