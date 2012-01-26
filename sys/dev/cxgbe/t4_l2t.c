/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sbuf.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <net/if_dl.h>
#include <net/if_llatbl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

#include "common/common.h"
#include "common/jhash.h"
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
 * Note: We do not take refereces to ifnets in this module because both
 * the TOE and the sockets already hold references to the interfaces and the
 * lifetime of an L2T entry is fully contained in the lifetime of the TOE.
 */

/* identifies sync vs async L2T_WRITE_REQs */
#define S_SYNC_WR    12
#define V_SYNC_WR(x) ((x) << S_SYNC_WR)
#define F_SYNC_WR    V_SYNC_WR(1)

enum {
	L2T_STATE_VALID,	/* entry is up to date */
	L2T_STATE_STALE,	/* entry may be used but needs revalidation */
	L2T_STATE_RESOLVING,	/* entry needs address resolution */
	L2T_STATE_SYNC_WRITE,	/* synchronous write of entry underway */

	/* when state is one of the below the entry is not hashed */
	L2T_STATE_SWITCHING,	/* entry is being used by a switching filter */
	L2T_STATE_UNUSED	/* entry not in use */
};

struct l2t_data {
	struct rwlock lock;
	volatile int nfree;	/* number of free entries */
	struct l2t_entry *rover;/* starting point for next allocation */
	struct l2t_entry l2tab[L2T_SIZE];
};

static int do_l2t_write_rpl(struct sge_iq *, const struct rss_header *,
    struct mbuf *);

#define VLAN_NONE	0xfff
#define SA(x)           ((struct sockaddr *)(x))
#define SIN(x)          ((struct sockaddr_in *)(x))
#define SINADDR(x)      (SIN(x)->sin_addr.s_addr)

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
	for (e = d->rover, end = &d->l2tab[L2T_SIZE]; e != end; ++e)
		if (atomic_load_acq_int(&e->refcnt) == 0)
			goto found;

	for (e = d->l2tab; atomic_load_acq_int(&e->refcnt); ++e) ;
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

/*
 * Write an L2T entry.  Must be called with the entry locked.
 * The write may be synchronous or asynchronous.
 */
static int
write_l2e(struct adapter *sc, struct l2t_entry *e, int sync)
{
	struct mbuf *m;
	struct cpl_l2t_write_req *req;

	mtx_assert(&e->lock, MA_OWNED);

	if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
		return (ENOMEM);

	req = mtod(m, struct cpl_l2t_write_req *);
	m->m_pkthdr.len = m->m_len = sizeof(*req);

	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_L2T_WRITE_REQ, e->idx |
	    V_SYNC_WR(sync) | V_TID_QID(sc->sge.fwq.abs_id)));
	req->params = htons(V_L2T_W_PORT(e->lport) | V_L2T_W_NOREPLY(!sync));
	req->l2t_idx = htons(e->idx);
	req->vlan = htons(e->vlan);
	memcpy(req->dst_mac, e->dmac, sizeof(req->dst_mac));

	t4_mgmt_tx(sc, m);

	if (sync && e->state != L2T_STATE_SWITCHING)
		e->state = L2T_STATE_SYNC_WRITE;

	return (0);
}

/*
 * Allocate an L2T entry for use by a switching rule.  Such need to be
 * explicitly freed and while busy they are not on any hash chain, so normal
 * address resolution updates do not see them.
 */
struct l2t_entry *
t4_l2t_alloc_switching(struct l2t_data *d)
{
	struct l2t_entry *e;

	rw_rlock(&d->lock);
	e = alloc_l2e(d);
	if (e) {
		mtx_lock(&e->lock);          /* avoid race with t4_l2t_free */
		e->state = L2T_STATE_SWITCHING;
		atomic_store_rel_int(&e->refcnt, 1);
		mtx_unlock(&e->lock);
	}
	rw_runlock(&d->lock);
	return e;
}

/*
 * Sets/updates the contents of a switching L2T entry that has been allocated
 * with an earlier call to @t4_l2t_alloc_switching.
 */
int
t4_l2t_set_switching(struct adapter *sc, struct l2t_entry *e, uint16_t vlan,
    uint8_t port, uint8_t *eth_addr)
{
	int rc;

	e->vlan = vlan;
	e->lport = port;
	memcpy(e->dmac, eth_addr, ETHER_ADDR_LEN);
	mtx_lock(&e->lock);
	rc = write_l2e(sc, e, 0);
	mtx_unlock(&e->lock);
	return (rc);
}

