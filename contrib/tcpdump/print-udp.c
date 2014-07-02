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
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-udp.c,v 1.142 2007-08-08 17:20:58 hannes Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#ifdef SEGSIZE
#undef SEGSIZE
#endif

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#include "appletalk.h"

#include "udp.h"

#include "ip.h"
#ifdef INET6
#include "ip6.h"
#endif
#include "ipproto.h"
#include "rpc_auth.h"
#include "rpc_msg.h"

#include "nameser.h"
#include "nfs.h"
#include "bootp.h"

struct rtcphdr {
	u_int16_t rh_flags;	/* T:2 P:1 CNT:5 PT:8 */
	u_int16_t rh_len;	/* length of message (in words) */
	u_int32_t rh_ssrc;	/* synchronization src id */
};

typedef struct {
	u_int32_t upper;	/* more significant 32 bits */
	u_int32_t lower;	/* less significant 32 bits */
} ntp64;

/*
 * Sender report.
 */
struct rtcp_sr {
	ntp64 sr_ntp;		/* 64-bit ntp timestamp */
	u_int32_t sr_ts;	/* reference media timestamp */
	u_int32_t sr_np;	/* no. packets sent */
	u_int32_t sr_nb;	/* no. bytes sent */
};

/*
 * Receiver report.
 * Time stamps are middle 32-bits of ntp timestamp.
 */
struct rtcp_rr {
	u_int32_t rr_srcid;	/* sender being reported */
	u_int32_t rr_nl;	/* no. packets lost */
	u_int32_t rr_ls;	/* extended last seq number received */
	u_int32_t rr_dv;	/* jitter (delay variance) */
	u_int32_t rr_lsr;	/* orig. ts from last rr from this src  */
	u_int32_t rr_dlsr;	/* time from recpt of last rr to xmit time */
};

/*XXX*/
#define RTCP_PT_SR	200
#define RTCP_PT_RR	201
#define RTCP_PT_SDES	202
#define 	RTCP_SDES_CNAME	1
#define 	RTCP_SDES_NAME	2
#define 	RTCP_SDES_EMAIL	3
#define 	RTCP_SDES_PHONE	4
#define 	RTCP_SDES_LOC	5
#define 	RTCP_SDES_TOOL	6
#define 	RTCP_SDES_NOTE	7
#define 	RTCP_SDES_PRIV	8
#define RTCP_PT_BYE	203
#define RTCP_PT_APP	204

