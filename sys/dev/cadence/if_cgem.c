/*-
 * Copyright (c) 2012-2013 Thomas Skibo
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * A network interface driver for Cadence GEM Gigabit Ethernet
 * interface such as the one used in Xilinx Zynq-7000 SoC.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.4) November 16, 2012.  Xilinx doc UG585.  GEM is covered in Ch. 16
 * and register definitions are in appendix B.18.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/cadence/if_cgem_hw.h>

#include "miibus_if.h"

#define IF_CGEM_NAME "cgem"

#define CGEM_NUM_RX_DESCS	256	/* size of receive descriptor ring */
#define CGEM_NUM_TX_DESCS	256	/* size of transmit descriptor ring */

#define MAX_DESC_RING_SIZE (MAX(CGEM_NUM_RX_DESCS*sizeof(struct cgem_rx_desc),\
				CGEM_NUM_TX_DESCS*sizeof(struct cgem_tx_desc)))


/* Default for sysctl rxbufs.  Must be < CGEM_NUM_RX_DESCS of course. */
#define DEFAULT_NUM_RX_BUFS	64	/* number of receive bufs to queue. */

#define TX_MAX_DMA_SEGS		4	/* maximum segs in a tx mbuf dma */

#define CGEM_CKSUM_ASSIST	(CSUM_IP | CSUM_TCP | CSUM_UDP | \
				 CSUM_TCP_IPV6 | CSUM_UDP_IPV6)

struct cgem_softc {
	struct ifnet		*ifp;
	struct mtx		sc_mtx;
	device_t		dev;
	device_t		miibus;
	int			if_old_flags;
	struct resource 	*mem_res;
	struct resource 	*irq_res;
	void			*intrhand;
	struct callout		tick_ch;
	uint32_t		net_ctl_shadow;
	u_char			eaddr[6];

	bus_dma_tag_t		desc_dma_tag;
	bus_dma_tag_t		mbuf_dma_tag;

	/* receive descriptor ring */
	struct cgem_rx_desc	*rxring;
	bus_addr_t		rxring_physaddr;
	struct mbuf		*rxring_m[CGEM_NUM_RX_DESCS];
	bus_dmamap_t		rxring_m_dmamap[CGEM_NUM_RX_DESCS];
	int			rxring_hd_ptr;	/* where to put rcv bufs */
	int			rxring_tl_ptr;	/* where to get receives */
	int			rxring_queued;	/* how many rcv bufs queued */
 	bus_dmamap_t		rxring_dma_map;
	int			rxbufs;		/* tunable number rcv bufs */
	int			rxoverruns;	/* rx ring overruns */

	/* transmit descriptor ring */
	struct cgem_tx_desc	*txring;
	bus_addr_t		txring_physaddr;
	struct mbuf		*txring_m[CGEM_NUM_TX_DESCS];
	bus_dmamap_t		txring_m_dmamap[CGEM_NUM_TX_DESCS];
	int			txring_hd_ptr;	/* where to put next xmits */
	int			txring_tl_ptr;	/* next xmit mbuf to free */
	int			txring_queued;	/* num xmits segs queued */
	bus_dmamap_t		txring_dma_map;
};

#define RD4(sc, off) 		(bus_read_4((sc)->mem_res, (off)))
#define WR4(sc, off, val) 	(bus_write_4((sc)->mem_res, (off), (val)))
#define BARRIER(sc, off, len, flags) \
	(bus_barrier((sc)->mem_res, (off), (len), (flags))

#define CGEM_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define CGEM_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)
#define CGEM_LOCK_INIT(sc)	\
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev), \
		 MTX_NETWORK_LOCK, MTX_DEF)
#define CGEM_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->sc_mtx)
#define CGEM_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

static devclass_t cgem_devclass;

static int cgem_probe(device_t dev);
static int cgem_attach(device_t dev);
static int cgem_detach(device_t dev);
static void cgem_tick(void *);
static void cgem_intr(void *);

static void
cgem_get_mac(struct cgem_softc *sc, u_char eaddr[])
{
	int i;
	uint32_t rnd;

	/* See if boot loader gave us a MAC address already. */
	for (i = 0; i < 4; i++) {
		uint32_t low = RD4(sc, CGEM_SPEC_ADDR_LOW(i));
		uint32_t high = RD4(sc, CGEM_SPEC_ADDR_HI(i)) & 0xffff;
		if (low != 0 || high != 0) {
			eaddr[0] = low & 0xff;
			eaddr[1] = (low >> 8) & 0xff;
			eaddr[2] = (low >> 16) & 0xff;
			eaddr[3] = (low >> 24) & 0xff;
			eaddr[4] = high & 0xff;
			eaddr[5] = (high >> 8) & 0xff;
			break;
		}
	}

	/* No MAC from boot loader?  Assign a random one. */
	if (i == 4) {
		rnd = arc4random();

		eaddr[0] = 'b';
		eaddr[1] = 's';
		eaddr[2] = 'd';
		eaddr[3] = (rnd >> 16) & 0xff;
		eaddr[4] = (rnd >> 8) & 0xff;
		eaddr[5] = rnd & 0xff;

		device_printf(sc->dev, "no mac address found, assigning "
			      "random: %02x:%02x:%02x:%02x:%02x:%02x\n",
			      eaddr[0], eaddr[1], eaddr[2],
			      eaddr[3], eaddr[4], eaddr[5]);

		WR4(sc, CGEM_SPEC_ADDR_LOW(0), (eaddr[3] << 24) |
		    (eaddr[2] << 16) | (eaddr[1] << 8) | eaddr[0]);
		WR4(sc, CGEM_SPEC_ADDR_HI(0), (eaddr[5] << 8) | eaddr[4]);
	}
}

