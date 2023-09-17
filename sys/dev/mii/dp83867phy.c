/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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

/*
 * Driver for TI DP83867 Ethernet PHY
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/socket.h>

#include <machine/resource.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miidevs.h"
#include "miibus_if.h"

#define BIT(x)			(1 << (x))

#define DP83867_PHYCR		0x10
#define DP83867_PHYSTS		0x11
#define DP83867_MICR		0x12
#define DP83867_ISR		0x13
#define DP83867_CFG3		0x1E
#define DP83867_CTRL		0x1F
#define DP83867_CFG4		0x31
#define DP83867_RGMIICTL	0x32
#define DP83867_STRP_STS1	0x6E
#define DP83867_STRP_STS2	0x6F

#define DP83867_PHYSTS_LINK_UP		BIT(10)
#define DP83867_PHYSTS_ANEG_PENDING	BIT(11)
#define DP83867_PHYSTS_FD		BIT(13)
#define DP83867_PHYSTS_SPEED_MASK	(BIT(15) | BIT(14))
#define DP83867_PHYSTS_SPEED_1000	BIT(15)
#define DP83867_PHYSTS_SPEED_100	BIT(14)
#define DP83867_PHYSTS_SPEED_10		0

#define DP83867_MICR_AN_ERR		BIT(15)
#define DP83867_MICR_SPEED_CHG		BIT(14)
#define DP83867_MICR_DP_MODE_CHG	BIT(13)
#define DP83867_MICR_AN_CMPL		BIT(11)
#define DP83867_MICR_LINK_CHG		BIT(10)

#define DP83867_CFG3_INT_OE		BIT(7)

#define DP83867_CFG4_TST_MODE1		BIT(7)
#define DP83867_CFG4_ANEG_MASK		(BIT(5) | BIT(6))
#define DP83867_CFG4_ANEG_16MS		(0 << 5)

#define BMSR_100_MASK	(BMSR_100T4 | BMSR_100TXFDX | BMSR_100TXHDX | \
			 BMSR_100T2FDX | BMSR_100T2HDX)

static int dp_probe(device_t);
static int dp_attach(device_t);

static int dp_service(struct mii_softc*, struct mii_data*, int);
static void dp_status(struct mii_softc*);

struct dp83867_softc {
	struct mii_softc mii_sc;
	struct resource *irq_res;
	void 		*irq_cookie;
};

static const struct mii_phydesc dpphys[] = {
	MII_PHY_DESC(xxTI, DP83867),
	MII_PHY_END
};

static const struct mii_phy_funcs dpphy_funcs = {
	dp_service,
	dp_status,
	mii_phy_reset
};

static void
dp_intr(void *arg)
{
	struct mii_softc *sc = (struct mii_softc *)arg;
	uint32_t status;

	status = PHY_READ(sc, DP83867_ISR);
	status &= PHY_READ(sc, DP83867_MICR);
	if (!status)
		return;

	PHY_STATUS(sc);
	mii_phy_update(sc, MII_MEDIACHG);
}

static int
dp_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, dpphys, BUS_PROBE_DEFAULT));
}

static int
dp_attach(device_t dev)
{
	struct dp83867_softc *sc;
	struct mii_softc *mii_sc;
	uint32_t value, maxspeed;
	ssize_t size;
	int rid, error;

	sc = device_get_softc(dev);
	mii_sc = &sc->mii_sc;

	size = device_get_property(dev, "max-speed", &maxspeed,
	    sizeof(maxspeed), DEVICE_PROP_UINT32);
	if (size <= 0)
		maxspeed = 0;

	mii_sc->mii_maxspeed = maxspeed;
	mii_phy_dev_attach(dev, MIIF_NOMANPAUSE, &dpphy_funcs, 1);

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL)
		goto no_irq;

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dp_intr, sc, &sc->irq_cookie);
	if (error != 0) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
		sc->irq_res = NULL;
		goto no_irq;
	}

	/* Ack and unmask all relevant interrupts. */
	(void)PHY_READ(mii_sc, DP83867_ISR);
	value = DP83867_MICR_AN_ERR |
	    DP83867_MICR_SPEED_CHG |
	    DP83867_MICR_DP_MODE_CHG |
	    DP83867_MICR_AN_CMPL |
	    DP83867_MICR_LINK_CHG;
	PHY_WRITE(mii_sc, DP83867_MICR, value);

	value = PHY_READ(mii_sc, DP83867_CFG3);
	value |= DP83867_CFG3_INT_OE;
	PHY_WRITE(mii_sc, DP83867_CFG3, value);

no_irq:
	/* Set autonegotation timeout to max possible value. */
	value = PHY_READ(mii_sc, DP83867_CFG4);
	value &= ~DP83867_CFG4_ANEG_MASK;
	value &= ~DP83867_CFG4_TST_MODE1;
	value |= DP83867_CFG4_ANEG_16MS;
	PHY_WRITE(mii_sc, DP83867_CFG4, value);

	return (0);
}

static int
dp_detach(device_t dev)
{
	struct dp83867_softc *sc;

	sc = device_get_softc(dev);

	bus_teardown_intr(dev, sc->irq_res, sc->irq_cookie);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);

	return (mii_phy_detach(dev));
}

static int
dp_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

	PHY_STATUS(sc);
	mii_phy_update(sc, cmd);
	return (0);
}

static void
dp_status(struct mii_softc *sc)
{
	struct mii_data *mii;
	int bmsr, bmcr, physts;

	mii = sc->mii_pdata;
	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	physts = PHY_READ(sc, DP83867_PHYSTS);

	if ((bmsr & BMSR_LINK) && (physts & DP83867_PHYSTS_LINK_UP))
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	/* Autoneg in progress. */
	if (!(physts & DP83867_PHYSTS_ANEG_PENDING)) {
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	switch (physts & DP83867_PHYSTS_SPEED_MASK) {
	case DP83867_PHYSTS_SPEED_1000:
		mii->mii_media_active |= IFM_1000_T;
		break;
	case DP83867_PHYSTS_SPEED_100:
		mii->mii_media_active |= IFM_100_TX;
		break;
	case DP83867_PHYSTS_SPEED_10:
		mii->mii_media_active |= IFM_10_T;
		break;
	default:
		mii->mii_media_active |= IFM_NONE;
		break;
	}
	if (physts & DP83867_PHYSTS_FD)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

}

static device_method_t dp_methods[] = {
	DEVMETHOD(device_probe,         dp_probe),
	DEVMETHOD(device_attach,        dp_attach),
	DEVMETHOD(device_detach,        dp_detach),
	DEVMETHOD_END
};

static driver_t dp_driver = {
	"dp83867phy",
	dp_methods,
	sizeof(struct dp83867_softc)
};

DRIVER_MODULE(dp83867phy, miibus, dp_driver, 0, 0);
