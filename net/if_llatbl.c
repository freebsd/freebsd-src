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

#include "opt_ddb.h"
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

#ifdef DDB
#include <ddb/ddb.h>
#endif

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
 *
 * Returns the number of held packets, if any, that were dropped.
 */
size_t
llentry_free(struct llentry *lle)
{
	size_t pkts_dropped;
	struct mbuf *next;

	IF_AFDATA_WLOCK_ASSERT(lle->lle_tbl->llt_ifp);
	LLE_WLOCK_ASSERT(lle);

	LIST_REMOVE(lle, lle_next);
	lle->la_flags &= ~(LLE_VALID | LLE_LINKED);

	pkts_dropped = 0;
	while ((lle->la_numheld > 0) && (lle->la_hold != NULL)) {
		next = lle->la_hold->m_nextpkt;
		m_freem(lle->la_hold);
		lle->la_hold = next;
		lle->la_numheld--;
		pkts_dropped++;
	}

	KASSERT(lle->la_numheld == 0,
		("%s: la_numheld %d > 0, pkts_droped %zd", __func__,
		 lle->la_numheld, pkts_dropped));

	LLE_FREE_LOCKED(lle);

	return (pkts_dropped);
}

/*
 * (al)locate an llentry for address dst (equivalent to rtalloc for new-arp).
 *
 * If found the llentry * is returned referenced and unlocked.
 */
struct llentry *
llentry_alloc(struct ifnet *ifp, struct lltable *lt,
    struct sockaddr_storage *dst)
{
	struct llentry *la;

	IF_AFDATA_RLOCK(ifp);
	la = lla_lookup(lt, LLE_EXCLUSIVE, (struct sockaddr *)dst);
	IF_AFDATA_RUNLOCK(ifp);
	if ((la == NULL) &&
	    (ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) == 0) {
		IF_AFDATA_WLOCK(ifp);
		la = lla_lookup(lt, (LLE_CREATE | LLE_EXCLUSIVE),
		    (struct sockaddr *)dst);
		IF_AFDATA_WUNLOCK(ifp);
	}

	if (la != NULL) {
		LLE_ADDREF(la);
		LLE_WUNLOCK(la);
	}

	return (la);
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

	IF_AFDATA_WLOCK(llt->llt_ifp);
	for (i = 0; i < LLTBL_HASHTBL_SIZE; i++) {
		LIST_FOREACH_SAFE(lle, &llt->lle_head[i], lle_next, next) {
			LLE_WLOCK(lle);
			if (callout_stop(&lle->la_timer))
				LLE_REMREF(lle);
			llentry_free(lle);
		}
	}
	IF_AFDATA_WUNLOCK(llt->llt_ifp);

	free(llt, M_LLTABLE);
}

#if 0
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
				LLE_WLOCK(lle);
				if (lle->la_hold) {
					m_freem(lle->la_hold);
					lle->la_hold = NULL;
				}
				LLE_WUNLOCK(lle);
			}
		}
	}
	LLTABLE_RUNLOCK();
}
#endif

