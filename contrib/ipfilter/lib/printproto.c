/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"


#if !defined(lint)
static const char rcsid[] = "@(#)$Id$";
#endif


void
printproto(pr, p, np)
	struct protoent *pr;
	int p;
	ipnat_t *np;
{
	if (np != NULL) {
		if ((np->in_flags & IPN_TCPUDP) == IPN_TCPUDP)
			PRINTF("tcp/udp");
		else if (np->in_flags & IPN_TCP)
			PRINTF("tcp");
		else if (np->in_flags & IPN_UDP)
			PRINTF("udp");
		else if (np->in_flags & IPN_ICMPQUERY)
			PRINTF("icmp");
#ifdef _AIX51
		/*
		 * To make up for "ip = 252" and "hopopt = 0" in /etc/protocols
		 * The IANA has doubled up on the definition of 0 - it is now
		 * also used for IPv6 hop-opts, so we can no longer rely on
		 * /etc/protocols providing the correct name->number mapping.
		 */
#endif
		else if (np->in_pr[0] == 0)
			PRINTF("ip");
		else if (pr != NULL)
			PRINTF("%s", pr->p_name);
		else
			PRINTF("%d", np->in_pr[0]);
	} else {
#ifdef _AIX51
		if (p == 0)
			PRINTF("ip");
		else
#endif
		if (pr != NULL)
			PRINTF("%s", pr->p_name);
		else
			PRINTF("%d", p);
	}
}
