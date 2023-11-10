/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002, 2003, 2004, 2005 Jeffrey Roberson <jeff@FreeBSD.org>
 * Copyright (c) 2004, 2005 Bosko Milekic <bmilekic@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

/*
 * uma_dbg.c	Debugging features for UMA users
 *
 */

#include <sys/cdefs.h>
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>
#include <vm/memguard.h>

#include <machine/stack.h>

static const u_long uma_junk = (u_long)0xdeadc0dedeadc0de;

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed,
 * prior to subsequent reallocation.
 *
 * Complies with standard ctor arg/return.  arg should be zone pointer or NULL.
 */
int
trash_ctor(void *mem, int size, void *arg, int flags)
{
	struct uma_zone *zone = arg;
	u_long *p = mem, *e;
	int off;

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(mem))
		return (0);
#endif

	e = p + size / sizeof(*p);
	for (; p < e; p++) {
		if (__predict_false(*p != uma_junk))
			goto dopanic;
	}
	return (0);

dopanic:
	off = (uintptr_t)p - (uintptr_t)mem;
	panic("Memory modified after free %p (%d, %s) + %d = %lx\n",
	    mem, size, zone ? zone->uz_name : "", off,  *p);
	return (0);
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard dtor arg/return
 */
void
trash_dtor(void *mem, int size, void *arg)
{
	u_long *p = mem, *e;

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(mem))
		return;
#endif

	e = p + size / sizeof(*p);
	for (; p < e; p++)
		*p = uma_junk;
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard init arg/return
 */
int
trash_init(void *mem, int size, int flags)
{
	trash_dtor(mem, size, NULL);
	return (0);
}

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed,
 * prior to freeing it back to available memory.
 *
 * Complies with standard fini arg/return
 */
void
trash_fini(void *mem, int size)
{
	(void)trash_ctor(mem, size, NULL, 0);
}

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed,
 * prior to subsequent reallocation.
 *
 * Complies with standard ctor arg/return.  arg should be zone pointer or NULL.
 */
int
mtrash_ctor(void *mem, int size, void *arg, int flags)
{
	struct uma_zone *zone = arg;
	u_long *p = mem, *e;
	struct malloc_type **ksp;
	int off, osize = size;

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(mem))
		return (0);
#endif

	size -= sizeof(struct malloc_type *);

	e = p + size / sizeof(*p);
	for (; p < e; p++) {
		if (__predict_false(*p != uma_junk))
			goto dopanic;
	}
	return (0);

dopanic:
	off = (uintptr_t)p - (uintptr_t)mem;
	ksp = (struct malloc_type **)mem;
	ksp += size / sizeof(struct malloc_type *);
	if (*ksp != NULL
#ifdef INKERNEL
	    && INKERNEL((uintptr_t)*ksp)
#endif
	    ) {
		/*
		 * If *ksp is corrupted we may be unable to panic clean,
		 * so print what we have reliably while we still can.
		 */
		printf("Memory modified after free %p (%d, %s, %p) + %d = %lx\n",
		    mem, osize, zone ? zone->uz_name : "", *ksp, off, *p);
		panic("Memory modified after free %p (%d, %s, %s) + %d = %lx\n",
		    mem, osize, zone ? zone->uz_name : "", (*ksp)->ks_shortdesc,
		    off, *p);
	} else {
		panic("Memory modified after free %p (%d, %s, %p) + %d = %lx\n",
		    mem, osize, zone ? zone->uz_name : "", *ksp, off, *p);
	}
	return (0);
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard dtor arg/return
 */
void
mtrash_dtor(void *mem, int size, void *arg)
{
	u_long *p = mem, *e;

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(mem))
		return;
#endif

	size -= sizeof(struct malloc_type *);

	e = p + size / sizeof(*p);
	for (; p < e; p++)
		*p = uma_junk;
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard init arg/return
 */
int
mtrash_init(void *mem, int size, int flags)
{
	struct malloc_type **ksp;

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(mem))
		return (0);
#endif

	mtrash_dtor(mem, size, NULL);

	ksp = (struct malloc_type **)mem;
	ksp += (size / sizeof(struct malloc_type *)) - 1;
	*ksp = NULL;
	return (0);
}

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed,
 * prior to freeing it back to available memory.
 *
 * Complies with standard fini arg/return
 */
void
mtrash_fini(void *mem, int size)
{
	(void)mtrash_ctor(mem, size, NULL, 0);
}
