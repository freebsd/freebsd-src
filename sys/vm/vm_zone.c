/*
 * Copyright (c) 1997 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice immediately at the beginning of the file, without modification,
 *	this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *	John S. Dyson.
 * 4. This work was done expressly for inclusion into FreeBSD.  Other use
 *	is allowed if this notation is included.
 * 5. Modifications may be freely made to this file if the above conditions
 *	are met.
 *
 * $Id: vm_zone.c,v 1.11 1997/12/05 19:55:52 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

static MALLOC_DEFINE(M_ZONE, "ZONE", "Zone header");

/*
 * This file comprises a very simple zone allocator.  This is used
 * in lieu of the malloc allocator, where needed or more optimal.
 *
 * Note that the initial implementation of this had coloring, and
 * absolutely no improvement (actually perf degradation) occurred.
 *
 * zinitna, zinit, zbootinit are the initialization routines.
 * zalloc, zfree, are the interrupt/lock unsafe allocation/free routines.
 * zalloci, zfreei, are the interrupt/lock safe allocation/free routines.
 */

struct vm_zone *zlist;
int sysctl_vm_zone SYSCTL_HANDLER_ARGS;

/*
 * Create a zone, but don't allocate the zone structure.  If the
 * zone had been previously created by the zone boot code, initialize
 * various parts of the zone code.
 *
 * If waits are not allowed during allocation (e.g. during interrupt
 * code), a-priori allocate the kernel virtual space, and allocate
 * only pages when needed.
 *
 * Arguments:
 * z		pointer to zone structure.
 * obj		pointer to VM object (opt).
 * name		name of zone.
 * size		size of zone entries.
 * nentries	number of zone entries allocated (only ZONE_INTERRUPT.)
 * flags	ZONE_INTERRUPT -- items can be allocated at interrupt time.
 * zalloc	number of pages allocated when memory is needed.
 *
 * Note that when using ZONE_INTERRUPT, the size of the zone is limited
 * by the nentries argument.  The size of the memory allocatable is
 * unlimited if ZONE_INTERRUPT is not set.
 *
 */
int
zinitna(vm_zone_t z, vm_object_t obj, char *name, int size,
	int nentries, int flags, int zalloc)
{
	int totsize;

	if ((z->zflags & ZONE_BOOT) == 0) {
		z->zsize = (size + ZONE_ROUNDING - 1) & ~(ZONE_ROUNDING - 1);
		simple_lock_init(&z->zlock);
		z->zfreecnt = 0;
		z->ztotal = 0;
		z->zmax = 0;
		z->zname = name;
		z->znalloc = 0;
		z->zitems = NULL;

		if (zlist == 0) {
			zlist = z;
		} else {
			z->znext = zlist;
			zlist = z;
		}
	}

	z->zflags |= flags;

	/*
	 * If we cannot wait, allocate KVA space up front, and we will fill
	 * in pages as needed.
	 */
	if (z->zflags & ZONE_INTERRUPT) {

		totsize = round_page(z->zsize * nentries);

		z->zkva = kmem_alloc_pageable(kernel_map, totsize);
		if (z->zkva == 0)
			return 0;

		z->zpagemax = totsize / PAGE_SIZE;
		if (obj == NULL) {
			z->zobj = vm_object_allocate(OBJT_DEFAULT, z->zpagemax);
		} else {
			z->zobj = obj;
			_vm_object_allocate(OBJT_DEFAULT, z->zpagemax, obj);
		}
		z->zallocflag = VM_ALLOC_INTERRUPT;
		z->zmax += nentries;
	} else {
		z->zallocflag = VM_ALLOC_SYSTEM;
		z->zmax = 0;
	}


	if (z->zsize > PAGE_SIZE)
		z->zfreemin = 1;
	else
		z->zfreemin = PAGE_SIZE / z->zsize;

	z->zpagecount = 0;
	if (zalloc)
		z->zalloc = zalloc;
	else
		z->zalloc = 1;

	return 1;
}

