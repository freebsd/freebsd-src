/*
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
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
 *
 * $FreeBSD$
 *
 */

/*
 * uma_dbg.c	Debugging features for UMA users
 *
 */


#include <sys/param.h>
#include <sys/systm.h>
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

static const u_int32_t uma_junk = 0xdeadc0de;

/*
 * Checks an item to make sure it hasn't been overwritten since freed.
 *
 * Complies with standard ctor arg/return
 *
 */
void
trash_ctor(void *mem, int size, void *arg)
{
	int cnt;
	u_int32_t *p;

	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		if (*p != uma_junk)
			panic("Memory modified after free %p(%d)\n",
			    mem, size);
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
	u_int32_t *p;

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
void
trash_init(void *mem, int size)
{
	trash_dtor(mem, size, NULL);
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
	trash_ctor(mem, size, NULL);
}

/*
 * Checks an item to make sure it hasn't been overwritten since freed.
 *
 * Complies with standard ctor arg/return
 *
 */
void
mtrash_ctor(void *mem, int size, void *arg)
{
	struct malloc_type **ksp;
	u_int32_t *p = mem;
	int cnt;

	size -= sizeof(struct malloc_type *);
	ksp = (struct malloc_type **)mem;
	ksp += size / sizeof(struct malloc_type *);
	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		if (*p != uma_junk) {
			printf("Memory modified after free %p(%d)\n",
			    mem, size);
			panic("Most recently used by %s\n", (*ksp == NULL)?
			    "none" : (*ksp)->ks_shortdesc);
		}
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
	u_int32_t *p;

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
void
mtrash_init(void *mem, int size)
{
	struct malloc_type **ksp;

	mtrash_dtor(mem, size, NULL);

	ksp = (struct malloc_type **)mem;
	ksp += (size / sizeof(struct malloc_type *)) - 1;
	*ksp = NULL;
}

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed.
 *
 * Complies with standard fini arg/return
 *
 */
void
mtrash_fini(void *mem, int size)
{
	mtrash_ctor(mem, size, NULL);
}

static uma_slab_t
uma_dbg_getslab(uma_zone_t zone, void *item)
{
	uma_slab_t slab;
	u_int8_t *mem;

	mem = (u_int8_t *)((unsigned long)item & (~UMA_SLAB_MASK));
	if (zone->uz_flags & UMA_ZFLAG_MALLOC) {
		slab = vtoslab((vm_offset_t)mem);
	} else if (zone->uz_flags & UMA_ZFLAG_HASH) {
		ZONE_LOCK(zone);
		slab = hash_sfind(&zone->uz_hash, mem);
		ZONE_UNLOCK(zone);
	} else {
		mem += zone->uz_pgoff;
		slab = (uma_slab_t)mem;
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
	int freei;

	if (slab == NULL) {
		slab = uma_dbg_getslab(zone, item);
		if (slab == NULL) 
			panic("uma: item %p did not belong to zone %s\n",
			    item, zone->uz_name);
	}

	freei = ((unsigned long)item - (unsigned long)slab->us_data)
	    / zone->uz_rsize;

	atomic_set_8(&slab->us_freelist[freei], 255);

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
	int freei;

	if (slab == NULL) {
		slab = uma_dbg_getslab(zone, item);
		if (slab == NULL) 
			panic("uma: Freed item %p did not belong to zone %s\n",
			    item, zone->uz_name);
	}

	freei = ((unsigned long)item - (unsigned long)slab->us_data)
	    / zone->uz_rsize;

	if (freei >= zone->uz_ipers)
		panic("zone: %s(%p) slab %p freelist %d out of range 0-%d\n",
		    zone->uz_name, zone, slab, freei, zone->uz_ipers-1);

	if (((freei * zone->uz_rsize) + slab->us_data) != item) {
		printf("zone: %s(%p) slab %p freed address %p unaligned.\n",
		    zone->uz_name, zone, slab, item);
		panic("should be %p\n",
		    (freei * zone->uz_rsize) + slab->us_data);
	}

	if (slab->us_freelist[freei] != 255) {
		printf("Slab at %p, freei %d = %d.\n",
		    slab, freei, slab->us_freelist[freei]);
		panic("Duplicate free of item %p from zone %p(%s)\n",
		    item, zone, zone->uz_name);
	}

	/*
	 * When this is actually linked into the slab this will change.
	 * Until then the count of valid slabs will make sure we don't
	 * accidentally follow this and assume it's a valid index.
	 */
	atomic_set_8(&slab->us_freelist[freei], 0);
}
