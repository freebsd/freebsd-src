/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-isakmp.c,v 1.29 2001/10/26 03:41:29 itojun Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

struct mbuf;
struct rtentry;

#include <netinet/in.h>

#include <stdio.h>
#include <netdb.h>

#include "isakmp.h"
#include "ipsec_doi.h"
#include "oakley.h"
#include "interface.h"
#include "addrtoname.h"
#include "extract.h"                    /* must come after interface.h */

#include "ip.h"
#ifdef INET6
#include "ip6.h"
#endif

#ifndef HAVE_SOCKADDR_STORAGE
#define sockaddr_storage sockaddr
#endif

static u_char *isakmp_sa_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_p_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_t_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_ke_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_id_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_cert_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_cr_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_sig_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_hash_print(struct isakmp_gen *, u_char *,
	u_int32_t, u_int32_t, u_int32_t);
static u_char *isakmp_nonce_print(struct isakmp_gen *, u_char *,
	u_int32_t, u_int32_t, u_int32_t);
static u_char *isakmp_n_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_d_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_vid_print(struct isakmp_gen *, u_char *, u_int32_t,
	u_int32_t, u_int32_t);
static u_char *isakmp_sub0_print(u_char, struct isakmp_gen *, u_char *,
	u_int32_t, u_int32_t, u_int32_t);
static u_char *isakmp_sub_print(u_char, struct isakmp_gen *, u_char *,
	u_int32_t, u_int32_t, u_int32_t);
static char *numstr(int);
static void safememcpy(void *, void *, size_t);

#define MAXINITIATORS	20
int ninitiator = 0;
struct {
	cookie_t initiator;
	struct sockaddr_storage iaddr;
	struct sockaddr_storage raddr;
} cookiecache[MAXINITIATORS];

/* protocol id */
static char *protoidstr[] = {
	NULL, "isakmp", "ipsec-ah", "ipsec-esp", "ipcomp",
};

/* isakmp->np */
static char *npstr[] = {
	"none", "sa", "p", "t", "ke", "id", "cert", "cr", "hash",
	"sig", "nonce", "n", "d", "vid"
};

/* isakmp->np */
static u_char *(*npfunc[])(struct isakmp_gen *, u_char *, u_int32_t,
		u_int32_t, u_int32_t) = {
	NULL,
	isakmp_sa_print,
	isakmp_p_print,
	isakmp_t_print,
	isakmp_ke_print,
	isakmp_id_print,
	isakmp_cert_print,
	isakmp_cr_print,
	isakmp_hash_print,
	isakmp_sig_print,
	isakmp_nonce_print,
	isakmp_n_print,
	isakmp_d_print,
	isakmp_vid_print,
};

/* isakmp->etype */
static char *etypestr[] = {
	"none", "base", "ident", "auth", "agg", "inf", NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"oakley-quick", "oakley-newgroup",
};

#define STR_OR_ID(x, tab) \
	(((x) < sizeof(tab)/sizeof(tab[0]) && tab[(x)])	? tab[(x)] : numstr(x))
#define PROTOIDSTR(x)	STR_OR_ID(x, protoidstr)
#define NPSTR(x)	STR_OR_ID(x, npstr)
#define ETYPESTR(x)	STR_OR_ID(x, etypestr)

#define NPFUNC(x) \
	(((x) < sizeof(npfunc)/sizeof(npfunc[0]) && npfunc[(x)]) \
		? npfunc[(x)] : NULL)

static int
iszero(u_char *p, size_t l)
{
	while (l--) {
		if (*p++)
			return 0;
	}
	return 1;
}

/* find cookie from initiator cache */
static int
cookie_find(cookie_t *in)
{
	int i;

	for (i = 0; i < MAXINITIATORS; i++) {
		if (memcmp(in, &cookiecache[i].initiator, sizeof(*in)) == 0)
			return i;
	}

	return -1;
}

/* record initiator */
static void
cookie_record(cookie_t *in, const u_char *bp2)
{
	int i;
	struct ip *ip;
	struct sockaddr_in *sin;
#ifdef INET6
	struct ip6_hdr *ip6;
	struct sockaddr_in6 *sin6;
#endif

	i = cookie_find(in);
	if (0 <= i) {
		ninitiator = (i + 1) % MAXINITIATORS;
		return;
	}

	ip = (struct ip *)bp2;
	switch (IP_V(ip)) {
	case 4:
		memset(&cookiecache[ninitiator].iaddr, 0,
			sizeof(cookiecache[ninitiator].iaddr));
		memset(&cookiecache[ninitiator].raddr, 0,
			sizeof(cookiecache[ninitiator].raddr));

		sin = (struct sockaddr_in *)&cookiecache[ninitiator].iaddr;
#ifdef HAVE_SOCKADDR_SA_LEN
		sin->sin_len = sizeof(struct sockaddr_in);
#endif
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &ip->ip_src, sizeof(ip->ip_src));
		sin = (struct sockaddr_in *)&cookiecache[ninitiator].raddr;
#ifdef HAVE_SOCKADDR_SA_LEN
		sin->sin_len = sizeof(struct sockaddr_in);
#endif
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &ip->ip_dst, sizeof(ip->ip_dst));
		break;
