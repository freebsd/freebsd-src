/*
 * Copyright (c) 1997, 1998 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice immediately at the beginning of the file, without modification,
 *	this list of conditions, and the following disclaimer.
 * 2. Absolutely no warranty of function or purpose is made by the author
 *	John S. Dyson.
 *
 * $FreeBSD$
 */

#ifndef _SYS_ZONE_H

#define _SYS_ZONE_H

#define ZONE_INTERRUPT 1 /* Use this if you need to allocate at int time */
#define ZONE_BOOT 16	 /* This is an internal flag used by zbootinit */

#include	<sys/_lock.h>
#include	<sys/_mutex.h>

typedef struct vm_zone {
	struct mtx	zmtx;		/* lock for data structure */
	void		*zitems;	/* linked list of items */
	int		zfreecnt;	/* free entries */
	int		zfreemin;	/* minimum number of free entries */
	int		znalloc;	/* number of allocations */
	vm_offset_t	zkva;		/* Base kva of zone */
	int		zpagecount;	/* Total # of allocated pages */
	int		zpagemax;	/* Max address space */
	int		zmax;		/* Max number of entries allocated */
	int		ztotal;		/* Total entries allocated now */
	int		zsize;		/* size of each entry */
	int		zalloc;		/* hint for # of pages to alloc */
	int		zflags;		/* flags for zone */
	int		zallocflag;	/* flag for allocation */
	struct vm_object *zobj;		/* object to hold zone */
	char		*zname;		/* name for diags */
	/* NOTE: zent is protected by the subsystem lock, *not* by zmtx */
	SLIST_ENTRY(vm_zone) zent;	/* singly-linked list of zones */
} *vm_zone_t;


void		 vm_zone_init(void);
void		 vm_zone_init2(void);

int		 zinitna(vm_zone_t z, struct vm_object *obj, char *name,
                     int size, int nentries, int flags, int zalloc);
vm_zone_t	 zinit(char *name, int size, int nentries,
                     int flags, int zalloc);
void		 zbootinit(vm_zone_t z, char *name, int size,
                     void *item, int nitems);
void		*zalloc(vm_zone_t z);
void		 zfree(vm_zone_t z, void *item);

#endif /* _SYS_ZONE_H */
