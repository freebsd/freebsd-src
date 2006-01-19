/*-
 * Copyright (C) 2006 Jason Evans <jasone@FreeBSD.org>.
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
 * Following is a brief list of features that distinguish this malloc
 * implementation:
 *
 *   + Multiple arenas are used if there are multiple CPUs, which reduces lock
 *     contention and cache sloshing.
 *
 *   + Cache line sharing between arenas is avoided for internal data
 *     structures.
 *
 *   + Memory is managed in chunks, rather than as individual pages.
 *
 *   + Allocations are region-based; internal region size is a discrete
 *     multiple of a quantum that is appropriate for alignment constraints.
 *     This applies to allocations that are up to half the chunk size.
 *
 *   + Coalescence of regions is delayed in order to reduce overhead and
 *     fragmentation.
 *
 *   + realloc() always moves data, in order to reduce fragmentation.
 *
 *   + Red-black trees are used to sort large regions.
 *
 *   + Data structures for huge allocations are stored separately from
 *     allocations, which reduces thrashing during low memory conditions.
 *
 *******************************************************************************
 */

/*
 *******************************************************************************
 *
 * Ring macros.
 *
 *******************************************************************************
 */

/* Ring definitions. */
#define	qr(a_type) struct {						\
	a_type *qre_next;						\
	a_type *qre_prev;						\
}

#define	qr_initializer {NULL, NULL}

/* Ring functions. */
#define	qr_new(a_qr, a_field) do {					\
	(a_qr)->a_field.qre_next = (a_qr);				\
	(a_qr)->a_field.qre_prev = (a_qr);				\
} while (0)

#define	qr_next(a_qr, a_field) ((a_qr)->a_field.qre_next)

#define	qr_prev(a_qr, a_field) ((a_qr)->a_field.qre_prev)

#define	qr_before_insert(a_qrelm, a_qr, a_field) do {			\
	(a_qr)->a_field.qre_prev = (a_qrelm)->a_field.qre_prev;		\
	(a_qr)->a_field.qre_next = (a_qrelm);				\
	(a_qr)->a_field.qre_prev->a_field.qre_next = (a_qr);		\
	(a_qrelm)->a_field.qre_prev = (a_qr);				\
} while (0)

#define	qr_after_insert(a_qrelm, a_qr, a_field) do {			\
	(a_qr)->a_field.qre_next = (a_qrelm)->a_field.qre_next;		\
	(a_qr)->a_field.qre_prev = (a_qrelm);				\
	(a_qr)->a_field.qre_next->a_field.qre_prev = (a_qr);		\
	(a_qrelm)->a_field.qre_next = (a_qr);				\
} while (0)

#define	qr_meld(a_qr_a, a_qr_b, a_type, a_field) do {			\
	a_type *t;							\
	(a_qr_a)->a_field.qre_prev->a_field.qre_next = (a_qr_b);	\
	(a_qr_b)->a_field.qre_prev->a_field.qre_next = (a_qr_a);	\
	t = (a_qr_a)->a_field.qre_prev;					\
	(a_qr_a)->a_field.qre_prev = (a_qr_b)->a_field.qre_prev;	\
	(a_qr_b)->a_field.qre_prev = t;					\
} while (0)

/* qr_meld() and qr_split() are functionally equivalent, so there's no need to
 * have two copies of the code. */
#define	qr_split(a_qr_a, a_qr_b, a_type, a_field)			\
	qr_meld((a_qr_a), (a_qr_b), a_type, a_field)

#define	qr_remove(a_qr, a_field) do {					\
	(a_qr)->a_field.qre_prev->a_field.qre_next			\
	    = (a_qr)->a_field.qre_next;					\
	(a_qr)->a_field.qre_next->a_field.qre_prev			\
	    = (a_qr)->a_field.qre_prev;					\
	(a_qr)->a_field.qre_next = (a_qr);				\
	(a_qr)->a_field.qre_prev = (a_qr);				\
} while (0)

#define	qr_foreach(var, a_qr, a_field)					\
	for ((var) = (a_qr);						\
	    (var) != NULL;						\
	    (var) = (((var)->a_field.qre_next != (a_qr))		\
	    ? (var)->a_field.qre_next : NULL))

#define	qr_reverse_foreach(var, a_qr, a_field)				\
	for ((var) = ((a_qr) != NULL) ? qr_prev(a_qr, a_field) : NULL;	\
	    (var) != NULL;						\
	    (var) = (((var) != (a_qr))					\
	    ? (var)->a_field.qre_prev : NULL))

/******************************************************************************/

#define	MALLOC_DEBUG

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
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "un-namespace.h"

/*
 * Calculate statistics that can be used to get an idea of how well caching is
 * working.
 */
#define	MALLOC_STATS
#define	MALLOC_STATS_ARENAS

/*
 * Include redzones before/after every region, and check for buffer overflows.
 */
#define MALLOC_REDZONES
#ifdef MALLOC_REDZONES
#  define MALLOC_RED_2POW	4
#  define MALLOC_RED		((size_t)(1 << MALLOC_RED_2POW))
#endif

#ifndef MALLOC_DEBUG
#  ifndef NDEBUG
#    define NDEBUG
#  endif
#endif
#include <assert.h>

#ifdef MALLOC_DEBUG
   /* Disable inlining to make debugging easier. */
#  define __inline
#endif

/* Size of stack-allocated buffer passed to strerror_r(). */
#define	STRERROR_BUF 64

/* Number of quantum-spaced bins to store free regions in. */
#define	NBINS 128

/* Minimum alignment of allocations is 2^QUANTUM_2POW_MIN bytes. */
#ifdef __i386__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR		4
#  define USE_BRK
#endif
#ifdef __ia64__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR		8
#  define NO_TLS
#endif
#ifdef __alpha__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR		8
#  define NO_TLS
#endif
#ifdef __sparc64__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR		8
#  define NO_TLS
#endif
#ifdef __amd64__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR		8
#endif
#ifdef __arm__
#  define QUANTUM_2POW_MIN	3
#  define SIZEOF_PTR		4
#  define USE_BRK
#  define NO_TLS
#endif
#ifdef __powerpc__
#  define QUANTUM_2POW_MIN	4
#  define SIZEOF_PTR		4
#  define USE_BRK
#endif

/* We can't use TLS in non-PIC programs, since TLS relies on loader magic. */
#if (!defined(PIC) && !defined(NO_TLS))
#  define NO_TLS
#endif

/*
 * Size and alignment of memory chunks that are allocated by the OS's virtual
 * memory system.
 *
 * chunksize limits:
 *
 *   pagesize <= chunk_size <= 2^29
 */
#define	CHUNK_2POW_DEFAULT	24
#define	CHUNK_2POW_MAX		29

/*
 * Maximum size of L1 cache line.  This is used to avoid cache line aliasing,
 * so over-estimates are okay (up to a point), but under-estimates will
 * negatively affect performance.
 */
#define	CACHELINE_2POW 6
#define	CACHELINE ((size_t) (1 << CACHELINE_2POW))

/* Default number of regions to delay coalescence for. */
#define NDELAY 256

/******************************************************************************/

/*
 * Mutexes based on spinlocks.  We can't use normal pthread mutexes, because
 * they require malloc()ed memory.
 */
typedef struct {
	spinlock_t	lock;
} malloc_mutex_t;

static bool malloc_initialized = false;

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

	/*
	 * Number of best-fit allocations that were successfully serviced by
	 * this bin.
	 */
	uint64_t	nfit;

	/*
	 * Number of allocation requests that were successfully serviced by this
	 * bin, but that a smaller bin could have serviced.
	 */
	uint64_t	noverfit;

	/* High-water marks for this bin. */
	unsigned long	highcached;

	/*
	 * Current number of regions in this bin.  This number isn't needed
	 * during normal operation, so is maintained here in order to allow
	 * calculating the high water mark.
	 */
	unsigned	nregions;
};

typedef struct arena_stats_s arena_stats_t;
struct arena_stats_s {
	/* Number of times each function was called. */
	uint64_t	nmalloc;
	uint64_t	npalloc;
	uint64_t	ncalloc;
	uint64_t	ndalloc;
	uint64_t	nralloc;

	/* Number of region splits. */
	uint64_t	nsplit;

	/* Number of region coalescences. */
	uint64_t	ncoalesce;

	/* Bin statistics. */
	malloc_bin_stats_t bins[NBINS];

	/* Split statistics. */
	struct {
		/*
		 * Number of times a region is requested from the "split" field
		 * of the arena.
		 */
		uint64_t	nrequests;

		/*
		 * Number of times the "split" field of the arena successfully
		 * services requests.
		 */
		uint64_t	nserviced;
	}	split;

	/* Frag statistics. */
	struct {
		/*
		 * Number of times a region is cached in the "frag" field of
		 * the arena.
		 */
		uint64_t	ncached;

		/*
		 * Number of times a region is requested from the "frag" field
		 * of the arena.
		 */
		uint64_t	nrequests;

		/*
		 * Number of times the "frag" field of the arena successfully
		 * services requests.
		 */
		uint64_t	nserviced;
	}	frag;

	/* large and large_regions statistics. */
	struct {
		/*
		 * Number of allocation requests that were too large for a bin,
		 * but not large enough for a hugh allocation.
		 */
		uint64_t	nrequests;

		/*
		 * Number of best-fit allocations that were successfully
		 * serviced by large_regions.
		 */
		uint64_t	nfit;

		/*
		 * Number of allocation requests that were successfully serviced
		 * large_regions, but that a bin could have serviced.
		 */
		uint64_t	noverfit;

		/*
		 * High-water mark for large_regions (number of nodes in tree).
		 */
		unsigned long	highcached;

		/*
		 * Used only to store the current number of nodes, since this
		 * number isn't maintained anywhere else.
		 */
		unsigned long	curcached;
	}	large;

	/* Huge allocation statistics. */
	struct {
		/* Number of huge allocation requests. */
		uint64_t	nrequests;
	}	huge;
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

/* Needed by chunk data structures. */
typedef struct	arena_s arena_t;

/* Tree of chunks. */
typedef struct chunk_node_s chunk_node_t;
struct chunk_node_s {
	/*
	 * For an active chunk that is currently carved into regions by an
	 * arena allocator, this points to the arena that owns the chunk.  We
	 * set this pointer even for huge allocations, so that it is possible
	 * to tell whether a huge allocation was done on behalf of a user
	 * allocation request, or on behalf of an internal allocation request.
	 */
	arena_t *arena;

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

	/* Number of trailing bytes that are not used. */
	size_t	extra;
};
typedef struct chunk_tree_s chunk_tree_t;
RB_HEAD(chunk_tree_s, chunk_node_s);

/******************************************************************************/
/*
 * Region data structures.
 */

typedef struct region_s region_t;

/*
 * Tree of region headers, used for free regions that don't fit in the arena
 * bins.
 */
typedef struct region_node_s region_node_t;
struct region_node_s {
	RB_ENTRY(region_node_s)	link;
	region_t		*reg;
};
typedef struct region_tree_s region_tree_t;
RB_HEAD(region_tree_s, region_node_s);

typedef struct region_prev_s region_prev_t;
struct region_prev_s {
	uint32_t	size;
};

#define NEXT_SIZE_MASK 0x1fffffffU
typedef struct {
#ifdef MALLOC_REDZONES
	char		prev_red[MALLOC_RED];
#endif
	/*
	 * Typical bit pattern for bits:
	 *
	 *   pncsssss ssssssss ssssssss ssssssss
	 *
	 *   p : Previous free?
	 *   n : Next free?
	 *   c : Part of a range of contiguous allocations?
	 *   s : Next size (number of quanta).
	 *
	 * It's tempting to use structure bitfields here, but the compiler has
	 * to make assumptions that make the code slower than direct bit
	 * manipulations, and these fields are manipulated a lot.
	 */
	uint32_t	bits;

#ifdef MALLOC_REDZONES
	size_t		next_exact_size;
	char		next_red[MALLOC_RED];
#endif
} region_sep_t;

typedef struct region_next_small_sizer_s region_next_small_sizer_t;
struct region_next_small_sizer_s
{
	qr(region_t)	link;
};

typedef struct region_next_small_s region_next_small_t;
struct region_next_small_s
{
	qr(region_t)	link;

	/* Only accessed for delayed regions & footer invalid. */
	uint32_t	slot;
};

typedef struct region_next_large_s region_next_large_t;
struct region_next_large_s
{
	region_node_t	node;

	/* Use for LRU vs MRU tree ordering. */
	bool		lru;
};

typedef struct region_next_s region_next_t;
struct region_next_s {
	union {
		region_next_small_t	s;
		region_next_large_t	l;
	}	u;
};

/*
 * Region separator, including prev/next fields that are only accessible when
 * the neighboring regions are free.
 */
struct region_s {
	/* This field must only be accessed if sep.prev_free is true. */
	region_prev_t   prev;

	/* Actual region separator that is always present between regions. */
	region_sep_t    sep;

	/*
	 * These fields must only be accessed if sep.next_free or
	 * sep.next_contig is true. 
	 */
	region_next_t   next;
};

/* Small variant of region separator, only used for size calculations. */
typedef struct region_small_sizer_s region_small_sizer_t;
struct region_small_sizer_s {
	region_prev_t   prev;
	region_sep_t    sep;
	region_next_small_sizer_t   next;
};

/******************************************************************************/
/*
 * Arena data structures.
 */

typedef struct arena_bin_s arena_bin_t;
struct arena_bin_s {
	/*
	 * Link into ring before the oldest free region and just after the
	 * newest free region.
	 */
	region_t	regions;
};

struct arena_s {
#ifdef MALLOC_DEBUG
	uint32_t	magic;
#  define ARENA_MAGIC 0x947d3d24
#endif

	/* All operations on this arena require that mtx be locked. */
	malloc_mutex_t	mtx;

	/*
	 * bins is used to store rings of free regions of the following sizes,
	 * assuming a 16-byte quantum (sizes include region separators):
	 *
	 *   bins[i] | size |
	 *   --------+------+
	 *        0  |   32 |
	 *        1  |   48 |
	 *        2  |   64 |
	 *           :      :
	 *           :      :
	 *   --------+------+
	 */
	arena_bin_t	bins[NBINS];

	/*
	 * A bitmask that corresponds to which bins have elements in them.
	 * This is used when searching for the first bin that contains a free
	 * region that is large enough to service an allocation request.
	 */
#define	BINMASK_NELMS (NBINS / (sizeof(int) << 3))
	int		bins_mask[BINMASK_NELMS];

	/*
	 * Tree of free regions of the size range [bin_maxsize..~chunk).  These
	 * are sorted primarily by size, and secondarily by LRU.
	 */
	region_tree_t	large_regions;