#ifdef INET6
	case 6:
		memset(&cookiecache[ninitiator].iaddr, 0,
			sizeof(cookiecache[ninitiator].iaddr));
		memset(&cookiecache[ninitiator].raddr, 0,
			sizeof(cookiecache[ninitiator].raddr));

		ip6 = (struct ip6_hdr *)bp2;
		sin6 = (struct sockaddr_in6 *)&cookiecache[ninitiator].iaddr;
#ifdef HAVE_SOCKADDR_SA_LEN
		sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
		sin6->sin6_family = AF_INET6;
		memcpy(&sin6->sin6_addr, &ip6->ip6_src, sizeof(ip6->ip6_src));
		sin6 = (struct sockaddr_in6 *)&cookiecache[ninitiator].raddr;
#ifdef HAVE_SOCKADDR_SA_LEN
		sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
		sin6->sin6_family = AF_INET6;
		memcpy(&sin6->sin6_addr, &ip6->ip6_dst, sizeof(ip6->ip6_dst));
		break;
#endif
	default:
		return;
	}
	memcpy(&cookiecache[ninitiator].initiator, in, sizeof(*in));
	ninitiator = (ninitiator + 1) % MAXINITIATORS;
}

#define cookie_isinitiator(x, y)	cookie_sidecheck((x), (y), 1)
#define cookie_isresponder(x, y)	cookie_sidecheck((x), (y), 0)
static int
cookie_sidecheck(int i, const u_char *bp2, int initiator)
{
	struct sockaddr_storage ss;
	struct sockaddr *sa;
	struct ip *ip;
	struct sockaddr_in *sin;
#ifdef INET6
	struct ip6_hdr *ip6;
	struct sockaddr_in6 *sin6;
#endif
	int salen;

	memset(&ss, 0, sizeof(ss));
	ip = (struct ip *)bp2;
	switch (IP_V(ip)) {
	case 4:
		sin = (struct sockaddr_in *)&ss;
#ifdef HAVE_SOCKADDR_SA_LEN
		sin->sin_len = sizeof(struct sockaddr_in);
#endif
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &ip->ip_src, sizeof(ip->ip_src));
		break;
#ifdef INET6
	case 6:
		ip6 = (struct ip6_hdr *)bp2;
		sin6 = (struct sockaddr_in6 *)&ss;
#ifdef HAVE_SOCKADDR_SA_LEN
		sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
		sin6->sin6_family = AF_INET6;
		memcpy(&sin6->sin6_addr, &ip6->ip6_src, sizeof(ip6->ip6_src));
		break;
#endif
	default:
		return 0;
	}

	sa = (struct sockaddr *)&ss;
	if (initiator) {
		if (sa->sa_family != ((struct sockaddr *)&cookiecache[i].iaddr)->sa_family)
			return 0;
#ifdef HAVE_SOCKADDR_SA_LEN
		salen = sa->sa_len;
#else
#ifdef INET6
		if (sa->sa_family == AF_INET6)
			salen = sizeof(struct sockaddr_in6);
		else
			salen = sizeof(struct sockaddr);
#else
		salen = sizeof(struct sockaddr);
#endif
#endif
		if (memcmp(&ss, &cookiecache[i].iaddr, salen) == 0)
			return 1;
	} else {
		if (sa->sa_family != ((struct sockaddr *)&cookiecache[i].raddr)->sa_family)
			return 0;
#ifdef HAVE_SOCKADDR_SA_LEN
		salen = sa->sa_len;
#else
#ifdef INET6
		if (sa->sa_family == AF_INET6)
			salen = sizeof(struct sockaddr_in6);
		else
			salen = sizeof(struct sockaddr);
#else
		salen = sizeof(struct sockaddr);
#endif
#endif
		if (memcmp(&ss, &cookiecache[i].raddr, salen) == 0)
			return 1;
	}
	return 0;
}

static void
rawprint(caddr_t loc, size_t len)
{
	static u_char *p;
	int i;

	p = (u_char *)loc;
	for (i = 0; i < len; i++)
		printf("%02x", p[i] & 0xff);
}

