/*
 * Copyright (C) 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef lint
static const char rcsid[] =
     "@(#) $Header: /tcpdump/master/tcpdump/print-bgp.c,v 1.9.2.2 2000/01/25 18:32:53 itojun Exp $";
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"

struct bgp {
	u_int8_t bgp_marker[16];
	u_int16_t bgp_len;
	u_int8_t bgp_type;
};
#define BGP_SIZE		19	/* unaligned */

#define BGP_OPEN		1
#define BGP_UPDATE		2
#define BGP_NOTIFICATION	3
#define BGP_KEEPALIVE		4

struct bgp_open {
	u_int8_t bgpo_marker[16];
	u_int16_t bgpo_len;
	u_int8_t bgpo_type;
	u_int8_t bgpo_version;
	u_int16_t bgpo_myas;
	u_int16_t bgpo_holdtime;
	u_int32_t bgpo_id;
	u_int8_t bgpo_optlen;
	/* options should follow */
};

struct bgp_opt {
	u_int8_t bgpopt_type;
	u_int8_t bgpopt_len;
	/* variable length */
};

struct bgp_notification {
	u_int8_t bgpn_marker[16];
	u_int16_t bgpn_len;
	u_int8_t bgpn_type;
	u_int8_t bgpn_major;
	u_int8_t bgpn_minor;
	/* data should follow */
};

struct bgp_attr {
	u_int8_t bgpa_flags;
	u_int8_t bgpa_type;
	union {
		u_int8_t len;
		u_int16_t elen;
	} bgpa_len;
#define bgp_attr_len(p) \
	(((p)->bgpa_flags & 0x10) ? \
		ntohs((p)->bgpa_len.elen) : (p)->bgpa_len.len)
#define bgp_attr_off(p) \
	(((p)->bgpa_flags & 0x10) ? 4 : 3)
};

#define BGPTYPE_ORIGIN			1
#define BGPTYPE_AS_PATH			2
#define BGPTYPE_NEXT_HOP		3
#define BGPTYPE_MULTI_EXIT_DISC		4
#define BGPTYPE_LOCAL_PREF		5
#define BGPTYPE_ATOMIC_AGGREGATE	6
#define BGPTYPE_AGGREGATOR		7
#define	BGPTYPE_COMMUNITIES		8	/* RFC1997 */
#define	BGPTYPE_ORIGINATOR_ID		9	/* RFC1998 */
#define	BGPTYPE_CLUSTER_LIST		10	/* RFC1998 */
#define	BGPTYPE_DPA			11	/* work in progress */
#define	BGPTYPE_ADVERTISERS		12	/* RFC1863 */
#define	BGPTYPE_RCID_PATH		13	/* RFC1863 */
#define BGPTYPE_MP_REACH_NLRI		14	/* RFC2283 */
#define BGPTYPE_MP_UNREACH_NLRI		15	/* RFC2283 */


static const char *bgptype[] = {
	NULL, "OPEN", "UPDATE", "NOTIFICATION", "KEEPALIVE",
};
#define bgp_type(x) num_or_str(bgptype, sizeof(bgptype)/sizeof(bgptype[0]), (x))

static const char *bgpopt_type[] = {
	NULL, "Authentication Information",
};
#define bgp_opttype(x) \
	num_or_str(bgpopt_type, sizeof(bgpopt_type)/sizeof(bgpopt_type[0]), (x))

static const char *bgpnotify_major[] = {
	NULL, "Message Header Error",
	"OPEN Message Error", "UPDATE Message Error",
	"Hold Timer Expired", "Finite State Machine Error",
	"Cease",
};
#define bgp_notify_major(x) \
	num_or_str(bgpnotify_major, \
		sizeof(bgpnotify_major)/sizeof(bgpnotify_major[0]), (x))

static const char *bgpnotify_minor_1[] = {
	NULL, "Connection Not Synchronized",
	"Bad Message Length", "Bad Message Type",
};

static const char *bgpnotify_minor_2[] = {
	NULL, "Unsupported Version Number",
	"Bad Peer AS", "Bad BGP Identifier",
	"Unsupported Optional Parameter", "Authentication Failure",
	"Unacceptable Hold Time",
};