/* cgem_mac_hash():  map 48-bit address to a 6-bit hash.
 * The 6-bit hash corresponds to a bit in a 64-bit hash
 * register.  Setting that bit in the hash register enables
 * reception of all frames with a destination address that hashes
 * to that 6-bit value.
 *
 * The hash function is described in sec. 16.2.3 in the Zynq-7000 Tech
 * Reference Manual.  Bits 0-5 in the hash are the exclusive-or of
 * every sixth bit in the destination address.
 */
static int
cgem_mac_hash(u_char eaddr[])
{
	int hash;
	int i, j;

	hash = 0;
	for (i = 0; i < 6; i++)
		for (j = i; j < 48; j += 6)
			if ((eaddr[j >> 3] & (1 << (j & 7))) != 0)
				hash ^= (1 << i);

	return hash;
}

/* After any change in rx flags or multi-cast addresses, set up
 * hash registers and net config register bits.
 */
static void
cgem_rx_filter(struct cgem_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	struct ifmultiaddr *ifma;
	int index;
	uint32_t hash_hi, hash_lo;
	uint32_t net_cfg;

	hash_hi = 0;
	hash_lo = 0;

	net_cfg = RD4(sc, CGEM_NET_CFG);

	net_cfg &= ~(CGEM_NET_CFG_MULTI_HASH_EN |
		     CGEM_NET_CFG_NO_BCAST | 
		     CGEM_NET_CFG_COPY_ALL);

	if ((ifp->if_flags & IFF_PROMISC) != 0)
		net_cfg |= CGEM_NET_CFG_COPY_ALL;
	else {
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			net_cfg |= CGEM_NET_CFG_NO_BCAST;
		if ((ifp->if_flags & IFF_ALLMULTI) != 0) {
			hash_hi = 0xffffffff;
			hash_lo = 0xffffffff;
		} else {
			if_maddr_rlock(ifp);
			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;
				index = cgem_mac_hash(
					LLADDR((struct sockaddr_dl *)
					       ifma->ifma_addr));
				if (index > 31)
					hash_hi |= (1<<(index-32));
				else
					hash_lo |= (1<<index);
			}
			if_maddr_runlock(ifp);
		}

		if (hash_hi != 0 || hash_lo != 0)
			net_cfg |= CGEM_NET_CFG_MULTI_HASH_EN;
	}

	WR4(sc, CGEM_HASH_TOP, hash_hi);
	WR4(sc, CGEM_HASH_BOT, hash_lo);
	WR4(sc, CGEM_NET_CFG, net_cfg);
}

/* For bus_dmamap_load() callback. */
static void
cgem_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (nsegs != 1 || error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/* Create DMA'able descriptor rings. */
static int
cgem_setup_descs(struct cgem_softc *sc)
{
	int i, err;

	sc->txring = NULL;
	sc->rxring = NULL;

	/* Allocate non-cached DMA space for RX and TX descriptors.
	 */
	err = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
				 BUS_SPACE_MAXADDR_32BIT,
				 BUS_SPACE_MAXADDR,
				 NULL, NULL,
				 MAX_DESC_RING_SIZE,
				 1,
				 MAX_DESC_RING_SIZE,
				 0,
				 busdma_lock_mutex,
				 &sc->sc_mtx,
				 &sc->desc_dma_tag);
	if (err)
		return (err);

	/* Set up a bus_dma_tag for mbufs. */
	err = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
				 BUS_SPACE_MAXADDR_32BIT,
				 BUS_SPACE_MAXADDR,
				 NULL, NULL,
				 MCLBYTES,
				 TX_MAX_DMA_SEGS,
				 MCLBYTES,
				 0,
				 busdma_lock_mutex,
				 &sc->sc_mtx,
				 &sc->mbuf_dma_tag);
	if (err)
		return (err);

	/* Allocate DMA memory in non-cacheable space. */
	err = bus_dmamem_alloc(sc->desc_dma_tag,
			       (void **)&sc->rxring,
			       BUS_DMA_NOWAIT | BUS_DMA_COHERENT,
			       &sc->rxring_dma_map);
	if (err)
		return (err);

	/* Load descriptor DMA memory. */
	err = bus_dmamap_load(sc->desc_dma_tag, sc->rxring_dma_map,
			      (void *)sc->rxring,
			      CGEM_NUM_RX_DESCS*sizeof(struct cgem_rx_desc),
			      cgem_getaddr, &sc->rxring_physaddr,
			      BUS_DMA_NOWAIT);
	if (err)
		return (err);

	/* Initialize RX descriptors. */
	for (i = 0; i < CGEM_NUM_RX_DESCS; i++) {
		sc->rxring[i].addr = CGEM_RXDESC_OWN;
		sc->rxring[i].ctl = 0;
		sc->rxring_m[i] = NULL;
		err = bus_dmamap_create(sc->mbuf_dma_tag, 0,
					&sc->rxring_m_dmamap[i]);
		if (err)
			return (err);
	}
	sc->rxring[CGEM_NUM_RX_DESCS - 1].addr |= CGEM_RXDESC_WRAP;

	sc->rxring_hd_ptr = 0;
	sc->rxring_tl_ptr = 0;
	sc->rxring_queued = 0;

	/* Allocate DMA memory for TX descriptors in non-cacheable space. */
	err = bus_dmamem_alloc(sc->desc_dma_tag,
			       (void **)&sc->txring,
			       BUS_DMA_NOWAIT | BUS_DMA_COHERENT,
			       &sc->txring_dma_map);
	if (err)
		return (err);

	/* Load TX descriptor DMA memory. */
	err = bus_dmamap_load(sc->desc_dma_tag, sc->txring_dma_map,
			      (void *)sc->txring,
			      CGEM_NUM_TX_DESCS*sizeof(struct cgem_tx_desc),
			      cgem_getaddr, &sc->txring_physaddr, 
			      BUS_DMA_NOWAIT);
	if (err)
		return (err);

	/* Initialize TX descriptor ring. */
	for (i = 0; i < CGEM_NUM_TX_DESCS; i++) {
		sc->txring[i].addr = 0;
		sc->txring[i].ctl = CGEM_TXDESC_USED;
		sc->txring_m[i] = NULL;
		err = bus_dmamap_create(sc->mbuf_dma_tag, 0,
					&sc->txring_m_dmamap[i]);
		if (err)
			return (err);
	}
	sc->txring[CGEM_NUM_TX_DESCS - 1].ctl |= CGEM_TXDESC_WRAP;

	sc->txring_hd_ptr = 0;
	sc->txring_tl_ptr = 0;
	sc->txring_queued = 0;

	return (0);
}

