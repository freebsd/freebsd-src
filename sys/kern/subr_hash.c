/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/ck.h>
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/hash.h>

#define	ASSERT_NOPAD(head, lock)	_Static_assert(			\
    sizeof(head ## _HEAD(, foo)) + sizeof(struct lock) ==		\
    sizeof(struct { head ## _HEAD(, foo) h; struct lock l; }),		\
    "Structure of " #head "_HEAD and " #lock " has padding")
ASSERT_NOPAD(LIST, mtx);
ASSERT_NOPAD(CK_LIST, mtx);
ASSERT_NOPAD(SLIST, mtx);
ASSERT_NOPAD(CK_SLIST, mtx);
ASSERT_NOPAD(STAILQ, mtx);
ASSERT_NOPAD(CK_STAILQ, mtx);
ASSERT_NOPAD(TAILQ, mtx);
ASSERT_NOPAD(LIST, rwlock);
ASSERT_NOPAD(CK_LIST, rwlock);
ASSERT_NOPAD(SLIST, rwlock);
ASSERT_NOPAD(CK_SLIST, rwlock);
ASSERT_NOPAD(STAILQ, rwlock);
ASSERT_NOPAD(CK_STAILQ, rwlock);
ASSERT_NOPAD(TAILQ, rwlock);
ASSERT_NOPAD(LIST, sx);
ASSERT_NOPAD(CK_LIST, sx);
ASSERT_NOPAD(SLIST, sx);
ASSERT_NOPAD(CK_SLIST, sx);
ASSERT_NOPAD(STAILQ, sx);
ASSERT_NOPAD(CK_STAILQ, sx);
ASSERT_NOPAD(TAILQ, sx);
ASSERT_NOPAD(LIST, rmlock);
ASSERT_NOPAD(CK_LIST, rmlock);
ASSERT_NOPAD(SLIST, rmlock);
ASSERT_NOPAD(CK_SLIST, rmlock);
ASSERT_NOPAD(STAILQ, rmlock);
ASSERT_NOPAD(CK_STAILQ, rmlock);
ASSERT_NOPAD(TAILQ, rmlock);
ASSERT_NOPAD(LIST, rmslock);
ASSERT_NOPAD(CK_LIST, rmslock);
ASSERT_NOPAD(SLIST, rmslock);
ASSERT_NOPAD(CK_SLIST, rmslock);
ASSERT_NOPAD(STAILQ, rmslock);
ASSERT_NOPAD(CK_STAILQ, rmslock);
ASSERT_NOPAD(TAILQ, rmslock);
#undef ASSERT_NOPAD

static inline void
hashalloc_sizes(struct hashalloc_args *args, size_t *hdrsize, size_t *loffset)
{
	switch (args->head) {
	case HASH_HEAD_LIST:
		*loffset = sizeof(LIST_HEAD(, foo));
		break;
	case HASH_HEAD_CK_LIST:
		*loffset = sizeof(CK_LIST_HEAD(, foo));
		break;
	case HASH_HEAD_SLIST:
		*loffset = sizeof(SLIST_HEAD(, foo));
		break;
	case HASH_HEAD_CK_SLIST:
		*loffset = sizeof(CK_SLIST_HEAD(, foo));
		break;
	case HASH_HEAD_STAILQ:
		*loffset = sizeof(STAILQ_HEAD(, foo));
		break;
	case HASH_HEAD_CK_STAILQ:
		*loffset = sizeof(CK_STAILQ_HEAD(, foo));
		break;
	case HASH_HEAD_TAILQ:
		*loffset = sizeof(TAILQ_HEAD(, foo));
		break;
	}

	switch (args->lock) {
	case HASH_LOCK_NONE:
		*hdrsize = *loffset;
		break;
	case HASH_LOCK_MTX:
		*hdrsize = *loffset + sizeof(struct mtx);
		break;
	case HASH_LOCK_RWLOCK:
		*hdrsize = *loffset + sizeof(struct rwlock);
		break;
	case HASH_LOCK_SX:
		*hdrsize = *loffset + sizeof(struct sx);
		break;
	case HASH_LOCK_RMLOCK:
		*hdrsize = *loffset + sizeof(struct rmlock);
		break;
	case HASH_LOCK_RMSLOCK:
		*hdrsize = *loffset + sizeof(struct rmslock);
		break;
	}

	if (args->hdrsize > 0) {
		MPASS(args->hdrsize >= *hdrsize);
		*hdrsize = args->hdrsize;
	} else
		args->hdrsize = *hdrsize;
}

void *
hashalloc(struct hashalloc_args *args)
{
	static const int primes[] = { 1, 13, 31, 61, 127, 251, 509, 761, 1021,
	    1531, 2039, 2557, 3067, 3583, 4093, 4603, 5119, 5623, 6143, 6653,
	    7159, 7673, 8191, 12281, 16381, 24571, 32749 };
	void *mem;
	size_t size, hdrsize, loffset;
	u_int i;

	MPASS(args->version == 0);
	MPASS(args->size > 0);

	switch (args->type) {
	case HASH_TYPE_POWER2:
		for (size = 1; size <= args->size; size <<= 1)
			continue;
		size >>= 1;
		break;
	case HASH_TYPE_PRIME:
		for (i = nitems(primes); args->size < primes[i]; i--)
			;
		size = primes[i];
		break;
	}

	hashalloc_sizes(args, &hdrsize, &loffset);

	mem = malloc(size * hdrsize, args->mtype, args->mflags);
	if (mem == NULL) {
		args->error = ENOMEM;
		return (NULL);
	}

	switch (args->lock) {
	case HASH_LOCK_NONE:
		break;
	case HASH_LOCK_MTX:
		MPASS(args->lname != NULL);
		if ((args->mflags & M_ZERO) == 0)
			args->lopts |= MTX_NEW;
		break;
	case HASH_LOCK_RWLOCK:
		MPASS(args->lname != NULL);
		if ((args->mflags & M_ZERO) == 0)
			args->lopts |= RW_NEW;
		break;
	case HASH_LOCK_SX:
		MPASS(args->lname != NULL);
		if ((args->mflags & M_ZERO) == 0)
			args->lopts |= SX_NEW;
		break;
	case HASH_LOCK_RMLOCK:
		MPASS(args->lname != NULL);
		if ((args->mflags & M_ZERO) == 0)
			args->lopts |= RM_NEW;
		break;
	case HASH_LOCK_RMSLOCK:
		MPASS(args->lname != NULL);
		break;
	}

	for (i = 0; i < size; i++) {
		void *slot;

		slot = (char *)mem + i * hdrsize;
		switch (args->head) {
		case HASH_HEAD_LIST:
			LIST_INIT((LIST_HEAD(, foo) *)slot);
			break;
		case HASH_HEAD_CK_LIST:
			CK_LIST_INIT((CK_LIST_HEAD(, foo) *)slot);
			break;
		case HASH_HEAD_SLIST:
			SLIST_INIT((SLIST_HEAD(, foo) *)slot);
			break;
		case HASH_HEAD_CK_SLIST:
			CK_SLIST_INIT((CK_SLIST_HEAD(, foo) *)slot);
			break;
		case HASH_HEAD_STAILQ:
			STAILQ_INIT((STAILQ_HEAD(, foo) *)slot);
			break;
		case HASH_HEAD_CK_STAILQ:
			CK_STAILQ_INIT((CK_STAILQ_HEAD(, foo) *)slot);
			break;
		case HASH_HEAD_TAILQ:
			TAILQ_INIT((TAILQ_HEAD(, foo) *)slot);
			break;
		}

		slot = (char *)slot + loffset;
		switch (args->lock) {
		case HASH_LOCK_NONE:
			break;
		case HASH_LOCK_MTX:
			mtx_init((struct mtx *)slot, args->lname, NULL,
			    args->lopts);
			break;
		case HASH_LOCK_RWLOCK:
			rw_init_flags((struct rwlock *)slot, args->lname,
			    args->lopts);
			break;
		case HASH_LOCK_SX:
			sx_init_flags((struct sx *)slot, args->lname,
			    args->lopts);
			break;
		case HASH_LOCK_RMLOCK:
			rm_init_flags((struct rmlock *)slot, args->lname,
			    args->lopts);
			break;
		case HASH_LOCK_RMSLOCK:
			rms_init((struct rmslock *)slot, args->lname);
			break;
		}

		if (args->ctor != NULL) {
			slot = (char *)mem + i * hdrsize;
			if ((args->error = args->ctor(slot)) != 0) {
				slot = (char *)slot + loffset;
				switch (args->lock) {
				case HASH_LOCK_NONE:
					break;
				case HASH_LOCK_MTX:
					mtx_destroy((struct mtx *)slot);
					break;
				case HASH_LOCK_RWLOCK:
					rw_destroy((struct rwlock *)slot);
					break;
				case HASH_LOCK_SX:
					sx_destroy((struct sx *)slot);
					break;
				case HASH_LOCK_RMLOCK:
					rm_destroy((struct rmlock *)slot);
					break;
				case HASH_LOCK_RMSLOCK:
					rms_destroy((struct rmslock *)slot);
					break;
				}
				args->size = i;
				hashfree(mem, args);
				return (NULL);
			}
		}
	}

	args->size = size;
	return (mem);
}

void
hashfree(void *mem, struct hashalloc_args *args)
{
	size_t hdrsize, loffset;

	if (__predict_false(mem == NULL))
		return;

	hashalloc_sizes(args, &hdrsize, &loffset);

	for (u_int i = 0; i < args->size; i++) {
#ifdef INVARIANTS
		static const char msg[] =
		    "%s: hashtbl %p not empty (malloc type %s)";
#endif
#define	HPASS(exp) KASSERT(exp, (msg, __func__, mem, args->mtype->ks_shortdesc))
		void *slot;

		slot = (char *)mem + i * hdrsize;
		if (args->dtor != NULL)
			args->dtor(slot);
		switch (args->head) {
		case HASH_HEAD_LIST:
			HPASS(LIST_EMPTY((LIST_HEAD(, foo) *)slot));
			break;
		case HASH_HEAD_CK_LIST:
			HPASS(CK_LIST_EMPTY((CK_LIST_HEAD(, foo) *)slot));
			break;
		case HASH_HEAD_SLIST:
			HPASS(SLIST_EMPTY((SLIST_HEAD(, foo) *)slot));
			break;
		case HASH_HEAD_CK_SLIST:
			HPASS(CK_SLIST_EMPTY((CK_SLIST_HEAD(, foo) *)slot));
			break;
		case HASH_HEAD_STAILQ:
			HPASS(STAILQ_EMPTY((STAILQ_HEAD(, foo) *)slot));
			break;
		case HASH_HEAD_CK_STAILQ:
			HPASS(CK_STAILQ_EMPTY((CK_STAILQ_HEAD(, foo) *)slot));
			break;
		case HASH_HEAD_TAILQ:
			HPASS(TAILQ_EMPTY((TAILQ_HEAD(, foo) *)slot));
			break;
		}
#undef HPASS

		slot = (char *)slot + loffset;
		switch (args->lock) {
		case HASH_LOCK_NONE:
			break;
		case HASH_LOCK_MTX:
			mtx_destroy((struct mtx *)slot);
			break;
		case HASH_LOCK_RWLOCK:
			rw_destroy((struct rwlock *)slot);
			break;
		case HASH_LOCK_SX:
			sx_destroy((struct sx *)slot);
			break;
		case HASH_LOCK_RMLOCK:
			rm_destroy((struct rmlock *)slot);
			break;
		case HASH_LOCK_RMSLOCK:
			rms_destroy((struct rmslock *)slot);
			break;
		}
	}

	free(mem, args->mtype);
}

static __inline int
hash_mflags(int flags)
{

	return ((flags & HASH_NOWAIT) ? M_NOWAIT : M_WAITOK);
}

/*
 * General routine to allocate a hash table with control of memory flags.
 */
void *
hashinit_flags(int elements, struct malloc_type *type, u_long *hashmask,
    int flags)
{
	struct hashalloc_args args = {
		.size = elements,
		.mtype = type,
		.mflags = hash_mflags(flags),
	};
	void *rv;

	rv = hashalloc(&args);
	if (rv != NULL)
		*hashmask = args.size - 1;
	return (rv);
}

/*
 * Allocate and initialize a hash table with default flag: may sleep.
 */
void *
hashinit(int elements, struct malloc_type *type, u_long *hashmask)
{

	return (hashinit_flags(elements, type, hashmask, HASH_WAITOK));
}

void
hashdestroy(void *vhashtbl, struct malloc_type *type, u_long hashmask)
{
	struct hashalloc_args args = {
		.size = hashmask + 1,
		.mtype = type,
	};

	hashfree(vhashtbl, &args);
}

/*
 * General routine to allocate a prime number sized hash table with control of
 * memory flags.
 */
void *
phashinit_flags(int elements, struct malloc_type *type, u_long *nentries, int flags)
{
	struct hashalloc_args args = {
		.size = elements,
		.mtype = type,
		.type = HASH_TYPE_PRIME,
		.mflags = hash_mflags(flags),
	};
	void *rv;

	rv = hashalloc(&args);
	if (rv != NULL)
		*nentries = args.size;
	return (rv);
}

/*
 * Allocate and initialize a prime number sized hash table with default flag:
 * may sleep.
 */
void *
phashinit(int elements, struct malloc_type *type, u_long *nentries)
{

	return (phashinit_flags(elements, type, nentries, HASH_WAITOK));
}
