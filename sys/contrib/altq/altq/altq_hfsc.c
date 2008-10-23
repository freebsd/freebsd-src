/*	$FreeBSD$	*/
/*	$KAME: altq_hfsc.c,v 1.24 2003/12/05 05:40:46 kjc Exp $	*/

/*
 * Copyright (c) 1997-1999 Carnegie Mellon University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation is hereby granted (including for commercial or
 * for-profit use), provided that both the copyright notice and this
 * permission notice appear in all copies of the software, derivative
 * works, or modified versions, and any portions thereof.
 *
 * THIS SOFTWARE IS EXPERIMENTAL AND IS KNOWN TO HAVE BUGS, SOME OF
 * WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON PROVIDES THIS
 * SOFTWARE IN ITS ``AS IS'' CONDITION, AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Carnegie Mellon encourages (but does not require) users of this
 * software to return any improvements or extensions that they make,
 * and to grant Carnegie Mellon the rights to redistribute these
 * changes without encumbrance.
 */
/*
 * H-FSC is described in Proceedings of SIGCOMM'97,
 * "A Hierarchical Fair Service Curve Algorithm for Link-Sharing,
 * Real-Time and Priority Service"
 * by Ion Stoica, Hui Zhang, and T. S. Eugene Ng.
 *
 * Oleg Cherevko <olwi@aq.ml.com.ua> added the upperlimit for link-sharing.
 * when a class has an upperlimit, the fit-time is computed from the
 * upperlimit service curve.  the link-sharing scheduler does not schedule
 * a class whose fit-time exceeds the current time.
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

#ifdef ALTQ_HFSC  /* hfsc is enabled by ALTQ_HFSC option in opt_altq.h */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/queue.h>
#if 1 /* ALTQ3_COMPAT */
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#endif /* ALTQ3_COMPAT */

#include <net/if.h>
#include <netinet/in.h>

#include <net/pfvar.h>
#include <altq/altq.h>
#include <altq/altq_hfsc.h>
#ifdef ALTQ3_COMPAT
#include <altq/altq_conf.h>
#endif

/*
 * function prototypes
 */
static int			 hfsc_clear_interface(struct hfsc_if *);
static int			 hfsc_request(struct ifaltq *, int, void *);
static void			 hfsc_purge(struct hfsc_if *);
static struct hfsc_class	*hfsc_class_create(struct hfsc_if *,
    struct service_curve *, struct service_curve *, struct service_curve *,
    struct hfsc_class *, int, int, int);
static int			 hfsc_class_destroy(struct hfsc_class *);
static struct hfsc_class	*hfsc_nextclass(struct hfsc_class *);
static int			 hfsc_enqueue(struct ifaltq *, struct mbuf *,
				    struct altq_pktattr *);
static struct mbuf		*hfsc_dequeue(struct ifaltq *, int);

static int		 hfsc_addq(struct hfsc_class *, struct mbuf *);
static struct mbuf	*hfsc_getq(struct hfsc_class *);
static struct mbuf	*hfsc_pollq(struct hfsc_class *);
static void		 hfsc_purgeq(struct hfsc_class *);

static void		 update_cfmin(struct hfsc_class *);
static void		 set_active(struct hfsc_class *, int);
static void		 set_passive(struct hfsc_class *);

static void		 init_ed(struct hfsc_class *, int);
static void		 update_ed(struct hfsc_class *, int);
static void		 update_d(struct hfsc_class *, int);
static void		 init_vf(struct hfsc_class *, int);
static void		 update_vf(struct hfsc_class *, int, u_int64_t);
static ellist_t		*ellist_alloc(void);
static void		 ellist_destroy(ellist_t *);
static void		 ellist_insert(struct hfsc_class *);
static void		 ellist_remove(struct hfsc_class *);
static void		 ellist_update(struct hfsc_class *);
struct hfsc_class	*ellist_get_mindl(ellist_t *, u_int64_t);
static actlist_t	*actlist_alloc(void);
static void		 actlist_destroy(actlist_t *);
static void		 actlist_insert(struct hfsc_class *);
static void		 actlist_remove(struct hfsc_class *);
static void		 actlist_update(struct hfsc_class *);

static struct hfsc_class	*actlist_firstfit(struct hfsc_class *,
				    u_int64_t);

static __inline u_int64_t	seg_x2y(u_int64_t, u_int64_t);
static __inline u_int64_t	seg_y2x(u_int64_t, u_int64_t);
static __inline u_int64_t	m2sm(u_int);
static __inline u_int64_t	m2ism(u_int);
static __inline u_int64_t	d2dx(u_int);
static u_int			sm2m(u_int64_t);
static u_int			dx2d(u_int64_t);

static void		sc2isc(struct service_curve *, struct internal_sc *);
static void		rtsc_init(struct runtime_sc *, struct internal_sc *,
			    u_int64_t, u_int64_t);
static u_int64_t	rtsc_y2x(struct runtime_sc *, u_int64_t);
static u_int64_t	rtsc_x2y(struct runtime_sc *, u_int64_t);
static void		rtsc_min(struct runtime_sc *, struct internal_sc *,
			    u_int64_t, u_int64_t);

static void			 get_class_stats(struct hfsc_classstats *,
				    struct hfsc_class *);
static struct hfsc_class	*clh_to_clp(struct hfsc_if *, u_int32_t);


#ifdef ALTQ3_COMPAT
static struct hfsc_if *hfsc_attach(struct ifaltq *, u_int);
static int hfsc_detach(struct hfsc_if *);
static int hfsc_class_modify(struct hfsc_class *, struct service_curve *,
    struct service_curve *, struct service_curve *);

static int hfsccmd_if_attach(struct hfsc_attach *);
static int hfsccmd_if_detach(struct hfsc_interface *);
static int hfsccmd_add_class(struct hfsc_add_class *);
static int hfsccmd_delete_class(struct hfsc_delete_class *);
static int hfsccmd_modify_class(struct hfsc_modify_class *);
static int hfsccmd_add_filter(struct hfsc_add_filter *);
static int hfsccmd_delete_filter(struct hfsc_delete_filter *);
static int hfsccmd_class_stats(struct hfsc_class_stats *);

altqdev_decl(hfsc);
#endif /* ALTQ3_COMPAT */

/*
 * macros
 */
#define	is_a_parent_class(cl)	((cl)->cl_children != NULL)

#define	HT_INFINITY	0xffffffffffffffffLL	/* infinite time value */

#ifdef ALTQ3_COMPAT
/* hif_list keeps all hfsc_if's allocated. */
static struct hfsc_if *hif_list = NULL;
#endif /* ALTQ3_COMPAT */

int
hfsc_pfattach(struct pf_altq *a)
{
	struct ifnet *ifp;
	int s, error;

	if ((ifp = ifunit(a->ifname)) == NULL || a->altq_disc == NULL)
		return (EINVAL);
#ifdef __NetBSD__
	s = splnet();
#else
	s = splimp();
#endif
	error = altq_attach(&ifp->if_snd, ALTQT_HFSC, a->altq_disc,
	    hfsc_enqueue, hfsc_dequeue, hfsc_request, NULL, NULL);
	splx(s);
	return (error);
}

int
hfsc_add_altq(struct pf_altq *a)
{
	struct hfsc_if *hif;
	struct ifnet *ifp;

	if ((ifp = ifunit(a->ifname)) == NULL)
		return (EINVAL);
	if (!ALTQ_IS_READY(&ifp->if_snd))
		return (ENODEV);

	hif = malloc(sizeof(struct hfsc_if),
	    M_DEVBUF, M_WAITOK);
	if (hif == NULL)
		return (ENOMEM);
	bzero(hif, sizeof(struct hfsc_if));

	hif->hif_eligible = ellist_alloc();
	if (hif->hif_eligible == NULL) {
		free(hif, M_DEVBUF);
		return (ENOMEM);
	}

	hif->hif_ifq = &ifp->if_snd;

	/* keep the state in pf_altq */
	a->altq_disc = hif;

	return (0);
}

int
hfsc_remove_altq(struct pf_altq *a)
{
	struct hfsc_if *hif;

	if ((hif = a->altq_disc) == NULL)
		return (EINVAL);
	a->altq_disc = NULL;

	(void)hfsc_clear_interface(hif);
	(void)hfsc_class_destroy(hif->hif_rootclass);

	ellist_destroy(hif->hif_eligible);

	free(hif, M_DEVBUF);

	return (0);
}

