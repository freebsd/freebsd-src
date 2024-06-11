/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ganbold Tsagaankhuu <ganbold@FreeBSD.org>
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
 * Rockchip RK3399 eMMC PHY
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/syscon/syscon.h>
#include <dev/phy/phy.h>

#include "syscon_if.h"

#define	GRF_EMMCPHY_BASE	0xf780
#define	GRF_EMMCPHY_CON0	(GRF_EMMCPHY_BASE + 0x00)
#define	 PHYCTRL_FRQSEL		(1 << 13) | (1 << 12)
#define	  PHYCTRL_FRQSEL_200M	0
#define	  PHYCTRL_FRQSEL_50M	1
#define	  PHYCTRL_FRQSEL_100M	2
#define	  PHYCTRL_FRQSEL_150M	3
#define	 PHYCTRL_OTAPDLYENA	(1 << 11)
#define	 PHYCTRL_OTAPDLYSEL	(1 << 10) | (1 << 9) | (1 << 8) | (1 << 7)
#define	 PHYCTRL_ITAPCHGWIN	(1 << 6)
#define	 PHYCTRL_ITAPDLYSEL	(1 << 5) | (1 << 4)  | (1 << 3) | (1 << 2) | \
    (1 << 1)
#define	 PHYCTRL_ITAPDLYENA	(1 << 0)
#define	GRF_EMMCPHY_CON1	(GRF_EMMCPHY_BASE + 0x04)
#define	 PHYCTRL_CLKBUFSEL	(1 << 8) | (1 << 7) | (1 << 6)
#define	 PHYCTRL_SELDLYTXCLK	(1 << 5)
#define	 PHYCTRL_SELDLYRXCLK	(1 << 4)
#define	 PHYCTRL_STRBSEL	0xf
#define	GRF_EMMCPHY_CON2	(GRF_EMMCPHY_BASE + 0x08)
#define	 PHYCTRL_REN_STRB	(1 << 9)
#define	 PHYCTRL_REN_CMD	(1 << 8)
#define	 PHYCTRL_REN_DAT	0xff
#define	GRF_EMMCPHY_CON3	(GRF_EMMCPHY_BASE + 0x0c)
#define	 PHYCTRL_PU_STRB	(1 << 9)
#define	 PHYCTRL_PU_CMD		(1 << 8)
#define	 PHYCTRL_PU_DAT		0xff
#define	GRF_EMMCPHY_CON4	(GRF_EMMCPHY_BASE + 0x10)
#define	 PHYCTRL_OD_RELEASE_CMD		(1 << 9)
#define	 PHYCTRL_OD_RELEASE_STRB	(1 << 8)
#define	 PHYCTRL_OD_RELEASE_DAT		0xff
#define	GRF_EMMCPHY_CON5	(GRF_EMMCPHY_BASE + 0x14)
#define	 PHYCTRL_ODEN_STRB	(1 << 9)
#define	 PHYCTRL_ODEN_CMD	(1 << 8)
#define	 PHYCTRL_ODEN_DAT	0xff
#define	GRF_EMMCPHY_CON6	(GRF_EMMCPHY_BASE + 0x18)
#define	 PHYCTRL_DLL_TRM_ICP	(1 << 12) | (1 << 11) | (1 << 10) | (1 << 9)
#define	 PHYCTRL_EN_RTRIM	(1 << 8)
#define	 PHYCTRL_RETRIM		(1 << 7)
#define	 PHYCTRL_DR_TY		(1 << 6) | (1 << 5) | (1 << 4)
#define	 PHYCTRL_RETENB		(1 << 3)
#define	 PHYCTRL_RETEN		(1 << 2)
#define	 PHYCTRL_ENDLL		(1 << 1)
#define	 PHYCTRL_PDB		(1 << 0)
#define	GRF_EMMCPHY_STATUS	(GRF_EMMCPHY_BASE + 0x20)
#define	 PHYCTRL_CALDONE	(1 << 6)
#define	 PHYCTRL_DLLRDY		(1 << 5)
#define	 PHYCTRL_RTRIM		(1 << 4) | (1 << 3) | (1 << 2) | (1 << 1)
#define	 PHYCTRL_EXR_NINST	(1 << 0)

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3399-emmc-phy",	1 },
	{ NULL,				0 }
};

