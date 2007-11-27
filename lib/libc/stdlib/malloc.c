/*-
 * Copyright (C) 2006,2007 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************
 *
 * This allocator implementation is designed to provide scalable performance
 * for multi-threaded programs on multi-processor systems.  The following
 * features are included for this purpose:
 *
 *   + Multiple arenas are used if there are multiple CPUs, which reduces lock
 *     contention and cache sloshing.
 *
 *   + Cache line sharing between arenas is avoided for internal data
 *     structures.
 *
 *   + Memory is managed in chunks and runs (chunks can be split into runs),
 *     rather than as individual pages.  This provides a constant-time
 *     mechanism for associating allocations with particular arenas.
 *
 * Allocation requests are rounded up to the nearest size class, and no record
 * of the original request size is maintained.  Allocations are broken into
 * categories according to size class.  Assuming runtime defaults, 4 kB pages
 * and a 16 byte quantum, the size classes in each category are as follows:
 *
 *   |=====================================|
 *   | Category | Subcategory    |    Size |
 *   |=====================================|
 *   | Small    | Tiny           |       2 |
 *   |          |                |       4 |
 *   |          |                |       8 |
 *   |          |----------------+---------|
 *   |          | Quantum-spaced |      16 |
 *   |          |                |      32 |
 *   |          |                |      48 |
 *   |          |                |     ... |
 *   |          |                |     480 |
 *   |          |                |     496 |
 *   |          |                |     512 |
 *   |          |----------------+---------|
 *   |          | Sub-page       |    1 kB |
 *   |          |                |    2 kB |
 *   |=====================================|
 *   | Large                     |    4 kB |
 *   |                           |    8 kB |
 *   |                           |   12 kB |
 *   |                           |     ... |
 *   |                           | 1012 kB |
 *   |                           | 1016 kB |
 *   |                           | 1020 kB |
 *   |=====================================|
 *   | Huge                      |    1 MB |
 *   |                           |    2 MB |
 *   |                           |    3 MB |
 *   |                           |     ... |
 *   |=====================================|
 *
 * A different mechanism is used for each category:
 *
 *   Small : Each size class is segregated into its own set of runs.  Each run
 *           maintains a bitmap of which regions are free/allocated.
 *
 *   Large : Each allocation is backed by a dedicated run.  Metadata are stored
 *           in the associated arena chunk header maps.
 *
 *   Huge : Each allocation is backed by a dedicated contiguous set of chunks.
 *          Metadata are stored in a separate red-black tree.
 *
 *******************************************************************************
 */

/*
 * MALLOC_PRODUCTION disables assertions and statistics gathering.  It also
 * defaults the A and J runtime options to off.  These settings are appropriate
 * for production systems.
 */
/* #define	MALLOC_PRODUCTION */

#ifndef MALLOC_PRODUCTION
#  define MALLOC_DEBUG
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "libc_private.h"
#ifdef MALLOC_DEBUG
#  define _LOCK_DEBUG
#endif
#include "spinlock.h"
#include "namespace.h"
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stddef.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <sys/ktrace.h> /* Must come after several other sys/ includes. */

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/vmparam.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "un-namespace.h"

/* MALLOC_STATS enables statistics calculation. */
#ifndef MALLOC_PRODUCTION
#  define MALLOC_STATS
#endif

#ifdef MALLOC_DEBUG
#  ifdef NDEBUG
#    undef NDEBUG
#  endif
#else
#  ifndef NDEBUG
#    define NDEBUG
#  endif
#endif
#include <assert.h>

#ifdef MALLOC_DEBUG
   /* Disable inlining to make debugging easier. */
#  define inline
#endif

/* Size of stack-allocated buffer passed to strerror_r(). */
#define	STRERROR_BUF		64

/* Minimum alignment of allocations is 2^QUANTUM_2POW_MIN bytes. */
#ifdef __i386__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR_2POW	2
#  define USE_BRK
#endif
#ifdef __ia64__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR_2POW	3
#endif
#ifdef __alpha__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR_2POW	3
#  define NO_TLS
#endif
#ifdef __sparc64__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR_2POW	3
#  define NO_TLS
#endif
#ifdef __amd64__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR_2POW	3
#endif
#ifdef __arm__
#  define QUANTUM_2POW_MIN	3
#  define SIZEOF_PTR_2POW	2
#  define USE_BRK
#  define NO_TLS
#endif
#ifdef __powerpc__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR_2POW	2
#  define USE_BRK
#endif

#define	SIZEOF_PTR		(1U << SIZEOF_PTR_2POW)

/* sizeof(int) == (1U << SIZEOF_INT_2POW). */
#ifndef SIZEOF_INT_2POW
#  define SIZEOF_INT_2POW	2
#endif

/* We can't use TLS in non-PIC programs, since TLS relies on loader magic. */
#if (!defined(PIC) && !defined(NO_TLS))
#  define NO_TLS
#endif

/*
 * Size and alignment of memory chunks that are allocated by the OS's virtual
 * memory system.
 */
#define	CHUNK_2POW_DEFAULT	20

/*
 * Maximum size of L1 cache line.  This is used to avoid cache line aliasing,
 * so over-estimates are okay (up to a point), but under-estimates will
 * negatively affect performance.
 */
#define	CACHELINE_2POW		6
#define	CACHELINE		((size_t)(1U << CACHELINE_2POW))

/* Smallest size class to support. */
#define	TINY_MIN_2POW		1

/*
 * Maximum size class that is a multiple of the quantum, but not (necessarily)
 * a power of 2.  Above this size, allocations are rounded up to the nearest
 * power of 2.
 */
#define	SMALL_MAX_2POW_DEFAULT	9
#define	SMALL_MAX_DEFAULT	(1U << SMALL_MAX_2POW_DEFAULT)

/*
 * Maximum desired run header overhead.  Runs are sized as small as possible
 * such that this setting is still honored, without violating other constraints.
 * The goal is to make runs as small as possible without exceeding a per run
 * external fragmentation threshold.
 *
 * Note that it is possible to set this low enough that it cannot be honored
 * for some/all object sizes, since there is one bit of header overhead per
 * object (plus a constant).  In such cases, this constraint is relaxed.
 *
 * RUN_MAX_OVRHD_RELAX specifies the maximum number of bits per region of
 * overhead for which RUN_MAX_OVRHD is relaxed.
 */
#define	RUN_MAX_OVRHD		0.015
#define	RUN_MAX_OVRHD_RELAX	1.5

/* Put a cap on small object run size.  This overrides RUN_MAX_OVRHD. */
#define	RUN_MAX_SMALL_2POW	15
#define	RUN_MAX_SMALL		(1U << RUN_MAX_SMALL_2POW)

/******************************************************************************/

/*
 * Mutexes based on spinlocks.  We can't use normal pthread mutexes, because
 * they require malloc()ed memory.
 */
typedef struct {
	spinlock_t	lock;
} malloc_mutex_t;

/* Set to true once the allocator has been initialized. */
static bool malloc_initialized = false;

/* Used to avoid initialization races. */
static malloc_mutex_t init_lock = {_SPINLOCK_INITIALIZER};

/******************************************************************************/
/*
 * Statistics data structures.
 */

#ifdef MALLOC_STATS

typedef struct malloc_bin_stats_s malloc_bin_stats_t;
struct malloc_bin_stats_s {
	/*
	 * Number of allocation requests that corresponded to the size of this
	 * bin.
	 */
	uint64_t	nrequests;

	/* Total number of runs created for this bin's size class. */
	uint64_t	nruns;

	/*
	 * Total number of runs reused by extracting them from the runs tree for
	 * this bin's size class.
	 */
	uint64_t	reruns;

	/* High-water mark for this bin. */
	unsigned long	highruns;

	/* Current number of runs in this bin. */
	unsigned long	curruns;
};

typedef struct arena_stats_s arena_stats_t;
struct arena_stats_s {
	/* Number of bytes currently mapped. */
	size_t		mapped;

	/* Per-size-category statistics. */
	size_t		allocated_small;
	uint64_t	nmalloc_small;
	uint64_t	ndalloc_small;

	size_t		allocated_large;
	uint64_t	nmalloc_large;
	uint64_t	ndalloc_large;
};

typedef struct chunk_stats_s chunk_stats_t;
struct chunk_stats_s {
	/* Number of chunks that were allocated. */
	uint64_t	nchunks;

	/* High-water mark for number of chunks allocated. */
	unsigned long	highchunks;

	/*
	 * Current number of chunks allocated.  This value isn't maintained for
	 * any other purpose, so keep track of it in order to be able to set
	 * highchunks.
	 */
	unsigned long	curchunks;
};

#endif /* #ifdef MALLOC_STATS */

/******************************************************************************/
/*
 * Chunk data structures.
 */

/* Tree of chunks. */
typedef struct chunk_node_s chunk_node_t;
struct chunk_node_s {
	/* Linkage for the chunk tree. */
	RB_ENTRY(chunk_node_s) link;

	/*
	 * Pointer to the chunk that this tree node is responsible for.  In some
	 * (but certainly not all) cases, this data structure is placed at the
	 * beginning of the corresponding chunk, so this field may point to this
	 * node.
	 */
	void	*chunk;

	/* Total chunk size. */
	size_t	size;
};
typedef struct chunk_tree_s chunk_tree_t;
RB_HEAD(chunk_tree_s, chunk_node_s);

/******************************************************************************/
/*
 * Arena data structures.
 */

typedef struct arena_s arena_t;
typedef struct arena_bin_s arena_bin_t;

typedef struct arena_chunk_map_s arena_chunk_map_t;
struct arena_chunk_map_s {
	/*
	 * Number of pages in run.  For a free run that has never been touched,
	 * this is NPAGES_EMPTY for the central pages, which allows us to avoid
	 * zero-filling untouched pages for calloc().
	 */
#define	NPAGES_EMPTY ((uint32_t)0x0U)
	uint32_t	npages;
	/*
	 * Position within run.  For a free run, this is POS_EMPTY/POS_FREE for
	 * the first and last pages.  The special values make it possible to
	 * quickly coalesce free runs.  POS_EMPTY indicates that the run has
	 * never been touched, which allows us to avoid zero-filling untouched
	 * pages for calloc().
	 *
	 * This is the limiting factor for chunksize; there can be at most 2^31
	 * pages in a run.
	 *
	 * POS_EMPTY is assumed by arena_run_dalloc() to be less than POS_FREE.
	 */
#define	POS_EMPTY ((uint32_t)0xfffffffeU)
#define	POS_FREE ((uint32_t)0xffffffffU)
	uint32_t	pos;
};

/* Arena chunk header. */
typedef struct arena_chunk_s arena_chunk_t;
struct arena_chunk_s {
	/* Arena that owns the chunk. */
	arena_t *arena;

	/* Linkage for the arena's chunk tree. */
	RB_ENTRY(arena_chunk_s) link;

	/*
	 * Number of pages in use.  This is maintained in order to make
	 * detection of empty chunks fast.
	 */
	uint32_t pages_used;

	/*
	 * Every time a free run larger than this value is created/coalesced,
	 * this value is increased.  The only way that the value decreases is if
	 * arena_run_alloc() fails to find a free run as large as advertised by
	 * this value.
	 */
	uint32_t max_frun_npages;

	/*
	 * Every time a free run that starts at an earlier page than this value
	 * is created/coalesced, this value is decreased.  It is reset in a
	 * similar fashion to max_frun_npages.
	 */
	uint32_t min_frun_ind;

	/*
	 * Map of pages within chunk that keeps track of free/large/small.  For
	 * free runs, only the map entries for the first and last pages are
	 * kept up to date, so that free runs can be quickly coalesced.
	 */
	arena_chunk_map_t map[1]; /* Dynamically sized. */
};
typedef struct arena_chunk_tree_s arena_chunk_tree_t;
RB_HEAD(arena_chunk_tree_s, arena_chunk_s);

typedef struct arena_run_s arena_run_t;
struct arena_run_s {
	/* Linkage for run trees. */
	RB_ENTRY(arena_run_s) link;

#ifdef MALLOC_DEBUG
	uint32_t	magic;
#  define ARENA_RUN_MAGIC 0x384adf93
#endif

	/* Bin this run is associated with. */
	arena_bin_t	*bin;

	/* Index of first element that might have a free region. */
	unsigned	regs_minelm;

	/* Number of free regions in run. */
	unsigned	nfree;

	/* Bitmask of in-use regions (0: in use, 1: free). */
	unsigned	regs_mask[1]; /* Dynamically sized. */
};
typedef struct arena_run_tree_s arena_run_tree_t;
RB_HEAD(arena_run_tree_s, arena_run_s);

struct arena_bin_s {
	/*
	 * Current run being used to service allocations of this bin's size
	 * class.
	 */
	arena_run_t	*runcur;

	/*
	 * Tree of non-full runs.  This tree is used when looking for an
	 * existing run when runcur is no longer usable.  We choose the
	 * non-full run that is lowest in memory; this policy tends to keep
	 * objects packed well, and it can also help reduce the number of
	 * almost-empty chunks.
	 */
	arena_run_tree_t runs;

	/* Size of regions in a run for this bin's size class. */
	size_t		reg_size;

	/* Total size of a run for this bin's size class. */
	size_t		run_size;

	/* Total number of regions in a run for this bin's size class. */
	uint32_t	nregs;

	/* Number of elements in a run's regs_mask for this bin's size class. */
	uint32_t	regs_mask_nelms;

	/* Offset of first region in a run for this bin's size class. */
	uint32_t	reg0_offset;

#ifdef MALLOC_STATS
	/* Bin statistics. */
	malloc_bin_stats_t stats;
#endif
};

struct arena_s {
#ifdef MALLOC_DEBUG
	uint32_t		magic;
#  define ARENA_MAGIC 0x947d3d24
#endif

	/* All operations on this arena require that mtx be locked. */
	malloc_mutex_t		mtx;

#ifdef MALLOC_STATS
	arena_stats_t		stats;
#endif

	/*
	 * Tree of chunks this arena manages.
	 */
	arena_chunk_tree_t	chunks;

	/*
	 * In order to avoid rapid chunk allocation/deallocation when an arena
	 * oscillates right on the cusp of needing a new chunk, cache the most
	 * recently freed chunk.  This caching is disabled by opt_hint.
	 *
	 * There is one spare chunk per arena, rather than one spare total, in
	 * order to avoid interactions between multiple threads that could make
	 * a single spare inadequate.
	 */
	arena_chunk_t		*spare;

