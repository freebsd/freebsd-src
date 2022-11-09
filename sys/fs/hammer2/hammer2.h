/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * HAMMER2 in-memory cache of media structures.
 *
 * This header file contains structures used internally by the HAMMER2
 * implementation.  See hammer2_disk.h for on-disk structures.
 *
 * There is an in-memory representation of all on-media data structure.
 * Almost everything is represented by a hammer2_chain structure in-memory.
 * Other higher-level structures typically map to chains.
 *
 * A great deal of data is accessed simply via its buffer cache buffer,
 * which is mapped for the duration of the chain's lock.  HAMMER2 must
 * implement its own buffer cache layer on top of the system layer to
 * allow for different threads to lock different sub-block-sized buffers.
 *
 * The in-memory representation may remain cached even after the related
 * data has been detached.
 */

#ifndef _FS_HAMMER2_HAMMER2_H_
#define _FS_HAMMER2_HAMMER2_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/gsb_crc32.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sx.h>
#include <sys/tree.h>
#include <sys/uuid.h>
#include <sys/vnode.h>

#include <machine/atomic.h>
#include <vm/uma.h>

#include "hammer2_disk.h"
#include "hammer2_rb.h"

/* KASSERT variant from DragonFly */
#ifdef INVARIANTS
#define KKASSERT(exp)	do { if (__predict_false(!(exp)))	  \
				panic("assertion \"%s\" failed "  \
				"in %s at %s:%u", #exp, __func__, \
				__FILE__, __LINE__); } while (0)
#else
#define KKASSERT(exp)	do { } while (0)
#endif

/* printf(9) variants for HAMMER2 */
#ifdef INVARIANTS
#define HFMT	"%s(%s|%d): "
#define HARGS	__func__, \
    curproc ? curproc->p_comm : "-", \
    curthread ? curthread->td_tid : -1
#else
#define HFMT	"%s: "
#define HARGS	__func__
#endif

#define hprintf(X, ...)	printf(HFMT X, HARGS, ## __VA_ARGS__)
#define hpanic(X, ...)	panic(HFMT X, HARGS, ## __VA_ARGS__)

#ifdef INVARIANTS
#define debug_hprintf	hprintf
#else
#define debug_hprintf(X, ...)	do { } while (0)
#endif

struct hammer2_chain;
struct hammer2_dev;
struct hammer2_inode;
struct hammer2_io;
struct hammer2_pfs;
union hammer2_xop;

typedef struct hammer2_chain hammer2_chain_t;
typedef struct hammer2_dev hammer2_dev_t;
typedef struct hammer2_inode hammer2_inode_t;
typedef struct hammer2_io hammer2_io_t;
typedef struct hammer2_pfs hammer2_pfs_t;
typedef union hammer2_xop hammer2_xop_t;

/*
 * Mutex and lock shims.
 * Normal synchronous non-abortable locks can be substituted for spinlocks.
 * FreeBSD HAMMER2 currently uses sx(9) for both mtx and spinlock.
 */
typedef struct sx hammer2_mtx_t;

/* Zero on success. */
#define hammer2_mtx_init(p, s)		sx_init(p, s)
#define hammer2_mtx_init_recurse(p, s)	sx_init_flags(p, s, SX_RECURSE)
#define hammer2_mtx_ex(p)		sx_xlock(p)
#define hammer2_mtx_ex_try(p)		(!sx_try_xlock(p))
#define hammer2_mtx_sh(p)		sx_slock(p)
#define hammer2_mtx_sh_try(p)		(!sx_try_slock(p))
#define hammer2_mtx_unlock(p)		sx_unlock(p)
#define hammer2_mtx_destroy(p)		sx_destroy(p)
#define hammer2_mtx_sleep(c, p, s)	sx_sleep(c, p, 0, s, 0)
#define hammer2_mtx_wakeup(c)		wakeup(c)

/* sx_try_upgrade panics on INVARIANTS if already exclusively locked. */
#define hammer2_mtx_upgrade_try(p)	(!sx_try_upgrade(p))

/* Non-zero if exclusively locked by the calling thread. */
#define hammer2_mtx_owned(p)		sx_xlocked(p)

