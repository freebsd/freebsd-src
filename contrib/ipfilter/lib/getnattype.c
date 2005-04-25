/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */
#include "ipf.h"
#include "kmem.h"

#if !defined(lint)
static const char rcsid[] = "@(#)Id: getnattype.c,v 1.3 2004/01/17 17:26:07 darrenr Exp";
#endif


/*
 * Get a nat filter type given its kernel address.
 */
char *getnattype(ipnat)
ipnat_t *ipnat;
{
	static char unknownbuf[20];
	ipnat_t ipnatbuff;
	char *which;

	if (!ipnat)
		return "???";
	if (kmemcpy((char *)&ipnatbuff, (long)ipnat, sizeof(ipnatbuff)))
		return "!!!";

	switch (ipnatbuff.in_redir)
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
		sprintf(unknownbuf, "unknown(%04x)",
			ipnatbuff.in_redir & 0xffffffff);
		which = unknownbuf;
		break;
	}
	return which;
}