struct attrmap {
	char *type;
	int nvalue;
	char *value[30];	/*XXX*/
};

static u_char *
isakmp_attrmap_print(u_char *p, u_char *ep, struct attrmap *map, size_t nmap)
{
	u_int16_t *q;
	int totlen;
	u_int32_t t, v;

	q = (u_int16_t *)p;
	if (p[0] & 0x80)
		totlen = 4;
	else
		totlen = 4 + ntohs(q[1]);
	if (ep < p + totlen) {
		printf("[|attr]");
		return ep + 1;
	}

	printf("(");
	t = ntohs(q[0]) & 0x7fff;
	if (map && t < nmap && map[t].type)
		printf("type=%s ", map[t].type);
	else
		printf("type=#%d ", t);
	if (p[0] & 0x80) {
		printf("value=");
		v = ntohs(q[1]);
		if (map && t < nmap && v < map[t].nvalue && map[t].value[v])
			printf("%s", map[t].value[v]);
		else
			rawprint((caddr_t)&q[1], 2);
	} else {
		printf("len=%d value=", ntohs(q[1]));
		rawprint((caddr_t)&p[4], ntohs(q[1]));
	}
	printf(")");
	return p + totlen;
}

static u_char *
isakmp_attr_print(u_char *p, u_char *ep)
{
	u_int16_t *q;
	int totlen;
	u_int32_t t;

	q = (u_int16_t *)p;
	if (p[0] & 0x80)
		totlen = 4;
	else
		totlen = 4 + ntohs(q[1]);
	if (ep < p + totlen) {
		printf("[|attr]");
		return ep + 1;
	}

	printf("(");
	t = ntohs(q[0]) & 0x7fff;
	printf("type=#%d ", t);
	if (p[0] & 0x80) {
		printf("value=");
		t = q[1];
		rawprint((caddr_t)&q[1], 2);
	} else {
		printf("len=%d value=", ntohs(q[1]));
		rawprint((caddr_t)&p[2], ntohs(q[1]));
	}
	printf(")");
	return p + totlen;
}

static u_char *
isakmp_sa_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi0, u_int32_t proto0)
{
	struct isakmp_pl_sa *p, sa;
	u_int32_t *q;
	u_int32_t doi, sit, ident;
	u_char *cp, *np;
	int t;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_SA));

	p = (struct isakmp_pl_sa *)ext;
	safememcpy(&sa, ext, sizeof(sa));
	doi = ntohl(sa.doi);
	sit = ntohl(sa.sit);
	if (doi != 1) {
		printf(" doi=%d", doi);
		printf(" situation=%u", (u_int32_t)ntohl(sa.sit));
		return (u_char *)(p + 1);
	}

	printf(" doi=ipsec");
	q = (u_int32_t *)&sa.sit;
	printf(" situation=");
	t = 0;
	if (sit & 0x01) {
		printf("identity");
		t++;
	}
	if (sit & 0x02) {
		printf("%ssecrecy", t ? "+" : "");
		t++;
	}
	if (sit & 0x04)
		printf("%sintegrity", t ? "+" : "");

	np = (u_char *)ext + sizeof(sa);
	if (sit != 0x01) {
		safememcpy(&ident, ext + 1, sizeof(ident));
		printf(" ident=%u", (u_int32_t)ntohl(ident));
		np += sizeof(ident);
	}

	ext = (struct isakmp_gen *)np;

	cp = isakmp_sub_print(ISAKMP_NPTYPE_P, ext, ep, phase, doi, proto0);

	return cp;
}

static u_char *
isakmp_p_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi0, u_int32_t proto0)
{
	struct isakmp_pl_p *p, prop;
	u_char *cp;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_P));

	p = (struct isakmp_pl_p *)ext;
	safememcpy(&prop, ext, sizeof(prop));
	printf(" #%d protoid=%s transform=%d",
		prop.p_no, PROTOIDSTR(prop.prot_id), prop.num_t);
	if (prop.spi_size) {
		printf(" spi=");
		rawprint((caddr_t)(p + 1), prop.spi_size);
	}

	ext = (struct isakmp_gen *)((u_char *)(p + 1) + prop.spi_size);

	cp = isakmp_sub_print(ISAKMP_NPTYPE_T, ext, ep, phase, doi0,
		prop.prot_id);

	return cp;
}

static char *isakmp_p_map[] = {
	NULL, "ike",
};

static char *ah_p_map[] = {
	NULL, "(reserved)", "md5", "sha", "1des",
	"sha2-256", "sha2-384", "sha2-512",
};

