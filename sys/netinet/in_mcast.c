/*-
 * Copyright (c) 2007 Bruce M. Simpson.
 * Copyright (c) 2005 Robert N. M. Watson.
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

/*
 * IPv4 multicast socket, group, and socket option processing module.
 * Until further notice, this file requires INET to compile.
 * TODO: Make this infrastructure independent of address family.
 * TODO: Teach netinet6 to use this code.
 * TODO: Hook up SSM logic to IGMPv3/MLDv2.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/igmp_var.h>

#ifndef __SOCKUNION_DECLARED
union sockunion {
	struct sockaddr_storage	ss;
	struct sockaddr		sa;
	struct sockaddr_dl	sdl;
	struct sockaddr_in	sin;
#ifdef INET6
	struct sockaddr_in6	sin6;
#endif
};
typedef union sockunion sockunion_t;
#define __SOCKUNION_DECLARED
#endif /* __SOCKUNION_DECLARED */

static MALLOC_DEFINE(M_IPMADDR, "in_multi", "IPv4 multicast group");
static MALLOC_DEFINE(M_IPMOPTS, "ip_moptions", "IPv4 multicast options");
static MALLOC_DEFINE(M_IPMSOURCE, "in_msource", "IPv4 multicast source filter");

/*
 * The IPv4 multicast list (in_multihead and associated structures) are
 * protected by the global in_multi_mtx.  See in_var.h for more details.  For
 * now, in_multi_mtx is marked as recursible due to IGMP's calling back into
 * ip_output() to send IGMP packets while holding the lock; this probably is
 * not quite desirable.
 */
struct in_multihead in_multihead;	/* XXX BSS initialization */
struct mtx in_multi_mtx;
MTX_SYSINIT(in_multi_mtx, &in_multi_mtx, "in_multi_mtx", MTX_DEF | MTX_RECURSE);

/*
 * Functions with non-static linkage defined in this file should be
 * declared in in_var.h:
 *  imo_match_group()
 *  imo_match_source()
 *  in_addmulti()
 *  in_delmulti()
 *  in_delmulti_locked()
 * and ip_var.h:
 *  inp_freemoptions()
 *  inp_getmoptions()
 *  inp_setmoptions()
 */
static int	imo_grow(struct ip_moptions *);
static int	imo_join_source(struct ip_moptions *, size_t, sockunion_t *);
static int	imo_leave_source(struct ip_moptions *, size_t, sockunion_t *);
static int	inp_change_source_filter(struct inpcb *, struct sockopt *);
static struct ip_moptions *
		inp_findmoptions(struct inpcb *);
static int	inp_get_source_filters(struct inpcb *, struct sockopt *);
static int	inp_join_group(struct inpcb *, struct sockopt *);
static int	inp_leave_group(struct inpcb *, struct sockopt *);
static int	inp_set_multicast_if(struct inpcb *, struct sockopt *);
static int	inp_set_source_filters(struct inpcb *, struct sockopt *);

/*
 * Resize the ip_moptions vector to the next power-of-two minus 1.
 * May be called with locks held; do not sleep.
 */
static int
imo_grow(struct ip_moptions *imo)
{
	struct in_multi		**nmships;
	struct in_multi		**omships;
	struct in_mfilter	 *nmfilters;
	struct in_mfilter	 *omfilters;
	size_t			  idx;
	size_t			  newmax;
	size_t			  oldmax;

	nmships = NULL;
	nmfilters = NULL;
	omships = imo->imo_membership;
	omfilters = imo->imo_mfilters;
	oldmax = imo->imo_max_memberships;
	newmax = ((oldmax + 1) * 2) - 1;

	if (newmax <= IP_MAX_MEMBERSHIPS) {
		nmships = (struct in_multi **)realloc(omships,
		    sizeof(struct in_multi *) * newmax, M_IPMOPTS, M_NOWAIT);
		nmfilters = (struct in_mfilter *)realloc(omfilters,
		    sizeof(struct in_mfilter) * newmax, M_IPMSOURCE, M_NOWAIT);
		if (nmships != NULL && nmfilters != NULL) {
			/* Initialize newly allocated source filter heads. */
			for (idx = oldmax; idx < newmax; idx++) {
				nmfilters[idx].imf_fmode = MCAST_EXCLUDE;
				nmfilters[idx].imf_nsources = 0;
				TAILQ_INIT(&nmfilters[idx].imf_sources);
			}
			imo->imo_max_memberships = newmax;
			imo->imo_membership = nmships;
			imo->imo_mfilters = nmfilters;
		}
	}

	if (nmships == NULL || nmfilters == NULL) {
		if (nmships != NULL)
			free(nmships, M_IPMOPTS);
		if (nmfilters != NULL)
			free(nmfilters, M_IPMSOURCE);
		return (ETOOMANYREFS);
	}

	return (0);
}

/*
 * Add a source to a multicast filter list.
 * Assumes the associated inpcb is locked.
 */
static int
imo_join_source(struct ip_moptions *imo, size_t gidx, sockunion_t *src)
{
	struct in_msource	*ims, *nims;
	struct in_mfilter	*imf;

	KASSERT(src->ss.ss_family == AF_INET, ("%s: !AF_INET", __func__));
	KASSERT(imo->imo_mfilters != NULL,
	    ("%s: imo_mfilters vector not allocated", __func__));

	imf = &imo->imo_mfilters[gidx];
	if (imf->imf_nsources == IP_MAX_SOURCE_FILTER)
		return (ENOBUFS);

	ims = imo_match_source(imo, gidx, &src->sa);
	if (ims != NULL)
		return (EADDRNOTAVAIL);

	/* Do not sleep with inp lock held. */
	MALLOC(nims, struct in_msource *, sizeof(struct in_msource),
	    M_IPMSOURCE, M_NOWAIT | M_ZERO);
	if (nims == NULL)
		return (ENOBUFS);

	nims->ims_addr = src->ss;
	TAILQ_INSERT_TAIL(&imf->imf_sources, nims, ims_next);
	imf->imf_nsources++;

	return (0);
}

static int
imo_leave_source(struct ip_moptions *imo, size_t gidx, sockunion_t *src)
{
	struct in_msource	*ims;
	struct in_mfilter	*imf;

	KASSERT(src->ss.ss_family == AF_INET, ("%s: !AF_INET", __func__));
	KASSERT(imo->imo_mfilters != NULL,
	    ("%s: imo_mfilters vector not allocated", __func__));

	imf = &imo->imo_mfilters[gidx];
	if (imf->imf_nsources == IP_MAX_SOURCE_FILTER)
		return (ENOBUFS);

	ims = imo_match_source(imo, gidx, &src->sa);
	if (ims == NULL)
		return (EADDRNOTAVAIL);

	TAILQ_REMOVE(&imf->imf_sources, ims, ims_next);
	FREE(ims, M_IPMSOURCE);
	imf->imf_nsources--;

	return (0);
}

/*
 * Find an IPv4 multicast group entry for this ip_moptions instance
 * which matches the specified group, and optionally an interface.
 * Return its index into the array, or -1 if not found.
 */
size_t
imo_match_group(struct ip_moptions *imo, struct ifnet *ifp,
    struct sockaddr *group)
{
	sockunion_t	 *gsa;
	struct in_multi	**pinm;
	int		  idx;
	int		  nmships;