int
hfsc_add_queue(struct pf_altq *a)
{
	struct hfsc_if *hif;
	struct hfsc_class *cl, *parent;
	struct hfsc_opts *opts;
	struct service_curve rtsc, lssc, ulsc;

	if ((hif = a->altq_disc) == NULL)
		return (EINVAL);

	opts = &a->pq_u.hfsc_opts;

	if (a->parent_qid == HFSC_NULLCLASS_HANDLE &&
	    hif->hif_rootclass == NULL)
		parent = NULL;
	else if ((parent = clh_to_clp(hif, a->parent_qid)) == NULL)
		return (EINVAL);

	if (a->qid == 0)
		return (EINVAL);

	if (clh_to_clp(hif, a->qid) != NULL)
		return (EBUSY);

	rtsc.m1 = opts->rtsc_m1;
	rtsc.d  = opts->rtsc_d;
	rtsc.m2 = opts->rtsc_m2;
	lssc.m1 = opts->lssc_m1;
	lssc.d  = opts->lssc_d;
	lssc.m2 = opts->lssc_m2;
	ulsc.m1 = opts->ulsc_m1;
	ulsc.d  = opts->ulsc_d;
	ulsc.m2 = opts->ulsc_m2;

	cl = hfsc_class_create(hif, &rtsc, &lssc, &ulsc,
	    parent, a->qlimit, opts->flags, a->qid);
	if (cl == NULL)
		return (ENOMEM);

	return (0);
}

int
hfsc_remove_queue(struct pf_altq *a)
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;

	if ((hif = a->altq_disc) == NULL)
		return (EINVAL);

	if ((cl = clh_to_clp(hif, a->qid)) == NULL)
		return (EINVAL);

	return (hfsc_class_destroy(cl));
}

int
hfsc_getqstats(struct pf_altq *a, void *ubuf, int *nbytes)
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;
	struct hfsc_classstats stats;
	int error = 0;

	if ((hif = altq_lookup(a->ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, a->qid)) == NULL)
		return (EINVAL);

	if (*nbytes < sizeof(stats))
		return (EINVAL);

	get_class_stats(&stats, cl);

	if ((error = copyout((caddr_t)&stats, ubuf, sizeof(stats))) != 0)
		return (error);
	*nbytes = sizeof(stats);
	return (0);
}

/*
 * bring the interface back to the initial state by discarding
 * all the filters and classes except the root class.
 */
static int
hfsc_clear_interface(struct hfsc_if *hif)
{
	struct hfsc_class	*cl;

#ifdef ALTQ3_COMPAT
	/* free the filters for this interface */
	acc_discard_filters(&hif->hif_classifier, NULL, 1);
#endif

	/* clear out the classes */
	while (hif->hif_rootclass != NULL &&
	    (cl = hif->hif_rootclass->cl_children) != NULL) {
		/*
		 * remove the first leaf class found in the hierarchy
		 * then start over
		 */
		for (; cl != NULL; cl = hfsc_nextclass(cl)) {
			if (!is_a_parent_class(cl)) {
				(void)hfsc_class_destroy(cl);
				break;
			}
		}
	}

	return (0);
}

static int
hfsc_request(struct ifaltq *ifq, int req, void *arg)
{
	struct hfsc_if	*hif = (struct hfsc_if *)ifq->altq_disc;

	IFQ_LOCK_ASSERT(ifq);

	switch (req) {
	case ALTRQ_PURGE:
		hfsc_purge(hif);
		break;
	}
	return (0);
}

/* discard all the queued packets on the interface */
static void
hfsc_purge(struct hfsc_if *hif)
{
	struct hfsc_class *cl;

	for (cl = hif->hif_rootclass; cl != NULL; cl = hfsc_nextclass(cl))
		if (!qempty(cl->cl_q))
			hfsc_purgeq(cl);
	if (ALTQ_IS_ENABLED(hif->hif_ifq))
		hif->hif_ifq->ifq_len = 0;
}

struct hfsc_class *
hfsc_class_create(struct hfsc_if *hif, struct service_curve *rsc,
    struct service_curve *fsc, struct service_curve *usc,
    struct hfsc_class *parent, int qlimit, int flags, int qid)
{
	struct hfsc_class *cl, *p;
	int i, s;

	if (hif->hif_classes >= HFSC_MAX_CLASSES)
		return (NULL);

#ifndef ALTQ_RED
	if (flags & HFCF_RED) {
#ifdef ALTQ_DEBUG
		printf("hfsc_class_create: RED not configured for HFSC!\n");
#endif
		return (NULL);
	}
#endif

	cl = malloc(sizeof(struct hfsc_class),
	       M_DEVBUF, M_WAITOK);
	if (cl == NULL)
		return (NULL);
	bzero(cl, sizeof(struct hfsc_class));

	cl->cl_q = malloc(sizeof(class_queue_t),
	       M_DEVBUF, M_WAITOK);
	if (cl->cl_q == NULL)
		goto err_ret;
	bzero(cl->cl_q, sizeof(class_queue_t));

	cl->cl_actc = actlist_alloc();
	if (cl->cl_actc == NULL)
		goto err_ret;

