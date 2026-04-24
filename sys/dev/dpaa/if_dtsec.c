/*-
 * Copyright (c) 2011-2012 Semihalf.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "miibus_if.h"

#include "dpaa_eth.h"
#include "fman.h"
#include "fman_port.h"
#include "if_dtsec.h"

#include "fman_if.h"
#include "fman_port_if.h"

#define	DTSEC_MIN_FRAME_SIZE	64
#define	DTSEC_MAX_FRAME_SIZE	9600

#define	DTSEC_REG_MAXFRM	0x110
#define	DTSEC_REG_IGADDR(i)	(0x080 + 4 * (i))
#define	DTSEC_REG_GADDR(i)	(0x0a0 + 4 * (i))

#define	DTSEC_ECNTRL		0x014
#define	  ECNTRL_R100M		  0x00000008
#define	DTSEC_TCTRL		0x040
#define	  TCTRL_GTS		  0x00000020
#define	DTSEC_RCTRL		0x050
#define	  RCTRL_CFA		  0x00008000
#define	  RCTRL_GHTX		  0x00000400
#define	  RCTRL_GRS		  0x00000020
#define	  RCTRL_MPROM		  0x00000008
#define	DTSEC_MACCFG1		0x100
#define	DTSEC_MACCFG2		0x104
#define	  MACCFG_IF_M		  0x00000300
#define	  MACCFG_IF_10_100	  0x00000100
#define	  MACCFG_IF_1G		  0x00000200
#define	  MACCFG_FULLDUPLEX	  0x00000001
#define	DTSEC_MACSTNADDR1	0x140
#define	DTSEC_MACSTNADDR2	0x144

static void dtsec_if_init_locked(struct dtsec_softc *sc);

/**
 * @group FMan MAC routines.
 * @{
 */

static int
dtsec_fm_mac_init(struct dtsec_softc *sc, uint8_t *mac)
{
	FMAN_GET_REVISION(device_get_parent(sc->sc_base.sc_dev), &sc->sc_base.sc_rev_major,
	    &sc->sc_base.sc_rev_minor);

	if (FMAN_RESET_MAC(device_get_parent(sc->sc_base.sc_dev), sc->sc_base.sc_eth_id) != 0)
		return (ENXIO);

	return (0);
}
/** @} */


/**
 * @group IFnet routines.
 * @{
 */
static int
dtsec_set_mtu(struct dtsec_softc *sc, unsigned int mtu)
{

	mtu += ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN + ETHER_CRC_LEN;

	DTSEC_LOCK_ASSERT(sc);

	if (mtu >= DTSEC_MIN_FRAME_SIZE && mtu <= DTSEC_MAX_FRAME_SIZE) {
		bus_write_4(sc->sc_base.sc_mem, DTSEC_REG_MAXFRM, mtu);
		return (mtu);
	}

	return (0);
}

static u_int
dtsec_hash_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	uint32_t h, *hashtable = arg;

	h = (ether_crc32_be(LLADDR(sdl), ETHER_ADDR_LEN) >> 24) & 0xFF;
	hashtable[(h >> 5)] |= 1 << (0x1F - (h & 0x1F));

	return (1);
}

static void
dtsec_setup_multicast(struct dtsec_softc *sc)
{
	uint32_t hashtable[8] = {};
	int i;

	if (if_getflags(sc->sc_base.sc_ifnet) & IFF_ALLMULTI) {
		for (i = 0; i < 8; i++)
			bus_write_4(sc->sc_base.sc_mem, DTSEC_REG_GADDR(i), 0xFFFFFFFF);
		bus_write_4(sc->sc_base.sc_mem, DTSEC_RCTRL,
		    bus_read_4(sc->sc_base.sc_mem, DTSEC_RCTRL) | RCTRL_MPROM);

		return;
	}
	bus_write_4(sc->sc_base.sc_mem, DTSEC_RCTRL,
	    bus_read_4(sc->sc_base.sc_mem, DTSEC_RCTRL) & ~RCTRL_MPROM);

	if_foreach_llmaddr(sc->sc_base.sc_ifnet, dtsec_hash_maddr, hashtable);
	for (i = 0; i < 8; i++)
		bus_write_4(sc->sc_base.sc_mem, DTSEC_REG_GADDR(i),
		    hashtable[i]);
}

static void
dtsec_if_graceful_stop(struct dtsec_softc *sc)
{
	bus_write_4(sc->sc_base.sc_mem, DTSEC_RCTRL,
	    bus_read_4(sc->sc_base.sc_mem, DTSEC_RCTRL) | RCTRL_GRS);
	if (sc->sc_base.sc_rev_major == 2)
		DELAY(100);
	else
		DELAY(10);

	bus_write_4(sc->sc_base.sc_mem, DTSEC_TCTRL,
	    bus_read_4(sc->sc_base.sc_mem, DTSEC_TCTRL) | TCTRL_GTS);
}

