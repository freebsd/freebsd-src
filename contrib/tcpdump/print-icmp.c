/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994, 1995, 1996
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
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-icmp.c,v 1.62.4.1 2002/06/01 23:51:13 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <netdb.h>		/* for MAXHOSTNAMELEN on some platforms */

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#include "ip.h"
#include "udp.h"

/*
 * Interface Control Message Protocol Definitions.
 * Per RFC 792, September 1981.
 */

/*
 * Structure of an icmp header.
 */
struct icmp {
	u_int8_t  icmp_type;		/* type of message, see below */
	u_int8_t  icmp_code;		/* type sub code */
	u_int16_t icmp_cksum;		/* ones complement cksum of struct */
	union {
		u_int8_t ih_pptr;			/* ICMP_PARAMPROB */
		struct in_addr ih_gwaddr;	/* ICMP_REDIRECT */
		struct ih_idseq {
			u_int16_t icd_id;
			u_int16_t icd_seq;
		} ih_idseq;
		u_int32_t ih_void;

		/* ICMP_UNREACH_NEEDFRAG -- Path MTU Discovery (RFC1191) */
		struct ih_pmtu {
			u_int16_t ipm_void;    
			u_int16_t ipm_nextmtu;
		} ih_pmtu;
	} icmp_hun;
#define	icmp_pptr	icmp_hun.ih_pptr
#define	icmp_gwaddr	icmp_hun.ih_gwaddr
#define	icmp_id		icmp_hun.ih_idseq.icd_id
#define	icmp_seq	icmp_hun.ih_idseq.icd_seq
#define	icmp_void	icmp_hun.ih_void
#define	icmp_pmvoid	icmp_hun.ih_pmtu.ipm_void
#define	icmp_nextmtu	icmp_hun.ih_pmtu.ipm_nextmtu
	union {
		struct id_ts {
			u_int32_t its_otime;
			u_int32_t its_rtime;
			u_int32_t its_ttime;
		} id_ts;
		struct id_ip  {
			struct ip idi_ip;
			/* options and then 64 bits of data */
		} id_ip;
		u_int32_t id_mask;
		u_int8_t id_data[1];
	} icmp_dun;
#define	icmp_otime	icmp_dun.id_ts.its_otime
#define	icmp_rtime	icmp_dun.id_ts.its_rtime
#define	icmp_ttime	icmp_dun.id_ts.its_ttime
#define	icmp_ip		icmp_dun.id_ip.idi_ip
#define	icmp_mask	icmp_dun.id_mask
#define	icmp_data	icmp_dun.id_data
};

/*
 * Lower bounds on packet lengths for various types.
 * For the error advice packets must first insure that the
 * packet is large enought to contain the returned ip header.
 * Only then can we do the check to see if 64 bits of packet
 * data have been returned, since we need to check the returned
 * ip header length.
 */
#define	ICMP_MINLEN	8				/* abs minimum */
#define	ICMP_TSLEN	(8 + 3 * sizeof (u_int32_t))	/* timestamp */
#define	ICMP_MASKLEN	12				/* address mask */
#define	ICMP_ADVLENMIN	(8 + sizeof (struct ip) + 8)	/* min */
#define	ICMP_ADVLEN(p)	(8 + (IP_HL(&(p)->icmp_ip) << 2) + 8)
	/* N.B.: must separately check that ip_hl >= 5 */

/*
 * Definition of type and code field values.
 */
#define	ICMP_ECHOREPLY		0		/* echo reply */
#define	ICMP_UNREACH		3		/* dest unreachable, codes: */
#define		ICMP_UNREACH_NET	0		/* bad net */
#define		ICMP_UNREACH_HOST	1		/* bad host */
#define		ICMP_UNREACH_PROTOCOL	2		/* bad protocol */
#define		ICMP_UNREACH_PORT	3		/* bad port */
#define		ICMP_UNREACH_NEEDFRAG	4		/* IP_DF caused drop */
#define		ICMP_UNREACH_SRCFAIL	5		/* src route failed */
#define		ICMP_UNREACH_NET_UNKNOWN 6		/* unknown net */
#define		ICMP_UNREACH_HOST_UNKNOWN 7		/* unknown host */
#define		ICMP_UNREACH_ISOLATED	8		/* src host isolated */
#define		ICMP_UNREACH_NET_PROHIB	9		/* prohibited access */
#define		ICMP_UNREACH_HOST_PROHIB 10		/* ditto */
#define		ICMP_UNREACH_TOSNET	11		/* bad tos for net */
#define		ICMP_UNREACH_TOSHOST	12		/* bad tos for host */
#define	ICMP_SOURCEQUENCH	4		/* packet lost, slow down */
#define	ICMP_REDIRECT		5		/* shorter route, codes: */
#define		ICMP_REDIRECT_NET	0		/* for network */
#define		ICMP_REDIRECT_HOST	1		/* for host */
#define		ICMP_REDIRECT_TOSNET	2		/* for tos and net */
#define		ICMP_REDIRECT_TOSHOST	3		/* for tos and host */
#define	ICMP_ECHO		8		/* echo service */
#define	ICMP_ROUTERADVERT	9		/* router advertisement */
#define	ICMP_ROUTERSOLICIT	10		/* router solicitation */
#define	ICMP_TIMXCEED		11		/* time exceeded, code: */
#define		ICMP_TIMXCEED_INTRANS	0		/* ttl==0 in transit */
#define		ICMP_TIMXCEED_REASS	1		/* ttl==0 in reass */
#define	ICMP_PARAMPROB		12		/* ip header bad */
#define		ICMP_PARAMPROB_OPTABSENT 1		/* req. opt. absent */
#define	ICMP_TSTAMP		13		/* timestamp request */
#define	ICMP_TSTAMPREPLY	14		/* timestamp reply */
#define	ICMP_IREQ		15		/* information request */
#define	ICMP_IREQREPLY		16		/* information reply */
#define	ICMP_MASKREQ		17		/* address mask request */
#define	ICMP_MASKREPLY		18		/* address mask reply */