	/*
	 * bins is used to store rings of free regions of the following sizes,
	 * assuming a 16-byte quantum, 4kB pagesize, and default MALLOC_OPTIONS.
	 *
	 *   bins[i] | size |
	 *   --------+------+
	 *        0  |    2 |
	 *        1  |    4 |
	 *        2  |    8 |
	 *   --------+------+
	 *        3  |   16 |
	 *        4  |   32 |
	 *        5  |   48 |
	 *        6  |   64 |
	 *           :      :
	 *           :      :
	 *       33  |  496 |
	 *       34  |  512 |
	 *   --------+------+
	 *       35  | 1024 |
	 *       36  | 2048 |
	 *   --------+------+
	 */
	arena_bin_t		bins[1]; /* Dynamically sized. */
};

/******************************************************************************/
/*
 * Data.
 */

/* Number of CPUs. */
static unsigned		ncpus;

/* VM page size. */
static size_t		pagesize;
static size_t		pagesize_mask;
static size_t		pagesize_2pow;

/* Various bin-related settings. */
static size_t		bin_maxclass; /* Max size class for bins. */
static unsigned		ntbins; /* Number of (2^n)-spaced tiny bins. */
static unsigned		nqbins; /* Number of quantum-spaced bins. */
static unsigned		nsbins; /* Number of (2^n)-spaced sub-page bins. */
static size_t		small_min;
static size_t		small_max;

/* Various quantum-related settings. */
static size_t		quantum;
static size_t		quantum_mask; /* (quantum - 1). */

/* Various chunk-related settings. */
static size_t		chunksize;
static size_t		chunksize_mask; /* (chunksize - 1). */
static unsigned		chunk_npages;
static unsigned		arena_chunk_header_npages;
static size_t		arena_maxclass; /* Max size class for arenas. */

/********/
/*
 * Chunks.
 */

/* Protects chunk-related data structures. */
static malloc_mutex_t	chunks_mtx;

/* Tree of chunks that are stand-alone huge allocations. */
static chunk_tree_t	huge;

#ifdef USE_BRK
/*
 * Try to use brk for chunk-size allocations, due to address space constraints.
 */
/*
 * Protects sbrk() calls.  This must be separate from chunks_mtx, since
 * base_pages_alloc() also uses sbrk(), but cannot lock chunks_mtx (doing so
 * could cause recursive lock acquisition).
 */
static malloc_mutex_t	brk_mtx;
/* Result of first sbrk(0) call. */
static void		*brk_base;
/* Current end of brk, or ((void *)-1) if brk is exhausted. */
static void		*brk_prev;
/* Current upper limit on brk addresses. */
static void		*brk_max;
#endif

#ifdef MALLOC_STATS
/* Huge allocation statistics. */
static uint64_t		huge_nmalloc;
static uint64_t		huge_ndalloc;
static size_t		huge_allocated;
#endif

/*
 * Tree of chunks that were previously allocated.  This is used when allocating
 * chunks, in an attempt to re-use address space.
 */
static chunk_tree_t	old_chunks;

/****************************/
/*
 * base (internal allocation).
 */

/*
 * Current pages that are being used for internal memory allocations.  These
 * pages are carved up in cacheline-size quanta, so that there is no chance of
 * false cache line sharing.
 */
static void		*base_pages;
static void		*base_next_addr;
static void		*base_past_addr; /* Addr immediately past base_pages. */
static chunk_node_t	*base_chunk_nodes; /* LIFO cache of chunk nodes. */
static malloc_mutex_t	base_mtx;
#ifdef MALLOC_STATS
static size_t		base_mapped;
#endif

/********/
/*
 * Arenas.
 */

/*
 * Arenas that are used to service external requests.  Not all elements of the
 * arenas array are necessarily used; arenas are created lazily as needed.
 */
static arena_t		**arenas;
static unsigned		narenas;
#ifndef NO_TLS
static unsigned		next_arena;
#endif
static malloc_mutex_t	arenas_mtx; /* Protects arenas initialization. */

#ifndef NO_TLS
/*
 * Map of pthread_self() --> arenas[???], used for selecting an arena to use
 * for allocations.
 */
static __thread arena_t	*arenas_map;
#endif

#ifdef MALLOC_STATS
/* Chunk statistics. */
static chunk_stats_t	stats_chunks;
#endif

/*******************************/
/*
 * Runtime configuration options.
 */
const char	*_malloc_options;

#ifndef MALLOC_PRODUCTION
static bool	opt_abort = true;
static bool	opt_junk = true;
#else
static bool	opt_abort = false;
static bool	opt_junk = false;
#endif
static bool	opt_hint = false;
static bool	opt_print_stats = false;
static size_t	opt_quantum_2pow = QUANTUM_2POW_MIN;
static size_t	opt_small_max_2pow = SMALL_MAX_2POW_DEFAULT;
static size_t	opt_chunk_2pow = CHUNK_2POW_DEFAULT;
static bool	opt_utrace = false;
static bool	opt_sysv = false;
static bool	opt_xmalloc = false;
static bool	opt_zero = false;
static int	opt_narenas_lshift = 0;

typedef struct {
	void	*p;
	size_t	s;
	void	*r;
} malloc_utrace_t;

#define	UTRACE(a, b, c)							\
	if (opt_utrace) {						\
		malloc_utrace_t ut = {a, b, c};				\
		utrace(&ut, sizeof(ut));				\
	}

/******************************************************************************/
/*
 * Begin function prototypes for non-inline static functions.
 */

static void	malloc_mutex_init(malloc_mutex_t *a_mutex);
static void	wrtmessage(const char *p1, const char *p2, const char *p3,
		const char *p4);
#ifdef MALLOC_STATS
static void	malloc_printf(const char *format, ...);
#endif
static char	*umax2s(uintmax_t x, char *s);
static bool	base_pages_alloc(size_t minsize);
static void	*base_alloc(size_t size);
static chunk_node_t *base_chunk_node_alloc(void);
static void	base_chunk_node_dealloc(chunk_node_t *node);
#ifdef MALLOC_STATS
static void	stats_print(arena_t *arena);
#endif
static void	*pages_map(void *addr, size_t size);
static void	pages_unmap(void *addr, size_t size);
static void	*chunk_alloc(size_t size);
static void	chunk_dealloc(void *chunk, size_t size);
#ifndef NO_TLS
static arena_t	*choose_arena_hard(void);
#endif
static void	arena_run_split(arena_t *arena, arena_run_t *run, size_t size,
    bool zero);
static arena_chunk_t *arena_chunk_alloc(arena_t *arena);
static void	arena_chunk_dealloc(arena_t *arena, arena_chunk_t *chunk);
static arena_run_t *arena_run_alloc(arena_t *arena, size_t size, bool zero);
static void	arena_run_dalloc(arena_t *arena, arena_run_t *run, size_t size);
static arena_run_t *arena_bin_nonfull_run_get(arena_t *arena, arena_bin_t *bin);
static void *arena_bin_malloc_hard(arena_t *arena, arena_bin_t *bin);
static size_t arena_bin_run_size_calc(arena_bin_t *bin, size_t min_run_size);
static void	*arena_malloc(arena_t *arena, size_t size, bool zero);
static void	*arena_palloc(arena_t *arena, size_t alignment, size_t size,
    size_t alloc_size);
static size_t	arena_salloc(const void *ptr);
static void	*arena_ralloc(void *ptr, size_t size, size_t oldsize);
static void	arena_dalloc(arena_t *arena, arena_chunk_t *chunk, void *ptr);
static bool	arena_new(arena_t *arena);
static arena_t	*arenas_extend(unsigned ind);
static void	*huge_malloc(size_t size, bool zero);
static void	*huge_palloc(size_t alignment, size_t size);
static void	*huge_ralloc(void *ptr, size_t size, size_t oldsize);
static void	huge_dalloc(void *ptr);
static void	*imalloc(size_t size);
static void	*ipalloc(size_t alignment, size_t size);
static void	*icalloc(size_t size);
static size_t	isalloc(const void *ptr);
static void	*iralloc(void *ptr, size_t size);
static void	idalloc(void *ptr);
static void	malloc_print_stats(void);
static bool	malloc_init_hard(void);

/*
 * End function prototypes.
 */
/******************************************************************************/
/*
 * Begin mutex.
 */

static void
malloc_mutex_init(malloc_mutex_t *a_mutex)
{
	static const spinlock_t lock = _SPINLOCK_INITIALIZER;

	a_mutex->lock = lock;
}

static inline void
malloc_mutex_lock(malloc_mutex_t *a_mutex)
{

	if (__isthreaded)
		_SPINLOCK(&a_mutex->lock);
}

static inline void
malloc_mutex_unlock(malloc_mutex_t *a_mutex)
{

	if (__isthreaded)
		_SPINUNLOCK(&a_mutex->lock);
}

/*
 * End mutex.
 */
/******************************************************************************/
/*
 * Begin Utility functions/macros.
 */

/* Return the chunk address for allocation address a. */
#define	CHUNK_ADDR2BASE(a)						\
	((void *)((uintptr_t)(a) & ~chunksize_mask))

/* Return the chunk offset of address a. */
#define	CHUNK_ADDR2OFFSET(a)						\
	((size_t)((uintptr_t)(a) & chunksize_mask))

/* Return the smallest chunk multiple that is >= s. */
#define	CHUNK_CEILING(s)						\
	(((s) + chunksize_mask) & ~chunksize_mask)

/* Return the smallest cacheline multiple that is >= s. */
#define	CACHELINE_CEILING(s)						\
	(((s) + (CACHELINE - 1)) & ~(CACHELINE - 1))

/* Return the smallest quantum multiple that is >= a. */
#define	QUANTUM_CEILING(a)						\
	(((a) + quantum_mask) & ~quantum_mask)

/* Return the smallest pagesize multiple that is >= s. */
#define	PAGE_CEILING(s)							\
	(((s) + pagesize_mask) & ~pagesize_mask)

/* Compute the smallest power of 2 that is >= x. */
static inline size_t
pow2_ceil(size_t x)
{

	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
#if (SIZEOF_PTR == 8)
	x |= x >> 32;
#endif
	x++;
	return (x);
}

static void
wrtmessage(const char *p1, const char *p2, const char *p3, const char *p4)
{

	_write(STDERR_FILENO, p1, strlen(p1));
	_write(STDERR_FILENO, p2, strlen(p2));
	_write(STDERR_FILENO, p3, strlen(p3));
	_write(STDERR_FILENO, p4, strlen(p4));
}

void	(*_malloc_message)(const char *p1, const char *p2, const char *p3,
	    const char *p4) = wrtmessage;

#ifdef MALLOC_STATS
/*
 * Print to stderr in such a way as to (hopefully) avoid memory allocation.
 */
static void
malloc_printf(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	_malloc_message(buf, "", "", "");
}
#endif

/*
 * We don't want to depend on vsnprintf() for production builds, since that can
 * cause unnecessary bloat for static binaries.  umax2s() provides minimal
 * integer printing functionality, so that malloc_printf() use can be limited to
 * MALLOC_STATS code.
 */
#define	UMAX2S_BUFSIZE	21
static char *
umax2s(uintmax_t x, char *s)
{
	unsigned i;

	/* Make sure UMAX2S_BUFSIZE is large enough. */
	assert(sizeof(uintmax_t) <= 8);

	i = UMAX2S_BUFSIZE - 1;
	s[i] = '\0';
	do {
		i--;
		s[i] = "0123456789"[x % 10];
		x /= 10;
	} while (x > 0);

	return (&s[i]);
}

/******************************************************************************/

static bool
base_pages_alloc(size_t minsize)
{
	size_t csize;

#ifdef USE_BRK
	/*
	 * Do special brk allocation here, since base allocations don't need to
	 * be chunk-aligned.
	 */
	if (brk_prev != (void *)-1) {
		void *brk_cur;
		intptr_t incr;

		if (minsize != 0)
			csize = CHUNK_CEILING(minsize);

		malloc_mutex_lock(&brk_mtx);
		do {
			/* Get the current end of brk. */
			brk_cur = sbrk(0);

			/*
			 * Calculate how much padding is necessary to
			 * chunk-align the end of brk.  Don't worry about
			 * brk_cur not being chunk-aligned though.
			 */
			incr = (intptr_t)chunksize
			    - (intptr_t)CHUNK_ADDR2OFFSET(brk_cur);
			if (incr < minsize)
				incr += csize;

			brk_prev = sbrk(incr);
			if (brk_prev == brk_cur) {
				/* Success. */
				malloc_mutex_unlock(&brk_mtx);
				base_pages = brk_cur;
				base_next_addr = base_pages;
				base_past_addr = (void *)((uintptr_t)base_pages
				    + incr);
#ifdef MALLOC_STATS
				base_mapped += incr;
#endif
				return (false);
			}
		} while (brk_prev != (void *)-1);
		malloc_mutex_unlock(&brk_mtx);
	}
	if (minsize == 0) {
		/*
		 * Failure during initialization doesn't matter, so avoid
		 * falling through to the mmap-based page mapping code.
		 */
		return (true);
	}
#endif
	assert(minsize != 0);
	csize = PAGE_CEILING(minsize);
	base_pages = pages_map(NULL, csize);
	if (base_pages == NULL)
		return (true);
	base_next_addr = base_pages;
	base_past_addr = (void *)((uintptr_t)base_pages + csize);
#ifdef MALLOC_STATS
	base_mapped += csize;
#endif
	return (false);
}

static void *
base_alloc(size_t size)
{
	void *ret;
	size_t csize;

	/* Round size up to nearest multiple of the cacheline size. */
	csize = CACHELINE_CEILING(size);

	malloc_mutex_lock(&base_mtx);

	/* Make sure there's enough space for the allocation. */
	if ((uintptr_t)base_next_addr + csize > (uintptr_t)base_past_addr) {
		if (base_pages_alloc(csize)) {
			ret = NULL;
			goto RETURN;
		}
	}

	/* Allocate. */
	ret = base_next_addr;
	base_next_addr = (void *)((uintptr_t)base_next_addr + csize);

RETURN:
	malloc_mutex_unlock(&base_mtx);
	return (ret);
}

