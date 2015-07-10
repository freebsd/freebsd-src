/*
 * Copyright (C) 2014 Vincenzo Maffione, Luigi Rizzo. All rights reserved.
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
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>


#define SOFTC_T	vtnet_softc

/* Free all the unused buffer in all the RX virtqueues.
 * This function is called when entering and exiting netmap mode.
 * - buffers queued by the virtio driver return skbuf/mbuf pointer
 *   and need to be freed;
 * - buffers queued by netmap return the txq/rxq, and do not need work
 */
static void
vtnet_netmap_free_bufs(struct SOFTC_T* sc)
{
	int i, nmb = 0, n = 0, last;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		struct vtnet_rxq *rxq = &sc->vtnet_rxqs[i];
		struct virtqueue *vq;
		struct mbuf *m;
		struct vtnet_txq *txq = &sc->vtnet_txqs[i];
                struct vtnet_tx_header *txhdr;

		last = 0;
		vq = rxq->vtnrx_vq;
		while ((m = virtqueue_drain(vq, &last)) != NULL) {
			n++;
			if (m != (void *)rxq)
				m_freem(m);
			else
				nmb++;
		}

		last = 0;
		vq = txq->vtntx_vq;
		while ((txhdr = virtqueue_drain(vq, &last)) != NULL) {
			n++;
			if (txhdr != (void *)txq) {
				m_freem(txhdr->vth_mbuf);
				uma_zfree(vtnet_tx_header_zone, txhdr);
			} else
				nmb++;
		}
	}
	D("freed %d mbufs, %d netmap bufs on %d queues",
		n - nmb, nmb, i);
}

/* Register and unregister. */
static int
vtnet_netmap_reg(struct netmap_adapter *na, int onoff)
{
        struct ifnet *ifp = na->ifp;
	struct SOFTC_T *sc = ifp->if_softc;

	VTNET_CORE_LOCK(sc);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	/* enable or disable flags and callbacks in na and ifp */
	if (onoff) {
		nm_set_native_flags(na);
	} else {
		nm_clear_native_flags(na);
	}
	/* drain queues so netmap and native drivers
	 * do not interfere with each other
	 */
	vtnet_netmap_free_bufs(sc);
        vtnet_init_locked(sc);       /* also enable intr */
        VTNET_CORE_UNLOCK(sc);
        return (ifp->if_drv_flags & IFF_DRV_RUNNING ? 0 : 1);
}


/* Reconcile kernel and user view of the transmit ring. */
static int
vtnet_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
        struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int ring_nr = kring->ring_id;
	u_int nm_i;	/* index into the netmap ring */
	u_int nic_i;	/* index into the NIC ring */
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;

	/* device-specific */
	struct SOFTC_T *sc = ifp->if_softc;
	struct vtnet_txq *txq = &sc->vtnet_txqs[ring_nr];
	struct virtqueue *vq = txq->vtntx_vq;

	/*
	 * First part: process new packets to send.
	 */
	rmb();
	
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {	/* we have new packets to send */
		struct sglist *sg = txq->vtntx_sg;

		nic_i = netmap_idx_k2n(kring, nm_i);
		for (n = 0; nm_i != head; n++) {
			/* we use an empty header here */
			static struct virtio_net_hdr_mrg_rxbuf hdr;
			struct netmap_slot *slot = &ring->slot[nm_i];
			u_int len = slot->len;
			uint64_t paddr;
			void *addr = PNMB(na, slot, &paddr);
                        int err;

			NM_CHECK_ADDR_LEN(na, addr, len);

			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);
			/* Initialize the scatterlist, expose it to the hypervisor,
			 * and kick the hypervisor (if necessary).
			 */
			sglist_reset(sg); // cheap
			// if vtnet_hdr_size > 0 ...
			err = sglist_append(sg, &hdr, sc->vtnet_hdr_size);
			// XXX later, support multi segment
			err = sglist_append_phys(sg, paddr, len);
			/* use na as the cookie */
                        err = virtqueue_enqueue(vq, txq, sg, sg->sg_nseg, 0);
                        if (unlikely(err < 0)) {
                                D("virtqueue_enqueue failed");
                                break;
                        }

			nm_i = nm_next(nm_i, lim);
			nic_i = nm_next(nic_i, lim);
		}
		/* Update hwcur depending on where we stopped. */
		kring->nr_hwcur = nm_i; /* note we migth break early */

		/* No more free TX slots? Ask the hypervisor for notifications,
		 * possibly only when a considerable amount of work has been
		 * done.
		 */
		ND(3,"sent %d packets, hwcur %d", n, nm_i);
		virtqueue_disable_intr(vq);
		virtqueue_notify(vq);
	} else {
		if (ring->head != ring->tail)
		    ND(5, "pure notify ? head %d tail %d nused %d %d",
			ring->head, ring->tail, virtqueue_nused(vq),
			(virtqueue_dump(vq), 1));
		virtqueue_notify(vq);
		virtqueue_enable_intr(vq); // like postpone with 0
	}

	
        /* Free used slots. We only consider our own used buffers, recognized
	 * by the token we passed to virtqueue_add_outbuf.
	 */
        n = 0;
        for (;;) {
                struct vtnet_tx_header *txhdr = virtqueue_dequeue(vq, NULL);
                if (txhdr == NULL)
                        break;
                if (likely(txhdr == (void *)txq)) {
                        n++;
			if (virtqueue_nused(vq) < 32) { // XXX slow release
				break;
			}
		} else { /* leftover from previous transmission */
			m_freem(txhdr->vth_mbuf);
			uma_zfree(vtnet_tx_header_zone, txhdr);
		}
        }
	if (n) {
		kring->nr_hwtail += n;
		if (kring->nr_hwtail > lim)
			kring->nr_hwtail -= lim + 1;
	}
	if (nm_i != kring->nr_hwtail /* && vtnet_txq_below_threshold(txq) == 0*/) {
		ND(3, "disable intr, hwcur %d", nm_i);
		virtqueue_disable_intr(vq);
	} else {
		ND(3, "enable intr, hwcur %d", nm_i);
		virtqueue_postpone_intr(vq, VQ_POSTPONE_SHORT);
	}

        return 0;
}

