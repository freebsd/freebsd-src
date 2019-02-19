/*
 * Copyright (C) 2019 Universita` di Pisa.
 * Sponsored by Sunny Valley Networks.
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

/* $FreeBSD$ */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

static int
vmxnet3_netmap_reg(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	struct vmxnet3_softc *sc = ifp->if_softc;

	VMXNET3_CORE_LOCK(sc);
	vmxnet3_stop(sc);
	if (onoff) {
		nm_set_native_flags(na);
	} else {
		nm_clear_native_flags(na);
	}
	vmxnet3_init_locked(sc);
	VMXNET3_CORE_UNLOCK(sc);
	return 0;
}

static void
vmxnet3_netmap_rxq_init(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rxq,
		struct vmxnet3_rxring *rxr, struct netmap_slot *slot)
{
	struct ifnet *ifp = sc->vmx_ifp;
	struct netmap_adapter *na = NA(ifp);
	struct vmxnet3_rxdesc *rxd;
	int q, i;

	q = rxq - sc->vmx_rxq;

	for (i = 0; ; i++) {
		int idx = rxr->vxrxr_fill;
		int si = netmap_idx_n2k(na->rx_rings[q], idx);
		struct vmxnet3_rxbuf  *rxb = &rxr->vxrxr_rxbuf[idx];
		uint64_t paddr;
		void *addr;

		addr = PNMB(na, slot +  si, &paddr);
		netmap_load_map(na, rxr->vxrxr_rxtag, rxb->vrxb_dmamap, addr);

		rxd = &rxr->vxrxr_rxd[idx];
		rxd->addr = paddr;
		rxd->len = NETMAP_BUF_SIZE(na);
		rxd->gen = rxr->vxrxr_gen ^ 1;
		rxd->btype = VMXNET3_BTYPE_HEAD;
		nm_prdis("%d: addr %lx len %u btype %u gen %u",
			idx, rxd->addr, rxd->len, rxd->btype, rxd->gen);

		if (i == rxr->vxrxr_ndesc -1)
			break;

		rxd->gen ^= 1;
		vmxnet3_rxr_increment_fill(rxr);
	}
}

static void
vmxnet3_netmap_txq_init(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *txq)
{
	struct ifnet *ifp = sc->vmx_ifp;
	struct netmap_adapter *na;
	struct netmap_slot *slot;
	struct vmxnet3_txring *txr;
	int i, gen, q;

	q = txq - sc->vmx_txq;

	na = NA(ifp);

	slot = netmap_reset(na, NR_TX, q, 0);
	if (slot == NULL)
		return;

	txr = &txq->vxtxq_cmd_ring;
	gen = txr->vxtxr_gen ^ 1;

	for (i = 0; i < txr->vxtxr_ndesc; i++) {
		int si = netmap_idx_n2k(na->tx_rings[q], i);
		struct vmxnet3_txdesc *txd = &txr->vxtxr_txd[i];
		uint64_t paddr;
		void *addr;

		addr = PNMB(na, slot +  si, &paddr);

		txd->addr = paddr;
		txd->len = 0;
		txd->gen = gen;
		txd->dtype = 0;
		txd->offload_mode = VMXNET3_OM_NONE;
		txd->offload_pos = 0;
		txd->hlen = 0;
		txd->eop = 0;
		txd->compreq = 0;
		txd->vtag_mode = 0;
		txd->vtag = 0;

		netmap_load_map(na, txr->vxtxr_txtag,
				txr->vxtxr_txbuf[i].vtxb_dmamap, addr);
	}
}

