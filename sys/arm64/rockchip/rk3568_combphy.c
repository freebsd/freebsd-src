/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2022 Soren Schmidt <sos@deepcore.dk>
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
 *
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/simple_mfd.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>
#include <dev/syscon/syscon.h>
#include <dev/phy/phy.h>

#include <contrib/device-tree/include/dt-bindings/phy/phy.h>

#include "syscon_if.h"
#include "phydev_if.h"
#include "phynode_if.h"


static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3568-naneng-combphy",	1},
	{NULL, 0}
};

struct rk3568_combphy_softc {
	device_t	dev;
	phandle_t	node;
	struct resource	*mem;
	struct phynode	*phynode;
	struct syscon	*pipe_grf;
	struct syscon	*pipe_phy_grf;
	clk_t		ref_clk;
	clk_t		apb_clk;
	clk_t		pipe_clk;
	hwreset_t	phy_reset;
	int		mode;
};

#define	PHYREG6				0x14
#define	 PHYREG6_PLL_DIV_MASK			0xc0
#define	 PHYREG6_PLL_DIV_2			(1 << 6)
#define	PHYREG7				0x18
#define	 PHYREG7_TX_RTERM_50OHM			(8 << 4)
#define	 PHYREG7_RX_RTERM_44OHM			(15 << 0)
#define	PHYREG8				0x1c
#define	 PHYREG8_SSC_EN			0x10
#define	PHYREG11			0x28
#define	 PHYREG11_SU_TRIM_0_7			0xf0
#define	PHYREG12			0x2c
#define	 PHYREG12_PLL_LPF_ADJ_VALUE		4
#define	PHYREG15			0x38
#define	 PHYREG15_CTLE_EN			0x01
#define	 PHYREG15_SSC_CNT_MASK			0xc0
#define	 PHYREG15_SSC_CNT_VALUE			(1 << 6)
#define	PHYREG16			0x3c
#define	 PHYREG16_SSC_CNT_VALUE			0x5f
#define	PHYREG18			0x44
#define	 PHYREG18_PLL_LOOP			0x32
#define	PHYREG32			0x7c
#define	 PHYREG32_SSC_MASK			0xf0
#define	 PHYREG32_SSC_UPWARD			(0 << 4)
#define	 PHYREG32_SSC_DOWNWARD			(1 << 4)
#define	 PHYREG32_SSC_OFFSET_500PPM		(1 << 6)
#define	PHYREG33			0x80
#define	 PHYREG33_PLL_KVCO_MASK			0x1c
#define	 PHYREG33_PLL_KVCO_VALUE		(2 << 2)

#define	PIPE_MASK_ALL			(0xffff << 16)
#define	PIPE_PHY_GRF_PIPE_CON0		0x00
#define	 PIPE_DATABUSWIDTH_MASK			0x3
#define	 PIPE_DATABUSWIDTH_32BIT		0
#define	 PIPE_DATABUSWIDTH_16BIT		1
#define	 PIPE_PHYMODE_MASK			(3 << 2)
#define	 PIPE_PHYMODE_PCIE			(0 << 2)
#define	 PIPE_PHYMODE_USB3			(1 << 2)
#define	 PIPE_PHYMODE_SATA			(2 << 2)
#define	 PIPE_RATE_MASK				(3 << 4)
#define	 PIPE_RATE_PCIE_2_5GBPS			(0 << 4)
#define	 PIPE_RATE_PCIE_5GBPS			(1 << 4)
#define	 PIPE_RATE_USB3_5GBPS			(0 << 4)
#define	 PIPE_RATE_SATA_1GBPS5			(0 << 4)
#define	 PIPE_RATE_SATA_3GBPS			(1 << 4)
#define	 PIPE_RATE_SATA_6GBPS			(2 << 4)
#define	 PIPE_MAC_PCLKREQ_N			(1 << 8)
#define	 PIPE_L1SUB_ENTREQ			(1 << 9)
#define	 PIPE_RXTERM				(1 << 12)
#define	PIPE_PHY_GRF_PIPE_CON1		0x04
#define	 PHY_CLK_SEL_MASK			(3 << 13)
#define	 PHY_CLK_SEL_24M			(0 << 13)
#define	 PHY_CLK_SEL_25M			(1 << 13)
#define	 PHY_CLK_SEL_100M			(2 << 13)
#define	PIPE_PHY_GRF_PIPE_CON2		0x08
#define	 SEL_PIPE_TXCOMPLIANCE_I		(1 << 15)
#define	 SEL_PIPE_TXELECIDLE			(1 << 12)
#define	 SEL_PIPE_RXTERM			(1 << 8)
#define	 SEL_PIPE_BYPASS_CODEC			(1 << 7)
#define	 SEL_PIPE_PIPE_EBUF			(1 << 6)
#define	 SEL_PIPE_PIPE_PHYMODE			(1 << 1)
#define	 SEL_PIPE_DATABUSWIDTH			(1 << 0)
#define	PIPE_PHY_GRF_PIPE_CON3		0x0c
#define	 PIPE_SEL_MASK				(3 << 13)
#define	 PIPE_SEL_PCIE				(0 << 13)
#define	 PIPE_SEL_USB3				(1 << 13)
#define	 PIPE_SEL_SATA				(2 << 13)
#define	 PIPE_CLK_REF_SRC_I_MASK		(3 << 8)
#define	 PIPE_CLK_REF_SRC_I_PLL_CKREF_INNER	(2 << 8)
#define	 PIPE_RXELECIDLE			(1 << 10)
#define	 PIPE_FROM_PCIE_IO			(1 << 11)