static chunk_node_t *
base_chunk_node_alloc(void)
{
	chunk_node_t *ret;

	malloc_mutex_lock(&base_mtx);
	if (base_chunk_nodes != NULL) {
		ret = base_chunk_nodes;
		base_chunk_nodes = *(chunk_node_t **)ret;
		malloc_mutex_unlock(&base_mtx);
	} else {
		malloc_mutex_unlock(&base_mtx);
		ret = (chunk_node_t *)base_alloc(sizeof(chunk_node_t));
	}

	return (ret);
}

static void
base_chunk_node_dealloc(chunk_node_t *node)
{

	malloc_mutex_lock(&base_mtx);
	*(chunk_node_t **)node = base_chunk_nodes;
	base_chunk_nodes = node;
	malloc_mutex_unlock(&base_mtx);
}

/******************************************************************************/

#ifdef MALLOC_STATS
static void
stats_print(arena_t *arena)
{
	unsigned i;
	int gap_start;

	malloc_printf(
	    "          allocated/mapped            nmalloc      ndalloc\n");
	malloc_printf("small: %12llu %-12s %12llu %12llu\n",
	    arena->stats.allocated_small, "", arena->stats.nmalloc_small,
	    arena->stats.ndalloc_small);
	malloc_printf("large: %12llu %-12s %12llu %12llu\n",
	    arena->stats.allocated_large, "", arena->stats.nmalloc_large,
	    arena->stats.ndalloc_large);
	malloc_printf("total: %12llu/%-12llu %12llu %12llu\n",
	    arena->stats.allocated_small + arena->stats.allocated_large,
	    arena->stats.mapped,
	    arena->stats.nmalloc_small + arena->stats.nmalloc_large,
	    arena->stats.ndalloc_small + arena->stats.ndalloc_large);

	malloc_printf("bins:     bin   size regs pgs  requests   newruns"
	    "    reruns maxruns curruns\n");
	for (i = 0, gap_start = -1; i < ntbins + nqbins + nsbins; i++) {
		if (arena->bins[i].stats.nrequests == 0) {
			if (gap_start == -1)
				gap_start = i;
		} else {
			if (gap_start != -1) {
				if (i > gap_start + 1) {
					/* Gap of more than one size class. */
					malloc_printf("[%u..%u]\n",
					    gap_start, i - 1);
				} else {
					/* Gap of one size class. */
					malloc_printf("[%u]\n", gap_start);
				}
				gap_start = -1;
			}
			malloc_printf(
			    "%13u %1s %4u %4u %3u %9llu %9llu"
			    " %9llu %7lu %7lu\n",
			    i,
			    i < ntbins ? "T" : i < ntbins + nqbins ? "Q" : "S",
			    arena->bins[i].reg_size,
			    arena->bins[i].nregs,
			    arena->bins[i].run_size >> pagesize_2pow,
			    arena->bins[i].stats.nrequests,
			    arena->bins[i].stats.nruns,
			    arena->bins[i].stats.reruns,
			    arena->bins[i].stats.highruns,
			    arena->bins[i].stats.curruns);
		}
	}
	if (gap_start != -1) {
		if (i > gap_start + 1) {
			/* Gap of more than one size class. */
			malloc_printf("[%u..%u]\n", gap_start, i - 1);
		} else {
			/* Gap of one size class. */
			malloc_printf("[%u]\n", gap_start);
		}
	}
}
#endif

/*
 * End Utility functions/macros.
 */
/******************************************************************************/
/*
 * Begin chunk management functions.
 */

static inline int
chunk_comp(chunk_node_t *a, chunk_node_t *b)
{

	assert(a != NULL);
	assert(b != NULL);

	if ((uintptr_t)a->chunk < (uintptr_t)b->chunk)
		return (-1);
	else if (a->chunk == b->chunk)
		return (0);
	else
		return (1);
}

/* Generate red-black tree code for chunks. */
RB_GENERATE_STATIC(chunk_tree_s, chunk_node_s, link, chunk_comp);

static void *
pages_map(void *addr, size_t size)
{
	void *ret;

	/*
	 * We don't use MAP_FIXED here, because it can cause the *replacement*
	 * of existing mappings, and we only want to create new mappings.
	 */
	ret = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
	    -1, 0);
	assert(ret != NULL);

	if (ret == MAP_FAILED)
		ret = NULL;
	else if (addr != NULL && ret != addr) {
		/*
		 * We succeeded in mapping memory, but not in the right place.
		 */
		if (munmap(ret, size) == -1) {
			char buf[STRERROR_BUF];

			strerror_r(errno, buf, sizeof(buf));
			_malloc_message(_getprogname(),
			    ": (malloc) Error in munmap(): ", buf, "\n");
			if (opt_abort)
				abort();
		}
		ret = NULL;
	}

	assert(ret == NULL || (addr == NULL && ret != addr)
	    || (addr != NULL && ret == addr));
	return (ret);
}

static void
pages_unmap(void *addr, size_t size)
{

	if (munmap(addr, size) == -1) {
		char buf[STRERROR_BUF];

		strerror_r(errno, buf, sizeof(buf));
		_malloc_message(_getprogname(),
		    ": (malloc) Error in munmap(): ", buf, "\n");
		if (opt_abort)
			abort();
	}
}

static void *
chunk_alloc(size_t size)
{
	void *ret, *chunk;
	chunk_node_t *tchunk, *delchunk;

	assert(size != 0);
	assert((size & chunksize_mask) == 0);

	malloc_mutex_lock(&chunks_mtx);

	if (size == chunksize) {
		/*
		 * Check for address ranges that were previously chunks and try
		 * to use them.
		 */

		tchunk = RB_MIN(chunk_tree_s, &old_chunks);
		while (tchunk != NULL) {
			/* Found an address range.  Try to recycle it. */

			chunk = tchunk->chunk;
			delchunk = tchunk;
			tchunk = RB_NEXT(chunk_tree_s, &old_chunks, delchunk);

			/* Remove delchunk from the tree. */
			RB_REMOVE(chunk_tree_s, &old_chunks, delchunk);
			base_chunk_node_dealloc(delchunk);

#ifdef USE_BRK
			if ((uintptr_t)chunk >= (uintptr_t)brk_base
			    && (uintptr_t)chunk < (uintptr_t)brk_max) {
				/* Re-use a previously freed brk chunk. */
				ret = chunk;
				/*
				 * Maintain invariant that all newly allocated
				 * chunks are untouched or zero-filled.
				 */
				memset(ret, 0, size);
				goto RETURN;
			}
#endif
			if ((ret = pages_map(chunk, size)) != NULL) {
				/* Success. */
				goto RETURN;
			}
		}
	}

	/*
	 * Try to over-allocate, but allow the OS to place the allocation
	 * anywhere.  Beware of size_t wrap-around.
	 */
	if (size + chunksize > size) {
		if ((ret = pages_map(NULL, size + chunksize)) != NULL) {
			size_t offset = CHUNK_ADDR2OFFSET(ret);

			/*
			 * Success.  Clean up unneeded leading/trailing space.
			 */
			if (offset != 0) {
				/* Leading space. */
				pages_unmap(ret, chunksize - offset);

				ret = (void *)((uintptr_t)ret + (chunksize -
				    offset));

				/* Trailing space. */
				pages_unmap((void *)((uintptr_t)ret + size),
				    offset);
			} else {
				/* Trailing space only. */
				pages_unmap((void *)((uintptr_t)ret + size),
				    chunksize);
			}
			goto RETURN;
		}
	}

#ifdef USE_BRK
	/*
	 * Try to create allocations in brk, in order to make full use of
	 * limited address space.
	 */
	if (brk_prev != (void *)-1) {
		void *brk_cur;
		intptr_t incr;

		/*
		 * The loop is necessary to recover from races with other
		 * threads that are using brk for something other than malloc.
		 */
		malloc_mutex_lock(&brk_mtx);
		do {
			/* Get the current end of brk. */
			brk_cur = sbrk(0);

			/*
			 * Calculate how much padding is necessary to
			 * chunk-align the end of brk.
			 */
			incr = (intptr_t)size
			    - (intptr_t)CHUNK_ADDR2OFFSET(brk_cur);
			if (incr == size) {
				ret = brk_cur;
			} else {
				ret = (void *)((intptr_t)brk_cur + incr);
				incr += size;
			}

			brk_prev = sbrk(incr);
			if (brk_prev == brk_cur) {
				/* Success. */
				malloc_mutex_unlock(&brk_mtx);
				brk_max = (void *)((intptr_t)ret + size);
				goto RETURN;
			}
		} while (brk_prev != (void *)-1);
		malloc_mutex_unlock(&brk_mtx);
	}
#endif

	/* All strategies for allocation failed. */
	ret = NULL;
RETURN:
	if (ret != NULL) {
		chunk_node_t key;
		/*
		 * Clean out any entries in old_chunks that overlap with the
		 * memory we just allocated.
		 */
		key.chunk = ret;
		tchunk = RB_NFIND(chunk_tree_s, &old_chunks, &key);
		while (tchunk != NULL
		    && (uintptr_t)tchunk->chunk >= (uintptr_t)ret
		    && (uintptr_t)tchunk->chunk < (uintptr_t)ret + size) {
			delchunk = tchunk;
			tchunk = RB_NEXT(chunk_tree_s, &old_chunks, delchunk);
			RB_REMOVE(chunk_tree_s, &old_chunks, delchunk);
			base_chunk_node_dealloc(delchunk);
		}

	}
#ifdef MALLOC_STATS
	if (ret != NULL) {
		stats_chunks.nchunks += (size / chunksize);
		stats_chunks.curchunks += (size / chunksize);
	}
	if (stats_chunks.curchunks > stats_chunks.highchunks)
		stats_chunks.highchunks = stats_chunks.curchunks;
#endif
	malloc_mutex_unlock(&chunks_mtx);

	assert(CHUNK_ADDR2BASE(ret) == ret);
	return (ret);
}

static void
chunk_dealloc(void *chunk, size_t size)
{
	chunk_node_t *node;

	assert(chunk != NULL);
	assert(CHUNK_ADDR2BASE(chunk) == chunk);
	assert(size != 0);
	assert((size & chunksize_mask) == 0);

	malloc_mutex_lock(&chunks_mtx);

#ifdef USE_BRK
	if ((uintptr_t)chunk >= (uintptr_t)brk_base
	    && (uintptr_t)chunk < (uintptr_t)brk_max) {
		void *brk_cur;

		malloc_mutex_lock(&brk_mtx);
		/* Get the current end of brk. */
		brk_cur = sbrk(0);

		/*
		 * Try to shrink the data segment if this chunk is at the end
		 * of the data segment.  The sbrk() call here is subject to a
		 * race condition with threads that use brk(2) or sbrk(2)
		 * directly, but the alternative would be to leak memory for
		 * the sake of poorly designed multi-threaded programs.
		 */
		if (brk_cur == brk_max
		    && (void *)((uintptr_t)chunk + size) == brk_max
		    && sbrk(-(intptr_t)size) == brk_max) {
			malloc_mutex_unlock(&brk_mtx);
			if (brk_prev == brk_max) {
				/* Success. */
				brk_prev = (void *)((intptr_t)brk_max
				    - (intptr_t)size);
				brk_max = brk_prev;
			}
		} else {
			size_t offset;

			malloc_mutex_unlock(&brk_mtx);
			madvise(chunk, size, MADV_FREE);

			/*
			 * Iteratively create records of each chunk-sized
			 * memory region that 'chunk' is comprised of, so that
			 * the address range can be recycled if memory usage
			 * increases later on.
			 */
			for (offset = 0; offset < size; offset += chunksize) {
				node = base_chunk_node_alloc();
				if (node == NULL)
					break;

				node->chunk = (void *)((uintptr_t)chunk
				    + (uintptr_t)offset);
				node->size = chunksize;
				RB_INSERT(chunk_tree_s, &old_chunks, node);
			}
		}
	} else {
#endif
		pages_unmap(chunk, size);

		/*
		 * Make a record of the chunk's address, so that the address
		 * range can be recycled if memory usage increases later on.
		 * Don't bother to create entries if (size > chunksize), since
		 * doing so could cause scalability issues for truly gargantuan
		 * objects (many gigabytes or larger).
		 */
		if (size == chunksize) {
			node = base_chunk_node_alloc();
			if (node != NULL) {
				node->chunk = (void *)(uintptr_t)chunk;
				node->size = chunksize;
				RB_INSERT(chunk_tree_s, &old_chunks, node);
			}
		}
#ifdef USE_BRK
	}
#endif

#ifdef MALLOC_STATS
	stats_chunks.curchunks -= (size / chunksize);
#endif
	malloc_mutex_unlock(&chunks_mtx);
}

/*
 * End chunk management functions.
 */
/******************************************************************************/
/*
 * Begin arena.
 */

/*
 * Choose an arena based on a per-thread value (fast-path code, calls slow-path
 * code if necessary).
 */
static inline arena_t *
choose_arena(void)
{
	arena_t *ret;

	/*
	 * We can only use TLS if this is a PIC library, since for the static
	 * library version, libc's malloc is used by TLS allocation, which
	 * introduces a bootstrapping issue.
	 */
#ifndef NO_TLS
	if (__isthreaded == false) {
	    /*
	     * Avoid the overhead of TLS for single-threaded operation.  If the
	     * app switches to threaded mode, the initial thread may end up
	     * being assigned to some other arena, but this one-time switch
	     * shouldn't cause significant issues.
	     */
	    return (arenas[0]);
	}

	ret = arenas_map;
	if (ret == NULL) {
		ret = choose_arena_hard();
		assert(ret != NULL);
	}
#else
	if (__isthreaded) {
		unsigned long ind;

		/*
		 * Hash _pthread_self() to one of the arenas.  There is a prime
		 * number of arenas, so this has a reasonable chance of
		 * working.  Even so, the hashing can be easily thwarted by
		 * inconvenient _pthread_self() values.  Without specific
		 * knowledge of how _pthread_self() calculates values, we can't
		 * easily do much better than this.
		 */
		ind = (unsigned long) _pthread_self() % narenas;

		/*
		 * Optimistially assume that arenas[ind] has been initialized.
		 * At worst, we find out that some other thread has already
		 * done so, after acquiring the lock in preparation.  Note that
		 * this lazy locking also has the effect of lazily forcing
		 * cache coherency; without the lock acquisition, there's no
		 * guarantee that modification of arenas[ind] by another thread
		 * would be seen on this CPU for an arbitrary amount of time.
		 *
		 * In general, this approach to modifying a synchronized value
		 * isn't a good idea, but in this case we only ever modify the
		 * value once, so things work out well.
		 */
		ret = arenas[ind];
		if (ret == NULL) {
			/*
			 * Avoid races with another thread that may have already
			 * initialized arenas[ind].
			 */
			malloc_mutex_lock(&arenas_mtx);
			if (arenas[ind] == NULL)
				ret = arenas_extend((unsigned)ind);
			else
				ret = arenas[ind];
			malloc_mutex_unlock(&arenas_mtx);
		}
	} else
		ret = arenas[0];
#endif

	assert(ret != NULL);
	return (ret);
}

