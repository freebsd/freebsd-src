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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-tcp.c,v 1.63 1999/12/22 15:44:10 itojun Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

/* Compatibility */
#ifndef TCPOPT_WSCALE
#define	TCPOPT_WSCALE		3	/* window scale factor (rfc1072) */
#endif
#ifndef TCPOPT_SACKOK
#define	TCPOPT_SACKOK		4	/* selective ack ok (rfc1072) */
#endif
#ifndef TCPOPT_SACK
#define	TCPOPT_SACK		5	/* selective ack (rfc1072) */
#endif
#ifndef TCPOPT_ECHO
#define	TCPOPT_ECHO		6	/* echo (rfc1072) */
#endif
#ifndef TCPOPT_ECHOREPLY
#define	TCPOPT_ECHOREPLY	7	/* echo (rfc1072) */
#endif
#ifndef TCPOPT_TIMESTAMP
#define TCPOPT_TIMESTAMP	8	/* timestamps (rfc1323) */
#endif
#ifndef TCPOPT_CC
#define TCPOPT_CC		11	/* T/TCP CC options (rfc1644) */
#endif
#ifndef TCPOPT_CCNEW
#define TCPOPT_CCNEW		12	/* T/TCP CC options (rfc1644) */
#endif
#ifndef TCPOPT_CCECHO
#define TCPOPT_CCECHO		13	/* T/TCP CC options (rfc1644) */
#endif

struct tha {
#ifndef INET6
	struct in_addr src;
	struct in_addr dst;
#else
	struct in6_addr src;
	struct in6_addr dst;
#endif /*INET6*/
	u_int port;
};

struct tcp_seq_hash {
	struct tcp_seq_hash *nxt;
	struct tha addr;
	tcp_seq seq;
	tcp_seq ack;
};

#define TSEQ_HASHSIZE 919

/* These tcp optinos do not have the size octet */
#define ZEROLENOPT(o) ((o) == TCPOPT_EOL || (o) == TCPOPT_NOP)

static struct tcp_seq_hash tcp_seq_hash[TSEQ_HASHSIZE];


#ifndef TELNET_PORT
#define TELNET_PORT	23
#endif
#ifndef BGP_PORT
#define BGP_PORT	179
#endif
#define NETBIOS_SSN_PORT 139

