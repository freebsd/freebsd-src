/*-
 * Copyright (c) 2001 Matthew Dillon.  All Rights Reserved.  Copyright
 * terms are as specified in the COPYRIGHT file at the base of the source
 * tree.
 *
 * Mutex pool routines.  These routines are designed to be used as short
 * term leaf mutexes (e.g. the last mutex you might aquire other then
 * calling msleep()).  They operate using a shared pool.  A mutex is chosen
 * from the pool based on the supplied pointer (which may or may not be
 * valid).
 *
 * Advantages:
 *	- no structural overhead.  Mutexes can be associated with structures
 *	  without adding bloat to the structures.
 *	- mutexes can be obtained for invalid pointers, useful when uses
 *	  mutexes to interlock destructor ops.
 *	- no initialization/destructor overhead.
 *	- can be used with msleep.
 *
 * Disadvantages:
 *	- should generally only be used as leaf mutexes.
 *	- pool/pool dependancy ordering cannot be depended on.
 *	- possible L1 cache mastersip contention between cpus.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>


MALLOC_DEFINE(M_MTXPOOL, "mtx_pool", "mutex pool");

/* Pool sizes must be a power of two */
#ifndef MTX_POOL_LOCKBUILDER_SIZE
#define MTX_POOL_LOCKBUILDER_SIZE	128
#endif
#ifndef MTX_POOL_SLEEP_SIZE
#define MTX_POOL_SLEEP_SIZE		128
#endif

struct mtxpool_header {
	int		mtxpool_size;
	int		mtxpool_mask;
	int		mtxpool_shift;
	int		mtxpool_next;
};

struct mtx_pool {
	struct mtxpool_header mtx_pool_header;
	struct mtx	mtx_pool_ary[1];
};

static struct mtx_pool_lockbuilder {
	struct mtxpool_header mtx_pool_header;
	struct mtx	mtx_pool_ary[MTX_POOL_LOCKBUILDER_SIZE];
} lockbuilder_pool;

#define mtx_pool_size	mtx_pool_header.mtxpool_size
#define mtx_pool_mask	mtx_pool_header.mtxpool_mask
#define mtx_pool_shift	mtx_pool_header.mtxpool_shift
#define mtx_pool_next	mtx_pool_header.mtxpool_next

struct mtx_pool *mtxpool_sleep;
struct mtx_pool *mtxpool_lockbuilder;

#if UINTPTR_MAX == UINT64_MAX	/* 64 bits */
# define POINTER_BITS		64
# define HASH_MULTIPLIER	11400714819323198485u /* (2^64)*(sqrt(5)-1)/2 */
#else				/* assume 32 bits */
# define POINTER_BITS		32
# define HASH_MULTIPLIER	2654435769u	      /* (2^32)*(sqrt(5)-1)/2 */
#endif

/*
 * Return the (shared) pool mutex associated with the specified address.
 * The returned mutex is a leaf level mutex, meaning that if you obtain it
 * you cannot obtain any other mutexes until you release it.  You can
 * legally msleep() on the mutex.
 */
struct mtx *
mtx_pool_find(struct mtx_pool *pool, void *ptr)
{
	int p;

	KASSERT(pool != NULL, ("_mtx_pool_find(): null pool"));
	/*
	 * Fibonacci hash, see Knuth's
	 * _Art of Computer Programming, Volume 3 / Sorting and Searching_
	 */
	p = ((HASH_MULTIPLIER * (uintptr_t)ptr) >> pool->mtx_pool_shift) &
	    pool->mtx_pool_mask;
	return (&pool->mtx_pool_ary[p]);
}

static void
mtx_pool_initialize(struct mtx_pool *pool, const char *mtx_name, int pool_size,
    int opts)
{
	int i, maskbits;

	pool->mtx_pool_size = pool_size;
	pool->mtx_pool_mask = pool_size - 1;
	for (i = 1, maskbits = 0; (i & pool_size) == 0; i = i << 1)
		maskbits++;
	pool->mtx_pool_shift = POINTER_BITS - maskbits;
	pool->mtx_pool_next = 0;
	for (i = 0; i < pool_size; ++i)
		mtx_init(&pool->mtx_pool_ary[i], mtx_name, NULL, opts);
}

struct mtx_pool *
mtx_pool_create(const char *mtx_name, int pool_size, int opts)
{
	struct mtx_pool *pool;

	if (pool_size <= 0 || !powerof2(pool_size)) {
		printf("WARNING: %s pool size is not a power of 2.\n",
		    mtx_name);
		pool_size = 128;
	}
	MALLOC(pool, struct mtx_pool *,
	    sizeof (struct mtx_pool) + ((pool_size - 1) * sizeof (struct mtx)),
	    M_MTXPOOL, M_WAITOK | M_ZERO);
	mtx_pool_initialize(pool, mtx_name, pool_size, opts);
	return pool;
}

void
mtx_pool_destroy(struct mtx_pool **poolp)
{
	int i;
	struct mtx_pool *pool = *poolp;

	for (i = pool->mtx_pool_size - 1; i >= 0; --i)
		mtx_destroy(&pool->mtx_pool_ary[i]);
	FREE(pool, M_MTXPOOL);
	*poolp = NULL;
}

static void
mtx_pool_setup_static(void *dummy __unused)
{
	mtx_pool_initialize((struct mtx_pool *)&lockbuilder_pool,
	    "lockbuilder mtxpool", MTX_POOL_LOCKBUILDER_SIZE,
	    MTX_DEF | MTX_NOWITNESS | MTX_QUIET);
	mtxpool_lockbuilder = (struct mtx_pool *)&lockbuilder_pool;
}

static void
mtx_pool_setup_dynamic(void *dummy __unused)
{
	mtxpool_sleep = mtx_pool_create("sleep mtxpool",
	    MTX_POOL_SLEEP_SIZE, MTX_DEF);
}

/*
 * Obtain a (shared) mutex from the pool.  The returned mutex is a leaf
 * level mutex, meaning that if you obtain it you cannot obtain any other
 * mutexes until you release it.  You can legally msleep() on the mutex.
 */
struct mtx *
mtx_pool_alloc(struct mtx_pool *pool)
{
	int i;

	KASSERT(pool != NULL, ("mtx_pool_alloc(): null pool"));
	/*
	 * mtx_pool_next is unprotected against multiple accesses,
	 * but simultaneous access by two CPUs should not be very
	 * harmful.
	 */
	i = pool->mtx_pool_next;
	pool->mtx_pool_next = (i + 1) & pool->mtx_pool_mask;
	return (&pool->mtx_pool_ary[i]);
}

/*
 * The lockbuilder pool must be initialized early because the lockmgr
 * and sx locks depend on it.  The sx locks are used in the kernel
 * memory allocator.  The lockmgr subsystem is initialized by
 * SYSINIT(..., SI_SUB_LOCKMGR, ...).
 *
 * We can't call MALLOC() to dynamically allocate the sleep pool
 * until after kmeminit() has been called, which is done by
 * SYSINIT(..., SI_SUB_KMEM, ...).
 */
SYSINIT(mtxpooli1, SI_SUB_MTX_POOL_STATIC, SI_ORDER_FIRST,
    mtx_pool_setup_static, NULL);
SYSINIT(mtxpooli2, SI_SUB_MTX_POOL_DYNAMIC, SI_ORDER_FIRST,
    mtx_pool_setup_dynamic, NULL);
