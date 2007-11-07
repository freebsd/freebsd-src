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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * DVA-based Adjustable Replacement Cache
 *
 * While much of the theory of operation used here is
 * based on the self-tuning, low overhead replacement cache
 * presented by Megiddo and Modha at FAST 2003, there are some
 * significant differences:
 *
 * 1. The Megiddo and Modha model assumes any page is evictable.
 * Pages in its cache cannot be "locked" into memory.  This makes
 * the eviction algorithm simple: evict the last page in the list.
 * This also make the performance characteristics easy to reason
 * about.  Our cache is not so simple.  At any given moment, some
 * subset of the blocks in the cache are un-evictable because we
 * have handed out a reference to them.  Blocks are only evictable
 * when there are no external references active.  This makes
 * eviction far more problematic:  we choose to evict the evictable
 * blocks that are the "lowest" in the list.
 *
 * There are times when it is not possible to evict the requested
 * space.  In these circumstances we are unable to adjust the cache
 * size.  To prevent the cache growing unbounded at these times we
 * implement a "cache throttle" that slowes the flow of new data
 * into the cache until we can make space avaiable.
 *
 * 2. The Megiddo and Modha model assumes a fixed cache size.
 * Pages are evicted when the cache is full and there is a cache
 * miss.  Our model has a variable sized cache.  It grows with
 * high use, but also tries to react to memory preasure from the
 * operating system: decreasing its size when system memory is
 * tight.
 *
 * 3. The Megiddo and Modha model assumes a fixed page size. All
 * elements of the cache are therefor exactly the same size.  So
 * when adjusting the cache size following a cache miss, its simply
 * a matter of choosing a single page to evict.  In our model, we
 * have variable sized cache blocks (rangeing from 512 bytes to
 * 128K bytes).  We therefor choose a set of blocks to evict to make
 * space for a cache miss that approximates as closely as possible
 * the space used by the new block.
 *
 * See also:  "ARC: A Self-Tuning, Low Overhead Replacement Cache"
 * by N. Megiddo & D. Modha, FAST 2003
 */

/*
 * The locking model:
 *
 * A new reference to a cache buffer can be obtained in two
 * ways: 1) via a hash table lookup using the DVA as a key,
 * or 2) via one of the ARC lists.  The arc_read() inerface
 * uses method 1, while the internal arc algorithms for
 * adjusting the cache use method 2.  We therefor provide two
 * types of locks: 1) the hash table lock array, and 2) the
 * arc list locks.
 *
 * Buffers do not have their own mutexs, rather they rely on the
 * hash table mutexs for the bulk of their protection (i.e. most
 * fields in the arc_buf_hdr_t are protected by these mutexs).
 *
 * buf_hash_find() returns the appropriate mutex (held) when it
 * locates the requested buffer in the hash table.  It returns
 * NULL for the mutex if the buffer was not in the table.
 *
 * buf_hash_remove() expects the appropriate hash mutex to be
 * already held before it is invoked.
 *
 * Each arc state also has a mutex which is used to protect the
 * buffer list associated with the state.  When attempting to
 * obtain a hash table lock while holding an arc list lock you
 * must use: mutex_tryenter() to avoid deadlock.  Also note that
 * the active state mutex must be held before the ghost state mutex.
 *
 * Arc buffers may have an associated eviction callback function.
 * This function will be invoked prior to removing the buffer (e.g.
 * in arc_do_user_evicts()).  Note however that the data associated
 * with the buffer may be evicted prior to the callback.  The callback
 * must be made with *no locks held* (to prevent deadlock).  Additionally,
 * the users of callbacks must ensure that their private data is
 * protected from simultaneous callbacks from arc_buf_evict()
 * and arc_do_user_evicts().
 *
 * Note that the majority of the performance stats are manipulated
 * with atomic operations.
 */

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/refcount.h>
#ifdef _KERNEL
#include <sys/dnlc.h>
#endif
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/sdt.h>

static kmutex_t		arc_reclaim_thr_lock;
static kcondvar_t	arc_reclaim_thr_cv;	/* used to signal reclaim thr */
static uint8_t		arc_thread_exit;

#define	ARC_REDUCE_DNLC_PERCENT	3
uint_t arc_reduce_dnlc_percent = ARC_REDUCE_DNLC_PERCENT;

typedef enum arc_reclaim_strategy {
	ARC_RECLAIM_AGGR,		/* Aggressive reclaim strategy */
	ARC_RECLAIM_CONS		/* Conservative reclaim strategy */
} arc_reclaim_strategy_t;

/* number of seconds before growing cache again */
static int		arc_grow_retry = 60;

/*
 * minimum lifespan of a prefetch block in clock ticks
 * (initialized in arc_init())
 */
static int		arc_min_prefetch_lifespan;

static int arc_dead;

/*
 * These tunables are for performance analysis.
 */
u_long zfs_arc_max;
u_long zfs_arc_min;
TUNABLE_ULONG("vfs.zfs.arc_max", &zfs_arc_max);
TUNABLE_ULONG("vfs.zfs.arc_min", &zfs_arc_min);
SYSCTL_DECL(_vfs_zfs);
SYSCTL_ULONG(_vfs_zfs, OID_AUTO, arc_max, CTLFLAG_RDTUN, &zfs_arc_max, 0,
    "Maximum ARC size");
SYSCTL_ULONG(_vfs_zfs, OID_AUTO, arc_min, CTLFLAG_RDTUN, &zfs_arc_min, 0,
    "Minimum ARC size");

/*
 * Note that buffers can be on one of 5 states:
 *	ARC_anon	- anonymous (discussed below)
 *	ARC_mru		- recently used, currently cached
 *	ARC_mru_ghost	- recentely used, no longer in cache
 *	ARC_mfu		- frequently used, currently cached
 *	ARC_mfu_ghost	- frequently used, no longer in cache
 * When there are no active references to the buffer, they
 * are linked onto one of the lists in arc.  These are the
 * only buffers that can be evicted or deleted.
 *
 * Anonymous buffers are buffers that are not associated with
 * a DVA.  These are buffers that hold dirty block copies
 * before they are written to stable storage.  By definition,
 * they are "ref'd" and are considered part of arc_mru
 * that cannot be freed.  Generally, they will aquire a DVA
 * as they are written and migrate onto the arc_mru list.
 */

typedef struct arc_state {
	list_t	arcs_list;	/* linked list of evictable buffer in state */
	uint64_t arcs_lsize;	/* total size of buffers in the linked list */
	uint64_t arcs_size;	/* total size of all buffers in this state */
	kmutex_t arcs_mtx;
} arc_state_t;

/* The 5 states: */
static arc_state_t ARC_anon;
static arc_state_t ARC_mru;
static arc_state_t ARC_mru_ghost;
static arc_state_t ARC_mfu;
static arc_state_t ARC_mfu_ghost;

typedef struct arc_stats {
	kstat_named_t arcstat_hits;
	kstat_named_t arcstat_misses;
	kstat_named_t arcstat_demand_data_hits;
	kstat_named_t arcstat_demand_data_misses;
	kstat_named_t arcstat_demand_metadata_hits;
	kstat_named_t arcstat_demand_metadata_misses;
	kstat_named_t arcstat_prefetch_data_hits;
	kstat_named_t arcstat_prefetch_data_misses;
	kstat_named_t arcstat_prefetch_metadata_hits;
	kstat_named_t arcstat_prefetch_metadata_misses;
	kstat_named_t arcstat_mru_hits;
	kstat_named_t arcstat_mru_ghost_hits;
	kstat_named_t arcstat_mfu_hits;
	kstat_named_t arcstat_mfu_ghost_hits;
	kstat_named_t arcstat_deleted;
	kstat_named_t arcstat_recycle_miss;
	kstat_named_t arcstat_mutex_miss;
	kstat_named_t arcstat_evict_skip;
	kstat_named_t arcstat_hash_elements;
	kstat_named_t arcstat_hash_elements_max;
	kstat_named_t arcstat_hash_collisions;
	kstat_named_t arcstat_hash_chains;
	kstat_named_t arcstat_hash_chain_max;
	kstat_named_t arcstat_p;
	kstat_named_t arcstat_c;
	kstat_named_t arcstat_c_min;
	kstat_named_t arcstat_c_max;
	kstat_named_t arcstat_size;
} arc_stats_t;

static arc_stats_t arc_stats = {
	{ "hits",			KSTAT_DATA_UINT64 },
	{ "misses",			KSTAT_DATA_UINT64 },
	{ "demand_data_hits",		KSTAT_DATA_UINT64 },
	{ "demand_data_misses",		KSTAT_DATA_UINT64 },
	{ "demand_metadata_hits",	KSTAT_DATA_UINT64 },
	{ "demand_metadata_misses",	KSTAT_DATA_UINT64 },
	{ "prefetch_data_hits",		KSTAT_DATA_UINT64 },
	{ "prefetch_data_misses",	KSTAT_DATA_UINT64 },
	{ "prefetch_metadata_hits",	KSTAT_DATA_UINT64 },
	{ "prefetch_metadata_misses",	KSTAT_DATA_UINT64 },
	{ "mru_hits",			KSTAT_DATA_UINT64 },
	{ "mru_ghost_hits",		KSTAT_DATA_UINT64 },
	{ "mfu_hits",			KSTAT_DATA_UINT64 },
	{ "mfu_ghost_hits",		KSTAT_DATA_UINT64 },
	{ "deleted",			KSTAT_DATA_UINT64 },
	{ "recycle_miss",		KSTAT_DATA_UINT64 },
	{ "mutex_miss",			KSTAT_DATA_UINT64 },
	{ "evict_skip",			KSTAT_DATA_UINT64 },
	{ "hash_elements",		KSTAT_DATA_UINT64 },
	{ "hash_elements_max",		KSTAT_DATA_UINT64 },
	{ "hash_collisions",		KSTAT_DATA_UINT64 },
	{ "hash_chains",		KSTAT_DATA_UINT64 },
	{ "hash_chain_max",		KSTAT_DATA_UINT64 },
	{ "p",				KSTAT_DATA_UINT64 },
	{ "c",				KSTAT_DATA_UINT64 },
	{ "c_min",			KSTAT_DATA_UINT64 },
	{ "c_max",			KSTAT_DATA_UINT64 },
	{ "size",			KSTAT_DATA_UINT64 }
};

#define	ARCSTAT(stat)	(arc_stats.stat.value.ui64)

#define	ARCSTAT_INCR(stat, val) \
	atomic_add_64(&arc_stats.stat.value.ui64, (val));

#define	ARCSTAT_BUMP(stat) 	ARCSTAT_INCR(stat, 1)
#define	ARCSTAT_BUMPDOWN(stat)	ARCSTAT_INCR(stat, -1)

#define	ARCSTAT_MAX(stat, val) {					\
	uint64_t m;							\
	while ((val) > (m = arc_stats.stat.value.ui64) &&		\
	    (m != atomic_cas_64(&arc_stats.stat.value.ui64, m, (val))))	\
		continue;						\
}

