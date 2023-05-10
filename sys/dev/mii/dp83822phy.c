/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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

/*
 * Driver for TI DP83822 Ethernet PHY
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#define	BIT(x)			(1 << (x))

#define	DP83822_PHYSTS			0x10
#define	DP83822_PHYSTS_LINK_UP		BIT(0)
#define	DP83822_PHYSTS_SPEED_100	BIT(1)
#define	DP83822_PHYSTS_FD		BIT(2)

#define	DP83822_PHYSCR		0x11
#define	DP83822_PHYSCR_INT_OE	BIT(0)	/* Behaviour of INT pin. */
#define	DP83822_PHYSCR_INT_EN	BIT(1)

#define	DP83822_MISR1			0x12
#define	DP83822_MISR1_AN_CMPL_EN	BIT(2)
#define	DP83822_MISR1_DP_CHG_EN		BIT(3)
#define	DP83822_MISR1_SPD_CHG_EN	BIT(4)
#define	DP83822_MISR1_LINK_CHG_EN	BIT(5)
#define	DP83822_MISR1_INT_MASK		0xFF
#define	DP83822_MISR1_INT_STS_SHIFT	8

#define	DP83822_MISR2			0x13
#define	DP83822_MISR2_AN_ERR_EN		BIT(6)
#define	DP83822_MISR2_INT_MASK		0xFF
#define	DP83822_MISR2_INT_STS_SHIFT	8

static int dp_service(struct mii_softc*, struct mii_data*, int);

struct dp83822_softc {
	struct mii_softc mii_sc;
	struct resource *irq_res;
	void 		*irq_cookie;
};

static const struct mii_phydesc dpphys[] = {
	MII_PHY_DESC(xxTI, DP83822),
	MII_PHY_END
};

static const struct mii_phy_funcs dpphy_funcs = {
	dp_service,
	ukphy_status,
	mii_phy_reset
};

static void
dp_intr(void *arg)
{
	struct mii_softc *sc = (struct mii_softc *)arg;
	uint32_t status;

	status = PHY_READ(sc, DP83822_MISR1);

	if (!((status >> DP83822_MISR1_INT_STS_SHIFT) &
	    (status & DP83822_MISR1_INT_MASK)))
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
	struct dp83822_softc *sc;
	struct mii_softc *mii_sc;
	uint32_t value;
	int error, rid;

	sc = device_get_softc(dev);
	mii_sc = &sc->mii_sc;
	mii_phy_dev_attach(dev, MIIF_NOMANPAUSE, &dpphy_funcs, 1);

	PHY_RESET(mii_sc);

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

	/*
	 * ACK and unmask all relevant interrupts.
	 * We have a single register that serves the purpose
	 * of both interrupt status and mask.
	 * Interrupts are cleared on read.
	 */
	(void)PHY_READ(mii_sc, DP83822_MISR1);
	value = DP83822_MISR1_AN_CMPL_EN |
		DP83822_MISR1_DP_CHG_EN  |
		DP83822_MISR1_SPD_CHG_EN |
		DP83822_MISR1_LINK_CHG_EN;
	PHY_WRITE(mii_sc, DP83822_MISR1, value);
	value = PHY_READ(mii_sc, DP83822_PHYSCR);
	value |= DP83822_PHYSCR_INT_OE |
		 DP83822_PHYSCR_INT_EN;
	PHY_WRITE(mii_sc, DP83822_PHYSCR, value);

no_irq:
	return (0);
}

static int
dp_detach(device_t dev)
{
	struct dp83822_softc *sc;

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

static device_method_t dp_methods[] = {
	DEVMETHOD(device_probe,         dp_probe),
	DEVMETHOD(device_attach,        dp_attach),
	DEVMETHOD(device_detach,        dp_detach),
	DEVMETHOD_END
};

static driver_t dp_driver = {
	"dp83822phy",
	dp_methods,
	sizeof(struct dp83822_softc)
};

DRIVER_MODULE(dp83822phy, miibus, dp_driver, 0, 0);
