/*	$OpenBSD: print-cnfp.c,v 1.2 1998/06/25 20:26:59 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Cisco NetFlow protocol */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-cnfp.c,v 1.8 2001/09/17 21:57:58 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"

#include "tcp.h"

struct nfhdr {
	u_int32_t	ver_cnt;	/* version [15], and # of records */
	u_int32_t	msys_uptime;
	u_int32_t	utc_sec;
	u_int32_t	utc_nsec;
	u_int32_t	sequence;	/* v5 flow sequence number */
	u_int32_t	reserved;	/* v5 only */
};

struct nfrec {
	struct in_addr	src_ina;
	struct in_addr	dst_ina;
	struct in_addr	nhop_ina;
	u_int32_t	ifaces;		/* src,dst ifaces */
	u_int32_t	packets;
	u_int32_t	octets;
	u_int32_t	start_time;	/* sys_uptime value */
	u_int32_t	last_time;	/* sys_uptime value */
	u_int32_t	ports;		/* src,dst ports */
	u_int32_t	proto_tos;	/* proto, tos, pad, flags(v5) */
	u_int32_t	asses;		/* v1: flags; v5: src,dst AS */
	u_int32_t	masks;		/* src,dst addr prefix; v6: encaps */
	struct in_addr	peer_nexthop;	/* v6: IP address of the nexthop within the peer (FIB)*/
};

void
cnfp_print(const u_char *cp, u_int len, const u_char *bp)
{
	register const struct nfhdr *nh;
	register const struct nfrec *nr;
	register const struct ip *ip;
	struct protoent *pent;
	int nrecs, ver;
	time_t t;

	ip = (const struct ip *)bp;
	nh = (const struct nfhdr *)cp;

	if ((const u_char *)(nh + 1) > snapend)
		return;

	nrecs = ntohl(nh->ver_cnt) & 0xffff;
	ver = (ntohl(nh->ver_cnt) & 0xffff0000) >> 16;
	t = ntohl(nh->utc_sec);
/*	(p = ctime(&t))[24] = '\0'; */

	printf("NetFlow v%x, %u.%03u uptime, %u.%09u, ", ver,
	       (unsigned)ntohl(nh->msys_uptime)/1000,
	       (unsigned)ntohl(nh->msys_uptime)%1000,
	       (unsigned)ntohl(nh->utc_sec), (unsigned)ntohl(nh->utc_nsec));

	if (ver == 5 || ver == 6) {
		printf("#%u, ", (unsigned)htonl(nh->sequence));
		nr = (const struct nfrec *)&nh[1];
		snaplen -= 24;
	} else {
		nr = (const struct nfrec *)&nh->sequence;
		snaplen -= 16;
	}

	printf("%2u recs", nrecs);

	for (; nrecs-- && (const u_char *)(nr + 1) <= snapend; nr++) {
		char buf[20];
		char asbuf[20];

		printf("\n  started %u.%03u, last %u.%03u",
		       (unsigned)ntohl(nr->start_time)/1000,
		       (unsigned)ntohl(nr->start_time)%1000,
		       (unsigned)ntohl(nr->last_time)/1000,
		       (unsigned)ntohl(nr->last_time)%1000);

		asbuf[0] = buf[0] = '\0';
		if (ver == 5 || ver == 6) {
			snprintf(buf, sizeof(buf), "/%u",
				 (unsigned)(ntohl(nr->masks) >> 24) & 0xff);
			snprintf(asbuf, sizeof(asbuf), ":%u",
				 (unsigned)(ntohl(nr->asses) >> 16) & 0xffff);
		}
		printf("\n    %s%s%s:%u ", inet_ntoa(nr->src_ina), buf, asbuf,
			(unsigned)ntohl(nr->ports) >> 16);

		if (ver == 5 || ver ==6) {
			snprintf(buf, sizeof(buf), "/%d",
				 (unsigned)(ntohl(nr->masks) >> 16) & 0xff);
			snprintf(asbuf, sizeof(asbuf), ":%u",
				 (unsigned)ntohl(nr->asses) & 0xffff);
		}
		printf("> %s%s%s:%u ", inet_ntoa(nr->dst_ina), buf, asbuf,
			(unsigned)ntohl(nr->ports) & 0xffff);

		printf(">> %s\n    ", inet_ntoa(nr->nhop_ina));

		pent = getprotobynumber((ntohl(nr->proto_tos) >> 8) & 0xff);
		if (!pent || nflag)
			printf("%u ",
			       (unsigned)(ntohl(nr->proto_tos) >> 8) & 0xff);
		else
			printf("%s ", pent->p_name);

		/* tcp flags for tcp only */
		if (pent && pent->p_proto == IPPROTO_TCP) {
			int flags;
			if (ver == 1)
				flags = (ntohl(nr->asses) >> 24) & 0xff;
			else
				flags = (ntohl(nr->proto_tos) >> 16) & 0xff;
			if (flags & TH_FIN)	putchar('F');
			if (flags & TH_SYN)	putchar('S');
			if (flags & TH_RST)	putchar('R');
			if (flags & TH_PUSH)	putchar('P');
			if (flags & TH_ACK)	putchar('A');
			if (flags & TH_URG)	putchar('U');
			if (flags)
				putchar(' ');
		}

		buf[0]='\0';
		if (ver == 6) {
			snprintf(buf, sizeof(buf), "(%u<>%u encaps)",
				 (unsigned)(ntohl(nr->masks) >> 8) & 0xff,
				 (unsigned)(ntohl(nr->masks)) & 0xff);
		}
		printf("tos %u, %u (%u octets) %s",
		       (unsigned)ntohl(nr->proto_tos) & 0xff,
		       (unsigned)ntohl(nr->packets),
		       (unsigned)ntohl(nr->octets), buf);
	}
}
