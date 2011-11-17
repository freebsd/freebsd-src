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
 * $Id: if_re_netmap.h 9662 2011-11-16 13:18:06Z luigi $
 *
 * netmap support for if_re
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>

static int re_netmap_reg(struct ifnet *, int onoff);
static int re_netmap_txsync(void *, u_int, int);
static int re_netmap_rxsync(void *, u_int, int);
static void re_netmap_lock_wrapper(void *, int, u_int);

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
	na.buff_size = MCLBYTES;
	netmap_attach(&na, 1);
}


/*
 * wrapper to export locks to the generic code
 * We should not use the tx/rx locks
 */
static void
re_netmap_lock_wrapper(void *_a, int what, u_int queueid)
{
	struct rl_softc *adapter = _a;

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

	if (!na)
		return EINVAL;
	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	re_stop(adapter);

	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		/* save if_transmit and restore it */
		na->if_transmit = ifp->if_transmit;
		/* XXX if_start and if_qflush ??? */
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
 *
 * Userspace has filled tx slots up to cur (excluded).
 * The last unused slot previously known to the kernel was nr_hwcur,
 * and the last interrupt reported nr_hwavail slots available
 * (using the special value -1 to indicate idle transmit ring).
 * The function must first update avail to what the kernel
 * knows (translating the -1 to nkr_num_slots - 1),
 * subtract the newly used slots (cur - nr_hwcur)
 * from both avail and nr_hwavail, and set nr_hwcur = cur
 * issuing a dmamap_sync on all slots.
 */
static int
re_netmap_txsync(void *a, u_int ring_nr, int do_lock)
{
	struct rl_softc *sc = a;
	struct rl_txdesc *txd = sc->rl_ldata.rl_tx_desc;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if ( (kring->nr_kflags & NR_REINIT) || k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		RL_LOCK(sc);

	/* Sync the TX descriptor list */
	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
            sc->rl_ldata.rl_tx_list_map,
            BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* record completed transmissions */
        for (n = 0, j = sc->rl_ldata.rl_tx_considx;
	    j != sc->rl_ldata.rl_tx_prodidx;
	    n++, j = RL_TX_DESC_NXT(sc, j)) {
		uint32_t cmdstat =
			le32toh(sc->rl_ldata.rl_tx_list[j].rl_cmdstat);
		if (cmdstat & RL_TDESC_STAT_OWN)
			break;
	}
	if (n > 0) {
		sc->rl_ldata.rl_tx_considx = j;
		sc->rl_ldata.rl_tx_free += n;
		kring->nr_hwavail += n;
	}

	/* update avail to what the hardware knows */
	ring->avail = kring->nr_hwavail;
	
	/* we trust prodidx, not hwcur */
	j = kring->nr_hwcur = sc->rl_ldata.rl_tx_prodidx;
	if (j != k) {	/* we have new packets to send */
		n = 0;
		while (j != k) {
			struct netmap_slot *slot = &ring->slot[j];
			struct rl_desc *desc = &sc->rl_ldata.rl_tx_list[j];
			int cmd = slot->len | RL_TDESC_CMD_EOF |
				RL_TDESC_CMD_OWN | RL_TDESC_CMD_SOF ;
			void *addr = NMB(slot);
			int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				if (do_lock)
					RL_UNLOCK(sc);
				return netmap_ring_reinit(kring);
			}
			
			if (j == lim)	/* mark end of ring */
				cmd |= RL_TDESC_CMD_EOR;

			if (slot->flags & NS_BUF_CHANGED) {
				uint64_t paddr = vtophys(addr);
				desc->rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
				desc->rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
				/* buffer has changed, unload and reload map */
				netmap_reload_map(sc->rl_ldata.rl_tx_mtag,
					txd[j].tx_dmamap, addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			slot->flags &= ~NS_REPORT;
			desc->rl_cmdstat = htole32(cmd);
			bus_dmamap_sync(sc->rl_ldata.rl_tx_mtag,
				txd[j].tx_dmamap, BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			n++;
		}
		sc->rl_ldata.rl_tx_prodidx = kring->nr_hwcur = ring->cur;

		/* decrease avail by number of sent packets */
		ring->avail -= n;
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
 *
 * Userspace has read rx slots up to cur (excluded).
 * The last unread slot previously known to the kernel was nr_hwcur,
 * and the last interrupt reported nr_hwavail slots available.
 * We must subtract the newly consumed slots (cur - nr_hwcur)
 * from nr_hwavail, clearing the descriptors for the next
 * read, tell the hardware that they are available,
 * and set nr_hwcur = cur and avail = nr_hwavail.
 * issuing a dmamap_sync on all slots.
 */
static int
re_netmap_rxsync(void *a, u_int ring_nr, int do_lock)
{
	struct rl_softc *sc = a;
	struct rl_rxdesc *rxd = sc->rl_ldata.rl_rx_desc;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if ( (kring->nr_kflags & NR_REINIT) || k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		RL_LOCK(sc);
	/* XXX check sync modes */
	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * The device uses all the buffers in the ring, so we need
	 * another termination condition in addition to RL_RDESC_STAT_OWN
	 * cleared (all buffers could have it cleared. The easiest one
	 * is to limit the amount of data reported up to 'lim'
	 */
	j = sc->rl_ldata.rl_rx_prodidx;
	for (n = kring->nr_hwavail; n < lim ; n++) {
		struct rl_desc *cur_rx = &sc->rl_ldata.rl_rx_list[j];
		uint32_t rxstat = le32toh(cur_rx->rl_cmdstat);
		uint32_t total_len;

		if ((rxstat & RL_RDESC_STAT_OWN) != 0)
			break;
		total_len = rxstat & sc->rl_rxlenmask;
		/* XXX subtract crc */
		total_len = (total_len < 4) ? 0 : total_len - 4;
		kring->ring->slot[j].len = total_len;
		/*  sync was in re_newbuf() */
		bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
		    rxd[j].rx_dmamap, BUS_DMASYNC_POSTREAD);
		j = RL_RX_DESC_NXT(sc, j);
	}
	if (n != kring->nr_hwavail) {
		sc->rl_ldata.rl_rx_prodidx = j;
		sc->rl_ifp->if_ipackets += n - kring->nr_hwavail;
		kring->nr_hwavail = n;
	}

	/* skip past packets that userspace has already processed,
	 * making them available for reception.
	 * advance nr_hwcur and issue a bus_dmamap_sync on the
	 * buffers so it is safe to write to them.
	 * Also increase nr_hwavail
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* userspace has read some packets. */
		n = 0;
		while (j != k) {
			struct netmap_slot *slot = ring->slot + j;
			struct rl_desc *desc = &sc->rl_ldata.rl_rx_list[j];
			int cmd = na->buff_size | RL_RDESC_CMD_OWN;
			void *addr = NMB(slot);

			if (addr == netmap_buffer_base) { /* bad buf */
				if (do_lock)
					RL_UNLOCK(sc);
				return netmap_ring_reinit(kring);
			}

			if (j == lim)	/* mark end of ring */
				cmd |= RL_RDESC_CMD_EOR;

			desc->rl_cmdstat = htole32(cmd);
			slot->flags &= ~NS_REPORT;
			if (slot->flags & NS_BUF_CHANGED) {
				uint64_t paddr = vtophys(addr);
				desc->rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
				desc->rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
				netmap_reload_map(sc->rl_ldata.rl_rx_mtag,
					rxd[j].rx_dmamap, addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
				rxd[j].rx_dmamap, BUS_DMASYNC_PREREAD);
			j = (j == lim) ? 0 : j + 1;
			n++;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
		/* Flush the RX DMA ring */

		bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
		    sc->rl_ldata.rl_rx_list_map,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail ;
	if (do_lock)
		RL_UNLOCK(sc);
	return 0;
}

static void
re_netmap_tx_init(struct rl_softc *sc)
{   
	struct rl_txdesc *txd;
	struct rl_desc *desc;
	int i;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_TX, 0, 0);

	/* slot is NULL if we are not in netmap mode */
	if (!slot)
		return;
	/* in netmap mode, overwrite addresses and maps */
	txd = sc->rl_ldata.rl_tx_desc;
	desc = sc->rl_ldata.rl_tx_list;

	for (i = 0; i < sc->rl_ldata.rl_tx_desc_cnt; i++) {
		void *addr = NMB(slot+i);
		uint64_t paddr = vtophys(addr);

		desc[i].rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
		desc[i].rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
		netmap_load_map(sc->rl_ldata.rl_tx_mtag,
			txd[i].tx_dmamap, addr, na->buff_size);
	}
}

static void
re_netmap_rx_init(struct rl_softc *sc)
{
	/* slot is NULL if we are not in netmap mode */  
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_RX, 0, 0);
	struct rl_desc *desc = sc->rl_ldata.rl_rx_list;
	uint32_t cmdstat;
	int i;

	if (!slot)
		return;

	for (i = 0; i < sc->rl_ldata.rl_rx_desc_cnt; i++) {
		void *addr = NMB(slot+i);
		uint64_t paddr = vtophys(addr);

		desc[i].rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
		desc[i].rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
		cmdstat = slot[i].len = na->buff_size; // XXX
		if (i == sc->rl_ldata.rl_rx_desc_cnt - 1)
			cmdstat |= RL_RDESC_CMD_EOR;
		desc[i].rl_cmdstat = htole32(cmdstat | RL_RDESC_CMD_OWN);

		netmap_reload_map(sc->rl_ldata.rl_rx_mtag,
			sc->rl_ldata.rl_rx_desc[i].rx_dmamap,
			addr, na->buff_size);
	}
}