	/*
	 * If not NULL, a region that is not stored in bins or large_regions.
	 * If large enough, this region is used instead of any region stored in
	 * bins or large_regions, in order to reduce the number of insert/remove
	 * operations, and in order to increase locality of allocation in
	 * common cases.
	 */
	region_t	*split;

	/*
	 * If not NULL, a region that is not stored in bins or large_regions.
	 * If large enough, this region is preferentially used for small
	 * allocations over any region in large_regions, split, or over-fit
	 * small bins.
	 */
	region_t	*frag;

	/* Tree of chunks that this arena currenly owns. */
	chunk_tree_t	chunks;
	unsigned	nchunks;

	/*
	 * FIFO ring of free regions for which coalescence is delayed.  A slot
	 * that contains NULL is considered empty.  opt_ndelay stores how many
	 * elements there are in the FIFO.
	 */
	region_t	**delayed;
	uint32_t	next_delayed; /* Next slot in delayed to use. */

#ifdef MALLOC_STATS
	/* Total byte count of allocated memory, not including overhead. */
	size_t		allocated;

	arena_stats_t stats;
#endif
};

/******************************************************************************/
/*
 * Data.
 */

/* Used as a special "nil" return value for malloc(0). */
static int		nil;

/* Number of CPUs. */
static unsigned		ncpus;

/* VM page size. */
static unsigned		pagesize;

/* Various quantum-related settings. */
static size_t		quantum;
static size_t		quantum_mask; /* (quantum - 1). */
static size_t		bin_shift;
static size_t		bin_maxsize;

/* Various chunk-related settings. */
static size_t		chunk_size;
static size_t		chunk_size_mask; /* (chunk_size - 1). */

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
void *brk_base; /* Result of first sbrk(0) call. */
void *brk_prev; /* Current end of brk, or ((void *)-1) if brk is exhausted. */
void *brk_max; /* Upper limit on brk addresses (may be an over-estimate). */
#endif

#ifdef MALLOC_STATS
/*
 * Byte counters for allocated/total space used by the chunks in the huge
 * allocations tree.
 */
static size_t		huge_allocated;
static size_t		huge_total;
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
 * Current chunk that is being used for internal memory allocations.  This
 * chunk is carved up in cacheline-size quanta, so that there is no chance of
 * false cach sharing. 
 * */
void			*base_chunk;
void			*base_next_addr;
void			*base_past_addr; /* Addr immediately past base_chunk. */
chunk_node_t		*base_chunk_nodes; /* LIFO cache of chunk nodes. */
malloc_mutex_t		base_mtx;
#ifdef MALLOC_STATS
uint64_t		base_total;
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
static __thread arena_t *arenas_map;
#endif

#ifdef MALLOC_STATS
/* Chunk statistics. */
chunk_stats_t		stats_chunks;
#endif

/*******************************/
/*
 * Runtime configuration options.
 */
const char	*_malloc_options;

static bool	opt_abort = true;
static bool	opt_junk = true;
static bool	opt_print_stats = false;
static size_t	opt_quantum_2pow = QUANTUM_2POW_MIN;
static size_t	opt_chunk_2pow = CHUNK_2POW_DEFAULT;
static bool	opt_utrace = false;
static bool	opt_sysv = false;
static bool	opt_xmalloc = false;
static bool	opt_zero = false;
static uint32_t	opt_ndelay = NDELAY;
static int32_t	opt_narenas_lshift = 0;

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
static void	malloc_printf(const char *format, ...);
static void	*base_alloc(size_t size);
static chunk_node_t *base_chunk_node_alloc(void);
static void	base_chunk_node_dealloc(chunk_node_t *node);
#ifdef MALLOC_STATS
static void	stats_merge(arena_t *arena, arena_stats_t *stats_arenas);
static void	stats_print(arena_stats_t *stats_arenas);
#endif
static void	*pages_map(void *addr, size_t size);
static void	pages_unmap(void *addr, size_t size);
static void	*chunk_alloc(size_t size);
static void	chunk_dealloc(void *chunk, size_t size);
static unsigned	arena_bins_search(arena_t *arena, size_t size);
static bool	arena_coalesce(arena_t *arena, region_t **reg, size_t size);
static void	arena_coalesce_hard(arena_t *arena, region_t *reg,
		region_t *next, size_t size, bool split_adjacent);
static void	arena_large_insert(arena_t *arena, region_t *reg, bool lru);
static void	arena_large_cache(arena_t *arena, region_t *reg, bool lru);
static void	arena_lru_cache(arena_t *arena, region_t *reg);
static void	arena_delay_cache(arena_t *arena, region_t *reg);
static region_t	*arena_split_reg_alloc(arena_t *arena, size_t size, bool fit);
static void	arena_reg_fit(arena_t *arena, size_t size, region_t *reg,
		bool restore_split);
static region_t	*arena_large_reg_alloc(arena_t *arena, size_t size, bool fit);
static region_t	*arena_chunk_reg_alloc(arena_t *arena, size_t size, bool fit);
static void	*arena_malloc(arena_t *arena, size_t size);
static void	*arena_palloc(arena_t *arena, size_t alignment, size_t size);
static void	*arena_calloc(arena_t *arena, size_t num, size_t size);
static size_t	arena_salloc(arena_t *arena, void *ptr);
#ifdef MALLOC_REDZONES
static void	redzone_check(void *ptr);
#endif
static void	arena_dalloc(arena_t *arena, void *ptr);
#ifdef NOT_YET
static void	*arena_ralloc(arena_t *arena, void *ptr, size_t size);
#endif
#ifdef MALLOC_STATS
static bool	arena_stats(arena_t *arena, size_t *allocated, size_t *total);
#endif
static bool	arena_new(arena_t *arena);
static arena_t	*arenas_extend(unsigned ind);
#ifndef NO_TLS
static arena_t	*choose_arena_hard(void);
#endif
static void	*huge_malloc(arena_t *arena, size_t size);
static void	huge_dalloc(void *ptr);
static void	*imalloc(arena_t *arena, size_t size);
static void	*ipalloc(arena_t *arena, size_t alignment, size_t size);
static void	*icalloc(arena_t *arena, size_t num, size_t size);
static size_t	isalloc(void *ptr);
static void	idalloc(void *ptr);
static void	*iralloc(arena_t *arena, void *ptr, size_t size);
#ifdef MALLOC_STATS
static void	istats(size_t *allocated, size_t *total);
#endif
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

static __inline void
malloc_mutex_lock(malloc_mutex_t *a_mutex)
{

	if (__isthreaded)
		_SPINLOCK(&a_mutex->lock);
}

static __inline void
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
	((void *) ((size_t) (a) & ~chunk_size_mask))

/* Return the chunk offset of address a. */
#define	CHUNK_ADDR2OFFSET(a)						\
	((size_t) (a) & chunk_size_mask)

/* Return the smallest chunk multiple that is >= s. */
#define	CHUNK_CEILING(s)						\
	(((s) + chunk_size_mask) & ~chunk_size_mask)

/* Return the smallest cacheline multiple that is >= s. */
#define	CACHELINE_CEILING(s)						\
	(((s) + (CACHELINE - 1)) & ~(CACHELINE - 1))

/* Return the smallest quantum multiple that is >= a. */
#define	QUANTUM_CEILING(a)						\
	(((a) + quantum_mask) & ~quantum_mask)

/* Return the offset within a chunk to the first region separator. */
#define	CHUNK_REG_OFFSET						\
	(QUANTUM_CEILING(sizeof(chunk_node_t) +				\
	    sizeof(region_sep_t)) - offsetof(region_t, next))

/*
 * Return how many bytes of usable space are needed for an allocation of size
 * bytes.  This value is not a multiple of quantum, since it doesn't include
 * the region separator.
 */
static __inline size_t
region_ceiling(size_t size)
{
	size_t quantum_size, min_reg_quantum;

	quantum_size = QUANTUM_CEILING(size + sizeof(region_sep_t));
	min_reg_quantum = QUANTUM_CEILING(sizeof(region_small_sizer_t));
	if (quantum_size >= min_reg_quantum)
		return (quantum_size);
	else
		return (min_reg_quantum);
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

/******************************************************************************/

static void *
base_alloc(size_t size)
{
	void *ret;
	size_t csize;

	/* Round size up to nearest multiple of the cacheline size. */
	csize = CACHELINE_CEILING(size);

	malloc_mutex_lock(&base_mtx);

	/* Make sure there's enough space for the allocation. */
	if ((size_t)base_next_addr + csize > (size_t)base_past_addr) {
		void *tchunk;
		size_t alloc_size;

		/*
		 * If chunk_size and opt_ndelay are sufficiently small and
		 * large, respectively, it's possible for an allocation request
		 * to exceed a single chunk here.  Deal with this, but don't
		 * worry about internal fragmentation.
		 */

		if (csize <= chunk_size)
			alloc_size = chunk_size;
		else
			alloc_size = CHUNK_CEILING(csize);

		tchunk = chunk_alloc(alloc_size);
		if (tchunk == NULL) {
			ret = NULL;
			goto RETURN;
		}
		base_chunk = tchunk;
		base_next_addr = (void *)base_chunk;
		base_past_addr = (void *)((size_t)base_chunk + alloc_size);
#ifdef MALLOC_STATS
		base_total += alloc_size;
#endif
	}

	/* Allocate. */
	ret = base_next_addr;
	base_next_addr = (void *)((size_t)base_next_addr + csize);

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

/*
 * Note that no bitshifting is done for booleans in any of the bitmask-based
 * flag manipulation functions that follow; test for non-zero versus zero.
 */

/**********************/
static __inline uint32_t
region_prev_free_get(region_sep_t *sep)
{

	return ((sep->bits) & 0x80000000U);
}

static __inline void
region_prev_free_set(region_sep_t *sep)
{

	sep->bits = ((sep->bits) | 0x80000000U);
}

static __inline void
region_prev_free_unset(region_sep_t *sep)
{

	sep->bits = ((sep->bits) & 0x7fffffffU);
}

/**********************/
static __inline uint32_t
region_next_free_get(region_sep_t *sep)
{

	return ((sep->bits) & 0x40000000U);
}

static __inline void
region_next_free_set(region_sep_t *sep)
{

	sep->bits = ((sep->bits) | 0x40000000U);
}

static __inline void
region_next_free_unset(region_sep_t *sep)
{

	sep->bits = ((sep->bits) & 0xbfffffffU);
}

/**********************/
static __inline uint32_t
region_next_contig_get(region_sep_t *sep)
{

	return ((sep->bits) & 0x20000000U);
}

static __inline void
region_next_contig_set(region_sep_t *sep)
{

	sep->bits = ((sep->bits) | 0x20000000U);
}

static __inline void
region_next_contig_unset(region_sep_t *sep)
{

	sep->bits = ((sep->bits) & 0xdfffffffU);
}

/********************/
static __inline size_t
region_next_size_get(region_sep_t *sep)
{

	return ((size_t) (((sep->bits) & NEXT_SIZE_MASK) << opt_quantum_2pow));
}

static __inline void
region_next_size_set(region_sep_t *sep, size_t size)
{
	uint32_t bits;

	assert(size % quantum == 0);

	bits = sep->bits;
	bits &= ~NEXT_SIZE_MASK;
	bits |= (((uint32_t) size) >> opt_quantum_2pow);

	sep->bits = bits;
}

#ifdef MALLOC_STATS
static void
stats_merge(arena_t *arena, arena_stats_t *stats_arenas)
{
	unsigned i;

	stats_arenas->nmalloc += arena->stats.nmalloc;
	stats_arenas->npalloc += arena->stats.npalloc;
	stats_arenas->ncalloc += arena->stats.ncalloc;
	stats_arenas->ndalloc += arena->stats.ndalloc;
	stats_arenas->nralloc += arena->stats.nralloc;

	stats_arenas->nsplit += arena->stats.nsplit;
	stats_arenas->ncoalesce += arena->stats.ncoalesce;

	/* Split. */
	stats_arenas->split.nrequests += arena->stats.split.nrequests;
	stats_arenas->split.nserviced += arena->stats.split.nserviced;

	/* Frag. */
	stats_arenas->frag.ncached += arena->stats.frag.ncached;
	stats_arenas->frag.nrequests += arena->stats.frag.nrequests;
	stats_arenas->frag.nserviced += arena->stats.frag.nserviced;

	/* Bins. */
	for (i = 0; i < NBINS; i++) {
		stats_arenas->bins[i].nrequests +=
		    arena->stats.bins[i].nrequests;
		stats_arenas->bins[i].nfit += arena->stats.bins[i].nfit;
		stats_arenas->bins[i].noverfit += arena->stats.bins[i].noverfit;
		if (arena->stats.bins[i].highcached
		    > stats_arenas->bins[i].highcached) {
		    stats_arenas->bins[i].highcached
			= arena->stats.bins[i].highcached;
		}
	}

	/* large and large_regions. */
	stats_arenas->large.nrequests += arena->stats.large.nrequests;
	stats_arenas->large.nfit += arena->stats.large.nfit;
	stats_arenas->large.noverfit += arena->stats.large.noverfit;
	if (arena->stats.large.highcached > stats_arenas->large.highcached)
		stats_arenas->large.highcached = arena->stats.large.highcached;
	stats_arenas->large.curcached += arena->stats.large.curcached;

	/* Huge allocations. */
	stats_arenas->huge.nrequests += arena->stats.huge.nrequests;
}

static void
stats_print(arena_stats_t *stats_arenas)
{
	unsigned i;

	malloc_printf("calls:\n");
	malloc_printf(" %13s%13s%13s%13s%13s\n", "nmalloc", "npalloc",
	    "ncalloc", "ndalloc", "nralloc");
	malloc_printf(" %13llu%13llu%13llu%13llu%13llu\n",
	    stats_arenas->nmalloc, stats_arenas->npalloc, stats_arenas->ncalloc,
	    stats_arenas->ndalloc, stats_arenas->nralloc);

	malloc_printf("region events:\n");
	malloc_printf(" %13s%13s\n", "nsplit", "ncoalesce");
	malloc_printf(" %13llu%13llu\n", stats_arenas->nsplit,
	    stats_arenas->ncoalesce);

	malloc_printf("cached split usage:\n");
	malloc_printf(" %13s%13s\n", "nrequests", "nserviced");
	malloc_printf(" %13llu%13llu\n", stats_arenas->split.nrequests,
	    stats_arenas->split.nserviced);

	malloc_printf("cached frag usage:\n");
	malloc_printf(" %13s%13s%13s\n", "ncached", "nrequests", "nserviced");
	malloc_printf(" %13llu%13llu%13llu\n", stats_arenas->frag.ncached,
	    stats_arenas->frag.nrequests, stats_arenas->frag.nserviced);

	malloc_printf("bins:\n");
	malloc_printf(" %4s%7s%13s%13s%13s%11s\n", "bin", 
	    "size", "nrequests", "nfit", "noverfit", "highcached");
	for (i = 0; i < NBINS; i++) {
		malloc_printf(
		    " %4u%7u%13llu%13llu%13llu%11lu\n",
		    i, ((i + bin_shift) << opt_quantum_2pow),
		    stats_arenas->bins[i].nrequests, stats_arenas->bins[i].nfit,
		    stats_arenas->bins[i].noverfit,
		    stats_arenas->bins[i].highcached);
	}

	malloc_printf("large:\n");
	malloc_printf(" %13s%13s%13s%13s%13s\n", "nrequests", "nfit",
	    "noverfit", "highcached", "curcached");
	malloc_printf(" %13llu%13llu%13llu%13lu%13lu\n",
	    stats_arenas->large.nrequests, stats_arenas->large.nfit,
	    stats_arenas->large.noverfit, stats_arenas->large.highcached,
	    stats_arenas->large.curcached);

	malloc_printf("huge\n");
	malloc_printf(" %13s\n", "nrequests");
	malloc_printf(" %13llu\n", stats_arenas->huge.nrequests);
}
#endif

/*
 * End Utility functions/macros.
 */
/******************************************************************************/
/*
 * Begin Mem.
 */

static __inline int
chunk_comp(chunk_node_t *a, chunk_node_t *b)
{
	int ret;

	assert(a != NULL);
	assert(b != NULL);

	if ((size_t) a->chunk < (size_t) b->chunk)
		ret = -1;
	else if (a->chunk == b->chunk)
		ret = 0;
	else
		ret = 1;

	return (ret);
}

/* Generate red-black tree code for chunks. */
RB_GENERATE(chunk_tree_s, chunk_node_s, link, chunk_comp);

static __inline int
region_comp(region_node_t *a, region_node_t *b)
{
	int ret;
	size_t size_a, size_b;

	assert(a != NULL);
	assert(b != NULL);

	size_a = region_next_size_get(&a->reg->sep);
	size_b = region_next_size_get(&b->reg->sep);
	if (size_a < size_b)
		ret = -1;
	else if (size_a == size_b) {
		if (a == b) {
			/* Regions are equal with themselves. */
			ret = 0;
		} else {
			if (a->reg->next.u.l.lru) {
				/*
				 * Oldest region comes first (secondary LRU
				 * ordering).  a is guaranteed to be the search
				 * key, which is how we can enforce this
				 * secondary ordering.
				 */
				ret = 1;
			} else {
				/*
				 * Oldest region comes last (secondary MRU
				 * ordering).  a is guaranteed to be the search
				 * key, which is how we can enforce this
				 * secondary ordering.
				 */
				ret = -1;
			}
		}
	} else
		ret = 1;

	return (ret);
}

/* Generate red-black tree code for regions. */
RB_GENERATE(region_tree_s, region_node_s, link, region_comp);

static void *
pages_map(void *addr, size_t size)
{
	void *ret;

#ifdef USE_BRK
AGAIN:
#endif
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
			malloc_printf("%s: (malloc) Error in munmap(): %s\n",
			    _getprogname(), buf);
			if (opt_abort)
				abort();
		}
		ret = NULL;
	}
#ifdef USE_BRK
	else if ((size_t)ret >= (size_t)brk_base
	    && (size_t)ret < (size_t)brk_max) {
		/*
		 * We succeeded in mapping memory, but at a location that could
		 * be confused with brk.  Leave the mapping intact so that this
		 * won't ever happen again, then try again.
		 */
		assert(addr == NULL);
		goto AGAIN;
	}
#endif

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
		malloc_printf("%s: (malloc) Error in munmap(): %s\n",
		    _getprogname(), buf);
		if (opt_abort)
			abort();
	}
}

