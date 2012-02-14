/*	$FreeBSD$	*/
/*	$KAME: altq_rio.c,v 1.17 2003/07/10 12:07:49 kjc Exp $	*/

/*
 * Copyright (C) 1998-2003
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
#ifdef ALTQ_RIO	/* rio is enabled by ALTQ_RIO option in opt_altq.h */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/errno.h>
#if 1 /* ALTQ3_COMPAT */
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#endif

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/pfvar.h>
#include <altq/altq.h>
#include <altq/altq_cdnr.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>
#ifdef ALTQ3_COMPAT
#include <altq/altq_conf.h>
#endif

/*
 * RIO: RED with IN/OUT bit
 *   described in
 *	"Explicit Allocation of Best Effort Packet Delivery Service"
 *	David D. Clark and Wenjia Fang, MIT Lab for Computer Science
 *	http://diffserv.lcs.mit.edu/Papers/exp-alloc-ddc-wf.{ps,pdf}
 *
 * this implementation is extended to support more than 2 drop precedence
 * values as described in RFC2597 (Assured Forwarding PHB Group).
 *
 */
/*
 * AF DS (differentiated service) codepoints.
 * (classes can be mapped to CBQ or H-FSC classes.)
 *
 *      0   1   2   3   4   5   6   7
 *    +---+---+---+---+---+---+---+---+
 *    |   CLASS   |DropPre| 0 |  CU   |
 *    +---+---+---+---+---+---+---+---+
 *
 *    class 1: 001
 *    class 2: 010
 *    class 3: 011
 *    class 4: 100
 *
 *    low drop prec:    01
 *    medium drop prec: 10
 *    high drop prec:   01
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
#define	TH_MIN		 5	/* min threshold */
#define	TH_MAX		15	/* max threshold */

#define	RIO_LIMIT	60	/* default max queue lenght */
#define	RIO_STATS		/* collect statistics */

#define	TV_DELTA(a, b, delta) {					\
	register int	xxs;					\
								\
	delta = (a)->tv_usec - (b)->tv_usec; 			\
	if ((xxs = (a)->tv_sec - (b)->tv_sec) != 0) { 		\
		if (xxs < 0) { 					\
			delta = 60000000;			\
		} else if (xxs > 4)  {				\
			if (xxs > 60)				\
				delta = 60000000;		\
			else					\
				delta += xxs * 1000000;		\
		} else while (xxs > 0) {			\
			delta += 1000000;			\
			xxs--;					\
		}						\
	}							\
}

#ifdef ALTQ3_COMPAT
/* rio_list keeps all rio_queue_t's allocated. */
static rio_queue_t *rio_list = NULL;
#endif
/* default rio parameter values */
static struct redparams default_rio_params[RIO_NDROPPREC] = {
  /* th_min,		 th_max,     inv_pmax */
  { TH_MAX * 2 + TH_MIN, TH_MAX * 3, INV_P_MAX }, /* low drop precedence */
  { TH_MAX + TH_MIN,	 TH_MAX * 2, INV_P_MAX }, /* medium drop precedence */
  { TH_MIN,		 TH_MAX,     INV_P_MAX }  /* high drop precedence */
};

/* internal function prototypes */
static int dscp2index(u_int8_t);
#ifdef ALTQ3_COMPAT
static int rio_enqueue(struct ifaltq *, struct mbuf *, struct altq_pktattr *);
static struct mbuf *rio_dequeue(struct ifaltq *, int);
static int rio_request(struct ifaltq *, int, void *);
static int rio_detach(rio_queue_t *);

/*
 * rio device interface
 */
altqdev_decl(rio);

#endif /* ALTQ3_COMPAT */

