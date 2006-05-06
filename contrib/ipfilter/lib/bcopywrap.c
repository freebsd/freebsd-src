/*	$FreeBSD: src/contrib/ipfilter/lib/bcopywrap.c,v 1.2 2005/04/25 18:20:12 darrenr Exp $	*/

#include "ipf.h"

int bcopywrap(from, to, size)
void *from, *to;
size_t size;
{
	bcopy((caddr_t)from, (caddr_t)to, size);
	return 0;
}