	gsa = (sockunion_t *)group;

	/* The imo_membership array may be lazy allocated. */
	if (imo->imo_membership == NULL || imo->imo_num_memberships == 0)
		return (-1);

	nmships = imo->imo_num_memberships;
	pinm = &imo->imo_membership[0];
	for (idx = 0; idx < nmships; idx++, pinm++) {
		if (*pinm == NULL)
			continue;
#if 0
		printf("%s: trying ifp = %p, inaddr = %s ", __func__,
		    ifp, inet_ntoa(gsa->sin.sin_addr));
		printf("against %p, %s\n",
		    (*pinm)->inm_ifp, inet_ntoa((*pinm)->inm_addr));
#endif
		if ((ifp == NULL || ((*pinm)->inm_ifp == ifp)) &&
		    (*pinm)->inm_addr.s_addr == gsa->sin.sin_addr.s_addr) {
			break;
		}
	}
	if (idx >= nmships)
		idx = -1;

	return (idx);
}

/*
 * Find a multicast source entry for this imo which matches
 * the given group index for this socket, and source address.
 */
struct in_msource *
imo_match_source(struct ip_moptions *imo, size_t gidx, struct sockaddr *src)
{
	struct in_mfilter	*imf;
	struct in_msource	*ims, *pims;

	KASSERT(src->sa_family == AF_INET, ("%s: !AF_INET", __func__));
	KASSERT(gidx != -1 && gidx < imo->imo_num_memberships,
	    ("%s: invalid index %d\n", __func__, (int)gidx));

	/* The imo_mfilters array may be lazy allocated. */
	if (imo->imo_mfilters == NULL)
		return (NULL);

	pims = NULL;
	imf = &imo->imo_mfilters[gidx];
	TAILQ_FOREACH(ims, &imf->imf_sources, ims_next) {
		/*
		 * Perform bitwise comparison of two IPv4 addresses.
		 * TODO: Do the same for IPv6.
		 * Do not use sa_equal() for this as it is not aware of
		 * deeper structure in sockaddr_in or sockaddr_in6.
		 */
		if (((struct sockaddr_in *)&ims->ims_addr)->sin_addr.s_addr ==
		    ((struct sockaddr_in *)src)->sin_addr.s_addr) {
			pims = ims;
			break;
		}
	}

	return (pims);
}

/*
 * Join an IPv4 multicast group.
 */
struct in_multi *
in_addmulti(struct in_addr *ap, struct ifnet *ifp)
{
	struct in_multi *inm;

	inm = NULL;

	IFF_LOCKGIANT(ifp);
	IN_MULTI_LOCK();

	IN_LOOKUP_MULTI(*ap, ifp, inm);
	if (inm != NULL) {
		/*
		 * If we already joined this group, just bump the
		 * refcount and return it.
		 */
		KASSERT(inm->inm_refcount >= 1,
		    ("%s: bad refcount %d", __func__, inm->inm_refcount));
		++inm->inm_refcount;
	} else do {
		sockunion_t		 gsa;
		struct ifmultiaddr	*ifma;
		struct in_multi		*ninm;
		int			 error;

		memset(&gsa, 0, sizeof(gsa));
		gsa.sin.sin_family = AF_INET;
		gsa.sin.sin_len = sizeof(struct sockaddr_in);
		gsa.sin.sin_addr = *ap;

		/*
		 * Check if a link-layer group is already associated
		 * with this network-layer group on the given ifnet.
		 * If so, bump the refcount on the existing network-layer
		 * group association and return it.
		 */
		error = if_addmulti(ifp, &gsa.sa, &ifma);
		if (error)
			break;
		if (ifma->ifma_protospec != NULL) {
			inm = (struct in_multi *)ifma->ifma_protospec;
#ifdef INVARIANTS
			if (inm->inm_ifma != ifma || inm->inm_ifp != ifp ||
			    inm->inm_addr.s_addr != ap->s_addr)
				panic("%s: ifma is inconsistent", __func__);
#endif
			++inm->inm_refcount;
			break;
		}

		/*
		 * A new membership is needed; construct it and
		 * perform the IGMP join.
		 */
		ninm = malloc(sizeof(*ninm), M_IPMADDR, M_NOWAIT | M_ZERO);
		if (ninm == NULL) {
			if_delmulti_ifma(ifma);
			break;
		}
		ninm->inm_addr = *ap;
		ninm->inm_ifp = ifp;
		ninm->inm_ifma = ifma;
		ninm->inm_refcount = 1;
		ifma->ifma_protospec = ninm;
		LIST_INSERT_HEAD(&in_multihead, ninm, inm_link);

		igmp_joingroup(ninm);

		inm = ninm;
	} while (0);

	IN_MULTI_UNLOCK();
	IFF_UNLOCKGIANT(ifp);

	return (inm);
}

/*
 * Leave an IPv4 multicast group.
 * It is OK to call this routine if the underlying ifnet went away.
 *
 * XXX: To deal with the ifp going away, we cheat; the link-layer code in net
 * will set ifma_ifp to NULL when the associated ifnet instance is detached
 * from the system.
 *
 * The only reason we need to violate layers and check ifma_ifp here at all
 * is because certain hardware drivers still require Giant to be held,
 * and it must always be taken before other locks.
 */
void
in_delmulti(struct in_multi *inm)
{
	struct ifnet *ifp;

	KASSERT(inm != NULL, ("%s: inm is NULL", __func__));
	KASSERT(inm->inm_ifma != NULL, ("%s: no ifma", __func__));
	ifp = inm->inm_ifma->ifma_ifp;

	if (ifp != NULL) {
		/*
		 * Sanity check that netinet's notion of ifp is the
		 * same as net's.
		 */
		KASSERT(inm->inm_ifp == ifp, ("%s: bad ifp", __func__));
		IFF_LOCKGIANT(ifp);
	}

	IN_MULTI_LOCK();
	in_delmulti_locked(inm);
	IN_MULTI_UNLOCK();

	if (ifp != NULL)
		IFF_UNLOCKGIANT(ifp);
}

/*
 * Delete a multicast address record, with locks held.
 *
 * It is OK to call this routine if the ifp went away.
 * Assumes that caller holds the IN_MULTI lock, and that
 * Giant was taken before other locks if required by the hardware.
 */
void
in_delmulti_locked(struct in_multi *inm)
{
	struct ifmultiaddr *ifma;

	IN_MULTI_LOCK_ASSERT();
	KASSERT(inm->inm_refcount >= 1, ("%s: freeing freed inm", __func__));

	if (--inm->inm_refcount == 0) {
		igmp_leavegroup(inm);

		ifma = inm->inm_ifma;
#ifdef DIAGNOSTIC
		if (bootverbose)
			printf("%s: purging ifma %p\n", __func__, ifma);
#endif
		KASSERT(ifma->ifma_protospec == inm,
		    ("%s: ifma_protospec != inm", __func__));
		ifma->ifma_protospec = NULL;

		LIST_REMOVE(inm, inm_link);
		free(inm, M_IPMADDR);

		if_delmulti_ifma(ifma);
	}
}

/*
 * Block or unblock an ASM/SSM multicast source on an inpcb.
 */
