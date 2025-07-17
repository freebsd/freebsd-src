
/*
 * Copyright (C) 2000-2004 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: tcp_flags.c,v 1.8.2.1 2006/06/16 17:21:17 darrenr Exp $
 */

#include "ipf.h"

extern	char	 flagset[];
extern	uint16_t flags[];


uint16_t tcp_flags(char *flgs, uint16_t *mask, int linenum)
{
	uint16_t tcpf = 0, tcpfm = 0;
	char *s;

	s = strchr(flgs, '/');
	if (s)
		*s++ = '\0';

	if (*flgs == '0') {
		tcpf = strtol(flgs, NULL, 0);
	} else {
		tcpf = tcpflags(flgs);
	}

	if (s != NULL) {
		if (*s == '0')
			tcpfm = strtol(s, NULL, 0);
		else
			tcpfm = tcpflags(s);
	}

	if (!tcpfm) {
		if (tcpf == TH_SYN)
			tcpfm = TH_FLAGS & ~(TH_ECN|TH_CWR);
		else
			tcpfm = TH_FLAGS & ~(TH_ECN);
	}
	*mask = tcpfm;
	return (tcpf);
}
