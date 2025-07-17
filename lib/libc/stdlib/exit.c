/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include "namespace.h"
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "un-namespace.h"

#include "atexit.h"
#include "libc_private.h"

void (*__cleanup)(void);

/*
 * This variable is zero until a process has created a thread.
 * It is used to avoid calling locking functions in libc when they
 * are not required. By default, libc is intended to be(come)
 * thread-safe, but without a (significant) penalty to non-threaded
 * processes.
 */
int	__isthreaded	= 0;

static pthread_mutex_t exit_mutex;
static pthread_once_t exit_mutex_once = PTHREAD_ONCE_INIT;

static void
exit_mutex_init_once(void)
{
	pthread_mutexattr_t ma;

	_pthread_mutexattr_init(&ma);
	_pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
	_pthread_mutex_init(&exit_mutex, &ma);
	_pthread_mutexattr_destroy(&ma);
}

/*
 * Exit, flushing stdio buffers if necessary.
 */
void
exit(int status)
{
	/* Ensure that the auto-initialization routine is linked in: */
	extern int _thread_autoinit_dummy_decl;

	_thread_autoinit_dummy_decl = 1;

	/* Make exit(3) thread-safe */
	if (__isthreaded) {
		_once(&exit_mutex_once, exit_mutex_init_once);
		_pthread_mutex_lock(&exit_mutex);
	}

	/*
	 * We're dealing with cleaning up thread_local destructors in the case of
	 * the process termination through main() exit.
	 * Other cases are handled elsewhere.
	 */
	__cxa_thread_call_dtors();
	__cxa_finalize(NULL);
	if (__cleanup)
		(*__cleanup)();
	_exit(status);
}
