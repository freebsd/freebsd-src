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
 * $Id: ixgbe_netmap.h 9802 2011-12-02 18:42:37Z luigi $
 *
 * netmap modifications for ixgbe
 *
 * This file is meant to be a reference on how to implement
 * netmap support for a network driver.
 * This file contains code but only static or inline functions
 * that are used by a single driver. To avoid replication of
 * code we just #include it near the beginning of the
 * standard driver.
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
/*
 * Some drivers may need the following headers. Others
 * already include them by default

#include <vm/vm.h>
#include <vm/pmap.h>

 */

#include <dev/netmap/netmap_kern.h>

/*
 * prototypes for the new API calls that are used by the
 * *_netmap_attach() routine.
 */
static int	ixgbe_netmap_reg(struct ifnet *, int onoff);
static int	ixgbe_netmap_txsync(void *, u_int, int);
static int	ixgbe_netmap_rxsync(void *, u_int, int);
static void	ixgbe_netmap_lock_wrapper(void *, int, u_int);


/*
 * The attach routine, called near the end of ixgbe_attach(),
 * fills the parameters for netmap_attach() and calls it.
 * It cannot fail, in the worst case (such as no memory)
 * netmap mode will be disabled and the driver will only
 * operate in standard mode.
 */
static void
ixgbe_netmap_attach(struct adapter *adapter)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = adapter->ifp;
	na.separate_locks = 1;	/* this card has separate rx/tx locks */
	na.num_tx_desc = adapter->num_tx_desc;
	na.num_rx_desc = adapter->num_rx_desc;
	na.nm_txsync = ixgbe_netmap_txsync;
	na.nm_rxsync = ixgbe_netmap_rxsync;
	na.nm_lock = ixgbe_netmap_lock_wrapper;
	na.nm_register = ixgbe_netmap_reg;
	/*
	 * XXX where do we put this comment ?
	 * adapter->rx_mbuf_sz is set by SIOCSETMTU, but in netmap mode
	 * we allocate the buffers on the first register. So we must
	 * disallow a SIOCSETMTU when if_capenable & IFCAP_NETMAP is set.
	 */
	na.buff_size = NETMAP_BUF_SIZE;
	netmap_attach(&na, adapter->num_queues);
}	


/*
 * wrapper to export locks to the generic netmap code.
 */
static void
ixgbe_netmap_lock_wrapper(void *_a, int what, u_int queueid)
{
	struct adapter *adapter = _a;

	ASSERT(queueid < adapter->num_queues);
	switch (what) {
	case NETMAP_CORE_LOCK:
		IXGBE_CORE_LOCK(adapter);
		break;
	case NETMAP_CORE_UNLOCK:
		IXGBE_CORE_UNLOCK(adapter);
		break;
	case NETMAP_TX_LOCK:
		IXGBE_TX_LOCK(&adapter->tx_rings[queueid]);
		break;
	case NETMAP_TX_UNLOCK:
		IXGBE_TX_UNLOCK(&adapter->tx_rings[queueid]);
		break;
	case NETMAP_RX_LOCK:
		IXGBE_RX_LOCK(&adapter->rx_rings[queueid]);
		break;
	case NETMAP_RX_UNLOCK:
		IXGBE_RX_UNLOCK(&adapter->rx_rings[queueid]);
		break;
	}
}


/*
 * Netmap register/unregister. We are already under core lock.
 * Only called on the first register or the last unregister.
 */
static int
ixgbe_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (!na) /* probably, netmap_attach() failed */
		return EINVAL;

	ixgbe_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	if (onoff) { /* enable netmap mode */
		ifp->if_capenable |= IFCAP_NETMAP;

		/* save if_transmit and replace with our routine */
		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_start;

		/*
		 * reinitialize the adapter, now with netmap flag set,
		 * so the rings will be set accordingly.
		 */
		ixgbe_init_locked(adapter);
		if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) == 0) {
			error = ENOMEM;
			goto fail;
		}
	} else { /* reset normal mode (explicit request or netmap failed) */
fail:
		/* restore if_transmit */
		ifp->if_transmit = na->if_transmit;
		ifp->if_capenable &= ~IFCAP_NETMAP;
		/* initialize the card, this time in standard mode */
		ixgbe_init_locked(adapter);	/* also enables intr */
	}
	return (error);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 * This routine might be called frequently so it must be efficient.
 *
 * Userspace has filled tx slots up to ring->cur (excluded).
 * The last unused slot previously known to the kernel was kring->nkr_hwcur,
 * and the last interrupt reported kring->nr_hwavail slots available.
 *
 * This function runs under lock (acquired from the caller or internally).
 * It must first update ring->avail to what the kernel knows,
 * subtract the newly used slots (ring->cur - kring->nkr_hwcur)
 * from both avail and nr_hwavail, and set ring->nkr_hwcur = ring->cur
 * issuing a dmamap_sync on all slots.
 *
 * Since ring comes from userspace, its content must be read only once,
 * and validated before being used to update the kernel's structures.
 * (this is also true for every use of ring in the kernel).
 *
 * ring->avail is never used, only checked for bogus values.
 */