#ifndef NO_TLS
/*
 * Choose an arena based on a per-thread value (slow-path code only, called
 * only by choose_arena()).
 */
static arena_t *
choose_arena_hard(void)
{
	arena_t *ret;

	assert(__isthreaded);

	/* Assign one of the arenas to this thread, in a round-robin fashion. */
	malloc_mutex_lock(&arenas_mtx);
	ret = arenas[next_arena];
	if (ret == NULL)
		ret = arenas_extend(next_arena);
	if (ret == NULL) {
		/*
		 * Make sure that this function never returns NULL, so that
		 * choose_arena() doesn't have to check for a NULL return
		 * value.
		 */
		ret = arenas[0];
	}
	next_arena = (next_arena + 1) % narenas;
	malloc_mutex_unlock(&arenas_mtx);
	arenas_map = ret;

	return (ret);
}
#endif

static inline int
arena_chunk_comp(arena_chunk_t *a, arena_chunk_t *b)
{

	assert(a != NULL);
	assert(b != NULL);

	if ((uintptr_t)a < (uintptr_t)b)
		return (-1);
	else if (a == b)
		return (0);
	else
		return (1);
}

/* Generate red-black tree code for arena chunks. */
RB_GENERATE_STATIC(arena_chunk_tree_s, arena_chunk_s, link, arena_chunk_comp);

static inline int
arena_run_comp(arena_run_t *a, arena_run_t *b)
{

	assert(a != NULL);
	assert(b != NULL);

	if ((uintptr_t)a < (uintptr_t)b)
		return (-1);
	else if (a == b)
		return (0);
	else
		return (1);
}

/* Generate red-black tree code for arena runs. */
RB_GENERATE_STATIC(arena_run_tree_s, arena_run_s, link, arena_run_comp);

static inline void *
arena_run_reg_alloc(arena_run_t *run, arena_bin_t *bin)
{
	void *ret;
	unsigned i, mask, bit, regind;

	assert(run->magic == ARENA_RUN_MAGIC);
	assert(run->regs_minelm < bin->regs_mask_nelms);

	/*
	 * Move the first check outside the loop, so that run->regs_minelm can
	 * be updated unconditionally, without the possibility of updating it
	 * multiple times.
	 */
	i = run->regs_minelm;
	mask = run->regs_mask[i];
	if (mask != 0) {
		/* Usable allocation found. */
		bit = ffs((int)mask) - 1;

		regind = ((i << (SIZEOF_INT_2POW + 3)) + bit);
		ret = (void *)(((uintptr_t)run) + bin->reg0_offset
		    + (bin->reg_size * regind));

		/* Clear bit. */
		mask ^= (1U << bit);
		run->regs_mask[i] = mask;

		return (ret);
	}

	for (i++; i < bin->regs_mask_nelms; i++) {
		mask = run->regs_mask[i];
		if (mask != 0) {
			/* Usable allocation found. */
			bit = ffs((int)mask) - 1;

			regind = ((i << (SIZEOF_INT_2POW + 3)) + bit);
			ret = (void *)(((uintptr_t)run) + bin->reg0_offset
			    + (bin->reg_size * regind));

			/* Clear bit. */
			mask ^= (1U << bit);
			run->regs_mask[i] = mask;

			/*
			 * Make a note that nothing before this element
			 * contains a free region.
			 */
			run->regs_minelm = i; /* Low payoff: + (mask == 0); */

			return (ret);
		}
	}
	/* Not reached. */
	assert(0);
	return (NULL);
}

static inline void
arena_run_reg_dalloc(arena_run_t *run, arena_bin_t *bin, void *ptr, size_t size)
{
	/*
	 * To divide by a number D that is not a power of two we multiply
	 * by (2^21 / D) and then right shift by 21 positions.
	 *
	 *   X / D
	 *
	 * becomes
	 *
	 *   (X * size_invs[(D >> QUANTUM_2POW_MIN) - 3]) >> SIZE_INV_SHIFT
	 */
#define	SIZE_INV_SHIFT 21
#define	SIZE_INV(s) (((1U << SIZE_INV_SHIFT) / (s << QUANTUM_2POW_MIN)) + 1)
	static const unsigned size_invs[] = {
	    SIZE_INV(3),
	    SIZE_INV(4), SIZE_INV(5), SIZE_INV(6), SIZE_INV(7),
	    SIZE_INV(8), SIZE_INV(9), SIZE_INV(10), SIZE_INV(11),
	    SIZE_INV(12),SIZE_INV(13), SIZE_INV(14), SIZE_INV(15),
	    SIZE_INV(16),SIZE_INV(17), SIZE_INV(18), SIZE_INV(19),
	    SIZE_INV(20),SIZE_INV(21), SIZE_INV(22), SIZE_INV(23),
	    SIZE_INV(24),SIZE_INV(25), SIZE_INV(26), SIZE_INV(27),
	    SIZE_INV(28),SIZE_INV(29), SIZE_INV(30), SIZE_INV(31)
#if (QUANTUM_2POW_MIN < 4)
	    ,
	    SIZE_INV(32), SIZE_INV(33), SIZE_INV(34), SIZE_INV(35),
	    SIZE_INV(36), SIZE_INV(37), SIZE_INV(38), SIZE_INV(39),
	    SIZE_INV(40), SIZE_INV(41), SIZE_INV(42), SIZE_INV(43),
	    SIZE_INV(44), SIZE_INV(45), SIZE_INV(46), SIZE_INV(47),
	    SIZE_INV(48), SIZE_INV(49), SIZE_INV(50), SIZE_INV(51),
	    SIZE_INV(52), SIZE_INV(53), SIZE_INV(54), SIZE_INV(55),
	    SIZE_INV(56), SIZE_INV(57), SIZE_INV(58), SIZE_INV(59),
	    SIZE_INV(60), SIZE_INV(61), SIZE_INV(62), SIZE_INV(63)
#endif
	};
	unsigned diff, regind, elm, bit;

	assert(run->magic == ARENA_RUN_MAGIC);
	assert(((sizeof(size_invs)) / sizeof(unsigned)) + 3
	    >= (SMALL_MAX_DEFAULT >> QUANTUM_2POW_MIN));

	/*
	 * Avoid doing division with a variable divisor if possible.  Using
	 * actual division here can reduce allocator throughput by over 20%!
	 */
	diff = (unsigned)((uintptr_t)ptr - (uintptr_t)run - bin->reg0_offset);
	if ((size & (size - 1)) == 0) {
		/*
		 * log2_table allows fast division of a power of two in the
		 * [1..128] range.
		 *
		 * (x / divisor) becomes (x >> log2_table[divisor - 1]).
		 */
		static const unsigned char log2_table[] = {
		    0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7
		};

		if (size <= 128)
			regind = (diff >> log2_table[size - 1]);
		else if (size <= 32768)
			regind = diff >> (8 + log2_table[(size >> 8) - 1]);
		else {
			/*
			 * The page size is too large for us to use the lookup
			 * table.  Use real division.
			 */
			regind = diff / size;
		}
	} else if (size <= ((sizeof(size_invs) / sizeof(unsigned))
	    << QUANTUM_2POW_MIN) + 2) {
		regind = size_invs[(size >> QUANTUM_2POW_MIN) - 3] * diff;
		regind >>= SIZE_INV_SHIFT;
	} else {
		/*
		 * size_invs isn't large enough to handle this size class, so
		 * calculate regind using actual division.  This only happens
		 * if the user increases small_max via the 'S' runtime
		 * configuration option.
		 */
		regind = diff / size;
	};
	assert(diff == regind * size);
	assert(regind < bin->nregs);

	elm = regind >> (SIZEOF_INT_2POW + 3);
	if (elm < run->regs_minelm)
		run->regs_minelm = elm;
	bit = regind - (elm << (SIZEOF_INT_2POW + 3));
	assert((run->regs_mask[elm] & (1U << bit)) == 0);
	run->regs_mask[elm] |= (1U << bit);
#undef SIZE_INV
#undef SIZE_INV_SHIFT
}

static void
arena_run_split(arena_t *arena, arena_run_t *run, size_t size, bool zero)
{
	arena_chunk_t *chunk;
	unsigned run_ind, map_offset, total_pages, need_pages, rem_pages;
	unsigned i;
	uint32_t pos_beg, pos_end;

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(run);
	run_ind = (unsigned)(((uintptr_t)run - (uintptr_t)chunk)
	    >> pagesize_2pow);
	total_pages = chunk->map[run_ind].npages;
	need_pages = (size >> pagesize_2pow);
	assert(need_pages > 0);
	assert(need_pages <= total_pages);
	rem_pages = total_pages - need_pages;

	/* Split enough pages from the front of run to fit allocation size. */
	map_offset = run_ind;
	pos_beg = chunk->map[map_offset].pos;
	pos_end = chunk->map[map_offset + total_pages - 1].pos;
	if (zero == false) {
		for (i = 0; i < need_pages; i++) {
			chunk->map[map_offset + i].npages = need_pages;
			chunk->map[map_offset + i].pos = i;
		}
	} else {
		/*
		 * Handle first page specially, since we need to look for
		 * POS_EMPTY rather than NPAGES_EMPTY.
		 */
		i = 0;
		if (chunk->map[map_offset + i].pos != POS_EMPTY) {
			memset((void *)((uintptr_t)chunk + ((map_offset + i) <<
			    pagesize_2pow)), 0, pagesize);
		}
		chunk->map[map_offset + i].npages = need_pages;
		chunk->map[map_offset + i].pos = i;

		/* Handle central pages. */
		for (i++; i < need_pages - 1; i++) {
			if (chunk->map[map_offset + i].npages != NPAGES_EMPTY) {
				memset((void *)((uintptr_t)chunk + ((map_offset
				    + i) << pagesize_2pow)), 0, pagesize);
			}
			chunk->map[map_offset + i].npages = need_pages;
			chunk->map[map_offset + i].pos = i;
		}

		/*
		 * Handle last page specially, since we need to look for
		 * POS_EMPTY rather than NPAGES_EMPTY.
		 */
		if (i < need_pages) {
			if (chunk->map[map_offset + i].npages != POS_EMPTY) {
				memset((void *)((uintptr_t)chunk + ((map_offset
				    + i) << pagesize_2pow)), 0, pagesize);
			}
			chunk->map[map_offset + i].npages = need_pages;
			chunk->map[map_offset + i].pos = i;
		}
	}

	/* Keep track of trailing unused pages for later use. */
	if (rem_pages > 0) {
		/* Update map for trailing pages. */
		map_offset += need_pages;
		chunk->map[map_offset].npages = rem_pages;
		chunk->map[map_offset].pos = pos_beg;
		chunk->map[map_offset + rem_pages - 1].npages = rem_pages;
		chunk->map[map_offset + rem_pages - 1].pos = pos_end;
	}

	chunk->pages_used += need_pages;
}

static arena_chunk_t *
arena_chunk_alloc(arena_t *arena)
{
	arena_chunk_t *chunk;

	if (arena->spare != NULL) {
		chunk = arena->spare;
		arena->spare = NULL;

		RB_INSERT(arena_chunk_tree_s, &arena->chunks, chunk);
	} else {
		unsigned i;

		chunk = (arena_chunk_t *)chunk_alloc(chunksize);
		if (chunk == NULL)
			return (NULL);
#ifdef MALLOC_STATS
		arena->stats.mapped += chunksize;
#endif

		chunk->arena = arena;

		RB_INSERT(arena_chunk_tree_s, &arena->chunks, chunk);

		/*
		 * Claim that no pages are in use, since the header is merely
		 * overhead.
		 */
		chunk->pages_used = 0;

		chunk->max_frun_npages = chunk_npages -
		    arena_chunk_header_npages;
		chunk->min_frun_ind = arena_chunk_header_npages;

		/*
		 * Initialize enough of the map to support one maximal free run.
		 */
		i = arena_chunk_header_npages;
		chunk->map[i].npages = chunk_npages - arena_chunk_header_npages;
		chunk->map[i].pos = POS_EMPTY;

		/* Mark the free run's central pages as untouched. */
		for (i++; i < chunk_npages - 1; i++)
			chunk->map[i].npages = NPAGES_EMPTY;

		/* Take care when (chunk_npages == 2). */
		if (i < chunk_npages) {
			chunk->map[i].npages = chunk_npages -
			    arena_chunk_header_npages;
			chunk->map[i].pos = POS_EMPTY;
		}
	}

	return (chunk);
}

static void
arena_chunk_dealloc(arena_t *arena, arena_chunk_t *chunk)
{

	/*
	 * Remove chunk from the chunk tree, regardless of whether this chunk
	 * will be cached, so that the arena does not use it.
	 */
	RB_REMOVE(arena_chunk_tree_s, &chunk->arena->chunks, chunk);

	if (opt_hint == false) {
		if (arena->spare != NULL) {
			chunk_dealloc((void *)arena->spare, chunksize);
#ifdef MALLOC_STATS
			arena->stats.mapped -= chunksize;
#endif
		}
		arena->spare = chunk;
	} else {
		assert(arena->spare == NULL);
		chunk_dealloc((void *)chunk, chunksize);
#ifdef MALLOC_STATS
		arena->stats.mapped -= chunksize;
#endif
	}
}