static void
dtsec_if_graceful_start(struct dtsec_softc *sc)
{
	bus_write_4(sc->sc_base.sc_mem, DTSEC_RCTRL,
	    bus_read_4(sc->sc_base.sc_mem, DTSEC_RCTRL) & ~RCTRL_GRS);
	if (sc->sc_base.sc_rev_major == 2)
		DELAY(100);
	else
		DELAY(10);

	bus_write_4(sc->sc_base.sc_mem, DTSEC_TCTRL,
	    bus_read_4(sc->sc_base.sc_mem, DTSEC_TCTRL) & ~TCTRL_GTS);
}

static int
dtsec_if_enable_locked(struct dtsec_softc *sc)
{
	int error;

	DTSEC_LOCK_ASSERT(sc);

	dtsec_if_graceful_start(sc);

	error = FMAN_PORT_ENABLE(sc->sc_base.sc_rx_port);
	if (error != 0)
		return (EIO);

	error = FMAN_PORT_ENABLE(sc->sc_base.sc_tx_port);
	if (error != 0)
		return (EIO);

	dtsec_setup_multicast(sc);

	if_setdrvflagbits(sc->sc_base.sc_ifnet, IFF_DRV_RUNNING, 0);

	/* Refresh link state */
	dtsec_miibus_statchg(sc->sc_base.sc_dev);

	return (0);
}

static int
dtsec_if_disable_locked(struct dtsec_softc *sc)
{
	int error;

	DTSEC_LOCK_ASSERT(sc);

	dtsec_if_graceful_stop(sc);

	error = FMAN_PORT_DISABLE(sc->sc_base.sc_rx_port);
	if (error != 0)
		return (EIO);

	error = FMAN_PORT_DISABLE(sc->sc_base.sc_tx_port);
	if (error != 0)
		return (EIO);

	if_setdrvflagbits(sc->sc_base.sc_ifnet, 0, IFF_DRV_RUNNING);

	return (0);
}

static int
dtsec_if_ioctl(if_t ifp, u_long command, caddr_t data)
{
	struct dtsec_softc *sc;
	struct ifreq *ifr;
	int error;

	sc = if_getsoftc(ifp);
	ifr = (struct ifreq *)data;
	error = 0;

	/* Basic functionality to achieve media status reports */
	switch (command) {
	case SIOCSIFMTU:
		DTSEC_LOCK(sc);
		if (dtsec_set_mtu(sc, ifr->ifr_mtu))
			if_setmtu(ifp, ifr->ifr_mtu);
		else
			error = EINVAL;
		DTSEC_UNLOCK(sc);
		break;
	case SIOCSIFFLAGS:
		DTSEC_LOCK(sc);

		if (if_getflags(ifp) & IFF_UP) {
			if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
				dtsec_if_init_locked(sc);
		} else
			error = dtsec_if_disable_locked(sc);

		DTSEC_UNLOCK(sc);
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_base.sc_mii->mii_media,
		    command);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
	}

	return (error);
}

static void
dtsec_if_tick(void *arg)
{
	struct dtsec_softc *sc;

	sc = arg;

	/* TODO */
	DTSEC_LOCK(sc);

	mii_tick(sc->sc_base.sc_mii);
	callout_reset(&sc->sc_base.sc_tick_callout, hz, dtsec_if_tick, sc);

	DTSEC_UNLOCK(sc);
}

static void
dtsec_if_deinit_locked(struct dtsec_softc *sc)
{

	DTSEC_LOCK_ASSERT(sc);

	DTSEC_UNLOCK(sc);
	callout_drain(&sc->sc_base.sc_tick_callout);
	DTSEC_LOCK(sc);
}

static void
dtsec_if_set_macaddr(struct dtsec_softc *sc, const char *addr)
{
	uint32_t reg;

	reg = (addr[5] << 24) | (addr[4] << 16) | (addr[3] << 8) | addr[2];
	bus_write_4(sc->sc_base.sc_mem, DTSEC_MACSTNADDR1, reg);
	reg = (addr[1] << 24) | (addr[0] << 16);
	bus_write_4(sc->sc_base.sc_mem, DTSEC_MACSTNADDR2, reg);
}

static void
dtsec_if_init_locked(struct dtsec_softc *sc)
{
	int error;
	const char *macaddr;

	DTSEC_LOCK_ASSERT(sc);

	macaddr = if_getlladdr(sc->sc_base.sc_ifnet);
	dtsec_if_set_macaddr(sc, macaddr);

	/* Start MII polling */
	if (sc->sc_base.sc_mii)
		callout_reset(&sc->sc_base.sc_tick_callout, hz,
		    dtsec_if_tick, sc);

	if (if_getflags(sc->sc_base.sc_ifnet) & IFF_UP) {
		error = dtsec_if_enable_locked(sc);
		if (error != 0)
			goto err;
	} else {
		error = dtsec_if_disable_locked(sc);
		if (error != 0)
			goto err;
	}

	return;

err:
	dtsec_if_deinit_locked(sc);
	device_printf(sc->sc_base.sc_dev, "initialization error.\n");
	return;
}