void
lltable_prefix_free(int af, struct sockaddr *prefix, struct sockaddr *mask,
    u_int flags)
{
	struct lltable *llt;

	LLTABLE_RLOCK();
	SLIST_FOREACH(llt, &V_lltables, llt_link) {
		if (llt->llt_af != af)
			continue;

		llt->llt_prefix_free(llt, prefix, mask, flags);
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

	KASSERT(dl != NULL && dl->sdl_family == AF_LINK,
	    ("%s: invalid dl\n", __func__));

	ifp = ifnet_byindex(dl->sdl_index);
	if (ifp == NULL) {
		log(LOG_INFO, "%s: invalid ifp (sdl_index %d)\n",
		    __func__, dl->sdl_index);
		return EINVAL;
	}

	switch (rtm->rtm_type) {
	case RTM_ADD:
		if (rtm->rtm_flags & RTF_ANNOUNCE)
			flags |= LLE_PUB;
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

	if (flags & LLE_CREATE)
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
			lle->la_flags |= (flags & LLE_PUB);
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
			/* gratuitous ARP */
			if ((laflags & LLE_PUB) && dst->sa_family == AF_INET)
				arprequest(ifp,
				    &((struct sockaddr_in *)dst)->sin_addr,
				    &((struct sockaddr_in *)dst)->sin_addr,
				    (u_char *)LLADDR(dl));
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

#ifdef DDB
struct llentry_sa {
	struct llentry		base;
	struct sockaddr		l3_addr;
};

static void
llatbl_lle_show(struct llentry_sa *la)
{
	struct llentry *lle;
	uint8_t octet[6];

	lle = &la->base;
	db_printf("lle=%p\n", lle);
	db_printf(" lle_next=%p\n", lle->lle_next.le_next);
	db_printf(" lle_lock=%p\n", &lle->lle_lock);
	db_printf(" lle_tbl=%p\n", lle->lle_tbl);
	db_printf(" lle_head=%p\n", lle->lle_head);
	db_printf(" la_hold=%p\n", lle->la_hold);
	db_printf(" la_numheld=%d\n", lle->la_numheld);
	db_printf(" la_expire=%ju\n", (uintmax_t)lle->la_expire);
	db_printf(" la_flags=0x%04x\n", lle->la_flags);
	db_printf(" la_asked=%u\n", lle->la_asked);
	db_printf(" la_preempt=%u\n", lle->la_preempt);
	db_printf(" ln_byhint=%u\n", lle->ln_byhint);
	db_printf(" ln_state=%d\n", lle->ln_state);
	db_printf(" ln_router=%u\n", lle->ln_router);
	db_printf(" ln_ntick=%ju\n", (uintmax_t)lle->ln_ntick);
	db_printf(" lle_refcnt=%d\n", lle->lle_refcnt);
	bcopy(&lle->ll_addr.mac16, octet, sizeof(octet));
	db_printf(" ll_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
	    octet[0], octet[1], octet[2], octet[3], octet[4], octet[5]);
	db_printf(" la_timer=%p\n", &lle->la_timer);

	switch (la->l3_addr.sa_family) {
#ifdef INET
	case AF_INET:
	{
		struct sockaddr_in *sin;
		char l3s[INET_ADDRSTRLEN];

		sin = (struct sockaddr_in *)&la->l3_addr;
		inet_ntoa_r(sin->sin_addr, l3s);
		db_printf(" l3_addr=%s\n", l3s);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;
		char l3s[INET6_ADDRSTRLEN];

		sin6 = (struct sockaddr_in6 *)&la->l3_addr;
		ip6_sprintf(l3s, &sin6->sin6_addr);
		db_printf(" l3_addr=%s\n", l3s);
		break;
	}
#endif
	default:
		db_printf(" l3_addr=N/A (af=%d)\n", la->l3_addr.sa_family);
		break;
	}
}

DB_SHOW_COMMAND(llentry, db_show_llentry)
{

	if (!have_addr) {
		db_printf("usage: show llentry <struct llentry *>\n");
		return;
	}

	llatbl_lle_show((struct llentry_sa *)addr);
}

static void
llatbl_llt_show(struct lltable *llt)
{
	int i;
	struct llentry *lle;

	db_printf("llt=%p llt_af=%d llt_ifp=%p\n",
	    llt, llt->llt_af, llt->llt_ifp);

	for (i = 0; i < LLTBL_HASHTBL_SIZE; i++) {
		LIST_FOREACH(lle, &llt->lle_head[i], lle_next) {

			llatbl_lle_show((struct llentry_sa *)lle);
			if (db_pager_quit)
				return;
		}
	}
}

DB_SHOW_COMMAND(lltable, db_show_lltable)
{

	if (!have_addr) {
		db_printf("usage: show lltable <struct lltable *>\n");
		return;
	}

	llatbl_llt_show((struct lltable *)addr);
}

DB_SHOW_ALL_COMMAND(lltables, db_show_all_lltables)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct lltable *llt;

	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
#ifdef VIMAGE
		db_printf("vnet=%p\n", curvnet);
#endif
		SLIST_FOREACH(llt, &V_lltables, llt_link) {
			db_printf("llt=%p llt_af=%d llt_ifp=%p(%s)\n",
			    llt, llt->llt_af, llt->llt_ifp,
			    (llt->llt_ifp != NULL) ?
				llt->llt_ifp->if_xname : "?");
			if (have_addr && addr != 0) /* verbose */
				llatbl_llt_show(llt);
			if (db_pager_quit) {
				CURVNET_RESTORE();
				return;
			}
		}
		CURVNET_RESTORE();
	}
}
#endif
