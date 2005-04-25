/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: make_range.c,v 1.2 2002/05/18 07:27:52 darrenr Exp
 */
#include "ipf.h"


alist_t	*make_range(not, a1, a2)
int not;
struct in_addr a1, a2;
{
	alist_t *a;

	a = (alist_t *)calloc(1, sizeof(*a));
	if (a != NULL) {
		a->al_1 = a1.s_addr;
		a->al_2 = a2.s_addr;
		a->al_not = not;
	}
	return a;
}