int
t4_init_l2t(struct adapter *sc, int flags)
{
	int i;
	struct l2t_data *d;

	d = malloc(sizeof(*d), M_CXGBE, M_ZERO | flags);
	if (!d)
		return (ENOMEM);

	d->rover = d->l2tab;
	atomic_store_rel_int(&d->nfree, L2T_SIZE);
	rw_init(&d->lock, "L2T");

	for (i = 0; i < L2T_SIZE; i++) {
		d->l2tab[i].idx = i;
		d->l2tab[i].state = L2T_STATE_UNUSED;
		mtx_init(&d->l2tab[i].lock, "L2T_E", NULL, MTX_DEF);
		atomic_store_rel_int(&d->l2tab[i].refcnt, 0);
	}

	sc->l2t = d;
	t4_register_cpl_handler(sc, CPL_L2T_WRITE_RPL, do_l2t_write_rpl);

	return (0);
}

int
t4_free_l2t(struct l2t_data *d)
{
	int i;

	for (i = 0; i < L2T_SIZE; i++)
		mtx_destroy(&d->l2tab[i].lock);
	rw_destroy(&d->lock);
	free(d, M_CXGBE);

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
	case L2T_STATE_RESOLVING: return e->arpq_head ? 'A' : 'R';
	case L2T_STATE_SWITCHING: return 'X';
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
	char ip[60];

	if (l2t == NULL)
		return (ENXIO);

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	e = &l2t->l2tab[0];
	for (i = 0; i < L2T_SIZE; i++, e++) {
		mtx_lock(&e->lock);
		if (e->state == L2T_STATE_UNUSED)
			goto skip;

		if (header == 0) {
			sbuf_printf(sb, " Idx IP address      "
			    "Ethernet address  VLAN/P LP State Users Port");
			header = 1;
		}
		if (e->state == L2T_STATE_SWITCHING || e->v6)
			ip[0] = 0;
		else
			snprintf(ip, sizeof(ip), "%s",
			    inet_ntoa(*(struct in_addr *)&e->addr[0]));

		/* XXX: accessing lle probably not safe? */
		sbuf_printf(sb, "\n%4u %-15s %02x:%02x:%02x:%02x:%02x:%02x %4d"
			   " %u %2u   %c   %5u %s",
			   e->idx, ip, e->dmac[0], e->dmac[1], e->dmac[2],
			   e->dmac[3], e->dmac[4], e->dmac[5],
			   e->vlan & 0xfff, vlan_prio(e), e->lport,
			   l2e_state(e), atomic_load_acq_int(&e->refcnt),
			   e->lle ? e->lle->lle_tbl->llt_ifp->if_xname : "");
skip:
		mtx_unlock(&e->lock);
	}

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

#ifndef TCP_OFFLOAD_DISABLE
static inline void
l2t_hold(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, 1) == 0)  /* 0 -> 1 transition */
		atomic_subtract_int(&d->nfree, 1);
}

/*
 * To avoid having to check address families we do not allow v4 and v6
 * neighbors to be on the same hash chain.  We keep v4 entries in the first
 * half of available hash buckets and v6 in the second.
 */
enum {
	L2T_SZ_HALF = L2T_SIZE / 2,
	L2T_HASH_MASK = L2T_SZ_HALF - 1
};

static inline unsigned int
arp_hash(const uint32_t *key, int ifindex)
{
	return jhash_2words(*key, ifindex, 0) & L2T_HASH_MASK;
}

static inline unsigned int
ipv6_hash(const uint32_t *key, int ifindex)
{
	uint32_t xor = key[0] ^ key[1] ^ key[2] ^ key[3];

	return L2T_SZ_HALF + (jhash_2words(xor, ifindex, 0) & L2T_HASH_MASK);
}

static inline unsigned int
addr_hash(const uint32_t *addr, int addr_len, int ifindex)
{
	return addr_len == 4 ? arp_hash(addr, ifindex) :
			       ipv6_hash(addr, ifindex);
}