rio_t *
rio_alloc(int weight, struct redparams *params, int flags, int pkttime)
{
	rio_t	*rp;
	int	 w, i;
	int	 npkts_per_sec;

	rp = malloc(sizeof(rio_t), M_DEVBUF, M_WAITOK);
	if (rp == NULL)
		return (NULL);
	bzero(rp, sizeof(rio_t));

	rp->rio_flags = flags;
	if (pkttime == 0)
		/* default packet time: 1000 bytes / 10Mbps * 8 * 1000000 */
		rp->rio_pkttime = 800;
	else
		rp->rio_pkttime = pkttime;

	if (weight != 0)
		rp->rio_weight = weight;
	else {
		/* use default */
		rp->rio_weight = W_WEIGHT;

		/* when the link is very slow, adjust red parameters */
		npkts_per_sec = 1000000 / rp->rio_pkttime;
		if (npkts_per_sec < 50) {
			/* up to about 400Kbps */
			rp->rio_weight = W_WEIGHT_2;
		} else if (npkts_per_sec < 300) {
			/* up to about 2.4Mbps */
			rp->rio_weight = W_WEIGHT_1;
		}
	}

	/* calculate wshift.  weight must be power of 2 */
	w = rp->rio_weight;
	for (i = 0; w > 1; i++)
		w = w >> 1;
	rp->rio_wshift = i;
	w = 1 << rp->rio_wshift;
	if (w != rp->rio_weight) {
		printf("invalid weight value %d for red! use %d\n",
		       rp->rio_weight, w);
		rp->rio_weight = w;
	}

	/* allocate weight table */
	rp->rio_wtab = wtab_alloc(rp->rio_weight);

	for (i = 0; i < RIO_NDROPPREC; i++) {
		struct dropprec_state *prec = &rp->rio_precstate[i];

		prec->avg = 0;
		prec->idle = 1;

		if (params == NULL || params[i].inv_pmax == 0)
			prec->inv_pmax = default_rio_params[i].inv_pmax;
		else
			prec->inv_pmax = params[i].inv_pmax;
		if (params == NULL || params[i].th_min == 0)
			prec->th_min = default_rio_params[i].th_min;
		else
			prec->th_min = params[i].th_min;
		if (params == NULL || params[i].th_max == 0)
			prec->th_max = default_rio_params[i].th_max;
		else
			prec->th_max = params[i].th_max;

		/*
		 * th_min_s and th_max_s are scaled versions of th_min
		 * and th_max to be compared with avg.
		 */
		prec->th_min_s = prec->th_min << (rp->rio_wshift + FP_SHIFT);
		prec->th_max_s = prec->th_max << (rp->rio_wshift + FP_SHIFT);

		/*
		 * precompute probability denominator
		 *  probd = (2 * (TH_MAX-TH_MIN) / pmax) in fixed-point
		 */
		prec->probd = (2 * (prec->th_max - prec->th_min)
			       * prec->inv_pmax) << FP_SHIFT;

		microtime(&prec->last);
	}

	return (rp);
}

void
rio_destroy(rio_t *rp)
{
	wtab_destroy(rp->rio_wtab);
	free(rp, M_DEVBUF);
}

void
rio_getstats(rio_t *rp, struct redstats *sp)
{
	int	i;

	for (i = 0; i < RIO_NDROPPREC; i++) {
		bcopy(&rp->q_stats[i], sp, sizeof(struct redstats));
		sp->q_avg = rp->rio_precstate[i].avg >> rp->rio_wshift;
		sp++;
	}
}

#if (RIO_NDROPPREC == 3)
/*
 * internally, a drop precedence value is converted to an index
 * starting from 0.
 */
static int
dscp2index(u_int8_t dscp)
{
	int	dpindex = dscp & AF_DROPPRECMASK;

	if (dpindex == 0)
		return (0);
	return ((dpindex >> 3) - 1);
}
#endif

#if 1
/*
 * kludge: when a packet is dequeued, we need to know its drop precedence
 * in order to keep the queue length of each drop precedence.
 * use m_pkthdr.rcvif to pass this info.
 */
#define	RIOM_SET_PRECINDEX(m, idx)	\
	do { (m)->m_pkthdr.rcvif = (void *)((long)(idx)); } while (0)
#define	RIOM_GET_PRECINDEX(m)	\
	({ long idx; idx = (long)((m)->m_pkthdr.rcvif); \
	(m)->m_pkthdr.rcvif = NULL; idx; })
#endif

int
rio_addq(rio_t *rp, class_queue_t *q, struct mbuf *m,
    struct altq_pktattr *pktattr)
{
	int			 avg, droptype;
	u_int8_t		 dsfield, odsfield;
	int			 dpindex, i, n, t;
	struct timeval		 now;
	struct dropprec_state	*prec;

	dsfield = odsfield = read_dsfield(m, pktattr);
	dpindex = dscp2index(dsfield);

