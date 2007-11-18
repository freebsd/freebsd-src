/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printmask.c,v 1.5.4.1 2006/06/16 17:21:13 darrenr Exp $
 */

#include "ipf.h"


void	printmask(mask)
u_32_t	*mask;
{
	struct in_addr ipa;
	int ones;

#ifdef  USE_INET6
	if (use_inet6)
		printf("/%d", count6bits(mask));
	else
#endif
	if ((ones = count4bits(*mask)) == -1) {
		ipa.s_addr = *mask;
		printf("/%s", inet_ntoa(ipa));
	} else
		printf("/%d", ones);
}
