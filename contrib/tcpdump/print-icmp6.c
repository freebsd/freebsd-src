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
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-icmp6.c,v 1.42 2000/12/13 07:57:05 itojun Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef INET6

#include <ctype.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>


#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <netdb.h>

#include "ip6.h"
#include "icmp6.h"

#include "interface.h"
#include "addrtoname.h"

#include "udp.h"
#include "ah.h"

void icmp6_opt_print(const u_char *, int);
void mld6_print(const u_char *);
static struct udphdr *get_upperlayer(u_char *, int *);
static void dnsname_print(const u_char *, const u_char *);
void icmp6_nodeinfo_print(int, const u_char *, const u_char *);
void icmp6_rrenum_print(int, const u_char *, const u_char *);

#ifndef abs
#define abs(a)	((0 < (a)) ? (a) : -(a))
#endif

void
icmp6_print(register const u_char *bp, register const u_char *bp2)
{
	const struct icmp6_hdr *dp;
	register const struct ip6_hdr *ip;
	register const char *str;
	register const struct ip6_hdr *oip;
	register const struct udphdr *ouh;
	register int dport;
	register const u_char *ep;
	char buf[256];
	int icmp6len, prot;

#if 0
#define TCHECK(var) if ((u_char *)&(var) > ep - sizeof(var)) goto trunc
#endif

	dp = (struct icmp6_hdr *)bp;
	ip = (struct ip6_hdr *)bp2;
	oip = (struct ip6_hdr *)(dp + 1);
	str = buf;
	/* 'ep' points to the end of available data. */
	ep = snapend;
	if (ip->ip6_plen)
		icmp6len = (ntohs(ip->ip6_plen) + sizeof(struct ip6_hdr) -
			    (bp - bp2));
	else			/* XXX: jumbo payload case... */
		icmp6len = snapend - bp;

#if 0
	(void)printf("%s > %s: ",
		ip6addr_string(&ip->ip6_src),
		ip6addr_string(&ip->ip6_dst));
#endif

	TCHECK(dp->icmp6_code);
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

			dport = ntohs(ouh->uh_dport);
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
		printf("icmp6: too big %u\n", (u_int32_t)ntohl(dp->icmp6_mtu));
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
			printf("icmp6: parameter problem errorneous - octet %u\n",
				(u_int32_t)ntohl(dp->icmp6_pptr));
			break;
		case ICMP6_PARAMPROB_NEXTHEADER:
			printf("icmp6: parameter problem next header - octet %u\n",
				(u_int32_t)ntohl(dp->icmp6_pptr));
			break;
		case ICMP6_PARAMPROB_OPTION:
			printf("icmp6: parameter problem option - octet %u\n",
				(u_int32_t)ntohl(dp->icmp6_pptr));
			break;
		default:
			printf("icmp6: parameter problem code-#%d",
			       dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_ECHO_REQUEST:
		printf("icmp6: echo request");
		break;
	case ICMP6_ECHO_REPLY:
		printf("icmp6: echo reply");
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
					icmp6len - RTSOLLEN);
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
#ifndef ND_RA_FLAG_HA
#define ND_RA_FLAG_HA	0x20
#endif
			if (p->nd_ra_flags_reserved & ND_RA_FLAG_HA)
				printf("H");
			if (p->nd_ra_flags_reserved != 0)
				printf(" ");
			printf("router_ltime=%d, ", ntohs(p->nd_ra_router_lifetime));
			printf("reachable_time=%u, ",
				(u_int32_t)ntohl(p->nd_ra_reachable));
			printf("retrans_time=%u)",
				(u_int32_t)ntohl(p->nd_ra_retransmit));
#define RTADVLEN 16
			icmp6_opt_print((const u_char *)dp + RTADVLEN,
					icmp6len - RTADVLEN);
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
					icmp6len - NDSOLLEN);
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
					icmp6len - NDADVLEN);
#undef NDADVLEN
		}
	    }
		break;
	case ND_REDIRECT:
#define RDR(i) ((struct nd_redirect *)(i))
		TCHECK(RDR(dp)->nd_rd_dst);
		printf("icmp6: redirect %s",
		    getname6((const u_char *)&RDR(dp)->nd_rd_dst));
		printf(" to %s",
		    getname6((const u_char*)&RDR(dp)->nd_rd_target));
#define REDIRECTLEN 40
		if (vflag) {
			icmp6_opt_print((const u_char *)dp + REDIRECTLEN,
					icmp6len - REDIRECTLEN);
		}
		break;
