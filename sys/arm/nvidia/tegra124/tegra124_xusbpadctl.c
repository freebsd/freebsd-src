/*-
 * Copyright (c) 2016 Michal Meloun <mmel@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/fdt/fdt_common.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <gnu/dts/include/dt-bindings/pinctrl/pinctrl-tegra-xusb.h>

#include "phy_if.h"

#define	XUSB_PADCTL_USB2_PAD_MUX		0x004

#define	XUSB_PADCTL_ELPG_PROGRAM		0x01C
#define	 ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN		(1 << 26)
#define	 ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY	(1 << 25)
#define	 ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN		(1 << 24)

#define	XUSB_PADCTL_IOPHY_PLL_P0_CTL1		0x040
#define	 IOPHY_PLL_P0_CTL1_PLL0_LOCKDET			(1 << 19)
#define	 IOPHY_PLL_P0_CTL1_REFCLK_SEL_MASK		(0xf<< 12)
#define	 IOPHY_PLL_P0_CTL1_PLL_RST			(1 << 1)

#define	XUSB_PADCTL_IOPHY_PLL_P0_CTL2		0x044
#define	 IOPHY_PLL_P0_CTL2_REFCLKBUF_EN			(1 << 6)
#define	 IOPHY_PLL_P0_CTL2_TXCLKREF_EN			(1 << 5)
#define	 IOPHY_PLL_P0_CTL2_TXCLKREF_SEL			(1 << 4)


#define	XUSB_PADCTL_USB3_PAD_MUX		0x134

#define	XUSB_PADCTL_IOPHY_PLL_S0_CTL1		0x138
#define	 IOPHY_PLL_S0_CTL1_PLL1_LOCKDET			(1 << 27)
#define	 IOPHY_PLL_S0_CTL1_PLL1_MODE			(1 << 24)
#define	 IOPHY_PLL_S0_CTL1_PLL_PWR_OVRD			(1 << 3)
#define	 IOPHY_PLL_S0_CTL1_PLL_RST_L			(1 << 1)
#define	 IOPHY_PLL_S0_CTL1_PLL_IDDQ			(1 << 0)

#define	XUSB_PADCTL_IOPHY_PLL_S0_CTL2		0x13C
#define	XUSB_PADCTL_IOPHY_PLL_S0_CTL3		0x140
#define	XUSB_PADCTL_IOPHY_PLL_S0_CTL4		0x144

#define	XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL1	0x148
#define	 IOPHY_MISC_PAD_S0_CTL1_IDDQ_OVRD		(1 << 1)
#define	 IOPHY_MISC_PAD_S0_CTL1_IDDQ			(1 << 0)

#define	XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL2	0x14C
#define	XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL3	0x150
#define	XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL4	0x154
#define	XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL5	0x158
#define	XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL6	0x15C

struct lane_cfg {
	char	*function;
	char 	**lanes;
	int 	iddq;
};

struct xusbpadctl_softc {
	device_t	dev;
	struct resource	*mem_res;
	hwreset_t		rst;
	int		phy_ena_cnt;
};

static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra124-xusb-padctl",	1},
	{NULL,				0},
};

struct padctl_lane {
	const char *name;
	bus_size_t reg;
	uint32_t shift;
	uint32_t mask;
	int iddq;
	char **mux;
	int nmux;
};

static char *otg_mux[] = {"snps", "xusb", "uart", "rsvd"};
static char *usb_mux[] = {"snps", "xusb"};
static char *pci_mux[] = {"pcie", "usb3", "sata", "rsvd"};

#define	LANE(n, r, s, m, i, mx)					\
{								\
	.name = n,						\
	.reg = r,						\
	.shift = s,						\
	.mask = m,						\
	.iddq = i,						\
	.mux = mx,						\
	.nmux = nitems(mx),					\
}

static const struct padctl_lane lanes_tbl[] = {
	LANE("otg-0",  XUSB_PADCTL_USB2_PAD_MUX,  0, 0x3, -1, otg_mux),
	LANE("otg-1",  XUSB_PADCTL_USB2_PAD_MUX,  2, 0x3, -1, otg_mux),
	LANE("otg-2",  XUSB_PADCTL_USB2_PAD_MUX,  4, 0x3, -1, otg_mux),
	LANE("ulpi-0", XUSB_PADCTL_USB2_PAD_MUX, 12, 0x1, -1, usb_mux),
	LANE("hsic-0", XUSB_PADCTL_USB2_PAD_MUX, 14, 0x1, -1, usb_mux),
	LANE("hsic-1", XUSB_PADCTL_USB2_PAD_MUX, 15, 0x1, -1, usb_mux),
	LANE("pcie-0", XUSB_PADCTL_USB3_PAD_MUX, 16, 0x3,  1, pci_mux),
	LANE("pcie-1", XUSB_PADCTL_USB3_PAD_MUX, 18, 0x3,  2, pci_mux),
	LANE("pcie-2", XUSB_PADCTL_USB3_PAD_MUX, 20, 0x3,  3, pci_mux),
	LANE("pcie-3", XUSB_PADCTL_USB3_PAD_MUX, 22, 0x3,  4, pci_mux),
	LANE("pcie-4", XUSB_PADCTL_USB3_PAD_MUX, 24, 0x3,  5, pci_mux),
	LANE("sata-0", XUSB_PADCTL_USB3_PAD_MUX, 26, 0x3,  6, pci_mux),
};

static int
xusbpadctl_mux_function(const struct padctl_lane *lane, char *fnc_name)
{
	int i;

	for (i = 0; i < lane->nmux; i++) {
		if (strcmp(fnc_name, lane->mux[i]) == 0)
			return 	(i);
	}

	return (-1);
}

static int
xusbpadctl_config_lane(struct xusbpadctl_softc *sc, char *lane_name,
    const struct padctl_lane *lane, struct lane_cfg *cfg)
{

	int tmp;
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, lane->reg);
	if (cfg->function != NULL) {
		tmp = xusbpadctl_mux_function(lane, cfg->function);
		if (tmp == -1) {
			device_printf(sc->dev,
			    "Unknown function %s for lane %s\n", cfg->function,
			    lane_name);
			return (EINVAL);
		}
		reg &= ~(lane->mask << lane->shift);
		reg |=  (tmp & lane->mask) << lane->shift;
	}
	if (cfg->iddq != -1) {
		if (lane->iddq == -1) {
			device_printf(sc->dev, "Invalid IDDQ for lane %s\n",
			lane_name);
			return (EINVAL);
		}
		if (cfg->iddq != 0)
			reg &= ~(1 << lane->iddq);
		else
			reg |= 1 << lane->iddq;
	}

	bus_write_4(sc->mem_res, lane->reg, reg);
	return (0);
}

static const struct padctl_lane *
xusbpadctl_search_lane(char *lane_name)
{
	int i;

	for (i = 0; i < nitems(lanes_tbl); i++) {
		if (strcmp(lane_name, lanes_tbl[i].name) == 0)
			return 	(&lanes_tbl[i]);
	}

	return (NULL);
}

static int
xusbpadctl_config_node(struct xusbpadctl_softc *sc, char *lane_name,
    struct lane_cfg *cfg)
{
	const struct padctl_lane *lane;
	int rv;

	lane = xusbpadctl_search_lane(lane_name);
	if (lane == NULL) {
		device_printf(sc->dev, "Unknown lane: %s\n", lane_name);
		return (ENXIO);
	}
	rv = xusbpadctl_config_lane(sc, lane_name, lane, cfg);
	return (rv);
}

static int
xusbpadctl_read_node(struct xusbpadctl_softc *sc, phandle_t node,
    struct lane_cfg *cfg, char **lanes, int *llanes)
{
	int rv;

	*llanes = OF_getprop_alloc(node, "nvidia,lanes", 1, (void **)lanes);
	if (*llanes <= 0)
		return (ENOENT);

	/* Read function (mux) settings. */
	rv = OF_getprop_alloc(node, "nvidia,function", 1,
	    (void **)&cfg->function);
	if (rv <= 0)
		cfg->function = NULL;
	/* Read numeric properties. */
	rv = OF_getencprop(node, "nvidia,iddq", &cfg->iddq,
	    sizeof(cfg->iddq));
	if (rv <= 0)
		cfg->iddq = -1;
	return (0);
}