static char *esp_p_map[] = {
	NULL, "1des-iv64", "1des", "3des", "rc5", "idea", "cast",
	"blowfish", "3idea", "1des-iv32", "rc4", "null", "aes"
};

static char *ipcomp_p_map[] = {
	NULL, "oui", "deflate", "lzs",
};

struct attrmap ipsec_t_map[] = {
	{ NULL,	0, },
	{ "lifetype", 3, { NULL, "sec", "kb", }, },
	{ "life", 0, },
	{ "group desc", 5,	{ NULL, "modp768", "modp1024", "EC2N 2^155",
				  "EC2N 2^185", }, },
	{ "enc mode", 3, { NULL, "tunnel", "transport", }, },
	{ "auth", 5, { NULL, "hmac-md5", "hmac-sha1", "1des-mac", "keyed", }, },
	{ "keylen", 0, },
	{ "rounds", 0, },
	{ "dictsize", 0, },
	{ "privalg", 0, },
};

struct attrmap oakley_t_map[] = {
	{ NULL,	0 },
	{ "enc", 8,	{ NULL, "1des", "idea", "blowfish", "rc5",
		 	  "3des", "cast", "aes", }, },
	{ "hash", 7,	{ NULL, "md5", "sha1", "tiger",
			  "sha2-256", "sha2-384", "sha2-512", }, },
	{ "auth", 6,	{ NULL, "preshared", "dss", "rsa sig", "rsa enc",
			  "rsa enc revised", }, },
	{ "group desc", 5,	{ NULL, "modp768", "modp1024", "EC2N 2^155",
				  "EC2N 2^185", }, },
	{ "group type", 4,	{ NULL, "MODP", "ECP", "EC2N", }, },
	{ "group prime", 0, },
	{ "group gen1", 0, },
	{ "group gen2", 0, },
	{ "group curve A", 0, },
	{ "group curve B", 0, },
	{ "lifetype", 3,	{ NULL, "sec", "kb", }, },
	{ "lifeduration", 0, },
	{ "prf", 0, },
	{ "keylen", 0, },
	{ "field", 0, },
	{ "order", 0, },
};

static u_char *
isakmp_t_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
	struct isakmp_pl_t *p, t;
	u_char *cp;
	char *idstr;
	struct attrmap *map;
	size_t nmap;
	u_char *ep2;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_T));

	p = (struct isakmp_pl_t *)ext;
	safememcpy(&t, ext, sizeof(t));

	switch (proto) {
	case 1:
		idstr = STR_OR_ID(t.t_id, isakmp_p_map);
		map = oakley_t_map;
		nmap = sizeof(oakley_t_map)/sizeof(oakley_t_map[0]);
		break;
	case 2:
		idstr = STR_OR_ID(t.t_id, ah_p_map);
		map = ipsec_t_map;
		nmap = sizeof(ipsec_t_map)/sizeof(ipsec_t_map[0]);
		break;
	case 3:
		idstr = STR_OR_ID(t.t_id, esp_p_map);
		map = ipsec_t_map;
		nmap = sizeof(ipsec_t_map)/sizeof(ipsec_t_map[0]);
		break;
	case 4:
		idstr = STR_OR_ID(t.t_id, ipcomp_p_map);
		map = ipsec_t_map;
		nmap = sizeof(ipsec_t_map)/sizeof(ipsec_t_map[0]);
		break;
	default:
		idstr = NULL;
		map = NULL;
		nmap = 0;
		break;
	}

	if (idstr)
		printf(" #%d id=%s ", t.t_no, idstr);
	else
		printf(" #%d id=%d ", t.t_no, t.t_id);
	cp = (u_char *)(p + 1);
	ep2 = (u_char *)p + ntohs(t.h.len);
	while (cp < ep && cp < ep2) {
		if (map && nmap) {
			cp = isakmp_attrmap_print(cp, (ep < ep2) ? ep : ep2,
				map, nmap);
		} else
			cp = isakmp_attr_print(cp, (ep < ep2) ? ep : ep2);
	}
	if (ep < ep2)
		printf("...");
	return cp;
}

static u_char *
isakmp_ke_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
	struct isakmp_gen e;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_KE));

	safememcpy(&e, ext, sizeof(e));
	printf(" key len=%d", ntohs(e.len) - 4);
	if (2 < vflag && 4 < ntohs(e.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(e.len) - 4);
	}
	return (u_char *)ext + ntohs(e.len);
}