#define	PIPE_GRF_PIPE_CON0		0x00
#define	 SATA2_PHY_SPDMODE_1GBPS5		(0 << 12)
#define	 SATA2_PHY_SPDMODE_3GBPS		(1 << 12)
#define	 SATA2_PHY_SPDMODE_6GBPS		(2 << 12)
#define	 SATA1_PHY_SPDMODE_1GBPS5		(0 << 8)
#define	 SATA1_PHY_SPDMODE_3GBPS		(1 << 8)
#define	 SATA1_PHY_SPDMODE_6GBPS		(2 << 8)
#define	 SATA0_PHY_SPDMODE_1GBPS5		(0 << 4)
#define	 SATA0_PHY_SPDMODE_3GBPS		(1 << 4)
#define	 SATA0_PHY_SPDMODE_6GBPS		(2 << 4)

#define	PIPE_GRF_SATA_CON0		0x10
#define	PIPE_GRF_SATA_CON1		0x14
#define	PIPE_GRF_SATA_CON2		0x18
#define	PIPE_GRF_XPCS_CON0		0x40


/* PHY class and methods */
static int
rk3568_combphy_enable(struct phynode *phynode, bool enable)
{
	device_t dev = phynode_get_device(phynode);
	struct rk3568_combphy_softc *sc = device_get_softc(dev);
	uint64_t rate;

	if (enable == false)
		return (0);

	switch (sc->mode) {
	case PHY_TYPE_SATA:
		device_printf(dev, "configuring for SATA");

		/* tx_rterm 50 ohm & rx_rterm 44 ohm */
		bus_write_4(sc->mem, PHYREG7,
		    PHYREG7_TX_RTERM_50OHM | PHYREG7_RX_RTERM_44OHM);

		/* Adaptive CTLE */
		bus_write_4(sc->mem, PHYREG15,
		    bus_read_4(sc->mem, PHYREG15) | PHYREG15_CTLE_EN);

		/* config grf_pipe for PCIe */
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON3,
		    PIPE_MASK_ALL | PIPE_SEL_SATA | PIPE_RXELECIDLE | 0x7);

		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON2,
		    PIPE_MASK_ALL | SEL_PIPE_TXCOMPLIANCE_I |
		    SEL_PIPE_DATABUSWIDTH | 0xc3);

		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON0,
		    PIPE_MASK_ALL | PIPE_RXTERM | PIPE_DATABUSWIDTH_16BIT |
		    PIPE_RATE_SATA_3GBPS | PIPE_PHYMODE_SATA);

		SYSCON_WRITE_4(sc->pipe_grf, PIPE_GRF_PIPE_CON0,
		    PIPE_MASK_ALL | SATA0_PHY_SPDMODE_6GBPS |
		    SATA1_PHY_SPDMODE_6GBPS | SATA2_PHY_SPDMODE_6GBPS);
		break;

	case PHY_TYPE_PCIE:
		device_printf(dev, "configuring for PCIe");

		/* Set SSC downward spread spectrum */
		bus_write_4(sc->mem, PHYREG32,
		    (bus_read_4(sc->mem, PHYREG32) & PHYREG32_SSC_MASK) |
		    PHYREG32_SSC_DOWNWARD);

		/* config grf_pipe for PCIe */
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON3,
		    PIPE_MASK_ALL | PIPE_SEL_PCIE |
		    PIPE_CLK_REF_SRC_I_PLL_CKREF_INNER);
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON2,
		    PIPE_MASK_ALL | SEL_PIPE_RXTERM | SEL_PIPE_DATABUSWIDTH);
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON0,
		    PIPE_MASK_ALL | PIPE_RXTERM | PIPE_DATABUSWIDTH_32BIT |
		    PIPE_RATE_PCIE_2_5GBPS | PIPE_PHYMODE_PCIE);
		break;

	case PHY_TYPE_USB3:
		device_printf(dev, "configuring for USB3");

		/* Set SSC downward spread spectrum */
		bus_write_4(sc->mem, PHYREG32,
		    (bus_read_4(sc->mem, PHYREG32) & PHYREG32_SSC_MASK) |
		    PHYREG32_SSC_DOWNWARD);

		/* Adaptive CTLE */
		bus_write_4(sc->mem, PHYREG15,
		    bus_read_4(sc->mem, PHYREG15) | PHYREG15_CTLE_EN);

		/* Set PLL KVCO fine tuning signals */
		bus_write_4(sc->mem, PHYREG33,
		    (bus_read_4(sc->mem, PHYREG33) & PHYREG33_PLL_KVCO_MASK) |
		    PHYREG33_PLL_KVCO_VALUE);

		/* Enable controlling random jitter. */
		bus_write_4(sc->mem, PHYREG12, PHYREG12_PLL_LPF_ADJ_VALUE);

		/* Set PLL input clock divider 1/2 */
		bus_write_4(sc->mem, PHYREG6,
		    (bus_read_4(sc->mem, PHYREG6) & PHYREG6_PLL_DIV_MASK) |
		    PHYREG6_PLL_DIV_2);

		/* Set PLL loop divider */
		bus_write_4(sc->mem, PHYREG18, PHYREG18_PLL_LOOP);

		/* Set PLL LPF R1 to su_trim[0:7] */
		bus_write_4(sc->mem, PHYREG11, PHYREG11_SU_TRIM_0_7);

		/* config grf_pipe for USB3 */
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON3,
		    PIPE_MASK_ALL | PIPE_SEL_USB3);
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON2,
		    PIPE_MASK_ALL);
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON0,
		    PIPE_MASK_ALL | PIPE_DATABUSWIDTH_16BIT |
		    PIPE_PHYMODE_USB3 | PIPE_RATE_USB3_5GBPS);
		break;

	default:
		printf("Unsupported mode=%d\n", sc->mode);
		return (-1);
	}

	clk_get_freq(sc->ref_clk, &rate);
	printf(" ref_clk=%lu\n", rate);

	switch (rate) {
	case 24000000:
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON1,
		    (PHY_CLK_SEL_MASK << 16) | PHY_CLK_SEL_24M);

		if (sc->mode == PHY_TYPE_USB3 || sc->mode == PHY_TYPE_SATA) {
			/* Adaptive CTLE */
			bus_write_4(sc->mem, PHYREG15,
			    (bus_read_4(sc->mem, PHYREG15) &
			    PHYREG15_SSC_CNT_MASK) | PHYREG15_SSC_CNT_VALUE);

			/* SSC control period */
			bus_write_4(sc->mem, PHYREG16, PHYREG16_SSC_CNT_VALUE);
		}
		break;

	case 25000000:
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON1,
		    (PHY_CLK_SEL_MASK << 16) | PHY_CLK_SEL_25M);
		break;

	case 100000000:
		SYSCON_WRITE_4(sc->pipe_phy_grf, PIPE_PHY_GRF_PIPE_CON1,
		    (PHY_CLK_SEL_MASK << 16) | PHY_CLK_SEL_100M);

		if (sc->mode == PHY_TYPE_PCIE) {
			/* Set PLL KVCO fine tuning signals */
			bus_write_4(sc->mem, PHYREG33,
			    (bus_read_4(sc->mem, PHYREG33) &
			    PHYREG33_PLL_KVCO_MASK) | PHYREG33_PLL_KVCO_VALUE);

			/* Enable controlling random jitter. */
			bus_write_4(sc->mem, PHYREG12,
			    PHYREG12_PLL_LPF_ADJ_VALUE);

			/* Set PLL input clock divider 1/2 */
			bus_write_4(sc->mem, PHYREG6,
			    (bus_read_4(sc->mem, PHYREG6) &
			    PHYREG6_PLL_DIV_MASK) | PHYREG6_PLL_DIV_2);

			/* Set PLL loop divider */
			bus_write_4(sc->mem, PHYREG18, PHYREG18_PLL_LOOP);

			/* Set PLL LPF R1 to su_trim[0:7] */
			bus_write_4(sc->mem, PHYREG11, PHYREG11_SU_TRIM_0_7);
		}
		if (sc->mode == PHY_TYPE_SATA) {
			/* Set SSC downward spread spectrum */
			bus_write_4(sc->mem, PHYREG32,
			    (bus_read_4(sc->mem, PHYREG32) & ~0x000000f0) |
			    PHYREG32_SSC_DOWNWARD | PHYREG32_SSC_OFFSET_500PPM);
		}
		break;

	default:
		device_printf(dev, "unknown ref rate=%lu\n", rate);
		break;
	}

	if (OF_hasprop(sc->node, "rockchip,ext-refclk")) {
		device_printf(dev, "UNSUPPORTED rockchip,ext-refclk\n");
	}
	if (OF_hasprop(sc->node, "rockchip,enable-ssc")) {
		device_printf(dev, "setting rockchip,enable-ssc\n");
		bus_write_4(sc->mem, PHYREG8,
		    bus_read_4(sc->mem, PHYREG8) | PHYREG8_SSC_EN);
	}

	if (hwreset_deassert(sc->phy_reset))
		device_printf(dev, "phy_reset failed to clear\n");

	return (0);
}

