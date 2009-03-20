/*
 * Copyright (c) 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/Attic/savestr.c,v 1.6 2000/07/11 00:49:02 assar Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "savestr.h"

/* A replacement for strdup() that cuts down on malloc() overhead */
char *
savestr(register const char *str)
{
	register u_int size;
	register char *p;
	static char *strptr = NULL;
	static u_int strsize = 0;

	size = strlen(str) + 1;
	if (size > strsize) {
		strsize = 1024;
		if (strsize < size)
			strsize = size;
		strptr = (char *)malloc(strsize);
		if (strptr == NULL) {
			fprintf(stderr, "savestr: malloc\n");
			exit(1);
		}
	}
	(void)strcpy(strptr, str);
	p = strptr;
	strptr += size;
	strsize -= size;
	return (p);
}