static u_char *
isakmp_id_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
#define USE_IPSECDOI_IN_PHASE1	1
	struct isakmp_pl_id *p, id;
	static char *idtypestr[] = {
		"IPv4", "IPv4net", "IPv6", "IPv6net",
	};
	static char *ipsecidtypestr[] = {
		NULL, "IPv4", "FQDN", "user FQDN", "IPv4net", "IPv6",
		"IPv6net", "IPv4range", "IPv6range", "ASN1 DN", "ASN1 GN",
		"keyid",
	};
	int len;
	u_char *data;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_ID));

	p = (struct isakmp_pl_id *)ext;
	safememcpy(&id, ext, sizeof(id));
	if (sizeof(*p) < id.h.len)
		data = (u_char *)(p + 1);
	else
		data = NULL;
	len = ntohs(id.h.len) - sizeof(*p);

#if 0 /*debug*/
	printf(" [phase=%d doi=%d proto=%d]", phase, doi, proto);
#endif
	switch (phase) {
#ifndef USE_IPSECDOI_IN_PHASE1
	case 1:
#endif
	default:
		printf(" idtype=%s", STR_OR_ID(id.d.id_type, idtypestr));
		printf(" doi_data=%u",
			(u_int32_t)(ntohl(id.d.doi_data) & 0xffffff));
		break;

#ifdef USE_IPSECDOI_IN_PHASE1
	case 1:
#endif
	case 2:
	    {
		struct ipsecdoi_id *p, id;
		struct protoent *pe;

		p = (struct ipsecdoi_id *)ext;
		safememcpy(&id, ext, sizeof(id));
		printf(" idtype=%s", STR_OR_ID(id.type, ipsecidtypestr));
		if (id.proto_id) {
			setprotoent(1);
			pe = getprotobynumber(id.proto_id);
			if (pe)
				printf(" protoid=%s", pe->p_name);
			endprotoent();
		} else {
			/* it DOES NOT mean IPPROTO_IP! */
			printf(" protoid=%s", "0");
		}
		printf(" port=%d", ntohs(id.port));
		if (!len)
			break;
		switch (id.type) {
		case IPSECDOI_ID_IPV4_ADDR:
			printf(" len=%d %s", len, ipaddr_string(data));
			len = 0;
			break;
		case IPSECDOI_ID_FQDN:
		case IPSECDOI_ID_USER_FQDN:
		    {
			int i;
			printf(" len=%d ", len);
			for (i = 0; i < len; i++)
				safeputchar(data[i]);
			len = 0;
			break;
		    }
		case IPSECDOI_ID_IPV4_ADDR_SUBNET:
		    {
			u_char *mask;
			mask = data + sizeof(struct in_addr);
			printf(" len=%d %s/%u.%u.%u.%u", len,
				ipaddr_string(data),
				mask[0], mask[1], mask[2], mask[3]);
			len = 0;
			break;
		    }
#ifdef INET6
		case IPSECDOI_ID_IPV6_ADDR:
			printf(" len=%d %s", len, ip6addr_string(data));
			len = 0;
			break;
		case IPSECDOI_ID_IPV6_ADDR_SUBNET:
		    {
			u_int32_t *mask;
			mask = (u_int32_t *)(data + sizeof(struct in6_addr));
			/*XXX*/
			printf(" len=%d %s/0x%08x%08x%08x%08x", len,
				ip6addr_string(data),
				mask[0], mask[1], mask[2], mask[3]);
			len = 0;
			break;
		    }
#endif /*INET6*/
		case IPSECDOI_ID_IPV4_ADDR_RANGE:
			printf(" len=%d %s-%s", len, ipaddr_string(data),
				ipaddr_string(data + sizeof(struct in_addr)));
			len = 0;
			break;
#ifdef INET6
		case IPSECDOI_ID_IPV6_ADDR_RANGE:
			printf(" len=%d %s-%s", len, ip6addr_string(data),
				ip6addr_string(data + sizeof(struct in6_addr)));
			len = 0;
			break;
#endif /*INET6*/
		case IPSECDOI_ID_DER_ASN1_DN:
		case IPSECDOI_ID_DER_ASN1_GN:
		case IPSECDOI_ID_KEY_ID:
			break;
		}
		break;
	    }
	}
	if (data && len) {
		printf(" len=%d", len);
		if (2 < vflag) {
			printf(" ");
			rawprint((caddr_t)data, len);
		}
	}
	return (u_char *)ext + ntohs(id.h.len);
}