struct rk_emmcphy_softc {
	struct syscon		*syscon;
	struct rk_emmcphy_conf	*phy_conf;
	clk_t			clk;
};

#define	LOWEST_SET_BIT(mask)	((((mask) - 1) & (mask)) ^ (mask))
#define	SHIFTIN(x, mask)	((x) * LOWEST_SET_BIT(mask))

/* Phy class and methods. */
static int rk_emmcphy_enable(struct phynode *phynode, bool enable);
static phynode_method_t rk_emmcphy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,	rk_emmcphy_enable),
	PHYNODEMETHOD_END
};

DEFINE_CLASS_1(rk_emmcphy_phynode, rk_emmcphy_phynode_class,
    rk_emmcphy_phynode_methods, 0, phynode_class);

static int
rk_emmcphy_enable(struct phynode *phynode, bool enable)
{
	struct rk_emmcphy_softc *sc;
	device_t dev;
	intptr_t phy;
	uint64_t rate, frqsel;
	uint32_t mask, val;
	int error;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (bootverbose)
		device_printf(dev, "Phy id: %ld\n", phy);

	if (phy != 0) {
		device_printf(dev, "Unknown phy: %ld\n", phy);
		return (ERANGE);
	}
	if (enable) {
		/* Drive strength */
		mask = PHYCTRL_DR_TY;
		val = SHIFTIN(0, PHYCTRL_DR_TY);
		SYSCON_WRITE_4(sc->syscon, GRF_EMMCPHY_CON6,
		    (mask << 16) | val);

		/* Enable output tap delay */
		mask = PHYCTRL_OTAPDLYENA | PHYCTRL_OTAPDLYSEL;
		val = PHYCTRL_OTAPDLYENA | SHIFTIN(4, PHYCTRL_OTAPDLYSEL);
		SYSCON_WRITE_4(sc->syscon, GRF_EMMCPHY_CON0,
		    (mask << 16) | val);
	}

	/* Power down PHY and disable DLL before making changes */
	mask = PHYCTRL_ENDLL | PHYCTRL_PDB;
	val = 0;
	SYSCON_WRITE_4(sc->syscon, GRF_EMMCPHY_CON6, (mask << 16) | val);

	if (enable == false)
		return (0);

	sc->phy_conf = (struct rk_emmcphy_conf *)ofw_bus_search_compatible(dev,
	    compat_data)->ocd_data;

	/* Get clock */
	error = clk_get_by_ofw_name(dev, 0, "emmcclk", &sc->clk);
	if (error != 0) {
		device_printf(dev, "cannot get emmcclk clock, continue\n");
		sc->clk = NULL;
	} else
		device_printf(dev, "got emmcclk clock\n");

	if (sc->clk) {
		error = clk_get_freq(sc->clk, &rate);
		if (error != 0) {
			device_printf(dev, "cannot get clock frequency\n");
			return (ENXIO);
		}
	} else
		rate = 0;

	if (rate != 0) {
		if (rate < 75000000)
			frqsel = PHYCTRL_FRQSEL_50M;
		else if (rate < 125000000)
			frqsel = PHYCTRL_FRQSEL_100M;
		else if (rate < 175000000)
			frqsel = PHYCTRL_FRQSEL_150M;
		else
			frqsel = PHYCTRL_FRQSEL_200M;
	} else
		frqsel = PHYCTRL_FRQSEL_200M;

	DELAY(3);

	/* Power up PHY */
	mask = PHYCTRL_PDB;
	val = PHYCTRL_PDB;
	SYSCON_WRITE_4(sc->syscon, GRF_EMMCPHY_CON6, (mask << 16) | val);

	/* Wait for calibration */
	DELAY(10);
	val = SYSCON_READ_4(sc->syscon, GRF_EMMCPHY_STATUS);
	if ((val & PHYCTRL_CALDONE) == 0) {
		device_printf(dev, "PHY calibration did not complete\n");
		return (ENXIO);
	}

	/* Set DLL frequency */
	mask = PHYCTRL_FRQSEL;
	val = SHIFTIN(frqsel, PHYCTRL_FRQSEL);
	SYSCON_WRITE_4(sc->syscon, GRF_EMMCPHY_CON0, (mask << 16) | val);

	/* Enable DLL */
	mask = PHYCTRL_ENDLL;
	val = PHYCTRL_ENDLL;
	SYSCON_WRITE_4(sc->syscon, GRF_EMMCPHY_CON6, (mask << 16) | val);

	if (rate != 0) {
		/*
		 * Rockchip RK3399 TRM V1.3 Part2.pdf says in page 698:
		 * After the DLL control loop reaches steady state a DLL
		 * ready signal is generated by the DLL circuits
		 * 'phyctrl_dllrdy'.
		 * The time from 'phyctrl_endll' to DLL ready signal
		 * 'phyctrl_dllrdy' varies with the clock frequency.
		 * At 200MHz clock frequency the DLL ready delay is 2.56us,
		 * at 100MHz clock frequency the DLL ready delay is 5.112us and
		 * at 50 MHz clock frequency the DLL ready delay is 10.231us.
		 * We could use safe values for wait, 12us, 8us, 6us and 4us
		 * respectively.
		 * However due to some unknown reason it is not working and
		 * DLL seems to take extra long time to lock.
		 * So we will use more safe value 50ms here.
		 */

		/* Wait for DLL ready */
		DELAY(50000);
		val = SYSCON_READ_4(sc->syscon, GRF_EMMCPHY_STATUS);
		if ((val & PHYCTRL_DLLRDY) == 0) {
			device_printf(dev, "DLL loop failed to lock\n");
			return (ENXIO);
		}
	}

	return (0);
}