	/*
	 * update avg of the precedence states whose drop precedence
	 * is larger than or equal to the drop precedence of the packet
	 */
	now.tv_sec = 0;
	for (i = dpindex; i < RIO_NDROPPREC; i++) {
		prec = &rp->rio_precstate[i];
		avg = prec->avg;
		if (prec->idle) {
			prec->idle = 0;
			if (now.tv_sec == 0)
				microtime(&now);
			t = (now.tv_sec - prec->last.tv_sec);
			if (t > 60)
				avg = 0;
			else {
				t = t * 1000000 +
					(now.tv_usec - prec->last.tv_usec);
				n = t / rp->rio_pkttime;
				/* calculate (avg = (1 - Wq)^n * avg) */
				if (n > 0)
					avg = (avg >> FP_SHIFT) *
						pow_w(rp->rio_wtab, n);
			}
		}

		/* run estimator. (avg is scaled by WEIGHT in fixed-point) */
		avg += (prec->qlen << FP_SHIFT) - (avg >> rp->rio_wshift);
		prec->avg = avg;		/* save the new value */
		/*
		 * count keeps a tally of arriving traffic that has not
		 * been dropped.
		 */
		prec->count++;
	}

	prec = &rp->rio_precstate[dpindex];
	avg = prec->avg;

	/* see if we drop early */
	droptype = DTYPE_NODROP;
	if (avg >= prec->th_min_s && prec->qlen > 1) {
		if (avg >= prec->th_max_s) {
			/* avg >= th_max: forced drop */
			droptype = DTYPE_FORCED;
		} else if (prec->old == 0) {
			/* first exceeds th_min */
			prec->count = 1;
			prec->old = 1;
		} else if (drop_early((avg - prec->th_min_s) >> rp->rio_wshift,
				      prec->probd, prec->count)) {
			/* unforced drop by red */
			droptype = DTYPE_EARLY;
		}
	} else {
		/* avg < th_min */
		prec->old = 0;
	}

	/*
	 * if the queue length hits the hard limit, it's a forced drop.
	 */
	if (droptype == DTYPE_NODROP && qlen(q) >= qlimit(q))
		droptype = DTYPE_FORCED;

	if (droptype != DTYPE_NODROP) {
		/* always drop incoming packet (as opposed to randomdrop) */
		for (i = dpindex; i < RIO_NDROPPREC; i++)
			rp->rio_precstate[i].count = 0;
#ifdef RIO_STATS
		if (droptype == DTYPE_EARLY)
			rp->q_stats[dpindex].drop_unforced++;
		else
			rp->q_stats[dpindex].drop_forced++;
		PKTCNTR_ADD(&rp->q_stats[dpindex].drop_cnt, m_pktlen(m));
#endif
		m_freem(m);
		return (-1);
	}

	for (i = dpindex; i < RIO_NDROPPREC; i++)
		rp->rio_precstate[i].qlen++;

	/* save drop precedence index in mbuf hdr */
	RIOM_SET_PRECINDEX(m, dpindex);

	if (rp->rio_flags & RIOF_CLEARDSCP)
		dsfield &= ~DSCP_MASK;

	if (dsfield != odsfield)
		write_dsfield(m, pktattr, dsfield);

	_addq(q, m);

#ifdef RIO_STATS
	PKTCNTR_ADD(&rp->q_stats[dpindex].xmit_cnt, m_pktlen(m));
#endif
	return (0);
}

struct mbuf *
rio_getq(rio_t *rp, class_queue_t *q)
{
	struct mbuf	*m;
	int		 dpindex, i;

	if ((m = _getq(q)) == NULL)
		return NULL;

	dpindex = RIOM_GET_PRECINDEX(m);
	for (i = dpindex; i < RIO_NDROPPREC; i++) {
		if (--rp->rio_precstate[i].qlen == 0) {
			if (rp->rio_precstate[i].idle == 0) {
				rp->rio_precstate[i].idle = 1;
				microtime(&rp->rio_precstate[i].last);
			}
		}
	}
	return (m);
}

#ifdef ALTQ3_COMPAT
int
rioopen(dev, flag, fmt, p)
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
rioclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
#if (__FreeBSD_version > 500000)
	struct thread *p;
#else
	struct proc *p;
