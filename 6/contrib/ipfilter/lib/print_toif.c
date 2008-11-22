/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: print_toif.c,v 1.8.4.1 2006/06/16 17:21:09 darrenr Exp $
 */

#include "ipf.h"


void print_toif(tag, fdp)
char *tag;
frdest_t *fdp;
{
	printf("%s %s%s", tag, fdp->fd_ifname,
		     (fdp->fd_ifp || (long)fdp->fd_ifp == -1) ? "" : "(!)");
#ifdef	USE_INET6
	if (use_inet6 && IP6_NOTZERO(&fdp->fd_ip6.in6)) {
		char ipv6addr[80];

		inet_ntop(AF_INET6, &fdp->fd_ip6, ipv6addr,
			  sizeof(fdp->fd_ip6));
		printf(":%s", ipv6addr);
	} else
#endif
		if (fdp->fd_ip.s_addr)
			printf(":%s", inet_ntoa(fdp->fd_ip));
	putchar(' ');
}
