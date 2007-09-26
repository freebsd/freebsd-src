/*-
 * Copyright (C) 2001 Eduardo Horvath.
 * Copyright (c) 2001-2003 Thomas Moestl
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: gem.c,v 1.21 2002/06/01 23:50:58 lukem Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for Apple GMAC, Sun ERI and Sun GEM Ethernet controllers
 */

#if 0
#define	GEM_DEBUG
#endif

#if 0	/* XXX: In case of emergency, re-enable this. */
#define	GEM_RINT_TIMEOUT
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/rman.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/gem/if_gemreg.h>
#include <dev/gem/if_gemvar.h>

CTASSERT(powerof2(GEM_NRXDESC) && GEM_NRXDESC >= 32 && GEM_NRXDESC <= 8192);
CTASSERT(powerof2(GEM_NTXDESC) && GEM_NTXDESC >= 32 && GEM_NTXDESC <= 8192);

#define TRIES	10000

/*
 * The GEM hardware support basic TCP/UDP checksum offloading. However,
 * the hardware doesn't compensate the checksum for UDP datagram which
 * can yield to 0x0. As a safe guard, UDP checksum offload is disabled
 * by default. It can be reactivated by setting special link option
 * link0 with ifconfig(8).
 */
#define	GEM_CSUM_FEATURES	(CSUM_TCP)

static void	gem_start(struct ifnet *);
static void	gem_start_locked(struct ifnet *);
static void	gem_stop(struct ifnet *, int);
static int	gem_ioctl(struct ifnet *, u_long, caddr_t);
static void	gem_cddma_callback(void *, bus_dma_segment_t *, int, int);
static __inline void gem_txcksum(struct gem_softc *, struct mbuf *, uint64_t *);
static __inline void gem_rxcksum(struct mbuf *, uint64_t);
static void	gem_tick(void *);
static int	gem_watchdog(struct gem_softc *);
static void	gem_init(void *);
static void	gem_init_locked(struct gem_softc *);
static void	gem_init_regs(struct gem_softc *);
static u_int	gem_ringsize(u_int);
static int	gem_meminit(struct gem_softc *);
static struct mbuf *gem_defrag(struct mbuf *, int, int);
static int	gem_load_txmbuf(struct gem_softc *, struct mbuf **);
static void	gem_mifinit(struct gem_softc *);
static int	gem_bitwait(struct gem_softc *, bus_addr_t, u_int32_t,
    u_int32_t);
static void	gem_reset(struct gem_softc *);
static int	gem_reset_rx(struct gem_softc *);
static void	gem_reset_rxdma(struct gem_softc *sc);
static int	gem_reset_tx(struct gem_softc *);
static int	gem_disable_rx(struct gem_softc *);
static int	gem_disable_tx(struct gem_softc *);
static void	gem_rxdrain(struct gem_softc *);
static int	gem_add_rxbuf(struct gem_softc *, int);
static void	gem_setladrf(struct gem_softc *);

struct mbuf	*gem_get(struct gem_softc *, int, int);
static void	gem_eint(struct gem_softc *, u_int);
static void	gem_rint(struct gem_softc *);
#ifdef GEM_RINT_TIMEOUT
static void	gem_rint_timeout(void *);
#endif
static void	gem_tint(struct gem_softc *);

devclass_t gem_devclass;
DRIVER_MODULE(miibus, gem, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(gem, miibus, 1, 1, 1);

#ifdef GEM_DEBUG
#include <sys/ktr.h>
#define	KTR_GEM		KTR_CT2
#endif

#define	GEM_NSEGS GEM_NTXDESC

/*
 * gem_attach:
 *
 *	Attach a Gem interface to the system.
 */
int
gem_attach(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp;
	int i, error;
	u_int32_t v;

	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		return (ENOSPC);

	callout_init_mtx(&sc->sc_tick_ch, &sc->sc_mtx, 0);
#ifdef GEM_RINT_TIMEOUT
	callout_init_mtx(&sc->sc_rx_ch, &sc->sc_mtx, 0);
#endif

	/* Make sure the chip is stopped. */
	ifp->if_softc = sc;
	gem_reset(sc);

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, 0, BUS_SPACE_MAXSIZE_32BIT, 0, NULL, NULL,
	    &sc->sc_pdmatag);
	if (error)
		goto fail_ifnet;

	error = bus_dma_tag_create(sc->sc_pdmatag, 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    1, MCLBYTES, BUS_DMA_ALLOCNOW, NULL, NULL, &sc->sc_rdmatag);
	if (error)
		goto fail_ptag;

	error = bus_dma_tag_create(sc->sc_pdmatag, 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    MCLBYTES * GEM_NTXSEGS, GEM_NTXSEGS, MCLBYTES,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &sc->sc_tdmatag);
	if (error)
		goto fail_rtag;

	error = bus_dma_tag_create(sc->sc_pdmatag, PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct gem_control_data), 1,
	    sizeof(struct gem_control_data), 0,
	    NULL, NULL, &sc->sc_cdmatag);
	if (error)
		goto fail_ttag;

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_cdmatag,
	    (void **)&sc->sc_control_data,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &sc->sc_cddmamap))) {
		device_printf(sc->sc_dev, "unable to allocate control data,"
		    " error = %d\n", error);
		goto fail_ctag;
	}

	sc->sc_cddma = 0;
	if ((error = bus_dmamap_load(sc->sc_cdmatag, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct gem_control_data),
	    gem_cddma_callback, sc, 0)) != 0 || sc->sc_cddma == 0) {
		device_printf(sc->sc_dev, "unable to load control data DMA "
		    "map, error = %d\n", error);
		goto fail_cmem;
	}

	/*
	 * Initialize the transmit job descriptors.
	 */
	STAILQ_INIT(&sc->sc_txfreeq);
	STAILQ_INIT(&sc->sc_txdirtyq);

	/*
	 * Create the transmit buffer DMA maps.
	 */
	error = ENOMEM;
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		struct gem_txsoft *txs;

		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		txs->txs_ndescs = 0;
		if ((error = bus_dmamap_create(sc->sc_tdmatag, 0,
		    &txs->txs_dmamap)) != 0) {
			device_printf(sc->sc_dev, "unable to create tx DMA map "
			    "%d, error = %d\n", i, error);
			goto fail_txd;
		}
		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < GEM_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_rdmatag, 0,
		    &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			device_printf(sc->sc_dev, "unable to create rx DMA map "
			    "%d, error = %d\n", i, error);
			goto fail_rxd;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/* Bad things will happen when touching this register on ERI. */
	if (sc->sc_variant != GEM_SUN_ERI)
		bus_write_4(sc->sc_res[0], GEM_MII_DATAPATH_MODE,
		    GEM_MII_DATAPATH_MII);

	gem_mifinit(sc);

	/*
	 * Look for an external PHY.
	 */
	error = ENXIO;
	v = bus_read_4(sc->sc_res[0], GEM_MIF_CONFIG);
	if ((v & GEM_MIF_CONFIG_MDI1) != 0) {
		v |= GEM_MIF_CONFIG_PHY_SEL;
		bus_write_4(sc->sc_res[0], GEM_MIF_CONFIG, v);
		switch (sc->sc_variant) {
		case GEM_SUN_ERI:
			sc->sc_phyad = GEM_PHYAD_EXTERNAL;
			break;
		default:
			sc->sc_phyad = -1;
			break;
		}
		error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
		    gem_mediachange, gem_mediastatus);
	}

	/*
	 * Fall back on an internal PHY if no external PHY was found.
	 */
	if (error != 0 && (v & GEM_MIF_CONFIG_MDI0) != 0) {
		v &= ~GEM_MIF_CONFIG_PHY_SEL;
		bus_write_4(sc->sc_res[0], GEM_MIF_CONFIG, v);
		switch (sc->sc_variant) {
		case GEM_SUN_ERI:
		case GEM_APPLE_K2_GMAC:
			sc->sc_phyad = GEM_PHYAD_INTERNAL;
			break;
		case GEM_APPLE_GMAC:
			sc->sc_phyad = GEM_PHYAD_EXTERNAL;
			break;
		default:
			sc->sc_phyad = -1;
			break;
		}
		error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
		    gem_mediachange, gem_mediastatus);
	}

	/*
	 * Try the external PCS SERDES if we didn't find any PHYs.
	 */
	if (error != 0 && sc->sc_variant == GEM_SUN_GEM) {
		bus_write_4(sc->sc_res[0], GEM_MII_DATAPATH_MODE,
		    GEM_MII_DATAPATH_SERDES);
		bus_write_4(sc->sc_res[0], GEM_MII_SLINK_CONTROL,
		    GEM_MII_SLINK_LOOPBACK | GEM_MII_SLINK_EN_SYNC_D);
		bus_write_4(sc->sc_res[0], GEM_MII_CONFIG,
		    GEM_MII_CONFIG_ENABLE);
		sc->sc_flags |= GEM_SERDES;
		sc->sc_phyad = GEM_PHYAD_EXTERNAL;
		error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
		    gem_mediachange, gem_mediastatus);
	}

	if (error != 0) {
		device_printf(sc->sc_dev, "PHY probe failed: %d\n", error);
		goto fail_rxd;
	}
	sc->sc_mii = device_get_softc(sc->sc_miibus);

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */

	/* Get RX FIFO size */
	sc->sc_rxfifosize = 64 *
	    bus_read_4(sc->sc_res[0], GEM_RX_FIFO_SIZE);

	/* Get TX FIFO size */
	v = bus_read_4(sc->sc_res[0], GEM_TX_FIFO_SIZE);
	device_printf(sc->sc_dev, "%ukB RX FIFO, %ukB TX FIFO\n",
	    sc->sc_rxfifosize / 1024, v / 16);

	sc->sc_csum_features = GEM_CSUM_FEATURES;
	/* Initialize ifnet structure. */
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = gem_start;
	ifp->if_ioctl = gem_ioctl;
	ifp->if_init = gem_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, GEM_TXQUEUELEN);
	ifp->if_snd.ifq_drv_maxlen = GEM_TXQUEUELEN;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	ether_ifattach(ifp, sc->sc_enaddr);

	/*
	 * Tell the upper layer(s) we support long frames/checksum offloads.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU | IFCAP_HWCSUM;
	ifp->if_hwassist |= sc->sc_csum_features;
	ifp->if_capenable |= IFCAP_VLAN_MTU | IFCAP_HWCSUM;

	return (0);

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
fail_rxd:
	for (i = 0; i < GEM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_rdmatag,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
fail_txd:
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_tdmatag,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_cdmatag, sc->sc_cddmamap);
fail_cmem:
	bus_dmamem_free(sc->sc_cdmatag, sc->sc_control_data,
	    sc->sc_cddmamap);
fail_ctag:
	bus_dma_tag_destroy(sc->sc_cdmatag);
fail_ttag:
	bus_dma_tag_destroy(sc->sc_tdmatag);
fail_rtag:
	bus_dma_tag_destroy(sc->sc_rdmatag);
fail_ptag:
	bus_dma_tag_destroy(sc->sc_pdmatag);
fail_ifnet:
	if_free(ifp);
	return (error);
}

void
gem_detach(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	int i;

	GEM_LOCK(sc);
	gem_stop(ifp, 1);
	GEM_UNLOCK(sc);
	callout_drain(&sc->sc_tick_ch);
#ifdef GEM_RINT_TIMEOUT
	callout_drain(&sc->sc_rx_ch);
#endif
	ether_ifdetach(ifp);
	if_free(ifp);
	device_delete_child(sc->sc_dev, sc->sc_miibus);

	for (i = 0; i < GEM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_rdmatag,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_tdmatag,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTREAD);
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_cdmatag, sc->sc_cddmamap);
	bus_dmamem_free(sc->sc_cdmatag, sc->sc_control_data,
	    sc->sc_cddmamap);
	bus_dma_tag_destroy(sc->sc_cdmatag);
	bus_dma_tag_destroy(sc->sc_tdmatag);
	bus_dma_tag_destroy(sc->sc_rdmatag);
	bus_dma_tag_destroy(sc->sc_pdmatag);
}

void
gem_suspend(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;

	GEM_LOCK(sc);
	gem_stop(ifp, 0);
	GEM_UNLOCK(sc);
}

void
gem_resume(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;

	GEM_LOCK(sc);
	/*
	 * On resume all registers have to be initialized again like
	 * after power-on.
	 */
	sc->sc_flags &= ~GEM_INITED;
	if (ifp->if_flags & IFF_UP)
		gem_init_locked(sc);
	GEM_UNLOCK(sc);
}