static void *
chunk_alloc(size_t size)
{
	void *ret, *chunk;
	chunk_node_t *tchunk, *delchunk;
	chunk_tree_t delchunks;

	assert(size != 0);
	assert(size % chunk_size == 0);

	RB_INIT(&delchunks);

	malloc_mutex_lock(&chunks_mtx);

	if (size == chunk_size) {
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

			/*
			 * Remove delchunk from the tree, but keep track of the
			 * address.
			 */
			RB_REMOVE(chunk_tree_s, &old_chunks, delchunk);

			/*
			 * Keep track of the node so that it can be deallocated
			 * after chunks_mtx is released.
			 */
			RB_INSERT(chunk_tree_s, &delchunks, delchunk);

#ifdef USE_BRK
			if ((size_t)chunk >= (size_t)brk_base
			    && (size_t)chunk < (size_t)brk_max) {
				/* Re-use a previously freed brk chunk. */
				ret = chunk;
				goto RETURN;
			}
#endif
			if ((ret = pages_map(chunk, size)) != NULL) {
				/* Success. */
				goto RETURN;
			}
		}

#ifdef USE_BRK
		/*
		 * Try to create chunk-size allocations in brk, in order to
		 * make full use of limited address space.
		 */
		if (brk_prev != (void *)-1) {
			void *brk_cur;
			intptr_t incr;

			/*
			 * The loop is necessary to recover from races with
			 * other threads that are using brk for something other
			 * than malloc.
			 */
			do {
				/* Get the current end of brk. */
				brk_cur = sbrk(0);

				/*
				 * Calculate how much padding is necessary to
				 * chunk-align the end of brk.
				 */
				incr = (char *)chunk_size
				    - (char *)CHUNK_ADDR2OFFSET(brk_cur);
				if (incr == chunk_size) {
					ret = brk_cur;
				} else {
					ret = (char *)brk_cur + incr;
					incr += chunk_size;
				}

				brk_prev = sbrk(incr);
				if (brk_prev == brk_cur) {
					/* Success. */
					goto RETURN;
				}
			} while (brk_prev != (void *)-1);
		}
#endif
	}

	/*
	 * Try to over-allocate, but allow the OS to place the allocation
	 * anywhere.  Beware of size_t wrap-around.
	 */
	if (size + chunk_size > size) {
		if ((ret = pages_map(NULL, size + chunk_size)) != NULL) {
			size_t offset = CHUNK_ADDR2OFFSET(ret);

			/*
			 * Success.  Clean up unneeded leading/trailing space.
			 */
			if (offset != 0) {
				/* Leading space. */
				pages_unmap(ret, chunk_size - offset);

				ret = (void *) ((size_t) ret + (chunk_size -
				    offset));

				/* Trailing space. */
				pages_unmap((void *) ((size_t) ret + size),
				    offset);
			} else {
				/* Trailing space only. */
				pages_unmap((void *) ((size_t) ret + size),
				    chunk_size);
			}
			goto RETURN;
		}
	}

	/* All strategies for allocation failed. */
	ret = NULL;
RETURN:
#ifdef MALLOC_STATS
	if (ret != NULL) {
		stats_chunks.nchunks += (size / chunk_size);
		stats_chunks.curchunks += (size / chunk_size);
	}
	if (stats_chunks.curchunks > stats_chunks.highchunks)
		stats_chunks.highchunks = stats_chunks.curchunks;
#endif
	malloc_mutex_unlock(&chunks_mtx);

	/*
	 * Deallocation of the chunk nodes must be done after releasing
	 * chunks_mtx, in case deallocation causes a chunk to be unmapped.
	 */
	tchunk = RB_MIN(chunk_tree_s, &delchunks);
	while (tchunk != NULL) {
		delchunk = tchunk;
		tchunk = RB_NEXT(chunk_tree_s, &delchunks, delchunk);
		RB_REMOVE(chunk_tree_s, &delchunks, delchunk);
		base_chunk_node_dealloc(delchunk);
	}

	assert(CHUNK_ADDR2BASE(ret) == ret);
	return (ret);
}

static void
chunk_dealloc(void *chunk, size_t size)
{

	assert(chunk != NULL);
	assert(CHUNK_ADDR2BASE(chunk) == chunk);
	assert(size != 0);
	assert(size % chunk_size == 0);

	if (size == chunk_size) {
		chunk_node_t *node;

		node = base_chunk_node_alloc();

		malloc_mutex_lock(&chunks_mtx);
		if (node != NULL) {
			/*
			 * Create a record of this chunk before deallocating
			 * it, so that the address range can be recycled if
			 * memory usage increases later on.
			 */
			node->arena = NULL;
			node->chunk = chunk;
			node->size = size;
			node->extra = 0;

			RB_INSERT(chunk_tree_s, &old_chunks, node);
		}
		malloc_mutex_unlock(&chunks_mtx);
	}

#ifdef USE_BRK
	if ((size_t)chunk >= (size_t)brk_base
	    && (size_t)chunk < (size_t)brk_max)
		madvise(chunk, size, MADV_FREE);
	else
#endif
		pages_unmap(chunk, size);

#ifdef MALLOC_STATS
	malloc_mutex_lock(&chunks_mtx);
	stats_chunks.curchunks -= (size / chunk_size);
	malloc_mutex_unlock(&chunks_mtx);
#endif
}

/******************************************************************************/
/*
 * arena.
 */

static __inline void
arena_mask_set(arena_t *arena, unsigned bin)
{
	unsigned elm, bit;

	assert(bin < NBINS);

	elm = bin / (sizeof(int) << 3);
	bit = bin - (elm * (sizeof(int) << 3));
	assert((arena->bins_mask[elm] & (1 << bit)) == 0);
	arena->bins_mask[elm] |= (1 << bit);
}

static __inline void
arena_mask_unset(arena_t *arena, unsigned bin)
{
	unsigned elm, bit;

	assert(bin < NBINS);

	elm = bin / (sizeof(int) << 3);
	bit = bin - (elm * (sizeof(int) << 3));
	assert((arena->bins_mask[elm] & (1 << bit)) != 0);
	arena->bins_mask[elm] ^= (1 << bit);
}

static unsigned
arena_bins_search(arena_t *arena, size_t size)
{
	unsigned ret, minbin, i;
	int bit;

	assert(QUANTUM_CEILING(size) == size);
	assert((size >> opt_quantum_2pow) >= bin_shift);

	if (size > bin_maxsize) {
		ret = UINT_MAX;
		goto RETURN;
	}

	minbin = (size >> opt_quantum_2pow) - bin_shift;
	assert(minbin < NBINS);
	for (i = minbin / (sizeof(int) << 3); i < BINMASK_NELMS; i++) {
		bit = ffs(arena->bins_mask[i]
		    & (UINT_MAX << (minbin % (sizeof(int) << 3))));
		if (bit != 0) {
			/* Usable allocation found. */
			ret = (i * (sizeof(int) << 3)) + bit - 1;
#ifdef MALLOC_STATS
			if (ret == minbin)
				arena->stats.bins[minbin].nfit++;
			else
				arena->stats.bins[ret].noverfit++;
#endif
			goto RETURN;
		}
	}

	ret = UINT_MAX;
RETURN:
	return (ret);
}

static __inline void
arena_delayed_extract(arena_t *arena, region_t *reg)
{

	if (region_next_contig_get(&reg->sep)) {
		uint32_t slot;

		/* Extract this region from the delayed FIFO. */
		assert(region_next_free_get(&reg->sep) == false);

		slot = reg->next.u.s.slot;
		assert(arena->delayed[slot] == reg);
		arena->delayed[slot] = NULL;
	}
#ifdef MALLOC_DEBUG
	else {
		region_t *next;

		assert(region_next_free_get(&reg->sep));

		next = (region_t *) &((char *) reg)
		    [region_next_size_get(&reg->sep)];
		assert(region_prev_free_get(&next->sep));
	}
#endif
}

static __inline void
arena_bin_extract(arena_t *arena, unsigned bin, region_t *reg)
{
	arena_bin_t *tbin;

	assert(bin < NBINS);

	tbin = &arena->bins[bin];

	assert(qr_next(&tbin->regions, next.u.s.link) != &tbin->regions);
#ifdef MALLOC_DEBUG
	{
		region_t *next;

		next = (region_t *) &((char *) reg)
		    [region_next_size_get(&reg->sep)];
		if (region_next_free_get(&reg->sep)) {
			assert(region_prev_free_get(&next->sep));
			assert(region_next_size_get(&reg->sep)
			    == next->prev.size);
		} else {
			assert(region_prev_free_get(&next->sep) == false);
		}
	}
#endif
	assert(region_next_size_get(&reg->sep)
	    == ((bin + bin_shift) << opt_quantum_2pow));

	qr_remove(reg, next.u.s.link);
#ifdef MALLOC_STATS
	arena->stats.bins[bin].nregions--;
#endif
	if (qr_next(&tbin->regions, next.u.s.link) == &tbin->regions)
		arena_mask_unset(arena, bin);

	arena_delayed_extract(arena, reg);
}

static __inline void
arena_extract(arena_t *arena, region_t *reg)
{
	size_t size;

	assert(region_next_free_get(&reg->sep));
#ifdef MALLOC_DEBUG
	{
		region_t *next;

		next = (region_t *)&((char *)reg)
		    [region_next_size_get(&reg->sep)];
	}
#endif

	assert(reg != arena->split);
	assert(reg != arena->frag);
	if ((size = region_next_size_get(&reg->sep)) <= bin_maxsize) {
		arena_bin_extract(arena, (size >> opt_quantum_2pow) - bin_shift,
		    reg);
	} else {
		RB_REMOVE(region_tree_s, &arena->large_regions,
		    &reg->next.u.l.node);
#ifdef MALLOC_STATS
		arena->stats.large.curcached--;
#endif
	}
}