static void
vat_print(packetbody_t hdr, __capability const struct udphdr *up)
{
	/* vat/vt audio */
	u_int ts = *(u_int16_t *)hdr;
	if ((ts & 0xf060) != 0) {
		/* probably vt */
		(void)printf("udp/vt %u %d / %d",
			     (u_int32_t)(EXTRACT_16BITS(&up->uh_ulen) - sizeof(*up)),
			     ts & 0x3ff, ts >> 10);
	} else {
		/* probably vat */
		u_int32_t i0 = EXTRACT_32BITS(&((__capability u_int *)hdr)[0]);
		u_int32_t i1 = EXTRACT_32BITS(&((__capability u_int *)hdr)[1]);
		printf("udp/vat %u c%d %u%s",
			(u_int32_t)(EXTRACT_16BITS(&up->uh_ulen) - sizeof(*up) - 8),
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
rtp_print(packetbody_t hdr, u_int len, __capability const struct udphdr *up)
{
	/* rtp v1 or v2 */
	__capability const u_int *ip = (__capability const u_int *)hdr;
	u_int hasopt, hasext, contype, hasmarker;
	u_int32_t i0 = EXTRACT_32BITS(&((__capability u_int *)hdr)[0]);
	u_int32_t i1 = EXTRACT_32BITS(&((__capability u_int *)hdr)[1]);
	u_int dlen = EXTRACT_16BITS(&up->uh_ulen) - sizeof(*up) - 8;
	const char * ptype;

	ip += 2;
	len >>= 2;
	len -= 2;
	hasopt = 0;
	hasext = 0;
	if ((i0 >> 30) == 1) {
		/* rtp v1 */
		hasopt = i0 & 0x800000;
		contype = (i0 >> 16) & 0x3f;
		hasmarker = i0 & 0x400000;
		ptype = "rtpv1";
	} else {
		/* rtp v2 */
		hasext = i0 & 0x10000000;
		contype = (i0 >> 16) & 0x7f;
		hasmarker = i0 & 0x800000;
		dlen -= 4;
		ptype = "rtp";
		ip += 1;
		len -= 1;
	}
	printf("udp/%s %d c%d %s%s %d %u",
		ptype,
		dlen,
		contype,
		(hasopt || hasext)? "+" : "",
		hasmarker? "*" : "",
		i0 & 0xffff,
		i1);
	if (vflag) {
		printf(" %u", EXTRACT_32BITS(&((__capability u_int *)hdr)[2]));
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
				len -= optlen;
			} while ((int)i2 >= 0);
		}
		if (hasext) {
			u_int i2, extlen;
			i2 = ip[0];
			extlen = (i2 & 0xffff) + 1;
			if (extlen > len) {
				printf(" !ext");
				return;
			}
			ip += extlen;
		}
		if (contype == 0x1f) /*XXX H.261 */
			printf(" 0x%04x", ip[0] >> 16);
	}
}

static packetbody_t
rtcp_print(packetbody_t hdr, packetbody_t ep)
{
	/* rtp v2 control (rtcp) */
	__capability const struct rtcp_rr *rr = NULL;
	__capability const struct rtcp_sr *sr;
	__capability const struct rtcphdr *rh = (__capability const struct rtcphdr *)hdr;
	u_int len;
	u_int16_t flags;
	int cnt;
	double ts, dts;
	if ((packetbody_t)(rh + 1) > ep) {
		printf(" [|rtcp]");
		return (ep);
	}
	len = (EXTRACT_16BITS(&rh->rh_len) + 1) * 4;
	flags = EXTRACT_16BITS(&rh->rh_flags);
	cnt = (flags >> 8) & 0x1f;
	switch (flags & 0xff) {
	case RTCP_PT_SR:
		sr = (__capability const struct rtcp_sr *)(rh + 1);
		printf(" sr");
		if (len != cnt * sizeof(*rr) + sizeof(*sr) + sizeof(*rh))
			printf(" [%d]", len);
		if (vflag)
			printf(" %u", EXTRACT_32BITS(&rh->rh_ssrc));
		if ((u_char *)(sr + 1) > ep) {
			printf(" [|rtcp]");
			return (ep);
		}
		ts = (double)(EXTRACT_32BITS(&sr->sr_ntp.upper)) +
		    ((double)(EXTRACT_32BITS(&sr->sr_ntp.lower)) /
		    4294967296.0);
		printf(" @%.2f %u %up %ub", ts, EXTRACT_32BITS(&sr->sr_ts),
		    EXTRACT_32BITS(&sr->sr_np), EXTRACT_32BITS(&sr->sr_nb));
		rr = (__capability const struct rtcp_rr *)(sr + 1);
		break;
	case RTCP_PT_RR:
		printf(" rr");
		if (len != cnt * sizeof(*rr) + sizeof(*rh))
			printf(" [%d]", len);
		rr = (__capability const struct rtcp_rr *)(rh + 1);
		if (vflag)
			printf(" %u", EXTRACT_32BITS(&rh->rh_ssrc));
		break;
	case RTCP_PT_SDES:
		printf(" sdes %d", len);
		if (vflag)
			printf(" %u", EXTRACT_32BITS(&rh->rh_ssrc));
		cnt = 0;
		break;
	case RTCP_PT_BYE:
		printf(" bye %d", len);
		if (vflag)
			printf(" %u", EXTRACT_32BITS(&rh->rh_ssrc));
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
		if ((u_char *)(rr + 1) > ep) {
			printf(" [|rtcp]");
			return (ep);
		}
		if (vflag)
			printf(" %u", EXTRACT_32BITS(&rr->rr_srcid));
		ts = (double)(EXTRACT_32BITS(&rr->rr_lsr)) / 65536.;
		dts = (double)(EXTRACT_32BITS(&rr->rr_dlsr)) / 65536.;
		printf(" %ul %us %uj @%.2f+%.2f",
		    EXTRACT_32BITS(&rr->rr_nl) & 0x00ffffff,
		    EXTRACT_32BITS(&rr->rr_ls),
		    EXTRACT_32BITS(&rr->rr_dv), ts, dts);
	}
	return (hdr + len);
}

static int udp_cksum(__capability const struct ip *ip,
		     __capability const struct udphdr *up,
		     register u_int len)
{
	return (nextproto4_cksum(ip, (packetbody_t)up, len,
	    IPPROTO_UDP));
}

#ifdef INET6
static int udp6_cksum(__capability const struct ip6_hdr *ip6,
		      __capability const struct udphdr *up,
		      u_int len)
{
	return (nextproto6_cksum(ip6, (packetbody_t)up, len,
	    IPPROTO_UDP));
}
#endif

static void
udpipaddr_print(__capability const struct ip *ip, int sport, int dport)
{
#ifdef INET6
	__capability const struct ip6_hdr *ip6;

	if (IP_V(ip) == 6)
		ip6 = (__capability const struct ip6_hdr *)ip;
	else
		ip6 = NULL;

	if (ip6) {
		if (ip6->ip6_nxt == IPPROTO_UDP) {
			if (sport == -1) {
				(void)printf("%s > %s: ",
					ip6addr_string(&ip6->ip6_src),
					ip6addr_string(&ip6->ip6_dst));
			} else {
				(void)printf("%s.%s > %s.%s: ",
					ip6addr_string(&ip6->ip6_src),
					udpport_string(sport),
					ip6addr_string(&ip6->ip6_dst),
					udpport_string(dport));
			}
		} else {
			if (sport != -1) {
				(void)printf("%s > %s: ",
					udpport_string(sport),
					udpport_string(dport));
			}
		}
	} else
#endif /*INET6*/
	{
		if (ip->ip_p == IPPROTO_UDP) {
			if (sport == -1) {
				(void)printf("%s > %s: ",
					ipaddr_string(&ip->ip_src),
					ipaddr_string(&ip->ip_dst));
			} else {
				(void)printf("%s.%s > %s.%s: ",
					ipaddr_string(&ip->ip_src),
					udpport_string(sport),
					ipaddr_string(&ip->ip_dst),
					udpport_string(dport));
			}
		} else {
			if (sport != -1) {
				(void)printf("%s > %s: ",
					udpport_string(sport),
					udpport_string(dport));
			}
		}
	}
}

void
udp_print(packetbody_t bp, u_int length, packetbody_t bp2, int fragmented)
{
	__capability const struct udphdr *up;
	__capability const struct ip *ip;
	packetbody_t cp;
	packetbody_t ep = bp + length;
	u_int16_t sport, dport, ulen;
#ifdef INET6
	__capability const struct ip6_hdr *ip6;
#endif

	if (ep > snapend)
		ep = snapend;
	up = (__capability const struct udphdr *)bp;
	ip = (__capability const struct ip *)bp2;
#ifdef INET6
	if (IP_V(ip) == 6)
		ip6 = (__capability const struct ip6_hdr *)bp2;
	else
		ip6 = NULL;
#endif /*INET6*/
	cp = (packetbody_t)(up + 1);
	if (!TTEST(up->uh_dport)) {
		udpipaddr_print(ip, -1, -1);
		(void)printf("[|udp]");
		return;
	}

	sport = EXTRACT_16BITS(&up->uh_sport);
	dport = EXTRACT_16BITS(&up->uh_dport);

	if (length < sizeof(struct udphdr)) {
		udpipaddr_print(ip, sport, dport);
		(void)printf("truncated-udp %d", length);
		return;
	}
	length -= sizeof(struct udphdr);

	if (cp > snapend) {
		udpipaddr_print(ip, sport, dport);
		(void)printf("[|udp]");
		return;
	}
	cp = (packetbody_t)(up + 1);

	ulen = EXTRACT_16BITS(&up->uh_ulen);
	if (ulen < 8) {
		udpipaddr_print(ip, sport, dport);
		(void)printf("truncated-udplength %d", ulen);
		return;
	}
	if (packettype) {
		__capability const struct sunrpc_msg *rp;
		enum sunrpc_msg_type direction;

		switch (packettype) {

		case PT_VAT:
			udpipaddr_print(ip, sport, dport);
			vat_print((packetbody_t)(up + 1), up);
			break;

		case PT_WB:
			udpipaddr_print(ip, sport, dport);
			wb_print((packetbody_t)(up + 1), length);
			break;

		case PT_RPC:
			rp = (__capability const struct sunrpc_msg *)(up + 1);
			direction = (enum sunrpc_msg_type)EXTRACT_32BITS(&rp->rm_direction);
			if (direction == SUNRPC_CALL)
				sunrpcrequest_print((packetbody_t)rp, length,
				    (packetbody_t)ip);
			else
				nfsreply_print((packetbody_t)rp, length,
				    (packetbody_t)ip);			/*XXX*/
			break;

		case PT_RTP:
			udpipaddr_print(ip, sport, dport);
			rtp_print((packetbody_t)(up + 1), length, up);
			break;

		case PT_RTCP:
			udpipaddr_print(ip, sport, dport);
			while (cp < ep)
				cp = rtcp_print(cp, ep);
			break;

		case PT_SNMP:
			udpipaddr_print(ip, sport, dport);
			snmp_print((packetbody_t)(up + 1), length);
			break;

		case PT_CNFP:
			udpipaddr_print(ip, sport, dport);
			cnfp_print(cp, (packetbody_t)ip);
			break;

		case PT_TFTP:
			udpipaddr_print(ip, sport, dport);
			tftp_print(cp, length);
			break;

		case PT_AODV:
			udpipaddr_print(ip, sport, dport);
			aodv_print((packetbody_t)(up + 1), length,
#ifdef INET6
			    ip6 != NULL);
#else
			    0);
#endif
			break;

		case PT_RADIUS:
			udpipaddr_print(ip, sport, dport);
			radius_print(cp, length);
			break;

		case PT_VXLAN:
			udpipaddr_print(ip, sport, dport);
			vxlan_print((packetbody_t)(up + 1), length);
			break;
		}
		return;
	}

	if (!qflag) {
		__capability const struct sunrpc_msg *rp;
		enum sunrpc_msg_type direction;

		rp = (__capability const struct sunrpc_msg *)(up + 1);
		if (TTEST(rp->rm_direction)) {
			direction = (enum sunrpc_msg_type)EXTRACT_32BITS(&rp->rm_direction);
			if (dport == NFS_PORT && direction == SUNRPC_CALL) {
				nfsreq_print((packetbody_t)rp, length,
				    (packetbody_t)ip);
				return;
			}
			if (sport == NFS_PORT && direction == SUNRPC_REPLY) {
				nfsreply_print((packetbody_t)rp, length,
				    (packetbody_t)ip);
				return;
			}
#ifdef notdef
			if (dport == SUNRPC_PORT && direction == SUNRPC_CALL) {
				sunrpcrequest_print((packetbody_t)rp, length,
				    (packetbody_t)ip);
				return;
			}
#endif
		}
		if (TTEST(((struct LAP *)cp)->type) &&
		    ((struct LAP *)cp)->type == lapDDP &&
		    (atalk_port(sport) || atalk_port(dport))) {
			if (vflag)
				fputs("kip ", stdout);
			llap_print(cp, length);
			return;
		}
	}
	udpipaddr_print(ip, sport, dport);

	if (vflag && !Kflag && !fragmented) {
                /* Check the checksum, if possible. */
                u_int16_t sum, udp_sum;

		/*
		 * XXX - do this even if vflag == 1?
		 * TCP does, and we do so for UDP-over-IPv6.
		 */
	        if (IP_V(ip) == 4 && (vflag > 1)) {
			udp_sum = EXTRACT_16BITS(&up->uh_sum);
			if (udp_sum == 0) {
				(void)printf("[no cksum] ");
			} else if (TTEST2(cp[0], length)) {
				sum = udp_cksum(ip, up, length + sizeof(struct udphdr));

	                        if (sum != 0) {
        	                        (void)printf("[bad udp cksum 0x%04x -> 0x%04x!] ",
					    udp_sum,
					    in_cksum_shouldbe(udp_sum, sum));
				} else
					(void)printf("[udp sum ok] ");
			}
		}
#ifdef INET6
		else if (IP_V(ip) == 6 && ip6->ip6_plen) {
			/* for IPv6, UDP checksum is mandatory */
			if (TTEST2(cp[0], length)) {
				sum = udp6_cksum(ip6, up, length + sizeof(struct udphdr));
				udp_sum = EXTRACT_16BITS(&up->uh_sum);

	                        if (sum != 0) {
        	                        (void)printf("[bad udp cksum 0x%04x -> 0x%04x!] ",
					    udp_sum,
					    in_cksum_shouldbe(udp_sum, sum));
				} else
					(void)printf("[udp sum ok] ");
			}
		}
#endif
	}

	if (!qflag) {
#define ISPORT(p) (dport == (p) || sport == (p))
		if (ISPORT(NAMESERVER_PORT))
			ns_print((packetbody_t)(up + 1), length, 0);
		else if (ISPORT(MULTICASTDNS_PORT))
			ns_print((packetbody_t)(up + 1), length, 1);
		else if (ISPORT(TIMED_PORT))
			timed_print((packetbody_t)(up + 1));
		else if (ISPORT(TFTP_PORT))
			tftp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(IPPORT_BOOTPC) || ISPORT(IPPORT_BOOTPS))
			bootp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(RIP_PORT))
			rip_print((packetbody_t)(up + 1), length);
		else if (ISPORT(AODV_PORT))
			aodv_print((packetbody_t)(up + 1), length,
#ifdef INET6
			    ip6 != NULL);
