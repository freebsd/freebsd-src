/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_FIREWIRE_FW_NET_H_
#define _DEV_FIREWIRE_FW_NET_H_

/*
 * Allocate mbuf clusters for isochronous receive bulk transfers
 * and insert them into the free queue.
 */
static __inline void
fw_net_init_iso_chunks(struct fw_xferq *xferq)
{
	struct mbuf *m;
	int i;

	for (i = 0; i < xferq->bnchunk; i++) {
		m = m_getcl(M_WAITOK, MT_DATA, M_PKTHDR);
		xferq->bulkxfer[i].mbuf = m;
		m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
		STAILQ_INSERT_TAIL(&xferq->stfree, &xferq->bulkxfer[i], link);
	}
}

/*
 * Allocate a single TX xfer with speed, callback, and softc pre-filled.
 */
static __inline struct fw_xfer *
fw_net_alloc_txfer(struct firewire_comm *fc, int speed,
    void *sc, void (*hand)(struct fw_xfer *),
    struct malloc_type *mtype)
{
	struct fw_xfer *xfer;

	xfer = fw_xfer_alloc(mtype);
	if (xfer == NULL)
		return (NULL);
	xfer->send.spd = speed;
	xfer->fc = fc;
	xfer->sc = (caddr_t)sc;
	xfer->hand = hand;
	return (xfer);
}

/*
 * Free all xfers on a STAILQ list.
 */
#define	FW_NET_FREE_XFERLIST(head) do {					\
	struct fw_xfer *_xfer, *_next;					\
	for (_xfer = STAILQ_FIRST(head); _xfer != NULL;		\
	    _xfer = _next) {						\
		_next = STAILQ_NEXT(_xfer, link);			\
		fw_xfer_free(_xfer);					\
	}								\
} while (0)

/*
 * Drain the send queue, counting each dropped packet as an output error.
 */
static __inline void
fw_net_drain_sendq(if_t ifp)
{
	struct mbuf *m;

	do {
		m = if_dequeue(ifp);
		if (m != NULL)
			m_freem(m);
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	} while (m != NULL);
}

/*
 * Handle SIOCSIFCAP for DEVICE_POLLING.
 * Returns 0 if handled (caller should return), -1 if not a polling request.
 */
#ifdef DEVICE_POLLING
static __inline int
fw_net_poll_ioctl(if_t ifp, struct ifreq *ifr,
    struct firewire_comm *fc, poll_handler_t *poll_fn)
{
	int error;

	if (ifr->ifr_reqcap & IFCAP_POLLING &&
	    !(if_getcapenable(ifp) & IFCAP_POLLING)) {
		error = ether_poll_register(poll_fn, ifp);
		if (error)
			return (error);
		/* Disable interrupts */
		fc->set_intr(fc, 0);
		if_setcapenablebit(ifp, IFCAP_POLLING, 0);
		return (0);
	}
	if (!(ifr->ifr_reqcap & IFCAP_POLLING) &&
	    if_getcapenable(ifp) & IFCAP_POLLING) {
		error = ether_poll_deregister(ifp);
		/* Enable interrupts. */
		fc->set_intr(fc, 1);
		if_setcapenablebit(ifp, 0, IFCAP_POLLING);
		return (error);
	}
	return (-1);
}
#endif /* DEVICE_POLLING */

#endif /* !_DEV_FIREWIRE_FW_NET_H_ */