static arena_run_t *
arena_run_alloc(arena_t *arena, size_t size, bool zero)
{
	arena_chunk_t *chunk;
	arena_run_t *run;
	unsigned need_npages, limit_pages, compl_need_npages;

	assert(size <= (chunksize - (arena_chunk_header_npages <<
	    pagesize_2pow)));
	assert((size & pagesize_mask) == 0);

	/*
	 * Search through arena's chunks in address order for a free run that is
	 * large enough.  Look for the first fit.
	 */
	need_npages = (size >> pagesize_2pow);
	limit_pages = chunk_npages - arena_chunk_header_npages;
	compl_need_npages = limit_pages - need_npages;
	RB_FOREACH(chunk, arena_chunk_tree_s, &arena->chunks) {
		/*
		 * Avoid searching this chunk if there are not enough
		 * contiguous free pages for there to possibly be a large
		 * enough free run.
		 */
		if (chunk->pages_used <= compl_need_npages &&
		    need_npages <= chunk->max_frun_npages) {
			arena_chunk_map_t *mapelm;
			unsigned i;
			unsigned max_frun_npages = 0;
			unsigned min_frun_ind = chunk_npages;

			assert(chunk->min_frun_ind >=
			    arena_chunk_header_npages);
			for (i = chunk->min_frun_ind; i < chunk_npages;) {
				mapelm = &chunk->map[i];
				if (mapelm->pos >= POS_EMPTY) {
					if (mapelm->npages >= need_npages) {
						run = (arena_run_t *)
						    ((uintptr_t)chunk + (i <<
						    pagesize_2pow));
						/* Update page map. */
						arena_run_split(arena, run,
						    size, zero);
						return (run);
					}
					if (mapelm->npages >
					    max_frun_npages) {
						max_frun_npages =
						    mapelm->npages;
					}
					if (i < min_frun_ind) {
						min_frun_ind = i;
						if (i < chunk->min_frun_ind)
							chunk->min_frun_ind = i;
					}
				}
				i += mapelm->npages;
			}
			/*
			 * Search failure.  Reset cached chunk->max_frun_npages.
			 * chunk->min_frun_ind was already reset above (if
			 * necessary).
			 */
			chunk->max_frun_npages = max_frun_npages;
		}
	}

	/*
	 * No usable runs.  Create a new chunk from which to allocate the run.
	 */
	chunk = arena_chunk_alloc(arena);
	if (chunk == NULL)
		return (NULL);
	run = (arena_run_t *)((uintptr_t)chunk + (arena_chunk_header_npages <<
	    pagesize_2pow));
	/* Update page map. */
	arena_run_split(arena, run, size, zero);
	return (run);
}

static void
arena_run_dalloc(arena_t *arena, arena_run_t *run, size_t size)
{
	arena_chunk_t *chunk;
	unsigned run_ind, run_pages;

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(run);

	run_ind = (unsigned)(((uintptr_t)run - (uintptr_t)chunk)
	    >> pagesize_2pow);
	assert(run_ind >= arena_chunk_header_npages);
	assert(run_ind < (chunksize >> pagesize_2pow));
	run_pages = (size >> pagesize_2pow);
	assert(run_pages == chunk->map[run_ind].npages);

	/* Subtract pages from count of pages used in chunk. */
	chunk->pages_used -= run_pages;

	/* Mark run as deallocated. */
	assert(chunk->map[run_ind].npages == run_pages);
	chunk->map[run_ind].pos = POS_FREE;
	assert(chunk->map[run_ind + run_pages - 1].npages == run_pages);
	chunk->map[run_ind + run_pages - 1].pos = POS_FREE;

	/*
	 * Tell the kernel that we don't need the data in this run, but only if
	 * requested via runtime configuration.
	 */
	if (opt_hint)
		madvise(run, size, MADV_FREE);

	/* Try to coalesce with neighboring runs. */
	if (run_ind > arena_chunk_header_npages &&
	    chunk->map[run_ind - 1].pos >= POS_EMPTY) {
		unsigned prev_npages;

		/* Coalesce with previous run. */
		prev_npages = chunk->map[run_ind - 1].npages;
		/*
		 * The way run allocation currently works (lowest first fit),
		 * it is impossible for a free run to have empty (untouched)
		 * pages followed by dirty pages.  If the run allocation policy
		 * changes, then we will need to account for it here.
		 */
		assert(chunk->map[run_ind - 1].pos != POS_EMPTY);
#if 0
		if (prev_npages > 1 && chunk->map[run_ind - 1].pos == POS_EMPTY)
			chunk->map[run_ind - 1].npages = NPAGES_EMPTY;
#endif
		run_ind -= prev_npages;
		assert(chunk->map[run_ind].npages == prev_npages);
		assert(chunk->map[run_ind].pos >= POS_EMPTY);
		run_pages += prev_npages;

		chunk->map[run_ind].npages = run_pages;
		assert(chunk->map[run_ind].pos >= POS_EMPTY);
		chunk->map[run_ind + run_pages - 1].npages = run_pages;
		assert(chunk->map[run_ind + run_pages - 1].pos >= POS_EMPTY);
	}

	if (run_ind + run_pages < chunk_npages &&
	    chunk->map[run_ind + run_pages].pos >= POS_EMPTY) {
		unsigned next_npages;

		/* Coalesce with next run. */
		next_npages = chunk->map[run_ind + run_pages].npages;
		if (next_npages > 1 && chunk->map[run_ind + run_pages].pos ==
		    POS_EMPTY)
			chunk->map[run_ind + run_pages].npages = NPAGES_EMPTY;
		run_pages += next_npages;
		assert(chunk->map[run_ind + run_pages - 1].npages ==
		    next_npages);
		assert(chunk->map[run_ind + run_pages - 1].pos >= POS_EMPTY);

		chunk->map[run_ind].npages = run_pages;
		assert(chunk->map[run_ind].pos >= POS_EMPTY);
		chunk->map[run_ind + run_pages - 1].npages = run_pages;
		assert(chunk->map[run_ind + run_pages - 1].pos >= POS_EMPTY);
	}

	if (chunk->map[run_ind].npages > chunk->max_frun_npages)
		chunk->max_frun_npages = chunk->map[run_ind].npages;
	if (run_ind < chunk->min_frun_ind)
		chunk->min_frun_ind = run_ind;

	/* Deallocate chunk if it is now completely unused. */
	if (chunk->pages_used == 0)
		arena_chunk_dealloc(arena, chunk);
}

static arena_run_t *
arena_bin_nonfull_run_get(arena_t *arena, arena_bin_t *bin)
{
	arena_run_t *run;
	unsigned i, remainder;

	/* Look for a usable run. */
	if ((run = RB_MIN(arena_run_tree_s, &bin->runs)) != NULL) {
		/* run is guaranteed to have available space. */
		RB_REMOVE(arena_run_tree_s, &bin->runs, run);
#ifdef MALLOC_STATS
		bin->stats.reruns++;
#endif
		return (run);
	}
	/* No existing runs have any space available. */

	/* Allocate a new run. */
	run = arena_run_alloc(arena, bin->run_size, false);
	if (run == NULL)
		return (NULL);

	/* Initialize run internals. */
	run->bin = bin;

	for (i = 0; i < bin->regs_mask_nelms; i++)
		run->regs_mask[i] = UINT_MAX;
	remainder = bin->nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1);
	if (remainder != 0) {
		/* The last element has spare bits that need to be unset. */
		run->regs_mask[i] = (UINT_MAX >> ((1U << (SIZEOF_INT_2POW + 3))
		    - remainder));
	}

	run->regs_minelm = 0;

	run->nfree = bin->nregs;
#ifdef MALLOC_DEBUG
	run->magic = ARENA_RUN_MAGIC;
#endif

#ifdef MALLOC_STATS
	bin->stats.nruns++;
	bin->stats.curruns++;
	if (bin->stats.curruns > bin->stats.highruns)
		bin->stats.highruns = bin->stats.curruns;
#endif
	return (run);
}

/* bin->runcur must have space available before this function is called. */
static inline void *
arena_bin_malloc_easy(arena_t *arena, arena_bin_t *bin, arena_run_t *run)
{
	void *ret;

	assert(run->magic == ARENA_RUN_MAGIC);
	assert(run->nfree > 0);

	ret = arena_run_reg_alloc(run, bin);
	assert(ret != NULL);
	run->nfree--;

	return (ret);
}

/* Re-fill bin->runcur, then call arena_bin_malloc_easy(). */
static void *
arena_bin_malloc_hard(arena_t *arena, arena_bin_t *bin)
{

	bin->runcur = arena_bin_nonfull_run_get(arena, bin);
	if (bin->runcur == NULL)
		return (NULL);
	assert(bin->runcur->magic == ARENA_RUN_MAGIC);
	assert(bin->runcur->nfree > 0);

	return (arena_bin_malloc_easy(arena, bin, bin->runcur));
}

/*
 * Calculate bin->run_size such that it meets the following constraints:
 *
 *   *) bin->run_size >= min_run_size
 *   *) bin->run_size <= arena_maxclass
 *   *) bin->run_size <= RUN_MAX_SMALL
 *   *) run header overhead <= RUN_MAX_OVRHD (or header overhead relaxed).
 *
 * bin->nregs, bin->regs_mask_nelms, and bin->reg0_offset are
 * also calculated here, since these settings are all interdependent.
 */
static size_t
arena_bin_run_size_calc(arena_bin_t *bin, size_t min_run_size)
{
	size_t try_run_size, good_run_size;
	unsigned good_nregs, good_mask_nelms, good_reg0_offset;
	unsigned try_nregs, try_mask_nelms, try_reg0_offset;
	float max_ovrhd = RUN_MAX_OVRHD;

	assert(min_run_size >= pagesize);
	assert(min_run_size <= arena_maxclass);
	assert(min_run_size <= RUN_MAX_SMALL);

	/*
	 * Calculate known-valid settings before entering the run_size
	 * expansion loop, so that the first part of the loop always copies
	 * valid settings.
	 *
	 * The do..while loop iteratively reduces the number of regions until
	 * the run header and the regions no longer overlap.  A closed formula
	 * would be quite messy, since there is an interdependency between the
	 * header's mask length and the number of regions.
	 */
	try_run_size = min_run_size;
	try_nregs = ((try_run_size - sizeof(arena_run_t)) / bin->reg_size)
	    + 1; /* Counter-act the first line of the loop. */
	do {
		try_nregs--;
		try_mask_nelms = (try_nregs >> (SIZEOF_INT_2POW + 3)) +
		    ((try_nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1)) ? 1 : 0);
		try_reg0_offset = try_run_size - (try_nregs * bin->reg_size);
	} while (sizeof(arena_run_t) + (sizeof(unsigned) * (try_mask_nelms - 1))
	    > try_reg0_offset);

	/* run_size expansion loop. */
	do {
		/*
		 * Copy valid settings before trying more aggressive settings.
		 */
		good_run_size = try_run_size;
		good_nregs = try_nregs;
		good_mask_nelms = try_mask_nelms;
		good_reg0_offset = try_reg0_offset;

		/* Try more aggressive settings. */
		try_run_size += pagesize;
		try_nregs = ((try_run_size - sizeof(arena_run_t)) /
		    bin->reg_size) + 1; /* Counter-act try_nregs-- in loop. */
		do {
			try_nregs--;
			try_mask_nelms = (try_nregs >> (SIZEOF_INT_2POW + 3)) +
			    ((try_nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1)) ?
			    1 : 0);
			try_reg0_offset = try_run_size - (try_nregs *
			    bin->reg_size);
		} while (sizeof(arena_run_t) + (sizeof(unsigned) *
		    (try_mask_nelms - 1)) > try_reg0_offset);
	} while (try_run_size <= arena_maxclass && try_run_size <= RUN_MAX_SMALL
	    && max_ovrhd > RUN_MAX_OVRHD_RELAX / ((float)(bin->reg_size << 3))
	    && ((float)(try_reg0_offset)) / ((float)(try_run_size)) >
	    max_ovrhd);

	assert(sizeof(arena_run_t) + (sizeof(unsigned) * (good_mask_nelms - 1))
	    <= good_reg0_offset);
	assert((good_mask_nelms << (SIZEOF_INT_2POW + 3)) >= good_nregs);

	/* Copy final settings. */
	bin->run_size = good_run_size;
	bin->nregs = good_nregs;
	bin->regs_mask_nelms = good_mask_nelms;
	bin->reg0_offset = good_reg0_offset;

	return (good_run_size);
}

static void *
arena_malloc(arena_t *arena, size_t size, bool zero)
{
	void *ret;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(size != 0);
	assert(QUANTUM_CEILING(size) <= arena_maxclass);

	if (size <= bin_maxclass) {
		arena_bin_t *bin;
		arena_run_t *run;

		/* Small allocation. */

		if (size < small_min) {
			/* Tiny. */
			size = pow2_ceil(size);
			bin = &arena->bins[ffs((int)(size >> (TINY_MIN_2POW +
			    1)))];
#if (!defined(NDEBUG) || defined(MALLOC_STATS))
			/*
			 * Bin calculation is always correct, but we may need
			 * to fix size for the purposes of assertions and/or
			 * stats accuracy.
			 */
			if (size < (1U << TINY_MIN_2POW))
				size = (1U << TINY_MIN_2POW);
#endif
		} else if (size <= small_max) {
			/* Quantum-spaced. */
			size = QUANTUM_CEILING(size);
			bin = &arena->bins[ntbins + (size >> opt_quantum_2pow)
			    - 1];
		} else {
			/* Sub-page. */
			size = pow2_ceil(size);
			bin = &arena->bins[ntbins + nqbins
			    + (ffs((int)(size >> opt_small_max_2pow)) - 2)];
		}
		assert(size == bin->reg_size);

		malloc_mutex_lock(&arena->mtx);
		if ((run = bin->runcur) != NULL && run->nfree > 0)
			ret = arena_bin_malloc_easy(arena, bin, run);
		else
			ret = arena_bin_malloc_hard(arena, bin);

		if (ret == NULL) {
			malloc_mutex_unlock(&arena->mtx);
			return (NULL);
		}

#ifdef MALLOC_STATS
		bin->stats.nrequests++;
		arena->stats.nmalloc_small++;
		arena->stats.allocated_small += size;
#endif
		malloc_mutex_unlock(&arena->mtx);

		if (zero == false) {
			if (opt_junk)
				memset(ret, 0xa5, size);
			else if (opt_zero)
				memset(ret, 0, size);
		} else
			memset(ret, 0, size);
	} else {
		/* Large allocation. */
		size = PAGE_CEILING(size);
		malloc_mutex_lock(&arena->mtx);
		ret = (void *)arena_run_alloc(arena, size, true); // XXX zero?
		if (ret == NULL) {
			malloc_mutex_unlock(&arena->mtx);
			return (NULL);
		}
#ifdef MALLOC_STATS
		arena->stats.nmalloc_large++;
		arena->stats.allocated_large += size;
#endif
		malloc_mutex_unlock(&arena->mtx);

		if (zero == false) {
			if (opt_junk)
				memset(ret, 0xa5, size);
			else if (opt_zero)
				memset(ret, 0, size);
		}
	}

	return (ret);
}