static __inline void
gem_txcksum(struct gem_softc *sc, struct mbuf *m, uint64_t *cflags)
{
	struct ip *ip;
	uint64_t offset, offset2;
	char *p;

	offset = sizeof(struct ip) + ETHER_HDR_LEN;
	for(; m && m->m_len == 0; m = m->m_next)
		;
	if (m == NULL || m->m_len < ETHER_HDR_LEN) {
		device_printf(sc->sc_dev, "%s: m_len < ETHER_HDR_LEN\n",
		    __func__);
		/* checksum will be corrupted */
		goto sendit;
	}
	if (m->m_len < ETHER_HDR_LEN + sizeof(uint32_t)) {
		if (m->m_len != ETHER_HDR_LEN) {
			device_printf(sc->sc_dev,
			    "%s: m_len != ETHER_HDR_LEN\n", __func__);
			/* checksum will be corrupted */
			goto sendit;
		}
		for(m = m->m_next; m && m->m_len == 0; m = m->m_next)
			;
		if (m == NULL) {
			/* checksum will be corrupted */
			goto sendit;
		}
		ip = mtod(m, struct ip *);
	} else {
		p = mtod(m, uint8_t *);
		p += ETHER_HDR_LEN;
		ip = (struct ip *)p;
	}
	offset = (ip->ip_hl << 2) + ETHER_HDR_LEN;

sendit:
	offset2 = m->m_pkthdr.csum_data;
	*cflags = offset << GEM_TD_CXSUM_STARTSHFT;
	*cflags |= ((offset + offset2) << GEM_TD_CXSUM_STUFFSHFT);
	*cflags |= GEM_TD_CXSUM_ENABLE;
}

static __inline void
gem_rxcksum(struct mbuf *m, uint64_t flags)
{
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *uh;
	int32_t hlen, len, pktlen;
	uint16_t cksum, *opts;
	uint32_t temp32;

	pktlen = m->m_pkthdr.len;
	if (pktlen < sizeof(struct ether_header) + sizeof(struct ip))
		return;
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type != htons(ETHERTYPE_IP))
		return;
	ip = (struct ip *)(eh + 1);
	if (ip->ip_v != IPVERSION)
		return;

	hlen = ip->ip_hl << 2;
	pktlen -= sizeof(struct ether_header);
	if (hlen < sizeof(struct ip))
		return;
	if (ntohs(ip->ip_len) < hlen)
		return;
	if (ntohs(ip->ip_len) != pktlen)
		return;
	if (ip->ip_off & htons(IP_MF | IP_OFFMASK))
		return;	/* can't handle fragmented packet */

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		if (pktlen < (hlen + sizeof(struct tcphdr)))
			return;
		break;
	case IPPROTO_UDP:
		if (pktlen < (hlen + sizeof(struct udphdr)))
			return;
		uh = (struct udphdr *)((uint8_t *)ip + hlen);
		if (uh->uh_sum == 0)
			return; /* no checksum */
		break;
	default:
		return;
	}

	cksum = ~(flags & GEM_RD_CHECKSUM);
	/* checksum fixup for IP options */
	len = hlen - sizeof(struct ip);
	if (len > 0) {
		opts = (uint16_t *)(ip + 1);
		for (; len > 0; len -= sizeof(uint16_t), opts++) {
			temp32 = cksum - *opts;
			temp32 = (temp32 >> 16) + (temp32 & 65535);
			cksum = temp32 & 65535;
		}
	}
	m->m_pkthdr.csum_flags |= CSUM_DATA_VALID;
	m->m_pkthdr.csum_data = cksum;
}

static void
gem_cddma_callback(xsc, segs, nsegs, error)
	void *xsc;
	bus_dma_segment_t *segs;
	int nsegs;
	int error;
{
	struct gem_softc *sc = (struct gem_softc *)xsc;

	if (error != 0)
		return;
	if (nsegs != 1) {
		/* can't happen... */
		panic("%s: bad control buffer segment count", __func__);
	}
	sc->sc_cddma = segs[0].ds_addr;
}

static void
gem_tick(arg)
	void *arg;
{
	struct gem_softc *sc = arg;
	struct ifnet *ifp;

	GEM_LOCK_ASSERT(sc, MA_OWNED);

	ifp = sc->sc_ifp;
	/*
	 * Unload collision counters
	 */
	ifp->if_collisions +=
	    bus_read_4(sc->sc_res[0], GEM_MAC_NORM_COLL_CNT) +
	    bus_read_4(sc->sc_res[0], GEM_MAC_FIRST_COLL_CNT) +
	    bus_read_4(sc->sc_res[0], GEM_MAC_EXCESS_COLL_CNT) +
	    bus_read_4(sc->sc_res[0], GEM_MAC_LATE_COLL_CNT);

	/*
	 * then clear the hardware counters.
	 */
	bus_write_4(sc->sc_res[0], GEM_MAC_NORM_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_FIRST_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_EXCESS_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_LATE_COLL_CNT, 0);

	mii_tick(sc->sc_mii);

	if (gem_watchdog(sc) == EJUSTRETURN)
		return;

	callout_reset(&sc->sc_tick_ch, hz, gem_tick, sc);
}

static int
gem_bitwait(sc, r, clr, set)
	struct gem_softc *sc;
	bus_addr_t r;
	u_int32_t clr;
	u_int32_t set;
{
	int i;
	u_int32_t reg;

	for (i = TRIES; i--; DELAY(100)) {
		reg = bus_read_4(sc->sc_res[0], r);
		if ((reg & clr) == 0 && (reg & set) == set)
			return (1);
	}
	return (0);
}

static void
gem_reset(sc)
	struct gem_softc *sc;
{

#ifdef GEM_DEBUG
	CTR2(KTR_GEM, "%s: %s", device_get_name(sc->sc_dev), __func__);
#endif
	gem_reset_rx(sc);
	gem_reset_tx(sc);

	/* Do a full reset */
	bus_write_4(sc->sc_res[0], GEM_RESET, GEM_RESET_RX | GEM_RESET_TX);
	bus_barrier(sc->sc_res[0], GEM_RESET, 4, BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_RESET, GEM_RESET_RX | GEM_RESET_TX, 0))
		device_printf(sc->sc_dev, "cannot reset device\n");
}

/*
 * gem_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
gem_rxdrain(sc)
	struct gem_softc *sc;
{
	struct gem_rxsoft *rxs;
	int i;

	for (i = 0; i < GEM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_rdmatag, rxs->rxs_dmamap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_rdmatag, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * Reset the whole thing.
 */