#define hammer2_mtx_assert_locked(p)	sx_assert(p, SA_LOCKED)
#define hammer2_mtx_assert_unlocked(p)	sx_assert(p, SA_UNLOCKED)
#define hammer2_mtx_assert_ex(p)	sx_assert(p, SA_XLOCKED)
#define hammer2_mtx_assert_sh(p)	sx_assert(p, SA_SLOCKED)

typedef struct sx hammer2_spin_t;

/* Zero on success. */
#define hammer2_spin_init(p, s)		sx_init(p, s)
#define hammer2_spin_ex(p)		sx_xlock(p)
#define hammer2_spin_sh(p)		sx_slock(p)
#define hammer2_spin_unex(p)		sx_xunlock(p)
#define hammer2_spin_unsh(p)		sx_sunlock(p)
#define hammer2_spin_destroy(p)		sx_destroy(p)

#define hammer2_spin_assert_locked(p)	sx_assert(p, SA_LOCKED)
#define hammer2_spin_assert_unlocked(p)	sx_assert(p, SA_UNLOCKED)
#define hammer2_spin_assert_ex(p)	sx_assert(p, SA_XLOCKED)
#define hammer2_spin_assert_sh(p)	sx_assert(p, SA_SLOCKED)

/* per HAMMER2 list of device vnode */
TAILQ_HEAD(hammer2_devvp_list, hammer2_devvp); /* <-> hammer2_devvp::entry */
typedef struct hammer2_devvp_list hammer2_devvp_list_t;

/* per PFS list of LRU chain */
TAILQ_HEAD(hammer2_chain_list, hammer2_chain); /* <-> hammer2_chain::entry */
typedef struct hammer2_chain_list hammer2_chain_list_t;

/* per PFS list of inode */
LIST_HEAD(hammer2_ipdep_list, hammer2_inode); /* <-> hammer2_inode::entry */
typedef struct hammer2_ipdep_list hammer2_ipdep_list_t;

/* per HAMMER2 rbtree of dio */
RB_HEAD(hammer2_io_tree, hammer2_io); /* <-> hammer2_io::rbnode */
typedef struct hammer2_io_tree hammer2_io_tree_t;

/* per PFS rbtree of inode */
RB_HEAD(hammer2_inode_tree, hammer2_inode); /* <-> hammer2_inode::rbnode */
typedef struct hammer2_inode_tree hammer2_inode_tree_t;

/* per chain rbtree of sub-chain */
RB_HEAD(hammer2_chain_tree, hammer2_chain); /* <-> hammer2_chain::rbnode */
typedef struct hammer2_chain_tree hammer2_chain_tree_t;

/*
 * HAMMER2 dio - Management structure wrapping system buffer cache.
 *
 * HAMMER2 uses an I/O abstraction that allows it to cache and manipulate
 * fixed-sized filesystem buffers frontend by variable-sized hammer2_chain
 * structures.
 */
struct hammer2_io {
	RB_ENTRY(hammer2_io)	rbnode;		/* indexed by device offset */
	hammer2_mtx_t		lock;
	hammer2_dev_t		*hmp;
	struct vnode		*devvp;
	struct buf		*bp;
	unsigned int		refs;
	off_t			dbase;		/* offset of devvp within volumes */
	off_t			pbase;
	int			psize;
	int			act;		/* activity */
	int			ticks;
	int			error;
};

#define HAMMER2_DIO_GOOD	0x40000000U	/* dio->bp is stable */
#define HAMMER2_DIO_MASK	0x00FFFFFFU

/*
 * The chain structure tracks a portion of the media topology from the
 * root (volume) down.  Chains represent volumes, inodes, indirect blocks,
 * data blocks, and freemap nodes and leafs.
 */
/*
 * Core topology for chain (embedded in chain).  Protected by a spinlock.
 */
struct hammer2_chain_core {
	hammer2_chain_tree_t	rbtree;		/* sub-chains */
	hammer2_spin_t		spin;
	int			live_zero;	/* blockref array opt */
	unsigned int		chain_count;	/* live + deleted chains under core */
	int			generation;	/* generation number (inserts only) */
};

typedef struct hammer2_chain_core hammer2_chain_core_t;

/*
 * Primary chain structure keeps track of the topology in-memory.
 */
