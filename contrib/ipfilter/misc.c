/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#if (SOLARIS2 >= 7)
# define _SYS_VARARGS_H
# define _VARARGS_H
#endif
#if defined(__STDC__)
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ip_compat.h"
#include <netinet/tcpip.h>
#include "ip_fil.h"
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)misc.c	1.3 2/4/96 (C) 1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: misc.c,v 2.2.2.8 2002/04/26 10:24:24 darrenr Exp $";
#endif

extern	int	opts;


void	printpacket(ip)
ip_t	*ip;
{
	tcphdr_t	*tcp;
	u_short	len;

	if (ip->ip_v == 4)
		len = ntohs(ip->ip_len);
	else if (ip->ip_v == 6)
		len = ntohs(((u_short *)ip)[2]) + 40;
	else
		len = 0;

	if ((opts & OPT_HEX) == OPT_HEX) {
		u_char *s;
		int i;

		for (s = (u_char *)ip, i = 0; i < len; i++) {
			printf("%02x", *s++ & 0xff);
			if (len - i > 1) {
				i++;
				printf("%02x", *s++ & 0xff);
			}
			if (i + 1 != len)
				putchar(' ');
		}
		putchar('\n');
		return;
	}

	if (ip->ip_v == 6) {
		printpacket6(ip);
		return;
	}

	tcp = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
	printf("ip %d(%d) %d", ntohs(ip->ip_len), ip->ip_hl << 2, ip->ip_p);
	if (ip->ip_off & IP_OFFMASK)
		printf(" @%d", ip->ip_off << 3);
	(void)printf(" %s", inet_ntoa(ip->ip_src));
	if (!(ip->ip_off & IP_OFFMASK))
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_sport));
	(void)printf(" > ");
	(void)printf("%s", inet_ntoa(ip->ip_dst));
	if (!(ip->ip_off & IP_OFFMASK)) {
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_dport));
		if ((ip->ip_p == IPPROTO_TCP) && (tcp->th_flags)) {
			putchar(' ');
			if (tcp->th_flags & TH_FIN)
				putchar('F');
			if (tcp->th_flags & TH_SYN)
				putchar('S');
			if (tcp->th_flags & TH_RST)
				putchar('R');
			if (tcp->th_flags & TH_PUSH)
				putchar('P');
			if (tcp->th_flags & TH_ACK)
				putchar('A');
			if (tcp->th_flags & TH_URG)
				putchar('U');
			if (tcp->th_flags & TH_ECN)
				putchar('E');
			if (tcp->th_flags & TH_CWR)
				putchar('C');
		}
	}
	putchar('\n');
}


/*
 * This is meant to work without the IPv6 header files being present or
 * the inet_ntop() library.
 */
void	printpacket6(ip)
ip_t	*ip;
{
	u_char *buf, p, hops;
	u_short plen, *addrs;
	tcphdr_t *tcp;
	u_32_t flow;

	buf = (u_char *)ip;
	tcp = (tcphdr_t *)(buf + 40);
	p = buf[6];
	hops = buf[7];
	flow = ntohl(*(u_32_t *)buf);
	flow &= 0xfffff;
	plen = ntohs(*((u_short *)buf +2));
	addrs = (u_short *)buf + 4;

	printf("ip6/%d %d %#x %d", buf[0] & 0xf, plen, flow, p);
	printf(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ntohs(addrs[0]), ntohs(addrs[1]), ntohs(addrs[2]),
		ntohs(addrs[3]), ntohs(addrs[4]), ntohs(addrs[5]),
		ntohs(addrs[6]), ntohs(addrs[7]));
	if (plen >= 4)
		if (p == IPPROTO_TCP || p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_sport));
	printf(" >");
	addrs += 8;
	printf(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ntohs(addrs[0]), ntohs(addrs[1]), ntohs(addrs[2]),
		ntohs(addrs[3]), ntohs(addrs[4]), ntohs(addrs[5]),
		ntohs(addrs[6]), ntohs(addrs[7]));
	if (plen >= 4)
		if (p == IPPROTO_TCP || p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_dport));
	putchar('\n');
}


#if defined(__STDC__)
void	verbose(char *fmt, ...)
#else
void	verbose(fmt, va_alist)
char	*fmt;
va_dcl
#endif
{
	va_list pvar;

	va_start(pvar, fmt);
	if (opts & OPT_VERBOSE)
		vprintf(fmt, pvar);
	va_end(pvar);
}


#ifdef	__STDC__
void	debug(char *fmt, ...)
#else
void	debug(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list pvar;

	va_start(pvar, fmt);
	if (opts & OPT_DEBUG)
		vprintf(fmt, pvar);
	va_end(pvar);
}
