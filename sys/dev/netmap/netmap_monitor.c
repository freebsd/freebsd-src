/*
 * Copyright (C) 2014 Giuseppe Lettieri. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
 * Monitors
 *
 * netmap monitors can be used to do zero-copy monitoring of network traffic
 * on another adapter, when the latter adapter is working in netmap mode.
 *
 * Monitors offer to userspace the same interface as any other netmap port,
 * with as many pairs of netmap rings as the monitored adapter.
 * However, only the rx rings are actually used. Each monitor rx ring receives
 * the traffic transiting on both the tx and rx corresponding rings in the
 * monitored adapter. During registration, the user can choose if she wants
 * to intercept tx only, rx only, or both tx and rx traffic.
 *
 * The monitor only sees the frames after they have been consumed in the
 * monitored adapter:
 *
 *  - For tx traffic, this is after the slots containing the frames have been
 *    marked as free. Note that this may happen at a considerably delay after
 *    frame transmission, since freeing of slots is often done lazily.
 *
 *  - For rx traffic, this is after the consumer on the monitored adapter
 *    has released them. In most cases, the consumer is a userspace
 *    application which may have modified the frame contents.
 *
 * If the monitor is not able to cope with the stream of frames, excess traffic
 * will be dropped.
 *
 * Each ring can be monitored by at most one monitor. This may change in the
 * future, if we implement monitor chaining.
 *
 */


#if defined(__FreeBSD__)
#include <sys/cdefs.h> /* prerequisite */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/socket.h> /* sockaddrs */
#include <net/if.h>
#include <net/if_var.h>
#include <machine/bus.h>	/* bus_dmamap_* */
#include <sys/refcount.h>


#elif defined(linux)

#include "bsd_glue.h"

#elif defined(__APPLE__)

#warning OSX support is only partial
#include "osx_glue.h"

#else

#error	Unsupported platform

#endif /* unsupported */

/*
 * common headers
 */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>

#ifdef WITH_MONITOR

#define NM_MONITOR_MAXSLOTS 4096

/* monitor works by replacing the nm_sync callbacks in the monitored rings.
 * The actions to be performed are the same on both tx and rx rings, so we
 * have collected them here
 */
static int
netmap_monitor_parent_sync(struct netmap_kring *kring, int flags, u_int* ringptr)
{
	struct netmap_monitor_adapter *mna = kring->monitor;
	struct netmap_kring *mkring = &mna->up.rx_rings[kring->ring_id];
	struct netmap_ring *ring = kring->ring, *mring = mkring->ring;
	int error;
	int rel_slots, free_slots, busy;
	u_int beg, end, i;
	u_int lim = kring->nkr_num_slots - 1,
	      mlim = mkring->nkr_num_slots - 1;

	/* get the relased slots (rel_slots) */
	beg = *ringptr;
	error = kring->save_sync(kring, flags);
	if (error)
		return error;
	end = *ringptr;
	rel_slots = end - beg;
	if (rel_slots < 0)
		rel_slots += kring->nkr_num_slots;

	if (!rel_slots) {
		return 0;
	}

	/* we need to lock the monitor receive ring, since it
	 * is the target of bot tx and rx traffic from the monitored
	 * adapter
	 */
	mtx_lock(&mkring->q_lock);
	/* get the free slots available on the monitor ring */
	i = mkring->nr_hwtail;
	busy = i - mkring->nr_hwcur;
	if (busy < 0)
		busy += mkring->nkr_num_slots;
	free_slots = mlim - busy;

	if (!free_slots) {
		mtx_unlock(&mkring->q_lock);
		return 0;
	}

	/* swap min(free_slots, rel_slots) slots */
	if (free_slots < rel_slots) {
		beg += (rel_slots - free_slots);
		if (beg > lim)
			beg = 0;
		rel_slots = free_slots;
	}

	for ( ; rel_slots; rel_slots--) {
		struct netmap_slot *s = &ring->slot[beg];
		struct netmap_slot *ms = &mring->slot[i];
		uint32_t tmp;

		tmp = ms->buf_idx;
		ms->buf_idx = s->buf_idx;
		s->buf_idx = tmp;

		tmp = ms->len;
		ms->len = s->len;
		s->len = tmp;

		s->flags |= NS_BUF_CHANGED;

		beg = nm_next(beg, lim);
		i = nm_next(i, mlim);

	}
	wmb();
	mkring->nr_hwtail = i;

	mtx_unlock(&mkring->q_lock);
	/* notify the new frames to the monitor */
	mna->up.nm_notify(&mna->up, mkring->ring_id, NR_RX, 0);
	return 0;
}

