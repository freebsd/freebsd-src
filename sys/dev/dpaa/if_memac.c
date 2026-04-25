/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
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
#include "if_memac.h"

#include "fman_if.h"
#include "fman_port_if.h"

#define	MEMAC_MIN_FRAME_SIZE	64
#define	MEMAC_MAX_FRAME_SIZE	32736

#define	MEMAC_COMMAND_CONFIG		0x008
#define	  COMMAND_CONFIG_RXSTP		  0x20000000
#define	  COMMAND_CONFIG_NO_LEN_CHK	  0x00020000
#define	  COMMAND_CONFIG_SWR		  0x00001000
#define	  COMMAND_CONFIG_TXP		  0x00000800
#define	  COMMAND_CONFIG_CRC		  0x00000040
#define	  COMMAND_CONFIG_PROMISC	  0x00000010
#define	  COMMAND_CONFIG_RX_EN		  0x00000002
#define	  COMMAND_CONFIG_TX_EN		  0x00000001
#define	MEMAC_MAC_ADDR_0		0x00c
#define	MEMAC_MAC_ADDR_1		0x010
#define	MEMAC_REG_MAXFRM		0x14
#define	MEMAC_REG_TX_FIFO_SECTIONS	0x020
#define	  TX_FIFO_SECTIONS_TX_EMPTY_M	  0xffff0000
#define	  TX_FIFO_SECTIONS_TX_EMPTY_S	  16
#define	  TX_FIFO_SECTIONS_TX_AVAIL_M	  0x0000ffff

#define	HASHTABLE_CTRL		0x02c
#define	  CTRL_MCAST		  0x00000100
#define	  CTRL_HASH_ADDR_M	  0x0000003f
#define	  HASHTABLE_SIZE	  64
#define	MEMAC_IEVENT		0x040
#define	  IEVENT_RX_EMPTY	  0x00000040
#define	  IEVENT_TX_EMPTY	  0x00000020
#define	MEMAC_CL01_PAUSE_QUANTA	0x054
#define	MEMAC_IF_MODE		0x300
#define	  IF_MODE_ENA		  0x00008000
#define	  IF_MODE_SSP_M		  0x00006000
#define	  IF_MODE_SSP_100MB	  0x00000000
#define	  IF_MODE_SSP_10MB	  0x00002000
#define	  IF_MODE_SSP_1GB	  0x00004000
#define	  IF_MODE_SFD		  0x00001000
#define	  IF_MODE_MSG		  0x00000200
#define	  IF_MODE_HG		  0x00000100
#define	  IF_MODE_HD		  0x00000040
#define	  IF_MODE_RLP		  0x00000020
#define	  IF_MODE_RG		  0x00000004
#define	  IF_MODE_IFMODE_M	  0x00000003
#define	  IF_MODE_IFMODE_XGMII	  0x00000000
#define	  IF_MODE_IFMODE_MII	  0x00000001
#define	  IF_MODE_IFMODE_GMII	  0x00000002

#define	DEFAULT_PAUSE_QUANTA	0xf000

#define	DPAA_CSUM_TX_OFFLOAD	(CSUM_IP | CSUM_DELAY_DATA | CSUM_DELAY_DATA_IPV6)


/**
 * @group FMan MAC routines.
 * @{
 */
#define	MEMAC_MAC_EXCEPTIONS_END	(-1)

static void memac_if_init_locked(struct memac_softc *sc);

static int
memac_fm_mac_init(struct memac_softc *sc, uint8_t *mac)
{
	uint32_t reg;

	FMAN_GET_REVISION(device_get_parent(sc->sc_base.sc_dev), &sc->sc_base.sc_rev_major,
	    &sc->sc_base.sc_rev_minor);

	if (FMAN_RESET_MAC(device_get_parent(sc->sc_base.sc_dev), sc->sc_base.sc_eth_id) != 0)
		return (ENXIO);

	reg = bus_read_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG);
	reg |= COMMAND_CONFIG_SWR;
	bus_write_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG, reg);

	while (bus_read_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG) & COMMAND_CONFIG_SWR)
		;

	/* TODO: TX_FIFO_SECTIONS */
	/* TODO: CL01 pause quantum */
	bus_write_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG,
	    COMMAND_CONFIG_NO_LEN_CHK | COMMAND_CONFIG_TXP | COMMAND_CONFIG_CRC);

	reg = bus_read_4(sc->sc_base.sc_mem, MEMAC_IF_MODE);
	reg &= ~(IF_MODE_IFMODE_M | IF_MODE_RG);
	switch (sc->sc_base.sc_mac_enet_mode) {
	case MII_CONTYPE_RGMII:
		reg |= IF_MODE_RG;
		/* FALLTHROUGH */
	case MII_CONTYPE_GMII:
	case MII_CONTYPE_SGMII:
	case MII_CONTYPE_QSGMII:
		reg |= IF_MODE_IFMODE_GMII;
		break;
	case MII_CONTYPE_RMII:
		reg |= IF_MODE_RG;
		/* FALLTHROUGH */
	case MII_CONTYPE_MII:
		reg |= IF_MODE_IFMODE_MII;
		break;
	}

	bus_write_4(sc->sc_base.sc_mem, MEMAC_IF_MODE, reg);

	return (0);
}
/** @} */


