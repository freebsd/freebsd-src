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
    "@(#) $Header: print-tcp.c,v 1.28 94/06/16 01:26:40 mccanne Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"

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

struct tha {
	struct in_addr src;
	struct in_addr dst;
	u_int port;
};

struct tcp_seq_hash {
	struct tcp_seq_hash *nxt;
	struct tha addr;
	tcp_seq seq;
	tcp_seq ack;
};

#define TSEQ_HASHSIZE 919

static struct tcp_seq_hash tcp_seq_hash[TSEQ_HASHSIZE];


void
tcp_print(register const u_char *bp, register int length,
	  register const u_char *bp2)
{
	register const struct tcphdr *tp;
	register const struct ip *ip;
	register u_char flags;
	register int hlen;
	u_short sport, dport, win, urp;
	u_int32 seq, ack;

	tp = (struct tcphdr *)bp;
	ip = (struct ip *)bp2;
	if ((const u_char *)(tp + 1)  > snapend) {
		printf("[|tcp]");
		return;
	}
	if (length < sizeof(struct tcphdr)) {
		(void)printf("truncated-tcp %d", length);
		return;
	}

	sport = ntohs(tp->th_sport);
	dport = ntohs(tp->th_dport);
	seq = ntohl(tp->th_seq);
	ack = ntohl(tp->th_ack);
	win = ntohs(tp->th_win);
	urp = ntohs(tp->th_urp);

	(void)printf("%s.%s > %s.%s: ",
		ipaddr_string(&ip->ip_src), tcpport_string(sport),
		ipaddr_string(&ip->ip_dst), tcpport_string(dport));

	if (qflag) {
		(void)printf("tcp %d", length - tp->th_off * 4);
		return;
	}
	if ((flags = tp->th_flags) & (TH_SYN|TH_FIN|TH_RST|TH_PUSH)) {
		if (flags & TH_SYN)
			putchar('S');
		if (flags & TH_FIN)
			putchar('F');
		if (flags & TH_RST)
			putchar('R');
		if (flags & TH_PUSH)
			putchar('P');
	} else
		putchar('.');

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

		for (th = &tcp_seq_hash[tha.port % TSEQ_HASHSIZE];
		     th->nxt; th = th->nxt)
			if (!bcmp((char *)&tha, (char *)&th->addr,
				  sizeof(th->addr)))
				break;

		if (!th->nxt || flags & TH_SYN) {
			/* didn't find it or new conversation */
			if (!th->nxt)
				th->nxt = (struct tcp_seq_hash *)
					calloc(1, sizeof (*th));
			th->addr = tha;
			if (rev)
				th->ack = seq, th->seq = ack - 1;
			else
				th->seq = seq, th->ack = ack - 1;
		} else {
			if (rev)
				seq -= th->ack, ack -= th->seq;
			else
				seq -= th->seq, ack -= th->ack;
		}
	}
	hlen = tp->th_off * 4;
	length -= hlen;
	if (length > 0 || flags & (TH_SYN | TH_FIN | TH_RST))
		(void)printf(" %u:%u(%d)", seq, seq + length, length);
	if (flags & TH_ACK)
		(void)printf(" ack %u", ack);

	(void)printf(" win %d", win);

	if (flags & TH_URG)
		(void)printf(" urg %d", urp);
	/*
	 * Handle any options.
	 */
	if ((hlen -= sizeof(struct tcphdr)) > 0) {
		register const u_char *cp = (const u_char *)tp + sizeof(*tp);
		int i;
		char ch = '<';

		putchar(' ');
		while (--hlen >= 0) {
			putchar(ch);
			switch (*cp++) {
			case TCPOPT_MAXSEG:
			{
				(void)printf("mss %d", cp[1] << 8 | cp[2]);
				if (*cp != 4)
					(void)printf("[len %d]", *cp);
				cp += 3;
				hlen -= 3;
				break;
			}
			case TCPOPT_EOL:
				(void)printf("eol");
				break;
			case TCPOPT_NOP:
				(void)printf("nop");
				break;
			case TCPOPT_WSCALE:
				(void)printf("wscale %d", cp[1]);
				if (*cp != 3)
					(void)printf("[len %d]", *cp);
				cp += 2;
				hlen -= 2;
				break;
			case TCPOPT_SACKOK:
				(void)printf("sackOK");
				if (*cp != 2)
					(void)printf("[len %d]", *cp);
				cp += 1;
				hlen -= 1;
				break;
			case TCPOPT_ECHO:
			{
				(void)printf("echo %u",
					     cp[1] << 24 | cp[2] << 16 |
					     cp[3] << 8 | cp[4]);
				if (*cp != 6)
					(void)printf("[len %d]", *cp);
				cp += 5;
				hlen -= 5;
				break;
			}
			case TCPOPT_ECHOREPLY:
			{
				(void)printf("echoreply %u",
					     cp[1] << 24 | cp[2] << 16 |
					     cp[3] << 8 | cp[4]);
				if (*cp != 6)
					(void)printf("[len %d]", *cp);
				cp += 5;
				hlen -= 5;
				break;
			}
			case TCPOPT_TIMESTAMP:
			{
				(void)printf("timestamp %lu %lu",
					     cp[1] << 24 | cp[2] << 16 |
					     cp[3] << 8 | cp[4],
					     cp[5] << 24 | cp[6] << 16 |
					     cp[7] << 8 | cp[8]);
				if (*cp != 10)
					(void)printf("[len %d]", *cp);
				cp += 9;
				hlen -= 9;
				break;
  			}
			default:
				(void)printf("opt-%d:", cp[-1]);
				for (i = *cp++ - 2, hlen -= i + 1; i > 0; --i)
					(void)printf("%02x", *cp++);
				break;
			}
			ch = ',';
		}
		putchar('>');
	}
}