/* Fill receive descriptor ring with mbufs. */
static void
cgem_fill_rqueue(struct cgem_softc *sc)
{
	struct mbuf *m = NULL;
	bus_dma_segment_t segs[TX_MAX_DMA_SEGS];
	int nsegs;

	CGEM_ASSERT_LOCKED(sc);

	while (sc->rxring_queued < sc->rxbufs) {
		/* Get a cluster mbuf. */
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			break;

		m->m_len = MCLBYTES;
		m->m_pkthdr.len = MCLBYTES;
		m->m_pkthdr.rcvif = sc->ifp;

		/* Load map and plug in physical address. */
		if (bus_dmamap_load_mbuf_sg(sc->mbuf_dma_tag, 
			      sc->rxring_m_dmamap[sc->rxring_hd_ptr], m,
			      segs, &nsegs, BUS_DMA_NOWAIT)) {
			/* XXX: warn? */
			m_free(m);
			break;
		}
		sc->rxring_m[sc->rxring_hd_ptr] = m;

		/* Sync cache with receive buffer. */
		bus_dmamap_sync(sc->mbuf_dma_tag,
				sc->rxring_m_dmamap[sc->rxring_hd_ptr],
				BUS_DMASYNC_PREREAD);

		/* Write rx descriptor and increment head pointer. */
		sc->rxring[sc->rxring_hd_ptr].ctl = 0;
		if (sc->rxring_hd_ptr == CGEM_NUM_RX_DESCS - 1) {
			sc->rxring[sc->rxring_hd_ptr].addr = segs[0].ds_addr |
				CGEM_RXDESC_WRAP;
			sc->rxring_hd_ptr = 0;
		} else
			sc->rxring[sc->rxring_hd_ptr++].addr = segs[0].ds_addr;
			
		sc->rxring_queued++;
	}
}

/* Pull received packets off of receive descriptor ring. */
static void
cgem_recv(struct cgem_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	struct mbuf *m;
	uint32_t ctl;

	CGEM_ASSERT_LOCKED(sc);

	/* Pick up all packets in which the OWN bit is set. */
	while (sc->rxring_queued > 0 &&
	       (sc->rxring[sc->rxring_tl_ptr].addr & CGEM_RXDESC_OWN) != 0) {

		ctl = sc->rxring[sc->rxring_tl_ptr].ctl;

		/* Grab filled mbuf. */
		m = sc->rxring_m[sc->rxring_tl_ptr];
		sc->rxring_m[sc->rxring_tl_ptr] = NULL;

		/* Sync cache with receive buffer. */
		bus_dmamap_sync(sc->mbuf_dma_tag,
				sc->rxring_m_dmamap[sc->rxring_tl_ptr],
				BUS_DMASYNC_POSTREAD);

		/* Unload dmamap. */
		bus_dmamap_unload(sc->mbuf_dma_tag,
		  	sc->rxring_m_dmamap[sc->rxring_tl_ptr]);

		/* Increment tail pointer. */
		if (++sc->rxring_tl_ptr == CGEM_NUM_RX_DESCS)
			sc->rxring_tl_ptr = 0;
		sc->rxring_queued--;

		/* Check FCS and make sure entire packet landed in one mbuf
		 * cluster (which is much bigger than the largest ethernet
		 * packet).
		 */
		if ((ctl & CGEM_RXDESC_BAD_FCS) != 0 ||
		    (ctl & (CGEM_RXDESC_SOF | CGEM_RXDESC_EOF)) !=
		           (CGEM_RXDESC_SOF | CGEM_RXDESC_EOF)) {
			/* discard. */
			m_free(m);
			ifp->if_ierrors++;
			continue;
		}

		/* Hand it off to upper layers. */
		m->m_data += ETHER_ALIGN;
		m->m_len = (ctl & CGEM_RXDESC_LENGTH_MASK);
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len;

		/* Are we using hardware checksumming?  Check the
		 * status in the receive descriptor.
		 */
		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			/* TCP or UDP checks out, IP checks out too. */
			if ((ctl & CGEM_RXDESC_CKSUM_STAT_MASK) ==
			    CGEM_RXDESC_CKSUM_STAT_TCP_GOOD ||
			    (ctl & CGEM_RXDESC_CKSUM_STAT_MASK) ==
			    CGEM_RXDESC_CKSUM_STAT_UDP_GOOD) {
				m->m_pkthdr.csum_flags |=
					CSUM_IP_CHECKED | CSUM_IP_VALID |
					CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			} else if ((ctl & CGEM_RXDESC_CKSUM_STAT_MASK) ==
				   CGEM_RXDESC_CKSUM_STAT_IP_GOOD) {
				/* Only IP checks out. */
				m->m_pkthdr.csum_flags |=
					CSUM_IP_CHECKED | CSUM_IP_VALID;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}

		ifp->if_ipackets++;
		CGEM_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		CGEM_LOCK(sc);
	}
}

