/*-
 * Copyright 1999 John D. Polstra.
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
 * Default thread locking implementation for the dynamic linker.  It
 * is used until the client registers a different implementation with
 * dllockinit().  The default implementation does mutual exclusion
 * by blocking the SIGVTALRM, SIGPROF, and SIGALRM signals.  This is
 * based on the observation that most userland thread packages use one
 * of these signals to support preemption.
 */

#include <dlfcn.h>
#include <signal.h>
#include <stdlib.h>

#include "debug.h"
#include "rtld.h"

typedef struct Struct_LockDflt {
    sigset_t lock_mask;
    sigset_t old_mask;
    int depth;
} LockDflt;

void
lockdflt_acquire(void *lock)
{
    LockDflt *l = (LockDflt *)lock;
    sigset_t old_mask;

    sigprocmask(SIG_BLOCK, &l->lock_mask, &old_mask);
    if (l->depth == 0)
	l->old_mask = old_mask;
    l->depth++;
}

void *
lockdflt_create(void *context)
{
    LockDflt *l;

    l = NEW(LockDflt);
    l->depth = 0;
    sigemptyset(&l->lock_mask);
    sigaddset(&l->lock_mask, SIGVTALRM);
    sigaddset(&l->lock_mask, SIGPROF);
    sigaddset(&l->lock_mask, SIGALRM);
    return l;
}

void
lockdflt_destroy(void *lock)
{
    LockDflt *l = (LockDflt *)lock;
    free(l);
}

void
lockdflt_release(void *lock)
{
    LockDflt *l = (LockDflt *)lock;
    l->depth--;
    assert(l->depth >= 0);
    if (l->depth == 0)
	sigprocmask(SIG_SETMASK, &l->old_mask, NULL);
}

void
lockdflt_init(void)
{
    dllockinit(NULL, lockdflt_create, lockdflt_acquire, lockdflt_acquire,
      lockdflt_release, lockdflt_destroy, NULL);
}
