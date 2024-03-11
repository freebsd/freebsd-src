#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_ratelimit.h"
#include "opt_kern_tls.h"
#include <sys/param.h>
#include <sys/arb.h>
#include <sys/module.h>
#include <sys/kernel.h>
#ifdef TCP_HHOOK
#include <sys/hhook.h>
#endif
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#ifdef STATS
#include <sys/qmath.h>
#include <sys/tree.h>
#include <sys/stats.h> /* Must come after qmath.h and tree.h */
#else
#include <sys/tree.h>
#endif
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/tim_filter.h>
#include <sys/smp.h>
#include <sys/kthread.h>
#include <sys/kern_prefetch.h>
#include <sys/protosw.h>
#ifdef TCP_ACCOUNTING
#include <sys/sched.h>
#include <machine/cpu.h>
#endif
#include <vm/uma.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/vnet.h>

#define TCPSTATES		/* for logging */

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* required for icmp_var.h */
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM */
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_syncache.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_ratelimit.h>
#include <netinet/tcp_accounting.h>
#include <netinet/tcpip.h>
#include <netinet/cc/cc.h>
#include <netinet/cc/cc_newreno.h>
#include <netinet/tcp_fastopen.h>
#include <netinet/tcp_lro.h>
#ifdef NETFLIX_SHARED_CWND
#include <netinet/tcp_shared_cwnd.h>
#endif
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcp_ecn.h>

#include <netipsec/ipsec_support.h>

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif				/* IPSEC */

#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <machine/in_cksum.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif
#include "sack_filter.h"
#include "tcp_rack.h"
#include "tailq_hash.h"


struct rack_sendmap *
tqhash_min(struct tailq_hash *hs)
{
	struct rack_sendmap *rsm;

	rsm = tqhash_find(hs, hs->min);
	return(rsm);
}

struct rack_sendmap *
tqhash_max(struct tailq_hash *hs)
{
	struct rack_sendmap *rsm;

	rsm = tqhash_find(hs, (hs->max - 1));
	return (rsm);
}

int
tqhash_empty(struct tailq_hash *hs)
{
	if (hs->count == 0)
		return(1);
	return(0);
}

struct rack_sendmap *
tqhash_find(struct tailq_hash *hs, uint32_t seq)
{
	struct rack_sendmap *e;
	int bindex, pbucket, fc = 1;

	if ((SEQ_LT(seq, hs->min)) ||
	    (hs->count == 0) ||
	    (SEQ_GEQ(seq, hs->max))) {
		/* Not here */
		return (NULL);
	}
	bindex = seq / SEQ_BUCKET_SIZE;
	bindex %= MAX_HASH_ENTRIES;
	/* Lets look through the bucket it belongs to */
	if (TAILQ_EMPTY(&hs->ht[bindex])) {
		goto look_backwards;
	}
	TAILQ_FOREACH(e, &hs->ht[bindex], next) {
		if (fc == 1) {
			/*
			 * Special check for when a cum-ack
			 * as moved up over a seq and now its
			 * a bucket behind where it belongs. In
			 * the case of SACKs which create new rsm's
			 * this won't occur.
			 */
			if (SEQ_GT(e->r_start, seq)) {
				goto look_backwards;
			}
			fc = 0;
		}
		if (SEQ_GEQ(seq, e->r_start) &&
		    (SEQ_LT(seq, e->r_end))) {
			/* Its in this block */
			return (e);
		}
	}
	/* Did not find it */
	return (NULL);
look_backwards:
	if (bindex == 0)
		pbucket = MAX_HASH_ENTRIES - 1;
	else
		pbucket = bindex - 1;
	TAILQ_FOREACH_REVERSE(e, &hs->ht[pbucket], rack_head, next) {
		if (SEQ_GEQ(seq, e->r_start) &&
		    (SEQ_LT(seq, e->r_end))) {
			/* Its in this block */
			return (e);
		}
		if (SEQ_GEQ(e->r_end, seq))
			break;
	}
	return (NULL);
}

struct rack_sendmap *
tqhash_next(struct tailq_hash *hs, struct rack_sendmap *rsm)
{
	struct rack_sendmap *e;

	e = TAILQ_NEXT(rsm, next);
	if (e == NULL) {
		/* Move to next bucket */
		int nxt;

		nxt = rsm->bindex + 1;
		if (nxt >= MAX_HASH_ENTRIES)
			nxt = 0;
		e = TAILQ_FIRST(&hs->ht[nxt]);
	}
	return(e);
}

