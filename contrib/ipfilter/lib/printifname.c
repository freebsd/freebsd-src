/*	$FreeBSD: src/contrib/ipfilter/lib/printifname.c,v 1.4.14.1 2010/12/21 17:10:29 kensmith Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printifname.c,v 1.2.4.1 2006/06/16 17:21:12 darrenr Exp $
 */

#include "ipf.h"

void printifname(format, name, ifp)
char *format, *name;
void *ifp;
{
	printf("%s%s", format, name);
	if ((ifp == NULL) && strcmp(name, "-") && strcmp(name, "*"))
		printf("(!)");
}
