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

#ifndef	_NET_IF_LLATBL_H_
#define	_NET_IF_LLATBL_H_

#include <sys/_rwlock.h>
#include <netinet/in.h>

struct ifnet;
struct sysctl_req;
struct rt_msghdr;
struct rt_addrinfo;

struct llentry;
LIST_HEAD(llentries, llentry);

extern struct rwlock lltable_rwlock;
#define	LLTABLE_RLOCK()		rw_rlock(&lltable_rwlock)
#define	LLTABLE_RUNLOCK()	rw_runlock(&lltable_rwlock)
#define	LLTABLE_WLOCK()		rw_wlock(&lltable_rwlock)
#define	LLTABLE_WUNLOCK()	rw_wunlock(&lltable_rwlock)
#define	LLTABLE_LOCK_ASSERT()	rw_assert(&lltable_rwlock, RA_LOCKED)

/*
 * Code referencing llentry must at least hold
 * a shared lock
 */
struct llentry {
	LIST_ENTRY(llentry)	 lle_next;
	struct rwlock		 lle_lock;
	struct lltable		 *lle_tbl;
	struct llentries	 *lle_head;
	struct mbuf		 *la_hold;
	int     		 la_numheld;  /* # of packets currently held */
	time_t			 la_expire;
	uint16_t		 la_flags;    
	uint16_t		 la_asked;
	uint16_t		 la_preempt;
	uint16_t		 ln_byhint;
	int16_t			 ln_state;	/* IPv6 has ND6_LLINFO_NOSTATE == -2 */
	uint16_t		 ln_router; 
	time_t			 ln_ntick;
	int			 lle_refcnt;
				 
	union {
		uint64_t	mac_aligned;
		uint16_t	mac16[3];
	} ll_addr;

	/* XXX af-private? */
	union {
		struct callout	ln_timer_ch;
		struct callout  la_timer;
	} lle_timer;
	/* NB: struct sockaddr must immediately follow */
};

#define	LLE_WLOCK(lle)		rw_wlock(&(lle)->lle_lock)
#define	LLE_RLOCK(lle)		rw_rlock(&(lle)->lle_lock)
#define	LLE_WUNLOCK(lle)	rw_wunlock(&(lle)->lle_lock)
#define	LLE_RUNLOCK(lle)	rw_runlock(&(lle)->lle_lock)
#define	LLE_DOWNGRADE(lle)	rw_downgrade(&(lle)->lle_lock)
#define	LLE_TRY_UPGRADE(lle)	rw_try_upgrade(&(lle)->lle_lock)
#define	LLE_LOCK_INIT(lle)	rw_init_flags(&(lle)->lle_lock, "lle", RW_DUPOK)
#define	LLE_LOCK_DESTROY(lle)	rw_destroy(&(lle)->lle_lock)
#define	LLE_WLOCK_ASSERT(lle)	rw_assert(&(lle)->lle_lock, RA_WLOCKED)

#define LLE_IS_VALID(lle)	(((lle) != NULL) && ((lle) != (void *)-1))

#define	LLE_ADDREF(lle) do {					\
	LLE_WLOCK_ASSERT(lle);					\
	KASSERT((lle)->lle_refcnt >= 0,				\
		("negative refcnt %d", (lle)->lle_refcnt));	\
	(lle)->lle_refcnt++;					\
} while (0)

#define	LLE_REMREF(lle)	do {					\
	LLE_WLOCK_ASSERT(lle);					\
	KASSERT((lle)->lle_refcnt > 1,				\
		("bogus refcnt %d", (lle)->lle_refcnt));	\
	(lle)->lle_refcnt--;					\
} while (0)

#define	LLE_FREE_LOCKED(lle) do {				\
	if ((lle)->lle_refcnt <= 1)				\
		(lle)->lle_tbl->llt_free((lle)->lle_tbl, (lle));\
	else {							\
		(lle)->lle_refcnt--;				\
		LLE_WUNLOCK(lle);				\
	}							\
	/* guard against invalid refs */			\
	lle = 0;						\
} while (0)