/* Try to coalesce reg with its neighbors.  Return NULL if coalescing fails. */
static bool
arena_coalesce(arena_t *arena, region_t **reg, size_t size)
{
	bool ret;
	region_t *prev, *treg, *next, *nextnext;
	size_t tsize, prev_size, next_size;

	ret = false;

	treg = *reg;

	/*
	 * Keep track of the size while coalescing, then just set the size in
	 * the header/footer once at the end of coalescing.
	 */
	assert(size == region_next_size_get(&(*reg)->sep));
	tsize = size;

	next = (region_t *)&((char *)treg)[tsize];
	assert(region_next_free_get(&treg->sep));
	assert(region_prev_free_get(&next->sep));
	assert(region_next_size_get(&treg->sep) == next->prev.size);

	while (region_prev_free_get(&treg->sep)) {
		prev_size = treg->prev.size;
		prev = (region_t *)&((char *)treg)[-prev_size];
		assert(region_next_free_get(&prev->sep));

		arena_extract(arena, prev);

		tsize += prev_size;

		treg = prev;

#ifdef MALLOC_STATS
		if (ret == false)
			arena->stats.ncoalesce++;
#endif
		ret = true;
	}

	while (region_next_free_get(&next->sep)) {
		next_size = region_next_size_get(&next->sep);
		nextnext = (region_t *)&((char *)next)[next_size];
		assert(region_prev_free_get(&nextnext->sep));

		assert(region_next_size_get(&next->sep) == nextnext->prev.size);

		arena_extract(arena, next);

		assert(region_next_size_get(&next->sep) == nextnext->prev.size);

		tsize += next_size;

#ifdef MALLOC_STATS
		if (ret == false)
			arena->stats.ncoalesce++;
#endif
		ret = true;

		next = (region_t *)&((char *)treg)[tsize];
	}

	/* Update header/footer. */
	if (ret) {
		region_next_size_set(&treg->sep, tsize);
		next->prev.size = tsize;
	}

	/*
	 * Now that coalescing with adjacent free regions is done, we need to
	 * try to coalesce with "split" and "frag".  Those two regions are
	 * marked as allocated, which is why this takes special effort.  There
	 * are seven possible cases, but we want to make the (hopefully) common
	 * case of no coalescence fast, so the checks are optimized for that
	 * case.  The seven cases are:
	 *
	 *   /------\
	 * 0 | treg | No coalescence needed.  Make this case fast.
	 *   \------/
	 *
	 *   /------+------\
	 * 1 | frag | treg |
	 *   \------+------/
	 *
	 *   /------+------\
	 * 2 | treg | frag |
	 *   \------+------/
	 *
	 *   /-------+------\
	 * 3 | split | treg |
	 *   \-------+------/
	 *
	 *   /------+-------\
	 * 4 | treg | split |
	 *   \------+-------/
	 *
	 *   /------+------+-------\
	 * 5 | frag | treg | split |
	 *   \------+------+-------/
	 *
	 *   /-------+------+------\
	 * 6 | split | treg | frag |
	 *   \-------+------+------/
	 */

	if (arena->split == NULL) {
		/* Cases 3-6 ruled out. */
	} else if ((size_t)next < (size_t)arena->split) {
		/* Cases 3-6 ruled out. */
	} else {
		region_t *split_next;
		size_t split_size;

		split_size = region_next_size_get(&arena->split->sep);
		split_next = (region_t *)&((char *)arena->split)[split_size];

		if ((size_t)split_next < (size_t)treg) {
			/* Cases 3-6 ruled out. */
		} else {
			/*
			 * Split is adjacent to treg.  Take the slow path and
			 * coalesce.
			 */

			arena_coalesce_hard(arena, treg, next, tsize, true);

			treg = NULL;
#ifdef MALLOC_STATS
			if (ret == false)
				arena->stats.ncoalesce++;
#endif
			ret = true;
			goto RETURN;
		}
	}

	/* If we get here, then cases 3-6 have been ruled out. */
	if (arena->frag == NULL) {
		/* Cases 1-6 ruled out. */
	} else if ((size_t)next < (size_t)arena->frag) {
		/* Cases 1-6 ruled out. */
	} else {
		region_t *frag_next;
		size_t frag_size;

		frag_size = region_next_size_get(&arena->frag->sep);
		frag_next = (region_t *)&((char *)arena->frag)[frag_size];

		if ((size_t)frag_next < (size_t)treg) {
			/* Cases 1-6 ruled out. */
		} else {
			/*
			 * Frag is adjacent to treg.  Take the slow path and
			 * coalesce.
			 */

			arena_coalesce_hard(arena, treg, next, tsize, false);

			treg = NULL;
#ifdef MALLOC_STATS
			if (ret == false)
				arena->stats.ncoalesce++;
#endif
			ret = true;
			goto RETURN;
		}
	}

	/* If we get here, no coalescence with "split" or "frag" was needed. */

	/* Finish updating header. */
	region_next_contig_unset(&treg->sep);

	assert(region_next_free_get(&treg->sep));
	assert(region_prev_free_get(&next->sep));
	assert(region_prev_free_get(&treg->sep) == false);
	assert(region_next_free_get(&next->sep) == false);

RETURN:
	if (ret)
		*reg = treg;
	return (ret);
}

/*
 * arena_coalesce() calls this function if it determines that a region needs to
 * be coalesced with "split" and/or "frag".
 */
static void
arena_coalesce_hard(arena_t *arena, region_t *reg, region_t *next, size_t size,
    bool split_adjacent)
{
	bool frag_adjacent;

	assert(next == (region_t *)&((char *)reg)[size]);
	assert(region_next_free_get(&reg->sep));
	assert(region_next_size_get(&reg->sep) == size);
	assert(region_prev_free_get(&next->sep));
	assert(next->prev.size == size);

	if (split_adjacent == false)
		frag_adjacent = true;
	else if (arena->frag != NULL) {
		/* Determine whether frag will be coalesced with. */

		if ((size_t)next < (size_t)arena->frag)
			frag_adjacent = false;
		else {
			region_t *frag_next;
			size_t frag_size;

			frag_size = region_next_size_get(&arena->frag->sep);
			frag_next = (region_t *)&((char *)arena->frag)
			    [frag_size];

			if ((size_t)frag_next < (size_t)reg)
				frag_adjacent = false;
			else
				frag_adjacent = true;
		}
	} else
		frag_adjacent = false;

	if (split_adjacent && frag_adjacent) {
		region_t *a;
		size_t a_size, b_size;

		/* Coalesce all three regions. */

		if (arena->frag == next)
			a = arena->split;
		else {
			a = arena->frag;
			arena->split = a;
		}
		arena->frag = NULL;

		a_size = region_next_size_get(&a->sep);
		assert(a_size == (size_t)reg - (size_t)a);

		b_size = region_next_size_get(&next->sep);

		region_next_size_set(&a->sep, a_size + size + b_size);
		assert(region_next_free_get(&a->sep) == false);
	} else {
		/* Coalesce two regions. */

		if (split_adjacent) {
			size += region_next_size_get(&arena->split->sep);
			if (arena->split == next) {
				/* reg comes before split. */
				region_next_size_set(&reg->sep, size);
				
				assert(region_next_free_get(&reg->sep));
				region_next_free_unset(&reg->sep);

				arena->split = reg;
			} else {
				/* reg comes after split. */
				region_next_size_set(&arena->split->sep, size);

				assert(region_next_free_get(&arena->split->sep)
				    == false);

				assert(region_prev_free_get(&next->sep));
				region_prev_free_unset(&next->sep);
			}
		} else {
			assert(frag_adjacent);
			size += region_next_size_get(&arena->frag->sep);
			if (arena->frag == next) {
				/* reg comes before frag. */
				region_next_size_set(&reg->sep, size);

				assert(region_next_free_get(&reg->sep));
				region_next_free_unset(&reg->sep);

				arena->frag = reg;
			} else {
				/* reg comes after frag. */
				region_next_size_set(&arena->frag->sep, size);

				assert(region_next_free_get(&arena->frag->sep)
				    == false);

				assert(region_prev_free_get(&next->sep));
				region_prev_free_unset(&next->sep);
			}
		}
	}
}

static __inline void
arena_bin_append(arena_t *arena, unsigned bin, region_t *reg)
{
	arena_bin_t *tbin;

	assert(bin < NBINS);
	assert((region_next_size_get(&reg->sep) >> opt_quantum_2pow)
	    >= bin_shift);
	assert(region_next_size_get(&reg->sep)
	    == ((bin + bin_shift) << opt_quantum_2pow));

	tbin = &arena->bins[bin];

	if (qr_next(&tbin->regions, next.u.s.link) == &tbin->regions)
		arena_mask_set(arena, bin);

	qr_new(reg, next.u.s.link);
	qr_before_insert(&tbin->regions, reg, next.u.s.link);
#ifdef MALLOC_STATS
	arena->stats.bins[bin].nregions++;

	if (arena->stats.bins[bin].nregions
	    > arena->stats.bins[bin].highcached) {
		arena->stats.bins[bin].highcached
		    = arena->stats.bins[bin].nregions;
	}
#endif
}

static __inline void
arena_bin_push(arena_t *arena, unsigned bin, region_t *reg)
{
	arena_bin_t *tbin;

	assert(bin < NBINS);
	assert((region_next_size_get(&reg->sep) >> opt_quantum_2pow)
	    >= bin_shift);
	assert(region_next_size_get(&reg->sep)
	    == ((bin + bin_shift) << opt_quantum_2pow));

	tbin = &arena->bins[bin];

	if (qr_next(&tbin->regions, next.u.s.link) == &tbin->regions)
		arena_mask_set(arena, bin);

	region_next_contig_unset(&reg->sep);
	qr_new(reg, next.u.s.link);
	qr_after_insert(&tbin->regions, reg, next.u.s.link);
#ifdef MALLOC_STATS
	arena->stats.bins[bin].nregions++;

	if (arena->stats.bins[bin].nregions
	    > arena->stats.bins[bin].highcached) {
		arena->stats.bins[bin].highcached
		    = arena->stats.bins[bin].nregions;
	}
#endif
}

static __inline region_t *
arena_bin_pop(arena_t *arena, unsigned bin)
{
	region_t *ret;
	arena_bin_t *tbin;

	assert(bin < NBINS);

	tbin = &arena->bins[bin];

	assert(qr_next(&tbin->regions, next.u.s.link) != &tbin->regions);

	ret = qr_next(&tbin->regions, next.u.s.link);
	assert(region_next_size_get(&ret->sep)
	    == ((bin + bin_shift) << opt_quantum_2pow));
	qr_remove(ret, next.u.s.link);
#ifdef MALLOC_STATS
	arena->stats.bins[bin].nregions--;
#endif
	if (qr_next(&tbin->regions, next.u.s.link) == &tbin->regions)
		arena_mask_unset(arena, bin);

	arena_delayed_extract(arena, ret);

	if (region_next_free_get(&ret->sep)) {
		region_t *next;

		/* Non-delayed region. */
		region_next_free_unset(&ret->sep);

		next = (region_t *)&((char *)ret)
		    [(bin + bin_shift) << opt_quantum_2pow];
		assert(next->prev.size == region_next_size_get(&ret->sep));
		assert(region_prev_free_get(&next->sep));
		region_prev_free_unset(&next->sep);
	}

	return (ret);
}

static void
arena_large_insert(arena_t *arena, region_t *reg, bool lru)
{

	assert(region_next_free_get(&reg->sep));
#ifdef MALLOC_DEBUG
	{
		region_t *next;

		next = (region_t *)&((char *)reg)
		    [region_next_size_get(&reg->sep)];
		assert(region_prev_free_get(&next->sep));
		assert(next->prev.size == region_next_size_get(&reg->sep));
	}
#endif

	/* Coalescing should have already been done. */
	assert(arena_coalesce(arena, &reg, region_next_size_get(&reg->sep))
	    == false);

	if (region_next_size_get(&reg->sep) < chunk_size
	    - (CHUNK_REG_OFFSET + offsetof(region_t, next))) {
		/*
		 * Make sure not to cache a large region with the nextContig
		 * flag set, in order to simplify the logic that determines
		 * whether a region needs to be extracted from "delayed".
		 */
		region_next_contig_unset(&reg->sep);

		/* Store the region in the large_regions tree. */
		reg->next.u.l.node.reg = reg;
		reg->next.u.l.lru = lru;

		RB_INSERT(region_tree_s, &arena->large_regions,
		    &reg->next.u.l.node);
#ifdef MALLOC_STATS
		arena->stats.large.curcached++;
		if (arena->stats.large.curcached
		    > arena->stats.large.highcached) {
			arena->stats.large.highcached
			    = arena->stats.large.curcached;
		}
#endif
	} else {
		chunk_node_t *node;

		/*
		 * This region now spans an entire chunk. Deallocate the chunk.
		 *
		 * Note that it is possible for allocation of a large region
		 * from a pristine chunk, followed by deallocation of the
		 * region, can cause the chunk to immediately be unmapped.
		 * This isn't ideal, but 1) such scenarios seem unlikely, and
		 * 2) delaying coalescence for large regions could cause
		 * excessive fragmentation for common scenarios.
		 */

		node = (chunk_node_t *)CHUNK_ADDR2BASE(reg);
		RB_REMOVE(chunk_tree_s, &arena->chunks, node);
		arena->nchunks--;
		assert(node->chunk == (chunk_node_t *)node);
		chunk_dealloc(node->chunk, chunk_size);
	}
}

static void
arena_large_cache(arena_t *arena, region_t *reg, bool lru)
{
	size_t size;

	/* Try to coalesce before storing this region anywhere. */
	size = region_next_size_get(&reg->sep);
	if (arena_coalesce(arena, &reg, size)) {
		if (reg == NULL) {
			/* Region no longer needs cached. */
			return;
		}
		size = region_next_size_get(&reg->sep);
	}

	arena_large_insert(arena, reg, lru);
}

static void
arena_lru_cache(arena_t *arena, region_t *reg)
{
	size_t size;

	assert(region_next_free_get(&reg->sep));
#ifdef MALLOC_DEBUG
	{
		region_t *next;

		next = (region_t *)&((char *)reg)
		    [region_next_size_get(&reg->sep)];
		assert(region_prev_free_get(&next->sep));
		assert(next->prev.size == region_next_size_get(&reg->sep));
	}
#endif
	assert(region_next_size_get(&reg->sep) % quantum == 0);
	assert(region_next_size_get(&reg->sep)
	    >= QUANTUM_CEILING(sizeof(region_small_sizer_t)));

	size = region_next_size_get(&reg->sep);
	if (size <= bin_maxsize) {
		arena_bin_append(arena, (size >> opt_quantum_2pow) - bin_shift,
		    reg);
	} else
		arena_large_cache(arena, reg, true);
}

static __inline void
arena_mru_cache(arena_t *arena, region_t *reg, size_t size)
{

	assert(region_next_free_get(&reg->sep));
#ifdef MALLOC_DEBUG
	{
		region_t *next;

		next = (region_t *)&((char *)reg)
		    [region_next_size_get(&reg->sep)];
		assert(region_prev_free_get(&next->sep));
		assert(next->prev.size == region_next_size_get(&reg->sep));
	}
#endif
	assert(region_next_size_get(&reg->sep) % quantum == 0);
	assert(region_next_size_get(&reg->sep)
	    >= QUANTUM_CEILING(sizeof(region_small_sizer_t)));
	assert(size == region_next_size_get(&reg->sep));

	if (size <= bin_maxsize) {
		arena_bin_push(arena, (size >> opt_quantum_2pow) - bin_shift,
		    reg);
	} else
		arena_large_cache(arena, reg, false);
}

