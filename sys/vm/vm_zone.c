/*
 * Copyright (c) 1997 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is allowed if this notation is included.
 * 5. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $Id$
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/lock.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>
#include <vm/vm_pageout.h>

/*
 * This file comprises a very simple zone allocator.  This is used
 * in lieu of the malloc allocator, where needed or more optimal.
 *
 * Note that the initial implementation of this had coloring, and
 * absolutely no improvement (actually perf degradation) occurred.
 *
 * _zinit, zinit, _zbootinit are the initialization routines.
 * zalloc, zfree, are the interrupt/lock unsafe allocation/free routines.
 * zalloci, zfreei, are the interrupt/lock safe allocation/free routines.
 */

int
_zinit(vm_zone_t z, vm_object_t obj, char *name, int size,
	int nentries, int flags, int zalloc) {
	int totsize;

	if ((z->zflags & ZONE_BOOT) == 0) {

		z->zsize = size;
		simple_lock_init(&z->zlock);
		z->zfreecnt = 0;
		z->zname = name;

	}

	z->zflags |= flags;

	/*
	 * If we cannot wait, allocate KVA space up front, and we will fill
	 * in pages as needed.
	 */
	if ((z->zflags & ZONE_WAIT) == 0) {

		totsize = round_page(z->zsize * nentries);

		z->zkva = kmem_alloc_pageable(kernel_map, totsize);
		if (z->zkva == 0) {
			return 0;
		}

		z->zpagemax = totsize / PAGE_SIZE;
		if (obj == NULL) {
			z->zobj = vm_object_allocate(OBJT_DEFAULT, z->zpagemax);
		} else {
			z->zobj = obj;
			_vm_object_allocate(OBJT_DEFAULT, z->zpagemax, obj);
		}
	}

	if ( z->zsize > PAGE_SIZE)
		z->zfreemin = 1;
	else
		z->zfreemin = PAGE_SIZE / z->zsize;

	z->zallocflag = VM_ALLOC_SYSTEM;
	if (z->zflags & ZONE_INTERRUPT)
		z->zallocflag = VM_ALLOC_INTERRUPT;

	z->zpagecount = 0;
	if (zalloc)
		z->zalloc = zalloc;
	else
		z->zalloc = 1;

	return 1;
}

vm_zone_t
zinit(char *name, int size, int nentries, int flags, int zalloc) {
	vm_zone_t z;
	z = (vm_zone_t) malloc(sizeof (struct vm_zone), M_ZONE, M_NOWAIT);
	if (z == NULL)
		return NULL;

	if (_zinit(z, NULL, name, size, nentries, flags, zalloc) == 0) {
		free(z, M_ZONE);
		return NULL;
	}

	return z;
}

void
_zbootinit(vm_zone_t z, char *name, int size, void *item, int nitems) {

	int i;

	z->zname = name;
	z->zsize = size;
	z->zpagemax = 0;
	z->zobj = NULL;
	z->zflags = ZONE_BOOT;
	z->zfreemin = 0;
	z->zallocflag = 0;
	z->zpagecount = 0;
	z->zalloc = 0;
	simple_lock_init(&z->zlock);

	for (i = 0; i < nitems; i++) {
		* (void **) item = z->zitems;
		z->zitems = item;
		++z->zfreecnt;
		(char *) item += z->zsize;
	}
}

static inline int
zlock(vm_zone_t z) {
	int s;
	s = splhigh();
	simple_lock(&z->zlock);
	return s;
}

static inline void
zunlock(vm_zone_t z, int s) {
	simple_unlock(&z->zlock);
	splx(s);
}

void *
zalloci(vm_zone_t z) {
	int s;
	void *item;

	s = zlock(z);
	item = zalloc(z);
	zunlock(z, s);
	return item;
}

void
zfreei(vm_zone_t z, void *item) {
	int s;

	s = zlock(z);
	zfree(z, item);
	zunlock(z, s);
	return;
}

void *
zget(vm_zone_t z, int s) {
	int i;
	vm_page_t m;
	int nitems;
	void *item, *litem;

	if ((z->zflags & ZONE_WAIT) == 0) {
		item = (char *) z->zkva + z->zpagecount * PAGE_SIZE;
		for( i = 0; ((i < z->zalloc) && (z->zpagecount < z->zpagemax)); i++) {

			m = vm_page_alloc( z->zobj, z->zpagecount, z->zallocflag);
			if (m == NULL) {
				break;
			}

			pmap_kenter(z->zkva + z->zpagecount * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
			++z->zpagecount;
		}
		nitems = (i * PAGE_SIZE) / z->zsize;
	} else {
		/*
		 * We can wait, so just do normal kernel map allocation
		 */
		item = (void *) kmem_alloc(kernel_map, z->zalloc * PAGE_SIZE);
		nitems = (z->zalloc * PAGE_SIZE) / z->zsize;
	}

	/*
	 * Save one for immediate allocation
	 */
	nitems -= 1;
	for (i = 0; i < nitems; i++) {
		* (void **) item = z->zitems;
		z->zitems = item;
		(char *) item += z->zsize;
		++z->zfreecnt;
	}
		 
	return item;
}

