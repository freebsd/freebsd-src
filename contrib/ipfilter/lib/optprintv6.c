/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: optprintv6.c,v 1.2 2003/04/30 00:39:39 darrenr Exp
 */
#include "ipf.h"


#ifdef	USE_INET6

void optprintv6(sec, optmsk, optbits)
u_short *sec;
u_long optmsk, optbits;
{
	u_short secmsk = sec[0], secbits = sec[1];
	struct ipopt_names *io;
	char *s;

	s = " v6hdrs ";
	for (io = v6ionames; io->on_name; io++)
		if ((io->on_bit & optmsk) &&
		    ((io->on_bit & optmsk) == (io->on_bit & optbits))) {
			printf("%s%s", s, io->on_name);
			s = ",";
		}

	if ((optmsk && (optmsk != optbits)) ||
	    (secmsk && (secmsk != secbits))) {
		s = " ";
		printf(" not v6hdrs");
		if (optmsk != optbits) {
			for (io = v6ionames; io->on_name; io++)
				if ((io->on_bit & optmsk) &&
				    ((io->on_bit & optmsk) !=
				     (io->on_bit & optbits))) {
					printf("%s%s", s, io->on_name);
					s = ",";
				}
		}

	}
}
#endif