static const char *bgpnotify_minor_3[] = {
	NULL, "Malformed Attribute List",
	"Unrecognized Well-known Attribute", "Missing Well-known Attribute",
	"Attribute Flags Error", "Attribute Length Error",
	"Invalid ORIGIN Attribute", "AS Routing Loop",
	"Invalid NEXT_HOP Attribute", "Optional Attribute Error",
	"Invalid Network Field", "Malformed AS_PATH",
};

static const char **bgpnotify_minor[] = {
	NULL, bgpnotify_minor_1, bgpnotify_minor_2, bgpnotify_minor_3,
};
static const int bgpnotify_minor_siz[] = {
	0, sizeof(bgpnotify_minor_1)/sizeof(bgpnotify_minor_1[0]),
	sizeof(bgpnotify_minor_2)/sizeof(bgpnotify_minor_2[0]),
	sizeof(bgpnotify_minor_3)/sizeof(bgpnotify_minor_3[0]),
};

static const char *bgpattr_origin[] = {
	"IGP", "EGP", "INCOMPLETE",
};
#define bgp_attr_origin(x) \
	num_or_str(bgpattr_origin, \
		sizeof(bgpattr_origin)/sizeof(bgpattr_origin[0]), (x))

static const char *bgpattr_type[] = {
	NULL, "ORIGIN", "AS_PATH", "NEXT_HOP",
	"MULTI_EXIT_DISC", "LOCAL_PREF", "ATOMIC_AGGREGATE", "AGGREGATOR",
	"COMMUNITIES", "ORIGINATOR_ID", "CLUSTER_LIST", "DPA",
	"ADVERTISERS", "RCID_PATH", "MP_REACH_NLRI", "MP_UNREACH_NLRI",
};
#define bgp_attr_type(x) \
	num_or_str(bgpattr_type, \
		sizeof(bgpattr_type)/sizeof(bgpattr_type[0]), (x))

/* Subsequent address family identifier, RFC2283 section 7 */
static const char *bgpattr_nlri_safi[] = {
    "Reserved", "Unicast", "Multicast", "Unicast+Multicast",
};
#define bgp_attr_nlri_safi(x) \
	num_or_str(bgpattr_nlri_safi, \
		sizeof(bgpattr_nlri_safi)/sizeof(bgpattr_nlri_safi[0]), (x))

/* well-known community */
#define BGP_COMMUNITY_NO_EXPORT			0xffffff01
#define BGP_COMMUNITY_NO_ADVERT			0xffffff02
#define BGP_COMMUNITY_NO_EXPORT_SUBCONFED	0xffffff03

/* RFC1700 address family numbers */
#define AFNUM_INET	1
#define AFNUM_INET6	2
#define AFNUM_NSAP	3
#define AFNUM_HDLC	4
#define AFNUM_BBN1822	5
#define AFNUM_802	6
#define AFNUM_E163	7
#define AFNUM_E164	8
#define AFNUM_F69	9
#define AFNUM_X121	10
#define AFNUM_IPX	11
#define AFNUM_ATALK	12
#define AFNUM_DECNET	13
#define AFNUM_BANYAN	14
#define AFNUM_E164NSAP	15

static const char *afnumber[] = {
	"Reserved", "IPv4", "IPv6", "NSAP", "HDLC",
	"BBN 1822", "802", "E.163", "E.164", "F.69",
	"X.121", "IPX", "Appletalk", "Decnet IV", "Banyan Vines",
	"E.164 with NSAP subaddress",
};
#define af_name(x) \
	(((x) == 65535) ? afnumber[0] : \
		num_or_str(afnumber, \
			sizeof(afnumber)/sizeof(afnumber[0]), (x)))


static const char *
num_or_str(const char **table, size_t siz, int value)
{
	static char buf[20];
	if (value < 0 || siz <= value || table[value] == NULL) {
		snprintf(buf, sizeof(buf), "#%d", value);
		return buf;
	} else
		return table[value];
}

static const char *
bgp_notify_minor(int major, int minor)
{
	static const char **table;
	int siz;
	static char buf[20];
	const char *p;

	if (0 <= major
	 && major < sizeof(bgpnotify_minor)/sizeof(bgpnotify_minor[0])
	 && bgpnotify_minor[major]) {
		table = bgpnotify_minor[major];
		siz = bgpnotify_minor_siz[major];
		if (0 <= minor && minor < siz && table[minor])
			p = table[minor];
		else
			p = NULL;
	} else
		p = NULL;
	if (p == NULL) {
		snprintf(buf, sizeof(buf), "#%d", minor);
		return buf;
	} else
		return p;
}

