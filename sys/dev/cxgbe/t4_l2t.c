/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sbuf.h>
#include <netinet/in.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "t4_l2t.h"

/*
 * Module locking notes:  There is a RW lock protecting the L2 table as a
 * whole plus a spinlock per L2T entry.  Entry lookups and allocations happen
 * under the protection of the table lock, individual entry changes happen
 * while holding that entry's spinlock.  The table lock nests outside the
 * entry locks.  Allocations of new entries take the table lock as writers so
 * no other lookups can happen while allocating new entries.  Entry updates
 * take the table lock as readers so multiple entries can be updated in
 * parallel.  An L2T entry can be dropped by decrementing its reference count
 * and therefore can happen in parallel with entry allocation but no entry
 * can change state or increment its ref count during allocation as both of
 * these perform lookups.
 *
 * Note: We do not take references to ifnets in this module because both
 * the TOE and the sockets already hold references to the interfaces and the
 * lifetime of an L2T entry is fully contained in the lifetime of the TOE.
 */

/*
 * Allocate a free L2T entry.  Must be called with l2t_data.lock held.
 */
struct l2t_entry *
t4_alloc_l2e(struct l2t_data *d)
{
	struct l2t_entry *end, *e, **p;

	rw_assert(&d->lock, RA_WLOCKED);

	if (!atomic_load_acq_int(&d->nfree))
		return (NULL);

	/* there's definitely a free entry */
	for (e = d->rover, end = &d->l2tab[d->l2t_size]; e != end; ++e)
		if (atomic_load_acq_int(&e->refcnt) == 0)
			goto found;

	for (e = d->l2tab; atomic_load_acq_int(&e->refcnt); ++e)
		continue;
found:
	d->rover = e + 1;
	atomic_subtract_int(&d->nfree, 1);

	/*
	 * The entry we found may be an inactive entry that is
	 * presently in the hash table.  We need to remove it.
	 */
	if (e->state < L2T_STATE_SWITCHING) {
		for (p = &d->l2tab[e->hash].first; *p; p = &(*p)->next) {
			if (*p == e) {
				*p = e->next;
				e->next = NULL;
				break;
			}
		}
	}

	e->state = L2T_STATE_UNUSED;
	return (e);
}

static struct l2t_entry *
find_or_alloc_l2e(struct l2t_data *d, uint16_t vlan, uint8_t port, uint8_t *dmac)
{
	struct l2t_entry *end, *e, **p;
	struct l2t_entry *first_free = NULL;

	for (e = &d->l2tab[0], end = &d->l2tab[d->l2t_size]; e != end; ++e) {
		if (atomic_load_acq_int(&e->refcnt) == 0) {
			if (!first_free)
				first_free = e;
		} else if (e->state == L2T_STATE_SWITCHING &&
		    memcmp(e->dmac, dmac, ETHER_ADDR_LEN) == 0 &&
		    e->vlan == vlan && e->lport == port)
			return (e);	/* Found existing entry that matches. */
	}

	if (first_free == NULL)
		return (NULL);	/* No match and no room for a new entry. */

	/*
	 * The entry we found may be an inactive entry that is
	 * presently in the hash table.  We need to remove it.
	 */
	e = first_free;
	if (e->state < L2T_STATE_SWITCHING) {
		for (p = &d->l2tab[e->hash].first; *p; p = &(*p)->next) {
			if (*p == e) {
				*p = e->next;
				e->next = NULL;
				break;
			}
		}
	}
	e->state = L2T_STATE_UNUSED;
	return (e);
}

static void
mk_write_l2e(struct adapter *sc, struct l2t_entry *e, int sync, int reply,
    void *dst)
{
	struct cpl_l2t_write_req *req;
	int idx;

	req = dst;
	idx = e->idx + sc->vres.l2t.start;
	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_L2T_WRITE_REQ, idx |
	    V_SYNC_WR(sync) | V_TID_QID(e->iqid)));
	req->params = htons(V_L2T_W_PORT(e->lport) | V_L2T_W_NOREPLY(!reply));
	req->l2t_idx = htons(idx);
	req->vlan = htons(e->vlan);
	memcpy(req->dst_mac, e->dmac, sizeof(req->dst_mac));
}

/*
 * Write an L2T entry.  Must be called with the entry locked.
 * The write may be synchronous or asynchronous.
 */