static inline void
arena_palloc_trim(arena_t *arena, arena_chunk_t *chunk, unsigned pageind,
    unsigned npages)
{
	unsigned i;

	assert(npages > 0);

	/*
	 * Modifiy the map such that arena_run_dalloc() sees the run as
	 * separately allocated.
	 */
	for (i = 0; i < npages; i++) {
		chunk->map[pageind + i].npages = npages;
		chunk->map[pageind + i].pos = i;
	}
	arena_run_dalloc(arena, (arena_run_t *)((uintptr_t)chunk + (pageind <<
	    pagesize_2pow)), npages << pagesize_2pow);
}

/* Only handles large allocations that require more than page alignment. */
static void *
arena_palloc(arena_t *arena, size_t alignment, size_t size, size_t alloc_size)
{
	void *ret;
	size_t offset;
	arena_chunk_t *chunk;
	unsigned pageind, i, npages;

	assert((size & pagesize_mask) == 0);
	assert((alignment & pagesize_mask) == 0);

	npages = size >> pagesize_2pow;

	malloc_mutex_lock(&arena->mtx);
	ret = (void *)arena_run_alloc(arena, alloc_size, false);
	if (ret == NULL) {
		malloc_mutex_unlock(&arena->mtx);
		return (NULL);
	}

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ret);

	offset = (uintptr_t)ret & (alignment - 1);
	assert((offset & pagesize_mask) == 0);
	assert(offset < alloc_size);
	if (offset == 0) {
		pageind = (((uintptr_t)ret - (uintptr_t)chunk) >>
		    pagesize_2pow);

		/* Update the map for the run to be kept. */
		for (i = 0; i < npages; i++) {
			chunk->map[pageind + i].npages = npages;
			assert(chunk->map[pageind + i].pos == i);
		}

		/* Trim trailing space. */
		arena_palloc_trim(arena, chunk, pageind + npages,
		    (alloc_size - size) >> pagesize_2pow);
	} else {
		size_t leadsize, trailsize;

		leadsize = alignment - offset;
		ret = (void *)((uintptr_t)ret + leadsize);
		pageind = (((uintptr_t)ret - (uintptr_t)chunk) >>
		    pagesize_2pow);

		/* Update the map for the run to be kept. */
		for (i = 0; i < npages; i++) {
			chunk->map[pageind + i].npages = npages;
			chunk->map[pageind + i].pos = i;
		}

		/* Trim leading space. */
		arena_palloc_trim(arena, chunk, pageind - (leadsize >>
		    pagesize_2pow), leadsize >> pagesize_2pow);

		trailsize = alloc_size - leadsize - size;
		if (trailsize != 0) {
			/* Trim trailing space. */
			assert(trailsize < alloc_size);
			arena_palloc_trim(arena, chunk, pageind + npages,
			    trailsize >> pagesize_2pow);
		}
	}

#ifdef MALLOC_STATS
	arena->stats.nmalloc_large++;
	arena->stats.allocated_large += size;
#endif
	malloc_mutex_unlock(&arena->mtx);

	if (opt_junk)
		memset(ret, 0xa5, size);
	else if (opt_zero)
		memset(ret, 0, size);
	return (ret);
}

/* Return the size of the allocation pointed to by ptr. */
static size_t
arena_salloc(const void *ptr)
{
	size_t ret;
	arena_chunk_t *chunk;
	arena_chunk_map_t *mapelm;
	unsigned pageind;

	assert(ptr != NULL);
	assert(CHUNK_ADDR2BASE(ptr) != ptr);

	/*
	 * No arena data structures that we query here can change in a way that
	 * affects this function, so we don't need to lock.
	 */
	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
	pageind = (((uintptr_t)ptr - (uintptr_t)chunk) >> pagesize_2pow);
	mapelm = &chunk->map[pageind];
	if (mapelm->pos != 0 || ptr != (void *)((uintptr_t)chunk) + (pageind <<
	    pagesize_2pow)) {
		arena_run_t *run;

		pageind -= mapelm->pos;

		run = (arena_run_t *)((uintptr_t)chunk + (pageind <<
		    pagesize_2pow));
		assert(run->magic == ARENA_RUN_MAGIC);
		ret = run->bin->reg_size;
	} else
		ret = mapelm->npages << pagesize_2pow;

	return (ret);
}

static void *
arena_ralloc(void *ptr, size_t size, size_t oldsize)
{
	void *ret;

	/* Avoid moving the allocation if the size class would not change. */
	if (size < small_min) {
		if (oldsize < small_min &&
		    ffs((int)(pow2_ceil(size) >> (TINY_MIN_2POW + 1)))
		    == ffs((int)(pow2_ceil(oldsize) >> (TINY_MIN_2POW + 1))))
			goto IN_PLACE;
	} else if (size <= small_max) {
		if (oldsize >= small_min && oldsize <= small_max &&
		    (QUANTUM_CEILING(size) >> opt_quantum_2pow)
		    == (QUANTUM_CEILING(oldsize) >> opt_quantum_2pow))
			goto IN_PLACE;
	} else {
		/*
		 * We make no attempt to resize runs here, though it would be
		 * possible to do so.
		 */
		if (oldsize > small_max && PAGE_CEILING(size) == oldsize)
			goto IN_PLACE;
	}

	/*
	 * If we get here, then size and oldsize are different enough that we
	 * need to use a different size class.  In that case, fall back to
	 * allocating new space and copying.
	 */
	ret = arena_malloc(choose_arena(), size, false);
	if (ret == NULL)
		return (NULL);

	/* Junk/zero-filling were already done by arena_malloc(). */
	if (size < oldsize)
		memcpy(ret, ptr, size);
	else
		memcpy(ret, ptr, oldsize);
	idalloc(ptr);
	return (ret);
IN_PLACE:
	if (opt_junk && size < oldsize)
		memset((void *)((uintptr_t)ptr + size), 0x5a, oldsize - size);
	else if (opt_zero && size > oldsize)
		memset((void *)((uintptr_t)ptr + oldsize), 0, size - oldsize);
	return (ptr);
}

static inline void
arena_dalloc_small(arena_t *arena, arena_chunk_t *chunk, void *ptr,
    unsigned pageind, arena_chunk_map_t *mapelm)
{
	arena_run_t *run;
	arena_bin_t *bin;
	size_t size;

	pageind -= mapelm->pos;

	run = (arena_run_t *)((uintptr_t)chunk + (pageind << pagesize_2pow));
	assert(run->magic == ARENA_RUN_MAGIC);
	bin = run->bin;
	size = bin->reg_size;

	if (opt_junk)
		memset(ptr, 0x5a, size);

	arena_run_reg_dalloc(run, bin, ptr, size);
	run->nfree++;

	if (run->nfree == bin->nregs) {
		/* Deallocate run. */
		if (run == bin->runcur)
			bin->runcur = NULL;
		else if (bin->nregs != 1) {
			/*
			 * This block's conditional is necessary because if the
			 * run only contains one region, then it never gets
			 * inserted into the non-full runs tree.
			 */
			RB_REMOVE(arena_run_tree_s, &bin->runs, run);
		}
#ifdef MALLOC_DEBUG
		run->magic = 0;
#endif
		arena_run_dalloc(arena, run, bin->run_size);
#ifdef MALLOC_STATS
		bin->stats.curruns--;
#endif
	} else if (run->nfree == 1 && run != bin->runcur) {
		/*
		 * Make sure that bin->runcur always refers to the lowest
		 * non-full run, if one exists.
		 */
		if (bin->runcur == NULL)
			bin->runcur = run;
		else if ((uintptr_t)run < (uintptr_t)bin->runcur) {
			/* Switch runcur. */
			if (bin->runcur->nfree > 0) {
				/* Insert runcur. */
				RB_INSERT(arena_run_tree_s, &bin->runs,
				    bin->runcur);
			}
			bin->runcur = run;
		} else
			RB_INSERT(arena_run_tree_s, &bin->runs, run);
	}
#ifdef MALLOC_STATS
	arena->stats.allocated_small -= size;
	arena->stats.ndalloc_small++;
#endif
}

static void
arena_dalloc(arena_t *arena, arena_chunk_t *chunk, void *ptr)
{
	unsigned pageind;
	arena_chunk_map_t *mapelm;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(chunk->arena == arena);
	assert(ptr != NULL);
	assert(CHUNK_ADDR2BASE(ptr) != ptr);

	pageind = (((uintptr_t)ptr - (uintptr_t)chunk) >> pagesize_2pow);
	mapelm = &chunk->map[pageind];
	if (mapelm->pos != 0 || ptr != (void *)((uintptr_t)chunk) + (pageind <<
	    pagesize_2pow)) {
		/* Small allocation. */
		malloc_mutex_lock(&arena->mtx);
		arena_dalloc_small(arena, chunk, ptr, pageind, mapelm);
		malloc_mutex_unlock(&arena->mtx);
	} else {
		size_t size;

		/* Large allocation. */

		size = mapelm->npages << pagesize_2pow;
		assert((((uintptr_t)ptr) & pagesize_mask) == 0);

		if (opt_junk)
			memset(ptr, 0x5a, size);

		malloc_mutex_lock(&arena->mtx);
		arena_run_dalloc(arena, (arena_run_t *)ptr, size);
#ifdef MALLOC_STATS
		arena->stats.allocated_large -= size;
		arena->stats.ndalloc_large++;
#endif
		malloc_mutex_unlock(&arena->mtx);
	}
}

static bool
arena_new(arena_t *arena)
{
	unsigned i;
	arena_bin_t *bin;
	size_t pow2_size, prev_run_size;

	malloc_mutex_init(&arena->mtx);

#ifdef MALLOC_STATS
	memset(&arena->stats, 0, sizeof(arena_stats_t));
#endif

	/* Initialize chunks. */
	RB_INIT(&arena->chunks);
	arena->spare = NULL;

	/* Initialize bins. */
	prev_run_size = pagesize;

	/* (2^n)-spaced tiny bins. */
	for (i = 0; i < ntbins; i++) {
		bin = &arena->bins[i];
		bin->runcur = NULL;
		RB_INIT(&bin->runs);

		bin->reg_size = (1U << (TINY_MIN_2POW + i));

		prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

#ifdef MALLOC_STATS
		memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
#endif
	}

	/* Quantum-spaced bins. */
	for (; i < ntbins + nqbins; i++) {
		bin = &arena->bins[i];
		bin->runcur = NULL;
		RB_INIT(&bin->runs);

		bin->reg_size = quantum * (i - ntbins + 1);

		pow2_size = pow2_ceil(quantum * (i - ntbins + 1));
		prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

#ifdef MALLOC_STATS
		memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
#endif
	}

	/* (2^n)-spaced sub-page bins. */
	for (; i < ntbins + nqbins + nsbins; i++) {
		bin = &arena->bins[i];
		bin->runcur = NULL;
		RB_INIT(&bin->runs);

		bin->reg_size = (small_max << (i - (ntbins + nqbins) + 1));

		prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

#ifdef MALLOC_STATS
		memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
#endif
	}

#ifdef MALLOC_DEBUG
	arena->magic = ARENA_MAGIC;
#endif

	return (false);
}

/* Create a new arena and insert it into the arenas array at index ind. */
static arena_t *
arenas_extend(unsigned ind)
{
	arena_t *ret;

	/* Allocate enough space for trailing bins. */
	ret = (arena_t *)base_alloc(sizeof(arena_t)
	    + (sizeof(arena_bin_t) * (ntbins + nqbins + nsbins - 1)));
	if (ret != NULL && arena_new(ret) == false) {
		arenas[ind] = ret;
		return (ret);
	}
	/* Only reached if there is an OOM error. */

	/*
	 * OOM here is quite inconvenient to propagate, since dealing with it
	 * would require a check for failure in the fast path.  Instead, punt
	 * by using arenas[0].  In practice, this is an extremely unlikely
	 * failure.
	 */
	_malloc_message(_getprogname(),
	    ": (malloc) Error initializing arena\n", "", "");
	if (opt_abort)
		abort();

	return (arenas[0]);
}

/*
 * End arena.
 */
/******************************************************************************/
/*
 * Begin general internal functions.
 */

static void *
huge_malloc(size_t size, bool zero)
{
	void *ret;
	size_t csize;
	chunk_node_t *node;

	/* Allocate one or more contiguous chunks for this request. */

	csize = CHUNK_CEILING(size);
	if (csize == 0) {
		/* size is large enough to cause size_t wrap-around. */
		return (NULL);
	}

	/* Allocate a chunk node with which to track the chunk. */
	node = base_chunk_node_alloc();
	if (node == NULL)
		return (NULL);

	ret = chunk_alloc(csize);
	if (ret == NULL) {
		base_chunk_node_dealloc(node);
		return (NULL);
	}

	/* Insert node into huge. */
	node->chunk = ret;
	node->size = csize;

	malloc_mutex_lock(&chunks_mtx);
	RB_INSERT(chunk_tree_s, &huge, node);
#ifdef MALLOC_STATS
	huge_nmalloc++;
	huge_allocated += csize;
#endif
	malloc_mutex_unlock(&chunks_mtx);

	if (zero == false) {
		if (opt_junk)
			memset(ret, 0xa5, csize);
		else if (opt_zero)
			memset(ret, 0, csize);
	}

	return (ret);
}

