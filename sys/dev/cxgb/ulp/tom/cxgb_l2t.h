/**************************************************************************

Copyright (c) 2007-2009, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/
#ifndef _CHELSIO_L2T_H
#define _CHELSIO_L2T_H

#include <sys/lock.h>
#include <sys/rwlock.h>

enum {
	L2T_SIZE = 2048
};

enum {
	L2T_STATE_VALID,	/* entry is up to date */
	L2T_STATE_STALE,	/* entry may be used but needs revalidation */
	L2T_STATE_RESOLVING,	/* entry needs address resolution */
	L2T_STATE_FAILED,	/* failed to resolve */
	L2T_STATE_UNUSED	/* entry not in use */
};

/*
 * Each L2T entry plays multiple roles.  First of all, it keeps state for the
 * corresponding entry of the HW L2 table and maintains a queue of offload
 * packets awaiting address resolution.  Second, it is a node of a hash table
 * chain, where the nodes of the chain are linked together through their next
 * pointer.  Finally, each node is a bucket of a hash table, pointing to the
 * first element in its chain through its first pointer.
 */
struct l2t_entry {
	uint16_t state;               /* entry state */
	uint16_t idx;                 /* entry index */
	uint32_t addr;                /* nexthop IP address */
	struct ifnet *ifp;            /* outgoing interface */
	uint16_t smt_idx;             /* SMT index */
	uint16_t vlan;                /* VLAN TCI (id: bits 0-11, prio: 13-15 */
	struct l2t_entry *first;      /* start of hash chain */
	struct l2t_entry *next;       /* next l2t_entry on chain */
	struct mbuf *arpq_head;       /* queue of packets awaiting resolution */
	struct mbuf *arpq_tail;
	struct mtx lock;
	volatile uint32_t refcnt;     /* entry reference count */
	uint8_t dmac[ETHER_ADDR_LEN]; /* nexthop's MAC address */
};

struct l2t_data {
	unsigned int nentries;      /* number of entries */
	struct l2t_entry *rover;    /* starting point for next allocation */
	volatile uint32_t nfree;    /* number of free entries */
	struct rwlock lock;
	struct l2t_entry l2tab[0];
};

void t3_l2e_free(struct l2t_data *, struct l2t_entry *e);
void t3_l2_update(struct toedev *tod, struct ifnet *ifp, struct sockaddr *sa,
    uint8_t *lladdr, uint16_t vtag);
struct l2t_entry *t3_l2t_get(struct port_info *, struct ifnet *,
    struct sockaddr *);
int t3_l2t_send_slow(struct adapter *, struct mbuf *, struct l2t_entry *);
struct l2t_data *t3_init_l2t(unsigned int);
void t3_free_l2t(struct l2t_data *);
void t3_init_l2t_cpl_handlers(struct adapter *);

static inline int
l2t_send(struct adapter *sc, struct mbuf *m, struct l2t_entry *e)
{
	if (__predict_true(e->state == L2T_STATE_VALID))
		return t3_offload_tx(sc, m);
	else
		return t3_l2t_send_slow(sc, m, e);
}

static inline void
l2t_release(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, -1) == 1) /* 1 -> 0 transition */
		atomic_add_int(&d->nfree, 1);
}

static inline void
l2t_hold(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, 1) == 0)  /* 0 -> 1 transition */
		atomic_add_int(&d->nfree, -1);
}

#endif