static void
gem_stop(ifp, disable)
	struct ifnet *ifp;
	int disable;
{
	struct gem_softc *sc = (struct gem_softc *)ifp->if_softc;
	struct gem_txsoft *txs;

#ifdef GEM_DEBUG
	CTR2(KTR_GEM, "%s: %s", device_get_name(sc->sc_dev), __func__);
#endif

	callout_stop(&sc->sc_tick_ch);
#ifdef GEM_RINT_TIMEOUT
	callout_stop(&sc->sc_rx_ch);
#endif

	/* XXX - Should we reset these instead? */
	gem_disable_tx(sc);
	gem_disable_rx(sc);

	/*
	 * Release any queued transmit buffers.
	 */
	while ((txs = STAILQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		if (txs->txs_ndescs != 0) {
			bus_dmamap_sync(sc->sc_tdmatag, txs->txs_dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_tdmatag, txs->txs_dmamap);
			if (txs->txs_mbuf != NULL) {
				m_freem(txs->txs_mbuf);
				txs->txs_mbuf = NULL;
			}
		}
		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	if (disable)
		gem_rxdrain(sc);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->sc_flags &= ~GEM_LINK;
	sc->sc_wdog_timer = 0;
}

/*
 * Reset the receiver
 */
static int
gem_reset_rx(sc)
	struct gem_softc *sc;
{

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	gem_disable_rx(sc);
	bus_write_4(sc->sc_res[0], GEM_RX_CONFIG, 0);
	bus_barrier(sc->sc_res[0], GEM_RX_CONFIG, 4, BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_RX_CONFIG, GEM_RX_CONFIG_RXDMA_EN, 0))
		device_printf(sc->sc_dev, "cannot disable RX DMA\n");

	/* Finally, reset the ERX */
	bus_write_4(sc->sc_res[0], GEM_RESET, GEM_RESET_RX);
	bus_barrier(sc->sc_res[0], GEM_RESET, 4, BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_RESET, GEM_RESET_RX | GEM_RESET_TX, 0)) {
		device_printf(sc->sc_dev, "cannot reset receiver\n");
		return (1);
	}
	return (0);
}

/*
 * Reset the receiver DMA engine.
 *
 * Intended to be used in case of GEM_INTR_RX_TAG_ERR, GEM_MAC_RX_OVERFLOW
 * etc in order to reset the receiver DMA engine only and not do a full
 * reset which amongst others also downs the link and clears the FIFOs.
 */
static void
gem_reset_rxdma(struct gem_softc *sc)
{
	int i;

	if (gem_reset_rx(sc) != 0)
		return (gem_init_locked(sc));
	for (i = 0; i < GEM_NRXDESC; i++)
		if (sc->sc_rxsoft[i].rxs_mbuf != NULL)
			GEM_UPDATE_RXDESC(sc, i);
	sc->sc_rxptr = 0;
	GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
	GEM_CDSYNC(sc, BUS_DMASYNC_PREREAD);

	/* NOTE: we use only 32-bit DMA addresses here. */
	bus_write_4(sc->sc_res[0], GEM_RX_RING_PTR_HI, 0);
	bus_write_4(sc->sc_res[0], GEM_RX_RING_PTR_LO, GEM_CDRXADDR(sc, 0));
	bus_write_4(sc->sc_res[0], GEM_RX_KICK, GEM_NRXDESC - 4);
	bus_write_4(sc->sc_res[0], GEM_RX_CONFIG,
	    gem_ringsize(GEM_NRXDESC /*XXX*/) |
	    ((ETHER_HDR_LEN + sizeof(struct ip)) <<
	    GEM_RX_CONFIG_CXM_START_SHFT) |
	    (GEM_THRSH_1024 << GEM_RX_CONFIG_FIFO_THRS_SHIFT) |
	    (2 << GEM_RX_CONFIG_FBOFF_SHFT));
	bus_write_4(sc->sc_res[0], GEM_RX_BLANKING,
	    (6 << GEM_RX_BLANKING_TIME_SHIFT) | 6);
	bus_write_4(sc->sc_res[0], GEM_RX_PAUSE_THRESH,
	    (3 * sc->sc_rxfifosize / 256) | ((sc->sc_rxfifosize / 256) << 12));
	bus_write_4(sc->sc_res[0], GEM_RX_CONFIG,
	    bus_read_4(sc->sc_res[0], GEM_RX_CONFIG) | GEM_RX_CONFIG_RXDMA_EN);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_MASK,
	    GEM_MAC_RX_DONE | GEM_MAC_RX_FRAME_CNT);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG,
	    bus_read_4(sc->sc_res[0], GEM_MAC_RX_CONFIG) | GEM_MAC_RX_ENABLE);
}

/*
 * Reset the transmitter
 */
static int
gem_reset_tx(sc)
	struct gem_softc *sc;
{

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	gem_disable_tx(sc);
	bus_write_4(sc->sc_res[0], GEM_TX_CONFIG, 0);
	bus_barrier(sc->sc_res[0], GEM_TX_CONFIG, 4, BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_TX_CONFIG, GEM_TX_CONFIG_TXDMA_EN, 0))
		device_printf(sc->sc_dev, "cannot disable TX DMA\n");

	/* Finally, reset the ETX */
	bus_write_4(sc->sc_res[0], GEM_RESET, GEM_RESET_TX);
	bus_barrier(sc->sc_res[0], GEM_RESET, 4, BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_RESET, GEM_RESET_RX | GEM_RESET_TX, 0)) {
		device_printf(sc->sc_dev, "cannot reset transmitter\n");
		return (1);
	}
	return (0);
}

/*
 * disable receiver.
 */
static int
gem_disable_rx(sc)
	struct gem_softc *sc;
{
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_read_4(sc->sc_res[0], GEM_MAC_RX_CONFIG);
	cfg &= ~GEM_MAC_RX_ENABLE;
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, cfg);
	bus_barrier(sc->sc_res[0], GEM_MAC_RX_CONFIG, 4,
	   BUS_SPACE_BARRIER_WRITE);
	return (gem_bitwait(sc, GEM_MAC_RX_CONFIG, GEM_MAC_RX_ENABLE, 0));
}

/*
 * disable transmitter.
 */
static int
gem_disable_tx(sc)
	struct gem_softc *sc;
{
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_read_4(sc->sc_res[0], GEM_MAC_TX_CONFIG);
	cfg &= ~GEM_MAC_TX_ENABLE;
	bus_write_4(sc->sc_res[0], GEM_MAC_TX_CONFIG, cfg);
	bus_barrier(sc->sc_res[0], GEM_MAC_TX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	return (gem_bitwait(sc, GEM_MAC_TX_CONFIG, GEM_MAC_TX_ENABLE, 0));
}

/*
 * Initialize interface.
 */
static int
gem_meminit(sc)
	struct gem_softc *sc;
{
	struct gem_rxsoft *rxs;
	int i, error;

	/*
	 * Initialize the transmit descriptor ring.
	 */
	for (i = 0; i < GEM_NTXDESC; i++) {
		sc->sc_txdescs[i].gd_flags = 0;
		sc->sc_txdescs[i].gd_addr = 0;
	}
	sc->sc_txfree = GEM_MAXTXFREE;
	sc->sc_txnext = 0;
	sc->sc_txwin = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < GEM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = gem_add_rxbuf(sc, i)) != 0) {
				device_printf(sc->sc_dev, "unable to "
				    "allocate or map rx buffer %d, error = "
				    "%d\n", i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				gem_rxdrain(sc);
				return (1);
			}
		} else
			GEM_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;
	GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
	GEM_CDSYNC(sc, BUS_DMASYNC_PREREAD);

	return (0);
}

static u_int
gem_ringsize(sz)
	u_int sz;
{

	switch (sz) {
	case 32:
		return (GEM_RING_SZ_32);
	case 64:
		return (GEM_RING_SZ_64);
	case 128:
		return (GEM_RING_SZ_128);
	case 256:
		return (GEM_RING_SZ_256);
	case 512:
		return (GEM_RING_SZ_512);
	case 1024:
		return (GEM_RING_SZ_1024);
	case 2048:
		return (GEM_RING_SZ_2048);
	case 4096:
		return (GEM_RING_SZ_4096);
	case 8192:
		return (GEM_RING_SZ_8192);
	default:
		printf("%s: invalid ring size %d\n", __func__, sz);
		return (GEM_RING_SZ_32);
	}
}

