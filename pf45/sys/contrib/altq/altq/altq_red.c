/*	$FreeBSD$	*/
/*	$KAME: altq_red.c,v 1.18 2003/09/05 22:40:36 itojun Exp $	*/

/*
 * Copyright (C) 1997-2003
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Copyright (c) 1990-1994 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include "opt_altq.h"
#if (__FreeBSD__ != 2)
#include "opt_inet.h"
#ifdef __FreeBSD__
#include "opt_inet6.h"
#endif
#endif
#endif /* __FreeBSD__ || __NetBSD__ */
#ifdef ALTQ_RED	/* red is enabled by ALTQ_RED option in opt_altq.h */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/errno.h>
#if 1 /* ALTQ3_COMPAT */
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#ifdef ALTQ_FLOWVALVE
#include <sys/queue.h>
#include <sys/time.h>
#endif
#endif /* ALTQ3_COMPAT */

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/pfvar.h>
#include <altq/altq.h>
#include <altq/altq_red.h>
#ifdef ALTQ3_COMPAT
#include <altq/altq_conf.h>
#ifdef ALTQ_FLOWVALVE
#include <altq/altq_flowvalve.h>
#endif
#endif

/*
 * ALTQ/RED (Random Early Detection) implementation using 32-bit
 * fixed-point calculation.
 *
 * written by kjc using the ns code as a reference.
 * you can learn more about red and ns from Sally's home page at
 * http://www-nrg.ee.lbl.gov/floyd/
 *
 * most of the red parameter values are fixed in this implementation
 * to prevent fixed-point overflow/underflow.
 * if you change the parameters, watch out for overflow/underflow!
 *
 * the parameters used are recommended values by Sally.
 * the corresponding ns config looks:
 *	q_weight=0.00195
 *	minthresh=5 maxthresh=15 queue-size=60
 *	linterm=30
 *	dropmech=drop-tail
 *	bytes=false (can't be handled by 32-bit fixed-point)
 *	doubleq=false dqthresh=false
 *	wait=true
 */
/*
 * alternative red parameters for a slow link.
 *
 * assume the queue length becomes from zero to L and keeps L, it takes
 * N packets for q_avg to reach 63% of L.
 * when q_weight is 0.002, N is about 500 packets.
 * for a slow link like dial-up, 500 packets takes more than 1 minute!
 * when q_weight is 0.008, N is about 127 packets.
 * when q_weight is 0.016, N is about 63 packets.
 * bursts of 50 packets are allowed for 0.002, bursts of 25 packets
 * are allowed for 0.016.
 * see Sally's paper for more details.
 */
/* normal red parameters */
#define	W_WEIGHT	512	/* inverse of weight of EWMA (511/512) */
				/* q_weight = 0.00195 */

/* red parameters for a slow link */
#define	W_WEIGHT_1	128	/* inverse of weight of EWMA (127/128) */
				/* q_weight = 0.0078125 */

/* red parameters for a very slow link (e.g., dialup) */
#define	W_WEIGHT_2	64	/* inverse of weight of EWMA (63/64) */
				/* q_weight = 0.015625 */

/* fixed-point uses 12-bit decimal places */
#define	FP_SHIFT	12	/* fixed-point shift */

/* red parameters for drop probability */
#define	INV_P_MAX	10	/* inverse of max drop probability */
#define	TH_MIN		5	/* min threshold */
#define	TH_MAX		15	/* max threshold */

#define	RED_LIMIT	60	/* default max queue lenght */
#define	RED_STATS		/* collect statistics */

/*
 * our default policy for forced-drop is drop-tail.
 * (in altq-1.1.2 or earlier, the default was random-drop.
 * but it makes more sense to punish the cause of the surge.)
 * to switch to the random-drop policy, define "RED_RANDOM_DROP".
 */

#ifdef ALTQ3_COMPAT
#ifdef ALTQ_FLOWVALVE
/*
 * flow-valve is an extention to protect red from unresponsive flows
 * and to promote end-to-end congestion control.
 * flow-valve observes the average drop rates of the flows that have
 * experienced packet drops in the recent past.
 * when the average drop rate exceeds the threshold, the flow is
 * blocked by the flow-valve.  the trapped flow should back off
 * exponentially to escape from the flow-valve.
 */
#ifdef RED_RANDOM_DROP
#error "random-drop can't be used with flow-valve!"
#endif
#endif /* ALTQ_FLOWVALVE */

/* red_list keeps all red_queue_t's allocated. */
static red_queue_t *red_list = NULL;

#endif /* ALTQ3_COMPAT */

/* default red parameter values */
static int default_th_min = TH_MIN;
static int default_th_max = TH_MAX;
static int default_inv_pmax = INV_P_MAX;

#ifdef ALTQ3_COMPAT
/* internal function prototypes */
static int red_enqueue(struct ifaltq *, struct mbuf *, struct altq_pktattr *);
static struct mbuf *red_dequeue(struct ifaltq *, int);
static int red_request(struct ifaltq *, int, void *);
static void red_purgeq(red_queue_t *);
static int red_detach(red_queue_t *);
#ifdef ALTQ_FLOWVALVE
static __inline struct fve *flowlist_lookup(struct flowvalve *,
			 struct altq_pktattr *, struct timeval *);
static __inline struct fve *flowlist_reclaim(struct flowvalve *,
					     struct altq_pktattr *);
static __inline void flowlist_move_to_head(struct flowvalve *, struct fve *);
static __inline int fv_p2f(struct flowvalve *, int);
#if 0 /* XXX: make the compiler happy (fv_alloc unused) */
static struct flowvalve *fv_alloc(struct red *);
#endif
static void fv_destroy(struct flowvalve *);
static int fv_checkflow(struct flowvalve *, struct altq_pktattr *,
			struct fve **);
