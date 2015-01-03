/*
 * Copyright (c) 2014 Alexander V. Chernikov. All rights reserved.
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
__FBSDID("$FreeBSD$");

#ifndef	_NET_IF_LLATBL_VAR_H_
#define	_NET_IF_LLATBL_VAR_H_


extern struct rwlock lltable_rwlock;
#define	LLTABLE_RLOCK()		rw_rlock(&lltable_rwlock)
#define	LLTABLE_RUNLOCK()	rw_runlock(&lltable_rwlock)
#define	LLTABLE_WLOCK()		rw_wlock(&lltable_rwlock)
#define	LLTABLE_WUNLOCK()	rw_wunlock(&lltable_rwlock)
#define	LLTABLE_LOCK_ASSERT()	rw_assert(&lltable_rwlock, RA_LOCKED)

#ifndef LLTBL_HASHTBL_SIZE
#define	LLTBL_HASHTBL_SIZE	32	/* default 32 ? */
#endif

#ifndef LLTBL_HASHMASK
#define	LLTBL_HASHMASK	(LLTBL_HASHTBL_SIZE - 1)
#endif

#define LLATBL_HASH(key, mask) \
	(((((((key >> 8) ^ key) >> 8) ^ key) >> 8) ^ key) & mask)

typedef	struct llentry *(llt_lookup_t)(struct lltable *, u_int flags,
    const void *paddr);
typedef	struct llentry *(llt_create_t)(struct lltable *, u_int flags,
    const void *paddr);
typedef int (llt_dump_entry_t)(struct lltable *, struct llentry *,
    struct sysctl_req *);
typedef uint32_t (llt_hash_t)(const struct llentry *);
typedef int (llt_match_prefix_t)(const struct sockaddr *,
    const struct sockaddr *, u_int, struct llentry *);
typedef void (llt_clear_entry_t)(struct lltable *, struct llentry *);
typedef void (llt_free_tbl_t)(struct lltable *);
typedef void (llt_link_entry_t)(struct lltable *, struct llentry *);
typedef void (llt_unlink_entry_t)(struct llentry *);
typedef int (llt_prepare_sentry_t)(struct lltable *, struct llentry *,
    struct rt_addrinfo *);
typedef const void *(llt_get_sa_addr_t)(const struct sockaddr *l3addr);
typedef void (llt_fill_sa_entry_t)(const struct llentry *, struct sockaddr *);

typedef int (llt_foreach_cb_t)(struct lltable *, struct llentry *, void *);
typedef int (llt_foreach_entry_t)(struct lltable *, llt_foreach_cb_t *, void *);


struct lltable {
	SLIST_ENTRY(lltable)	llt_link;
	struct llentries	lle_head[LLTBL_HASHTBL_SIZE];
	int			llt_af;
	struct ifnet		*llt_ifp;

	llt_lookup_t		*llt_lookup;
	llt_create_t		*llt_create;
	llt_dump_entry_t	*llt_dump_entry;
	llt_hash_t		*llt_hash;
	llt_match_prefix_t	*llt_match_prefix;
	llt_clear_entry_t	*llt_clear_entry;
	llt_foreach_entry_t	*llt_foreach_entry;
	llt_link_entry_t	*llt_link_entry;
	llt_unlink_entry_t	*llt_unlink_entry;
	llt_prepare_sentry_t	*llt_prepare_static_entry;
	llt_get_sa_addr_t	*llt_get_sa_addr;
	llt_fill_sa_entry_t	*llt_fill_sa_entry;
	llt_free_tbl_t		*llt_free_tbl;
};

MALLOC_DECLARE(M_LLTABLE);

void lltable_link(struct lltable *llt);
void lltable_free(struct lltable *llt);

/* helper functions */
size_t lltable_drop_entry_queue(struct llentry *);

/*
 * Generic link layer table methods.
 */

static __inline struct llentry *
lltable_lookup_lle(struct lltable *llt, u_int flags,
    const void *paddr)
{

	return (llt->llt_lookup(llt, flags, paddr));
}

struct llentry *lltable_create_lle(struct lltable *llt, u_int flags,
    const void *paddr);
void lltable_link_entry(struct lltable *llt, struct llentry *lle);
void lltable_unlink_entry(struct lltable *llt, struct llentry *lle);


#endif