static __inline void
arena_undelay(arena_t *arena, uint32_t slot)
{
	region_t *reg, *next;
	size_t size;

	assert(slot == arena->next_delayed);
	assert(arena->delayed[slot] != NULL);

	/* Try to coalesce reg. */
	reg = arena->delayed[slot];

	size = region_next_size_get(&reg->sep);

	assert(region_next_contig_get(&reg->sep));
	assert(reg->next.u.s.slot == slot);

	arena_bin_extract(arena, (size >> opt_quantum_2pow) - bin_shift, reg);

	arena->delayed[slot] = NULL;

	next = (region_t *) &((char *) reg)[size];

	region_next_free_set(&reg->sep);
	region_prev_free_set(&next->sep);
	next->prev.size = size;

	if (arena_coalesce(arena, &reg, size) == false) {
		/* Coalescing failed.  Cache this region. */
		arena_mru_cache(arena, reg, size);
	} else {
		/* Coalescing succeeded. */

		if (reg == NULL) {
			/* Region no longer needs undelayed. */
			return;
		}

		if (region_next_size_get(&reg->sep) < chunk_size
		    - (CHUNK_REG_OFFSET + offsetof(region_t, next))) {
			/*
			 * Insert coalesced region into appropriate bin (or
			 * largeRegions).
			 */
			arena_lru_cache(arena, reg);
		} else {
			chunk_node_t *node;

			/*
			 * This region now spans an entire chunk.  Deallocate
			 * the chunk.
			 */

			node = (chunk_node_t *) CHUNK_ADDR2BASE(reg);
			RB_REMOVE(chunk_tree_s, &arena->chunks, node);
			arena->nchunks--;
			assert(node->chunk == (chunk_node_t *) node);
			chunk_dealloc(node->chunk, chunk_size);
		}
	}
}

static void
arena_delay_cache(arena_t *arena, region_t *reg)
{
	region_t *next;
	size_t size;

	assert(region_next_free_get(&reg->sep) == false);
	assert(region_next_size_get(&reg->sep) % quantum == 0);
	assert(region_next_size_get(&reg->sep)
	    >= QUANTUM_CEILING(sizeof(region_small_sizer_t)));

	size = region_next_size_get(&reg->sep);
	next = (region_t *)&((char *)reg)[size];
	assert(region_prev_free_get(&next->sep) == false);

	if (size <= bin_maxsize) {
		if (region_next_contig_get(&reg->sep)) {
			uint32_t slot;

			/* Insert into delayed. */

			/* Clear a slot, then put reg in it. */
			slot = arena->next_delayed;
			if (arena->delayed[slot] != NULL)
				arena_undelay(arena, slot);
			assert(slot == arena->next_delayed);
			assert(arena->delayed[slot] == NULL);

			reg->next.u.s.slot = slot;

			arena->delayed[slot] = reg;

			/* Update next_delayed. */
			slot++;
			slot &= (opt_ndelay - 1); /* Handle wrap-around. */
			arena->next_delayed = slot;

			arena_bin_append(arena, (size >> opt_quantum_2pow)
			    - bin_shift, reg);
		} else {
			/*
			 * This region was a fragment when it was allocated, so
			 * don't delay coalescence for it.
			 */

			region_next_free_set(&reg->sep);
			region_prev_free_set(&next->sep);
			next->prev.size = size;

			if (arena_coalesce(arena, &reg, size)) {
				/* Coalescing succeeded. */

				if (reg == NULL) {
					/* Region no longer needs cached. */
					return;
				}

				size = region_next_size_get(&reg->sep);
			}

			arena_mru_cache(arena, reg, size);
		}
	} else {
		region_next_free_set(&reg->sep);
		region_prev_free_set(&next->sep);
		region_next_contig_unset(&reg->sep);
		next->prev.size = size;

		arena_large_cache(arena, reg, true);
	}
}

static __inline region_t *
arena_frag_reg_alloc(arena_t *arena, size_t size, bool fit)
{
	region_t *ret;

	/*
	 * Try to fill frag if it's empty.  Frag needs to be marked as
	 * allocated.
	 */
	if (arena->frag == NULL) {
		region_node_t *node;

		node = RB_MIN(region_tree_s, &arena->large_regions);
		if (node != NULL) {
			region_t *frag, *next;

			RB_REMOVE(region_tree_s, &arena->large_regions, node);

			frag = node->reg;
#ifdef MALLOC_STATS
			arena->stats.frag.ncached++;
#endif
			assert(region_next_free_get(&frag->sep));
			region_next_free_unset(&frag->sep);

			next = (region_t *)&((char *)frag)[region_next_size_get(
			    &frag->sep)];
			assert(region_prev_free_get(&next->sep));
			region_prev_free_unset(&next->sep);

			arena->frag = frag;
		}
	}

	if (arena->frag != NULL) {
#ifdef MALLOC_STATS
		arena->stats.frag.nrequests++;
#endif

		if (region_next_size_get(&arena->frag->sep) >= size) {
			if (fit) {
				size_t total_size;

				/*
				 * Use frag, but try to use the beginning for
				 * smaller regions, and the end for larger
				 * regions.  This reduces fragmentation in some
				 * pathological use cases.  It tends to group
				 * short-lived (smaller) regions, which
				 * increases the effectiveness of coalescing.
				 */

				total_size =
				    region_next_size_get(&arena->frag->sep);
				assert(size % quantum == 0);

				if (total_size - size >= QUANTUM_CEILING(
				    sizeof(region_small_sizer_t))) {
					if (size <= bin_maxsize) {
						region_t *next;

						/*
						 * Carve space from the
						 * beginning of frag.
						 */

						/* ret. */
						ret = arena->frag;
						region_next_size_set(&ret->sep,
						    size);
						assert(region_next_free_get(
						    &ret->sep) == false);

						/* next. */
						next = (region_t *)&((char *)
						    ret)[size];
						region_next_size_set(&next->sep,
						    total_size - size);
						assert(size >=
						    QUANTUM_CEILING(sizeof(
						    region_small_sizer_t)));
						region_prev_free_unset(
						    &next->sep);
						region_next_free_unset(
						    &next->sep);

						/* Update frag. */
						arena->frag = next;
					} else {
						region_t *prev;
						size_t prev_size;

						/*
						 * Carve space from the end of
						 * frag.
						 */

						/* prev. */
						prev_size = total_size - size;
						prev = arena->frag;
						region_next_size_set(&prev->sep,
						    prev_size);
						assert(prev_size >=
						    QUANTUM_CEILING(sizeof(
						    region_small_sizer_t)));
						assert(region_next_free_get(
						    &prev->sep) == false);

						/* ret. */
						ret = (region_t *)&((char *)
						    prev)[prev_size];
						region_next_size_set(&ret->sep,
						    size);
						region_prev_free_unset(
						    &ret->sep);
						region_next_free_unset(
						    &ret->sep);

#ifdef MALLOC_DEBUG
						{
					region_t *next;

					/* next. */
					next = (region_t *)&((char *) ret)
					    [region_next_size_get(&ret->sep)];
					assert(region_prev_free_get(&next->sep)
					    == false);
						}
#endif
					}
#ifdef MALLOC_STATS
					arena->stats.nsplit++;
#endif
				} else {
					/*
					 * frag is close enough to the right
					 * size that there isn't enough room to
					 * create a neighboring region.
					 */

					/* ret. */
					ret = arena->frag;
					arena->frag = NULL;
					assert(region_next_free_get(&ret->sep)
					    == false);

#ifdef MALLOC_DEBUG
					{
						region_t *next;

						/* next. */
						next = (region_t *)&((char *)
						    ret)[region_next_size_get(
						    &ret->sep)];
						assert(region_prev_free_get(
						    &next->sep) == false);
					}
#endif
				}
#ifdef MALLOC_STATS
				arena->stats.frag.nserviced++;
#endif
			} else {
				/* Don't fit to the allocation size. */

				/* ret. */
				ret = arena->frag;
				arena->frag = NULL;
				assert(region_next_free_get(&ret->sep)
				    == false);

#ifdef MALLOC_DEBUG
				{
					region_t *next;

					/* next. */
					next = (region_t *) &((char *) ret)
					   [region_next_size_get(&ret->sep)];
					assert(region_prev_free_get(&next->sep)
					    == false);
				}
#endif
			}
			region_next_contig_set(&ret->sep);
			goto RETURN;
		} else if (size <= bin_maxsize) {
			region_t *reg;

			/*
			 * The frag region is too small to service a small
			 * request.  Clear frag.
			 */

			reg = arena->frag;
			region_next_contig_set(&reg->sep);

			arena->frag = NULL;

			arena_delay_cache(arena, reg);
		}
	}

	ret = NULL;
RETURN:
	return (ret);
}

static region_t *
arena_split_reg_alloc(arena_t *arena, size_t size, bool fit)
{
	region_t *ret;

	if (arena->split != NULL) {
#ifdef MALLOC_STATS
		arena->stats.split.nrequests++;
#endif

		if (region_next_size_get(&arena->split->sep) >= size) {
			if (fit) {
				size_t total_size;

				/*
				 * Use split, but try to use the beginning for
				 * smaller regions, and the end for larger
				 * regions.  This reduces fragmentation in some
				 * pathological use cases.  It tends to group
				 * short-lived (smaller) regions, which
				 * increases the effectiveness of coalescing.
				 */

				total_size =
				    region_next_size_get(&arena->split->sep);
				assert(size % quantum == 0);

				if (total_size - size >= QUANTUM_CEILING(
				    sizeof(region_small_sizer_t))) {
					if (size <= bin_maxsize) {
						region_t *next;

						/*
						 * Carve space from the
						 * beginning of split.
						 */

						/* ret. */
						ret = arena->split;
						region_next_size_set(&ret->sep,
						    size);
						assert(region_next_free_get(
						    &ret->sep) == false);

						/* next. */
						next = (region_t *)&((char *)
						    ret)[size];
						region_next_size_set(&next->sep,
						    total_size - size);
						assert(size >=
						    QUANTUM_CEILING(sizeof(
						    region_small_sizer_t)));
						region_prev_free_unset(
						    &next->sep);
						region_next_free_unset(
						    &next->sep);

						/* Update split. */
						arena->split = next;
					} else {
						region_t *prev;
						size_t prev_size;

						/*
						 * Carve space from the end of
						 * split.
						 */

						/* prev. */
						prev_size = total_size - size;
						prev = arena->split;
						region_next_size_set(&prev->sep,
						    prev_size);
						assert(prev_size >=
						    QUANTUM_CEILING(sizeof(
						    region_small_sizer_t)));
						assert(region_next_free_get(
						    &prev->sep) == false);

						/* ret. */
						ret = (region_t *)&((char *)
						    prev)[prev_size];
						region_next_size_set(&ret->sep,
						    size);
						region_prev_free_unset(
						    &ret->sep);
						region_next_free_unset(
						    &ret->sep);

#ifdef MALLOC_DEBUG
						{
					region_t *next;

					/* next. */
					next = (region_t *)&((char *) ret)
					    [region_next_size_get(&ret->sep)];
					assert(region_prev_free_get(&next->sep)
					    == false);
						}
#endif
					}
#ifdef MALLOC_STATS
					arena->stats.nsplit++;
#endif
				} else {
					/*
					 * split is close enough to the right
					 * size that there isn't enough room to
					 * create a neighboring region.
					 */

					/* ret. */
					ret = arena->split;
					arena->split = NULL;
					assert(region_next_free_get(&ret->sep)
					    == false);

#ifdef MALLOC_DEBUG
					{
						region_t *next;

						/* next. */
						next = (region_t *)&((char *)
						    ret)[region_next_size_get(
						    &ret->sep)];
						assert(region_prev_free_get(
						    &next->sep) == false);
					}
#endif
				}

#ifdef MALLOC_STATS
				arena->stats.split.nserviced++;
#endif
			} else {
				/* Don't fit to the allocation size. */

				/* ret. */
				ret = arena->split;
				arena->split = NULL;
				assert(region_next_free_get(&ret->sep)
				    == false);

#ifdef MALLOC_DEBUG
				{
					region_t *next;

					/* next. */
					next = (region_t *) &((char *) ret)
					   [region_next_size_get(&ret->sep)];
					assert(region_prev_free_get(&next->sep)
					    == false);
				}
#endif
			}
			region_next_contig_set(&ret->sep);
			goto RETURN;
		} else if (size <= bin_maxsize) {
			region_t *reg;

			/*
			 * The split region is too small to service a small
			 * request.  Clear split.
			 */

			reg = arena->split;
			region_next_contig_set(&reg->sep);

			arena->split = NULL;

			arena_delay_cache(arena, reg);
		}
	}

	ret = NULL;
RETURN:
	return (ret);
}

/*
 * Split reg if necessary.  The region must be overly large enough to be able
 * to contain a trailing region.
 */
static void
arena_reg_fit(arena_t *arena, size_t size, region_t *reg, bool restore_split)
{
	assert(QUANTUM_CEILING(size) == size);
	assert(region_next_free_get(&reg->sep) == 0);

	if (region_next_size_get(&reg->sep)
	    >= size + QUANTUM_CEILING(sizeof(region_small_sizer_t))) {
		size_t total_size;
		region_t *next;

		total_size = region_next_size_get(&reg->sep);

		region_next_size_set(&reg->sep, size);

		next = (region_t *) &((char *) reg)[size];
		region_next_size_set(&next->sep, total_size - size);
		assert(region_next_size_get(&next->sep)
		    >= QUANTUM_CEILING(sizeof(region_small_sizer_t)));
		region_prev_free_unset(&next->sep);

		if (restore_split) {
			/* Restore what's left to "split". */
			region_next_free_unset(&next->sep);
			arena->split = next;
		} else if (arena->frag == NULL && total_size - size
		    > bin_maxsize) {
			/* This region is large enough to use for "frag". */
			region_next_free_unset(&next->sep);
			arena->frag = next;
		} else {
			region_t *nextnext;
			size_t next_size;

			region_next_free_set(&next->sep);

			assert(region_next_size_get(&next->sep) == total_size
			    - size);
			next_size = total_size - size;
			nextnext = (region_t *) &((char *) next)[next_size];
			nextnext->prev.size = next_size;
			assert(region_prev_free_get(&nextnext->sep) == false);
			region_prev_free_set(&nextnext->sep);

			arena_mru_cache(arena, next, next_size);
		}

#ifdef MALLOC_STATS
		arena->stats.nsplit++;
#endif
	}
}