static void fv_dropbyred(struct flowvalve *fv, struct altq_pktattr *,
			 struct fve *);
#endif
#endif /* ALTQ3_COMPAT */

/*
 * red support routines
 */
red_t *
red_alloc(int weight, int inv_pmax, int th_min, int th_max, int flags,
   int pkttime)
{
	red_t	*rp;
	int	 w, i;
	int	 npkts_per_sec;

	rp = malloc(sizeof(red_t), M_DEVBUF, M_WAITOK);
	if (rp == NULL)
		return (NULL);
	bzero(rp, sizeof(red_t));

	rp->red_avg = 0;
	rp->red_idle = 1;

	if (weight == 0)
		rp->red_weight = W_WEIGHT;
	else
		rp->red_weight = weight;
	if (inv_pmax == 0)
		rp->red_inv_pmax = default_inv_pmax;
	else
		rp->red_inv_pmax = inv_pmax;
	if (th_min == 0)
		rp->red_thmin = default_th_min;
	else
		rp->red_thmin = th_min;
	if (th_max == 0)
		rp->red_thmax = default_th_max;
	else
		rp->red_thmax = th_max;

	rp->red_flags = flags;

	if (pkttime == 0)
		/* default packet time: 1000 bytes / 10Mbps * 8 * 1000000 */
		rp->red_pkttime = 800;
	else
		rp->red_pkttime = pkttime;

	if (weight == 0) {
		/* when the link is very slow, adjust red parameters */
		npkts_per_sec = 1000000 / rp->red_pkttime;
		if (npkts_per_sec < 50) {
			/* up to about 400Kbps */
			rp->red_weight = W_WEIGHT_2;
		} else if (npkts_per_sec < 300) {
			/* up to about 2.4Mbps */
			rp->red_weight = W_WEIGHT_1;
		}
	}

	/* calculate wshift.  weight must be power of 2 */
	w = rp->red_weight;
	for (i = 0; w > 1; i++)
		w = w >> 1;
	rp->red_wshift = i;
	w = 1 << rp->red_wshift;
	if (w != rp->red_weight) {
		printf("invalid weight value %d for red! use %d\n",
		       rp->red_weight, w);
		rp->red_weight = w;
	}

	/*
	 * thmin_s and thmax_s are scaled versions of th_min and th_max
	 * to be compared with avg.
	 */
	rp->red_thmin_s = rp->red_thmin << (rp->red_wshift + FP_SHIFT);
	rp->red_thmax_s = rp->red_thmax << (rp->red_wshift + FP_SHIFT);

	/*
	 * precompute probability denominator
	 *  probd = (2 * (TH_MAX-TH_MIN) / pmax) in fixed-point
	 */
	rp->red_probd = (2 * (rp->red_thmax - rp->red_thmin)
			 * rp->red_inv_pmax) << FP_SHIFT;

	/* allocate weight table */
	rp->red_wtab = wtab_alloc(rp->red_weight);

	microtime(&rp->red_last);
	return (rp);
}

void
red_destroy(red_t *rp)
{
#ifdef ALTQ3_COMPAT
#ifdef ALTQ_FLOWVALVE
	if (rp->red_flowvalve != NULL)
		fv_destroy(rp->red_flowvalve);
#endif
#endif /* ALTQ3_COMPAT */
	wtab_destroy(rp->red_wtab);
	free(rp, M_DEVBUF);
}

void
red_getstats(red_t *rp, struct redstats *sp)
{
	sp->q_avg		= rp->red_avg >> rp->red_wshift;
	sp->xmit_cnt		= rp->red_stats.xmit_cnt;
	sp->drop_cnt		= rp->red_stats.drop_cnt;
	sp->drop_forced		= rp->red_stats.drop_forced;
	sp->drop_unforced	= rp->red_stats.drop_unforced;
	sp->marked_packets	= rp->red_stats.marked_packets;
}

int
red_addq(red_t *rp, class_queue_t *q, struct mbuf *m,
    struct altq_pktattr *pktattr)
{
	int avg, droptype;
	int n;
#ifdef ALTQ3_COMPAT
#ifdef ALTQ_FLOWVALVE
	struct fve *fve = NULL;

	if (rp->red_flowvalve != NULL && rp->red_flowvalve->fv_flows > 0)
		if (fv_checkflow(rp->red_flowvalve, pktattr, &fve)) {
			m_freem(m);
			return (-1);
		}
#endif
#endif /* ALTQ3_COMPAT */

	avg = rp->red_avg;

	/*
	 * if we were idle, we pretend that n packets arrived during
	 * the idle period.
	 */
	if (rp->red_idle) {
		struct timeval now;
		int t;

		rp->red_idle = 0;
		microtime(&now);
		t = (now.tv_sec - rp->red_last.tv_sec);
		if (t > 60) {
			/*
			 * being idle for more than 1 minute, set avg to zero.
			 * this prevents t from overflow.
			 */
			avg = 0;
		} else {
			t = t * 1000000 + (now.tv_usec - rp->red_last.tv_usec);
			n = t / rp->red_pkttime - 1;

			/* the following line does (avg = (1 - Wq)^n * avg) */
			if (n > 0)
				avg = (avg >> FP_SHIFT) *
				    pow_w(rp->red_wtab, n);
		}
	}

	/* run estimator. (note: avg is scaled by WEIGHT in fixed-point) */
	avg += (qlen(q) << FP_SHIFT) - (avg >> rp->red_wshift);
	rp->red_avg = avg;		/* save the new value */