/**
 * @group IFnet routines.
 * @{
 */
static int
memac_set_mtu(struct memac_softc *sc, unsigned int mtu)
{

	mtu += ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN + ETHER_CRC_LEN;

	MEMAC_LOCK_ASSERT(sc);

	if (mtu >= MEMAC_MIN_FRAME_SIZE && mtu <= MEMAC_MAX_FRAME_SIZE) {
		bus_write_4(sc->sc_base.sc_mem, MEMAC_REG_MAXFRM, mtu);
		return (mtu);
	}

	return (0);
}

static u_int
memac_hash_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct memac_softc *sc = arg;
	uint8_t *addr = LLADDR(sdl);
	uint32_t hash = 0;
	uint8_t a, h;

	/* Hash is 6 bits, composed if [XOR{47:40},XOR{39:32},....] */
	for (int i = 0; i < 6; i++) {
		a = addr[i];
		h = 0;
		for (int j = 0; j < 8; j++, a >>= 1) {
			h ^= (a & 0x1);
		}
		hash |= (h << i);
	}
	bus_write_4(sc->sc_base.sc_mem, HASHTABLE_CTRL, hash | CTRL_MCAST);

	return (1);
}

static void
memac_setup_multicast(struct memac_softc *sc)
{

	if (if_getflags(sc->sc_base.sc_ifnet) & IFF_ALLMULTI) {
		for (int i = 0; i < HASHTABLE_SIZE; i++)
			bus_write_4(sc->sc_base.sc_mem,
			    HASHTABLE_CTRL, CTRL_MCAST | i);
	} else {
		/* Clear the hash table */
		for (int i = 0; i < HASHTABLE_SIZE; i++)
			bus_write_4(sc->sc_base.sc_mem,
			    HASHTABLE_CTRL, i);
	}

	if_foreach_llmaddr(sc->sc_base.sc_ifnet, memac_hash_maddr, sc);
}

static void
memac_setup_promisc(struct memac_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG);
	reg &= ~COMMAND_CONFIG_PROMISC;

	if ((if_getflags(sc->sc_base.sc_ifnet) & IFF_PROMISC) != 0)
		bus_write_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG,
		    reg | COMMAND_CONFIG_PROMISC);
}

static void
memac_if_graceful_stop(struct memac_softc *sc)
{
	struct resource *regs = sc->sc_base.sc_mem;
	uint32_t reg;

	reg = bus_read_4(regs, MEMAC_COMMAND_CONFIG);
	reg |= COMMAND_CONFIG_RXSTP;

	bus_write_4(regs, MEMAC_COMMAND_CONFIG, reg);
	while ((bus_read_4(regs, MEMAC_IEVENT) & IEVENT_RX_EMPTY) == 0)
		;
	reg &= COMMAND_CONFIG_RX_EN;
	bus_write_4(regs, MEMAC_COMMAND_CONFIG, reg);

	while ((bus_read_4(regs, MEMAC_IEVENT) & IEVENT_TX_EMPTY) == 0)
		;
	bus_write_4(regs, MEMAC_COMMAND_CONFIG, reg & ~COMMAND_CONFIG_TX_EN);
}

static void
memac_mac_enable(struct memac_softc *sc)
{
	uint32_t reg = bus_read_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG);

	reg |= (COMMAND_CONFIG_RX_EN | COMMAND_CONFIG_TX_EN);

	bus_write_4(sc->sc_base.sc_mem, MEMAC_COMMAND_CONFIG, reg);
}

static int
memac_if_enable_locked(struct memac_softc *sc)
{
	int error;

	MEMAC_LOCK_ASSERT(sc);

	memac_set_mtu(sc, if_getmtu(sc->sc_base.sc_ifnet));
	memac_mac_enable(sc);

	error = FMAN_PORT_ENABLE(sc->sc_base.sc_rx_port);
	if (error != 0)
		return (EIO);

	error = FMAN_PORT_ENABLE(sc->sc_base.sc_tx_port);
	if (error != 0)
		return (EIO);

	bus_write_4(sc->sc_base.sc_mem, MEMAC_IEVENT, 0);
	memac_setup_multicast(sc);
	memac_setup_promisc(sc);

	if_setdrvflagbits(sc->sc_base.sc_ifnet, IFF_DRV_RUNNING, 0);

	/* Refresh link state */
	memac_miibus_statchg(sc->sc_base.sc_dev);

	return (0);
}

