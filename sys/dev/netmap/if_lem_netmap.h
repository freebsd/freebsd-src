/*
 * Copyright (C) 2011 Matteo Landi, Luigi Rizzo. All rights reserved.
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
 * netmap support for "lem"
 *
 * For details on netmap support please see ixgbe_netmap.h
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>


static void
lem_netmap_lock_wrapper(struct ifnet *ifp, int what, u_int ringid)
{
	struct adapter *adapter = ifp->if_softc;

	/* only one ring here so ignore the ringid */
	switch (what) {
	case NETMAP_CORE_LOCK:
		EM_CORE_LOCK(adapter);
		break;
	case NETMAP_CORE_UNLOCK:
		EM_CORE_UNLOCK(adapter);
		break;
	case NETMAP_TX_LOCK:
		EM_TX_LOCK(adapter);
		break;
	case NETMAP_TX_UNLOCK:
		EM_TX_UNLOCK(adapter);
		break;
	case NETMAP_RX_LOCK:
		EM_RX_LOCK(adapter);
		break;
	case NETMAP_RX_UNLOCK:
		EM_RX_UNLOCK(adapter);
		break;
	}
}


/*
 * Register/unregister
 */
static int
lem_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (na == NULL)
		return EINVAL;

	lem_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

#ifndef EM_LEGACY_IRQ // XXX do we need this ?
	taskqueue_block(adapter->tq);
	taskqueue_drain(adapter->tq, &adapter->rxtx_task);
	taskqueue_drain(adapter->tq, &adapter->link_task);
#endif /* !EM_LEGCY_IRQ */
	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_start;

		lem_init_locked(adapter);
		if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) == 0) {
			error = ENOMEM;
			goto fail;
		}
	} else {
fail:
		/* return to non-netmap mode */
		ifp->if_transmit = na->if_transmit;
		ifp->if_capenable &= ~IFCAP_NETMAP;
		lem_init_locked(adapter);	/* also enable intr */
	}

#ifndef EM_LEGACY_IRQ
	taskqueue_unblock(adapter->tq); // XXX do we need this ?
#endif /* !EM_LEGCY_IRQ */

	return (error);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 */
static int
lem_netmap_txsync(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	u_int j, k, l, n = 0, lim = kring->nkr_num_slots - 1;

	/* generate an interrupt approximately every half ring */
	int report_frequency = kring->nkr_num_slots >> 1;

	/* take a copy of ring->cur now, and never read it again */
	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		EM_TX_LOCK(adapter);
	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
			BUS_DMASYNC_POSTREAD);
	/*
	 * Process new packets to send. j is the current index in the
	 * netmap ring, l is the corresponding index in the NIC ring.
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* we have new packets to send */
		l = netmap_idx_k2n(kring, j);
		for (n = 0; j != k; n++) {
			/* slot is the current slot in the netmap ring */
			struct netmap_slot *slot = &ring->slot[j];
			/* curr is the current slot in the nic ring */
			struct e1000_tx_desc *curr = &adapter->tx_desc_base[l];
			struct em_buffer *txbuf = &adapter->tx_buffer_area[l];
			int flags = ((slot->flags & NS_REPORT) ||
				j == 0 || j == report_frequency) ?
					E1000_TXD_CMD_RS : 0;
			uint64_t paddr;
			void *addr = PNMB(slot, &paddr);
			u_int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				if (do_lock)
					EM_TX_UNLOCK(adapter);
				return netmap_ring_reinit(kring);
			}

			slot->flags &= ~NS_REPORT;
			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(adapter->txtag, txbuf->map, addr);
				curr->buffer_addr = htole64(paddr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			curr->upper.data = 0;
			curr->lower.data =
			    htole32( adapter->txd_cmd | len |
				(E1000_TXD_CMD_EOP | flags) );

			bus_dmamap_sync(adapter->txtag, txbuf->map,
			    BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		kring->nr_hwcur = k; /* the saved ring->cur */
		kring->nr_hwavail -= n;

		bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), l);
	}

	if (n == 0 || kring->nr_hwavail < 1) {
		int delta;

		/* record completed transmissions using TDH */
		l = E1000_READ_REG(&adapter->hw, E1000_TDH(0));
		if (l >= kring->nkr_num_slots) { /* XXX can it happen ? */
			D("bad TDH %d", l);
			l -= kring->nkr_num_slots;
		}
		delta = l - adapter->next_tx_to_clean;
		if (delta) {
			/* some tx completed, increment hwavail. */
			if (delta < 0)
				delta += kring->nkr_num_slots;
			adapter->next_tx_to_clean = l;
			kring->nr_hwavail += delta;
		}
	}
	/* update avail to what the kernel knows */
	ring->avail = kring->nr_hwavail;

	if (do_lock)
		EM_TX_UNLOCK(adapter);
	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring.
 */