#else
			    0);
#endif
	        else if (ISPORT(ISAKMP_PORT))
			 isakmp_print(gndo, (packetbody_t)(up + 1), length, bp2);
  	        else if (ISPORT(ISAKMP_PORT_NATT))
			 isakmp_rfc3948_print(gndo, (packetbody_t)(up + 1), length, bp2);
#if 1 /*???*/
   	        else if (ISPORT(ISAKMP_PORT_USER1) || ISPORT(ISAKMP_PORT_USER2))
			isakmp_print(gndo, (packetbody_t)(up + 1), length, bp2);
#endif
		else if (ISPORT(SNMP_PORT) || ISPORT(SNMPTRAP_PORT))
			snmp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(NTP_PORT))
			ntp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(KERBEROS_PORT) || ISPORT(KERBEROS_SEC_PORT))
			krb_print((packetbody_t)(up + 1));
		else if (ISPORT(L2TP_PORT))
			l2tp_print((packetbody_t)(up + 1), length);
#ifdef TCPDUMP_DO_SMB
		else if (ISPORT(NETBIOS_NS_PORT))
			nbt_udp137_print((packetbody_t)(up + 1), length);
		else if (ISPORT(NETBIOS_DGRAM_PORT))
			nbt_udp138_print((packetbody_t)(up + 1), length);