/* callback used to replace the nm_sync callback in the monitored tx rings */
static int
netmap_monitor_parent_txsync(struct netmap_kring *kring, int flags)
{
        ND("%s %x", kring->name, flags);
        return netmap_monitor_parent_sync(kring, flags, &kring->nr_hwtail);
}

/* callback used to replace the nm_sync callback in the monitored rx rings */
static int
netmap_monitor_parent_rxsync(struct netmap_kring *kring, int flags)
{
        ND("%s %x", kring->name, flags);
        return netmap_monitor_parent_sync(kring, flags, &kring->rcur);
}

/* nm_sync callback for the monitor's own tx rings.
 * This makes no sense and always returns error
 */
static int
netmap_monitor_txsync(struct netmap_kring *kring, int flags)
{
        D("%s %x", kring->name, flags);
	return EIO;
}

/* nm_sync callback for the monitor's own rx rings.
 * Note that the lock in netmap_monitor_parent_sync only protects
 * writers among themselves. Synchronization between writers
 * (i.e., netmap_monitor_parent_txsync and netmap_monitor_parent_rxsync)
 * and readers (i.e., netmap_monitor_rxsync) relies on memory barriers.
 */
static int
netmap_monitor_rxsync(struct netmap_kring *kring, int flags)
{
        ND("%s %x", kring->name, flags);
	kring->nr_hwcur = kring->rcur;
	rmb();
	nm_rxsync_finalize(kring);
        return 0;
}

/* nm_krings_create callbacks for monitors.
 * We could use the default netmap_hw_krings_monitor, but
 * we don't need the mbq.
 */
static int
netmap_monitor_krings_create(struct netmap_adapter *na)
{
	return netmap_krings_create(na, 0);
}


/* nm_register callback for monitors.
 *
 * On registration, replace the nm_sync callbacks in the monitored
 * rings with our own, saving the previous ones in the monitored
 * rings themselves, where they are used by netmap_monitor_parent_sync.
 *
 * On de-registration, restore the original callbacks. We need to
 * stop traffic while we are doing this, since the monitored adapter may
 * have already started executing a netmap_monitor_parent_sync
 * and may not like the kring->save_sync pointer to become NULL.
 */
