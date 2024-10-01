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

/* \summary: UDP printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"
#include "appletalk.h"

#include "udp.h"

#include "ip.h"
#include "ip6.h"
#include "ipproto.h"
#include "rpc_auth.h"
#include "rpc_msg.h"

#include "nfs.h"


struct rtcphdr {
	nd_uint16_t rh_flags;	/* T:2 P:1 CNT:5 PT:8 */
	nd_uint16_t rh_len;	/* length of message (in words) */
	nd_uint32_t rh_ssrc;	/* synchronization src id */
};

typedef struct {
	nd_uint32_t upper;	/* more significant 32 bits */
	nd_uint32_t lower;	/* less significant 32 bits */
} ntp64;

/*
 * Sender report.
 */
struct rtcp_sr {
	ntp64       sr_ntp;	/* 64-bit ntp timestamp */
	nd_uint32_t sr_ts;	/* reference media timestamp */
	nd_uint32_t sr_np;	/* no. packets sent */
	nd_uint32_t sr_nb;	/* no. bytes sent */
};

/*
 * Receiver report.
 * Time stamps are middle 32-bits of ntp timestamp.
 */
struct rtcp_rr {
	nd_uint32_t rr_srcid;	/* sender being reported */
	nd_uint32_t rr_nl;	/* no. packets lost */
	nd_uint32_t rr_ls;	/* extended last seq number received */
	nd_uint32_t rr_dv;	/* jitter (delay variance) */
	nd_uint32_t rr_lsr;	/* orig. ts from last rr from this src  */
	nd_uint32_t rr_dlsr;	/* time from recpt of last rr to xmit time */
};

/*XXX*/
#define RTCP_PT_SR	200
#define RTCP_PT_RR	201
#define RTCP_PT_SDES	202
#define    RTCP_SDES_CNAME	1
#define    RTCP_SDES_NAME	2
#define    RTCP_SDES_EMAIL	3
#define    RTCP_SDES_PHONE	4
#define    RTCP_SDES_LOC	5
#define    RTCP_SDES_TOOL	6
#define    RTCP_SDES_NOTE	7
#define    RTCP_SDES_PRIV	8
#define RTCP_PT_BYE	203
#define RTCP_PT_APP	204

static void
vat_print(netdissect_options *ndo, const u_char *hdr, u_int length)
{
	/* vat/vt audio */
	u_int ts;

	ndo->ndo_protocol = "vat";
	if (length < 2) {
		ND_PRINT("udp/va/vat, length %u < 2", length);
		return;
	}
	ts = GET_BE_U_2(hdr);
	if ((ts & 0xf060) != 0) {
		/* probably vt */
		ND_PRINT("udp/vt %u %u / %u",
			     length,
			     ts & 0x3ff, ts >> 10);
	} else {
		/* probably vat */
		uint32_t i0, i1;

		if (length < 8) {
			ND_PRINT("udp/vat, length %u < 8", length);
			return;
		}
		i0 = GET_BE_U_4(&((const u_int *)hdr)[0]);
		i1 = GET_BE_U_4(&((const u_int *)hdr)[1]);
		ND_PRINT("udp/vat %u c%u %u%s",
			length - 8,
			i0 & 0xffff,
			i1, i0 & 0x800000? "*" : "");
		/* audio format */
		if (i0 & 0x1f0000)
			ND_PRINT(" f%u", (i0 >> 16) & 0x1f);
		if (i0 & 0x3f000000)
			ND_PRINT(" s%u", (i0 >> 24) & 0x3f);
	}
}

