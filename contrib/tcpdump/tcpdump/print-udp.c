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
    "@(#) $Header: /a/cvs/386BSD/src/contrib/tcpdump/tcpdump/print-udp.c,v 1.2 1993/09/15 20:27:25 jtc Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <arpa/nameser.h>
#include <arpa/tftp.h>
#include <errno.h>
#include <sys/time.h>
#include <rpc/rpc.h>

#include "interface.h"
/* These must come after interface.h for BSD. */
#if BSD >= 199006
#include <sys/ucred.h>
#include <nfs/nfsv2.h>
#endif
#include <nfs/nfs.h>

#include "addrtoname.h"
#include "appletalk.h"

#include "bootp.h"

/* XXX probably should use getservbyname() and cache answers */
#define TFTP_PORT 69		/*XXX*/
#define SUNRPC_PORT 111		/*XXX*/
#define SNMP_PORT 161		/*XXX*/
#define NTP_PORT 123		/*XXX*/
#define SNMPTRAP_PORT 162	/*XXX*/
#define RIP_PORT 520		/*XXX*/

void
udp_print(up, length, ip)
	register struct udphdr *up;
	int length;
	register struct ip *ip;
{
	register u_char  *cp = (u_char *)(up + 1);

	if (cp > snapend) {
		printf("[|udp]");
		return;
	}
	if (length < sizeof(struct udphdr)) {
		(void)printf(" truncated-udp %d", length);
		return;
	}
	length -= sizeof(struct udphdr);

	NTOHS(up->uh_sport);
	NTOHS(up->uh_dport);
	NTOHS(up->uh_ulen);

	if (! qflag) {
		register struct rpc_msg *rp;
		enum msg_type direction;

		rp = (struct rpc_msg *)(up + 1);
		direction = (enum msg_type)ntohl(rp->rm_direction);
		if (up->uh_dport == NFS_PORT && direction == CALL) {
			nfsreq_print(rp, length, ip);
			return;
		}
		else if (up->uh_sport == NFS_PORT && direction == REPLY) {
			nfsreply_print(rp, length, ip);
			return;
		}
#ifdef notdef
		else if (up->uh_dport == SUNRPC_PORT && direction == CALL) {
			sunrpcrequest_print(rp, length, ip);
			return;
		}
#endif
		else if (cp[2] == 2 && (atalk_port(up->uh_sport) ||
			 atalk_port(up->uh_dport))) {
			ddp_print((struct atDDP *)(&cp[3]), length - 3);
			return;
		}
	}
	(void)printf("%s.%s > %s.%s:",
		ipaddr_string(&ip->ip_src), udpport_string(up->uh_sport),
		ipaddr_string(&ip->ip_dst), udpport_string(up->uh_dport));

	if (!qflag) {
#define ISPORT(p) (up->uh_dport == (p) || up->uh_sport == (p))
		if (ISPORT(NAMESERVER_PORT))
			ns_print((HEADER *)(up + 1), length);
		else if (ISPORT(TFTP_PORT))
			tftp_print((struct tftphdr *)(up + 1), length);
		else if (ISPORT(IPPORT_BOOTPC) || ISPORT(IPPORT_BOOTPS))
			bootp_print((struct bootp *)(up + 1), length,
			    up->uh_sport, up->uh_dport);
		else if (up->uh_dport == RIP_PORT)
			rip_print((u_char *)(up + 1), length);
		else if (ISPORT(SNMP_PORT) || ISPORT(SNMPTRAP_PORT))
			snmp_print((u_char *)(up + 1), length);
		else if (ISPORT(NTP_PORT))
			ntp_print((struct ntpdata *)(up + 1), length);
		else
			(void)printf(" udp %d", up->uh_ulen - sizeof(*up));
#undef ISPORT
	} else
		(void)printf(" udp %d", up->uh_ulen - sizeof(*up));
}
