/*
 * Copyright (C) 2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"


#if !defined(lint)
static const char rcsid[] = "@(#)$Id: printproto.c,v 1.1.2.2 2006/06/16 17:21:14 darrenr Exp $";
#endif


void printproto(pr, p, np)
struct protoent *pr;
int p;
ipnat_t *np;
{
	if (np != NULL) {
		if ((np->in_flags & IPN_TCPUDP) == IPN_TCPUDP)
			printf("tcp/udp");
		else if (np->in_flags & IPN_TCP)
			printf("tcp");
		else if (np->in_flags & IPN_UDP)
			printf("udp");
		else if (np->in_flags & IPN_ICMPQUERY)
			printf("icmp");
#ifdef _AIX51
		/*
		 * To make up for "ip = 252" and "hopopt = 0" in /etc/protocols
		 */
		else if (np->in_p == 0)
			printf("ip");
#endif
		else if (pr != NULL)
			printf("%s", pr->p_name);
		else
			printf("%d", np->in_p);
	} else {
#ifdef _AIX51
		if (p == 0)
			printf("ip");
		else
#endif
		if (pr != NULL)
			printf("%s", pr->p_name);
		else
			printf("%d", p);
	}
}