static int
memac_if_disable_locked(struct memac_softc *sc)
{
	int error;

	MEMAC_LOCK_ASSERT(sc);

	error = FMAN_PORT_DISABLE(sc->sc_base.sc_tx_port);
	if (error != 0)
		return (EIO);

	memac_if_graceful_stop(sc);

	error = FMAN_PORT_DISABLE(sc->sc_base.sc_rx_port);
	if (error != 0)
		return (EIO);

	if_setdrvflagbits(sc->sc_base.sc_ifnet, 0, IFF_DRV_RUNNING);

	return (0);
}

static int
memac_if_ioctl(if_t ifp, u_long command, caddr_t data)
{
	struct memac_softc *sc;
	struct ifreq *ifr;
	uint32_t changed;
	int error;

	sc = if_getsoftc(ifp);
	ifr = (struct ifreq *)data;
	error = 0;

	/* Basic functionality to achieve media status reports */
	switch (command) {
	case SIOCSIFMTU:
		MEMAC_LOCK(sc);
		if (memac_set_mtu(sc, ifr->ifr_mtu))
			if_setmtu(ifp, ifr->ifr_mtu);
		else
			error = EINVAL;
		MEMAC_UNLOCK(sc);
		break;
	case SIOCSIFFLAGS:
		MEMAC_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
				memac_if_init_locked(sc);
		} else if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
			error = memac_if_disable_locked(sc);

		MEMAC_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getflags(sc->sc_base.sc_ifnet) & IFF_UP) {
			MEMAC_LOCK(sc);
			memac_setup_multicast(sc);
			MEMAC_UNLOCK(sc);
		}
		break;

	case SIOCSIFCAP:
		changed = if_getcapenable(ifp) ^ ifr->ifr_reqcap;
		if ((changed & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) != 0)
			if_togglecapenable(ifp,
			    IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6);
		if ((changed & (IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6)) != 0) {
			if_togglecapenable(ifp,
			    IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6);
			if_togglehwassist(ifp, DPAA_CSUM_TX_OFFLOAD);
		}
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
memac_if_tick(void *arg)
{
	struct memac_softc *sc;

	sc = arg;

	/* TODO */
	MEMAC_LOCK(sc);

	mii_tick(sc->sc_base.sc_mii);
	callout_reset(&sc->sc_base.sc_tick_callout, hz, memac_if_tick, sc);

	MEMAC_UNLOCK(sc);
}

static void
memac_if_deinit_locked(struct memac_softc *sc)
{

	MEMAC_LOCK_ASSERT(sc);

	MEMAC_UNLOCK(sc);
	callout_drain(&sc->sc_base.sc_tick_callout);
	MEMAC_LOCK(sc);
}

static void
memac_if_set_macaddr(struct memac_softc *sc, const char *addr)
{
	uint32_t reg;

	reg = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	bus_write_4(sc->sc_base.sc_mem, MEMAC_MAC_ADDR_0, reg);
	reg = (addr[5] << 8) | (addr[4]);
	bus_write_4(sc->sc_base.sc_mem, MEMAC_MAC_ADDR_1, reg);
}

static void
memac_if_init_locked(struct memac_softc *sc)
{
	int error;
	const char *macaddr;

	MEMAC_LOCK_ASSERT(sc);

	macaddr = if_getlladdr(sc->sc_base.sc_ifnet);
	memac_if_set_macaddr(sc, macaddr);

	/* Start MII polling */
	if (sc->sc_base.sc_mii)
		callout_reset(&sc->sc_base.sc_tick_callout, hz,
		    memac_if_tick, sc);

	if (if_getflags(sc->sc_base.sc_ifnet) & IFF_UP) {
		error = memac_if_enable_locked(sc);
		if (error != 0)
			goto err;
	} else {
		error = memac_if_disable_locked(sc);
		if (error != 0)
			goto err;
	}

	if_link_state_change(sc->sc_base.sc_ifnet, LINK_STATE_UP);

	bus_write_4(sc->sc_base.sc_mem, MEMAC_CL01_PAUSE_QUANTA,
	    DEFAULT_PAUSE_QUANTA);

	return;

err:
	memac_if_deinit_locked(sc);
	device_printf(sc->sc_base.sc_dev, "initialization error.\n");
	return;
}

