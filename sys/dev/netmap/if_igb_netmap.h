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
 *
 * Netmap support for igb, partly contributed by Ahmed Kooli
 * For details on netmap support please see ixgbe_netmap.h
 */


#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>


/*
 * register-unregister routine
 */
static int
igb_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (na == NULL)
		return EINVAL;	/* no netmap support here */

	igb_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_transmit;

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
		igb_init_locked(adapter);	/* also enable intr */
	}
	return (error);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 */
static int
igb_netmap_txsync(struct ifnet *ifp, u_int ring_nr, int flags)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring *txr = &adapter->tx_rings[ring_nr];
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	u_int j, k, l, n = 0, lim = kring->nkr_num_slots - 1;

	/* generate an interrupt approximately every half ring */
	u_int report_frequency = kring->nkr_num_slots >> 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

	/* check for new packets to send.
	 * j indexes the netmap ring, l indexes the nic ring, and
	 *      j = kring->nr_hwcur, l = E1000_TDT (not tracked),
	 *      j == (l + kring->nkr_hwofs) % ring_size
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* we have new packets to send */
		/* 82575 needs the queue index added */
		u32 olinfo_status =
		    (adapter->hw.mac.type == e1000_82575) ? (txr->me << 4) : 0;

		l = netmap_idx_k2n(kring, j);
		for (n = 0; j != k; n++) {
			/* slot is the current slot in the netmap ring */
			struct netmap_slot *slot = &ring->slot[j];
			/* curr is the current slot in the nic ring */
			union e1000_adv_tx_desc *curr =
			    (union e1000_adv_tx_desc *)&txr->tx_base[l];
#ifndef IGB_MEDIA_RESET
/* at the same time as IGB_MEDIA_RESET was defined, the
 * tx buffer descriptor was renamed, so use this to revert
 * back to the old name.
 */
#define igb_tx_buf igb_tx_buffer
#endif
			struct igb_tx_buf *txbuf = &txr->tx_buffers[l];
			int flags = ((slot->flags & NS_REPORT) ||
				j == 0 || j == report_frequency) ?
					E1000_ADVTXD_DCMD_RS : 0;
			uint64_t paddr;
			void *addr = PNMB(slot, &paddr);
			u_int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				return netmap_ring_reinit(kring);
			}

			slot->flags &= ~NS_REPORT;
			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(txr->txtag, txbuf->map, addr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			curr->read.buffer_addr = htole64(paddr);
			// XXX check olinfo and cmd_type_len
			curr->read.olinfo_status =
			    htole32(olinfo_status |
				(len<< E1000_ADVTXD_PAYLEN_SHIFT));
			curr->read.cmd_type_len =
			    htole32(len | E1000_ADVTXD_DTYP_DATA |
				    E1000_ADVTXD_DCMD_IFCS |
				    E1000_ADVTXD_DCMD_DEXT |
				    E1000_ADVTXD_DCMD_EOP | flags);

			bus_dmamap_sync(txr->txtag, txbuf->map,
				BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		kring->nr_hwcur = k; /* the saved ring->cur */
		kring->nr_hwavail -= n;

		/* Set the watchdog XXX ? */
		txr->queue_status = IGB_QUEUE_WORKING;
		txr->watchdog_time = ticks;

		bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		E1000_WRITE_REG(&adapter->hw, E1000_TDT(txr->me), l);
	}

	if (n == 0 || kring->nr_hwavail < 1) {
		int delta;

		/* record completed transmissions using TDH */
		l = E1000_READ_REG(&adapter->hw, E1000_TDH(ring_nr));
		if (l >= kring->nkr_num_slots) { /* XXX can it happen ? */
			D("TDH wrap %d", l);
			l -= kring->nkr_num_slots;
		}
		delta = l - txr->next_to_clean;
		if (delta) {
			/* some completed, increment hwavail. */
			if (delta < 0)
				delta += kring->nkr_num_slots;
			txr->next_to_clean = l;
			kring->nr_hwavail += delta;
		}
	}
	/* update avail to what the kernel knows */
	ring->avail = kring->nr_hwavail;

	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring.
 */
static int
igb_netmap_rxsync(struct ifnet *ifp, u_int ring_nr, int flags)
{
	struct adapter *adapter = ifp->if_softc;
	struct rx_ring *rxr = &adapter->rx_rings[ring_nr];
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	u_int j, l, n, lim = kring->nkr_num_slots - 1;
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;
	u_int k = ring->cur, resvd = ring->reserved;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	/* XXX check sync modes */
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * import newly received packets into the netmap ring.
	 * j is an index in the netmap ring, l in the NIC ring.
	 */
	l = rxr->next_to_check;
	j = netmap_idx_n2k(kring, l);
	if (netmap_no_pendintr || force_update) {
		uint16_t slot_flags = kring->nkr_slot_flags;

		for (n = 0; ; n++) {
			union e1000_adv_rx_desc *curr = &rxr->rx_base[l];
			uint32_t staterr = le32toh(curr->wb.upper.status_error);

			if ((staterr & E1000_RXD_STAT_DD) == 0)
				break;
			ring->slot[j].len = le16toh(curr->wb.upper.length);
			ring->slot[j].flags = slot_flags;
			bus_dmamap_sync(rxr->ptag,
				rxr->rx_buffers[l].pmap, BUS_DMASYNC_POSTREAD);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		if (n) { /* update the state variables */
			rxr->next_to_check = l;
			kring->nr_hwavail += n;
		}
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	/* skip past packets that userspace has released */
        j = kring->nr_hwcur;    /* netmap ring index */
	if (resvd > 0) {
		if (resvd + ring->avail >= lim + 1) {
			D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
			ring->reserved = resvd = 0; // XXX panic...
		}
		k = (k >= resvd) ? k - resvd : k + lim + 1 - resvd;
	}
	if (j != k) { /* userspace has released some packets. */
		l = netmap_idx_k2n(kring, j);
		for (n = 0; j != k; n++) {
			struct netmap_slot *slot = ring->slot + j;
			union e1000_adv_rx_desc *curr = &rxr->rx_base[l];
			struct igb_rx_buf *rxbuf = rxr->rx_buffers + l;
			uint64_t paddr;
			void *addr = PNMB(slot, &paddr);

			if (addr == netmap_buffer_base) { /* bad buf */
				return netmap_ring_reinit(kring);
			}

			if (slot->flags & NS_BUF_CHANGED) {
				netmap_reload_map(rxr->ptag, rxbuf->pmap, addr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			curr->read.pkt_addr = htole64(paddr);
			curr->wb.upper.status_error = 0;
			bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
				BUS_DMASYNC_PREREAD);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/*
		 * IMPORTANT: we must leave one free slot in the ring,
		 * so move l back by one unit
		 */
		l = (l == 0) ? lim : l - 1;
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(rxr->me), l);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail - resvd;
	return 0;
}


static void
igb_netmap_attach(struct adapter *adapter)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = adapter->ifp;
	na.na_flags = NAF_BDG_MAYSLEEP;
	na.num_tx_desc = adapter->num_tx_desc;
	na.num_rx_desc = adapter->num_rx_desc;
	na.nm_txsync = igb_netmap_txsync;
	na.nm_rxsync = igb_netmap_rxsync;
	na.nm_register = igb_netmap_reg;
	netmap_attach(&na, adapter->num_queues);
}	
/* end of file */