#endif
{
	rio_queue_t *rqp;
	int err, error = 0;

	while ((rqp = rio_list) != NULL) {
		/* destroy all */
		err = rio_detach(rqp);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
rioioctl(dev, cmd, addr, flag, p)
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
	rio_queue_t *rqp;
	struct rio_interface *ifacep;
	struct ifnet *ifp;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	case RIO_GETSTATS:
		break;
	default:
#if (__FreeBSD_version > 700000)
		if ((error = priv_check(p, PRIV_ALTQ_MANAGE)) != 0)
			return (error);
#elsif (__FreeBSD_version > 400000)
		if ((error = suser(p)) != 0)
			return (error);
#else
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
#endif
		break;
	}

	switch (cmd) {

	case RIO_ENABLE:
		ifacep = (struct rio_interface *)addr;
		if ((rqp = altq_lookup(ifacep->rio_ifname, ALTQT_RIO)) == NULL) {
			error = EBADF;
			break;
		}
		error = altq_enable(rqp->rq_ifq);
		break;

	case RIO_DISABLE:
		ifacep = (struct rio_interface *)addr;
		if ((rqp = altq_lookup(ifacep->rio_ifname, ALTQT_RIO)) == NULL) {
			error = EBADF;
			break;
		}
		error = altq_disable(rqp->rq_ifq);
		break;

	case RIO_IF_ATTACH:
		ifp = ifunit(((struct rio_interface *)addr)->rio_ifname);
		if (ifp == NULL) {
			error = ENXIO;
			break;
		}

		/* allocate and initialize rio_queue_t */
		rqp = malloc(sizeof(rio_queue_t), M_DEVBUF, M_WAITOK);
		if (rqp == NULL) {
			error = ENOMEM;
			break;
		}
		bzero(rqp, sizeof(rio_queue_t));

		rqp->rq_q = malloc(sizeof(class_queue_t),
		       M_DEVBUF, M_WAITOK);
		if (rqp->rq_q == NULL) {
			free(rqp, M_DEVBUF);
			error = ENOMEM;
			break;
		}
		bzero(rqp->rq_q, sizeof(class_queue_t));

		rqp->rq_rio = rio_alloc(0, NULL, 0, 0);
		if (rqp->rq_rio == NULL) {
			free(rqp->rq_q, M_DEVBUF);
			free(rqp, M_DEVBUF);
			error = ENOMEM;
			break;
		}

		rqp->rq_ifq = &ifp->if_snd;
		qtail(rqp->rq_q) = NULL;
		qlen(rqp->rq_q) = 0;
		qlimit(rqp->rq_q) = RIO_LIMIT;
		qtype(rqp->rq_q) = Q_RIO;

		/*
		 * set RIO to this ifnet structure.
		 */
		error = altq_attach(rqp->rq_ifq, ALTQT_RIO, rqp,
				    rio_enqueue, rio_dequeue, rio_request,
				    NULL, NULL);
		if (error) {
			rio_destroy(rqp->rq_rio);
			free(rqp->rq_q, M_DEVBUF);
			free(rqp, M_DEVBUF);
			break;
		}

		/* add this state to the rio list */
		rqp->rq_next = rio_list;
		rio_list = rqp;
		break;

	case RIO_IF_DETACH:
		ifacep = (struct rio_interface *)addr;
		if ((rqp = altq_lookup(ifacep->rio_ifname, ALTQT_RIO)) == NULL) {
			error = EBADF;
			break;
		}
		error = rio_detach(rqp);
		break;

	case RIO_GETSTATS:
		do {
			struct rio_stats *q_stats;
			rio_t *rp;
			int i;

			q_stats = (struct rio_stats *)addr;
			if ((rqp = altq_lookup(q_stats->iface.rio_ifname,
					       ALTQT_RIO)) == NULL) {
				error = EBADF;
				break;
			}

			rp = rqp->rq_rio;

			q_stats->q_limit = qlimit(rqp->rq_q);
			q_stats->weight	= rp->rio_weight;
			q_stats->flags = rp->rio_flags;

			for (i = 0; i < RIO_NDROPPREC; i++) {
				q_stats->q_len[i] = rp->rio_precstate[i].qlen;
				bcopy(&rp->q_stats[i], &q_stats->q_stats[i],
				      sizeof(struct redstats));
				q_stats->q_stats[i].q_avg =
				    rp->rio_precstate[i].avg >> rp->rio_wshift;

				q_stats->q_params[i].inv_pmax
					= rp->rio_precstate[i].inv_pmax;
				q_stats->q_params[i].th_min
					= rp->rio_precstate[i].th_min;
				q_stats->q_params[i].th_max
					= rp->rio_precstate[i].th_max;
			}
		} while (/*CONSTCOND*/ 0);
		break;

	case RIO_CONFIG:
		do {
			struct rio_conf *fc;
			rio_t	*new;
			int s, limit, i;

			fc = (struct rio_conf *)addr;
			if ((rqp = altq_lookup(fc->iface.rio_ifname,
					       ALTQT_RIO)) == NULL) {
				error = EBADF;
				break;
			}

			new = rio_alloc(fc->rio_weight, &fc->q_params[0],
					fc->rio_flags, fc->rio_pkttime);
			if (new == NULL) {
				error = ENOMEM;
				break;
			}

#ifdef __NetBSD__
			s = splnet();
#else
			s = splimp();
#endif
			_flushq(rqp->rq_q);
			limit = fc->rio_limit;
			if (limit < fc->q_params[RIO_NDROPPREC-1].th_max)
				limit = fc->q_params[RIO_NDROPPREC-1].th_max;
			qlimit(rqp->rq_q) = limit;

			rio_destroy(rqp->rq_rio);
			rqp->rq_rio = new;

			splx(s);

			/* write back new values */
			fc->rio_limit = limit;
			for (i = 0; i < RIO_NDROPPREC; i++) {
				fc->q_params[i].inv_pmax =
					rqp->rq_rio->rio_precstate[i].inv_pmax;
				fc->q_params[i].th_min =
					rqp->rq_rio->rio_precstate[i].th_min;
				fc->q_params[i].th_max =
					rqp->rq_rio->rio_precstate[i].th_max;
			}
		} while (/*CONSTCOND*/ 0);
		break;

	case RIO_SETDEFAULTS:
		do {
			struct redparams *rp;
			int i;

			rp = (struct redparams *)addr;
			for (i = 0; i < RIO_NDROPPREC; i++)
				default_rio_params[i] = rp[i];
		} while (/*CONSTCOND*/ 0);
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int
rio_detach(rqp)
	rio_queue_t *rqp;
{
	rio_queue_t *tmp;
	int error = 0;

	if (ALTQ_IS_ENABLED(rqp->rq_ifq))
		altq_disable(rqp->rq_ifq);

	if ((error = altq_detach(rqp->rq_ifq)))
		return (error);

	if (rio_list == rqp)
		rio_list = rqp->rq_next;
	else {
		for (tmp = rio_list; tmp != NULL; tmp = tmp->rq_next)
			if (tmp->rq_next == rqp) {
				tmp->rq_next = rqp->rq_next;
				break;
			}
		if (tmp == NULL)
			printf("rio_detach: no state found in rio_list!\n");
	}

	rio_destroy(rqp->rq_rio);
	free(rqp->rq_q, M_DEVBUF);
	free(rqp, M_DEVBUF);
	return (error);
}

/*
 * rio support routines
 */
static int
rio_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	rio_queue_t *rqp = (rio_queue_t *)ifq->altq_disc;

	IFQ_LOCK_ASSERT(ifq);

	switch (req) {
	case ALTRQ_PURGE:
		_flushq(rqp->rq_q);
		if (ALTQ_IS_ENABLED(ifq))
			ifq->ifq_len = 0;
		break;
	}
	return (0);
}

/*
 * enqueue routine:
 *
 *	returns: 0 when successfully queued.
 *		 ENOBUFS when drop occurs.
 */
static int
rio_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	rio_queue_t *rqp = (rio_queue_t *)ifq->altq_disc;
	int error = 0;

	IFQ_LOCK_ASSERT(ifq);

	if (rio_addq(rqp->rq_rio, rqp->rq_q, m, pktattr) == 0)
		ifq->ifq_len++;
	else
		error = ENOBUFS;
	return error;
}

/*
 * dequeue routine:
 *	must be called in splimp.
 *
 *	returns: mbuf dequeued.
 *		 NULL when no packet is available in the queue.
 */

static struct mbuf *
rio_dequeue(ifq, op)
	struct ifaltq *ifq;
	int op;
{
	rio_queue_t *rqp = (rio_queue_t *)ifq->altq_disc;
	struct mbuf *m = NULL;

	IFQ_LOCK_ASSERT(ifq);

	if (op == ALTDQ_POLL)
		return qhead(rqp->rq_q);

	m = rio_getq(rqp->rq_rio, rqp->rq_q);
	if (m != NULL)
		ifq->ifq_len--;
	return m;
}

#ifdef KLD_MODULE

static struct altqsw rio_sw =
	{"rio", rioopen, rioclose, rioioctl};

ALTQ_MODULE(altq_rio, ALTQT_RIO, &rio_sw);
MODULE_VERSION(altq_rio, 1);
MODULE_DEPEND(altq_rio, altq_red, 1, 1, 1);

#endif /* KLD_MODULE */
#endif /* ALTQ3_COMPAT */

#endif /* ALTQ_RIO */
