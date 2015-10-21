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

/* $FreeBSD$ */

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

#ifdef WITH_PIPES

#define NM_PIPE_MAXSLOTS	4096

int netmap_default_pipes = 0; /* ignored, kept for compatibility */
SYSCTL_DECL(_dev_netmap);
SYSCTL_INT(_dev_netmap, OID_AUTO, default_pipes, CTLFLAG_RW, &netmap_default_pipes, 0 , "");

/* allocate the pipe array in the parent adapter */
static int
nm_pipe_alloc(struct netmap_adapter *na, u_int npipes)
{
	size_t len;
	struct netmap_pipe_adapter **npa;

	if (npipes <= na->na_max_pipes)
		/* we already have more entries that requested */
		return 0;
	
	if (npipes < na->na_next_pipe || npipes > NM_MAXPIPES)
		return EINVAL;

        len = sizeof(struct netmap_pipe_adapter *) * npipes;
	npa = realloc(na->na_pipes, len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (npa == NULL)
		return ENOMEM;

	na->na_pipes = npa;
	na->na_max_pipes = npipes;

	return 0;
}

/* deallocate the parent array in the parent adapter */
void
netmap_pipe_dealloc(struct netmap_adapter *na)
{
	if (na->na_pipes) {
		if (na->na_next_pipe > 0) {
			D("freeing not empty pipe array for %s (%d dangling pipes)!", na->name,
					na->na_next_pipe);
		}
		free(na->na_pipes, M_DEVBUF);
		na->na_pipes = NULL;
		na->na_max_pipes = 0;
		na->na_next_pipe = 0;
	}
}

/* find a pipe endpoint with the given id among the parent's pipes */
static struct netmap_pipe_adapter *
netmap_pipe_find(struct netmap_adapter *parent, u_int pipe_id)
{
	int i;
	struct netmap_pipe_adapter *na;

	for (i = 0; i < parent->na_next_pipe; i++) {
		na = parent->na_pipes[i];
		if (na->id == pipe_id) {
			return na;
		}
	}
	return NULL;
}

/* add a new pipe endpoint to the parent array */
static int
netmap_pipe_add(struct netmap_adapter *parent, struct netmap_pipe_adapter *na)
{
	if (parent->na_next_pipe >= parent->na_max_pipes) {
		u_int npipes = parent->na_max_pipes ?  2*parent->na_max_pipes : 2;
		int error = nm_pipe_alloc(parent, npipes);
		if (error)
			return error;
	}

	parent->na_pipes[parent->na_next_pipe] = na;
	na->parent_slot = parent->na_next_pipe;
	parent->na_next_pipe++;
	return 0;
}

/* remove the given pipe endpoint from the parent array */
static void
netmap_pipe_remove(struct netmap_adapter *parent, struct netmap_pipe_adapter *na)
{
	u_int n;
	n = --parent->na_next_pipe;
	if (n != na->parent_slot) {
		struct netmap_pipe_adapter **p =
			&parent->na_pipes[na->parent_slot];
		*p = parent->na_pipes[n];
		(*p)->parent_slot = na->parent_slot;
	}
	parent->na_pipes[n] = NULL;
}

static int
netmap_pipe_txsync(struct netmap_kring *txkring, int flags)
{
        struct netmap_kring *rxkring = txkring->pipe;
        u_int limit; /* slots to transfer */
        u_int j, k, lim_tx = txkring->nkr_num_slots - 1,
                lim_rx = rxkring->nkr_num_slots - 1;
        int m, busy;

        ND("%p: %s %x -> %s", txkring, txkring->name, flags, rxkring->name);
        ND(2, "before: hwcur %d hwtail %d cur %d head %d tail %d", txkring->nr_hwcur, txkring->nr_hwtail,
                txkring->rcur, txkring->rhead, txkring->rtail);

        j = rxkring->nr_hwtail; /* RX */
        k = txkring->nr_hwcur;  /* TX */
        m = txkring->rhead - txkring->nr_hwcur; /* new slots */
        if (m < 0)
                m += txkring->nkr_num_slots;
        limit = m;
        m = lim_rx; /* max avail space on destination */
        busy = j - rxkring->nr_hwcur; /* busy slots */
	if (busy < 0)
		busy += rxkring->nkr_num_slots;
	m -= busy; /* subtract busy slots */
        ND(2, "m %d limit %d", m, limit);
        if (m < limit)
                limit = m;

	if (limit == 0) {
		/* either the rxring is full, or nothing to send */
		return 0;
	}

        while (limit-- > 0) {
                struct netmap_slot *rs = &rxkring->save_ring->slot[j];
                struct netmap_slot *ts = &txkring->ring->slot[k];
                struct netmap_slot tmp;

                /* swap the slots */
                tmp = *rs;
                *rs = *ts;
                *ts = tmp;

                /* report the buffer change */
		ts->flags |= NS_BUF_CHANGED;
		rs->flags |= NS_BUF_CHANGED;

                j = nm_next(j, lim_rx);
                k = nm_next(k, lim_tx);
        }

        mb(); /* make sure the slots are updated before publishing them */
        rxkring->nr_hwtail = j;
        txkring->nr_hwcur = k;
        txkring->nr_hwtail = nm_prev(k, lim_tx);

        ND(2, "after: hwcur %d hwtail %d cur %d head %d tail %d j %d", txkring->nr_hwcur, txkring->nr_hwtail,
                txkring->rcur, txkring->rhead, txkring->rtail, j);

        mb(); /* make sure rxkring->nr_hwtail is updated before notifying */
        rxkring->nm_notify(rxkring, 0);

	return 0;
}

static int
netmap_pipe_rxsync(struct netmap_kring *rxkring, int flags)
{
        struct netmap_kring *txkring = rxkring->pipe;
	uint32_t oldhwcur = rxkring->nr_hwcur;

        ND("%s %x <- %s", rxkring->name, flags, txkring->name);
        rxkring->nr_hwcur = rxkring->rhead; /* recover user-relased slots */
        ND(5, "hwcur %d hwtail %d cur %d head %d tail %d", rxkring->nr_hwcur, rxkring->nr_hwtail,
                rxkring->rcur, rxkring->rhead, rxkring->rtail);
        mb(); /* paired with the first mb() in txsync */

	if (oldhwcur != rxkring->nr_hwcur) {
		/* we have released some slots, notify the other end */
		mb(); /* make sure nr_hwcur is updated before notifying */
		txkring->nm_notify(txkring, 0);
	}
        return 0;
}

/* Pipe endpoints are created and destroyed together, so that endopoints do not
 * have to check for the existence of their peer at each ?xsync.
 *
 * To play well with the existing netmap infrastructure (refcounts etc.), we
 * adopt the following strategy:
 *
 * 1) The first endpoint that is created also creates the other endpoint and
 * grabs a reference to it.
 *
 *    state A)  user1 --> endpoint1 --> endpoint2
 *
 * 2) If, starting from state A, endpoint2 is then registered, endpoint1 gives
 * its reference to the user:
 *
 *    state B)  user1 --> endpoint1     endpoint2 <--- user2
 *
 * 3) Assume that, starting from state B endpoint2 is closed. In the unregister
 * callback endpoint2 notes that endpoint1 is still active and adds a reference
 * from endpoint1 to itself. When user2 then releases her own reference,
 * endpoint2 is not destroyed and we are back to state A. A symmetrical state
 * would be reached if endpoint1 were released instead.
 *
 * 4) If, starting from state A, endpoint1 is closed, the destructor notes that
 * it owns a reference to endpoint2 and releases it.
 *
 * Something similar goes on for the creation and destruction of the krings.
 */


/* netmap_pipe_krings_delete.
 *
 * There are two cases:
 *
 * 1) state is
 *
 *        usr1 --> e1 --> e2
 *
 *    and we are e1. We have to create both sets
 *    of krings.
 *
 * 2) state is
 *
 *        usr1 --> e1 --> e2
 *
 *    and we are e2. e1 is certainly registered and our
 *    krings already exist, but they may be hidden.
 */
static int
netmap_pipe_krings_create(struct netmap_adapter *na)
{
	struct netmap_pipe_adapter *pna =
		(struct netmap_pipe_adapter *)na;
	struct netmap_adapter *ona = &pna->peer->up;
	int error = 0;
	enum txrx t;

	if (pna->peer_ref) {
		int i;

		/* case 1) above */
		ND("%p: case 1, create everything", na);
		error = netmap_krings_create(na, 0);
		if (error)
			goto err;

		/* we also create all the rings, since we need to
                 * update the save_ring pointers.
                 * netmap_mem_rings_create (called by our caller)
                 * will not create the rings again
                 */

		error = netmap_mem_rings_create(na);
		if (error)
			goto del_krings1;

		/* update our hidden ring pointers */
		for_rx_tx(t) {
			for (i = 0; i < nma_get_nrings(na, t) + 1; i++)
				NMR(na, t)[i].save_ring = NMR(na, t)[i].ring;
		}

		/* now, create krings and rings of the other end */
		error = netmap_krings_create(ona, 0);
		if (error)
			goto del_rings1;

		error = netmap_mem_rings_create(ona);
		if (error)
			goto del_krings2;

		for_rx_tx(t) {
			for (i = 0; i < nma_get_nrings(ona, t) + 1; i++)
				NMR(ona, t)[i].save_ring = NMR(ona, t)[i].ring;
		}

		/* cross link the krings */
		for_rx_tx(t) {
			enum txrx r= nm_txrx_swap(t); /* swap NR_TX <-> NR_RX */
			for (i = 0; i < nma_get_nrings(na, t); i++) {
				NMR(na, t)[i].pipe = NMR(&pna->peer->up, r) + i;
				NMR(&pna->peer->up, r)[i].pipe = NMR(na, t) + i;
			}
		}
	} else {
		int i;
		/* case 2) above */
		/* recover the hidden rings */
		ND("%p: case 2, hidden rings", na);
		for_rx_tx(t) {
			for (i = 0; i < nma_get_nrings(na, t) + 1; i++)
				NMR(na, t)[i].ring = NMR(na, t)[i].save_ring;
		}
	}
	return 0;

del_krings2:
	netmap_krings_delete(ona);
del_rings1:
	netmap_mem_rings_delete(na);
del_krings1:
	netmap_krings_delete(na);
err:
	return error;
}

/* netmap_pipe_reg.
 *
 * There are two cases on registration (onoff==1)
 *
 * 1.a) state is
 *
 *        usr1 --> e1 --> e2
 *
 *      and we are e1. Nothing special to do.
 *
 * 1.b) state is
 *
 *        usr1 --> e1 --> e2 <-- usr2
 *
 *      and we are e2. Drop the ref e1 is holding.
 *
 *  There are two additional cases on unregister (onoff==0)
 *
 *  2.a) state is
 *
 *         usr1 --> e1 --> e2
 *
 *       and we are e1. Nothing special to do, e2 will
 *       be cleaned up by the destructor of e1.
 *
 *  2.b) state is
 *
 *         usr1 --> e1     e2 <-- usr2
 *
 *       and we are either e1 or e2. Add a ref from the
 *       other end and hide our rings.
 */
static int
netmap_pipe_reg(struct netmap_adapter *na, int onoff)
{
	struct netmap_pipe_adapter *pna =
		(struct netmap_pipe_adapter *)na;
	enum txrx t;

	ND("%p: onoff %d", na, onoff);
	if (onoff) {
		na->na_flags |= NAF_NETMAP_ON;
	} else {
		na->na_flags &= ~NAF_NETMAP_ON;
	}
	if (pna->peer_ref) {
		ND("%p: case 1.a or 2.a, nothing to do", na);
		return 0;
	}
	if (onoff) {
		ND("%p: case 1.b, drop peer", na);
		pna->peer->peer_ref = 0;
		netmap_adapter_put(na);
	} else {
		int i;
		ND("%p: case 2.b, grab peer", na);
		netmap_adapter_get(na);
		pna->peer->peer_ref = 1;
		/* hide our rings from netmap_mem_rings_delete */
		for_rx_tx(t) {
			for (i = 0; i < nma_get_nrings(na, t) + 1; i++) {
				NMR(na, t)[i].ring = NULL;
			}
		}
	}
	return 0;
}

/* netmap_pipe_krings_delete.
 *
 * There are two cases:
 *
 * 1) state is
 *
 *                usr1 --> e1 --> e2
 *
 *    and we are e1 (e2 is not registered, so krings_delete cannot be
 *    called on it);
 *
 * 2) state is
 *
 *                usr1 --> e1     e2 <-- usr2
 *
 *    and we are either e1 or e2.
 *
 * In the former case we have to also delete the krings of e2;
 * in the latter case we do nothing (note that our krings
 * have already been hidden in the unregister callback).
 */
static void
netmap_pipe_krings_delete(struct netmap_adapter *na)
{
	struct netmap_pipe_adapter *pna =
		(struct netmap_pipe_adapter *)na;
	struct netmap_adapter *ona; /* na of the other end */
	int i;
	enum txrx t;

	if (!pna->peer_ref) {
		ND("%p: case 2, kept alive by peer",  na);
		return;
	}
	/* case 1) above */
	ND("%p: case 1, deleting everyhing", na);
	netmap_krings_delete(na); /* also zeroes tx_rings etc. */
	/* restore the ring to be deleted on the peer */
	ona = &pna->peer->up;
	if (ona->tx_rings == NULL) {
		/* already deleted, we must be on an
                 * cleanup-after-error path */
		return;
	}
	for_rx_tx(t) {
		for (i = 0; i < nma_get_nrings(ona, t) + 1; i++)
			NMR(ona, t)[i].ring = NMR(ona, t)[i].save_ring;
	}
	netmap_mem_rings_delete(ona);
	netmap_krings_delete(ona);
}


static void
netmap_pipe_dtor(struct netmap_adapter *na)
{
	struct netmap_pipe_adapter *pna =
		(struct netmap_pipe_adapter *)na;
	ND("%p", na);
	if (pna->peer_ref) {
		ND("%p: clean up peer", na);
		pna->peer_ref = 0;
		netmap_adapter_put(&pna->peer->up);
	}
	if (pna->role == NR_REG_PIPE_MASTER)
		netmap_pipe_remove(pna->parent, pna);
	netmap_adapter_put(pna->parent);
	pna->parent = NULL;
}

int
netmap_get_pipe_na(struct nmreq *nmr, struct netmap_adapter **na, int create)
{
	struct nmreq pnmr;
	struct netmap_adapter *pna; /* parent adapter */
	struct netmap_pipe_adapter *mna, *sna, *req;
	u_int pipe_id;
	int role = nmr->nr_flags & NR_REG_MASK;
	int error;

	ND("flags %x", nmr->nr_flags);

	if (role != NR_REG_PIPE_MASTER && role != NR_REG_PIPE_SLAVE) {
		ND("not a pipe");
		return 0;
	}
	role = nmr->nr_flags & NR_REG_MASK;

	/* first, try to find the parent adapter */
	bzero(&pnmr, sizeof(pnmr));
	memcpy(&pnmr.nr_name, nmr->nr_name, IFNAMSIZ);
	/* pass to parent the requested number of pipes */
	pnmr.nr_arg1 = nmr->nr_arg1;
	error = netmap_get_na(&pnmr, &pna, create);
	if (error) {
		ND("parent lookup failed: %d", error);
		return error;
	}
	ND("found parent: %s", na->name);

	if (NETMAP_OWNED_BY_KERN(pna)) {
		ND("parent busy");
		error = EBUSY;
		goto put_out;
	}

	/* next, lookup the pipe id in the parent list */
	req = NULL;
	pipe_id = nmr->nr_ringid & NETMAP_RING_MASK;
	mna = netmap_pipe_find(pna, pipe_id);
	if (mna) {
		if (mna->role == role) {
			ND("found %d directly at %d", pipe_id, mna->parent_slot);
			req = mna;
		} else {
			ND("found %d indirectly at %d", pipe_id, mna->parent_slot);
			req = mna->peer;
		}
		/* the pipe we have found already holds a ref to the parent,
                 * so we need to drop the one we got from netmap_get_na()
                 */
		netmap_adapter_put(pna);
		goto found;
	}
	ND("pipe %d not found, create %d", pipe_id, create);
	if (!create) {
		error = ENODEV;
		goto put_out;
	}
	/* we create both master and slave.
         * The endpoint we were asked for holds a reference to
         * the other one.
         */
	mna = malloc(sizeof(*mna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mna == NULL) {
		error = ENOMEM;
		goto put_out;
	}
	snprintf(mna->up.name, sizeof(mna->up.name), "%s{%d", pna->name, pipe_id);

	mna->id = pipe_id;
	mna->role = NR_REG_PIPE_MASTER;
	mna->parent = pna;

	mna->up.nm_txsync = netmap_pipe_txsync;
	mna->up.nm_rxsync = netmap_pipe_rxsync;
	mna->up.nm_register = netmap_pipe_reg;
	mna->up.nm_dtor = netmap_pipe_dtor;
	mna->up.nm_krings_create = netmap_pipe_krings_create;
	mna->up.nm_krings_delete = netmap_pipe_krings_delete;
	mna->up.nm_mem = pna->nm_mem;
	mna->up.na_lut = pna->na_lut;

	mna->up.num_tx_rings = 1;
	mna->up.num_rx_rings = 1;
	mna->up.num_tx_desc = nmr->nr_tx_slots;
	nm_bound_var(&mna->up.num_tx_desc, pna->num_tx_desc,
			1, NM_PIPE_MAXSLOTS, NULL);
	mna->up.num_rx_desc = nmr->nr_rx_slots;
	nm_bound_var(&mna->up.num_rx_desc, pna->num_rx_desc,
			1, NM_PIPE_MAXSLOTS, NULL);
	error = netmap_attach_common(&mna->up);
	if (error)
		goto free_mna;
	/* register the master with the parent */
	error = netmap_pipe_add(pna, mna);
	if (error)
		goto free_mna;

	/* create the slave */
	sna = malloc(sizeof(*mna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sna == NULL) {
		error = ENOMEM;
		goto unregister_mna;
	}
	/* most fields are the same, copy from master and then fix */
	*sna = *mna;
	snprintf(sna->up.name, sizeof(sna->up.name), "%s}%d", pna->name, pipe_id);
	sna->role = NR_REG_PIPE_SLAVE;
	error = netmap_attach_common(&sna->up);
	if (error)
		goto free_sna;

	/* join the two endpoints */
	mna->peer = sna;
	sna->peer = mna;

	/* we already have a reference to the parent, but we
         * need another one for the other endpoint we created
         */
	netmap_adapter_get(pna);

	if (role == NR_REG_PIPE_MASTER) {
		req = mna;
		mna->peer_ref = 1;
		netmap_adapter_get(&sna->up);
	} else {
		req = sna;
		sna->peer_ref = 1;
		netmap_adapter_get(&mna->up);
	}
	ND("created master %p and slave %p", mna, sna);
found:

	ND("pipe %d %s at %p", pipe_id,
		(req->role == NR_REG_PIPE_MASTER ? "master" : "slave"), req);
	*na = &req->up;
	netmap_adapter_get(*na);

	/* write the configuration back */
	nmr->nr_tx_rings = req->up.num_tx_rings;
	nmr->nr_rx_rings = req->up.num_rx_rings;
	nmr->nr_tx_slots = req->up.num_tx_desc;
	nmr->nr_rx_slots = req->up.num_rx_desc;

	/* keep the reference to the parent.
         * It will be released by the req destructor
         */

	return 0;

free_sna:
	free(sna, M_DEVBUF);
unregister_mna:
	netmap_pipe_remove(pna, mna);
free_mna:
	free(mna, M_DEVBUF);
put_out:
	netmap_adapter_put(pna);
	return error;
}


#endif /* WITH_PIPES */
