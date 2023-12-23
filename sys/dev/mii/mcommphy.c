/*
 * Copyright (c) 2022 Jared McNeill <jmcneill@invisible.ca>
 * Copyright (c) 2022 Soren Schmidt <sos@deepcore.dk>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Motorcomm YT8511C / YT8511H Integrated 10/100/1000 Gigabit Ethernet phy
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"

#define	MCOMMPHY_OUI			0x000000
#define	MCOMMPHY_MODEL			0x10
#define	MCOMMPHY_REV			0x0a

#define	EXT_REG_ADDR			0x1e
#define	EXT_REG_DATA			0x1f

/* Extended registers */
#define	PHY_CLOCK_GATING_REG		0x0c
#define	 RX_CLK_DELAY_EN		0x0001
#define	 CLK_25M_SEL			0x0006
#define	 CLK_25M_SEL_125M		3
#define	 TX_CLK_DELAY_SEL		0x00f0
#define	PHY_SLEEP_CONTROL1_REG		0x27
#define	 PLLON_IN_SLP			0x4000

#define	LOWEST_SET_BIT(mask)		((((mask) - 1) & (mask)) ^ (mask))
#define	SHIFTIN(x, mask)		((x) * LOWEST_SET_BIT(mask))

static int
mcommphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	PHY_STATUS(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);

	return (0);
}

static const struct mii_phy_funcs mcommphy_funcs = {
	mcommphy_service,
	ukphy_status,
	mii_phy_reset
};

static int
mcommphy_probe(device_t dev)
{
	struct mii_attach_args *ma = device_get_ivars(dev);

	/*
	 * The YT8511C reports an OUI of 0. Best we can do here is to match
	 * exactly the contents of the PHY identification registers.
	 */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MCOMMPHY_OUI &&
	    MII_MODEL(ma->mii_id2) == MCOMMPHY_MODEL &&
	    MII_REV(ma->mii_id2) == MCOMMPHY_REV) {
		device_set_desc(dev, "Motorcomm YT8511 media interface");
		return BUS_PROBE_DEFAULT;
	}
	return (ENXIO);
}

static int
mcommphy_attach(device_t dev)
{
	struct mii_softc *sc = device_get_softc(dev);
	uint16_t oldaddr, data;

	mii_phy_dev_attach(dev, MIIF_NOMANPAUSE, &mcommphy_funcs, 0);

	PHY_RESET(sc);

	/* begin chip stuff */
	oldaddr = PHY_READ(sc, EXT_REG_ADDR);

	PHY_WRITE(sc, EXT_REG_ADDR, PHY_CLOCK_GATING_REG);
	data = PHY_READ(sc, EXT_REG_DATA);
	data &= ~CLK_25M_SEL;
	data |= SHIFTIN(CLK_25M_SEL_125M, CLK_25M_SEL);;
	if (sc->mii_flags & MIIF_RX_DELAY) {
		data |= RX_CLK_DELAY_EN;
	} else {
		data &= ~RX_CLK_DELAY_EN;
	}
	data &= ~TX_CLK_DELAY_SEL;
	if (sc->mii_flags & MIIF_TX_DELAY) {
		data |= SHIFTIN(0xf, TX_CLK_DELAY_SEL);
	} else {
		data |= SHIFTIN(0x2, TX_CLK_DELAY_SEL);
	}
	PHY_WRITE(sc, EXT_REG_DATA, data);

	PHY_WRITE(sc, EXT_REG_ADDR, PHY_SLEEP_CONTROL1_REG);
	data = PHY_READ(sc, EXT_REG_DATA);
	data |= PLLON_IN_SLP;
	PHY_WRITE(sc, EXT_REG_DATA, data);

	PHY_WRITE(sc, EXT_REG_ADDR, oldaddr);
	/* end chip stuff */

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & sc->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);

	return (0);
}


static device_method_t mcommphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		mcommphy_probe),
	DEVMETHOD(device_attach,	mcommphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD_END
};

static driver_t mcommphy_driver = {
	"mcommphy",
	mcommphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(mcommphy, miibus, mcommphy_driver, 0, 0);
