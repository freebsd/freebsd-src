/*
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>

#include "spinlock.h"

long _atomic_lock_stub(volatile long *);
void _spinlock_stub(spinlock_t *);
void _spinunlock_stub(spinlock_t *);
void _spinlock_debug_stub(spinlock_t *, char *, int);

/*
 * Declare weak definitions in case the application is not linked
 * with libpthread.
 */
__weak_reference(_atomic_lock_stub, _atomic_lock);
__weak_reference(_spinlock_stub, _spinlock);
__weak_reference(_spinunlock_stub, _spinunlock);
__weak_reference(_spinlock_debug_stub, _spinlock_debug);

/*
 * This function is a stub for the _atomic_lock function in libpthread.
 */
long
_atomic_lock_stub(volatile long *lck __unused)
{
	return (0L);
}


/*
 * This function is a stub for the spinlock function in libpthread.
 */
void
_spinlock_stub(spinlock_t *lck __unused)
{
}

/*
 * This function is a stub for the spinunlock function in libpthread.
 */
void
_spinunlock_stub(spinlock_t *lck __unused)
{
}

/*
 * This function is a stub for the debug spinlock function in libpthread.
 */
void
_spinlock_debug_stub(spinlock_t *lck __unused, char *fname __unused, int lineno __unused)
{
}