static void
dtsec_if_init(void *data)
{
	struct dtsec_softc *sc;

	sc = data;

	DTSEC_LOCK(sc);
	dtsec_if_init_locked(sc);
	DTSEC_UNLOCK(sc);
}

static void
dtsec_if_start(if_t ifp)
{
	struct dtsec_softc *sc;

	sc = if_getsoftc(ifp);
	DTSEC_LOCK(sc);
	dpaa_eth_if_start_locked(&sc->sc_base);
	DTSEC_UNLOCK(sc);
}

static void
dtsec_if_watchdog(if_t ifp)
{
	/* TODO */
}
/** @} */


/**
 * @group IFmedia routines.
 * @{
 */
static int
dtsec_ifmedia_upd(if_t ifp)
{
	struct dtsec_softc *sc = if_getsoftc(ifp);

	DTSEC_LOCK(sc);
	mii_mediachg(sc->sc_base.sc_mii);
	DTSEC_UNLOCK(sc);

	return (0);
}

static void
dtsec_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct dtsec_softc *sc = if_getsoftc(ifp);

	DTSEC_LOCK(sc);

	mii_pollstat(sc->sc_base.sc_mii);

	ifmr->ifm_active = sc->sc_base.sc_mii->mii_media_active;
	ifmr->ifm_status = sc->sc_base.sc_mii->mii_media_status;

	DTSEC_UNLOCK(sc);
}
/** @} */


/**
 * @group dTSEC bus interface.
 * @{
 */

int
dtsec_attach(device_t dev)
{
	struct dtsec_softc *sc;
	cell_t ports[2];
	phandle_t node;
	int error;
	if_t ifp;

	sc = device_get_softc(dev);

	sc->sc_base.sc_dev = dev;
	node = ofw_bus_get_node(dev);

	/* Init locks */
	mtx_init(&sc->sc_base.sc_lock, device_get_nameunit(dev),
	    "DTSEC Global Lock", MTX_DEF);

	mtx_init(&sc->sc_base.sc_mii_lock, device_get_nameunit(dev),
	    "DTSEC MII Lock", MTX_DEF);

	/* Init callouts */
	callout_init(&sc->sc_base.sc_tick_callout, CALLOUT_MPSAFE);

	/* Create RX buffer pool */
	error = dpaa_eth_pool_rx_init(&sc->sc_base);
	if (error != 0)
		return (EIO);

	/* Create RX frame queue range */
	error = dpaa_eth_fq_rx_init(&sc->sc_base);
	if (error != 0)
		return (EIO);

	/* Create frame info pool */
	error = dpaa_eth_fi_pool_init(&sc->sc_base);
	if (error != 0)
		return (EIO);

	/* Create TX frame queue range */
	error = dpaa_eth_fq_tx_init(&sc->sc_base);
	if (error != 0)
		return (EIO);

	if (OF_getencprop(node, "fsl,fman-ports", ports, sizeof(ports)) < 0) {
		device_printf(dev, "missing ports in device tree\n");
		return (ENXIO);
	}
	/* Init FMan MAC module. */
	error = dtsec_fm_mac_init(sc, sc->sc_base.sc_mac_addr);
	if (error != 0) {
		dtsec_detach(dev);
		return (ENXIO);
	}

	sc->sc_base.sc_rx_port = OF_device_from_xref(ports[0]);
	sc->sc_base.sc_tx_port = OF_device_from_xref(ports[1]);
	dpaa_eth_fm_port_rx_init(&sc->sc_base);
	dpaa_eth_fm_port_tx_init(&sc->sc_base);

	if (sc->sc_base.sc_rx_port == NULL || sc->sc_base.sc_tx_port == NULL) {
		device_printf(dev, "invalid ports");
		dtsec_detach(dev);
		return (ENXIO);
	}

	/* Create network interface for upper layers */
	ifp = sc->sc_base.sc_ifnet = if_alloc(IFT_ETHER);
	if_setsoftc(ifp, sc);

	if_setflags(ifp, IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST);
	if_setinitfn(ifp, dtsec_if_init);
	if_setstartfn(ifp, dtsec_if_start);
	if_setioctlfn(ifp, dtsec_if_ioctl);
	if_setsendqlen(ifp, IFQ_MAXLEN);

	if (sc->sc_base.sc_phy_addr >= 0)
		if_initname(ifp, device_get_name(sc->sc_base.sc_dev),
		    device_get_unit(sc->sc_base.sc_dev));
	else
		if_initname(ifp, "dtsec_phy",
		    device_get_unit(sc->sc_base.sc_dev));

	/* TODO */
#if 0
	if_setsendqlen(ifp, TSEC_TX_NUM_DESC - 1);
	if_setsendqready(ifp);
#endif

	if_setcapabilities(ifp, IFCAP_JUMBO_MTU | IFCAP_VLAN_MTU);
	if_setcapenable(ifp, if_getcapabilities(ifp));

	/* Attach PHY(s) */
	error = mii_attach(sc->sc_base.sc_dev, &sc->sc_base.sc_mii_dev,
	    ifp, dtsec_ifmedia_upd, dtsec_ifmedia_sts, BMSR_DEFCAPMASK,
	    sc->sc_base.sc_phy_addr, MII_OFFSET_ANY, 0);
	if (error) {
		device_printf(sc->sc_base.sc_dev,
		    "attaching PHYs failed: %d\n", error);
		dtsec_detach(sc->sc_base.sc_dev);
		return (error);
	}

	/* Attach to stack */
	ether_ifattach(ifp, sc->sc_base.sc_mac_addr);

	return (0);
}

