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
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: print-domain.c,v 1.16 92/05/25 14:28:59 mccanne Exp $ (LBL)";
#endif

#include <sys/param.h>
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
#include <arpa/nameser.h>

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
static u_char *
ns_nskip(cp)
	register u_char *cp;
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
ns_nprint(cp, bp, ep)
	register u_char *cp;
	register u_char *bp;
	register u_char *ep;
{
	register u_int i;

	putchar(' ');
	if (i = *cp++)
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


/* print a query */
static void
ns_qprint(cp, bp, ep)
	register u_char *cp;
	register u_char *bp;
	register u_char *ep;
{
	u_char *np = cp;
	register u_int i;

	cp = ns_nskip(cp);

	if (cp + 4 > ep)
		return;

	/* print the qtype and qclass (if it's not IN) */
	i = *cp++ << 8;
	switch (i |= *cp++) {
	case T_A:	printf(" A"); break;
	case T_NS:	printf(" NS"); break;
	case T_MD:	printf(" MD"); break;
	case T_MF:	printf(" MF"); break;
	case T_CNAME:	printf(" CNAME"); break;
	case T_SOA:	printf(" SOA"); break;
	case T_MB:	printf(" MB"); break;
	case T_MG:	printf(" MG"); break;
	case T_MR:	printf(" MR"); break;
	case T_NULL:	printf(" NULL"); break;
	case T_WKS:	printf(" WKS"); break;
	case T_PTR:	printf(" PTR"); break;
	case T_HINFO:	printf(" HINFO"); break;
	case T_MINFO:	printf(" MINFO"); break;
	case T_MX:	printf(" MX"); break;
	case T_UINFO:	printf(" UINFO"); break;
	case T_UID:	printf(" UID"); break;
	case T_GID:	printf(" GID"); break;
#ifdef T_UNSPEC
	case T_UNSPEC:	printf(" UNSPEC"); break;
#endif
	case T_AXFR:	printf(" AXFR"); break;
	case T_MAILB:	printf(" MAILB"); break;
	case T_MAILA:	printf(" MAILA"); break;
	case T_ANY:	printf(" ANY"); break;
	default:	printf(" Type%d", i); break;
	}
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
ns_rprint(cp, bp, ep)
	register u_char *cp;
	register u_char *bp;
	register u_char *ep;
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
	switch (typ) {
	case T_A:	printf(" A %s", ipaddr_string(cp)); break;
	case T_NS:	printf(" NS"); ns_nprint(cp, bp, ep); break;
	case T_MD:	printf(" MD"); break;
	case T_MF:	printf(" MF"); break;
	case T_CNAME:	printf(" CNAME"); ns_nprint(cp, bp, ep); break;
	case T_SOA:	printf(" SOA"); break;
	case T_MB:	printf(" MB"); break;
	case T_MG:	printf(" MG"); break;
	case T_MR:	printf(" MR"); break;
	case T_NULL:	printf(" NULL"); break;
	case T_WKS:	printf(" WKS"); break;
	case T_PTR:	printf(" PTR"); ns_nprint(cp, bp, ep); break;
	case T_HINFO:	printf(" HINFO"); break;
	case T_MINFO:	printf(" MINFO"); break;
	case T_MX:	printf(" MX"); ns_nprint(cp+2, bp, ep);
#ifndef TCPDUMP_ALIGN
			printf(" %d", *(short *)cp);
#else
			{
			    u_short x = *cp | cp[1] << 8; 
			    printf(" %d", ntohs(x));
			}
#endif
			break;
	case T_UINFO:	printf(" UINFO"); break;
	case T_UID:	printf(" UID"); break;
	case T_GID:	printf(" GID"); break;
#ifdef T_UNSPEC
	case T_UNSPEC:	printf(" UNSPEC"); break;
#endif
	case T_AXFR:	printf(" AXFR"); break;
	case T_MAILB:	printf(" MAILB"); break;
	case T_MAILA:	printf(" MAILA"); break;
	case T_ANY:	printf(" ANY"); break;
	default:	printf(" Type%d", typ); break;
	}
}

void
ns_print(np, length)
	register HEADER *np;
	int length;
{
	u_char *ep = (u_char *)snapend;

	/* get the byte-order right */
	NTOHS(np->id);
	NTOHS(np->qdcount);
	NTOHS(np->ancount);
	NTOHS(np->nscount);
	NTOHS(np->arcount);

	if (np->qr) {
		/* this is a response */
		printf(" %d%s%s%s%s%s",
			np->id,
			ns_ops[np->opcode],
			ns_resp[np->rcode],
			np->aa? "*" : "",
			np->ra? "" : "-",
			np->tc? "|" : "");
		if (np->qdcount != 1)
			printf(" [%dq]", np->qdcount);
		printf(" %d/%d/%d", np->ancount, np->nscount, np->arcount);
		if (np->ancount)
			ns_rprint(ns_nskip((u_char *)(np + 1)) + 4,
				  (u_char *)np, ep);
	}
	else {
		/* this is a request */
		printf(" %d%s%s",
			np->id,
			ns_ops[np->opcode],
			np->rd? "+" : "");

		/* any weirdness? */
 		if (*(((u_short *)np)+1) & htons(0x6ff))
 			printf(" [b2&3=0x%x]", ntohs(*(((u_short *)np)+1)));

		if (np->opcode == IQUERY) {
			if (np->qdcount)
				printf(" [%dq]", np->qdcount);
			if (np->ancount != 1)
				printf(" [%da]", np->ancount);
		}
		else {
			if (np->ancount)
				printf(" [%da]", np->ancount);
			if (np->qdcount != 1)
				printf(" [%dq]", np->qdcount);
		}
		if (np->nscount)
			printf(" [%dn]", np->nscount);
		if (np->arcount)
			printf(" [%dau]", np->arcount);

		ns_qprint((u_char *)(np + 1), (u_char *)np, ep);
	}
	printf(" (%d)", length);
}
