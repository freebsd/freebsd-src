/*
 * Copyright (C) 2011 Luigi Rizzo. All rights reserved.
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
 * $FreeBSD$
 *
 * netmap support for "re"
 * For details on netmap support please see ixgbe_netmap.h
 */


#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>


/*
 * wrapper to export locks to the generic code
 * We should not use the tx/rx locks
 */
static void
re_netmap_lock_wrapper(struct ifnet *ifp, int what, u_int queueid)
{
	struct rl_softc *adapter = ifp->if_softc;

	switch (what) {
	case NETMAP_CORE_LOCK:
		RL_LOCK(adapter);
		break;
	case NETMAP_CORE_UNLOCK:
		RL_UNLOCK(adapter);
		break;

	case NETMAP_TX_LOCK:
	case NETMAP_RX_LOCK:
	case NETMAP_TX_UNLOCK:
	case NETMAP_RX_UNLOCK:
		D("invalid lock call %d, no tx/rx locks here", what);
		break;
	}
}


/*
 * support for netmap register/unregisted. We are already under core lock.
 * only called on the first register or the last unregister.
 */
static int
re_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct rl_softc *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (na == NULL)
		return EINVAL;
	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	re_stop(adapter);

	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		/* save if_transmit to restore it later */
		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_start;

		re_init_locked(adapter);

		if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) == 0) {
			error = ENOMEM;
			goto fail;
		}
	} else {
fail:
		/* restore if_transmit */
		ifp->if_transmit = na->if_transmit;
		ifp->if_capenable &= ~IFCAP_NETMAP;
		re_init_locked(adapter);	/* also enables intr */
	}
	return (error);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 */
