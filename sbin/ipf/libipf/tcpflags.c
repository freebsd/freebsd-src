
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"


/*
 * ECN is a new addition to TCP - RFC 2481
 */
#ifndef TH_ECN
# define	TH_ECN  0x40
#endif
#ifndef TH_CWR
# define	TH_CWR  0x80
#endif
#ifndef TH_AE
# define	TH_AE  0x100
#endif

extern	char	 flagset[];
extern	uint16_t flags[];


uint16_t tcpflags(char *flgs)
{
	uint16_t tcpf = 0;
	char *s, *t;

	for (s = flgs; *s; s++) {
		if (*s == 'W')
			tcpf |= TH_CWR;
		else {
			if (!(t = strchr(flagset, *s))) {
				return (0);
			}
			tcpf |= flags[t - flagset];
		}
	}
	return (tcpf);
}
