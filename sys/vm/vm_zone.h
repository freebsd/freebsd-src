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
#include	<vm/uma.h>

typedef uma_zone_t vm_zone_t;

#if 0
static void		 vm_zone_init(void);
static void		 vm_zone_init2(void);

static vm_zone_t	 zinit(char *name, int size, int nentries,
                     int flags, int zalloc);
int		 zinitna(vm_zone_t z, struct vm_object *obj, char *name,
                     int size, int nentries, int flags, int zalloc);
void		 zbootinit(vm_zone_t z, char *name, int size,
                     void *item, int nitems);
static void		 zdestroy(vm_zone_t z);
static void		*zalloc(vm_zone_t z);
static void		 zfree(vm_zone_t z, void *item);
#endif

#define vm_zone_init2() uma_startup2()

#define zinit(name, size, nentries, flags, zalloc)		\
	uma_zcreate((name), (size), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE)
#define zdestroy()
#define zalloc(z) uma_zalloc((z), M_WAITOK)
#define zfree(z, item) uma_zfree((z), (item))
#endif /* _SYS_ZONE_H */
