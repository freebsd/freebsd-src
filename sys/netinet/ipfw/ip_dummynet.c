/*-
 * Copyright (c) 1998-2002 Luigi Rizzo, Universita` di Pisa
 * Portions Copyright (c) 2000 Akamba Corp.
 * All rights reserved
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

#define	DUMMYNET_DEBUG

#include "opt_inet6.h"

/*
 * This module implements IP dummynet, a bandwidth limiter/delay emulator
 * used in conjunction with the ipfw package.
 * Description of the data structures used is in ip_dummynet.h
 * Here you mainly find the following blocks of code:
 *  + variable declarations;
 *  + heap management functions;
 *  + scheduler and dummynet functions;
 *  + configuration and initialization.
 *
 * NOTA BENE: critical sections are protected by the "dummynet lock".
 *
 * Most important Changes:
 *
 * 011004: KLDable
 * 010124: Fixed WF2Q behaviour
 * 010122: Fixed spl protection.
 * 000601: WF2Q support
 * 000106: large rewrite, use heaps to handle very many pipes.
 * 980513:	initial release
 *
 * include files marked with XXX are probably not needed
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <net/if.h>	/* IFNAMSIZ, struct ifaddr, ifq head, lock.h mutex.h */
#include <net/netisr.h>
#include <netinet/in.h>
#include <netinet/ip.h>		/* ip_len, ip_off */
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>
#include <netinet/ip_dummynet.h>
#include <netinet/ip_var.h>	/* ip_output(), IP_FORWARDING */

#include <netinet/if_ether.h> /* various ether_* routines */

#include <netinet/ip6.h>       /* for ip6_input, ip6_output prototypes */
#include <netinet6/ip6_var.h>

/*
 * We keep a private variable for the simulation time, but we could
 * probably use an existing one ("softticks" in sys/kern/kern_timeout.c)
 */
static dn_key curr_time = 0 ; /* current simulation time */

static int dn_hash_size = 64 ;	/* default hash size */

/* statistics on number of queue searches and search steps */
static long searches, search_steps ;
static int pipe_expire = 1 ;   /* expire queue if empty */
static int dn_max_ratio = 16 ; /* max queues/buckets ratio */

static long pipe_slot_limit = 100; /* Foot shooting limit for pipe queues. */
static long pipe_byte_limit = 1024 * 1024;

static int red_lookup_depth = 256;	/* RED - default lookup table depth */
static int red_avg_pkt_size = 512;      /* RED - default medium packet size */
static int red_max_pkt_size = 1500;     /* RED - default max packet size */

static struct timeval prev_t, t;
static long tick_last;			/* Last tick duration (usec). */
static long tick_delta;			/* Last vs standard tick diff (usec). */
static long tick_delta_sum;		/* Accumulated tick difference (usec).*/
static long tick_adjustment;		/* Tick adjustments done. */
static long tick_lost;			/* Lost(coalesced) ticks number. */
/* Adjusted vs non-adjusted curr_time difference (ticks). */
static long tick_diff;

static int		io_fast;
static unsigned long	io_pkt;
static unsigned long	io_pkt_fast;
static unsigned long	io_pkt_drop;

/*
 * Three heaps contain queues and pipes that the scheduler handles:
 *
 * ready_heap contains all dn_flow_queue related to fixed-rate pipes.
 *
 * wfq_ready_heap contains the pipes associated with WF2Q flows
 *
 * extract_heap contains pipes associated with delay lines.
 *
 */

MALLOC_DEFINE(M_DUMMYNET, "dummynet", "dummynet heap");

static struct dn_heap ready_heap, extract_heap, wfq_ready_heap ;

static int	heap_init(struct dn_heap *h, int size);
static int	heap_insert (struct dn_heap *h, dn_key key1, void *p);
static void	heap_extract(struct dn_heap *h, void *obj);
static void	transmit_event(struct dn_pipe *pipe, struct mbuf **head,
		    struct mbuf **tail);
static void	ready_event(struct dn_flow_queue *q, struct mbuf **head,
		    struct mbuf **tail);
static void	ready_event_wfq(struct dn_pipe *p, struct mbuf **head,
		    struct mbuf **tail);

#define	HASHSIZE	16
#define	HASH(num)	((((num) >> 8) ^ ((num) >> 4) ^ (num)) & 0x0f)
static struct dn_pipe_head	pipehash[HASHSIZE];	/* all pipes */
static struct dn_flow_set_head	flowsethash[HASHSIZE];	/* all flowsets */

static struct callout dn_timeout;

extern	void (*bridge_dn_p)(struct mbuf *, struct ifnet *);

#ifdef SYSCTL_NODE
SYSCTL_DECL(_net_inet);
SYSCTL_DECL(_net_inet_ip);

SYSCTL_NODE(_net_inet_ip, OID_AUTO, dummynet, CTLFLAG_RW, 0, "Dummynet");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, hash_size,
    CTLFLAG_RW, &dn_hash_size, 0, "Default hash table size");
#if 0	/* curr_time is 64 bit */
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, curr_time,
    CTLFLAG_RD, &curr_time, 0, "Current tick");
#endif
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, ready_heap,
    CTLFLAG_RD, &ready_heap.size, 0, "Size of ready heap");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, extract_heap,
    CTLFLAG_RD, &extract_heap.size, 0, "Size of extract heap");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, searches,
    CTLFLAG_RD, &searches, 0, "Number of queue searches");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, search_steps,
    CTLFLAG_RD, &search_steps, 0, "Number of queue search steps");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, expire,
    CTLFLAG_RW, &pipe_expire, 0, "Expire queue if empty");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, max_chain_len,
    CTLFLAG_RW, &dn_max_ratio, 0,
    "Max ratio between dynamic queues and buckets");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, red_lookup_depth,
    CTLFLAG_RD, &red_lookup_depth, 0, "Depth of RED lookup table");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, red_avg_pkt_size,
    CTLFLAG_RD, &red_avg_pkt_size, 0, "RED Medium packet size");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, red_max_pkt_size,
    CTLFLAG_RD, &red_max_pkt_size, 0, "RED Max packet size");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, tick_delta,
    CTLFLAG_RD, &tick_delta, 0, "Last vs standard tick difference (usec).");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, tick_delta_sum,
    CTLFLAG_RD, &tick_delta_sum, 0, "Accumulated tick difference (usec).");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, tick_adjustment,
    CTLFLAG_RD, &tick_adjustment, 0, "Tick adjustments done.");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, tick_diff,
    CTLFLAG_RD, &tick_diff, 0,
    "Adjusted vs non-adjusted curr_time difference (ticks).");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, tick_lost,
    CTLFLAG_RD, &tick_lost, 0,
    "Number of ticks coalesced by dummynet taskqueue.");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, io_fast,
    CTLFLAG_RW, &io_fast, 0, "Enable fast dummynet io.");
SYSCTL_ULONG(_net_inet_ip_dummynet, OID_AUTO, io_pkt,
    CTLFLAG_RD, &io_pkt, 0,
    "Number of packets passed to dummynet.");
SYSCTL_ULONG(_net_inet_ip_dummynet, OID_AUTO, io_pkt_fast,
    CTLFLAG_RD, &io_pkt_fast, 0,
    "Number of packets bypassed dummynet scheduler.");
SYSCTL_ULONG(_net_inet_ip_dummynet, OID_AUTO, io_pkt_drop,
    CTLFLAG_RD, &io_pkt_drop, 0,
    "Number of packets dropped by dummynet.");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, pipe_slot_limit,
    CTLFLAG_RW, &pipe_slot_limit, 0, "Upper limit in slots for pipe queue.");
SYSCTL_LONG(_net_inet_ip_dummynet, OID_AUTO, pipe_byte_limit,
    CTLFLAG_RW, &pipe_byte_limit, 0, "Upper limit in bytes for pipe queue.");
#endif

#ifdef DUMMYNET_DEBUG
int	dummynet_debug = 0;
#ifdef SYSCTL_NODE
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, debug, CTLFLAG_RW, &dummynet_debug,
	    0, "control debugging printfs");
#endif
#define	DPRINTF(X)	if (dummynet_debug) printf X
#else
#define	DPRINTF(X)
#endif

static struct task	dn_task;
static struct taskqueue	*dn_tq = NULL;
static void dummynet_task(void *, int);

static struct mtx dummynet_mtx;
#define	DUMMYNET_LOCK_INIT() \
	mtx_init(&dummynet_mtx, "dummynet", NULL, MTX_DEF)
#define	DUMMYNET_LOCK_DESTROY()	mtx_destroy(&dummynet_mtx)
#define	DUMMYNET_LOCK()		mtx_lock(&dummynet_mtx)
#define	DUMMYNET_UNLOCK()	mtx_unlock(&dummynet_mtx)
#define	DUMMYNET_LOCK_ASSERT()	mtx_assert(&dummynet_mtx, MA_OWNED)

static int	config_pipe(struct dn_pipe *p);
static int	ip_dn_ctl(struct sockopt *sopt);

static void	dummynet(void *);
static void	dummynet_flush(void);
static void	dummynet_send(struct mbuf *);
void		dummynet_drain(void);
static int	dummynet_io(struct mbuf **, int , struct ip_fw_args *);

/*
 * Flow queue is idle if:
 *   1) it's empty for at least 1 tick
 *   2) it has invalid timestamp (WF2Q case)
 *   3) parent pipe has no 'exhausted' burst.
 */
#define QUEUE_IS_IDLE(q) ((q)->head == NULL && (q)->S == (q)->F + 1 && \
	curr_time > (q)->idle_time + 1 && \
	((q)->numbytes + (curr_time - (q)->idle_time - 1) * \
	(q)->fs->pipe->bandwidth >= (q)->fs->pipe->burst))

/*
 * Heap management functions.
 *
 * In the heap, first node is element 0. Children of i are 2i+1 and 2i+2.
 * Some macros help finding parent/children so we can optimize them.
 *
 * heap_init() is called to expand the heap when needed.
 * Increment size in blocks of 16 entries.
 * XXX failure to allocate a new element is a pretty bad failure
 * as we basically stall a whole queue forever!!
 * Returns 1 on error, 0 on success
 */