#define	ICMP_MAXTYPE		18

#define	ICMP_INFOTYPE(type) \
	((type) == ICMP_ECHOREPLY || (type) == ICMP_ECHO || \
	(type) == ICMP_ROUTERADVERT || (type) == ICMP_ROUTERSOLICIT || \
	(type) == ICMP_TSTAMP || (type) == ICMP_TSTAMPREPLY || \
	(type) == ICMP_IREQ || (type) == ICMP_IREQREPLY || \
	(type) == ICMP_MASKREQ || (type) == ICMP_MASKREPLY)
/* rfc1700 */
#ifndef ICMP_UNREACH_NET_UNKNOWN
#define ICMP_UNREACH_NET_UNKNOWN	6	/* destination net unknown */
#endif
#ifndef ICMP_UNREACH_HOST_UNKNOWN
#define ICMP_UNREACH_HOST_UNKNOWN	7	/* destination host unknown */
#endif
#ifndef ICMP_UNREACH_ISOLATED
#define ICMP_UNREACH_ISOLATED		8	/* source host isolated */
#endif
#ifndef ICMP_UNREACH_NET_PROHIB
#define ICMP_UNREACH_NET_PROHIB		9	/* admin prohibited net */
#endif
#ifndef ICMP_UNREACH_HOST_PROHIB
#define ICMP_UNREACH_HOST_PROHIB	10	/* admin prohibited host */
#endif
#ifndef ICMP_UNREACH_TOSNET
#define ICMP_UNREACH_TOSNET		11	/* tos prohibited net */
#endif
#ifndef ICMP_UNREACH_TOSHOST
#define ICMP_UNREACH_TOSHOST		12	/* tos prohibited host */
#endif

/* rfc1716 */
#ifndef ICMP_UNREACH_FILTER_PROHIB
#define ICMP_UNREACH_FILTER_PROHIB	13	/* admin prohibited filter */
#endif
#ifndef ICMP_UNREACH_HOST_PRECEDENCE
#define ICMP_UNREACH_HOST_PRECEDENCE	14	/* host precedence violation */
#endif
#ifndef ICMP_UNREACH_PRECEDENCE_CUTOFF
#define ICMP_UNREACH_PRECEDENCE_CUTOFF	15	/* precedence cutoff */
#endif

/* Most of the icmp types */
static struct tok icmp2str[] = {
	{ ICMP_ECHOREPLY,		"echo reply" },
	{ ICMP_SOURCEQUENCH,		"source quench" },
	{ ICMP_ECHO,			"echo request" },
	{ ICMP_ROUTERSOLICIT,		"router solicitation" },
	{ ICMP_TSTAMP,			"time stamp request" },
	{ ICMP_TSTAMPREPLY,		"time stamp reply" },
	{ ICMP_IREQ,			"information request" },
	{ ICMP_IREQREPLY,		"information reply" },
	{ ICMP_MASKREQ,			"address mask request" },
	{ 0,				NULL }
};

