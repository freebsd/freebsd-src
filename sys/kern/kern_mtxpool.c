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
 *	- no initialization/destructor overhead
 *	- can be used with msleep.
 *
 * Disadvantages:
 *	- should generally only be used as leaf mutexes
 *	- pool/pool dependancy ordering cannot be depended on.
 *	- possible L1 cache mastersip contention between cpus
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#ifndef MTX_POOL_SIZE
#define MTX_POOL_SIZE	128
#endif
#define MTX_POOL_MASK	(MTX_POOL_SIZE-1)

static struct mtx mtx_pool_ary[MTX_POOL_SIZE];

int mtx_pool_valid = 0;

/*
 * Inline version of mtx_pool_find(), used to streamline our main API
 * function calls.
 */
static __inline
struct mtx *
_mtx_pool_find(void *ptr)
{
    return(&mtx_pool_ary[((int)ptr ^ ((int)ptr >> 6)) & MTX_POOL_MASK]);
}

static void
mtx_pool_setup(void *dummy __unused)
{
    int i;

    for (i = 0; i < MTX_POOL_SIZE; ++i)
	mtx_init(&mtx_pool_ary[i], "pool mutex", MTX_DEF);
    mtx_pool_valid = 1;
}

/*
 * Obtain a (shared) mutex from the pool.  The returned mutex is a leaf
 * level mutex, meaning that if you obtain it you cannot obtain any other
 * mutexes until you release it.  You can legally msleep() on the mutex.
 */
struct mtx *
mtx_pool_alloc(void)
{
    static int si;
    return(&mtx_pool_ary[si++ & MTX_POOL_MASK]);
}

/*
 * Return the (shared) pool mutex associated with the specified address.
 * The returned mutex is a leaf level mutex, meaning that if you obtain it
 * you cannot obtain any other mutexes until you release it.  You can
 * legally msleep() on the mutex.
 */
struct mtx *
mtx_pool_find(void *ptr)
{
    return(_mtx_pool_find(ptr));
}

/*
 * Combined find/lock operation.  Lock the pool mutex associated with
 * the specified address.
 */
void 
mtx_pool_lock(void *ptr)
{
    mtx_lock(_mtx_pool_find(ptr));
}

/*
 * Combined find/unlock operation.  Unlock the pool mutex associated with
 * the specified address.
 */
void
mtx_pool_unlock(void *ptr)
{
    mtx_unlock(_mtx_pool_find(ptr));
}

SYSINIT(mtxpooli, SI_SUB_MUTEX, SI_ORDER_FIRST, mtx_pool_setup, NULL)   