static void
rtp_print(netdissect_options *ndo, const u_char *hdr, u_int len)
{
	/* rtp v1 or v2 */
	const u_int *ip = (const u_int *)hdr;
	u_int hasopt, hasext, contype, hasmarker, dlen;
	uint32_t i0, i1;
	const char * ptype;

	ndo->ndo_protocol = "rtp";
	if (len < 8) {
		ND_PRINT("udp/rtp, length %u < 8", len);
		return;
	}
	i0 = GET_BE_U_4(&((const u_int *)hdr)[0]);
	i1 = GET_BE_U_4(&((const u_int *)hdr)[1]);
	dlen = len - 8;
	ip += 2;
	len >>= 2;
	len -= 2;
	hasopt = 0;
	hasext = 0;
	if ((i0 >> 30) == 1) {
		/* rtp v1 - draft-ietf-avt-rtp-04 */
		hasopt = i0 & 0x800000;
		contype = (i0 >> 16) & 0x3f;
		hasmarker = i0 & 0x400000;
		ptype = "rtpv1";
	} else {
		/* rtp v2 - RFC 3550 */
		if (dlen < 4) {
			ND_PRINT("udp/rtp, length %u < 12", dlen + 8);
			return;
		}
		hasext = i0 & 0x10000000;
		contype = (i0 >> 16) & 0x7f;
		hasmarker = i0 & 0x800000;
		dlen -= 4;
		ptype = "rtp";
		ip += 1;
		len -= 1;
	}
	ND_PRINT("udp/%s %u c%u %s%s %u %u",
		ptype,
		dlen,
		contype,
		(hasopt || hasext)? "+" : "",
		hasmarker? "*" : "",
		i0 & 0xffff,
		i1);
	if (ndo->ndo_vflag) {
		ND_PRINT(" %u", GET_BE_U_4(&((const u_int *)hdr)[2]));
		if (hasopt) {
			u_int i2, optlen;
			do {
				i2 = GET_BE_U_4(ip);
				optlen = (i2 >> 16) & 0xff;
				if (optlen == 0 || optlen > len) {
					ND_PRINT(" !opt");
					return;
				}
				ip += optlen;
				len -= optlen;
			} while ((int)i2 >= 0);
		}
		if (hasext) {
			u_int i2, extlen;
			i2 = GET_BE_U_4(ip);
			extlen = (i2 & 0xffff) + 1;
			if (extlen > len) {
				ND_PRINT(" !ext");
				return;
			}
			ip += extlen;
		}
		if (contype == 0x1f) /*XXX H.261 */
			ND_PRINT(" 0x%04x", GET_BE_U_4(ip) >> 16);
	}
}