static int
vmxnet3_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;
	u_int nic_i;
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;

	/*
	 * interrupts on every tx packet are expensive so request
	 * them every half ring, or where NS_REPORT is set
	 */
	u_int report_frequency = kring->nkr_num_slots >> 1;
	/* device specific */
	struct vmxnet3_softc *sc = ifp->if_softc;
	struct vmxnet3_txqueue *txq = &sc->vmx_txq[kring->ring_id];
	struct vmxnet3_txring *txr = &txq->vxtxq_cmd_ring;
	struct vmxnet3_comp_ring *txc = &txq->vxtxq_comp_ring;
	struct vmxnet3_txcompdesc *txcd = txc->vxcr_u.txcd;
	int gen = txr->vxtxr_gen;

	/* no need to dma-sync the ring; memory barriers are sufficient */

	nm_i = kring->nr_hwcur;
	if (nm_i != head) {
		nic_i = netmap_idx_k2n(kring, nm_i);
		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			u_int len = slot->len;
			uint64_t paddr;
			void *addr = PNMB(na, slot, &paddr);
			int compreq = !!(slot->flags & NS_REPORT ||
				nic_i == 0 || nic_i == report_frequency);

			/* device specific */
			struct vmxnet3_txdesc *curr = &txr->vxtxr_txd[nic_i];
			struct vmxnet3_txbuf *txbuf = &txr->vxtxr_txbuf[nic_i];

			NM_CHECK_ADDR_LEN(na, addr, len);

			/* fill the slot in the NIC ring */
			curr->len = len;
			curr->eop = 1; /* NS_MOREFRAG not supported */
			curr->compreq = compreq;

			if (slot->flags & NS_BUF_CHANGED) {
				curr->addr = paddr;
				netmap_reload_map(na, txr->vxtxr_txtag,
						txbuf->vtxb_dmamap, addr);
			}
			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);

			/* make sure changes to the buffer are synced */
			bus_dmamap_sync(txr->vxtxr_txtag, txbuf->vtxb_dmamap,
					BUS_DMASYNC_PREWRITE);

			/* pass ownership */
			vmxnet3_barrier(sc, VMXNET3_BARRIER_WR);
			curr->gen = gen;

			nm_i = nm_next(nm_i, lim);
			nic_i++;
			if (unlikely(nic_i == lim + 1)) {
				nic_i = 0;
				gen = txr->vxtxr_gen ^= 1;
			}
		}

		vmxnet3_write_bar0(sc, VMXNET3_BAR0_TXH(txq->vxtxq_id), nic_i);
	}
	kring->nr_hwcur = nm_i;

	/* reclaim completed packets */
	for (;;) {
		u_int sop;
		struct vmxnet3_txbuf *txb;

		txcd = &txc->vxcr_u.txcd[txc->vxcr_next];
		if (txcd->gen != txc->vxcr_gen)
			break;

		vmxnet3_barrier(sc, VMXNET3_BARRIER_RD);

		if (++txc->vxcr_next == txc->vxcr_ndesc) {
			txc->vxcr_next = 0;
			txc->vxcr_gen ^= 1;
		}

		sop = txr->vxtxr_next;
		txb = &txr->vxtxr_txbuf[sop];

		bus_dmamap_sync(txr->vxtxr_txtag, txb->vtxb_dmamap,
		   BUS_DMASYNC_POSTWRITE);

		txr->vxtxr_next = (txcd->eop_idx + 1) % txr->vxtxr_ndesc;
	}
	kring->nr_hwtail = nm_prev(netmap_idx_n2k(kring, txr->vxtxr_next), lim);

	return 0;
}

