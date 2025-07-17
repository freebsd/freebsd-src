
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"

#ifndef	IP_OFFMASK
# define	IP_OFFMASK	0x3fff
#endif

void
printpacket(int dir, mb_t *m)
{
	u_short len, off;
	tcphdr_t *tcp;
	uint16_t tcpflags;
	ip_t *ip;

	ip = MTOD(m, ip_t *);

	if (IP_V(ip) == 6) {
#ifdef USE_INET6
		len = ntohs(((ip6_t *)ip)->ip6_plen);
#else
		len = ntohs(((u_short *)ip)[2]);
#endif
		len += 40;
	} else {
		len = ntohs(ip->ip_len);
	}
	ASSERT(len == msgdsize(m));

	if ((opts & OPT_HEX) == OPT_HEX) {
		u_char *s;
		int i;

		for (; m != NULL; m = m->mb_next) {
			len = m->mb_len;
			for (s = (u_char *)m->mb_data, i = 0; i < len; i++) {
				PRINTF("%02x", *s++ & 0xff);
				if (len - i > 1) {
					i++;
					PRINTF("%02x", *s++ & 0xff);
				}
				putchar(' ');
			}
		}
		putchar('\n');
		putchar('\n');
		return;
	}

	if (IP_V(ip) == 6) {
		printpacket6(dir, m);
		return;
	}

	if (dir)
		PRINTF("> ");
	else
		PRINTF("< ");

	PRINTF("%s ", IFNAME(m->mb_ifp));

	off = ntohs(ip->ip_off);
	tcp = (struct tcphdr *)((char *)ip + (IP_HL(ip) << 2));
	PRINTF("ip #%d %d(%d) %d", ntohs(ip->ip_id), ntohs(ip->ip_len),
	       IP_HL(ip) << 2, ip->ip_p);
	if (off & IP_OFFMASK)
		PRINTF(" @%d", off << 3);
	PRINTF(" %s", inet_ntoa(ip->ip_src));
	if (!(off & IP_OFFMASK))
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			PRINTF(",%d", ntohs(tcp->th_sport));
	PRINTF(" > ");
	PRINTF("%s", inet_ntoa(ip->ip_dst));
	if (!(off & IP_OFFMASK)) {
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			PRINTF(",%d", ntohs(tcp->th_dport));
		if ((ip->ip_p == IPPROTO_TCP) &&
		    ((tcpflags = __tcp_get_flags(tcp)) != 0)) {
			putchar(' ');
			if (tcpflags & TH_FIN)
				putchar('F');
			if (tcpflags & TH_SYN)
				putchar('S');
			if (tcpflags & TH_RST)
				putchar('R');
			if (tcpflags & TH_PUSH)
				putchar('P');
			if (tcpflags & TH_ACK)
				putchar('A');
			if (tcpflags & TH_URG)
				putchar('U');
			if (tcpflags & TH_ECN)
				putchar('E');
			if (tcpflags & TH_CWR)
				putchar('W');
			if (tcpflags & TH_AE)
				putchar('e');
		}
	}

	putchar('\n');
}
