/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
static const char rcsid[] =
    "@(#) $Header: print-domain.c,v 1.37 96/12/10 23:21:06 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#undef NOERROR					/* Solaris sucks */
#undef T_UNSPEC					/* SINIX does too */
#include <arpa/nameser.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"                    /* must come after interface.h */

/* Compatibility */
#ifndef T_TXT
#define T_TXT		16		/* text strings */
#endif
#ifndef T_RP
#define T_RP		17		/* responsible person */
#endif
#ifndef T_AFSDB
#define T_AFSDB		18		/* AFS cell database */
#endif
#ifndef T_X25
#define T_X25		19		/* X_25 calling address */
#endif
#ifndef T_ISDN
#define T_ISDN		20		/* ISDN calling address */
#endif
#ifndef T_RT
#define T_RT		21		/* router */
#endif
#ifndef T_NSAP
#define T_NSAP		22		/* NSAP address */
#endif
#ifndef T_NSAP_PTR
#define T_NSAP_PTR	23		/* reverse NSAP lookup (deprecated) */
#endif
#ifndef T_SIG
#define T_SIG		24		/* security signature */
#endif
#ifndef T_KEY
#define T_KEY		25		/* security key */
#endif
#ifndef T_PX
#define T_PX		26		/* X.400 mail mapping */
#endif
#ifndef T_GPOS
#define T_GPOS		27		/* geographical position (withdrawn) */
#endif
#ifndef T_AAAA
#define T_AAAA		28		/* IP6 Address */
#endif
#ifndef T_LOC
#define T_LOC		29		/* Location Information */
#endif

#ifndef T_UNSPEC
#define T_UNSPEC	103		/* Unspecified format (binary data) */
#endif
#ifndef T_UNSPECA
#define T_UNSPECA	104		/* "unspecified ascii". Ugly MIT hack */
#endif

#ifndef C_CHAOS
#define C_CHAOS		3		/* for chaos net (MIT) */
#endif
#ifndef C_HS
#define C_HS		4		/* for Hesiod name server (MIT) (XXX) */
#endif

