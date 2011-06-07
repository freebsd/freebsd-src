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
#include <net/if.h>
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
#include "offload.h"
#include "t4_l2t.h"

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
static inline unsigned int
vlan_prio(const struct l2t_entry *e)
{
	return e->vlan >> 13;
}

static inline void
l2t_hold(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, 1) == 0)  /* 0 -> 1 transition */
		atomic_add_int(&d->nfree, -1);
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
 * Write an L2T entry.  Must be called with the entry locked (XXX: really?).
 * The write may be synchronous or asynchronous.
 */
static int
write_l2e(struct adapter *sc, struct l2t_entry *e, int sync)
{
	struct mbuf *m;
	struct cpl_l2t_write_req *req;

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
 * Add a packet to an L2T entry's queue of packets awaiting resolution.
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
	atomic_add_int(&d->nfree, -1);

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
	return e;
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
	e->vlan = vlan;
	e->lport = port;
	memcpy(e->dmac, eth_addr, ETHER_ADDR_LEN);
	return write_l2e(sc, e, 0);
}

struct l2t_data *
t4_init_l2t(int flags)
{
	int i;
	struct l2t_data *d;

	d = malloc(sizeof(*d), M_CXGBE, M_ZERO | flags);
	if (!d)
		return (NULL);

	d->rover = d->l2tab;
	atomic_store_rel_int(&d->nfree, L2T_SIZE);
	rw_init(&d->lock, "L2T");

	for (i = 0; i < L2T_SIZE; i++) {
		d->l2tab[i].idx = i;
		d->l2tab[i].state = L2T_STATE_UNUSED;
		mtx_init(&d->l2tab[i].lock, "L2T_E", NULL, MTX_DEF);
		atomic_store_rel_int(&d->l2tab[i].refcnt, 0);
	}

	return (d);
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