static __inline region_t *
arena_bin_reg_alloc(arena_t *arena, size_t size, bool fit)
{
	region_t *ret, *header;
	unsigned bin;

	/*
	 * Look for an exact fit in bins (region cached in smallest possible
	 * bin).
	 */
	bin = (size >> opt_quantum_2pow) - bin_shift;
#ifdef MALLOC_STATS
	arena->stats.bins[bin].nrequests++;
#endif
	header = &arena->bins[bin].regions;
	if (qr_next(header, next.u.s.link) != header) {
		/* Exact fit. */
		ret = arena_bin_pop(arena, bin);
		assert(region_next_size_get(&ret->sep) >= size);
#ifdef MALLOC_STATS
		arena->stats.bins[bin].nfit++;
#endif
		goto RETURN;
	}

	/* Look at frag to see whether it's large enough. */
	ret = arena_frag_reg_alloc(arena, size, fit);
	if (ret != NULL)
		goto RETURN;

	/* Look in all bins for a large enough region. */
	if ((bin = arena_bins_search(arena, size)) == (size >> opt_quantum_2pow)
	    - bin_shift) {
		/* Over-fit. */
		ret = arena_bin_pop(arena, bin);
		assert(region_next_size_get(&ret->sep) >= size);

		if (fit)
			arena_reg_fit(arena, size, ret, false);

#ifdef MALLOC_STATS
		arena->stats.bins[bin].noverfit++;
#endif
		goto RETURN;
	}

	ret = NULL;
RETURN:
	return (ret);
}

/* Look in large_regions for a large enough region. */
static region_t *
arena_large_reg_alloc(arena_t *arena, size_t size, bool fit)
{
	region_t *ret, *next;
	region_node_t *node;
	region_t key;

#ifdef MALLOC_STATS
	arena->stats.large.nrequests++;
#endif

	key.next.u.l.node.reg = &key;
	key.next.u.l.lru = true;
	region_next_size_set(&key.sep, size);
	node = RB_NFIND(region_tree_s, &arena->large_regions,
	    &key.next.u.l.node);
	if (node == NULL) {
		ret = NULL;
		goto RETURN;
	}

	/* Cached large region found. */
	ret = node->reg;
	assert(region_next_free_get(&ret->sep));

	RB_REMOVE(region_tree_s, &arena->large_regions, node);
#ifdef MALLOC_STATS
	arena->stats.large.curcached--;
#endif

	region_next_free_unset(&ret->sep);

	next = (region_t *)&((char *)ret)[region_next_size_get(&ret->sep)];
	assert(region_prev_free_get(&next->sep));
	region_prev_free_unset(&next->sep);

	if (fit)
		arena_reg_fit(arena, size, ret, false);

#ifdef MALLOC_STATS
	if (size > bin_maxsize)
		arena->stats.large.nfit++;
	else
		arena->stats.large.noverfit++;
#endif

RETURN:
	return (ret);
}

/* Allocate a new chunk and create a single region from it. */
static region_t *
arena_chunk_reg_alloc(arena_t *arena, size_t size, bool fit)
{
	region_t *ret, *next;
	chunk_node_t *chunk;

	chunk = chunk_alloc(chunk_size);
	if (chunk == NULL) {
		ret = NULL;
		goto RETURN;
	}

#ifdef MALLOC_DEBUG
	{
		chunk_node_t *tchunk;
		chunk_node_t key;

		key.chunk = chunk;
		tchunk = RB_FIND(chunk_tree_s, &arena->chunks, &key);
		assert(tchunk == NULL);
	}
#endif
	chunk->arena = arena;
	chunk->chunk = chunk;
	chunk->size = chunk_size;
	chunk->extra = 0;

	RB_INSERT(chunk_tree_s, &arena->chunks, chunk);
	arena->nchunks++;

	/* Carve a region from the new chunk. */
	ret = (region_t *) &((char *) chunk)[CHUNK_REG_OFFSET];
	region_next_size_set(&ret->sep, chunk_size - (CHUNK_REG_OFFSET
	    + offsetof(region_t, next)));
	region_prev_free_unset(&ret->sep);
	region_next_free_unset(&ret->sep);

	/* Create a separator at the end of this new region. */
	next = (region_t *)&((char *)ret)[region_next_size_get(&ret->sep)];
	region_next_size_set(&next->sep, 0);
	region_prev_free_unset(&next->sep);
	region_next_free_unset(&next->sep);
	region_next_contig_unset(&next->sep);

	if (fit)
		arena_reg_fit(arena, size, ret, (arena->split == NULL));

RETURN:
	return (ret);
}

/*
 * Find a region that is at least as large as aSize, and return a pointer to
 * the separator that precedes the region.  The return value is ready for use,
 * though it may be larger than is necessary if fit is false.
 */
static __inline region_t *
arena_reg_alloc(arena_t *arena, size_t size, bool fit)
{
	region_t *ret;

	assert(QUANTUM_CEILING(size) == size);
	assert(size >= QUANTUM_CEILING(sizeof(region_small_sizer_t)));
	assert(size <= (chunk_size >> 1));

	if (size <= bin_maxsize) {
		ret = arena_bin_reg_alloc(arena, size, fit);
		if (ret != NULL)
			goto RETURN;
	}

	ret = arena_large_reg_alloc(arena, size, fit);
	if (ret != NULL)
		goto RETURN;

	ret = arena_split_reg_alloc(arena, size, fit);
	if (ret != NULL)
		goto RETURN;

	/*
	 * Only try allocating from frag here if size is large, since
	 * arena_bin_reg_alloc() already falls back to allocating from frag for
	 * small regions.
	 */
	if (size > bin_maxsize) {
		ret = arena_frag_reg_alloc(arena, size, fit);
		if (ret != NULL)
			goto RETURN;
	}

	ret = arena_chunk_reg_alloc(arena, size, fit);
	if (ret != NULL)
		goto RETURN;

	ret = NULL;
RETURN:
	return (ret);
}

static void *
arena_malloc(arena_t *arena, size_t size)
{
	void *ret;
	region_t *reg;
	size_t quantum_size;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(size != 0);
	assert(region_ceiling(size) <= (chunk_size >> 1));

	quantum_size = region_ceiling(size);
	if (quantum_size < size) {
		/* size is large enough to cause size_t wrap-around. */
		ret = NULL;
		goto RETURN;
	}
	assert(quantum_size >= QUANTUM_CEILING(sizeof(region_small_sizer_t)));

	malloc_mutex_lock(&arena->mtx);
	reg = arena_reg_alloc(arena, quantum_size, true);
	if (reg == NULL) {
		malloc_mutex_unlock(&arena->mtx);
		ret = NULL;
		goto RETURN;
	}

#ifdef MALLOC_STATS
	arena->allocated += quantum_size;
#endif

	malloc_mutex_unlock(&arena->mtx);

	ret = (void *)&reg->next;
#ifdef MALLOC_REDZONES
	{
		region_t *next;
		size_t total_size;

		memset(reg->sep.next_red, 0xa5, MALLOC_RED);

		/*
		 * Unused trailing space in the region is considered part of the
		 * trailing redzone.
		 */
		total_size = region_next_size_get(&reg->sep);
		assert(total_size >= size);
		memset(&((char *)ret)[size], 0xa5,
		    total_size - size - sizeof(region_sep_t));

		reg->sep.next_exact_size = size;

		next = (region_t *)&((char *)reg)[total_size];
		memset(next->sep.prev_red, 0xa5, MALLOC_RED);
	}
#endif
RETURN:
	return (ret);
}

static void *
arena_palloc(arena_t *arena, size_t alignment, size_t size)
{
	void *ret;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);

	if (alignment <= quantum) {
		/*
		 * The requested alignment is always guaranteed, so use the
		 * normal allocation function.
		 */
		ret = arena_malloc(arena, size);
	} else {
		region_t *reg, *old_split;
		size_t quantum_size, alloc_size, offset, total_size;

		/*
		 * Allocate more space than necessary, then carve an aligned
		 * region out of it.  The smallest allowable region is
		 * potentially a multiple of the quantum size, so care must be
		 * taken to carve things up such that all resulting regions are
		 * large enough.
		 */

		quantum_size = region_ceiling(size);
		if (quantum_size < size) {
			/* size is large enough to cause size_t wrap-around. */
			ret = NULL;
			goto RETURN;
		}

		/*
		 * Calculate how large of a region to allocate.  There must be
		 * enough space to advance far enough to have at least
		 * sizeof(region_small_sizer_t) leading bytes, yet also land at
		 * an alignment boundary.
		 */
		if (alignment >= sizeof(region_small_sizer_t)) {
			alloc_size =
			    QUANTUM_CEILING(sizeof(region_small_sizer_t))
			    + alignment + quantum_size;
		} else {
			alloc_size =
			    (QUANTUM_CEILING(sizeof(region_small_sizer_t)) << 1)
			    + quantum_size;
		}

		if (alloc_size < quantum_size) {
			/* size_t wrap-around occurred. */
			ret = NULL;
			goto RETURN;
		}

		malloc_mutex_lock(&arena->mtx);
		old_split = arena->split;
		reg = arena_reg_alloc(arena, alloc_size, false);
		if (reg == NULL) {
			malloc_mutex_unlock(&arena->mtx);
			ret = NULL;
			goto RETURN;
		}
		if (reg == old_split) {
			/*
			 * We requested a non-fit allocation that was serviced
			 * by split, which means that we need to take care to
			 * restore split in the arena_reg_fit() call later on.
			 *
			 * Do nothing; a non-NULL old_split will be used as the
			 * signal to restore split.
			 */
		} else
			old_split = NULL;

		total_size = region_next_size_get(&reg->sep);

		if (alignment > bin_maxsize || size > bin_maxsize) {
			size_t split_size, p;

			/*
			 * Put this allocation toward the end of reg, since
			 * it is large, and we try to put all large regions at
			 * the end of split regions.
			 */
			split_size = region_next_size_get(&reg->sep);
			p = (size_t)&((char *)&reg->next)[split_size];
			p -= offsetof(region_t, next);
			p -= size;
			p &= ~(alignment - 1);
			p -= offsetof(region_t, next);

			offset = p - (size_t)reg;
		} else {
			if ((((size_t)&reg->next) & (alignment - 1)) != 0) {
				size_t p;

				/*
				 * reg is unaligned.  Calculate the offset into
				 * reg to actually base the allocation at.
				 */
				p = ((size_t)&reg->next + alignment)
				    & ~(alignment - 1);
				while (p - (size_t)&reg->next
				    < QUANTUM_CEILING(sizeof(
				    region_small_sizer_t)))
					p += alignment;
				p -= offsetof(region_t, next);

				offset = p - (size_t)reg;
			} else
				offset = 0;
		}
		assert(offset % quantum == 0);
		assert(offset < total_size);

		if (offset != 0) {
			region_t *prev;

			/*
			 * Move ret to an alignment boundary that is far enough
			 * past the beginning of the allocation to leave a
			 * leading free region, then trim the leading space.
			 */

			assert(offset >= QUANTUM_CEILING(
			    sizeof(region_small_sizer_t)));
			assert(offset + size <= total_size);

			prev = reg;
			reg = (region_t *)&((char *)prev)[offset];
			assert(((size_t)&reg->next & (alignment - 1)) == 0);

			/* prev. */
			region_next_size_set(&prev->sep, offset);
			reg->prev.size = offset;

			/* reg. */
			region_next_size_set(&reg->sep, total_size - offset);
			region_next_free_unset(&reg->sep);
			if (region_next_contig_get(&prev->sep))
				region_next_contig_set(&reg->sep);
			else
				region_next_contig_unset(&reg->sep);

			if (old_split != NULL && (alignment > bin_maxsize
			    || size > bin_maxsize)) {
				/* Restore to split. */
				region_prev_free_unset(&reg->sep);

				arena->split = prev;
				old_split = NULL;
			} else {
				region_next_free_set(&prev->sep);
				region_prev_free_set(&reg->sep);

				arena_mru_cache(arena, prev, offset);
			}
#ifdef MALLOC_STATS
			arena->stats.nsplit++;
#endif
		}

		arena_reg_fit(arena, quantum_size, reg, (old_split != NULL));

#ifdef MALLOC_STATS
		arena->allocated += quantum_size;
#endif

		malloc_mutex_unlock(&arena->mtx);

		ret = (void *)&reg->next;
#ifdef MALLOC_REDZONES
		{
			region_t *next;
			size_t total_size;

			memset(reg->sep.next_red, 0xa5, MALLOC_RED);

			/*
			 * Unused trailing space in the region is considered
			 * part of the trailing redzone.
			 */
			total_size = region_next_size_get(&reg->sep);
			assert(total_size >= size);
			memset(&((char *)ret)[size], 0xa5,
			    total_size - size - sizeof(region_sep_t));

			reg->sep.next_exact_size = size;

			next = (region_t *)&((char *)reg)[total_size];
			memset(next->sep.prev_red, 0xa5, MALLOC_RED);
		}
#endif
	}

RETURN:
	assert(((size_t)ret & (alignment - 1)) == 0);
	return (ret);
}

static void *
arena_calloc(arena_t *arena, size_t num, size_t size)
{
	void *ret;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(num * size != 0);

	ret = arena_malloc(arena, num * size);
	if (ret == NULL)
		goto RETURN;

	memset(ret, 0, num * size);

RETURN:
	return (ret);
}

static size_t
arena_salloc(arena_t *arena, void *ptr)
{
	size_t ret;
	region_t *reg;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(ptr != NULL);
	assert(ptr != &nil);
	assert(CHUNK_ADDR2BASE(ptr) != ptr);

	reg = (region_t *)&((char *)ptr)[-offsetof(region_t, next)];

	ret = region_next_size_get(&reg->sep);

	return (ret);
}