static const u_char *
rtcp_print(netdissect_options *ndo, const u_char *hdr, const u_char *ep)
{
	/* rtp v2 control (rtcp) */
	const struct rtcp_rr *rr = 0;
	const struct rtcp_sr *sr;
	const struct rtcphdr *rh = (const struct rtcphdr *)hdr;
	u_int len;
	uint16_t flags;
	u_int cnt;
	double ts, dts;

	ndo->ndo_protocol = "rtcp";
	if ((const u_char *)(rh + 1) > ep)
		goto trunc;
	ND_TCHECK_SIZE(rh);
	len = (GET_BE_U_2(rh->rh_len) + 1) * 4;
	flags = GET_BE_U_2(rh->rh_flags);
	cnt = (flags >> 8) & 0x1f;
	switch (flags & 0xff) {
	case RTCP_PT_SR:
		sr = (const struct rtcp_sr *)(rh + 1);
		ND_PRINT(" sr");
		if (len != cnt * sizeof(*rr) + sizeof(*sr) + sizeof(*rh))
			ND_PRINT(" [%u]", len);
		if (ndo->ndo_vflag)
			ND_PRINT(" %u", GET_BE_U_4(rh->rh_ssrc));
		if ((const u_char *)(sr + 1) > ep)
			goto trunc;
		ND_TCHECK_SIZE(sr);
		ts = (double)(GET_BE_U_4(sr->sr_ntp.upper)) +
		    ((double)(GET_BE_U_4(sr->sr_ntp.lower)) /
		     FMAXINT);
		ND_PRINT(" @%.2f %u %up %ub", ts, GET_BE_U_4(sr->sr_ts),
			  GET_BE_U_4(sr->sr_np), GET_BE_U_4(sr->sr_nb));
		rr = (const struct rtcp_rr *)(sr + 1);
		break;
	case RTCP_PT_RR:
		ND_PRINT(" rr");
		if (len != cnt * sizeof(*rr) + sizeof(*rh))
			ND_PRINT(" [%u]", len);
		rr = (const struct rtcp_rr *)(rh + 1);
		if (ndo->ndo_vflag)
			ND_PRINT(" %u", GET_BE_U_4(rh->rh_ssrc));
		break;
	case RTCP_PT_SDES:
		ND_PRINT(" sdes %u", len);
		if (ndo->ndo_vflag)
			ND_PRINT(" %u", GET_BE_U_4(rh->rh_ssrc));
		cnt = 0;
		break;
	case RTCP_PT_BYE:
		ND_PRINT(" bye %u", len);
		if (ndo->ndo_vflag)
			ND_PRINT(" %u", GET_BE_U_4(rh->rh_ssrc));
		cnt = 0;
		break;
	default:
		ND_PRINT(" type-0x%x %u", flags & 0xff, len);
		cnt = 0;
		break;
	}
	if (cnt > 1)
		ND_PRINT(" c%u", cnt);
	while (cnt != 0) {
		if ((const u_char *)(rr + 1) > ep)
			goto trunc;
		ND_TCHECK_SIZE(rr);
		if (ndo->ndo_vflag)
			ND_PRINT(" %u", GET_BE_U_4(rr->rr_srcid));
		ts = (double)(GET_BE_U_4(rr->rr_lsr)) / 65536.;
		dts = (double)(GET_BE_U_4(rr->rr_dlsr)) / 65536.;
		ND_PRINT(" %ul %us %uj @%.2f+%.2f",
		    GET_BE_U_4(rr->rr_nl) & 0x00ffffff,
		    GET_BE_U_4(rr->rr_ls),
		    GET_BE_U_4(rr->rr_dv), ts, dts);
		cnt--;
	}
	return (hdr + len);

trunc:
	nd_print_trunc(ndo);
	return ep;
}

static uint16_t udp_cksum(netdissect_options *ndo, const struct ip *ip,
		     const struct udphdr *up,
		     u_int len)
{
	return nextproto4_cksum(ndo, ip, (const uint8_t *)(const void *)up, len, len,
				IPPROTO_UDP);
}

static uint16_t udp6_cksum(netdissect_options *ndo, const struct ip6_hdr *ip6,
		      const struct udphdr *up, u_int len)
{
	return nextproto6_cksum(ndo, ip6, (const uint8_t *)(const void *)up, len, len,
				IPPROTO_UDP);
}

static void
udpipaddr_print(netdissect_options *ndo, const struct ip *ip, int sport, int dport)
{
	const struct ip6_hdr *ip6;

	if (IP_V(ip) == 6)
		ip6 = (const struct ip6_hdr *)ip;
	else
		ip6 = NULL;

	if (ip6) {
		if (GET_U_1(ip6->ip6_nxt) == IPPROTO_UDP) {
			if (sport == -1) {
				ND_PRINT("%s > %s: ",
					GET_IP6ADDR_STRING(ip6->ip6_src),
					GET_IP6ADDR_STRING(ip6->ip6_dst));
			} else {
				ND_PRINT("%s.%s > %s.%s: ",
					GET_IP6ADDR_STRING(ip6->ip6_src),
					udpport_string(ndo, (uint16_t)sport),
					GET_IP6ADDR_STRING(ip6->ip6_dst),
					udpport_string(ndo, (uint16_t)dport));
			}
		} else {
			if (sport != -1) {
				ND_PRINT("%s > %s: ",
					udpport_string(ndo, (uint16_t)sport),
					udpport_string(ndo, (uint16_t)dport));
			}
		}
	} else {
		if (GET_U_1(ip->ip_p) == IPPROTO_UDP) {
			if (sport == -1) {
				ND_PRINT("%s > %s: ",
					GET_IPADDR_STRING(ip->ip_src),
					GET_IPADDR_STRING(ip->ip_dst));
			} else {
				ND_PRINT("%s.%s > %s.%s: ",
					GET_IPADDR_STRING(ip->ip_src),
					udpport_string(ndo, (uint16_t)sport),
					GET_IPADDR_STRING(ip->ip_dst),
					udpport_string(ndo, (uint16_t)dport));
			}
		} else {
			if (sport != -1) {
				ND_PRINT("%s > %s: ",
					udpport_string(ndo, (uint16_t)sport),
					udpport_string(ndo, (uint16_t)dport));
			}
		}
	}
}

