/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-domain.c,v 1.2 1995/03/08 12:52:29 olah Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#undef NOERROR					/* Solaris sucks */
#include <arpa/nameser.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

static char *ns_ops[] = {
	"", " inv_q", " stat", " op3", " op4", " op5", " op6", " op7",
	" op8", " updataA", " updateD", " updateDA",
	" updateM", " updateMA", " zoneInit", " zoneRef",
};

static char *ns_resp[] = {
	"", " FormErr", " ServFail", " NXDomain",
	" NotImp", " Refused", " Resp6", " Resp7",
	" Resp8", " Resp9", " Resp10", " Resp11",
	" Resp12", " Resp13", " Resp14", " NoChange",
};

/* skip over a domain name */
static const u_char *
ns_nskip(register const u_char *cp)
{
	register u_char i;

	if (((i = *cp++) & 0xc0) == 0xc0)
		return (cp + 1);
	while (i) {
		cp += i;
		i = *cp++;
	}
	return (cp);
}

/* print a domain name */
static void
ns_nprint(register const u_char *cp, register const u_char *bp,
	  register const u_char *ep)
{
	register u_int i;

	putchar(' ');
	if ((i = *cp++) != 0)
		while (i && cp < ep) {
			if ((i & 0xc0) == 0xc0) {
				cp = bp + (((i << 8) | *cp) & 0x3fff);
				i = *cp++;
				continue;
			}
			do {
				putchar(*cp++);
			} while (--i);
			putchar('.');
			i = *cp++;
		}
	else
		putchar('.');
}

static struct token type2str[] = {
	{ T_A,		"A" },
	{ T_NS,		"NS" },
	{ T_MD,		"MD" },
	{ T_MF,		"MF" },
	{ T_CNAME,	"CNAME" },
	{ T_SOA,	"SOA" },
	{ T_MB,		"MB" },
	{ T_MG,		"MG" },
	{ T_MR,		"MR" },
	{ T_NULL,	"NULL" },
	{ T_WKS,	"WKS" },
	{ T_PTR,	"PTR" },
	{ T_HINFO,	"HINFO" },
	{ T_MINFO,	"MINFO" },
	{ T_MX,		"MX" },
	{ T_UINFO,	"UINFO" },
	{ T_UID,	"UID" },
	{ T_GID,	"GID" },
#ifdef T_UNSPEC
	{ T_UNSPEC,	"UNSPEC" },
#endif
	{ T_AXFR,	"AXFR" },
	{ T_MAILB,	"MAILB" },
	{ T_MAILA,	"MAILA" },
	{ T_ANY,	"ANY" },
	{ 0,		NULL }
};

/* print a query */
static void
ns_qprint(register const u_char *cp, register const u_char *bp,
	  register const u_char *ep)
{
	const u_char *np = cp;
	register u_int i;

	cp = ns_nskip(cp);

	if (cp + 4 > ep)
		return;

	/* print the qtype and qclass (if it's not IN) */
	i = *cp++ << 8;
	i |= *cp++;
	printf(" %s", tok2str(type2str, "Type%d", i));
	i = *cp++ << 8;
	if ((i |= *cp++) != C_IN)
		if (i == C_ANY)
			printf("(c_any)");
		else
			printf("(Class %d)", i);

	putchar('?');
	ns_nprint(np, bp, ep);
}


/* print a reply */
static void
ns_rprint(register const u_char *cp, register const u_char *bp,
	  register const u_char *ep)
{
	register u_int i;
	u_short typ;

	cp = ns_nskip(cp);

	if (cp + 10 > ep)
		return;

	/* print the type/qtype and class (if it's not IN) */
	typ = *cp++ << 8;
	typ |= *cp++;
	i = *cp++ << 8;
	if ((i |= *cp++) != C_IN)
		if (i == C_ANY)
			printf("(c_any)");
		else
			printf("(Class %d)", i);

	/* ignore ttl & len */
	cp += 6;
	printf(" %s", tok2str(type2str, "Type%d", typ));
	switch (typ) {

	case T_A:
		printf(" %s", ipaddr_string(cp));
		break;

	case T_NS:
	case T_CNAME:
	case T_PTR:
		ns_nprint(cp, bp, ep);
		break;

	case T_MX:
		ns_nprint(cp+2, bp, ep);
#ifndef TCPDUMP_ALIGN
		printf(" %d", *(short *)cp);
#else
		{
		    u_short x = *cp | cp[1] << 8;
		    printf(" %d", ntohs(x));
		}
#endif
		break;
	}
}

void
ns_print(register const u_char *bp, int length)
{
	register const HEADER *np;
	int qdcount, ancount, nscount, arcount;
	const u_char *ep = snapend;

	np = (const HEADER *)bp;
	/* get the byte-order right */
	qdcount = ntohs(np->qdcount);
	ancount = ntohs(np->ancount);
	nscount = ntohs(np->nscount);
	arcount = ntohs(np->arcount);

	if (np->qr) {
		/* this is a response */
		printf(" %d%s%s%s%s%s",
			ntohs(np->id),
			ns_ops[np->opcode],
			ns_resp[np->rcode],
			np->aa? "*" : "",
			np->ra? "" : "-",
			np->tc? "|" : "");
		if (qdcount != 1)
			printf(" [%dq]", qdcount);
		printf(" %d/%d/%d", ancount, nscount, arcount);
		if (ancount)
			ns_rprint(ns_nskip((const u_char *)(np + 1)) + 4,
				  (const u_char *)np, ep);
	}
	else {
		/* this is a request */
		printf(" %d%s%s",
		        ntohs(np->id),
			ns_ops[np->opcode],
			np->rd? "+" : "");

		/* any weirdness? */
		if (*(((u_short *)np)+1) & htons(0x6ff))
			printf(" [b2&3=0x%x]", ntohs(*(((u_short *)np)+1)));

		if (np->opcode == IQUERY) {
			if (qdcount)
				printf(" [%dq]", qdcount);
			if (ancount != 1)
				printf(" [%da]", ancount);
		}
		else {
			if (ancount)
				printf(" [%da]", ancount);
			if (qdcount != 1)
				printf(" [%dq]", qdcount);
		}
		if (nscount)
			printf(" [%dn]", nscount);
		if (arcount)
			printf(" [%dau]", arcount);

		ns_qprint((const u_char *)(np + 1), (const u_char *)np, ep);
	}
	printf(" (%d)", length);
}