struct hammer2_chain {
	RB_ENTRY(hammer2_chain) rbnode;		/* live chain(s) */
	TAILQ_ENTRY(hammer2_chain) entry;	/* 0-refs LRU */
	hammer2_mtx_t		lock;
	hammer2_mtx_t		inp_lock;
	hammer2_chain_core_t	core;
	hammer2_blockref_t	bref;
	hammer2_dev_t		*hmp;
	hammer2_pfs_t		*pmp;		/* A PFS or super-root (spmp) */
	hammer2_chain_t		*parent;
	hammer2_io_t		*dio;		/* physical data buffer */
	hammer2_media_data_t	*data;		/* data pointer shortcut */
	unsigned int		refs;
	unsigned int		lockcnt;
	unsigned int		flags;		/* for HAMMER2_CHAIN_xxx */
	unsigned int		bytes;		/* physical data size */
	int			error;		/* on-lock data error state */
	int			cache_index;	/* heur speeds up lookup */
};

#define HAMMER2_CHAIN_ALLOCATED		0x00000002	/* kmalloc'd chain */
#define HAMMER2_CHAIN_DESTROY		0x00000004
#define HAMMER2_CHAIN_TESTEDGOOD	0x00000100	/* crc tested good */
#define HAMMER2_CHAIN_COUNTEDBREFS	0x00002000	/* block table stats */
#define HAMMER2_CHAIN_ONRBTREE		0x00004000	/* on parent RB tree */
#define HAMMER2_CHAIN_ONLRU		0x00008000	/* on LRU list */
#define HAMMER2_CHAIN_RELEASE		0x00020000	/* don't keep around */
#define HAMMER2_CHAIN_IOINPROG		0x00100000	/* I/O interlock */
#define HAMMER2_CHAIN_IOSIGNAL		0x00200000	/* I/O interlock */
#define HAMMER2_CHAIN_LRUHINT		0x01000000	/* was reused */

/*
 * HAMMER2 error codes, used by chain->error and cluster->error.  The error
 * code is typically set on-lock unless no I/O was requested, and set on
 * I/O otherwise.  If set for a cluster it generally means that the cluster
 * code could not find a valid copy to present.
 *
 * All HAMMER2 error codes are flags and can be accumulated by ORing them
 * together.
 *
 * EIO		- An I/O error occurred
 * CHECK	- I/O succeeded but did not match the check code
 *
 * NOTE: API allows callers to check zero/non-zero to determine if an error
 *	 condition exists.
 *
 * NOTE: Chain's data field is usually NULL on an IO error but not necessarily
 *	 NULL on other errors.  Check chain->error, not chain->data.
 */
#define HAMMER2_ERROR_EIO		0x00000001	/* device I/O error */
#define HAMMER2_ERROR_CHECK		0x00000002	/* check code error */
#define HAMMER2_ERROR_ENOENT		0x00000040	/* entry not found */
#define HAMMER2_ERROR_EAGAIN		0x00000100	/* retry */
#define HAMMER2_ERROR_ABORTED		0x00001000	/* aborted operation */

/*
 * Flags passed to hammer2_chain_lookup() and hammer2_chain_next().
 *
 * NOTES:
 *	SHARED	    - The input chain is expected to be locked shared,
 *		      and the output chain is locked shared.
 *	ALWAYS	    - Always resolve the data.
 */
#define HAMMER2_LOOKUP_SHARED		0x00000100
#define HAMMER2_LOOKUP_ALWAYS		0x00000800	/* resolve data */

/*
 * Flags passed to hammer2_chain_lock().
 */
#define HAMMER2_RESOLVE_MAYBE		2
#define HAMMER2_RESOLVE_ALWAYS		3
#define HAMMER2_RESOLVE_MASK		0x0F

#define HAMMER2_RESOLVE_SHARED		0x10	/* request shared lock */
#define HAMMER2_RESOLVE_LOCKAGAIN	0x20	/* another shared lock */

/*
 * HAMMER2 cluster - A set of chains representing the same entity.
 *
 * Currently a valid cluster can only have 1 set of chains (nchains)
 * representing the same entity.
 */
#define HAMMER2_XOPFIFO		16

#define HAMMER2_MAXCLUSTER	8
#define HAMMER2_XOPMASK_VOP	((uint32_t)0x80000000U)

#define HAMMER2_XOPMASK_ALLDONE	(HAMMER2_XOPMASK_VOP)

struct hammer2_cluster_item {
	hammer2_chain_t		*chain;
	uint32_t		flags;		/* for HAMMER2_CITEM_xxx */
	int			error;
};

