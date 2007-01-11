/*	$FreeBSD: src/contrib/ipfilter/lib/kmemcpywrap.c,v 1.2 2005/04/25 18:20:12 darrenr Exp $	*/

#include "ipf.h"
#include "kmem.h"

int kmemcpywrap(from, to, size)
void *from, *to;
size_t size;
{
	int ret;

	ret = kmemcpy((caddr_t)to, (u_long)from, size);
	return ret;
}