#undef REDIRECTLEN
#undef RDR
	case ICMP6_ROUTER_RENUMBERING:
		icmp6_rrenum_print(icmp6len, bp, ep);
		break;
	case ICMP6_NI_QUERY:
	case ICMP6_NI_REPLY:
		icmp6_nodeinfo_print(icmp6len, bp, ep);
		break;
	default:
		printf("icmp6: type-#%d", dp->icmp6_type);
		break;
	}
	return;
trunc:
	fputs("[|icmp6]", stdout);
#if 0
#undef TCHECK
#endif
}

static struct udphdr *
get_upperlayer(register u_char *bp, int *prot)
{
	register const u_char *ep;
	struct ip6_hdr *ip6 = (struct ip6_hdr *)bp;
	struct udphdr *uh;
	struct ip6_hbh *hbh;
	struct ip6_frag *fragh;
	struct ah *ah;
	int nh, hlen;

	/* 'ep' points to the end of available data. */
	ep = snapend;

	if (TTEST(ip6->ip6_nxt) == 0)
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
			if (TTEST(hbh->ip6h_len) == 0)
				return(NULL);
			nh = hbh->ip6h_nxt;
			hlen = (hbh->ip6h_len + 1) << 3;
			break;

		case IPPROTO_FRAGMENT: /* this should be odd, but try anyway */
			fragh = (struct ip6_frag *)bp;
			if (TTEST(fragh->ip6f_offlg) == 0)
				return(NULL);
			/* fragments with non-zero offset are meaningless */
			if ((fragh->ip6f_offlg & IP6F_OFF_MASK) != 0)
				return(NULL);
			nh = fragh->ip6f_nxt;
			hlen = sizeof(struct ip6_frag);
			break;

		case IPPROTO_AH:
			ah = (struct ah *)bp;
			if (TTEST(ah->ah_len) == 0)
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

void
icmp6_opt_print(register const u_char *bp, int resid)
{
	register const struct nd_opt_hdr *op;
	register const struct nd_opt_hdr *opl;	/* why there's no struct? */
	register const struct nd_opt_prefix_info *opp;
	register const struct icmp6_opts_redirect *opr;
	register const struct nd_opt_mtu *opm;
	register const struct nd_opt_advint *opa;
	register const u_char *ep;
	int	opts_len;
#if 0
	register const struct ip6_hdr *ip;
	register const char *str;
	register const struct ip6_hdr *oip;
	register const struct udphdr *ouh;
	register int hlen, dport;
	char buf[256];
#endif

#define ECHECK(var) if ((u_char *)&(var) > ep - sizeof(var)) return

	op = (struct nd_opt_hdr *)bp;
#if 0
	ip = (struct ip6_hdr *)bp2;
	oip = &dp->icmp6_ip6;
	str = buf;
#endif
	/* 'ep' points to the end of available data. */
	ep = snapend;

	ECHECK(op->nd_opt_len);
	if (resid <= 0)
		return;
	switch (op->nd_opt_type) {
	case ND_OPT_SOURCE_LINKADDR:
		opl = (struct nd_opt_hdr *)op;
#if 1
		if ((u_char *)opl + (opl->nd_opt_len << 3) > ep)
			goto trunc;
#else
		TCHECK((u_char *)opl + (opl->nd_opt_len << 3) - 1);
#endif
		printf("(src lladdr: %s",	/*)*/
			etheraddr_string((u_char *)(opl + 1)));
		if (opl->nd_opt_len != 1)
			printf("!");
		/*(*/
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_TARGET_LINKADDR:
		opl = (struct nd_opt_hdr *)op;
#if 1
		if ((u_char *)opl + (opl->nd_opt_len << 3) > ep)
			goto trunc;
#else
		TCHECK((u_char *)opl + (opl->nd_opt_len << 3) - 1);
#endif
		printf("(tgt lladdr: %s",	/*)*/
			etheraddr_string((u_char *)(opl + 1)));
		if (opl->nd_opt_len != 1)
			printf("!");
		/*(*/
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_PREFIX_INFORMATION:
		opp = (struct nd_opt_prefix_info *)op;
		TCHECK(opp->nd_opt_pi_prefix);
		printf("(prefix info: ");	/*)*/
		if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK)
			printf("L");
		if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO)
			printf("A");
		if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ROUTER)
			printf("R");
		if (opp->nd_opt_pi_flags_reserved)
			printf(" ");
		printf("valid_ltime=");
		if ((u_int32_t)ntohl(opp->nd_opt_pi_valid_time) == ~0U)
			printf("infinity");
		else {
			printf("%u", (u_int32_t)ntohl(opp->nd_opt_pi_valid_time));
		}
		printf(", ");
		printf("preffered_ltime=");
		if ((u_int32_t)ntohl(opp->nd_opt_pi_preferred_time) == ~0U)
			printf("infinity");
		else {
			printf("%u", (u_int32_t)ntohl(opp->nd_opt_pi_preferred_time));
		}
		printf(", ");
		printf("prefix=%s/%d", ip6addr_string(&opp->nd_opt_pi_prefix),
			opp->nd_opt_pi_prefix_len);
		if (opp->nd_opt_pi_len != 4)
			printf("!");
		/*(*/
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_REDIRECTED_HEADER:
		opr = (struct icmp6_opts_redirect *)op;
		printf("(redirect)");
		/* xxx */
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_MTU:
		opm = (struct nd_opt_mtu *)op;
		TCHECK(opm->nd_opt_mtu_mtu);
		printf("(mtu: ");	/*)*/
		printf("mtu=%u", (u_int32_t)ntohl(opm->nd_opt_mtu_mtu));
		if (opm->nd_opt_mtu_len != 1)
			printf("!");
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
        case ND_OPT_ADVINT:
		opa = (struct nd_opt_advint *)op;
		TCHECK(opa->nd_opt_advint_advint);
		printf("(advint: ");	/*)*/
		printf("advint=%u",
		    (u_int32_t)ntohl(opa->nd_opt_advint_advint));
		/*(*/
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;                
	default:
		opts_len = op->nd_opt_len;
		printf("(unknwon opt_type=%d, opt_len=%d)",
		       op->nd_opt_type, opts_len);
		if (opts_len == 0)
			opts_len = 1; /* XXX */
		icmp6_opt_print((const u_char *)op + (opts_len << 3),
				resid - (opts_len << 3));
		break;
	}
	return;
 trunc:
	fputs("[ndp opt]", stdout);
	return;