/*
 * Subroutine same as zinitna, except zone data structure is allocated
 * automatically by malloc.  This routine should normally be used, except
 * in certain tricky startup conditions in the VM system -- then
 * zbootinit and zinitna can be used.  Zinit is the standard zone
 * initialization call.
 */
vm_zone_t
zinit(char *name, int size, int nentries, int flags, int zalloc)
{
	vm_zone_t z;

	z = (vm_zone_t) malloc(sizeof (struct vm_zone), M_ZONE, M_NOWAIT);
	if (z == NULL)
		return NULL;

	z->zflags = 0;
	if (zinitna(z, NULL, name, size, nentries, flags, zalloc) == 0) {
		free(z, M_ZONE);
		return NULL;
	}

	return z;
}

/*
 * Initialize a zone before the system is fully up.  This routine should
 * only be called before full VM startup.
 */
void
zbootinit(vm_zone_t z, char *name, int size, void *item, int nitems)
{
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
	z->znalloc = 0;
	simple_lock_init(&z->zlock);

	z->zitems = NULL;
	for (i = 0; i < nitems; i++) {
		((void **) item)[0] = z->zitems;
#if defined(DIAGNOSTIC)
		((void **) item)[1] = (void *) ZENTRY_FREE;
#endif
		z->zitems = item;
		(char *) item += z->zsize;
	}
	z->zfreecnt = nitems;
	z->zmax = nitems;
	z->ztotal = nitems;

	if (zlist == 0) {
		zlist = z;
	} else {
		z->znext = zlist;
		zlist = z;
	}
}

/*
 * Zone critical region locks.
 */
static inline int
zlock(vm_zone_t z)
{
	int s;

	s = splhigh();
	simple_lock(&z->zlock);
	return s;
}

static inline void
zunlock(vm_zone_t z, int s)
{
	simple_unlock(&z->zlock);
	splx(s);
}

/*
 * void *zalloc(vm_zone_t zone) --
 *	Returns an item from a specified zone.
 *
 * void zfree(vm_zone_t zone, void *item) --
 *  Frees an item back to a specified zone.
 *
 * void *zalloci(vm_zone_t zone) --
 *	Returns an item from a specified zone, interrupt safe.
 *
 * void zfreei(vm_zone_t zone, void *item) --
 *  Frees an item back to a specified zone, interrupt safe.
 *
 */

/*
 * Zone allocator/deallocator.  These are interrupt / (or potentially SMP)
 * safe.  The raw zalloc/zfree routines are in the vm_zone header file,
 * and are not interrupt safe, but are fast.
 */
void *
zalloci(vm_zone_t z)
{
	int s;
	void *item;

	s = zlock(z);
	item = _zalloc(z);
	zunlock(z, s);
	return item;
}

void
zfreei(vm_zone_t z, void *item)
{
	int s;

	s = zlock(z);
	_zfree(z, item);
	zunlock(z, s);
	return;
}

/*
 * Internal zone routine.  Not to be called from external (non vm_zone) code.
 */