static int
vmxnet3_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;
	u_int nic_i;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	int force_update = (flags & NAF_FORCE_READ);

	struct ifnet *ifp = na->ifp;
	struct vmxnet3_softc *sc = ifp->if_softc;
	struct vmxnet3_rxqueue *rxq = &sc->vmx_rxq[kring->ring_id];
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_comp_ring *rxc;

	if (head > lim)
		return netmap_ring_reinit(kring);

	rxr = &rxq->vxrxq_cmd_ring[0];

	/* no need to dma-sync the ring; memory barriers are sufficient */

	/* first part: import newly received packets */
	if (netmap_no_pendintr || force_update) {
		rxc = &rxq->vxrxq_comp_ring;
		nm_i = kring->nr_hwtail;
		nic_i = netmap_idx_k2n(kring, nm_i);
		for (;;) {
			struct vmxnet3_rxcompdesc *rxcd;
			struct vmxnet3_rxbuf *rxb;

			rxcd = &rxc->vxcr_u.rxcd[rxc->vxcr_next];

			if (rxcd->gen != rxc->vxcr_gen)
				break;
			vmxnet3_barrier(sc, VMXNET3_BARRIER_RD);

			while (__predict_false(rxcd->rxd_idx != nic_i)) {
				nm_prlim(1, "%u skipped! idx %u", nic_i, rxcd->rxd_idx);
				/* to shelter the application from this  we
				 * would need to rotate the kernel-owned
				 * portion of the netmap and nic rings. We
				 * return len=0 for now and hope for the best.
				 */
				ring->slot[nm_i].len = 0;
				nic_i = nm_next(nm_i, lim);
				nm_i = nm_next(nm_i, lim);
			}

			rxb = &rxr->vxrxr_rxbuf[nic_i];

			ring->slot[nm_i].len = rxcd->len;
			ring->slot[nm_i].flags = 0;

			bus_dmamap_sync(rxr->vxrxr_rxtag, rxb->vrxb_dmamap,
					BUS_DMASYNC_POSTREAD);

			nic_i = nm_next(nm_i, lim);
			nm_i = nm_next(nm_i, lim);

			rxc->vxcr_next++;
			if (__predict_false(rxc->vxcr_next == rxc->vxcr_ndesc)) {
				rxc->vxcr_next = 0;
				rxc->vxcr_gen ^= 1;
			}
		}
		kring->nr_hwtail = nm_i;
	}
	/* second part: skip past packets that userspace has released */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {
		nic_i = netmap_idx_k2n(kring, nm_i);
		while (nm_i != head) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			struct vmxnet3_rxdesc *rxd_fill;
			struct vmxnet3_rxbuf *rxbuf;

			if (slot->flags & NS_BUF_CHANGED) {
				uint64_t paddr;
				void *addr = PNMB(na, slot, &paddr);
				struct vmxnet3_rxdesc *rxd = &rxr->vxrxr_rxd[nic_i];


				if (addr == NETMAP_BUF_BASE(na))
					return netmap_ring_reinit(kring);

				rxd->addr = paddr;
				rxbuf = &rxr->vxrxr_rxbuf[nic_i];
				netmap_reload_map(na, rxr->vxrxr_rxtag,
						rxbuf->vrxb_dmamap, addr);
				slot->flags &= ~NS_BUF_CHANGED;
				vmxnet3_barrier(sc, VMXNET3_BARRIER_WR);
			}

			rxd_fill = &rxr->vxrxr_rxd[rxr->vxrxr_fill];
			rxbuf = &rxr->vxrxr_rxbuf[rxr->vxrxr_fill];

			bus_dmamap_sync(rxr->vxrxr_rxtag, rxbuf->vrxb_dmamap,
					BUS_DMASYNC_PREREAD);

			rxd_fill->gen = rxr->vxrxr_gen;
			vmxnet3_rxr_increment_fill(rxr);

			nm_i = nm_next(nm_i, lim);
			nic_i = nm_next(nic_i, lim);
		}
		kring->nr_hwcur = head;
		if (__predict_false(rxq->vxrxq_rs->update_rxhead)) {
			vmxnet3_write_bar0(sc,
				VMXNET3_BAR0_RXH1(kring->ring_id), rxr->vxrxr_fill);
		}
	}
	return 0;
}

static void
vmxnet3_netmap_attach(struct vmxnet3_softc *sc)
{
	struct netmap_adapter na;
	int enable = 0;

	if (getenv_int("vmxnet3.netmap_native", &enable) < 0 || !enable) {
		return;
	}

	bzero(&na, sizeof(na));

	na.ifp = sc->vmx_ifp;
	na.na_flags = NAF_BDG_MAYSLEEP;
	na.num_tx_desc = sc->vmx_ntxdescs;
	na.num_rx_desc = sc->vmx_nrxdescs;
	na.num_tx_rings = sc->vmx_ntxqueues;
	na.num_rx_rings = sc->vmx_nrxqueues;
	na.nm_register = vmxnet3_netmap_reg;
	na.nm_txsync = vmxnet3_netmap_txsync;
	na.nm_rxsync = vmxnet3_netmap_rxsync;
	netmap_attach(&na);
}
