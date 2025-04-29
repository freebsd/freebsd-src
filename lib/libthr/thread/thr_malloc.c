/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <rtld_malloc.h>
#include "thr_private.h"

int npagesizes;
size_t *pagesizes;
static size_t pagesizes_d[2];
static struct umutex thr_malloc_umtx;
static u_int thr_malloc_umtx_level;

void
__thr_malloc_init(void)
{

	if (npagesizes != 0)
		return;
	npagesizes = getpagesizes(pagesizes_d, nitems(pagesizes_d));
	if (npagesizes == -1) {
		PANIC("Unable to read page sizes");
	}
	pagesizes = pagesizes_d;
	_thr_umutex_init(&thr_malloc_umtx);
}

static void
thr_malloc_lock(struct pthread *curthread)
{
	uint32_t curtid;

	if (curthread == NULL)
		return;
	curthread->locklevel++;
	curtid = TID(curthread);
	if ((uint32_t)thr_malloc_umtx.m_owner == curtid)
		thr_malloc_umtx_level++;
	else
		_thr_umutex_lock(&thr_malloc_umtx, curtid);
}

static void
thr_malloc_unlock(struct pthread *curthread)
{

	if (curthread == NULL)
		return;
	if (thr_malloc_umtx_level > 0)
		thr_malloc_umtx_level--;
	else
		_thr_umutex_unlock(&thr_malloc_umtx, TID(curthread));
	curthread->locklevel--;
	_thr_ast(curthread);
}

void *
__thr_calloc(size_t num, size_t size)
{
	struct pthread *curthread;
	void *res;

	curthread = _get_curthread();
	thr_malloc_lock(curthread);
	res = __crt_calloc(num, size);
	thr_malloc_unlock(curthread);
	return (res);
}

void
__thr_free(void *cp)
{
	struct pthread *curthread;

	curthread = _get_curthread();
	thr_malloc_lock(curthread);
	__crt_free(cp);
	thr_malloc_unlock(curthread);
}

void *
__thr_malloc(size_t nbytes)
{
	struct pthread *curthread;
	void *res;

	curthread = _get_curthread();
	thr_malloc_lock(curthread);
	res = __crt_malloc(nbytes);
	thr_malloc_unlock(curthread);
	return (res);
}

void *
__thr_aligned_alloc_offset(size_t align, size_t size, size_t offset)
{
	struct pthread *curthread;
	void *res;

	curthread = _get_curthread();
	thr_malloc_lock(curthread);
	res = __crt_aligned_alloc_offset(align, size, offset);
	thr_malloc_unlock(curthread);
	return (res);
}

void *
__thr_realloc(void *cp, size_t nbytes)
{
	struct pthread *curthread;
	void *res;

	curthread = _get_curthread();
	thr_malloc_lock(curthread);
	res = __crt_realloc(cp, nbytes);
	thr_malloc_unlock(curthread);
	return (res);
}

void
__thr_malloc_prefork(struct pthread *curthread)
{

	_thr_umutex_lock(&thr_malloc_umtx, TID(curthread));
}

void
__thr_malloc_postfork(struct pthread *curthread)
{

	_thr_umutex_unlock(&thr_malloc_umtx, TID(curthread));
}
