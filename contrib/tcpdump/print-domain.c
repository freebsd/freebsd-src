/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-domain.c,v 1.64 2001/01/02 23:24:51 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>

#ifdef NOERROR
#undef NOERROR					/* Solaris sucks */
#endif
#ifdef NOERROR
#undef T_UNSPEC					/* SINIX does too */
#endif
#include "nameser.h"

#include <stdio.h>
#include <string.h>

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
#ifndef T_NXT
#define T_NXT		30		/* Next Valid Name in Zone */
#endif
#ifndef T_EID
#define T_EID		31		/* Endpoint identifier */
#endif
#ifndef T_NIMLOC
#define T_NIMLOC	32		/* Nimrod locator */
#endif
#ifndef T_SRV
#define T_SRV		33		/* Server selection */
#endif
#ifndef T_ATMA
#define T_ATMA		34		/* ATM Address */
#endif
#ifndef T_NAPTR
#define T_NAPTR		35		/* Naming Authority PoinTeR */
#endif
#ifndef T_A6
#define T_A6		38		/* IP6 address */
#endif
#ifndef T_DNAME
#define T_DNAME		39		/* non-terminal redirection */
#endif

#ifndef T_OPT
#define T_OPT		41		/* EDNS0 option (meta-RR) */
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
	if (cp >= snapend)
		return(NULL);
	while (i && cp < snapend) {
		if ((i & INDIR_MASK) == EDNS0_MASK) {
			int bitlen, bytelen;

			if ((i & ~INDIR_MASK) != EDNS0_ELT_BITLABEL)
				return(NULL); /* unknown ELT */
			if ((bitlen = *cp++) == 0)
				bitlen = 256;
			bytelen = (bitlen + 7) / 8;
			cp += bytelen;
		} else
			cp += i;
		if (cp >= snapend)
			return(NULL);
		i = *cp++;
	}
	return (cp);
}

/* print a <domain-name> */
static const u_char *
blabel_print(const u_char *cp)
{
	int bitlen, slen, b;
	int truncated = 0;
	const u_char *bitp, *lim;
	char tc;

	if (cp >= snapend)
		return(NULL);
	if ((bitlen = *cp) == 0)
		bitlen = 256;
	slen = (bitlen + 3) / 4;
	if ((lim = cp + 1 + slen) > snapend) {
		truncated = 1;
		lim = snapend;
	}

	/* print the bit string as a hex string */
	printf("\\[x");
	for (bitp = cp + 1, b = bitlen; bitp < lim && b > 7; b -= 8, bitp++)
		printf("%02x", *bitp);
	if (bitp == lim)
		printf("...");
	else if (b > 4) {
		tc = *bitp++;
		printf("%02x", tc & (0xff << (8 - b)));
	} else if (b > 0) {
		tc = *bitp++;
		printf("%1x", ((tc >> 4) & 0x0f) & (0x0f << (4 - b)));
	}
	printf("/%d]", bitlen);

	return(truncated ? NULL : lim);
}

static int
labellen(const u_char *cp)
{
	register u_int i;

	if (cp >= snapend)
		return(-1);
	i = *cp;
	if ((i & INDIR_MASK) == EDNS0_MASK) {
		int bitlen, elt;

		if ((elt = (i & ~INDIR_MASK)) != EDNS0_ELT_BITLABEL)
			return(-1);
		if (cp + 1 >= snapend)
			return(-1);
		if ((bitlen = *(cp + 1)) == 0)
			bitlen = 256;
		return(((bitlen + 7) / 8) + 1);
	} else
		return(i);
}