static int
inp_change_source_filter(struct inpcb *inp, struct sockopt *sopt)
{
	struct group_source_req		 gsr;
	sockunion_t			*gsa, *ssa;
	struct ifnet			*ifp;
	struct in_mfilter		*imf;
	struct ip_moptions		*imo;
	struct in_msource		*ims;
	size_t				 idx;
	int				 error;
	int				 block;

	ifp = NULL;
	error = 0;
	block = 0;

	memset(&gsr, 0, sizeof(struct group_source_req));
	gsa = (sockunion_t *)&gsr.gsr_group;
	ssa = (sockunion_t *)&gsr.gsr_source;

	switch (sopt->sopt_name) {
	case IP_BLOCK_SOURCE:
	case IP_UNBLOCK_SOURCE: {
		struct ip_mreq_source	 mreqs;

		error = sooptcopyin(sopt, &mreqs,
		    sizeof(struct ip_mreq_source),
		    sizeof(struct ip_mreq_source));
		if (error)
			return (error);

		gsa->sin.sin_family = AF_INET;
		gsa->sin.sin_len = sizeof(struct sockaddr_in);
		gsa->sin.sin_addr = mreqs.imr_multiaddr;

		ssa->sin.sin_family = AF_INET;
		ssa->sin.sin_len = sizeof(struct sockaddr_in);
		ssa->sin.sin_addr = mreqs.imr_sourceaddr;

		if (mreqs.imr_interface.s_addr != INADDR_ANY)
			INADDR_TO_IFP(mreqs.imr_interface, ifp);

		if (sopt->sopt_name == IP_BLOCK_SOURCE)
			block = 1;

#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: imr_interface = %s, ifp = %p\n",
			    __func__, inet_ntoa(mreqs.imr_interface), ifp);
		}
#endif
		break;
	    }

	case MCAST_BLOCK_SOURCE:
	case MCAST_UNBLOCK_SOURCE:
		error = sooptcopyin(sopt, &gsr,
		    sizeof(struct group_source_req),
		    sizeof(struct group_source_req));
		if (error)
			return (error);

		if (gsa->sin.sin_family != AF_INET ||
		    gsa->sin.sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);

		if (ssa->sin.sin_family != AF_INET ||
		    ssa->sin.sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);

		if (gsr.gsr_interface == 0 || if_index < gsr.gsr_interface)
			return (EADDRNOTAVAIL);

		ifp = ifnet_byindex(gsr.gsr_interface);

		if (sopt->sopt_name == MCAST_BLOCK_SOURCE)
			block = 1;
		break;

	default:
#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: unknown sopt_name %d\n", __func__,
			    sopt->sopt_name);
		}
#endif
		return (EOPNOTSUPP);
		break;
	}

	/* XXX INET6 */
	if (!IN_MULTICAST(ntohl(gsa->sin.sin_addr.s_addr)))
		return (EINVAL);

	/*
	 * Check if we are actually a member of this group.
	 */
	imo = inp_findmoptions(inp);
	idx = imo_match_group(imo, ifp, &gsa->sa);
	if (idx == -1 || imo->imo_mfilters == NULL) {
		error = EADDRNOTAVAIL;
		goto out_locked;
	}

	KASSERT(imo->imo_mfilters != NULL,
	    ("%s: imo_mfilters not allocated", __func__));
	imf = &imo->imo_mfilters[idx];

	/*
	 * SSM multicast truth table for block/unblock operations.
	 *
	 * Operation   Filter Mode  Entry exists?   Action
	 *
	 * block       exclude      no              add source to filter
	 * unblock     include      no              add source to filter
	 * block       include      no              EINVAL
	 * unblock     exclude      no              EINVAL
	 * block       exclude      yes             EADDRNOTAVAIL
	 * unblock     include      yes             EADDRNOTAVAIL
	 * block       include      yes             remove source from filter
	 * unblock     exclude      yes             remove source from filter
	 *
	 * FreeBSD does not explicitly distinguish between ASM and SSM
	 * mode sockets; all sockets are assumed to have a filter list.
	 */
#ifdef DIAGNOSTIC
	if (bootverbose) {
		printf("%s: imf_fmode is %s\n", __func__,
		    imf->imf_fmode == MCAST_INCLUDE ? "include" : "exclude");
	}
#endif
	ims = imo_match_source(imo, idx, &ssa->sa);
	if (ims == NULL) {
		if ((block == 1 && imf->imf_fmode == MCAST_EXCLUDE) ||
		    (block == 0 && imf->imf_fmode == MCAST_INCLUDE)) {
#ifdef DIAGNOSTIC
			if (bootverbose) {
				printf("%s: adding %s to filter list\n",
				    __func__, inet_ntoa(ssa->sin.sin_addr));
			}
#endif
			error = imo_join_source(imo, idx, ssa);
		}
		if ((block == 1 && imf->imf_fmode == MCAST_INCLUDE) ||
		    (block == 0 && imf->imf_fmode == MCAST_EXCLUDE)) {
			/*
			 * If the socket is in inclusive mode:
			 *  the source is already blocked as it has no entry.
			 * If the socket is in exclusive mode:
			 *  the source is already unblocked as it has no entry.
			 */
#ifdef DIAGNOSTIC
			if (bootverbose) {
				printf("%s: ims %p; %s already [un]blocked\n",
				    __func__, ims,
				    inet_ntoa(ssa->sin.sin_addr));
			}
#endif
			error = EINVAL;
		}
	} else {
		if ((block == 1 && imf->imf_fmode == MCAST_EXCLUDE) ||
		    (block == 0 && imf->imf_fmode == MCAST_INCLUDE)) {
			/*
			 * If the socket is in exclusive mode:
			 *  the source is already blocked as it has an entry.
			 * If the socket is in inclusive mode:
			 *  the source is already unblocked as it has an entry.
			 */
#ifdef DIAGNOSTIC
			if (bootverbose) {
				printf("%s: ims %p; %s already [un]blocked\n",
				    __func__, ims,
				    inet_ntoa(ssa->sin.sin_addr));
			}
#endif
			error = EADDRNOTAVAIL;
		}
		if ((block == 1 && imf->imf_fmode == MCAST_INCLUDE) ||
		    (block == 0 && imf->imf_fmode == MCAST_EXCLUDE)) {
#ifdef DIAGNOSTIC
			if (bootverbose) {
				printf("%s: removing %s from filter list\n",
				    __func__, inet_ntoa(ssa->sin.sin_addr));
			}
#endif
			error = imo_leave_source(imo, idx, ssa);
		}
	}

out_locked:
	INP_UNLOCK(inp);
	return (error);
}

/*
 * Given an inpcb, return its multicast options structure pointer.  Accepts
 * an unlocked inpcb pointer, but will return it locked.  May sleep.
 */