void
udp_print(netdissect_options *ndo, const u_char *bp, u_int length,
	  const u_char *bp2, int fragmented, u_int ttl_hl)
{
	const struct udphdr *up;
	const struct ip *ip;
	const u_char *cp;
	const u_char *ep = ndo->ndo_snapend;
	uint16_t sport, dport;
	u_int ulen;
	const struct ip6_hdr *ip6;

	ndo->ndo_protocol = "udp";
	up = (const struct udphdr *)bp;
	ip = (const struct ip *)bp2;
	if (IP_V(ip) == 6)
		ip6 = (const struct ip6_hdr *)bp2;
	else
		ip6 = NULL;
	if (!ND_TTEST_2(up->uh_dport)) {
		udpipaddr_print(ndo, ip, -1, -1);
		goto trunc;
	}

	sport = GET_BE_U_2(up->uh_sport);
	dport = GET_BE_U_2(up->uh_dport);

	if (length < sizeof(struct udphdr)) {
		udpipaddr_print(ndo, ip, sport, dport);
		ND_PRINT("truncated-udp %u", length);
		return;
	}
	if (!ND_TTEST_2(up->uh_ulen)) {
		udpipaddr_print(ndo, ip, sport, dport);
		goto trunc;
	}
	ulen = GET_BE_U_2(up->uh_ulen);
	/*
	 * IPv6 Jumbo Datagrams; see RFC 2675.
	 * If the length is zero, and the length provided to us is
	 * > 65535, use the provided length as the length.
	 */
	if (ulen == 0 && length > 65535)
		ulen = length;
	if (ulen < sizeof(struct udphdr)) {
		udpipaddr_print(ndo, ip, sport, dport);
		ND_PRINT("truncated-udplength %u", ulen);
		return;
	}
	ulen -= sizeof(struct udphdr);
	length -= sizeof(struct udphdr);
	if (ulen < length)
		length = ulen;

	cp = (const u_char *)(up + 1);
	if (cp > ndo->ndo_snapend) {
		udpipaddr_print(ndo, ip, sport, dport);
		goto trunc;
	}

	if (ndo->ndo_packettype) {
		const struct sunrpc_msg *rp;
		enum sunrpc_msg_type direction;

		switch (ndo->ndo_packettype) {

		case PT_VAT:
			udpipaddr_print(ndo, ip, sport, dport);
			vat_print(ndo, cp, length);
			break;

		case PT_WB:
			udpipaddr_print(ndo, ip, sport, dport);
			wb_print(ndo, cp, length);
			break;

		case PT_RPC:
			rp = (const struct sunrpc_msg *)cp;
			direction = (enum sunrpc_msg_type) GET_BE_U_4(rp->rm_direction);
			if (direction == SUNRPC_CALL)
				sunrpc_print(ndo, (const u_char *)rp, length,
				    (const u_char *)ip);
			else
				nfsreply_print(ndo, (const u_char *)rp, length,
				    (const u_char *)ip);			/*XXX*/
			break;

		case PT_RTP:
			udpipaddr_print(ndo, ip, sport, dport);
			rtp_print(ndo, cp, length);
			break;

		case PT_RTCP:
			udpipaddr_print(ndo, ip, sport, dport);
			while (cp < ep)
				cp = rtcp_print(ndo, cp, ep);
			break;

		case PT_SNMP:
			udpipaddr_print(ndo, ip, sport, dport);
			snmp_print(ndo, cp, length);
			break;

		case PT_CNFP:
			udpipaddr_print(ndo, ip, sport, dport);
			cnfp_print(ndo, cp);
			break;

		case PT_TFTP:
			udpipaddr_print(ndo, ip, sport, dport);
			tftp_print(ndo, cp, length);
			break;

		case PT_AODV:
			udpipaddr_print(ndo, ip, sport, dport);
			aodv_print(ndo, cp, length, IP_V(ip) == 6);
			break;

		case PT_RADIUS:
			udpipaddr_print(ndo, ip, sport, dport);
			radius_print(ndo, cp, length);
			break;

		case PT_VXLAN:
			udpipaddr_print(ndo, ip, sport, dport);
			vxlan_print(ndo, cp, length);
			break;

		case PT_PGM:
		case PT_PGM_ZMTP1:
			udpipaddr_print(ndo, ip, sport, dport);
			pgm_print(ndo, cp, length, bp2);
			break;
		case PT_LMP:
			udpipaddr_print(ndo, ip, sport, dport);
			lmp_print(ndo, cp, length);
			break;
		case PT_PTP:
			udpipaddr_print(ndo, ip, sport, dport);
			ptp_print(ndo, cp, length);
			break;
		case PT_SOMEIP:
			udpipaddr_print(ndo, ip, sport, dport);
			someip_print(ndo, cp, length);
			break;
		case PT_DOMAIN:
			udpipaddr_print(ndo, ip, sport, dport);
			/* over_tcp: FALSE, is_mdns: FALSE */
			domain_print(ndo, cp, length, FALSE, FALSE);
			break;
		}
		return;
	}

	udpipaddr_print(ndo, ip, sport, dport);
	if (!ndo->ndo_qflag) {
		const struct sunrpc_msg *rp;
		enum sunrpc_msg_type direction;

		rp = (const struct sunrpc_msg *)cp;
		if (ND_TTEST_4(rp->rm_direction)) {
			direction = (enum sunrpc_msg_type) GET_BE_U_4(rp->rm_direction);
			if (dport == NFS_PORT && direction == SUNRPC_CALL) {
				ND_PRINT("NFS request xid %u ",
					 GET_BE_U_4(rp->rm_xid));
				nfsreq_noaddr_print(ndo, (const u_char *)rp, length,
				    (const u_char *)ip);
				return;
			}
			if (sport == NFS_PORT && direction == SUNRPC_REPLY) {
				ND_PRINT("NFS reply xid %u ",
					 GET_BE_U_4(rp->rm_xid));
				nfsreply_noaddr_print(ndo, (const u_char *)rp, length,
				    (const u_char *)ip);
				return;
			}
#ifdef notdef
			if (dport == SUNRPC_PORT && direction == SUNRPC_CALL) {
				sunrpc_print((const u_char *)rp, length, (const u_char *)ip);
				return;
			}
#endif
		}
	}

	if (ndo->ndo_vflag && !ndo->ndo_Kflag && !fragmented) {
		/* Check the checksum, if possible. */
		uint16_t sum, udp_sum;

		/*
		 * XXX - do this even if vflag == 1?
		 * TCP does, and we do so for UDP-over-IPv6.
		 */
		if (IP_V(ip) == 4 && (ndo->ndo_vflag > 1)) {
			udp_sum = GET_BE_U_2(up->uh_sum);
			if (udp_sum == 0) {
				ND_PRINT("[no cksum] ");
			} else if (ND_TTEST_LEN(cp, length)) {
				sum = udp_cksum(ndo, ip, up, length + sizeof(struct udphdr));

				if (sum != 0) {
					ND_PRINT("[bad udp cksum 0x%04x -> 0x%04x!] ",
					    udp_sum,
					    in_cksum_shouldbe(udp_sum, sum));
				} else
					ND_PRINT("[udp sum ok] ");
			}
		} else if (IP_V(ip) == 6) {
			/* for IPv6, UDP checksum is mandatory */
			if (ND_TTEST_LEN(cp, length)) {
				sum = udp6_cksum(ndo, ip6, up, length + sizeof(struct udphdr));
				udp_sum = GET_BE_U_2(up->uh_sum);

				if (sum != 0) {
					ND_PRINT("[bad udp cksum 0x%04x -> 0x%04x!] ",
					    udp_sum,
					    in_cksum_shouldbe(udp_sum, sum));
				} else
					ND_PRINT("[udp sum ok] ");
			}
		}
	}

	if (!ndo->ndo_qflag) {
		if (IS_SRC_OR_DST_PORT(NAMESERVER_PORT))
			/* over_tcp: FALSE, is_mdns: FALSE */
			domain_print(ndo, cp, length, FALSE, FALSE);
		else if (IS_SRC_OR_DST_PORT(BOOTPC_PORT) ||
			 IS_SRC_OR_DST_PORT(BOOTPS_PORT))
			bootp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(TFTP_PORT))
			tftp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(KERBEROS_PORT))
			krb_print(ndo, (const u_char *)cp);
		else if (IS_SRC_OR_DST_PORT(NTP_PORT))
			ntp_print(ndo, cp, length);