/* Find completed transmits and free their mbufs. */
static void
cgem_clean_tx(struct cgem_softc *sc)
{
	struct mbuf *m;
	uint32_t ctl;

	CGEM_ASSERT_LOCKED(sc);

	/* free up finished transmits. */
	while (sc->txring_queued > 0 &&
	       ((ctl = sc->txring[sc->txring_tl_ptr].ctl) &
		CGEM_TXDESC_USED) != 0) {

		/* Sync cache.  nop? */
		bus_dmamap_sync(sc->mbuf_dma_tag,
				sc->txring_m_dmamap[sc->txring_tl_ptr],
				BUS_DMASYNC_POSTWRITE);

		/* Unload DMA map. */
		bus_dmamap_unload(sc->mbuf_dma_tag,
				  sc->txring_m_dmamap[sc->txring_tl_ptr]);

		/* Free up the mbuf. */
		m = sc->txring_m[sc->txring_tl_ptr];
		sc->txring_m[sc->txring_tl_ptr] = NULL;
		m_freem(m);

		/* Check the status. */
		if ((ctl & CGEM_TXDESC_AHB_ERR) != 0) {
			/* Serious bus error. log to console. */
			device_printf(sc->dev, "cgem_clean_tx: Whoa! "
				   "AHB error, addr=0x%x\n",
				   sc->txring[sc->txring_tl_ptr].addr);
		} else if ((ctl & (CGEM_TXDESC_RETRY_ERR |
				   CGEM_TXDESC_LATE_COLL)) != 0) {
			sc->ifp->if_oerrors++;
		} else
			sc->ifp->if_opackets++;

		/* If the packet spanned more than one tx descriptor,
		 * skip descriptors until we find the end so that only
		 * start-of-frame descriptors are processed.
		 */
		while ((ctl & CGEM_TXDESC_LAST_BUF) == 0) {
			if ((ctl & CGEM_TXDESC_WRAP) != 0)
				sc->txring_tl_ptr = 0;
			else
				sc->txring_tl_ptr++;
			sc->txring_queued--;

			ctl = sc->txring[sc->txring_tl_ptr].ctl;

			sc->txring[sc->txring_tl_ptr].ctl =
				ctl | CGEM_TXDESC_USED;
		}

		/* Next descriptor. */
		if ((ctl & CGEM_TXDESC_WRAP) != 0)
			sc->txring_tl_ptr = 0;
		else
			sc->txring_tl_ptr++;
		sc->txring_queued--;
	}
}

/* Start transmits. */
static void
cgem_start_locked(struct ifnet *ifp)
{
	struct cgem_softc *sc = (struct cgem_softc *) ifp->if_softc;
	struct mbuf *m;
	bus_dma_segment_t segs[TX_MAX_DMA_SEGS];
	uint32_t ctl;
	int i, nsegs, wrap, err;

	CGEM_ASSERT_LOCKED(sc);

	if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) != 0)
		return;

	for (;;) {
		/* Check that there is room in the descriptor ring. */
		if (sc->txring_queued >= CGEM_NUM_TX_DESCS -
		    TX_MAX_DMA_SEGS - 1) {

			/* Try to make room. */
			cgem_clean_tx(sc);

			/* Still no room? */
			if (sc->txring_queued >= CGEM_NUM_TX_DESCS -
			    TX_MAX_DMA_SEGS - 1) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
		}

		/* Grab next transmit packet. */
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		/* Load DMA map. */
		err = bus_dmamap_load_mbuf_sg(sc->mbuf_dma_tag,
				      sc->txring_m_dmamap[sc->txring_hd_ptr],
				      m, segs, &nsegs, BUS_DMA_NOWAIT);
		if (err == EFBIG) {
			/* Too many segments!  defrag and try again. */
			struct mbuf *m2 = m_defrag(m, M_NOWAIT);

			if (m2 == NULL) {
				m_freem(m);
				continue;
			}
			m = m2;
			err = bus_dmamap_load_mbuf_sg(sc->mbuf_dma_tag,
				      sc->txring_m_dmamap[sc->txring_hd_ptr],
				      m, segs, &nsegs, BUS_DMA_NOWAIT);
		}
		if (err) {
			/* Give up. */
			m_freem(m);
			continue;
		}
		sc->txring_m[sc->txring_hd_ptr] = m;

		/* Sync tx buffer with cache. */
		bus_dmamap_sync(sc->mbuf_dma_tag,
				sc->txring_m_dmamap[sc->txring_hd_ptr],
				BUS_DMASYNC_PREWRITE);

		/* Set wrap flag if next packet might run off end of ring. */
		wrap = sc->txring_hd_ptr + nsegs + TX_MAX_DMA_SEGS >=
			CGEM_NUM_TX_DESCS;

		/* Fill in the TX descriptors back to front so that USED
		 * bit in first descriptor is cleared last.
		 */
		for (i = nsegs - 1; i >= 0; i--) {
			/* Descriptor address. */
			sc->txring[sc->txring_hd_ptr + i].addr =
				segs[i].ds_addr;

			/* Descriptor control word. */
			ctl = segs[i].ds_len;
			if (i == nsegs - 1) {
				ctl |= CGEM_TXDESC_LAST_BUF;
				if (wrap)
					ctl |= CGEM_TXDESC_WRAP;
			}
			sc->txring[sc->txring_hd_ptr + i].ctl = ctl;

			if (i != 0)
				sc->txring_m[sc->txring_hd_ptr + i] = NULL;
		}

		if (wrap)
			sc->txring_hd_ptr = 0;
		else
			sc->txring_hd_ptr += nsegs;
		sc->txring_queued += nsegs;

		/* Kick the transmitter. */
		WR4(sc, CGEM_NET_CTRL, sc->net_ctl_shadow |
		    CGEM_NET_CTRL_START_TX);
	}

}