static int
rk_emmcphy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3399 eMMC PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_emmcphy_attach(device_t dev)
{
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	struct rk_emmcphy_softc *sc;
	phandle_t node;
	phandle_t xnode;
	pcell_t handle;
	intptr_t phy;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (OF_getencprop(node, "clocks", (void *)&handle,
	    sizeof(handle)) <= 0) {
		device_printf(dev, "cannot get clocks handle\n");
		return (ENXIO);
	}
	xnode = OF_node_from_xref(handle);
	if (OF_hasprop(xnode, "arasan,soc-ctl-syscon") &&
	    syscon_get_by_ofw_property(dev, xnode,
	    "arasan,soc-ctl-syscon", &sc->syscon) != 0) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	if (sc->syscon == NULL) {
		device_printf(dev, "failed to get syscon\n");
		return (ENXIO);
	}

	/* Create and register phy */
	bzero(&phy_init, sizeof(phy_init));
	phy_init.id = 0;
	phy_init.ofw_node = ofw_bus_get_node(dev);
	phynode = phynode_create(dev, &rk_emmcphy_phynode_class, &phy_init);
	if (phynode == NULL) {
		device_printf(dev, "failed to create eMMC PHY\n");
		return (ENXIO);
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(dev, "failed to register eMMC PHY\n");
		return (ENXIO);
	}
	if (bootverbose) {
		phy = phynode_get_id(phynode);
		device_printf(dev, "Attached phy id: %ld\n", phy);
	}
	return (0);
}

static device_method_t rk_emmcphy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_emmcphy_probe),
	DEVMETHOD(device_attach,	rk_emmcphy_attach),

	DEVMETHOD_END
};

static driver_t rk_emmcphy_driver = {
	"rk_emmcphy",
	rk_emmcphy_methods,
	sizeof(struct rk_emmcphy_softc)
};

EARLY_DRIVER_MODULE(rk_emmcphy, simplebus, rk_emmcphy_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_emmcphy, 1);