static int
xusbpadctl_process_node(struct xusbpadctl_softc *sc, phandle_t node)
{
	struct lane_cfg cfg;
	char *lanes, *lname;
	int i, len, llanes, rv;

	rv = xusbpadctl_read_node(sc, node, &cfg, &lanes, &llanes);
	if (rv != 0)
		return (rv);

	len = 0;
	lname = lanes;
	do {
		i = strlen(lname) + 1;
		rv = xusbpadctl_config_node(sc, lname, &cfg);
		if (rv != 0)
			device_printf(sc->dev,
			    "Cannot configure lane: %s: %d\n", lname, rv);

		len += i;
		lname += i;
	} while (len < llanes);

	if (lanes != NULL)
		OF_prop_free(lanes);
	if (cfg.function != NULL)
		OF_prop_free(cfg.function);
	return (rv);
}


static int
xusbpadctl_pinctrl_cfg(device_t dev, phandle_t cfgxref)
{
	struct xusbpadctl_softc *sc;
	phandle_t node, cfgnode;
	int rv;

	sc = device_get_softc(dev);
	cfgnode = OF_node_from_xref(cfgxref);

	rv = 0;
	for (node = OF_child(cfgnode); node != 0; node = OF_peer(node)) {
		if (!fdt_is_enabled(node))
			continue;
		rv = xusbpadctl_process_node(sc, node);
		if (rv != 0)
			return (rv);
	}

	return (rv);
}