	if (qlimit == 0)
		qlimit = 50;  /* use default */
	qlimit(cl->cl_q) = qlimit;
	qtype(cl->cl_q) = Q_DROPTAIL;
	qlen(cl->cl_q) = 0;
	cl->cl_flags = flags;
#ifdef ALTQ_RED
	if (flags & (HFCF_RED|HFCF_RIO)) {
		int red_flags, red_pkttime;
		u_int m2;

		m2 = 0;
		if (rsc != NULL && rsc->m2 > m2)
			m2 = rsc->m2;
		if (fsc != NULL && fsc->m2 > m2)
			m2 = fsc->m2;
		if (usc != NULL && usc->m2 > m2)
			m2 = usc->m2;

		red_flags = 0;
		if (flags & HFCF_ECN)
			red_flags |= REDF_ECN;
#ifdef ALTQ_RIO
		if (flags & HFCF_CLEARDSCP)
			red_flags |= RIOF_CLEARDSCP;
#endif
		if (m2 < 8)
			red_pkttime = 1000 * 1000 * 1000; /* 1 sec */
		else
			red_pkttime = (int64_t)hif->hif_ifq->altq_ifp->if_mtu
				* 1000 * 1000 * 1000 / (m2 / 8);
		if (flags & HFCF_RED) {
			cl->cl_red = red_alloc(0, 0,
			    qlimit(cl->cl_q) * 10/100,
			    qlimit(cl->cl_q) * 30/100,
			    red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RED;
		}
#ifdef ALTQ_RIO
		else {
			cl->cl_red = (red_t *)rio_alloc(0, NULL,
			    red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RIO;
		}
#endif
	}
#endif /* ALTQ_RED */

	if (rsc != NULL && (rsc->m1 != 0 || rsc->m2 != 0)) {
		cl->cl_rsc = malloc(		    sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (cl->cl_rsc == NULL)
			goto err_ret;
		sc2isc(rsc, cl->cl_rsc);
		rtsc_init(&cl->cl_deadline, cl->cl_rsc, 0, 0);
		rtsc_init(&cl->cl_eligible, cl->cl_rsc, 0, 0);
	}
	if (fsc != NULL && (fsc->m1 != 0 || fsc->m2 != 0)) {
		cl->cl_fsc = malloc(		    sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (cl->cl_fsc == NULL)
			goto err_ret;
		sc2isc(fsc, cl->cl_fsc);
		rtsc_init(&cl->cl_virtual, cl->cl_fsc, 0, 0);
	}
	if (usc != NULL && (usc->m1 != 0 || usc->m2 != 0)) {
		cl->cl_usc = malloc(		    sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (cl->cl_usc == NULL)
			goto err_ret;
		sc2isc(usc, cl->cl_usc);
		rtsc_init(&cl->cl_ulimit, cl->cl_usc, 0, 0);
	}

	cl->cl_id = hif->hif_classid++;
	cl->cl_handle = qid;
	cl->cl_hif = hif;
	cl->cl_parent = parent;

#ifdef __NetBSD__
	s = splnet();
#else
	s = splimp();
#endif
	IFQ_LOCK(hif->hif_ifq);
	hif->hif_classes++;

	/*
	 * find a free slot in the class table.  if the slot matching
	 * the lower bits of qid is free, use this slot.  otherwise,
	 * use the first free slot.
	 */
	i = qid % HFSC_MAX_CLASSES;
	if (hif->hif_class_tbl[i] == NULL)
		hif->hif_class_tbl[i] = cl;
	else {
		for (i = 0; i < HFSC_MAX_CLASSES; i++)
			if (hif->hif_class_tbl[i] == NULL) {
				hif->hif_class_tbl[i] = cl;
				break;
			}
		if (i == HFSC_MAX_CLASSES) {
			IFQ_UNLOCK(hif->hif_ifq);
			splx(s);
			goto err_ret;
		}
	}

	if (flags & HFCF_DEFAULTCLASS)
		hif->hif_defaultclass = cl;

	if (parent == NULL) {
		/* this is root class */
		hif->hif_rootclass = cl;
	} else {
		/* add this class to the children list of the parent */
		if ((p = parent->cl_children) == NULL)
			parent->cl_children = cl;
		else {
			while (p->cl_siblings != NULL)
				p = p->cl_siblings;
			p->cl_siblings = cl;
		}
	}
	IFQ_UNLOCK(hif->hif_ifq);
	splx(s);

	return (cl);

 err_ret:
	if (cl->cl_actc != NULL)
		actlist_destroy(cl->cl_actc);
	if (cl->cl_red != NULL) {
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	}
	if (cl->cl_fsc != NULL)
		free(cl->cl_fsc, M_DEVBUF);
	if (cl->cl_rsc != NULL)
		free(cl->cl_rsc, M_DEVBUF);
	if (cl->cl_usc != NULL)
		free(cl->cl_usc, M_DEVBUF);
	if (cl->cl_q != NULL)
		free(cl->cl_q, M_DEVBUF);
	free(cl, M_DEVBUF);
	return (NULL);
}

static int
hfsc_class_destroy(struct hfsc_class *cl)
{
	int i, s;

	if (cl == NULL)
		return (0);

	if (is_a_parent_class(cl))
		return (EBUSY);

#ifdef __NetBSD__
	s = splnet();
#else
	s = splimp();
#endif
	IFQ_LOCK(cl->cl_hif->hif_ifq);

#ifdef ALTQ3_COMPAT
	/* delete filters referencing to this class */
	acc_discard_filters(&cl->cl_hif->hif_classifier, cl, 0);
#endif /* ALTQ3_COMPAT */

	if (!qempty(cl->cl_q))
		hfsc_purgeq(cl);

	if (cl->cl_parent == NULL) {
		/* this is root class */
	} else {
		struct hfsc_class *p = cl->cl_parent->cl_children;

		if (p == cl)
			cl->cl_parent->cl_children = cl->cl_siblings;
		else do {
			if (p->cl_siblings == cl) {
				p->cl_siblings = cl->cl_siblings;
				break;
			}
		} while ((p = p->cl_siblings) != NULL);
		ASSERT(p != NULL);
	}

	for (i = 0; i < HFSC_MAX_CLASSES; i++)
		if (cl->cl_hif->hif_class_tbl[i] == cl) {
			cl->cl_hif->hif_class_tbl[i] = NULL;
			break;
		}

	cl->cl_hif->hif_classes--;
	IFQ_UNLOCK(cl->cl_hif->hif_ifq);
	splx(s);

	actlist_destroy(cl->cl_actc);

	if (cl->cl_red != NULL) {
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	}

	IFQ_LOCK(cl->cl_hif->hif_ifq);
	if (cl == cl->cl_hif->hif_rootclass)
		cl->cl_hif->hif_rootclass = NULL;
	if (cl == cl->cl_hif->hif_defaultclass)
		cl->cl_hif->hif_defaultclass = NULL;
	IFQ_UNLOCK(cl->cl_hif->hif_ifq);

	if (cl->cl_usc != NULL)
		free(cl->cl_usc, M_DEVBUF);
	if (cl->cl_fsc != NULL)
		free(cl->cl_fsc, M_DEVBUF);
	if (cl->cl_rsc != NULL)
		free(cl->cl_rsc, M_DEVBUF);
	free(cl->cl_q, M_DEVBUF);
	free(cl, M_DEVBUF);

	return (0);
}

/*
 * hfsc_nextclass returns the next class in the tree.
 *   usage:
 *	for (cl = hif->hif_rootclass; cl != NULL; cl = hfsc_nextclass(cl))
 *		do_something;
 */
static struct hfsc_class *
hfsc_nextclass(struct hfsc_class *cl)
{
	if (cl->cl_children != NULL)
		cl = cl->cl_children;
	else if (cl->cl_siblings != NULL)
		cl = cl->cl_siblings;
	else {
		while ((cl = cl->cl_parent) != NULL)
			if (cl->cl_siblings) {
				cl = cl->cl_siblings;
				break;
			}
	}

	return (cl);
}

/*
 * hfsc_enqueue is an enqueue function to be registered to
 * (*altq_enqueue) in struct ifaltq.
 */
static int
hfsc_enqueue(struct ifaltq *ifq, struct mbuf *m, struct altq_pktattr *pktattr)
{
	struct hfsc_if	*hif = (struct hfsc_if *)ifq->altq_disc;
	struct hfsc_class *cl;
	struct pf_mtag *t;
	int len;

	IFQ_LOCK_ASSERT(ifq);

	/* grab class set by classifier */
	if ((m->m_flags & M_PKTHDR) == 0) {
		/* should not happen */
#if defined(__NetBSD__) || defined(__OpenBSD__)\
    || (defined(__FreeBSD__) && __FreeBSD_version >= 501113)
		printf("altq: packet for %s does not have pkthdr\n",
		    ifq->altq_ifp->if_xname);
#else
		printf("altq: packet for %s%d does not have pkthdr\n",
		    ifq->altq_ifp->if_name, ifq->altq_ifp->if_unit);
#endif
		m_freem(m);
		return (ENOBUFS);
	}
	cl = NULL;
	if ((t = pf_find_mtag(m)) != NULL)
		cl = clh_to_clp(hif, t->qid);
#ifdef ALTQ3_COMPAT
	else if ((ifq->altq_flags & ALTQF_CLASSIFY) && pktattr != NULL)
		cl = pktattr->pattr_class;
#endif
	if (cl == NULL || is_a_parent_class(cl)) {
		cl = hif->hif_defaultclass;
		if (cl == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
	}
#ifdef ALTQ3_COMPAT
	if (pktattr != NULL)
		cl->cl_pktattr = pktattr;  /* save proto hdr used by ECN */
	else
#endif
		cl->cl_pktattr = NULL;
	len = m_pktlen(m);
	if (hfsc_addq(cl, m) != 0) {
		/* drop occurred.  mbuf was freed in hfsc_addq. */
		PKTCNTR_ADD(&cl->cl_stats.drop_cnt, len);
		return (ENOBUFS);
	}
	IFQ_INC_LEN(ifq);
	cl->cl_hif->hif_packets++;

	/* successfully queued. */
	if (qlen(cl->cl_q) == 1)
		set_active(cl, m_pktlen(m));

	return (0);
}

/*
 * hfsc_dequeue is a dequeue function to be registered to
 * (*altq_dequeue) in struct ifaltq.
 *
 * note: ALTDQ_POLL returns the next packet without removing the packet
 *	from the queue.  ALTDQ_REMOVE is a normal dequeue operation.
 *	ALTDQ_REMOVE must return the same packet if called immediately
 *	after ALTDQ_POLL.
 */
static struct mbuf *
hfsc_dequeue(struct ifaltq *ifq, int op)
{
	struct hfsc_if	*hif = (struct hfsc_if *)ifq->altq_disc;
	struct hfsc_class *cl;
	struct mbuf *m;
	int len, next_len;
	int realtime = 0;
	u_int64_t cur_time;

	IFQ_LOCK_ASSERT(ifq);

	if (hif->hif_packets == 0)
		/* no packet in the tree */
		return (NULL);

	cur_time = read_machclk();

	if (op == ALTDQ_REMOVE && hif->hif_pollcache != NULL) {

		cl = hif->hif_pollcache;
		hif->hif_pollcache = NULL;
		/* check if the class was scheduled by real-time criteria */
		if (cl->cl_rsc != NULL)
			realtime = (cl->cl_e <= cur_time);
	} else {
		/*
		 * if there are eligible classes, use real-time criteria.
		 * find the class with the minimum deadline among
		 * the eligible classes.
		 */
		if ((cl = ellist_get_mindl(hif->hif_eligible, cur_time))
		    != NULL) {
			realtime = 1;
		} else {
#ifdef ALTQ_DEBUG
			int fits = 0;
#endif
			/*
			 * use link-sharing criteria
			 * get the class with the minimum vt in the hierarchy
			 */
			cl = hif->hif_rootclass;
			while (is_a_parent_class(cl)) {

				cl = actlist_firstfit(cl, cur_time);
				if (cl == NULL) {
#ifdef ALTQ_DEBUG
					if (fits > 0)
						printf("%d fit but none found\n",fits);
#endif
					return (NULL);
				}
				/*
				 * update parent's cl_cvtmin.
				 * don't update if the new vt is smaller.
				 */
				if (cl->cl_parent->cl_cvtmin < cl->cl_vt)
					cl->cl_parent->cl_cvtmin = cl->cl_vt;
#ifdef ALTQ_DEBUG
				fits++;
#endif
			}
		}

		if (op == ALTDQ_POLL) {
			hif->hif_pollcache = cl;
			m = hfsc_pollq(cl);
			return (m);
		}
	}

	m = hfsc_getq(cl);
	if (m == NULL)
		panic("hfsc_dequeue:");
	len = m_pktlen(m);
	cl->cl_hif->hif_packets--;
	IFQ_DEC_LEN(ifq);
	PKTCNTR_ADD(&cl->cl_stats.xmit_cnt, len);

	update_vf(cl, len, cur_time);
	if (realtime)
		cl->cl_cumul += len;

	if (!qempty(cl->cl_q)) {
		if (cl->cl_rsc != NULL) {
			/* update ed */
			next_len = m_pktlen(qhead(cl->cl_q));

			if (realtime)
				update_ed(cl, next_len);
			else
				update_d(cl, next_len);
		}
	} else {
		/* the class becomes passive */
		set_passive(cl);
	}

	return (m);
}

static int
hfsc_addq(struct hfsc_class *cl, struct mbuf *m)
{

#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_addq((rio_t *)cl->cl_red, cl->cl_q,
				m, cl->cl_pktattr);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_addq(cl->cl_red, cl->cl_q, m, cl->cl_pktattr);
#endif
	if (qlen(cl->cl_q) >= qlimit(cl->cl_q)) {
		m_freem(m);
		return (-1);
	}

	if (cl->cl_flags & HFCF_CLEARDSCP)
		write_dsfield(m, cl->cl_pktattr, 0);

	_addq(cl->cl_q, m);

	return (0);
}

static struct mbuf *
hfsc_getq(struct hfsc_class *cl)
{
#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_getq((rio_t *)cl->cl_red, cl->cl_q);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_getq(cl->cl_red, cl->cl_q);
#endif
	return _getq(cl->cl_q);
}

static struct mbuf *
hfsc_pollq(struct hfsc_class *cl)
{
	return qhead(cl->cl_q);
}

static void
hfsc_purgeq(struct hfsc_class *cl)
{
	struct mbuf *m;

	if (qempty(cl->cl_q))
		return;

	while ((m = _getq(cl->cl_q)) != NULL) {
		PKTCNTR_ADD(&cl->cl_stats.drop_cnt, m_pktlen(m));
		m_freem(m);
		cl->cl_hif->hif_packets--;
		IFQ_DEC_LEN(cl->cl_hif->hif_ifq);
	}
	ASSERT(qlen(cl->cl_q) == 0);

	update_vf(cl, 0, 0);	/* remove cl from the actlist */
	set_passive(cl);
}

static void
set_active(struct hfsc_class *cl, int len)
{
	if (cl->cl_rsc != NULL)
		init_ed(cl, len);
	if (cl->cl_fsc != NULL)
		init_vf(cl, len);

	cl->cl_stats.period++;
}

static void
set_passive(struct hfsc_class *cl)
{
	if (cl->cl_rsc != NULL)
		ellist_remove(cl);

	/*
	 * actlist is now handled in update_vf() so that update_vf(cl, 0, 0)
	 * needs to be called explicitly to remove a class from actlist
	 */
}

static void
init_ed(struct hfsc_class *cl, int next_len)
{
	u_int64_t cur_time;

	cur_time = read_machclk();

	/* update the deadline curve */
	rtsc_min(&cl->cl_deadline, cl->cl_rsc, cur_time, cl->cl_cumul);

	/*
	 * update the eligible curve.
	 * for concave, it is equal to the deadline curve.
	 * for convex, it is a linear curve with slope m2.
	 */
	cl->cl_eligible = cl->cl_deadline;
	if (cl->cl_rsc->sm1 <= cl->cl_rsc->sm2) {
		cl->cl_eligible.dx = 0;
		cl->cl_eligible.dy = 0;
	}

	/* compute e and d */
	cl->cl_e = rtsc_y2x(&cl->cl_eligible, cl->cl_cumul);
	cl->cl_d = rtsc_y2x(&cl->cl_deadline, cl->cl_cumul + next_len);

	ellist_insert(cl);
}

static void
update_ed(struct hfsc_class *cl, int next_len)
{
	cl->cl_e = rtsc_y2x(&cl->cl_eligible, cl->cl_cumul);
	cl->cl_d = rtsc_y2x(&cl->cl_deadline, cl->cl_cumul + next_len);

	ellist_update(cl);
}

static void
update_d(struct hfsc_class *cl, int next_len)
{
	cl->cl_d = rtsc_y2x(&cl->cl_deadline, cl->cl_cumul + next_len);
}

static void
init_vf(struct hfsc_class *cl, int len)
{
	struct hfsc_class *max_cl, *p;
	u_int64_t vt, f, cur_time;
	int go_active;

	cur_time = 0;
	go_active = 1;
	for ( ; cl->cl_parent != NULL; cl = cl->cl_parent) {

		if (go_active && cl->cl_nactive++ == 0)
			go_active = 1;
		else
			go_active = 0;

		if (go_active) {
			max_cl = actlist_last(cl->cl_parent->cl_actc);
			if (max_cl != NULL) {
				/*
				 * set vt to the average of the min and max
				 * classes.  if the parent's period didn't
				 * change, don't decrease vt of the class.
				 */
				vt = max_cl->cl_vt;
				if (cl->cl_parent->cl_cvtmin != 0)
					vt = (cl->cl_parent->cl_cvtmin + vt)/2;

				if (cl->cl_parent->cl_vtperiod !=
				    cl->cl_parentperiod || vt > cl->cl_vt)
					cl->cl_vt = vt;
			} else {
				/*
				 * first child for a new parent backlog period.
				 * add parent's cvtmax to vtoff of children
				 * to make a new vt (vtoff + vt) larger than
				 * the vt in the last period for all children.
				 */
				vt = cl->cl_parent->cl_cvtmax;
				for (p = cl->cl_parent->cl_children; p != NULL;
				     p = p->cl_siblings)
					p->cl_vtoff += vt;
				cl->cl_vt = 0;
				cl->cl_parent->cl_cvtmax = 0;
				cl->cl_parent->cl_cvtmin = 0;
			}
			cl->cl_initvt = cl->cl_vt;

			/* update the virtual curve */
			vt = cl->cl_vt + cl->cl_vtoff;
			rtsc_min(&cl->cl_virtual, cl->cl_fsc, vt, cl->cl_total);
			if (cl->cl_virtual.x == vt) {
				cl->cl_virtual.x -= cl->cl_vtoff;
				cl->cl_vtoff = 0;
			}
			cl->cl_vtadj = 0;

			cl->cl_vtperiod++;  /* increment vt period */
			cl->cl_parentperiod = cl->cl_parent->cl_vtperiod;
			if (cl->cl_parent->cl_nactive == 0)
				cl->cl_parentperiod++;
			cl->cl_f = 0;

			actlist_insert(cl);

			if (cl->cl_usc != NULL) {
				/* class has upper limit curve */
				if (cur_time == 0)
					cur_time = read_machclk();

				/* update the ulimit curve */
				rtsc_min(&cl->cl_ulimit, cl->cl_usc, cur_time,
				    cl->cl_total);
				/* compute myf */
				cl->cl_myf = rtsc_y2x(&cl->cl_ulimit,
				    cl->cl_total);
				cl->cl_myfadj = 0;
			}
		}

		if (cl->cl_myf > cl->cl_cfmin)
			f = cl->cl_myf;
		else
			f = cl->cl_cfmin;
		if (f != cl->cl_f) {
			cl->cl_f = f;
			update_cfmin(cl->cl_parent);
		}
	}
}

static void
update_vf(struct hfsc_class *cl, int len, u_int64_t cur_time)
{
	u_int64_t f, myf_bound, delta;
	int go_passive;

	go_passive = qempty(cl->cl_q);

	for (; cl->cl_parent != NULL; cl = cl->cl_parent) {

		cl->cl_total += len;

		if (cl->cl_fsc == NULL || cl->cl_nactive == 0)
			continue;

		if (go_passive && --cl->cl_nactive == 0)
			go_passive = 1;
		else
			go_passive = 0;

		if (go_passive) {
			/* no more active child, going passive */

			/* update cvtmax of the parent class */
			if (cl->cl_vt > cl->cl_parent->cl_cvtmax)
				cl->cl_parent->cl_cvtmax = cl->cl_vt;

			/* remove this class from the vt list */
			actlist_remove(cl);

			update_cfmin(cl->cl_parent);

			continue;
		}

		/*
		 * update vt and f
		 */
		cl->cl_vt = rtsc_y2x(&cl->cl_virtual, cl->cl_total)
		    - cl->cl_vtoff + cl->cl_vtadj;

		/*
		 * if vt of the class is smaller than cvtmin,
		 * the class was skipped in the past due to non-fit.
		 * if so, we need to adjust vtadj.
		 */
		if (cl->cl_vt < cl->cl_parent->cl_cvtmin) {
			cl->cl_vtadj += cl->cl_parent->cl_cvtmin - cl->cl_vt;
			cl->cl_vt = cl->cl_parent->cl_cvtmin;
		}

		/* update the vt list */
		actlist_update(cl);

		if (cl->cl_usc != NULL) {
			cl->cl_myf = cl->cl_myfadj
			    + rtsc_y2x(&cl->cl_ulimit, cl->cl_total);

			/*
			 * if myf lags behind by more than one clock tick
			 * from the current time, adjust myfadj to prevent
			 * a rate-limited class from going greedy.
			 * in a steady state under rate-limiting, myf
			 * fluctuates within one clock tick.
			 */
			myf_bound = cur_time - machclk_per_tick;
			if (cl->cl_myf < myf_bound) {
				delta = cur_time - cl->cl_myf;
				cl->cl_myfadj += delta;
				cl->cl_myf += delta;
			}
		}

		/* cl_f is max(cl_myf, cl_cfmin) */
		if (cl->cl_myf > cl->cl_cfmin)
			f = cl->cl_myf;
		else
			f = cl->cl_cfmin;
		if (f != cl->cl_f) {
			cl->cl_f = f;
			update_cfmin(cl->cl_parent);
		}
	}
}

static void
update_cfmin(struct hfsc_class *cl)
{
	struct hfsc_class *p;
	u_int64_t cfmin;

	if (TAILQ_EMPTY(cl->cl_actc)) {
		cl->cl_cfmin = 0;
		return;
	}
	cfmin = HT_INFINITY;
	TAILQ_FOREACH(p, cl->cl_actc, cl_actlist) {
		if (p->cl_f == 0) {
			cl->cl_cfmin = 0;
			return;
		}
		if (p->cl_f < cfmin)
			cfmin = p->cl_f;
	}
	cl->cl_cfmin = cfmin;
}

/*
 * TAILQ based ellist and actlist implementation
 * (ion wanted to make a calendar queue based implementation)
 */
/*
 * eligible list holds backlogged classes being sorted by their eligible times.
 * there is one eligible list per interface.
 */

static ellist_t *
ellist_alloc(void)
{
	ellist_t *head;

	head = malloc(sizeof(ellist_t), M_DEVBUF, M_WAITOK);
	TAILQ_INIT(head);
	return (head);
}

static void
ellist_destroy(ellist_t *head)
{
	free(head, M_DEVBUF);
}

static void
ellist_insert(struct hfsc_class *cl)
{
	struct hfsc_if	*hif = cl->cl_hif;
	struct hfsc_class *p;

	/* check the last entry first */
	if ((p = TAILQ_LAST(hif->hif_eligible, _eligible)) == NULL ||
	    p->cl_e <= cl->cl_e) {
		TAILQ_INSERT_TAIL(hif->hif_eligible, cl, cl_ellist);
		return;
	}

	TAILQ_FOREACH(p, hif->hif_eligible, cl_ellist) {
		if (cl->cl_e < p->cl_e) {
			TAILQ_INSERT_BEFORE(p, cl, cl_ellist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

static void
ellist_remove(struct hfsc_class *cl)
{
	struct hfsc_if	*hif = cl->cl_hif;

	TAILQ_REMOVE(hif->hif_eligible, cl, cl_ellist);
}

static void
ellist_update(struct hfsc_class *cl)
{
	struct hfsc_if	*hif = cl->cl_hif;
	struct hfsc_class *p, *last;

	/*
	 * the eligible time of a class increases monotonically.
	 * if the next entry has a larger eligible time, nothing to do.
	 */
	p = TAILQ_NEXT(cl, cl_ellist);
	if (p == NULL || cl->cl_e <= p->cl_e)
		return;

	/* check the last entry */
	last = TAILQ_LAST(hif->hif_eligible, _eligible);
	ASSERT(last != NULL);
	if (last->cl_e <= cl->cl_e) {
		TAILQ_REMOVE(hif->hif_eligible, cl, cl_ellist);
		TAILQ_INSERT_TAIL(hif->hif_eligible, cl, cl_ellist);
		return;
	}

	/*
	 * the new position must be between the next entry
	 * and the last entry
	 */
	while ((p = TAILQ_NEXT(p, cl_ellist)) != NULL) {
		if (cl->cl_e < p->cl_e) {
			TAILQ_REMOVE(hif->hif_eligible, cl, cl_ellist);
			TAILQ_INSERT_BEFORE(p, cl, cl_ellist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

/* find the class with the minimum deadline among the eligible classes */
struct hfsc_class *
ellist_get_mindl(ellist_t *head, u_int64_t cur_time)
{
	struct hfsc_class *p, *cl = NULL;

	TAILQ_FOREACH(p, head, cl_ellist) {
		if (p->cl_e > cur_time)
			break;
		if (cl == NULL || p->cl_d < cl->cl_d)
			cl = p;
	}
	return (cl);
}

/*
 * active children list holds backlogged child classes being sorted
 * by their virtual time.
 * each intermediate class has one active children list.
 */
static actlist_t *
actlist_alloc(void)
{
	actlist_t *head;

	head = malloc(sizeof(actlist_t), M_DEVBUF, M_WAITOK);
	TAILQ_INIT(head);
	return (head);
}

static void
actlist_destroy(actlist_t *head)
{
	free(head, M_DEVBUF);
}
static void
actlist_insert(struct hfsc_class *cl)
{
	struct hfsc_class *p;

	/* check the last entry first */
	if ((p = TAILQ_LAST(cl->cl_parent->cl_actc, _active)) == NULL
	    || p->cl_vt <= cl->cl_vt) {
		TAILQ_INSERT_TAIL(cl->cl_parent->cl_actc, cl, cl_actlist);
		return;
	}

	TAILQ_FOREACH(p, cl->cl_parent->cl_actc, cl_actlist) {
		if (cl->cl_vt < p->cl_vt) {
			TAILQ_INSERT_BEFORE(p, cl, cl_actlist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

static void
actlist_remove(struct hfsc_class *cl)
{
	TAILQ_REMOVE(cl->cl_parent->cl_actc, cl, cl_actlist);
}

static void
actlist_update(struct hfsc_class *cl)
{
	struct hfsc_class *p, *last;

	/*
	 * the virtual time of a class increases monotonically during its
	 * backlogged period.
	 * if the next entry has a larger virtual time, nothing to do.
	 */
	p = TAILQ_NEXT(cl, cl_actlist);
	if (p == NULL || cl->cl_vt < p->cl_vt)
		return;

	/* check the last entry */
	last = TAILQ_LAST(cl->cl_parent->cl_actc, _active);
	ASSERT(last != NULL);
	if (last->cl_vt <= cl->cl_vt) {
		TAILQ_REMOVE(cl->cl_parent->cl_actc, cl, cl_actlist);
		TAILQ_INSERT_TAIL(cl->cl_parent->cl_actc, cl, cl_actlist);
		return;
	}

	/*
	 * the new position must be between the next entry
	 * and the last entry
	 */
	while ((p = TAILQ_NEXT(p, cl_actlist)) != NULL) {
		if (cl->cl_vt < p->cl_vt) {
			TAILQ_REMOVE(cl->cl_parent->cl_actc, cl, cl_actlist);
			TAILQ_INSERT_BEFORE(p, cl, cl_actlist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

static struct hfsc_class *
actlist_firstfit(struct hfsc_class *cl, u_int64_t cur_time)
{
	struct hfsc_class *p;

	TAILQ_FOREACH(p, cl->cl_actc, cl_actlist) {
		if (p->cl_f <= cur_time)
			return (p);
	}
	return (NULL);
}

/*
 * service curve support functions
 *
 *  external service curve parameters
 *	m: bits/sec
 *	d: msec
 *  internal service curve parameters
 *	sm: (bytes/tsc_interval) << SM_SHIFT
 *	ism: (tsc_count/byte) << ISM_SHIFT
 *	dx: tsc_count
 *
 * SM_SHIFT and ISM_SHIFT are scaled in order to keep effective digits.
 * we should be able to handle 100K-1Gbps linkspeed with 200Hz-1GHz CPU
 * speed.  SM_SHIFT and ISM_SHIFT are selected to have at least 3 effective
 * digits in decimal using the following table.
 *
 *  bits/sec    100Kbps     1Mbps     10Mbps     100Mbps    1Gbps
 *  ----------+-------------------------------------------------------
 *  bytes/nsec  12.5e-6    125e-6     1250e-6    12500e-6   125000e-6
 *  sm(500MHz)  25.0e-6    250e-6     2500e-6    25000e-6   250000e-6
 *  sm(200MHz)  62.5e-6    625e-6     6250e-6    62500e-6   625000e-6
 *
 *  nsec/byte   80000      8000       800        80         8
 *  ism(500MHz) 40000      4000       400        40         4
 *  ism(200MHz) 16000      1600       160        16         1.6
 */
#define	SM_SHIFT	24
#define	ISM_SHIFT	10

#define	SM_MASK		((1LL << SM_SHIFT) - 1)
#define	ISM_MASK	((1LL << ISM_SHIFT) - 1)

static __inline u_int64_t
seg_x2y(u_int64_t x, u_int64_t sm)
{
	u_int64_t y;

	/*
	 * compute
	 *	y = x * sm >> SM_SHIFT
	 * but divide it for the upper and lower bits to avoid overflow
	 */
	y = (x >> SM_SHIFT) * sm + (((x & SM_MASK) * sm) >> SM_SHIFT);
	return (y);
}

static __inline u_int64_t
seg_y2x(u_int64_t y, u_int64_t ism)
{
	u_int64_t x;

	if (y == 0)
		x = 0;
	else if (ism == HT_INFINITY)
		x = HT_INFINITY;
	else {
		x = (y >> ISM_SHIFT) * ism
		    + (((y & ISM_MASK) * ism) >> ISM_SHIFT);
	}
	return (x);
}

static __inline u_int64_t
m2sm(u_int m)
{
	u_int64_t sm;

	sm = ((u_int64_t)m << SM_SHIFT) / 8 / machclk_freq;
	return (sm);
}

static __inline u_int64_t
m2ism(u_int m)
{
	u_int64_t ism;

	if (m == 0)
		ism = HT_INFINITY;
	else
		ism = ((u_int64_t)machclk_freq << ISM_SHIFT) * 8 / m;
	return (ism);
}

static __inline u_int64_t
d2dx(u_int d)
{
	u_int64_t dx;

	dx = ((u_int64_t)d * machclk_freq) / 1000;
	return (dx);
}

static u_int
sm2m(u_int64_t sm)
{
	u_int64_t m;

	m = (sm * 8 * machclk_freq) >> SM_SHIFT;
	return ((u_int)m);
}

static u_int
dx2d(u_int64_t dx)
{
	u_int64_t d;

	d = dx * 1000 / machclk_freq;
	return ((u_int)d);
}

static void
sc2isc(struct service_curve *sc, struct internal_sc *isc)
{
	isc->sm1 = m2sm(sc->m1);
	isc->ism1 = m2ism(sc->m1);
	isc->dx = d2dx(sc->d);
	isc->dy = seg_x2y(isc->dx, isc->sm1);
	isc->sm2 = m2sm(sc->m2);
	isc->ism2 = m2ism(sc->m2);
}

/*
 * initialize the runtime service curve with the given internal
 * service curve starting at (x, y).
 */
static void
rtsc_init(struct runtime_sc *rtsc, struct internal_sc * isc, u_int64_t x,
    u_int64_t y)
{
	rtsc->x =	x;
	rtsc->y =	y;
	rtsc->sm1 =	isc->sm1;
	rtsc->ism1 =	isc->ism1;
	rtsc->dx =	isc->dx;
	rtsc->dy =	isc->dy;
	rtsc->sm2 =	isc->sm2;
	rtsc->ism2 =	isc->ism2;
}

/*
 * calculate the y-projection of the runtime service curve by the
 * given x-projection value
 */
static u_int64_t
rtsc_y2x(struct runtime_sc *rtsc, u_int64_t y)
{
	u_int64_t	x;

	if (y < rtsc->y)
		x = rtsc->x;
	else if (y <= rtsc->y + rtsc->dy) {
		/* x belongs to the 1st segment */
		if (rtsc->dy == 0)
			x = rtsc->x + rtsc->dx;
		else
			x = rtsc->x + seg_y2x(y - rtsc->y, rtsc->ism1);
	} else {
		/* x belongs to the 2nd segment */
		x = rtsc->x + rtsc->dx
		    + seg_y2x(y - rtsc->y - rtsc->dy, rtsc->ism2);
	}
	return (x);
}

static u_int64_t
rtsc_x2y(struct runtime_sc *rtsc, u_int64_t x)
{
	u_int64_t	y;

	if (x <= rtsc->x)
		y = rtsc->y;
	else if (x <= rtsc->x + rtsc->dx)
		/* y belongs to the 1st segment */
		y = rtsc->y + seg_x2y(x - rtsc->x, rtsc->sm1);
	else
		/* y belongs to the 2nd segment */
		y = rtsc->y + rtsc->dy
		    + seg_x2y(x - rtsc->x - rtsc->dx, rtsc->sm2);
	return (y);
}

/*
 * update the runtime service curve by taking the minimum of the current
 * runtime service curve and the service curve starting at (x, y).
 */
static void
rtsc_min(struct runtime_sc *rtsc, struct internal_sc *isc, u_int64_t x,
    u_int64_t y)
{
	u_int64_t	y1, y2, dx, dy;

	if (isc->sm1 <= isc->sm2) {
		/* service curve is convex */
		y1 = rtsc_x2y(rtsc, x);
		if (y1 < y)
			/* the current rtsc is smaller */
			return;
		rtsc->x = x;
		rtsc->y = y;
		return;
	}

	/*
	 * service curve is concave
	 * compute the two y values of the current rtsc
	 *	y1: at x
	 *	y2: at (x + dx)
	 */
	y1 = rtsc_x2y(rtsc, x);
	if (y1 <= y) {
		/* rtsc is below isc, no change to rtsc */
		return;
	}

	y2 = rtsc_x2y(rtsc, x + isc->dx);
	if (y2 >= y + isc->dy) {
		/* rtsc is above isc, replace rtsc by isc */
		rtsc->x = x;
		rtsc->y = y;
		rtsc->dx = isc->dx;
		rtsc->dy = isc->dy;
		return;
	}

	/*
	 * the two curves intersect
	 * compute the offsets (dx, dy) using the reverse
	 * function of seg_x2y()
	 *	seg_x2y(dx, sm1) == seg_x2y(dx, sm2) + (y1 - y)
	 */
	dx = ((y1 - y) << SM_SHIFT) / (isc->sm1 - isc->sm2);
	/*
	 * check if (x, y1) belongs to the 1st segment of rtsc.
	 * if so, add the offset.
	 */
	if (rtsc->x + rtsc->dx > x)
		dx += rtsc->x + rtsc->dx - x;
	dy = seg_x2y(dx, isc->sm1);

	rtsc->x = x;
	rtsc->y = y;
	rtsc->dx = dx;
	rtsc->dy = dy;
	return;
}

static void
get_class_stats(struct hfsc_classstats *sp, struct hfsc_class *cl)
{
	sp->class_id = cl->cl_id;
	sp->class_handle = cl->cl_handle;

	if (cl->cl_rsc != NULL) {
		sp->rsc.m1 = sm2m(cl->cl_rsc->sm1);
		sp->rsc.d = dx2d(cl->cl_rsc->dx);
		sp->rsc.m2 = sm2m(cl->cl_rsc->sm2);
	} else {
		sp->rsc.m1 = 0;
		sp->rsc.d = 0;
		sp->rsc.m2 = 0;
	}
	if (cl->cl_fsc != NULL) {
		sp->fsc.m1 = sm2m(cl->cl_fsc->sm1);
		sp->fsc.d = dx2d(cl->cl_fsc->dx);
		sp->fsc.m2 = sm2m(cl->cl_fsc->sm2);
	} else {
		sp->fsc.m1 = 0;
		sp->fsc.d = 0;
		sp->fsc.m2 = 0;
	}
	if (cl->cl_usc != NULL) {
		sp->usc.m1 = sm2m(cl->cl_usc->sm1);
		sp->usc.d = dx2d(cl->cl_usc->dx);
		sp->usc.m2 = sm2m(cl->cl_usc->sm2);
	} else {
		sp->usc.m1 = 0;
		sp->usc.d = 0;
		sp->usc.m2 = 0;
	}

	sp->total = cl->cl_total;
	sp->cumul = cl->cl_cumul;

	sp->d = cl->cl_d;
	sp->e = cl->cl_e;
	sp->vt = cl->cl_vt;
	sp->f = cl->cl_f;

	sp->initvt = cl->cl_initvt;
	sp->vtperiod = cl->cl_vtperiod;
	sp->parentperiod = cl->cl_parentperiod;
	sp->nactive = cl->cl_nactive;
	sp->vtoff = cl->cl_vtoff;
	sp->cvtmax = cl->cl_cvtmax;
	sp->myf = cl->cl_myf;
	sp->cfmin = cl->cl_cfmin;
	sp->cvtmin = cl->cl_cvtmin;
	sp->myfadj = cl->cl_myfadj;
	sp->vtadj = cl->cl_vtadj;

	sp->cur_time = read_machclk();
	sp->machclk_freq = machclk_freq;

	sp->qlength = qlen(cl->cl_q);
	sp->qlimit = qlimit(cl->cl_q);
	sp->xmit_cnt = cl->cl_stats.xmit_cnt;
	sp->drop_cnt = cl->cl_stats.drop_cnt;
	sp->period = cl->cl_stats.period;

	sp->qtype = qtype(cl->cl_q);
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		red_getstats(cl->cl_red, &sp->red[0]);
#endif
#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		rio_getstats((rio_t *)cl->cl_red, &sp->red[0]);
#endif
}

/* convert a class handle to the corresponding class pointer */
static struct hfsc_class *
clh_to_clp(struct hfsc_if *hif, u_int32_t chandle)
{
	int i;
	struct hfsc_class *cl;

	if (chandle == 0)
		return (NULL);
	/*
	 * first, try optimistically the slot matching the lower bits of
	 * the handle.  if it fails, do the linear table search.
	 */
	i = chandle % HFSC_MAX_CLASSES;
	if ((cl = hif->hif_class_tbl[i]) != NULL && cl->cl_handle == chandle)
		return (cl);
	for (i = 0; i < HFSC_MAX_CLASSES; i++)
		if ((cl = hif->hif_class_tbl[i]) != NULL &&
		    cl->cl_handle == chandle)
			return (cl);
	return (NULL);
}

#ifdef ALTQ3_COMPAT
static struct hfsc_if *
hfsc_attach(ifq, bandwidth)
	struct ifaltq *ifq;
	u_int bandwidth;
{
	struct hfsc_if *hif;

	hif = malloc(sizeof(struct hfsc_if),
	       M_DEVBUF, M_WAITOK);
	if (hif == NULL)
		return (NULL);
	bzero(hif, sizeof(struct hfsc_if));

	hif->hif_eligible = ellist_alloc();
	if (hif->hif_eligible == NULL) {
		free(hif, M_DEVBUF);
		return NULL;
	}

	hif->hif_ifq = ifq;

	/* add this state to the hfsc list */
	hif->hif_next = hif_list;
	hif_list = hif;

	return (hif);
}

static int
hfsc_detach(hif)
	struct hfsc_if *hif;
{
	(void)hfsc_clear_interface(hif);
	(void)hfsc_class_destroy(hif->hif_rootclass);

	/* remove this interface from the hif list */
	if (hif_list == hif)
		hif_list = hif->hif_next;
	else {
		struct hfsc_if *h;

		for (h = hif_list; h != NULL; h = h->hif_next)
			if (h->hif_next == hif) {
				h->hif_next = hif->hif_next;
				break;
			}
		ASSERT(h != NULL);
	}

	ellist_destroy(hif->hif_eligible);

	free(hif, M_DEVBUF);

	return (0);
}

static int
hfsc_class_modify(cl, rsc, fsc, usc)
	struct hfsc_class *cl;
	struct service_curve *rsc, *fsc, *usc;
{
	struct internal_sc *rsc_tmp, *fsc_tmp, *usc_tmp;
	u_int64_t cur_time;
	int s;

	rsc_tmp = fsc_tmp = usc_tmp = NULL;
	if (rsc != NULL && (rsc->m1 != 0 || rsc->m2 != 0) &&
	    cl->cl_rsc == NULL) {
		rsc_tmp = malloc(		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (rsc_tmp == NULL)
			return (ENOMEM);
	}
	if (fsc != NULL && (fsc->m1 != 0 || fsc->m2 != 0) &&
	    cl->cl_fsc == NULL) {
		fsc_tmp = malloc(		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (fsc_tmp == NULL)
			return (ENOMEM);
	}
	if (usc != NULL && (usc->m1 != 0 || usc->m2 != 0) &&
	    cl->cl_usc == NULL) {
		usc_tmp = malloc(		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (usc_tmp == NULL)
			return (ENOMEM);
	}

	cur_time = read_machclk();
#ifdef __NetBSD__
	s = splnet();
#else
	s = splimp();
#endif
	IFQ_LOCK(cl->cl_hif->hif_ifq);

	if (rsc != NULL) {
		if (rsc->m1 == 0 && rsc->m2 == 0) {
			if (cl->cl_rsc != NULL) {
				if (!qempty(cl->cl_q))
					hfsc_purgeq(cl);
				free(cl->cl_rsc, M_DEVBUF);
				cl->cl_rsc = NULL;
			}
		} else {
			if (cl->cl_rsc == NULL)
				cl->cl_rsc = rsc_tmp;
			sc2isc(rsc, cl->cl_rsc);
			rtsc_init(&cl->cl_deadline, cl->cl_rsc, cur_time,
			    cl->cl_cumul);
			cl->cl_eligible = cl->cl_deadline;
			if (cl->cl_rsc->sm1 <= cl->cl_rsc->sm2) {
				cl->cl_eligible.dx = 0;
				cl->cl_eligible.dy = 0;
			}
		}
	}

	if (fsc != NULL) {
		if (fsc->m1 == 0 && fsc->m2 == 0) {
			if (cl->cl_fsc != NULL) {
				if (!qempty(cl->cl_q))
					hfsc_purgeq(cl);
				free(cl->cl_fsc, M_DEVBUF);
				cl->cl_fsc = NULL;
			}
		} else {
			if (cl->cl_fsc == NULL)
				cl->cl_fsc = fsc_tmp;
			sc2isc(fsc, cl->cl_fsc);
			rtsc_init(&cl->cl_virtual, cl->cl_fsc, cl->cl_vt,
			    cl->cl_total);
		}
	}

	if (usc != NULL) {
		if (usc->m1 == 0 && usc->m2 == 0) {
			if (cl->cl_usc != NULL) {
				free(cl->cl_usc, M_DEVBUF);
				cl->cl_usc = NULL;
				cl->cl_myf = 0;
			}
		} else {
			if (cl->cl_usc == NULL)
				cl->cl_usc = usc_tmp;
			sc2isc(usc, cl->cl_usc);
			rtsc_init(&cl->cl_ulimit, cl->cl_usc, cur_time,
			    cl->cl_total);
		}
	}

	if (!qempty(cl->cl_q)) {
		if (cl->cl_rsc != NULL)
			update_ed(cl, m_pktlen(qhead(cl->cl_q)));
		if (cl->cl_fsc != NULL)
			update_vf(cl, 0, cur_time);
		/* is this enough? */
	}

	IFQ_UNLOCK(cl->cl_hif->hif_ifq);
	splx(s);

	return (0);
}

/*
 * hfsc device interface
 */
int
hfscopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
#if (__FreeBSD_version > 500000)
	struct thread *p;
#else
	struct proc *p;
#endif
{
	if (machclk_freq == 0)
		init_machclk();

	if (machclk_freq == 0) {
		printf("hfsc: no cpu clock available!\n");
		return (ENXIO);
	}

	/* everything will be done when the queueing scheme is attached. */
	return 0;
}

int
hfscclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
#if (__FreeBSD_version > 500000)
	struct thread *p;
#else
	struct proc *p;
#endif
{
	struct hfsc_if *hif;
	int err, error = 0;

	while ((hif = hif_list) != NULL) {
		/* destroy all */
		if (ALTQ_IS_ENABLED(hif->hif_ifq))
			altq_disable(hif->hif_ifq);

		err = altq_detach(hif->hif_ifq);
		if (err == 0)
			err = hfsc_detach(hif);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
hfscioctl(dev, cmd, addr, flag, p)
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
	struct hfsc_if *hif;
	struct hfsc_interface *ifacep;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	case HFSC_GETSTATS:
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

	case HFSC_IF_ATTACH:
		error = hfsccmd_if_attach((struct hfsc_attach *)addr);
		break;

	case HFSC_IF_DETACH:
		error = hfsccmd_if_detach((struct hfsc_interface *)addr);
		break;

	case HFSC_ENABLE:
	case HFSC_DISABLE:
	case HFSC_CLEAR_HIERARCHY:
		ifacep = (struct hfsc_interface *)addr;
		if ((hif = altq_lookup(ifacep->hfsc_ifname,
				       ALTQT_HFSC)) == NULL) {
			error = EBADF;
			break;
		}

		switch (cmd) {

		case HFSC_ENABLE:
			if (hif->hif_defaultclass == NULL) {
#ifdef ALTQ_DEBUG
				printf("hfsc: no default class\n");
#endif
				error = EINVAL;
				break;
			}
			error = altq_enable(hif->hif_ifq);
			break;

		case HFSC_DISABLE:
			error = altq_disable(hif->hif_ifq);
			break;

		case HFSC_CLEAR_HIERARCHY:
			hfsc_clear_interface(hif);
			break;
		}
		break;

	case HFSC_ADD_CLASS:
		error = hfsccmd_add_class((struct hfsc_add_class *)addr);
		break;

	case HFSC_DEL_CLASS:
		error = hfsccmd_delete_class((struct hfsc_delete_class *)addr);
		break;

	case HFSC_MOD_CLASS:
		error = hfsccmd_modify_class((struct hfsc_modify_class *)addr);
		break;

	case HFSC_ADD_FILTER:
		error = hfsccmd_add_filter((struct hfsc_add_filter *)addr);
		break;

	case HFSC_DEL_FILTER:
		error = hfsccmd_delete_filter((struct hfsc_delete_filter *)addr);
		break;

	case HFSC_GETSTATS:
		error = hfsccmd_class_stats((struct hfsc_class_stats *)addr);
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

static int
hfsccmd_if_attach(ap)
	struct hfsc_attach *ap;
{
	struct hfsc_if *hif;
	struct ifnet *ifp;
	int error;

	if ((ifp = ifunit(ap->iface.hfsc_ifname)) == NULL)
		return (ENXIO);

	if ((hif = hfsc_attach(&ifp->if_snd, ap->bandwidth)) == NULL)
		return (ENOMEM);

	/*
	 * set HFSC to this ifnet structure.
	 */
	if ((error = altq_attach(&ifp->if_snd, ALTQT_HFSC, hif,
				 hfsc_enqueue, hfsc_dequeue, hfsc_request,
				 &hif->hif_classifier, acc_classify)) != 0)
		(void)hfsc_detach(hif);

	return (error);
}

static int
hfsccmd_if_detach(ap)
	struct hfsc_interface *ap;
{
	struct hfsc_if *hif;
	int error;

	if ((hif = altq_lookup(ap->hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if (ALTQ_IS_ENABLED(hif->hif_ifq))
		altq_disable(hif->hif_ifq);

	if ((error = altq_detach(hif->hif_ifq)))
		return (error);

	return hfsc_detach(hif);
}

static int
hfsccmd_add_class(ap)
	struct hfsc_add_class *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl, *parent;
	int	i;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if (ap->parent_handle == HFSC_NULLCLASS_HANDLE &&
	    hif->hif_rootclass == NULL)
		parent = NULL;
	else if ((parent = clh_to_clp(hif, ap->parent_handle)) == NULL)
		return (EINVAL);

	/* assign a class handle (use a free slot number for now) */
	for (i = 1; i < HFSC_MAX_CLASSES; i++)
		if (hif->hif_class_tbl[i] == NULL)
			break;
	if (i == HFSC_MAX_CLASSES)
		return (EBUSY);

	if ((cl = hfsc_class_create(hif, &ap->service_curve, NULL, NULL,
	    parent, ap->qlimit, ap->flags, i)) == NULL)
		return (ENOMEM);

	/* return a class handle to the user */
	ap->class_handle = i;

	return (0);
}

static int
hfsccmd_delete_class(ap)
	struct hfsc_delete_class *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, ap->class_handle)) == NULL)
		return (EINVAL);

	return hfsc_class_destroy(cl);
}

static int
hfsccmd_modify_class(ap)
	struct hfsc_modify_class *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;
	struct service_curve *rsc = NULL;
	struct service_curve *fsc = NULL;
	struct service_curve *usc = NULL;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, ap->class_handle)) == NULL)
		return (EINVAL);

	if (ap->sctype & HFSC_REALTIMESC)
		rsc = &ap->service_curve;
	if (ap->sctype & HFSC_LINKSHARINGSC)
		fsc = &ap->service_curve;
	if (ap->sctype & HFSC_UPPERLIMITSC)
		usc = &ap->service_curve;

	return hfsc_class_modify(cl, rsc, fsc, usc);
}

static int
hfsccmd_add_filter(ap)
	struct hfsc_add_filter *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, ap->class_handle)) == NULL)
		return (EINVAL);

	if (is_a_parent_class(cl)) {
#ifdef ALTQ_DEBUG
		printf("hfsccmd_add_filter: not a leaf class!\n");
#endif
		return (EINVAL);
	}

	return acc_add_filter(&hif->hif_classifier, &ap->filter,
			      cl, &ap->filter_handle);
}

static int
hfsccmd_delete_filter(ap)
	struct hfsc_delete_filter *ap;
{
	struct hfsc_if *hif;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	return acc_delete_filter(&hif->hif_classifier,
				 ap->filter_handle);
}

static int
hfsccmd_class_stats(ap)
	struct hfsc_class_stats *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;
	struct hfsc_classstats stats, *usp;
	int	n, nclasses, error;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	ap->cur_time = read_machclk();
	ap->machclk_freq = machclk_freq;
	ap->hif_classes = hif->hif_classes;
	ap->hif_packets = hif->hif_packets;

	/* skip the first N classes in the tree */
	nclasses = ap->nskip;
	for (cl = hif->hif_rootclass, n = 0; cl != NULL && n < nclasses;
	     cl = hfsc_nextclass(cl), n++)
		;
	if (n != nclasses)
		return (EINVAL);

	/* then, read the next N classes in the tree */
	nclasses = ap->nclasses;
	usp = ap->stats;
	for (n = 0; cl != NULL && n < nclasses; cl = hfsc_nextclass(cl), n++) {

		get_class_stats(&stats, cl);

		if ((error = copyout((caddr_t)&stats, (caddr_t)usp++,
				     sizeof(stats))) != 0)
			return (error);
	}

	ap->nclasses = n;

	return (0);
}

#ifdef KLD_MODULE

static struct altqsw hfsc_sw =
	{"hfsc", hfscopen, hfscclose, hfscioctl};

ALTQ_MODULE(altq_hfsc, ALTQT_HFSC, &hfsc_sw);
MODULE_DEPEND(altq_hfsc, altq_red, 1, 1, 1);
MODULE_DEPEND(altq_hfsc, altq_rio, 1, 1, 1);

#endif /* KLD_MODULE */
#endif /* ALTQ3_COMPAT */

#endif /* ALTQ_HFSC */