static struct ip_moptions *
inp_findmoptions(struct inpcb *inp)
{
	struct ip_moptions	 *imo;
	struct in_multi		**immp;
	struct in_mfilter	 *imfp;
	size_t			  idx;

	INP_LOCK(inp);
	if (inp->inp_moptions != NULL)
		return (inp->inp_moptions);

	INP_UNLOCK(inp);

	imo = (struct ip_moptions *)malloc(sizeof(*imo), M_IPMOPTS,
	    M_WAITOK);
	immp = (struct in_multi **)malloc(sizeof(*immp) * IP_MIN_MEMBERSHIPS,
	    M_IPMOPTS, M_WAITOK | M_ZERO);
	imfp = (struct in_mfilter *)malloc(
	    sizeof(struct in_mfilter) * IP_MIN_MEMBERSHIPS,
	    M_IPMSOURCE, M_WAITOK);

	imo->imo_multicast_ifp = NULL;
	imo->imo_multicast_addr.s_addr = INADDR_ANY;
	imo->imo_multicast_vif = -1;
	imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
	imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
	imo->imo_num_memberships = 0;
	imo->imo_max_memberships = IP_MIN_MEMBERSHIPS;
	imo->imo_membership = immp;

	/* Initialize per-group source filters. */
	for (idx = 0; idx < IP_MIN_MEMBERSHIPS; idx++) {
		imfp[idx].imf_fmode = MCAST_EXCLUDE;
		imfp[idx].imf_nsources = 0;
		TAILQ_INIT(&imfp[idx].imf_sources);
	}
	imo->imo_mfilters = imfp;

	INP_LOCK(inp);
	if (inp->inp_moptions != NULL) {
		free(imfp, M_IPMSOURCE);
		free(immp, M_IPMOPTS);
		free(imo, M_IPMOPTS);
		return (inp->inp_moptions);
	}
	inp->inp_moptions = imo;
	return (imo);
}

/*
 * Discard the IP multicast options (and source filters).
 */
void
inp_freemoptions(struct ip_moptions *imo)
{
	struct in_mfilter	*imf;
	struct in_msource	*ims, *tims;
	size_t			 idx, nmships;

	KASSERT(imo != NULL, ("%s: ip_moptions is NULL", __func__));

	nmships = imo->imo_num_memberships;
	for (idx = 0; idx < nmships; ++idx) {
		in_delmulti(imo->imo_membership[idx]);

		if (imo->imo_mfilters != NULL) {
			imf = &imo->imo_mfilters[idx];
			TAILQ_FOREACH_SAFE(ims, &imf->imf_sources,
			    ims_next, tims) {
				TAILQ_REMOVE(&imf->imf_sources, ims, ims_next);
				FREE(ims, M_IPMSOURCE);
				imf->imf_nsources--;
			}
			KASSERT(imf->imf_nsources == 0,
			    ("%s: did not free all imf_nsources", __func__));
		}
	}

	if (imo->imo_mfilters != NULL)
		free(imo->imo_mfilters, M_IPMSOURCE);
	free(imo->imo_membership, M_IPMOPTS);
	free(imo, M_IPMOPTS);
}

/*
 * Atomically get source filters on a socket for an IPv4 multicast group.
 * Called with INP lock held; returns with lock released.
 */
static int
inp_get_source_filters(struct inpcb *inp, struct sockopt *sopt)
{
	struct __msfilterreq	 msfr;
	sockunion_t		*gsa;
	struct ifnet		*ifp;
	struct ip_moptions	*imo;
	struct in_mfilter	*imf;
	struct in_msource	*ims;
	struct sockaddr_storage	*ptss;
	struct sockaddr_storage	*tss;
	int			 error;
	size_t			 idx;

	INP_LOCK_ASSERT(inp);

	imo = inp->inp_moptions;
	KASSERT(imo != NULL, ("%s: null ip_moptions", __func__));

	INP_UNLOCK(inp);

	error = sooptcopyin(sopt, &msfr, sizeof(struct __msfilterreq),
	    sizeof(struct __msfilterreq));
	if (error)
		return (error);

	if (msfr.msfr_ifindex == 0 || if_index < msfr.msfr_ifindex)
		return (EINVAL);

	ifp = ifnet_byindex(msfr.msfr_ifindex);
	if (ifp == NULL)
		return (EINVAL);

	INP_LOCK(inp);

	/*
	 * Lookup group on the socket.
	 */
	gsa = (sockunion_t *)&msfr.msfr_group;
	idx = imo_match_group(imo, ifp, &gsa->sa);
	if (idx == -1 || imo->imo_mfilters == NULL) {
		INP_UNLOCK(inp);
		return (EADDRNOTAVAIL);
	}

	imf = &imo->imo_mfilters[idx];
	msfr.msfr_fmode = imf->imf_fmode;
	msfr.msfr_nsrcs = imf->imf_nsources;

	/*
	 * If the user specified a buffer, copy out the source filter
	 * entries to userland gracefully.
	 * msfr.msfr_nsrcs is always set to the total number of filter
	 * entries which the kernel currently has for this group.
	 */
	tss = NULL;
	if (msfr.msfr_srcs != NULL && msfr.msfr_nsrcs > 0) {
		/*
		 * Make a copy of the source vector so that we do not
		 * thrash the inpcb lock whilst copying it out.
		 * We only copy out the number of entries which userland
		 * has asked for, but we always tell userland how big the
		 * buffer really needs to be.
		 */
		MALLOC(tss, struct sockaddr_storage *,
		    sizeof(struct sockaddr_storage) * msfr.msfr_nsrcs,
		    M_TEMP, M_NOWAIT);
		if (tss == NULL) {
			error = ENOBUFS;
		} else {
			ptss = tss;
			TAILQ_FOREACH(ims, &imf->imf_sources, ims_next) {
				memcpy(ptss++, &ims->ims_addr,
				    sizeof(struct sockaddr_storage));
			}
		}
	}

	INP_UNLOCK(inp);

	if (tss != NULL) {
		error = copyout(tss, msfr.msfr_srcs,
		    sizeof(struct sockaddr_storage) * msfr.msfr_nsrcs);
		FREE(tss, M_TEMP);
	}

	if (error)
		return (error);

	error = sooptcopyout(sopt, &msfr, sizeof(struct __msfilterreq));

	return (error);
}

/*
 * Return the IP multicast options in response to user getsockopt().
 */
int
inp_getmoptions(struct inpcb *inp, struct sockopt *sopt)
{
	struct ip_mreqn		 mreqn;
	struct ip_moptions	*imo;
	struct ifnet		*ifp;
	struct in_ifaddr	*ia;
	int			 error, optval;
	u_char			 coptval;

	INP_LOCK(inp);
	imo = inp->inp_moptions;

	error = 0;
	switch (sopt->sopt_name) {
	case IP_MULTICAST_VIF:
		if (imo != NULL)
			optval = imo->imo_multicast_vif;
		else
			optval = -1;
		INP_UNLOCK(inp);
		error = sooptcopyout(sopt, &optval, sizeof(int));
		break;

	case IP_MULTICAST_IF:
		memset(&mreqn, 0, sizeof(struct ip_mreqn));
		if (imo != NULL) {
			ifp = imo->imo_multicast_ifp;
			if (imo->imo_multicast_addr.s_addr != INADDR_ANY) {
				mreqn.imr_address = imo->imo_multicast_addr;
			} else if (ifp != NULL) {
				mreqn.imr_ifindex = ifp->if_index;
				IFP_TO_IA(ifp, ia);
				if (ia != NULL) {
					mreqn.imr_address =
					    IA_SIN(ia)->sin_addr;
				}
			}
		}
		INP_UNLOCK(inp);
		if (sopt->sopt_valsize == sizeof(struct ip_mreqn)) {
			error = sooptcopyout(sopt, &mreqn,
			    sizeof(struct ip_mreqn));
		} else {
			error = sooptcopyout(sopt, &mreqn.imr_address,
			    sizeof(struct in_addr));
		}
		break;

	case IP_MULTICAST_TTL:
		if (imo == 0)
			optval = coptval = IP_DEFAULT_MULTICAST_TTL;
		else
			optval = coptval = imo->imo_multicast_ttl;
		INP_UNLOCK(inp);
		if (sopt->sopt_valsize == sizeof(u_char))
			error = sooptcopyout(sopt, &coptval, sizeof(u_char));
		else
			error = sooptcopyout(sopt, &optval, sizeof(int));
		break;

	case IP_MULTICAST_LOOP:
		if (imo == 0)
			optval = coptval = IP_DEFAULT_MULTICAST_LOOP;
		else
			optval = coptval = imo->imo_multicast_loop;
		INP_UNLOCK(inp);
		if (sopt->sopt_valsize == sizeof(u_char))
			error = sooptcopyout(sopt, &coptval, sizeof(u_char));
		else
			error = sooptcopyout(sopt, &optval, sizeof(int));
		break;

	case IP_MSFILTER:
		if (imo == NULL) {
			error = EADDRNOTAVAIL;
			INP_UNLOCK(inp);
		} else {
			error = inp_get_source_filters(inp, sopt);
		}
		break;

	default:
		INP_UNLOCK(inp);
		error = ENOPROTOOPT;
		break;
	}

	INP_UNLOCK_ASSERT(inp);

	return (error);
}