static u_char *
isakmp_cert_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi0, u_int32_t proto0)
{
	struct isakmp_pl_cert *p, cert;
	static char *certstr[] = {
		"none",	"pkcs7", "pgp", "dns",
		"x509sign", "x509ke", "kerberos", "crl",
		"arl", "spki", "x509attr",
	};

	printf("%s:", NPSTR(ISAKMP_NPTYPE_CERT));

	p = (struct isakmp_pl_cert *)ext;
	safememcpy(&cert, ext, sizeof(cert));
	printf(" len=%d", ntohs(cert.h.len) - 4);
	printf(" type=%s", STR_OR_ID((cert.encode), certstr));
	if (2 < vflag && 4 < ntohs(cert.h.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(cert.h.len) - 4);
	}
	return (u_char *)ext + ntohs(cert.h.len);
}

static u_char *
isakmp_cr_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi0, u_int32_t proto0)
{
	struct isakmp_pl_cert *p, cert;
	static char *certstr[] = {
		"none",	"pkcs7", "pgp", "dns",
		"x509sign", "x509ke", "kerberos", "crl",
		"arl", "spki", "x509attr",
	};

	printf("%s:", NPSTR(ISAKMP_NPTYPE_CR));

	p = (struct isakmp_pl_cert *)ext;
	safememcpy(&cert, ext, sizeof(cert));
	printf(" len=%d", ntohs(cert.h.len) - 4);
	printf(" type=%s", STR_OR_ID((cert.encode), certstr));
	if (2 < vflag && 4 < ntohs(cert.h.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(cert.h.len) - 4);
	}
	return (u_char *)ext + ntohs(cert.h.len);
}

static u_char *
isakmp_hash_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
	struct isakmp_gen e;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_HASH));

	safememcpy(&e, ext, sizeof(e));
	printf(" len=%d", ntohs(e.len) - 4);
	if (2 < vflag && 4 < ntohs(e.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(e.len) - 4);
	}
	return (u_char *)ext + ntohs(e.len);
}

static u_char *
isakmp_sig_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
	struct isakmp_gen e;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_SIG));

	safememcpy(&e, ext, sizeof(e));
	printf(" len=%d", ntohs(e.len) - 4);
	if (2 < vflag && 4 < ntohs(e.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(e.len) - 4);
	}
	return (u_char *)ext + ntohs(e.len);
}

static u_char *
isakmp_nonce_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
	struct isakmp_gen e;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_NONCE));

	safememcpy(&e, ext, sizeof(e));
	printf(" n len=%d", ntohs(e.len) - 4);
	if (2 < vflag && 4 < ntohs(e.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(e.len) - 4);
	}
	return (u_char *)ext + ntohs(e.len);
}