static void
gem_init(xsc)
	void *xsc;
{
	struct gem_softc *sc = (struct gem_softc *)xsc;

	GEM_LOCK(sc);
	gem_init_locked(sc);
	GEM_UNLOCK(sc);
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
static void
gem_init_locked(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	u_int32_t v;

	GEM_LOCK_ASSERT(sc, MA_OWNED);

#ifdef GEM_DEBUG
	CTR2(KTR_GEM, "%s: %s: calling stop", device_get_name(sc->sc_dev),
	    __func__);
#endif
	/*
	 * Initialization sequence. The numbered steps below correspond
	 * to the sequence outlined in section 6.3.5.1 in the Ethernet
	 * Channel Engine manual (part of the PCIO manual).
	 * See also the STP2002-STQ document from Sun Microsystems.
	 */

	/* step 1 & 2. Reset the Ethernet Channel */
	gem_stop(sc->sc_ifp, 0);
	gem_reset(sc);
#ifdef GEM_DEBUG
	CTR2(KTR_GEM, "%s: %s: restarting", device_get_name(sc->sc_dev),
	    __func__);
#endif

	/* Re-initialize the MIF */
	gem_mifinit(sc);

	/* step 3. Setup data structures in host memory */
	if (gem_meminit(sc) != 0)
		return;

	/* step 4. TX MAC registers & counters */
	gem_init_regs(sc);

	/* step 5. RX MAC registers & counters */
	gem_setladrf(sc);

	/* step 6 & 7. Program Descriptor Ring Base Addresses */
	/* NOTE: we use only 32-bit DMA addresses here. */
	bus_write_4(sc->sc_res[0], GEM_TX_RING_PTR_HI, 0);
	bus_write_4(sc->sc_res[0], GEM_TX_RING_PTR_LO, GEM_CDTXADDR(sc, 0));

	bus_write_4(sc->sc_res[0], GEM_RX_RING_PTR_HI, 0);
	bus_write_4(sc->sc_res[0], GEM_RX_RING_PTR_LO, GEM_CDRXADDR(sc, 0));
#ifdef GEM_DEBUG
	CTR3(KTR_GEM, "loading rx ring %lx, tx ring %lx, cddma %lx",
	    GEM_CDRXADDR(sc, 0), GEM_CDTXADDR(sc, 0), sc->sc_cddma);
#endif

	/* step 8. Global Configuration & Interrupt Mask */
	bus_write_4(sc->sc_res[0], GEM_INTMASK,
	    ~(GEM_INTR_TX_INTME | GEM_INTR_TX_EMPTY | GEM_INTR_RX_DONE |
	    GEM_INTR_RX_NOBUF | GEM_INTR_RX_TAG_ERR | GEM_INTR_PERR |
	    GEM_INTR_BERR
#ifdef GEM_DEBUG
	    | GEM_INTR_PCS | GEM_INTR_MIF
#endif
	    ));
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_MASK,
	    GEM_MAC_RX_DONE | GEM_MAC_RX_FRAME_CNT);
	bus_write_4(sc->sc_res[0], GEM_MAC_TX_MASK,
	    GEM_MAC_TX_XMIT_DONE | GEM_MAC_TX_DEFER_EXP);
#ifdef GEM_DEBUG
	bus_write_4(sc->sc_res[0], GEM_MAC_CONTROL_MASK,
	    ~(GEM_MAC_PAUSED | GEM_MAC_PAUSE | GEM_MAC_RESUME));
#else
	bus_write_4(sc->sc_res[0], GEM_MAC_CONTROL_MASK,
	    GEM_MAC_PAUSED | GEM_MAC_PAUSE | GEM_MAC_RESUME);
#endif

	/* step 9. ETX Configuration: use mostly default values */

	/* Enable DMA */
	v = gem_ringsize(GEM_NTXDESC /*XXX*/);
	bus_write_4(sc->sc_res[0], GEM_TX_CONFIG,
		v|GEM_TX_CONFIG_TXDMA_EN|
		((0x400<<10)&GEM_TX_CONFIG_TXFIFO_TH));

	/* step 10. ERX Configuration */

	/* Encode Receive Descriptor ring size. */
	v = gem_ringsize(GEM_NRXDESC /*XXX*/);
	/* Rx TCP/UDP checksum offset */
	v |= ((ETHER_HDR_LEN + sizeof(struct ip)) <<
	    GEM_RX_CONFIG_CXM_START_SHFT);

	/* Enable DMA */
	bus_write_4(sc->sc_res[0], GEM_RX_CONFIG,
		v|(GEM_THRSH_1024<<GEM_RX_CONFIG_FIFO_THRS_SHIFT)|
		(2<<GEM_RX_CONFIG_FBOFF_SHFT)|GEM_RX_CONFIG_RXDMA_EN);

	bus_write_4(sc->sc_res[0], GEM_RX_BLANKING,
	    (6 << GEM_RX_BLANKING_TIME_SHIFT) | 6);

	/*
	 * The following value is for an OFF Threshold of about 3/4 full
	 * and an ON Threshold of 1/4 full.
	 */
	bus_write_4(sc->sc_res[0], GEM_RX_PAUSE_THRESH,
	    (3 * sc->sc_rxfifosize / 256) |
	    (   (sc->sc_rxfifosize / 256) << 12));

	/* step 11. Configure Media */

	/* step 12. RX_MAC Configuration Register */
	v = bus_read_4(sc->sc_res[0], GEM_MAC_RX_CONFIG);
	v |= GEM_MAC_RX_STRIP_CRC;
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, 0);
	bus_barrier(sc->sc_res[0], GEM_MAC_RX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_MAC_RX_CONFIG, GEM_MAC_RX_ENABLE, 0))
		device_printf(sc->sc_dev, "cannot disable RX MAC\n");
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, v);

	/* step 14. Issue Transmit Pending command */

	/* step 15.  Give the reciever a swift kick */
	bus_write_4(sc->sc_res[0], GEM_RX_KICK, GEM_NRXDESC-4);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->sc_ifflags = ifp->if_flags;

	sc->sc_flags &= ~GEM_LINK;
	mii_mediachg(sc->sc_mii);

	/* Start the one second timer. */
	sc->sc_wdog_timer = 0;
	callout_reset(&sc->sc_tick_ch, hz, gem_tick, sc);
}

/*
 * It's copy of ath_defrag(ath(4)).
 *
 * Defragment an mbuf chain, returning at most maxfrags separate
 * mbufs+clusters.  If this is not possible NULL is returned and
 * the original mbuf chain is left in it's present (potentially
 * modified) state.  We use two techniques: collapsing consecutive
 * mbufs and replacing consecutive mbufs by a cluster.
 */
static struct mbuf *
gem_defrag(m0, how, maxfrags)
	struct mbuf *m0;
	int how;
	int maxfrags;
{
	struct mbuf *m, *n, *n2, **prev;
	u_int curfrags;

	/*
	 * Calculate the current number of frags.
	 */
	curfrags = 0;
	for (m = m0; m != NULL; m = m->m_next)
		curfrags++;
	/*
	 * First, try to collapse mbufs.  Note that we always collapse
	 * towards the front so we don't need to deal with moving the
	 * pkthdr.  This may be suboptimal if the first mbuf has much
	 * less data than the following.
	 */
	m = m0;
again:
	for (;;) {
		n = m->m_next;
		if (n == NULL)
			break;
		if ((m->m_flags & M_RDONLY) == 0 &&
		    n->m_len < M_TRAILINGSPACE(m)) {
			bcopy(mtod(n, void *), mtod(m, char *) + m->m_len,
				n->m_len);
			m->m_len += n->m_len;
			m->m_next = n->m_next;
			m_free(n);
			if (--curfrags <= maxfrags)
				return (m0);
		} else
			m = n;
	}
	KASSERT(maxfrags > 1,
		("maxfrags %u, but normal collapse failed", maxfrags));
	/*
	 * Collapse consecutive mbufs to a cluster.
	 */
	prev = &m0->m_next;		/* NB: not the first mbuf */
	while ((n = *prev) != NULL) {
		if ((n2 = n->m_next) != NULL &&
		    n->m_len + n2->m_len < MCLBYTES) {
			m = m_getcl(how, MT_DATA, 0);
			if (m == NULL)
				goto bad;
			bcopy(mtod(n, void *), mtod(m, void *), n->m_len);
			bcopy(mtod(n2, void *), mtod(m, char *) + n->m_len,
				n2->m_len);
			m->m_len = n->m_len + n2->m_len;
			m->m_next = n2->m_next;
			*prev = m;
			m_free(n);
			m_free(n2);
			if (--curfrags <= maxfrags)	/* +1 cl -2 mbufs */
				return m0;
			/*
			 * Still not there, try the normal collapse
			 * again before we allocate another cluster.
			 */
			goto again;
		}
		prev = &n->m_next;
	}
	/*
	 * No place where we can collapse to a cluster; punt.
	 * This can occur if, for example, you request 2 frags
	 * but the packet requires that both be clusters (we
	 * never reallocate the first mbuf to avoid moving the
	 * packet header).
	 */
bad:
	return (NULL);
}

static int
gem_load_txmbuf(sc, m_head)
	struct gem_softc *sc;
	struct mbuf **m_head;
{
	struct gem_txsoft *txs;
	bus_dma_segment_t txsegs[GEM_NTXSEGS];
	struct mbuf *m;
	uint64_t flags, cflags;
	int error, nexttx, nsegs, seg;