#define	ARCSTAT_MAXSTAT(stat) \
	ARCSTAT_MAX(stat##_max, arc_stats.stat.value.ui64)

/*
 * We define a macro to allow ARC hits/misses to be easily broken down by
 * two separate conditions, giving a total of four different subtypes for
 * each of hits and misses (so eight statistics total).
 */
#define	ARCSTAT_CONDSTAT(cond1, stat1, notstat1, cond2, stat2, notstat2, stat) \
	if (cond1) {							\
		if (cond2) {						\
			ARCSTAT_BUMP(arcstat_##stat1##_##stat2##_##stat); \
		} else {						\
			ARCSTAT_BUMP(arcstat_##stat1##_##notstat2##_##stat); \
		}							\
	} else {							\
		if (cond2) {						\
			ARCSTAT_BUMP(arcstat_##notstat1##_##stat2##_##stat); \
		} else {						\
			ARCSTAT_BUMP(arcstat_##notstat1##_##notstat2##_##stat);\
		}							\
	}

kstat_t			*arc_ksp;
static arc_state_t 	*arc_anon;
static arc_state_t	*arc_mru;
static arc_state_t	*arc_mru_ghost;
static arc_state_t	*arc_mfu;
static arc_state_t	*arc_mfu_ghost;

/*
 * There are several ARC variables that are critical to export as kstats --
 * but we don't want to have to grovel around in the kstat whenever we wish to
 * manipulate them.  For these variables, we therefore define them to be in
 * terms of the statistic variable.  This assures that we are not introducing
 * the possibility of inconsistency by having shadow copies of the variables,
 * while still allowing the code to be readable.
 */
#define	arc_size	ARCSTAT(arcstat_size)	/* actual total arc size */
#define	arc_p		ARCSTAT(arcstat_p)	/* target size of MRU */
#define	arc_c		ARCSTAT(arcstat_c)	/* target size of cache */
#define	arc_c_min	ARCSTAT(arcstat_c_min)	/* min target cache size */
#define	arc_c_max	ARCSTAT(arcstat_c_max)	/* max target cache size */

static int		arc_no_grow;	/* Don't try to grow cache size */
static uint64_t		arc_tempreserve;

typedef struct arc_callback arc_callback_t;

struct arc_callback {
	void			*acb_private;
	arc_done_func_t		*acb_done;
	arc_byteswap_func_t	*acb_byteswap;
	arc_buf_t		*acb_buf;
	zio_t			*acb_zio_dummy;
	arc_callback_t		*acb_next;
};

typedef struct arc_write_callback arc_write_callback_t;

struct arc_write_callback {
	void		*awcb_private;
	arc_done_func_t	*awcb_ready;
	arc_done_func_t	*awcb_done;
	arc_buf_t	*awcb_buf;
};

struct arc_buf_hdr {
	/* protected by hash lock */
	dva_t			b_dva;
	uint64_t		b_birth;
	uint64_t		b_cksum0;

	kmutex_t		b_freeze_lock;
	zio_cksum_t		*b_freeze_cksum;

	arc_buf_hdr_t		*b_hash_next;
	arc_buf_t		*b_buf;
	uint32_t		b_flags;
	uint32_t		b_datacnt;

	arc_callback_t		*b_acb;
	kcondvar_t		b_cv;

	/* immutable */
	arc_buf_contents_t	b_type;
	uint64_t		b_size;
	spa_t			*b_spa;

	/* protected by arc state mutex */
	arc_state_t		*b_state;
	list_node_t		b_arc_node;

	/* updated atomically */
	clock_t			b_arc_access;

	/* self protecting */
	refcount_t		b_refcnt;
};

static arc_buf_t *arc_eviction_list;
static kmutex_t arc_eviction_mtx;
static arc_buf_hdr_t arc_eviction_hdr;
static void arc_get_data_buf(arc_buf_t *buf);
static void arc_access(arc_buf_hdr_t *buf, kmutex_t *hash_lock);

#define	GHOST_STATE(state)	\
	((state) == arc_mru_ghost || (state) == arc_mfu_ghost)

/*
 * Private ARC flags.  These flags are private ARC only flags that will show up
 * in b_flags in the arc_hdr_buf_t.  Some flags are publicly declared, and can
 * be passed in as arc_flags in things like arc_read.  However, these flags
 * should never be passed and should only be set by ARC code.  When adding new
 * public flags, make sure not to smash the private ones.
 */

#define	ARC_IN_HASH_TABLE	(1 << 9)	/* this buffer is hashed */
#define	ARC_IO_IN_PROGRESS	(1 << 10)	/* I/O in progress for buf */
#define	ARC_IO_ERROR		(1 << 11)	/* I/O failed for buf */
#define	ARC_FREED_IN_READ	(1 << 12)	/* buf freed while in read */
#define	ARC_BUF_AVAILABLE	(1 << 13)	/* block not in active use */
#define	ARC_INDIRECT		(1 << 14)	/* this is an indirect block */

#define	HDR_IN_HASH_TABLE(hdr)	((hdr)->b_flags & ARC_IN_HASH_TABLE)
#define	HDR_IO_IN_PROGRESS(hdr)	((hdr)->b_flags & ARC_IO_IN_PROGRESS)
#define	HDR_IO_ERROR(hdr)	((hdr)->b_flags & ARC_IO_ERROR)
#define	HDR_FREED_IN_READ(hdr)	((hdr)->b_flags & ARC_FREED_IN_READ)
#define	HDR_BUF_AVAILABLE(hdr)	((hdr)->b_flags & ARC_BUF_AVAILABLE)

/*
 * Hash table routines
 */

#define	HT_LOCK_PAD	128

struct ht_lock {
	kmutex_t	ht_lock;
#ifdef _KERNEL
	unsigned char	pad[(HT_LOCK_PAD - sizeof (kmutex_t))];
#endif
};

#define	BUF_LOCKS 256
typedef struct buf_hash_table {
	uint64_t ht_mask;
	arc_buf_hdr_t **ht_table;
	struct ht_lock ht_locks[BUF_LOCKS];
} buf_hash_table_t;

static buf_hash_table_t buf_hash_table;

#define	BUF_HASH_INDEX(spa, dva, birth) \
	(buf_hash(spa, dva, birth) & buf_hash_table.ht_mask)
#define	BUF_HASH_LOCK_NTRY(idx) (buf_hash_table.ht_locks[idx & (BUF_LOCKS-1)])
#define	BUF_HASH_LOCK(idx)	(&(BUF_HASH_LOCK_NTRY(idx).ht_lock))
#define	HDR_LOCK(buf) \
	(BUF_HASH_LOCK(BUF_HASH_INDEX(buf->b_spa, &buf->b_dva, buf->b_birth)))

uint64_t zfs_crc64_table[256];

static uint64_t
buf_hash(spa_t *spa, dva_t *dva, uint64_t birth)
{
	uintptr_t spav = (uintptr_t)spa;
	uint8_t *vdva = (uint8_t *)dva;
	uint64_t crc = -1ULL;
	int i;

	ASSERT(zfs_crc64_table[128] == ZFS_CRC64_POLY);

	for (i = 0; i < sizeof (dva_t); i++)
		crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ vdva[i]) & 0xFF];

	crc ^= (spav>>8) ^ birth;

	return (crc);
}

#define	BUF_EMPTY(buf)						\
	((buf)->b_dva.dva_word[0] == 0 &&			\
	(buf)->b_dva.dva_word[1] == 0 &&			\
	(buf)->b_birth == 0)

#define	BUF_EQUAL(spa, dva, birth, buf)				\
	((buf)->b_dva.dva_word[0] == (dva)->dva_word[0]) &&	\
	((buf)->b_dva.dva_word[1] == (dva)->dva_word[1]) &&	\
	((buf)->b_birth == birth) && ((buf)->b_spa == spa)

static arc_buf_hdr_t *
buf_hash_find(spa_t *spa, dva_t *dva, uint64_t birth, kmutex_t **lockp)
{
	uint64_t idx = BUF_HASH_INDEX(spa, dva, birth);
	kmutex_t *hash_lock = BUF_HASH_LOCK(idx);
	arc_buf_hdr_t *buf;

	mutex_enter(hash_lock);
	for (buf = buf_hash_table.ht_table[idx]; buf != NULL;
	    buf = buf->b_hash_next) {
		if (BUF_EQUAL(spa, dva, birth, buf)) {
			*lockp = hash_lock;
			return (buf);
		}
	}
	mutex_exit(hash_lock);
	*lockp = NULL;
	return (NULL);
}

/*
 * Insert an entry into the hash table.  If there is already an element
 * equal to elem in the hash table, then the already existing element
 * will be returned and the new element will not be inserted.
 * Otherwise returns NULL.
 */
static arc_buf_hdr_t *
buf_hash_insert(arc_buf_hdr_t *buf, kmutex_t **lockp)
{
	uint64_t idx = BUF_HASH_INDEX(buf->b_spa, &buf->b_dva, buf->b_birth);
	kmutex_t *hash_lock = BUF_HASH_LOCK(idx);
	arc_buf_hdr_t *fbuf;
	uint32_t i;

	ASSERT(!HDR_IN_HASH_TABLE(buf));
	*lockp = hash_lock;
	mutex_enter(hash_lock);
	for (fbuf = buf_hash_table.ht_table[idx], i = 0; fbuf != NULL;
	    fbuf = fbuf->b_hash_next, i++) {
		if (BUF_EQUAL(buf->b_spa, &buf->b_dva, buf->b_birth, fbuf))
			return (fbuf);
	}

	buf->b_hash_next = buf_hash_table.ht_table[idx];
	buf_hash_table.ht_table[idx] = buf;
	buf->b_flags |= ARC_IN_HASH_TABLE;

	/* collect some hash table performance data */
	if (i > 0) {
		ARCSTAT_BUMP(arcstat_hash_collisions);
		if (i == 1)
			ARCSTAT_BUMP(arcstat_hash_chains);

		ARCSTAT_MAX(arcstat_hash_chain_max, i);
	}

	ARCSTAT_BUMP(arcstat_hash_elements);
	ARCSTAT_MAXSTAT(arcstat_hash_elements);

	return (NULL);
}

static void
buf_hash_remove(arc_buf_hdr_t *buf)
{
	arc_buf_hdr_t *fbuf, **bufp;
	uint64_t idx = BUF_HASH_INDEX(buf->b_spa, &buf->b_dva, buf->b_birth);

	ASSERT(MUTEX_HELD(BUF_HASH_LOCK(idx)));
	ASSERT(HDR_IN_HASH_TABLE(buf));

	bufp = &buf_hash_table.ht_table[idx];
	while ((fbuf = *bufp) != buf) {
		ASSERT(fbuf != NULL);
		bufp = &fbuf->b_hash_next;
	}
	*bufp = buf->b_hash_next;
	buf->b_hash_next = NULL;
	buf->b_flags &= ~ARC_IN_HASH_TABLE;

	/* collect some hash table performance data */
	ARCSTAT_BUMPDOWN(arcstat_hash_elements);

	if (buf_hash_table.ht_table[idx] &&
	    buf_hash_table.ht_table[idx]->b_hash_next == NULL)
		ARCSTAT_BUMPDOWN(arcstat_hash_chains);
}

/*
 * Global data structures and functions for the buf kmem cache.
 */
static kmem_cache_t *hdr_cache;
static kmem_cache_t *buf_cache;

static void
buf_fini(void)
{
	int i;

	kmem_free(buf_hash_table.ht_table,
	    (buf_hash_table.ht_mask + 1) * sizeof (void *));
	for (i = 0; i < BUF_LOCKS; i++)
		mutex_destroy(&buf_hash_table.ht_locks[i].ht_lock);
	kmem_cache_destroy(hdr_cache);
	kmem_cache_destroy(buf_cache);
}

/*
 * Constructor callback - called when the cache is empty
 * and a new buf is requested.
 */
/* ARGSUSED */
static int
hdr_cons(void *vbuf, void *unused, int kmflag)
{
	arc_buf_hdr_t *buf = vbuf;

	bzero(buf, sizeof (arc_buf_hdr_t));
	refcount_create(&buf->b_refcnt);
	cv_init(&buf->b_cv, NULL, CV_DEFAULT, NULL);
	return (0);
}

/*
 * Destructor callback - called when a cached buf is
 * no longer required.
 */
/* ARGSUSED */
static void
hdr_dest(void *vbuf, void *unused)
{
	arc_buf_hdr_t *buf = vbuf;

	refcount_destroy(&buf->b_refcnt);
	cv_destroy(&buf->b_cv);
}

/*
 * Reclaim callback -- invoked when memory is low.
 */
/* ARGSUSED */
static void
hdr_recl(void *unused)
{
	dprintf("hdr_recl called\n");
	/*
	 * umem calls the reclaim func when we destroy the buf cache,
	 * which is after we do arc_fini().
	 */
	if (!arc_dead)
		cv_signal(&arc_reclaim_thr_cv);
}

static void
buf_init(void)
{
	uint64_t *ct;
	uint64_t hsize = 1ULL << 12;
	int i, j;

	/*
	 * The hash table is big enough to fill all of physical memory
	 * with an average 64K block size.  The table will take up
	 * totalmem*sizeof(void*)/64K (eg. 128KB/GB with 8-byte pointers).
	 */
	while (hsize * 65536 < (uint64_t)physmem * PAGESIZE)
		hsize <<= 1;
retry:
	buf_hash_table.ht_mask = hsize - 1;
	buf_hash_table.ht_table =
	    kmem_zalloc(hsize * sizeof (void*), KM_NOSLEEP);
	if (buf_hash_table.ht_table == NULL) {
		ASSERT(hsize > (1ULL << 8));
		hsize >>= 1;
		goto retry;
	}

	hdr_cache = kmem_cache_create("arc_buf_hdr_t", sizeof (arc_buf_hdr_t),
	    0, hdr_cons, hdr_dest, hdr_recl, NULL, NULL, 0);
	buf_cache = kmem_cache_create("arc_buf_t", sizeof (arc_buf_t),
	    0, NULL, NULL, NULL, NULL, NULL, 0);

	for (i = 0; i < 256; i++)
		for (ct = zfs_crc64_table + i, *ct = i, j = 8; j > 0; j--)
			*ct = (*ct >> 1) ^ (-(*ct & 1) & ZFS_CRC64_POLY);

	for (i = 0; i < BUF_LOCKS; i++) {
		mutex_init(&buf_hash_table.ht_locks[i].ht_lock,
		    NULL, MUTEX_DEFAULT, NULL);
	}
}

#define	ARC_MINTIME	(hz>>4) /* 62 ms */

static void
arc_cksum_verify(arc_buf_t *buf)
{
	zio_cksum_t zc;

	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

	mutex_enter(&buf->b_hdr->b_freeze_lock);
	if (buf->b_hdr->b_freeze_cksum == NULL ||
	    (buf->b_hdr->b_flags & ARC_IO_ERROR)) {
		mutex_exit(&buf->b_hdr->b_freeze_lock);
		return;
	}
	fletcher_2_native(buf->b_data, buf->b_hdr->b_size, &zc);
	if (!ZIO_CHECKSUM_EQUAL(*buf->b_hdr->b_freeze_cksum, zc))
		panic("buffer modified while frozen!");
	mutex_exit(&buf->b_hdr->b_freeze_lock);
}

static void
arc_cksum_compute(arc_buf_t *buf)
{
	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

	mutex_enter(&buf->b_hdr->b_freeze_lock);
	if (buf->b_hdr->b_freeze_cksum != NULL) {
		mutex_exit(&buf->b_hdr->b_freeze_lock);
		return;
	}
	buf->b_hdr->b_freeze_cksum = kmem_alloc(sizeof (zio_cksum_t), KM_SLEEP);
	fletcher_2_native(buf->b_data, buf->b_hdr->b_size,
	    buf->b_hdr->b_freeze_cksum);
	mutex_exit(&buf->b_hdr->b_freeze_lock);
}

void
arc_buf_thaw(arc_buf_t *buf)
{
	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

	if (buf->b_hdr->b_state != arc_anon)
		panic("modifying non-anon buffer!");
	if (buf->b_hdr->b_flags & ARC_IO_IN_PROGRESS)
		panic("modifying buffer while i/o in progress!");
	arc_cksum_verify(buf);
	mutex_enter(&buf->b_hdr->b_freeze_lock);
	if (buf->b_hdr->b_freeze_cksum != NULL) {
		kmem_free(buf->b_hdr->b_freeze_cksum, sizeof (zio_cksum_t));
		buf->b_hdr->b_freeze_cksum = NULL;
	}
	mutex_exit(&buf->b_hdr->b_freeze_lock);
}

void
arc_buf_freeze(arc_buf_t *buf)
{
	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

	ASSERT(buf->b_hdr->b_freeze_cksum != NULL ||
	    buf->b_hdr->b_state == arc_anon);
	arc_cksum_compute(buf);
}

static void
add_reference(arc_buf_hdr_t *ab, kmutex_t *hash_lock, void *tag)
{
	ASSERT(MUTEX_HELD(hash_lock));

	if ((refcount_add(&ab->b_refcnt, tag) == 1) &&
	    (ab->b_state != arc_anon)) {
		uint64_t delta = ab->b_size * ab->b_datacnt;

		ASSERT(!MUTEX_HELD(&ab->b_state->arcs_mtx));
		mutex_enter(&ab->b_state->arcs_mtx);
		ASSERT(list_link_active(&ab->b_arc_node));
		list_remove(&ab->b_state->arcs_list, ab);
		if (GHOST_STATE(ab->b_state)) {
			ASSERT3U(ab->b_datacnt, ==, 0);
			ASSERT3P(ab->b_buf, ==, NULL);
			delta = ab->b_size;
		}
		ASSERT(delta > 0);
		ASSERT3U(ab->b_state->arcs_lsize, >=, delta);
		atomic_add_64(&ab->b_state->arcs_lsize, -delta);
		mutex_exit(&ab->b_state->arcs_mtx);
		/* remove the prefetch flag is we get a reference */
		if (ab->b_flags & ARC_PREFETCH)
			ab->b_flags &= ~ARC_PREFETCH;
	}
}

static int
remove_reference(arc_buf_hdr_t *ab, kmutex_t *hash_lock, void *tag)
{
	int cnt;
	arc_state_t *state = ab->b_state;

	ASSERT(state == arc_anon || MUTEX_HELD(hash_lock));
	ASSERT(!GHOST_STATE(state));

	if (((cnt = refcount_remove(&ab->b_refcnt, tag)) == 0) &&
	    (state != arc_anon)) {
		ASSERT(!MUTEX_HELD(&state->arcs_mtx));
		mutex_enter(&state->arcs_mtx);
		ASSERT(!list_link_active(&ab->b_arc_node));
		list_insert_head(&state->arcs_list, ab);
		ASSERT(ab->b_datacnt > 0);
		atomic_add_64(&state->arcs_lsize, ab->b_size * ab->b_datacnt);
		ASSERT3U(state->arcs_size, >=, state->arcs_lsize);
		mutex_exit(&state->arcs_mtx);
	}
	return (cnt);
}

/*
 * Move the supplied buffer to the indicated state.  The mutex
 * for the buffer must be held by the caller.
 */
static void
arc_change_state(arc_state_t *new_state, arc_buf_hdr_t *ab, kmutex_t *hash_lock)
{
	arc_state_t *old_state = ab->b_state;
	int64_t refcnt = refcount_count(&ab->b_refcnt);
	uint64_t from_delta, to_delta;

	ASSERT(MUTEX_HELD(hash_lock));
	ASSERT(new_state != old_state);
	ASSERT(refcnt == 0 || ab->b_datacnt > 0);
	ASSERT(ab->b_datacnt == 0 || !GHOST_STATE(new_state));

	from_delta = to_delta = ab->b_datacnt * ab->b_size;

	/*
	 * If this buffer is evictable, transfer it from the
	 * old state list to the new state list.
	 */
	if (refcnt == 0) {
		if (old_state != arc_anon) {
			int use_mutex = !MUTEX_HELD(&old_state->arcs_mtx);

			if (use_mutex)
				mutex_enter(&old_state->arcs_mtx);

			ASSERT(list_link_active(&ab->b_arc_node));
			list_remove(&old_state->arcs_list, ab);

			/*
			 * If prefetching out of the ghost cache,
			 * we will have a non-null datacnt.
			 */
			if (GHOST_STATE(old_state) && ab->b_datacnt == 0) {
				/* ghost elements have a ghost size */
				ASSERT(ab->b_buf == NULL);
				from_delta = ab->b_size;
			}
			ASSERT3U(old_state->arcs_lsize, >=, from_delta);
			atomic_add_64(&old_state->arcs_lsize, -from_delta);

			if (use_mutex)
				mutex_exit(&old_state->arcs_mtx);
		}
		if (new_state != arc_anon) {
			int use_mutex = !MUTEX_HELD(&new_state->arcs_mtx);

			if (use_mutex)
				mutex_enter(&new_state->arcs_mtx);

			list_insert_head(&new_state->arcs_list, ab);

			/* ghost elements have a ghost size */
			if (GHOST_STATE(new_state)) {
				ASSERT(ab->b_datacnt == 0);
				ASSERT(ab->b_buf == NULL);
				to_delta = ab->b_size;
			}
			atomic_add_64(&new_state->arcs_lsize, to_delta);
			ASSERT3U(new_state->arcs_size + to_delta, >=,
			    new_state->arcs_lsize);

			if (use_mutex)
				mutex_exit(&new_state->arcs_mtx);
		}
	}

	ASSERT(!BUF_EMPTY(ab));
	if (new_state == arc_anon && old_state != arc_anon) {
		buf_hash_remove(ab);
	}

	/* adjust state sizes */
	if (to_delta)
		atomic_add_64(&new_state->arcs_size, to_delta);
	if (from_delta) {
		ASSERT3U(old_state->arcs_size, >=, from_delta);
		atomic_add_64(&old_state->arcs_size, -from_delta);
	}
	ab->b_state = new_state;
}

arc_buf_t *
arc_buf_alloc(spa_t *spa, int size, void *tag, arc_buf_contents_t type)
{
	arc_buf_hdr_t *hdr;
	arc_buf_t *buf;

	ASSERT3U(size, >, 0);
	hdr = kmem_cache_alloc(hdr_cache, KM_SLEEP);
	ASSERT(BUF_EMPTY(hdr));
	hdr->b_size = size;
	hdr->b_type = type;
	hdr->b_spa = spa;
	hdr->b_state = arc_anon;
	hdr->b_arc_access = 0;
	mutex_init(&hdr->b_freeze_lock, NULL, MUTEX_DEFAULT, NULL);
	buf = kmem_cache_alloc(buf_cache, KM_SLEEP);
	buf->b_hdr = hdr;
	buf->b_data = NULL;
	buf->b_efunc = NULL;
	buf->b_private = NULL;
	buf->b_next = NULL;
	hdr->b_buf = buf;
	arc_get_data_buf(buf);
	hdr->b_datacnt = 1;
	hdr->b_flags = 0;
	ASSERT(refcount_is_zero(&hdr->b_refcnt));
	(void) refcount_add(&hdr->b_refcnt, tag);

	return (buf);
}

static arc_buf_t *
arc_buf_clone(arc_buf_t *from)
{
	arc_buf_t *buf;
	arc_buf_hdr_t *hdr = from->b_hdr;
	uint64_t size = hdr->b_size;

	buf = kmem_cache_alloc(buf_cache, KM_SLEEP);
	buf->b_hdr = hdr;
	buf->b_data = NULL;
	buf->b_efunc = NULL;
	buf->b_private = NULL;
	buf->b_next = hdr->b_buf;
	hdr->b_buf = buf;
	arc_get_data_buf(buf);
	bcopy(from->b_data, buf->b_data, size);
	hdr->b_datacnt += 1;
	return (buf);
}

void
arc_buf_add_ref(arc_buf_t *buf, void* tag)
{
	arc_buf_hdr_t *hdr;
	kmutex_t *hash_lock;

	/*
	 * Check to see if this buffer is currently being evicted via
	 * arc_do_user_evicts().
	 */
	mutex_enter(&arc_eviction_mtx);
	hdr = buf->b_hdr;
	if (hdr == NULL) {
		mutex_exit(&arc_eviction_mtx);
		return;
	}
	hash_lock = HDR_LOCK(hdr);
	mutex_exit(&arc_eviction_mtx);

	mutex_enter(hash_lock);
	if (buf->b_data == NULL) {
		/*
		 * This buffer is evicted.
		 */
		mutex_exit(hash_lock);
		return;
	}

	ASSERT(buf->b_hdr == hdr);
	ASSERT(hdr->b_state == arc_mru || hdr->b_state == arc_mfu);
	add_reference(hdr, hash_lock, tag);
	arc_access(hdr, hash_lock);
	mutex_exit(hash_lock);
	ARCSTAT_BUMP(arcstat_hits);
	ARCSTAT_CONDSTAT(!(hdr->b_flags & ARC_PREFETCH),
	    demand, prefetch, hdr->b_type != ARC_BUFC_METADATA,
	    data, metadata, hits);
}

static void
arc_buf_destroy(arc_buf_t *buf, boolean_t recycle, boolean_t all)
{
	arc_buf_t **bufp;

	/* free up data associated with the buf */
	if (buf->b_data) {
		arc_state_t *state = buf->b_hdr->b_state;
		uint64_t size = buf->b_hdr->b_size;
		arc_buf_contents_t type = buf->b_hdr->b_type;

		arc_cksum_verify(buf);
		if (!recycle) {
			if (type == ARC_BUFC_METADATA) {
				zio_buf_free(buf->b_data, size);
			} else {
				ASSERT(type == ARC_BUFC_DATA);
				zio_data_buf_free(buf->b_data, size);
			}
			atomic_add_64(&arc_size, -size);
		}
		if (list_link_active(&buf->b_hdr->b_arc_node)) {
			ASSERT(refcount_is_zero(&buf->b_hdr->b_refcnt));
			ASSERT(state != arc_anon);
			ASSERT3U(state->arcs_lsize, >=, size);
			atomic_add_64(&state->arcs_lsize, -size);
		}
		ASSERT3U(state->arcs_size, >=, size);
		atomic_add_64(&state->arcs_size, -size);
		buf->b_data = NULL;
		ASSERT(buf->b_hdr->b_datacnt > 0);
		buf->b_hdr->b_datacnt -= 1;
	}

	/* only remove the buf if requested */
	if (!all)
		return;

	/* remove the buf from the hdr list */
	for (bufp = &buf->b_hdr->b_buf; *bufp != buf; bufp = &(*bufp)->b_next)
		continue;
	*bufp = buf->b_next;

	ASSERT(buf->b_efunc == NULL);

	/* clean up the buf */
	buf->b_hdr = NULL;
	kmem_cache_free(buf_cache, buf);
}

static void
arc_hdr_destroy(arc_buf_hdr_t *hdr)
{
	ASSERT(refcount_is_zero(&hdr->b_refcnt));
	ASSERT3P(hdr->b_state, ==, arc_anon);
	ASSERT(!HDR_IO_IN_PROGRESS(hdr));

	if (!BUF_EMPTY(hdr)) {
		ASSERT(!HDR_IN_HASH_TABLE(hdr));
		bzero(&hdr->b_dva, sizeof (dva_t));
		hdr->b_birth = 0;
		hdr->b_cksum0 = 0;
	}
	while (hdr->b_buf) {
		arc_buf_t *buf = hdr->b_buf;

		if (buf->b_efunc) {
			mutex_enter(&arc_eviction_mtx);
			ASSERT(buf->b_hdr != NULL);
			arc_buf_destroy(hdr->b_buf, FALSE, FALSE);
			hdr->b_buf = buf->b_next;
			buf->b_hdr = &arc_eviction_hdr;
			buf->b_next = arc_eviction_list;
			arc_eviction_list = buf;
			mutex_exit(&arc_eviction_mtx);
		} else {
			arc_buf_destroy(hdr->b_buf, FALSE, TRUE);
		}
	}
	if (hdr->b_freeze_cksum != NULL) {
		kmem_free(hdr->b_freeze_cksum, sizeof (zio_cksum_t));
		hdr->b_freeze_cksum = NULL;
	}
	mutex_destroy(&hdr->b_freeze_lock);

	ASSERT(!list_link_active(&hdr->b_arc_node));
	ASSERT3P(hdr->b_hash_next, ==, NULL);
	ASSERT3P(hdr->b_acb, ==, NULL);
	kmem_cache_free(hdr_cache, hdr);
}

void
arc_buf_free(arc_buf_t *buf, void *tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;
	int hashed = hdr->b_state != arc_anon;

	ASSERT(buf->b_efunc == NULL);
	ASSERT(buf->b_data != NULL);

	if (hashed) {
		kmutex_t *hash_lock = HDR_LOCK(hdr);

		mutex_enter(hash_lock);
		(void) remove_reference(hdr, hash_lock, tag);
		if (hdr->b_datacnt > 1)
			arc_buf_destroy(buf, FALSE, TRUE);
		else
			hdr->b_flags |= ARC_BUF_AVAILABLE;
		mutex_exit(hash_lock);
	} else if (HDR_IO_IN_PROGRESS(hdr)) {
		int destroy_hdr;
		/*
		 * We are in the middle of an async write.  Don't destroy
		 * this buffer unless the write completes before we finish
		 * decrementing the reference count.
		 */
		mutex_enter(&arc_eviction_mtx);
		(void) remove_reference(hdr, NULL, tag);
		ASSERT(refcount_is_zero(&hdr->b_refcnt));
		destroy_hdr = !HDR_IO_IN_PROGRESS(hdr);
		mutex_exit(&arc_eviction_mtx);
		if (destroy_hdr)
			arc_hdr_destroy(hdr);
	} else {
		if (remove_reference(hdr, NULL, tag) > 0) {
			ASSERT(HDR_IO_ERROR(hdr));
			arc_buf_destroy(buf, FALSE, TRUE);
		} else {
			arc_hdr_destroy(hdr);
		}
	}
}

int
arc_buf_remove_ref(arc_buf_t *buf, void* tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;
	kmutex_t *hash_lock = HDR_LOCK(hdr);
	int no_callback = (buf->b_efunc == NULL);

	if (hdr->b_state == arc_anon) {
		arc_buf_free(buf, tag);
		return (no_callback);
	}

	mutex_enter(hash_lock);
	ASSERT(hdr->b_state != arc_anon);
	ASSERT(buf->b_data != NULL);

	(void) remove_reference(hdr, hash_lock, tag);
	if (hdr->b_datacnt > 1) {
		if (no_callback)
			arc_buf_destroy(buf, FALSE, TRUE);
	} else if (no_callback) {
		ASSERT(hdr->b_buf == buf && buf->b_next == NULL);
		hdr->b_flags |= ARC_BUF_AVAILABLE;
	}
	ASSERT(no_callback || hdr->b_datacnt > 1 ||
	    refcount_is_zero(&hdr->b_refcnt));
	mutex_exit(hash_lock);
	return (no_callback);
}

int
arc_buf_size(arc_buf_t *buf)
{
	return (buf->b_hdr->b_size);
}

/*
 * Evict buffers from list until we've removed the specified number of
 * bytes.  Move the removed buffers to the appropriate evict state.
 * If the recycle flag is set, then attempt to "recycle" a buffer:
 * - look for a buffer to evict that is `bytes' long.
 * - return the data block from this buffer rather than freeing it.
 * This flag is used by callers that are trying to make space for a
 * new buffer in a full arc cache.
 */
static void *
arc_evict(arc_state_t *state, int64_t bytes, boolean_t recycle,
    arc_buf_contents_t type)
{
	arc_state_t *evicted_state;
	uint64_t bytes_evicted = 0, skipped = 0, missed = 0;
	arc_buf_hdr_t *ab, *ab_prev = NULL;
	kmutex_t *hash_lock;
	boolean_t have_lock;
	void *stolen = NULL;

	ASSERT(state == arc_mru || state == arc_mfu);

	evicted_state = (state == arc_mru) ? arc_mru_ghost : arc_mfu_ghost;

	mutex_enter(&state->arcs_mtx);
	mutex_enter(&evicted_state->arcs_mtx);

	for (ab = list_tail(&state->arcs_list); ab; ab = ab_prev) {
		ab_prev = list_prev(&state->arcs_list, ab);
		/* prefetch buffers have a minimum lifespan */
		if (HDR_IO_IN_PROGRESS(ab) ||
		    (ab->b_flags & (ARC_PREFETCH|ARC_INDIRECT) &&
		    lbolt - ab->b_arc_access < arc_min_prefetch_lifespan)) {
			skipped++;
			continue;
		}
		/* "lookahead" for better eviction candidate */
		if (recycle && ab->b_size != bytes &&
		    ab_prev && ab_prev->b_size == bytes)
			continue;
		hash_lock = HDR_LOCK(ab);
		have_lock = MUTEX_HELD(hash_lock);
		if (have_lock || mutex_tryenter(hash_lock)) {
			ASSERT3U(refcount_count(&ab->b_refcnt), ==, 0);
			ASSERT(ab->b_datacnt > 0);
			while (ab->b_buf) {
				arc_buf_t *buf = ab->b_buf;
				if (buf->b_data) {
					bytes_evicted += ab->b_size;
					if (recycle && ab->b_type == type &&
					    ab->b_size == bytes) {
						stolen = buf->b_data;
						recycle = FALSE;
					}
				}
				if (buf->b_efunc) {
					mutex_enter(&arc_eviction_mtx);
					arc_buf_destroy(buf,
					    buf->b_data == stolen, FALSE);
					ab->b_buf = buf->b_next;
					buf->b_hdr = &arc_eviction_hdr;
					buf->b_next = arc_eviction_list;
					arc_eviction_list = buf;
					mutex_exit(&arc_eviction_mtx);
				} else {
					arc_buf_destroy(buf,
					    buf->b_data == stolen, TRUE);
				}
			}
			ASSERT(ab->b_datacnt == 0);
			arc_change_state(evicted_state, ab, hash_lock);
			ASSERT(HDR_IN_HASH_TABLE(ab));
			ab->b_flags = ARC_IN_HASH_TABLE;
			DTRACE_PROBE1(arc__evict, arc_buf_hdr_t *, ab);
			if (!have_lock)
				mutex_exit(hash_lock);
			if (bytes >= 0 && bytes_evicted >= bytes)
				break;
		} else {
			missed += 1;
		}
	}

	mutex_exit(&evicted_state->arcs_mtx);
	mutex_exit(&state->arcs_mtx);

	if (bytes_evicted < bytes)
		dprintf("only evicted %lld bytes from %x",
		    (longlong_t)bytes_evicted, state);

	if (skipped)
		ARCSTAT_INCR(arcstat_evict_skip, skipped);

	if (missed)
		ARCSTAT_INCR(arcstat_mutex_miss, missed);

	return (stolen);
}

/*
 * Remove buffers from list until we've removed the specified number of
 * bytes.  Destroy the buffers that are removed.
 */
static void
arc_evict_ghost(arc_state_t *state, int64_t bytes)
{
	arc_buf_hdr_t *ab, *ab_prev;
	kmutex_t *hash_lock;
	uint64_t bytes_deleted = 0;
	uint64_t bufs_skipped = 0;

	ASSERT(GHOST_STATE(state));
top:
	mutex_enter(&state->arcs_mtx);
	for (ab = list_tail(&state->arcs_list); ab; ab = ab_prev) {
		ab_prev = list_prev(&state->arcs_list, ab);
		hash_lock = HDR_LOCK(ab);
		if (mutex_tryenter(hash_lock)) {
			ASSERT(!HDR_IO_IN_PROGRESS(ab));
			ASSERT(ab->b_buf == NULL);
			arc_change_state(arc_anon, ab, hash_lock);
			mutex_exit(hash_lock);
			ARCSTAT_BUMP(arcstat_deleted);
			bytes_deleted += ab->b_size;
			arc_hdr_destroy(ab);
			DTRACE_PROBE1(arc__delete, arc_buf_hdr_t *, ab);
			if (bytes >= 0 && bytes_deleted >= bytes)
				break;
		} else {
			if (bytes < 0) {
				mutex_exit(&state->arcs_mtx);
				mutex_enter(hash_lock);
				mutex_exit(hash_lock);
				goto top;
			}
			bufs_skipped += 1;
		}
	}
	mutex_exit(&state->arcs_mtx);

	if (bufs_skipped) {
		ARCSTAT_INCR(arcstat_mutex_miss, bufs_skipped);
		ASSERT(bytes >= 0);
	}

	if (bytes_deleted < bytes)
		dprintf("only deleted %lld bytes from %p",
		    (longlong_t)bytes_deleted, state);
}

static void
arc_adjust(void)
{
	int64_t top_sz, mru_over, arc_over, todelete;

	top_sz = arc_anon->arcs_size + arc_mru->arcs_size;

	if (top_sz > arc_p && arc_mru->arcs_lsize > 0) {
		int64_t toevict = MIN(arc_mru->arcs_lsize, top_sz - arc_p);
		(void) arc_evict(arc_mru, toevict, FALSE, ARC_BUFC_UNDEF);
		top_sz = arc_anon->arcs_size + arc_mru->arcs_size;
	}

	mru_over = top_sz + arc_mru_ghost->arcs_size - arc_c;

	if (mru_over > 0) {
		if (arc_mru_ghost->arcs_lsize > 0) {
			todelete = MIN(arc_mru_ghost->arcs_lsize, mru_over);
			arc_evict_ghost(arc_mru_ghost, todelete);
		}
	}

	if ((arc_over = arc_size - arc_c) > 0) {
		int64_t tbl_over;

		if (arc_mfu->arcs_lsize > 0) {
			int64_t toevict = MIN(arc_mfu->arcs_lsize, arc_over);
			(void) arc_evict(arc_mfu, toevict, FALSE,
			    ARC_BUFC_UNDEF);
		}

		tbl_over = arc_size + arc_mru_ghost->arcs_lsize +
		    arc_mfu_ghost->arcs_lsize - arc_c*2;

		if (tbl_over > 0 && arc_mfu_ghost->arcs_lsize > 0) {
			todelete = MIN(arc_mfu_ghost->arcs_lsize, tbl_over);
			arc_evict_ghost(arc_mfu_ghost, todelete);
		}
	}
}

static void
arc_do_user_evicts(void)
{
	mutex_enter(&arc_eviction_mtx);
	while (arc_eviction_list != NULL) {
		arc_buf_t *buf = arc_eviction_list;
		arc_eviction_list = buf->b_next;
		buf->b_hdr = NULL;
		mutex_exit(&arc_eviction_mtx);

		if (buf->b_efunc != NULL)
			VERIFY(buf->b_efunc(buf) == 0);

		buf->b_efunc = NULL;
		buf->b_private = NULL;
		kmem_cache_free(buf_cache, buf);
		mutex_enter(&arc_eviction_mtx);
	}
	mutex_exit(&arc_eviction_mtx);
}

/*
 * Flush all *evictable* data from the cache.
 * NOTE: this will not touch "active" (i.e. referenced) data.
 */
void
arc_flush(void)
{
	while (list_head(&arc_mru->arcs_list))
		(void) arc_evict(arc_mru, -1, FALSE, ARC_BUFC_UNDEF);
	while (list_head(&arc_mfu->arcs_list))
		(void) arc_evict(arc_mfu, -1, FALSE, ARC_BUFC_UNDEF);

	arc_evict_ghost(arc_mru_ghost, -1);
	arc_evict_ghost(arc_mfu_ghost, -1);

	mutex_enter(&arc_reclaim_thr_lock);
	arc_do_user_evicts();
	mutex_exit(&arc_reclaim_thr_lock);
	ASSERT(arc_eviction_list == NULL);
}

int arc_shrink_shift = 5;		/* log2(fraction of arc to reclaim) */

void
arc_shrink(void)
{
	if (arc_c > arc_c_min) {
		uint64_t to_free;

#ifdef _KERNEL
		to_free = arc_c >> arc_shrink_shift;
#else
		to_free = arc_c >> arc_shrink_shift;
#endif
		if (arc_c > arc_c_min + to_free)
			atomic_add_64(&arc_c, -to_free);
		else
			arc_c = arc_c_min;

		atomic_add_64(&arc_p, -(arc_p >> arc_shrink_shift));
		if (arc_c > arc_size)
			arc_c = MAX(arc_size, arc_c_min);
		if (arc_p > arc_c)
			arc_p = (arc_c >> 1);
		ASSERT(arc_c >= arc_c_min);
		ASSERT((int64_t)arc_p >= 0);
	}

	if (arc_size > arc_c)
		arc_adjust();
}

static int zfs_needfree = 0;

static int
arc_reclaim_needed(void)
{
#if 0
	uint64_t extra;
#endif

#ifdef _KERNEL

	if (zfs_needfree)
		return (1);

#if 0
	/*
	 * check to make sure that swapfs has enough space so that anon
	 * reservations can still succeeed. anon_resvmem() checks that the
	 * availrmem is greater than swapfs_minfree, and the number of reserved
	 * swap pages.  We also add a bit of extra here just to prevent
	 * circumstances from getting really dire.
	 */
	if (availrmem < swapfs_minfree + swapfs_reserve + extra)
		return (1);

	/*
	 * If zio data pages are being allocated out of a separate heap segment,
	 * then check that the size of available vmem for this area remains
	 * above 1/4th free.  This needs to be done when the size of the
	 * non-default segment is smaller than physical memory, so we could
	 * conceivably run out of VA in that segment before running out of
	 * physical memory.
	 */
	if (zio_arena != NULL) {
		size_t arc_ziosize =
		    btop(vmem_size(zio_arena, VMEM_FREE | VMEM_ALLOC));

		if ((physmem > arc_ziosize) &&
		    (btop(vmem_size(zio_arena, VMEM_FREE)) < arc_ziosize >> 2))
			return (1);
	}

#if defined(__i386)
	/*
	 * If we're on an i386 platform, it's possible that we'll exhaust the
	 * kernel heap space before we ever run out of available physical
	 * memory.  Most checks of the size of the heap_area compare against
	 * tune.t_minarmem, which is the minimum available real memory that we
	 * can have in the system.  However, this is generally fixed at 25 pages
	 * which is so low that it's useless.  In this comparison, we seek to
	 * calculate the total heap-size, and reclaim if more than 3/4ths of the
	 * heap is allocated.  (Or, in the caclulation, if less than 1/4th is
	 * free)
	 */
	if (btop(vmem_size(heap_arena, VMEM_FREE)) <
	    (btop(vmem_size(heap_arena, VMEM_FREE | VMEM_ALLOC)) >> 2))
		return (1);
#endif
#else
	if (kmem_used() > (kmem_size() * 4) / 5)
		return (1);
#endif

#else
	if (spa_get_random(100) == 0)
		return (1);
#endif
	return (0);
}

static void
arc_kmem_reap_now(arc_reclaim_strategy_t strat)
{
#ifdef ZIO_USE_UMA
	size_t			i;
	kmem_cache_t		*prev_cache = NULL;
	kmem_cache_t		*prev_data_cache = NULL;
	extern kmem_cache_t	*zio_buf_cache[];
	extern kmem_cache_t	*zio_data_buf_cache[];
#endif

#ifdef _KERNEL
	/*
	 * First purge some DNLC entries, in case the DNLC is using
	 * up too much memory.
	 */
	dnlc_reduce_cache((void *)(uintptr_t)arc_reduce_dnlc_percent);

#if defined(__i386)
	/*
	 * Reclaim unused memory from all kmem caches.
	 */
	kmem_reap();
#endif
#endif

	/*
	 * An agressive reclamation will shrink the cache size as well as
	 * reap free buffers from the arc kmem caches.
	 */
	if (strat == ARC_RECLAIM_AGGR)
		arc_shrink();

#ifdef ZIO_USE_UMA
	for (i = 0; i < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT; i++) {
		if (zio_buf_cache[i] != prev_cache) {
			prev_cache = zio_buf_cache[i];
			kmem_cache_reap_now(zio_buf_cache[i]);
		}
		if (zio_data_buf_cache[i] != prev_data_cache) {
			prev_data_cache = zio_data_buf_cache[i];
			kmem_cache_reap_now(zio_data_buf_cache[i]);
		}
	}
#endif
	kmem_cache_reap_now(buf_cache);
	kmem_cache_reap_now(hdr_cache);
}

static void
arc_reclaim_thread(void *dummy __unused)
{
	clock_t			growtime = 0;
	arc_reclaim_strategy_t	last_reclaim = ARC_RECLAIM_CONS;
	callb_cpr_t		cpr;

	CALLB_CPR_INIT(&cpr, &arc_reclaim_thr_lock, callb_generic_cpr, FTAG);

	mutex_enter(&arc_reclaim_thr_lock);
	while (arc_thread_exit == 0) {
		if (arc_reclaim_needed()) {

			if (arc_no_grow) {
				if (last_reclaim == ARC_RECLAIM_CONS) {
					last_reclaim = ARC_RECLAIM_AGGR;
				} else {
					last_reclaim = ARC_RECLAIM_CONS;
				}
			} else {
				arc_no_grow = TRUE;
				last_reclaim = ARC_RECLAIM_AGGR;
				membar_producer();
			}

			/* reset the growth delay for every reclaim */
			growtime = lbolt + (arc_grow_retry * hz);
			ASSERT(growtime > 0);

			if (zfs_needfree && last_reclaim == ARC_RECLAIM_CONS) {
				/*
				 * If zfs_needfree is TRUE our vm_lowmem hook
				 * was called and in that case we must free some
				 * memory, so switch to aggressive mode.
				 */
				arc_no_grow = TRUE;
				last_reclaim = ARC_RECLAIM_AGGR;
			}
			arc_kmem_reap_now(last_reclaim);
		} else if ((growtime > 0) && ((growtime - lbolt) <= 0)) {
			arc_no_grow = FALSE;
		}

		if (zfs_needfree ||
		    (2 * arc_c < arc_size +
		    arc_mru_ghost->arcs_size + arc_mfu_ghost->arcs_size))
			arc_adjust();

		if (arc_eviction_list != NULL)
			arc_do_user_evicts();

		if (arc_reclaim_needed()) {
			zfs_needfree = 0;
#ifdef _KERNEL
			wakeup(&zfs_needfree);
#endif
		}

		/* block until needed, or one second, whichever is shorter */
		CALLB_CPR_SAFE_BEGIN(&cpr);
		(void) cv_timedwait(&arc_reclaim_thr_cv,
		    &arc_reclaim_thr_lock, hz);
		CALLB_CPR_SAFE_END(&cpr, &arc_reclaim_thr_lock);
	}

	arc_thread_exit = 0;
	cv_broadcast(&arc_reclaim_thr_cv);
	CALLB_CPR_EXIT(&cpr);		/* drops arc_reclaim_thr_lock */
	thread_exit();
}

/*
 * Adapt arc info given the number of bytes we are trying to add and
 * the state that we are comming from.  This function is only called
 * when we are adding new content to the cache.
 */
static void
arc_adapt(int bytes, arc_state_t *state)
{
	int mult;

	ASSERT(bytes > 0);
	/*
	 * Adapt the target size of the MRU list:
	 *	- if we just hit in the MRU ghost list, then increase
	 *	  the target size of the MRU list.
	 *	- if we just hit in the MFU ghost list, then increase
	 *	  the target size of the MFU list by decreasing the
	 *	  target size of the MRU list.
	 */
	if (state == arc_mru_ghost) {
		mult = ((arc_mru_ghost->arcs_size >= arc_mfu_ghost->arcs_size) ?
		    1 : (arc_mfu_ghost->arcs_size/arc_mru_ghost->arcs_size));

		arc_p = MIN(arc_c, arc_p + bytes * mult);
	} else if (state == arc_mfu_ghost) {
		mult = ((arc_mfu_ghost->arcs_size >= arc_mru_ghost->arcs_size) ?
		    1 : (arc_mru_ghost->arcs_size/arc_mfu_ghost->arcs_size));

		arc_p = MAX(0, (int64_t)arc_p - bytes * mult);
	}
	ASSERT((int64_t)arc_p >= 0);

	if (arc_reclaim_needed()) {
		cv_signal(&arc_reclaim_thr_cv);
		return;
	}

	if (arc_no_grow)
		return;

	if (arc_c >= arc_c_max)
		return;

	/*
	 * If we're within (2 * maxblocksize) bytes of the target
	 * cache size, increment the target cache size
	 */
	if (arc_size > arc_c - (2ULL << SPA_MAXBLOCKSHIFT)) {
		atomic_add_64(&arc_c, (int64_t)bytes);
		if (arc_c > arc_c_max)
			arc_c = arc_c_max;
		else if (state == arc_anon)
			atomic_add_64(&arc_p, (int64_t)bytes);
		if (arc_p > arc_c)
			arc_p = arc_c;
	}
	ASSERT((int64_t)arc_p >= 0);
}

/*
 * Check if the cache has reached its limits and eviction is required
 * prior to insert.
 */
static int
arc_evict_needed()
{
	if (arc_reclaim_needed())
		return (1);

	return (arc_size > arc_c);
}

/*
 * The buffer, supplied as the first argument, needs a data block.
 * So, if we are at cache max, determine which cache should be victimized.
 * We have the following cases:
 *
 * 1. Insert for MRU, p > sizeof(arc_anon + arc_mru) ->
 * In this situation if we're out of space, but the resident size of the MFU is
 * under the limit, victimize the MFU cache to satisfy this insertion request.
 *
 * 2. Insert for MRU, p <= sizeof(arc_anon + arc_mru) ->
 * Here, we've used up all of the available space for the MRU, so we need to
 * evict from our own cache instead.  Evict from the set of resident MRU
 * entries.
 *
 * 3. Insert for MFU (c - p) > sizeof(arc_mfu) ->
 * c minus p represents the MFU space in the cache, since p is the size of the
 * cache that is dedicated to the MRU.  In this situation there's still space on
 * the MFU side, so the MRU side needs to be victimized.
 *
 * 4. Insert for MFU (c - p) < sizeof(arc_mfu) ->
 * MFU's resident set is consuming more space than it has been allotted.  In
 * this situation, we must victimize our own cache, the MFU, for this insertion.
 */
static void
arc_get_data_buf(arc_buf_t *buf)
{
	arc_state_t		*state = buf->b_hdr->b_state;
	uint64_t		size = buf->b_hdr->b_size;
	arc_buf_contents_t	type = buf->b_hdr->b_type;

	arc_adapt(size, state);

	/*
	 * We have not yet reached cache maximum size,
	 * just allocate a new buffer.
	 */
	if (!arc_evict_needed()) {
		if (type == ARC_BUFC_METADATA) {
			buf->b_data = zio_buf_alloc(size);
		} else {
			ASSERT(type == ARC_BUFC_DATA);
			buf->b_data = zio_data_buf_alloc(size);
		}
		atomic_add_64(&arc_size, size);
		goto out;
	}

	/*
	 * If we are prefetching from the mfu ghost list, this buffer
	 * will end up on the mru list; so steal space from there.
	 */
	if (state == arc_mfu_ghost)
		state = buf->b_hdr->b_flags & ARC_PREFETCH ? arc_mru : arc_mfu;
	else if (state == arc_mru_ghost)
		state = arc_mru;

	if (state == arc_mru || state == arc_anon) {
		uint64_t mru_used = arc_anon->arcs_size + arc_mru->arcs_size;
		state = (arc_p > mru_used) ? arc_mfu : arc_mru;
	} else {
		/* MFU cases */
		uint64_t mfu_space = arc_c - arc_p;
		state =  (mfu_space > arc_mfu->arcs_size) ? arc_mru : arc_mfu;
	}
	if ((buf->b_data = arc_evict(state, size, TRUE, type)) == NULL) {
		if (type == ARC_BUFC_METADATA) {
			buf->b_data = zio_buf_alloc(size);
		} else {
			ASSERT(type == ARC_BUFC_DATA);
			buf->b_data = zio_data_buf_alloc(size);
		}
		atomic_add_64(&arc_size, size);
		ARCSTAT_BUMP(arcstat_recycle_miss);
	}
	ASSERT(buf->b_data != NULL);
out:
	/*
	 * Update the state size.  Note that ghost states have a
	 * "ghost size" and so don't need to be updated.
	 */
	if (!GHOST_STATE(buf->b_hdr->b_state)) {
		arc_buf_hdr_t *hdr = buf->b_hdr;

		atomic_add_64(&hdr->b_state->arcs_size, size);
		if (list_link_active(&hdr->b_arc_node)) {
			ASSERT(refcount_is_zero(&hdr->b_refcnt));
			atomic_add_64(&hdr->b_state->arcs_lsize, size);
		}
		/*
		 * If we are growing the cache, and we are adding anonymous
		 * data, and we have outgrown arc_p, update arc_p
		 */
		if (arc_size < arc_c && hdr->b_state == arc_anon &&
		    arc_anon->arcs_size + arc_mru->arcs_size > arc_p)
			arc_p = MIN(arc_c, arc_p + size);
	}
}

/*
 * This routine is called whenever a buffer is accessed.
 * NOTE: the hash lock is dropped in this function.
 */
static void
arc_access(arc_buf_hdr_t *buf, kmutex_t *hash_lock)
{
	ASSERT(MUTEX_HELD(hash_lock));

	if (buf->b_state == arc_anon) {
		/*
		 * This buffer is not in the cache, and does not
		 * appear in our "ghost" list.  Add the new buffer
		 * to the MRU state.
		 */

		ASSERT(buf->b_arc_access == 0);
		buf->b_arc_access = lbolt;
		DTRACE_PROBE1(new_state__mru, arc_buf_hdr_t *, buf);
		arc_change_state(arc_mru, buf, hash_lock);

	} else if (buf->b_state == arc_mru) {
		/*
		 * If this buffer is here because of a prefetch, then either:
		 * - clear the flag if this is a "referencing" read
		 *   (any subsequent access will bump this into the MFU state).
		 * or
		 * - move the buffer to the head of the list if this is
		 *   another prefetch (to make it less likely to be evicted).
		 */
		if ((buf->b_flags & ARC_PREFETCH) != 0) {
			if (refcount_count(&buf->b_refcnt) == 0) {
				ASSERT(list_link_active(&buf->b_arc_node));
				mutex_enter(&arc_mru->arcs_mtx);
				list_remove(&arc_mru->arcs_list, buf);
				list_insert_head(&arc_mru->arcs_list, buf);
				mutex_exit(&arc_mru->arcs_mtx);
			} else {
				buf->b_flags &= ~ARC_PREFETCH;
				ARCSTAT_BUMP(arcstat_mru_hits);
			}
			buf->b_arc_access = lbolt;
			return;
		}

		/*
		 * This buffer has been "accessed" only once so far,
		 * but it is still in the cache. Move it to the MFU
		 * state.
		 */
		if (lbolt > buf->b_arc_access + ARC_MINTIME) {
			/*
			 * More than 125ms have passed since we
			 * instantiated this buffer.  Move it to the
			 * most frequently used state.
			 */
			buf->b_arc_access = lbolt;
			DTRACE_PROBE1(new_state__mfu, arc_buf_hdr_t *, buf);
			arc_change_state(arc_mfu, buf, hash_lock);
		}
		ARCSTAT_BUMP(arcstat_mru_hits);
	} else if (buf->b_state == arc_mru_ghost) {
		arc_state_t	*new_state;
		/*
		 * This buffer has been "accessed" recently, but
		 * was evicted from the cache.  Move it to the
		 * MFU state.
		 */

		if (buf->b_flags & ARC_PREFETCH) {
			new_state = arc_mru;
			if (refcount_count(&buf->b_refcnt) > 0)
				buf->b_flags &= ~ARC_PREFETCH;
			DTRACE_PROBE1(new_state__mru, arc_buf_hdr_t *, buf);
		} else {
			new_state = arc_mfu;
			DTRACE_PROBE1(new_state__mfu, arc_buf_hdr_t *, buf);
		}

		buf->b_arc_access = lbolt;
		arc_change_state(new_state, buf, hash_lock);

		ARCSTAT_BUMP(arcstat_mru_ghost_hits);
	} else if (buf->b_state == arc_mfu) {
		/*
		 * This buffer has been accessed more than once and is
		 * still in the cache.  Keep it in the MFU state.
		 *
		 * NOTE: an add_reference() that occurred when we did
		 * the arc_read() will have kicked this off the list.
		 * If it was a prefetch, we will explicitly move it to
		 * the head of the list now.
		 */
		if ((buf->b_flags & ARC_PREFETCH) != 0) {
			ASSERT(refcount_count(&buf->b_refcnt) == 0);
			ASSERT(list_link_active(&buf->b_arc_node));
			mutex_enter(&arc_mfu->arcs_mtx);
			list_remove(&arc_mfu->arcs_list, buf);
			list_insert_head(&arc_mfu->arcs_list, buf);
			mutex_exit(&arc_mfu->arcs_mtx);
		}
		ARCSTAT_BUMP(arcstat_mfu_hits);
		buf->b_arc_access = lbolt;
	} else if (buf->b_state == arc_mfu_ghost) {
		arc_state_t	*new_state = arc_mfu;
		/*
		 * This buffer has been accessed more than once but has
		 * been evicted from the cache.  Move it back to the
		 * MFU state.
		 */

		if (buf->b_flags & ARC_PREFETCH) {
			/*
			 * This is a prefetch access...
			 * move this block back to the MRU state.
			 */
			ASSERT3U(refcount_count(&buf->b_refcnt), ==, 0);
			new_state = arc_mru;
		}

		buf->b_arc_access = lbolt;
		DTRACE_PROBE1(new_state__mfu, arc_buf_hdr_t *, buf);
		arc_change_state(new_state, buf, hash_lock);

		ARCSTAT_BUMP(arcstat_mfu_ghost_hits);
	} else {
		ASSERT(!"invalid arc state");
	}
}

/* a generic arc_done_func_t which you can use */
/* ARGSUSED */
void
arc_bcopy_func(zio_t *zio, arc_buf_t *buf, void *arg)
{
	bcopy(buf->b_data, arg, buf->b_hdr->b_size);
	VERIFY(arc_buf_remove_ref(buf, arg) == 1);
}

/* a generic arc_done_func_t which you can use */
void
arc_getbuf_func(zio_t *zio, arc_buf_t *buf, void *arg)
{
	arc_buf_t **bufp = arg;
	if (zio && zio->io_error) {
		VERIFY(arc_buf_remove_ref(buf, arg) == 1);
		*bufp = NULL;
	} else {
		*bufp = buf;
	}
}

static void
arc_read_done(zio_t *zio)
{
	arc_buf_hdr_t	*hdr, *found;
	arc_buf_t	*buf;
	arc_buf_t	*abuf;	/* buffer we're assigning to callback */
	kmutex_t	*hash_lock;
	arc_callback_t	*callback_list, *acb;
	int		freeable = FALSE;

	buf = zio->io_private;
	hdr = buf->b_hdr;

	/*
	 * The hdr was inserted into hash-table and removed from lists
	 * prior to starting I/O.  We should find this header, since
	 * it's in the hash table, and it should be legit since it's
	 * not possible to evict it during the I/O.  The only possible
	 * reason for it not to be found is if we were freed during the
	 * read.
	 */
	found = buf_hash_find(zio->io_spa, &hdr->b_dva, hdr->b_birth,
	    &hash_lock);

	ASSERT((found == NULL && HDR_FREED_IN_READ(hdr) && hash_lock == NULL) ||
	    (found == hdr && DVA_EQUAL(&hdr->b_dva, BP_IDENTITY(zio->io_bp))));

	/* byteswap if necessary */
	callback_list = hdr->b_acb;
	ASSERT(callback_list != NULL);
	if (BP_SHOULD_BYTESWAP(zio->io_bp) && callback_list->acb_byteswap)
		callback_list->acb_byteswap(buf->b_data, hdr->b_size);

	arc_cksum_compute(buf);

	/* create copies of the data buffer for the callers */
	abuf = buf;
	for (acb = callback_list; acb; acb = acb->acb_next) {
		if (acb->acb_done) {
			if (abuf == NULL)
				abuf = arc_buf_clone(buf);
			acb->acb_buf = abuf;
			abuf = NULL;
		}
	}
	hdr->b_acb = NULL;
	hdr->b_flags &= ~ARC_IO_IN_PROGRESS;
	ASSERT(!HDR_BUF_AVAILABLE(hdr));
	if (abuf == buf)
		hdr->b_flags |= ARC_BUF_AVAILABLE;

	ASSERT(refcount_is_zero(&hdr->b_refcnt) || callback_list != NULL);

	if (zio->io_error != 0) {
		hdr->b_flags |= ARC_IO_ERROR;
		if (hdr->b_state != arc_anon)
			arc_change_state(arc_anon, hdr, hash_lock);
		if (HDR_IN_HASH_TABLE(hdr))
			buf_hash_remove(hdr);
		freeable = refcount_is_zero(&hdr->b_refcnt);
		/* convert checksum errors into IO errors */
		if (zio->io_error == ECKSUM)
			zio->io_error = EIO;
	}

	/*
	 * Broadcast before we drop the hash_lock to avoid the possibility
	 * that the hdr (and hence the cv) might be freed before we get to
	 * the cv_broadcast().
	 */
	cv_broadcast(&hdr->b_cv);

	if (hash_lock) {
		/*
		 * Only call arc_access on anonymous buffers.  This is because
		 * if we've issued an I/O for an evicted buffer, we've already
		 * called arc_access (to prevent any simultaneous readers from
		 * getting confused).
		 */
		if (zio->io_error == 0 && hdr->b_state == arc_anon)
			arc_access(hdr, hash_lock);
		mutex_exit(hash_lock);
	} else {
		/*
		 * This block was freed while we waited for the read to
		 * complete.  It has been removed from the hash table and
		 * moved to the anonymous state (so that it won't show up
		 * in the cache).
		 */
		ASSERT3P(hdr->b_state, ==, arc_anon);
		freeable = refcount_is_zero(&hdr->b_refcnt);
	}

	/* execute each callback and free its structure */
	while ((acb = callback_list) != NULL) {
		if (acb->acb_done)
			acb->acb_done(zio, acb->acb_buf, acb->acb_private);

		if (acb->acb_zio_dummy != NULL) {
			acb->acb_zio_dummy->io_error = zio->io_error;
			zio_nowait(acb->acb_zio_dummy);
		}

		callback_list = acb->acb_next;
		kmem_free(acb, sizeof (arc_callback_t));
	}

	if (freeable)
		arc_hdr_destroy(hdr);
}

/*
 * "Read" the block block at the specified DVA (in bp) via the
 * cache.  If the block is found in the cache, invoke the provided
 * callback immediately and return.  Note that the `zio' parameter
 * in the callback will be NULL in this case, since no IO was
 * required.  If the block is not in the cache pass the read request
 * on to the spa with a substitute callback function, so that the
 * requested block will be added to the cache.
 *
 * If a read request arrives for a block that has a read in-progress,
 * either wait for the in-progress read to complete (and return the
 * results); or, if this is a read with a "done" func, add a record
 * to the read to invoke the "done" func when the read completes,
 * and return; or just return.
 *
 * arc_read_done() will invoke all the requested "done" functions
 * for readers of this block.
 */
int
arc_read(zio_t *pio, spa_t *spa, blkptr_t *bp, arc_byteswap_func_t *swap,
    arc_done_func_t *done, void *private, int priority, int flags,
    uint32_t *arc_flags, zbookmark_t *zb)
{
	arc_buf_hdr_t *hdr;
	arc_buf_t *buf;
	kmutex_t *hash_lock;
	zio_t	*rzio;

top:
	hdr = buf_hash_find(spa, BP_IDENTITY(bp), bp->blk_birth, &hash_lock);
	if (hdr && hdr->b_datacnt > 0) {

		*arc_flags |= ARC_CACHED;

		if (HDR_IO_IN_PROGRESS(hdr)) {

			if (*arc_flags & ARC_WAIT) {
				cv_wait(&hdr->b_cv, hash_lock);
				mutex_exit(hash_lock);
				goto top;
			}
			ASSERT(*arc_flags & ARC_NOWAIT);

			if (done) {
				arc_callback_t	*acb = NULL;

				acb = kmem_zalloc(sizeof (arc_callback_t),
				    KM_SLEEP);
				acb->acb_done = done;
				acb->acb_private = private;
				acb->acb_byteswap = swap;
				if (pio != NULL)
					acb->acb_zio_dummy = zio_null(pio,
					    spa, NULL, NULL, flags);

				ASSERT(acb->acb_done != NULL);
				acb->acb_next = hdr->b_acb;
				hdr->b_acb = acb;
				add_reference(hdr, hash_lock, private);
				mutex_exit(hash_lock);
				return (0);
			}
			mutex_exit(hash_lock);
			return (0);
		}

		ASSERT(hdr->b_state == arc_mru || hdr->b_state == arc_mfu);

		if (done) {
			add_reference(hdr, hash_lock, private);
			/*
			 * If this block is already in use, create a new
			 * copy of the data so that we will be guaranteed
			 * that arc_release() will always succeed.
			 */
			buf = hdr->b_buf;
			ASSERT(buf);
			ASSERT(buf->b_data);
			if (HDR_BUF_AVAILABLE(hdr)) {
				ASSERT(buf->b_efunc == NULL);
				hdr->b_flags &= ~ARC_BUF_AVAILABLE;
			} else {
				buf = arc_buf_clone(buf);
			}
		} else if (*arc_flags & ARC_PREFETCH &&
		    refcount_count(&hdr->b_refcnt) == 0) {
			hdr->b_flags |= ARC_PREFETCH;
		}
		DTRACE_PROBE1(arc__hit, arc_buf_hdr_t *, hdr);
		arc_access(hdr, hash_lock);
		mutex_exit(hash_lock);
		ARCSTAT_BUMP(arcstat_hits);
		ARCSTAT_CONDSTAT(!(hdr->b_flags & ARC_PREFETCH),
		    demand, prefetch, hdr->b_type != ARC_BUFC_METADATA,
		    data, metadata, hits);

		if (done)
			done(NULL, buf, private);
	} else {
		uint64_t size = BP_GET_LSIZE(bp);
		arc_callback_t	*acb;

		if (hdr == NULL) {
			/* this block is not in the cache */
			arc_buf_hdr_t	*exists;
			arc_buf_contents_t type = BP_GET_BUFC_TYPE(bp);
			buf = arc_buf_alloc(spa, size, private, type);
			hdr = buf->b_hdr;
			hdr->b_dva = *BP_IDENTITY(bp);
			hdr->b_birth = bp->blk_birth;
			hdr->b_cksum0 = bp->blk_cksum.zc_word[0];
			exists = buf_hash_insert(hdr, &hash_lock);
			if (exists) {
				/* somebody beat us to the hash insert */
				mutex_exit(hash_lock);
				bzero(&hdr->b_dva, sizeof (dva_t));
				hdr->b_birth = 0;
				hdr->b_cksum0 = 0;
				(void) arc_buf_remove_ref(buf, private);
				goto top; /* restart the IO request */
			}
			/* if this is a prefetch, we don't have a reference */
			if (*arc_flags & ARC_PREFETCH) {
				(void) remove_reference(hdr, hash_lock,
				    private);
				hdr->b_flags |= ARC_PREFETCH;
			}
			if (BP_GET_LEVEL(bp) > 0)
				hdr->b_flags |= ARC_INDIRECT;
		} else {
			/* this block is in the ghost cache */
			ASSERT(GHOST_STATE(hdr->b_state));
			ASSERT(!HDR_IO_IN_PROGRESS(hdr));
			ASSERT3U(refcount_count(&hdr->b_refcnt), ==, 0);
			ASSERT(hdr->b_buf == NULL);

			/* if this is a prefetch, we don't have a reference */
			if (*arc_flags & ARC_PREFETCH)
				hdr->b_flags |= ARC_PREFETCH;
			else
				add_reference(hdr, hash_lock, private);
			buf = kmem_cache_alloc(buf_cache, KM_SLEEP);
			buf->b_hdr = hdr;
			buf->b_data = NULL;
			buf->b_efunc = NULL;
			buf->b_private = NULL;
			buf->b_next = NULL;
			hdr->b_buf = buf;
			arc_get_data_buf(buf);
			ASSERT(hdr->b_datacnt == 0);
			hdr->b_datacnt = 1;

		}

		acb = kmem_zalloc(sizeof (arc_callback_t), KM_SLEEP);
		acb->acb_done = done;
		acb->acb_private = private;
		acb->acb_byteswap = swap;

		ASSERT(hdr->b_acb == NULL);
		hdr->b_acb = acb;
		hdr->b_flags |= ARC_IO_IN_PROGRESS;

		/*
		 * If the buffer has been evicted, migrate it to a present state
		 * before issuing the I/O.  Once we drop the hash-table lock,
		 * the header will be marked as I/O in progress and have an
		 * attached buffer.  At this point, anybody who finds this
		 * buffer ought to notice that it's legit but has a pending I/O.
		 */

		if (GHOST_STATE(hdr->b_state))
			arc_access(hdr, hash_lock);
		mutex_exit(hash_lock);

		ASSERT3U(hdr->b_size, ==, size);
		DTRACE_PROBE3(arc__miss, blkptr_t *, bp, uint64_t, size,
		    zbookmark_t *, zb);
		ARCSTAT_BUMP(arcstat_misses);
		ARCSTAT_CONDSTAT(!(hdr->b_flags & ARC_PREFETCH),
		    demand, prefetch, hdr->b_type != ARC_BUFC_METADATA,
		    data, metadata, misses);

		rzio = zio_read(pio, spa, bp, buf->b_data, size,
		    arc_read_done, buf, priority, flags, zb);

		if (*arc_flags & ARC_WAIT)
			return (zio_wait(rzio));

		ASSERT(*arc_flags & ARC_NOWAIT);
		zio_nowait(rzio);
	}
	return (0);
}

/*
 * arc_read() variant to support pool traversal.  If the block is already
 * in the ARC, make a copy of it; otherwise, the caller will do the I/O.
 * The idea is that we don't want pool traversal filling up memory, but
 * if the ARC already has the data anyway, we shouldn't pay for the I/O.
 */
int
arc_tryread(spa_t *spa, blkptr_t *bp, void *data)
{
	arc_buf_hdr_t *hdr;
	kmutex_t *hash_mtx;
	int rc = 0;

	hdr = buf_hash_find(spa, BP_IDENTITY(bp), bp->blk_birth, &hash_mtx);

	if (hdr && hdr->b_datacnt > 0 && !HDR_IO_IN_PROGRESS(hdr)) {
		arc_buf_t *buf = hdr->b_buf;

		ASSERT(buf);
		while (buf->b_data == NULL) {
			buf = buf->b_next;
			ASSERT(buf);
		}
		bcopy(buf->b_data, data, hdr->b_size);
	} else {
		rc = ENOENT;
	}

	if (hash_mtx)
		mutex_exit(hash_mtx);

	return (rc);
}

void
arc_set_callback(arc_buf_t *buf, arc_evict_func_t *func, void *private)
{
	ASSERT(buf->b_hdr != NULL);
	ASSERT(buf->b_hdr->b_state != arc_anon);
	ASSERT(!refcount_is_zero(&buf->b_hdr->b_refcnt) || func == NULL);
	buf->b_efunc = func;
	buf->b_private = private;
}

/*
 * This is used by the DMU to let the ARC know that a buffer is
 * being evicted, so the ARC should clean up.  If this arc buf
 * is not yet in the evicted state, it will be put there.
 */
int
arc_buf_evict(arc_buf_t *buf)
{
	arc_buf_hdr_t *hdr;
	kmutex_t *hash_lock;
	arc_buf_t **bufp;

	mutex_enter(&arc_eviction_mtx);
	hdr = buf->b_hdr;
	if (hdr == NULL) {
		/*
		 * We are in arc_do_user_evicts().
		 */
		ASSERT(buf->b_data == NULL);
		mutex_exit(&arc_eviction_mtx);
		return (0);
	}
	hash_lock = HDR_LOCK(hdr);
	mutex_exit(&arc_eviction_mtx);

	mutex_enter(hash_lock);

	if (buf->b_data == NULL) {
		/*
		 * We are on the eviction list.
		 */
		mutex_exit(hash_lock);
		mutex_enter(&arc_eviction_mtx);
		if (buf->b_hdr == NULL) {
			/*
			 * We are already in arc_do_user_evicts().
			 */
			mutex_exit(&arc_eviction_mtx);
			return (0);
		} else {
			arc_buf_t copy = *buf; /* structure assignment */
			/*
			 * Process this buffer now
			 * but let arc_do_user_evicts() do the reaping.
			 */
			buf->b_efunc = NULL;
			mutex_exit(&arc_eviction_mtx);
			VERIFY(copy.b_efunc(&copy) == 0);
			return (1);
		}
	}

	ASSERT(buf->b_hdr == hdr);
	ASSERT3U(refcount_count(&hdr->b_refcnt), <, hdr->b_datacnt);
	ASSERT(hdr->b_state == arc_mru || hdr->b_state == arc_mfu);

	/*
	 * Pull this buffer off of the hdr
	 */
	bufp = &hdr->b_buf;
	while (*bufp != buf)
		bufp = &(*bufp)->b_next;
	*bufp = buf->b_next;

	ASSERT(buf->b_data != NULL);
	arc_buf_destroy(buf, FALSE, FALSE);

	if (hdr->b_datacnt == 0) {
		arc_state_t *old_state = hdr->b_state;
		arc_state_t *evicted_state;

		ASSERT(refcount_is_zero(&hdr->b_refcnt));

		evicted_state =
		    (old_state == arc_mru) ? arc_mru_ghost : arc_mfu_ghost;

		mutex_enter(&old_state->arcs_mtx);
		mutex_enter(&evicted_state->arcs_mtx);

		arc_change_state(evicted_state, hdr, hash_lock);
		ASSERT(HDR_IN_HASH_TABLE(hdr));
		hdr->b_flags = ARC_IN_HASH_TABLE;

		mutex_exit(&evicted_state->arcs_mtx);
		mutex_exit(&old_state->arcs_mtx);
	}
	mutex_exit(hash_lock);

	VERIFY(buf->b_efunc(buf) == 0);
	buf->b_efunc = NULL;
	buf->b_private = NULL;
	buf->b_hdr = NULL;
	kmem_cache_free(buf_cache, buf);
	return (1);
}

/*
 * Release this buffer from the cache.  This must be done
 * after a read and prior to modifying the buffer contents.
 * If the buffer has more than one reference, we must make
 * make a new hdr for the buffer.
 */
void
arc_release(arc_buf_t *buf, void *tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;
	kmutex_t *hash_lock = HDR_LOCK(hdr);

	/* this buffer is not on any list */
	ASSERT(refcount_count(&hdr->b_refcnt) > 0);

	if (hdr->b_state == arc_anon) {
		/* this buffer is already released */
		ASSERT3U(refcount_count(&hdr->b_refcnt), ==, 1);
		ASSERT(BUF_EMPTY(hdr));
		ASSERT(buf->b_efunc == NULL);
		arc_buf_thaw(buf);
		return;
	}

	mutex_enter(hash_lock);

	/*
	 * Do we have more than one buf?
	 */
	if (hdr->b_buf != buf || buf->b_next != NULL) {
		arc_buf_hdr_t *nhdr;
		arc_buf_t **bufp;
		uint64_t blksz = hdr->b_size;
		spa_t *spa = hdr->b_spa;
		arc_buf_contents_t type = hdr->b_type;

		ASSERT(hdr->b_datacnt > 1);
		/*
		 * Pull the data off of this buf and attach it to
		 * a new anonymous buf.
		 */
		(void) remove_reference(hdr, hash_lock, tag);
		bufp = &hdr->b_buf;
		while (*bufp != buf)
			bufp = &(*bufp)->b_next;
		*bufp = (*bufp)->b_next;
		buf->b_next = NULL;

		ASSERT3U(hdr->b_state->arcs_size, >=, hdr->b_size);
		atomic_add_64(&hdr->b_state->arcs_size, -hdr->b_size);
		if (refcount_is_zero(&hdr->b_refcnt)) {
			ASSERT3U(hdr->b_state->arcs_lsize, >=, hdr->b_size);
			atomic_add_64(&hdr->b_state->arcs_lsize, -hdr->b_size);
		}
		hdr->b_datacnt -= 1;
		arc_cksum_verify(buf);

		mutex_exit(hash_lock);

		nhdr = kmem_cache_alloc(hdr_cache, KM_SLEEP);
		nhdr->b_size = blksz;
		nhdr->b_spa = spa;
		nhdr->b_type = type;
		nhdr->b_buf = buf;
		nhdr->b_state = arc_anon;
		nhdr->b_arc_access = 0;
		nhdr->b_flags = 0;
		nhdr->b_datacnt = 1;
		nhdr->b_freeze_cksum = NULL;
		mutex_init(&nhdr->b_freeze_lock, NULL, MUTEX_DEFAULT, NULL);
		(void) refcount_add(&nhdr->b_refcnt, tag);
		buf->b_hdr = nhdr;
		atomic_add_64(&arc_anon->arcs_size, blksz);

		hdr = nhdr;
	} else {
		ASSERT(refcount_count(&hdr->b_refcnt) == 1);
		ASSERT(!list_link_active(&hdr->b_arc_node));
		ASSERT(!HDR_IO_IN_PROGRESS(hdr));
		arc_change_state(arc_anon, hdr, hash_lock);
		hdr->b_arc_access = 0;
		mutex_exit(hash_lock);
		bzero(&hdr->b_dva, sizeof (dva_t));
		hdr->b_birth = 0;
		hdr->b_cksum0 = 0;
		arc_buf_thaw(buf);
	}
	buf->b_efunc = NULL;
	buf->b_private = NULL;
}

int
arc_released(arc_buf_t *buf)
{
	return (buf->b_data != NULL && buf->b_hdr->b_state == arc_anon);
}

int
arc_has_callback(arc_buf_t *buf)
{
	return (buf->b_efunc != NULL);
}

#ifdef ZFS_DEBUG
int
arc_referenced(arc_buf_t *buf)
{
	return (refcount_count(&buf->b_hdr->b_refcnt));
}
#endif

static void
arc_write_ready(zio_t *zio)
{
	arc_write_callback_t *callback = zio->io_private;
	arc_buf_t *buf = callback->awcb_buf;

	if (callback->awcb_ready) {
		ASSERT(!refcount_is_zero(&buf->b_hdr->b_refcnt));
		callback->awcb_ready(zio, buf, callback->awcb_private);
	}
	arc_cksum_compute(buf);
}

static void
arc_write_done(zio_t *zio)
{
	arc_write_callback_t *callback = zio->io_private;
	arc_buf_t *buf = callback->awcb_buf;
	arc_buf_hdr_t *hdr = buf->b_hdr;

	hdr->b_acb = NULL;

	/* this buffer is on no lists and is not in the hash table */
	ASSERT3P(hdr->b_state, ==, arc_anon);

	hdr->b_dva = *BP_IDENTITY(zio->io_bp);
	hdr->b_birth = zio->io_bp->blk_birth;
	hdr->b_cksum0 = zio->io_bp->blk_cksum.zc_word[0];
	/*
	 * If the block to be written was all-zero, we may have
	 * compressed it away.  In this case no write was performed
	 * so there will be no dva/birth-date/checksum.  The buffer
	 * must therefor remain anonymous (and uncached).
	 */
	if (!BUF_EMPTY(hdr)) {
		arc_buf_hdr_t *exists;
		kmutex_t *hash_lock;

		arc_cksum_verify(buf);

		exists = buf_hash_insert(hdr, &hash_lock);
		if (exists) {
			/*
			 * This can only happen if we overwrite for
			 * sync-to-convergence, because we remove
			 * buffers from the hash table when we arc_free().
			 */
			ASSERT(DVA_EQUAL(BP_IDENTITY(&zio->io_bp_orig),
			    BP_IDENTITY(zio->io_bp)));
			ASSERT3U(zio->io_bp_orig.blk_birth, ==,
			    zio->io_bp->blk_birth);

			ASSERT(refcount_is_zero(&exists->b_refcnt));
			arc_change_state(arc_anon, exists, hash_lock);
			mutex_exit(hash_lock);
			arc_hdr_destroy(exists);
			exists = buf_hash_insert(hdr, &hash_lock);
			ASSERT3P(exists, ==, NULL);
		}
		hdr->b_flags &= ~ARC_IO_IN_PROGRESS;
		arc_access(hdr, hash_lock);
		mutex_exit(hash_lock);
	} else if (callback->awcb_done == NULL) {
		int destroy_hdr;
		/*
		 * This is an anonymous buffer with no user callback,
		 * destroy it if there are no active references.
		 */
		mutex_enter(&arc_eviction_mtx);
		destroy_hdr = refcount_is_zero(&hdr->b_refcnt);
		hdr->b_flags &= ~ARC_IO_IN_PROGRESS;
		mutex_exit(&arc_eviction_mtx);
		if (destroy_hdr)
			arc_hdr_destroy(hdr);
	} else {
		hdr->b_flags &= ~ARC_IO_IN_PROGRESS;
	}

	if (callback->awcb_done) {
		ASSERT(!refcount_is_zero(&hdr->b_refcnt));
		callback->awcb_done(zio, buf, callback->awcb_private);
	}

	kmem_free(callback, sizeof (arc_write_callback_t));
}

zio_t *
arc_write(zio_t *pio, spa_t *spa, int checksum, int compress, int ncopies,
    uint64_t txg, blkptr_t *bp, arc_buf_t *buf,
    arc_done_func_t *ready, arc_done_func_t *done, void *private, int priority,
    int flags, zbookmark_t *zb)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;
	arc_write_callback_t *callback;
	zio_t	*zio;

	/* this is a private buffer - no locking required */
	ASSERT3P(hdr->b_state, ==, arc_anon);
	ASSERT(BUF_EMPTY(hdr));
	ASSERT(!HDR_IO_ERROR(hdr));
	ASSERT((hdr->b_flags & ARC_IO_IN_PROGRESS) == 0);
	ASSERT(hdr->b_acb == 0);
	callback = kmem_zalloc(sizeof (arc_write_callback_t), KM_SLEEP);
	callback->awcb_ready = ready;
	callback->awcb_done = done;
	callback->awcb_private = private;
	callback->awcb_buf = buf;
	hdr->b_flags |= ARC_IO_IN_PROGRESS;
	zio = zio_write(pio, spa, checksum, compress, ncopies, txg, bp,
	    buf->b_data, hdr->b_size, arc_write_ready, arc_write_done, callback,
	    priority, flags, zb);

	return (zio);
}

int
arc_free(zio_t *pio, spa_t *spa, uint64_t txg, blkptr_t *bp,
    zio_done_func_t *done, void *private, uint32_t arc_flags)
{
	arc_buf_hdr_t *ab;
	kmutex_t *hash_lock;
	zio_t	*zio;

	/*
	 * If this buffer is in the cache, release it, so it
	 * can be re-used.
	 */
	ab = buf_hash_find(spa, BP_IDENTITY(bp), bp->blk_birth, &hash_lock);
	if (ab != NULL) {
		/*
		 * The checksum of blocks to free is not always
		 * preserved (eg. on the deadlist).  However, if it is
		 * nonzero, it should match what we have in the cache.
		 */
		ASSERT(bp->blk_cksum.zc_word[0] == 0 ||
		    ab->b_cksum0 == bp->blk_cksum.zc_word[0]);
		if (ab->b_state != arc_anon)
			arc_change_state(arc_anon, ab, hash_lock);
		if (HDR_IO_IN_PROGRESS(ab)) {
			/*
			 * This should only happen when we prefetch.
			 */
			ASSERT(ab->b_flags & ARC_PREFETCH);
			ASSERT3U(ab->b_datacnt, ==, 1);
			ab->b_flags |= ARC_FREED_IN_READ;
			if (HDR_IN_HASH_TABLE(ab))
				buf_hash_remove(ab);
			ab->b_arc_access = 0;
			bzero(&ab->b_dva, sizeof (dva_t));
			ab->b_birth = 0;
			ab->b_cksum0 = 0;
			ab->b_buf->b_efunc = NULL;
			ab->b_buf->b_private = NULL;
			mutex_exit(hash_lock);
		} else if (refcount_is_zero(&ab->b_refcnt)) {
			mutex_exit(hash_lock);
			arc_hdr_destroy(ab);
			ARCSTAT_BUMP(arcstat_deleted);
		} else {
			/*
			 * We still have an active reference on this
			 * buffer.  This can happen, e.g., from
			 * dbuf_unoverride().
			 */
			ASSERT(!HDR_IN_HASH_TABLE(ab));
			ab->b_arc_access = 0;
			bzero(&ab->b_dva, sizeof (dva_t));
			ab->b_birth = 0;
			ab->b_cksum0 = 0;
			ab->b_buf->b_efunc = NULL;
			ab->b_buf->b_private = NULL;
			mutex_exit(hash_lock);
		}
	}

	zio = zio_free(pio, spa, txg, bp, done, private);

	if (arc_flags & ARC_WAIT)
		return (zio_wait(zio));

	ASSERT(arc_flags & ARC_NOWAIT);
	zio_nowait(zio);

	return (0);
}

void
arc_tempreserve_clear(uint64_t tempreserve)
{
	atomic_add_64(&arc_tempreserve, -tempreserve);
	ASSERT((int64_t)arc_tempreserve >= 0);
}

int
arc_tempreserve_space(uint64_t tempreserve)
{
#ifdef ZFS_DEBUG
	/*
	 * Once in a while, fail for no reason.  Everything should cope.
	 */
	if (spa_get_random(10000) == 0) {
		dprintf("forcing random failure\n");
		return (ERESTART);
	}
#endif
	if (tempreserve > arc_c/4 && !arc_no_grow)
		arc_c = MIN(arc_c_max, tempreserve * 4);
	if (tempreserve > arc_c)
		return (ENOMEM);

	/*
	 * Throttle writes when the amount of dirty data in the cache
	 * gets too large.  We try to keep the cache less than half full
	 * of dirty blocks so that our sync times don't grow too large.
	 * Note: if two requests come in concurrently, we might let them
	 * both succeed, when one of them should fail.  Not a huge deal.
	 *
	 * XXX The limit should be adjusted dynamically to keep the time
	 * to sync a dataset fixed (around 1-5 seconds?).
	 */

	if (tempreserve + arc_tempreserve + arc_anon->arcs_size > arc_c / 2 &&
	    arc_tempreserve + arc_anon->arcs_size > arc_c / 4) {
		dprintf("failing, arc_tempreserve=%lluK anon=%lluK "
		    "tempreserve=%lluK arc_c=%lluK\n",
		    arc_tempreserve>>10, arc_anon->arcs_lsize>>10,
		    tempreserve>>10, arc_c>>10);
		return (ERESTART);
	}
	atomic_add_64(&arc_tempreserve, tempreserve);
	return (0);
}

static kmutex_t arc_lowmem_lock;
#ifdef _KERNEL
static eventhandler_tag arc_event_lowmem = NULL;

static void
arc_lowmem(void *arg __unused, int howto __unused)
{

	/* Serialize access via arc_lowmem_lock. */
	mutex_enter(&arc_lowmem_lock);
	zfs_needfree = 1;
	cv_signal(&arc_reclaim_thr_cv);
	while (zfs_needfree)
		tsleep(&zfs_needfree, 0, "zfs:lowmem", hz / 5);
	mutex_exit(&arc_lowmem_lock);
}
#endif

void
arc_init(void)
{
	mutex_init(&arc_reclaim_thr_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&arc_reclaim_thr_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&arc_lowmem_lock, NULL, MUTEX_DEFAULT, NULL);

	/* Convert seconds to clock ticks */
	arc_min_prefetch_lifespan = 1 * hz;

	/* Start out with 1/8 of all memory */
	arc_c = kmem_size() / 8;
#if 0
#ifdef _KERNEL
	/*
	 * On architectures where the physical memory can be larger
	 * than the addressable space (intel in 32-bit mode), we may
	 * need to limit the cache to 1/8 of VM size.
	 */
	arc_c = MIN(arc_c, vmem_size(heap_arena, VMEM_ALLOC | VMEM_FREE) / 8);
#endif
#endif
	/* set min cache to 1/32 of all memory, or 16MB, whichever is more */
	arc_c_min = MAX(arc_c / 4, 64<<18);
	/* set max to 1/2 of all memory, or all but 1GB, whichever is more */
	if (arc_c * 8 >= 1<<30)
		arc_c_max = (arc_c * 8) - (1<<30);
	else
		arc_c_max = arc_c_min;
	arc_c_max = MAX(arc_c * 6, arc_c_max);
#ifdef _KERNEL
	/*
	 * Allow the tunables to override our calculations if they are
	 * reasonable (ie. over 16MB)
	 */
	if (zfs_arc_max >= 64<<18 && zfs_arc_max < kmem_size())
		arc_c_max = zfs_arc_max;
	if (zfs_arc_min >= 64<<18 && zfs_arc_min <= arc_c_max)
		arc_c_min = zfs_arc_min;
#endif
	arc_c = arc_c_max;
	arc_p = (arc_c >> 1);

	/* if kmem_flags are set, lets try to use less memory */
	if (kmem_debugging())
		arc_c = arc_c / 2;
	if (arc_c < arc_c_min)
		arc_c = arc_c_min;

	zfs_arc_min = arc_c_min;
	zfs_arc_max = arc_c_max;

	arc_anon = &ARC_anon;
	arc_mru = &ARC_mru;
	arc_mru_ghost = &ARC_mru_ghost;
	arc_mfu = &ARC_mfu;
	arc_mfu_ghost = &ARC_mfu_ghost;
	arc_size = 0;

	mutex_init(&arc_anon->arcs_mtx, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&arc_mru->arcs_mtx, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&arc_mru_ghost->arcs_mtx, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&arc_mfu->arcs_mtx, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&arc_mfu_ghost->arcs_mtx, NULL, MUTEX_DEFAULT, NULL);

	list_create(&arc_mru->arcs_list, sizeof (arc_buf_hdr_t),
	    offsetof(arc_buf_hdr_t, b_arc_node));
	list_create(&arc_mru_ghost->arcs_list, sizeof (arc_buf_hdr_t),
	    offsetof(arc_buf_hdr_t, b_arc_node));
	list_create(&arc_mfu->arcs_list, sizeof (arc_buf_hdr_t),
	    offsetof(arc_buf_hdr_t, b_arc_node));
	list_create(&arc_mfu_ghost->arcs_list, sizeof (arc_buf_hdr_t),
	    offsetof(arc_buf_hdr_t, b_arc_node));

	buf_init();

	arc_thread_exit = 0;
	arc_eviction_list = NULL;
	mutex_init(&arc_eviction_mtx, NULL, MUTEX_DEFAULT, NULL);
	bzero(&arc_eviction_hdr, sizeof (arc_buf_hdr_t));

	arc_ksp = kstat_create("zfs", 0, "arcstats", "misc", KSTAT_TYPE_NAMED,
	    sizeof (arc_stats) / sizeof (kstat_named_t), KSTAT_FLAG_VIRTUAL);

	if (arc_ksp != NULL) {
		arc_ksp->ks_data = &arc_stats;
		kstat_install(arc_ksp);
	}

	(void) thread_create(NULL, 0, arc_reclaim_thread, NULL, 0, &p0,
	    TS_RUN, minclsyspri);

#ifdef _KERNEL
	arc_event_lowmem = EVENTHANDLER_REGISTER(vm_lowmem, arc_lowmem, NULL,
	    EVENTHANDLER_PRI_FIRST);
#endif

	arc_dead = FALSE;

#ifdef _KERNEL
	/* Warn about ZFS memory requirements. */
	if (((uint64_t)physmem * PAGESIZE) < (256 + 128 + 64) * (1 << 20)) {
		printf("ZFS WARNING: Recommended minimum RAM size is 512MB; "
		    "expect unstable behavior.\n");
	} else if (kmem_size() < 512 * (1 << 20)) {
		printf("ZFS WARNING: Recommended minimum kmem_size is 512MB; "
		    "expect unstable behavior.\n");
		printf("	     Consider tuning vm.kmem_size and "
		    "vm.kmem_size_max\n");
		printf("	     in /boot/loader.conf.\n");
	}
#endif
}

void
arc_fini(void)
{
	mutex_enter(&arc_reclaim_thr_lock);
	arc_thread_exit = 1;
	cv_signal(&arc_reclaim_thr_cv);
	while (arc_thread_exit != 0)
		cv_wait(&arc_reclaim_thr_cv, &arc_reclaim_thr_lock);
	mutex_exit(&arc_reclaim_thr_lock);

	arc_flush();

	arc_dead = TRUE;

	if (arc_ksp != NULL) {
		kstat_delete(arc_ksp);
		arc_ksp = NULL;
	}

	mutex_destroy(&arc_eviction_mtx);
	mutex_destroy(&arc_reclaim_thr_lock);
	cv_destroy(&arc_reclaim_thr_cv);

	list_destroy(&arc_mru->arcs_list);
	list_destroy(&arc_mru_ghost->arcs_list);
	list_destroy(&arc_mfu->arcs_list);
	list_destroy(&arc_mfu_ghost->arcs_list);

	mutex_destroy(&arc_anon->arcs_mtx);
	mutex_destroy(&arc_mru->arcs_mtx);
	mutex_destroy(&arc_mru_ghost->arcs_mtx);
	mutex_destroy(&arc_mfu->arcs_mtx);
	mutex_destroy(&arc_mfu_ghost->arcs_mtx);

	buf_fini();

	mutex_destroy(&arc_lowmem_lock);
#ifdef _KERNEL
	if (arc_event_lowmem != NULL)
		EVENTHANDLER_DEREGISTER(vm_lowmem, arc_event_lowmem);
#endif
}
