/*-
 * Copyright 1999, 2000 John D. Polstra.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: FreeBSD: src/libexec/rtld-elf/alpha/lockdflt.c,v 1.6 2000/07/17
 * $FreeBSD$
 */

/*
 * Thread locking implementation for the dynamic linker.
 *
 * We use the "simple, non-scalable reader-preference lock" from:
 *
 *   J. M. Mellor-Crummey and M. L. Scott. "Scalable Reader-Writer
 *   Synchronization for Shared-Memory Multiprocessors." 3rd ACM Symp. on
 *   Principles and Practice of Parallel Programming, April 1991.
 *
 * In this algorithm the lock is a single word.  Its low-order bit is
 * set when a writer holds the lock.  The remaining high-order bits
 * contain a count of readers desiring the lock.  The algorithm requires
 * atomic "compare_and_store" and "add" operations, which we implement
 * using assembly language sequences in "rtld_start.S".
 *
 * These are spinlocks.  When spinning we call nanosleep() for 1
 * microsecond each time around the loop.  This will most likely yield
 * the CPU to other threads (including, we hope, the lockholder) allowing
 * them to make some progress.
 */

#include <signal.h>
#include <stdlib.h>
#include <time.h>

#include <machine/atomic.h>

#include "debug.h"
#include "rtld.h"
#include "rtld_machdep.h"

#define WAFLAG		0x1	/* A writer holds the lock */
#define RC_INCR		0x2	/* Adjusts count of readers desiring lock */

typedef struct Struct_Lock {
	volatile int lock;
	void *base;
} Lock;

static const struct timespec usec = { 0, 1000 };	/* 1 usec. */
static sigset_t fullsigmask, oldsigmask;

static void *
lock_create(void *context)
{
    void *base;
    char *p;
    uintptr_t r;
    Lock *l;

    /*
     * Arrange for the lock to occupy its own cache line.  First, we
     * optimistically allocate just a cache line, hoping that malloc
     * will give us a well-aligned block of memory.  If that doesn't
     * work, we allocate a larger block and take a well-aligned cache
     * line from it.
     */
    base = xmalloc(CACHE_LINE_SIZE);
    p = (char *)base;
    if ((uintptr_t)p % CACHE_LINE_SIZE != 0) {
	free(base);
	base = xmalloc(2 * CACHE_LINE_SIZE);
	p = (char *)base;
	if ((r = (uintptr_t)p % CACHE_LINE_SIZE) != 0)
	    p += CACHE_LINE_SIZE - r;
    }
    l = (Lock *)p;
    l->base = base;
    l->lock = 0;
    return l;
}

static void
lock_destroy(void *lock)
{
    Lock *l = (Lock *)lock;

    free(l->base);
}

static void
rlock_acquire(void *lock)
{
    Lock *l = (Lock *)lock;

    atomic_add_acq_int(&l->lock, RC_INCR);
    while (l->lock & WAFLAG)
	    nanosleep(&usec, NULL);
}

static void
wlock_acquire(void *lock)
{
    Lock *l = (Lock *)lock;
    sigset_t tmp_oldsigmask;

    for ( ; ; ) {
	sigprocmask(SIG_BLOCK, &fullsigmask, &tmp_oldsigmask);
	if (atomic_cmpset_acq_int(&l->lock, 0, WAFLAG))
	    break;
	sigprocmask(SIG_SETMASK, &tmp_oldsigmask, NULL);
	nanosleep(&usec, NULL);
    }
    oldsigmask = tmp_oldsigmask;
}

static void
rlock_release(void *lock)
{
    Lock *l = (Lock *)lock;

    atomic_add_rel_int(&l->lock, -RC_INCR);
}

static void
wlock_release(void *lock)
{
    Lock *l = (Lock *)lock;

    atomic_add_rel_int(&l->lock, -WAFLAG);
    sigprocmask(SIG_SETMASK, &oldsigmask, NULL);
}

void
lockdflt_init(LockInfo *li)
{
    li->context = NULL;
    li->lock_create = lock_create;
    li->rlock_acquire = rlock_acquire;
    li->wlock_acquire = wlock_acquire;
    li->rlock_release = rlock_release;
    li->wlock_release = wlock_release;
    li->lock_destroy = lock_destroy;
    li->context_destroy = NULL;
    /*
     * Construct a mask to block all signals except traps which might
     * conceivably be generated within the dynamic linker itself.
     */
    sigfillset(&fullsigmask);
    sigdelset(&fullsigmask, SIGILL);
    sigdelset(&fullsigmask, SIGTRAP);
    sigdelset(&fullsigmask, SIGABRT);
    sigdelset(&fullsigmask, SIGEMT);
    sigdelset(&fullsigmask, SIGFPE);
    sigdelset(&fullsigmask, SIGBUS);
    sigdelset(&fullsigmask, SIGSEGV);
    sigdelset(&fullsigmask, SIGSYS);
}