/* Only handles large allocations that require more than chunk alignment. */
static void *
huge_palloc(size_t alignment, size_t size)
{
	void *ret;
	size_t alloc_size, chunk_size, offset;
	chunk_node_t *node;

	/*
	 * This allocation requires alignment that is even larger than chunk
	 * alignment.  This means that huge_malloc() isn't good enough.
	 *
	 * Allocate almost twice as many chunks as are demanded by the size or
	 * alignment, in order to assure the alignment can be achieved, then
	 * unmap leading and trailing chunks.
	 */
	assert(alignment >= chunksize);

	chunk_size = CHUNK_CEILING(size);

	if (size >= alignment)
		alloc_size = chunk_size + alignment - chunksize;
	else
		alloc_size = (alignment << 1) - chunksize;

	/* Allocate a chunk node with which to track the chunk. */
	node = base_chunk_node_alloc();
	if (node == NULL)
		return (NULL);

	ret = chunk_alloc(alloc_size);
	if (ret == NULL) {
		base_chunk_node_dealloc(node);
		return (NULL);
	}

	offset = (uintptr_t)ret & (alignment - 1);
	assert((offset & chunksize_mask) == 0);
	assert(offset < alloc_size);
	if (offset == 0) {
		/* Trim trailing space. */
		chunk_dealloc((void *)((uintptr_t)ret + chunk_size), alloc_size
		    - chunk_size);
	} else {
		size_t trailsize;

		/* Trim leading space. */
		chunk_dealloc(ret, alignment - offset);

		ret = (void *)((uintptr_t)ret + (alignment - offset));

		trailsize = alloc_size - (alignment - offset) - chunk_size;
		if (trailsize != 0) {
		    /* Trim trailing space. */
		    assert(trailsize < alloc_size);
		    chunk_dealloc((void *)((uintptr_t)ret + chunk_size),
			trailsize);
		}
	}

	/* Insert node into huge. */
	node->chunk = ret;
	node->size = chunk_size;

	malloc_mutex_lock(&chunks_mtx);
	RB_INSERT(chunk_tree_s, &huge, node);
#ifdef MALLOC_STATS
	huge_nmalloc++;
	huge_allocated += chunk_size;
#endif
	malloc_mutex_unlock(&chunks_mtx);

	if (opt_junk)
		memset(ret, 0xa5, chunk_size);
	else if (opt_zero)
		memset(ret, 0, chunk_size);

	return (ret);
}

static void *
huge_ralloc(void *ptr, size_t size, size_t oldsize)
{
	void *ret;

	/* Avoid moving the allocation if the size class would not change. */
	if (oldsize > arena_maxclass &&
	    CHUNK_CEILING(size) == CHUNK_CEILING(oldsize)) {
		if (opt_junk && size < oldsize) {
			memset((void *)((uintptr_t)ptr + size), 0x5a, oldsize
			    - size);
		} else if (opt_zero && size > oldsize) {
			memset((void *)((uintptr_t)ptr + oldsize), 0, size
			    - oldsize);
		}
		return (ptr);
	}

	/*
	 * If we get here, then size and oldsize are different enough that we
	 * need to use a different size class.  In that case, fall back to
	 * allocating new space and copying.
	 */
	ret = huge_malloc(size, false);
	if (ret == NULL)
		return (NULL);

	if (CHUNK_ADDR2BASE(ptr) == ptr) {
		/* The old allocation is a chunk. */
		if (size < oldsize)
			memcpy(ret, ptr, size);
		else
			memcpy(ret, ptr, oldsize);
	} else {
		/* The old allocation is a region. */
		assert(oldsize < size);
		memcpy(ret, ptr, oldsize);
	}
	idalloc(ptr);
	return (ret);
}

static void
huge_dalloc(void *ptr)
{
	chunk_node_t key;
	chunk_node_t *node;

	malloc_mutex_lock(&chunks_mtx);

	/* Extract from tree of huge allocations. */
	key.chunk = ptr;
	node = RB_FIND(chunk_tree_s, &huge, &key);
	assert(node != NULL);
	assert(node->chunk == ptr);
	RB_REMOVE(chunk_tree_s, &huge, node);

#ifdef MALLOC_STATS
	huge_ndalloc++;
	huge_allocated -= node->size;
#endif

	malloc_mutex_unlock(&chunks_mtx);

	/* Unmap chunk. */
#ifdef USE_BRK
	if (opt_junk)
		memset(node->chunk, 0x5a, node->size);
#endif
	chunk_dealloc(node->chunk, node->size);

	base_chunk_node_dealloc(node);
}

static void *
imalloc(size_t size)
{
	void *ret;

	assert(size != 0);

	if (size <= arena_maxclass)
		ret = arena_malloc(choose_arena(), size, false);
	else
		ret = huge_malloc(size, false);

	return (ret);
}

static void *
ipalloc(size_t alignment, size_t size)
{
	void *ret;
	size_t ceil_size;

	/*
	 * Round size up to the nearest multiple of alignment.
	 *
	 * This done, we can take advantage of the fact that for each small
	 * size class, every object is aligned at the smallest power of two
	 * that is non-zero in the base two representation of the size.  For
	 * example:
	 *
	 *   Size |   Base 2 | Minimum alignment
	 *   -----+----------+------------------
	 *     96 |  1100000 |  32
	 *    144 | 10100000 |  32
	 *    192 | 11000000 |  64
	 *
	 * Depending on runtime settings, it is possible that arena_malloc()
	 * will further round up to a power of two, but that never causes
	 * correctness issues.
	 */
	ceil_size = (size + (alignment - 1)) & (-alignment);
	/*
	 * (ceil_size < size) protects against the combination of maximal
	 * alignment and size greater than maximal alignment.
	 */
	if (ceil_size < size) {
		/* size_t overflow. */
		return (NULL);
	}

	if (ceil_size <= pagesize || (alignment <= pagesize
	    && ceil_size <= arena_maxclass))
		ret = arena_malloc(choose_arena(), ceil_size, false);
	else {
		size_t run_size;

		/*
		 * We can't achieve sub-page alignment, so round up alignment
		 * permanently; it makes later calculations simpler.
		 */
		alignment = PAGE_CEILING(alignment);
		ceil_size = PAGE_CEILING(size);
		/*
		 * (ceil_size < size) protects against very large sizes within
		 * pagesize of SIZE_T_MAX.
		 *
		 * (ceil_size + alignment < ceil_size) protects against the
		 * combination of maximal alignment and ceil_size large enough
		 * to cause overflow.  This is similar to the first overflow
		 * check above, but it needs to be repeated due to the new
		 * ceil_size value, which may now be *equal* to maximal
		 * alignment, whereas before we only detected overflow if the
		 * original size was *greater* than maximal alignment.
		 */
		if (ceil_size < size || ceil_size + alignment < ceil_size) {
			/* size_t overflow. */
			return (NULL);
		}

		/*
		 * Calculate the size of the over-size run that arena_palloc()
		 * would need to allocate in order to guarantee the alignment.
		 */
		if (ceil_size >= alignment)
			run_size = ceil_size + alignment - pagesize;
		else {
			/*
			 * It is possible that (alignment << 1) will cause
			 * overflow, but it doesn't matter because we also
			 * subtract pagesize, which in the case of overflow
			 * leaves us with a very large run_size.  That causes
			 * the first conditional below to fail, which means
			 * that the bogus run_size value never gets used for
			 * anything important.
			 */
			run_size = (alignment << 1) - pagesize;
		}

		if (run_size <= arena_maxclass) {
			ret = arena_palloc(choose_arena(), alignment, ceil_size,
			    run_size);
		} else if (alignment <= chunksize)
			ret = huge_malloc(ceil_size, false);
		else
			ret = huge_palloc(alignment, ceil_size);
	}

	assert(((uintptr_t)ret & (alignment - 1)) == 0);
	return (ret);
}

static void *
icalloc(size_t size)
{
	void *ret;

	if (size <= arena_maxclass)
		ret = arena_malloc(choose_arena(), size, true);
	else
		ret = huge_malloc(size, true);

	return (ret);
}

static size_t
isalloc(const void *ptr)
{
	size_t ret;
	arena_chunk_t *chunk;

	assert(ptr != NULL);

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
	if (chunk != ptr) {
		/* Region. */
		assert(chunk->arena->magic == ARENA_MAGIC);

		ret = arena_salloc(ptr);
	} else {
		chunk_node_t *node, key;

		/* Chunk (huge allocation). */

		malloc_mutex_lock(&chunks_mtx);

		/* Extract from tree of huge allocations. */
		key.chunk = __DECONST(void *, ptr);
		node = RB_FIND(chunk_tree_s, &huge, &key);
		assert(node != NULL);

		ret = node->size;

		malloc_mutex_unlock(&chunks_mtx);
	}

	return (ret);
}

static void *
iralloc(void *ptr, size_t size)
{
	void *ret;
	size_t oldsize;

	assert(ptr != NULL);
	assert(size != 0);

	oldsize = isalloc(ptr);

	if (size <= arena_maxclass)
		ret = arena_ralloc(ptr, size, oldsize);
	else
		ret = huge_ralloc(ptr, size, oldsize);

	return (ret);
}

static void
idalloc(void *ptr)
{
	arena_chunk_t *chunk;

	assert(ptr != NULL);

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
	if (chunk != ptr) {
		/* Region. */
		arena_dalloc(chunk->arena, chunk, ptr);
	} else
		huge_dalloc(ptr);
}

static void
malloc_print_stats(void)
{

	if (opt_print_stats) {
		char s[UMAX2S_BUFSIZE];
		_malloc_message("___ Begin malloc statistics ___\n", "", "",
		    "");
		_malloc_message("Assertions ",
#ifdef NDEBUG
		    "disabled",
#else
		    "enabled",
#endif
		    "\n", "");
		_malloc_message("Boolean MALLOC_OPTIONS: ",
		    opt_abort ? "A" : "a",
		    opt_junk ? "J" : "j",
		    opt_hint ? "H" : "h");
		_malloc_message(opt_utrace ? "PU" : "Pu",
		    opt_sysv ? "V" : "v",
		    opt_xmalloc ? "X" : "x",
		    opt_zero ? "Z\n" : "z\n");

		_malloc_message("CPUs: ", umax2s(ncpus, s), "\n", "");
		_malloc_message("Max arenas: ", umax2s(narenas, s), "\n", "");
		_malloc_message("Pointer size: ", umax2s(sizeof(void *), s),
		    "\n", "");
		_malloc_message("Quantum size: ", umax2s(quantum, s), "\n", "");
		_malloc_message("Max small size: ", umax2s(small_max, s), "\n",
		    "");

		_malloc_message("Chunk size: ", umax2s(chunksize, s), "", "");
		_malloc_message(" (2^", umax2s(opt_chunk_2pow, s), ")\n", "");

#ifdef MALLOC_STATS
		{
			size_t allocated, mapped;
			unsigned i;
			arena_t *arena;

			/* Calculate and print allocated/mapped stats. */

			/* arenas. */
			for (i = 0, allocated = 0; i < narenas; i++) {
				if (arenas[i] != NULL) {
					malloc_mutex_lock(&arenas[i]->mtx);
					allocated +=
					    arenas[i]->stats.allocated_small;
					allocated +=
					    arenas[i]->stats.allocated_large;
					malloc_mutex_unlock(&arenas[i]->mtx);
				}
			}

			/* huge/base. */
			malloc_mutex_lock(&chunks_mtx);
			allocated += huge_allocated;
			mapped = stats_chunks.curchunks * chunksize;
			malloc_mutex_unlock(&chunks_mtx);

			malloc_mutex_lock(&base_mtx);
			mapped += base_mapped;
			malloc_mutex_unlock(&base_mtx);

			malloc_printf("Allocated: %zu, mapped: %zu\n",
			    allocated, mapped);

			/* Print chunk stats. */
			{
				chunk_stats_t chunks_stats;

				malloc_mutex_lock(&chunks_mtx);
				chunks_stats = stats_chunks;
				malloc_mutex_unlock(&chunks_mtx);

				malloc_printf("chunks: nchunks   "
				    "highchunks    curchunks\n");
				malloc_printf("  %13llu%13lu%13lu\n",
				    chunks_stats.nchunks,
				    chunks_stats.highchunks,
				    chunks_stats.curchunks);
			}

			/* Print chunk stats. */
			malloc_printf(
			    "huge: nmalloc      ndalloc    allocated\n");
			malloc_printf(" %12llu %12llu %12zu\n",
			    huge_nmalloc, huge_ndalloc, huge_allocated);

			/* Print stats for each arena. */
			for (i = 0; i < narenas; i++) {
				arena = arenas[i];
				if (arena != NULL) {
					malloc_printf(
					    "\narenas[%u]:\n", i);
					malloc_mutex_lock(&arena->mtx);
					stats_print(arena);
					malloc_mutex_unlock(&arena->mtx);
				}
			}
		}
#endif /* #ifdef MALLOC_STATS */
		_malloc_message("--- End malloc statistics ---\n", "", "", "");
	}
}

/*
 * FreeBSD's pthreads implementation calls malloc(3), so the malloc
 * implementation has to take pains to avoid infinite recursion during
 * initialization.
 */
static inline bool
malloc_init(void)
{

	if (malloc_initialized == false)
		return (malloc_init_hard());

	return (false);
}

