/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * Ethernet media access controller (EMAC)
 * Chapter 17, Altera Cyclone V Device Handbook (CV-5V2 2014.07.22)
 *
 * EMAC is an instance of the Synopsys DesignWare 3504-0
 * Universal 10/100/1000 Ethernet MAC (DWC_gmac).
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/mii/mii_fdt.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_core.h>
#include <dev/dwc/dwc1000_dma.h>

#include "if_dwc_if.h"
#include "gpio_if.h"
#include "miibus_if.h"

static struct resource_spec dwc_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static void dwc_stop_locked(struct dwc_softc *sc);

static void dwc_tick(void *arg);

/*
 * Media functions
 */

static void
dwc_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct dwc_softc *sc;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	mii = sc->mii_softc;
	DWC_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	DWC_UNLOCK(sc);
}

static int
dwc_media_change_locked(struct dwc_softc *sc)
{

	return (mii_mediachg(sc->mii_softc));
}

static int
dwc_media_change(if_t ifp)
{
	struct dwc_softc *sc;
	int error;

	sc = if_getsoftc(ifp);

	DWC_LOCK(sc);
	error = dwc_media_change_locked(sc);
	DWC_UNLOCK(sc);
	return (error);
}

/*
 * if_ functions
 */

static void
dwc_txstart_locked(struct dwc_softc *sc)
{
	if_t ifp;

	DWC_ASSERT_LOCKED(sc);

	if (!sc->link_is_up)
		return;

	ifp = sc->ifp;

	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	dma1000_txstart(sc);
}

static void
dwc_txstart(if_t ifp)
{
	struct dwc_softc *sc = if_getsoftc(ifp);

	DWC_LOCK(sc);
	dwc_txstart_locked(sc);
	DWC_UNLOCK(sc);
}

static void
dwc_init_locked(struct dwc_softc *sc)
{
	if_t ifp = sc->ifp;

	DWC_ASSERT_LOCKED(sc);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	/*
	 * Call mii_mediachg() which will call back into dwc1000_miibus_statchg()
	 * to set up the remaining config registers based on current media.
	 */
	mii_mediachg(sc->mii_softc);

	dwc1000_setup_rxfilter(sc);
	dwc1000_core_setup(sc);
	dwc1000_enable_mac(sc, true);
	dwc1000_enable_csum_offload(sc);
	dma1000_start(sc);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	callout_reset(&sc->dwc_callout, hz, dwc_tick, sc);
}

static void
dwc_init(void *if_softc)
{
	struct dwc_softc *sc = if_softc;

	DWC_LOCK(sc);
	dwc_init_locked(sc);
	DWC_UNLOCK(sc);
}

static void
dwc_stop_locked(struct dwc_softc *sc)
{
	if_t ifp;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->tx_watchdog_count = 0;
	sc->stats_harvest_count = 0;

	callout_stop(&sc->dwc_callout);

	dma1000_stop(sc);
	dwc1000_enable_mac(sc, false);
}

static int
dwc_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct dwc_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int flags, mask, error;

	sc = if_getsoftc(ifp);
	ifr = (struct ifreq *)data;

	error = 0;
	switch (cmd) {
	case SIOCSIFFLAGS:
		DWC_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				flags = if_getflags(ifp) ^ sc->if_flags;
				if ((flags & (IFF_PROMISC|IFF_ALLMULTI)) != 0)
					dwc1000_setup_rxfilter(sc);
			} else {
				if (!sc->is_detaching)
					dwc_init_locked(sc);
			}
		} else {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
				dwc_stop_locked(sc);
		}
		sc->if_flags = if_getflags(ifp);
		DWC_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			DWC_LOCK(sc);
			dwc1000_setup_rxfilter(sc);
			DWC_UNLOCK(sc);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = sc->mii_softc;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
		if (mask & IFCAP_VLAN_MTU) {
			/* No work to do except acknowledge the change took */
			if_togglecapenable(ifp, IFCAP_VLAN_MTU);
		}
		if (mask & IFCAP_RXCSUM)
			if_togglecapenable(ifp, IFCAP_RXCSUM);
		if (mask & IFCAP_TXCSUM)
			if_togglecapenable(ifp, IFCAP_TXCSUM);
		if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0)
			if_sethwassistbits(ifp, CSUM_IP | CSUM_UDP | CSUM_TCP, 0);
		else
			if_sethwassistbits(ifp, 0, CSUM_IP | CSUM_UDP | CSUM_TCP);

		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			DWC_LOCK(sc);
			dwc1000_enable_csum_offload(sc);
			DWC_UNLOCK(sc);
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/*
 * Interrupts functions
 */