	/*
	 * red_count keeps a tally of arriving traffic that has not
	 * been dropped.
	 */
	rp->red_count++;

	/* see if we drop early */
	droptype = DTYPE_NODROP;
	if (avg >= rp->red_thmin_s && qlen(q) > 1) {
		if (avg >= rp->red_thmax_s) {
			/* avg >= th_max: forced drop */
			droptype = DTYPE_FORCED;
		} else if (rp->red_old == 0) {
			/* first exceeds th_min */
			rp->red_count = 1;
			rp->red_old = 1;
		} else if (drop_early((avg - rp->red_thmin_s) >> rp->red_wshift,
				      rp->red_probd, rp->red_count)) {
			/* mark or drop by red */
			if ((rp->red_flags & REDF_ECN) &&
			    mark_ecn(m, pktattr, rp->red_flags)) {
				/* successfully marked.  do not drop. */
				rp->red_count = 0;
#ifdef RED_STATS
				rp->red_stats.marked_packets++;
#endif
			} else {
				/* unforced drop by red */
				droptype = DTYPE_EARLY;
			}
		}
	} else {
		/* avg < th_min */
		rp->red_old = 0;
	}

	/*
	 * if the queue length hits the hard limit, it's a forced drop.
	 */
	if (droptype == DTYPE_NODROP && qlen(q) >= qlimit(q))
		droptype = DTYPE_FORCED;

#ifdef RED_RANDOM_DROP
	/* if successful or forced drop, enqueue this packet. */
	if (droptype != DTYPE_EARLY)
		_addq(q, m);
#else
	/* if successful, enqueue this packet. */
	if (droptype == DTYPE_NODROP)
		_addq(q, m);
#endif
	if (droptype != DTYPE_NODROP) {
		if (droptype == DTYPE_EARLY) {
			/* drop the incoming packet */
#ifdef RED_STATS
			rp->red_stats.drop_unforced++;
#endif
		} else {
			/* forced drop, select a victim packet in the queue. */
#ifdef RED_RANDOM_DROP
			m = _getq_random(q);
#endif
#ifdef RED_STATS
			rp->red_stats.drop_forced++;
#endif
		}
#ifdef RED_STATS
		PKTCNTR_ADD(&rp->red_stats.drop_cnt, m_pktlen(m));
#endif
		rp->red_count = 0;
#ifdef ALTQ3_COMPAT
#ifdef ALTQ_FLOWVALVE
		if (rp->red_flowvalve != NULL)
			fv_dropbyred(rp->red_flowvalve, pktattr, fve);
#endif
#endif /* ALTQ3_COMPAT */
		m_freem(m);
		return (-1);
	}
	/* successfully queued */
#ifdef RED_STATS
	PKTCNTR_ADD(&rp->red_stats.xmit_cnt, m_pktlen(m));
#endif
	return (0);
}

/*
 * early-drop probability is calculated as follows:
 *   prob = p_max * (avg - th_min) / (th_max - th_min)
 *   prob_a = prob / (2 - count*prob)
 *	    = (avg-th_min) / (2*(th_max-th_min)*inv_p_max - count*(avg-th_min))
 * here prob_a increases as successive undrop count increases.
 * (prob_a starts from prob/2, becomes prob when (count == (1 / prob)),
 * becomes 1 when (count >= (2 / prob))).
 */
int
drop_early(int fp_len, int fp_probd, int count)
{
	int	d;		/* denominator of drop-probability */

	d = fp_probd - count * fp_len;
	if (d <= 0)
		/* count exceeds the hard limit: drop or mark */
		return (1);

	/*
	 * now the range of d is [1..600] in fixed-point. (when
	 * th_max-th_min=10 and p_max=1/30)
	 * drop probability = (avg - TH_MIN) / d
	 */

	if ((arc4random() % d) < fp_len) {
		/* drop or mark */
		return (1);
	}
	/* no drop/mark */
	return (0);
}

/*
 * try to mark CE bit to the packet.
 *    returns 1 if successfully marked, 0 otherwise.
 */
int
mark_ecn(struct mbuf *m, struct altq_pktattr *pktattr, int flags)
{
	struct mbuf	*m0;
	struct pf_mtag	*at;
	void		*hdr;

	at = pf_find_mtag(m);
	if (at != NULL) {
		hdr = at->hdr;
#ifdef ALTQ3_COMPAT
	} else if (pktattr != NULL) {
		af = pktattr->pattr_af;
		hdr = pktattr->pattr_hdr;
#endif /* ALTQ3_COMPAT */
	} else
		return (0);

	/* verify that pattr_hdr is within the mbuf data */
	for (m0 = m; m0 != NULL; m0 = m0->m_next)
		if (((caddr_t)hdr >= m0->m_data) &&
		    ((caddr_t)hdr < m0->m_data + m0->m_len))
			break;
	if (m0 == NULL) {
		/* ick, tag info is stale */
		return (0);
	}

	switch (((struct ip *)hdr)->ip_v) {
	case IPVERSION:
		if (flags & REDF_ECN4) {
			struct ip *ip = hdr;
			u_int8_t otos;
			int sum;

			if (ip->ip_v != 4)
				return (0);	/* version mismatch! */

			if ((ip->ip_tos & IPTOS_ECN_MASK) == IPTOS_ECN_NOTECT)
				return (0);	/* not-ECT */
			if ((ip->ip_tos & IPTOS_ECN_MASK) == IPTOS_ECN_CE)
				return (1);	/* already marked */

			/*
			 * ecn-capable but not marked,
			 * mark CE and update checksum
			 */
			otos = ip->ip_tos;
			ip->ip_tos |= IPTOS_ECN_CE;
			/*
			 * update checksum (from RFC1624)
			 *	   HC' = ~(~HC + ~m + m')
			 */
			sum = ~ntohs(ip->ip_sum) & 0xffff;
			sum += (~otos & 0xffff) + ip->ip_tos;
			sum = (sum >> 16) + (sum & 0xffff);
			sum += (sum >> 16);  /* add carry */
			ip->ip_sum = htons(~sum & 0xffff);
			return (1);
		}
		break;
#ifdef INET6
	case 6:
		if (flags & REDF_ECN6) {
			struct ip6_hdr *ip6 = hdr;
			u_int32_t flowlabel;

			flowlabel = ntohl(ip6->ip6_flow);
			if ((flowlabel >> 28) != 6)
				return (0);	/* version mismatch! */
			if ((flowlabel & (IPTOS_ECN_MASK << 20)) ==
			    (IPTOS_ECN_NOTECT << 20))
				return (0);	/* not-ECT */
			if ((flowlabel & (IPTOS_ECN_MASK << 20)) ==
			    (IPTOS_ECN_CE << 20))
				return (1);	/* already marked */
			/*
			 * ecn-capable but not marked,  mark CE
			 */
			flowlabel |= (IPTOS_ECN_CE << 20);
			ip6->ip6_flow = htonl(flowlabel);
			return (1);
		}
		break;
#endif  /* INET6 */
	}

	/* not marked */
	return (0);
}