static int
re_netmap_txsync(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	struct rl_softc *sc = ifp->if_softc;
	struct rl_txdesc *txd = sc->rl_ldata.rl_tx_desc;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		RL_LOCK(sc);

	/* Sync the TX descriptor list */
	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
            sc->rl_ldata.rl_tx_list_map,
            BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* XXX move after the transmissions */
	/* record completed transmissions */
        for (n = 0, l = sc->rl_ldata.rl_tx_considx;
	    l != sc->rl_ldata.rl_tx_prodidx;
	    n++, l = RL_TX_DESC_NXT(sc, l)) {
		uint32_t cmdstat =
			le32toh(sc->rl_ldata.rl_tx_list[l].rl_cmdstat);
		if (cmdstat & RL_TDESC_STAT_OWN)
			break;
	}
	if (n > 0) {
		sc->rl_ldata.rl_tx_considx = l;
		sc->rl_ldata.rl_tx_free += n;
		kring->nr_hwavail += n;
	}

	/* update avail to what the kernel knows */
	ring->avail = kring->nr_hwavail;

	j = kring->nr_hwcur;
	if (j != k) {	/* we have new packets to send */
		l = sc->rl_ldata.rl_tx_prodidx;
		for (n = 0; j != k; n++) {
			struct netmap_slot *slot = &ring->slot[j];
			struct rl_desc *desc = &sc->rl_ldata.rl_tx_list[l];
			int cmd = slot->len | RL_TDESC_CMD_EOF |
				RL_TDESC_CMD_OWN | RL_TDESC_CMD_SOF ;
			uint64_t paddr;
			void *addr = PNMB(slot, &paddr);
			int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				if (do_lock)
					RL_UNLOCK(sc);
				// XXX what about prodidx ?
				return netmap_ring_reinit(kring);
			}

			if (l == lim)	/* mark end of ring */
				cmd |= RL_TDESC_CMD_EOR;

			if (slot->flags & NS_BUF_CHANGED) {
				desc->rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
				desc->rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
				/* buffer has changed, unload and reload map */
				netmap_reload_map(sc->rl_ldata.rl_tx_mtag,
					txd[l].tx_dmamap, addr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			slot->flags &= ~NS_REPORT;
			desc->rl_cmdstat = htole32(cmd);
			bus_dmamap_sync(sc->rl_ldata.rl_tx_mtag,
				txd[l].tx_dmamap, BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		sc->rl_ldata.rl_tx_prodidx = l;
		kring->nr_hwcur = k; /* the saved ring->cur */
		ring->avail -= n; // XXX see others
		kring->nr_hwavail = ring->avail;

		bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
		    sc->rl_ldata.rl_tx_list_map,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

		/* start ? */
		CSR_WRITE_1(sc, sc->rl_txstart, RL_TXSTART_START);
	}
	if (do_lock)
		RL_UNLOCK(sc);
	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring.
 */
static int
re_netmap_rxsync(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	struct rl_softc *sc = ifp->if_softc;
	struct rl_rxdesc *rxd = sc->rl_ldata.rl_rx_desc;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, l, n, lim = kring->nkr_num_slots - 1;
	int force_update = do_lock || kring->nr_kflags & NKR_PENDINTR;
	u_int k = ring->cur, resvd = ring->reserved;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		RL_LOCK(sc);
	/* XXX check sync modes */
	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * Import newly received packets into the netmap ring.
	 * j is an index in the netmap ring, l in the NIC ring.
	 *
	 * The device uses all the buffers in the ring, so we need
	 * another termination condition in addition to RL_RDESC_STAT_OWN
	 * cleared (all buffers could have it cleared. The easiest one
	 * is to limit the amount of data reported up to 'lim'
	 */
	l = sc->rl_ldata.rl_rx_prodidx; /* next pkt to check */
	j = netmap_idx_n2k(kring, l); /* the kring index */
	if (netmap_no_pendintr || force_update) {
		uint16_t slot_flags = kring->nkr_slot_flags;

		for (n = kring->nr_hwavail; n < lim ; n++) {
			struct rl_desc *cur_rx = &sc->rl_ldata.rl_rx_list[l];
			uint32_t rxstat = le32toh(cur_rx->rl_cmdstat);
			uint32_t total_len;

			if ((rxstat & RL_RDESC_STAT_OWN) != 0)
				break;
			total_len = rxstat & sc->rl_rxlenmask;
			/* XXX subtract crc */
			total_len = (total_len < 4) ? 0 : total_len - 4;
			kring->ring->slot[j].len = total_len;
			kring->ring->slot[j].flags = slot_flags;
			/*  sync was in re_newbuf() */
			bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
			    rxd[l].rx_dmamap, BUS_DMASYNC_POSTREAD);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		if (n != kring->nr_hwavail) {
			sc->rl_ldata.rl_rx_prodidx = l;
			sc->rl_ifp->if_ipackets += n - kring->nr_hwavail;
			kring->nr_hwavail = n;
		}
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	/* skip past packets that userspace has released */
	j = kring->nr_hwcur;
	if (resvd > 0) {
		if (resvd + ring->avail >= lim + 1) {
			D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
			ring->reserved = resvd = 0; // XXX panic...
		}
		k = (k >= resvd) ? k - resvd : k + lim + 1 - resvd;
	}
	if (j != k) { /* userspace has released some packets. */
		l = netmap_idx_k2n(kring, j); /* the NIC index */
		for (n = 0; j != k; n++) {
			struct netmap_slot *slot = ring->slot + j;
			struct rl_desc *desc = &sc->rl_ldata.rl_rx_list[l];
			int cmd = NETMAP_BUF_SIZE | RL_RDESC_CMD_OWN;
			uint64_t paddr;
			void *addr = PNMB(slot, &paddr);

			if (addr == netmap_buffer_base) { /* bad buf */
				if (do_lock)
					RL_UNLOCK(sc);
				return netmap_ring_reinit(kring);
			}

			if (l == lim)	/* mark end of ring */
				cmd |= RL_RDESC_CMD_EOR;

			slot->flags &= ~NS_REPORT;
			if (slot->flags & NS_BUF_CHANGED) {
				netmap_reload_map(sc->rl_ldata.rl_rx_mtag,
					rxd[l].rx_dmamap, addr);
				desc->rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
				desc->rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
				slot->flags &= ~NS_BUF_CHANGED;
			}
			desc->rl_cmdstat = htole32(cmd);
			bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
				rxd[l].rx_dmamap, BUS_DMASYNC_PREREAD);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
		/* Flush the RX DMA ring */

		bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
		    sc->rl_ldata.rl_rx_list_map,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail - resvd;
	if (do_lock)
		RL_UNLOCK(sc);
	return 0;
}

/*
 * Additional routines to init the tx and rx rings.
 * In other drivers we do that inline in the main code.
 */
static void
re_netmap_tx_init(struct rl_softc *sc)
{
	struct rl_txdesc *txd;
	struct rl_desc *desc;
	int i, n;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_TX, 0, 0);

	/* slot is NULL if we are not in netmap mode */
	if (!slot)
		return;
	/* in netmap mode, overwrite addresses and maps */
	txd = sc->rl_ldata.rl_tx_desc;
	desc = sc->rl_ldata.rl_tx_list;
	n = sc->rl_ldata.rl_tx_desc_cnt;

	/* l points in the netmap ring, i points in the NIC ring */
	for (i = 0; i < n; i++) {
		uint64_t paddr;
		int l = netmap_idx_n2k(&na->tx_rings[0], i);
		void *addr = PNMB(slot + l, &paddr);

		desc[i].rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
		desc[i].rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
		netmap_load_map(sc->rl_ldata.rl_tx_mtag,
			txd[i].tx_dmamap, addr);
	}
}

static void
re_netmap_rx_init(struct rl_softc *sc)
{
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_RX, 0, 0);
	struct rl_desc *desc = sc->rl_ldata.rl_rx_list;
	uint32_t cmdstat;
	int i, n, max_avail;

	if (!slot)
		return;
	n = sc->rl_ldata.rl_rx_desc_cnt;
	/*
	 * Userspace owned hwavail packets before the reset,
	 * so the NIC that last hwavail descriptors of the ring
	 * are still owned by the driver (and keep one empty).
	 */
	max_avail = n - 1 - na->rx_rings[0].nr_hwavail;
	for (i = 0; i < n; i++) {
		void *addr;
		uint64_t paddr;
		int l = netmap_idx_n2k(&na->rx_rings[0], i);

		addr = PNMB(slot + l, &paddr);

		netmap_reload_map(sc->rl_ldata.rl_rx_mtag,
		    sc->rl_ldata.rl_rx_desc[i].rx_dmamap, addr);
		bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
		    sc->rl_ldata.rl_rx_desc[i].rx_dmamap, BUS_DMASYNC_PREREAD);
		desc[i].rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
		desc[i].rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
		cmdstat = NETMAP_BUF_SIZE;
		if (i == n - 1) /* mark the end of ring */
			cmdstat |= RL_RDESC_CMD_EOR;
		if (i < max_avail)
			cmdstat |= RL_RDESC_CMD_OWN;
		desc[i].rl_cmdstat = htole32(cmdstat);
	}
}


static void
re_netmap_attach(struct rl_softc *sc)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = sc->rl_ifp;
	na.separate_locks = 0;
	na.num_tx_desc = sc->rl_ldata.rl_tx_desc_cnt;
	na.num_rx_desc = sc->rl_ldata.rl_rx_desc_cnt;
	na.nm_txsync = re_netmap_txsync;
	na.nm_rxsync = re_netmap_rxsync;
	na.nm_lock = re_netmap_lock_wrapper;
	na.nm_register = re_netmap_reg;
	netmap_attach(&na, 1);
}
/* end of file */
