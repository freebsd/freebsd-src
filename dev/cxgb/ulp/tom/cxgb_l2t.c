/*-
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

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <netinet/in.h>
#include <netinet/toecore.h>

#include "cxgb_include.h"
#include "ulp/tom/cxgb_tom.h"
#include "ulp/tom/cxgb_l2t.h"

#define VLAN_NONE	0xfff
#define SA(x)		((struct sockaddr *)(x))
#define SIN(x)		((struct sockaddr_in *)(x))
#define SINADDR(x)	(SIN(x)->sin_addr.s_addr)

/*
 * Module locking notes:  There is a RW lock protecting the L2 table as a
 * whole plus a mutex per L2T entry.  Entry lookups and allocations happen
 * under the protection of the table lock, individual entry changes happen
 * while holding that entry's mutex.  The table lock nests outside the
 * entry locks.  Allocations of new entries take the table lock as writers so
 * no other lookups can happen while allocating new entries.  Entry updates
 * take the table lock as readers so multiple entries can be updated in
 * parallel.  An L2T entry can be dropped by decrementing its reference count
 * and therefore can happen in parallel with entry allocation but no entry
 * can change state or increment its ref count during allocation as both of
 * these perform lookups.
 *
 * When acquiring multiple locks, the order is llentry -> L2 table -> L2 entry.
 */

static inline unsigned int
arp_hash(u32 key, int ifindex, const struct l2t_data *d)
{
	return jhash_2words(key, ifindex, 0) & (d->nentries - 1);
}

/*
 * Set up an L2T entry and send any packets waiting in the arp queue.  Must be
 * called with the entry locked.
 */
static int
setup_l2e_send_pending(struct adapter *sc, struct l2t_entry *e)
{
	struct mbuf *m;
	struct cpl_l2t_write_req *req;
	struct port_info *pi = &sc->port[e->smt_idx];	/* smt_idx is port_id */

	mtx_assert(&e->lock, MA_OWNED);

	m = M_GETHDR_OFLD(pi->first_qset, CPL_PRIORITY_CONTROL, req);
	if (m == NULL) {
		log(LOG_ERR, "%s: no mbuf, can't setup L2 entry at index %d\n",
		    __func__, e->idx);
		return (ENOMEM);
	}

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_L2T_WRITE_REQ, e->idx));
	req->params = htonl(V_L2T_W_IDX(e->idx) | V_L2T_W_IFF(e->smt_idx) |
	    V_L2T_W_VLAN(e->vlan & EVL_VLID_MASK) |
	    V_L2T_W_PRIO(EVL_PRIOFTAG(e->vlan)));
	memcpy(req->dst_mac, e->dmac, sizeof(req->dst_mac));

	t3_offload_tx(sc, m);

	/*
	 * XXX: We used pi->first_qset to send the L2T_WRITE_REQ.  If any mbuf
	 * on the arpq is going out via another queue set associated with the
	 * port then it has a bad race with the L2T_WRITE_REQ.  Ideally we
	 * should wait till the reply to the write before draining the arpq.
	 */
	while (e->arpq_head) {
		m = e->arpq_head;
		e->arpq_head = m->m_next;
		m->m_next = NULL;
		t3_offload_tx(sc, m);
	}
	e->arpq_tail = NULL;

	return (0);
}

/*
 * Add a packet to the an L2T entry's queue of packets awaiting resolution.
 * Must be called with the entry's lock held.
 */
static inline void
arpq_enqueue(struct l2t_entry *e, struct mbuf *m)
{
	mtx_assert(&e->lock, MA_OWNED);

	m->m_next = NULL;
	if (e->arpq_head)
		e->arpq_tail->m_next = m;
	else
		e->arpq_head = m;
	e->arpq_tail = m;
}

static void
resolution_failed_mbuf(struct mbuf *m)
{
	log(LOG_ERR, "%s: leaked mbuf %p, CPL at %p",
	    __func__, m, mtod(m, void *));
}

static void
resolution_failed(struct l2t_entry *e)
{
	struct mbuf *m;

	mtx_assert(&e->lock, MA_OWNED);

	while (e->arpq_head) {
		m = e->arpq_head;
		e->arpq_head = m->m_next;
		m->m_next = NULL;
		resolution_failed_mbuf(m);
	}
	e->arpq_tail = NULL;
}