int
t4_write_l2e(struct l2t_entry *e, int sync)
{
	struct sge_wrq *wrq;
	struct adapter *sc;
	struct wrq_cookie cookie;
	struct cpl_l2t_write_req *req;

	mtx_assert(&e->lock, MA_OWNED);
	MPASS(e->wrq != NULL);

	wrq = e->wrq;
	sc = wrq->adapter;

	req = start_wrq_wr(wrq, howmany(sizeof(*req), 16), &cookie);
	if (req == NULL)
		return (ENOMEM);

	mk_write_l2e(sc, e, sync, sync, req);

	commit_wrq_wr(wrq, req, &cookie);

	if (sync && e->state != L2T_STATE_SWITCHING)
		e->state = L2T_STATE_SYNC_WRITE;

	return (0);
}

/*
 * Allocate an L2T entry for use by a TLS connection.  These entries are
 * associated with a specific VLAN and destination MAC that never changes.
 * However, multiple TLS connections might share a single entry.
 *
 * If a new L2T entry is allocated, a work request to initialize it is
 * written to 'txq' and 'ndesc' will be set to 1.  Otherwise, 'ndesc'
 * will be set to 0.
 *
 * To avoid races, separate L2T entries are reserved for individual
 * queues since the L2T entry update is written to a txq just prior to
 * TLS work requests that will depend on it being written.
 */
struct l2t_entry *
t4_l2t_alloc_tls(struct adapter *sc, struct sge_txq *txq, void *dst,
    int *ndesc, uint16_t vlan, uint8_t port, uint8_t *eth_addr)
{
	struct l2t_data *d;
	struct l2t_entry *e;
	int i;

	TXQ_LOCK_ASSERT_OWNED(txq);

	d = sc->l2t;
	*ndesc = 0;

	rw_rlock(&d->lock);

	/* First, try to find an existing entry. */
	for (i = 0; i < d->l2t_size; i++) {
		e = &d->l2tab[i];
		if (e->state != L2T_STATE_TLS)
			continue;
		if (e->vlan == vlan && e->lport == port &&
		    e->wrq == (struct sge_wrq *)txq &&
		    memcmp(e->dmac, eth_addr, ETHER_ADDR_LEN) == 0) {
			if (atomic_fetchadd_int(&e->refcnt, 1) == 0) {
				/*
				 * This entry wasn't held but is still
				 * valid, so decrement nfree.
				 */
				atomic_subtract_int(&d->nfree, 1);
			}
			KASSERT(e->refcnt > 0,
			    ("%s: refcount overflow", __func__));
			rw_runlock(&d->lock);
			return (e);
		}
	}

	/*
	 * Don't bother rechecking if the upgrade fails since the txq is
	 * already locked.
	 */
	if (!rw_try_upgrade(&d->lock)) {
		rw_runlock(&d->lock);
		rw_wlock(&d->lock);
	}

	/* Match not found, allocate a new entry. */
	e = t4_alloc_l2e(d);
	if (e == NULL) {
		rw_wunlock(&d->lock);
		return (e);
	}

	/* Initialize the entry. */
	e->state = L2T_STATE_TLS;
	e->vlan = vlan;
	e->lport = port;
	e->iqid = sc->sge.fwq.abs_id;
	e->wrq = (struct sge_wrq *)txq;
	memcpy(e->dmac, eth_addr, ETHER_ADDR_LEN);
	atomic_store_rel_int(&e->refcnt, 1);
	rw_wunlock(&d->lock);

	/* Write out the work request. */
	*ndesc = howmany(sizeof(struct cpl_l2t_write_req), EQ_ESIZE);
	MPASS(*ndesc == 1);
	mk_write_l2e(sc, e, 1, 0, dst);

	return (e);
}

/*
 * Allocate an L2T entry for use by a switching rule.  Such need to be
 * explicitly freed and while busy they are not on any hash chain, so normal
 * address resolution updates do not see them.
 */
struct l2t_entry *
t4_l2t_alloc_switching(struct adapter *sc, uint16_t vlan, uint8_t port,
    uint8_t *eth_addr)
{
	struct l2t_data *d = sc->l2t;
	struct l2t_entry *e;
	int rc;

	rw_wlock(&d->lock);
	e = find_or_alloc_l2e(d, vlan, port, eth_addr);
	if (e) {
		if (atomic_load_acq_int(&e->refcnt) == 0) {
			mtx_lock(&e->lock);    /* avoid race with t4_l2t_free */
			e->wrq = &sc->sge.ctrlq[0];
			e->iqid = sc->sge.fwq.abs_id;
			e->state = L2T_STATE_SWITCHING;
			e->vlan = vlan;
			e->lport = port;
			memcpy(e->dmac, eth_addr, ETHER_ADDR_LEN);
			atomic_store_rel_int(&e->refcnt, 1);
			atomic_subtract_int(&d->nfree, 1);
			rc = t4_write_l2e(e, 0);
			mtx_unlock(&e->lock);
			if (rc != 0)
				e = NULL;
		} else {
			MPASS(e->vlan == vlan);
			MPASS(e->lport == port);
			atomic_add_int(&e->refcnt, 1);
		}
	}
	rw_wunlock(&d->lock);
	return (e);
}