static char *ns_ops[] = {
	"", " inv_q", " stat", " op3", " notify", " op5", " op6", " op7",
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
ns_nskip(register const u_char *cp, register const u_char *bp)
{
	register u_char i;

	if (((i = *cp++) & INDIR_MASK) == INDIR_MASK)
		return (cp + 1);
	while (i && cp < snapend) {
		cp += i;
		i = *cp++;
	}
	return (cp);
}

/* print a <domain-name> */
static const u_char *
ns_nprint(register const u_char *cp, register const u_char *bp)
{
	register u_int i;
	register const u_char *rp;
	register int compress;

	i = *cp++;
	rp = cp + i;
	if ((i & INDIR_MASK) == INDIR_MASK) {
		rp = cp + 1;
		compress = 1;
	} else
		compress = 0;
	if (i != 0)
		while (i && cp < snapend) {
			if ((i & INDIR_MASK) == INDIR_MASK) {
				cp = bp + (((i << 8) | *cp) & 0x3fff);
				i = *cp++;
				continue;
			}
			if (fn_printn(cp, i, snapend))
				break;
			cp += i;
			putchar('.');
			i = *cp++;
			if (!compress)
				rp += i + 1;
		}
	else
		putchar('.');
	return (rp);
}

/* print a <character-string> */
static const u_char *
ns_cprint(register const u_char *cp, register const u_char *bp)
{
	register u_int i;

	i = *cp++;
	(void)fn_printn(cp, i, snapend);
	return (cp + i);
}

static struct tok type2str[] = {
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
	{ T_TXT,	"TXT" },
	{ T_RP,		"RP" },
	{ T_AFSDB,	"AFSDB" },
	{ T_X25,	"X25" },
	{ T_ISDN,	"ISDN" },
	{ T_RT,		"RT" },
	{ T_NSAP,	"NSAP" },
	{ T_NSAP_PTR,	"NSAP_PTR" },
	{ T_SIG,	"SIG" },
	{ T_KEY,	"KEY" },
	{ T_PX,		"PX" },
	{ T_GPOS,	"GPOS" },
	{ T_AAAA,	"AAAA" },
	{ T_LOC ,	"LOC " },
	{ T_UINFO,	"UINFO" },
	{ T_UID,	"UID" },
	{ T_GID,	"GID" },
	{ T_UNSPEC,	"UNSPEC" },
	{ T_UNSPECA,	"UNSPECA" },
	{ T_AXFR,	"AXFR" },
	{ T_MAILB,	"MAILB" },
	{ T_MAILA,	"MAILA" },
	{ T_ANY,	"ANY" },
	{ 0,		NULL }
};

static struct tok class2str[] = {
	{ C_IN,		"IN" },		/* Not used */
	{ C_CHAOS,	"CHAOS)" },
	{ C_HS,		"HS" },
	{ C_ANY,	"ANY" },
	{ 0,		NULL }
};

/* print a query */
static void
ns_qprint(register const u_char *cp, register const u_char *bp)
{
	register const u_char *np = cp;
	register u_int i;

	cp = ns_nskip(cp, bp);

	if (cp + 4 > snapend)
		return;

	/* print the qtype and qclass (if it's not IN) */
	i = *cp++ << 8;
	i |= *cp++;
	printf(" %s", tok2str(type2str, "Type%d", i));
	i = *cp++ << 8;
	i |= *cp++;
	if (i != C_IN)
		printf(" %s", tok2str(class2str, "(Class %d)", i));

	fputs("? ", stdout);
	ns_nprint(np, bp);
}

/* print a reply */
static const u_char *
ns_rprint(register const u_char *cp, register const u_char *bp)
{
	register u_int i;
	register u_short typ, len;
	register const u_char *rp;

	if (vflag) {
		putchar(' ');
		cp = ns_nprint(cp, bp);
	} else
		cp = ns_nskip(cp, bp);

	if (cp + 10 > snapend)
		return (snapend);

	/* print the type/qtype and class (if it's not IN) */
	typ = *cp++ << 8;
	typ |= *cp++;
	i = *cp++ << 8;
	i |= *cp++;
	if (i != C_IN)
		printf(" %s", tok2str(class2str, "(Class %d)", i));

	/* ignore ttl */
	cp += 4;

	len = *cp++ << 8;
	len |= *cp++;

	rp = cp + len;

	printf(" %s", tok2str(type2str, "Type%d", typ));
	switch (typ) {

	case T_A:
		printf(" %s", ipaddr_string(cp));
		break;

	case T_NS:
	case T_CNAME:
	case T_PTR:
		putchar(' ');
		(void)ns_nprint(cp, bp);
		break;

	case T_MX:
		putchar(' ');
		(void)ns_nprint(cp + 2, bp);
		printf(" %d", EXTRACT_16BITS(cp));
		break;

	case T_TXT:
		putchar(' ');
		(void)ns_cprint(cp, bp);
		break;

	case T_UNSPECA:		/* One long string */
	        printf(" %.*s", len, cp);
		break;
	}
	return (rp);		/* XXX This isn't always right */
}

void
ns_print(register const u_char *bp, u_int length)
{
	register const HEADER *np;
	register int qdcount, ancount, nscount, arcount;
	register const u_char *cp;

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
		/* Print QUESTION section on -vv */
		if (vflag > 1) {
		            fputs(" q: ", stdout);
			    cp = ns_nprint((const u_char *)(np + 1), bp);
		} else
			    cp = ns_nskip((const u_char *)(np + 1), bp);
		printf(" %d/%d/%d", ancount, nscount, arcount);
		if (ancount--) {
			cp = ns_rprint(cp + 4, bp);
			while (ancount-- && cp < snapend) {
				putchar(',');
				cp = ns_rprint(cp, bp);
			}
		}
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

		ns_qprint((const u_char *)(np + 1), (const u_char *)np);
	}
	printf(" (%d)", length);
}