	/* Get a work queue entry. */
	if ((txs = STAILQ_FIRST(&sc->sc_txfreeq)) == NULL) {
		/* Ran out of descriptors. */
		return (ENOBUFS);
	}
	error = bus_dmamap_load_mbuf_sg(sc->sc_tdmatag, txs->txs_dmamap,
	    *m_head, txsegs, &nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		m = gem_defrag(*m_head, M_DONTWAIT, GEM_NTXSEGS);
		if (m == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;
		error = bus_dmamap_load_mbuf_sg(sc->sc_tdmatag, txs->txs_dmamap,
		    *m_head, txsegs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			m_freem(*m_head);
			*m_head = NULL;
			return (error);
		}
	} else if (error != 0)
		return (error);
	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	/*
	 * Ensure we have enough descriptors free to describe
	 * the packet.  Note, we always reserve one descriptor
	 * at the end of the ring as a termination point, to
	 * prevent wrap-around.
	 */
	if (nsegs > sc->sc_txfree - 1) {
		txs->txs_ndescs = 0;
		bus_dmamap_unload(sc->sc_tdmatag, txs->txs_dmamap);
		return (ENOBUFS);
	}

	flags = cflags = 0;
	if (((*m_head)->m_pkthdr.csum_flags & sc->sc_csum_features) != 0)
		gem_txcksum(sc, *m_head, &cflags);

	txs->txs_ndescs = nsegs;
	txs->txs_firstdesc = sc->sc_txnext;
	nexttx = txs->txs_firstdesc;
	for (seg = 0; seg < nsegs; seg++, nexttx = GEM_NEXTTX(nexttx)) {
#ifdef GEM_DEBUG
		CTR6(KTR_GEM, "%s: mapping seg %d (txd %d), len "
		    "%lx, addr %#lx (%#lx)", __func__, seg, nexttx,
		    txsegs[seg].ds_len, txsegs[seg].ds_addr,
		    GEM_DMA_WRITE(sc, txsegs[seg].ds_addr));
#endif
		sc->sc_txdescs[nexttx].gd_addr =
		    GEM_DMA_WRITE(sc, txsegs[seg].ds_addr);
		KASSERT(txsegs[seg].ds_len < GEM_TD_BUFSIZE,
		    ("%s: segment size too large!", __func__));
		flags = txsegs[seg].ds_len & GEM_TD_BUFSIZE;
		sc->sc_txdescs[nexttx].gd_flags =
		    GEM_DMA_WRITE(sc, flags | cflags);
		txs->txs_lastdesc = nexttx;
	}

	/* set EOP on the last descriptor */
#ifdef GEM_DEBUG
	CTR3(KTR_GEM, "%s: end of packet at seg %d, tx %d", __func__, seg,
	    nexttx);
#endif
	sc->sc_txdescs[txs->txs_lastdesc].gd_flags |=
	    GEM_DMA_WRITE(sc, GEM_TD_END_OF_PACKET);

	/* Lastly set SOP on the first descriptor */
#ifdef GEM_DEBUG
	CTR3(KTR_GEM, "%s: start of packet at seg %d, tx %d", __func__, seg,
	    nexttx);
#endif
	if (++sc->sc_txwin > GEM_NTXSEGS * 2 / 3) {
		sc->sc_txwin = 0;
		flags |= GEM_TD_INTERRUPT_ME;
		sc->sc_txdescs[txs->txs_firstdesc].gd_flags |=
		    GEM_DMA_WRITE(sc, GEM_TD_INTERRUPT_ME |
		    GEM_TD_START_OF_PACKET);
	} else
		sc->sc_txdescs[txs->txs_firstdesc].gd_flags |=
		    GEM_DMA_WRITE(sc, GEM_TD_START_OF_PACKET);

	/* Sync the DMA map. */
	bus_dmamap_sync(sc->sc_tdmatag, txs->txs_dmamap, BUS_DMASYNC_PREWRITE);

#ifdef GEM_DEBUG
	CTR4(KTR_GEM, "%s: setting firstdesc=%d, lastdesc=%d, ndescs=%d",
	    __func__, txs->txs_firstdesc, txs->txs_lastdesc, txs->txs_ndescs);
#endif
	STAILQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
	STAILQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);
	txs->txs_mbuf = *m_head;

	sc->sc_txnext = GEM_NEXTTX(txs->txs_lastdesc);
	sc->sc_txfree -= txs->txs_ndescs;

	return (0);
}

static void
gem_init_regs(sc)
	struct gem_softc *sc;
{
	const u_char *laddr = IF_LLADDR(sc->sc_ifp);

	/* These regs are not cleared on reset */
	if ((sc->sc_flags & GEM_INITED) == 0) {
		/* Wooo.  Magic values. */
		bus_write_4(sc->sc_res[0], GEM_MAC_IPG0, 0);
		bus_write_4(sc->sc_res[0], GEM_MAC_IPG1, 8);
		bus_write_4(sc->sc_res[0], GEM_MAC_IPG2, 4);

		bus_write_4(sc->sc_res[0], GEM_MAC_MAC_MIN_FRAME, ETHER_MIN_LEN);
		/* Max frame and max burst size */
		bus_write_4(sc->sc_res[0], GEM_MAC_MAC_MAX_FRAME,
		    (ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN) | (0x2000 << 16));

		bus_write_4(sc->sc_res[0], GEM_MAC_PREAMBLE_LEN, 0x7);
		bus_write_4(sc->sc_res[0], GEM_MAC_JAM_SIZE, 0x4);
		bus_write_4(sc->sc_res[0], GEM_MAC_ATTEMPT_LIMIT, 0x10);
		/* Dunno.... */
		bus_write_4(sc->sc_res[0], GEM_MAC_CONTROL_TYPE, 0x8088);
		bus_write_4(sc->sc_res[0], GEM_MAC_RANDOM_SEED,
		    ((laddr[5]<<8)|laddr[4])&0x3ff);

		/* Secondary MAC addr set to 0:0:0:0:0:0 */
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR3, 0);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR4, 0);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR5, 0);

		/* MAC control addr set to 01:80:c2:00:00:01 */
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR6, 0x0001);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR7, 0xc200);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR8, 0x0180);

		/* MAC filter addr set to 0:0:0:0:0:0 */
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR_FILTER0, 0);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR_FILTER1, 0);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADDR_FILTER2, 0);

		bus_write_4(sc->sc_res[0], GEM_MAC_ADR_FLT_MASK1_2, 0);
		bus_write_4(sc->sc_res[0], GEM_MAC_ADR_FLT_MASK0, 0);

		sc->sc_flags |= GEM_INITED;
	}

	/* Counters need to be zeroed */
	bus_write_4(sc->sc_res[0], GEM_MAC_NORM_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_FIRST_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_EXCESS_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_LATE_COLL_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_DEFER_TMR_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_PEAK_ATTEMPTS, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_FRAME_COUNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_LEN_ERR_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_ALIGN_ERR, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CRC_ERR_CNT, 0);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CODE_VIOL, 0);

	/* Set XOFF PAUSE time. */
	bus_write_4(sc->sc_res[0], GEM_MAC_SEND_PAUSE_CMD, 0x1BF0);

	/*
	 * Set the internal arbitration to "infinite" bursts of the
	 * maximum length of 31 * 64 bytes so DMA transfers aren't
	 * split up in cache line size chunks. This greatly improves
	 * especially RX performance.
	 * Enable silicon bug workarounds for the Apple variants.
	 */
	bus_write_4(sc->sc_res[0], GEM_CONFIG,
	    GEM_CONFIG_TXDMA_LIMIT | GEM_CONFIG_RXDMA_LIMIT |
	    GEM_CONFIG_BURST_INF | (GEM_IS_APPLE(sc) ?
	    GEM_CONFIG_RONPAULBIT | GEM_CONFIG_BUG2FIX : 0));

	/*
	 * Set the station address.
	 */
	bus_write_4(sc->sc_res[0], GEM_MAC_ADDR0, (laddr[4]<<8)|laddr[5]);
	bus_write_4(sc->sc_res[0], GEM_MAC_ADDR1, (laddr[2]<<8)|laddr[3]);
	bus_write_4(sc->sc_res[0], GEM_MAC_ADDR2, (laddr[0]<<8)|laddr[1]);

	/* Enable MII outputs. */
	bus_write_4(sc->sc_res[0], GEM_MAC_XIF_CONFIG, GEM_MAC_XIF_TX_MII_ENA);
}

static void
gem_start(ifp)
	struct ifnet *ifp;
{
	struct gem_softc *sc = (struct gem_softc *)ifp->if_softc;

	GEM_LOCK(sc);
	gem_start_locked(ifp);
	GEM_UNLOCK(sc);
}

static void
gem_start_locked(ifp)
	struct ifnet *ifp;
{
	struct gem_softc *sc = (struct gem_softc *)ifp->if_softc;
	struct mbuf *m;
	int ntx = 0;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || (sc->sc_flags & GEM_LINK) == 0)
		return;

#ifdef GEM_DEBUG
	CTR4(KTR_GEM, "%s: %s: txfree %d, txnext %d",
	    device_get_name(sc->sc_dev), __func__, sc->sc_txfree,
	    sc->sc_txnext);
#endif
	for (; !IFQ_DRV_IS_EMPTY(&ifp->if_snd) && sc->sc_txfree > 1;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		if (gem_load_txmbuf(sc, &m) != 0) {
			if (m == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			break;
		}
		ntx++;
		/* Kick the transmitter. */
#ifdef GEM_DEBUG
		CTR3(KTR_GEM, "%s: %s: kicking tx %d",
		    device_get_name(sc->sc_dev), __func__, sc->sc_txnext);
#endif
		GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
		bus_write_4(sc->sc_res[0], GEM_TX_KICK,
			sc->sc_txnext);

		BPF_MTAP(ifp, m);
	}

	if (ntx > 0) {
#ifdef GEM_DEBUG
		CTR2(KTR_GEM, "%s: packets enqueued, OWN on %d",
		    device_get_name(sc->sc_dev), sc->sc_txnext);
#endif

		/* Set a watchdog timer in case the chip flakes out. */
		sc->sc_wdog_timer = 5;
#ifdef GEM_DEBUG
		CTR3(KTR_GEM, "%s: %s: watchdog %d",
		    device_get_name(sc->sc_dev), __func__, sc->sc_wdog_timer);
#endif
	}
}

/*
 * Transmit interrupt.
 */
static void
gem_tint(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct gem_txsoft *txs;
	int txlast;
	int progress = 0;

#ifdef GEM_DEBUG
	CTR2(KTR_GEM, "%s: %s", device_get_name(sc->sc_dev), __func__);
#endif

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTREAD);
	while ((txs = STAILQ_FIRST(&sc->sc_txdirtyq)) != NULL) {

#ifdef GEM_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			int i;
			printf("    txsoft %p transmit chain:\n", txs);
			for (i = txs->txs_firstdesc;; i = GEM_NEXTTX(i)) {
				printf("descriptor %d: ", i);
				printf("gd_flags: 0x%016llx\t", (long long)
					GEM_DMA_READ(sc, sc->sc_txdescs[i].gd_flags));
				printf("gd_addr: 0x%016llx\n", (long long)
					GEM_DMA_READ(sc, sc->sc_txdescs[i].gd_addr));
				if (i == txs->txs_lastdesc)
					break;
			}
		}
#endif

		/*
		 * In theory, we could harvest some descriptors before
		 * the ring is empty, but that's a bit complicated.
		 *
		 * GEM_TX_COMPLETION points to the last descriptor
		 * processed +1.
		 */
		txlast = bus_read_4(sc->sc_res[0], GEM_TX_COMPLETION);
#ifdef GEM_DEBUG
		CTR4(KTR_GEM, "%s: txs->txs_firstdesc = %d, "
		    "txs->txs_lastdesc = %d, txlast = %d",
		    __func__, txs->txs_firstdesc, txs->txs_lastdesc, txlast);
#endif
		if (txs->txs_firstdesc <= txs->txs_lastdesc) {
			if ((txlast >= txs->txs_firstdesc) &&
				(txlast <= txs->txs_lastdesc))
				break;
		} else {
			/* Ick -- this command wraps */
			if ((txlast >= txs->txs_firstdesc) ||
				(txlast <= txs->txs_lastdesc))
				break;
		}

#ifdef GEM_DEBUG
		CTR1(KTR_GEM, "%s: releasing a desc", __func__);
#endif
		STAILQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		sc->sc_txfree += txs->txs_ndescs;

		bus_dmamap_sync(sc->sc_tdmatag, txs->txs_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_tdmatag, txs->txs_dmamap);
		if (txs->txs_mbuf != NULL) {
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}

		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		ifp->if_opackets++;
		progress = 1;
	}

#ifdef GEM_DEBUG
	CTR4(KTR_GEM, "%s: GEM_TX_STATE_MACHINE %x "
	    "GEM_TX_DATA_PTR %llx "
	    "GEM_TX_COMPLETION %x",
	    __func__,
	    bus_read_4(sc->sc_res[0], GEM_TX_STATE_MACHINE),
	    ((long long) bus_read_4(sc->sc_res[0], GEM_TX_DATA_PTR_HI) << 32) |
	    bus_read_4(sc->sc_res[0], GEM_TX_DATA_PTR_LO),
	    bus_read_4(sc->sc_res[0], GEM_TX_COMPLETION));
#endif

	if (progress) {
		if (sc->sc_txfree == GEM_NTXDESC - 1)
			sc->sc_txwin = 0;

		/* Freed some descriptors, so reset IFF_DRV_OACTIVE and restart. */
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		sc->sc_wdog_timer = STAILQ_EMPTY(&sc->sc_txdirtyq) ? 0 : 5;

		if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
		    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			gem_start_locked(ifp);
	}

#ifdef GEM_DEBUG
	CTR3(KTR_GEM, "%s: %s: watchdog %d",
	    device_get_name(sc->sc_dev), __func__, sc->sc_wdog_timer);
#endif
}

#ifdef GEM_RINT_TIMEOUT
static void
gem_rint_timeout(arg)
	void *arg;
{
	struct gem_softc *sc = (struct gem_softc *)arg;

	GEM_LOCK_ASSERT(sc, MA_OWNED);
	gem_rint(sc);
}
#endif

/*
 * Receive interrupt.
 */
static void
gem_rint(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	u_int64_t rxstat;
	u_int32_t rxcomp;

#ifdef GEM_RINT_TIMEOUT
	callout_stop(&sc->sc_rx_ch);
#endif
#ifdef GEM_DEBUG
	CTR2(KTR_GEM, "%s: %s", device_get_name(sc->sc_dev), __func__);
#endif

	/*
	 * Read the completion register once.  This limits
	 * how long the following loop can execute.
	 */
	rxcomp = bus_read_4(sc->sc_res[0], GEM_RX_COMPLETION);

#ifdef GEM_DEBUG
	CTR3(KTR_GEM, "%s: sc->rxptr %d, complete %d",
	    __func__, sc->sc_rxptr, rxcomp);
#endif
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTREAD);
	for (; sc->sc_rxptr != rxcomp;) {
		m = sc->sc_rxsoft[sc->sc_rxptr].rxs_mbuf;
		rxstat = GEM_DMA_READ(sc,
		    sc->sc_rxdescs[sc->sc_rxptr].gd_flags);

		if (rxstat & GEM_RD_OWN) {
#ifdef GEM_RINT_TIMEOUT
			/*
			 * The descriptor is still marked as owned, although
			 * it is supposed to have completed. This has been
			 * observed on some machines. Just exiting here
			 * might leave the packet sitting around until another
			 * one arrives to trigger a new interrupt, which is
			 * generally undesirable, so set up a timeout.
			 */
			callout_reset(&sc->sc_rx_ch, GEM_RXOWN_TICKS,
			    gem_rint_timeout, sc);
#endif
			m = NULL;
			goto kickit;
		}

		if (rxstat & GEM_RD_BAD_CRC) {
			ifp->if_ierrors++;
			device_printf(sc->sc_dev, "receive error: CRC error\n");
			GEM_INIT_RXDESC(sc, sc->sc_rxptr);
			m = NULL;
			goto kickit;
		}

#ifdef GEM_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			printf("    rxsoft %p descriptor %d: ",
			    &sc->sc_rxsoft[sc->sc_rxptr], sc->sc_rxptr);
			printf("gd_flags: 0x%016llx\t", (long long)
			    GEM_DMA_READ(sc, sc->sc_rxdescs[
			    sc->sc_rxptr].gd_flags));
			printf("gd_addr: 0x%016llx\n", (long long)
			    GEM_DMA_READ(sc, sc->sc_rxdescs[
			    sc->sc_rxptr].gd_addr));
		}
#endif

		/*
		 * Allocate a new mbuf cluster.  If that fails, we are
		 * out of memory, and must drop the packet and recycle
		 * the buffer that's already attached to this descriptor.
		 */
		if (gem_add_rxbuf(sc, sc->sc_rxptr) != 0) {
			ifp->if_ierrors++;
			GEM_INIT_RXDESC(sc, sc->sc_rxptr);
			m = NULL;
		}

kickit:
    		/*
		 * Update the RX kick register. This register has to point
		 * to the descriptor after the last valid one (before the
		 * current batch) and must be incremented in multiples of
		 * 4 (because the DMA engine fetches/updates descriptors
		 * in batches of 4).
		 */
		sc->sc_rxptr = GEM_NEXTRX(sc->sc_rxptr);
		if ((sc->sc_rxptr % 4) == 0) {
			GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
			bus_write_4(sc->sc_res[0], GEM_RX_KICK,
			    (sc->sc_rxptr + GEM_NRXDESC - 4) &
			    GEM_NRXDESC_MASK);
		}

		if (m == NULL) {
			if (rxstat & GEM_RD_OWN)
				break;
			continue;
		}

		ifp->if_ipackets++;
		m->m_data += 2; /* We're already off by two */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = GEM_RD_BUFLEN(rxstat);

		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0)
			gem_rxcksum(m, rxstat);

		/* Pass it on. */
		GEM_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		GEM_LOCK(sc);
	}

#ifdef GEM_DEBUG
	CTR3(KTR_GEM, "%s: done sc->rxptr %d, complete %d", __func__,
		sc->sc_rxptr, bus_read_4(sc->sc_res[0], GEM_RX_COMPLETION));
#endif
}

/*
 * gem_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
gem_add_rxbuf(sc, idx)
	struct gem_softc *sc;
	int idx;
{
	struct gem_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	bus_dma_segment_t segs[1];
	int error, nsegs;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;

#ifdef GEM_DEBUG
	/* bzero the packet to check dma */
	memset(m->m_ext.ext_buf, 0, m->m_ext.ext_size);
#endif

	if (rxs->rxs_mbuf != NULL) {
		bus_dmamap_sync(sc->sc_rdmatag, rxs->rxs_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_rdmatag, rxs->rxs_dmamap);
	}

	error = bus_dmamap_load_mbuf_sg(sc->sc_rdmatag, rxs->rxs_dmamap,
	    m, segs, &nsegs, BUS_DMA_NOWAIT);
	/* If nsegs is wrong then the stack is corrupt. */
	KASSERT(nsegs == 1, ("Too many segments returned!"));
	if (error != 0) {
		device_printf(sc->sc_dev, "can't load rx DMA map %d, error = "
		    "%d\n", idx, error);
		m_freem(m);
		return (error);
	}
	rxs->rxs_mbuf = m;
	rxs->rxs_paddr = segs[0].ds_addr;

	bus_dmamap_sync(sc->sc_rdmatag, rxs->rxs_dmamap, BUS_DMASYNC_PREREAD);

	GEM_INIT_RXDESC(sc, idx);

	return (0);
}

static void
gem_eint(sc, status)
	struct gem_softc *sc;
	u_int status;
{

	sc->sc_ifp->if_ierrors++;
	if ((status & GEM_INTR_RX_TAG_ERR) != 0) {
		gem_reset_rxdma(sc);
		return;
	}

	device_printf(sc->sc_dev, "%s: status=%x\n", __func__, status);
}

void
gem_intr(v)
	void *v;
{
	struct gem_softc *sc = (struct gem_softc *)v;
	uint32_t status, status2;

	GEM_LOCK(sc);
	status = bus_read_4(sc->sc_res[0], GEM_STATUS);

#ifdef GEM_DEBUG
	CTR4(KTR_GEM, "%s: %s: cplt %x, status %x",
		device_get_name(sc->sc_dev), __func__, (status>>19),
		(u_int)status);

	/*
	 * PCS interrupts must be cleared, otherwise no traffic is passed!
	 */
	if ((status & GEM_INTR_PCS) != 0) {
		status2 = bus_read_4(sc->sc_res[0], GEM_MII_INTERRUP_STATUS) |
		    bus_read_4(sc->sc_res[0], GEM_MII_INTERRUP_STATUS);
		if ((status2 & GEM_MII_INTERRUP_LINK) != 0)
			device_printf(sc->sc_dev,
			    "%s: PCS link status changed\n", __func__);
	}
	if ((status & GEM_MAC_CONTROL_STATUS) != 0) {
		status2 = bus_read_4(sc->sc_res[0], GEM_MAC_CONTROL_STATUS);
		if ((status2 & GEM_MAC_PAUSED) != 0)
			device_printf(sc->sc_dev,
			    "%s: PAUSE received (PAUSE time %d slots)\n",
			    __func__, GEM_MAC_PAUSE_TIME(status2));
		if ((status2 & GEM_MAC_PAUSE) != 0)
			device_printf(sc->sc_dev,
			    "%s: transited to PAUSE state\n", __func__);
		if ((status2 & GEM_MAC_RESUME) != 0)
			device_printf(sc->sc_dev,
			    "%s: transited to non-PAUSE state\n", __func__);
	}
	if ((status & GEM_INTR_MIF) != 0)
		device_printf(sc->sc_dev, "%s: MIF interrupt\n", __func__);
#endif

	if ((status &
	    (GEM_INTR_RX_TAG_ERR | GEM_INTR_PERR | GEM_INTR_BERR)) != 0)
		gem_eint(sc, status);

	if ((status & (GEM_INTR_RX_DONE | GEM_INTR_RX_NOBUF)) != 0)
		gem_rint(sc);

	if ((status & (GEM_INTR_TX_EMPTY | GEM_INTR_TX_INTME)) != 0)
		gem_tint(sc);

	if (status & GEM_INTR_TX_MAC) {
		status2 = bus_read_4(sc->sc_res[0], GEM_MAC_TX_STATUS);
		if (status2 & ~(GEM_MAC_TX_XMIT_DONE | GEM_MAC_TX_DEFER_EXP))
			device_printf(sc->sc_dev, "MAC tx fault, status %x\n",
			    status2);
		if (status2 & (GEM_MAC_TX_UNDERRUN | GEM_MAC_TX_PKT_TOO_LONG))
			gem_init_locked(sc);
	}
	if (status & GEM_INTR_RX_MAC) {
		status2 = bus_read_4(sc->sc_res[0], GEM_MAC_RX_STATUS);
		/*
		 * At least with GEM_SUN_GEM and some GEM_SUN_ERI
		 * revisions GEM_MAC_RX_OVERFLOW happen often due to a
		 * silicon bug so handle them silently. Moreover, it's
		 * likely that the receiver has hung so we reset it.
		 */
		if (status2 & GEM_MAC_RX_OVERFLOW) {
			sc->sc_ifp->if_ierrors++;
			gem_reset_rxdma(sc);
		} else if (status2 & ~(GEM_MAC_RX_DONE | GEM_MAC_RX_FRAME_CNT))
			device_printf(sc->sc_dev, "MAC rx fault, status %x\n",
			    status2);
	}
	GEM_UNLOCK(sc);
}

static int
gem_watchdog(sc)
	struct gem_softc *sc;
{

	GEM_LOCK_ASSERT(sc, MA_OWNED);

#ifdef GEM_DEBUG
	CTR4(KTR_GEM, "%s: GEM_RX_CONFIG %x GEM_MAC_RX_STATUS %x "
		"GEM_MAC_RX_CONFIG %x", __func__,
		bus_read_4(sc->sc_res[0], GEM_RX_CONFIG),
		bus_read_4(sc->sc_res[0], GEM_MAC_RX_STATUS),
		bus_read_4(sc->sc_res[0], GEM_MAC_RX_CONFIG));
	CTR4(KTR_GEM, "%s: GEM_TX_CONFIG %x GEM_MAC_TX_STATUS %x "
		"GEM_MAC_TX_CONFIG %x", __func__,
		bus_read_4(sc->sc_res[0], GEM_TX_CONFIG),
		bus_read_4(sc->sc_res[0], GEM_MAC_TX_STATUS),
		bus_read_4(sc->sc_res[0], GEM_MAC_TX_CONFIG));
#endif

	if (sc->sc_wdog_timer == 0 || --sc->sc_wdog_timer != 0)
		return (0);

	if ((sc->sc_flags & GEM_LINK) != 0)
		device_printf(sc->sc_dev, "device timeout\n");
	else if (bootverbose)
		device_printf(sc->sc_dev, "device timeout (no link)\n");
	++sc->sc_ifp->if_oerrors;

	/* Try to get more packets going. */
	gem_init_locked(sc);
	return (EJUSTRETURN);
}

/*
 * Initialize the MII Management Interface
 */
static void
gem_mifinit(sc)
	struct gem_softc *sc;
{

	/* Configure the MIF in frame mode */
	bus_write_4(sc->sc_res[0], GEM_MIF_CONFIG, bus_read_4(sc->sc_res[0],
	    GEM_MIF_CONFIG) & ~GEM_MIF_CONFIG_BB_ENA);
}

/*
 * MII interface
 *
 * The GEM MII interface supports at least three different operating modes:
 *
 * Bitbang mode is implemented using data, clock and output enable registers.
 *
 * Frame mode is implemented by loading a complete frame into the frame
 * register and polling the valid bit for completion.
 *
 * Polling mode uses the frame register but completion is indicated by
 * an interrupt.
 *
 */
int
gem_mii_readreg(dev, phy, reg)
	device_t dev;
	int phy, reg;
{
	struct gem_softc *sc = device_get_softc(dev);
	int n;
	u_int32_t v;

#ifdef GEM_DEBUG_PHY
	printf("%s: phy %d reg %d\n", __func__, phy, reg);
#endif

	if (sc->sc_phyad != -1 && phy != sc->sc_phyad)
		return (0);

	if ((sc->sc_flags & GEM_SERDES) != 0) {
		switch (reg) {
		case MII_BMCR:
			reg = GEM_MII_CONTROL;
			break;
		case MII_BMSR:
			reg = GEM_MII_STATUS;
			break;
		case MII_PHYIDR1:
		case MII_PHYIDR2:
			return (0);
		case MII_ANAR:
			reg = GEM_MII_ANAR;
			break;
		case MII_ANLPAR:
			reg = GEM_MII_ANLPAR;
			break;
		case MII_EXTSR:
			return (EXTSR_1000XFDX | EXTSR_1000XHDX);
		default:
			device_printf(sc->sc_dev,
			    "%s: unhandled register %d\n", __func__, reg);
			return (0);
		}
		return (bus_read_4(sc->sc_res[0], reg));
	}

	/* Construct the frame command */
	v = GEM_MIF_FRAME_READ |
	    (phy << GEM_MIF_PHY_SHIFT) |
	    (reg << GEM_MIF_REG_SHIFT);

	bus_write_4(sc->sc_res[0], GEM_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_read_4(sc->sc_res[0], GEM_MIF_FRAME);
		if (v & GEM_MIF_FRAME_TA0)
			return (v & GEM_MIF_FRAME_DATA);
	}

	device_printf(sc->sc_dev, "mii_read timeout\n");
	return (0);
}

int
gem_mii_writereg(dev, phy, reg, val)
	device_t dev;
	int phy, reg, val;
{
	struct gem_softc *sc = device_get_softc(dev);
	int n;
	u_int32_t v;

#ifdef GEM_DEBUG_PHY
	printf("%s: phy %d reg %d val %x\n", phy, reg, val, __func__);
#endif

	if (sc->sc_phyad != -1 && phy != sc->sc_phyad)
		return (0);

	if ((sc->sc_flags & GEM_SERDES) != 0) {
		switch (reg) {
		case MII_BMCR:
			reg = GEM_MII_CONTROL;
			break;
		case MII_BMSR:
			reg = GEM_MII_STATUS;
			break;
		case MII_ANAR:
			bus_write_4(sc->sc_res[0], GEM_MII_CONFIG, 0);
			bus_barrier(sc->sc_res[0], GEM_MII_CONFIG, 4,
			    BUS_SPACE_BARRIER_WRITE);
			bus_write_4(sc->sc_res[0], GEM_MII_ANAR, val);
			bus_write_4(sc->sc_res[0], GEM_MII_SLINK_CONTROL,
			    GEM_MII_SLINK_LOOPBACK | GEM_MII_SLINK_EN_SYNC_D);
			bus_write_4(sc->sc_res[0], GEM_MII_CONFIG,
			    GEM_MII_CONFIG_ENABLE);
			return (0);
		case MII_ANLPAR:
			reg = GEM_MII_ANLPAR;
			break;
		default:
			device_printf(sc->sc_dev,
			    "%s: unhandled register %d\n", __func__, reg);
			return (0);
		}
		bus_write_4(sc->sc_res[0], reg, val);
		return (0);
	}

	/* Construct the frame command */
	v = GEM_MIF_FRAME_WRITE |
	    (phy << GEM_MIF_PHY_SHIFT) |
	    (reg << GEM_MIF_REG_SHIFT) |
	    (val & GEM_MIF_FRAME_DATA);

	bus_write_4(sc->sc_res[0], GEM_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_read_4(sc->sc_res[0], GEM_MIF_FRAME);
		if (v & GEM_MIF_FRAME_TA0)
			return (1);
	}

	device_printf(sc->sc_dev, "mii_write timeout\n");
	return (0);
}

void
gem_mii_statchg(dev)
	device_t dev;
{
	struct gem_softc *sc = device_get_softc(dev);
	int gigabit;
	uint32_t rxcfg, txcfg, v;

#ifdef GEM_DEBUG
	if ((sc->sc_ifflags & IFF_DEBUG) != 0)
		device_printf(sc->sc_dev, "%s: status change: PHY = %d\n",
		    __func__, sc->sc_phyad);
#endif

	if ((sc->sc_mii->mii_media_status & IFM_ACTIVE) != 0 &&
	    IFM_SUBTYPE(sc->sc_mii->mii_media_active) != IFM_NONE)
		sc->sc_flags |= GEM_LINK;
	else
		sc->sc_flags &= ~GEM_LINK;

	switch (IFM_SUBTYPE(sc->sc_mii->mii_media_active)) {
	case IFM_1000_SX:
	case IFM_1000_LX:
	case IFM_1000_CX:
	case IFM_1000_T:
		gigabit = 1;
		break;
	default:
		gigabit = 0;
	}

	/*
	 * The configuration done here corresponds to the steps F) and
	 * G) and as far as enabling of RX and TX MAC goes also step H)
	 * of the initialization sequence outlined in section 3.2.1 of
	 * the GEM Gigabit Ethernet ASIC Specification.
	 */

	rxcfg = bus_read_4(sc->sc_res[0], GEM_MAC_RX_CONFIG);
	rxcfg &= ~(GEM_MAC_RX_CARR_EXTEND | GEM_MAC_RX_ENABLE);
	txcfg = GEM_MAC_TX_ENA_IPG0 | GEM_MAC_TX_NGU | GEM_MAC_TX_NGU_LIMIT;
	if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_FDX) != 0)
		txcfg |= GEM_MAC_TX_IGN_CARRIER | GEM_MAC_TX_IGN_COLLIS;
	else if (gigabit != 0) {
		rxcfg |= GEM_MAC_RX_CARR_EXTEND;
		txcfg |= GEM_MAC_TX_CARR_EXTEND;
	}
	bus_write_4(sc->sc_res[0], GEM_MAC_TX_CONFIG, 0);
	bus_barrier(sc->sc_res[0], GEM_MAC_TX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_MAC_TX_CONFIG, GEM_MAC_TX_ENABLE, 0))
		device_printf(sc->sc_dev, "cannot disable TX MAC\n");
	bus_write_4(sc->sc_res[0], GEM_MAC_TX_CONFIG, txcfg);
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, 0);
	bus_barrier(sc->sc_res[0], GEM_MAC_RX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_MAC_RX_CONFIG, GEM_MAC_RX_ENABLE, 0))
		device_printf(sc->sc_dev, "cannot disable RX MAC\n");
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, rxcfg);

	v = bus_read_4(sc->sc_res[0], GEM_MAC_CONTROL_CONFIG) &
	    ~(GEM_MAC_CC_RX_PAUSE | GEM_MAC_CC_TX_PAUSE);
#ifdef notyet
	if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
		v |= GEM_MAC_CC_RX_PAUSE;
	if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
		v |= GEM_MAC_CC_TX_PAUSE;
#endif
	bus_write_4(sc->sc_res[0], GEM_MAC_CONTROL_CONFIG, v);

	if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_FDX) == 0 &&
	    gigabit != 0)
		bus_write_4(sc->sc_res[0], GEM_MAC_SLOT_TIME,
		    GEM_MAC_SLOT_TIME_CARR_EXTEND);
	else
		bus_write_4(sc->sc_res[0], GEM_MAC_SLOT_TIME,
		    GEM_MAC_SLOT_TIME_NORMAL);

	/* XIF Configuration */
	v = GEM_MAC_XIF_LINK_LED;
	v |= GEM_MAC_XIF_TX_MII_ENA;
	if ((sc->sc_flags & GEM_SERDES) == 0) {
		if ((bus_read_4(sc->sc_res[0], GEM_MIF_CONFIG) &
		    GEM_MIF_CONFIG_PHY_SEL) != 0 &&
		    (IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_FDX) == 0)
			/* External MII needs echo disable if half duplex. */
 			v |= GEM_MAC_XIF_ECHO_DISABL;
		else
			/*
			 * Internal MII needs buffer enable.
			 * XXX buffer enable makes only sense for an
			 * external PHY.
			 */
			v |= GEM_MAC_XIF_MII_BUF_ENA;
	}
	if (gigabit != 0)
		v |= GEM_MAC_XIF_GMII_MODE;
	if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_FDX) != 0)
		v |= GEM_MAC_XIF_FDPLX_LED;
	bus_write_4(sc->sc_res[0], GEM_MAC_XIF_CONFIG, v);

	if ((sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) != 0 &&
	    (sc->sc_flags & GEM_LINK) != 0) {
		bus_write_4(sc->sc_res[0], GEM_MAC_TX_CONFIG,
		    txcfg | GEM_MAC_TX_ENABLE);
		bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG,
		    rxcfg | GEM_MAC_RX_ENABLE);
	}
}

int
gem_mediachange(ifp)
	struct ifnet *ifp;
{
	struct gem_softc *sc = ifp->if_softc;
	int error;

	/* XXX Add support for serial media. */

	GEM_LOCK(sc);
	error = mii_mediachg(sc->sc_mii);
	GEM_UNLOCK(sc);
	return (error);
}

void
gem_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct gem_softc *sc = ifp->if_softc;

	GEM_LOCK(sc);
	if ((ifp->if_flags & IFF_UP) == 0) {
		GEM_UNLOCK(sc);
		return;
	}

	mii_pollstat(sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii->mii_media_active;
	ifmr->ifm_status = sc->sc_mii->mii_media_status;
	GEM_UNLOCK(sc);
}

/*
 * Process an ioctl request.
 */
static int
gem_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct gem_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		GEM_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0 &&
			    ((ifp->if_flags ^ sc->sc_ifflags) &
			    (IFF_ALLMULTI | IFF_PROMISC)) != 0)
				gem_setladrf(sc);
			else
				gem_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				gem_stop(ifp, 0);
		}
		if ((ifp->if_flags & IFF_LINK0) != 0)
			sc->sc_csum_features |= CSUM_UDP;
		else
			sc->sc_csum_features &= ~CSUM_UDP;
		if ((ifp->if_capenable & IFCAP_TXCSUM) != 0)
			ifp->if_hwassist = sc->sc_csum_features;
		sc->sc_ifflags = ifp->if_flags;
		GEM_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		GEM_LOCK(sc);
		gem_setladrf(sc);
		GEM_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
		GEM_LOCK(sc);
		ifp->if_capenable = ifr->ifr_reqcap;
		if ((ifp->if_capenable & IFCAP_TXCSUM) != 0)
			ifp->if_hwassist = sc->sc_csum_features;
		else
			ifp->if_hwassist = 0;
		GEM_UNLOCK(sc);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/*
 * Set up the logical address filter.
 */
static void
gem_setladrf(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ifmultiaddr *inm;
	u_int32_t crc;
	u_int32_t hash[16];
	u_int32_t v;
	int i;

	GEM_LOCK_ASSERT(sc, MA_OWNED);

	/* Get current RX configuration */
	v = bus_read_4(sc->sc_res[0], GEM_MAC_RX_CONFIG);

	/*
	 * Turn off promiscuous mode, promiscuous group mode (all multicast),
	 * and hash filter.  Depending on the case, the right bit will be
	 * enabled.
	 */
	v &= ~(GEM_MAC_RX_PROMISCUOUS|GEM_MAC_RX_HASH_FILTER|
	    GEM_MAC_RX_PROMISC_GRP);

	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, v);
	bus_barrier(sc->sc_res[0], GEM_MAC_RX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, GEM_MAC_RX_CONFIG, GEM_MAC_RX_HASH_FILTER, 0))
		device_printf(sc->sc_dev, "cannot disable RX hash filter\n");

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		v |= GEM_MAC_RX_PROMISCUOUS;
		goto chipit;
	}
	if ((ifp->if_flags & IFF_ALLMULTI) != 0) {
		v |= GEM_MAC_RX_PROMISC_GRP;
		goto chipit;
	}

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 8 bits as an
	 * index into the 256 bit logical address filter.  The high order 4
	 * bits selects the word, while the other 4 bits select the bit within
	 * the word (where bit 0 is the MSB).
	 */

	/* Clear hash table */
	memset(hash, 0, sizeof(hash));

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(inm, &ifp->if_multiaddrs, ifma_link) {
		if (inm->ifma_addr->sa_family != AF_LINK)
			continue;
		crc = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    inm->ifma_addr), ETHER_ADDR_LEN);

		/* Just want the 8 most significant bits. */
		crc >>= 24;

		/* Set the corresponding bit in the filter. */
		hash[crc >> 4] |= 1 << (15 - (crc & 15));
	}
	IF_ADDR_UNLOCK(ifp);

	v |= GEM_MAC_RX_HASH_FILTER;

	/* Now load the hash table into the chip (if we are using it) */
	for (i = 0; i < 16; i++) {
		bus_write_4(sc->sc_res[0],
		    GEM_MAC_HASH0 + i * (GEM_MAC_HASH1-GEM_MAC_HASH0),
		    hash[i]);
	}

chipit:
	bus_write_4(sc->sc_res[0], GEM_MAC_RX_CONFIG, v);
}
