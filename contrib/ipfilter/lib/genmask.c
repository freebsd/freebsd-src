/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: genmask.c,v 1.7 2003/11/11 13:40:15 darrenr Exp
 */

#include "ipf.h"


int genmask(msk, mskp)
char *msk;
u_32_t *mskp;
{
	char *endptr = 0L;
	int bits;

	if (strchr(msk, '.') || strchr(msk, 'x') || strchr(msk, ':')) {
		/* possibly of the form xxx.xxx.xxx.xxx
		 * or 0xYYYYYYYY */
#ifdef	USE_INET6
		if (use_inet6) {
			if (inet_pton(AF_INET6, msk, mskp) != 1)
				return -1;
		} else
#endif
		if (inet_aton(msk, (struct in_addr *)mskp) == 0)
			return -1;
	} else {
		/*
		 * set x most significant bits
		 */
		bits = (int)strtol(msk, &endptr, 0);
#ifdef	USE_INET6
		if ((*endptr != '\0') ||
		    ((bits > 32) && !use_inet6) || (bits < 0) ||
		    ((bits > 128) && use_inet6))
#else
		if (*endptr != '\0' || bits > 32 || bits < 0)
#endif
			return -1;
#ifdef	USE_INET6
		if (use_inet6)
			fill6bits(bits, mskp);
		else
#endif
		if (bits == 0)
			*mskp = 0;
		else
			*mskp = htonl(0xffffffff << (32 - bits));
	}
	return 0;
}