static const u_char *
ns_nprint(register const u_char *cp, register const u_char *bp)
{
	register u_int i, l;
	register const u_char *rp = NULL;
	register int compress = 0;
	int chars_processed;
	int elt;
	int data_size = snapend - bp;

	if ((l = labellen(cp)) < 0)
		return(NULL);
	if (cp >= snapend)
		return(NULL);
	chars_processed = 1;
	if (((i = *cp++) & INDIR_MASK) != INDIR_MASK) {
		compress = 0;
		rp = cp + l;
	}

	if (i != 0)
		while (i && cp < snapend) {
			if ((i & INDIR_MASK) == INDIR_MASK) {
				if (!compress) {
					rp = cp + 1;
					compress = 1;
				}
				cp = bp + (((i << 8) | *cp) & 0x3fff);
				if (cp >= snapend)
					return(NULL);
				if ((l = labellen(cp)) < 0)
					return(NULL);
				i = *cp++;
				chars_processed++;

				/*
				 * If we've looked at every character in
				 * the message, this pointer will make
				 * us look at some character again,
				 * which means we're looping.
				 */
				if (chars_processed >= data_size) {
					printf("<LOOP>");
					return (NULL);
				}
				continue;
			}
			if ((i & INDIR_MASK) == EDNS0_MASK) {
				elt = (i & ~INDIR_MASK);
				switch(elt) {
				case EDNS0_ELT_BITLABEL:
					blabel_print(cp);
					break;
				default:
					/* unknown ELT */
					printf("<ELT %d>", elt);
					return(NULL);
				}
			} else {
				if (fn_printn(cp, l, snapend))
					break;
			}

			cp += l;
			chars_processed += l;
			putchar('.');
			if (cp >= snapend || (l = labellen(cp)) < 0)
				return(NULL);
			i = *cp++;
			chars_processed++;
			if (!compress)
				rp += l + 1;
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

	if (cp >= snapend)
		return NULL;
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
	{ T_LOC,	"LOC " },
	{ T_NXT,	"NXT " },
	{ T_EID,	"EID " },
	{ T_NIMLOC,	"NIMLOC " },
	{ T_SRV,	"SRV " },
	{ T_ATMA,	"ATMA " },
	{ T_NAPTR,	"NAPTR " },
	{ T_A6,		"A6 " },
	{ T_DNAME,	"DNAME " },
	{ T_OPT,	"OPT " },
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
static const u_char *
ns_qprint(register const u_char *cp, register const u_char *bp)
{
	register const u_char *np = cp;
	register u_int i;

	cp = ns_nskip(cp, bp);

	if (cp + 4 > snapend || cp == NULL)
		return(NULL);

	/* print the qtype and qclass (if it's not IN) */
	i = *cp++ << 8;
	i |= *cp++;
	printf(" %s", tok2str(type2str, "Type%d", i));
	i = *cp++ << 8;
	i |= *cp++;
	if (i != C_IN)
		printf(" %s", tok2str(class2str, "(Class %d)", i));

	fputs("? ", stdout);
	cp = ns_nprint(np, bp);
	return(cp ? cp + 4 : NULL);
}

/* print a reply */
static const u_char *
ns_rprint(register const u_char *cp, register const u_char *bp)
{
	register u_int class;
	register u_short typ, len;
	register const u_char *rp;

	if (vflag) {
		putchar(' ');
		if ((cp = ns_nprint(cp, bp)) == NULL)
			return NULL;
	} else
		cp = ns_nskip(cp, bp);

	if (cp + 10 > snapend || cp == NULL)
		return (snapend);

	/* print the type/qtype and class (if it's not IN) */
	typ = *cp++ << 8;
	typ |= *cp++;
	class = *cp++ << 8;
	class |= *cp++;
	if (class != C_IN && typ != T_OPT)
		printf(" %s", tok2str(class2str, "(Class %d)", class));

	/* ignore ttl */
	cp += 4;

	len = *cp++ << 8;
	len |= *cp++;

	rp = cp + len;

	printf(" %s", tok2str(type2str, "Type%d", typ));
	if (rp > snapend)
		return(NULL);

	switch (typ) {
	case T_A:
		if (cp + sizeof(struct in_addr) > snapend)
			return(NULL);
		printf(" %s", ipaddr_string(cp));
		break;

	case T_NS:
	case T_CNAME:
	case T_PTR:
#ifdef T_DNAME
	case T_DNAME:
#endif
		putchar(' ');
		if (ns_nprint(cp, bp) == NULL)
			return(NULL);
		break;

	case T_SOA:
		if (!vflag)
			break;
		putchar(' ');
		if ((cp = ns_nprint(cp, bp)) == NULL)
			return(NULL);
		putchar(' ');
		if ((cp = ns_nprint(cp, bp)) == NULL)
			return(NULL);
		if (cp + 5 * 4 > snapend)
			return(NULL);
		printf(" %u", EXTRACT_32BITS(cp));
		cp += 4;
		printf(" %u", EXTRACT_32BITS(cp));
		cp += 4;
		printf(" %u", EXTRACT_32BITS(cp));
		cp += 4;
		printf(" %u", EXTRACT_32BITS(cp));
		cp += 4;
		printf(" %u", EXTRACT_32BITS(cp));
		cp += 4;
		break;
	case T_MX:
		putchar(' ');
		if (cp + 2 > snapend)
			return(NULL);
		if (ns_nprint(cp + 2, bp) == NULL)
			return(NULL);
		printf(" %d", EXTRACT_16BITS(cp));
		break;

	case T_TXT:
		putchar(' ');
		(void)ns_cprint(cp, bp);
		break;

#ifdef INET6
	case T_AAAA:
		if (cp + sizeof(struct in6_addr) > snapend)
			return(NULL);
		printf(" %s", ip6addr_string(cp));
		break;

	case T_A6:
	    {
		struct in6_addr a;
		int pbit, pbyte;

		pbit = *cp;
		pbyte = (pbit & ~7) / 8;
		if (pbit > 128) {
			printf(" %u(bad plen)", pbit);
			break;
		} else if (pbit < 128) {
			memset(&a, 0, sizeof(a));
			memcpy(&a.s6_addr[pbyte], cp + 1, sizeof(a) - pbyte);
			printf(" %u %s", pbit, ip6addr_string(&a));
		}
		if (pbit > 0) {
			putchar(' ');
			if (ns_nprint(cp + 1 + sizeof(a) - pbyte, bp) == NULL)
				return(NULL);
		}
		break;
	    }
#endif /*INET6*/

	case T_OPT:
		printf(" UDPsize=%u", class);
		break;

	case T_UNSPECA:		/* One long string */
		if (cp + len > snapend)
			return(NULL);
		fn_printn(cp, len, snapend);
		break;
	}
	return (rp);		/* XXX This isn't always right */
}

void
ns_print(register const u_char *bp, u_int length)
{
	register const HEADER *np;
	register int qdcount, ancount, nscount, arcount;
	register const u_char *cp = NULL;

	np = (const HEADER *)bp;
	/* get the byte-order right */
	qdcount = ntohs(np->qdcount);
	ancount = ntohs(np->ancount);
	nscount = ntohs(np->nscount);
	arcount = ntohs(np->arcount);

	if (DNS_QR(np)) {
		/* this is a response */
		printf(" %d%s%s%s%s%s%s",
			ntohs(np->id),
			ns_ops[DNS_OPCODE(np)],
			ns_resp[DNS_RCODE(np)],
			DNS_AA(np)? "*" : "",
			DNS_RA(np)? "" : "-",
			DNS_TC(np)? "|" : "",
			DNS_CD(np)? "%" : "");

		if (qdcount != 1)
			printf(" [%dq]", qdcount);
		/* Print QUESTION section on -vv */
		if (vflag > 1) {
			fputs(" q:", stdout);
			if ((cp = ns_qprint((const u_char *)(np + 1), bp))
			    == NULL)
				goto trunc;
		} else {
			if ((cp = ns_nskip((const u_char *)(np + 1), bp))
			    == NULL)
				goto trunc;
			cp += 4;
		}
		printf(" %d/%d/%d", ancount, nscount, arcount);
		if (ancount--) {
			if ((cp = ns_rprint(cp, bp)) == NULL)
				goto trunc;
			while (ancount-- && cp < snapend) {
				putchar(',');
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
			}
		}
		/* Print NS and AR sections on -vv */
		if (vflag > 1) {
			if (nscount-- && cp < snapend) {
				fputs(" ns:", stdout);
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
				while (nscount-- && cp < snapend) {
					putchar(',');
					if ((cp = ns_rprint(cp, bp)) == NULL)
						goto trunc;
				}
			}
			if (arcount-- && cp < snapend) {
				fputs(" ar:", stdout);
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
				while (arcount-- && cp < snapend) {
					putchar(',');
					if ((cp = ns_rprint(cp, bp)) == NULL)
						goto trunc;
				}
			}
		}
	}
	else {
		/* this is a request */
		printf(" %d%s%s%s", ntohs(np->id), ns_ops[DNS_OPCODE(np)],
		    DNS_RD(np) ? "+" : "",
		    DNS_AD(np) ? "$" : "");

		/* any weirdness? */
		if (*(((u_short *)np)+1) & htons(0x6cf))
			printf(" [b2&3=0x%x]", ntohs(*(((u_short *)np)+1)));

		if (DNS_OPCODE(np) == IQUERY) {
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

		if (qdcount--) {
			cp = ns_qprint((const u_char *)(np + 1),
				       (const u_char *)np);
			if (!cp)
				goto trunc;
			if ((cp = ns_rprint(cp, bp)) == NULL)
				goto trunc;
			while (qdcount-- && cp < snapend) {
				cp = ns_qprint((const u_char *)cp,
					       (const u_char *)np);
				if (!cp)
					goto trunc;
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
			}
		}

		/* Print remaining sections on -vv */
		if (vflag > 1) {
			if (ancount--) {
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
				while (ancount-- && cp < snapend) {
					putchar(',');
					if ((cp = ns_rprint(cp, bp)) == NULL)
						goto trunc;
				}
			}
			if (nscount-- && cp < snapend) {
				fputs(" ns:", stdout);
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
				while (nscount-- && cp < snapend) {
					putchar(',');
					if ((cp = ns_rprint(cp, bp)) == NULL)
						goto trunc;
				}
			}
			if (arcount-- && cp < snapend) {
				fputs(" ar:", stdout);
				if ((cp = ns_rprint(cp, bp)) == NULL)
					goto trunc;
				while (arcount-- && cp < snapend) {
					putchar(',');
					if ((cp = ns_rprint(cp, bp)) == NULL)
						goto trunc;
				}
			}
		}
	}
	printf(" (%d)", length);
	return;

  trunc:
	printf("[|domain]");
	return;
}
