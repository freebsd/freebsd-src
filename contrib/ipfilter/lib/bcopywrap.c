/*	$FreeBSD: src/contrib/ipfilter/lib/bcopywrap.c,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *   
 * $Id: bcopywrap.c,v 1.1.4.1 2006/06/16 17:20:56 darrenr Exp $
 */  

#include "ipf.h"

int bcopywrap(from, to, size)
void *from, *to;
size_t size;
{
	bcopy((caddr_t)from, (caddr_t)to, size);
	return 0;
}