static void
dwc_intr(void *arg)
{
	struct dwc_softc *sc;
	int rv;

	sc = arg;
	DWC_LOCK(sc);
	dwc1000_intr(sc);
	rv = dma1000_intr(sc);
	if (rv == EIO) {
		device_printf(sc->dev,
		  "Ethernet DMA error, restarting controller.\n");
		dwc_stop_locked(sc);
		dwc_init_locked(sc);
	}
	DWC_UNLOCK(sc);
}

static void
dwc_tick(void *arg)
{
	struct dwc_softc *sc;
	if_t ifp;
	int link_was_up;

	sc = arg;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
	    return;

	/*
	 * Typical tx watchdog.  If this fires it indicates that we enqueued
	 * packets for output and never got a txdone interrupt for them.  Maybe
	 * it's a missed interrupt somehow, just pretend we got one.
	 */
	if (sc->tx_watchdog_count > 0) {
		if (--sc->tx_watchdog_count == 0) {
			dma1000_txfinish_locked(sc);
		}
	}

	/* Gather stats from hardware counters. */
	dwc1000_harvest_stats(sc);

	/* Check the media status. */
	link_was_up = sc->link_is_up;
	mii_tick(sc->mii_softc);
	if (sc->link_is_up && !link_was_up)
		dwc_txstart_locked(sc);

	/* Schedule another check one second from now. */
	callout_reset(&sc->dwc_callout, hz, dwc_tick, sc);
}

static int
dwc_reset_phy(struct dwc_softc *sc)
{
	pcell_t gpio_prop[4];
	pcell_t delay_prop[3];
	phandle_t gpio_node;
	device_t gpio;
	uint32_t pin, flags;
	uint32_t pin_value;

	/*
	 * All those properties are deprecated but still used in some DTS.
	 * The new way to deal with this is to use the generic bindings
	 * present in the ethernet-phy node.
	 */
	if (OF_getencprop(sc->node, "snps,reset-gpio",
	    gpio_prop, sizeof(gpio_prop)) <= 0)
		return (0);

	if (OF_getencprop(sc->node, "snps,reset-delays-us",
	    delay_prop, sizeof(delay_prop)) <= 0) {
		device_printf(sc->dev,
		    "Wrong property for snps,reset-delays-us");
		return (ENXIO);
	}

	gpio_node = OF_node_from_xref(gpio_prop[0]);
	if ((gpio = OF_device_from_xref(gpio_prop[0])) == NULL) {
		device_printf(sc->dev,
		    "Can't find gpio controller for phy reset\n");
		return (ENXIO);
	}

	if (GPIO_MAP_GPIOS(gpio, sc->node, gpio_node,
	    nitems(gpio_prop) - 1,
	    gpio_prop + 1, &pin, &flags) != 0) {
		device_printf(sc->dev, "Can't map gpio for phy reset\n");
		return (ENXIO);
	}

	pin_value = GPIO_PIN_LOW;
	if (OF_hasprop(sc->node, "snps,reset-active-low"))
		pin_value = GPIO_PIN_HIGH;

	GPIO_PIN_SETFLAGS(gpio, pin, GPIO_PIN_OUTPUT);
	GPIO_PIN_SET(gpio, pin, pin_value);
	DELAY(delay_prop[0] * 5);
	GPIO_PIN_SET(gpio, pin, !pin_value);
	DELAY(delay_prop[1] * 5);
	GPIO_PIN_SET(gpio, pin, pin_value);
	DELAY(delay_prop[2] * 5);

	return (0);
}