static int
ixgbe_netmap_txsync(void *a, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = a;
	struct tx_ring *txr = &adapter->tx_rings[ring_nr];
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n = 0, lim = kring->nkr_num_slots - 1;

	/*
	 * ixgbe can generate an interrupt on every tx packet, but it
	 * seems very expensive, so we interrupt once every half ring,
	 * or when requested with NS_REPORT
	 */
	int report_frequency = kring->nkr_num_slots >> 1;

	if (do_lock)
		IXGBE_TX_LOCK(txr);
	/* take a copy of ring->cur now, and never read it again */
	k = ring->cur;
	l = k - kring->nr_hwcur;
	if (l < 0)
		l += lim + 1;
	/* if cur is invalid reinitialize the ring. */
	if (k > lim || l > kring->nr_hwavail) {
		if (do_lock)
			IXGBE_TX_UNLOCK(txr);
		return netmap_ring_reinit(kring);
	}

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
			BUS_DMASYNC_POSTREAD);

	/*
	 * Process new packets to send. j is the current index in the
	 * netmap ring, l is the corresponding index in the NIC ring.
	 * The two numbers differ because upon a *_init() we reset
	 * the NIC ring but leave the netmap ring unchanged.
	 * For the transmit ring, we have
	 *
	 *		j = kring->nr_hwcur
	 *		l = IXGBE_TDT (not tracked in the driver)
	 * and
	 * 		j == (l + kring->nkr_hwofs) % ring_size
	 *
	 * In this driver kring->nkr_hwofs >= 0, but for other
	 * drivers it might be negative as well.
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* we have new packets to send */
		l = j - kring->nkr_hwofs;
		if (l < 0)	/* wraparound */
			l += lim + 1;

		while (j != k) {
			/*
			 * Collect per-slot info.
			 * Note that txbuf and curr are indexed by l.
			 *
			 * In this driver we collect the buffer address
			 * (using the NMB() macro) because we always
			 * need to rewrite it into the NIC ring.
			 * Many other drivers preserve the address, so
			 * we only need to access it if NS_BUF_CHANGED
			 * is set.
			 */
			struct netmap_slot *slot = &ring->slot[j];
			struct ixgbe_tx_buf *txbuf = &txr->tx_buffers[l];
			union ixgbe_adv_tx_desc *curr = &txr->tx_base[l];
			void *addr = NMB(slot);
			// XXX type for flags and len ?
			int flags = ((slot->flags & NS_REPORT) ||
				j == 0 || j == report_frequency) ?
					IXGBE_TXD_CMD_RS : 0;
			int len = slot->len;

			/*
			 * Quick check for valid addr and len.
			 * NMB() returns netmap_buffer_base for invalid
			 * buffer indexes (but the address is still a
			 * valid one to be used in a ring). slot->len is
			 * unsigned so no need to check for negative values.
			 */
			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
ring_reset:
				if (do_lock)
					IXGBE_TX_UNLOCK(txr);
				return netmap_ring_reinit(kring);
			}

			slot->flags &= ~NS_REPORT;
			/*
			 * Fill the slot in the NIC ring.
			 * In this driver we need to rewrite the buffer
			 * address in the NIC ring. Other drivers do not
			 * need this.
			 */
			curr->read.buffer_addr = htole64(vtophys(addr));
			curr->read.olinfo_status = 0;
			curr->read.cmd_type_len =
			    htole32(txr->txd_cmd | len |
				(IXGBE_ADVTXD_DTYP_DATA |
				    IXGBE_ADVTXD_DCMD_IFCS |
				    IXGBE_TXD_CMD_EOP | flags) );
			/* If the buffer has changed, unload and reload map
			 * (and possibly the physical address in the NIC
			 * slot, but we did it already).
			 */
			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, unload and reload map */
				netmap_reload_map(txr->txtag, txbuf->map,
					addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}

			/* make sure changes to the buffer are synced */
			bus_dmamap_sync(txr->txtag, txbuf->map,
				BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
			n++;
		}
		kring->nr_hwcur = k; /* the saved ring->cur */

		/* decrease avail by number of sent packets */
		kring->nr_hwavail -= n;

		/* synchronize the NIC ring */
		bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/* (re)start the transmitter up to slot l (excluded) */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDT(txr->me), l);
	}

	/*
	 * If no packets are sent, or there is no room in the tx ring,
	 * Check whether there are completed transmissions.
	 * Because this is expensive (we need a register etc.)
	 * we only do it if absolutely necessary, i.e. there is no room
	 * in the tx ring, or where were no completed transmissions
	 * (meaning that probably the caller really wanted to check
	 * for completed transmissions).
	 */
	if (n == 0 || kring->nr_hwavail < 1) {
		int delta;

		/*
		 * Record completed transmissions.
		 * We (re)use the driver's txr->next_to_clean to keep
		 * track of the most recently completed transmission.
		 *
		 * The datasheet discourages the use of TDH to find out the
		 * number of sent packets. We should rather check the DD
		 * status bit in a packet descriptor. However, we only set
		 * the "report status" bit for some descriptors (a kind of
		 * interrupt mitigation), so we can only check on those.
		 * For the time being we use TDH, as we do it infrequently
		 * enough not to pose performance problems.
		 */
		l = IXGBE_READ_REG(&adapter->hw, IXGBE_TDH(ring_nr));
		if (l >= kring->nkr_num_slots) { /* XXX can happen */
			D("TDH wrap %d", l);
			l -= kring->nkr_num_slots;
		}
		delta = l - txr->next_to_clean;
		if (delta) {
			/* some tx completed, increment avail */
			if (delta < 0)
				delta += kring->nkr_num_slots;
			txr->next_to_clean = l;
			kring->nr_hwavail += delta;
			if (kring->nr_hwavail > lim)
				goto ring_reset;
		}
	}
	/* update avail to what the kernel knows */
	ring->avail = kring->nr_hwavail;

	if (do_lock)
		IXGBE_TX_UNLOCK(txr);
	return 0;

}