#ifdef ENABLE_SMB
		else if (IS_SRC_OR_DST_PORT(NETBIOS_NS_PORT))
			nbt_udp137_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(NETBIOS_DGRAM_PORT))
			nbt_udp138_print(ndo, cp, length);
#endif
		else if (IS_SRC_OR_DST_PORT(SNMP_PORT) ||
			 IS_SRC_OR_DST_PORT(SNMPTRAP_PORT))
			snmp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(PTP_EVENT_PORT) ||
			 IS_SRC_OR_DST_PORT(PTP_GENERAL_PORT))
			ptp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(CISCO_AUTORP_PORT))
			cisco_autorp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(ISAKMP_PORT))
			 isakmp_print(ndo, cp, length, bp2);
		else if (IS_SRC_OR_DST_PORT(SYSLOG_PORT))
			syslog_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(RIP_PORT))
			rip_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(RIPNG_PORT))
			ripng_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(TIMED_PORT))
			timed_print(ndo, (const u_char *)cp);
		else if (IS_SRC_OR_DST_PORT(DHCP6_SERV_PORT) ||
			 IS_SRC_OR_DST_PORT(DHCP6_CLI_PORT))
			dhcp6_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(LDP_PORT))
			ldp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(AODV_PORT))
			aodv_print(ndo, cp, length, IP_V(ip) == 6);
		else if (IS_SRC_OR_DST_PORT(OLSR_PORT))
			olsr_print(ndo, cp, length, IP_V(ip) == 6);
		else if (IS_SRC_OR_DST_PORT(LMP_PORT))
			lmp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(KERBEROS_SEC_PORT))
			krb_print(ndo, (const u_char *)cp);
		else if (IS_SRC_OR_DST_PORT(LWRES_PORT))
			lwres_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(MULTICASTDNS_PORT))
			/* over_tcp: FALSE, is_mdns: TRUE */
			domain_print(ndo, cp, length, FALSE, TRUE);
		else if (IS_SRC_OR_DST_PORT(ISAKMP_PORT_NATT))
			 isakmp_rfc3948_print(ndo, cp, length, bp2, IP_V(ip), fragmented, ttl_hl);
		else if (IS_SRC_OR_DST_PORT(ISAKMP_PORT_USER1) || IS_SRC_OR_DST_PORT(ISAKMP_PORT_USER2))
			isakmp_print(ndo, cp, length, bp2);
		else if (IS_SRC_OR_DST_PORT(L2TP_PORT))
			l2tp_print(ndo, cp, length);
		else if (dport == VAT_PORT)
			vat_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(ZEPHYR_SRV_PORT) || IS_SRC_OR_DST_PORT(ZEPHYR_CLT_PORT))
			zephyr_print(ndo, cp, length);
		/*
		 * Since there are 10 possible ports to check, I think
		 * a <> test would be more efficient
		 */
		else if ((sport >= RX_PORT_LOW && sport <= RX_PORT_HIGH) ||
			 (dport >= RX_PORT_LOW && dport <= RX_PORT_HIGH))
			rx_print(ndo, cp, length, sport, dport,
				 (const u_char *) ip);
		else if (IS_SRC_OR_DST_PORT(AHCP_PORT))
			ahcp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(BABEL_PORT) || IS_SRC_OR_DST_PORT(BABEL_PORT_OLD))
			babel_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(HNCP_PORT))
			hncp_print(ndo, cp, length);
		/*
		 * Kludge in test for whiteboard packets.
		 */
		else if (dport == WB_PORT)
			wb_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(RADIUS_PORT) ||
			 IS_SRC_OR_DST_PORT(RADIUS_NEW_PORT) ||
			 IS_SRC_OR_DST_PORT(RADIUS_ACCOUNTING_PORT) ||
			 IS_SRC_OR_DST_PORT(RADIUS_NEW_ACCOUNTING_PORT) ||
			 IS_SRC_OR_DST_PORT(RADIUS_CISCO_COA_PORT) ||
			 IS_SRC_OR_DST_PORT(RADIUS_COA_PORT) )
			radius_print(ndo, cp, length);
		else if (dport == HSRP_PORT)
			hsrp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(MPLS_LSP_PING_PORT))
			lspping_print(ndo, cp, length);
		else if (dport == BFD_CONTROL_PORT ||
			 dport == BFD_MULTIHOP_PORT ||
			 dport == BFD_LAG_PORT ||
			 dport == BFD_ECHO_PORT )
			bfd_print(ndo, cp, length, dport);
		else if (IS_SRC_OR_DST_PORT(VQP_PORT))
			vqp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(SFLOW_PORT))
			sflow_print(ndo, cp, length);
		else if (dport == LWAPP_CONTROL_PORT)
			lwapp_control_print(ndo, cp, length, 1);
		else if (sport == LWAPP_CONTROL_PORT)
			lwapp_control_print(ndo, cp, length, 0);
		else if (IS_SRC_OR_DST_PORT(LWAPP_DATA_PORT))
			lwapp_data_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(SIP_PORT))
			sip_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(OTV_PORT))
			otv_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(VXLAN_PORT))
			vxlan_print(ndo, cp, length);
		else if (dport == GENEVE_PORT)
			geneve_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(LISP_CONTROL_PORT))
			lisp_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(VXLAN_GPE_PORT))
			vxlan_gpe_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(ZEP_PORT))
			zep_print(ndo, cp, length);
		else if (IS_SRC_OR_DST_PORT(MPLS_PORT))
			mpls_print(ndo, cp, length);
		else if (ND_TTEST_1(((const struct LAP *)cp)->type) &&
			 GET_U_1(((const struct LAP *)cp)->type) == lapDDP &&
			 (atalk_port(sport) || atalk_port(dport))) {
			if (ndo->ndo_vflag)
				ND_PRINT("kip ");
			llap_print(ndo, cp, length);
		} else if (IS_SRC_OR_DST_PORT(SOMEIP_PORT))
			someip_print(ndo, cp, length);
		else if (sport == BCM_LI_PORT)
			bcm_li_print(ndo, cp, length);
		else {
			if (ulen > length && !fragmented)
				ND_PRINT("UDP, bad length %u > %u",
				    ulen, length);
			else
				ND_PRINT("UDP, length %u", ulen);
		}
	} else {
		if (ulen > length && !fragmented)
			ND_PRINT("UDP, bad length %u > %u",
			    ulen, length);
		else
			ND_PRINT("UDP, length %u", ulen);
	}
	return;

trunc:
	nd_print_trunc(ndo);
}