#define HEAP_FATHER(x) ( ( (x) - 1 ) / 2 )
#define HEAP_LEFT(x) ( 2*(x) + 1 )
#define HEAP_IS_LEFT(x) ( (x) & 1 )
#define HEAP_RIGHT(x) ( 2*(x) + 2 )
#define	HEAP_SWAP(a, b, buffer) { buffer = a ; a = b ; b = buffer ; }
#define HEAP_INCREMENT	15

static int
heap_init(struct dn_heap *h, int new_size)
{
    struct dn_heap_entry *p;

    if (h->size >= new_size ) {
	printf("dummynet: %s, Bogus call, have %d want %d\n", __func__,
		h->size, new_size);
	return 0 ;
    }
    new_size = (new_size + HEAP_INCREMENT ) & ~HEAP_INCREMENT ;
    p = malloc(new_size * sizeof(*p), M_DUMMYNET, M_NOWAIT);
    if (p == NULL) {
	printf("dummynet: %s, resize %d failed\n", __func__, new_size );
	return 1 ; /* error */
    }
    if (h->size > 0) {
	bcopy(h->p, p, h->size * sizeof(*p) );
	free(h->p, M_DUMMYNET);
    }
    h->p = p ;
    h->size = new_size ;
    return 0 ;
}

/*
 * Insert element in heap. Normally, p != NULL, we insert p in
 * a new position and bubble up. If p == NULL, then the element is
 * already in place, and key is the position where to start the
 * bubble-up.
 * Returns 1 on failure (cannot allocate new heap entry)
 *
 * If offset > 0 the position (index, int) of the element in the heap is
 * also stored in the element itself at the given offset in bytes.
 */
#define SET_OFFSET(heap, node) \
    if (heap->offset > 0) \
	    *((int *)((char *)(heap->p[node].object) + heap->offset)) = node ;
/*
 * RESET_OFFSET is used for sanity checks. It sets offset to an invalid value.
 */
#define RESET_OFFSET(heap, node) \
    if (heap->offset > 0) \
	    *((int *)((char *)(heap->p[node].object) + heap->offset)) = -1 ;
static int
heap_insert(struct dn_heap *h, dn_key key1, void *p)
{
    int son = h->elements ;

    if (p == NULL)	/* data already there, set starting point */
	son = key1 ;
    else {		/* insert new element at the end, possibly resize */
	son = h->elements ;
	if (son == h->size) /* need resize... */
	    if (heap_init(h, h->elements+1) )
		return 1 ; /* failure... */
	h->p[son].object = p ;
	h->p[son].key = key1 ;
	h->elements++ ;
    }
    while (son > 0) {				/* bubble up */
	int father = HEAP_FATHER(son) ;
	struct dn_heap_entry tmp  ;

	if (DN_KEY_LT( h->p[father].key, h->p[son].key ) )
	    break ; /* found right position */
	/* son smaller than father, swap and repeat */
	HEAP_SWAP(h->p[son], h->p[father], tmp) ;
	SET_OFFSET(h, son);
	son = father ;
    }
    SET_OFFSET(h, son);
    return 0 ;
}

/*
 * remove top element from heap, or obj if obj != NULL
 */
static void
heap_extract(struct dn_heap *h, void *obj)
{
    int child, father, max = h->elements - 1 ;

    if (max < 0) {
	printf("dummynet: warning, extract from empty heap 0x%p\n", h);
	return ;
    }
    father = 0 ; /* default: move up smallest child */
    if (obj != NULL) { /* extract specific element, index is at offset */
	if (h->offset <= 0)
	    panic("dummynet: heap_extract from middle not supported on this heap!!!\n");
	father = *((int *)((char *)obj + h->offset)) ;
	if (father < 0 || father >= h->elements) {
	    printf("dummynet: heap_extract, father %d out of bound 0..%d\n",
		father, h->elements);
	    panic("dummynet: heap_extract");
	}
    }
    RESET_OFFSET(h, father);
    child = HEAP_LEFT(father) ;		/* left child */
    while (child <= max) {		/* valid entry */
	if (child != max && DN_KEY_LT(h->p[child+1].key, h->p[child].key) )
	    child = child+1 ;		/* take right child, otherwise left */
	h->p[father] = h->p[child] ;
	SET_OFFSET(h, father);
	father = child ;
	child = HEAP_LEFT(child) ;   /* left child for next loop */
    }
    h->elements-- ;
    if (father != max) {
	/*
	 * Fill hole with last entry and bubble up, reusing the insert code
	 */
	h->p[father] = h->p[max] ;
	heap_insert(h, father, NULL); /* this one cannot fail */
    }
}

#if 0
/*
 * change object position and update references
 * XXX this one is never used!
 */
static void
heap_move(struct dn_heap *h, dn_key new_key, void *object)
{
    int temp;
    int i ;
    int max = h->elements-1 ;
    struct dn_heap_entry buf ;

    if (h->offset <= 0)
	panic("cannot move items on this heap");

    i = *((int *)((char *)object + h->offset));
    if (DN_KEY_LT(new_key, h->p[i].key) ) { /* must move up */
	h->p[i].key = new_key ;
	for (; i>0 && DN_KEY_LT(new_key, h->p[(temp = HEAP_FATHER(i))].key) ;
		 i = temp ) { /* bubble up */
	    HEAP_SWAP(h->p[i], h->p[temp], buf) ;
	    SET_OFFSET(h, i);
	}
    } else {		/* must move down */
	h->p[i].key = new_key ;
	while ( (temp = HEAP_LEFT(i)) <= max ) { /* found left child */
	    if ((temp != max) && DN_KEY_GT(h->p[temp].key, h->p[temp+1].key))
		temp++ ; /* select child with min key */
	    if (DN_KEY_GT(new_key, h->p[temp].key)) { /* go down */
		HEAP_SWAP(h->p[i], h->p[temp], buf) ;
		SET_OFFSET(h, i);
	    } else
		break ;
	    i = temp ;
	}
    }
    SET_OFFSET(h, i);
}
#endif /* heap_move, unused */

/*
 * heapify() will reorganize data inside an array to maintain the
 * heap property. It is needed when we delete a bunch of entries.
 */
static void
heapify(struct dn_heap *h)
{
    int i ;

    for (i = 0 ; i < h->elements ; i++ )
	heap_insert(h, i , NULL) ;
}

/*
 * cleanup the heap and free data structure
 */
static void
heap_free(struct dn_heap *h)
{
    if (h->size >0 )
	free(h->p, M_DUMMYNET);
    bzero(h, sizeof(*h) );
}

/*
 * --- end of heap management functions ---
 */

/*
 * Dispose a packet in dummynet. Use an inline functions so if we
 * need to free extra state associated to a packet, this is a
 * central point to do it.
 */
static __inline void *dn_free_pkt(struct mbuf *m)
{
	m_freem(m);
	return NULL;
}

static __inline void dn_free_pkts(struct mbuf *mnext)
{
	struct mbuf *m;

	while ((m = mnext) != NULL) {
		mnext = m->m_nextpkt;
		dn_free_pkt(m);
	}
}

/*
 * Return the mbuf tag holding the dummynet state.  As an optimization
 * this is assumed to be the first tag on the list.  If this turns out
 * wrong we'll need to search the list.
 */
static struct dn_pkt_tag *
dn_tag_get(struct mbuf *m)
{
    struct m_tag *mtag = m_tag_first(m);
    KASSERT(mtag != NULL &&
	    mtag->m_tag_cookie == MTAG_ABI_COMPAT &&
	    mtag->m_tag_id == PACKET_TAG_DUMMYNET,
	    ("packet on dummynet queue w/o dummynet tag!"));
    return (struct dn_pkt_tag *)(mtag+1);
}

/*
 * Scheduler functions:
 *
 * transmit_event() is called when the delay-line needs to enter
 * the scheduler, either because of existing pkts getting ready,
 * or new packets entering the queue. The event handled is the delivery
 * time of the packet.
 *
 * ready_event() does something similar with fixed-rate queues, and the
 * event handled is the finish time of the head pkt.
 *
 * wfq_ready_event() does something similar with WF2Q queues, and the
 * event handled is the start time of the head pkt.
 *
 * In all cases, we make sure that the data structures are consistent
 * before passing pkts out, because this might trigger recursive
 * invocations of the procedures.
 */
static void
transmit_event(struct dn_pipe *pipe, struct mbuf **head, struct mbuf **tail)
{
	struct mbuf *m;
	struct dn_pkt_tag *pkt;

	DUMMYNET_LOCK_ASSERT();

	while ((m = pipe->head) != NULL) {
		pkt = dn_tag_get(m);
		if (!DN_KEY_LEQ(pkt->output_time, curr_time))
			break;

		pipe->head = m->m_nextpkt;
		if (*tail != NULL)
			(*tail)->m_nextpkt = m;
		else
			*head = m;
		*tail = m;
	}
	if (*tail != NULL)
		(*tail)->m_nextpkt = NULL;

	/* If there are leftover packets, put into the heap for next event. */
	if ((m = pipe->head) != NULL) {
		pkt = dn_tag_get(m);
		/*
		 * XXX Should check errors on heap_insert, by draining the
		 * whole pipe p and hoping in the future we are more successful.
		 */
		heap_insert(&extract_heap, pkt->output_time, pipe);
	}
}

#define div64(a, b)	((int64_t)(a) / (int64_t)(b))
#define DN_TO_DROP	0xffff
/*
 * Compute how many ticks we have to wait before being able to send
 * a packet. This is computed as the "wire time" for the packet
 * (length + extra bits), minus the credit available, scaled to ticks.
 * Check that the result is not be negative (it could be if we have
 * too much leftover credit in q->numbytes).
 */
static inline dn_key
set_ticks(struct mbuf *m, struct dn_flow_queue *q, struct dn_pipe *p)
{
	int64_t ret;

	ret = div64( (m->m_pkthdr.len * 8 + q->extra_bits) * hz
		- q->numbytes + p->bandwidth - 1 , p->bandwidth);
	if (ret < 0)
		ret = 0;
	return ret;
}

/*
 * Convert the additional MAC overheads/delays into an equivalent
 * number of bits for the given data rate. The samples are in milliseconds
 * so we need to divide by 1000.
 */