static void
memac_if_init(void *data)
{
	struct memac_softc *sc;

	sc = data;

	MEMAC_LOCK(sc);
	memac_if_init_locked(sc);
	MEMAC_UNLOCK(sc);
}

static void
memac_if_start(if_t ifp)
{
	struct memac_softc *sc;

	sc = if_getsoftc(ifp);
	MEMAC_LOCK(sc);
	dpaa_eth_if_start_locked(&sc->sc_base);
	MEMAC_UNLOCK(sc);
}

static void
memac_if_watchdog(if_t ifp)
{
	/* TODO */
}
/** @} */


/**
 * @group IFmedia routines.
 * @{
 */
static int
memac_ifmedia_upd(if_t ifp)
{
	struct memac_softc *sc = if_getsoftc(ifp);

	return (0);
	MEMAC_LOCK(sc);
	mii_mediachg(sc->sc_base.sc_mii);
	MEMAC_UNLOCK(sc);

	return (0);
}

static void
memac_ifmedia_fixed_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct memac_softc *sc = if_getsoftc(ifp);

	MEMAC_LOCK(sc);
	ifmr->ifm_count = 1;
	ifmr->ifm_mask = 0;
	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	ifmr->ifm_current = ifmr->ifm_active =
	    sc->sc_base.sc_mii->mii_media.ifm_cur->ifm_media;
	ifmr->ifm_active = ifmr->ifm_current;

	/*
	 * In non-PHY usecases, we need to signal link state up, otherwise
	 * certain things requiring a link event (e.g async DHCP client) from
	 * devd do not happen.
	 */
	if (if_getlinkstate(ifp) == LINK_STATE_UNKNOWN) {
		if_link_state_change(ifp, LINK_STATE_UP);
	}

	/* We assume the link is static, as in a peer switch. */

	MEMAC_UNLOCK(sc);

	return;
}

static void
memac_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct memac_softc *sc = if_getsoftc(ifp);

	MEMAC_LOCK(sc);

	mii_pollstat(sc->sc_base.sc_mii);

	ifmr->ifm_active = sc->sc_base.sc_mii->mii_media_active;
	ifmr->ifm_status = sc->sc_base.sc_mii->mii_media_status;

	MEMAC_UNLOCK(sc);
}
/** @} */


/**
 * @group dTSEC bus interface.
 * @{
 */

