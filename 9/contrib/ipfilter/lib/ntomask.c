/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
 * 
 * See the IPFILTER.LICENCE file for details on licencing.  
 *   
 * $Id: ntomask.c,v 1.6.2.1 2006/06/16 17:21:07 darrenr Exp $ 
 */     

#include "ipf.h"

int ntomask(v, nbits, ap)
int v, nbits;
u_32_t *ap;
{
	u_32_t mask;

	if (nbits < 0)
		return -1;

	switch (v)
	{
	case 4 :
		if (nbits > 32 || use_inet6 != 0)
			return -1;
		if (nbits == 0) {
			mask = 0;
		} else {
			mask = 0xffffffff;
			mask <<= (32 - nbits);
		}
		*ap = htonl(mask);
		break;

	case 6 :
		if ((nbits > 128) || (use_inet6 == 0))
			return -1;
		fill6bits(nbits, ap);
		break;

	default :
		return -1;
	}
	return 0;
}