struct rack_sendmap *
tqhash_prev(struct tailq_hash *hs, struct rack_sendmap *rsm)
{
	struct rack_sendmap *e;

	e = TAILQ_PREV(rsm, rack_head, next);
	if (e == NULL) {
		int prev;

		if (rsm->bindex > 0)
			prev = rsm->bindex - 1;
		else
			prev = MAX_HASH_ENTRIES - 1;
		e = TAILQ_LAST(&hs->ht[prev], rack_head);
	}
	return (e);
}

void
tqhash_remove(struct tailq_hash *hs, struct rack_sendmap *rsm, int type)
{
	TAILQ_REMOVE(&hs->ht[rsm->bindex], rsm, next);
	hs->count--;
	if (hs->count == 0) {
		hs->min = hs->max;
	} else if (type == REMOVE_TYPE_CUMACK) {
		hs->min = rsm->r_end;
	}
}

int
tqhash_insert(struct tailq_hash *hs, struct rack_sendmap *rsm)
{
	struct rack_sendmap *e, *l;
	int inserted = 0;
	uint32_t ebucket;

	if (hs->count > 0) {
		if ((rsm->r_end - hs->min) >  MAX_ALLOWED_SEQ_RANGE) {
			return (-1);
		}
		e = tqhash_find(hs, rsm->r_start);
		if (e) {
			return (-2);
		}
	}
	rsm->bindex = rsm->r_start / SEQ_BUCKET_SIZE;
	rsm->bindex %= MAX_HASH_ENTRIES;
	ebucket = rsm->r_end / SEQ_BUCKET_SIZE;
	ebucket %= MAX_HASH_ENTRIES;
	if (ebucket != rsm->bindex) {
		/* This RSM straddles the bucket boundary */
		rsm->r_flags |= RACK_STRADDLE;
	} else {
		rsm->r_flags &= ~RACK_STRADDLE;
	}
	if (hs->count == 0) {
		/* Special case */
		hs->min = rsm->r_start;
		hs->max = rsm->r_end;
		hs->count = 1;
	} else {
		hs->count++;
		if (SEQ_GT(rsm->r_end, hs->max))
			hs->max = rsm->r_end;
		if (SEQ_LT(rsm->r_start, hs->min))
			hs->min = rsm->r_start;
	}
	/* Check the common case of inserting at the end */
	l = TAILQ_LAST(&hs->ht[rsm->bindex], rack_head);
	if ((l == NULL) || (SEQ_GT(rsm->r_start, l->r_start))) {
		TAILQ_INSERT_TAIL(&hs->ht[rsm->bindex], rsm, next);
		return (0);
	}
	TAILQ_FOREACH(e, &hs->ht[rsm->bindex], next) {
		if (SEQ_LEQ(rsm->r_start, e->r_start)) {
			inserted = 1;
			TAILQ_INSERT_BEFORE(e, rsm, next);
			break;
		}
	}
	if (inserted == 0) {
		TAILQ_INSERT_TAIL(&hs->ht[rsm->bindex], rsm, next);
	}
	return (0);
}

void
tqhash_init(struct tailq_hash *hs)
{
	int i;

	for(i = 0; i < MAX_HASH_ENTRIES; i++) {
		TAILQ_INIT(&hs->ht[i]);
	}
	hs->min = hs->max = 0;
	hs->count = 0;
}

int
tqhash_trim(struct tailq_hash *hs, uint32_t th_ack)
{
	struct rack_sendmap *rsm;

	if (SEQ_LT(th_ack, hs->min)) {
		/* It can't be behind our current min */
		return (-1);
	}
	if (SEQ_GEQ(th_ack, hs->max)) {
		/*  It can't be beyond or at our current max */
		return (-2);
	}
	rsm = tqhash_min(hs);
	if (rsm == NULL) {
		/* nothing to trim */
		return (-3);
	}
	if (SEQ_GEQ(th_ack, rsm->r_end)) {
		/*
		 * You can't trim all bytes instead
		 * you need to remove it.
		 */
		return (-4);
	}
	if (SEQ_GT(th_ack, hs->min))
	    hs->min = th_ack;
	/*
	 * Should we trim it for the caller?
	 * they may have already which is ok...
	 */
	if (SEQ_GT(th_ack, rsm->r_start)) {
		rsm->r_start = th_ack;
	}
	return (0);
}