/* Formats for most of the ICMP_UNREACH codes */
static struct tok unreach2str[] = {
	{ ICMP_UNREACH_NET,		"net %s unreachable" },
	{ ICMP_UNREACH_HOST,		"host %s unreachable" },
	{ ICMP_UNREACH_SRCFAIL,
	    "%s unreachable - source route failed" },
	{ ICMP_UNREACH_NET_UNKNOWN,	"net %s unreachable - unknown" },
	{ ICMP_UNREACH_HOST_UNKNOWN,	"host %s unreachable - unknown" },
	{ ICMP_UNREACH_ISOLATED,
	    "%s unreachable - source host isolated" },
	{ ICMP_UNREACH_NET_PROHIB,
	    "net %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_HOST_PROHIB,
	    "host %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_TOSNET,
	    "net %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_TOSHOST,
	    "host %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_FILTER_PROHIB,
	    "host %s unreachable - admin prohibited filter" },
	{ ICMP_UNREACH_HOST_PRECEDENCE,
	    "host %s unreachable - host precedence violation" },
	{ ICMP_UNREACH_PRECEDENCE_CUTOFF,
	    "host %s unreachable - precedence cutoff" },
	{ 0,				NULL }
};

/* Formats for the ICMP_REDIRECT codes */
static struct tok type2str[] = {
	{ ICMP_REDIRECT_NET,		"redirect %s to net %s" },
	{ ICMP_REDIRECT_HOST,		"redirect %s to host %s" },
	{ ICMP_REDIRECT_TOSNET,		"redirect-tos %s to net %s" },
	{ ICMP_REDIRECT_TOSHOST,	"redirect-tos %s to host %s" },
	{ 0,				NULL }
};

/* rfc1191 */
struct mtu_discovery {
	u_int16_t unused;
	u_int16_t nexthopmtu;
};

/* rfc1256 */
struct ih_rdiscovery {
	u_int8_t ird_addrnum;
	u_int8_t ird_addrsiz;
	u_int16_t ird_lifetime;
};

struct id_rdiscovery {
	u_int32_t ird_addr;
	u_int32_t ird_pref;
};