void
tcp_print(register const u_char *bp, register u_int length,
	  register const u_char *bp2)
{
	register const struct tcphdr *tp;
	register const struct ip *ip;
	register u_char flags;
	register int hlen;
	register char ch;
	u_short sport, dport, win, urp;
	u_int32_t seq, ack, thseq, thack;
	int threv;
#ifdef INET6
	register const struct ip6_hdr *ip6;
#endif

	tp = (struct tcphdr *)bp;
	ip = (struct ip *)bp2;
#ifdef INET6
	if (ip->ip_v == 6)
		ip6 = (struct ip6_hdr *)bp2;
	else
		ip6 = NULL;
#endif /*INET6*/
	ch = '\0';
	if (!TTEST(tp->th_dport)) {
		(void)printf("%s > %s: [|tcp]",
			ipaddr_string(&ip->ip_src),
			ipaddr_string(&ip->ip_dst));
		return;
	}

	sport = ntohs(tp->th_sport);
	dport = ntohs(tp->th_dport);

#ifdef INET6
	if (ip6) {
		if (ip6->ip6_nxt == IPPROTO_TCP) {
			(void)printf("%s.%s > %s.%s: ",
				ip6addr_string(&ip6->ip6_src),
				tcpport_string(sport),
				ip6addr_string(&ip6->ip6_dst),
				tcpport_string(dport));
		} else {
			(void)printf("%s > %s: ",
				tcpport_string(sport), tcpport_string(dport));
		}
	} else
#endif /*INET6*/
	{
		if (ip->ip_p == IPPROTO_TCP) {
			(void)printf("%s.%s > %s.%s: ",
				ipaddr_string(&ip->ip_src),
				tcpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				tcpport_string(dport));
		} else {
			(void)printf("%s > %s: ",
				tcpport_string(sport), tcpport_string(dport));
		}
	}

	TCHECK(*tp);

	seq = ntohl(tp->th_seq);
	ack = ntohl(tp->th_ack);
	win = ntohs(tp->th_win);
	urp = ntohs(tp->th_urp);

	if (qflag) {
		(void)printf("tcp %d", length - tp->th_off * 4);
		return;
	}
#ifdef TH_ECN
	if ((flags = tp->th_flags) & (TH_SYN|TH_FIN|TH_RST|TH_PUSH|TH_ECN))
#else
	if ((flags = tp->th_flags) & (TH_SYN|TH_FIN|TH_RST|TH_PUSH))
#endif
	{
		if (flags & TH_SYN)
			putchar('S');
		if (flags & TH_FIN)
			putchar('F');
		if (flags & TH_RST)
			putchar('R');
		if (flags & TH_PUSH)
			putchar('P');
#ifdef TH_ECN
		if (flags & TH_ECN)
			putchar('C');
#endif
	} else
		putchar('.');

	if (flags&0xc0) {
		printf(" [");
		if (flags&0x40)
			printf("ECN-Echo");
		if (flags&0x80)
			printf("%sCWR", (flags&0x40) ? "," : "");
		printf("]");
	}

	if (!Sflag && (flags & TH_ACK)) {
		register struct tcp_seq_hash *th;
		register int rev;
		struct tha tha;
		/*
		 * Find (or record) the initial sequence numbers for
		 * this conversation.  (we pick an arbitrary
		 * collating order so there's only one entry for
		 * both directions).
		 */
#ifdef INET6
		bzero(&tha, sizeof(tha));
		rev = 0;
		if (ip6) {
			if (sport > dport) {
				rev = 1;
			} else if (sport == dport) {
			    int i;

			    for (i = 0; i < 4; i++) {
				if (((u_int32_t *)(&ip6->ip6_src))[i] >
				    ((u_int32_t *)(&ip6->ip6_dst))[i]) {
					rev = 1;
					break;
				}
			    }
			}
			if (rev) {
				tha.src = ip6->ip6_dst;
				tha.dst = ip6->ip6_src;
				tha.port = dport << 16 | sport;
			} else {
				tha.dst = ip6->ip6_dst;
				tha.src = ip6->ip6_src;
				tha.port = sport << 16 | dport;
			}
		} else {
			if (sport > dport ||
			    (sport == dport &&
			     ip->ip_src.s_addr > ip->ip_dst.s_addr)) {
				rev = 1;
			}
			if (rev) {
				*(struct in_addr *)&tha.src = ip->ip_dst;
				*(struct in_addr *)&tha.dst = ip->ip_src;
				tha.port = dport << 16 | sport;
			} else {
				*(struct in_addr *)&tha.dst = ip->ip_dst;
				*(struct in_addr *)&tha.src = ip->ip_src;
				tha.port = sport << 16 | dport;
			}
		}
#else
		if (sport < dport ||
		    (sport == dport &&
		     ip->ip_src.s_addr < ip->ip_dst.s_addr)) {
			tha.src = ip->ip_src, tha.dst = ip->ip_dst;
			tha.port = sport << 16 | dport;
			rev = 0;
		} else {
			tha.src = ip->ip_dst, tha.dst = ip->ip_src;
			tha.port = dport << 16 | sport;
			rev = 1;
		}
#endif

		threv = rev;
		for (th = &tcp_seq_hash[tha.port % TSEQ_HASHSIZE];
		     th->nxt; th = th->nxt)
			if (!memcmp((char *)&tha, (char *)&th->addr,
				  sizeof(th->addr)))
				break;

		if (!th->nxt || flags & TH_SYN) {
			/* didn't find it or new conversation */
			if (th->nxt == NULL) {
				th->nxt = (struct tcp_seq_hash *)
					calloc(1, sizeof(*th));
				if (th->nxt == NULL)
					error("tcp_print: calloc");
			}
			th->addr = tha;
			if (rev)
				th->ack = seq, th->seq = ack - 1;
			else
				th->seq = seq, th->ack = ack - 1;
		} else {
	  
			thseq = th->seq;
			thack = th->ack;

			if (rev)
				seq -= th->ack, ack -= th->seq;
			else
				seq -= th->seq, ack -= th->ack;
		}
	}
	hlen = tp->th_off * 4;
	if (hlen > length) {
		(void)printf(" [bad hdr length]");
		return;
	}
	length -= hlen;
	if (vflag > 1 || length > 0 || flags & (TH_SYN | TH_FIN | TH_RST))
		(void)printf(" %u:%u(%d)", seq, seq + length, length);
	if (flags & TH_ACK)
		(void)printf(" ack %u", ack);

	(void)printf(" win %d", win);

	if (flags & TH_URG)
		(void)printf(" urg %d", urp);
	/*
	 * Handle any options.
	 */
	if ((hlen -= sizeof(*tp)) > 0) {
		register const u_char *cp;
		register int i, opt, len, datalen;

		cp = (const u_char *)tp + sizeof(*tp);
		putchar(' ');
		ch = '<';
		while (hlen > 0) {
			putchar(ch);
			TCHECK(*cp);
			opt = *cp++;
			if (ZEROLENOPT(opt))
				len = 1;
			else {
				TCHECK(*cp);
				len = *cp++;	/* total including type, len */
				if (len < 2 || len > hlen)
					goto bad;
				--hlen;		/* account for length byte */
			}
			--hlen;			/* account for type byte */
			datalen = 0;

/* Bail if "l" bytes of data are not left or were not captured  */
#define LENCHECK(l) { if ((l) > hlen) goto bad; TCHECK2(*cp, l); }

			switch (opt) {

			case TCPOPT_MAXSEG:
				(void)printf("mss");
				datalen = 2;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_16BITS(cp));

				break;

			case TCPOPT_EOL:
				(void)printf("eol");
				break;

			case TCPOPT_NOP:
				(void)printf("nop");
				break;

			case TCPOPT_WSCALE:
				(void)printf("wscale");
				datalen = 1;
				LENCHECK(datalen);
				(void)printf(" %u", *cp);
				break;

			case TCPOPT_SACKOK:
				(void)printf("sackOK");
				break;

			case TCPOPT_SACK:
				(void)printf("sack");
				datalen = len - 2;
				if (datalen % 8 != 0) {
					(void)printf(" malformed sack ");
				} else {
					u_int32_t s, e;

					(void)printf(" sack %d ", datalen / 8);
					for (i = 0; i < datalen; i += 8) {
						LENCHECK(i + 4);
						s = EXTRACT_32BITS(cp + i);
						LENCHECK(i + 8);
						e = EXTRACT_32BITS(cp + i + 4);
						if (threv) {
							s -= thseq;
							e -= thseq;
						} else {
							s -= thack;
							e -= thack;
						}
						(void)printf("{%u:%u}", s, e);
					}
					(void)printf(" ");
				}
				break;

			case TCPOPT_ECHO:
				(void)printf("echo");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_ECHOREPLY:
				(void)printf("echoreply");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_TIMESTAMP:
				(void)printf("timestamp");
				datalen = 8;
				LENCHECK(4);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp + 4));
				break;

			case TCPOPT_CC:
				(void)printf("cc");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_CCNEW:
				(void)printf("ccnew");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_CCECHO:
				(void)printf("ccecho");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			default:
				(void)printf("opt-%d:", opt);
				datalen = len - 2;
				for (i = 0; i < datalen; ++i) {
					LENCHECK(i);
					(void)printf("%02x", cp[i]);
				}
				break;
			}

			/* Account for data printed */
			cp += datalen;
			hlen -= datalen;

			/* Check specification against observed length */
			++datalen;			/* option octet */
			if (!ZEROLENOPT(opt))
				++datalen;		/* size octet */
			if (datalen != len)
				(void)printf("[len %d]", len);
			ch = ',';
			if (opt == TCPOPT_EOL)
				break;
		}
		putchar('>');
	}

	if (length <= 0)
		return;

	/*
	 * Decode payload if necessary.
	 */
	bp += (tp->th_off * 4);
	if (!qflag && vflag && length > 0
	 && (sport == TELNET_PORT || dport == TELNET_PORT))
		telnet_print(bp, length);
	else if (sport == BGP_PORT || dport == BGP_PORT)
		bgp_print(bp, length);
	else if (sport == NETBIOS_SSN_PORT || dport == NETBIOS_SSN_PORT)
		nbt_tcp_print(bp, length);
	return;
bad:
	fputs("[bad opt]", stdout);
	if (ch != '\0')
		putchar('>');
	return;
trunc:
	fputs("[|tcp]", stdout);
	if (ch != '\0')
		putchar('>');
}