/*
 * Reconcile kernel and user view of the receive ring.
 * Same as for the txsync, this routine must be efficient and
 * avoid races in accessing the shared regions.
 *
 * When called, userspace has read data from slots kring->nr_hwcur
 * up to ring->cur (excluded).
 *
 * The last interrupt reported kring->nr_hwavail slots available
 * after kring->nr_hwcur.
 * We must subtract the newly consumed slots (cur - nr_hwcur)
 * from nr_hwavail, make the descriptors available for the next reads,
 * and set kring->nr_hwcur = ring->cur and ring->avail = kring->nr_hwavail.
 */
static int
ixgbe_netmap_rxsync(void *a, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = a;
	struct rx_ring *rxr = &adapter->rx_rings[ring_nr];
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;	/* cache and check value, same as in txsync */
	n = k - kring->nr_hwcur;
	if (n < 0)
		n += lim + 1;
	if (k > lim || n > kring->nr_hwavail) /* userspace is cheating */
		return netmap_ring_reinit(kring);

	if (do_lock)
		IXGBE_RX_LOCK(rxr);
	if (n < 0)
		n += lim + 1;
	/* XXX check sync modes */
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * First part, import newly received packets into the netmap ring.
	 *
	 * j is the index of the next free slot in the netmap ring,
	 * and l is the index of the next received packet in the NIC ring,
	 * and they may differ in case if_init() has been called while
	 * in netmap mode. For the receive ring we have
	 *
	 *	j = (kring->nr_hwcur + kring->nr_hwavail) % ring_size
	 *	l = rxr->next_to_check;
	 * and
	 *	j == (l + kring->nkr_hwofs) % ring_size
	 *
	 * rxr->next_to_check is set to 0 on a ring reinit
	 */
	l = rxr->next_to_check;
	j = rxr->next_to_check + kring->nkr_hwofs;
	if (j > lim)
		j -= lim + 1;

	for (n = 0; ; n++) {
		union ixgbe_adv_rx_desc *curr = &rxr->rx_base[l];
		uint32_t staterr = le32toh(curr->wb.upper.status_error);

		if ((staterr & IXGBE_RXD_STAT_DD) == 0)
			break;
		ring->slot[j].len = le16toh(curr->wb.upper.length);
		bus_dmamap_sync(rxr->ptag,
			rxr->rx_buffers[l].pmap, BUS_DMASYNC_POSTREAD);
		j = (j == lim) ? 0 : j + 1;
		l = (l == lim) ? 0 : l + 1;
	}
	if (n) { /* update the state variables */
		rxr->next_to_check = l;
		kring->nr_hwavail += n;
	}

	/*
	 * Skip past packets that userspace has already processed
	 * (from kring->nr_hwcur to ring->cur excluded), and make
	 * the buffers available for reception.
	 * As usual j is the index in the netmap ring, l is the index
	 * in the NIC ring, and j == (l + kring->nkr_hwofs) % ring_size
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* userspace has read some packets. */
		n = 0;
		l = kring->nr_hwcur - kring->nkr_hwofs;
		if (l < 0)
			l += lim + 1;
		while (j != k) {
			/* collect per-slot info, with similar validations
			 * and flag handling as in the txsync code.
			 *
			 * NOTE curr and rxbuf are indexed by l.
			 * Also, this driver needs to update the physical				 * address in the NIC ring, but other drivers
			 * may not have this requirement.
			 */
			struct netmap_slot *slot = &ring->slot[j];
			union ixgbe_adv_rx_desc *curr = &rxr->rx_base[l];
			struct ixgbe_rx_buf *rxbuf = &rxr->rx_buffers[l];
			void *addr = NMB(slot);

			if (addr == netmap_buffer_base) /* bad buf */
				goto ring_reset;

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
		kring->nr_hwcur = k;
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/* IMPORTANT: we must leave one free slot in the ring,
		 * so move l back by one unit
		 */
		l = (l == 0) ? lim : l - 1;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDT(rxr->me), l);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail ;
	if (do_lock)
		IXGBE_RX_UNLOCK(rxr);
	return 0;

ring_reset:
	if (do_lock)
		IXGBE_RX_UNLOCK(rxr);
	return netmap_ring_reinit(kring);
}
/* end of file */