struct mbuf *
red_getq(rp, q)
	red_t *rp;
	class_queue_t *q;
{
	struct mbuf *m;

	if ((m = _getq(q)) == NULL) {
		if (rp->red_idle == 0) {
			rp->red_idle = 1;
			microtime(&rp->red_last);
		}
		return NULL;
	}

	rp->red_idle = 0;
	return (m);
}

/*
 * helper routine to calibrate avg during idle.
 * pow_w(wtab, n) returns (1 - Wq)^n in fixed-point
 * here Wq = 1/weight and the code assumes Wq is close to zero.
 *
 * w_tab[n] holds ((1 - Wq)^(2^n)) in fixed-point.
 */
static struct wtab *wtab_list = NULL;	/* pointer to wtab list */

struct wtab *
wtab_alloc(int weight)
{
	struct wtab	*w;
	int		 i;

	for (w = wtab_list; w != NULL; w = w->w_next)
		if (w->w_weight == weight) {
			w->w_refcount++;
			return (w);
		}

	w = malloc(sizeof(struct wtab), M_DEVBUF, M_WAITOK);
	if (w == NULL)
		panic("wtab_alloc: malloc failed!");
	bzero(w, sizeof(struct wtab));
	w->w_weight = weight;
	w->w_refcount = 1;
	w->w_next = wtab_list;
	wtab_list = w;

	/* initialize the weight table */
	w->w_tab[0] = ((weight - 1) << FP_SHIFT) / weight;
	for (i = 1; i < 32; i++) {
		w->w_tab[i] = (w->w_tab[i-1] * w->w_tab[i-1]) >> FP_SHIFT;
		if (w->w_tab[i] == 0 && w->w_param_max == 0)
			w->w_param_max = 1 << i;
	}

	return (w);
}

int
wtab_destroy(struct wtab *w)
{
	struct wtab	*prev;

	if (--w->w_refcount > 0)
		return (0);

	if (wtab_list == w)
		wtab_list = w->w_next;
	else for (prev = wtab_list; prev->w_next != NULL; prev = prev->w_next)
		if (prev->w_next == w) {
			prev->w_next = w->w_next;
			break;
		}

	free(w, M_DEVBUF);
	return (0);
}

int32_t
pow_w(struct wtab *w, int n)
{
	int	i, bit;
	int32_t	val;

	if (n >= w->w_param_max)
		return (0);

	val = 1 << FP_SHIFT;
	if (n <= 0)
		return (val);

	bit = 1;
	i = 0;
	while (n) {
		if (n & bit) {
			val = (val * w->w_tab[i]) >> FP_SHIFT;
			n &= ~bit;
		}
		i++;
		bit <<=  1;
	}
	return (val);
}

#ifdef ALTQ3_COMPAT
/*
 * red device interface
 */
altqdev_decl(red);

int
redopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
#if (__FreeBSD_version > 500000)
	struct thread *p;
#else
	struct proc *p;
#endif
{
	/* everything will be done when the queueing scheme is attached. */
	return 0;
}

int
redclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
#if (__FreeBSD_version > 500000)
	struct thread *p;
#else
	struct proc *p;
