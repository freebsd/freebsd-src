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
 * $Id: if_lem_netmap.h 9662 2011-11-16 13:18:06Z luigi $
 *
 * netmap support for if_lem.c
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>

static int	lem_netmap_reg(struct ifnet *, int onoff);
static int	lem_netmap_txsync(void *, u_int, int);
static int	lem_netmap_rxsync(void *, u_int, int);
static void	lem_netmap_lock_wrapper(void *, int, u_int);


SYSCTL_NODE(_dev, OID_AUTO, lem, CTLFLAG_RW, 0, "lem card");

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
	na.buff_size = MCLBYTES;
	netmap_attach(&na, 1);
}


static void
lem_netmap_lock_wrapper(void *_a, int what, u_int ringid)
{
	struct adapter *adapter = _a;

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
 * Reconcile kernel and user view of the transmit ring. see ixgbe.c
 */
static int
lem_netmap_txsync(void *a, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = a;
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring = &na->tx_rings[0];
	struct netmap_ring *ring = kring->ring;
	int j, k, n, lim = kring->nkr_num_slots - 1;

	/* generate an interrupt approximately every half ring */
	int report_frequency = kring->nkr_num_slots >> 1;

	k = ring->cur;
	if ( (kring->nr_kflags & NR_REINIT) || k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		EM_TX_LOCK(adapter);
	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
			BUS_DMASYNC_POSTREAD);

	/* record completed transmissions TODO
	 *
	 * instead of using TDH, we could read the transmitted status bit.
	 */
	j = E1000_READ_REG(&adapter->hw, E1000_TDH(0));
	if (j >= kring->nkr_num_slots) { /* can it happen ? */
		D("bad TDH %d", j);
		j -= kring->nkr_num_slots;
	}
	int delta = j - adapter->next_tx_to_clean;
	if (delta) {
		if (delta < 0)
			delta += kring->nkr_num_slots;
		adapter->next_tx_to_clean = j;
		kring->nr_hwavail += delta;
	}

	/* update avail to what the hardware knows */
	ring->avail = kring->nr_hwavail;

	j = kring->nr_hwcur;
	if (j != k) {	/* we have new packets to send */
		n = 0;
		while (j != k) {
			struct netmap_slot *slot = &ring->slot[j];
			struct e1000_tx_desc *curr = &adapter->tx_desc_base[j];
			struct em_buffer *txbuf = &adapter->tx_buffer_area[j];
			void *addr = NMB(slot);
			int flags = ((slot->flags & NS_REPORT) ||
				j == 0 || j == report_frequency) ?
					E1000_TXD_CMD_RS : 0;
			int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				if (do_lock)
					EM_TX_UNLOCK(adapter);
				return netmap_ring_reinit(kring);
			}

			curr->upper.data = 0;
			/* always interrupt. XXX make it conditional */
			curr->lower.data =
			    htole32( adapter->txd_cmd | len |
				(E1000_TXD_CMD_EOP | flags) );
			if (slot->flags & NS_BUF_CHANGED) {
				curr->buffer_addr = htole64(vtophys(addr));
				/* buffer has changed, unload and reload map */
				netmap_reload_map(adapter->txtag, txbuf->map,
					addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}

			bus_dmamap_sync(adapter->txtag, txbuf->map,
				BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			n++;
		}
		kring->nr_hwcur = ring->cur;

		/* decrease avail by number of sent packets */
		ring->avail -= n;
		kring->nr_hwavail = ring->avail;

		bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), ring->cur);
	}
	if (do_lock)
		EM_TX_UNLOCK(adapter);
	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring. see ixgbe.c
 */
static int
lem_netmap_rxsync(void *a, u_int ring_nr, int do_lock)
{
	struct adapter *adapter = a;
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring = &na->rx_rings[0];
	struct netmap_ring *ring = kring->ring;
	int j, k, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if ( (kring->nr_kflags & NR_REINIT) || k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		EM_RX_LOCK(adapter);
	/* XXX check sync modes */
	bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* acknowldge all the received packets. */
	j = adapter->next_rx_desc_to_check;
	for (n = 0; ; n++) {
		struct e1000_rx_desc *curr = &adapter->rx_desc_base[j];
		int len = le16toh(adapter->rx_desc_base[j].length) - 4; // CRC

		if ((curr->status & E1000_RXD_STAT_DD) == 0)
			break;

		if (len < 0) {
			D("bogus pkt size at %d", j);
			len = 0;
		}
		ring->slot[j].len = len;
		bus_dmamap_sync(adapter->rxtag, adapter->rx_buffer_area[j].map,
			BUS_DMASYNC_POSTREAD);
		j = (j == lim) ? 0 : j + 1;
	}
	if (n) {
		adapter->next_rx_desc_to_check = j;
		kring->nr_hwavail += n;
	}

	/* skip past packets that userspace has already processed,
	 * making them available for reception. We don't need to set
	 * the length as it is the same for all slots.
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* userspace has read some packets. */
		n = 0;
		while (j != k) {
			struct netmap_slot *slot = &ring->slot[j];
			struct e1000_rx_desc *curr = &adapter->rx_desc_base[j];
			struct em_buffer *rxbuf = &adapter->rx_buffer_area[j];
			void *addr = NMB(slot);

			if (addr == netmap_buffer_base) { /* bad buf */
				if (do_lock)
					EM_RX_UNLOCK(adapter);
				return netmap_ring_reinit(kring);
			}
			curr = &adapter->rx_desc_base[j];
			curr->status = 0;
			if (slot->flags & NS_BUF_CHANGED) {
				curr->buffer_addr = htole64(vtophys(addr));
				/* buffer has changed, unload and reload map */
				netmap_reload_map(adapter->rxtag, rxbuf->map,
					addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}

			bus_dmamap_sync(adapter->rxtag, rxbuf->map,
				BUS_DMASYNC_PREREAD);

			j = (j == lim) ? 0 : j + 1;
			n++;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = ring->cur;
		bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/*
		 * IMPORTANT: we must leave one free slot in the ring,
		 * so move j back by one unit
		 */
		j = (j == 0) ? lim : j - 1;
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), j);
	}

	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail ;
	if (do_lock)
		EM_RX_UNLOCK(adapter);
	return 0;
}


/*
 * Register/unregister routine
 */
static int
lem_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct adapter *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (!na)
		return EINVAL;

	lem_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* lem_netmap_block_tasks(adapter); */
#ifndef EM_LEGACY_IRQ
	taskqueue_block(adapter->tq);
	taskqueue_drain(adapter->tq, &adapter->rxtx_task);
	taskqueue_drain(adapter->tq, &adapter->link_task);
#endif /* !EM_LEGCY_IRQ */
	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		/* save if_transmit to restore it when exiting.
		 * XXX what about if_start and if_qflush ?
		 */
		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_start;

		lem_init_locked(adapter);
		if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) == 0) {
			error = ENOMEM;
			goto fail;
		}
	} else {
fail:
		/* restore non-netmap mode */
		ifp->if_transmit = na->if_transmit;
		ifp->if_capenable &= ~IFCAP_NETMAP;
		lem_init_locked(adapter);	/* also enables intr */
	}

#ifndef EM_LEGACY_IRQ
	taskqueue_unblock(adapter->tq);
#endif /* !EM_LEGCY_IRQ */

	return (error);
}