int
memac_attach(device_t dev)
{
	struct memac_softc *sc;
	int error;
	if_t ifp;

	sc = device_get_softc(dev);

	sc->sc_base.sc_dev = dev;

	/* Init locks */
	mtx_init(&sc->sc_base.sc_lock, device_get_nameunit(dev),
	    "mEMAC Global Lock", MTX_DEF);

	mtx_init(&sc->sc_base.sc_mii_lock, device_get_nameunit(dev),
	    "mEMAC MII Lock", MTX_DEF);

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

	/* Init FMan MAC module. */
	error = memac_fm_mac_init(sc, sc->sc_base.sc_mac_addr);
	if (error != 0) {
		memac_detach(dev);
		return (ENXIO);
	}

	dpaa_eth_fm_port_rx_init(&sc->sc_base);
	dpaa_eth_fm_port_tx_init(&sc->sc_base);

	/* Create network interface for upper layers */
	ifp = sc->sc_base.sc_ifnet = if_alloc(IFT_ETHER);
	if_setsoftc(ifp, sc);

	if_setflags(ifp, IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST);
	if_setinitfn(ifp, memac_if_init);
	if_setstartfn(ifp, memac_if_start);
	if_setioctlfn(ifp, memac_if_ioctl);
	if_setsendqlen(ifp, IFQ_MAXLEN);
	if_setsendqready(ifp);

	if (sc->sc_base.sc_phy_addr >= 0)
		if_initname(ifp, device_get_name(sc->sc_base.sc_dev),
		    device_get_unit(sc->sc_base.sc_dev));
	else
		if_initname(ifp, "memac_phy",
		    device_get_unit(sc->sc_base.sc_dev));


	if_setcapabilities(ifp, IFCAP_VLAN_MTU | IFCAP_VLAN_HWCSUM |
	    IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 |
	    IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6);
	if_setcapenable(ifp, if_getcapabilities(ifp));
	if_sethwassist(ifp, DPAA_CSUM_TX_OFFLOAD);

	/* Attach PHY(s) */
	if (!sc->sc_fixed_link) {
		error = mii_attach(sc->sc_base.sc_dev, &sc->sc_base.sc_mii_dev,
		    ifp, memac_ifmedia_upd, memac_ifmedia_sts, BMSR_DEFCAPMASK,
		    sc->sc_base.sc_phy_addr, MII_OFFSET_ANY, 0);
		if (error) {
			device_printf(sc->sc_base.sc_dev,
			    "attaching PHYs failed: %d\n", error);
			memac_detach(sc->sc_base.sc_dev);
			return (error);
		}
		sc->sc_base.sc_mii = device_get_softc(sc->sc_base.sc_mii_dev);
	} else {
		phandle_t node;
		uint32_t type = IFM_ETHER;
		uint32_t speed;

		node = ofw_bus_find_child(ofw_bus_get_node(dev), "fixed-link");
		if (OF_getencprop(node, "speed", &speed, sizeof(speed)) <= 0) {
			device_printf(dev,
			    "fixed link has no speed property\n");
			memac_detach(sc->sc_base.sc_dev);
			return (ENXIO);
		}
		switch (speed) {
		case 10:
			type |= IFM_10_T;
			break;
		case 100:
			type |= IFM_100_TX;
			break;
		case 1000:
			type |= IFM_1000_T;
			break;
		case 2500:
			type |= IFM_2500_T;
			break;
		case 5000:
			type |= IFM_5000_T;
			break;
		case 10000:
			type |= IFM_10G_T;
			break;
		}
		if (OF_hasprop(node, "full-duplex"))
			type |= IFM_FDX;
		sc->sc_base.sc_mii = malloc(sizeof(*sc->sc_base.sc_mii),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		ifmedia_init(&sc->sc_base.sc_mii->mii_media, 0,
		    memac_ifmedia_upd, memac_ifmedia_fixed_sts);
		ifmedia_add(&sc->sc_base.sc_mii->mii_media, type, 0, NULL);
		ifmedia_set(&sc->sc_base.sc_mii->mii_media, type);
	}

	/* Attach to stack */
	ether_ifattach(ifp, sc->sc_base.sc_mac_addr);

	return (0);
}

int
memac_detach(device_t dev)
{
	struct memac_softc *sc;
	if_t ifp;

	sc = device_get_softc(dev);
	ifp = sc->sc_base.sc_ifnet;

	if (device_is_attached(dev)) {
		ether_ifdetach(ifp);
		/* Shutdown interface */
		MEMAC_LOCK(sc);
		memac_if_deinit_locked(sc);
		MEMAC_UNLOCK(sc);
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
memac_suspend(device_t dev)
{

	return (0);
}

int
memac_resume(device_t dev)
{

	return (0);
}

int
memac_shutdown(device_t dev)
{

	return (0);
}
/** @} */


/**
 * @group MII bus interface.
 * @{
 */
int
memac_miibus_readreg(device_t dev, int phy, int reg)
{
	struct memac_softc *sc;

	sc = device_get_softc(dev);

	return (MIIBUS_READREG(sc->sc_base.sc_mdio, phy, reg));
}

int
memac_miibus_writereg(device_t dev, int phy, int reg, int value)
{

	struct memac_softc *sc;

	sc = device_get_softc(dev);

	return (MIIBUS_WRITEREG(sc->sc_base.sc_mdio, phy, reg, value));
}

void
memac_miibus_statchg(device_t dev)
{
	struct memac_softc *sc;
	uint32_t reg;
	bool duplex;
	int speed;

	sc = device_get_softc(dev);

	MEMAC_LOCK_ASSERT(sc);

	duplex = ((sc->sc_base.sc_mii->mii_media_active & IFM_GMASK) == IFM_FDX);

	switch (IFM_SUBTYPE(sc->sc_base.sc_mii->mii_media_active)) {
	case IFM_AUTO:
		speed = IF_MODE_ENA;
		break;
	case IFM_1000_T:
	case IFM_1000_SX:
		if (!duplex) {
			device_printf(sc->sc_base.sc_dev,
			    "Only full-duplex supported for 1Gbps speeds");
			return;
		}
		speed = IF_MODE_SSP_1GB;
		break;

	case IFM_100_TX:
		speed = IF_MODE_SSP_100MB;
		break;
	default:
		speed = IF_MODE_SSP_10MB;
		break;
	}

	reg = bus_read_4(sc->sc_base.sc_mem, MEMAC_IF_MODE);
	reg &= ~(IF_MODE_ENA | IF_MODE_SSP_M | IF_MODE_SFD);
	reg |= 0x2;

	if (duplex)
		reg |= IF_MODE_SFD;
	else
		reg |= IF_MODE_HD;
	reg |= speed;
	bus_write_4(sc->sc_base.sc_mem, MEMAC_IF_MODE, reg);
}
/** @} */