static void
cgem_start(struct ifnet *ifp)
{
	struct cgem_softc *sc = (struct cgem_softc *) ifp->if_softc;

	CGEM_LOCK(sc);
	cgem_start_locked(ifp);
	CGEM_UNLOCK(sc);
}

/* Respond to changes in media. */
static void
cgem_media_update(struct cgem_softc *sc, int active)
{
	uint32_t net_cfg;

	CGEM_ASSERT_LOCKED(sc);

	/* Update hardware to reflect phy status. */
	net_cfg = RD4(sc, CGEM_NET_CFG);
	net_cfg &= ~(CGEM_NET_CFG_SPEED100 | CGEM_NET_CFG_GIGE_EN |
		     CGEM_NET_CFG_FULL_DUPLEX);

	if (IFM_SUBTYPE(active) == IFM_1000_T)
		net_cfg |= (CGEM_NET_CFG_SPEED100 | CGEM_NET_CFG_GIGE_EN);
	else if (IFM_SUBTYPE(active) == IFM_100_TX)
		net_cfg |= CGEM_NET_CFG_SPEED100;

	if ((active & IFM_FDX) != 0)
		net_cfg |= CGEM_NET_CFG_FULL_DUPLEX;
	WR4(sc, CGEM_NET_CFG, net_cfg);
}

static void
cgem_tick(void *arg)
{
	struct cgem_softc *sc = (struct cgem_softc *)arg;
	struct mii_data *mii;
	int active;

	CGEM_ASSERT_LOCKED(sc);

	/* Poll the phy. */
	if (sc->miibus != NULL) {
		mii = device_get_softc(sc->miibus);
		active = mii->mii_media_active;
		mii_tick(mii);
		if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
		    (IFM_ACTIVE | IFM_AVALID) &&
		    active != mii->mii_media_active)
			cgem_media_update(sc, mii->mii_media_active);
	}

	/* Next callout in one second. */
	callout_reset(&sc->tick_ch, hz, cgem_tick, sc);
}

/* Interrupt handler. */
static void
cgem_intr(void *arg)
{
	struct cgem_softc *sc = (struct cgem_softc *)arg;
	uint32_t istatus;

	CGEM_LOCK(sc);

	if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		CGEM_UNLOCK(sc);
		return;
	}

	istatus = RD4(sc, CGEM_INTR_STAT);
	WR4(sc, CGEM_INTR_STAT, istatus &
	    (CGEM_INTR_RX_COMPLETE | CGEM_INTR_TX_USED_READ |
	     CGEM_INTR_RX_OVERRUN | CGEM_INTR_HRESP_NOT_OK));

	/* Hresp not ok.  Something very bad with DMA.  Try to clear. */
	if ((istatus & CGEM_INTR_HRESP_NOT_OK) != 0) {
		printf("cgem_intr: hresp not okay! rx_status=0x%x\n",
		       RD4(sc, CGEM_RX_STAT));
		WR4(sc, CGEM_RX_STAT, CGEM_RX_STAT_HRESP_NOT_OK);
	}

	/* Transmitter has idled.  Free up any spent transmit buffers. */
	if ((istatus & CGEM_INTR_TX_USED_READ) != 0)
		cgem_clean_tx(sc);

	/* Packets received or overflow. */
	if ((istatus & (CGEM_INTR_RX_COMPLETE | CGEM_INTR_RX_OVERRUN)) != 0) {
		cgem_recv(sc);
		cgem_fill_rqueue(sc);
		if ((istatus & CGEM_INTR_RX_OVERRUN) != 0) {
			/* Clear rx status register. */
			sc->rxoverruns++;
			WR4(sc, CGEM_RX_STAT, CGEM_RX_STAT_ALL);
		}
	}

	CGEM_UNLOCK(sc);
}

/* Reset hardware. */
static void
cgem_reset(struct cgem_softc *sc)
{

	CGEM_ASSERT_LOCKED(sc);

	WR4(sc, CGEM_NET_CTRL, 0);
	WR4(sc, CGEM_NET_CFG, 0);
	WR4(sc, CGEM_NET_CTRL, CGEM_NET_CTRL_CLR_STAT_REGS);
	WR4(sc, CGEM_TX_STAT, CGEM_TX_STAT_ALL);
	WR4(sc, CGEM_RX_STAT, CGEM_RX_STAT_ALL);
	WR4(sc, CGEM_INTR_DIS, CGEM_INTR_ALL);
	WR4(sc, CGEM_HASH_BOT, 0);
	WR4(sc, CGEM_HASH_TOP, 0);
	WR4(sc, CGEM_TX_QBAR, 0);	/* manual says do this. */
	WR4(sc, CGEM_RX_QBAR, 0);

	/* Get management port running even if interface is down. */
	WR4(sc, CGEM_NET_CFG,
	    CGEM_NET_CFG_DBUS_WIDTH_32 |
	    CGEM_NET_CFG_MDC_CLK_DIV_64);

	sc->net_ctl_shadow = CGEM_NET_CTRL_MGMT_PORT_EN;
	WR4(sc, CGEM_NET_CTRL, sc->net_ctl_shadow);
}