#undef ECHECK
}

void
mld6_print(register const u_char *bp)
{
	register struct mld6_hdr *mp = (struct mld6_hdr *)bp;
	register const u_char *ep;

	/* 'ep' points to the end of available data. */
	ep = snapend;

	if ((u_char *)mp + sizeof(*mp) > ep)
		return;

	printf("max resp delay: %d ", ntohs(mp->mld6_maxdelay));
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

void
icmp6_nodeinfo_print(int icmp6len, const u_char *bp, const u_char *ep)
{
	struct icmp6_nodeinfo *ni6;
	struct icmp6_hdr *dp;
	const u_char *cp;
	int siz, i;
	int needcomma;

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
		switch (ntohs(ni6->ni_qtype)) {
		case NI_QTYPE_NOOP:
			printf("noop");
			break;
		case NI_QTYPE_SUPTYPES:
			printf("supported qtypes");
			i = ntohs(ni6->ni_flags);
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

		switch (ntohs(ni6->ni_qtype)) {
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
			i = ntohs(ni6->ni_flags);
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
			if ((ntohs(ni6->ni_flags) & 0x01) != 0)
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
				printf("(%d)", ntohl(*(int32_t *)(bp + i)));
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

void
icmp6_rrenum_print(int icmp6len, const u_char *bp, const u_char *ep)
{
	struct icmp6_router_renum *rr6;
	struct icmp6_hdr *dp;
	size_t siz;
	const char *cp;
	struct rr_pco_match *match;
	struct rr_pco_use *use;
	char hbuf[NI_MAXHOST];
	int n;

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

	printf(", seq=%u", (u_int32_t)ntohl(rr6->rr_seqnum));

	if (vflag) {
#define F(x, y)	((rr6->rr_flags) & (x) ? (y) : "")
		printf("[");	/*]*/
		if (rr6->rr_flags) {
			printf("%s%s%s%s%s,", F(ICMP6_RR_FLAGS_TEST, "T"),
			    F(ICMP6_RR_FLAGS_REQRESULT, "R"),
			    F(ICMP6_RR_FLAGS_ALLIF, "A"),
			    F(ICMP6_RR_FLAGS_SPECSITE, "S"),
			    F(ICMP6_RR_FLAGS_PREVDONE, "P"));
		}
		printf("seg=%u,", rr6->rr_segnum);
		printf("maxdelay=%u", rr6->rr_maxdelay);
		if (rr6->rr_reserved)
			printf("rsvd=0x%x", (u_int16_t)ntohs(rr6->rr_reserved));
		/*[*/
		printf("]");
#undef F
	}

	if (rr6->rr_code == ICMP6_ROUTER_RENUMBERING_COMMAND) {
		match = (struct rr_pco_match *)cp;
		cp = (const char *)(match + 1);

		TCHECK(match->rpm_prefix);

		if (vflag)
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

			if (vflag)
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
					    (u_int32_t)ntohl(use->rpu_vltime));
				if (~use->rpu_pltime == 0)
					printf("pltime=infty,");
				else
					printf("pltime=%u,",
					    (u_int32_t)ntohl(use->rpu_pltime));
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
