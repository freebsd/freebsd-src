/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
    "@(#) $Header: /home/ncvs/src/lib/libpcap/nametoaddr.c,v 1.2 1995/05/30 05:47:21 rgrimes Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pcap.h>
#include <pcap-namedb.h>
#include <stdio.h>
#include <stdlib.h>

#include "gencode.h"

#ifndef __GNUC__
#define inline
#endif

#ifndef NTOHL
#define NTOHL(x) (x) = ntohl(x)
#define NTOHS(x) (x) = ntohs(x)
#endif

static inline int xdtoi(int);

/*
 *  Convert host name to internet address.
 *  Return 0 upon failure.
 */
u_long **
pcap_nametoaddr(const char *name)
{
#ifndef h_addr
	static u_long *hlist[2];
#endif
	u_long **p;
	struct hostent *hp;

	if ((hp = gethostbyname(name)) != NULL) {
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
pcap_nametonetaddr(const char *name)
{
	struct netent *np;

	if ((np = getnetbyname(name)) != NULL)
		return np->n_net;
	else
		return 0;
}

/*
 * Convert a port name to its port and protocol numbers.
 * We assume only TCP or UDP.
 * Return 0 upon failure.
 */
int
pcap_nametoport(const char *name, int *port, int *proto)
{
	struct servent *sp;
	char *other;

	sp = getservbyname(name, (char *)0);
	if (sp != NULL) {
		NTOHS(sp->s_port);
		*port = sp->s_port;
		*proto = pcap_nametoproto(sp->s_proto);
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
				/* Can't handle ambiguous names that refer
				   to different port numbers. */
#ifdef notdef
				warning("ambiguous port %s in /etc/services",
					name);
#else
			;
#endif
			*proto = PROTO_UNDEF;
		}
		return 1;
	}
#if defined(ultrix) || defined(__osf__)
	/* Special hack in case NFS isn't in /etc/services */
	if (strcmp(name, "nfs") == 0) {
		*port = 2049;
		*proto = PROTO_UNDEF;
		return 1;
	}
#endif
	return 0;
}

int
pcap_nametoproto(const char *str)
{
	struct protoent *p;

	p = getprotobyname(str);
	if (p != 0)
		return p->p_proto;
	else
		return PROTO_UNDEF;
}

#include "ethertype.h"

struct eproto {
	char *s;
	u_short p;
};

/* Static data base of ether protocol types. */
struct eproto eproto_db[] = {
	{ "pup", ETHERTYPE_PUP },
	{ "xns", ETHERTYPE_NS },
	{ "ip", ETHERTYPE_IP },
	{ "arp", ETHERTYPE_ARP },
	{ "rarp", ETHERTYPE_REVARP },
	{ "sprite", ETHERTYPE_SPRITE },
	{ "mopdl", ETHERTYPE_MOPDL },
	{ "moprc", ETHERTYPE_MOPRC },
	{ "decnet", ETHERTYPE_DN },
	{ "lat", ETHERTYPE_LAT },
	{ "lanbridge", ETHERTYPE_LANBRIDGE },
	{ "vexp", ETHERTYPE_VEXP },
	{ "vprod", ETHERTYPE_VPROD },
	{ "atalk", ETHERTYPE_ATALK },
	{ "atalkarp", ETHERTYPE_AARP },
	{ "loopback", ETHERTYPE_LOOPBACK },
	{ "decdts", ETHERTYPE_DECDTS },
	{ "decdns", ETHERTYPE_DECDNS },
	{ (char *)0, 0 }
};

int
pcap_nametoeproto(const char *s)
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
	register int c;
{
	if (isdigit(c))
		return c - '0';
	else if (islower(c))
		return c - 'a' + 10;
	else
		return c - 'A' + 10;
}

u_long
__pcap_atoin(const char *s)
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

u_long
__pcap_atodn(const char *s)
{
#define AREASHIFT 10
#define AREAMASK 0176000
#define NODEMASK 01777

	u_long addr = 0;
	u_int node, area;

	if (sscanf((char *)s, "%d.%d", &area, &node) != 2)
		bpf_error("malformed decnet address '%s'", s);

	addr = (area << AREASHIFT) & AREAMASK;
	addr |= (node & NODEMASK);

	return(addr);
}

/*
 * Convert 's' which has the form "xx:xx:xx:xx:xx:xx" into a new
 * ethernet address.  Assumes 's' is well formed.
 */
u_char *
pcap_ether_aton(const char *s)
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

	return (e);
}

#ifndef ETHER_SERVICE
/* Roll our own */
u_char *
pcap_ether_hostton(const char *name)
{
	register struct pcap_etherent *ep;
	register u_char *ap;
	static FILE *fp = NULL;
	static init = 0;

	if (!init) {
		fp = fopen(PCAP_ETHERS_FILE, "r");
		++init;
		if (fp == NULL)
			return (NULL);
	} else if (fp == NULL)
		return (NULL);
	else
		rewind(fp);

	while ((ep = pcap_next_etherent(fp)) != NULL) {
		if (strcmp(ep->name, name) == 0) {
			ap = (u_char *)malloc(6);
			if (ap != NULL) {
				memcpy(ap, ep->addr, 6);
				return (ap);
			}
			break;
		}
	}
	return (NULL);
}
#else
/* Use the os supplied routines */
u_char *
pcap_ether_hostton(const char *name)
{
	register u_char *ap;
	u_char a[6];
#ifndef sgi
	extern int ether_hostton(char *, struct ether_addr *);
#endif

	ap = NULL;
	if (ether_hostton((char*)name, (struct ether_addr *)a) == 0) {
		ap = (u_char *)malloc(6);
		if (ap != NULL)
			memcpy(ap, a, 6);
	}
	return (ap);
}
#endif

u_short
__pcap_nametodnaddr(const char *name)
{
#ifdef	DECNETLIB
	struct nodeent *getnodebyname();
	struct nodeent *nep;
	unsigned short res;

	nep = getnodebyname(name);
	if (nep == ((struct nodeent *)0))
		bpf_error("unknown decnet host name '%s'\n", name);

	memcpy((char *)&res, (char *)nep->n_addr, sizeof(unsigned short));
	return(res);
#else
	bpf_error("decnet name support not included, '%s' cannot be translated\n",
		name);
#endif
}