static int
dwc_clock_init(struct dwc_softc *sc)
{
	int rv;
	int64_t freq;

	/* Required clock */
	rv = clk_get_by_ofw_name(sc->dev, 0, "stmmaceth", &sc->clk_stmmaceth);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get GMAC main clock\n");
		return (ENXIO);
	}
	if ((rv = clk_enable(sc->clk_stmmaceth)) != 0) {
		device_printf(sc->dev, "could not enable main clock\n");
		return (rv);
	}

	/* Optional clock */
	rv = clk_get_by_ofw_name(sc->dev, 0, "pclk", &sc->clk_pclk);
	if (rv != 0)
		return (0);
	if ((rv = clk_enable(sc->clk_pclk)) != 0) {
		device_printf(sc->dev, "could not enable peripheral clock\n");
		return (rv);
	}

	if (bootverbose) {
		clk_get_freq(sc->clk_stmmaceth, &freq);
		device_printf(sc->dev, "MAC clock(%s) freq: %jd\n",
		    clk_get_name(sc->clk_stmmaceth), (intmax_t)freq);
	}

	return (0);
}

static int
dwc_reset_deassert(struct dwc_softc *sc)
{
	int rv;

	/* Required reset */
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "stmmaceth", &sc->rst_stmmaceth);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get GMAC reset\n");
		return (ENXIO);
	}
	rv = hwreset_deassert(sc->rst_stmmaceth);
	if (rv != 0) {
		device_printf(sc->dev, "could not de-assert GMAC reset\n");
		return (rv);
	}

	/* Optional reset */
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "ahb", &sc->rst_ahb);
	if (rv != 0)
		return (0);
	rv = hwreset_deassert(sc->rst_ahb);
	if (rv != 0) {
		device_printf(sc->dev, "could not de-assert AHB reset\n");
		return (rv);
	}

	return (0);
}

/*
 * Probe/Attach functions
 */