static int
decode_prefix4(const u_char *pd, char *buf, int buflen)
{
	struct in_addr addr;
	int plen;

	plen = pd[0];
	if (plen < 0 || 32 < plen)
		return -1;

	memset(&addr, 0, sizeof(addr));
	memcpy(&addr, &pd[1], (plen + 7) / 8);
	if (plen % 8) {
		((u_char *)&addr)[(plen + 7) / 8 - 1] &=
			((0xff00 >> (plen % 8)) & 0xff);
	}
	snprintf(buf, buflen, "%s/%d", getname((char *)&addr), plen);
	return 1 + (plen + 7) / 8;
}

#ifdef INET6
static int
decode_prefix6(const u_char *pd, char *buf, int buflen)
{
	struct in6_addr addr;
	int plen;

	plen = pd[0];
	if (plen < 0 || 128 < plen)
		return -1;

	memset(&addr, 0, sizeof(addr));
	memcpy(&addr, &pd[1], (plen + 7) / 8);
	if (plen % 8) {
		addr.s6_addr[(plen + 7) / 8 - 1] &=
			((0xff00 >> (plen % 8)) & 0xff);
	}
	snprintf(buf, buflen, "%s/%d", getname6((char *)&addr), plen);
	return 1 + (plen + 7) / 8;
}
#endif

static void
bgp_attr_print(const struct bgp_attr *attr, const u_char *dat, int len)
{
	int i;
	u_int16_t af;
	u_int8_t safi, snpa;
	int advance;
	int tlen;
	const u_char *p;
	char buf[MAXHOSTNAMELEN + 100];

	p = dat;

	switch (attr->bgpa_type) {
	case BGPTYPE_ORIGIN:
		if (len != 1)
			printf(" invalid len");
		else
			printf(" %s", bgp_attr_origin(p[0]));
		break;
	case BGPTYPE_AS_PATH:
		if (len % 2) {
			printf(" invalid len");
			break;
		}
		while (p < dat + len) {
			/*
			 * under RFC1965, p[0] means:
			 * 1: AS_SET 2: AS_SEQUENCE
			 * 3: AS_CONFED_SET 4: AS_CONFED_SEQUENCE
			 */
			printf(" ");
			if (p[0] == 3 || p[0] == 4)
				printf("confed");
			printf("%s", (p[0] & 1) ? "{" : "");
			for (i = 0; i < p[1]; i += 2) {
				printf("%s%u", i == 0 ? "" : " ",
					ntohs(*(u_int16_t *)&p[2 + i]));
			}
			printf("%s", (p[0] & 1) ? "}" : "");
			p += 2 + p[1] * 2;
		}
		break;
	case BGPTYPE_NEXT_HOP:
		if (len != 4)
			printf(" invalid len");
		else
			printf(" %s", getname(p));
		break;
	case BGPTYPE_MULTI_EXIT_DISC:
	case BGPTYPE_LOCAL_PREF:
		if (len != 4)
			printf(" invalid len");
		else
			printf(" %u", (u_int32_t)ntohl(*(u_int32_t *)p));
		break;
	case BGPTYPE_ATOMIC_AGGREGATE:
		if (len != 0)
			printf(" invalid len");
		break;
	case BGPTYPE_AGGREGATOR:
		if (len != 6) {
			printf(" invalid len");
			break;
		}
		printf(" AS #%u, origin %s", ntohs(*(u_int16_t *)p),
			getname(p + 2));
		break;
	case BGPTYPE_COMMUNITIES:
		if (len % 4) {
			printf(" invalid len");
			break;
		}
		for (i = 0; i < len; i += 4) {
			u_int32_t comm;
			comm = (u_int32_t)ntohl(*(u_int32_t *)&p[i]);
			switch (comm) {
			case BGP_COMMUNITY_NO_EXPORT:
				printf(" NO_EXPORT");
				break;
			case BGP_COMMUNITY_NO_ADVERT:
				printf(" NO_ADVERTISE");
				break;
			case BGP_COMMUNITY_NO_EXPORT_SUBCONFED:
				printf(" NO_EXPORT_SUBCONFED");
				break;
			default:
				printf(" (AS #%d value 0x%04x)",
					(comm >> 16) & 0xffff, comm & 0xffff);
				break;
			}
		}
		break;
	case BGPTYPE_MP_REACH_NLRI:
		af = ntohs(*(u_int16_t *)p);
		safi = p[2];
		if (safi >= 128)
			printf(" %s vendor specific,", af_name(af));
		else {
			printf(" %s %s,", af_name(af),
				bgp_attr_nlri_safi(safi));
		}
		p += 3;

		if (af == AFNUM_INET)
			;
#ifdef INET6
		else if (af == AFNUM_INET6)
			;
#endif
		else
			break;

		tlen = p[0];
		if (tlen) {
			printf(" nexthop");
			if (af == AFNUM_INET)
				advance = 4;
#ifdef INET6
			else if (af == AFNUM_INET6)
				advance = 16;
#endif

			for (i = 0; i < tlen; i += advance) {
				if (af == AFNUM_INET)
					printf(" %s", getname(p + 1 + i));
#ifdef INET6
				else if (af == AFNUM_INET6)
					printf(" %s", getname6(p + 1 + i));
#endif
			}
			printf(",");
		}
		p += 1 + tlen;

		snpa = p[0];
		p++;
		if (snpa) {
			printf(" %u snpa", snpa);
			for (/*nothing*/; snpa > 0; snpa--) {
				printf("(%d bytes)", p[0]);
				p += p[0] + 1;
			}
			printf(",");
		}

		printf(" NLRI");
		while (len - (p - dat) > 0) {
			if (af == AFNUM_INET)
				advance = decode_prefix4(p, buf, sizeof(buf));
#ifdef INET6
			else if (af == AFNUM_INET6)
				advance = decode_prefix6(p, buf, sizeof(buf));
#endif
			printf(" %s", buf);

			p += advance;
		}

		break;

	case BGPTYPE_MP_UNREACH_NLRI:
		af = ntohs(*(u_int16_t *)p);
		safi = p[2];
		if (safi >= 128)
			printf(" %s vendor specific,", af_name(af));
		else {
			printf(" %s %s,", af_name(af),
				bgp_attr_nlri_safi(safi));
		}
		p += 3;

		printf(" Withdraw");
		while (len - (p - dat) > 0) {
			if (af == AFNUM_INET)
				advance = decode_prefix4(p, buf, sizeof(buf));
#ifdef INET6
			else if (af == AFNUM_INET6)
				advance = decode_prefix6(p, buf, sizeof(buf));
#endif
			printf(" %s", buf);

			p += advance;
		}
		break;
	default:
		break;
	}
}

