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

#define ZONE_INTERRUPT 0x0001	/* If you need to allocate at int time */
#define ZONE_PANICFAIL 0x0002	/* panic if the zalloc fails */
#define ZONE_BOOT      0x0010	/* Internal flag used by zbootinit */

#include	<machine/lock.h>

typedef struct vm_zone {
	struct simplelock zlock;	/* lock for data structure */
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
	struct vm_zone	*znext;		/* list of zones for sysctl */
} *vm_zone_t;


void		zerror __P((int)) __dead2;
vm_zone_t	zinit __P((char *name, int size, int nentries, int flags,
			   int zalloc));
int		zinitna __P((vm_zone_t z, struct vm_object *obj, char *name,
			     int size, int nentries, int flags, int zalloc));
void *		zalloc __P((vm_zone_t z));
void		zfree __P((vm_zone_t z, void *item));
void *		zalloci __P((vm_zone_t z));
void		zfreei __P((vm_zone_t z, void *item));
void		zbootinit __P((vm_zone_t z, char *name, int size, void *item,
			       int nitems));
void *		_zget __P((vm_zone_t z));

#endif /* _SYS_ZONE_H */