static int
vtnet_refill_rxq(struct netmap_kring *kring, u_int nm_i, u_int head)
{
	struct netmap_adapter *na = kring->na;
        struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int ring_nr = kring->ring_id;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int n;

	/* device-specific */
	struct SOFTC_T *sc = ifp->if_softc;
	struct vtnet_rxq *rxq = &sc->vtnet_rxqs[ring_nr];
	struct virtqueue *vq = rxq->vtnrx_vq;

	/* use a local sglist, default might be short */
	struct sglist_seg ss[2];
	struct sglist sg = { ss, 0, 0, 2 };

	for (n = 0; nm_i != head; n++) {
		static struct virtio_net_hdr_mrg_rxbuf hdr;
		struct netmap_slot *slot = &ring->slot[nm_i];
		uint64_t paddr;
		void *addr = PNMB(na, slot, &paddr);
		int err = 0;

		if (addr == NETMAP_BUF_BASE(na)) { /* bad buf */
			if (netmap_ring_reinit(kring))
				return -1;
		}

		slot->flags &= ~NS_BUF_CHANGED;
		sglist_reset(&sg); // cheap
		err = sglist_append(&sg, &hdr, sc->vtnet_hdr_size);
		err = sglist_append_phys(&sg, paddr, NETMAP_BUF_SIZE(na));
		/* writable for the host */
		err = virtqueue_enqueue(vq, rxq, &sg, 0, sg.sg_nseg);
		if (err < 0) {
			D("virtqueue_enqueue failed");
			break;
		}
		nm_i = nm_next(nm_i, lim);
	}
	return nm_i;
}

/* Reconcile kernel and user view of the receive ring. */
static int
vtnet_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
        struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int ring_nr = kring->ring_id;
	u_int nm_i;	/* index into the netmap ring */
	// u_int nic_i;	/* index into the NIC ring */
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;

	/* device-specific */
	struct SOFTC_T *sc = ifp->if_softc;
	struct vtnet_rxq *rxq = &sc->vtnet_rxqs[ring_nr];
	struct virtqueue *vq = rxq->vtnrx_vq;

	/* XXX netif_carrier_ok ? */

	if (head > lim)
		return netmap_ring_reinit(kring);

	rmb();
	/*
	 * First part: import newly received packets.
	 * Only accept our
	 * own buffers (matching the token). We should only get
	 * matching buffers, because of vtnet_netmap_free_rx_unused_bufs()
	 * and vtnet_netmap_init_buffers().
	 */
	if (netmap_no_pendintr || force_update) {
		uint16_t slot_flags = kring->nkr_slot_flags;
                struct netmap_adapter *token;

                nm_i = kring->nr_hwtail;
                n = 0;
		for (;;) {
			int len;
                        token = virtqueue_dequeue(vq, &len);
                        if (token == NULL)
                                break;
                        if (likely(token == (void *)rxq)) {
                            ring->slot[nm_i].len = len;
                            ring->slot[nm_i].flags = slot_flags;
                            nm_i = nm_next(nm_i, lim);
                            n++;
                        } else {
			    D("This should not happen");
                        }
		}
		kring->nr_hwtail = nm_i;
		kring->nr_kflags &= ~NKR_PENDINTR;
	}
        ND("[B] h %d c %d hwcur %d hwtail %d",
		ring->head, ring->cur, kring->nr_hwcur,
			      kring->nr_hwtail);

	/*
	 * Second part: skip past packets that userspace has released.
	 */
	nm_i = kring->nr_hwcur; /* netmap ring index */
	if (nm_i != head) {
		int err = vtnet_refill_rxq(kring, nm_i, head);
		if (err < 0)
			return 1;
		kring->nr_hwcur = err;
		virtqueue_notify(vq);
		/* After draining the queue may need an intr from the hypervisor */
        	vtnet_rxq_enable_intr(rxq);
	}

        ND("[C] h %d c %d t %d hwcur %d hwtail %d",
		ring->head, ring->cur, ring->tail,
		kring->nr_hwcur, kring->nr_hwtail);

	return 0;
}


/* Make RX virtqueues buffers pointing to netmap buffers. */
static int
vtnet_netmap_init_rx_buffers(struct SOFTC_T *sc)
{
	struct ifnet *ifp = sc->vtnet_ifp;
	struct netmap_adapter* na = NA(ifp);
	unsigned int r;

	if (!nm_native_on(na))
		return 0;
	for (r = 0; r < na->num_rx_rings; r++) {
                struct netmap_kring *kring = &na->rx_rings[r];
		struct vtnet_rxq *rxq = &sc->vtnet_rxqs[r];
		struct virtqueue *vq = rxq->vtnrx_vq;
	        struct netmap_slot* slot;
		int err = 0;

		slot = netmap_reset(na, NR_RX, r, 0);
		if (!slot) {
			D("strange, null netmap ring %d", r);
			return 0;
		}
		/* Add up to na>-num_rx_desc-1 buffers to this RX virtqueue.
		 * It's important to leave one virtqueue slot free, otherwise
		 * we can run into ring->cur/ring->tail wraparounds.
		 */
		err = vtnet_refill_rxq(kring, 0, na->num_rx_desc-1);
		if (err < 0)
			return 0;
		virtqueue_notify(vq);
	}

	return 1;
}

/* Update the virtio-net device configurations. Number of queues can
 * change dinamically, by 'ethtool --set-channels $IFNAME combined $N'.
 * This is actually the only way virtio-net can currently enable
 * the multiqueue mode.
 * XXX note that we seem to lose packets if the netmap ring has more
 * slots than the queue
 */
static int
vtnet_netmap_config(struct netmap_adapter *na, u_int *txr, u_int *txd,
						u_int *rxr, u_int *rxd)
{
	struct ifnet *ifp = na->ifp;
	struct SOFTC_T *sc = ifp->if_softc;

	*txr = *rxr = sc->vtnet_max_vq_pairs;
	*rxd = 512; // sc->vtnet_rx_nmbufs;
	*txd = *rxd; // XXX
        D("vtnet config txq=%d, txd=%d rxq=%d, rxd=%d",
					*txr, *txd, *rxr, *rxd);

	return 0;
}

static void
vtnet_netmap_attach(struct SOFTC_T *sc)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = sc->vtnet_ifp;
	na.num_tx_desc =  1024;// sc->vtnet_rx_nmbufs;
	na.num_rx_desc =  1024; // sc->vtnet_rx_nmbufs;
	na.nm_register = vtnet_netmap_reg;
	na.nm_txsync = vtnet_netmap_txsync;
	na.nm_rxsync = vtnet_netmap_rxsync;
	na.nm_config = vtnet_netmap_config;
	na.num_tx_rings = na.num_rx_rings = sc->vtnet_max_vq_pairs;
	D("max rings %d", sc->vtnet_max_vq_pairs);
	netmap_attach(&na);

        D("virtio attached txq=%d, txd=%d rxq=%d, rxd=%d",
			na.num_tx_rings, na.num_tx_desc,
			na.num_tx_rings, na.num_rx_desc);
}
/* end of file */
