/*
 * Copyright (C) 2011-2014 Matteo Landi, Luigi Rizzo. All rights reserved.
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
 * netmap support for: em.
 *
 * For more details on netmap support please see ixgbe_netmap.h
 */


#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>


// XXX do we need to block/unblock the tasks ?
static void
em_netmap_block_tasks(struct adapter *adapter)
{
	if (adapter->msix > 1) { /* MSIX */
		int i;
		struct tx_ring *txr = adapter->tx_rings;
		struct rx_ring *rxr = adapter->rx_rings;

		for (i = 0; i < adapter->num_queues; i++, txr++, rxr++) {
			taskqueue_block(txr->tq);
			taskqueue_drain(txr->tq, &txr->tx_task);
			taskqueue_block(rxr->tq);
			taskqueue_drain(rxr->tq, &rxr->rx_task);
		}
	} else {	/* legacy */
		taskqueue_block(adapter->tq);
		taskqueue_drain(adapter->tq, &adapter->link_task);
		taskqueue_drain(adapter->tq, &adapter->que_task);
	}
}


static void
em_netmap_unblock_tasks(struct adapter *adapter)
{
	if (adapter->msix > 1) {
		struct tx_ring *txr = adapter->tx_rings;
		struct rx_ring *rxr = adapter->rx_rings;
		int i;

		for (i = 0; i < adapter->num_queues; i++, txr++, rxr++) {
			taskqueue_unblock(txr->tq);
			taskqueue_unblock(rxr->tq);
		}
	} else { /* legacy */
		taskqueue_unblock(adapter->tq);
	}
}


/*
 * Register/unregister. We are already under netmap lock.
 */
static int
em_netmap_reg(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	struct adapter *adapter = ifp->if_softc;

	EM_CORE_LOCK(adapter);
	em_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	em_netmap_block_tasks(adapter);
	/* enable or disable flags and callbacks in na and ifp */
	if (onoff) {
		nm_set_native_flags(na);
	} else {
		nm_clear_native_flags(na);
	}
	em_init_locked(adapter);	/* also enable intr */
	em_netmap_unblock_tasks(adapter);
	EM_CORE_UNLOCK(adapter);
	return (ifp->if_drv_flags & IFF_DRV_RUNNING ? 0 : 1);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 */
static int
em_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;	/* index into the netmap ring */
	u_int nic_i;	/* index into the NIC ring */
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	/* generate an interrupt approximately every half ring */
	u_int report_frequency = kring->nkr_num_slots >> 1;

	/* device-specific */
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring *txr = &adapter->tx_rings[kring->ring_id];

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
			BUS_DMASYNC_POSTREAD);

	/*
	 * First part: process new packets to send.
	 */

	nm_i = kring->nr_hwcur;
	if (nm_i != head) {	/* we have new packets to send */
		nic_i = netmap_idx_k2n(kring, nm_i);
		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			u_int len = slot->len;
			uint64_t paddr;
			void *addr = PNMB(na, slot, &paddr);

			/* device-specific */
			struct e1000_tx_desc *curr = &txr->tx_base[nic_i];
			struct em_txbuffer *txbuf = &txr->tx_buffers[nic_i];
			int flags = (slot->flags & NS_REPORT ||
				nic_i == 0 || nic_i == report_frequency) ?
				E1000_TXD_CMD_RS : 0;

			NM_CHECK_ADDR_LEN(na, addr, len);

			if (slot->flags & NS_BUF_CHANGED) {
				curr->buffer_addr = htole64(paddr);
				/* buffer has changed, reload map */
				netmap_reload_map(na, txr->txtag, txbuf->map, addr);
			}
			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);

			/* Fill the slot in the NIC ring. */
			curr->upper.data = 0;
			curr->lower.data = htole32(adapter->txd_cmd | len |
				(E1000_TXD_CMD_EOP | flags) );
			bus_dmamap_sync(txr->txtag, txbuf->map,
				BUS_DMASYNC_PREWRITE);

			nm_i = nm_next(nm_i, lim);
			nic_i = nm_next(nic_i, lim);
		}
		kring->nr_hwcur = head;

		/* synchronize the NIC ring */
		bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* (re)start the tx unit up to slot nic_i (excluded) */
		E1000_WRITE_REG(&adapter->hw, E1000_TDT(txr->me), nic_i);
	}

	/*
	 * Second part: reclaim buffers for completed transmissions.
	 */
	if (flags & NAF_FORCE_RECLAIM || nm_kr_txempty(kring)) {
		/* record completed transmissions using TDH */
		nic_i = E1000_READ_REG(&adapter->hw, E1000_TDH(kring->ring_id));
		if (nic_i >= kring->nkr_num_slots) { /* XXX can it happen ? */
			D("TDH wrap %d", nic_i);
			nic_i -= kring->nkr_num_slots;
		}
		if (nic_i != txr->next_to_clean) {
			txr->next_to_clean = nic_i;
			kring->nr_hwtail = nm_prev(netmap_idx_n2k(kring, nic_i), lim);
		}
	}

	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring.
 */
