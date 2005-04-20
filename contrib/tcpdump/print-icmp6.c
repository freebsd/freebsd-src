/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994
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
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-icmp6.c,v 1.72.2.4 2004/03/24 00:14:09 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef INET6

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "ip6.h"
#include "icmp6.h"
#include "ipproto.h"

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#include "udp.h"
#include "ah.h"

static const char *get_rtpref(u_int);
static const char *get_lifetime(u_int32_t);
static void print_lladdr(const u_char *, size_t);
static void icmp6_opt_print(const u_char *, int);
static void mld6_print(const u_char *);
static struct udphdr *get_upperlayer(u_char *, u_int *);
static void dnsname_print(const u_char *, const u_char *);
static void icmp6_nodeinfo_print(u_int, const u_char *, const u_char *);
static void icmp6_rrenum_print(const u_char *, const u_char *);

#ifndef abs
#define abs(a)	((0 < (a)) ? (a) : -(a))
#endif

static const char *
get_rtpref(u_int v)
{
	static const char *rtpref_str[] = {
		"medium",		/* 00 */
		"high",			/* 01 */
		"rsv",			/* 10 */
		"low"			/* 11 */
	};

	return rtpref_str[((v & ND_RA_FLAG_RTPREF_MASK) >> 3) & 0xff];
}

static const char *
get_lifetime(u_int32_t v)
{
	static char buf[20];

	if (v == (u_int32_t)~0UL)
		return "infinity";
	else {
		snprintf(buf, sizeof(buf), "%u", v);
		return buf;
	}
}

static void
print_lladdr(const u_int8_t *p, size_t l)
{
	const u_int8_t *ep, *q;

	q = p;
	ep = p + l;
	while (l > 0 && q < ep) {
		if (q > p)
			printf(":");
		printf("%02x", *q++);
		l--;
	}
}

static int icmp6_cksum(const struct ip6_hdr *ip6, const struct icmp6_hdr *icp,
	u_int len)
{
	size_t i;
	register const u_int16_t *sp;
	u_int32_t sum;
	union {
		struct {
			struct in6_addr ph_src;
			struct in6_addr ph_dst;
			u_int32_t	ph_len;
			u_int8_t	ph_zero[3];
			u_int8_t	ph_nxt;
		} ph;
		u_int16_t pa[20];
	} phu;

	/* pseudo-header */
	memset(&phu, 0, sizeof(phu));
	phu.ph.ph_src = ip6->ip6_src;
	phu.ph.ph_dst = ip6->ip6_dst;
	phu.ph.ph_len = htonl(len);
	phu.ph.ph_nxt = IPPROTO_ICMPV6;

	sum = 0;
	for (i = 0; i < sizeof(phu.pa) / sizeof(phu.pa[0]); i++)
		sum += phu.pa[i];

	sp = (const u_int16_t *)icp;

	for (i = 0; i < (len & ~1); i += 2)
		sum += *sp++;

	if (len & 1)
		sum += htons((*(const u_int8_t *)sp) << 8);

	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum = ~sum & 0xffff;

	return (sum);
}