static dn_key
compute_extra_bits(struct mbuf *pkt, struct dn_pipe *p)
{
	int index;
	dn_key extra_bits;

	if (!p->samples || p->samples_no == 0)
		return 0;
	index  = random() % p->samples_no;
	extra_bits = div64((dn_key)p->samples[index] * p->bandwidth, 1000);
	if (index >= p->loss_level) {
		struct dn_pkt_tag *dt = dn_tag_get(pkt);
		if (dt)
			dt->dn_dir = DN_TO_DROP;
	}
	return extra_bits;
}

static void
free_pipe(struct dn_pipe *p)
{
	if (p->samples)
		free(p->samples, M_DUMMYNET);
	free(p, M_DUMMYNET);
}

/*
 * extract pkt from queue, compute output time (could be now)
 * and put into delay line (p_queue)
 */
static void
move_pkt(struct mbuf *pkt, struct dn_flow_queue *q, struct dn_pipe *p,
    int len)
{
    struct dn_pkt_tag *dt = dn_tag_get(pkt);

    q->head = pkt->m_nextpkt ;
    q->len-- ;
    q->len_bytes -= len ;

    dt->output_time = curr_time + p->delay ;

    if (p->head == NULL)
	p->head = pkt;
    else
	p->tail->m_nextpkt = pkt;
    p->tail = pkt;
    p->tail->m_nextpkt = NULL;
}

/*
 * ready_event() is invoked every time the queue must enter the
 * scheduler, either because the first packet arrives, or because
 * a previously scheduled event fired.
 * On invokation, drain as many pkts as possible (could be 0) and then
 * if there are leftover packets reinsert the pkt in the scheduler.
 */
static void
ready_event(struct dn_flow_queue *q, struct mbuf **head, struct mbuf **tail)
{
	struct mbuf *pkt;
	struct dn_pipe *p = q->fs->pipe;
	int p_was_empty;

	DUMMYNET_LOCK_ASSERT();

	if (p == NULL) {
		printf("dummynet: ready_event- pipe is gone\n");
		return;
	}
	p_was_empty = (p->head == NULL);

	/*
	 * Schedule fixed-rate queues linked to this pipe:
	 * account for the bw accumulated since last scheduling, then
	 * drain as many pkts as allowed by q->numbytes and move to
	 * the delay line (in p) computing output time.
	 * bandwidth==0 (no limit) means we can drain the whole queue,
	 * setting len_scaled = 0 does the job.
	 */
	q->numbytes += (curr_time - q->sched_time) * p->bandwidth;
	while ((pkt = q->head) != NULL) {
		int len = pkt->m_pkthdr.len;
		dn_key len_scaled = p->bandwidth ? len*8*hz
			+ q->extra_bits*hz
			: 0;

		if (DN_KEY_GT(len_scaled, q->numbytes))
			break;
		q->numbytes -= len_scaled;
		move_pkt(pkt, q, p, len);
		if (q->head)
			q->extra_bits = compute_extra_bits(q->head, p);
	}
	/*
	 * If we have more packets queued, schedule next ready event
	 * (can only occur when bandwidth != 0, otherwise we would have
	 * flushed the whole queue in the previous loop).
	 * To this purpose we record the current time and compute how many
	 * ticks to go for the finish time of the packet.
	 */
	if ((pkt = q->head) != NULL) {	/* this implies bandwidth != 0 */
		dn_key t = set_ticks(pkt, q, p); /* ticks i have to wait */

		q->sched_time = curr_time;
		heap_insert(&ready_heap, curr_time + t, (void *)q);
		/*
		 * XXX Should check errors on heap_insert, and drain the whole
		 * queue on error hoping next time we are luckier.
		 */
	} else		/* RED needs to know when the queue becomes empty. */
		q->idle_time = curr_time;

	/*
	 * If the delay line was empty call transmit_event() now.
	 * Otherwise, the scheduler will take care of it.
	 */
	if (p_was_empty)
		transmit_event(p, head, tail);
}

/*
 * Called when we can transmit packets on WF2Q queues. Take pkts out of
 * the queues at their start time, and enqueue into the delay line.
 * Packets are drained until p->numbytes < 0. As long as
 * len_scaled >= p->numbytes, the packet goes into the delay line
 * with a deadline p->delay. For the last packet, if p->numbytes < 0,
 * there is an additional delay.
 */
static void
ready_event_wfq(struct dn_pipe *p, struct mbuf **head, struct mbuf **tail)
{
	int p_was_empty = (p->head == NULL);
	struct dn_heap *sch = &(p->scheduler_heap);
	struct dn_heap *neh = &(p->not_eligible_heap);
	int64_t p_numbytes = p->numbytes;

	/*
	 * p->numbytes is only 32bits in FBSD7, but we might need 64 bits.
	 * Use a local variable for the computations, and write back the
	 * results when done, saturating if needed.
	 * The local variable has no impact on performance and helps
	 * reducing diffs between the various branches.
	 */

	DUMMYNET_LOCK_ASSERT();

	if (p->if_name[0] == 0)		/* tx clock is simulated */
		p_numbytes += (curr_time - p->sched_time) * p->bandwidth;
	else {	/*
		 * tx clock is for real,
		 * the ifq must be empty or this is a NOP.
		 */
		if (p->ifp && p->ifp->if_snd.ifq_head != NULL)
			return;
		else {
			DPRINTF(("dummynet: pipe %d ready from %s --\n",
			    p->pipe_nr, p->if_name));
		}
	}

	/*
	 * While we have backlogged traffic AND credit, we need to do
	 * something on the queue.
	 */
	while (p_numbytes >= 0 && (sch->elements > 0 || neh->elements > 0)) {
		if (sch->elements > 0) {
			/* Have some eligible pkts to send out. */
			struct dn_flow_queue *q = sch->p[0].object;
			struct mbuf *pkt = q->head;
			struct dn_flow_set *fs = q->fs;
			uint64_t len = pkt->m_pkthdr.len;
			int len_scaled = p->bandwidth ? len * 8 * hz : 0;

			heap_extract(sch, NULL); /* Remove queue from heap. */
			p_numbytes -= len_scaled;
			move_pkt(pkt, q, p, len);

			p->V += div64((len << MY_M), p->sum);	/* Update V. */
			q->S = q->F;			/* Update start time. */
			if (q->len == 0) {
				/* Flow not backlogged any more. */
				fs->backlogged--;
				heap_insert(&(p->idle_heap), q->F, q);
			} else {
				/* Still backlogged. */

				/*
				 * Update F and position in backlogged queue,
				 * then put flow in not_eligible_heap
				 * (we will fix this later).
				 */
				len = (q->head)->m_pkthdr.len;
				q->F += div64((len << MY_M), fs->weight);
				if (DN_KEY_LEQ(q->S, p->V))
					heap_insert(neh, q->S, q);
				else
					heap_insert(sch, q->F, q);
			}
		}
		/*
		 * Now compute V = max(V, min(S_i)). Remember that all elements
		 * in sch have by definition S_i <= V so if sch is not empty,
		 * V is surely the max and we must not update it. Conversely,
		 * if sch is empty we only need to look at neh.
		 */
		if (sch->elements == 0 && neh->elements > 0)
			p->V = MAX64(p->V, neh->p[0].key);
		/* Move from neh to sch any packets that have become eligible */
		while (neh->elements > 0 && DN_KEY_LEQ(neh->p[0].key, p->V)) {
			struct dn_flow_queue *q = neh->p[0].object;
			heap_extract(neh, NULL);
			heap_insert(sch, q->F, q);
		}

		if (p->if_name[0] != '\0') { /* Tx clock is from a real thing */
			p_numbytes = -1;	/* Mark not ready for I/O. */
			break;
		}
	}
	if (sch->elements == 0 && neh->elements == 0 && p_numbytes >= 0) {
		p->idle_time = curr_time;
		/*
		 * No traffic and no events scheduled.
		 * We can get rid of idle-heap.
		 */
		if (p->idle_heap.elements > 0) {
			int i;

			for (i = 0; i < p->idle_heap.elements; i++) {
				struct dn_flow_queue *q;
				
				q = p->idle_heap.p[i].object;
				q->F = 0;
				q->S = q->F + 1;
			}
			p->sum = 0;
			p->V = 0;
			p->idle_heap.elements = 0;
		}
	}
	/*
	 * If we are getting clocks from dummynet (not a real interface) and
	 * If we are under credit, schedule the next ready event.
	 * Also fix the delivery time of the last packet.
	 */
	if (p->if_name[0]==0 && p_numbytes < 0) { /* This implies bw > 0. */
		dn_key t = 0;		/* Number of ticks i have to wait. */

		if (p->bandwidth > 0)
			t = div64(p->bandwidth - 1 - p_numbytes, p->bandwidth);
		dn_tag_get(p->tail)->output_time += t;
		p->sched_time = curr_time;
		heap_insert(&wfq_ready_heap, curr_time + t, (void *)p);
		/*
		 * XXX Should check errors on heap_insert, and drain the whole
		 * queue on error hoping next time we are luckier.
		 */
	}

	/* Write back p_numbytes (adjust 64->32bit if necessary). */
	p->numbytes = p_numbytes;

	/*
	 * If the delay line was empty call transmit_event() now.
	 * Otherwise, the scheduler will take care of it.
	 */
	if (p_was_empty)
		transmit_event(p, head, tail);
}

/*
 * This is called one tick, after previous run. It is used to
 * schedule next run.
 */
static void
dummynet(void * __unused unused)
{

	taskqueue_enqueue(dn_tq, &dn_task);
}

/*
 * The main dummynet processing function.
 */
