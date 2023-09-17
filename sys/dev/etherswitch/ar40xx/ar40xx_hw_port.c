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
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_debug.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_port.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"


int
ar40xx_hw_port_init(struct ar40xx_softc *sc, int port)
{
	uint32_t reg;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_PORT_INIT,
	    "%s: called; port %d\n", __func__, port);

	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_STATUS(port), 0);
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_HEADER(port), 0);
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_VLAN0(port), 0);
	AR40XX_REG_BARRIER_WRITE(sc);

	DELAY(20);

	/*
	 * Ok! Here is where things get super fun in the AR40xx
	 * driver in uboot/linux.
	 *
	 * The earlier chipset switch drivers enable auto link enable here.
	 * The switch will poll the PHYs too, and configure appropriately.
	 *
	 * The ar40xx code in linux/u-boot instead has a whole workaround
	 * path that polls things directly and does some weird hijinx.
	 * NOTABLY - they do NOT enable the TX/RX MAC here or autoneg -
	 * it's done in the work around path.
	 *
	 * SO - for now the port is left off until the PHY state changes.
	 * And then we flip it on and off based on the PHY state.
	 */
#if 0
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_STATUS(port),
	    AR40XX_PORT_AUTO_LINK_EN);
#endif

	/*
	 * Configure the VLAN egress mode (don't touch them) and
	 * learning state for STP/ATU.  This isn't currently
	 * configurable so it's just nailed up here and left alone.
	 */
	reg = AR40XX_PORT_VLAN1_OUT_MODE_UNTOUCH
	     << AR40XX_PORT_VLAN1_OUT_MODE_S;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_VLAN1(port), reg);

	reg = AR40XX_PORT_LOOKUP_LEARN;
	reg |= AR40XX_PORT_STATE_FORWARD << AR40XX_PORT_LOOKUP_STATE_S;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_LOOKUP(port), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Call when the link for a non-CPU port is down.
 *
 * This will turn off the MAC/forwarding path for this port.
 */
int
ar40xx_hw_port_link_down(struct ar40xx_softc *sc, int port)
{

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_PORT_INIT,
	    "%s: called; port %d\n", __func__, port);

	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_STATUS(port), 0);

	return (0);
}

/*
 * Call when the link for a non-CPU port is up.
 *
 * This will turn on the default auto-link checking and
 * eventually enable the TX/RX MAC.
 */
int
ar40xx_hw_port_link_up(struct ar40xx_softc *sc, int port)
{
	uint32_t reg;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_PORT_INIT,
	    "%s: called; port %d\n", __func__, port);

	/* Auto-link enable */
	AR40XX_REG_BARRIER_READ(sc);
	reg = AR40XX_REG_READ(sc, AR40XX_REG_PORT_STATUS(port));
	reg |= AR40XX_PORT_AUTO_LINK_EN;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_STATUS(port), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Setup the CPU facing port.  For this device it'll only
 * be port 0.
 */
int
ar40xx_hw_port_cpuport_setup(struct ar40xx_softc *sc)
{
	uint32_t reg;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_PORT_INIT, "%s: called\n",
	    __func__);

	reg = AR40XX_PORT_STATUS_TXFLOW
	    | AR40XX_PORT_STATUS_RXFLOW
	    | AR40XX_PORT_TXHALF_FLOW
	    | AR40XX_PORT_DUPLEX
	    | AR40XX_PORT_SPEED_1000M;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_STATUS(0), reg);
	DELAY(20);

	reg |= AR40XX_PORT_TX_EN | AR40XX_PORT_RX_EN;
        AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_STATUS(0), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Fetch the port PVID.
 *
 * For 802.1q mode this is the default VLAN ID for the port.
 * Frames without an 802.1q VLAN will assume this VLAN ID for
 * transmit/receive.
 */
int
ar40xx_hw_get_port_pvid(struct ar40xx_softc *sc, int port, int *pvid)
{
	uint32_t reg;

	AR40XX_LOCK_ASSERT(sc);

	AR40XX_REG_BARRIER_READ(sc);
	reg = AR40XX_REG_READ(sc, AR40XX_REG_PORT_VLAN0(port));

	reg = reg >> AR40XX_PORT_VLAN0_DEF_CVID_S;
	reg = reg & 0x0fff; /* XXX */

	*pvid = reg;
	return (0);
}

/*
 * Set the port PVID.
 *
 * For now, since double-tagged frames aren't currently supported,
 * CVID=SVID here.
 */
int
ar40xx_hw_set_port_pvid(struct ar40xx_softc *sc, int port, int pvid)
{
	uint32_t reg;

	AR40XX_LOCK_ASSERT(sc);

	pvid &= ETHERSWITCH_VID_MASK;

	reg = pvid << AR40XX_PORT_VLAN0_DEF_SVID_S;
	reg |= pvid << AR40XX_PORT_VLAN0_DEF_CVID_S;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_VLAN0(port), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Setup the default port membership configuration.
 *
 * This configures the PVID for the port in the sc_vlan config,
 * along with a set of ports that constitute the "membership"
 * of this particular VID.
 *
 * For 802.1q mode the membership can be viewed as the default
 * learning port group, but this can be added to via VLAN membership.
 * (Eg you could in theory split two LAN ports into separate "member"
 * groups and they'd not learn MAC addresses from each other even
 * inside a VLAN; you'd then end up with the traffic being flooded to
 * the CPU port.)
 */
int
ar40xx_hw_port_setup(struct ar40xx_softc *sc, int port, uint32_t members)
{
	uint32_t egress, ingress, reg;
	uint32_t pvid = sc->sc_vlan.vlan_id[sc->sc_vlan.pvid[port]]
	    & ETHERSWITCH_VID_MASK;

	if (sc->sc_vlan.vlan) {
		egress = AR40XX_PORT_VLAN1_OUT_MODE_UNMOD;
		ingress = AR40XX_IN_SECURE;
	} else {
		egress = AR40XX_PORT_VLAN1_OUT_MODE_UNTOUCH;
		ingress = AR40XX_IN_PORT_ONLY;
	}

	reg = pvid << AR40XX_PORT_VLAN0_DEF_SVID_S;
	reg |= pvid << AR40XX_PORT_VLAN0_DEF_CVID_S;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_VLAN0(port), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	reg = AR40XX_PORT_VLAN1_PORT_VLAN_PROP;
	reg |= egress << AR40XX_PORT_VLAN1_OUT_MODE_S;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_VLAN1(port), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	reg = members;
	reg |= AR40XX_PORT_LOOKUP_LEARN;
	reg |= ingress << AR40XX_PORT_LOOKUP_IN_MODE_S;
	reg |= AR40XX_PORT_STATE_FORWARD << AR40XX_PORT_LOOKUP_STATE_S;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_LOOKUP(port), reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}
