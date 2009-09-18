/*
 * Copyright (c) 2004 Luigi Rizzo, Alessandro Cerri. All rights reserved.
 * Copyright (c) 2004-2008 Qing Li. All rights reserved.
 * Copyright (c) 2008 Kip Macy. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>

#include <vm/uma.h>

#include <netinet/in.h>
#include <net/if_llatbl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>
#include <netinet/if_ether.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

MALLOC_DEFINE(M_LLTABLE, "lltable", "link level address tables");

static VNET_DEFINE(SLIST_HEAD(, lltable), lltables);
#define	V_lltables	VNET(lltables)

extern void arprequest(struct ifnet *, struct in_addr *, struct in_addr *,
	u_char *);

static void vnet_lltable_init(void);

struct rwlock lltable_rwlock;
RW_SYSINIT(lltable_rwlock, &lltable_rwlock, "lltable_rwlock");

/*
 * Dump arp state for a specific address family.
 */
int
lltable_sysctl_dumparp(int af, struct sysctl_req *wr)
{
	struct lltable *llt;
	int error = 0;

	LLTABLE_RLOCK();
	SLIST_FOREACH(llt, &V_lltables, llt_link) {
		if (llt->llt_af == af) {
			error = llt->llt_dump(llt, wr);
			if (error != 0)
				goto done;
		}
	}
done:
	LLTABLE_RUNLOCK();
	return (error);
}

/*
 * Deletes an address from the address table.
 * This function is called by the timer functions
 * such as arptimer() and nd6_llinfo_timer(), and
 * the caller does the locking.
 */
void
llentry_free(struct llentry *lle)
{
	
	LLE_WLOCK_ASSERT(lle);
	LIST_REMOVE(lle, lle_next);

	if (lle->la_hold != NULL)
		m_freem(lle->la_hold);

	LLE_FREE_LOCKED(lle);
}

/*
 * Update an llentry for address dst (equivalent to rtalloc for new-arp)
 * Caller must pass in a valid struct llentry *
 *
 * if found the llentry * is returned referenced and unlocked
 */
int
llentry_update(struct llentry **llep, struct lltable *lt,
    struct sockaddr *dst, struct ifnet *ifp)
{
	struct llentry *la;

	IF_AFDATA_RLOCK(ifp);	
	la = lla_lookup(lt, LLE_EXCLUSIVE,
	    (struct sockaddr *)dst);
	IF_AFDATA_RUNLOCK(ifp);
	if ((la == NULL) && 
	    (ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) == 0) {
		IF_AFDATA_WLOCK(ifp);
		la = lla_lookup(lt,
		    (LLE_CREATE | LLE_EXCLUSIVE),
		    (struct sockaddr *)dst);
		IF_AFDATA_WUNLOCK(ifp);	
	}
	if (la != NULL && (*llep != la)) {
		if (*llep != NULL)
			LLE_FREE(*llep);
		LLE_ADDREF(la);
		LLE_WUNLOCK(la);
		*llep = la;
	} else if (la != NULL)
		LLE_WUNLOCK(la);

	if (la == NULL)
		return (ENOENT);

	return (0);
}

/*
 * Free all entries from given table and free itself.
 */
void
lltable_free(struct lltable *llt)
{
	struct llentry *lle, *next;
	int i;

	KASSERT(llt != NULL, ("%s: llt is NULL", __func__));

	LLTABLE_WLOCK();
	SLIST_REMOVE(&V_lltables, llt, lltable, llt_link);
	LLTABLE_WUNLOCK();

	for (i=0; i < LLTBL_HASHTBL_SIZE; i++) {
		LIST_FOREACH_SAFE(lle, &llt->lle_head[i], lle_next, next) {

			callout_drain(&lle->la_timer);
			LLE_WLOCK(lle);
			llentry_free(lle);
		}
	}

	free(llt, M_LLTABLE);
}

void
lltable_drain(int af)
{
	struct lltable	*llt;
	struct llentry	*lle;
	register int i;

	LLTABLE_RLOCK();
	SLIST_FOREACH(llt, &V_lltables, llt_link) {
		if (llt->llt_af != af)
			continue;

		for (i=0; i < LLTBL_HASHTBL_SIZE; i++) {
			LIST_FOREACH(lle, &llt->lle_head[i], lle_next) {
				if (lle->la_hold) {
					m_freem(lle->la_hold);
					lle->la_hold = NULL;
				}
			}
		}
	}
	LLTABLE_RUNLOCK();
}

void
lltable_prefix_free(int af, struct sockaddr *prefix, struct sockaddr *mask)
{
	struct lltable *llt;

	LLTABLE_RLOCK();
	SLIST_FOREACH(llt, &V_lltables, llt_link) {
		if (llt->llt_af != af)
			continue;

		llt->llt_prefix_free(llt, prefix, mask);
	}
	LLTABLE_RUNLOCK();
}



/*
 * Create a new lltable.
 */
