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
static char rcsid[] =
    "@(#) $Header: print-udp.c,v 1.55 96/07/23 14:17:28 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#undef NOERROR					/* Solaris sucks */
#undef T_UNSPEC					/* SINIX does too */
#include <arpa/nameser.h>
#include <arpa/tftp.h>

#include <rpc/rpc.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "appletalk.h"

#include "nfsv2.h"
#include "bootp.h"

struct rtcphdr {
	u_short rh_flags;	/* T:2 P:1 CNT:5 PT:8 */
	u_short rh_len;		/* length of message (in bytes) */
	u_int rh_ssrc;		/* synchronization src id */
};

typedef struct {
	u_int upper;		/* more significant 32 bits */
	u_int lower;		/* less significant 32 bits */
} ntp64;

/*
 * Sender report.
 */
struct rtcp_sr {
	ntp64 sr_ntp;		/* 64-bit ntp timestamp */
	u_int sr_ts;		/* reference media timestamp */
	u_int sr_np;		/* no. packets sent */
	u_int sr_nb;		/* no. bytes sent */
};

/*
 * Receiver report.
 * Time stamps are middle 32-bits of ntp timestamp.
 */
struct rtcp_rr {
	u_int rr_srcid;	/* sender being reported */
	u_int rr_nr;		/* no. packets received */
	u_int rr_np;		/* no. packets predicted */
	u_int rr_dv;		/* jitter (delay variance) */
	u_int rr_lsr;		/* orig. ts from last rr from this src  */
	u_int rr_dlsr;		/* time from recpt of last rr to xmit time */
};

/*XXX*/
#define RTCP_PT_SR	0
#define RTCP_PT_RR	1
#define RTCP_PT_SDES	2
#define 	RTCP_SDES_CNAME	1
#define 	RTCP_SDES_NAME	2
#define 	RTCP_SDES_EMAIL	3
#define 	RTCP_SDES_PHONE	4
#define 	RTCP_SDES_LOC	5
#define 	RTCP_SDES_TOOL	6
#define 	RTCP_SDES_TXT	7
#define RTCP_PT_BYE	3
#define RTCP_PT_APP	4

