/*	$FreeBSD$	*/

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

