/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * $FreeBSD$
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)atexit.c	8.2 (Berkeley) 7/3/94";
#endif /* LIBC_SCCS and not lint */

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include "atexit.h"

#include "libc_private.h"
#include "spinlock.h"

static spinlock_t thread_lock = _SPINLOCK_INITIALIZER;

#define THREAD_LOCK()		if (__isthreaded) _SPINLOCK(&thread_lock);
#define THREAD_UNLOCK()		if (__isthreaded) _SPINUNLOCK(&thread_lock);

struct atexit *__atexit;	/* points to head of LIFO stack */

/*
 * Register a function to be performed at exit.
 */
int
atexit(fn)
	void (*fn)();
{
	static struct atexit __atexit0;	/* one guaranteed table */
	register struct atexit *p;

	THREAD_LOCK();
	if ((p = __atexit) == NULL)
		__atexit = p = &__atexit0;
	else while (p->ind >= ATEXIT_SIZE) {
		struct atexit *old__atexit;
		old__atexit = __atexit;
	        THREAD_UNLOCK();
		if ((p = (struct atexit *)malloc(sizeof(*p))) == NULL)
			return (-1);
		THREAD_LOCK();
		if (old__atexit != __atexit) {
			/* Lost race, retry operation */
			THREAD_UNLOCK();
			free(p);
			THREAD_LOCK();
			p = __atexit;
			continue;
		}
		p->ind = 0;
		p->next = __atexit;
		__atexit = p;
	}
	p->fns[p->ind++] = fn;
	THREAD_UNLOCK();
	return (0);
}
