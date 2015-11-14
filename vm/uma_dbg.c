/*-
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
__FBSDID("$FreeBSD$");

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

static const uint32_t uma_junk = 0xdeadc0de;

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed,
 * prior to subsequent reallocation.
 *
 * Complies with standard ctor arg/return
 *
 */
int
trash_ctor(void *mem, int size, void *arg, int flags)
{
	int cnt;
	uint32_t *p;

	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		if (*p != uma_junk) {
#ifdef INVARIANTS
			panic("Memory modified after free %p(%d) val=%x @ %p\n",
			    mem, size, *p, p);
#else
			printf("Memory modified after free %p(%d) val=%x @ %p\n",
			    mem, size, *p, p);
#endif
			return (0);
		}
	return (0);
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard dtor arg/return
 *
 */
void
trash_dtor(void *mem, int size, void *arg)
{
	int cnt;
	uint32_t *p;

	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		*p = uma_junk;
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard init arg/return
 *
 */
int
trash_init(void *mem, int size, int flags)
{
	trash_dtor(mem, size, NULL);
	return (0);
}

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed.
 *
 * Complies with standard fini arg/return
 *
 */
void
trash_fini(void *mem, int size)
{
	(void)trash_ctor(mem, size, NULL, 0);
}

int
mtrash_ctor(void *mem, int size, void *arg, int flags)
{
	struct malloc_type **ksp;
	uint32_t *p = mem;
	int cnt;

	size -= sizeof(struct malloc_type *);
	ksp = (struct malloc_type **)mem;
	ksp += size / sizeof(struct malloc_type *);
	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		if (*p != uma_junk) {
			printf("Memory modified after free %p(%d) val=%x @ %p\n",
			    mem, size, *p, p);
			panic("Most recently used by %s\n", (*ksp == NULL)?
			    "none" : (*ksp)->ks_shortdesc);
		}
	return (0);
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard dtor arg/return
 *
 */
void
mtrash_dtor(void *mem, int size, void *arg)
{
	int cnt;
	uint32_t *p;

	size -= sizeof(struct malloc_type *);
	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		*p = uma_junk;
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard init arg/return
 *
 */
int
mtrash_init(void *mem, int size, int flags)
{
	struct malloc_type **ksp;

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
 *
 */
void
mtrash_fini(void *mem, int size)
{
	(void)mtrash_ctor(mem, size, NULL, 0);
}

#ifdef INVARIANTS
static uma_slab_t
uma_dbg_getslab(uma_zone_t zone, void *item)
{
	uma_slab_t slab;
	uma_keg_t keg;
	uint8_t *mem;

	mem = (uint8_t *)((uintptr_t)item & (~UMA_SLAB_MASK));
	if (zone->uz_flags & UMA_ZONE_VTOSLAB) {
		slab = vtoslab((vm_offset_t)mem);
	} else {
		/*
		 * It is safe to return the slab here even though the
		 * zone is unlocked because the item's allocation state
		 * essentially holds a reference.
		 */
		ZONE_LOCK(zone);
		keg = LIST_FIRST(&zone->uz_kegs)->kl_keg;
		if (keg->uk_flags & UMA_ZONE_HASH)
			slab = hash_sfind(&keg->uk_hash, mem);
		else
			slab = (uma_slab_t)(mem + keg->uk_pgoff);
		ZONE_UNLOCK(zone);
	}

	return (slab);
}

/*
 * Set up the slab's freei data such that uma_dbg_free can function.
 *
 */
void
uma_dbg_alloc(uma_zone_t zone, uma_slab_t slab, void *item)
{
	uma_keg_t keg;
	int freei;

	if (zone_first_keg(zone) == NULL)
		return;
	if (slab == NULL) {
		slab = uma_dbg_getslab(zone, item);
		if (slab == NULL) 
			panic("uma: item %p did not belong to zone %s\n",
			    item, zone->uz_name);
	}
	keg = slab->us_keg;
	freei = ((uintptr_t)item - (uintptr_t)slab->us_data) / keg->uk_rsize;

	if (BIT_ISSET(SLAB_SETSIZE, freei, &slab->us_debugfree))
		panic("Duplicate alloc of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);
	BIT_SET_ATOMIC(SLAB_SETSIZE, freei, &slab->us_debugfree);

	return;
}

/*
 * Verifies freed addresses.  Checks for alignment, valid slab membership
 * and duplicate frees.
 *
 */
void
uma_dbg_free(uma_zone_t zone, uma_slab_t slab, void *item)
{
	uma_keg_t keg;
	int freei;

	if (zone_first_keg(zone) == NULL)
		return;
	if (slab == NULL) {
		slab = uma_dbg_getslab(zone, item);
		if (slab == NULL) 
			panic("uma: Freed item %p did not belong to zone %s\n",
			    item, zone->uz_name);
	}
	keg = slab->us_keg;
	freei = ((uintptr_t)item - (uintptr_t)slab->us_data) / keg->uk_rsize;

	if (freei >= keg->uk_ipers)
		panic("Invalid free of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);

	if (((freei * keg->uk_rsize) + slab->us_data) != item) 
		panic("Unaligned free of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);

	if (!BIT_ISSET(SLAB_SETSIZE, freei, &slab->us_debugfree))
		panic("Duplicate free of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);

	BIT_CLR_ATOMIC(SLAB_SETSIZE, freei, &slab->us_debugfree);
}

#endif /* INVARIANTS */
