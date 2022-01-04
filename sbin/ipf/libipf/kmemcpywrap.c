/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"
#include "kmem.h"

int
kmemcpywrap(void *from, void *to, size_t size)
{
	int ret;

	ret = kmemcpy((caddr_t)to, (u_long)from, size);
	return (ret);
}