static void
update_entry(struct adapter *sc, struct l2t_entry *e, uint8_t *lladdr,
    uint16_t vtag)
{

	mtx_assert(&e->lock, MA_OWNED);

	/*
	 * The entry may be in active use (e->refcount > 0) or not.  We update
	 * it even when it's not as this simplifies the case where we decide to
	 * reuse the entry later.
	 */

	if (lladdr == NULL &&
	    (e->state == L2T_STATE_RESOLVING || e->state == L2T_STATE_FAILED)) {
		/*
		 * Never got a valid L2 address for this one.  Just mark it as
		 * failed instead of removing it from the hash (for which we'd
		 * need to wlock the table).
		 */
		e->state = L2T_STATE_FAILED;
		resolution_failed(e);
		return;

	} else if (lladdr == NULL) {

		/* Valid or already-stale entry was deleted (or expired) */

		KASSERT(e->state == L2T_STATE_VALID ||
		    e->state == L2T_STATE_STALE,
		    ("%s: lladdr NULL, state %d", __func__, e->state));

		e->state = L2T_STATE_STALE;

	} else {

		if (e->state == L2T_STATE_RESOLVING ||
		    e->state == L2T_STATE_FAILED ||
		    memcmp(e->dmac, lladdr, ETHER_ADDR_LEN)) {

			/* unresolved -> resolved; or dmac changed */

			memcpy(e->dmac, lladdr, ETHER_ADDR_LEN);
			e->vlan = vtag;
			setup_l2e_send_pending(sc, e);
		}
		e->state = L2T_STATE_VALID;
	}
}

static int
resolve_entry(struct adapter *sc, struct l2t_entry *e)
{
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	struct sockaddr_in sin = {0};
	uint8_t dmac[ETHER_ADDR_LEN];
	uint16_t vtag = EVL_VLID_MASK;
	int rc;

	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	SINADDR(&sin) = e->addr;

	rc = toe_l2_resolve(tod, e->ifp, SA(&sin), dmac, &vtag);
	if (rc == EWOULDBLOCK)
		return (rc);

	mtx_lock(&e->lock);
	update_entry(sc, e, rc == 0 ? dmac : NULL, vtag);
	mtx_unlock(&e->lock);

	return (rc);
}

int
t3_l2t_send_slow(struct adapter *sc, struct mbuf *m, struct l2t_entry *e)
{

again:
	switch (e->state) {
	case L2T_STATE_STALE:     /* entry is stale, kick off revalidation */

		if (resolve_entry(sc, e) != EWOULDBLOCK)
			goto again;	/* entry updated, re-examine state */

		/* Fall through */

	case L2T_STATE_VALID:     /* fast-path, send the packet on */

		return (t3_offload_tx(sc, m));

	case L2T_STATE_RESOLVING:
		mtx_lock(&e->lock);
		if (e->state != L2T_STATE_RESOLVING) {
			mtx_unlock(&e->lock);
			goto again;
		}
		arpq_enqueue(e, m);
		mtx_unlock(&e->lock);

		if (resolve_entry(sc, e) == EWOULDBLOCK)
			break;

		mtx_lock(&e->lock);
		if (e->state == L2T_STATE_VALID && e->arpq_head)
			setup_l2e_send_pending(sc, e);
		if (e->state == L2T_STATE_FAILED)
			resolution_failed(e);
		mtx_unlock(&e->lock);
		break;

	case L2T_STATE_FAILED:
		resolution_failed_mbuf(m);
		return (EHOSTUNREACH);
	}

	return (0);
}

/*
 * Allocate a free L2T entry.  Must be called with l2t_data.lock held.
 */
static struct l2t_entry *
alloc_l2e(struct l2t_data *d)
{
	struct l2t_entry *end, *e, **p;

	rw_assert(&d->lock, RA_WLOCKED);

	if (!atomic_load_acq_int(&d->nfree))
		return (NULL);

	/* there's definitely a free entry */
	for (e = d->rover, end = &d->l2tab[d->nentries]; e != end; ++e) {
		if (atomic_load_acq_int(&e->refcnt) == 0)
			goto found;
	}

	for (e = &d->l2tab[1]; atomic_load_acq_int(&e->refcnt); ++e)
		continue;
found:
	d->rover = e + 1;
	atomic_add_int(&d->nfree, -1);

	/*
	 * The entry we found may be an inactive entry that is
	 * presently in the hash table.  We need to remove it.
	 */
	if (e->state != L2T_STATE_UNUSED) {
		int hash = arp_hash(e->addr, e->ifp->if_index, d);

		for (p = &d->l2tab[hash].first; *p; p = &(*p)->next) {
			if (*p == e) {
				*p = e->next;
				break;
			}
		}
		e->state = L2T_STATE_UNUSED;
	}

	return (e);
}