/* Bring up the hardware. */
static void
cgem_config(struct cgem_softc *sc)
{
	uint32_t net_cfg;
	uint32_t dma_cfg;

	CGEM_ASSERT_LOCKED(sc);

	/* Program Net Config Register. */
	net_cfg = CGEM_NET_CFG_DBUS_WIDTH_32 |
		CGEM_NET_CFG_MDC_CLK_DIV_64 |
		CGEM_NET_CFG_FCS_REMOVE |
		CGEM_NET_CFG_RX_BUF_OFFSET(ETHER_ALIGN) |
		CGEM_NET_CFG_GIGE_EN |
		CGEM_NET_CFG_FULL_DUPLEX |
		CGEM_NET_CFG_SPEED100;

	/* Enable receive checksum offloading? */
	if ((sc->ifp->if_capenable & IFCAP_RXCSUM) != 0)
		net_cfg |=  CGEM_NET_CFG_RX_CHKSUM_OFFLD_EN;

	WR4(sc, CGEM_NET_CFG, net_cfg);

	/* Program DMA Config Register. */
	dma_cfg = CGEM_DMA_CFG_RX_BUF_SIZE(MCLBYTES) |
		CGEM_DMA_CFG_RX_PKTBUF_MEMSZ_SEL_8K |
		CGEM_DMA_CFG_TX_PKTBUF_MEMSZ_SEL |
		CGEM_DMA_CFG_AHB_FIXED_BURST_LEN_16;

	/* Enable transmit checksum offloading? */
	if ((sc->ifp->if_capenable & IFCAP_TXCSUM) != 0)
		dma_cfg |= CGEM_DMA_CFG_CHKSUM_GEN_OFFLOAD_EN;

	WR4(sc, CGEM_DMA_CFG, dma_cfg);

	/* Write the rx and tx descriptor ring addresses to the QBAR regs. */
	WR4(sc, CGEM_RX_QBAR, (uint32_t) sc->rxring_physaddr);
	WR4(sc, CGEM_TX_QBAR, (uint32_t) sc->txring_physaddr);
	
	/* Enable rx and tx. */
	sc->net_ctl_shadow |= (CGEM_NET_CTRL_TX_EN | CGEM_NET_CTRL_RX_EN);
	WR4(sc, CGEM_NET_CTRL, sc->net_ctl_shadow);

	/* Set up interrupts. */
	WR4(sc, CGEM_INTR_EN,
	    CGEM_INTR_RX_COMPLETE | CGEM_INTR_TX_USED_READ |
	    CGEM_INTR_RX_OVERRUN | CGEM_INTR_HRESP_NOT_OK);
}

/* Turn on interface and load up receive ring with buffers. */
static void
cgem_init_locked(struct cgem_softc *sc)
{
	struct mii_data *mii;

	CGEM_ASSERT_LOCKED(sc);

	if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	cgem_config(sc);
	cgem_fill_rqueue(sc);

	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	cgem_media_update(sc, mii->mii_media_active);
	cgem_start_locked(sc->ifp);

	callout_reset(&sc->tick_ch, hz, cgem_tick, sc);
}

static void
cgem_init(void *arg)
{
	struct cgem_softc *sc = (struct cgem_softc *)arg;

	CGEM_LOCK(sc);
	cgem_init_locked(sc);
	CGEM_UNLOCK(sc);
}

/* Turn off interface.  Free up any buffers in transmit or receive queues. */
static void
cgem_stop(struct cgem_softc *sc)
{
	int i;

	CGEM_ASSERT_LOCKED(sc);

	callout_stop(&sc->tick_ch);

	/* Shut down hardware. */
	cgem_reset(sc);

	/* Clear out transmit queue. */
	for (i = 0; i < CGEM_NUM_TX_DESCS; i++) {
		sc->txring[i].ctl = CGEM_TXDESC_USED;
		sc->txring[i].addr = 0;
		if (sc->txring_m[i]) {
			bus_dmamap_unload(sc->mbuf_dma_tag,
					  sc->txring_m_dmamap[i]);
			m_freem(sc->txring_m[i]);
			sc->txring_m[i] = NULL;
		}
	}
	sc->txring[CGEM_NUM_TX_DESCS - 1].ctl |= CGEM_TXDESC_WRAP;

	sc->txring_hd_ptr = 0;
	sc->txring_tl_ptr = 0;
	sc->txring_queued = 0;

	/* Clear out receive queue. */
	for (i = 0; i < CGEM_NUM_RX_DESCS; i++) {
		sc->rxring[i].addr = CGEM_RXDESC_OWN;
		sc->rxring[i].ctl = 0;
		if (sc->rxring_m[i]) {
			/* Unload dmamap. */
			bus_dmamap_unload(sc->mbuf_dma_tag,
				  sc->rxring_m_dmamap[sc->rxring_tl_ptr]);

			m_freem(sc->rxring_m[i]);
			sc->rxring_m[i] = NULL;
		}
	}
	sc->rxring[CGEM_NUM_RX_DESCS - 1].addr |= CGEM_RXDESC_WRAP;

	sc->rxring_hd_ptr = 0;
	sc->rxring_tl_ptr = 0;
	sc->rxring_queued = 0;
}


