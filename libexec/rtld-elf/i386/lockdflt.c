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
 * $FreeBSD$
 */

/*
 * Thread locking implementation for the dynamic linker.
 *
 * On 80486 and later CPUs we use the "simple, non-scalable
 * reader-preference lock" from:
 *
 *   J. M. Mellor-Crummey and M. L. Scott. "Scalable Reader-Writer
 *   Synchronization for Shared-Memory Multiprocessors." 3rd ACM Symp. on
 *   Principles and Practice of Parallel Programming, April 1991.
 *
 * In this algorithm the lock is a single word.  Its low-order bit is
 * set when a writer holds the lock.  The remaining high-order bits
 * contain a count of readers desiring the lock.  The algorithm requires
 * atomic "compare_and_store" and "add" operations.
 *
 * The "compare_and_store" operation requires the "cmpxchg" instruction
 * on the x86.  Unfortunately, the 80386 CPU does not support that
 * instruction -- only the 80486 and later models support it.  So on the
 * 80386 we must use simple test-and-set exclusive locks instead.  We
 * determine which kind of lock to use by trying to execute a "cmpxchg"
 * instruction and catching the SIGILL which results on the 80386.
 */

#include <setjmp.h>
#include <signal.h>

static inline int
cmpxchgl(int old, int new, volatile int *m)
{
	int result;

	__asm __volatile ("lock; cmpxchgl %2, %0"
	    : "+m"(*m), "=a"(result)
	    : "r"(new), "1"(old)
	    : "cc");

	return result;
}

static inline int
xchgl(int v, volatile int *m)
{
	int result;

	__asm __volatile ("xchgl %0, %1"
	    : "=r"(result), "+m"(*m)
	    : "0"(v));

	return result;
}

/*
 * Crude exclusive locks for the 80386, which does not support the
 * cmpxchg instruction.
 */
static void
lock80386_acquire(void *lock)
{
    Lock *l = (Lock *)lock;
    sigset_t tmp_oldsigmask;

    for ( ; ; ) {
	sigprocmask(SIG_BLOCK, &fullsigmask, &tmp_oldsigmask);
	if (xchgl(1, &l->lock) == 0)
	    break;
	sigprocmask(SIG_SETMASK, &tmp_oldsigmask, NULL);
	while (l->lock != 0)
	    ;	/* Spin */
    }
    oldsigmask = tmp_oldsigmask;
}

static void
lock80386_release(void *lock)
{
    Lock *l = (Lock *)lock;

    l->lock = 0;
    sigprocmask(SIG_SETMASK, &oldsigmask, NULL);
}

/*
 * Code to determine at runtime whether the CPU supports the cmpxchg
 * instruction.  This instruction allows us to use locks that are more
 * efficient, but it didn't exist on the 80386.
 */
static jmp_buf sigill_env;

static void
sigill(int sig)
{
    longjmp(sigill_env, 1);
}

static int
cpu_supports_cmpxchg(void)
{
    struct sigaction act, oact;
    int result;
    volatile int lock;

    memset(&act, 0, sizeof act);
    act.sa_handler = sigill;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    sigaction(SIGILL, &act, &oact);
    if (setjmp(sigill_env) == 0) {
	lock = 0;
	cmpxchgl(0, 1, &lock);
	result = 1;
    } else
	result = 0;
    sigaction(SIGILL, &oact, NULL);
    return result;
}

