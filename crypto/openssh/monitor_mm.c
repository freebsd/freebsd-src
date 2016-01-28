/* $OpenBSD: monitor_mm.c,v 1.21 2015/02/06 23:21:59 millert Exp $ */
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 */

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "openbsd-compat/sys-tree.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"
#include "ssh.h"
#include "log.h"
#include "monitor_mm.h"

static int
mm_compare(struct mm_share *a, struct mm_share *b)
{
	ptrdiff_t diff = (char *)a->address - (char *)b->address;

	if (diff == 0)
		return (0);
	else if (diff < 0)
		return (-1);
	else
		return (1);
}

RB_GENERATE(mmtree, mm_share, next, mm_compare)

static struct mm_share *
mm_make_entry(struct mm_master *mm, struct mmtree *head,
    void *address, size_t size)
{
	struct mm_share *tmp, *tmp2;

	if (mm->mmalloc == NULL)
		tmp = xcalloc(1, sizeof(struct mm_share));
	else
		tmp = mm_xmalloc(mm->mmalloc, sizeof(struct mm_share));
	tmp->address = address;
	tmp->size = size;

	tmp2 = RB_INSERT(mmtree, head, tmp);
	if (tmp2 != NULL)
		fatal("mm_make_entry(%p): double address %p->%p(%zu)",
		    mm, tmp2, address, size);

	return (tmp);
}

/* Creates a shared memory area of a certain size */

struct mm_master *
mm_create(struct mm_master *mmalloc, size_t size)
{
	void *address;
	struct mm_master *mm;

	if (mmalloc == NULL)
		mm = xcalloc(1, sizeof(struct mm_master));
	else
		mm = mm_xmalloc(mmalloc, sizeof(struct mm_master));

	/*
	 * If the memory map has a mm_master it can be completely
	 * shared including authentication between the child
	 * and the client.
	 */
	mm->mmalloc = mmalloc;

	address = xmmap(size);
	if (address == (void *)MAP_FAILED)
		fatal("mmap(%zu): %s", size, strerror(errno));

	mm->address = address;
	mm->size = size;

	RB_INIT(&mm->rb_free);
	RB_INIT(&mm->rb_allocated);

	mm_make_entry(mm, &mm->rb_free, address, size);

	return (mm);
}

/* Frees either the allocated or the free list */

static void
mm_freelist(struct mm_master *mmalloc, struct mmtree *head)
{
	struct mm_share *mms, *next;

	for (mms = RB_ROOT(head); mms; mms = next) {
		next = RB_NEXT(mmtree, head, mms);
		RB_REMOVE(mmtree, head, mms);
		if (mmalloc == NULL)
			free(mms);
		else
			mm_free(mmalloc, mms);
	}
}

/* Destroys a memory mapped area */

void
mm_destroy(struct mm_master *mm)
{
	mm_freelist(mm->mmalloc, &mm->rb_free);
	mm_freelist(mm->mmalloc, &mm->rb_allocated);

#ifdef HAVE_MMAP
	if (munmap(mm->address, mm->size) == -1)
		fatal("munmap(%p, %zu): %s", mm->address, mm->size,
		    strerror(errno));
#else
	fatal("%s: UsePrivilegeSeparation=yes and Compression=yes not supported",
	    __func__);
#endif
	if (mm->mmalloc == NULL)
		free(mm);
	else
		mm_free(mm->mmalloc, mm);
}

void *
mm_xmalloc(struct mm_master *mm, size_t size)
{
	void *address;

	address = mm_malloc(mm, size);
	if (address == NULL)
		fatal("%s: mm_malloc(%zu)", __func__, size);
	memset(address, 0, size);
	return (address);
}


/* Allocates data from a memory mapped area */

void *
mm_malloc(struct mm_master *mm, size_t size)
{
	struct mm_share *mms, *tmp;

	if (size == 0)
		fatal("mm_malloc: try to allocate 0 space");
	if (size > SIZE_MAX - MM_MINSIZE + 1)
		fatal("mm_malloc: size too big");

	size = ((size + (MM_MINSIZE - 1)) / MM_MINSIZE) * MM_MINSIZE;

	RB_FOREACH(mms, mmtree, &mm->rb_free) {
		if (mms->size >= size)
			break;
	}

	if (mms == NULL)
		return (NULL);

	/* Debug */
	memset(mms->address, 0xd0, size);

	tmp = mm_make_entry(mm, &mm->rb_allocated, mms->address, size);

	/* Does not change order in RB tree */
	mms->size -= size;
	mms->address = (char *)mms->address + size;

	if (mms->size == 0) {
		RB_REMOVE(mmtree, &mm->rb_free, mms);
		if (mm->mmalloc == NULL)
			free(mms);
		else
			mm_free(mm->mmalloc, mms);
	}

	return (tmp->address);
}

/* Frees memory in a memory mapped area */