int
dtsec_detach(device_t dev)
{
	struct dtsec_softc *sc;
	if_t ifp;

	sc = device_get_softc(dev);
	ifp = sc->sc_base.sc_ifnet;

	if (device_is_attached(dev)) {
		ether_ifdetach(ifp);
		/* Shutdown interface */
		DTSEC_LOCK(sc);
		dtsec_if_deinit_locked(sc);
		DTSEC_UNLOCK(sc);
	}

	if (sc->sc_base.sc_ifnet) {
		if_free(sc->sc_base.sc_ifnet);
		sc->sc_base.sc_ifnet = NULL;
	}

	/* Free RX/TX FQRs */
	dpaa_eth_fq_rx_free(&sc->sc_base);
	dpaa_eth_fq_tx_free(&sc->sc_base);

	/* Free frame info pool */
	dpaa_eth_fi_pool_free(&sc->sc_base);

	/* Free RX buffer pool */
	dpaa_eth_pool_rx_free(&sc->sc_base);

	/* Destroy lock */
	mtx_destroy(&sc->sc_base.sc_lock);

	return (0);
}

int
dtsec_suspend(device_t dev)
{

	return (0);
}

int
dtsec_resume(device_t dev)
{

	return (0);
}

int
dtsec_shutdown(device_t dev)
{

	return (0);
}
/** @} */


/**
 * @group MII bus interface.
 * @{
 */
int
dtsec_miibus_readreg(device_t dev, int phy, int reg)
{
	struct dtsec_softc *sc;

	sc = device_get_softc(dev);

	return (MIIBUS_READREG(sc->sc_base.sc_mdio, phy, reg));
}

int
dtsec_miibus_writereg(device_t dev, int phy, int reg, int value)
{

	struct dtsec_softc *sc;

	sc = device_get_softc(dev);

	return (MIIBUS_WRITEREG(sc->sc_base.sc_mdio, phy, reg, value));
}

void
dtsec_miibus_statchg(device_t dev)
{
	struct dtsec_softc *sc;
	uint32_t reg;
	bool duplex;
	int speed;

	sc = device_get_softc(dev);

	DTSEC_LOCK_ASSERT(sc);

	duplex = ((sc->sc_base.sc_mii->mii_media_active & IFM_GMASK) == IFM_FDX);

	switch (IFM_SUBTYPE(sc->sc_base.sc_mii->mii_media_active)) {
	case IFM_1000_T:
	case IFM_1000_SX:
		if (!duplex) {
			device_printf(sc->sc_base.sc_dev,
			    "Only full-duplex supported for 1Gbps speeds");
			return;
		}
		speed = MACCFG_IF_1G;
		break;

	default:
		speed = MACCFG_IF_10_100;
	}

	reg = bus_read_4(sc->sc_base.sc_mem, DTSEC_MACCFG2);
	reg &= ~(MACCFG_IF_M | MACCFG_FULLDUPLEX);

	if (duplex)
		reg |= MACCFG_FULLDUPLEX;
	reg |= speed;
	bus_write_4(sc->sc_base.sc_mem, DTSEC_MACCFG2, reg);

	reg = bus_read_4(sc->sc_base.sc_mem, DTSEC_ECNTRL) & ~ECNTRL_R100M;
	if (IFM_SUBTYPE(sc->sc_base.sc_mii->mii_media_active) == IFM_100_TX)
		reg |= ECNTRL_R100M;
	bus_write_4(sc->sc_base.sc_mem, DTSEC_ECNTRL, reg);
}
/** @} */