static phynode_method_t rk3568_combphy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,	rk3568_combphy_enable),

	PHYNODEMETHOD_END
};
DEFINE_CLASS_1(rk3568_combphy_phynode, rk3568_combphy_phynode_class,
    rk3568_combphy_phynode_methods, 0, phynode_class);


/* Device class and methods */
static int
rk3568_combphy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "RockChip combo PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
rk3568_combphy_attach(device_t dev)
{
	struct rk3568_combphy_softc *sc = device_get_softc(dev);
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	int rid = 0;

	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	/* Get memory resource */
	if (!(sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE))) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}

	/* Get syncons handles */
	if (OF_hasprop(sc->node, "rockchip,pipe-grf") &&
	    syscon_get_by_ofw_property(dev, sc->node, "rockchip,pipe-grf",
	    &sc->pipe_grf))
		return (ENXIO);
	if (OF_hasprop(sc->node, "rockchip,pipe-phy-grf") &&
	    syscon_get_by_ofw_property(dev, sc->node, "rockchip,pipe-phy-grf",
	    &sc->pipe_phy_grf))
		return (ENXIO);

	/* Get & enable clocks */
	if (clk_get_by_ofw_name(dev, 0, "ref", &sc->ref_clk)) {
		device_printf(dev, "getting ref failed\n");
		return (ENXIO);
	}
	if (clk_enable(sc->ref_clk))
		device_printf(dev, "enable ref failed\n");
	if (clk_get_by_ofw_name(dev, 0, "apb", &sc->apb_clk)) {
		device_printf(dev, "getting apb failed\n");
		return (ENXIO);
	}
	if (clk_enable(sc->apb_clk))
		device_printf(dev, "enable apb failed\n");
	if (clk_get_by_ofw_name(dev, 0, "pipe", &sc->pipe_clk)) {
		device_printf(dev, "getting pipe failed\n");
		return (ENXIO);
	}
	if (clk_enable(sc->pipe_clk))
		device_printf(dev, "enable pipe failed\n");

	/* get & assert reset */
	if (hwreset_get_by_ofw_idx(dev, sc->node, 0, &sc->phy_reset)) {
		device_printf(dev, "Cannot get reset\n");
		return (ENXIO);
	}
	hwreset_assert(sc->phy_reset);

	bzero(&phy_init, sizeof(phy_init));
	phy_init.id = 0;
	phy_init.ofw_node = sc->node;
	if (!(phynode = phynode_create(dev, &rk3568_combphy_phynode_class,
	    &phy_init))) {
		device_printf(dev, "failed to create combphy PHY\n");
		return (ENXIO);
	}
	if (!phynode_register(phynode)) {
		device_printf(dev, "failed to register combphy PHY\n");
		return (ENXIO);
	}
	sc->phynode = phynode;
	sc->mode = 0;

	return (0);
}

static int
rk3568_combphy_map(device_t dev, phandle_t xref, int ncells, pcell_t *cells,
    intptr_t *id)
{
	struct rk3568_combphy_softc *sc = device_get_softc(dev);

	if (phydev_default_ofw_map(dev, xref, ncells, cells, id))
		return (ERANGE);

	/* Store the phy mode that is handed to us in id */
	sc->mode = *id;

	/* Set our id to 0 so the std phy_get_*() works as usual */
	*id = 0;

	return (0);
}

static device_method_t rk3568_combphy_methods[] = {
	DEVMETHOD(device_probe,		rk3568_combphy_probe),
	DEVMETHOD(device_attach,	rk3568_combphy_attach),
	DEVMETHOD(phydev_map,		rk3568_combphy_map),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3568_combphy, rk3568_combphy_driver, rk3568_combphy_methods,
    sizeof(struct simple_mfd_softc), simple_mfd_driver);
EARLY_DRIVER_MODULE(rk3568_combphy, simplebus, rk3568_combphy_driver,
    0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_LATE);