/*
 * Checks if an L2T entry is for the given IP/IPv6 address.  It does not check
 * whether the L2T entry and the address are of the same address family.
 * Callers ensure an address is only checked against L2T entries of the same
 * family, something made trivial by the separation of IP and IPv6 hash chains
 * mentioned above.  Returns 0 if there's a match,
 */
static inline int
addreq(const struct l2t_entry *e, const uint32_t *addr)
{
	if (e->v6)
		return (e->addr[0] ^ addr[0]) | (e->addr[1] ^ addr[1]) |
		       (e->addr[2] ^ addr[2]) | (e->addr[3] ^ addr[3]);
	return e->addr[0] ^ addr[0];
}

/*
 * Add a packet to an L2T entry's queue of packets awaiting resolution.
 * Must be called with the entry's lock held.
 */
static inline void
arpq_enqueue(struct l2t_entry *e, struct mbuf *m)
{
	mtx_assert(&e->lock, MA_OWNED);

	KASSERT(m->m_nextpkt == NULL, ("%s: m_nextpkt not NULL", __func__));
	if (e->arpq_head)
		e->arpq_tail->m_nextpkt = m;
	else
		e->arpq_head = m;
	e->arpq_tail = m;
}

static inline void
send_pending(struct adapter *sc, struct l2t_entry *e)
{
	struct mbuf *m, *next;

	mtx_assert(&e->lock, MA_OWNED);

	for (m = e->arpq_head; m; m = next) {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		t4_wrq_tx(sc, MBUF_EQ(m), m);
	}
	e->arpq_head = e->arpq_tail = NULL;
}

#ifdef INET
/*
 * Looks up and fills up an l2t_entry's lle.  We grab all the locks that we need
 * ourself, and update e->state at the end if e->lle was successfully filled.
 *
 * The lle passed in comes from arpresolve and is ignored as it does not appear
 * to be of much use.
 */
static int
l2t_fill_lle(struct adapter *sc, struct l2t_entry *e, struct llentry *unused)
{
        int rc = 0;
        struct sockaddr_in sin;
        struct ifnet *ifp = e->ifp;
        struct llentry *lle;

        bzero(&sin, sizeof(struct sockaddr_in));
	if (e->v6)
		panic("%s: IPv6 L2 resolution not supported yet.", __func__);

	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, e->addr, sizeof(struct sockaddr_in));

        mtx_assert(&e->lock, MA_NOTOWNED);
        KASSERT(e->addr && ifp, ("%s: bad prep before call", __func__));

        IF_AFDATA_LOCK(ifp);
        lle = lla_lookup(LLTABLE(ifp), LLE_EXCLUSIVE, SA(&sin));
        IF_AFDATA_UNLOCK(ifp);
        if (!LLE_IS_VALID(lle))
                return (ENOMEM);
        if (!(lle->la_flags & LLE_VALID)) {
                rc = EINVAL;
                goto done;
        }

        LLE_ADDREF(lle);

        mtx_lock(&e->lock);
        if (e->state == L2T_STATE_RESOLVING) {
                KASSERT(e->lle == NULL, ("%s: lle already valid", __func__));
                e->lle = lle;
                memcpy(e->dmac, &lle->ll_addr, ETHER_ADDR_LEN);
		write_l2e(sc, e, 1);
        } else {
                KASSERT(e->lle == lle, ("%s: lle changed", __func__));
                LLE_REMREF(lle);
        }
        mtx_unlock(&e->lock);
done:
        LLE_WUNLOCK(lle);
        return (rc);
}
#endif

int
t4_l2t_send(struct adapter *sc, struct mbuf *m, struct l2t_entry *e)
{
#ifndef INET
	return (EINVAL);
#else
	struct llentry *lle = NULL;
	struct sockaddr_in sin;
	struct ifnet *ifp = e->ifp;

	if (e->v6)
		panic("%s: IPv6 L2 resolution not supported yet.", __func__);

        bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, e->addr, sizeof(struct sockaddr_in));