#ifdef MALLOC_REDZONES
static void
redzone_check(void *ptr)
{
	region_t *reg, *next;
	size_t size;
	unsigned i, ncorruptions;

	ncorruptions = 0;

	reg = (region_t *)&((char *)ptr)[-offsetof(region_t, next)];
	size = region_next_size_get(&reg->sep);
	next = (region_t *)&((char *)reg)[size];

	/* Leading redzone. */
	for (i = 0; i < MALLOC_RED; i++) {
		if ((unsigned char)reg->sep.next_red[i] != 0xa5) {
			size_t offset = (size_t)MALLOC_RED - i;

			ncorruptions++;
			malloc_printf("%s: (malloc) Corrupted redzone %zu "
			    "byte%s before %p (0x%x)\n", _getprogname(), 
			    offset, offset > 1 ? "s" : "", ptr,
			    (unsigned char)reg->sep.next_red[i]);
		}
	}
	memset(&reg->sep.next_red, 0x5a, MALLOC_RED);

	/* Bytes immediately trailing allocation. */
	for (i = 0; i < size - reg->sep.next_exact_size - sizeof(region_sep_t);
	    i++) {
		if ((unsigned char)((char *)ptr)[reg->sep.next_exact_size + i]
		    != 0xa5) {
			size_t offset = (size_t)(i + 1);

			ncorruptions++;
			malloc_printf("%s: (malloc) Corrupted redzone %zu "
			    "byte%s after %p (size %zu) (0x%x)\n",
			    _getprogname(), offset, offset > 1 ? "s" : "", ptr,
			    reg->sep.next_exact_size, (unsigned char)((char *)
			    ptr)[reg->sep.next_exact_size + i]);
		}
	}
	memset(&((char *)ptr)[reg->sep.next_exact_size], 0x5a,
	    size - reg->sep.next_exact_size - sizeof(region_sep_t));

	/* Trailing redzone. */
	for (i = 0; i < MALLOC_RED; i++) {
		if ((unsigned char)next->sep.prev_red[i] != 0xa5) {
			size_t offset = (size_t)(size - reg->sep.next_exact_size
			    - sizeof(region_sep_t) + i + 1);

			ncorruptions++;
			malloc_printf("%s: (malloc) Corrupted redzone %zu "
			    "byte%s after %p (size %zu) (0x%x)\n",
			    _getprogname(), offset, offset > 1 ? "s" : "", ptr,
			    reg->sep.next_exact_size,
			    (unsigned char)next->sep.prev_red[i]);
		}
	}
	memset(&next->sep.prev_red, 0x5a, MALLOC_RED);

	if (opt_abort && ncorruptions != 0)
		abort();

	reg->sep.next_exact_size = 0;
}
#endif

static void
arena_dalloc(arena_t *arena, void *ptr)
{
	region_t *reg;

	assert(arena != NULL);
	assert(ptr != NULL);
	assert(ptr != &nil);

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(CHUNK_ADDR2BASE(ptr) != ptr);

	reg = (region_t *)&((char *)ptr)[-offsetof(region_t, next)];

	malloc_mutex_lock(&arena->mtx);

#ifdef MALLOC_DEBUG
	{
		chunk_node_t *chunk, *node;
		chunk_node_t key;

		chunk = CHUNK_ADDR2BASE(ptr);
		assert(chunk->arena == arena);

		key.chunk = chunk;
		node = RB_FIND(chunk_tree_s, &arena->chunks, &key);
		assert(node == chunk);
	}
#endif
#ifdef MALLOC_REDZONES
	redzone_check(ptr);
#endif

#ifdef MALLOC_STATS
	arena->allocated -= region_next_size_get(&reg->sep);
#endif

	if (opt_junk) {
		memset(&reg->next, 0x5a,
		    region_next_size_get(&reg->sep) - sizeof(region_sep_t));
	}

	arena_delay_cache(arena, reg);

	malloc_mutex_unlock(&arena->mtx);
}

#ifdef NOT_YET
static void *
arena_ralloc(arena_t *arena, void *ptr, size_t size)
{

	/*
	 * Arenas don't need to support ralloc, since all reallocation is done
	 * by allocating new space and copying.  This function should never be
	 * called.
	 */
	/* NOTREACHED */
	assert(false);

	return (NULL);
}
#endif

#ifdef MALLOC_STATS
static bool
arena_stats(arena_t *arena, size_t *allocated, size_t *total)
{

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(allocated != NULL);
	assert(total != NULL);

	malloc_mutex_lock(&arena->mtx);
	*allocated = arena->allocated;
	*total = arena->nchunks * chunk_size;
	malloc_mutex_unlock(&arena->mtx);

	return (false);
}
#endif

static bool
arena_new(arena_t *arena)
{
	bool ret;
	unsigned i;

	malloc_mutex_init(&arena->mtx);

	for (i = 0; i < NBINS; i++)
		qr_new(&arena->bins[i].regions, next.u.s.link);

	for (i = 0; i < BINMASK_NELMS; i++)
		arena->bins_mask[i] = 0;

	arena->split = NULL;
	arena->frag = NULL;
	RB_INIT(&arena->large_regions);

	RB_INIT(&arena->chunks);
	arena->nchunks = 0;

	assert(opt_ndelay > 0);
	arena->delayed = (region_t **)base_alloc(opt_ndelay
	    * sizeof(region_t *));
	if (arena->delayed == NULL) {
		ret = true;
		goto RETURN;
	}
	memset(arena->delayed, 0, opt_ndelay * sizeof(region_t *));
	arena->next_delayed = 0;

#ifdef MALLOC_STATS
	arena->allocated = 0;

	memset(&arena->stats, 0, sizeof(arena_stats_t));
#endif

#ifdef MALLOC_DEBUG
	arena->magic = ARENA_MAGIC;
#endif

	ret = false;
RETURN:
	return (ret);
}

/* Create a new arena and insert it into the arenas array at index ind. */
static arena_t *
arenas_extend(unsigned ind)
{
	arena_t *ret;

	ret = (arena_t *)base_alloc(sizeof(arena_t));
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
	malloc_printf("%s: (malloc) Error initializing arena\n",
	    _getprogname());
	if (opt_abort)
		abort();

	return (arenas[0]);
}

/*
 * End arena.
 */
/******************************************************************************/
/*
 * Begin  general internal functions.
 */

/*
 * Choose an arena based on a per-thread value (fast-path code, calls slow-path
 * code if necessary.
 */
static __inline arena_t *
choose_arena(void)
{
	arena_t *ret;

	/*
	 * We can only use TLS if this is a PIC library, since for the static
	 * library version, libc's malloc is used by TLS allocation, which
	 * introduces a bootstrapping issue.
	 */
#ifndef NO_TLS
	ret = arenas_map;
	if (ret == NULL)
		ret = choose_arena_hard();
#else
	if (__isthreaded) {
		unsigned long ind;
		
		/*
		 * Hash _pthread_self() to one of the arenas.  There is a prime
		 * number of arenas, so this has a reasonable chance of
		 * working.  Even so, the hashing can be easily thwarted by
		 * inconvenient _pthread_self() values.  Without specific
		 * knowledge of how _pthread_self() calculates values, we can't
		 * do much better than this.
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

	/* Assign one of the arenas to this thread, in a round-robin fashion. */
	if (__isthreaded) {
		malloc_mutex_lock(&arenas_mtx);
		ret = arenas[next_arena];
		if (ret == NULL)
			ret = arenas_extend(next_arena);
		next_arena = (next_arena + 1) % narenas;
		malloc_mutex_unlock(&arenas_mtx);
	} else
		ret = arenas[0];
	arenas_map = ret;

	return (ret);
}
#endif

static void *
huge_malloc(arena_t *arena, size_t size)
{
	void *ret;
	size_t chunk_size;
	chunk_node_t *node;

	/* Allocate a chunk for this request. */

#ifdef MALLOC_STATS
	arena->stats.huge.nrequests++;
#endif

	chunk_size = CHUNK_CEILING(size);
	if (chunk_size == 0) {
		/* size is large enough to cause size_t wrap-around. */
		ret = NULL;
		goto RETURN;
	}

	/* Allocate a chunk node with which to track the chunk. */
	node = base_chunk_node_alloc();
	if (node == NULL) {
		ret = NULL;
		goto RETURN;
	}

	ret = chunk_alloc(chunk_size);
	if (ret == NULL) {
		base_chunk_node_dealloc(node);
		ret = NULL;
		goto RETURN;
	}

	/* Insert node into chunks. */
	node->arena = arena;
	node->chunk = ret;
	node->size = chunk_size;
	node->extra = chunk_size - size;

	malloc_mutex_lock(&chunks_mtx);
	RB_INSERT(chunk_tree_s, &huge, node);
#ifdef MALLOC_STATS
	huge_allocated += size;
	huge_total += chunk_size;
#endif
	malloc_mutex_unlock(&chunks_mtx);

RETURN:
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
	malloc_mutex_lock(&node->arena->mtx);
	node->arena->stats.ndalloc++;
	malloc_mutex_unlock(&node->arena->mtx);

	/* Update counters. */
	huge_allocated -= (node->size - node->extra);
	huge_total -= node->size;
#endif

	malloc_mutex_unlock(&chunks_mtx);

	/* Unmap chunk. */
	chunk_dealloc(node->chunk, node->size);

	base_chunk_node_dealloc(node);
}

static void *
imalloc(arena_t *arena, size_t size)
{
	void *ret;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(size != 0);

	if (region_ceiling(size) <= (chunk_size >> 1))
		ret = arena_malloc(arena, size);
	else
		ret = huge_malloc(arena, size);

#ifdef MALLOC_STATS
	malloc_mutex_lock(&arena->mtx);
	arena->stats.nmalloc++;
	malloc_mutex_unlock(&arena->mtx);
#endif

	if (opt_junk) {
		if (ret != NULL)
			memset(ret, 0xa5, size);
	}
	return (ret);
}

static void *
ipalloc(arena_t *arena, size_t alignment, size_t size)
{
	void *ret;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);

	/*
	 * The conditions that disallow calling arena_palloc() are quite
	 * tricky.
	 *
	 * The first main clause of the conditional mirrors that in imalloc(),
	 * and is necesary because arena_palloc() may in turn call
	 * arena_malloc().
	 *
	 * The second and third clauses are necessary because we want to be
	 * sure that it will not be necessary to allocate more than a
	 * half-chunk region at any point during the creation of the aligned
	 * allocation.  These checks closely mirror the calculation of
	 * alloc_size in arena_palloc().
	 *
	 * Finally, the fourth clause makes explicit the constraint on what
	 * alignments will be attempted via regions.  At first glance, this
	 * appears unnecessary, but in actuality, it protects against otherwise
	 * difficult-to-detect size_t wrap-around cases.
	 */
	if (region_ceiling(size) <= (chunk_size >> 1)

	    && (alignment < sizeof(region_small_sizer_t)
	    || (QUANTUM_CEILING(sizeof(region_small_sizer_t)) + alignment
	    + (region_ceiling(size))) <= (chunk_size >> 1))

	    && (alignment >= sizeof(region_small_sizer_t)
	    || ((QUANTUM_CEILING(sizeof(region_small_sizer_t)) << 1)
	    + (region_ceiling(size))) <= (chunk_size >> 1))

	    && alignment <= (chunk_size >> 2))
		ret = arena_palloc(arena, alignment, size);
	else {
		if (alignment <= chunk_size)
			ret = huge_malloc(arena, size);
		else {
			size_t chunksize, alloc_size, offset;
			chunk_node_t *node;

			/*
			 * This allocation requires alignment that is even
			 * larger than chunk alignment.  This means that
			 * huge_malloc() isn't good enough.
			 *
			 * Allocate almost twice as many chunks as are demanded
			 * by the size or alignment, in order to assure the
			 * alignment can be achieved, then unmap leading and
			 * trailing chunks.
			 */

			chunksize = CHUNK_CEILING(size);

			if (size >= alignment)
				alloc_size = chunksize + alignment - chunk_size;
			else
				alloc_size = (alignment << 1) - chunk_size;

			/*
			 * Allocate a chunk node with which to track the chunk.
			 */
			node = base_chunk_node_alloc();
			if (node == NULL) {
				ret = NULL;
				goto RETURN;
			}

			ret = chunk_alloc(alloc_size);
			if (ret == NULL) {
				base_chunk_node_dealloc(node);
				ret = NULL;
				goto RETURN;
			}

			offset = (size_t)ret & (alignment - 1);
			assert(offset % chunk_size == 0);
			assert(offset < alloc_size);
			if (offset == 0) {
				/* Trim trailing space. */
				chunk_dealloc((void *) ((size_t) ret
				    + chunksize), alloc_size - chunksize);
			} else {
				size_t trailsize;

				/* Trim leading space. */
				chunk_dealloc(ret, alignment - offset);

				ret = (void *) ((size_t) ret + (alignment
				    - offset));

				trailsize = alloc_size - (alignment - offset)
				    - chunksize;
				if (trailsize != 0) {
				    /* Trim trailing space. */
				    assert(trailsize < alloc_size);
				    chunk_dealloc((void *) ((size_t) ret
				        + chunksize), trailsize);
				}
			}

			/* Insert node into chunks. */
			node->arena = arena;
			node->chunk = ret;
			node->size = chunksize;
			node->extra = node->size - size;

			malloc_mutex_lock(&chunks_mtx);
			RB_INSERT(chunk_tree_s, &huge, node);
#ifdef MALLOC_STATS
			huge_allocated += size;
			huge_total += chunksize;
#endif
			malloc_mutex_unlock(&chunks_mtx);
		}
	}

RETURN:
#ifdef MALLOC_STATS
	malloc_mutex_lock(&arena->mtx);
	arena->stats.npalloc++;
	malloc_mutex_unlock(&arena->mtx);
#endif

	if (opt_junk) {
		if (ret != NULL)
			memset(ret, 0xa5, size);
	}
	assert(((size_t)ret & (alignment - 1)) == 0);
	return (ret);
}

static void *
icalloc(arena_t *arena, size_t num, size_t size)
{
	void *ret;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(num * size != 0);

	if (region_ceiling(num * size) <= (chunk_size >> 1))
		ret = arena_calloc(arena, num, size);
	else {
		/*
		 * The virtual memory system provides zero-filled pages, so
		 * there is no need to do so manually.
		 */
		ret = huge_malloc(arena, num * size);
#ifdef USE_BRK
		if ((size_t)ret >= (size_t)brk_base
		    && (size_t)ret < (size_t)brk_max) {
			/* 
			 * This may be a re-used brk chunk.  Therefore, zero
			 * the memory.
			 */
			memset(ret, 0, num * size);
		}
#endif
	}

#ifdef MALLOC_STATS
	malloc_mutex_lock(&arena->mtx);
	arena->stats.ncalloc++;
	malloc_mutex_unlock(&arena->mtx);
#endif

	return (ret);
}

static size_t
isalloc(void *ptr)
{
	size_t ret;
	chunk_node_t *node;

	assert(ptr != NULL);
	assert(ptr != &nil);

	node = CHUNK_ADDR2BASE(ptr);
	if (node != ptr) {
		/* Region. */
		assert(node->arena->magic == ARENA_MAGIC);

		ret = arena_salloc(node->arena, ptr);
	} else {
		chunk_node_t key;

		/* Chunk (huge allocation). */

		malloc_mutex_lock(&chunks_mtx);

		/* Extract from tree of huge allocations. */
		key.chunk = ptr;
		node = RB_FIND(chunk_tree_s, &huge, &key);
		assert(node != NULL);

		ret = node->size - node->extra;

		malloc_mutex_unlock(&chunks_mtx);
	}

	return (ret);
}

static void
idalloc(void *ptr)
{
	chunk_node_t *node;

	assert(ptr != NULL);
	assert(ptr != &nil);

	node = CHUNK_ADDR2BASE(ptr);
	if (node != ptr) {
		/* Region. */
#ifdef MALLOC_STATS
		malloc_mutex_lock(&node->arena->mtx);
		node->arena->stats.ndalloc++;
		malloc_mutex_unlock(&node->arena->mtx);
#endif
		arena_dalloc(node->arena, ptr);
	} else
		huge_dalloc(ptr);
}

