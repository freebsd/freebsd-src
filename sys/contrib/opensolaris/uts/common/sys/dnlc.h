/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#ifndef _SYS_DNLC_H
#define	_SYS_DNLC_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/kstat.h>

/*
 * DNLC - Directory name lookup cache.
 * There are now two sorts of name caching:
 *
 * Standard dnlc: This original cache holds recent mappings
 *                of <directory vnode, name> to vnode mappings.
 *
 * Directory caches: Entire large directories can be cached, subject to
 *		     memory availability and tunables. A directory cache
 *		     anchor point must be provided in the xxnode for
 *		     a directory.
 */


/*
 * Standard dnlc
 * =============
 */

/*
 * This structure describes the elements in the cache of recent
 * names looked up.
 *
 * Note namlen is a uchar_t to conserve space
 * and alignment padding. The max length of any
 * pathname component is defined as MAXNAMELEN
 * which is 256 (including the terminating null).
 * So provided this doesn't change, we don't include the null,
 * we always use bcmp to compare strings, and we don't start
 * storing full names, then we are ok. The space savings are worth it.
 */
typedef struct ncache {
	struct ncache *hash_next; 	/* hash chain, MUST BE FIRST */
	struct ncache *hash_prev;
	struct vnode *vp;		/* vnode the name refers to */
	struct vnode *dp;		/* vnode of parent of name */
	int hash;			/* hash signature */
	uchar_t namlen;			/* length of name */
	char name[1];			/* segment name - null terminated */
} ncache_t;

/*
 * Hash table bucket structure of name cache entries for fast lookup.
 */
typedef struct nc_hash	{
	ncache_t *hash_next;
	ncache_t *hash_prev;
	kmutex_t hash_lock;
} nc_hash_t;

/*
 * Statistics on name cache
 * Not protected by locks
 */
/*
 * ncstats has been deprecated, due to the integer size of the counters
 * which can easily overflow in the dnlc.
 * It is maintained (at some expense) for compatability.
 * The preferred interface is the kstat accessible nc_stats below, ehich
 * is actually shared with directory caching.
 */
struct ncstats {
	int	hits;		/* hits that we can really use */
	int	misses;		/* cache misses */
	int	enters;		/* number of enters done */
	int	dbl_enters;	/* number of enters tried when already cached */
	int	long_enter;	/* deprecated, no longer accounted */
	int	long_look;	/* deprecated, no longer accounted */
	int	move_to_front;	/* entry moved to front of hash chain */
	int	purges;		/* number of purges of cache */
};

struct nc_stats {
	kstat_named_t ncs_hits;		/* cache hits */
	kstat_named_t ncs_misses;	/* cache misses */
	kstat_named_t ncs_neg_hits;	/* negative cache hits */
	kstat_named_t ncs_enters;	/* enters */
	kstat_named_t ncs_dbl_enters;	/* enters when entry already cached */
	kstat_named_t ncs_purge_total;	/* total entries prurged */
	kstat_named_t ncs_purge_all;	/* dnlc_purge() calls */
	kstat_named_t ncs_purge_vp;	/* dnlc_purge_vp() calls */
	kstat_named_t ncs_purge_vfs;	/* dnlc_purge_vfs() calls */
	kstat_named_t ncs_purge_fs1;	/* dnlc_purge_fs1() calls */
	kstat_named_t ncs_pick_free;	/* found a free ncache */
	kstat_named_t ncs_pick_heur;	/* found ncache w/ NULL vpages */
	kstat_named_t ncs_pick_last;	/* found last ncache on chain */
};

/*
 * The dnlc hashing function.
 * Although really a kernel macro we export it to allow validation
 * of ncache_t entries by mdb. Note, mdb can handle the ASSERT.
 *
 * 'hash' and 'namlen' must be l-values. A check is made to ensure
 * the name length fits into an unsigned char (see ncache_t).
 */
#define	DNLCHASH(name, dvp, hash, namlen)			\
	{							\
		char Xc, *Xcp;					\
		hash = (int)((uintptr_t)(dvp)) >> 8;		\
		for (Xcp = (name); (Xc = *Xcp) != 0; Xcp++)	\
			(hash) = ((hash) << 4) + (hash) + Xc;	\
		ASSERT((Xcp - (name)) <= ((1 << NBBY) - 1));	\
		(namlen) = Xcp - (name);			\
	}

#if defined(_KERNEL)

#include <sys/vfs.h>
#include <sys/vnode.h>

extern int ncsize;		/* set in param_init() # of dnlc entries */
extern vnode_t negative_cache_vnode;
#define	DNLC_NO_VNODE &negative_cache_vnode

void	dnlc_update(vnode_t *, char *, vnode_t *);
vnode_t	*dnlc_lookup(vnode_t *, char *);
int	dnlc_purge_vfsp(vfs_t *, int);
void	dnlc_remove(vnode_t *, char *);
void	dnlc_reduce_cache(void *);

#endif	/* defined(_KERNEL) */


/*
 * Directory caching interfaces
 * ============================
 */

/*
 * Typically for large directories, the file names will be the same or
 * at least similar lengths. So there's no point in anything more elaborate
 * than a simple unordered linked list of free space entries.
 * For small directories the name length distribution doesn't really matter.
 */
typedef struct dcfree {
	uint64_t df_handle;		/* fs supplied handle */
	struct dcfree *df_next; 	/* link to next free entry in bucket */
	uint_t df_len;			/* length of free entry */
} dcfree_t;

typedef struct dcentry {
	uint64_t de_handle;		/* fs supplied and returned data */
	struct dcentry *de_next;	/* link to next name entry in bucket */
	int de_hash;			/* hash signature */
	uchar_t de_namelen;		/* length of name excluding null */
	char de_name[1];		/* null terminated name */
} dcentry_t;

typedef struct dircache {
	struct dircache *dc_next;	/* chain - for purge purposes */
	struct dircache *dc_prev;	/* chain - for purge purposes */
	int64_t dc_actime;		/* dir access time, from lbolt64 */
	dcentry_t **dc_namehash;	/* entry hash table pointer */
	dcfree_t **dc_freehash;		/* free entry hash table pointer */
	uint_t dc_num_entries;		/* no of named entries */
	uint_t dc_num_free;		/* no of free space entries */
	uint_t dc_nhash_mask;		/* name hash table size - 1 */
	uint_t dc_fhash_mask;		/* free space hash table size - 1 */
	struct dcanchor *dc_anchor;	/* back ptr to anchor */
	boolean_t dc_complete;		/* cache complete boolean */
} dircache_t;

typedef struct dcanchor {
	void *dca_dircache;	/* pointer to directory cache */
	kmutex_t dca_lock;		/* protects the directory cache */
} dcanchor_t;

/*
 * Head struct for doubly linked chain of dircache_t
 * The next and prev fields must match those of a dircache_t
 */
typedef struct {
	dircache_t *dch_next;		/* next in chain */
	dircache_t *dch_prev;		/* prev in chain */
	kmutex_t dch_lock;		/* lock for the chain */
} dchead_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DNLC_H */
