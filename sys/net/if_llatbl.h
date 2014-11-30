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

struct llentry {
	/* FIELDS PROTECTED BY IFDATA LOCK */
	LIST_ENTRY(llentry)	 lle_next;
	union {
		struct in_addr	addr4;
		struct in6_addr	addr6;
	} r_l3addr;
	union {
		uint64_t	mac_aligned;
		uint16_t	mac16[3];
		uint8_t		mac8[20];	/* IB needs 20 bytes. */
		char		ll_prepend[20];	/* L2 data to prepend */
	} ll_addr;
	uint16_t		r_flags;	/* runtime flags */
	uint16_t		r_len;		/* length of prepend data */
	uint64_t		r_kick;		/* for unused lle detection */

	/* FIELDS PROTECTED BY LLE rwlock */
	struct lltable		 *lle_tbl;
	struct llentries	 *lle_head;
	void			(*lle_free)(struct lltable *, struct llentry *);
	struct mbuf		 *la_hold;
	int			 la_numheld;  /* # of packets currently held */
	time_t			 la_expire;
	uint16_t		 la_flags;
	uint16_t		 la_asked;
	uint16_t		 la_preempt;
	uint16_t		 ln_byhint;
	int16_t			 ln_state;	/* IPv6 has ND6_LLINFO_NOSTATE == -2 */
	uint16_t		 ln_router;
	time_t			 ln_ntick;
	int			 lle_refcnt;
	LIST_ENTRY(llentry)	lle_chain;	/* chain of deleted items */
	struct rwlock		 lle_lock;

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
#define	LLE_LOCK_INIT(lle)	rw_init_flags(&(lle)->lle_lock, "lle", RW_DUPOK)
#define	LLE_LOCK_DESTROY(lle)	rw_destroy(&(lle)->lle_lock)
#define	LLE_WLOCK_ASSERT(lle)	rw_assert(&(lle)->lle_lock, RA_WLOCKED)

#define	LLE_ADDREF(lle) do {					\
	LLE_WLOCK_ASSERT(lle);					\
	KASSERT((lle)->lle_refcnt >= 0,				\
	    ("negative refcnt %d on lle %p",			\
	    (lle)->lle_refcnt, (lle)));				\
	(lle)->lle_refcnt++;					\
} while (0)

#define	LLE_REMREF(lle)	do {					\
	LLE_WLOCK_ASSERT(lle);					\
	KASSERT((lle)->lle_refcnt > 0,				\
	    ("bogus refcnt %d on lle %p",			\
	    (lle)->lle_refcnt, (lle)));				\
	(lle)->lle_refcnt--;					\
} while (0)

#define	LLE_FREE_LOCKED(lle) do {				\
	if ((lle)->lle_refcnt == 1)				\
		(lle)->lle_free((lle)->lle_tbl, (lle));		\
	else {							\
		LLE_REMREF(lle);				\
		LLE_WUNLOCK(lle);				\
	}							\
	/* guard against invalid refs */			\
	(lle) = NULL;						\
} while (0)

#define	LLE_FREE(lle) do {					\
	LLE_WLOCK(lle);						\
	LLE_FREE_LOCKED(lle);					\
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

typedef	struct llentry *(llt_lookup_t)(struct lltable *, u_int flags,
    const struct sockaddr *l3addr);
typedef	struct llentry *(llt_create_t)(struct lltable *, u_int flags,
    const struct sockaddr *l3addr);
typedef	int (llt_delete_t)(struct lltable *, u_int flags,
    const struct sockaddr *l3addr);
typedef int (llt_dump_entry_t)(struct lltable *, struct llentry *,
    struct sysctl_req *);
typedef uint32_t (llt_hash_t)(const struct llentry *);
typedef int (llt_match_prefix_t)(const struct sockaddr *,
    const struct sockaddr *, u_int, struct llentry *);
typedef void (llt_stop_timers_t)(struct llentry *lle);

struct lltable {
	SLIST_ENTRY(lltable)	llt_link;
	struct llentries	lle_head[LLTBL_HASHTBL_SIZE];
	int			llt_af;
	struct ifnet		*llt_ifp;

	llt_lookup_t		*llt_lookup;
	llt_create_t		*llt_create;
	llt_delete_t		*llt_delete;
	llt_dump_entry_t	*llt_dump_entry;
	llt_hash_t		*llt_hash;
	llt_match_prefix_t	*llt_match_prefix;
	llt_stop_timers_t	*llt_stop_timers;
};

MALLOC_DECLARE(M_LLTABLE);

/*
 * LLE flags used by fast path code
 */
#define	RLLE_VALID	0x0001	/* ll_addr can be used */

/*
 * Various LLE flags
 */
#define	LLE_DELETED	0x0001	/* entry must be deleted */
#define	LLE_STATIC	0x0002	/* entry is static */
#define	LLE_IFADDR	0x0004	/* entry is interface addr */
#define	LLE_VALID	0x0008	/* ll_addr is valid */
#define	LLE_PUB		0x0020	/* publish entry ??? */
#define	LLE_LINKED	0x0040	/* linked to lookup structure */
#define	LLE_CALLOUTREF	0x0080	/* callout set */
/* LLE request flags */
#define	LLE_UNLOCKED	0x0100	/* return lle unlocked  */
#define	LLE_EXCLUSIVE	0x0200	/* return lle wlocked  */

#define LLATBL_HASH(key, mask) \
	(((((((key >> 8) ^ key) >> 8) ^ key) >> 8) ^ key) & mask)

struct lltable *lltable_init(struct ifnet *, int);
void		lltable_free(struct lltable *);
void		lltable_prefix_free(int, struct sockaddr *,
		    struct sockaddr *, u_int);
#if 0
void		lltable_drain(int);
#endif
int		lltable_sysctl_dumparp(int, struct sysctl_req *);

void		llentry_link(struct lltable *, struct llentry *);
void		llentry_unlink(struct llentry *);
void		llentries_unlink(struct llentries *);
size_t		llentry_free(struct llentry *);
struct llentry  *llentry_alloc(struct ifnet *, struct lltable *,
		    struct sockaddr_storage *);

/*
 * Generic link layer address lookup function.
 */
static __inline struct llentry *
lltable_lookup_lle(struct lltable *llt, u_int flags,
    const struct sockaddr *l3addr)
{

	return llt->llt_lookup(llt, flags, l3addr);
}

static __inline struct llentry *
lltable_create_lle(struct lltable *llt, u_int flags,
    const struct sockaddr *l3addr)
{

	return llt->llt_create(llt, flags, l3addr);
}

static __inline int
lltable_delete_lle(struct lltable *llt, u_int flags,
    const struct sockaddr *l3addr)
{

	return llt->llt_delete(llt, flags, l3addr);
}


int		lla_rt_output(struct rt_msghdr *, struct rt_addrinfo *);

#include <sys/eventhandler.h>
enum {
	LLENTRY_RESOLVED,
	LLENTRY_TIMEDOUT,
	LLENTRY_DELETED,
	LLENTRY_EXPIRED,
};
typedef void (*lle_event_fn)(void *, struct llentry *, int);
EVENTHANDLER_DECLARE(lle_event, lle_event_fn);
#endif  /* _NET_IF_LLATBL_H_ */