#endif
{
	red_queue_t *rqp;
	int err, error = 0;

	while ((rqp = red_list) != NULL) {
		/* destroy all */
		err = red_detach(rqp);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
redioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
#if (__FreeBSD_version > 500000)
	struct thread *p;
#else
	struct proc *p;
#endif
{
	red_queue_t *rqp;
	struct red_interface *ifacep;
	struct ifnet *ifp;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	case RED_GETSTATS:
		break;
	default:
#if (__FreeBSD_version > 700000)
		if ((error = priv_check(p, PRIV_ALTQ_MANAGE)) != 0)
#elsif (__FreeBSD_version > 400000)
		if ((error = suser(p)) != 0)
#else
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
#endif
			return (error);
		break;
	}

	switch (cmd) {

	case RED_ENABLE:
		ifacep = (struct red_interface *)addr;
		if ((rqp = altq_lookup(ifacep->red_ifname, ALTQT_RED)) == NULL) {
			error = EBADF;
			break;
		}
		error = altq_enable(rqp->rq_ifq);
		break;

	case RED_DISABLE:
		ifacep = (struct red_interface *)addr;
		if ((rqp = altq_lookup(ifacep->red_ifname, ALTQT_RED)) == NULL) {
			error = EBADF;
			break;
		}
		error = altq_disable(rqp->rq_ifq);
		break;

	case RED_IF_ATTACH:
		ifp = ifunit(((struct red_interface *)addr)->red_ifname);
		if (ifp == NULL) {
			error = ENXIO;
			break;
		}

		/* allocate and initialize red_queue_t */
		rqp = malloc(sizeof(red_queue_t), M_DEVBUF, M_WAITOK);
		if (rqp == NULL) {
			error = ENOMEM;
			break;
		}
		bzero(rqp, sizeof(red_queue_t));

		rqp->rq_q = malloc(sizeof(class_queue_t),
		       M_DEVBUF, M_WAITOK);
		if (rqp->rq_q == NULL) {
			free(rqp, M_DEVBUF);
			error = ENOMEM;
			break;
		}
		bzero(rqp->rq_q, sizeof(class_queue_t));

		rqp->rq_red = red_alloc(0, 0, 0, 0, 0, 0);
		if (rqp->rq_red == NULL) {
			free(rqp->rq_q, M_DEVBUF);
			free(rqp, M_DEVBUF);
			error = ENOMEM;
			break;
		}

		rqp->rq_ifq = &ifp->if_snd;
		qtail(rqp->rq_q) = NULL;
		qlen(rqp->rq_q) = 0;
		qlimit(rqp->rq_q) = RED_LIMIT;
		qtype(rqp->rq_q) = Q_RED;

		/*
		 * set RED to this ifnet structure.
		 */
		error = altq_attach(rqp->rq_ifq, ALTQT_RED, rqp,
				    red_enqueue, red_dequeue, red_request,
				    NULL, NULL);
		if (error) {
			red_destroy(rqp->rq_red);
			free(rqp->rq_q, M_DEVBUF);
			free(rqp, M_DEVBUF);
			break;
		}

		/* add this state to the red list */
		rqp->rq_next = red_list;
		red_list = rqp;
		break;

	case RED_IF_DETACH:
		ifacep = (struct red_interface *)addr;
		if ((rqp = altq_lookup(ifacep->red_ifname, ALTQT_RED)) == NULL) {
			error = EBADF;
			break;
		}
		error = red_detach(rqp);
		break;

	case RED_GETSTATS:
		do {
			struct red_stats *q_stats;
			red_t *rp;

			q_stats = (struct red_stats *)addr;
			if ((rqp = altq_lookup(q_stats->iface.red_ifname,
					     ALTQT_RED)) == NULL) {
				error = EBADF;
				break;
			}

			q_stats->q_len 	   = qlen(rqp->rq_q);
			q_stats->q_limit   = qlimit(rqp->rq_q);

			rp = rqp->rq_red;
			q_stats->q_avg 	   = rp->red_avg >> rp->red_wshift;
			q_stats->xmit_cnt  = rp->red_stats.xmit_cnt;
			q_stats->drop_cnt  = rp->red_stats.drop_cnt;
			q_stats->drop_forced   = rp->red_stats.drop_forced;
			q_stats->drop_unforced = rp->red_stats.drop_unforced;
			q_stats->marked_packets = rp->red_stats.marked_packets;

			q_stats->weight		= rp->red_weight;
			q_stats->inv_pmax	= rp->red_inv_pmax;
			q_stats->th_min		= rp->red_thmin;
			q_stats->th_max		= rp->red_thmax;

#ifdef ALTQ_FLOWVALVE
			if (rp->red_flowvalve != NULL) {
				struct flowvalve *fv = rp->red_flowvalve;
				q_stats->fv_flows    = fv->fv_flows;
				q_stats->fv_pass     = fv->fv_stats.pass;
				q_stats->fv_predrop  = fv->fv_stats.predrop;
				q_stats->fv_alloc    = fv->fv_stats.alloc;
				q_stats->fv_escape   = fv->fv_stats.escape;
			} else {
#endif /* ALTQ_FLOWVALVE */
				q_stats->fv_flows    = 0;
				q_stats->fv_pass     = 0;
				q_stats->fv_predrop  = 0;
				q_stats->fv_alloc    = 0;
				q_stats->fv_escape   = 0;
#ifdef ALTQ_FLOWVALVE
			}
#endif /* ALTQ_FLOWVALVE */
		} while (/*CONSTCOND*/ 0);
		break;

	case RED_CONFIG:
		do {
			struct red_conf *fc;
			red_t *new;
			int s, limit;

			fc = (struct red_conf *)addr;
			if ((rqp = altq_lookup(fc->iface.red_ifname,
					       ALTQT_RED)) == NULL) {
				error = EBADF;
				break;
			}
			new = red_alloc(fc->red_weight,
					fc->red_inv_pmax,
					fc->red_thmin,
					fc->red_thmax,
					fc->red_flags,
					fc->red_pkttime);
			if (new == NULL) {
				error = ENOMEM;
				break;
			}

#ifdef __NetBSD__
			s = splnet();
#else
			s = splimp();
#endif
			red_purgeq(rqp);
			limit = fc->red_limit;
			if (limit < fc->red_thmax)
				limit = fc->red_thmax;
			qlimit(rqp->rq_q) = limit;
			fc->red_limit = limit;	/* write back the new value */

			red_destroy(rqp->rq_red);
			rqp->rq_red = new;

			splx(s);

			/* write back new values */
			fc->red_limit = limit;
			fc->red_inv_pmax = rqp->rq_red->red_inv_pmax;
			fc->red_thmin = rqp->rq_red->red_thmin;
			fc->red_thmax = rqp->rq_red->red_thmax;

		} while (/*CONSTCOND*/ 0);
		break;

	case RED_SETDEFAULTS:
		do {
			struct redparams *rp;

			rp = (struct redparams *)addr;

			default_th_min = rp->th_min;
			default_th_max = rp->th_max;
			default_inv_pmax = rp->inv_pmax;
		} while (/*CONSTCOND*/ 0);
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

static int
red_detach(rqp)
	red_queue_t *rqp;
{
	red_queue_t *tmp;
	int error = 0;

	if (ALTQ_IS_ENABLED(rqp->rq_ifq))
		altq_disable(rqp->rq_ifq);

	if ((error = altq_detach(rqp->rq_ifq)))
		return (error);

	if (red_list == rqp)
		red_list = rqp->rq_next;
	else {
		for (tmp = red_list; tmp != NULL; tmp = tmp->rq_next)
			if (tmp->rq_next == rqp) {
				tmp->rq_next = rqp->rq_next;
				break;
			}
		if (tmp == NULL)
			printf("red_detach: no state found in red_list!\n");
	}

	red_destroy(rqp->rq_red);
	free(rqp->rq_q, M_DEVBUF);
	free(rqp, M_DEVBUF);
	return (error);
}

/*
 * enqueue routine:
 *
 *	returns: 0 when successfully queued.
 *		 ENOBUFS when drop occurs.
 */
static int
red_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	red_queue_t *rqp = (red_queue_t *)ifq->altq_disc;

	IFQ_LOCK_ASSERT(ifq);

	if (red_addq(rqp->rq_red, rqp->rq_q, m, pktattr) < 0)
		return ENOBUFS;
	ifq->ifq_len++;
	return 0;
}

/*
 * dequeue routine:
 *	must be called in splimp.
 *
 *	returns: mbuf dequeued.
 *		 NULL when no packet is available in the queue.
 */

static struct mbuf *
red_dequeue(ifq, op)
	struct ifaltq *ifq;
	int op;
{
	red_queue_t *rqp = (red_queue_t *)ifq->altq_disc;
	struct mbuf *m;

	IFQ_LOCK_ASSERT(ifq);

	if (op == ALTDQ_POLL)
		return qhead(rqp->rq_q);

	/* op == ALTDQ_REMOVE */
	m =  red_getq(rqp->rq_red, rqp->rq_q);
	if (m != NULL)
		ifq->ifq_len--;
	return (m);
}

static int
red_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	red_queue_t *rqp = (red_queue_t *)ifq->altq_disc;

	IFQ_LOCK_ASSERT(ifq);

	switch (req) {
	case ALTRQ_PURGE:
		red_purgeq(rqp);
		break;
	}
	return (0);
}

static void
red_purgeq(rqp)
	red_queue_t *rqp;
{
	_flushq(rqp->rq_q);
	if (ALTQ_IS_ENABLED(rqp->rq_ifq))
		rqp->rq_ifq->ifq_len = 0;
}

#ifdef ALTQ_FLOWVALVE

#define	FV_PSHIFT	7	/* weight of average drop rate -- 1/128 */
#define	FV_PSCALE(x)	((x) << FV_PSHIFT)
#define	FV_PUNSCALE(x)	((x) >> FV_PSHIFT)
#define	FV_FSHIFT	5	/* weight of average fraction -- 1/32 */
#define	FV_FSCALE(x)	((x) << FV_FSHIFT)
#define	FV_FUNSCALE(x)	((x) >> FV_FSHIFT)

#define	FV_TIMER	(3 * hz)	/* timer value for garbage collector */
#define	FV_FLOWLISTSIZE		64	/* how many flows in flowlist */

#define	FV_N			10	/* update fve_f every FV_N packets */

#define	FV_BACKOFFTHRESH	1  /* backoff threshold interval in second */
#define	FV_TTHRESH		3  /* time threshold to delete fve */
#define	FV_ALPHA		5  /* extra packet count */

#define	FV_STATS

#if (__FreeBSD_version > 300000)
#define	FV_TIMESTAMP(tp)	getmicrotime(tp)
#else
#define	FV_TIMESTAMP(tp)	{ (*(tp)) = time; }
#endif

/*
 * Brtt table: 127 entry table to convert drop rate (p) to
 * the corresponding bandwidth fraction (f)
 * the following equation is implemented to use scaled values,
 * fve_p and fve_f, in the fixed point format.
 *
 *   Brtt(p) = 1 /(sqrt(4*p/3) + min(1,3*sqrt(p*6/8)) * p * (1+32 * p*p))
 *   f = Brtt(p) / (max_th + alpha)
 */
#define	BRTT_SIZE	128
#define	BRTT_SHIFT	12
#define	BRTT_MASK	0x0007f000
#define	BRTT_PMAX	(1 << (FV_PSHIFT + FP_SHIFT))

const int brtt_tab[BRTT_SIZE] = {
	0, 1262010, 877019, 703694, 598706, 525854, 471107, 427728,
	392026, 361788, 335598, 312506, 291850, 273158, 256081, 240361,
	225800, 212247, 199585, 187788, 178388, 169544, 161207, 153333,
	145888, 138841, 132165, 125836, 119834, 114141, 108739, 103612,
	98747, 94129, 89746, 85585, 81637, 77889, 74333, 70957,
	67752, 64711, 61824, 59084, 56482, 54013, 51667, 49440,
	47325, 45315, 43406, 41591, 39866, 38227, 36667, 35184,
	33773, 32430, 31151, 29933, 28774, 27668, 26615, 25611,
	24653, 23740, 22868, 22035, 21240, 20481, 19755, 19062,
	18399, 17764, 17157, 16576, 16020, 15487, 14976, 14487,
	14017, 13567, 13136, 12721, 12323, 11941, 11574, 11222,
	10883, 10557, 10243, 9942, 9652, 9372, 9103, 8844,
	8594, 8354, 8122, 7898, 7682, 7474, 7273, 7079,
	6892, 6711, 6536, 6367, 6204, 6046, 5893, 5746,
	5603, 5464, 5330, 5201, 5075, 4954, 4836, 4722,
	4611, 4504, 4400, 4299, 4201, 4106, 4014, 3924
};

static __inline struct fve *
flowlist_lookup(fv, pktattr, now)
	struct flowvalve *fv;
	struct altq_pktattr *pktattr;
	struct timeval *now;
{
	struct fve *fve;
	int flows;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct timeval tthresh;

	if (pktattr == NULL)
		return (NULL);

	tthresh.tv_sec = now->tv_sec - FV_TTHRESH;
	flows = 0;
	/*
	 * search the flow list
	 */
	switch (pktattr->pattr_af) {
	case AF_INET:
		ip = (struct ip *)pktattr->pattr_hdr;
		TAILQ_FOREACH(fve, &fv->fv_flowlist, fve_lru){
			if (fve->fve_lastdrop.tv_sec == 0)
				break;
			if (fve->fve_lastdrop.tv_sec < tthresh.tv_sec) {
				fve->fve_lastdrop.tv_sec = 0;
				break;
			}
			if (fve->fve_flow.flow_af == AF_INET &&
			    fve->fve_flow.flow_ip.ip_src.s_addr ==
			    ip->ip_src.s_addr &&
			    fve->fve_flow.flow_ip.ip_dst.s_addr ==
			    ip->ip_dst.s_addr)
				return (fve);
			flows++;
		}
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = (struct ip6_hdr *)pktattr->pattr_hdr;
		TAILQ_FOREACH(fve, &fv->fv_flowlist, fve_lru){
			if (fve->fve_lastdrop.tv_sec == 0)
				break;
			if (fve->fve_lastdrop.tv_sec < tthresh.tv_sec) {
				fve->fve_lastdrop.tv_sec = 0;
				break;
			}
			if (fve->fve_flow.flow_af == AF_INET6 &&
			    IN6_ARE_ADDR_EQUAL(&fve->fve_flow.flow_ip6.ip6_src,
					       &ip6->ip6_src) &&
			    IN6_ARE_ADDR_EQUAL(&fve->fve_flow.flow_ip6.ip6_dst,
					       &ip6->ip6_dst))
				return (fve);
			flows++;
		}
		break;
#endif /* INET6 */

	default:
		/* unknown protocol.  no drop. */
		return (NULL);
	}
	fv->fv_flows = flows;	/* save the number of active fve's */
	return (NULL);
}

static __inline struct fve *
flowlist_reclaim(fv, pktattr)
	struct flowvalve *fv;
	struct altq_pktattr *pktattr;
{
	struct fve *fve;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif

	/*
	 * get an entry from the tail of the LRU list.
	 */
	fve = TAILQ_LAST(&fv->fv_flowlist, fv_flowhead);

	switch (pktattr->pattr_af) {
	case AF_INET:
		ip = (struct ip *)pktattr->pattr_hdr;
		fve->fve_flow.flow_af = AF_INET;
		fve->fve_flow.flow_ip.ip_src = ip->ip_src;
		fve->fve_flow.flow_ip.ip_dst = ip->ip_dst;
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = (struct ip6_hdr *)pktattr->pattr_hdr;
		fve->fve_flow.flow_af = AF_INET6;
		fve->fve_flow.flow_ip6.ip6_src = ip6->ip6_src;
		fve->fve_flow.flow_ip6.ip6_dst = ip6->ip6_dst;
		break;
#endif
	}

	fve->fve_state = Green;
	fve->fve_p = 0.0;
	fve->fve_f = 0.0;
	fve->fve_ifseq = fv->fv_ifseq - 1;
	fve->fve_count = 0;

	fv->fv_flows++;
#ifdef FV_STATS
	fv->fv_stats.alloc++;
#endif
	return (fve);
}

static __inline void
flowlist_move_to_head(fv, fve)
	struct flowvalve *fv;
	struct fve *fve;
{
	if (TAILQ_FIRST(&fv->fv_flowlist) != fve) {
		TAILQ_REMOVE(&fv->fv_flowlist, fve, fve_lru);
		TAILQ_INSERT_HEAD(&fv->fv_flowlist, fve, fve_lru);
	}
}

#if 0 /* XXX: make the compiler happy (fv_alloc unused) */
/*
 * allocate flowvalve structure
 */
static struct flowvalve *
fv_alloc(rp)
	struct red *rp;
{
	struct flowvalve *fv;
	struct fve *fve;
	int i, num;

	num = FV_FLOWLISTSIZE;
	fv = malloc(sizeof(struct flowvalve),
	       M_DEVBUF, M_WAITOK);
	if (fv == NULL)
		return (NULL);
	bzero(fv, sizeof(struct flowvalve));

	fv->fv_fves = malloc(sizeof(struct fve) * num,
	       M_DEVBUF, M_WAITOK);
	if (fv->fv_fves == NULL) {
		free(fv, M_DEVBUF);
		return (NULL);
	}
	bzero(fv->fv_fves, sizeof(struct fve) * num);

	fv->fv_flows = 0;
	TAILQ_INIT(&fv->fv_flowlist);
	for (i = 0; i < num; i++) {
		fve = &fv->fv_fves[i];
		fve->fve_lastdrop.tv_sec = 0;
		TAILQ_INSERT_TAIL(&fv->fv_flowlist, fve, fve_lru);
	}

	/* initialize drop rate threshold in scaled fixed-point */
	fv->fv_pthresh = (FV_PSCALE(1) << FP_SHIFT) / rp->red_inv_pmax;

	/* initialize drop rate to fraction table */
	fv->fv_p2ftab = malloc(sizeof(int) * BRTT_SIZE,
	       M_DEVBUF, M_WAITOK);
	if (fv->fv_p2ftab == NULL) {
		free(fv->fv_fves, M_DEVBUF);
		free(fv, M_DEVBUF);
		return (NULL);
	}
	/*
	 * create the p2f table.
	 * (shift is used to keep the precision)
	 */
	for (i = 1; i < BRTT_SIZE; i++) {
		int f;

		f = brtt_tab[i] << 8;
		fv->fv_p2ftab[i] = (f / (rp->red_thmax + FV_ALPHA)) >> 8;
	}

	return (fv);
}
#endif

static void fv_destroy(fv)
	struct flowvalve *fv;
{
	free(fv->fv_p2ftab, M_DEVBUF);
	free(fv->fv_fves, M_DEVBUF);
	free(fv, M_DEVBUF);
}

static __inline int
fv_p2f(fv, p)
	struct flowvalve	*fv;
	int	p;
{
	int val, f;

	if (p >= BRTT_PMAX)
		f = fv->fv_p2ftab[BRTT_SIZE-1];
	else if ((val = (p & BRTT_MASK)))
		f = fv->fv_p2ftab[(val >> BRTT_SHIFT)];
	else
		f = fv->fv_p2ftab[1];
	return (f);
}

/*
 * check if an arriving packet should be pre-dropped.
 * called from red_addq() when a packet arrives.
 * returns 1 when the packet should be pre-dropped.
 * should be called in splimp.
 */
static int
fv_checkflow(fv, pktattr, fcache)
	struct flowvalve *fv;
	struct altq_pktattr *pktattr;
	struct fve **fcache;
{
	struct fve *fve;
	struct timeval now;

	fv->fv_ifseq++;
	FV_TIMESTAMP(&now);

	if ((fve = flowlist_lookup(fv, pktattr, &now)) == NULL)
		/* no matching entry in the flowlist */
		return (0);

	*fcache = fve;

	/* update fraction f for every FV_N packets */
	if (++fve->fve_count == FV_N) {
		/*
		 * f = Wf * N / (fv_ifseq - fve_ifseq) + (1 - Wf) * f
		 */
		fve->fve_f =
			(FV_N << FP_SHIFT) / (fv->fv_ifseq - fve->fve_ifseq)
			+ fve->fve_f - FV_FUNSCALE(fve->fve_f);
		fve->fve_ifseq = fv->fv_ifseq;
		fve->fve_count = 0;
	}

	/*
	 * overpumping test
	 */
	if (fve->fve_state == Green && fve->fve_p > fv->fv_pthresh) {
		int fthresh;

		/* calculate a threshold */
		fthresh = fv_p2f(fv, fve->fve_p);
		if (fve->fve_f > fthresh)
			fve->fve_state = Red;
	}

	if (fve->fve_state == Red) {
		/*
		 * backoff test
		 */
		if (now.tv_sec - fve->fve_lastdrop.tv_sec > FV_BACKOFFTHRESH) {
			/* no drop for at least FV_BACKOFFTHRESH sec */
			fve->fve_p = 0;
			fve->fve_state = Green;
#ifdef FV_STATS
			fv->fv_stats.escape++;
#endif
		} else {
			/* block this flow */
			flowlist_move_to_head(fv, fve);
			fve->fve_lastdrop = now;
#ifdef FV_STATS
			fv->fv_stats.predrop++;
#endif
			return (1);
		}
	}

	/*
	 * p = (1 - Wp) * p
	 */
	fve->fve_p -= FV_PUNSCALE(fve->fve_p);
	if (fve->fve_p < 0)
		fve->fve_p = 0;
#ifdef FV_STATS
	fv->fv_stats.pass++;
#endif
	return (0);
}

/*
 * called from red_addq when a packet is dropped by red.
 * should be called in splimp.
 */
static void fv_dropbyred(fv, pktattr, fcache)
	struct flowvalve *fv;
	struct altq_pktattr *pktattr;
	struct fve *fcache;
{
	struct fve *fve;
	struct timeval now;

	if (pktattr == NULL)
		return;
	FV_TIMESTAMP(&now);

	if (fcache != NULL)
		/* the fve of this packet is already cached */
		fve = fcache;
	else if ((fve = flowlist_lookup(fv, pktattr, &now)) == NULL)
		fve = flowlist_reclaim(fv, pktattr);

	flowlist_move_to_head(fv, fve);

	/*
	 * update p:  the following line cancels the update
	 *	      in fv_checkflow() and calculate
	 *	p = Wp + (1 - Wp) * p
	 */
	fve->fve_p = (1 << FP_SHIFT) + fve->fve_p;

	fve->fve_lastdrop = now;
}

#endif /* ALTQ_FLOWVALVE */

#ifdef KLD_MODULE

static struct altqsw red_sw =
	{"red", redopen, redclose, redioctl};

ALTQ_MODULE(altq_red, ALTQT_RED, &red_sw);
MODULE_VERSION(altq_red, 1);

#endif /* KLD_MODULE */
#endif /* ALTQ3_COMPAT */

#endif /* ALTQ_RED */