again:
	switch (e->state) {
	case L2T_STATE_STALE:     /* entry is stale, kick off revalidation */
		if (arpresolve(ifp, NULL, NULL, SA(&sin), e->dmac, &lle) == 0)
			l2t_fill_lle(sc, e, lle);

		/* Fall through */

	case L2T_STATE_VALID:     /* fast-path, send the packet on */
		return t4_wrq_tx(sc, MBUF_EQ(m), m);

	case L2T_STATE_RESOLVING:
	case L2T_STATE_SYNC_WRITE:
		mtx_lock(&e->lock);
		if (e->state != L2T_STATE_SYNC_WRITE &&
		    e->state != L2T_STATE_RESOLVING) {
			/* state changed by the time we got here */
			mtx_unlock(&e->lock);
			goto again;
		}
		arpq_enqueue(e, m);
		mtx_unlock(&e->lock);

		if (e->state == L2T_STATE_RESOLVING &&
		    arpresolve(ifp, NULL, NULL, SA(&sin), e->dmac, &lle) == 0)
			l2t_fill_lle(sc, e, lle);
	}

	return (0);
#endif
}

/*
 * Called when an L2T entry has no more users.  The entry is left in the hash
 * table since it is likely to be reused but we also bump nfree to indicate
 * that the entry can be reallocated for a different neighbor.  We also drop
 * the existing neighbor reference in case the neighbor is going away and is
 * waiting on our reference.
 *
 * Because entries can be reallocated to other neighbors once their ref count
 * drops to 0 we need to take the entry's lock to avoid races with a new
 * incarnation.
 */
static void
t4_l2e_free(struct l2t_entry *e)
{
	struct llentry *lle = NULL;
	struct l2t_data *d;

	mtx_lock(&e->lock);
	if (atomic_load_acq_int(&e->refcnt) == 0) {  /* hasn't been recycled */
		lle = e->lle;
		e->lle = NULL;
		/*
		 * Don't need to worry about the arpq, an L2T entry can't be
		 * released if any packets are waiting for resolution as we
		 * need to be able to communicate with the device to close a
		 * connection.
		 */
	}
	mtx_unlock(&e->lock);

	d = container_of(e, struct l2t_data, l2tab[e->idx]);
	atomic_add_int(&d->nfree, 1);

	if (lle)
		LLE_FREE(lle);
}

void
t4_l2t_release(struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, -1) == 1)
		t4_l2e_free(e);
}

static int
do_l2t_write_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_l2t_write_rpl *rpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(rpl);
	unsigned int idx = tid & (L2T_SIZE - 1);

	if (__predict_false(rpl->status != CPL_ERR_NONE)) {
		log(LOG_ERR,
		    "Unexpected L2T_WRITE_RPL status %u for entry %u\n",
		    rpl->status, idx);
		return (EINVAL);
	}

	if (tid & F_SYNC_WR) {
		struct l2t_entry *e = &sc->l2t->l2tab[idx];

		mtx_lock(&e->lock);
		if (e->state != L2T_STATE_SWITCHING) {
			send_pending(sc, e);
			e->state = L2T_STATE_VALID;
		}
		mtx_unlock(&e->lock);
	}

	return (0);
}

/*
 * Reuse an L2T entry that was previously used for the same next hop.
 */
static void
reuse_entry(struct l2t_entry *e)
{
	struct llentry *lle;

	mtx_lock(&e->lock);                /* avoid race with t4_l2t_free */
	lle = e->lle;
	if (lle) {
		KASSERT(lle->la_flags & LLE_VALID,
			("%s: invalid lle stored in l2t_entry", __func__));

		if (lle->la_expire >= time_uptime)
			e->state = L2T_STATE_STALE;
		else
			e->state = L2T_STATE_VALID;
	} else
		e->state = L2T_STATE_RESOLVING;
	mtx_unlock(&e->lock);
}

/*
 * The TOE wants an L2 table entry that it can use to reach the next hop over
 * the specified port.  Produce such an entry - create one if needed.
 *
 * Note that the ifnet could be a pseudo-device like if_vlan, if_lagg, etc. on
 * top of the real cxgbe interface.
 */
