/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: hostnum.c,v 1.10.2.1 2004/12/09 19:41:20 darrenr Exp
 */

#include <ctype.h>

#include "ipf.h"


/*
 * returns an ip address as a long var as a result of either a DNS lookup or
 * straight inet_addr() call
 */
int	hostnum(ipa, host, linenum, ifname)
u_32_t	*ipa;
char	*host;
int     linenum;
char	*ifname;
{
	struct	in_addr	ip;

	if (!strcasecmp("any", host) ||
	    (ifname && *ifname && !strcasecmp(ifname, host)))
		return 0;

#ifdef	USE_INET6
	if (use_inet6) {
		if (inet_pton(AF_INET6, host, ipa) == 1)
			return 0;
		else
			return -1;
	}
#endif
	if (ISDIGIT(*host) && inet_aton(host, &ip)) {
		*ipa = ip.s_addr;
		return 0;
	}

	if (!strcasecmp("<thishost>", host))
		host = thishost;

	return gethost(host, ipa);
}