static void
dummynet_task(void *context, int pending)
{
	struct mbuf *head = NULL, *tail = NULL;
	struct dn_pipe *pipe;
	struct dn_heap *heaps[3];
	struct dn_heap *h;
	void *p;	/* generic parameter to handler */
	int i;

	DUMMYNET_LOCK();

	heaps[0] = &ready_heap;			/* fixed-rate queues */
	heaps[1] = &wfq_ready_heap;		/* wfq queues */
	heaps[2] = &extract_heap;		/* delay line */

 	/* Update number of lost(coalesced) ticks. */
 	tick_lost += pending - 1;
 
 	getmicrouptime(&t);
 	/* Last tick duration (usec). */
 	tick_last = (t.tv_sec - prev_t.tv_sec) * 1000000 +
 	    (t.tv_usec - prev_t.tv_usec);
 	/* Last tick vs standard tick difference (usec). */
 	tick_delta = (tick_last * hz - 1000000) / hz;
 	/* Accumulated tick difference (usec). */
 	tick_delta_sum += tick_delta;
 
 	prev_t = t;
 
 	/*
 	 * Adjust curr_time if accumulated tick difference greater than
 	 * 'standard' tick. Since curr_time should be monotonically increasing,
 	 * we do positive adjustment as required and throttle curr_time in
 	 * case of negative adjustment.
 	 */
  	curr_time++;
 	if (tick_delta_sum - tick >= 0) {
 		int diff = tick_delta_sum / tick;
 
 		curr_time += diff;
 		tick_diff += diff;
 		tick_delta_sum %= tick;
 		tick_adjustment++;
 	} else if (tick_delta_sum + tick <= 0) {
 		curr_time--;
 		tick_diff--;
 		tick_delta_sum += tick;
 		tick_adjustment++;
 	}

	for (i = 0; i < 3; i++) {
		h = heaps[i];
		while (h->elements > 0 && DN_KEY_LEQ(h->p[0].key, curr_time)) {
			if (h->p[0].key > curr_time)
				printf("dummynet: warning, "
				    "heap %d is %d ticks late\n",
				    i, (int)(curr_time - h->p[0].key));
			/* store a copy before heap_extract */
			p = h->p[0].object;
			/* need to extract before processing */
			heap_extract(h, NULL);
			if (i == 0)
				ready_event(p, &head, &tail);
			else if (i == 1) {
				struct dn_pipe *pipe = p;
				if (pipe->if_name[0] != '\0')
					printf("dummynet: bad ready_event_wfq "
					    "for pipe %s\n", pipe->if_name);
				else
					ready_event_wfq(p, &head, &tail);
			} else
				transmit_event(p, &head, &tail);
		}
	}

	/* Sweep pipes trying to expire idle flow_queues. */
	for (i = 0; i < HASHSIZE; i++) {
		SLIST_FOREACH(pipe, &pipehash[i], next) {
			if (pipe->idle_heap.elements > 0 &&
			    DN_KEY_LT(pipe->idle_heap.p[0].key, pipe->V)) {
				struct dn_flow_queue *q =
				    pipe->idle_heap.p[0].object;

				heap_extract(&(pipe->idle_heap), NULL);
				/* Mark timestamp as invalid. */
				q->S = q->F + 1;
				pipe->sum -= q->fs->weight;
			}
		}
	}

	DUMMYNET_UNLOCK();

	if (head != NULL)
		dummynet_send(head);

	callout_reset(&dn_timeout, 1, dummynet, NULL);
}

static void
dummynet_send(struct mbuf *m)
{
	struct dn_pkt_tag *pkt;
	struct mbuf *n;
	struct ip *ip;
	int dst;

	for (; m != NULL; m = n) {
		n = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (m_tag_first(m) == NULL) {
			pkt = NULL; /* probably unnecessary */
			dst = DN_TO_DROP;
		} else {
			pkt = dn_tag_get(m);
			dst = pkt->dn_dir;
		}

		switch (dst) {
		case DN_TO_IP_OUT:
			ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL);
			break ;
		case DN_TO_IP_IN :
			ip = mtod(m, struct ip *);
			ip->ip_len = htons(ip->ip_len);
			ip->ip_off = htons(ip->ip_off);
			netisr_dispatch(NETISR_IP, m);
			break;
#ifdef INET6
		case DN_TO_IP6_IN:
			netisr_dispatch(NETISR_IPV6, m);
			break;

		case DN_TO_IP6_OUT:
			ip6_output(m, NULL, NULL, IPV6_FORWARDING, NULL, NULL, NULL);
			break;
#endif
		case DN_TO_IFB_FWD:
			if (bridge_dn_p != NULL)
				((*bridge_dn_p)(m, pkt->ifp));
			else
				printf("dummynet: if_bridge not loaded\n");

			break;
		case DN_TO_ETH_DEMUX:
			/*
			 * The Ethernet code assumes the Ethernet header is
			 * contiguous in the first mbuf header.
			 * Insure this is true.
			 */
			if (m->m_len < ETHER_HDR_LEN &&
			    (m = m_pullup(m, ETHER_HDR_LEN)) == NULL) {
				printf("dummynet/ether: pullup failed, "
				    "dropping packet\n");
				break;
			}
			ether_demux(m->m_pkthdr.rcvif, m);
			break;
		case DN_TO_ETH_OUT:
			ether_output_frame(pkt->ifp, m);
			break;

		case DN_TO_DROP:
			/* drop the packet after some time */
			dn_free_pkt(m);
			break;

		default:
			printf("dummynet: bad switch %d!\n", pkt->dn_dir);
			dn_free_pkt(m);
			break;
		}
	}
}

/*
 * Unconditionally expire empty queues in case of shortage.
 * Returns the number of queues freed.
 */
static int
expire_queues(struct dn_flow_set *fs)
{
    struct dn_flow_queue *q, *prev ;
    int i, initial_elements = fs->rq_elements ;

    if (fs->last_expired == time_uptime)
	return 0 ;
    fs->last_expired = time_uptime ;
    for (i = 0 ; i <= fs->rq_size ; i++) { /* last one is overflow */
	for (prev=NULL, q = fs->rq[i] ; q != NULL ; ) {
	    if (!QUEUE_IS_IDLE(q)) {
  		prev = q ;
  	        q = q->next ;
  	    } else { /* entry is idle, expire it */
		struct dn_flow_queue *old_q = q ;

		if (prev != NULL)
		    prev->next = q = q->next ;
		else
		    fs->rq[i] = q = q->next ;
		fs->rq_elements-- ;
		free(old_q, M_DUMMYNET);
	    }
	}
    }
    return initial_elements - fs->rq_elements ;
}

/*
 * If room, create a new queue and put at head of slot i;
 * otherwise, create or use the default queue.
 */
static struct dn_flow_queue *
create_queue(struct dn_flow_set *fs, int i)
{
	struct dn_flow_queue *q;

	if (fs->rq_elements > fs->rq_size * dn_max_ratio &&
	    expire_queues(fs) == 0) {
		/* No way to get room, use or create overflow queue. */
		i = fs->rq_size;
		if (fs->rq[i] != NULL)
		    return fs->rq[i];
	}
	q = malloc(sizeof(*q), M_DUMMYNET, M_NOWAIT | M_ZERO);
	if (q == NULL) {
		printf("dummynet: sorry, cannot allocate queue for new flow\n");
		return (NULL);
	}
	q->fs = fs;
	q->hash_slot = i;
	q->next = fs->rq[i];
	q->S = q->F + 1;	/* hack - mark timestamp as invalid. */
	q->numbytes = fs->pipe->burst + (io_fast ? fs->pipe->bandwidth : 0);
	fs->rq[i] = q;
	fs->rq_elements++;
	return (q);
}

/*
 * Given a flow_set and a pkt in last_pkt, find a matching queue
 * after appropriate masking. The queue is moved to front
 * so that further searches take less time.
 */
static struct dn_flow_queue *
find_queue(struct dn_flow_set *fs, struct ipfw_flow_id *id)
{
    int i = 0 ; /* we need i and q for new allocations */
    struct dn_flow_queue *q, *prev;
    int is_v6 = IS_IP6_FLOW_ID(id);

    if ( !(fs->flags_fs & DN_HAVE_FLOW_MASK) )
	q = fs->rq[0] ;
    else {
	/* first, do the masking, then hash */
	id->dst_port &= fs->flow_mask.dst_port ;
	id->src_port &= fs->flow_mask.src_port ;
	id->proto &= fs->flow_mask.proto ;
	id->flags = 0 ; /* we don't care about this one */
	if (is_v6) {
	    APPLY_MASK(&id->dst_ip6, &fs->flow_mask.dst_ip6);
	    APPLY_MASK(&id->src_ip6, &fs->flow_mask.src_ip6);
	    id->flow_id6 &= fs->flow_mask.flow_id6;

	    i = ((id->dst_ip6.__u6_addr.__u6_addr32[0]) & 0xffff)^
		((id->dst_ip6.__u6_addr.__u6_addr32[1]) & 0xffff)^
		((id->dst_ip6.__u6_addr.__u6_addr32[2]) & 0xffff)^
		((id->dst_ip6.__u6_addr.__u6_addr32[3]) & 0xffff)^

		((id->dst_ip6.__u6_addr.__u6_addr32[0] >> 15) & 0xffff)^
		((id->dst_ip6.__u6_addr.__u6_addr32[1] >> 15) & 0xffff)^
		((id->dst_ip6.__u6_addr.__u6_addr32[2] >> 15) & 0xffff)^
		((id->dst_ip6.__u6_addr.__u6_addr32[3] >> 15) & 0xffff)^

		((id->src_ip6.__u6_addr.__u6_addr32[0] << 1) & 0xfffff)^
		((id->src_ip6.__u6_addr.__u6_addr32[1] << 1) & 0xfffff)^
		((id->src_ip6.__u6_addr.__u6_addr32[2] << 1) & 0xfffff)^
		((id->src_ip6.__u6_addr.__u6_addr32[3] << 1) & 0xfffff)^

		((id->src_ip6.__u6_addr.__u6_addr32[0] << 16) & 0xffff)^
		((id->src_ip6.__u6_addr.__u6_addr32[1] << 16) & 0xffff)^
		((id->src_ip6.__u6_addr.__u6_addr32[2] << 16) & 0xffff)^
		((id->src_ip6.__u6_addr.__u6_addr32[3] << 16) & 0xffff)^

		(id->dst_port << 1) ^ (id->src_port) ^
		(id->proto ) ^
		(id->flow_id6);
	} else {
	    id->dst_ip &= fs->flow_mask.dst_ip ;
	    id->src_ip &= fs->flow_mask.src_ip ;

	    i = ( (id->dst_ip) & 0xffff ) ^
		( (id->dst_ip >> 15) & 0xffff ) ^
		( (id->src_ip << 1) & 0xffff ) ^
		( (id->src_ip >> 16 ) & 0xffff ) ^
		(id->dst_port << 1) ^ (id->src_port) ^
		(id->proto );
	}
	i = i % fs->rq_size ;
	/* finally, scan the current list for a match */
	searches++ ;
	for (prev=NULL, q = fs->rq[i] ; q ; ) {
	    search_steps++;
	    if (is_v6 &&
		    IN6_ARE_ADDR_EQUAL(&id->dst_ip6,&q->id.dst_ip6) &&  
		    IN6_ARE_ADDR_EQUAL(&id->src_ip6,&q->id.src_ip6) &&  
		    id->dst_port == q->id.dst_port &&
		    id->src_port == q->id.src_port &&
		    id->proto == q->id.proto &&
		    id->flags == q->id.flags &&
		    id->flow_id6 == q->id.flow_id6)
		break ; /* found */

	    if (!is_v6 && id->dst_ip == q->id.dst_ip &&
		    id->src_ip == q->id.src_ip &&
		    id->dst_port == q->id.dst_port &&
		    id->src_port == q->id.src_port &&
		    id->proto == q->id.proto &&
		    id->flags == q->id.flags)
		break ; /* found */

	    /* No match. Check if we can expire the entry */
	    if (pipe_expire && QUEUE_IS_IDLE(q)) {
		/* entry is idle and not in any heap, expire it */
		struct dn_flow_queue *old_q = q ;

		if (prev != NULL)
		    prev->next = q = q->next ;
		else
		    fs->rq[i] = q = q->next ;
		fs->rq_elements-- ;
		free(old_q, M_DUMMYNET);
		continue ;
	    }
	    prev = q ;
	    q = q->next ;
	}
	if (q && prev != NULL) { /* found and not in front */
	    prev->next = q->next ;
	    q->next = fs->rq[i] ;
	    fs->rq[i] = q ;
	}
    }
    if (q == NULL) { /* no match, need to allocate a new entry */
	q = create_queue(fs, i);
	if (q != NULL)
	q->id = *id ;
    }
    return q ;
}