static void
bgp_open_print(const u_char *dat, int length)
{
	struct bgp_open bgpo;
	struct bgp_opt bgpopt;
	int hlen;
	const u_char *opt;
	int i;

	memcpy(&bgpo, dat, sizeof(bgpo));
	hlen = ntohs(bgpo.bgpo_len);

	printf(": Version %d,", bgpo.bgpo_version);
	printf(" AS #%u,", ntohs(bgpo.bgpo_myas));
	printf(" Holdtime %u,", ntohs(bgpo.bgpo_holdtime));
	printf(" ID %s,", getname((char *)&bgpo.bgpo_id));
	printf(" Option length %u", bgpo.bgpo_optlen);

	/* ugly! */
	opt = &((struct bgp_open *)dat)->bgpo_optlen;
	opt++;

	for (i = 0; i < bgpo.bgpo_optlen; i++) {
		memcpy(&bgpopt, &opt[i], sizeof(bgpopt));
		if (i + 2 + bgpopt.bgpopt_len > bgpo.bgpo_optlen) {
			printf(" [|opt %d %d]", bgpopt.bgpopt_len, bgpopt.bgpopt_type);
			break;
		}

		printf(" (option %s, len=%d)", bgp_opttype(bgpopt.bgpopt_type),
			bgpopt.bgpopt_len);
		i += sizeof(bgpopt) + bgpopt.bgpopt_len;
	}
}