void *
_zget(vm_zone_t z)
{
	int i;
	vm_page_t m;
	int nitems, nbytes;
	void *item;

	if (z == NULL)
		panic("zget: null zone");

	if (z->zflags & ZONE_INTERRUPT) {
		item = (char *) z->zkva + z->zpagecount * PAGE_SIZE;
		for (i = 0; ((i < z->zalloc) && (z->zpagecount < z->zpagemax));
		     i++) {

			m = vm_page_alloc(z->zobj, z->zpagecount,
					  z->zallocflag);
			if (m == NULL)
				break;

			pmap_kenter(z->zkva + z->zpagecount * PAGE_SIZE,
				    VM_PAGE_TO_PHYS(m));
			z->zpagecount++;
		}
		nitems = (i * PAGE_SIZE) / z->zsize;
	} else {
		nbytes = z->zalloc * PAGE_SIZE;
		/*
		 * We can wait, so just do normal kernel map allocation
		 */
		item = (void *) kmem_alloc(kernel_map, nbytes);

#if 0
		if (z->zname)
			printf("zalloc: %s, %d (0x%x --> 0x%x)\n",
				z->zname, z->zalloc, item,
				(char *)item + nbytes);
		else
			printf("zalloc: XXX(%d), %d (0x%x --> 0x%x)\n",
				z->zsize, z->zalloc, item,
				(char *)item + nbytes);

		for (i = 0; i < nbytes; i += PAGE_SIZE)
			printf("(%x, %x)", (char *) item + i,
			       pmap_kextract((char *) item + i));
		printf("\n");
#endif
		nitems = nbytes / z->zsize;
	}
	z->ztotal += nitems;

	/*
	 * Save one for immediate allocation
	 */
	if (nitems != 0) {
		nitems -= 1;
		for (i = 0; i < nitems; i++) {
			((void **) item)[0] = z->zitems;
#if defined(DIAGNOSTIC)
			((void **) item)[1] = (void *) ZENTRY_FREE;
#endif
			z->zitems = item;
			(char *) item += z->zsize;
		}
		z->zfreecnt += nitems;
	} else if (z->zfreecnt > 0) {
		item = z->zitems;
		z->zitems = ((void **) item)[0];
#if defined(DIAGNOSTIC)
		if (((void **) item)[1] != (void *) ZENTRY_FREE)
			zerror(ZONE_ERROR_NOTFREE);
		((void **) item)[1] = 0;
#endif
		z->zfreecnt--;
	} else {
		item = NULL;
	}

	return item;
}

int
sysctl_vm_zone SYSCTL_HANDLER_ARGS
{
	int error=0;
	vm_zone_t curzone, nextzone;
	char tmpbuf[128];
	char tmpname[14];

	sprintf(tmpbuf, "\nITEM            SIZE     LIMIT    USED    FREE  REQUESTS\n");
	error = SYSCTL_OUT(req, tmpbuf, strlen(tmpbuf));
	if (error)
		return (error);

	for (curzone = zlist; curzone; curzone = nextzone) {
		int i;
		int len;
		int offset;

		nextzone = curzone->znext;
		len = strlen(curzone->zname);
		if (len >= (sizeof(tmpname) - 1))
			len = (sizeof(tmpname) - 1);
		for(i = 0; i < sizeof(tmpname) - 1; i++)
			tmpname[i] = ' ';
		tmpname[i] = 0;
		memcpy(tmpname, curzone->zname, len);
		tmpname[len] = ':';
		offset = 0;
		if (curzone == zlist) {
			offset = 1;
			tmpbuf[0] = '\n';
		}

		sprintf(tmpbuf + offset,
			"%s %6.6u, %8.8u, %6.6u, %6.6u, %8.8u\n",
			tmpname, curzone->zsize, curzone->zmax,
			(curzone->ztotal - curzone->zfreecnt),
			curzone->zfreecnt, curzone->znalloc);

		len = strlen((char *)tmpbuf);
		if (nextzone == NULL)
			tmpbuf[len - 1] = 0;

		error = SYSCTL_OUT(req, tmpbuf, len);

		if (error)
			return (error);
	}
	return (0);
}

#if defined(DIAGNOSTIC)
void
zerror(int error)
{
	char *msg;

	switch (error) {
	case ZONE_ERROR_INVALID:
		msg = "zone: invalid zone";
		break;
	case ZONE_ERROR_NOTFREE:
		msg = "zone: entry not free";
		break;
	case ZONE_ERROR_ALREADYFREE:
		msg = "zone: freeing free entry";
		break;
	default:
		msg = "zone: invalid error";
		break;
	}
	panic(msg);
}
#endif

SYSCTL_OID(_kern, OID_AUTO, zone, CTLTYPE_STRING|CTLFLAG_RD, \
	NULL, 0, sysctl_vm_zone, "A", "Zone Info");