static int
red_drops(struct dn_flow_set *fs, struct dn_flow_queue *q, int len)
{
	/*
	 * RED algorithm
	 *
	 * RED calculates the average queue size (avg) using a low-pass filter
	 * with an exponential weighted (w_q) moving average:
	 * 	avg  <-  (1-w_q) * avg + w_q * q_size
	 * where q_size is the queue length (measured in bytes or * packets).
	 *
	 * If q_size == 0, we compute the idle time for the link, and set
	 *	avg = (1 - w_q)^(idle/s)
	 * where s is the time needed for transmitting a medium-sized packet.
	 *
	 * Now, if avg < min_th the packet is enqueued.
	 * If avg > max_th the packet is dropped. Otherwise, the packet is
	 * dropped with probability P function of avg.
	 */

	int64_t p_b = 0;

	/* Queue in bytes or packets? */
	u_int q_size = (fs->flags_fs & DN_QSIZE_IS_BYTES) ?
	    q->len_bytes : q->len;

	DPRINTF(("\ndummynet: %d q: %2u ", (int)curr_time, q_size));

	/* Average queue size estimation. */
	if (q_size != 0) {
		/* Queue is not empty, avg <- avg + (q_size - avg) * w_q */
		int diff = SCALE(q_size) - q->avg;
		int64_t v = SCALE_MUL((int64_t)diff, (int64_t)fs->w_q);

		q->avg += (int)v;
	} else {
		/*
		 * Queue is empty, find for how long the queue has been
		 * empty and use a lookup table for computing
		 * (1 - * w_q)^(idle_time/s) where s is the time to send a
		 * (small) packet.
		 * XXX check wraps...
		 */
		if (q->avg) {
			u_int t = div64(curr_time - q->idle_time,
			    fs->lookup_step);

			q->avg = (t < fs->lookup_depth) ?
			    SCALE_MUL(q->avg, fs->w_q_lookup[t]) : 0;
		}
	}
	DPRINTF(("dummynet: avg: %u ", SCALE_VAL(q->avg)));

	/* Should i drop? */
	if (q->avg < fs->min_th) {
		q->count = -1;
		return (0);	/* accept packet */
	}
	if (q->avg >= fs->max_th) {	/* average queue >=  max threshold */
		if (fs->flags_fs & DN_IS_GENTLE_RED) {
			/*
			 * According to Gentle-RED, if avg is greater than
			 * max_th the packet is dropped with a probability
			 *	 p_b = c_3 * avg - c_4
			 * where c_3 = (1 - max_p) / max_th
			 *       c_4 = 1 - 2 * max_p
			 */
			p_b = SCALE_MUL((int64_t)fs->c_3, (int64_t)q->avg) -
			    fs->c_4;
		} else {
			q->count = -1;
			DPRINTF(("dummynet: - drop"));
			return (1);
		}
	} else if (q->avg > fs->min_th) {
		/*
		 * We compute p_b using the linear dropping function
		 *	 p_b = c_1 * avg - c_2
		 * where c_1 = max_p / (max_th - min_th)
		 * 	 c_2 = max_p * min_th / (max_th - min_th)
		 */
		p_b = SCALE_MUL((int64_t)fs->c_1, (int64_t)q->avg) - fs->c_2;
	}

	if (fs->flags_fs & DN_QSIZE_IS_BYTES)
		p_b = div64(p_b * len, fs->max_pkt_size);
	if (++q->count == 0)
		q->random = random() & 0xffff;
	else {
		/*
		 * q->count counts packets arrived since last drop, so a greater
		 * value of q->count means a greater packet drop probability.
		 */
		if (SCALE_MUL(p_b, SCALE((int64_t)q->count)) > q->random) {
			q->count = 0;
			DPRINTF(("dummynet: - red drop"));
			/* After a drop we calculate a new random value. */
			q->random = random() & 0xffff;
			return (1);	/* drop */
		}
	}
	/* End of RED algorithm. */

	return (0);	/* accept */
}

static __inline struct dn_flow_set *
locate_flowset(int fs_nr)
{
	struct dn_flow_set *fs;

	SLIST_FOREACH(fs, &flowsethash[HASH(fs_nr)], next)
		if (fs->fs_nr == fs_nr)
			return (fs);

	return (NULL);
}

static __inline struct dn_pipe *
locate_pipe(int pipe_nr)
{
	struct dn_pipe *pipe;

	SLIST_FOREACH(pipe, &pipehash[HASH(pipe_nr)], next)
		if (pipe->pipe_nr == pipe_nr)
			return (pipe);

	return (NULL);
}

/*
 * dummynet hook for packets. Below 'pipe' is a pipe or a queue
 * depending on whether WF2Q or fixed bw is used.
 *
 * pipe_nr	pipe or queue the packet is destined for.
 * dir		where shall we send the packet after dummynet.
 * m		the mbuf with the packet
 * ifp		the 'ifp' parameter from the caller.
 *		NULL in ip_input, destination interface in ip_output,
 * rule		matching rule, in case of multiple passes
 */
static int
dummynet_io(struct mbuf **m0, int dir, struct ip_fw_args *fwa)
{
	struct mbuf *m = *m0, *head = NULL, *tail = NULL;
	struct dn_pkt_tag *pkt;
	struct m_tag *mtag;
	struct dn_flow_set *fs = NULL;
	struct dn_pipe *pipe;
	uint64_t len = m->m_pkthdr.len;
	struct dn_flow_queue *q = NULL;
	int is_pipe = fwa->cookie & 0x8000000 ? 0 : 1;

	KASSERT(m->m_nextpkt == NULL,
	    ("dummynet_io: mbuf queue passed to dummynet"));

	DUMMYNET_LOCK();
	io_pkt++;
	/*
	 * This is a dummynet rule, so we expect an O_PIPE or O_QUEUE rule.
	 *
	 * XXXGL: probably the pipe->fs and fs->pipe logic here
	 * below can be simplified.
	 */
	if (is_pipe) {
		pipe = locate_pipe(fwa->cookie & 0xffff);
		if (pipe != NULL)
			fs = &(pipe->fs);
	} else
		fs = locate_flowset(fwa->cookie & 0xffff);

	if (fs == NULL)
		goto dropit;	/* This queue/pipe does not exist! */
	pipe = fs->pipe;
	if (pipe == NULL) {	/* Must be a queue, try find a matching pipe. */
		pipe = locate_pipe(fs->parent_nr);
		if (pipe != NULL)
			fs->pipe = pipe;
		else {
			printf("dummynet: no pipe %d for queue %d, drop pkt\n",
			    fs->parent_nr, fs->fs_nr);
			goto dropit;
		}
	}
	q = find_queue(fs, &(fwa->f_id));
	if (q == NULL)
		goto dropit;		/* Cannot allocate queue. */

	/* Update statistics, then check reasons to drop pkt. */
	q->tot_bytes += len;
	q->tot_pkts++;
	if (fs->plr && random() < fs->plr)
		goto dropit;		/* Random pkt drop. */
	if (fs->flags_fs & DN_QSIZE_IS_BYTES) {
		if (q->len_bytes > fs->qsize)
			goto dropit;	/* Queue size overflow. */
	} else {
		if (q->len >= fs->qsize)
			goto dropit;	/* Queue count overflow. */
	}
	if (fs->flags_fs & DN_IS_RED && red_drops(fs, q, len))
		goto dropit;

	/* XXX expensive to zero, see if we can remove it. */
	mtag = m_tag_get(PACKET_TAG_DUMMYNET,
	    sizeof(struct dn_pkt_tag), M_NOWAIT | M_ZERO);
	if (mtag == NULL)
		goto dropit;		/* Cannot allocate packet header. */
	m_tag_prepend(m, mtag);		/* Attach to mbuf chain. */

	pkt = (struct dn_pkt_tag *)(mtag + 1);
	/*
	 * Ok, i can handle the pkt now...
	 * Build and enqueue packet + parameters.
	 */
	pkt->slot = fwa->slot;
	pkt->rulenum = fwa->rulenum;
	pkt->rule_id = fwa->rule_id;
	pkt->chain_id = fwa->chain_id;
	pkt->dn_dir = dir;

	pkt->ifp = fwa->oif;

	if (q->head == NULL)
		q->head = m;
	else
		q->tail->m_nextpkt = m;
	q->tail = m;
	q->len++;
	q->len_bytes += len;

	if (q->head != m)		/* Flow was not idle, we are done. */
		goto done;

	if (is_pipe) {			/* Fixed rate queues. */
		if (q->idle_time < curr_time) {
			/* Calculate available burst size. */
			q->numbytes +=
			    (curr_time - q->idle_time - 1) * pipe->bandwidth;
			if (q->numbytes > pipe->burst)
				q->numbytes = pipe->burst;
			if (io_fast)
				q->numbytes += pipe->bandwidth;
		}
	} else {			/* WF2Q. */
		if (pipe->idle_time < curr_time &&
		    pipe->scheduler_heap.elements == 0 &&
		    pipe->not_eligible_heap.elements == 0) {
			/* Calculate available burst size. */
			pipe->numbytes +=
			    (curr_time - pipe->idle_time - 1) * pipe->bandwidth;
			if (pipe->numbytes > 0 && pipe->numbytes > pipe->burst)
				pipe->numbytes = pipe->burst;
			if (io_fast)
				pipe->numbytes += pipe->bandwidth;
		}
		pipe->idle_time = curr_time;
	}
	/* Necessary for both: fixed rate & WF2Q queues. */
	q->idle_time = curr_time;

	/*
	 * If we reach this point the flow was previously idle, so we need
	 * to schedule it. This involves different actions for fixed-rate or
	 * WF2Q queues.
	 */
	if (is_pipe) {
		/* Fixed-rate queue: just insert into the ready_heap. */
		dn_key t = 0;

		if (pipe->bandwidth) {
			q->extra_bits = compute_extra_bits(m, pipe);
			t = set_ticks(m, q, pipe);
		}
		q->sched_time = curr_time;
		if (t == 0)		/* Must process it now. */
			ready_event(q, &head, &tail);
		else
			heap_insert(&ready_heap, curr_time + t , q);
	} else {
		/*
		 * WF2Q. First, compute start time S: if the flow was
		 * idle (S = F + 1) set S to the virtual time V for the
		 * controlling pipe, and update the sum of weights for the pipe;
		 * otherwise, remove flow from idle_heap and set S to max(F,V).
		 * Second, compute finish time F = S + len / weight.
		 * Third, if pipe was idle, update V = max(S, V).
		 * Fourth, count one more backlogged flow.
		 */
		if (DN_KEY_GT(q->S, q->F)) { /* Means timestamps are invalid. */
			q->S = pipe->V;
			pipe->sum += fs->weight; /* Add weight of new queue. */
		} else {
			heap_extract(&(pipe->idle_heap), q);
			q->S = MAX64(q->F, pipe->V);
		}
		q->F = q->S + div64(len << MY_M, fs->weight);

		if (pipe->not_eligible_heap.elements == 0 &&
		    pipe->scheduler_heap.elements == 0)
			pipe->V = MAX64(q->S, pipe->V);
		fs->backlogged++;
		/*
		 * Look at eligibility. A flow is not eligibile if S>V (when
		 * this happens, it means that there is some other flow already
		 * scheduled for the same pipe, so the scheduler_heap cannot be
		 * empty). If the flow is not eligible we just store it in the
		 * not_eligible_heap. Otherwise, we store in the scheduler_heap
		 * and possibly invoke ready_event_wfq() right now if there is
		 * leftover credit.
		 * Note that for all flows in scheduler_heap (SCH), S_i <= V,
		 * and for all flows in not_eligible_heap (NEH), S_i > V.
		 * So when we need to compute max(V, min(S_i)) forall i in
		 * SCH+NEH, we only need to look into NEH.
		 */
		if (DN_KEY_GT(q->S, pipe->V)) {		/* Not eligible. */
			if (pipe->scheduler_heap.elements == 0)
				printf("dummynet: ++ ouch! not eligible but empty scheduler!\n");
			heap_insert(&(pipe->not_eligible_heap), q->S, q);
		} else {
			heap_insert(&(pipe->scheduler_heap), q->F, q);
			if (pipe->numbytes >= 0) {	 /* Pipe is idle. */
				if (pipe->scheduler_heap.elements != 1)
					printf("dummynet: OUCH! pipe should have been idle!\n");
				DPRINTF(("dummynet: waking up pipe %d at %d\n",
				    pipe->pipe_nr, (int)(q->F >> MY_M)));
				pipe->sched_time = curr_time;
				ready_event_wfq(pipe, &head, &tail);
			}
		}
	}
done:
	if (head == m && dir != DN_TO_IFB_FWD && dir != DN_TO_ETH_DEMUX &&
	    dir != DN_TO_ETH_OUT) {	/* Fast io. */
		io_pkt_fast++;
		if (m->m_nextpkt != NULL)
			printf("dummynet: fast io: pkt chain detected!\n");
		head = m->m_nextpkt = NULL;
	} else
		*m0 = NULL;		/* Normal io. */

	DUMMYNET_UNLOCK();
	if (head != NULL)
		dummynet_send(head);
	return (0);

dropit:
	io_pkt_drop++;
	if (q)
		q->drops++;
	DUMMYNET_UNLOCK();
	*m0 = dn_free_pkt(m);
	return ((fs && (fs->flags_fs & DN_NOERROR)) ? 0 : ENOBUFS);
}

/*
 * Dispose all packets and flow_queues on a flow_set.
 * If all=1, also remove red lookup table and other storage,
 * including the descriptor itself.
 * For the one in dn_pipe MUST also cleanup ready_heap...
 */
static void
purge_flow_set(struct dn_flow_set *fs, int all)
{
	struct dn_flow_queue *q, *qn;
	int i;

	DUMMYNET_LOCK_ASSERT();

	for (i = 0; i <= fs->rq_size; i++) {
		for (q = fs->rq[i]; q != NULL; q = qn) {
			dn_free_pkts(q->head);
			qn = q->next;
			free(q, M_DUMMYNET);
		}
		fs->rq[i] = NULL;
	}

	fs->rq_elements = 0;
	if (all) {
		/* RED - free lookup table. */
		if (fs->w_q_lookup != NULL)
			free(fs->w_q_lookup, M_DUMMYNET);
		if (fs->rq != NULL)
			free(fs->rq, M_DUMMYNET);
		/* If this fs is not part of a pipe, free it. */
		if (fs->pipe == NULL || fs != &(fs->pipe->fs))
			free(fs, M_DUMMYNET);
	}
}

/*
 * Dispose all packets queued on a pipe (not a flow_set).
 * Also free all resources associated to a pipe, which is about
 * to be deleted.
 */
static void
purge_pipe(struct dn_pipe *pipe)
{

    purge_flow_set( &(pipe->fs), 1 );

    dn_free_pkts(pipe->head);

    heap_free( &(pipe->scheduler_heap) );
    heap_free( &(pipe->not_eligible_heap) );
    heap_free( &(pipe->idle_heap) );
}

/*
 * Delete all pipes and heaps returning memory. Must also
 * remove references from all ipfw rules to all pipes.
 */
static void
dummynet_flush(void)
{
	struct dn_pipe *pipe, *pipe1;
	struct dn_flow_set *fs, *fs1;
	int i;

	DUMMYNET_LOCK();
	/* Free heaps so we don't have unwanted events. */
	heap_free(&ready_heap);
	heap_free(&wfq_ready_heap);
	heap_free(&extract_heap);

	/*
	 * Now purge all queued pkts and delete all pipes.
	 *
	 * XXXGL: can we merge the for(;;) cycles into one or not?
	 */
	for (i = 0; i < HASHSIZE; i++)
		SLIST_FOREACH_SAFE(fs, &flowsethash[i], next, fs1) {
			SLIST_REMOVE(&flowsethash[i], fs, dn_flow_set, next);
			purge_flow_set(fs, 1);
		}
	for (i = 0; i < HASHSIZE; i++)
		SLIST_FOREACH_SAFE(pipe, &pipehash[i], next, pipe1) {
			SLIST_REMOVE(&pipehash[i], pipe, dn_pipe, next);
			purge_pipe(pipe);
			free_pipe(pipe);
		}
	DUMMYNET_UNLOCK();
}

/*
 * setup RED parameters
 */
static int
config_red(struct dn_flow_set *p, struct dn_flow_set *x)
{
	int i;

	x->w_q = p->w_q;
	x->min_th = SCALE(p->min_th);
	x->max_th = SCALE(p->max_th);
	x->max_p = p->max_p;

	x->c_1 = p->max_p / (p->max_th - p->min_th);
	x->c_2 = SCALE_MUL(x->c_1, SCALE(p->min_th));

	if (x->flags_fs & DN_IS_GENTLE_RED) {
		x->c_3 = (SCALE(1) - p->max_p) / p->max_th;
		x->c_4 = SCALE(1) - 2 * p->max_p;
	}

	/* If the lookup table already exist, free and create it again. */
	if (x->w_q_lookup) {
		free(x->w_q_lookup, M_DUMMYNET);
		x->w_q_lookup = NULL;
	}
	if (red_lookup_depth == 0) {
		printf("\ndummynet: net.inet.ip.dummynet.red_lookup_depth"
		    "must be > 0\n");
		free(x, M_DUMMYNET);
		return (EINVAL);
	}
	x->lookup_depth = red_lookup_depth;
	x->w_q_lookup = (u_int *)malloc(x->lookup_depth * sizeof(int),
	    M_DUMMYNET, M_NOWAIT);
	if (x->w_q_lookup == NULL) {
		printf("dummynet: sorry, cannot allocate red lookup table\n");
		free(x, M_DUMMYNET);
		return(ENOSPC);
	}

	/* Fill the lookup table with (1 - w_q)^x */
	x->lookup_step = p->lookup_step;
	x->lookup_weight = p->lookup_weight;
	x->w_q_lookup[0] = SCALE(1) - x->w_q;

	for (i = 1; i < x->lookup_depth; i++)
		x->w_q_lookup[i] =
		    SCALE_MUL(x->w_q_lookup[i - 1], x->lookup_weight);

	if (red_avg_pkt_size < 1)
		red_avg_pkt_size = 512;
	x->avg_pkt_size = red_avg_pkt_size;
	if (red_max_pkt_size < 1)
		red_max_pkt_size = 1500;
	x->max_pkt_size = red_max_pkt_size;
	return (0);
}

static int
alloc_hash(struct dn_flow_set *x, struct dn_flow_set *pfs)
{
    if (x->flags_fs & DN_HAVE_FLOW_MASK) {     /* allocate some slots */
	int l = pfs->rq_size;

	if (l == 0)
	    l = dn_hash_size;
	if (l < 4)
	    l = 4;
	else if (l > DN_MAX_HASH_SIZE)
	    l = DN_MAX_HASH_SIZE;
	x->rq_size = l;
    } else                  /* one is enough for null mask */
	x->rq_size = 1;
    x->rq = malloc((1 + x->rq_size) * sizeof(struct dn_flow_queue *),
	    M_DUMMYNET, M_NOWAIT | M_ZERO);
    if (x->rq == NULL) {
	printf("dummynet: sorry, cannot allocate queue\n");
	return (ENOMEM);
    }
    x->rq_elements = 0;
    return 0 ;
}

static void
set_fs_parms(struct dn_flow_set *x, struct dn_flow_set *src)
{
	x->flags_fs = src->flags_fs;
	x->qsize = src->qsize;
	x->plr = src->plr;
	x->flow_mask = src->flow_mask;
	if (x->flags_fs & DN_QSIZE_IS_BYTES) {
		if (x->qsize > pipe_byte_limit)
			x->qsize = 1024 * 1024;
	} else {
		if (x->qsize == 0)
			x->qsize = 50;
		if (x->qsize > pipe_slot_limit)
			x->qsize = 50;
	}
	/* Configuring RED. */
	if (x->flags_fs & DN_IS_RED)
		config_red(src, x);	/* XXX should check errors */
}

/*
 * Setup pipe or queue parameters.
 */
static int
config_pipe(struct dn_pipe *p)
{
	struct dn_flow_set *pfs = &(p->fs);
	struct dn_flow_queue *q;
	int i, error;

	/*
	 * The config program passes parameters as follows:
	 * bw = bits/second (0 means no limits),
	 * delay = ms, must be translated into ticks.
	 * qsize = slots/bytes
	 */
	p->delay = (p->delay * hz) / 1000;
	/* Scale burst size: bytes -> bits * hz */
	p->burst *= 8 * hz;
	/* We need either a pipe number or a flow_set number. */
	if (p->pipe_nr == 0 && pfs->fs_nr == 0)
		return (EINVAL);
	if (p->pipe_nr != 0 && pfs->fs_nr != 0)
		return (EINVAL);
	if (p->pipe_nr != 0) {			/* this is a pipe */
		struct dn_pipe *pipe;

		DUMMYNET_LOCK();
		pipe = locate_pipe(p->pipe_nr);	/* locate pipe */

		if (pipe == NULL) {		/* new pipe */
			pipe = malloc(sizeof(struct dn_pipe), M_DUMMYNET,
			    M_NOWAIT | M_ZERO);
			if (pipe == NULL) {
				DUMMYNET_UNLOCK();
				printf("dummynet: no memory for new pipe\n");
				return (ENOMEM);
			}
			pipe->pipe_nr = p->pipe_nr;
			pipe->fs.pipe = pipe;
			/*
			 * idle_heap is the only one from which
			 * we extract from the middle.
			 */
			pipe->idle_heap.size = pipe->idle_heap.elements = 0;
			pipe->idle_heap.offset =
			    offsetof(struct dn_flow_queue, heap_pos);
		} else
			/* Flush accumulated credit for all queues. */
			for (i = 0; i <= pipe->fs.rq_size; i++)
				for (q = pipe->fs.rq[i]; q; q = q->next) {
					q->numbytes = p->burst +
					    (io_fast ? p->bandwidth : 0);
				}

		pipe->bandwidth = p->bandwidth;
		pipe->burst = p->burst;
		pipe->numbytes = pipe->burst + (io_fast ? pipe->bandwidth : 0);
		bcopy(p->if_name, pipe->if_name, sizeof(p->if_name));
		pipe->ifp = NULL;		/* reset interface ptr */
		pipe->delay = p->delay;
		set_fs_parms(&(pipe->fs), pfs);

		/* Handle changes in the delay profile. */
		if (p->samples_no > 0) {
			if (pipe->samples_no != p->samples_no) {
				if (pipe->samples != NULL)
					free(pipe->samples, M_DUMMYNET);
				pipe->samples =
				    malloc(p->samples_no*sizeof(dn_key),
					M_DUMMYNET, M_NOWAIT | M_ZERO);
				if (pipe->samples == NULL) {
					DUMMYNET_UNLOCK();
					printf("dummynet: no memory "
						"for new samples\n");
					return (ENOMEM);
				}
				pipe->samples_no = p->samples_no;
			}

			strncpy(pipe->name,p->name,sizeof(pipe->name));
			pipe->loss_level = p->loss_level;
			for (i = 0; i<pipe->samples_no; ++i)
				pipe->samples[i] = p->samples[i];
		} else if (pipe->samples != NULL) {
			free(pipe->samples, M_DUMMYNET);
			pipe->samples = NULL;
			pipe->samples_no = 0;
		}

		if (pipe->fs.rq == NULL) {	/* a new pipe */
			error = alloc_hash(&(pipe->fs), pfs);
			if (error) {
				DUMMYNET_UNLOCK();
				free_pipe(pipe);
				return (error);
			}
			SLIST_INSERT_HEAD(&pipehash[HASH(pipe->pipe_nr)],
			    pipe, next);
		}
		DUMMYNET_UNLOCK();
	} else {				/* config queue */
		struct dn_flow_set *fs;

		DUMMYNET_LOCK();
		fs = locate_flowset(pfs->fs_nr); /* locate flow_set */

		if (fs == NULL) {		/* new */
			if (pfs->parent_nr == 0) { /* need link to a pipe */
				DUMMYNET_UNLOCK();
				return (EINVAL);
			}
			fs = malloc(sizeof(struct dn_flow_set), M_DUMMYNET,
			    M_NOWAIT | M_ZERO);
			if (fs == NULL) {
				DUMMYNET_UNLOCK();
				printf(
				    "dummynet: no memory for new flow_set\n");
				return (ENOMEM);
			}
			fs->fs_nr = pfs->fs_nr;
			fs->parent_nr = pfs->parent_nr;
			fs->weight = pfs->weight;
			if (fs->weight == 0)
				fs->weight = 1;
			else if (fs->weight > 100)
				fs->weight = 100;
		} else {
			/*
			 * Change parent pipe not allowed;
			 * must delete and recreate.
			 */
			if (pfs->parent_nr != 0 &&
			    fs->parent_nr != pfs->parent_nr) {
				DUMMYNET_UNLOCK();
				return (EINVAL);
			}
		}

		set_fs_parms(fs, pfs);

		if (fs->rq == NULL) {		/* a new flow_set */
			error = alloc_hash(fs, pfs);
			if (error) {
				DUMMYNET_UNLOCK();
				free(fs, M_DUMMYNET);
				return (error);
			}
			SLIST_INSERT_HEAD(&flowsethash[HASH(fs->fs_nr)],
			    fs, next);
		}
		DUMMYNET_UNLOCK();
	}
	return (0);
}

/*
 * Helper function to remove from a heap queues which are linked to
 * a flow_set about to be deleted.
 */
static void
fs_remove_from_heap(struct dn_heap *h, struct dn_flow_set *fs)
{
    int i, found;

    for (i = found = 0 ; i < h->elements ;) {
	if ( ((struct dn_flow_queue *)h->p[i].object)->fs == fs) {
	    h->elements-- ;
	    h->p[i] = h->p[h->elements] ;
	    found++ ;
	} else
	    i++ ;
    }
    if (found)
	heapify(h);
}

/*
 * helper function to remove a pipe from a heap (can be there at most once)
 */
static void
pipe_remove_from_heap(struct dn_heap *h, struct dn_pipe *p)
{
	int i;

	for (i=0; i < h->elements ; i++ ) {
		if (h->p[i].object == p) { /* found it */
			h->elements-- ;
			h->p[i] = h->p[h->elements] ;
			heapify(h);
			break ;
		}
	}
}

/*
 * drain all queues. Called in case of severe mbuf shortage.
 */
void
dummynet_drain(void)
{
    struct dn_flow_set *fs;
    struct dn_pipe *pipe;
    int i;

    DUMMYNET_LOCK_ASSERT();

    heap_free(&ready_heap);
    heap_free(&wfq_ready_heap);
    heap_free(&extract_heap);
    /* remove all references to this pipe from flow_sets */
    for (i = 0; i < HASHSIZE; i++)
	SLIST_FOREACH(fs, &flowsethash[i], next)
		purge_flow_set(fs, 0);

    for (i = 0; i < HASHSIZE; i++) {
	SLIST_FOREACH(pipe, &pipehash[i], next) {
		purge_flow_set(&(pipe->fs), 0);
		dn_free_pkts(pipe->head);
		pipe->head = pipe->tail = NULL;
	}
    }
}

/*
 * Fully delete a pipe or a queue, cleaning up associated info.
 */
static int
delete_pipe(struct dn_pipe *p)
{

    if (p->pipe_nr == 0 && p->fs.fs_nr == 0)
	return EINVAL ;
    if (p->pipe_nr != 0 && p->fs.fs_nr != 0)
	return EINVAL ;
    if (p->pipe_nr != 0) { /* this is an old-style pipe */
	struct dn_pipe *pipe;
	struct dn_flow_set *fs;
	int i;

	DUMMYNET_LOCK();
	pipe = locate_pipe(p->pipe_nr);	/* locate pipe */

	if (pipe == NULL) {
	    DUMMYNET_UNLOCK();
	    return (ENOENT);	/* not found */
	}

	/* Unlink from list of pipes. */
	SLIST_REMOVE(&pipehash[HASH(pipe->pipe_nr)], pipe, dn_pipe, next);

	/* Remove all references to this pipe from flow_sets. */
	for (i = 0; i < HASHSIZE; i++) {
	    SLIST_FOREACH(fs, &flowsethash[i], next) {
		if (fs->pipe == pipe) {
			printf("dummynet: ++ ref to pipe %d from fs %d\n",
			    p->pipe_nr, fs->fs_nr);
			fs->pipe = NULL ;
			purge_flow_set(fs, 0);
		}
	    }
	}
	fs_remove_from_heap(&ready_heap, &(pipe->fs));
	purge_pipe(pipe); /* remove all data associated to this pipe */
	/* remove reference to here from extract_heap and wfq_ready_heap */
	pipe_remove_from_heap(&extract_heap, pipe);
	pipe_remove_from_heap(&wfq_ready_heap, pipe);
	DUMMYNET_UNLOCK();

	free_pipe(pipe);
    } else { /* this is a WF2Q queue (dn_flow_set) */
	struct dn_flow_set *fs;

	DUMMYNET_LOCK();
	fs = locate_flowset(p->fs.fs_nr); /* locate set */

	if (fs == NULL) {
	    DUMMYNET_UNLOCK();
	    return (ENOENT); /* not found */
	}

	/* Unlink from list of flowsets. */
	SLIST_REMOVE( &flowsethash[HASH(fs->fs_nr)], fs, dn_flow_set, next);

	if (fs->pipe != NULL) {
	    /* Update total weight on parent pipe and cleanup parent heaps. */
	    fs->pipe->sum -= fs->weight * fs->backlogged ;
	    fs_remove_from_heap(&(fs->pipe->not_eligible_heap), fs);
	    fs_remove_from_heap(&(fs->pipe->scheduler_heap), fs);
#if 1	/* XXX should i remove from idle_heap as well ? */
	    fs_remove_from_heap(&(fs->pipe->idle_heap), fs);
#endif
	}
	purge_flow_set(fs, 1);
	DUMMYNET_UNLOCK();
    }
    return 0 ;
}