static bool
malloc_init_hard(void)
{
	unsigned i, j;
	int linklen;
	char buf[PATH_MAX + 1];
	const char *opts;

	malloc_mutex_lock(&init_lock);
	if (malloc_initialized) {
		/*
		 * Another thread initialized the allocator before this one
		 * acquired init_lock.
		 */
		malloc_mutex_unlock(&init_lock);
		return (false);
	}

	/* Get number of CPUs. */
	{
		int mib[2];
		size_t len;

		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		len = sizeof(ncpus);
		if (sysctl(mib, 2, &ncpus, &len, (void *) 0, 0) == -1) {
			/* Error. */
			ncpus = 1;
		}
	}

	/* Get page size. */
	{
		long result;

		result = sysconf(_SC_PAGESIZE);
		assert(result != -1);
		pagesize = (unsigned) result;

		/*
		 * We assume that pagesize is a power of 2 when calculating
		 * pagesize_mask and pagesize_2pow.
		 */
		assert(((result - 1) & result) == 0);
		pagesize_mask = result - 1;
		pagesize_2pow = ffs((int)result) - 1;
	}

	for (i = 0; i < 3; i++) {
		/* Get runtime configuration. */
		switch (i) {
		case 0:
			if ((linklen = readlink("/etc/malloc.conf", buf,
						sizeof(buf) - 1)) != -1) {
				/*
				 * Use the contents of the "/etc/malloc.conf"
				 * symbolic link's name.
				 */
				buf[linklen] = '\0';
				opts = buf;
			} else {
				/* No configuration specified. */
				buf[0] = '\0';
				opts = buf;
			}
			break;
		case 1:
			if (issetugid() == 0 && (opts =
			    getenv("MALLOC_OPTIONS")) != NULL) {
				/*
				 * Do nothing; opts is already initialized to
				 * the value of the MALLOC_OPTIONS environment
				 * variable.
				 */
			} else {
				/* No configuration specified. */
				buf[0] = '\0';
				opts = buf;
			}
			break;
		case 2:
			if (_malloc_options != NULL) {
			    /*
			     * Use options that were compiled into the program.
			     */
			    opts = _malloc_options;
			} else {
				/* No configuration specified. */
				buf[0] = '\0';
				opts = buf;
			}
			break;
		default:
			/* NOTREACHED */
			assert(false);
		}

		for (j = 0; opts[j] != '\0'; j++) {
			switch (opts[j]) {
			case 'a':
				opt_abort = false;
				break;
			case 'A':
				opt_abort = true;
				break;
			case 'h':
				opt_hint = false;
				break;
			case 'H':
				opt_hint = true;
				break;
			case 'j':
				opt_junk = false;
				break;
			case 'J':
				opt_junk = true;
				break;
			case 'k':
				/*
				 * Chunks always require at least one header
				 * page, so chunks can never be smaller than
				 * two pages.
				 */
				if (opt_chunk_2pow > pagesize_2pow + 1)
					opt_chunk_2pow--;
				break;
			case 'K':
				/*
				 * There must be fewer pages in a chunk than
				 * can be recorded by the pos field of
				 * arena_chunk_map_t, in order to make POS_FREE
				 * special.
				 */
				if (opt_chunk_2pow - pagesize_2pow
				    < (sizeof(uint32_t) << 3) - 1)
					opt_chunk_2pow++;
				break;
			case 'n':
				opt_narenas_lshift--;
				break;
			case 'N':
				opt_narenas_lshift++;
				break;
			case 'p':
				opt_print_stats = false;
				break;
			case 'P':
				opt_print_stats = true;
				break;
			case 'q':
				if (opt_quantum_2pow > QUANTUM_2POW_MIN)
					opt_quantum_2pow--;
				break;
			case 'Q':
				if (opt_quantum_2pow < pagesize_2pow - 1)
					opt_quantum_2pow++;
				break;
			case 's':
				if (opt_small_max_2pow > QUANTUM_2POW_MIN)
					opt_small_max_2pow--;
				break;
			case 'S':
				if (opt_small_max_2pow < pagesize_2pow - 1)
					opt_small_max_2pow++;
				break;
			case 'u':
				opt_utrace = false;
				break;
			case 'U':
				opt_utrace = true;
				break;
			case 'v':
				opt_sysv = false;
				break;
			case 'V':
				opt_sysv = true;
				break;
			case 'x':
				opt_xmalloc = false;
				break;
			case 'X':
				opt_xmalloc = true;
				break;
			case 'z':
				opt_zero = false;
				break;
			case 'Z':
				opt_zero = true;
				break;
			default: {
				char cbuf[2];
				
				cbuf[0] = opts[j];
				cbuf[1] = '\0';
				_malloc_message(_getprogname(),
				    ": (malloc) Unsupported character in "
				    "malloc options: '", cbuf, "'\n");
			}
			}
		}
	}

	/* Take care to call atexit() only once. */
	if (opt_print_stats) {
		/* Print statistics at exit. */
		atexit(malloc_print_stats);
	}

	/* Set variables according to the value of opt_small_max_2pow. */
	if (opt_small_max_2pow < opt_quantum_2pow)
		opt_small_max_2pow = opt_quantum_2pow;
	small_max = (1U << opt_small_max_2pow);

	/* Set bin-related variables. */
	bin_maxclass = (pagesize >> 1);
	assert(opt_quantum_2pow >= TINY_MIN_2POW);
	ntbins = opt_quantum_2pow - TINY_MIN_2POW;
	assert(ntbins <= opt_quantum_2pow);
	nqbins = (small_max >> opt_quantum_2pow);
	nsbins = pagesize_2pow - opt_small_max_2pow - 1;

	/* Set variables according to the value of opt_quantum_2pow. */
	quantum = (1U << opt_quantum_2pow);
	quantum_mask = quantum - 1;
	if (ntbins > 0)
		small_min = (quantum >> 1) + 1;
	else
		small_min = 1;
	assert(small_min <= quantum);

	/* Set variables according to the value of opt_chunk_2pow. */
	chunksize = (1LU << opt_chunk_2pow);
	chunksize_mask = chunksize - 1;
	chunk_npages = (chunksize >> pagesize_2pow);
	{
		unsigned header_size;

		header_size = sizeof(arena_chunk_t) + (sizeof(arena_chunk_map_t)
		    * (chunk_npages - 1));
		arena_chunk_header_npages = (header_size >> pagesize_2pow);
		if ((header_size & pagesize_mask) != 0)
			arena_chunk_header_npages++;
	}
	arena_maxclass = chunksize - (arena_chunk_header_npages <<
	    pagesize_2pow);

	UTRACE(0, 0, 0);

#ifdef MALLOC_STATS
	memset(&stats_chunks, 0, sizeof(chunk_stats_t));
#endif

	/* Various sanity checks that regard configuration. */
	assert(quantum >= sizeof(void *));
	assert(quantum <= pagesize);
	assert(chunksize >= pagesize);
	assert(quantum * 4 <= chunksize);

	/* Initialize chunks data. */
	malloc_mutex_init(&chunks_mtx);
	RB_INIT(&huge);
#ifdef USE_BRK
	malloc_mutex_init(&brk_mtx);
	brk_base = sbrk(0);
	brk_prev = brk_base;
	brk_max = brk_base;
#endif
#ifdef MALLOC_STATS
	huge_nmalloc = 0;
	huge_ndalloc = 0;
	huge_allocated = 0;
#endif
	RB_INIT(&old_chunks);

	/* Initialize base allocation data structures. */
#ifdef MALLOC_STATS
	base_mapped = 0;
#endif
#ifdef USE_BRK
	/*
	 * Allocate a base chunk here, since it doesn't actually have to be
	 * chunk-aligned.  Doing this before allocating any other chunks allows
	 * the use of space that would otherwise be wasted.
	 */
	base_pages_alloc(0);
#endif
	base_chunk_nodes = NULL;
	malloc_mutex_init(&base_mtx);

	if (ncpus > 1) {
		/*
		 * For SMP systems, create four times as many arenas as there
		 * are CPUs by default.
		 */
		opt_narenas_lshift += 2;
	}

	/* Determine how many arenas to use. */
	narenas = ncpus;
	if (opt_narenas_lshift > 0) {
		if ((narenas << opt_narenas_lshift) > narenas)
			narenas <<= opt_narenas_lshift;
		/*
		 * Make sure not to exceed the limits of what base_alloc() can
		 * handle.
		 */
		if (narenas * sizeof(arena_t *) > chunksize)
			narenas = chunksize / sizeof(arena_t *);
	} else if (opt_narenas_lshift < 0) {
		if ((narenas >> -opt_narenas_lshift) < narenas)
			narenas >>= -opt_narenas_lshift;
		/* Make sure there is at least one arena. */
		if (narenas == 0)
			narenas = 1;
	}

#ifdef NO_TLS
	if (narenas > 1) {
		static const unsigned primes[] = {1, 3, 5, 7, 11, 13, 17, 19,
		    23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83,
		    89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149,
		    151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211,
		    223, 227, 229, 233, 239, 241, 251, 257, 263};
		unsigned nprimes, parenas;

		/*
		 * Pick a prime number of hash arenas that is more than narenas
		 * so that direct hashing of pthread_self() pointers tends to
		 * spread allocations evenly among the arenas.
		 */
		assert((narenas & 1) == 0); /* narenas must be even. */
		nprimes = (sizeof(primes) >> SIZEOF_INT_2POW);
		parenas = primes[nprimes - 1]; /* In case not enough primes. */
		for (i = 1; i < nprimes; i++) {
			if (primes[i] > narenas) {
				parenas = primes[i];
				break;
			}
		}
		narenas = parenas;
	}
#endif

#ifndef NO_TLS
	next_arena = 0;
#endif

	/* Allocate and initialize arenas. */
	arenas = (arena_t **)base_alloc(sizeof(arena_t *) * narenas);
	if (arenas == NULL) {
		malloc_mutex_unlock(&init_lock);
		return (true);
	}
	/*
	 * Zero the array.  In practice, this should always be pre-zeroed,
	 * since it was just mmap()ed, but let's be sure.
	 */
	memset(arenas, 0, sizeof(arena_t *) * narenas);

	/*
	 * Initialize one arena here.  The rest are lazily created in
	 * choose_arena_hard().
	 */
	arenas_extend(0);
	if (arenas[0] == NULL) {
		malloc_mutex_unlock(&init_lock);
		return (true);
	}

	malloc_mutex_init(&arenas_mtx);

	malloc_initialized = true;
	malloc_mutex_unlock(&init_lock);
	return (false);
}

/*
 * End general internal functions.
 */
/******************************************************************************/
/*
 * Begin malloc(3)-compatible functions.
 */

void *
malloc(size_t size)
{
	void *ret;

	if (malloc_init()) {
		ret = NULL;
		goto RETURN;
	}

	if (size == 0) {
		if (opt_sysv == false)
			size = 1;
		else {
			ret = NULL;
			goto RETURN;
		}
	}

	ret = imalloc(size);

RETURN:
	if (ret == NULL) {
		if (opt_xmalloc) {
			_malloc_message(_getprogname(),
			    ": (malloc) Error in malloc(): out of memory\n", "",
			    "");
			abort();
		}
		errno = ENOMEM;
	}

	UTRACE(0, size, ret);
	return (ret);
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int ret;
	void *result;

	if (malloc_init())
		result = NULL;
	else {
		/* Make sure that alignment is a large enough power of 2. */
		if (((alignment - 1) & alignment) != 0
		    || alignment < sizeof(void *)) {
			if (opt_xmalloc) {
				_malloc_message(_getprogname(),
				    ": (malloc) Error in posix_memalign(): "
				    "invalid alignment\n", "", "");
				abort();
			}
			result = NULL;
			ret = EINVAL;
			goto RETURN;
		}

		result = ipalloc(alignment, size);
	}

	if (result == NULL) {
		if (opt_xmalloc) {
			_malloc_message(_getprogname(),
			": (malloc) Error in posix_memalign(): out of memory\n",
			"", "");
			abort();
		}
		ret = ENOMEM;
		goto RETURN;
	}

	*memptr = result;
	ret = 0;

RETURN:
	UTRACE(0, size, result);
	return (ret);
}

void *
calloc(size_t num, size_t size)
{
	void *ret;
	size_t num_size;

	if (malloc_init()) {
		num_size = 0;
		ret = NULL;
		goto RETURN;
	}

	num_size = num * size;
	if (num_size == 0) {
		if ((opt_sysv == false) && ((num == 0) || (size == 0)))
			num_size = 1;
		else {
			ret = NULL;
			goto RETURN;
		}
	/*
	 * Try to avoid division here.  We know that it isn't possible to
	 * overflow during multiplication if neither operand uses any of the
	 * most significant half of the bits in a size_t.
	 */
	} else if (((num | size) & (SIZE_T_MAX << (sizeof(size_t) << 2)))
	    && (num_size / size != num)) {
		/* size_t overflow. */
		ret = NULL;
		goto RETURN;
	}

	ret = icalloc(num_size);

RETURN:
	if (ret == NULL) {
		if (opt_xmalloc) {
			_malloc_message(_getprogname(),
			    ": (malloc) Error in calloc(): out of memory\n", "",
			    "");
			abort();
		}
		errno = ENOMEM;
	}

	UTRACE(0, num_size, ret);
	return (ret);
}

void *
realloc(void *ptr, size_t size)
{
	void *ret;

	if (size == 0) {
		if (opt_sysv == false)
			size = 1;
		else {
			if (ptr != NULL)
				idalloc(ptr);
			ret = NULL;
			goto RETURN;
		}
	}

	if (ptr != NULL) {
		assert(malloc_initialized);

		ret = iralloc(ptr, size);

		if (ret == NULL) {
			if (opt_xmalloc) {
				_malloc_message(_getprogname(),
				    ": (malloc) Error in realloc(): out of "
				    "memory\n", "", "");
				abort();
			}
			errno = ENOMEM;
		}
	} else {
		if (malloc_init())
			ret = NULL;
		else
			ret = imalloc(size);

		if (ret == NULL) {
			if (opt_xmalloc) {
				_malloc_message(_getprogname(),
				    ": (malloc) Error in realloc(): out of "
				    "memory\n", "", "");
				abort();
			}
			errno = ENOMEM;
		}
	}

RETURN:
	UTRACE(ptr, size, ret);
	return (ret);
}

void
free(void *ptr)
{

	UTRACE(ptr, 0, 0);
	if (ptr != NULL) {
		assert(malloc_initialized);

		idalloc(ptr);
	}
}

/*
 * End malloc(3)-compatible functions.
 */
/******************************************************************************/
/*
 * Begin non-standard functions.
 */

size_t
malloc_usable_size(const void *ptr)
{

	assert(ptr != NULL);

	return (isalloc(ptr));
}

/*
 * End non-standard functions.
 */
/******************************************************************************/
/*
 * Begin library-private functions, used by threading libraries for protection
 * of malloc during fork().  These functions are only called if the program is
 * running in threaded mode, so there is no need to check whether the program
 * is threaded here.
 */

void
_malloc_prefork(void)
{
	unsigned i;

	/* Acquire all mutexes in a safe order. */

	malloc_mutex_lock(&arenas_mtx);
	for (i = 0; i < narenas; i++) {
		if (arenas[i] != NULL)
			malloc_mutex_lock(&arenas[i]->mtx);
	}
	malloc_mutex_unlock(&arenas_mtx);

	malloc_mutex_lock(&base_mtx);

	malloc_mutex_lock(&chunks_mtx);
}

void
_malloc_postfork(void)
{
	unsigned i;

	/* Release all mutexes, now that fork() has completed. */

	malloc_mutex_unlock(&chunks_mtx);

	malloc_mutex_unlock(&base_mtx);

	malloc_mutex_lock(&arenas_mtx);
	for (i = 0; i < narenas; i++) {
		if (arenas[i] != NULL)
			malloc_mutex_unlock(&arenas[i]->mtx);
	}
	malloc_mutex_unlock(&arenas_mtx);
}

/*
 * End library-private functions.
 */
/******************************************************************************/