typedef struct hammer2_cluster_item hammer2_cluster_item_t;

#define HAMMER2_CITEM_NULL	0x00000004

struct hammer2_cluster {
	hammer2_cluster_item_t	array[HAMMER2_MAXCLUSTER];
	hammer2_pfs_t		*pmp;
	hammer2_chain_t		*focus;		/* current focus (or mod) */
	int			nchains;
	int			error;		/* error code valid on lock */
};

typedef struct hammer2_cluster	hammer2_cluster_t;

/*
 * HAMMER2 inode.
 */
struct hammer2_inode {
	RB_ENTRY(hammer2_inode) rbnode;		/* inumber lookup (HL) */
	LIST_ENTRY(hammer2_inode) entry;
	hammer2_mtx_t		lock;		/* inode lock */
	hammer2_spin_t		cluster_spin;	/* update cluster */
	hammer2_cluster_t	cluster;
	hammer2_inode_meta_t	meta;		/* copy of meta-data */
	hammer2_pfs_t		*pmp;		/* PFS mount */
	struct vnode		*vp;
	unsigned int		refs;		/* +vpref, +flushref */
	unsigned int		flags;		/* for HAMMER2_INODE_xxx */
};

#define HAMMER2_INODE_ONRBTREE		0x0008

/*
 * HAMMER2 XOP - container for VOP/XOP operation.
 *
 * This structure is used to distribute a VOP operation across multiple
 * nodes.  In FreeBSD HAMMER2, XOP is currently just a function called by
 * VOP to handle chains.
 */
typedef void (*hammer2_xop_func_t)(union hammer2_xop *, int);

struct hammer2_xop_desc {
	hammer2_xop_func_t	storage_func;	/* local storage function */
	const char		*id;
};

typedef struct hammer2_xop_desc hammer2_xop_desc_t;

struct hammer2_xop_fifo {
	hammer2_chain_t		**array;
	int			*errors;
	int			ri;
	int			wi;
	int			flags;
};

typedef struct hammer2_xop_fifo hammer2_xop_fifo_t;

struct hammer2_xop_head {
	hammer2_xop_fifo_t	collect[HAMMER2_MAXCLUSTER];
	hammer2_cluster_t	cluster;
	hammer2_xop_desc_t	*desc;
	hammer2_inode_t		*ip1;
	hammer2_io_t		*focus_dio;
	hammer2_key_t		collect_key;
	uint32_t		run_mask;
	uint32_t		chk_mask;
	int			fifo_size;
	int			error;
	char			*name1;
	size_t			name1_len;
};

typedef struct hammer2_xop_head hammer2_xop_head_t;

#define fifo_mask(xop_head)	((xop_head)->fifo_size - 1)

struct hammer2_xop_readdir {
	hammer2_xop_head_t	head;
	hammer2_key_t		lkey;
};

struct hammer2_xop_nresolve {
	hammer2_xop_head_t	head;
};

struct hammer2_xop_lookup {
	hammer2_xop_head_t	head;
	hammer2_key_t		lhc;
};

struct hammer2_xop_bmap {
	hammer2_xop_head_t	head;
	daddr_t			lbn;
	daddr_t			pbn;
	int			runp;
	int			runb;
};

struct hammer2_xop_strategy {
	hammer2_xop_head_t	head;
	hammer2_key_t		lbase;
	struct buf		*bp;
};

typedef struct hammer2_xop_readdir hammer2_xop_readdir_t;
typedef struct hammer2_xop_nresolve hammer2_xop_nresolve_t;
typedef struct hammer2_xop_lookup hammer2_xop_lookup_t;
typedef struct hammer2_xop_bmap hammer2_xop_bmap_t;
typedef struct hammer2_xop_strategy hammer2_xop_strategy_t;

union hammer2_xop {
	hammer2_xop_head_t	head;
	hammer2_xop_readdir_t	xop_readdir;
	hammer2_xop_nresolve_t	xop_nresolve;
	hammer2_xop_lookup_t	xop_lookup;
	hammer2_xop_bmap_t	xop_bmap;
	hammer2_xop_strategy_t	xop_strategy;
};

/*
 * Device vnode management structure.
 */
struct hammer2_devvp {
	TAILQ_ENTRY(hammer2_devvp) entry;
	struct vnode		*devvp;		/* device vnode */
	char			*path;		/* device vnode path */
	int			open;		/* 1 if devvp open */
};