void
mm_free(struct mm_master *mm, void *address)
{
	struct mm_share *mms, *prev, tmp;

	tmp.address = address;
	mms = RB_FIND(mmtree, &mm->rb_allocated, &tmp);
	if (mms == NULL)
		fatal("mm_free(%p): can not find %p", mm, address);

	/* Debug */
	memset(mms->address, 0xd0, mms->size);

	/* Remove from allocated list and insert in free list */
	RB_REMOVE(mmtree, &mm->rb_allocated, mms);
	if (RB_INSERT(mmtree, &mm->rb_free, mms) != NULL)
		fatal("mm_free(%p): double address %p", mm, address);

	/* Find previous entry */
	prev = mms;
	if (RB_LEFT(prev, next)) {
		prev = RB_LEFT(prev, next);
		while (RB_RIGHT(prev, next))
			prev = RB_RIGHT(prev, next);
	} else {
		if (RB_PARENT(prev, next) &&
		    (prev == RB_RIGHT(RB_PARENT(prev, next), next)))
			prev = RB_PARENT(prev, next);
		else {
			while (RB_PARENT(prev, next) &&
			    (prev == RB_LEFT(RB_PARENT(prev, next), next)))
				prev = RB_PARENT(prev, next);
			prev = RB_PARENT(prev, next);
		}
	}

	/* Check if range does not overlap */
	if (prev != NULL && MM_ADDRESS_END(prev) > address)
		fatal("mm_free: memory corruption: %p(%zu) > %p",
		    prev->address, prev->size, address);

	/* See if we can merge backwards */
	if (prev != NULL && MM_ADDRESS_END(prev) == address) {
		prev->size += mms->size;
		RB_REMOVE(mmtree, &mm->rb_free, mms);
		if (mm->mmalloc == NULL)
			free(mms);
		else
			mm_free(mm->mmalloc, mms);
	} else
		prev = mms;

	if (prev == NULL)
		return;

	/* Check if we can merge forwards */
	mms = RB_NEXT(mmtree, &mm->rb_free, prev);
	if (mms == NULL)
		return;

	if (MM_ADDRESS_END(prev) > mms->address)
		fatal("mm_free: memory corruption: %p < %p(%zu)",
		    mms->address, prev->address, prev->size);
	if (MM_ADDRESS_END(prev) != mms->address)
		return;

	prev->size += mms->size;
	RB_REMOVE(mmtree, &mm->rb_free, mms);

	if (mm->mmalloc == NULL)
		free(mms);
	else
		mm_free(mm->mmalloc, mms);
}

static void
mm_sync_list(struct mmtree *oldtree, struct mmtree *newtree,
    struct mm_master *mm, struct mm_master *mmold)
{
	struct mm_master *mmalloc = mm->mmalloc;
	struct mm_share *mms, *new;

	/* Sync free list */
	RB_FOREACH(mms, mmtree, oldtree) {
		/* Check the values */
		mm_memvalid(mmold, mms, sizeof(struct mm_share));
		mm_memvalid(mm, mms->address, mms->size);

		new = mm_xmalloc(mmalloc, sizeof(struct mm_share));
		memcpy(new, mms, sizeof(struct mm_share));
		RB_INSERT(mmtree, newtree, new);
	}
}

void
mm_share_sync(struct mm_master **pmm, struct mm_master **pmmalloc)
{
	struct mm_master *mm;
	struct mm_master *mmalloc;
	struct mm_master *mmold;
	struct mmtree rb_free, rb_allocated;

	debug3("%s: Share sync", __func__);

	mm = *pmm;
	mmold = mm->mmalloc;
	mm_memvalid(mmold, mm, sizeof(*mm));

	mmalloc = mm_create(NULL, mm->size);
	mm = mm_xmalloc(mmalloc, sizeof(struct mm_master));
	memcpy(mm, *pmm, sizeof(struct mm_master));
	mm->mmalloc = mmalloc;

	rb_free = mm->rb_free;
	rb_allocated = mm->rb_allocated;

	RB_INIT(&mm->rb_free);
	RB_INIT(&mm->rb_allocated);

	mm_sync_list(&rb_free, &mm->rb_free, mm, mmold);
	mm_sync_list(&rb_allocated, &mm->rb_allocated, mm, mmold);

	mm_destroy(mmold);

	*pmm = mm;
	*pmmalloc = mmalloc;

	debug3("%s: Share sync end", __func__);
}

void
mm_memvalid(struct mm_master *mm, void *address, size_t size)
{
	void *end = (char *)address + size;

	if (address < mm->address)
		fatal("mm_memvalid: address too small: %p", address);
	if (end < address)
		fatal("mm_memvalid: end < address: %p < %p", end, address);
	if (end > MM_ADDRESS_END(mm))
		fatal("mm_memvalid: address too large: %p", address);
}