struct l2t_entry *
t3_l2t_get(struct port_info *pi, struct ifnet *ifp, struct sockaddr *sa)
{
	struct tom_data *td = pi->adapter->tom_softc;
	struct l2t_entry *e;
	struct l2t_data *d = td->l2t;
	uint32_t addr = SINADDR(sa);
	int hash = arp_hash(addr, ifp->if_index, d);
	unsigned int smt_idx = pi->port_id;

	rw_wlock(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next) {
		if (e->addr == addr && e->ifp == ifp && e->smt_idx == smt_idx) {
			l2t_hold(d, e);
			goto done;
		}
	}

	/* Need to allocate a new entry */
	e = alloc_l2e(d);
	if (e) {
		mtx_lock(&e->lock);          /* avoid race with t3_l2t_free */
		e->next = d->l2tab[hash].first;
		d->l2tab[hash].first = e;

		e->state = L2T_STATE_RESOLVING;
		e->addr = addr;
		e->ifp = ifp;
		e->smt_idx = smt_idx;
		atomic_store_rel_int(&e->refcnt, 1);

		KASSERT(ifp->if_vlantrunk == NULL, ("TOE+VLAN unimplemented."));
		e->vlan = VLAN_NONE;

		mtx_unlock(&e->lock);
	}

done:
	rw_wunlock(&d->lock);

	return (e);
}

void
t3_l2_update(struct toedev *tod, struct ifnet *ifp, struct sockaddr *sa,
    uint8_t *lladdr, uint16_t vtag)
{
	struct tom_data *td = t3_tomdata(tod);
	struct adapter *sc = tod->tod_softc;
	struct l2t_entry *e;
	struct l2t_data *d = td->l2t;
	u32 addr = *(u32 *) &SIN(sa)->sin_addr;
	int hash = arp_hash(addr, ifp->if_index, d);

	rw_rlock(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next)
		if (e->addr == addr && e->ifp == ifp) {
			mtx_lock(&e->lock);
			goto found;
		}
	rw_runlock(&d->lock);

	/*
	 * This is of no interest to us.  We've never had an offloaded
	 * connection to this destination, and we aren't attempting one right
	 * now.
	 */
	return;

found:
	rw_runlock(&d->lock);

	KASSERT(e->state != L2T_STATE_UNUSED,
	    ("%s: unused entry in the hash.", __func__));

	update_entry(sc, e, lladdr, vtag);
	mtx_unlock(&e->lock);
}

struct l2t_data *
t3_init_l2t(unsigned int l2t_capacity)
{
	struct l2t_data *d;
	int i, size = sizeof(*d) + l2t_capacity * sizeof(struct l2t_entry);

	d = malloc(size, M_CXGB, M_NOWAIT | M_ZERO);
	if (!d)
		return (NULL);

	d->nentries = l2t_capacity;
	d->rover = &d->l2tab[1];	/* entry 0 is not used */
	atomic_store_rel_int(&d->nfree, l2t_capacity - 1);
	rw_init(&d->lock, "L2T");

	for (i = 0; i < l2t_capacity; ++i) {
		d->l2tab[i].idx = i;
		d->l2tab[i].state = L2T_STATE_UNUSED;
		mtx_init(&d->l2tab[i].lock, "L2T_E", NULL, MTX_DEF);
		atomic_store_rel_int(&d->l2tab[i].refcnt, 0);
	}
	return (d);
}

void
t3_free_l2t(struct l2t_data *d)
{
	int i;

	rw_destroy(&d->lock);
	for (i = 0; i < d->nentries; ++i) 
		mtx_destroy(&d->l2tab[i].lock);

	free(d, M_CXGB);
}

static int
do_l2t_write_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct cpl_l2t_write_rpl *rpl = mtod(m, void *);

	if (rpl->status != CPL_ERR_NONE)
		log(LOG_ERR,
		       "Unexpected L2T_WRITE_RPL status %u for entry %u\n",
		       rpl->status, GET_TID(rpl));

	m_freem(m);
	return (0);
}

void
t3_init_l2t_cpl_handlers(struct adapter *sc)
{
	t3_register_cpl_handler(sc, CPL_L2T_WRITE_RPL, do_l2t_write_rpl);
}
#endif