static int
lem_netmap_rxsync(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, l, n, lim = kring->nkr_num_slots - 1;
	int force_update = do_lock || kring->nr_kflags & NKR_PENDINTR;
	u_int k = ring->cur, resvd = ring->reserved;

	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		EM_RX_LOCK(adapter);

	/* XXX check sync modes */
	bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * Import newly received packets into the netmap ring.
	 * j is an index in the netmap ring, l in the NIC ring.
	 */
	l = adapter->next_rx_desc_to_check;
	j = netmap_idx_n2k(kring, l);
	if (netmap_no_pendintr || force_update) {
		uint16_t slot_flags = kring->nkr_slot_flags;

		for (n = 0; ; n++) {
			struct e1000_rx_desc *curr = &adapter->rx_desc_base[l];
			uint32_t staterr = le32toh(curr->status);
			int len;

			if ((staterr & E1000_RXD_STAT_DD) == 0)
				break;
			len = le16toh(curr->length) - 4; // CRC
			if (len < 0) {
				D("bogus pkt size at %d", j);
				len = 0;
			}
			ring->slot[j].len = len;
			ring->slot[j].flags = slot_flags;
			bus_dmamap_sync(adapter->rxtag,
				adapter->rx_buffer_area[l].map,
				    BUS_DMASYNC_POSTREAD);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		if (n) { /* update the state variables */
			adapter->next_rx_desc_to_check = l;
			kring->nr_hwavail += n;
		}
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	/* skip past packets that userspace has released */
	j = kring->nr_hwcur;	/* netmap ring index */
	if (resvd > 0) {
		if (resvd + ring->avail >= lim + 1) {
			D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
			ring->reserved = resvd = 0; // XXX panic...
		}
		k = (k >= resvd) ? k - resvd : k + lim + 1 - resvd;
	}
	if (j != k) { /* userspace has released some packets. */
		l = netmap_idx_k2n(kring, j); /* NIC ring index */
		for (n = 0; j != k; n++) {
			struct netmap_slot *slot = &ring->slot[j];
			struct e1000_rx_desc *curr = &adapter->rx_desc_base[l];
			struct em_buffer *rxbuf = &adapter->rx_buffer_area[l];
			uint64_t paddr;
			void *addr = PNMB(slot, &paddr);

			if (addr == netmap_buffer_base) { /* bad buf */
				if (do_lock)
					EM_RX_UNLOCK(adapter);
				return netmap_ring_reinit(kring);
			}

			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(adapter->rxtag, rxbuf->map, addr);
				curr->buffer_addr = htole64(paddr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			curr->status = 0;

			bus_dmamap_sync(adapter->rxtag, rxbuf->map,
			    BUS_DMASYNC_PREREAD);

			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
		bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/*
		 * IMPORTANT: we must leave one free slot in the ring,
		 * so move l back by one unit
		 */
		l = (l == 0) ? lim : l - 1;
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), l);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail - resvd;
	if (do_lock)
		EM_RX_UNLOCK(adapter);
	return 0;
}


static void
lem_netmap_attach(struct adapter *adapter)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = adapter->ifp;
	na.separate_locks = 1;
	na.num_tx_desc = adapter->num_tx_desc;
	na.num_rx_desc = adapter->num_rx_desc;
	na.nm_txsync = lem_netmap_txsync;
	na.nm_rxsync = lem_netmap_rxsync;
	na.nm_lock = lem_netmap_lock_wrapper;
	na.nm_register = lem_netmap_reg;
	netmap_attach(&na, 1);
}

/* end of file */
