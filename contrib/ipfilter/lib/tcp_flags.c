/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: tcp_flags.c,v 1.8 2004/02/07 18:15:54 darrenr Exp
 */

#include "ipf.h"

extern	char	flagset[];
extern	u_char	flags[];


u_char tcp_flags(flgs, mask, linenum)
char *flgs;
u_char *mask;
int    linenum;
{
	u_char tcpf = 0, tcpfm = 0;
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
			tcpfm = 0xff & ~(TH_ECN|TH_CWR);
		else
			tcpfm = 0xff & ~(TH_ECN);
	}
	*mask = tcpfm;
	return tcpf;
}