typedef struct hammer2_devvp hammer2_devvp_t;


/*
 * Volume management structure.
 */
struct hammer2_volume {
	hammer2_devvp_t		*dev;		/* device vnode management */
	hammer2_off_t		offset;		/* offset within volumes */
	hammer2_off_t		size;		/* volume size */
	int			id;		/* volume id */
};

typedef struct hammer2_volume hammer2_volume_t;

/*
 * Global (per partition) management structure, represents a hard block
 * device.  Typically referenced by hammer2_chain structures when applicable.
 *
 * Note that a single hammer2_dev can be indirectly tied to multiple system
 * mount points.  There is no direct relationship.  System mounts are
 * per-cluster-id, not per-block-device, and a single hard mount might contain
 * many PFSs.
 */
struct hammer2_dev {
	TAILQ_ENTRY(hammer2_dev) mntentry;	/* hammer2_mntlist */
	hammer2_devvp_list_t	devvp_list;	/* list of device vnodes including *devvp */
	hammer2_io_tree_t	iotree;
	hammer2_mtx_t		iotree_lock;	/* iotree, iolruq access */
	hammer2_pfs_t		*spmp;		/* super-root pmp for transactions */
	struct vnode		*devvp;		/* device vnode for root volume */
	hammer2_chain_t		vchain;		/* anchor chain (topology) */
	hammer2_volume_data_t	voldata;
	hammer2_volume_t	volumes[HAMMER2_MAX_VOLUMES]; /* list of volumes */
	hammer2_off_t		total_size;	/* total size of volumes */
	uint32_t		hflags;		/* HMNT2 flags applicable to device */
	int			mount_count;	/* number of actively mounted PFSs */
	int			nvolumes;	/* total number of volumes */
	int			iofree_count;
};

/*
 * Per-cluster management structure.  This structure will be tied to a
 * system mount point if the system is mounting the PFS.
 *
 * This structure is also used to represent the super-root that hangs off
 * of a hard mount point.  The super-root is not really a cluster element.
 * In this case the spmp_hmp field will be non-NULL.  It's just easier to do
 * this than to special case super-root manipulation in the hammer2_chain*
 * code as being only hammer2_dev-related.
 *
 * WARNING! The chains making up pfs->iroot's cluster are accounted for in
 *	    hammer2_dev->mount_count when the pfs is associated with a mount
 *	    point.
 */
struct hammer2_pfs {
	TAILQ_ENTRY(hammer2_pfs) mntentry;	/* hammer2_pfslist */
	hammer2_inode_tree_t	inum_tree;	/* (not applicable to spmp) */
	hammer2_chain_list_t	lru_list;	/* basis for LRU tests */
	hammer2_ipdep_list_t	*ipdep_lists;	/* inode dependencies for XOP */
	hammer2_spin_t		inum_spin;	/* inumber lookup */
	hammer2_spin_t		lru_spin;
	hammer2_mtx_t		xop_lock;
	struct mount		*mp;
	struct uuid		pfs_clid;
	hammer2_inode_t		*iroot;		/* PFS root inode */
	hammer2_dev_t		*spmp_hmp;	/* only if super-root pmp */
	hammer2_dev_t		*force_local;	/* only if 'local' mount */
	hammer2_dev_t		*pfs_hmps[HAMMER2_MAXCLUSTER];
	char			*pfs_names[HAMMER2_MAXCLUSTER];
	uint8_t			pfs_types[HAMMER2_MAXCLUSTER];
	int			flags;		/* for HAMMER2_PMPF_xxx */
	int			lru_count;	/* #of chains on LRU */
	unsigned long		ipdep_mask;
	char			mntpt[128];
};

#define HAMMER2_PMPF_SPMP	0x00000001
#define HAMMER2_PMPF_WAITING	0x10000000

#define HAMMER2_IHASH_SIZE	16

/*
 * NOTE: The LRU list contains at least all the chains with refs == 0
 *	 that can be recycled, and may contain additional chains which
 *	 cannot.
 */
#define HAMMER2_LRU_LIMIT	4096

#define HAMMER2_CHECK_NULL	0x00000001

#define MPTOPMP(mp)	((hammer2_pfs_t *)(mp)->mnt_data)
#define VTOI(vp)	((hammer2_inode_t *)(vp)->v_data)