struct lltable *
lltable_init(struct ifnet *ifp, int af)
{
	struct lltable *llt;
	register int i;

	llt = malloc(sizeof(struct lltable), M_LLTABLE, M_WAITOK);
	if (llt == NULL)
		return (NULL);

	llt->llt_af = af;
	llt->llt_ifp = ifp;
	for (i = 0; i < LLTBL_HASHTBL_SIZE; i++)
		LIST_INIT(&llt->lle_head[i]);

	LLTABLE_WLOCK();
	SLIST_INSERT_HEAD(&V_lltables, llt, llt_link);
	LLTABLE_WUNLOCK();

	return (llt);
}

/*
 * Called in route_output when adding/deleting a route to an interface.
 */
int
lla_rt_output(struct rt_msghdr *rtm, struct rt_addrinfo *info)
{
	struct sockaddr_dl *dl =
	    (struct sockaddr_dl *)info->rti_info[RTAX_GATEWAY];
	struct sockaddr *dst = (struct sockaddr *)info->rti_info[RTAX_DST];
	struct ifnet *ifp;
	struct lltable *llt;
	struct llentry *lle;
	u_int laflags = 0, flags = 0;
	int error = 0;

	if (dl == NULL || dl->sdl_family != AF_LINK) {
		log(LOG_INFO, "%s: invalid dl\n", __func__);
		return EINVAL;
	}
	ifp = ifnet_byindex(dl->sdl_index);
	if (ifp == NULL) {
		log(LOG_INFO, "%s: invalid ifp (sdl_index %d)\n",
		    __func__, dl->sdl_index);
		return EINVAL;
	}

	switch (rtm->rtm_type) {
	case RTM_ADD:
		if (rtm->rtm_flags & RTF_ANNOUNCE) {
			flags |= LLE_PUB;
#ifdef INET
			if (dst->sa_family == AF_INET && 
			    ((struct sockaddr_inarp *)dst)->sin_other != 0) {
				struct rtentry *rt = rtalloc1(dst, 0, 0);
				if (rt == NULL || !(rt->rt_flags & RTF_HOST)) {
					log(LOG_INFO, "%s: RTM_ADD publish "
					    "(proxy only) is invalid\n",
					    __func__);
					if (rt)
						RTFREE_LOCKED(rt);
					return EINVAL;
				}
				RTFREE_LOCKED(rt);

				flags |= LLE_PROXY;
			}
#endif
		}
		flags |= LLE_CREATE;
		break;

	case RTM_DELETE:
		flags |= LLE_DELETE;
		break;

	case RTM_CHANGE:
		break;

	default:
		return EINVAL; /* XXX not implemented yet */
	}

	/* XXX linked list may be too expensive */
	LLTABLE_RLOCK();
	SLIST_FOREACH(llt, &V_lltables, llt_link) {
		if (llt->llt_af == dst->sa_family &&
		    llt->llt_ifp == ifp)
			break;
	}
	LLTABLE_RUNLOCK();
	KASSERT(llt != NULL, ("Yep, ugly hacks are bad\n"));

	if (flags && LLE_CREATE)
		flags |= LLE_EXCLUSIVE;
	
	IF_AFDATA_LOCK(ifp);
	lle = lla_lookup(llt, flags, dst);
	IF_AFDATA_UNLOCK(ifp);
	if (LLE_IS_VALID(lle)) {
		if (flags & LLE_CREATE) {
			/*
			 * If we delay the delete, then a subsequent
			 * "arp add" should look up this entry, reset the
			 * LLE_DELETED flag, and reset the expiration timer
			 */
			bcopy(LLADDR(dl), &lle->ll_addr, ifp->if_addrlen);
			lle->la_flags |= LLE_VALID;
			lle->la_flags &= ~LLE_DELETED;
#ifdef INET6
			/*
			 * ND6
			 */
			if (dst->sa_family == AF_INET6)
				lle->ln_state = ND6_LLINFO_REACHABLE;
#endif
			/*
			 * NB: arp and ndp always set (RTF_STATIC | RTF_HOST)
			 */

			if (rtm->rtm_rmx.rmx_expire == 0) {
				lle->la_flags |= LLE_STATIC;
				lle->la_expire = 0;
			} else
				lle->la_expire = rtm->rtm_rmx.rmx_expire;
			laflags = lle->la_flags;
			LLE_WUNLOCK(lle);
#ifdef INET
			/*  gratuitous ARP */
			if ((laflags & LLE_PUB) && dst->sa_family == AF_INET) {
				arprequest(ifp, 
				    &((struct sockaddr_in *)dst)->sin_addr,
				    &((struct sockaddr_in *)dst)->sin_addr,
				    ((laflags & LLE_PROXY) ?
					(u_char *)IF_LLADDR(ifp) :
					(u_char *)LLADDR(dl)));
			}
#endif
		} else {
			if (flags & LLE_EXCLUSIVE)
				LLE_WUNLOCK(lle);
			else
				LLE_RUNLOCK(lle);
		}
	} else if ((lle == NULL) && (flags & LLE_DELETE))
		error = EINVAL;


	return (error);
}

static void
vnet_lltable_init()
{

	SLIST_INIT(&V_lltables);
}
VNET_SYSINIT(vnet_lltable_init, SI_SUB_PSEUDO, SI_ORDER_FIRST,
    vnet_lltable_init, NULL);