/*
 * Join an IPv4 multicast group, possibly with a source.
 */
static int
inp_join_group(struct inpcb *inp, struct sockopt *sopt)
{
	struct group_source_req		 gsr;
	sockunion_t			*gsa, *ssa;
	struct ifnet			*ifp;
	struct in_mfilter		*imf;
	struct ip_moptions		*imo;
	struct in_multi			*inm;
	size_t				 idx;
	int				 error;

	ifp = NULL;
	error = 0;

	memset(&gsr, 0, sizeof(struct group_source_req));
	gsa = (sockunion_t *)&gsr.gsr_group;
	gsa->ss.ss_family = AF_UNSPEC;
	ssa = (sockunion_t *)&gsr.gsr_source;
	ssa->ss.ss_family = AF_UNSPEC;

	switch (sopt->sopt_name) {
	case IP_ADD_MEMBERSHIP:
	case IP_ADD_SOURCE_MEMBERSHIP: {
		struct ip_mreq_source	 mreqs;

		if (sopt->sopt_name == IP_ADD_MEMBERSHIP) {
			error = sooptcopyin(sopt, &mreqs,
			    sizeof(struct ip_mreq),
			    sizeof(struct ip_mreq));
			/*
			 * Do argument switcharoo from ip_mreq into
			 * ip_mreq_source to avoid using two instances.
			 */
			mreqs.imr_interface = mreqs.imr_sourceaddr;
			mreqs.imr_sourceaddr.s_addr = INADDR_ANY;
		} else if (sopt->sopt_name == IP_ADD_SOURCE_MEMBERSHIP) {
			error = sooptcopyin(sopt, &mreqs,
			    sizeof(struct ip_mreq_source),
			    sizeof(struct ip_mreq_source));
		}
		if (error)
			return (error);

		gsa->sin.sin_family = AF_INET;
		gsa->sin.sin_len = sizeof(struct sockaddr_in);
		gsa->sin.sin_addr = mreqs.imr_multiaddr;

		if (sopt->sopt_name == IP_ADD_SOURCE_MEMBERSHIP) {
			ssa->sin.sin_family = AF_INET;
			ssa->sin.sin_len = sizeof(struct sockaddr_in);
			ssa->sin.sin_addr = mreqs.imr_sourceaddr;
		}

		/*
		 * Obtain ifp. If no interface address was provided,
		 * use the interface of the route in the unicast FIB for
		 * the given multicast destination; usually, this is the
		 * default route.
		 * If this lookup fails, attempt to use the first non-loopback
		 * interface with multicast capability in the system as a
		 * last resort. The legacy IPv4 ASM API requires that we do
		 * this in order to allow groups to be joined when the routing
		 * table has not yet been populated during boot.
		 * If all of these conditions fail, return EADDRNOTAVAIL, and
		 * reject the IPv4 multicast join.
		 */
		if (mreqs.imr_interface.s_addr != INADDR_ANY) {
			INADDR_TO_IFP(mreqs.imr_interface, ifp);
		} else {
			struct route ro;

			ro.ro_rt = NULL;
			*(struct sockaddr_in *)&ro.ro_dst = gsa->sin;
			rtalloc_ign(&ro, RTF_CLONING);
			if (ro.ro_rt != NULL) {
				ifp = ro.ro_rt->rt_ifp;
				KASSERT(ifp != NULL, ("%s: null ifp",
				    __func__));
				RTFREE(ro.ro_rt);
			} else {
				struct in_ifaddr *ia;
				struct ifnet *mfp = NULL;
				TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
					mfp = ia->ia_ifp;
					if (!(mfp->if_flags & IFF_LOOPBACK) &&
					     (mfp->if_flags & IFF_MULTICAST)) {
						ifp = mfp;
						break;
					}
				}
			}
		}
#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: imr_interface = %s, ifp = %p\n",
			    __func__, inet_ntoa(mreqs.imr_interface), ifp);
		}
#endif
		break;
	}

	case MCAST_JOIN_GROUP:
	case MCAST_JOIN_SOURCE_GROUP:
		if (sopt->sopt_name == MCAST_JOIN_GROUP) {
			error = sooptcopyin(sopt, &gsr,
			    sizeof(struct group_req),
			    sizeof(struct group_req));
		} else if (sopt->sopt_name == MCAST_JOIN_SOURCE_GROUP) {
			error = sooptcopyin(sopt, &gsr,
			    sizeof(struct group_source_req),
			    sizeof(struct group_source_req));
		}
		if (error)
			return (error);

		if (gsa->sin.sin_family != AF_INET ||
		    gsa->sin.sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);

		/*
		 * Overwrite the port field if present, as the sockaddr
		 * being copied in may be matched with a binary comparison.
		 * XXX INET6
		 */
		gsa->sin.sin_port = 0;
		if (sopt->sopt_name == MCAST_JOIN_SOURCE_GROUP) {
			if (ssa->sin.sin_family != AF_INET ||
			    ssa->sin.sin_len != sizeof(struct sockaddr_in))
				return (EINVAL);
			ssa->sin.sin_port = 0;
		}

		/*
		 * Obtain the ifp.
		 */
		if (gsr.gsr_interface == 0 || if_index < gsr.gsr_interface)
			return (EADDRNOTAVAIL);
		ifp = ifnet_byindex(gsr.gsr_interface);

		break;

	default:
#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: unknown sopt_name %d\n", __func__,
			    sopt->sopt_name);
		}
