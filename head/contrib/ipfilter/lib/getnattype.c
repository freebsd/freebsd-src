/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2004 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */
#include "ipf.h"
#include "kmem.h"

#if !defined(lint)
static const char rcsid[] = "@(#)$Id: getnattype.c,v 1.3.2.2 2006/07/14 06:12:24 darrenr Exp $";
#endif


/*
 * Get a nat filter type given its kernel address.
 */
char *getnattype(nat, alive)
nat_t *nat;
int alive;
{
	static char unknownbuf[20];
	ipnat_t *ipn, ipnat;
	char *which;
	int type;

	if (!nat)
		return "???";
	if (alive) {
		type = nat->nat_redir;
	} else {
		ipn = nat->nat_ptr;
		if (kmemcpy((char *)&ipnat, (long)ipn, sizeof(ipnat)))
			return "!!!";
		type = ipnat.in_redir;
	}

	switch (type)
	{
	case NAT_MAP :
		which = "MAP";
		break;
	case NAT_MAPBLK :
		which = "MAP-BLOCK";
		break;
	case NAT_REDIRECT :
		which = "RDR";
		break;
	case NAT_BIMAP :
		which = "BIMAP";
		break;
	default :
		sprintf(unknownbuf, "unknown(%04x)", type & 0xffffffff);
		which = unknownbuf;
		break;
	}
	return which;
}