static u_char *
isakmp_n_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi0, u_int32_t proto0)
{
	struct isakmp_pl_n *p, n;
	u_char *cp;
	u_char *ep2;
	u_int32_t doi;
	u_int32_t proto;
	static char *notifystr[] = {
		NULL,				"INVALID-PAYLOAD-TYPE",
		"DOI-NOT-SUPPORTED",		"SITUATION-NOT-SUPPORTED",
		"INVALID-COOKIE",		"INVALID-MAJOR-VERSION",
		"INVALID-MINOR-VERSION",	"INVALID-EXCHANGE-TYPE",
		"INVALID-FLAGS",		"INVALID-MESSAGE-ID",
		"INVALID-PROTOCOL-ID",		"INVALID-SPI",
		"INVALID-TRANSFORM-ID",		"ATTRIBUTES-NOT-SUPPORTED",
		"NO-PROPOSAL-CHOSEN",		"BAD-PROPOSAL-SYNTAX",
		"PAYLOAD-MALFORMED",		"INVALID-KEY-INFORMATION",
		"INVALID-ID-INFORMATION",	"INVALID-CERT-ENCODING",
		"INVALID-CERTIFICATE",		"CERT-TYPE-UNSUPPORTED",
		"INVALID-CERT-AUTHORITY",	"INVALID-HASH-INFORMATION",
		"AUTHENTICATION-FAILED",	"INVALID-SIGNATURE",
		"ADDRESS-NOTIFICATION",		"NOTIFY-SA-LIFETIME",
		"CERTIFICATE-UNAVAILABLE",	"UNSUPPORTED-EXCHANGE-TYPE",
		"UNEQUAL-PAYLOAD-LENGTHS",
	};
	static char *ipsecnotifystr[] = {
		"RESPONDER-LIFETIME",		"REPLAY-STATUS",
		"INITIAL-CONTACT",
	};
/* NOTE: these macro must be called with x in proper range */
#define NOTIFYSTR(x) \
	(((x) == 16384) ? "CONNECTED" : STR_OR_ID((x), notifystr))
#define IPSECNOTIFYSTR(x) \
	(((x) == 8192) ? "RESERVED" : STR_OR_ID(((x) - 24576), ipsecnotifystr))

	printf("%s:", NPSTR(ISAKMP_NPTYPE_N));

	p = (struct isakmp_pl_n *)ext;
	safememcpy(&n, ext, sizeof(n));
	doi = ntohl(n.doi);
	proto = n.prot_id;
	if (doi != 1) {
		printf(" doi=%d", doi);
		printf(" proto=%d", proto);
		printf(" type=%s", NOTIFYSTR(ntohs(n.type)));
		if (n.spi_size) {
			printf(" spi=");
			rawprint((caddr_t)(p + 1), n.spi_size);
		}
		return (u_char *)(p + 1) + n.spi_size;
	}

	printf(" doi=ipsec");
	printf(" proto=%s", PROTOIDSTR(proto));
	if (ntohs(n.type) < 8192)
		printf(" type=%s", NOTIFYSTR(ntohs(n.type)));
	else if (ntohs(n.type) < 16384)
		printf(" type=%s", IPSECNOTIFYSTR(ntohs(n.type)));
	else if (ntohs(n.type) < 24576)
		printf(" type=%s", NOTIFYSTR(ntohs(n.type)));
	else if (ntohs(n.type) < 40960)
		printf(" type=%s", IPSECNOTIFYSTR(ntohs(n.type)));
	else
		printf(" type=%s", NOTIFYSTR(ntohs(n.type)));
	if (n.spi_size) {
		printf(" spi=");
		rawprint((caddr_t)(p + 1), n.spi_size);
	}

	cp = (u_char *)(p + 1) + n.spi_size;
	ep2 = (u_char *)p + ntohs(n.h.len);

	if (cp < ep) {
		printf(" orig=(");
		switch (ntohs(n.type)) {
		case IPSECDOI_NTYPE_RESPONDER_LIFETIME:
		    {
			struct attrmap *map = oakley_t_map;
			size_t nmap = sizeof(oakley_t_map)/sizeof(oakley_t_map[0]);
			while (cp < ep && cp < ep2) {
				cp = isakmp_attrmap_print(cp,
					(ep < ep2) ? ep : ep2, map, nmap);
			}
			break;
		    }
		case IPSECDOI_NTYPE_REPLAY_STATUS:
			printf("replay detection %sabled",
				(*(u_int32_t *)cp) ? "en" : "dis");
			break;
		case ISAKMP_NTYPE_NO_PROPOSAL_CHOSEN:
			isakmp_sub_print(ISAKMP_NPTYPE_SA,
				(struct isakmp_gen *)cp, ep, phase, doi, proto);
			break;
		default:
			/* NULL is dummy */
			isakmp_print(cp,
				ntohs(n.h.len) - sizeof(*p) - n.spi_size,
				NULL);
		}
		printf(")");
	}
	return (u_char *)ext + ntohs(n.h.len);
}

static u_char *
isakmp_d_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi0, u_int32_t proto0)
{
	struct isakmp_pl_d *p, d;
	u_int8_t *q;
	u_int32_t doi;
	u_int32_t proto;
	int i;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_D));

	p = (struct isakmp_pl_d *)ext;
	safememcpy(&d, ext, sizeof(d));
	doi = ntohl(d.doi);
	proto = d.prot_id;
	if (doi != 1) {
		printf(" doi=%u", doi);
		printf(" proto=%u", proto);
	} else {
		printf(" doi=ipsec");
		printf(" proto=%s", PROTOIDSTR(proto));
	}
	printf(" spilen=%u", d.spi_size);
	printf(" nspi=%u", ntohs(d.num_spi));
	printf(" spi=");
	q = (u_int8_t *)(p + 1);
	for (i = 0; i < ntohs(d.num_spi); i++) {
		if (i != 0)
			printf(",");
		rawprint((caddr_t)q, d.spi_size);
		q += d.spi_size;
	}
	return q;
}

static u_char *
isakmp_vid_print(struct isakmp_gen *ext, u_char *ep, u_int32_t phase,
	u_int32_t doi, u_int32_t proto)
{
	struct isakmp_gen e;

	printf("%s:", NPSTR(ISAKMP_NPTYPE_VID));

	safememcpy(&e, ext, sizeof(e));
	printf(" len=%d", ntohs(e.len) - 4);
	if (2 < vflag && 4 < ntohs(e.len)) {
		printf(" ");
		rawprint((caddr_t)(ext + 1), ntohs(e.len) - 4);
	}
	return (u_char *)ext + ntohs(e.len);
}

static u_char *
isakmp_sub0_print(u_char np, struct isakmp_gen *ext, u_char *ep,
	u_int32_t phase, u_int32_t doi, u_int32_t proto)
{
	u_char *cp;
	struct isakmp_gen e;

	cp = (u_char *)ext;
	safememcpy(&e, ext, sizeof(e));

	if (NPFUNC(np))
		cp = (*NPFUNC(np))(ext, ep, phase, doi, proto);
	else {
		printf("%s", NPSTR(np));
		cp += ntohs(e.len);
	}
	return cp;
}

