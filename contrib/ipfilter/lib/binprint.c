/*	$FreeBSD: src/contrib/ipfilter/lib/binprint.c,v 1.2 2005/04/25 18:20:12 darrenr Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: binprint.c,v 1.8 2002/05/14 15:18:56 darrenr Exp
 */

#include "ipf.h"


void binprint(ptr, size)
void *ptr;
size_t size;
{
	u_char *s;
	int i, j;

	for (i = size, j = 0, s = (u_char *)ptr; i; i--, s++) {
		j++;
		printf("%02x ", *s);
		if (j == 16) {
			printf("\n");
			j = 0;
		}
	}
	putchar('\n');
	(void)fflush(stdout);
}