struct l2t_entry *
t4_l2t_get(struct port_info *pi, struct ifnet *ifp, struct sockaddr *sa)
{
	struct l2t_entry *e;
	struct l2t_data *d = pi->adapter->l2t;
	int addr_len;
	uint32_t *addr;
	int hash;
	struct sockaddr_in6 *sin6;
	unsigned int smt_idx = pi->port_id;

	if (sa->sa_family == AF_INET) {
		addr = (uint32_t *)&SINADDR(sa);
		addr_len = sizeof(SINADDR(sa));
	} else if (sa->sa_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)sa;
		addr = (uint32_t *)&sin6->sin6_addr.s6_addr;
		addr_len = sizeof(sin6->sin6_addr.s6_addr);
	} else
		return (NULL);

	hash = addr_hash(addr, addr_len, ifp->if_index);

	rw_wlock(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next) {
		if (!addreq(e, addr) && e->ifp == ifp && e->smt_idx == smt_idx){
			l2t_hold(d, e);
			if (atomic_load_acq_int(&e->refcnt) == 1)
				reuse_entry(e);
			goto done;
		}
	}

	/* Need to allocate a new entry */
	e = alloc_l2e(d);
	if (e) {
		mtx_lock(&e->lock);          /* avoid race with t4_l2t_free */
		e->state = L2T_STATE_RESOLVING;
		memcpy(e->addr, addr, addr_len);
		e->ifindex = ifp->if_index;
		e->smt_idx = smt_idx;
		e->ifp = ifp;
		e->hash = hash;
		e->lport = pi->lport;
		e->v6 = (addr_len == 16);
		e->lle = NULL;
		atomic_store_rel_int(&e->refcnt, 1);
		if (ifp->if_type == IFT_L2VLAN)
			VLAN_TAG(ifp, &e->vlan);
		else
			e->vlan = VLAN_NONE;
		e->next = d->l2tab[hash].first;
		d->l2tab[hash].first = e;
		mtx_unlock(&e->lock);
	}
done:
	rw_wunlock(&d->lock);
	return e;
}

/*
 * Called when the host's neighbor layer makes a change to some entry that is
 * loaded into the HW L2 table.
 */
void
t4_l2t_update(struct adapter *sc, struct llentry *lle)
{
	struct l2t_entry *e;
	struct l2t_data *d = sc->l2t;
	struct sockaddr *sa = L3_ADDR(lle);
	struct llentry *old_lle = NULL;
	uint32_t *addr = (uint32_t *)&SINADDR(sa);
	struct ifnet *ifp = lle->lle_tbl->llt_ifp;
	int hash = addr_hash(addr, sizeof(*addr), ifp->if_index);

	KASSERT(d != NULL, ("%s: no L2 table", __func__));
	LLE_WLOCK_ASSERT(lle);
	KASSERT(lle->la_flags & LLE_VALID || lle->la_flags & LLE_DELETED,
	    ("%s: entry neither valid nor deleted.", __func__));

	rw_rlock(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next) {
		if (!addreq(e, addr) && e->ifp == ifp) {
			mtx_lock(&e->lock);
			if (atomic_load_acq_int(&e->refcnt))
				goto found;
			e->state = L2T_STATE_STALE;
			mtx_unlock(&e->lock);
			break;
		}
	}
	rw_runlock(&d->lock);

	/* The TOE has no interest in this LLE */
	return;

 found:
	rw_runlock(&d->lock);

        if (atomic_load_acq_int(&e->refcnt)) {

                /* Entry is referenced by at least 1 offloaded connection. */

                /* Handle deletes first */
                if (lle->la_flags & LLE_DELETED) {
                        if (lle == e->lle) {
                                e->lle = NULL;
                                e->state = L2T_STATE_RESOLVING;
                                LLE_REMREF(lle);
                        }
                        goto done;
                }

                if (lle != e->lle) {
                        old_lle = e->lle;
                        LLE_ADDREF(lle);
                        e->lle = lle;
                }

                if (e->state == L2T_STATE_RESOLVING ||
                    memcmp(e->dmac, &lle->ll_addr, ETHER_ADDR_LEN)) {

                        /* unresolved -> resolved; or dmac changed */

                        memcpy(e->dmac, &lle->ll_addr, ETHER_ADDR_LEN);
			write_l2e(sc, e, 1);
                } else {

                        /* +ve reinforcement of a valid or stale entry */

                }

                e->state = L2T_STATE_VALID;

        } else {
                /*
                 * Entry was used previously but is unreferenced right now.
                 * e->lle has been released and NULL'd out by t4_l2t_free, or
                 * l2t_release is about to call t4_l2t_free and do that.
                 *
                 * Either way this is of no interest to us.
                 */
        }

done:
        mtx_unlock(&e->lock);
        if (old_lle)
                LLE_FREE(old_lle);
}

#endif