static int
em_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;	/* index into the netmap ring */
	u_int nic_i;	/* index into the NIC ring */
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;

	/* device-specific */
	struct adapter *adapter = ifp->if_softc;
	struct rx_ring *rxr = &adapter->rx_rings[kring->ring_id];

	if (head > lim)
		return netmap_ring_reinit(kring);

	/* XXX check sync modes */
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * First part: import newly received packets.
	 */
	if (netmap_no_pendintr || force_update) {
		uint16_t slot_flags = kring->nkr_slot_flags;

		nic_i = rxr->next_to_check;
		nm_i = netmap_idx_n2k(kring, nic_i);

		for (n = 0; ; n++) { // XXX no need to count
			union e1000_rx_desc_extended *curr = &rxr->rx_base[nic_i];
			uint32_t staterr = le32toh(curr->wb.upper.status_error);

			if ((staterr & E1000_RXD_STAT_DD) == 0)
				break;
			ring->slot[nm_i].len = le16toh(curr->wb.upper.length);
			ring->slot[nm_i].flags = slot_flags;
			bus_dmamap_sync(rxr->rxtag, rxr->rx_buffers[nic_i].map,
				BUS_DMASYNC_POSTREAD);
			nm_i = nm_next(nm_i, lim);
			/* make sure next_to_refresh follows next_to_check */
			rxr->next_to_refresh = nic_i;	// XXX
			nic_i = nm_next(nic_i, lim);
		}
		if (n) { /* update the state variables */
			rxr->next_to_check = nic_i;
			kring->nr_hwtail = nm_i;
		}
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	/*
	 * Second part: skip past packets that userspace has released.
	 */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {
		nic_i = netmap_idx_k2n(kring, nm_i);
		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			uint64_t paddr;
			void *addr = PNMB(na, slot, &paddr);

			union e1000_rx_desc_extended *curr = &rxr->rx_base[nic_i];
			struct em_rxbuffer *rxbuf = &rxr->rx_buffers[nic_i];

			if (addr == NETMAP_BUF_BASE(na)) /* bad buf */
				goto ring_reset;

			curr->read.buffer_addr = htole64(paddr);
			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(na, rxr->rxtag, rxbuf->map, addr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			curr->wb.upper.status_error = 0;
			bus_dmamap_sync(rxr->rxtag, rxbuf->map,
			    BUS_DMASYNC_PREREAD);
			nm_i = nm_next(nm_i, lim);
			nic_i = nm_next(nic_i, lim);
		}
		kring->nr_hwcur = head;

		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/*
		 * IMPORTANT: we must leave one free slot in the ring,
		 * so move nic_i back by one unit
		 */
		nic_i = nm_prev(nic_i, lim);
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(rxr->me), nic_i);
	}

	return 0;

ring_reset:
	return netmap_ring_reinit(kring);
}


static void
em_netmap_attach(struct adapter *adapter)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = adapter->ifp;
	na.na_flags = NAF_BDG_MAYSLEEP;
	na.num_tx_desc = adapter->num_tx_desc;
	na.num_rx_desc = adapter->num_rx_desc;
	na.nm_txsync = em_netmap_txsync;
	na.nm_rxsync = em_netmap_rxsync;
	na.nm_register = em_netmap_reg;
	na.num_tx_rings = na.num_rx_rings = adapter->num_queues;
	netmap_attach(&na);
}

/* end of file */
