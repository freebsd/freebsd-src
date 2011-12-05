/*
 * Copyright (C) 2011 Universita` di Pisa. All rights reserved.
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
 * $Id: if_igb_netmap.h 9802 2011-12-02 18:42:37Z luigi $
 *
 * netmap modifications for igb
 * contribured by Ahmed Kooli
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>

static int	igb_netmap_reg(struct ifnet *, int onoff);
static int	igb_netmap_txsync(void *, u_int, int);
static int	igb_netmap_rxsync(void *, u_int, int);
static void	igb_netmap_lock_wrapper(void *, int, u_int);


static void
igb_netmap_attach(struct adapter *adapter)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = adapter->ifp;
	na.separate_locks = 1;
	na.num_tx_desc = adapter->num_tx_desc;
	na.num_rx_desc = adapter->num_rx_desc;
	na.nm_txsync = igb_netmap_txsync;
	na.nm_rxsync = igb_netmap_rxsync;
	na.nm_lock = igb_netmap_lock_wrapper;
	na.nm_register = igb_netmap_reg;
	na.buff_size = NETMAP_BUF_SIZE;
	netmap_attach(&na, adapter->num_queues);
}	


/*
 * wrapper to export locks to the generic code
 */
static void
igb_netmap_lock_wrapper(void *_a, int what, u_int queueid)
{
	struct adapter *adapter = _a;

	ASSERT(queueid < adapter->num_queues);
	switch (what) {
	case NETMAP_CORE_LOCK:
		IGB_CORE_LOCK(adapter);
		break;
	case NETMAP_CORE_UNLOCK:
		IGB_CORE_UNLOCK(adapter);
		break;
	case NETMAP_TX_LOCK:
		IGB_TX_LOCK(&adapter->tx_rings[queueid]);
		break;
	case NETMAP_TX_UNLOCK:
		IGB_TX_UNLOCK(&adapter->tx_rings[queueid]);
		break;
	case NETMAP_RX_LOCK:
		IGB_RX_LOCK(&adapter->rx_rings[queueid]);
		break;
	case NETMAP_RX_UNLOCK:
		IGB_RX_UNLOCK(&adapter->rx_rings[queueid]);
		break;
	}
}


/*
 * support for netmap register/unregisted. We are already under core lock.
 * only called on the first init or the last unregister.
 */
static int
igb_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (na == NULL)
		return EINVAL;

	igb_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		/* save if_transmit to restore it later */
		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_start;

		igb_init_locked(adapter);
		if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) == 0) {
			error = ENOMEM;
			goto fail;
		}
	} else {
fail:
		/* restore if_transmit */
		ifp->if_transmit = na->if_transmit;
		ifp->if_capenable &= ~IFCAP_NETMAP;
		igb_init_locked(adapter);	/* also enables intr */
	}
	return (error);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 */
