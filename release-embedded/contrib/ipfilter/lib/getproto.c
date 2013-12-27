/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"
#include <ctype.h>

int getproto(name)
	char *name;
{
	struct protoent *p;
	char *s;

	for (s = name; *s != '\0'; s++)
		if (!ISDIGIT(*s))
			break;
	if (*s == '\0')
		return atoi(name);

#ifdef _AIX51
	/*
	 * For some bogus reason, "ip" is 252 in /etc/protocols on AIX 5
	 * The IANA has doubled up on the definition of 0 - it is now also
	 * used for IPv6 hop-opts, so we can no longer rely on /etc/protocols
	 * providing the correct name->number mapping
	 */
#endif
	if (!strcasecmp(name, "ip"))
		return 0;

	p = getprotobyname(name);
	if (p != NULL)
		return p->p_proto;
	return -1;
}
