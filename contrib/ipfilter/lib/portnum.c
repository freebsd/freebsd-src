/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 *
 * Id: portnum.c,v 1.6.4.1 2004/12/09 19:41:22 darrenr Exp
 */

#include <ctype.h>

#include "ipf.h"


/*
 * find the port number given by the name, either from getservbyname() or
 * straight atoi(). Return 1 on success, 0 on failure
 */
int	portnum(name, proto, port, linenum)
char	*name, *proto;
u_short	*port;
int     linenum;
{
	struct	servent	*sp, *sp2;
	u_short	p1 = 0;
	int i;

	if (ISDIGIT(*name)) {
		if (ratoi(name, &i, 0, USHRT_MAX)) {
			*port = (u_short)i;
			return 1;
		}
		fprintf(stderr, "%d: unknown port \"%s\"\n", linenum, name);
		return 0;
	}
	if (proto != NULL && strcasecmp(proto, "tcp/udp") != 0) {
		sp = getservbyname(name, proto);
		if (sp) {
			*port = ntohs(sp->s_port);
			return 1;
		}
		fprintf(stderr, "%d: unknown service \"%s\".\n", linenum, name);
		return 0;
	}
	sp = getservbyname(name, "tcp");
	if (sp)
		p1 = sp->s_port;
	sp2 = getservbyname(name, "udp");
	if (!sp || !sp2) {
		fprintf(stderr, "%d: unknown tcp/udp service \"%s\".\n",
			linenum, name);
		return 0;
	}
	if (p1 != sp2->s_port) {
		fprintf(stderr, "%d: %s %d/tcp is a different port to ",
			linenum, name, p1);
		fprintf(stderr, "%d: %s %d/udp\n", linenum, name, sp->s_port);
		return 0;
	}
	*port = ntohs(p1);
	return 1;
}