void
icmp_print(const u_char *bp, u_int plen, const u_char *bp2)
{
	char *cp;
	const struct icmp *dp;
	const struct ip *ip;
	const char *str, *fmt;
	const struct ip *oip;
	const struct udphdr *ouh;
	u_int hlen, dport, mtu;
	char buf[MAXHOSTNAMELEN + 100];

	dp = (struct icmp *)bp;
	ip = (struct ip *)bp2;
	str = buf;

	TCHECK(dp->icmp_code);
	switch (dp->icmp_type) {

	case ICMP_UNREACH:
		TCHECK(dp->icmp_ip.ip_dst);
		switch (dp->icmp_code) {

		case ICMP_UNREACH_PROTOCOL:
			TCHECK(dp->icmp_ip.ip_p);
			(void)snprintf(buf, sizeof(buf),
			    "%s protocol %d unreachable",
			    ipaddr_string(&dp->icmp_ip.ip_dst),
			    dp->icmp_ip.ip_p);
			break;

		case ICMP_UNREACH_PORT:
			TCHECK(dp->icmp_ip.ip_p);
			oip = &dp->icmp_ip;
			hlen = IP_HL(oip) * 4;
			ouh = (struct udphdr *)(((u_char *)oip) + hlen);
			dport = ntohs(ouh->uh_dport);
			switch (oip->ip_p) {

			case IPPROTO_TCP:
				(void)snprintf(buf, sizeof(buf),
					"%s tcp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					tcpport_string(dport));
				break;

			case IPPROTO_UDP:
				(void)snprintf(buf, sizeof(buf),
					"%s udp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					udpport_string(dport));
				break;

			default:
				(void)snprintf(buf, sizeof(buf),
					"%s protocol %d port %d unreachable",
					ipaddr_string(&oip->ip_dst),
					oip->ip_p, dport);
				break;
			}
			break;

		case ICMP_UNREACH_NEEDFRAG:
		    {
			register const struct mtu_discovery *mp;
			mp = (struct mtu_discovery *)&dp->icmp_void;
			mtu = EXTRACT_16BITS(&mp->nexthopmtu);
			if (mtu) {
				(void)snprintf(buf, sizeof(buf),
				    "%s unreachable - need to frag (mtu %d)",
				    ipaddr_string(&dp->icmp_ip.ip_dst), mtu);
			} else {
				(void)snprintf(buf, sizeof(buf),
				    "%s unreachable - need to frag",
				    ipaddr_string(&dp->icmp_ip.ip_dst));
			}
		    }
			break;

		default:
			fmt = tok2str(unreach2str, "#%d %%s unreachable",
			    dp->icmp_code);
			(void)snprintf(buf, sizeof(buf), fmt,
			    ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		}
		break;

	case ICMP_REDIRECT:
		TCHECK(dp->icmp_ip.ip_dst);
		fmt = tok2str(type2str, "redirect-#%d %%s to net %%s",
		    dp->icmp_code);
		(void)snprintf(buf, sizeof(buf), fmt,
		    ipaddr_string(&dp->icmp_ip.ip_dst),
		    ipaddr_string(&dp->icmp_gwaddr));
		break;

	case ICMP_ROUTERADVERT:
	    {
		register const struct ih_rdiscovery *ihp;
		register const struct id_rdiscovery *idp;
		u_int lifetime, num, size;

		(void)snprintf(buf, sizeof(buf), "router advertisement");
		cp = buf + strlen(buf);

		ihp = (struct ih_rdiscovery *)&dp->icmp_void;
		TCHECK(*ihp);
		(void)strncpy(cp, " lifetime ", sizeof(buf) - (cp - buf));
		cp = buf + strlen(buf);
		lifetime = EXTRACT_16BITS(&ihp->ird_lifetime);
		if (lifetime < 60) {
			(void)snprintf(cp, sizeof(buf) - (cp - buf), "%u",
			    lifetime);
		} else if (lifetime < 60 * 60) {
			(void)snprintf(cp, sizeof(buf) - (cp - buf), "%u:%02u",
			    lifetime / 60, lifetime % 60);
		} else {
			(void)snprintf(cp, sizeof(buf) - (cp - buf),
			    "%u:%02u:%02u",
			    lifetime / 3600,
			    (lifetime % 3600) / 60,
			    lifetime % 60);
		}
		cp = buf + strlen(buf);

		num = ihp->ird_addrnum;
		(void)snprintf(cp, sizeof(buf) - (cp - buf), " %d:", num);
		cp = buf + strlen(buf);

		size = ihp->ird_addrsiz;
		if (size != 2) {
			(void)snprintf(cp, sizeof(buf) - (cp - buf),
			    " [size %d]", size);
			break;
		}
		idp = (struct id_rdiscovery *)&dp->icmp_data;
		while (num-- > 0) {
			TCHECK(*idp);
			(void)snprintf(cp, sizeof(buf) - (cp - buf), " {%s %u}",
			    ipaddr_string(&idp->ird_addr),
			    EXTRACT_32BITS(&idp->ird_pref));
			cp = buf + strlen(buf);
			++idp;
		}
	    }
		break;

	case ICMP_TIMXCEED:
		TCHECK(dp->icmp_ip.ip_dst);
		switch (dp->icmp_code) {

		case ICMP_TIMXCEED_INTRANS:
			str = "time exceeded in-transit";
			break;

		case ICMP_TIMXCEED_REASS:
			str = "ip reassembly time exceeded";
			break;

		default:
			(void)snprintf(buf, sizeof(buf), "time exceeded-#%d",
			    dp->icmp_code);
			break;
		}
		break;

	case ICMP_PARAMPROB:
		if (dp->icmp_code)
			(void)snprintf(buf, sizeof(buf),
			    "parameter problem - code %d", dp->icmp_code);
		else {
			TCHECK(dp->icmp_pptr);
			(void)snprintf(buf, sizeof(buf),
			    "parameter problem - octet %d", dp->icmp_pptr);
		}
		break;

	case ICMP_MASKREPLY:
		TCHECK(dp->icmp_mask);
		(void)snprintf(buf, sizeof(buf), "address mask is 0x%08x",
		    (unsigned)ntohl(dp->icmp_mask));
		break;

	case ICMP_TSTAMP:
		TCHECK(dp->icmp_seq);
		(void)snprintf(buf, sizeof(buf),
		    "time stamp query id %u seq %u",
		    (unsigned)ntohs(dp->icmp_id),
		    (unsigned)ntohs(dp->icmp_seq));
		break;

	case ICMP_TSTAMPREPLY:
		TCHECK(dp->icmp_ttime);
		(void)snprintf(buf, sizeof(buf),
		    "time stamp reply id %u seq %u : org 0x%lx recv 0x%lx xmit 0x%lx",
		    (unsigned)ntohs(dp->icmp_id),
		    (unsigned)ntohs(dp->icmp_seq),
		    (unsigned long)ntohl(dp->icmp_otime),
		    (unsigned long)ntohl(dp->icmp_rtime),
		    (unsigned long)ntohl(dp->icmp_ttime));
		break;

	default:
		str = tok2str(icmp2str, "type-#%d", dp->icmp_type);
		break;
	}
	(void)printf("icmp: %s", str);
	if (vflag) {
		if (TTEST2(*bp, plen)) {
			if (in_cksum((u_short*)dp, plen, 0))
				printf(" (wrong icmp csum)");
		}
	}
 	if (vflag > 1 && !ICMP_INFOTYPE(dp->icmp_type)) {
 		bp += 8;
 		(void)printf(" for ");
 		ip = (struct ip *)bp;
 		snaplen = snapend - bp;
 		ip_print(bp, ntohs(ip->ip_len));
 	}
	return;
trunc:
	fputs("[|icmp]", stdout);
}