MALLOC_DECLARE(M_HAMMER2);
extern uma_zone_t zone_buffer_read;
extern uma_zone_t zone_xops;

extern int hammer2_cluster_meta_read;
extern int hammer2_cluster_data_read;
extern long hammer2_inode_allocs;
extern long hammer2_chain_allocs;
extern long hammer2_dio_allocs;
extern int hammer2_dio_limit;

extern struct vop_vector hammer2_vnodeops;
extern struct vop_vector hammer2_fifoops;

extern hammer2_xop_desc_t hammer2_readdir_desc;
extern hammer2_xop_desc_t hammer2_nresolve_desc;
extern hammer2_xop_desc_t hammer2_lookup_desc;
extern hammer2_xop_desc_t hammer2_bmap_desc;
extern hammer2_xop_desc_t hammer2_strategy_read_desc;

/* hammer2_admin.c */
void *hammer2_xop_alloc(hammer2_inode_t *);
void hammer2_xop_setname(hammer2_xop_head_t *, const char *, size_t);
void hammer2_xop_start(hammer2_xop_head_t *, hammer2_xop_desc_t *);
void hammer2_xop_retire(hammer2_xop_head_t *, uint32_t);
int hammer2_xop_feed(hammer2_xop_head_t *, hammer2_chain_t *, int, int);
int hammer2_xop_collect(hammer2_xop_head_t *, int);

/* hammer2_chain.c */
void hammer2_chain_init(hammer2_chain_t *);
void hammer2_chain_ref(hammer2_chain_t *);
void hammer2_chain_ref_hold(hammer2_chain_t *);
void hammer2_chain_drop(hammer2_chain_t *);
void hammer2_chain_unhold(hammer2_chain_t *);
void hammer2_chain_drop_unhold(hammer2_chain_t *);
void hammer2_chain_rehold(hammer2_chain_t *);
int hammer2_chain_lock(hammer2_chain_t *, int);
void hammer2_chain_unlock(hammer2_chain_t *);
hammer2_chain_t *hammer2_chain_lookup_init(hammer2_chain_t *, int);
void hammer2_chain_lookup_done(hammer2_chain_t *);
hammer2_chain_t *hammer2_chain_lookup(hammer2_chain_t **, hammer2_key_t *,
    hammer2_key_t, hammer2_key_t, int *, int);
hammer2_chain_t *hammer2_chain_next(hammer2_chain_t **, hammer2_chain_t *,
    hammer2_key_t *, hammer2_key_t, hammer2_key_t, int *, int);
int hammer2_chain_inode_find(hammer2_pfs_t *, hammer2_key_t, int, int,
    hammer2_chain_t **, hammer2_chain_t **);
int hammer2_chain_dirent_test(const hammer2_chain_t *, const char *, size_t);
void hammer2_dump_chain(hammer2_chain_t *, int, int, int *, char, unsigned int);

/* hammer2_cluster.c */
uint8_t hammer2_cluster_type(const hammer2_cluster_t *);
void hammer2_cluster_bref(const hammer2_cluster_t *, hammer2_blockref_t *);
void hammer2_dummy_xop_from_chain(hammer2_xop_head_t *, hammer2_chain_t *);
void hammer2_cluster_unhold(hammer2_cluster_t *);
void hammer2_cluster_rehold(hammer2_cluster_t *);
int hammer2_cluster_check(hammer2_cluster_t *, hammer2_key_t, int);

/* hammer2_inode.c */
void hammer2_inode_lock(hammer2_inode_t *, int);
void hammer2_inode_unlock(hammer2_inode_t *);
hammer2_chain_t *hammer2_inode_chain(hammer2_inode_t *, int, int);
hammer2_chain_t *hammer2_inode_chain_and_parent(hammer2_inode_t *, int,
    hammer2_chain_t **, int);
hammer2_inode_t *hammer2_inode_lookup(hammer2_pfs_t *, hammer2_tid_t);
void hammer2_inode_ref(hammer2_inode_t *);
void hammer2_inode_drop(hammer2_inode_t *);
int hammer2_igetv(hammer2_inode_t *, int, struct vnode **);
hammer2_inode_t *hammer2_inode_get(hammer2_pfs_t *, hammer2_xop_head_t *,
    hammer2_tid_t, int);