#define	LLE_FREE(lle) do {					\
	LLE_WLOCK(lle);						\
	if ((lle)->lle_refcnt <= 1)				\
		(lle)->lle_tbl->llt_free((lle)->lle_tbl, (lle));\
	else {							\
		(lle)->lle_refcnt--;				\
		LLE_WUNLOCK(lle);				\
	}							\
	/* guard against invalid refs */			\
	lle = NULL;						\
} while (0)


#define	ln_timer_ch	lle_timer.ln_timer_ch
#define	la_timer	lle_timer.la_timer

/* XXX bad name */
#define	L3_ADDR(lle)	((struct sockaddr *)(&lle[1]))
#define	L3_ADDR_LEN(lle)	(((struct sockaddr *)(&lle[1]))->sa_len)

#ifndef LLTBL_HASHTBL_SIZE
#define	LLTBL_HASHTBL_SIZE	32	/* default 32 ? */
#endif

#ifndef LLTBL_HASHMASK
#define	LLTBL_HASHMASK	(LLTBL_HASHTBL_SIZE - 1)
#endif

struct lltable {
	SLIST_ENTRY(lltable)	llt_link;
	struct llentries	lle_head[LLTBL_HASHTBL_SIZE];
	int			llt_af;
	struct ifnet		*llt_ifp;

	struct llentry *	(*llt_new)(const struct sockaddr *, u_int);
	void			(*llt_free)(struct lltable *, struct llentry *);
	void			(*llt_prefix_free)(struct lltable *,
				    const struct sockaddr *prefix,
				    const struct sockaddr *mask);
	struct llentry *	(*llt_lookup)(struct lltable *, u_int flags,
				    const struct sockaddr *l3addr);
	int			(*llt_rtcheck)(struct ifnet *, u_int flags,
				    const struct sockaddr *);
	int			(*llt_dump)(struct lltable *,
				     struct sysctl_req *);
};
MALLOC_DECLARE(M_LLTABLE);

/*
 * flags to be passed to arplookup.
 */
#define	LLE_DELETED	0x0001	/* entry must be deleted */
#define	LLE_STATIC	0x0002	/* entry is static */
#define	LLE_IFADDR	0x0004	/* entry is interface addr */
#define	LLE_VALID	0x0008	/* ll_addr is valid */
#define	LLE_PROXY	0x0010	/* proxy entry ??? */
#define	LLE_PUB		0x0020	/* publish entry ??? */
#define	LLE_DELETE	0x4000	/* delete on a lookup - match LLE_IFADDR */
#define	LLE_CREATE	0x8000	/* create on a lookup miss */
#define	LLE_EXCLUSIVE	0x2000	/* return lle xlocked  */

#define LLATBL_HASH(key, mask) \
	(((((((key >> 8) ^ key) >> 8) ^ key) >> 8) ^ key) & mask)

struct lltable *lltable_init(struct ifnet *, int);
void		lltable_free(struct lltable *);
void		lltable_prefix_free(int, struct sockaddr *, 
                       struct sockaddr *);
#if 0
void		lltable_drain(int);
#endif
int		lltable_sysctl_dumparp(int, struct sysctl_req *);

size_t		llentry_free(struct llentry *);
int		llentry_update(struct llentry **, struct lltable *,
                       struct sockaddr_storage *, struct ifnet *);

/*
 * Generic link layer address lookup function.
 */
static __inline struct llentry *
lla_lookup(struct lltable *llt, u_int flags, const struct sockaddr *l3addr)
{
	return llt->llt_lookup(llt, flags, l3addr);
}

int		lla_rt_output(struct rt_msghdr *, struct rt_addrinfo *);
#endif  /* _NET_IF_LLATBL_H_ */