static int
xusbpadctl_phy_pcie_powerup(struct xusbpadctl_softc *sc)
{
	uint32_t reg;
	int i;

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1);
	reg &= ~IOPHY_PLL_P0_CTL1_REFCLK_SEL_MASK;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL2);
	reg |= IOPHY_PLL_P0_CTL2_REFCLKBUF_EN;
	reg |= IOPHY_PLL_P0_CTL2_TXCLKREF_EN;
	reg |= IOPHY_PLL_P0_CTL2_TXCLKREF_SEL;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL2, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1);
	reg |= IOPHY_PLL_P0_CTL1_PLL_RST;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1, reg);
	DELAY(100);

	for (i = 0; i < 100; i++) {
		reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1);
		if (reg & IOPHY_PLL_P0_CTL1_PLL0_LOCKDET)
			return (0);
		DELAY(10);
	}

	return (ETIMEDOUT);
}


static int
xusbpadctl_phy_pcie_powerdown(struct xusbpadctl_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1);
	reg &= ~IOPHY_PLL_P0_CTL1_PLL_RST;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_P0_CTL1, reg);
	DELAY(100);
	return (0);

}

static int
xusbpadctl_phy_sata_powerup(struct xusbpadctl_softc *sc)
{
	uint32_t reg;
	int i;

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL1);
	reg &= ~IOPHY_MISC_PAD_S0_CTL1_IDDQ_OVRD;
	reg &= ~IOPHY_MISC_PAD_S0_CTL1_IDDQ;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL1, reg);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
	reg &= ~IOPHY_PLL_S0_CTL1_PLL_PWR_OVRD;
	reg &= ~IOPHY_PLL_S0_CTL1_PLL_IDDQ;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1, reg);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
	reg |= IOPHY_PLL_S0_CTL1_PLL1_MODE;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1, reg);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
	reg |= IOPHY_PLL_S0_CTL1_PLL_RST_L;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1, reg);

	for (i = 100; i >= 0; i--) {
		reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
		if (reg & IOPHY_PLL_S0_CTL1_PLL1_LOCKDET)
			break;
		DELAY(100);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Failed to power up SATA phy\n");
		return (ETIMEDOUT);
	}

	return (0);
}

static int
xusbpadctl_phy_sata_powerdown(struct xusbpadctl_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
	reg &= ~IOPHY_PLL_S0_CTL1_PLL_RST_L;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
	reg &= ~IOPHY_PLL_S0_CTL1_PLL1_MODE;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1);
	reg |= IOPHY_PLL_S0_CTL1_PLL_PWR_OVRD;
	reg |= IOPHY_PLL_S0_CTL1_PLL_IDDQ;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_PLL_S0_CTL1, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL1);
	reg |= IOPHY_MISC_PAD_S0_CTL1_IDDQ_OVRD;
	reg |= IOPHY_MISC_PAD_S0_CTL1_IDDQ;
	bus_write_4(sc->mem_res, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL1, reg);
	DELAY(100);

	return (0);
}

