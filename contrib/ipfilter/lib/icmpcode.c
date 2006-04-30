/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: icmpcode.c,v 1.7.2.1 2004/12/09 19:41:20 darrenr Exp
 */

#include <ctype.h>

#include "ipf.h"

#ifndef	MIN
# define	MIN(a,b)	((a) > (b) ? (b) : (a))
#endif


char	*icmpcodes[MAX_ICMPCODE + 1] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag", "srcfail",
	"net-unk", "host-unk", "isolate", "net-prohib", "host-prohib",
	"net-tos", "host-tos", "filter-prohib", "host-preced", "preced-cutoff",
	NULL };

/*
 * Return the number for the associated ICMP unreachable code.
 */
int icmpcode(str)
char *str;
{
	char	*s;
	int	i, len;

	if ((s = strrchr(str, ')')))
		*s = '\0';
	if (ISDIGIT(*str)) {
		if (!ratoi(str, &i, 0, 255))
			return -1;
		else
			return i;
	}
	len = strlen(str);
	for (i = 0; icmpcodes[i]; i++)
		if (!strncasecmp(str, icmpcodes[i], MIN(len,
				 strlen(icmpcodes[i])) ))
			return i;
	return -1;
}
