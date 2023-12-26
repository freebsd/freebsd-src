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
#include <dev/etherswitch/ar40xx/ar40xx_hw_mdio.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_port.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_atu.h>
#include <dev/etherswitch/ar40xx/ar40xx_phy.h>
#include <dev/etherswitch/ar40xx/ar40xx_debug.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"


int
ar40xx_phy_tick(struct ar40xx_softc *sc)
{
	struct mii_softc *miisc;
	struct mii_data *mii;
	int phy;
	uint32_t reg;

	AR40XX_LOCK_ASSERT(sc);

	AR40XX_REG_BARRIER_READ(sc);
	/*
	 * Loop over; update phy port status here
	 */
	for (phy = 0; phy < AR40XX_NUM_PHYS; phy++) {
		/*
		 * Port here is PHY, not port!
		 */
		reg = AR40XX_REG_READ(sc, AR40XX_REG_PORT_STATUS(phy + 1));

		mii = device_get_softc(sc->sc_phys.miibus[phy]);

		/*
		 * Compare the current link status to the previous link
		 * status.  We may need to clear ATU / change phy config.
		 */
		if (((reg & AR40XX_PORT_STATUS_LINK_UP) != 0) &&
		    (mii->mii_media_status & IFM_ACTIVE) == 0) {
			AR40XX_DPRINTF(sc, AR40XX_DBG_PORT_STATUS,
			    "%s: PHY %d: down -> up\n", __func__, phy);
			ar40xx_hw_port_link_up(sc, phy + 1);
			ar40xx_hw_atu_flush_port(sc, phy + 1);
		}
		if (((reg & AR40XX_PORT_STATUS_LINK_UP) == 0) &&
		    (mii->mii_media_status & IFM_ACTIVE) != 0) {
			AR40XX_DPRINTF(sc, AR40XX_DBG_PORT_STATUS,
			    "%s: PHY %d: up -> down\n", __func__, phy);
			ar40xx_hw_port_link_down(sc, phy + 1);
			ar40xx_hw_atu_flush_port(sc, phy + 1);
		}

		mii_tick(mii);
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
			if (IFM_INST(mii->mii_media.ifm_cur->ifm_media) !=
			    miisc->mii_inst)
				continue;
			ukphy_status(miisc);
			mii_phy_update(miisc, MII_POLLSTAT);
		}
	}

	return (0);
}

static inline int
ar40xx_portforphy(int phy)
{

	return (phy+1);
}

struct mii_data *
ar40xx_phy_miiforport(struct ar40xx_softc *sc, int port)
{
	int phy;

	phy = port-1;

	if (phy < 0 || phy >= AR40XX_NUM_PHYS)
		return (NULL);
	return (device_get_softc(sc->sc_phys.miibus[phy]));
}

if_t 
ar40xx_phy_ifpforport(struct ar40xx_softc *sc, int port)
{
	int phy;

	phy = port-1;
	if (phy < 0 || phy >= AR40XX_NUM_PHYS)
		return (NULL);
	return (sc->sc_phys.ifp[phy]);
}

static int
ar40xx_ifmedia_upd(if_t ifp)
{
	struct ar40xx_softc *sc = if_getsoftc(ifp);
	struct mii_data *mii = ar40xx_phy_miiforport(sc, if_getdunit(ifp));

	AR40XX_DPRINTF(sc, AR40XX_DBG_PORT_STATUS, "%s: called, PHY %d\n",
	    __func__, if_getdunit(ifp));

	if (mii == NULL)
		return (ENXIO);
	mii_mediachg(mii);
	return (0);
}

static void
ar40xx_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct ar40xx_softc *sc = if_getsoftc(ifp);
	struct mii_data *mii = ar40xx_phy_miiforport(sc, if_getdunit(ifp));

	AR40XX_DPRINTF(sc, AR40XX_DBG_PORT_STATUS, "%s: called, PHY %d\n",
	    __func__, if_getdunit(ifp));

	if (mii == NULL)
		return;
	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

int
ar40xx_attach_phys(struct ar40xx_softc *sc)
{
	int phy, err = 0;
	char name[IFNAMSIZ];

	/* PHYs need an interface, so we generate a dummy one */
	snprintf(name, IFNAMSIZ, "%sport", device_get_nameunit(sc->sc_dev));
	for (phy = 0; phy < AR40XX_NUM_PHYS; phy++) {
		sc->sc_phys.ifp[phy] = if_alloc(IFT_ETHER);
		if (sc->sc_phys.ifp[phy] == NULL) {
			device_printf(sc->sc_dev,
			    "PHY %d: couldn't allocate ifnet structure\n",
			    phy);
			err = ENOMEM;
			break;
		}

		sc->sc_phys.ifp[phy]->if_softc = sc;
		sc->sc_phys.ifp[phy]->if_flags |= IFF_UP | IFF_BROADCAST |
		    IFF_DRV_RUNNING | IFF_SIMPLEX;
		sc->sc_phys.ifname[phy] = malloc(strlen(name)+1, M_DEVBUF,
		    M_WAITOK);
		bcopy(name, sc->sc_phys.ifname[phy], strlen(name)+1);
		if_initname(sc->sc_phys.ifp[phy], sc->sc_phys.ifname[phy],
		    ar40xx_portforphy(phy));
		err = mii_attach(sc->sc_dev, &sc->sc_phys.miibus[phy],
		    sc->sc_phys.ifp[phy], ar40xx_ifmedia_upd,
		    ar40xx_ifmedia_sts, BMSR_DEFCAPMASK,
		    phy, MII_OFFSET_ANY, 0);
		device_printf(sc->sc_dev,
		    "%s attached to pseudo interface %s\n",
		    device_get_nameunit(sc->sc_phys.miibus[phy]),
		    sc->sc_phys.ifp[phy]->if_xname);
		if (err != 0) {
			device_printf(sc->sc_dev,
			    "attaching PHY %d failed\n",
			    phy);
			return (err);
		}
	}
	return (0);
}

int
ar40xx_hw_phy_get_ids(struct ar40xx_softc *sc)
{
	int phy;
	uint32_t id1, id2;

	for (phy = 0; phy < AR40XX_NUM_PHYS; phy++) {
		id1 = MDIO_READREG(sc->sc_mdio_dev, phy, 2);
		id2 = MDIO_READREG(sc->sc_mdio_dev, phy, 3);
		device_printf(sc->sc_dev,
		    "%s: PHY %d: ID1=0x%04x, ID2=0x%04x\n",
		    __func__, phy, id1, id2);
	}

	return (0);
}