#endif
		return (EOPNOTSUPP);
		break;
	}

	if (!IN_MULTICAST(ntohl(gsa->sin.sin_addr.s_addr)))
		return (EINVAL);

	if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0)
		return (EADDRNOTAVAIL);

	/*
	 * Check if we already hold membership of this group for this inpcb.
	 * If so, we do not need to perform the initial join.
	 */
	imo = inp_findmoptions(inp);
	idx = imo_match_group(imo, ifp, &gsa->sa);
	if (idx != -1) {
		if (ssa->ss.ss_family != AF_UNSPEC) {
			/*
			 * Attempting to join an ASM group (when already
			 * an ASM or SSM member) is an error.
			 */
			error = EADDRNOTAVAIL;
		} else {
			imf = &imo->imo_mfilters[idx];
			if (imf->imf_nsources == 0) {
				/*
				 * Attempting to join an SSM group (when
				 * already an ASM member) is an error.
				 */
				error = EINVAL;
			} else {
				/*
				 * Attempting to join an SSM group (when
				 * already an SSM member) means "add this
				 * source to the inclusive filter list".
				 */
				error = imo_join_source(imo, idx, ssa);
			}
		}
		goto out_locked;
	}

	/*
	 * Call imo_grow() to reallocate the membership and source filter
	 * vectors if they are full. If the size would exceed the hard limit,
	 * then we know we've really run out of entries. We keep the INP
	 * lock held to avoid introducing a race condition.
	 */
	if (imo->imo_num_memberships == imo->imo_max_memberships) {
		error = imo_grow(imo);
		if (error)
			goto out_locked;
	}

	/*
	 * So far, so good: perform the layer 3 join, layer 2 join,
	 * and make an IGMP announcement if needed.
	 */
	inm = in_addmulti(&gsa->sin.sin_addr, ifp);
	if (inm == NULL) {
		error = ENOBUFS;
		goto out_locked;
	}
	idx = imo->imo_num_memberships;
	imo->imo_membership[idx] = inm;
	imo->imo_num_memberships++;

	KASSERT(imo->imo_mfilters != NULL,
	    ("%s: imf_mfilters vector was not allocated", __func__));
	imf = &imo->imo_mfilters[idx];
	KASSERT(TAILQ_EMPTY(&imf->imf_sources),
	    ("%s: imf_sources not empty", __func__));

	/*
	 * If this is a new SSM group join (i.e. a source was specified
	 * with this group), add this source to the filter list.
	 */
	if (ssa->ss.ss_family != AF_UNSPEC) {
		/*
		 * An initial SSM join implies that this socket's membership
		 * of the multicast group is now in inclusive mode.
		 */
		imf->imf_fmode = MCAST_INCLUDE;

		error = imo_join_source(imo, idx, ssa);
		if (error) {
			/*
			 * Drop inp lock before calling in_delmulti(),
			 * to prevent a lock order reversal.
			 */
			--imo->imo_num_memberships;
			INP_UNLOCK(inp);
			in_delmulti(inm);
			return (error);
		}
	}

out_locked:
	INP_UNLOCK(inp);
	return (error);
}

/*
 * Leave an IPv4 multicast group on an inpcb, possibly with a source.
 */
static int
inp_leave_group(struct inpcb *inp, struct sockopt *sopt)
{
	struct group_source_req		 gsr;
	struct ip_mreq_source		 mreqs;
	sockunion_t			*gsa, *ssa;
	struct ifnet			*ifp;
	struct in_mfilter		*imf;
	struct ip_moptions		*imo;
	struct in_msource		*ims, *tims;
	struct in_multi			*inm;
	size_t				 idx;
	int				 error;

	ifp = NULL;
	error = 0;

	memset(&gsr, 0, sizeof(struct group_source_req));
	gsa = (sockunion_t *)&gsr.gsr_group;
	gsa->ss.ss_family = AF_UNSPEC;
	ssa = (sockunion_t *)&gsr.gsr_source;
	ssa->ss.ss_family = AF_UNSPEC;

	switch (sopt->sopt_name) {
	case IP_DROP_MEMBERSHIP:
	case IP_DROP_SOURCE_MEMBERSHIP:
		if (sopt->sopt_name == IP_DROP_MEMBERSHIP) {
			error = sooptcopyin(sopt, &mreqs,
			    sizeof(struct ip_mreq),
			    sizeof(struct ip_mreq));
			/*
			 * Swap interface and sourceaddr arguments,
			 * as ip_mreq and ip_mreq_source are laid
			 * out differently.
			 */
			mreqs.imr_interface = mreqs.imr_sourceaddr;
			mreqs.imr_sourceaddr.s_addr = INADDR_ANY;
		} else if (sopt->sopt_name == IP_DROP_SOURCE_MEMBERSHIP) {
			error = sooptcopyin(sopt, &mreqs,
			    sizeof(struct ip_mreq_source),
			    sizeof(struct ip_mreq_source));
		}
		if (error)
			return (error);

		gsa->sin.sin_family = AF_INET;
		gsa->sin.sin_len = sizeof(struct sockaddr_in);
		gsa->sin.sin_addr = mreqs.imr_multiaddr;

		if (sopt->sopt_name == IP_DROP_SOURCE_MEMBERSHIP) {
			ssa->sin.sin_family = AF_INET;
			ssa->sin.sin_len = sizeof(struct sockaddr_in);
			ssa->sin.sin_addr = mreqs.imr_sourceaddr;
		}

		if (gsa->sin.sin_addr.s_addr != INADDR_ANY)
			INADDR_TO_IFP(mreqs.imr_interface, ifp);

#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: imr_interface = %s, ifp = %p\n",
			    __func__, inet_ntoa(mreqs.imr_interface), ifp);
		}
#endif
		break;

	case MCAST_LEAVE_GROUP:
	case MCAST_LEAVE_SOURCE_GROUP:
		if (sopt->sopt_name == MCAST_LEAVE_GROUP) {
			error = sooptcopyin(sopt, &gsr,
			    sizeof(struct group_req),
			    sizeof(struct group_req));
		} else if (sopt->sopt_name == MCAST_LEAVE_SOURCE_GROUP) {
			error = sooptcopyin(sopt, &gsr,
			    sizeof(struct group_source_req),
			    sizeof(struct group_source_req));
		}
		if (error)
			return (error);

		if (gsa->sin.sin_family != AF_INET ||
		    gsa->sin.sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);

		if (sopt->sopt_name == MCAST_LEAVE_SOURCE_GROUP) {
			if (ssa->sin.sin_family != AF_INET ||
			    ssa->sin.sin_len != sizeof(struct sockaddr_in))
				return (EINVAL);
		}

		if (gsr.gsr_interface == 0 || if_index < gsr.gsr_interface)
			return (EADDRNOTAVAIL);

		ifp = ifnet_byindex(gsr.gsr_interface);
		break;

	default:
#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: unknown sopt_name %d\n", __func__,
			    sopt->sopt_name);
		}