hammer2_key_t hammer2_inode_data_count(const hammer2_inode_t *);
hammer2_key_t hammer2_inode_inode_count(const hammer2_inode_t *);

/* hammer2_io.c */
hammer2_io_t *hammer2_io_getblk(hammer2_dev_t *, int, off_t, int, int);
void hammer2_io_putblk(hammer2_io_t **);
void hammer2_io_cleanup(hammer2_dev_t *, hammer2_io_tree_t *);
char *hammer2_io_data(hammer2_io_t *, off_t);
int hammer2_io_bread(hammer2_dev_t *, int, off_t, int, hammer2_io_t **);
void hammer2_io_bqrelse(hammer2_io_t **);

/* hammer2_ioctl.c */
int hammer2_ioctl_impl(hammer2_inode_t *, unsigned long, void *, int,
    struct ucred *);

/* hammer2_ondisk.c */
int hammer2_open_devvp(struct mount *, const hammer2_devvp_list_t *);
int hammer2_close_devvp(const hammer2_devvp_list_t *);
int hammer2_init_devvp(const struct mount *, const char *,
    hammer2_devvp_list_t *);
void hammer2_cleanup_devvp(hammer2_devvp_list_t *);
int hammer2_init_volumes(const hammer2_devvp_list_t *, hammer2_volume_t *,
    hammer2_volume_data_t *, struct vnode **);
hammer2_volume_t *hammer2_get_volume(hammer2_dev_t *, hammer2_off_t);

/* hammer2_strategy.c */
int hammer2_strategy(struct vop_strategy_args *);
void hammer2_xop_strategy_read(hammer2_xop_t *, int);

/* hammer2_subr.c */
int hammer2_get_dtype(uint8_t);
int hammer2_get_vtype(uint8_t);
void hammer2_time_to_timespec(uint64_t, struct timespec *);
uint32_t hammer2_to_unix_xid(const struct uuid *);
hammer2_key_t hammer2_dirhash(const unsigned char *, size_t);
int hammer2_calc_logical(hammer2_inode_t *, hammer2_off_t, hammer2_key_t *,
    hammer2_key_t *);
int hammer2_get_logical(void);
const char *hammer2_breftype_to_str(uint8_t);

/* hammer2_xops.c */
void hammer2_xop_readdir(hammer2_xop_t *, int);
void hammer2_xop_nresolve(hammer2_xop_t *, int);
void hammer2_xop_lookup(hammer2_xop_t *, int);
void hammer2_xop_bmap(hammer2_xop_t *, int);

static __inline int
hammer2_error_to_errno(int error)
{
	if (!error)
		return (0);
	else if (error & HAMMER2_ERROR_EIO)
		return (EIO);
	else if (error & HAMMER2_ERROR_CHECK)
		return (EDOM);
	else if (error & HAMMER2_ERROR_ENOENT)
		return (ENOENT);
	else if (error & HAMMER2_ERROR_EAGAIN)
		return (EAGAIN);
	else if (error & HAMMER2_ERROR_ABORTED)
		return (EINTR);
	else
		return (EDOM);
}

static __inline const hammer2_media_data_t *
hammer2_xop_gdata(hammer2_xop_head_t *xop)
{
	hammer2_chain_t *focus = xop->cluster.focus;
	const void *data;

	if (focus->dio) {
		if ((xop->focus_dio = focus->dio) != NULL)
			atomic_add_32(&xop->focus_dio->refs, 1);
		data = focus->data;
	} else {
		data = focus->data;
	}

	return (data);
}

static __inline void
hammer2_xop_pdata(hammer2_xop_head_t *xop)
{
	if (xop->focus_dio)
		hammer2_io_putblk(&xop->focus_dio);
}

static __inline void
hammer2_assert_cluster(const hammer2_cluster_t *cluster)
{
	/* Currently a valid cluster can only have 1 nchains. */
	KASSERT(cluster->nchains == 1,
	    ("unexpected cluster nchains %d", cluster->nchains));
}

static __inline uint32_t
hammer2_icrc32(const void *buf, size_t size)
{
	return (~calculate_crc32c(-1, buf, size));
}

static __inline uint32_t
hammer2_icrc32c(const void *buf, size_t size, uint32_t ocrc)
{
	return (~calculate_crc32c(~ocrc, buf, size));
}
#endif /* !_FS_HAMMER2_HAMMER2_H_ */