static int
dwc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "snps,dwmac"))
		return (ENXIO);

	device_set_desc(dev, "Gigabit Ethernet Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
dwc_attach(device_t dev)
{
	uint8_t macaddr[ETHER_ADDR_LEN];
	struct dwc_softc *sc;
	if_t ifp;
	int error;
	uint32_t pbl;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rx_idx = 0;
	sc->tx_desccount = TX_DESC_COUNT;
	sc->tx_mapcount = 0;

	sc->node = ofw_bus_get_node(dev);
	sc->phy_mode = mii_fdt_get_contype(sc->node);
	switch (sc->phy_mode) {
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
	case MII_CONTYPE_RMII:
	case MII_CONTYPE_MII:
		break;
	default:
		device_printf(dev, "Unsupported MII type\n");
		return (ENXIO);
	}

	if (OF_getencprop(sc->node, "snps,pbl", &pbl, sizeof(uint32_t)) <= 0)
		pbl = DMA_DEFAULT_PBL;
	if (OF_getencprop(sc->node, "snps,txpbl", &sc->txpbl, sizeof(uint32_t)) <= 0)
		sc->txpbl = pbl;
	if (OF_getencprop(sc->node, "snps,rxpbl", &sc->rxpbl, sizeof(uint32_t)) <= 0)
		sc->rxpbl = pbl;
	if (OF_hasprop(sc->node, "snps,no-pbl-x8") == 1)
		sc->nopblx8 = true;
	if (OF_hasprop(sc->node, "snps,fixed-burst") == 1)
		sc->fixed_burst = true;
	if (OF_hasprop(sc->node, "snps,mixed-burst") == 1)
		sc->mixed_burst = true;
	if (OF_hasprop(sc->node, "snps,aal") == 1)
		sc->aal = true;

	error = clk_set_assigned(dev, ofw_bus_get_node(dev));
	if (error != 0) {
		device_printf(dev, "clk_set_assigned failed\n");
		return (error);
	}

	/* Enable main clock */
	if ((error = dwc_clock_init(sc)) != 0)
		return (error);
	/* De-assert main reset */
	if ((error = dwc_reset_deassert(sc)) != 0)
		return (error);

	if (IF_DWC_INIT(dev) != 0)
		return (ENXIO);

	if ((sc->mii_clk = IF_DWC_MII_CLK(dev)) < 0) {
		device_printf(dev, "Cannot get mii clock value %d\n", -sc->mii_clk);
		return (ENXIO);
	}

	if (bus_alloc_resources(dev, dwc_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Read MAC before reset */
	dwc1000_get_hwaddr(sc, macaddr);

	/* Reset the PHY if needed */
	if (dwc_reset_phy(sc) != 0) {
		device_printf(dev, "Can't reset the PHY\n");
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	/* Reset */
	if ((error = dma1000_reset(sc)) != 0) {
		device_printf(sc->dev, "Can't reset DMA controller.\n");
		bus_release_resources(sc->dev, dwc_spec, sc->res);
		return (error);
	}

	if (dma1000_init(sc)) {
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	callout_init_mtx(&sc->dwc_callout, &sc->mtx, 0);

	/* Setup interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dwc_intr, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	/* Set up the ethernet interface. */
	sc->ifp = ifp = if_alloc(IFT_ETHER);

	if_setsoftc(ifp, sc);
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_setflags(sc->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(ifp, dwc_txstart);
	if_setioctlfn(ifp, dwc_ioctl);
	if_setinitfn(ifp, dwc_init);
	if_setsendqlen(ifp, TX_MAP_COUNT - 1);
	if_setsendqready(sc->ifp);
	if_sethwassist(sc->ifp, CSUM_IP | CSUM_UDP | CSUM_TCP);
	if_setcapabilities(sc->ifp, IFCAP_VLAN_MTU | IFCAP_HWCSUM);
	if_setcapenable(sc->ifp, if_getcapabilities(sc->ifp));

	/* Attach the mii driver. */
	error = mii_attach(dev, &sc->miibus, ifp, dwc_media_change,
	    dwc_media_status, BMSR_DEFCAPMASK, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	if (error != 0) {
		device_printf(dev, "PHY attach failed\n");
		bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}
	sc->mii_softc = device_get_softc(sc->miibus);

	/* All ready to run, attach the ethernet interface. */
	ether_ifattach(ifp, macaddr);
	sc->is_attached = true;

	return (0);
}

static int
dwc_detach(device_t dev)
{
	struct dwc_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Disable and tear down interrupts before anything else, so we don't
	 * race with the handler.
	 */
	dwc1000_intr_disable(sc);
	if (sc->intr_cookie != NULL) {
		bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);
	}

	if (sc->is_attached) {
		DWC_LOCK(sc);
		sc->is_detaching = true;
		dwc_stop_locked(sc);
		DWC_UNLOCK(sc);
		callout_drain(&sc->dwc_callout);
		ether_ifdetach(sc->ifp);
	}

	if (sc->miibus != NULL) {
		device_delete_child(dev, sc->miibus);
		sc->miibus = NULL;
	}
	bus_generic_detach(dev);

	/* Free DMA descriptors */
	dma1000_free(sc);

	if (sc->ifp != NULL) {
		if_free(sc->ifp);
		sc->ifp = NULL;
	}

	bus_release_resources(dev, dwc_spec, sc->res);

	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t dwc_methods[] = {
	DEVMETHOD(device_probe,		dwc_probe),
	DEVMETHOD(device_attach,	dwc_attach),
	DEVMETHOD(device_detach,	dwc_detach),

	/* MII Interface */
	DEVMETHOD(miibus_readreg,	dwc1000_miibus_read_reg),
	DEVMETHOD(miibus_writereg,	dwc1000_miibus_write_reg),
	DEVMETHOD(miibus_statchg,	dwc1000_miibus_statchg),

	{ 0, 0 }
};

driver_t dwc_driver = {
	"dwc",
	dwc_methods,
	sizeof(struct dwc_softc),
};

DRIVER_MODULE(dwc, simplebus, dwc_driver, 0, 0);
DRIVER_MODULE(miibus, dwc, miibus_driver, 0, 0);

MODULE_DEPEND(dwc, ether, 1, 1, 1);
MODULE_DEPEND(dwc, miibus, 1, 1, 1);