static int
cgem_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct cgem_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
	int error = 0, mask;

	switch (cmd) {
	case SIOCSIFFLAGS:
		CGEM_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
				if (((ifp->if_flags ^ sc->if_old_flags) &
				     (IFF_PROMISC | IFF_ALLMULTI)) != 0) {
					cgem_rx_filter(sc);
				}
			} else {
				cgem_init_locked(sc);
			}
		} else if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			cgem_stop(sc);
		}
		sc->if_old_flags = ifp->if_flags;
		CGEM_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Set up multi-cast filters. */
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			CGEM_LOCK(sc);
			cgem_rx_filter(sc);
			CGEM_UNLOCK(sc);
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	case SIOCSIFCAP:
		CGEM_LOCK(sc);
		mask = ifp->if_capenable ^ ifr->ifr_reqcap;

		if ((mask & IFCAP_TXCSUM) != 0) {
			if ((ifr->ifr_reqcap & IFCAP_TXCSUM) != 0) {
				/* Turn on TX checksumming. */
				ifp->if_capenable |= (IFCAP_TXCSUM |
						      IFCAP_TXCSUM_IPV6);
				ifp->if_hwassist |= CGEM_CKSUM_ASSIST;

				WR4(sc, CGEM_DMA_CFG,
				    RD4(sc, CGEM_DMA_CFG) |
				     CGEM_DMA_CFG_CHKSUM_GEN_OFFLOAD_EN);
			} else {
				/* Turn off TX checksumming. */
				ifp->if_capenable &= ~(IFCAP_TXCSUM |
						       IFCAP_TXCSUM_IPV6);
				ifp->if_hwassist &= ~CGEM_CKSUM_ASSIST;

				WR4(sc, CGEM_DMA_CFG,
				    RD4(sc, CGEM_DMA_CFG) &
				     ~CGEM_DMA_CFG_CHKSUM_GEN_OFFLOAD_EN);
			}
		}
		if ((mask & IFCAP_RXCSUM) != 0) {
			if ((ifr->ifr_reqcap & IFCAP_RXCSUM) != 0) {
				/* Turn on RX checksumming. */
				ifp->if_capenable |= (IFCAP_RXCSUM |
						      IFCAP_RXCSUM_IPV6);
				WR4(sc, CGEM_NET_CFG,
				    RD4(sc, CGEM_NET_CFG) |
				     CGEM_NET_CFG_RX_CHKSUM_OFFLD_EN);
			} else {
				/* Turn off RX checksumming. */
				ifp->if_capenable &= ~(IFCAP_RXCSUM |
						       IFCAP_RXCSUM_IPV6);
				WR4(sc, CGEM_NET_CFG,
				    RD4(sc, CGEM_NET_CFG) &
				     ~CGEM_NET_CFG_RX_CHKSUM_OFFLD_EN);
			}
		}

		CGEM_UNLOCK(sc);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/* MII bus support routines.
 */
static void
cgem_child_detached(device_t dev, device_t child)
{
	struct cgem_softc *sc = device_get_softc(dev);
	if (child == sc->miibus)
		sc->miibus = NULL;
}

static int
cgem_ifmedia_upd(struct ifnet *ifp)
{
	struct cgem_softc *sc = (struct cgem_softc *) ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	CGEM_LOCK(sc);
	mii_mediachg(mii);
	CGEM_UNLOCK(sc);
	return (0);
}

static void
cgem_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cgem_softc *sc = (struct cgem_softc *) ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	CGEM_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	CGEM_UNLOCK(sc);
}

static int
cgem_miibus_readreg(device_t dev, int phy, int reg)
{
	struct cgem_softc *sc = device_get_softc(dev);
	int tries, val;

	WR4(sc, CGEM_PHY_MAINT,
	    CGEM_PHY_MAINT_CLAUSE_22 | CGEM_PHY_MAINT_MUST_10 |
	    CGEM_PHY_MAINT_OP_READ |
	    (phy << CGEM_PHY_MAINT_PHY_ADDR_SHIFT) |
	    (reg << CGEM_PHY_MAINT_REG_ADDR_SHIFT));

	/* Wait for completion. */
	tries=0;
	while ((RD4(sc, CGEM_NET_STAT) & CGEM_NET_STAT_PHY_MGMT_IDLE) == 0) {
		DELAY(5);
		if (++tries > 200) {
			device_printf(dev, "phy read timeout: %d\n", reg);
			return (-1);
		}
	}

	val = RD4(sc, CGEM_PHY_MAINT) & CGEM_PHY_MAINT_DATA_MASK;

	return (val);
}

static int
cgem_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct cgem_softc *sc = device_get_softc(dev);
	int tries;
	
	WR4(sc, CGEM_PHY_MAINT,
	    CGEM_PHY_MAINT_CLAUSE_22 | CGEM_PHY_MAINT_MUST_10 |
	    CGEM_PHY_MAINT_OP_WRITE |
	    (phy << CGEM_PHY_MAINT_PHY_ADDR_SHIFT) |
	    (reg << CGEM_PHY_MAINT_REG_ADDR_SHIFT) |
	    (data & CGEM_PHY_MAINT_DATA_MASK));

	/* Wait for completion. */
	tries = 0;
	while ((RD4(sc, CGEM_NET_STAT) & CGEM_NET_STAT_PHY_MGMT_IDLE) == 0) {
		DELAY(5);
		if (++tries > 200) {
			device_printf(dev, "phy write timeout: %d\n", reg);
			return (-1);
		}
	}

	return (0);
}


static int
cgem_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "cadence,gem"))
		return (ENXIO);

	device_set_desc(dev, "Cadence CGEM Gigabit Ethernet Interface");
	return (0);
}