static int
xusbpadctl_phy_powerup(struct xusbpadctl_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM);
	reg &= ~ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	bus_write_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM);
	reg &= ~ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	bus_write_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM);
	reg &= ~ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	bus_write_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM, reg);
	DELAY(100);

	return (0);
}

static int
xusbpadctl_phy_powerdown(struct xusbpadctl_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM);
	reg |= ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	bus_write_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM);
	reg |= ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	bus_write_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM, reg);
	DELAY(100);

	reg = bus_read_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM);
	reg |= ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	bus_write_4(sc->mem_res, XUSB_PADCTL_ELPG_PROGRAM, reg);
	DELAY(100);

	return (0);
}

static int
xusbpadctl_phy_enable(device_t dev, intptr_t id, bool enable)
{
	struct xusbpadctl_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	if ((id != TEGRA_XUSB_PADCTL_PCIE) &&
	    (id != TEGRA_XUSB_PADCTL_SATA)) {
		device_printf(dev, "Unknown phy: %d\n", id);
		return (ENXIO);
	}

	rv = 0;
	if (enable) {
		if (sc->phy_ena_cnt == 0) {
			rv = xusbpadctl_phy_powerup(sc);
			if (rv != 0)
				return (rv);
		}
		sc->phy_ena_cnt++;
	}

	if (id == TEGRA_XUSB_PADCTL_PCIE) {
		if (enable)
			rv = xusbpadctl_phy_pcie_powerup(sc);
		else
			rv = xusbpadctl_phy_pcie_powerdown(sc);
		if (rv != 0)
			return (rv);
	} else if (id == TEGRA_XUSB_PADCTL_SATA) {
		if (enable)
			rv = xusbpadctl_phy_sata_powerup(sc);
		else
			rv = xusbpadctl_phy_sata_powerdown(sc);
		if (rv != 0)
			return (rv);
	}
	if (!enable) {
		 if (sc->phy_ena_cnt == 1) {
			rv = xusbpadctl_phy_powerdown(sc);
			if (rv != 0)
				return (rv);
		}
		sc->phy_ena_cnt--;
	}

	return (0);
}

static int
xusbpadctl_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Tegra XUSB phy");
	return (BUS_PROBE_DEFAULT);
}

static int
xusbpadctl_detach(device_t dev)
{

	/* This device is always present. */
	return (EBUSY);
}

static int
xusbpadctl_attach(device_t dev)
{
	struct xusbpadctl_softc * sc;
	int rid, rv;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}

	node = ofw_bus_get_node(dev);
	rv = hwreset_get_by_ofw_name(dev, "padctl", &sc->rst);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'padctl' reset: %d\n", rv);
		return (rv);
	}
	rv = hwreset_deassert(sc->rst);
	if (rv != 0) {
		device_printf(dev, "Cannot unreset 'padctl' reset: %d\n", rv);
		return (rv);
	}

	/* Register as a pinctrl device and use default configuration */
	fdt_pinctrl_register(dev, NULL);
	fdt_pinctrl_configure_by_name(dev, "default");
	phy_register_provider(dev);

	return (0);
}


static device_method_t tegra_xusbpadctl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         xusbpadctl_probe),
	DEVMETHOD(device_attach,        xusbpadctl_attach),
	DEVMETHOD(device_detach,        xusbpadctl_detach),

	/* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure, xusbpadctl_pinctrl_cfg),

	/* phy interface */
	DEVMETHOD(phy_enable,		xusbpadctl_phy_enable),

	DEVMETHOD_END
};

static driver_t tegra_xusbpadctl_driver = {
	"tegra_xusbpadctl",
	tegra_xusbpadctl_methods,
	sizeof(struct xusbpadctl_softc),
};

static devclass_t tegra_xusbpadctl_devclass;

EARLY_DRIVER_MODULE(tegra_xusbpadctl, simplebus, tegra_xusbpadctl_driver,
    tegra_xusbpadctl_devclass, 0, 0, 73);