int
t4_init_l2t(struct adapter *sc, int flags)
{
	int i, l2t_size;
	struct l2t_data *d;

	l2t_size = sc->vres.l2t.size;
	if (l2t_size < 2)	/* At least 1 bucket for IP and 1 for IPv6 */
		return (EINVAL);

	d = malloc(sizeof(*d) + l2t_size * sizeof (struct l2t_entry), M_CXGBE,
	    M_ZERO | flags);
	if (!d)
		return (ENOMEM);

	d->l2t_size = l2t_size;
	d->rover = d->l2tab;
	atomic_store_rel_int(&d->nfree, l2t_size);
	rw_init(&d->lock, "L2T");

	for (i = 0; i < l2t_size; i++) {
		struct l2t_entry *e = &d->l2tab[i];

		e->idx = i;
		e->state = L2T_STATE_UNUSED;
		mtx_init(&e->lock, "L2T_E", NULL, MTX_DEF);
		STAILQ_INIT(&e->wr_list);
		atomic_store_rel_int(&e->refcnt, 0);
	}

	sc->l2t = d;

	return (0);
}

int
t4_free_l2t(struct l2t_data *d)
{
	int i;

	for (i = 0; i < d->l2t_size; i++)
		mtx_destroy(&d->l2tab[i].lock);
	rw_destroy(&d->lock);
	free(d, M_CXGBE);

	return (0);
}

int
do_l2t_write_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_l2t_write_rpl *rpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(rpl);
	unsigned int idx = tid % L2T_SIZE;

	if (__predict_false(rpl->status != CPL_ERR_NONE)) {
		log(LOG_ERR,
		    "Unexpected L2T_WRITE_RPL (%u) for entry at hw_idx %u\n",
		    rpl->status, idx);
		return (EINVAL);
	}

	return (0);
}

static inline unsigned int
vlan_prio(const struct l2t_entry *e)
{
	return e->vlan >> 13;
}

static char
l2e_state(const struct l2t_entry *e)
{
	switch (e->state) {
	case L2T_STATE_VALID: return 'V';  /* valid, fast-path entry */
	case L2T_STATE_STALE: return 'S';  /* needs revalidation, but usable */
	case L2T_STATE_SYNC_WRITE: return 'W';
	case L2T_STATE_RESOLVING: return STAILQ_EMPTY(&e->wr_list) ? 'R' : 'A';
	case L2T_STATE_SWITCHING: return 'X';
	case L2T_STATE_TLS: return 'T';
	default: return 'U';
	}
}

int
sysctl_l2t(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct l2t_data *l2t = sc->l2t;
	struct l2t_entry *e;
	struct sbuf *sb;
	int rc, i, header = 0;
	char ip[INET6_ADDRSTRLEN];

	if (l2t == NULL)
		return (ENXIO);

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	e = &l2t->l2tab[0];
	for (i = 0; i < l2t->l2t_size; i++, e++) {
		mtx_lock(&e->lock);
		if (e->state == L2T_STATE_UNUSED)
			goto skip;

		if (header == 0) {
			sbuf_printf(sb, " Idx IP address      "
			    "Ethernet address  VLAN/P LP State Users Port");
			header = 1;
		}
		if (e->state >= L2T_STATE_SWITCHING)
			ip[0] = 0;
		else {
			inet_ntop(e->ipv6 ? AF_INET6 : AF_INET, &e->addr[0],
			    &ip[0], sizeof(ip));
		}

		/*
		 * XXX: IPv6 addresses may not align properly in the output.
		 */
		sbuf_printf(sb, "\n%4u %-15s %02x:%02x:%02x:%02x:%02x:%02x %4d"
			   " %u %2u   %c   %5u %s",
			   e->idx, ip, e->dmac[0], e->dmac[1], e->dmac[2],
			   e->dmac[3], e->dmac[4], e->dmac[5],
			   e->vlan & 0xfff, vlan_prio(e), e->lport,
			   l2e_state(e), atomic_load_acq_int(&e->refcnt),
			   e->ifp ? if_name(e->ifp) : "-");
skip:
		mtx_unlock(&e->lock);
	}

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}