static int
netmap_monitor_reg(struct netmap_adapter *na, int onoff)
{
	struct netmap_monitor_adapter *mna =
		(struct netmap_monitor_adapter *)na;
	struct netmap_priv_d *priv = &mna->priv;
	struct netmap_adapter *pna = priv->np_na;
	struct netmap_kring *kring;
	int i;

	ND("%p: onoff %d", na, onoff);
	if (onoff) {
		if (!nm_netmap_on(pna)) {
			/* parent left netmap mode, fatal */
			return ENXIO;
		}
		if (mna->flags & NR_MONITOR_TX) {
			for (i = priv->np_txqfirst; i < priv->np_txqlast; i++) {
				kring = &pna->tx_rings[i];
				kring->save_sync = kring->nm_sync;
				kring->nm_sync = netmap_monitor_parent_txsync;
			}
		}
		if (mna->flags & NR_MONITOR_RX) {
			for (i = priv->np_rxqfirst; i < priv->np_rxqlast; i++) {
				kring = &pna->rx_rings[i];
				kring->save_sync = kring->nm_sync;
				kring->nm_sync = netmap_monitor_parent_rxsync;
			}
		}
		na->na_flags |= NAF_NETMAP_ON;
	} else {
		if (!nm_netmap_on(pna)) {
			/* parent left netmap mode, nothing to restore */
			return 0;
		}
		na->na_flags &= ~NAF_NETMAP_ON;
		if (mna->flags & NR_MONITOR_TX) {
			for (i = priv->np_txqfirst; i < priv->np_txqlast; i++) {
				netmap_set_txring(pna, i, 1 /* stopped */);
				kring = &pna->tx_rings[i];
				kring->nm_sync = kring->save_sync;
				kring->save_sync = NULL;
				netmap_set_txring(pna, i, 0 /* enabled */);
			}
		}
		if (mna->flags & NR_MONITOR_RX) {
			for (i = priv->np_rxqfirst; i < priv->np_rxqlast; i++) {
				netmap_set_rxring(pna, i, 1 /* stopped */);
				kring = &pna->rx_rings[i];
				kring->nm_sync = kring->save_sync;
				kring->save_sync = NULL;
				netmap_set_rxring(pna, i, 0 /* enabled */);
			}
		}
	}
	return 0;
}
/* nm_krings_delete callback for monitors */
static void
netmap_monitor_krings_delete(struct netmap_adapter *na)
{
	netmap_krings_delete(na);
}


/* nm_dtor callback for monitors */
static void
netmap_monitor_dtor(struct netmap_adapter *na)
{
	struct netmap_monitor_adapter *mna =
		(struct netmap_monitor_adapter *)na;
	struct netmap_priv_d *priv = &mna->priv;
	struct netmap_adapter *pna = priv->np_na;
	int i;

	ND("%p", na);
	if (nm_netmap_on(pna)) {
		/* parent still in netmap mode, mark its krings as free */
		if (mna->flags & NR_MONITOR_TX) {
			for (i = priv->np_txqfirst; i < priv->np_txqlast; i++) {
				pna->tx_rings[i].monitor = NULL;
			}
		}
		if (mna->flags & NR_MONITOR_RX) {
			for (i = priv->np_rxqfirst; i < priv->np_rxqlast; i++) {
				pna->rx_rings[i].monitor = NULL;
			}
		}
	}
	netmap_adapter_put(pna);
}


