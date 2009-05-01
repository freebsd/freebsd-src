/*	$FreeBSD: src/contrib/ipfilter/lib/printbuf.c,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $	*/

/*
 * Copyright (C) 2000-2004 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printbuf.c,v 1.5.4.2 2006/06/16 17:21:10 darrenr Exp $
 */

#include <ctype.h>

#include "ipf.h"


void printbuf(buf, len, zend)
char *buf;
int len, zend;
{
	char *s, c;
	int i;

	for (s = buf, i = len; i; i--) {
		c = *s++;
		if (ISPRINT(c))
			putchar(c);
		else
			printf("\\%03o", c);
		if ((c == '\0') && zend)
			break;
	}
}