static int
igb_netmap_txsync(void *a, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = a;
	struct tx_ring *txr = &adapter->tx_rings[ring_nr];
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n = 0, lim = kring->nkr_num_slots - 1;

	/* generate an interrupt approximately every half ring */
	int report_frequency = kring->nkr_num_slots >> 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		IGB_TX_LOCK(txr);
	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

	/* update avail to what the hardware knows */
	ring->avail = kring->nr_hwavail;

	j = kring->nr_hwcur; /* netmap ring index */
	if (j != k) {	/* we have new packets to send */
		u32 olinfo_status = 0;
		int n = 0;

		l = j - kring->nkr_hwofs; /* NIC ring index */
		if (l < 0)
			l += lim + 1;
		/* 82575 needs the queue index added */
		if (adapter->hw.mac.type == e1000_82575)
			olinfo_status |= txr->me << 4;

		while (j != k) {
			struct netmap_slot *slot = &ring->slot[j];
			struct igb_tx_buffer *txbuf = &txr->tx_buffers[l];
			union e1000_adv_tx_desc *curr =
			    (union e1000_adv_tx_desc *)&txr->tx_base[l];
			void *addr = NMB(slot);
			int flags = ((slot->flags & NS_REPORT) ||
				j == 0 || j == report_frequency) ?
					E1000_ADVTXD_DCMD_RS : 0;
			int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				if (do_lock)
					IGB_TX_UNLOCK(txr);
				return netmap_ring_reinit(kring);
			}

			slot->flags &= ~NS_REPORT;
			// XXX do we need to set the address ?
			curr->read.buffer_addr = htole64(vtophys(addr));
			curr->read.olinfo_status =
			    htole32(olinfo_status |
				(len<< E1000_ADVTXD_PAYLEN_SHIFT));
			curr->read.cmd_type_len =
			    htole32(len | E1000_ADVTXD_DTYP_DATA |
				    E1000_ADVTXD_DCMD_IFCS |
				    E1000_ADVTXD_DCMD_DEXT |
				    E1000_ADVTXD_DCMD_EOP | flags);
			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(txr->txtag, txbuf->map,
					addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}

			bus_dmamap_sync(txr->txtag, txbuf->map,
				BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
			n++;
		}
		kring->nr_hwcur = k;

		/* decrease avail by number of sent packets */
		kring->nr_hwavail -= n;
		ring->avail = kring->nr_hwavail;

		/* Set the watchdog XXX ? */
		txr->queue_status = IGB_QUEUE_WORKING;
		txr->watchdog_time = ticks;

		bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		E1000_WRITE_REG(&adapter->hw, E1000_TDT(txr->me), l);
	}
	if (n == 0 || kring->nr_hwavail < 1) {
		int delta;

		/* record completed transmission using TDH */
		l = E1000_READ_REG(&adapter->hw, E1000_TDH(ring_nr));
		if (l >= kring->nkr_num_slots) /* XXX can it happen ? */
			l -= kring->nkr_num_slots;
		delta = l - txr->next_to_clean;
		if (delta) {
			/* new tx were completed */
			if (delta < 0)
				delta += kring->nkr_num_slots;
			txr->next_to_clean = l;
			kring->nr_hwavail += delta;
			ring->avail = kring->nr_hwavail;
		}
	}
	if (do_lock)
		IGB_TX_UNLOCK(txr);
	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring.
 */
static int
igb_netmap_rxsync(void *a, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = a;
	struct rx_ring *rxr = &adapter->rx_rings[ring_nr];
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		IGB_RX_LOCK(rxr);

	/* Sync the ring. */
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	l = rxr->next_to_check;
	j = l + kring->nkr_hwofs;
	if (j > lim)
		j -= lim + 1;
	for (n = 0; ; n++) {
		union e1000_adv_rx_desc *curr = &rxr->rx_base[l];
		uint32_t staterr = le32toh(curr->wb.upper.status_error);

		if ((staterr & E1000_RXD_STAT_DD) == 0)
			break;
		ring->slot[j].len = le16toh(curr->wb.upper.length);
		
		bus_dmamap_sync(rxr->ptag,
			rxr->rx_buffers[l].pmap, BUS_DMASYNC_POSTREAD);
		j = (j == lim) ? 0 : j + 1;
		l = (l == lim) ? 0 : l + 1;
	}
	if (n) {
		rxr->next_to_check = l;
		kring->nr_hwavail += n;
	}

	/* skip past packets that userspace has already processed,
	 * making them available for reception.
	 * advance nr_hwcur and issue a bus_dmamap_sync on the
	 * buffers so it is safe to write to them.
	 * Also increase nr_hwavail
	 */
	j = kring->nr_hwcur;
	l = kring->nr_hwcur - kring->nkr_hwofs;
	if (l < 0)
		l += lim + 1;
	if (j != k) {	/* userspace has read some packets. */
		n = 0;
		while (j != k) {
			struct netmap_slot *slot = ring->slot + j;
			union e1000_adv_rx_desc *curr = &rxr->rx_base[l];
			struct igb_rx_buf *rxbuf = rxr->rx_buffers + l;
			void *addr = NMB(slot);

			if (addr == netmap_buffer_base) { /* bad buf */
				if (do_lock)
					IGB_RX_UNLOCK(rxr);
				return netmap_ring_reinit(kring);
			}

			curr->wb.upper.status_error = 0;
			curr->read.pkt_addr = htole64(vtophys(addr));
			if (slot->flags & NS_BUF_CHANGED) {
				netmap_reload_map(rxr->ptag, rxbuf->pmap,
					addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}

			bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
				BUS_DMASYNC_PREREAD);

			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
			n++;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = ring->cur;
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/* IMPORTANT: we must leave one free slot in the ring,
		 * so move l back by one unit
		 */
		l = (l == 0) ? lim : l - 1;
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(rxr->me), l);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail ;
	if (do_lock)
		IGB_RX_UNLOCK(rxr);
	return 0;
}