/*
 * helper function used to copy data from kernel in DUMMYNET_GET
 */
static char *
dn_copy_set(struct dn_flow_set *set, char *bp)
{
    int i, copied = 0 ;
    struct dn_flow_queue *q, *qp = (struct dn_flow_queue *)bp;

    DUMMYNET_LOCK_ASSERT();

    for (i = 0 ; i <= set->rq_size ; i++) {
	for (q = set->rq[i] ; q ; q = q->next, qp++ ) {
	    if (q->hash_slot != i)
		printf("dummynet: ++ at %d: wrong slot (have %d, "
		    "should be %d)\n", copied, q->hash_slot, i);
	    if (q->fs != set)
		printf("dummynet: ++ at %d: wrong fs ptr (have %p, should be %p)\n",
			i, q->fs, set);
	    copied++ ;
	    bcopy(q, qp, sizeof( *q ) );
	    /* cleanup pointers */
	    qp->next = NULL ;
	    qp->head = qp->tail = NULL ;
	    qp->fs = NULL ;
	}
    }
    if (copied != set->rq_elements)
	printf("dummynet: ++ wrong count, have %d should be %d\n",
	    copied, set->rq_elements);
    return (char *)qp ;
}

static size_t
dn_calc_size(void)
{
    struct dn_flow_set *fs;
    struct dn_pipe *pipe;
    size_t size = 0;
    int i;

    DUMMYNET_LOCK_ASSERT();
    /*
     * Compute size of data structures: list of pipes and flow_sets.
     */
    for (i = 0; i < HASHSIZE; i++) {
	SLIST_FOREACH(pipe, &pipehash[i], next)
		size += sizeof(*pipe) +
		    pipe->fs.rq_elements * sizeof(struct dn_flow_queue);
	SLIST_FOREACH(fs, &flowsethash[i], next)
		size += sizeof (*fs) +
		    fs->rq_elements * sizeof(struct dn_flow_queue);
    }
    return size;
}

static int
dummynet_get(struct sockopt *sopt)
{
    char *buf, *bp ; /* bp is the "copy-pointer" */
    size_t size ;
    struct dn_flow_set *fs;
    struct dn_pipe *pipe;
    int error=0, i ;

    /* XXX lock held too long */
    DUMMYNET_LOCK();
    /*
     * XXX: Ugly, but we need to allocate memory with M_WAITOK flag and we
     *      cannot use this flag while holding a mutex.
     */
    for (i = 0; i < 10; i++) {
	size = dn_calc_size();
	DUMMYNET_UNLOCK();
	buf = malloc(size, M_TEMP, M_WAITOK);
	DUMMYNET_LOCK();
	if (size >= dn_calc_size())
		break;
	free(buf, M_TEMP);
	buf = NULL;
    }
    if (buf == NULL) {
	DUMMYNET_UNLOCK();
	return ENOBUFS ;
    }
    bp = buf;
    for (i = 0; i < HASHSIZE; i++) {
	SLIST_FOREACH(pipe, &pipehash[i], next) {
		struct dn_pipe *pipe_bp = (struct dn_pipe *)bp;

		/*
		 * Copy pipe descriptor into *bp, convert delay back to ms,
		 * then copy the flow_set descriptor(s) one at a time.
		 * After each flow_set, copy the queue descriptor it owns.
		 */
		bcopy(pipe, bp, sizeof(*pipe));
		pipe_bp->delay = (pipe_bp->delay * 1000) / hz;
		pipe_bp->burst = div64(pipe_bp->burst, 8 * hz);
		/*
		 * XXX the following is a hack based on ->next being the
		 * first field in dn_pipe and dn_flow_set. The correct
		 * solution would be to move the dn_flow_set to the beginning
		 * of struct dn_pipe.
		 */
		pipe_bp->next.sle_next = (struct dn_pipe *)DN_IS_PIPE;
		/* Clean pointers. */
		pipe_bp->head = pipe_bp->tail = NULL;
		pipe_bp->fs.next.sle_next = NULL;
		pipe_bp->fs.pipe = NULL;
		pipe_bp->fs.rq = NULL;
		pipe_bp->samples = NULL;

		bp += sizeof(*pipe) ;
		bp = dn_copy_set(&(pipe->fs), bp);
	}
    }

    for (i = 0; i < HASHSIZE; i++) {
	SLIST_FOREACH(fs, &flowsethash[i], next) {
		struct dn_flow_set *fs_bp = (struct dn_flow_set *)bp;

		bcopy(fs, bp, sizeof(*fs));
		/* XXX same hack as above */
		fs_bp->next.sle_next = (struct dn_flow_set *)DN_IS_QUEUE;
		fs_bp->pipe = NULL;
		fs_bp->rq = NULL;
		bp += sizeof(*fs);
		bp = dn_copy_set(fs, bp);
	}
    }

    DUMMYNET_UNLOCK();

    error = sooptcopyout(sopt, buf, size);
    free(buf, M_TEMP);
    return error ;
}

/*
 * Handler for the various dummynet socket options (get, flush, config, del)
 */
static int
ip_dn_ctl(struct sockopt *sopt)
{
    int error;
    struct dn_pipe *p = NULL;

    error = priv_check(sopt->sopt_td, PRIV_NETINET_DUMMYNET);
    if (error)
	return (error);

    /* Disallow sets in really-really secure mode. */
    if (sopt->sopt_dir == SOPT_SET) {
#if __FreeBSD_version >= 500034
	error =  securelevel_ge(sopt->sopt_td->td_ucred, 3);
	if (error)
	    return (error);
#else
	if (securelevel >= 3)
	    return (EPERM);
#endif
    }

    switch (sopt->sopt_name) {
    default :
	printf("dummynet: -- unknown option %d", sopt->sopt_name);
	error = EINVAL ;
	break;

    case IP_DUMMYNET_GET :
	error = dummynet_get(sopt);
	break ;

    case IP_DUMMYNET_FLUSH :
	dummynet_flush() ;
	break ;

    case IP_DUMMYNET_CONFIGURE :
	p = malloc(sizeof(struct dn_pipe_max), M_TEMP, M_WAITOK);
	error = sooptcopyin(sopt, p, sizeof(struct dn_pipe_max), sizeof *p);
	if (error)
	    break ;
	if (p->samples_no > 0)
	    p->samples = &(((struct dn_pipe_max *)p)->samples[0]);

	error = config_pipe(p);
	break ;

    case IP_DUMMYNET_DEL :	/* remove a pipe or queue */
	p = malloc(sizeof(struct dn_pipe), M_TEMP, M_WAITOK);
	error = sooptcopyin(sopt, p, sizeof(struct dn_pipe), sizeof *p);
	if (error)
	    break ;

	error = delete_pipe(p);
	break ;
    }

    if (p != NULL)
	free(p, M_TEMP);

    return error ;
}

static void
ip_dn_init(void)
{
	int i;

	if (bootverbose)
		printf("DUMMYNET with IPv6 initialized (040826)\n");

	DUMMYNET_LOCK_INIT();

	for (i = 0; i < HASHSIZE; i++) {
		SLIST_INIT(&pipehash[i]);
		SLIST_INIT(&flowsethash[i]);
	}
	ready_heap.size = ready_heap.elements = 0;
	ready_heap.offset = 0;

	wfq_ready_heap.size = wfq_ready_heap.elements = 0;
	wfq_ready_heap.offset = 0;

	extract_heap.size = extract_heap.elements = 0;
	extract_heap.offset = 0;

	ip_dn_ctl_ptr = ip_dn_ctl;
	ip_dn_io_ptr = dummynet_io;

	TASK_INIT(&dn_task, 0, dummynet_task, NULL);
	dn_tq = taskqueue_create_fast("dummynet", M_NOWAIT,
	    taskqueue_thread_enqueue, &dn_tq);
	taskqueue_start_threads(&dn_tq, 1, PI_NET, "dummynet");

	callout_init(&dn_timeout, CALLOUT_MPSAFE);
	callout_reset(&dn_timeout, 1, dummynet, NULL);

	/* Initialize curr_time adjustment mechanics. */
	getmicrouptime(&prev_t);
}

#ifdef KLD_MODULE
static void
ip_dn_destroy(void)
{
	ip_dn_ctl_ptr = NULL;
	ip_dn_io_ptr = NULL;

	DUMMYNET_LOCK();
	callout_stop(&dn_timeout);
	DUMMYNET_UNLOCK();
	taskqueue_drain(dn_tq, &dn_task);
	taskqueue_free(dn_tq);

	dummynet_flush();

	DUMMYNET_LOCK_DESTROY();
}
#endif /* KLD_MODULE */

static int
dummynet_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		if (ip_dn_io_ptr) {
		    printf("DUMMYNET already loaded\n");
		    return EEXIST ;
		}
		ip_dn_init();
		break;

	case MOD_UNLOAD:
#if !defined(KLD_MODULE)
		printf("dummynet statically compiled, cannot unload\n");
		return EINVAL ;
#else
		ip_dn_destroy();
#endif
		break ;
	default:
		return EOPNOTSUPP;
		break ;
	}
	return 0 ;
}

static moduledata_t dummynet_mod = {
	"dummynet",
	dummynet_modevent,
	NULL
};
DECLARE_MODULE(dummynet, dummynet_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_DEPEND(dummynet, ipfw, 2, 2, 2);
MODULE_VERSION(dummynet, 1);
/* end of file */