/* check if nmr is a request for a monitor adapter that we can satisfy */
int
netmap_get_monitor_na(struct nmreq *nmr, struct netmap_adapter **na, int create)
{
	struct nmreq pnmr;
	struct netmap_adapter *pna; /* parent adapter */
	struct netmap_monitor_adapter *mna;
	int i, error;

	if ((nmr->nr_flags & (NR_MONITOR_TX | NR_MONITOR_RX)) == 0) {
		ND("not a monitor");
		return 0;
	}
	/* this is a request for a monitor adapter */

	D("flags %x", nmr->nr_flags);

	mna = malloc(sizeof(*mna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mna == NULL) {
		D("memory error");
		return ENOMEM;
	}

	/* first, try to find the adapter that we want to monitor
	 * We use the same nmr, after we have turned off the monitor flags.
	 * In this way we can potentially monitor everything netmap understands,
	 * except other monitors.
	 */
	memcpy(&pnmr, nmr, sizeof(pnmr));
	pnmr.nr_flags &= ~(NR_MONITOR_TX | NR_MONITOR_RX);
	error = netmap_get_na(&pnmr, &pna, create);
	if (error) {
		D("parent lookup failed: %d", error);
		return error;
	}
	D("found parent: %s", pna->name);

	if (!nm_netmap_on(pna)) {
		/* parent not in netmap mode */
		/* XXX we can wait for the parent to enter netmap mode,
		 * by intercepting its nm_register callback (2014-03-16)
		 */
		D("%s not in netmap mode", pna->name);
		error = EINVAL;
		goto put_out;
	}

	/* grab all the rings we need in the parent */
	mna->priv.np_na = pna;
	error = netmap_interp_ringid(&mna->priv, nmr->nr_ringid, nmr->nr_flags);
	if (error) {
		D("ringid error");
		goto put_out;
	}
	if (nmr->nr_flags & NR_MONITOR_TX) {
		for (i = mna->priv.np_txqfirst; i < mna->priv.np_txqlast; i++) {
			struct netmap_kring *kring = &pna->tx_rings[i];
			if (kring->monitor) {
				error = EBUSY;
				D("ring busy");
				goto release_out;
			}
			kring->monitor = mna;
		}
	}
	if (nmr->nr_flags & NR_MONITOR_RX) {
		for (i = mna->priv.np_rxqfirst; i < mna->priv.np_rxqlast; i++) {
			struct netmap_kring *kring = &pna->rx_rings[i];
			if (kring->monitor) {
				error = EBUSY;
				D("ring busy");
				goto release_out;
			}
			kring->monitor = mna;
		}
	}

	snprintf(mna->up.name, sizeof(mna->up.name), "mon:%s", pna->name);

	/* the monitor supports the host rings iff the parent does */
	mna->up.na_flags = (pna->na_flags & NAF_HOST_RINGS);
	mna->up.nm_txsync = netmap_monitor_txsync;
	mna->up.nm_rxsync = netmap_monitor_rxsync;
	mna->up.nm_register = netmap_monitor_reg;
	mna->up.nm_dtor = netmap_monitor_dtor;
	mna->up.nm_krings_create = netmap_monitor_krings_create;
	mna->up.nm_krings_delete = netmap_monitor_krings_delete;
	mna->up.nm_mem = pna->nm_mem;
	mna->up.na_lut = pna->na_lut;
	mna->up.na_lut_objtotal = pna->na_lut_objtotal;
	mna->up.na_lut_objsize = pna->na_lut_objsize;

	mna->up.num_tx_rings = 1; // XXX we don't need it, but field can't be zero
	/* we set the number of our rx_rings to be max(num_rx_rings, num_rx_rings)
	 * in the parent
	 */
	mna->up.num_rx_rings = pna->num_rx_rings;
	if (pna->num_tx_rings > pna->num_rx_rings)
		mna->up.num_rx_rings = pna->num_tx_rings;
	/* by default, the number of slots is the same as in
	 * the parent rings, but the user may ask for a different
	 * number
	 */
	mna->up.num_tx_desc = nmr->nr_tx_slots;
	nm_bound_var(&mna->up.num_tx_desc, pna->num_tx_desc,
			1, NM_MONITOR_MAXSLOTS, NULL);
	mna->up.num_rx_desc = nmr->nr_rx_slots;
	nm_bound_var(&mna->up.num_rx_desc, pna->num_rx_desc,
			1, NM_MONITOR_MAXSLOTS, NULL);
	error = netmap_attach_common(&mna->up);
	if (error) {
		D("attach_common error");
		goto release_out;
	}

	/* remember the traffic directions we have to monitor */
	mna->flags = (nmr->nr_flags & (NR_MONITOR_TX | NR_MONITOR_RX));

	*na = &mna->up;
	netmap_adapter_get(*na);

	/* write the configuration back */
	nmr->nr_tx_rings = mna->up.num_tx_rings;
	nmr->nr_rx_rings = mna->up.num_rx_rings;
	nmr->nr_tx_slots = mna->up.num_tx_desc;
	nmr->nr_rx_slots = mna->up.num_rx_desc;

	/* keep the reference to the parent */
	D("monitor ok");

	return 0;

release_out:
	D("monitor error");
	for (i = mna->priv.np_txqfirst; i < mna->priv.np_txqlast; i++) {
		if (pna->tx_rings[i].monitor == mna)
			pna->tx_rings[i].monitor = NULL;
	}
	for (i = mna->priv.np_rxqfirst; i < mna->priv.np_rxqlast; i++) {
		if (pna->rx_rings[i].monitor == mna)
			pna->rx_rings[i].monitor = NULL;
	}
put_out:
	netmap_adapter_put(pna);
	free(mna, M_DEVBUF);
	return error;
}


#endif /* WITH_MONITOR */
