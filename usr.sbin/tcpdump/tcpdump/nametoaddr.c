/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Name to id translation routines used by the scanner.
 * These functions are not time critical.
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: nametoaddr.c,v 1.9 91/02/04 16:56:46 mccanne Exp $ (LBL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <errno.h>
#include <arpa/inet.h>

#ifdef ultrix
#include <sys/time.h>
#include <rpc/types.h>
#include <nfs/nfs.h>
#endif

#include "interface.h"
#include "etherent.h"
#include "nametoaddr.h"

/*
 *  Convert host name to internet address.
 *  Return 0 upon failure.
 */
u_long **
s_nametoaddr(name)
	char *name;
{
#ifndef h_addr
	static u_long *hlist[2];
#endif
	u_long **p;
	struct hostent *hp;

	if (hp = gethostbyname(name)) {
#ifndef h_addr
		hlist[0] = (u_long *)hp->h_addr;
		NTOHL(hp->h_addr);
		return hlist;
#else
		for (p = (u_long **)hp->h_addr_list; *p; ++p)
			NTOHL(**p);
		return (u_long **)hp->h_addr_list;
#endif
	}
	else
		return 0;
}

/*
 *  Convert net name to internet address.
 *  Return 0 upon failure.
 */
u_long
s_nametonetaddr(name)
	char *name;
{
	struct netent *np;

	if (np = getnetbyname(name))
		return np->n_net;
	else
		return 0;
}

/*
 * Convert a port name to its port and protocol numbers.
 * We assume only TCP or UDP.
 * Return 0 upon failure.
 */
s_nametoport(name, port, proto)
	char *name;
	int *port;
	int *proto;
{
	struct servent *sp;
	char *other;

	sp = getservbyname(name, (char *)0);
	if (sp != 0) {
		NTOHS(sp->s_port);
		*port = sp->s_port;
		*proto = s_nametoproto(sp->s_proto);
		/*
		 * We need to check /etc/services for ambiguous entries. 
		 * If we find the ambiguous entry, and it has the
		 * same port number, change the proto to PROTO_UNDEF
		 * so both TCP and UDP will be checked.
		 */
		if (*proto == IPPROTO_TCP)
			other = "udp";
		else
			other = "tcp";

		sp = getservbyname(name, other);
		if (sp != 0) {
			NTOHS(sp->s_port);
			if (*port != sp->s_port)
				/* Can't handle ambigous names that refer
				   to different port numbers. */
				warning("ambiguous port %s in /etc/services",
					name);
			*proto = PROTO_UNDEF;
		}
		return 1;
	}
#ifdef ultrix
	/* Special hack in case NFS isn't in /etc/services */
	if (strcmp(name, "nfs") == 0) {
		*port = NFS_PORT;
		*proto = PROTO_UNDEF;
		return 1;
	}
#endif
	return 0;
}

int
s_nametoproto(str)
	char *str;
{
	struct protoent *p;

	p = getprotobyname(str);
	if (p != 0)
		return p->p_proto;
	else
		return PROTO_UNDEF;
}

#include "etherproto.h"

int
s_nametoeproto(s)
	char *s;
{
	struct eproto *p = eproto_db;

	while (p->s != 0) {
		if (strcmp(p->s, s) == 0)
			return p->p;
		p += 1;
	}
	return PROTO_UNDEF;
}

/* Hex digit to integer. */
static inline int
xdtoi(c)
{
	if (isdigit(c))
		return c - '0';
	else if (islower(c))
		return c - 'a' + 10;
	else
		return c - 'A' + 10;
}

u_long
atoin(s)
	char *s;
{
	u_long addr = 0;
	u_int n;

	while (1) {
		n = 0;
		while (*s && *s != '.') 
			n = n * 10 + *s++ - '0';
		addr <<= 8;
		addr |= n & 0xff;
		if (*s == '\0')
			return addr;
		++s;
	}
	/* NOTREACHED */
}
	

/*
 * Convert 's' which has the form "xx:xx:xx:xx:xx:xx" into a new 
 * ethernet address.  Assumes 's' is well formed.
 */
u_char *
ETHER_aton(s)
	char *s;
{
	register u_char *ep, *e;
	register u_int d;

	e = ep = (u_char *)malloc(6);
	
	while (*s) {
		if (*s == ':')
			s += 1;
		d = xdtoi(*s++);
		if (isxdigit(*s)) {
			d <<= 4;
			d |= xdtoi(*s++);
		}
		*ep++ = d;
	}

	return e;
}

#ifndef ETHER_SERVICE
u_char *
ETHER_hostton(name)
	char *name;
{
	struct etherent *ep;
	FILE *fp;
	u_char *ap;

	fp = fopen(ETHERS_FILE, "r");
	if (fp != 0) {
		while (ep = next_etherent(fp)) {
			if (strcmp(ep->name, name) == 0) {
				ap = (u_char *)malloc(6);
				bcopy(ep->addr, ap, 6);
				return ap;
			}
		}
	}
	return (u_char *)0;
}
#endif