static u_char *
isakmp_sub_print(u_char np, struct isakmp_gen *ext, u_char *ep,
	u_int32_t phase, u_int32_t doi, u_int32_t proto)
{
	u_char *cp;
	static int depth = 0;
	int i;
	struct isakmp_gen e;

	cp = (u_char *)ext;

	while (np) {
		safememcpy(&e, ext, sizeof(e));

		if (ep < (u_char *)ext + ntohs(e.len)) {
			printf(" [|%s]", NPSTR(np));
			cp = ep + 1;
			break;
		}
		depth++;
		printf("\n");
		for (i = 0; i < depth; i++)
			printf("    ");
		printf("(");
		cp = isakmp_sub0_print(np, ext, ep, phase, doi, proto);
		printf(")");
		depth--;

		np = e.np;
		ext = (struct isakmp_gen *)cp;
	}
	return cp;
}

static char *
numstr(int x)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "#%d", x);
	return buf;
}

/*
 * some compiler tries to optimize memcpy(), using the alignment constraint
 * on the argument pointer type.  by using this function, we try to avoid the
 * optimization.
 */
static void
safememcpy(void *p, void *q, size_t l)
{
	memcpy(p, q, l);
}

void
isakmp_print(const u_char *bp, u_int length, const u_char *bp2)
{
	struct isakmp *p, base;
	u_char *ep;
	u_char np;
	int i;
	int phase;
	int major, minor;

	p = (struct isakmp *)bp;
	ep = (u_char *)snapend;

	if ((struct isakmp *)ep < p + 1) {
		printf("[|isakmp]");
		return;
	}

	safememcpy(&base, p, sizeof(base));

	printf("isakmp");
	if (vflag) {
		major = (base.vers & ISAKMP_VERS_MAJOR)
				>> ISAKMP_VERS_MAJOR_SHIFT;
		minor = (base.vers & ISAKMP_VERS_MINOR)
				>> ISAKMP_VERS_MINOR_SHIFT;
		printf(" %d.%d", major, minor);
	}

	if (vflag) {
		printf(" msgid ");
		rawprint((caddr_t)&base.msgid, sizeof(base.msgid));
	}

	if (1 < vflag) {
		printf(" cookie ");
		rawprint((caddr_t)&base.i_ck, sizeof(base.i_ck));
		printf("->");
		rawprint((caddr_t)&base.r_ck, sizeof(base.r_ck));
	}
	printf(":");

	phase = (*(u_int32_t *)base.msgid == 0) ? 1 : 2;
	if (phase == 1)
		printf(" phase %d", phase);
	else
		printf(" phase %d/others", phase);

	i = cookie_find(&base.i_ck);
	if (i < 0) {
		if (iszero((u_char *)&base.r_ck, sizeof(base.r_ck))) {
			/* the first packet */
			printf(" I");
			if (bp2)
				cookie_record(&base.i_ck, bp2);
		} else
			printf(" ?");
	} else {
		if (bp2 && cookie_isinitiator(i, bp2))
			printf(" I");
		else if (bp2 && cookie_isresponder(i, bp2))
			printf(" R");
		else
			printf(" ?");
	}

	printf(" %s", ETYPESTR(base.etype));
	if (base.flags) {
		printf("[%s%s]", base.flags & ISAKMP_FLAG_E ? "E" : "",
			base.flags & ISAKMP_FLAG_C ? "C" : "");
	}
	printf(":");

    {
	struct isakmp_gen *ext;
	int nparen;

#define CHECKLEN(p, np) \
	if (ep < (u_char *)(p)) {				\
		printf(" [|%s]", NPSTR(np));			\
		goto done;					\
	}

	/* regardless of phase... */
	if (base.flags & ISAKMP_FLAG_E) {
		/*
		 * encrypted, nothing we can do right now.
		 * we hope to decrypt the packet in the future...
		 */
		printf(" [encrypted %s]", NPSTR(base.np));
		goto done;
	}

	nparen = 0;
	CHECKLEN(p + 1, base.np)

	np = base.np;
	ext = (struct isakmp_gen *)(p + 1);
	isakmp_sub_print(np, ext, ep, phase, 0, 0);
    }

done:
	if (vflag) {
		if (ntohl(base.len) != length) {
			printf(" (len mismatch: isakmp %u/ip %d)",
				(u_int32_t)ntohl(base.len), length);
		}
	}
}