#endif
		return (EOPNOTSUPP);
		break;
	}

	if (!IN_MULTICAST(ntohl(gsa->sin.sin_addr.s_addr)))
		return (EINVAL);

	/*
	 * Find the membership in the membership array.
	 */
	imo = inp_findmoptions(inp);
	idx = imo_match_group(imo, ifp, &gsa->sa);
	if (idx == -1) {
		error = EADDRNOTAVAIL;
		goto out_locked;
	}
	imf = &imo->imo_mfilters[idx];

	/*
	 * If we were instructed only to leave a given source, do so.
	 */
	if (ssa->ss.ss_family != AF_UNSPEC) {
		if (imf->imf_nsources == 0 ||
		    imf->imf_fmode == MCAST_EXCLUDE) {
			/*
			 * Attempting to SSM leave an ASM group
			 * is an error; should use *_BLOCK_SOURCE instead.
			 * Attempting to SSM leave a source in a group when
			 * the socket is in 'exclude mode' is also an error.
			 */
			error = EINVAL;
		} else {
			error = imo_leave_source(imo, idx, ssa);
		}
		/*
		 * If an error occurred, or this source is not the last
		 * source in the group, do not leave the whole group.
		 */
		if (error || imf->imf_nsources > 0)
			goto out_locked;
	}

	/*
	 * Give up the multicast address record to which the membership points.
	 */
	inm = imo->imo_membership[idx];
	in_delmulti(inm);

	/*
	 * Free any source filters for this group if they exist.
	 * Revert inpcb to the default MCAST_EXCLUDE state.
	 */
	if (imo->imo_mfilters != NULL) {
		TAILQ_FOREACH_SAFE(ims, &imf->imf_sources, ims_next, tims) {
			TAILQ_REMOVE(&imf->imf_sources, ims, ims_next);
			FREE(ims, M_IPMSOURCE);
			imf->imf_nsources--;
		}
		KASSERT(imf->imf_nsources == 0,
		    ("%s: imf_nsources not 0", __func__));
		KASSERT(TAILQ_EMPTY(&imf->imf_sources),
		    ("%s: imf_sources not empty", __func__));
		imf->imf_fmode = MCAST_EXCLUDE;
	}

	/*
	 * Remove the gap in the membership array.
	 */
	for (++idx; idx < imo->imo_num_memberships; ++idx)
		imo->imo_membership[idx-1] = imo->imo_membership[idx];
	imo->imo_num_memberships--;

out_locked:
	INP_UNLOCK(inp);
	return (error);
}

/*
 * Select the interface for transmitting IPv4 multicast datagrams.
 *
 * Either an instance of struct in_addr or an instance of struct ip_mreqn
 * may be passed to this socket option. An address of INADDR_ANY or an
 * interface index of 0 is used to remove a previous selection.
 * When no interface is selected, one is chosen for every send.
 */
static int
inp_set_multicast_if(struct inpcb *inp, struct sockopt *sopt)
{
	struct in_addr		 addr;
	struct ip_mreqn		 mreqn;
	struct ifnet		*ifp;
	struct ip_moptions	*imo;
	int			 error;

	if (sopt->sopt_valsize == sizeof(struct ip_mreqn)) {
		/*
		 * An interface index was specified using the
		 * Linux-derived ip_mreqn structure.
		 */
		error = sooptcopyin(sopt, &mreqn, sizeof(struct ip_mreqn),
		    sizeof(struct ip_mreqn));
		if (error)
			return (error);

		if (mreqn.imr_ifindex < 0 || if_index < mreqn.imr_ifindex)
			return (EINVAL);

		if (mreqn.imr_ifindex == 0) {
			ifp = NULL;
		} else {
			ifp = ifnet_byindex(mreqn.imr_ifindex);
			if (ifp == NULL)
				return (EADDRNOTAVAIL);
		}
	} else {
		/*
		 * An interface was specified by IPv4 address.
		 * This is the traditional BSD usage.
		 */
		error = sooptcopyin(sopt, &addr, sizeof(struct in_addr),
		    sizeof(struct in_addr));
		if (error)
			return (error);
		if (addr.s_addr == INADDR_ANY) {
			ifp = NULL;
		} else {
			INADDR_TO_IFP(addr, ifp);
			if (ifp == NULL)
				return (EADDRNOTAVAIL);
		}
#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: ifp = %p, addr = %s\n",
			    __func__, ifp, inet_ntoa(addr)); /* XXX INET6 */
		}
#endif
	}

	/* Reject interfaces which do not support multicast. */
	if (ifp != NULL && (ifp->if_flags & IFF_MULTICAST) == 0)
		return (EOPNOTSUPP);

	imo = inp_findmoptions(inp);
	imo->imo_multicast_ifp = ifp;
	imo->imo_multicast_addr.s_addr = INADDR_ANY;
	INP_UNLOCK(inp);

	return (0);
}

/*
 * Atomically set source filters on a socket for an IPv4 multicast group.
 */