static void *
iralloc(arena_t *arena, void *ptr, size_t size)
{
	void *ret;
	size_t oldsize;

	assert(arena != NULL);
	assert(arena->magic == ARENA_MAGIC);
	assert(ptr != NULL);
	assert(ptr != &nil);
	assert(size != 0);

	oldsize = isalloc(ptr);

	if (region_ceiling(size) <= (chunk_size >> 1)) {
		ret = arena_malloc(arena, size);
		if (ret == NULL)
			goto RETURN;
		if (opt_junk)
			memset(ret, 0xa5, size);

		if (size < oldsize)
			memcpy(ret, ptr, size);
		else
			memcpy(ret, ptr, oldsize);
	} else {
		ret = huge_malloc(arena, size);
		if (ret == NULL)
			goto RETURN;
		if (opt_junk)
			memset(ret, 0xa5, size);

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
	}

	idalloc(ptr);

RETURN:
#ifdef MALLOC_STATS
	malloc_mutex_lock(&arena->mtx);
	arena->stats.nralloc++;
	malloc_mutex_unlock(&arena->mtx);
#endif
	return (ret);
}

#ifdef MALLOC_STATS
static void
istats(size_t *allocated, size_t *total)
{
	size_t tallocated, ttotal;
	size_t rallocated, rtotal;
	unsigned i;

	tallocated = 0;
	ttotal = base_total;

	/* arenas. */
	for (i = 0; i < narenas; i++) {
		if (arenas[i] != NULL) {
			arena_stats(arenas[i], &rallocated, &rtotal);
			tallocated += rallocated;
			ttotal += rtotal;
		}
	}

	/* huge. */
	malloc_mutex_lock(&chunks_mtx);
	tallocated += huge_allocated;
	ttotal += huge_total;
	malloc_mutex_unlock(&chunks_mtx);

	/* Return results. */
	*allocated = tallocated;
	*total = ttotal;
}
#endif

static void
malloc_print_stats(void)
{

	if (opt_print_stats) {
		malloc_printf("___ Begin malloc statistics ___\n");
		malloc_printf("Number of CPUs: %u\n", ncpus);
		malloc_printf("Number of arenas: %u\n", narenas);
		malloc_printf("Cache slots: %u\n", opt_ndelay);
		malloc_printf("Chunk size: %zu (2^%zu)\n", chunk_size,
		    opt_chunk_2pow);
		malloc_printf("Quantum size: %zu (2^%zu)\n", quantum, 
		    opt_quantum_2pow);
		malloc_printf("Pointer size: %u\n", sizeof(size_t));
		malloc_printf("Number of bins: %u\n", NBINS);
		malloc_printf("Maximum bin size: %u\n", bin_maxsize);
		malloc_printf("Assertions %s\n",
#ifdef NDEBUG
		    "disabled"
#else
		    "enabled"
#endif
		    );
		malloc_printf("Redzone size: %u\n", 
#ifdef MALLOC_REDZONES
				MALLOC_RED
#else
				0
#endif
				);

#ifdef MALLOC_STATS
		{
			size_t a, b;

			istats(&a, &b);
			malloc_printf("Allocated: %zu, space used: %zu\n", a,
			    b);
		}

		{
			arena_stats_t stats_arenas;
			arena_t *arena;
			unsigned i;

			/* Print chunk stats. */
			{
				chunk_stats_t chunks_stats;

				malloc_mutex_lock(&chunks_mtx);
				chunks_stats = stats_chunks;
				malloc_mutex_unlock(&chunks_mtx);

				malloc_printf("\nchunks:\n");
				malloc_printf(" %13s%13s%13s\n", "nchunks",
				    "highchunks", "curchunks");
				malloc_printf(" %13llu%13lu%13lu\n",
				    chunks_stats.nchunks, 
				    chunks_stats.highchunks,
				    chunks_stats.curchunks);
			}

#ifdef MALLOC_STATS_ARENAS
			/* Print stats for each arena. */
			for (i = 0; i < narenas; i++) {
				arena = arenas[i];
				if (arena != NULL) {
					malloc_printf(
					    "\narenas[%u] statistics:\n", i);
					malloc_mutex_lock(&arena->mtx);
					stats_print(&arena->stats);
					malloc_mutex_unlock(&arena->mtx);
				} else {
					malloc_printf("\narenas[%u] statistics:"
					    " unused arena\n", i);
				}
			}
#endif

			/* Merge arena stats from arenas. */
			memset(&stats_arenas, 0, sizeof(arena_stats_t));
			for (i = 0; i < narenas; i++) {
				arena = arenas[i];
				if (arena != NULL) {
					malloc_mutex_lock(&arena->mtx);
					stats_merge(arena, &stats_arenas);
					malloc_mutex_unlock(&arena->mtx);
				}
			}

			/* Print arena stats. */
			malloc_printf("\nMerged arena statistics:\n");
			stats_print(&stats_arenas);
		}
#endif /* #ifdef MALLOC_STATS */
		malloc_printf("--- End malloc statistics ---\n");
	}
}

/*
 * FreeBSD's pthreads implementation calls malloc(3), so the malloc
 * implementation has to take pains to avoid infinite recursion during
 * initialization.
 *
 * atomic_init_start() returns true if it started initializing.  In that case,
 * the caller must also call atomic_init_finish(), just before returning
 * to its caller.  This delayed finalization of initialization is critical,
 * since otherwise choose_arena() has no way to know whether it's safe
 * to call _pthread_self().
 */
static __inline bool
malloc_init(void)
{

	/*
	 * We always initialize before threads are created, since any thread
	 * creation first triggers allocations.
	 */
	assert(__isthreaded == 0 || malloc_initialized);

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
			case 'c':
				opt_ndelay <<= 1;
				if (opt_ndelay == 0)
					opt_ndelay = 1;
				break;
			case 'C':
				opt_ndelay >>= 1;
				if (opt_ndelay == 0)
					opt_ndelay = 1;
				break;
			case 'j':
				opt_junk = false;
				break;
			case 'J':
				opt_junk = true;
				break;
			case 'k':
				if ((1 << opt_chunk_2pow) > pagesize)
					opt_chunk_2pow--;
				break;
			case 'K':
				if (opt_chunk_2pow < CHUNK_2POW_MAX)
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
				if ((1 << opt_quantum_2pow) < pagesize)
					opt_quantum_2pow++;
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
			default:
				malloc_printf("%s: (malloc) Unsupported"
				    " character in malloc options: '%c'\n",
				    _getprogname(), opts[j]);
			}
		}
	}

	/* Take care to call atexit() only once. */
	if (opt_print_stats) {
		/* Print statistics at exit. */
		atexit(malloc_print_stats);
	}

	/* Set variables according to the value of opt_quantum_2pow. */
	quantum = (1 << opt_quantum_2pow);
	quantum_mask = quantum - 1;
	bin_shift = ((QUANTUM_CEILING(sizeof(region_small_sizer_t))
	    >> opt_quantum_2pow));
	bin_maxsize = ((NBINS + bin_shift - 1) * quantum);

	/* Set variables according to the value of opt_chunk_2pow. */
	chunk_size = (1 << opt_chunk_2pow);
	chunk_size_mask = chunk_size - 1;

	UTRACE(0, 0, 0);

#ifdef MALLOC_STATS
	memset(&stats_chunks, 0, sizeof(chunk_stats_t));
#endif

	/* Various sanity checks that regard configuration. */
	assert(quantum >= 2 * sizeof(void *));
	assert(quantum <= pagesize);
	assert(chunk_size >= pagesize);
	assert(quantum * 4 <= chunk_size);

	/* Initialize chunks data. */
	malloc_mutex_init(&chunks_mtx);
	RB_INIT(&huge);
#ifdef USE_BRK
	brk_base = sbrk(0);
	brk_prev = brk_base;
	brk_max = (void *)((size_t)brk_base + MAXDSIZ);
#endif
#ifdef MALLOC_STATS
	huge_allocated = 0;
	huge_total = 0;
#endif
	RB_INIT(&old_chunks);

	/* Initialize base allocation data structures. */
	base_chunk = NULL;
	base_next_addr = NULL;
	base_past_addr = NULL;
	base_chunk_nodes = NULL;
	malloc_mutex_init(&base_mtx);
#ifdef MALLOC_STATS
	base_total = 0;
#endif

	if (ncpus > 1) {
		/* 
		 * For SMP systems, create twice as many arenas as there are
		 * CPUs by default.
		 */
		opt_narenas_lshift++;
	}

	/* Determine how many arenas to use. */
	narenas = 1;
	if (opt_narenas_lshift > 0)
		narenas <<= opt_narenas_lshift;

#ifdef NO_TLS
	if (narenas > 1) {
		static const unsigned primes[] = {1, 3, 5, 7, 11, 13, 17, 19,
		    23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83,
		    89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149,
		    151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211,
		    223, 227, 229, 233, 239, 241, 251, 257, 263};
		unsigned i, nprimes, parenas;

		/*
		 * Pick a prime number of hash arenas that is more than narenas
		 * so that direct hashing of pthread_self() pointers tends to
		 * spread allocations evenly among the arenas.
		 */
		assert((narenas & 1) == 0); /* narenas must be even. */
		nprimes = sizeof(primes) / sizeof(unsigned);
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
	if (arenas == NULL)
		return (true);
	/*
	 * Zero the array.  In practice, this should always be pre-zeroed,
	 * since it was just mmap()ed, but let's be sure.
	 */
	memset(arenas, 0, sizeof(arena_t *) * narenas);

	/*
	 * Initialize one arena here.  The rest are lazily created in
	 * arena_choose_hard().
	 */
	arenas_extend(0);
	if (arenas[0] == NULL)
		return (true);

	malloc_mutex_init(&arenas_mtx);

	malloc_initialized = true;
	return (false);
}

/*
 * End library-internal functions.
 */
/******************************************************************************/
/*
 * Begin malloc(3)-compatible functions.
 */

void *
malloc(size_t size)
{
	void *ret;
	arena_t *arena;

	if (malloc_init()) {
		ret = NULL;
		goto RETURN;
	}

	if (size == 0) {
		if (opt_sysv == false)
			ret = &nil;
		else
			ret = NULL;
		goto RETURN;
	}

	arena = choose_arena();
	if (arena != NULL)
		ret = imalloc(arena, size);
	else
		ret = NULL;

RETURN:
	if (ret == NULL) {
		if (opt_xmalloc) {
			malloc_printf("%s: (malloc) Error in malloc(%zu):"
			    " out of memory\n", _getprogname(), size);
			abort();
		}
		errno = ENOMEM;
	} else if (opt_zero)
		memset(ret, 0, size);

	UTRACE(0, size, ret);
	return (ret);
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int ret;
	arena_t *arena;
	void *result;

	if (malloc_init())
		result = NULL;
	else {
		/* Make sure that alignment is a large enough power of 2. */
		if (((alignment - 1) & alignment) != 0
		    || alignment < sizeof(void *)) {
			if (opt_xmalloc) {
				malloc_printf("%s: (malloc) Error in"
				    " posix_memalign(%zu, %zu):"
				    " invalid alignment\n",
				    _getprogname(), alignment, size);
				abort();
			}
			result = NULL;
			ret = EINVAL;
			goto RETURN;
		}

		arena = choose_arena();
		if (arena != NULL)
			result = ipalloc(arena, alignment, size);
		else
			result = NULL;
	}

	if (result == NULL) {
		if (opt_xmalloc) {
			malloc_printf("%s: (malloc) Error in"
			    " posix_memalign(%zu, %zu): out of memory\n",
			    _getprogname(), alignment, size);
			abort();
		}
		ret = ENOMEM;
		goto RETURN;
	} else if (opt_zero)
		memset(result, 0, size);

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
	arena_t *arena;

	if (malloc_init()) {
		ret = NULL;
		goto RETURN;
	}

	if (num * size == 0) {
		if (opt_sysv == false)
			ret = &nil;
		else
			ret = NULL;
		goto RETURN;
	} else if ((num * size) / size != num) {
		/* size_t overflow. */
		ret = NULL;
		goto RETURN;
	}

	arena = choose_arena();
	if (arena != NULL)
		ret = icalloc(arena, num, size);
	else
		ret = NULL;

RETURN:
	if (ret == NULL) {
		if (opt_xmalloc) {
			malloc_printf("%s: (malloc) Error in"
			    " calloc(%zu, %zu): out of memory\n", 
			    _getprogname(), num, size);
			abort();
		}
		errno = ENOMEM;
	} else if (opt_zero) {
		/*
		 * This has the side effect of faulting pages in, even if the
		 * pages are pre-zeroed by the kernel.
		 */
		memset(ret, 0, num * size);
	}

	UTRACE(0, num * size, ret);
	return (ret);
}

void *
realloc(void *ptr, size_t size)
{
	void *ret;

	if (size != 0) {
		arena_t *arena;

		if (ptr != &nil && ptr != NULL) {
			assert(malloc_initialized);

			arena = choose_arena();
			if (arena != NULL)
				ret = iralloc(arena, ptr, size);
			else
				ret = NULL;

			if (ret == NULL) {
				if (opt_xmalloc) {
					malloc_printf("%s: (malloc) Error in"
					    " ralloc(%p, %zu): out of memory\n",
					    _getprogname(), ptr, size);
					abort();
				}
				errno = ENOMEM;
			} else if (opt_zero) {
				size_t old_size;

				old_size = isalloc(ptr);

				if (old_size < size) {
				    memset(&((char *)ret)[old_size], 0,
					    size - old_size);
				}
			}
		} else {
			if (malloc_init())
				ret = NULL;
			else {
				arena = choose_arena();
				if (arena != NULL)
					ret = imalloc(arena, size);
				else
					ret = NULL;
			}

			if (ret == NULL) {
				if (opt_xmalloc) {
					malloc_printf("%s: (malloc) Error in"
					    " ralloc(%p, %zu): out of memory\n",
					    _getprogname(), ptr, size);
					abort();
				}
				errno = ENOMEM;
			} else if (opt_zero)
				memset(ret, 0, size);
		}
	} else {
		if (ptr != &nil && ptr != NULL)
			idalloc(ptr);

		ret = &nil;
	}

	UTRACE(ptr, size, ret);
	return (ret);
}

void
free(void *ptr)
{

	UTRACE(ptr, 0, 0);
	if (ptr != &nil && ptr != NULL) {
		assert(malloc_initialized);

		idalloc(ptr);
	}
}

/*
 * End malloc(3)-compatible functions.
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
