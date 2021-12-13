/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"

char *getsumd(sum)
	u_32_t sum;
{
	static char sumdbuf[17];

	if (sum & NAT_HW_CKSUM)
		snprintf(sumdbuf, sizeof(sumdbuf), "hw(%#0x)", sum & 0xffff);
	else
		snprintf(sumdbuf, sizeof(sumdbuf), "%#0x", sum);
	return sumdbuf;
}