static int
inp_set_source_filters(struct inpcb *inp, struct sockopt *sopt)
{
	struct __msfilterreq	 msfr;
	sockunion_t		*gsa;
	struct ifnet		*ifp;
	struct in_mfilter	*imf;
	struct ip_moptions	*imo;
	struct in_msource	*ims, *tims;
	size_t			 idx;
	int			 error;

	error = sooptcopyin(sopt, &msfr, sizeof(struct __msfilterreq),
	    sizeof(struct __msfilterreq));
	if (error)
		return (error);

	if (msfr.msfr_nsrcs > IP_MAX_SOURCE_FILTER ||
	    (msfr.msfr_fmode != MCAST_EXCLUDE &&
	     msfr.msfr_fmode != MCAST_INCLUDE))
		return (EINVAL);

	if (msfr.msfr_group.ss_family != AF_INET ||
	    msfr.msfr_group.ss_len != sizeof(struct sockaddr_in))
		return (EINVAL);

	gsa = (sockunion_t *)&msfr.msfr_group;
	if (!IN_MULTICAST(ntohl(gsa->sin.sin_addr.s_addr)))
		return (EINVAL);

	gsa->sin.sin_port = 0;	/* ignore port */

	if (msfr.msfr_ifindex == 0 || if_index < msfr.msfr_ifindex)
		return (EADDRNOTAVAIL);

	ifp = ifnet_byindex(msfr.msfr_ifindex);
	if (ifp == NULL)
		return (EADDRNOTAVAIL);

	/*
	 * Take the INP lock.
	 * Check if this socket is a member of this group.
	 */
	imo = inp_findmoptions(inp);
	idx = imo_match_group(imo, ifp, &gsa->sa);
	if (idx == -1 || imo->imo_mfilters == NULL) {
		error = EADDRNOTAVAIL;
		goto out_locked;
	}
	imf = &imo->imo_mfilters[idx];

#ifdef DIAGNOSTIC
	if (bootverbose)
		printf("%s: clearing source list\n", __func__);
#endif

	/*
	 * Remove any existing source filters.
	 */
	TAILQ_FOREACH_SAFE(ims, &imf->imf_sources, ims_next, tims) {
		TAILQ_REMOVE(&imf->imf_sources, ims, ims_next);
		FREE(ims, M_IPMSOURCE);
		imf->imf_nsources--;
	}
	KASSERT(imf->imf_nsources == 0,
	    ("%s: source list not cleared", __func__));

	/*
	 * Apply any new source filters, if present.
	 */
	if (msfr.msfr_nsrcs > 0) {
		struct in_msource	**pnims;
		struct in_msource	*nims;
		struct sockaddr_storage	*kss;
		struct sockaddr_storage	*pkss;
		sockunion_t		*psu;
		int			 i, j;

		/*
		 * Drop the inp lock so we may sleep if we need to
		 * in order to satisfy a malloc request.
		 * We will re-take it before changing socket state.
		 */
		INP_UNLOCK(inp);
#ifdef DIAGNOSTIC
		if (bootverbose) {
			printf("%s: loading %lu source list entries\n",
			    __func__, (unsigned long)msfr.msfr_nsrcs);
		}
#endif
		/*
		 * Make a copy of the user-space source vector so
		 * that we may copy them with a single copyin. This
		 * allows us to deal with page faults up-front.
		 */
		MALLOC(kss, struct sockaddr_storage *,
		    sizeof(struct sockaddr_storage) * msfr.msfr_nsrcs,
		    M_TEMP, M_WAITOK);
		error = copyin(msfr.msfr_srcs, kss,
		    sizeof(struct sockaddr_storage) * msfr.msfr_nsrcs);
		if (error) {
			FREE(kss, M_TEMP);
			return (error);
		}

		/*
		 * Perform argument checking on every sockaddr_storage
		 * structure in the vector provided to us. Overwrite
		 * fields which should not apply to source entries.
		 * TODO: Check for duplicate sources on this pass.
		 */
		psu = (sockunion_t *)kss;
		for (i = 0; i < msfr.msfr_nsrcs; i++, psu++) {
			switch (psu->ss.ss_family) {
			case AF_INET:
				if (psu->sin.sin_len !=
				    sizeof(struct sockaddr_in)) {
					error = EINVAL;
				} else {
					psu->sin.sin_port = 0;
				}
				break;
#ifdef notyet
			case AF_INET6;
				if (psu->sin6.sin6_len !=
				    sizeof(struct sockaddr_in6)) {
					error = EINVAL;
				} else {
					psu->sin6.sin6_port = 0;
					psu->sin6.sin6_flowinfo = 0;
				}
				break;
#endif
			default:
				error = EAFNOSUPPORT;
				break;
			}
			if (error)
				break;
		}
		if (error) {
			FREE(kss, M_TEMP);
			return (error);
		}

		/*
		 * Allocate a block to track all the in_msource
		 * entries we are about to allocate, in case we
		 * abruptly need to free them.
		 */
		MALLOC(pnims, struct in_msource **,
		    sizeof(struct in_msource *) * msfr.msfr_nsrcs,
		    M_TEMP, M_WAITOK | M_ZERO);

		/*
		 * Allocate up to nsrcs individual chunks.
		 * If we encounter an error, backtrack out of
		 * all allocations cleanly; updates must be atomic.
		 */
		pkss = kss;
		nims = NULL;
		for (i = 0; i < msfr.msfr_nsrcs; i++, pkss++) {
			MALLOC(nims, struct in_msource *,
			    sizeof(struct in_msource) * msfr.msfr_nsrcs,
			    M_IPMSOURCE, M_WAITOK | M_ZERO);
			pnims[i] = nims;
		}
		if (i < msfr.msfr_nsrcs) {
			for (j = 0; j < i; j++) {
				if (pnims[j] != NULL)
					FREE(pnims[j], M_IPMSOURCE);
			}
			FREE(pnims, M_TEMP);
			FREE(kss, M_TEMP);
			return (ENOBUFS);
		}

		INP_UNLOCK_ASSERT(inp);

		/*
		 * Finally, apply the filters to the socket.
		 * Re-take the inp lock; we are changing socket state.
		 */
		pkss = kss;
		INP_LOCK(inp);
		for (i = 0; i < msfr.msfr_nsrcs; i++, pkss++) {
			memcpy(&(pnims[i]->ims_addr), pkss,
			    sizeof(struct sockaddr_storage));
			TAILQ_INSERT_TAIL(&imf->imf_sources, pnims[i],
			    ims_next);
			imf->imf_nsources++;
		}
		FREE(pnims, M_TEMP);
		FREE(kss, M_TEMP);
	}

	/*
	 * Update the filter mode on the socket before releasing the inpcb.
	 */
	INP_LOCK_ASSERT(inp);
	imf->imf_fmode = msfr.msfr_fmode;

out_locked:
	INP_UNLOCK(inp);
	return (error);
}

/*
 * Set the IP multicast options in response to user setsockopt().
 *
 * Many of the socket options handled in this function duplicate the
 * functionality of socket options in the regular unicast API. However,
 * it is not possible to merge the duplicate code, because the idempotence
 * of the IPv4 multicast part of the BSD Sockets API must be preserved;
 * the effects of these options must be treated as separate and distinct.
 */
int
inp_setmoptions(struct inpcb *inp, struct sockopt *sopt)
{
	struct ip_moptions	*imo;
	int			 error;

	error = 0;

	switch (sopt->sopt_name) {
	case IP_MULTICAST_VIF: {
		int vifi;
		/*
		 * Select a multicast VIF for transmission.
		 * Only useful if multicast forwarding is active.
		 */
		if (legal_vif_num == NULL) {
			error = EOPNOTSUPP;
			break;
		}
		error = sooptcopyin(sopt, &vifi, sizeof(int), sizeof(int));
		if (error)
			break;
		if (!legal_vif_num(vifi) && (vifi != -1)) {
			error = EINVAL;
			break;
		}
		imo = inp_findmoptions(inp);
		imo->imo_multicast_vif = vifi;
		INP_UNLOCK(inp);
		break;
	}

	case IP_MULTICAST_IF:
		error = inp_set_multicast_if(inp, sopt);
		break;

	case IP_MULTICAST_TTL: {
		u_char ttl;

		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 * The original multicast API required a char argument,
		 * which is inconsistent with the rest of the socket API.
		 * We allow either a char or an int.
		 */
		if (sopt->sopt_valsize == sizeof(u_char)) {
			error = sooptcopyin(sopt, &ttl, sizeof(u_char),
			    sizeof(u_char));
			if (error)
				break;
		} else {
			u_int ittl;

			error = sooptcopyin(sopt, &ittl, sizeof(u_int),
			    sizeof(u_int));
			if (error)
				break;
			if (ittl > 255) {
				error = EINVAL;
				break;
			}
			ttl = (u_char)ittl;
		}
		imo = inp_findmoptions(inp);
		imo->imo_multicast_ttl = ttl;
		INP_UNLOCK(inp);
		break;
	}

	case IP_MULTICAST_LOOP: {
		u_char loop;

		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.  The original multicast API required a
		 * char argument, which is inconsistent with the rest
		 * of the socket API.  We allow either a char or an int.
		 */
		if (sopt->sopt_valsize == sizeof(u_char)) {
			error = sooptcopyin(sopt, &loop, sizeof(u_char),
			    sizeof(u_char));
			if (error)
				break;
		} else {
			u_int iloop;

			error = sooptcopyin(sopt, &iloop, sizeof(u_int),
					    sizeof(u_int));
			if (error)
				break;
			loop = (u_char)iloop;
		}
		imo = inp_findmoptions(inp);
		imo->imo_multicast_loop = !!loop;
		INP_UNLOCK(inp);
		break;
	}

	case IP_ADD_MEMBERSHIP:
	case IP_ADD_SOURCE_MEMBERSHIP:
	case MCAST_JOIN_GROUP:
	case MCAST_JOIN_SOURCE_GROUP:
		error = inp_join_group(inp, sopt);
		break;

	case IP_DROP_MEMBERSHIP:
	case IP_DROP_SOURCE_MEMBERSHIP:
	case MCAST_LEAVE_GROUP:
	case MCAST_LEAVE_SOURCE_GROUP:
		error = inp_leave_group(inp, sopt);
		break;

	case IP_BLOCK_SOURCE:
	case IP_UNBLOCK_SOURCE:
	case MCAST_BLOCK_SOURCE:
	case MCAST_UNBLOCK_SOURCE:
		error = inp_change_source_filter(inp, sopt);
		break;

	case IP_MSFILTER:
		error = inp_set_source_filters(inp, sopt);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	INP_UNLOCK_ASSERT(inp);

	return (error);
}
