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
    "@(#) $Header: print-tcp.c,v 1.18 92/05/25 14:29:04 mccanne Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#ifdef X10
#include <X/X.h>
#include <X/Xproto.h>
#endif

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
tcp_print(tp, length, ip)
	register struct tcphdr *tp;
	register int length;
	register struct ip *ip;
{
	register u_char flags;
	register int hlen;

	if ((u_char *)(tp + 1)  > snapend) {
		printf("[|tcp]");
		return;
	}
	if (length < sizeof(struct tcphdr)) {
		(void)printf("truncated-tcp %d", length);
		return;
	}

	NTOHS(tp->th_sport);
	NTOHS(tp->th_dport);
	NTOHL(tp->th_seq);
	NTOHL(tp->th_ack);
	NTOHS(tp->th_win);
	NTOHS(tp->th_urp);

	(void)printf("%s.%s > %s.%s: ",
		ipaddr_string(&ip->ip_src), tcpport_string(tp->th_sport),
		ipaddr_string(&ip->ip_dst), tcpport_string(tp->th_dport));

	if (!qflag) {
#ifdef X10
		register int be;

		if ((be = (tp->th_sport == X_TCP_BI_PORT ||
		    tp->th_dport == X_TCP_BI_PORT)) ||
		    tp->th_sport == X_TCP_LI_PORT ||
		    tp->th_dport == X_TCP_LI_PORT) {
			register XReq *xp = (XReq *)(tp + 1);

			x10_print(xp, length - sizeof(struct tcphdr), be);
			return;
		}
#endif
	}

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
		if (tp->th_sport < tp->th_dport ||
		    (tp->th_sport == tp->th_dport &&
		     ip->ip_src.s_addr < ip->ip_dst.s_addr)) {
			tha.src = ip->ip_src, tha.dst = ip->ip_dst;
			tha.port = tp->th_sport << 16 | tp->th_dport;
			rev = 0;
		} else {
			tha.src = ip->ip_dst, tha.dst = ip->ip_src;
			tha.port = tp->th_dport << 16 | tp->th_sport;
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
				th->ack = tp->th_seq, th->seq = tp->th_ack - 1;
			else
				th->seq = tp->th_seq, th->ack = tp->th_ack - 1;
		} else {
			if (rev)
				tp->th_seq -= th->ack, tp->th_ack -= th->seq;
			else
				tp->th_seq -= th->seq, tp->th_ack -= th->ack;
		}
	}
	hlen = tp->th_off * 4;
	length -= hlen;
	if (length > 0 || flags & (TH_SYN | TH_FIN | TH_RST))
		(void)printf(" %lu:%lu(%d)", tp->th_seq, tp->th_seq + length, 
			     length);
	if (flags & TH_ACK)
		(void)printf(" ack %lu", tp->th_ack);

	(void)printf(" win %d", tp->th_win);

	if (flags & TH_URG)
		(void)printf(" urg %d", tp->th_urp);
	/*
	 * Handle any options.
	 */
	if ((hlen -= sizeof(struct tcphdr)) > 0) {
		register u_char *cp = (u_char *)tp + sizeof(struct tcphdr);
		int i;
		char ch = '<';

		putchar(' ');
		while (--hlen >= 0) {
			putchar(ch);
			switch (*cp++) {
			case TCPOPT_MAXSEG:
			{
				u_short mss;
#ifdef TCPDUMP_ALIGN
				bcopy((char *)cp + 1, (char *)&mss, 
				      sizeof(mss));
#else
				mss = *(u_short *)(cp + 1);
#endif				
				(void)printf("mss %d", ntohs(mss));
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
				u_long v;
#ifdef TCPDUMP_ALIGN
				bcopy((char *)cp + 1, (char *)&v, 
				      sizeof(v));
#else
				v = *(u_long *)(cp + 1);
#endif				
				(void)printf("echo %lu", v);
				if (*cp != 6)
					(void)printf("[len %d]", *cp);
				cp += 5;
				hlen -= 5;
				break;
			}
			case TCPOPT_ECHOREPLY:
			{
				u_long v;
#ifdef TCPDUMP_ALIGN
				bcopy((char *)cp + 1, (char *)&v, 
				      sizeof(v));
#else
				v = *(u_long *)(cp + 1);
#endif				
				(void)printf("echoreply %lu", v);
				if (*cp != 6)
					(void)printf("[len %d]", *cp);
				cp += 5;
				hlen -= 5;
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