#endif
		else if (dport == 3456)
			vat_print((packetbody_t)(up + 1), up);
		else if (ISPORT(ZEPHYR_SRV_PORT) || ISPORT(ZEPHYR_CLT_PORT))
			zephyr_print((packetbody_t)(up + 1), length);
		/*
		 * Since there are 10 possible ports to check, I think
		 * a <> test would be more efficient
		 */
#if 0
		else if ((sport >= RX_PORT_LOW && sport <= RX_PORT_HIGH) ||
			 (dport >= RX_PORT_LOW && dport <= RX_PORT_HIGH))
			rx_print((packetbody_t)(up + 1), length, sport, dport,
				 (packetbody_t)ip);
#endif
#ifdef INET6
		else if (ISPORT(RIPNG_PORT))
			ripng_print((packetbody_t)(up + 1), length);
		else if (ISPORT(DHCP6_SERV_PORT) || ISPORT(DHCP6_CLI_PORT))
			dhcp6_print((packetbody_t)(up + 1), length);
		else if (ISPORT(BABEL_PORT) || ISPORT(BABEL_PORT_OLD))
			babel_print((packetbody_t)(up + 1), length);
#endif /*INET6*/
		/*
		 * Kludge in test for whiteboard packets.
		 */
		else if (dport == 4567)
			wb_print((packetbody_t)(up + 1), length);
		else if (ISPORT(CISCO_AUTORP_PORT))
			cisco_autorp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(RADIUS_PORT) ||
			 ISPORT(RADIUS_NEW_PORT) ||
			 ISPORT(RADIUS_ACCOUNTING_PORT) ||
			 ISPORT(RADIUS_NEW_ACCOUNTING_PORT) )
			radius_print((packetbody_t)(up + 1), length);
		else if (dport == HSRP_PORT)
			hsrp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(LWRES_PORT))
			lwres_print((packetbody_t)(up + 1), length);
		else if (ISPORT(LDP_PORT))
			ldp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(OLSR_PORT))
			olsr_print((packetbody_t)(up + 1), length,
#if INET6
					(IP_V(ip) == 6) ? 1 : 0);
