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
    "$Id: print-udp.c,v 1.4 1995/08/29 19:48:11 wollman Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#undef NOERROR					/* Solaris sucks */
#include <arpa/nameser.h>
#include <arpa/tftp.h>

#ifdef SOLARIS
#include <tiuser.h>
#endif
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/svc.h>
#include <rpc/rpc_msg.h>

#include <errno.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "appletalk.h"

#include "nfsv2.h"
#include "bootp.h"

extern int packettype;

static void
vat_print(const void *hdr, int len, register const struct udphdr *up)
{
	/* vat/vt audio */
	u_int ts = *(u_short *)hdr;
	if ((ts & 0xf060) != 0) {
		/* probably vt */
		(void)printf(" udp/vt %d %d / %d",
			     ntohs(up->uh_ulen) - sizeof(*up),
			     ts & 0x3ff, ts >> 10);
	} else {
		/* probably vat */
		u_int i0 = ((u_int *)hdr)[0];
		u_int i1 = ((u_int *)hdr)[1];
		printf(" udp/vat %d c%d %u%s",
			ntohs(up->uh_ulen) - sizeof(*up) - 8,
			i0 & 0xffff,
			i1, i0 & 0x800000? "*" : "");
		/* audio format */
		if (i0 & 0x1f0000)
			printf(" f%d", (i0 >> 16) & 0x1f);
		if (i0 & 0x3f000000)
			printf(" s%d", (i0 >> 24) & 0x3f);
	}
}

static void
rtp_print(const void *hdr, int len, register const struct udphdr *up)
{
	/* rtp v1 video */
	u_int *ip = (u_int *)hdr;
	u_int i0 = ((u_int *)hdr)[0];
	u_int i1 = ((u_int *)hdr)[1];
	u_int hasopt = i0 & 0x800000;
	u_int contype = (i0 >> 16) & 0x3f;
	printf(" udp/rtp %d c%d %s%s %d",
		ntohs(up->uh_ulen) - sizeof(*up) - 8,
		contype,
		hasopt? "+" : "",
		i0 & 0x400000? "*" : "",
		i0 & 0xffff);
	if (contype == 31) {
		ip += 2;
		len >>= 2;
		len -= 2;
		if (hasopt) {
			u_int i2, optlen;
			do {
				i2 = ip[0];
				optlen = (i2 >> 16) & 0xff;
				if (optlen == 0 || optlen > len) {
					printf(" !opt");
					return;
				}
				ip += optlen;
			} while ((int)i2 >= 0);
		}
		printf(" 0x%04x", ip[0] >> 16);
	}
	if (vflag)
		printf(" %u", i1);
}

/* XXX probably should use getservbyname() and cache answers */
#define TFTP_PORT 69		/*XXX*/
#define KERBEROS_PORT 80	/*XXX*/
#define SUNRPC_PORT 111		/*XXX*/
#define SNMP_PORT 161		/*XXX*/
#define NTP_PORT 123		/*XXX*/
#define SNMPTRAP_PORT 162	/*XXX*/
#define RIP_PORT 520		/*XXX*/
#define KERBEROS_SEC_PORT 750	/*XXX*/
#define RSVP_ENCAP_PORT 3455	/*XXX*/

void
udp_print(register const u_char *bp, int length, register const u_char *bp2)
{
	register const struct udphdr *up;
	register const struct ip *ip;
	register const u_char *cp;
	u_short sport, dport, ulen;

	up = (struct udphdr *)bp;
	ip = (struct ip *)bp2;
	cp = (u_char *)(up + 1);
	if (cp > snapend) {
		printf("[|udp]");
		return;
	}
	if (length < sizeof(struct udphdr)) {
		(void)printf(" truncated-udp %d", length);
		return;
	}
	length -= sizeof(struct udphdr);

	sport = ntohs(up->uh_sport);
	dport = ntohs(up->uh_dport);
	ulen = ntohs(up->uh_ulen);
	if (packettype) {
		register struct rpc_msg *rp;
		enum msg_type direction;

		switch (packettype) {
		case 1:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			vat_print((void *)(up + 1), length, up);
			break;
		case 2:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			wb_print((void *)(up + 1), length);
			break;
		case 3:
			rp = (struct rpc_msg *)(up + 1);
			direction = (enum msg_type)ntohl(rp->rm_direction);
			if (direction == CALL)
				sunrpcrequest_print((u_char *)rp, length,
				    (u_char *)ip);
			else
				nfsreply_print((u_char *)rp, length,
				    (u_char *)ip);			/*XXX*/
			break;
		case 4:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			rtp_print((void *)(up + 1), length, up);
			break;
		}
		return;
	}

	if (! qflag) {
		register struct rpc_msg *rp;
		enum msg_type direction;

		rp = (struct rpc_msg *)(up + 1);
		direction = (enum msg_type)ntohl(rp->rm_direction);
		if (dport == NFS_PORT && direction == CALL) {
			nfsreq_print((u_char *)rp, length, (u_char *)ip);
			return;
		}
		else if (sport == NFS_PORT && direction == REPLY) {
			nfsreply_print((u_char *)rp, length, (u_char *)ip);
			return;
		}
#ifdef notdef
		else if (dport == SUNRPC_PORT && direction == CALL) {
			sunrpcrequest_print((u_char *)rp, length, (u_char *)ip);
			return;
		}
#endif
		else if (((struct LAP *)cp)->type == lapDDP &&
		    (atalk_port(sport) || atalk_port(dport))) {
			if (vflag)
				fputs("kip ", stdout);
			atalk_print(cp, length);
			return;
		}
	}
	(void)printf("%s.%s > %s.%s:",
		ipaddr_string(&ip->ip_src), udpport_string(sport),
		ipaddr_string(&ip->ip_dst), udpport_string(dport));

	if (!qflag) {
#define ISPORT(p) (dport == (p) || sport == (p))
		if (ISPORT(NAMESERVER_PORT))
			ns_print((const u_char *)(up + 1), length);
		else if (ISPORT(TFTP_PORT))
			tftp_print((const u_char *)(up + 1), length);
		else if (ISPORT(IPPORT_BOOTPC) || ISPORT(IPPORT_BOOTPS))
			bootp_print((const u_char *)(up + 1), length,
			    sport, dport);
		else if (dport == RIP_PORT)
			rip_print((const u_char *)(up + 1), length);
		else if (ISPORT(SNMP_PORT) || ISPORT(SNMPTRAP_PORT))
			snmp_print((const u_char *)(up + 1), length);
		else if (ISPORT(NTP_PORT))
			ntp_print((const u_char *)(up + 1), length);
		else if (ISPORT(KERBEROS_PORT) || ISPORT(KERBEROS_SEC_PORT))
			krb_print((const void *)(up + 1), length);
#if 0
		else if (ISPORT(RSVP_ENCAP_PORT))
			rsvpUDP_print((const u_char *)(up + 1), length);
#endif
		else if (dport == 3456)
			vat_print((const void *)(up + 1), length, up);
		/*
		 * Kludge in test for whiteboard packets.
		 */
		else if (dport == 4567)
			wb_print((const void *)(up + 1), length);
		else
			(void)printf(" udp %d", ulen - sizeof(*up));
#undef ISPORT
	} else
		(void)printf(" udp %d", ulen - sizeof(*up));
}