void
icmp6_print(const u_char *bp, u_int length, const u_char *bp2, int fragmented)
{
	const struct icmp6_hdr *dp;
	const struct ip6_hdr *ip;
	const char *str;
	const struct ip6_hdr *oip;
	const struct udphdr *ouh;
	int dport;
	const u_char *ep;
	char buf[256];
	u_int prot;

	dp = (struct icmp6_hdr *)bp;
	ip = (struct ip6_hdr *)bp2;
	oip = (struct ip6_hdr *)(dp + 1);
	str = buf;
	/* 'ep' points to the end of available data. */
	ep = snapend;

	TCHECK(dp->icmp6_cksum);

	if (vflag && !fragmented) {
		int sum = dp->icmp6_cksum;

		if (TTEST2(bp[0], length)) {
			sum = icmp6_cksum(ip, dp, length);
			if (sum != 0)
				(void)printf("[bad icmp6 cksum %x!] ", sum);
			else
				(void)printf("[icmp6 sum ok] ");
		}
	}

	switch (dp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		TCHECK(oip->ip6_dst);
		switch (dp->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			printf("icmp6: %s unreachable route",
			       ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			printf("icmp6: %s unreachable prohibited",
			       ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			printf("icmp6: %s beyond scope of source address %s",
			       ip6addr_string(&oip->ip6_dst),
			       ip6addr_string(&oip->ip6_src));
			break;
		case ICMP6_DST_UNREACH_ADDR:
			printf("icmp6: %s unreachable address",
			       ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			if ((ouh = get_upperlayer((u_char *)oip, &prot))
			    == NULL)
				goto trunc;

			dport = EXTRACT_16BITS(&ouh->uh_dport);
			switch (prot) {
			case IPPROTO_TCP:
				printf("icmp6: %s tcp port %s unreachable",
					ip6addr_string(&oip->ip6_dst),
					tcpport_string(dport));
				break;
			case IPPROTO_UDP:
				printf("icmp6: %s udp port %s unreachable",
					ip6addr_string(&oip->ip6_dst),
					udpport_string(dport));
				break;
			default:
				printf("icmp6: %s protocol %d port %d unreachable",
					ip6addr_string(&oip->ip6_dst),
					oip->ip6_nxt, dport);
				break;
			}
			break;
		default:
			printf("icmp6: %s unreachable code-#%d",
				ip6addr_string(&oip->ip6_dst),
				dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_PACKET_TOO_BIG:
		TCHECK(dp->icmp6_mtu);
		printf("icmp6: too big %u", EXTRACT_32BITS(&dp->icmp6_mtu));
		break;
	case ICMP6_TIME_EXCEEDED:
		TCHECK(oip->ip6_dst);
		switch (dp->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			printf("icmp6: time exceeded in-transit for %s",
				ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			printf("icmp6: ip6 reassembly time exceeded");
			break;
		default:
			printf("icmp6: time exceeded code-#%d",
				dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_PARAM_PROB:
		TCHECK(oip->ip6_dst);
		switch (dp->icmp6_code) {
		case ICMP6_PARAMPROB_HEADER:
			printf("icmp6: parameter problem errorneous - octet %u",
				EXTRACT_32BITS(&dp->icmp6_pptr));
			break;
		case ICMP6_PARAMPROB_NEXTHEADER:
			printf("icmp6: parameter problem next header - octet %u",
				EXTRACT_32BITS(&dp->icmp6_pptr));
			break;
		case ICMP6_PARAMPROB_OPTION:
			printf("icmp6: parameter problem option - octet %u",
				EXTRACT_32BITS(&dp->icmp6_pptr));
			break;
		default:
			printf("icmp6: parameter problem code-#%d",
			       dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_ECHO_REQUEST:
	case ICMP6_ECHO_REPLY:
		TCHECK(dp->icmp6_seq);
		printf("icmp6: echo %s seq %u",
			dp->icmp6_type == ICMP6_ECHO_REQUEST ?
			"request" : "reply",
			EXTRACT_16BITS(&dp->icmp6_seq));
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		printf("icmp6: multicast listener query ");
		mld6_print((const u_char *)dp);
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		printf("icmp6: multicast listener report ");
		mld6_print((const u_char *)dp);
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		printf("icmp6: multicast listener done ");
		mld6_print((const u_char *)dp);
		break;
	case ND_ROUTER_SOLICIT:
		printf("icmp6: router solicitation ");
		if (vflag) {
#define RTSOLLEN 8
			icmp6_opt_print((const u_char *)dp + RTSOLLEN,
					length - RTSOLLEN);
		}
		break;
	case ND_ROUTER_ADVERT:
		printf("icmp6: router advertisement");
		if (vflag) {
			struct nd_router_advert *p;

			p = (struct nd_router_advert *)dp;
			TCHECK(p->nd_ra_retransmit);
			printf("(chlim=%d, ", (int)p->nd_ra_curhoplimit);
			if (p->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED)
				printf("M");
			if (p->nd_ra_flags_reserved & ND_RA_FLAG_OTHER)
				printf("O");
			if (p->nd_ra_flags_reserved & ND_RA_FLAG_HOME_AGENT)
				printf("H");

			if ((p->nd_ra_flags_reserved & ~ND_RA_FLAG_RTPREF_MASK)
			    != 0)
				printf(" ");

			printf("pref=%s, ",
			    get_rtpref(p->nd_ra_flags_reserved));

			printf("router_ltime=%d, ", EXTRACT_16BITS(&p->nd_ra_router_lifetime));
			printf("reachable_time=%u, ",
				EXTRACT_32BITS(&p->nd_ra_reachable));
			printf("retrans_time=%u)",
				EXTRACT_32BITS(&p->nd_ra_retransmit));
#define RTADVLEN 16
			icmp6_opt_print((const u_char *)dp + RTADVLEN,
					length - RTADVLEN);
		}
		break;
	case ND_NEIGHBOR_SOLICIT:
	    {
		struct nd_neighbor_solicit *p;
		p = (struct nd_neighbor_solicit *)dp;
		TCHECK(p->nd_ns_target);
		printf("icmp6: neighbor sol: who has %s",
			ip6addr_string(&p->nd_ns_target));
		if (vflag) {
#define NDSOLLEN 24
			icmp6_opt_print((const u_char *)dp + NDSOLLEN,
					length - NDSOLLEN);
		}
	    }
		break;
	case ND_NEIGHBOR_ADVERT:
	    {
		struct nd_neighbor_advert *p;

		p = (struct nd_neighbor_advert *)dp;
		TCHECK(p->nd_na_target);
		printf("icmp6: neighbor adv: tgt is %s",
			ip6addr_string(&p->nd_na_target));
		if (vflag) {
#define ND_NA_FLAG_ALL	\
	(ND_NA_FLAG_ROUTER|ND_NA_FLAG_SOLICITED|ND_NA_FLAG_OVERRIDE)
			/* we don't need ntohl() here.  see advanced-api-04. */
			if (p->nd_na_flags_reserved &  ND_NA_FLAG_ALL) {
#undef ND_NA_FLAG_ALL
				u_int32_t flags;

				flags = p->nd_na_flags_reserved;
				printf("(");
				if (flags & ND_NA_FLAG_ROUTER)
					printf("R");
				if (flags & ND_NA_FLAG_SOLICITED)
					printf("S");
				if (flags & ND_NA_FLAG_OVERRIDE)
					printf("O");
				printf(")");
			}
#define NDADVLEN 24
			icmp6_opt_print((const u_char *)dp + NDADVLEN,
					length - NDADVLEN);
#undef NDADVLEN
		}
	    }
		break;
	case ND_REDIRECT:
#define RDR(i) ((struct nd_redirect *)(i))
		TCHECK(RDR(dp)->nd_rd_dst);
		printf("icmp6: redirect %s",
		    getname6((const u_char *)&RDR(dp)->nd_rd_dst));
		TCHECK(RDR(dp)->nd_rd_target);
		printf(" to %s",
		    getname6((const u_char*)&RDR(dp)->nd_rd_target));
#define REDIRECTLEN 40
		if (vflag) {
			icmp6_opt_print((const u_char *)dp + REDIRECTLEN,
					length - REDIRECTLEN);
		}
		break;
#undef REDIRECTLEN
#undef RDR
	case ICMP6_ROUTER_RENUMBERING:
		icmp6_rrenum_print(bp, ep);
		break;
	case ICMP6_NI_QUERY:
	case ICMP6_NI_REPLY:
		icmp6_nodeinfo_print(length, bp, ep);
		break;
	case ICMP6_HADISCOV_REQUEST:
		printf("icmp6: ha discovery request");
		if (vflag) {
			TCHECK(dp->icmp6_data16[0]);
			printf("(id=%d)", EXTRACT_16BITS(&dp->icmp6_data16[0]));
		}
		break;
	case ICMP6_HADISCOV_REPLY:
		printf("icmp6: ha discovery reply");
		if (vflag) {
			struct in6_addr *in6;
			u_char *cp;

			TCHECK(dp->icmp6_data16[0]);
			printf("(id=%d", EXTRACT_16BITS(&dp->icmp6_data16[0]));
			cp = (u_char *)dp + length;
			in6 = (struct in6_addr *)(dp + 1);
			for (; (u_char *)in6 < cp; in6++) {
				TCHECK(*in6);
				printf(", %s", ip6addr_string(in6));
			}
			printf(")");
		}
		break;
	case ICMP6_MOBILEPREFIX_SOLICIT:
		printf("icmp6: mobile router solicitation");
		if (vflag) {
			TCHECK(dp->icmp6_data16[0]);
			printf("(id=%d)", EXTRACT_16BITS(&dp->icmp6_data16[0]));
		}
		break;
	case ICMP6_MOBILEPREFIX_ADVERT:
		printf("icmp6: mobile router advertisement");
		if (vflag) {
			TCHECK(dp->icmp6_data16[0]);
			printf("(id=%d", EXTRACT_16BITS(&dp->icmp6_data16[0]));
			if (dp->icmp6_data16[1] & 0xc0)
				printf(" ");
			if (dp->icmp6_data16[1] & 0x80)
				printf("M");
			if (dp->icmp6_data16[1] & 0x40)
				printf("O");
			printf(")");
#define MPADVLEN 8
			icmp6_opt_print((const u_char *)dp + MPADVLEN,
					length - MPADVLEN);
		}
		break;
	default:
		printf("icmp6: type-#%d", dp->icmp6_type);
		break;
	}
	return;
trunc:
	fputs("[|icmp6]", stdout);
}

static struct udphdr *
get_upperlayer(u_char *bp, u_int *prot)
{
	const u_char *ep;
	struct ip6_hdr *ip6 = (struct ip6_hdr *)bp;
	struct udphdr *uh;
	struct ip6_hbh *hbh;
	struct ip6_frag *fragh;
	struct ah *ah;
	u_int nh;
	int hlen;

	/* 'ep' points to the end of available data. */
	ep = snapend;

	if (!TTEST(ip6->ip6_nxt))
		return NULL;

	nh = ip6->ip6_nxt;
	hlen = sizeof(struct ip6_hdr);

	while (bp < snapend) {
		bp += hlen;

		switch(nh) {
		case IPPROTO_UDP:
		case IPPROTO_TCP:
			uh = (struct udphdr *)bp;
			if (TTEST(uh->uh_dport)) {
				*prot = nh;
				return(uh);
			}
			else
				return(NULL);
			/* NOTREACHED */

		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
		case IPPROTO_ROUTING:
			hbh = (struct ip6_hbh *)bp;
			if (!TTEST(hbh->ip6h_len))
				return(NULL);
			nh = hbh->ip6h_nxt;
			hlen = (hbh->ip6h_len + 1) << 3;
			break;

		case IPPROTO_FRAGMENT: /* this should be odd, but try anyway */
			fragh = (struct ip6_frag *)bp;
			if (!TTEST(fragh->ip6f_offlg))
				return(NULL);
			/* fragments with non-zero offset are meaningless */
			if ((EXTRACT_16BITS(&fragh->ip6f_offlg) & IP6F_OFF_MASK) != 0)
				return(NULL);
			nh = fragh->ip6f_nxt;
			hlen = sizeof(struct ip6_frag);
			break;

		case IPPROTO_AH:
			ah = (struct ah *)bp;
			if (!TTEST(ah->ah_len))
				return(NULL);
			nh = ah->ah_nxt;
			hlen = (ah->ah_len + 2) << 2;
			break;

		default:	/* unknown or undecodable header */
			*prot = nh; /* meaningless, but set here anyway */
			return(NULL);
		}
	}

	return(NULL);		/* should be notreached, though */
}

static void
icmp6_opt_print(const u_char *bp, int resid)
{
	const struct nd_opt_hdr *op;
	const struct nd_opt_hdr *opl;	/* why there's no struct? */
	const struct nd_opt_prefix_info *opp;
	const struct icmp6_opts_redirect *opr;
	const struct nd_opt_mtu *opm;
	const struct nd_opt_advinterval *opa;
	const struct nd_opt_homeagent_info *oph;
	const struct nd_opt_route_info *opri;
	const u_char *cp, *ep;
	struct in6_addr in6, *in6p;
	size_t l;

#define ECHECK(var) if ((u_char *)&(var) > ep - sizeof(var)) return

	cp = bp;
	/* 'ep' points to the end of available data. */
	ep = snapend;

	while (cp < ep) {
		op = (struct nd_opt_hdr *)cp;

		ECHECK(op->nd_opt_len);
		if (resid <= 0)
			return;
		if (op->nd_opt_len == 0)
			goto trunc;
		if (cp + (op->nd_opt_len << 3) > ep)
			goto trunc;

		switch (op->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
			opl = (struct nd_opt_hdr *)op;
			printf("(src lladdr: ");
			l = (op->nd_opt_len << 3) - 2;
			print_lladdr(cp + 2, l);
			/*(*/
			printf(")");
			break;
		case ND_OPT_TARGET_LINKADDR:
			opl = (struct nd_opt_hdr *)op;
			printf("(tgt lladdr: ");
			l = (op->nd_opt_len << 3) - 2;
			print_lladdr(cp + 2, l);
			/*(*/
			printf(")");
			break;
		case ND_OPT_PREFIX_INFORMATION:
			opp = (struct nd_opt_prefix_info *)op;
			TCHECK(opp->nd_opt_pi_prefix);
			printf("(prefix info: ");	/*)*/
			if (op->nd_opt_len != 4) {
				printf("badlen");
				/*(*/
				printf(")");
				break;
			}
			if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK)
				printf("L");
			if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO)
				printf("A");
			if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ROUTER)
				printf("R");
			if (opp->nd_opt_pi_flags_reserved)
				printf(" ");
			printf("valid_ltime=%s,",
			    get_lifetime(EXTRACT_32BITS(&opp->nd_opt_pi_valid_time)));
			printf("preferred_ltime=%s,",
			    get_lifetime(EXTRACT_32BITS(&opp->nd_opt_pi_preferred_time)));
			printf("prefix=%s/%d",
			    ip6addr_string(&opp->nd_opt_pi_prefix),
			    opp->nd_opt_pi_prefix_len);
			if (opp->nd_opt_pi_len != 4)
				printf("!");
			/*(*/
			printf(")");
			break;
		case ND_OPT_REDIRECTED_HEADER:
			opr = (struct icmp6_opts_redirect *)op;
			printf("(redirect)");
			/* xxx */
			break;
		case ND_OPT_MTU:
			opm = (struct nd_opt_mtu *)op;
			TCHECK(opm->nd_opt_mtu_mtu);
			printf("(mtu:");	/*)*/
			if (op->nd_opt_len != 1) {
				printf("badlen");
				/*(*/
				printf(")");
				break;
			}
			printf(" mtu=%u", EXTRACT_32BITS(&opm->nd_opt_mtu_mtu));
			if (opm->nd_opt_mtu_len != 1)
				printf("!");
			printf(")");
			break;
		case ND_OPT_ADVINTERVAL:
			opa = (struct nd_opt_advinterval *)op;
			TCHECK(opa->nd_opt_adv_interval);
			printf("(advint:");	/*)*/
			printf(" advint=%u",
			    EXTRACT_32BITS(&opa->nd_opt_adv_interval));
			/*(*/
			printf(")");
			break;
		case ND_OPT_HOMEAGENT_INFO:
			oph = (struct nd_opt_homeagent_info *)op;
			TCHECK(oph->nd_opt_hai_lifetime);
			printf("(ha info:");	/*)*/
			printf(" pref=%d", EXTRACT_16BITS(&oph->nd_opt_hai_preference));
			printf(", lifetime=%u", EXTRACT_16BITS(&oph->nd_opt_hai_lifetime));
			printf(")");
			break;
		case ND_OPT_ROUTE_INFO:
			opri = (struct nd_opt_route_info *)op;
			TCHECK(opri->nd_opt_rti_lifetime);
			memset(&in6, 0, sizeof(in6));
			in6p = (struct in6_addr *)(opri + 1);
			switch (op->nd_opt_len) {
			case 1:
				break;
			case 2:
				TCHECK2(*in6p, 8);
				memcpy(&in6, opri + 1, 8);
				break;
			case 3:
				TCHECK(*in6p);
				memcpy(&in6, opri + 1, sizeof(in6));
				break;
			default:
				goto trunc;
			}
			printf("(rtinfo:");	/*)*/
			printf(" %s/%u", ip6addr_string(&in6),
			    opri->nd_opt_rti_prefixlen);
			printf(", pref=%s", get_rtpref(opri->nd_opt_rti_flags));
			printf(", lifetime=%s",
			    get_lifetime(EXTRACT_32BITS(&opri->nd_opt_rti_lifetime)));
			/*(*/
			printf(")");
			break;
		default:
			printf("(unknown opt_type=%d, opt_len=%d)",
			       op->nd_opt_type, op->nd_opt_len);
			break;
		}

		cp += op->nd_opt_len << 3;
		resid -= op->nd_opt_len << 3;
	}
	return;

 trunc:
	fputs("[ndp opt]", stdout);
	return;
#undef ECHECK
}

static void
mld6_print(const u_char *bp)
{
	struct mld6_hdr *mp = (struct mld6_hdr *)bp;
	const u_char *ep;

	/* 'ep' points to the end of available data. */
	ep = snapend;

	if ((u_char *)mp + sizeof(*mp) > ep)
		return;

	printf("max resp delay: %d ", EXTRACT_16BITS(&mp->mld6_maxdelay));
	printf("addr: %s", ip6addr_string(&mp->mld6_addr));
}

static void
dnsname_print(const u_char *cp, const u_char *ep)
{
	int i;

	/* DNS name decoding - no decompression */
	printf(", \"");
	while (cp < ep) {
		i = *cp++;
		if (i) {
			if (i > ep - cp) {
				printf("???");
				break;
			}
			while (i-- && cp < ep) {
				safeputchar(*cp);
				cp++;
			}
			if (cp + 1 < ep && *cp)
				printf(".");
		} else {
			if (cp == ep) {
				/* FQDN */
				printf(".");
			} else if (cp + 1 == ep && *cp == '\0') {
				/* truncated */
			} else {
				/* invalid */
				printf("???");
			}
			break;
		}
	}
	printf("\"");
}

static void
icmp6_nodeinfo_print(u_int icmp6len, const u_char *bp, const u_char *ep)
{
	struct icmp6_nodeinfo *ni6;
	struct icmp6_hdr *dp;
	const u_char *cp;
	size_t siz, i;
	int needcomma;

	if (ep < bp)
		return;
	dp = (struct icmp6_hdr *)bp;
	ni6 = (struct icmp6_nodeinfo *)bp;
	siz = ep - bp;

	switch (ni6->ni_type) {
	case ICMP6_NI_QUERY:
		if (siz == sizeof(*dp) + 4) {
			/* KAME who-are-you */
			printf("icmp6: who-are-you request");
			break;
		}
		printf("icmp6: node information query");

		TCHECK2(*dp, sizeof(*ni6));
		ni6 = (struct icmp6_nodeinfo *)dp;
		printf(" (");	/*)*/
		switch (EXTRACT_16BITS(&ni6->ni_qtype)) {
		case NI_QTYPE_NOOP:
			printf("noop");
			break;
		case NI_QTYPE_SUPTYPES:
			printf("supported qtypes");
			i = EXTRACT_16BITS(&ni6->ni_flags);
			if (i)
				printf(" [%s]", (i & 0x01) ? "C" : "");
			break;
			break;
		case NI_QTYPE_FQDN:
			printf("DNS name");
			break;
		case NI_QTYPE_NODEADDR:
			printf("node addresses");
			i = ni6->ni_flags;
			if (!i)
				break;
			/* NI_NODEADDR_FLAG_TRUNCATE undefined for query */
			printf(" [%s%s%s%s%s%s]",
			    (i & NI_NODEADDR_FLAG_ANYCAST) ? "a" : "",
			    (i & NI_NODEADDR_FLAG_GLOBAL) ? "G" : "",
			    (i & NI_NODEADDR_FLAG_SITELOCAL) ? "S" : "",
			    (i & NI_NODEADDR_FLAG_LINKLOCAL) ? "L" : "",
			    (i & NI_NODEADDR_FLAG_COMPAT) ? "C" : "",
			    (i & NI_NODEADDR_FLAG_ALL) ? "A" : "");
			break;
		default:
			printf("unknown");
			break;
		}

		if (ni6->ni_qtype == NI_QTYPE_NOOP ||
		    ni6->ni_qtype == NI_QTYPE_SUPTYPES) {
			if (siz != sizeof(*ni6))
				if (vflag)
					printf(", invalid len");
			/*(*/
			printf(")");
			break;
		}


		/* XXX backward compat, icmp-name-lookup-03 */
		if (siz == sizeof(*ni6)) {
			printf(", 03 draft");
			/*(*/
			printf(")");
			break;
		}

		switch (ni6->ni_code) {
		case ICMP6_NI_SUBJ_IPV6:
			if (!TTEST2(*dp,
			    sizeof(*ni6) + sizeof(struct in6_addr)))
				break;
			if (siz != sizeof(*ni6) + sizeof(struct in6_addr)) {
				if (vflag)
					printf(", invalid subject len");
				break;
			}
			printf(", subject=%s",
			    getname6((const u_char *)(ni6 + 1)));
			break;
		case ICMP6_NI_SUBJ_FQDN:
			printf(", subject=DNS name");
			cp = (const u_char *)(ni6 + 1);
			if (cp[0] == ep - cp - 1) {
				/* icmp-name-lookup-03, pascal string */
				if (vflag)
					printf(", 03 draft");
				cp++;
				printf(", \"");
				while (cp < ep) {
					safeputchar(*cp);
					cp++;
				}
				printf("\"");
			} else
				dnsname_print(cp, ep);
			break;
		case ICMP6_NI_SUBJ_IPV4:
			if (!TTEST2(*dp, sizeof(*ni6) + sizeof(struct in_addr)))
				break;
			if (siz != sizeof(*ni6) + sizeof(struct in_addr)) {
				if (vflag)
					printf(", invalid subject len");
				break;
			}
			printf(", subject=%s",
			    getname((const u_char *)(ni6 + 1)));
			break;
		default:
			printf(", unknown subject");
			break;
		}

		/*(*/
		printf(")");
		break;

	case ICMP6_NI_REPLY:
		if (icmp6len > siz) {
			printf("[|icmp6: node information reply]");
			break;
		}

		needcomma = 0;

		ni6 = (struct icmp6_nodeinfo *)dp;
		printf("icmp6: node information reply");
		printf(" (");	/*)*/
		switch (ni6->ni_code) {
		case ICMP6_NI_SUCCESS:
			if (vflag) {
				printf("success");
				needcomma++;
			}
			break;
		case ICMP6_NI_REFUSED:
			printf("refused");
			needcomma++;
			if (siz != sizeof(*ni6))
				if (vflag)
					printf(", invalid length");
			break;
		case ICMP6_NI_UNKNOWN:
			printf("unknown");
			needcomma++;
			if (siz != sizeof(*ni6))
				if (vflag)
					printf(", invalid length");
			break;
		}

		if (ni6->ni_code != ICMP6_NI_SUCCESS) {
			/*(*/
			printf(")");
			break;
		}

		switch (EXTRACT_16BITS(&ni6->ni_qtype)) {
		case NI_QTYPE_NOOP:
			if (needcomma)
				printf(", ");
			printf("noop");
			if (siz != sizeof(*ni6))
				if (vflag)
					printf(", invalid length");
			break;
		case NI_QTYPE_SUPTYPES:
			if (needcomma)
				printf(", ");
			printf("supported qtypes");
			i = EXTRACT_16BITS(&ni6->ni_flags);
			if (i)
				printf(" [%s]", (i & 0x01) ? "C" : "");
			break;
		case NI_QTYPE_FQDN:
			if (needcomma)
				printf(", ");
			printf("DNS name");
			cp = (const u_char *)(ni6 + 1) + 4;
			if (cp[0] == ep - cp - 1) {
				/* icmp-name-lookup-03, pascal string */
				if (vflag)
					printf(", 03 draft");
				cp++;
				printf(", \"");
				while (cp < ep) {
					safeputchar(*cp);
					cp++;
				}
				printf("\"");
			} else
				dnsname_print(cp, ep);
			if ((EXTRACT_16BITS(&ni6->ni_flags) & 0x01) != 0)
				printf(" [TTL=%u]", *(u_int32_t *)(ni6 + 1));
			break;
		case NI_QTYPE_NODEADDR:
			if (needcomma)
				printf(", ");
			printf("node addresses");
			i = sizeof(*ni6);
			while (i < siz) {
				if (i + sizeof(struct in6_addr) + sizeof(int32_t) > siz)
					break;
				printf(" %s", getname6(bp + i));
				i += sizeof(struct in6_addr);
				printf("(%d)", (int32_t)EXTRACT_32BITS(bp + i));
				i += sizeof(int32_t);
			}
			i = ni6->ni_flags;
			if (!i)
				break;
			printf(" [%s%s%s%s%s%s%s]",
			    (i & NI_NODEADDR_FLAG_ANYCAST) ? "a" : "",
			    (i & NI_NODEADDR_FLAG_GLOBAL) ? "G" : "",
			    (i & NI_NODEADDR_FLAG_SITELOCAL) ? "S" : "",
			    (i & NI_NODEADDR_FLAG_LINKLOCAL) ? "L" : "",
			    (i & NI_NODEADDR_FLAG_COMPAT) ? "C" : "",
			    (i & NI_NODEADDR_FLAG_ALL) ? "A" : "",
			    (i & NI_NODEADDR_FLAG_TRUNCATE) ? "T" : "");
			break;
		default:
			if (needcomma)
				printf(", ");
			printf("unknown");
			break;
		}

		/*(*/
		printf(")");
		break;
	}
	return;

trunc:
	fputs("[|icmp6]", stdout);
}

static void
icmp6_rrenum_print(const u_char *bp, const u_char *ep)
{
	struct icmp6_router_renum *rr6;
	struct icmp6_hdr *dp;
	size_t siz;
	const char *cp;
	struct rr_pco_match *match;
	struct rr_pco_use *use;
	char hbuf[NI_MAXHOST];
	int n;

	if (ep < bp)
		return;
	dp = (struct icmp6_hdr *)bp;
	rr6 = (struct icmp6_router_renum *)bp;
	siz = ep - bp;
	cp = (const char *)(rr6 + 1);

	TCHECK(rr6->rr_reserved);
	switch (rr6->rr_code) {
	case ICMP6_ROUTER_RENUMBERING_COMMAND:
		printf("router renum: command");
		break;
	case ICMP6_ROUTER_RENUMBERING_RESULT:
		printf("router renum: result");
		break;
	case ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET:
		printf("router renum: sequence number reset");
		break;
	default:
		printf("router renum: code-#%d", rr6->rr_code);
		break;
	}

	printf(", seq=%u", EXTRACT_32BITS(&rr6->rr_seqnum));

	if (vflag) {
#define F(x, y)	((rr6->rr_flags) & (x) ? (y) : "")
		printf("[");	/*]*/
		if (rr6->rr_flags) {
			printf("%s%s%s%s%s,", F(ICMP6_RR_FLAGS_TEST, "T"),
			    F(ICMP6_RR_FLAGS_REQRESULT, "R"),
			    F(ICMP6_RR_FLAGS_FORCEAPPLY, "A"),
			    F(ICMP6_RR_FLAGS_SPECSITE, "S"),
			    F(ICMP6_RR_FLAGS_PREVDONE, "P"));
		}
		printf("seg=%u,", rr6->rr_segnum);
		printf("maxdelay=%u", rr6->rr_maxdelay);
		if (rr6->rr_reserved)
			printf("rsvd=0x%x", EXTRACT_16BITS(&rr6->rr_reserved));
		/*[*/
		printf("]");
#undef F
	}

	if (rr6->rr_code == ICMP6_ROUTER_RENUMBERING_COMMAND) {
		match = (struct rr_pco_match *)cp;
		cp = (const char *)(match + 1);

		TCHECK(match->rpm_prefix);

		if (vflag > 1)
			printf("\n\t");
		else
			printf(" ");
		printf("match(");	/*)*/
		switch (match->rpm_code) {
		case RPM_PCO_ADD:	printf("add"); break;
		case RPM_PCO_CHANGE:	printf("change"); break;
		case RPM_PCO_SETGLOBAL:	printf("setglobal"); break;
		default:		printf("#%u", match->rpm_code); break;
		}

		if (vflag) {
			printf(",ord=%u", match->rpm_ordinal);
			printf(",min=%u", match->rpm_minlen);
			printf(",max=%u", match->rpm_maxlen);
		}
		if (inet_ntop(AF_INET6, &match->rpm_prefix, hbuf, sizeof(hbuf)))
			printf(",%s/%u", hbuf, match->rpm_matchlen);
		else
			printf(",?/%u", match->rpm_matchlen);
		/*(*/
		printf(")");

		n = match->rpm_len - 3;
		if (n % 4)
			goto trunc;
		n /= 4;
		while (n-- > 0) {
			use = (struct rr_pco_use *)cp;
			cp = (const char *)(use + 1);

			TCHECK(use->rpu_prefix);

			if (vflag > 1)
				printf("\n\t");
			else
				printf(" ");
			printf("use(");	/*)*/
			if (use->rpu_flags) {
#define F(x, y)	((use->rpu_flags) & (x) ? (y) : "")
				printf("%s%s,",
				    F(ICMP6_RR_PCOUSE_FLAGS_DECRVLTIME, "V"),
				    F(ICMP6_RR_PCOUSE_FLAGS_DECRPLTIME, "P"));
#undef F
			}
			if (vflag) {
				printf("mask=0x%x,", use->rpu_ramask);
				printf("raflags=0x%x,", use->rpu_raflags);
				if (~use->rpu_vltime == 0)
					printf("vltime=infty,");
				else
					printf("vltime=%u,",
					    EXTRACT_32BITS(&use->rpu_vltime));
				if (~use->rpu_pltime == 0)
					printf("pltime=infty,");
				else
					printf("pltime=%u,",
					    EXTRACT_32BITS(&use->rpu_pltime));
			}
			if (inet_ntop(AF_INET6, &use->rpu_prefix, hbuf,
			    sizeof(hbuf)))
				printf("%s/%u/%u", hbuf, use->rpu_uselen,
				    use->rpu_keeplen);
			else
				printf("?/%u/%u", use->rpu_uselen,
				    use->rpu_keeplen);
			/*(*/
			printf(")");
		}
	}

	return;

trunc:
	fputs("[|icmp6]", stdout);
}

#endif /* INET6 */