static int
cgem_attach(device_t dev)
{
	struct cgem_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = NULL;
	int rid, err;
	u_char eaddr[ETHER_ADDR_LEN];

	sc->dev = dev;
	CGEM_LOCK_INIT(sc);

	/* Get memory resource. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
					     RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		return (ENOMEM);
	}

	/* Get IRQ resource. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					     RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate interrupt resource.\n");
		cgem_detach(dev);
		return (ENOMEM);
	}

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "could not allocate ifnet structure\n");
		cgem_detach(dev);
		return (ENOMEM);
	}

	CGEM_LOCK(sc);

	/* Reset hardware. */
	cgem_reset(sc);

	/* Attach phy to mii bus. */
	err = mii_attach(dev, &sc->miibus, ifp,
			 cgem_ifmedia_upd, cgem_ifmedia_sts,
			 BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (err) {
		CGEM_UNLOCK(sc);
		device_printf(dev, "attaching PHYs failed\n");
		cgem_detach(dev);
		return (err);
	}

	/* Set up TX and RX descriptor area. */
	err = cgem_setup_descs(sc);
	if (err) {
		CGEM_UNLOCK(sc);
		device_printf(dev, "could not set up dma mem for descs.\n");
		cgem_detach(dev);
		return (ENOMEM);
	}

	/* Get a MAC address. */
	cgem_get_mac(sc, eaddr);

	/* Start ticks. */
	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	/* Set up ifnet structure. */
	ifp->if_softc = sc;
	if_initname(ifp, IF_CGEM_NAME, device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = cgem_start;
	ifp->if_ioctl = cgem_ioctl;
	ifp->if_init = cgem_init;
	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6;
	/* XXX: disable hw checksumming for now. */
	ifp->if_hwassist = 0;
	ifp->if_capenable = ifp->if_capabilities &
		~(IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6);
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	sc->if_old_flags = ifp->if_flags;
	sc->rxbufs = DEFAULT_NUM_RX_BUFS;

	ether_ifattach(ifp, eaddr);

	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE |
			     INTR_EXCL, NULL, cgem_intr, sc, &sc->intrhand);
	if (err) {
		CGEM_UNLOCK(sc);
		device_printf(dev, "could not set interrupt handler.\n");
		ether_ifdetach(ifp);
		cgem_detach(dev);
		return (err);
	}

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
		       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		       OID_AUTO, "rxbufs", CTLFLAG_RW,
		       &sc->rxbufs, 0,
		       "Number receive buffers to provide");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
		       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		       OID_AUTO, "_rxoverruns", CTLFLAG_RD,
		       &sc->rxoverruns, 0,
		       "Receive ring overrun events");

	CGEM_UNLOCK(sc);

	return (0);
}

static int
cgem_detach(device_t dev)
{
	struct cgem_softc *sc = device_get_softc(dev);
	int i;

	if (sc == NULL)
		return (ENODEV);

	if (device_is_attached(dev)) {
		CGEM_LOCK(sc);
		cgem_stop(sc);
		CGEM_UNLOCK(sc);
		callout_drain(&sc->tick_ch);
		sc->ifp->if_flags &= ~IFF_UP;
		ether_ifdetach(sc->ifp);
	}

	if (sc->miibus != NULL) {
		device_delete_child(dev, sc->miibus);
		sc->miibus = NULL;
	}

	/* Release resrouces. */
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     rman_get_rid(sc->mem_res), sc->mem_res);
		sc->mem_res = NULL;
	}
	if (sc->irq_res != NULL) {
		if (sc->intrhand)
			bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
		bus_release_resource(dev, SYS_RES_IRQ,
				     rman_get_rid(sc->irq_res), sc->irq_res);
		sc->irq_res = NULL;
	}

	/* Release DMA resources. */
	if (sc->rxring_dma_map != NULL) {
		bus_dmamem_free(sc->desc_dma_tag, sc->rxring,
				sc->rxring_dma_map);
		sc->rxring_dma_map = NULL;
		for (i = 0; i < CGEM_NUM_RX_DESCS; i++)
			if (sc->rxring_m_dmamap[i] != NULL) {
				bus_dmamap_destroy(sc->mbuf_dma_tag,
						   sc->rxring_m_dmamap[i]);
				sc->rxring_m_dmamap[i] = NULL;
			}
	}
	if (sc->txring_dma_map != NULL) {
		bus_dmamem_free(sc->desc_dma_tag, sc->txring,
				sc->txring_dma_map);
		sc->txring_dma_map = NULL;
		for (i = 0; i < CGEM_NUM_TX_DESCS; i++)
			if (sc->txring_m_dmamap[i] != NULL) {
				bus_dmamap_destroy(sc->mbuf_dma_tag,
						   sc->txring_m_dmamap[i]);
				sc->txring_m_dmamap[i] = NULL;
			}
	}
	if (sc->desc_dma_tag != NULL) {
		bus_dma_tag_destroy(sc->desc_dma_tag);
		sc->desc_dma_tag = NULL;
	}
	if (sc->mbuf_dma_tag != NULL) {
		bus_dma_tag_destroy(sc->mbuf_dma_tag);
		sc->mbuf_dma_tag = NULL;
	}

	bus_generic_detach(dev);

	CGEM_LOCK_DESTROY(sc);

	return (0);
}

static device_method_t cgem_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cgem_probe),
	DEVMETHOD(device_attach,	cgem_attach),
	DEVMETHOD(device_detach,	cgem_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	cgem_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	cgem_miibus_readreg),
	DEVMETHOD(miibus_writereg,	cgem_miibus_writereg),

	DEVMETHOD_END
};

static driver_t cgem_driver = {
	"cgem",
	cgem_methods,
	sizeof(struct cgem_softc),
};

DRIVER_MODULE(cgem, simplebus, cgem_driver, cgem_devclass, NULL, NULL);
DRIVER_MODULE(miibus, cgem, miibus_driver, miibus_devclass, NULL, NULL);
MODULE_DEPEND(cgem, miibus, 1, 1, 1);
MODULE_DEPEND(cgem, ether, 1, 1, 1);
