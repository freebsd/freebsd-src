/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
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

#include <ulp/toecore/cxgb_toedev.h>
#include <sys/lock.h>

#if __FreeBSD_version > 700000
#include <sys/rwlock.h>
#else
#define rwlock mtx
#define rw_wlock(x) mtx_lock((x))
#define rw_wunlock(x) mtx_unlock((x))
#define rw_rlock(x) mtx_lock((x))
#define rw_runlock(x) mtx_unlock((x))
#define rw_init(x, str) mtx_init((x), (str), NULL, MTX_DEF)
#define rw_destroy(x) mtx_destroy((x))
#endif

enum {
	L2T_STATE_VALID,      /* entry is up to date */
	L2T_STATE_STALE,      /* entry may be used but needs revalidation */
	L2T_STATE_RESOLVING,  /* entry needs address resolution */
	L2T_STATE_UNUSED      /* entry not in use */
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
	uint32_t addr;                /* dest IP address */
	int ifindex;                  /* neighbor's net_device's ifindex */
	uint16_t smt_idx;             /* SMT index */
	uint16_t vlan;                /* VLAN TCI (id: bits 0-11, prio: 13-15 */
	struct llentry *neigh;        /* associated neighbour */
	struct l2t_entry *first;      /* start of hash chain */
	struct l2t_entry *next;       /* next l2t_entry on chain */
	struct mbuf *arpq_head;       /* queue of packets awaiting resolution */
	struct mbuf *arpq_tail;
	struct mtx lock;
	volatile uint32_t refcnt;     /* entry reference count */
	uint8_t dmac[6];              /* neighbour's MAC address */
};

struct l2t_data {
	unsigned int nentries;      /* number of entries */
	struct l2t_entry *rover;    /* starting point for next allocation */
	volatile uint32_t nfree;    /* number of free entries */
	struct rwlock lock;
	struct l2t_entry l2tab[0];
};

typedef void (*arp_failure_handler_func)(struct t3cdev *dev,
					 struct mbuf *m);

typedef void (*opaque_arp_failure_handler_func)(void *dev,
					 struct mbuf *m);

/*
 * Callback stored in an skb to handle address resolution failure.
 */
struct l2t_mbuf_cb {
	arp_failure_handler_func arp_failure_handler;
};

/*
 * XXX 
 */
#define L2T_MBUF_CB(skb) ((struct l2t_mbuf_cb *)(skb)->cb)


static __inline void set_arp_failure_handler(struct mbuf *m,
					   arp_failure_handler_func hnd)
{
	m->m_pkthdr.header = (opaque_arp_failure_handler_func)hnd;

}

/*
 * Getting to the L2 data from an offload device.
 */
#define L2DATA(dev) ((dev)->l2opt)

void t3_l2e_free(struct l2t_data *d, struct l2t_entry *e);
void t3_l2t_update(struct t3cdev *dev, struct rtentry *rt, uint8_t *enaddr, struct sockaddr *sa);
struct l2t_entry *t3_l2t_get(struct t3cdev *dev, struct rtentry *neigh,
    struct ifnet *ifp, struct sockaddr *sa);
int t3_l2t_send_slow(struct t3cdev *dev, struct mbuf *m,
		     struct l2t_entry *e);
void t3_l2t_send_event(struct t3cdev *dev, struct l2t_entry *e);
struct l2t_data *t3_init_l2t(unsigned int l2t_capacity);
void t3_free_l2t(struct l2t_data *d);

#ifdef CONFIG_PROC_FS
int t3_l2t_proc_setup(struct proc_dir_entry *dir, struct l2t_data *d);
void t3_l2t_proc_free(struct proc_dir_entry *dir);
#else
#define l2t_proc_setup(dir, d) 0
#define l2t_proc_free(dir)
#endif

int cxgb_ofld_send(struct t3cdev *dev, struct mbuf *m);

static inline int l2t_send(struct t3cdev *dev, struct mbuf *m,
			   struct l2t_entry *e)
{
	if (__predict_true(e->state == L2T_STATE_VALID)) {
		return cxgb_ofld_send(dev, (struct mbuf *)m);
	}
	return t3_l2t_send_slow(dev, (struct mbuf *)m, e);
}

static inline void l2t_release(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, -1) == 1)
		t3_l2e_free(d, e);
}

static inline void l2t_hold(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, 1) == 1)  /* 0 -> 1 transition */
		atomic_add_int(&d->nfree, 1);
}

#endif