static void
bgp_update_print(const u_char *dat, int length)
{
	struct bgp bgp;
	struct bgp_attr bgpa;
	int hlen;
	const u_char *p;
	int len;
	int i;
	int newline;

	memcpy(&bgp, dat, sizeof(bgp));
	hlen = ntohs(bgp.bgp_len);
	p = dat + BGP_SIZE;	/*XXX*/
	printf(":");

	/* Unfeasible routes */
	len = ntohs(*(u_int16_t *)p);
	if (len) {
		printf(" (Withdrawn routes: %d bytes)", len);
	}
	p += 2 + len;

	len = ntohs(*(u_int16_t *)p);
	if (len) {
		/* do something more useful!*/
		i = 2;
		printf(" (Path attributes:");	/* ) */
		newline = 0;
		while (i < 2 + len) {
			int alen, aoff;

			memcpy(&bgpa, &p[i], sizeof(bgpa));
			alen = bgp_attr_len(&bgpa);
			aoff = bgp_attr_off(&bgpa);

			if (vflag && newline)
				printf("\n\t\t");
			else
				printf(" ");
			printf("(");		/* ) */
			printf("%s", bgp_attr_type(bgpa.bgpa_type));
			if (bgpa.bgpa_flags) {
				printf("[%s%s%s%s]",
					bgpa.bgpa_flags & 0x80 ? "O" : "",
					bgpa.bgpa_flags & 0x40 ? "T" : "",
					bgpa.bgpa_flags & 0x20 ? "P" : "",
					bgpa.bgpa_flags & 0x00 ? "E" : "");
			}

			bgp_attr_print(&bgpa, &p[i + aoff], alen);
			newline = 1;

			/* ( */
			printf(")");	

			i += aoff + alen;
		}

		/* ( */
		printf(")");
	}
	p += 2 + len;

	if (len && dat + length > p)
		printf("\n\t\t");
	if (dat + length > p) {
		printf("(NLRI:");	/* ) */
		while (dat + length > p) {
			char buf[MAXHOSTNAMELEN + 100];
			i = decode_prefix4(p, buf, sizeof(buf));
			printf(" %s", buf);
			if (i < 0)
				break;
			p += i;
		}

		/* ( */
		printf(")");
	}
}

static void
bgp_notification_print(const u_char *dat, int length)
{
	struct bgp_notification bgpn;
	int hlen;

	memcpy(&bgpn, dat, sizeof(bgpn));
	hlen = ntohs(bgpn.bgpn_len);

	printf(": error %s,", bgp_notify_major(bgpn.bgpn_major));
	printf(" subcode %s",
		bgp_notify_minor(bgpn.bgpn_major, bgpn.bgpn_minor));
}

static void
bgp_header_print(const u_char *dat, int length)
{
	struct bgp bgp;

	memcpy(&bgp, dat, sizeof(bgp));
	printf("(%s", bgp_type(bgp.bgp_type));		/* ) */

	switch (bgp.bgp_type) {
	case BGP_OPEN:
		bgp_open_print(dat, length);
		break;
	case BGP_UPDATE:
		bgp_update_print(dat, length);
		break;
	case BGP_NOTIFICATION:
		bgp_notification_print(dat, length);
		break;
	}

	/* ( */
	printf(")");
}

void
bgp_print(const u_char *dat, int length)
{
	const u_char *p;
	const u_char *ep;
	const u_char *start;
	const u_char marker[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	};
	struct bgp bgp;
	u_int16_t hlen;
	int newline;

	ep = dat + length;
	if (snapend < dat + length)
		ep = snapend;

	printf(": BGP");

	p = dat;
	newline = 0;
	start = p;
	while (p < snapend) {
		if (!TTEST2(p[0], 1))
			break;
		if (p[0] != 0xff) {
			p++;
			continue;
		}

		if (!TTEST2(p[0], sizeof(marker)))
			break;
		if (memcmp(p, marker, sizeof(marker)) != 0) {
			p++;
			continue;
		}

		/* found BGP header */
		TCHECK2(p[0], BGP_SIZE);	/*XXX*/
		memcpy(&bgp, p, sizeof(bgp));

		if (start != p)
			printf(" [|BGP]");

		hlen = ntohs(bgp.bgp_len);
		if (vflag && newline)
			printf("\n\t");
		else
			printf(" ");
		if (TTEST2(p[0], hlen)) {
			bgp_header_print(p, hlen);
			newline = 1;
			p += hlen;
			start = p;
		} else {
			printf("[|BGP %s]", bgp_type(bgp.bgp_type));
			break;
		}
	}

	return;

trunc:
	printf(" [|BGP]");
}
