/*
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include "thr_private.h"

__weak_reference(_pthread_atfork, pthread_atfork);

int
_pthread_atfork(void (*prepare)(void), void (*parent)(void),
    void (*child)(void))
{
	struct pthread *curthread;
	struct pthread_atfork *af;

	_thr_check_init();

	if ((af = malloc(sizeof(struct pthread_atfork))) == NULL)
		return (ENOMEM);

	curthread = _get_curthread();
	af->prepare = prepare;
	af->parent = parent;
	af->child = child;
	THR_UMTX_LOCK(curthread, &_thr_atfork_lock);
	TAILQ_INSERT_TAIL(&_thr_atfork_list, af, qe);
	THR_UMTX_UNLOCK(curthread, &_thr_atfork_lock);
	return (0);
}