static void
vat_print(const void *hdr, u_int len, register const struct udphdr *up)
{
	/* vat/vt audio */
	u_int ts = *(u_short *)hdr;
	if ((ts & 0xf060) != 0) {
		/* probably vt */
		(void)printf(" udp/vt %u %d / %d",
			     (u_int32_t)(ntohs(up->uh_ulen) - sizeof(*up)),
			     ts & 0x3ff, ts >> 10);
	} else {
		/* probably vat */
		u_int i0 = ntohl(((u_int *)hdr)[0]);
		u_int i1 = ntohl(((u_int *)hdr)[1]);
		printf(" udp/vat %u c%d %u%s",
			(u_int32_t)(ntohs(up->uh_ulen) - sizeof(*up) - 8),
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
rtp_print(const void *hdr, u_int len, register const struct udphdr *up)
{
	/* rtp v1 or v2 */
	u_int *ip = (u_int *)hdr;
	u_int hasopt, contype, hasmarker;
	u_int i0 = ntohl(((u_int *)hdr)[0]);
	u_int i1 = ntohl(((u_int *)hdr)[1]);
	u_int dlen = ntohs(up->uh_ulen) - sizeof(*up) - 8;
	const char* ptype;

	ip += 2;
	len >>= 2;
	len -= 2;
	if ((i0 >> 30) == 1) {
		/* rtp v1 */
		hasopt = i0 & 0x800000;
		contype = (i0 >> 16) & 0x3f;
		hasmarker = i0 & 0x400000;
		ptype = "rtpv1";
	} else { /*XXX*/
		/* rtp v2 */
		hasopt = i0 & 0x20000000;
		contype = (i0 >> 16) & 0x7f;
		hasmarker = i0 & 0x800000;
		dlen -= 4;
		ptype = "rtp";
		ip += 1;
		len -= 1;
	}
	printf(" udp/%s %d c%d %s%s %d",
		ptype,
		dlen,
		contype,
		hasopt? "+" : "",
		hasmarker? "*" : "",
		i0 & 0xffff);
	if (vflag) {
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
		if (contype == 0x1f)
			printf(" 0x%04x", ip[0] >> 16);
		printf(" %u", i1);
	}
}

static const u_char*
rtcp_print(const u_char *hdr)
{
	/* rtp v2 control (rtcp) */
	struct rtcp_rr* rr = 0;
	struct rtcp_sr* sr;
	struct rtcphdr* rh = (struct rtcphdr*)hdr;
	u_int len = (ntohs(rh->rh_len) + 1) * 4;
	u_short flags = ntohs(rh->rh_flags);
	int cnt = (flags >> 8) & 0x1f;
	double ts, dts, jitter;
	if (vflag)
		printf(" %u", (u_int32_t)ntohl(rh->rh_ssrc));
	switch (flags & 0xff) {
	case RTCP_PT_SR:
		sr = (struct rtcp_sr*)(rh + 1);
		printf(" sr");
		if (len != cnt * sizeof(*rr) + sizeof(*sr) + sizeof(*rh))
			printf(" [%d]", len);
		ts = (double)((u_int32_t)ntohl(sr->sr_ts)) / 65536.;
		printf(" @%.2f %up %ub", ts, (u_int32_t)ntohl(sr->sr_np),
		       (u_int32_t)ntohl(sr->sr_nb));
		rr = (struct rtcp_rr*)(sr + 1);
		break;
	case RTCP_PT_RR:
		printf(" rr");
		if (len != cnt * sizeof(*rr) + sizeof(*rh))
			printf(" [%d]", len);
		rr = (struct rtcp_rr*)(rh + 1);
		break;
	case RTCP_PT_SDES:
		printf(" sdes %d", len);
		cnt = 0;
		break;
	case RTCP_PT_BYE:
		printf(" bye %d", len);
		cnt = 0;
		break;
	default:
		printf(" type-0x%x %d", flags & 0xff, len);
		cnt = 0;
		break;
	}
	if (cnt > 1)
		printf(" c%d", cnt);
	while (--cnt >= 0) {
		if ((u_char*)(rr + 1) > snapend) {
			printf(" [|rtcp]");
			return (snapend);
		}
		if (vflag)
			printf(" %u", (u_int32_t)ntohl(rr->rr_srcid));
		ts = (double)((u_int32_t)ntohl(rr->rr_lsr)) / 65536.;
		dts = (double)((u_int32_t)ntohl(rr->rr_dlsr)) / 65536.;
		jitter = (double)((u_int32_t)ntohl(rr->rr_dv)) / 65536.;
		printf(" %ur %ue %.2fj @%.2f+%.2f",
		    (u_int32_t)ntohl(rr->rr_nr),
		    (u_int32_t)ntohl(rr->rr_np),
		    jitter, ts, dts);
	}
	return (hdr + len);
}

/* XXX probably should use getservbyname() and cache answers */
#define TFTP_PORT 69		/*XXX*/
#define KERBEROS_PORT 88	/*XXX*/
#define SUNRPC_PORT 111		/*XXX*/
#define SNMP_PORT 161		/*XXX*/
#define NTP_PORT 123		/*XXX*/
#define SNMPTRAP_PORT 162	/*XXX*/
#define RIP_PORT 520		/*XXX*/
#define KERBEROS_SEC_PORT 750	/*XXX*/

void
udp_print(register const u_char *bp, u_int length, register const u_char *bp2)
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

		case PT_VAT:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			vat_print((void *)(up + 1), length, up);
			break;

		case PT_WB:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			wb_print((void *)(up + 1), length);
			break;

		case PT_RPC:
			rp = (struct rpc_msg *)(up + 1);
			direction = (enum msg_type)ntohl(rp->rm_direction);
			if (direction == CALL)
				sunrpcrequest_print((u_char *)rp, length,
				    (u_char *)ip);
			else
				nfsreply_print((u_char *)rp, length,
				    (u_char *)ip);			/*XXX*/
			break;

		case PT_RTP:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			rtp_print((void *)(up + 1), length, up);
			break;

		case PT_RTCP:
			(void)printf("%s.%s > %s.%s:",
				ipaddr_string(&ip->ip_src),
				udpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				udpport_string(dport));
			while (cp < snapend)
				cp = rtcp_print(cp);
			break;
		}
		return;
	}

	if (! qflag) {
		register struct rpc_msg *rp;
		enum msg_type direction;

		rp = (struct rpc_msg *)(up + 1);
		TCHECK(rp->rm_direction);
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
		else {
			TCHECK2(cp[0], 1);
			if (((struct LAP *)cp)->type == lapDDP &&
			    (atalk_port(sport) || atalk_port(dport))) {
				if (vflag)
					fputs("kip ", stdout);
				atalk_print(cp, length);
				return;
			}
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
		else if (ISPORT(RIP_PORT))
			rip_print((const u_char *)(up + 1), length);
		else if (ISPORT(SNMP_PORT) || ISPORT(SNMPTRAP_PORT))
			snmp_print((const u_char *)(up + 1), length);
		else if (ISPORT(NTP_PORT))
			ntp_print((const u_char *)(up + 1), length);
		else if (ISPORT(KERBEROS_PORT) || ISPORT(KERBEROS_SEC_PORT))
			krb_print((const void *)(up + 1), length);
		else if (dport == 3456)
			vat_print((const void *)(up + 1), length, up);
		/*
		 * Kludge in test for whiteboard packets.
		 */
		else if (dport == 4567)
			wb_print((const void *)(up + 1), length);
		else
			(void)printf(" udp %u",
			    (u_int32_t)(ulen - sizeof(*up)));
#undef ISPORT
	} else
		(void)printf(" udp %u", (u_int32_t)(ulen - sizeof(*up)));
	return;
trunc:
	fputs("[|udp]", stdout);
}