#else
					0);
#endif
		else if (ISPORT(MPLS_LSP_PING_PORT))
			lspping_print((packetbody_t)(up + 1), length);
		else if (dport == BFD_CONTROL_PORT ||
			 dport == BFD_ECHO_PORT )
			bfd_print((packetbody_t)(up + 1), length, dport);
                else if (ISPORT(LMP_PORT))
			lmp_print((packetbody_t)(up + 1), length);
		else if (ISPORT(VQP_PORT))
			vqp_print((packetbody_t)(up + 1), length);
                else if (ISPORT(SFLOW_PORT))
                        sflow_print((packetbody_t)(up + 1), length);
	        else if (dport == LWAPP_CONTROL_PORT)
			lwapp_control_print((packetbody_t)(up + 1), length, 1);
                else if (sport == LWAPP_CONTROL_PORT)
                        lwapp_control_print((packetbody_t)(up + 1), length, 0);
                else if (ISPORT(LWAPP_DATA_PORT))
                        lwapp_data_print((packetbody_t)(up + 1), length);
                else if (ISPORT(SIP_PORT))
			sip_print((packetbody_t)(up + 1), length);
                else if (ISPORT(SYSLOG_PORT))
			syslog_print((packetbody_t)(up + 1), length);
                else if (ISPORT(OTV_PORT))
			otv_print((packetbody_t)(up + 1), length);
		else
			(void)printf("UDP, length %u",
			    (u_int32_t)(ulen - sizeof(*up)));
#undef ISPORT
	} else
		(void)printf("UDP, length %u", (u_int32_t)(ulen - sizeof(*up)));
}


/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */

