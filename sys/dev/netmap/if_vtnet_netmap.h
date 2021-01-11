/*
 * Copyright (C) 2014-2018 Vincenzo Maffione, Luigi Rizzo.
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

/* Register and unregister. */
static int
vtnet_netmap_reg(struct netmap_adapter *na, int state)
{
	struct ifnet *ifp = na->ifp;
	struct vtnet_softc *sc = ifp->if_softc;

	/*
	 * Trigger a device reinit, asking vtnet_init_locked() to
	 * also enter or exit netmap mode.
	 */
	VTNET_CORE_LOCK(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	vtnet_init_locked(sc, state ? VTNET_INIT_NETMAP_ENTER
	    : VTNET_INIT_NETMAP_EXIT);
	VTNET_CORE_UNLOCK(sc);

	return (0);
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
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;

	/* device-specific */
	struct vtnet_softc *sc = ifp->if_softc;
	struct vtnet_txq *txq = &sc->vtnet_txqs[ring_nr];
	struct virtqueue *vq = txq->vtntx_vq;
	int interrupts = !(kring->nr_kflags & NKR_NOINTR);
	u_int n;

	/*
	 * First part: process new packets to send.
	 */

	nm_i = kring->nr_hwcur;
	if (nm_i != head) {	/* we have new packets to send */
		struct sglist *sg = txq->vtntx_sg;

		for (; nm_i != head; nm_i = nm_next(nm_i, lim)) {
			/* we use an empty header here */
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
			err = sglist_append(sg, &txq->vtntx_shrhdr, sc->vtnet_hdr_size);
			err |= sglist_append_phys(sg, paddr, len);
			KASSERT(err == 0, ("%s: cannot append to sglist %d",
						__func__, err));
			err = virtqueue_enqueue(vq, /*cookie=*/txq, sg,
						/*readable=*/sg->sg_nseg,
						/*writeable=*/0);
			if (unlikely(err)) {
				if (err != ENOSPC)
					nm_prerr("virtqueue_enqueue(%s) failed: %d",
							kring->name, err);
				break;
			}
		}

		virtqueue_notify(vq);

		/* Update hwcur depending on where we stopped. */
		kring->nr_hwcur = nm_i; /* note we migth break early */
	}

	/* Free used slots. We only consider our own used buffers, recognized
	 * by the token we passed to virtqueue_enqueue.
	 */
	n = 0;
	for (;;) {
		void *token = virtqueue_dequeue(vq, NULL);
		if (token == NULL)
			break;
		if (unlikely(token != (void *)txq))
			nm_prerr("BUG: TX token mismatch");
		else
			n++;
	}
	if (n > 0) {
		kring->nr_hwtail += n;
		if (kring->nr_hwtail > lim)
			kring->nr_hwtail -= lim + 1;
	}

	if (interrupts && virtqueue_nfree(vq) < 32)
		virtqueue_postpone_intr(vq, VQ_POSTPONE_LONG);

	return 0;
}

/*
 * Publish 'num 'netmap receive buffers to the host, starting
 * from the next available one (rx->vtnrx_nm_refill).
 * Return a positive error code on error, and 0 on success.
 * If we could not publish all of the buffers that's an error,
 * since the netmap ring and the virtqueue would go out of sync.
 */
static int
vtnet_netmap_kring_refill(struct netmap_kring *kring, u_int num)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int ring_nr = kring->ring_id;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int nm_i;

	/* device-specific */
	struct vtnet_softc *sc = ifp->if_softc;
	struct vtnet_rxq *rxq = &sc->vtnet_rxqs[ring_nr];
	struct virtqueue *vq = rxq->vtnrx_vq;

	/* use a local sglist, default might be short */
	struct sglist_seg ss[2];
	struct sglist sg = { ss, 0, 0, 2 };

	for (nm_i = rxq->vtnrx_nm_refill; num > 0;
	    nm_i = nm_next(nm_i, lim), num--) {
		struct netmap_slot *slot = &ring->slot[nm_i];
		uint64_t paddr;
		void *addr = PNMB(na, slot, &paddr);
		int err;

		if (addr == NETMAP_BUF_BASE(na)) { /* bad buf */
			if (netmap_ring_reinit(kring))
				return EFAULT;
		}

		slot->flags &= ~NS_BUF_CHANGED;
		sglist_reset(&sg);
		err = sglist_append(&sg, &rxq->vtnrx_shrhdr, sc->vtnet_hdr_size);
		err |= sglist_append_phys(&sg, paddr, NETMAP_BUF_SIZE(na));
		KASSERT(err == 0, ("%s: cannot append to sglist %d",
					__func__, err));
		/* writable for the host */
		err = virtqueue_enqueue(vq, /*cookie=*/rxq, &sg,
				/*readable=*/0, /*writeable=*/sg.sg_nseg);
		if (unlikely(err)) {
			nm_prerr("virtqueue_enqueue(%s) failed: %d",
				kring->name, err);
			break;
		}
	}
	rxq->vtnrx_nm_refill = nm_i;

	return num == 0 ? 0 : ENOSPC;
}

/*
 * Publish netmap buffers on a RX virtqueue.
 * Returns -1 if this virtqueue is not being opened in netmap mode.
 * If the virtqueue is being opened in netmap mode, return 0 on success and
 * a positive error code on failure.
 */
static int
vtnet_netmap_rxq_populate(struct vtnet_rxq *rxq)
{
	struct netmap_adapter *na = NA(rxq->vtnrx_sc->vtnet_ifp);
	struct netmap_kring *kring;
	struct netmap_slot *slot;
	int error;
	int num;

	slot = netmap_reset(na, NR_RX, rxq->vtnrx_id, 0);
	if (slot == NULL)
		return -1;
	kring = na->rx_rings[rxq->vtnrx_id];

	/*
	 * Expose all the RX netmap buffers we can. In case of no indirect
	 * buffers, the number of netmap slots in the RX ring matches the
	 * maximum number of 2-elements sglist that the RX virtqueue can
	 * accommodate. We need to start from kring->nr_hwtail, which is 0
	 * on the first netmap register and may be different from 0 if a
	 * virtio re-init (caused by a netma register or i.e., ifconfig)
	 * happens while the device is in use by netmap.
	 */
	rxq->vtnrx_nm_refill = kring->nr_hwtail;
	num = na->num_rx_desc - 1 - nm_kr_rxspace(kring);
	error = vtnet_netmap_kring_refill(kring, num);
	virtqueue_notify(rxq->vtnrx_vq);

	return error;
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
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	int force_update = (flags & NAF_FORCE_READ) ||
				(kring->nr_kflags & NKR_PENDINTR);
	int interrupts = !(kring->nr_kflags & NKR_NOINTR);

	/* device-specific */
	struct vtnet_softc *sc = ifp->if_softc;
	struct vtnet_rxq *rxq = &sc->vtnet_rxqs[ring_nr];
	struct virtqueue *vq = rxq->vtnrx_vq;

	/*
	 * First part: import newly received packets.
	 * Only accept our own buffers (matching the token). We should only get
	 * matching buffers. The hwtail should never overrun hwcur, because
	 * we publish only N-1 receive buffers (and not N).
	 * In any case we must not leave this routine with the interrupts
	 * disabled, pending packets in the VQ and hwtail == (hwcur - 1),
	 * otherwise the pending packets could stall.
	 */
	if (netmap_no_pendintr || force_update) {
		uint32_t hwtail_lim = nm_prev(kring->nr_hwcur, lim);
		void *token;

		vtnet_rxq_disable_intr(rxq);

		nm_i = kring->nr_hwtail;
		for (;;) {
			int len;
			token = virtqueue_dequeue(vq, &len);
			if (token == NULL) {
				/*
				 * Enable the interrupts again and double-check
				 * for more work. We can go on until we win the
				 * race condition, since we are not replenishing
				 * in the meanwhile, and thus we will process at
				 * most N-1 slots.
				 */
				if (interrupts && vtnet_rxq_enable_intr(rxq)) {
					vtnet_rxq_disable_intr(rxq);
					continue;
				}
				break;
			}
			if (unlikely(token != (void *)rxq)) {
				nm_prerr("BUG: RX token mismatch");
			} else {
				if (nm_i == hwtail_lim) {
					KASSERT(false, ("hwtail would "
					    "overrun hwcur"));
				}

				/* Skip the virtio-net header. */
				len -= sc->vtnet_hdr_size;
				if (unlikely(len < 0)) {
					nm_prlim(1, "Truncated virtio-net-header, "
						"missing %d bytes", -len);
					len = 0;
				}
				ring->slot[nm_i].len = len;
				ring->slot[nm_i].flags = 0;
				nm_i = nm_next(nm_i, lim);
			}
		}
		kring->nr_hwtail = nm_i;
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	/*
	 * Second part: skip past packets that userspace has released.
	 */
	nm_i = kring->nr_hwcur; /* netmap ring index */
	if (nm_i != head) {
		int released;
		int error;

		released = head - nm_i;
		if (released < 0)
			released += kring->nkr_num_slots;
		error = vtnet_netmap_kring_refill(kring, released);
		if (error) {
			nm_prerr("Failed to replenish RX VQ with %u sgs",
			    released);
			return error;
		}
		kring->nr_hwcur = head;
		virtqueue_notify(vq);
	}

	nm_prdis("h %d c %d t %d hwcur %d hwtail %d", kring->rhead,
	    kring->rcur, kring->rtail, kring->nr_hwcur, kring->nr_hwtail);

	return 0;
}


/* Enable/disable interrupts on all virtqueues. */
static void
vtnet_netmap_intr(struct netmap_adapter *na, int state)
{
	struct vtnet_softc *sc = na->ifp->if_softc;
	int i;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		struct vtnet_rxq *rxq = &sc->vtnet_rxqs[i];
		struct vtnet_txq *txq = &sc->vtnet_txqs[i];
		struct virtqueue *txvq = txq->vtntx_vq;

		if (state) {
			vtnet_rxq_enable_intr(rxq);
			virtqueue_enable_intr(txvq);
		} else {
			vtnet_rxq_disable_intr(rxq);
			virtqueue_disable_intr(txvq);
		}
	}
}

static int
vtnet_netmap_tx_slots(struct vtnet_softc *sc)
{
	int div;

	/* We need to prepend a virtio-net header to each netmap buffer to be
	 * transmitted, therefore calling virtqueue_enqueue() passing sglist
	 * with 2 elements.
	 * TX virtqueues use indirect descriptors if the feature was negotiated
	 * with the host, and if sc->vtnet_tx_nsegs > 1. With indirect
	 * descriptors, a single virtio descriptor is sufficient to reference
	 * each TX sglist. Without them, we need two separate virtio descriptors
	 * for each TX sglist. We therefore compute the number of netmap TX
	 * slots according to these assumptions.
	 */
	if ((sc->vtnet_flags & VTNET_FLAG_INDIRECT) && sc->vtnet_tx_nsegs > 1)
		div = 1;
	else
		div = 2;

	return virtqueue_size(sc->vtnet_txqs[0].vtntx_vq) / div;
}

static int
vtnet_netmap_rx_slots(struct vtnet_softc *sc)
{
	int div;

	/* We need to prepend a virtio-net header to each netmap buffer to be
	 * received, therefore calling virtqueue_enqueue() passing sglist
	 * with 2 elements.
	 * RX virtqueues use indirect descriptors if the feature was negotiated
	 * with the host, and if sc->vtnet_rx_nsegs > 1. With indirect
	 * descriptors, a single virtio descriptor is sufficient to reference
	 * each RX sglist. Without them, we need two separate virtio descriptors
	 * for each RX sglist. We therefore compute the number of netmap RX
	 * slots according to these assumptions.
	 */
	if ((sc->vtnet_flags & VTNET_FLAG_INDIRECT) && sc->vtnet_rx_nsegs > 1)
		div = 1;
	else
		div = 2;

	return virtqueue_size(sc->vtnet_rxqs[0].vtnrx_vq) / div;
}

static int
vtnet_netmap_config(struct netmap_adapter *na, struct nm_config_info *info)
{
	struct vtnet_softc *sc = na->ifp->if_softc;

	info->num_tx_rings = sc->vtnet_act_vq_pairs;
	info->num_rx_rings = sc->vtnet_act_vq_pairs;
	info->num_tx_descs = vtnet_netmap_tx_slots(sc);
	info->num_rx_descs = vtnet_netmap_rx_slots(sc);
	info->rx_buf_maxsize = NETMAP_BUF_SIZE(na);

	return 0;
}

static void
vtnet_netmap_attach(struct vtnet_softc *sc)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = sc->vtnet_ifp;
	na.na_flags = 0;
	na.num_tx_desc = vtnet_netmap_tx_slots(sc);
	na.num_rx_desc = vtnet_netmap_rx_slots(sc);
	na.num_tx_rings = na.num_rx_rings = sc->vtnet_max_vq_pairs;
	na.rx_buf_maxsize = 0;
	na.nm_register = vtnet_netmap_reg;
	na.nm_txsync = vtnet_netmap_txsync;
	na.nm_rxsync = vtnet_netmap_rxsync;
	na.nm_intr = vtnet_netmap_intr;
	na.nm_config = vtnet_netmap_config;

	netmap_attach(&na);

	nm_prinf("vtnet attached txq=%d, txd=%d rxq=%d, rxd=%d",
			na.num_tx_rings, na.num_tx_desc,
			na.num_tx_rings, na.num_rx_desc);
}
/* end of file */
