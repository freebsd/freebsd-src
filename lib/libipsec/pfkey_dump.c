/*	$KAME: pfkey_dump.c,v 1.28 2001/06/27 10:46:51 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet6/ipsec.h>
#include <net/pfkeyv2.h>
#include <netkey/key_var.h>
#include <netkey/key_debug.h>

#include <netinet/in.h>
#include <netinet6/ipsec.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include "ipsec_strerror.h"
#include "libpfkey.h"

/* cope with old kame headers - ugly */
#ifndef SADB_X_AALG_MD5
#define SADB_X_AALG_MD5		SADB_AALG_MD5	
#endif
#ifndef SADB_X_AALG_SHA
#define SADB_X_AALG_SHA		SADB_AALG_SHA
#endif
#ifndef SADB_X_AALG_NULL
#define SADB_X_AALG_NULL	SADB_AALG_NULL
#endif

#ifndef SADB_X_EALG_BLOWFISHCBC
#define SADB_X_EALG_BLOWFISHCBC	SADB_EALG_BLOWFISHCBC
#endif
#ifndef SADB_X_EALG_CAST128CBC
#define SADB_X_EALG_CAST128CBC	SADB_EALG_CAST128CBC
#endif
#ifndef SADB_X_EALG_RC5CBC
#ifdef SADB_EALG_RC5CBC
#define SADB_X_EALG_RC5CBC	SADB_EALG_RC5CBC
#endif
#endif

#define GETMSGSTR(str, num) \
do { \
	if (sizeof((str)[0]) == 0 \
	 || num >= sizeof(str)/sizeof((str)[0])) \
		printf("%d ", (num)); \
	else if (strlen((str)[(num)]) == 0) \
		printf("%d ", (num)); \
	else \
		printf("%s ", (str)[(num)]); \
} while (0)

#define GETMSGV2S(v2s, num) \
do { \
	struct val2str *p;  \
	for (p = (v2s); p && p->str; p++) { \
		if (p->val == (num)) \
			break; \
	} \
	if (p && p->str) \
		printf("%s ", p->str); \
	else \
		printf("%d ", (num)); \
} while (0)

static char *str_ipaddr(struct sockaddr *);
static char *str_prefport(u_int, u_int, u_int);
static char *str_time(time_t);
static void str_lifetime_byte(struct sadb_lifetime *, char *);

struct val2str {
	int val;
	const char *str;
};

/*
 * Must to be re-written about following strings.
 */
static char *str_satype[] = {
	"unspec",
	"unknown",
	"ah",
	"esp",
	"unknown",
	"rsvp",
	"ospfv2",
	"ripv2",
	"mip",
	"ipcomp",
};

static char *str_mode[] = {
	"any",
	"transport",
	"tunnel",
};

static char *str_upper[] = {
/*0*/	"ip", "icmp", "igmp", "ggp", "ip4",
	"", "tcp", "", "egp", "",
/*10*/	"", "", "", "", "",
	"", "", "udp", "", "",
/*20*/	"", "", "idp", "", "",
	"", "", "", "", "tp",
/*30*/	"", "", "", "", "",
	"", "", "", "", "",
/*40*/	"", "ip6", "", "rt6", "frag6",
	"", "rsvp", "gre", "", "",
/*50*/	"esp", "ah", "", "", "",
	"", "", "", "icmp6", "none",
/*60*/	"dst6",
};

static char *str_state[] = {
	"larval",
	"mature",
	"dying",
	"dead",
};

static struct val2str str_alg_auth[] = {
	{ SADB_AALG_NONE, "none", },
	{ SADB_AALG_MD5HMAC, "hmac-md5", },
	{ SADB_AALG_SHA1HMAC, "hmac-sha1", },
	{ SADB_X_AALG_MD5, "md5", },
	{ SADB_X_AALG_SHA, "sha", },
	{ SADB_X_AALG_NULL, "null", },
#ifdef SADB_X_AALG_SHA2_256
	{ SADB_X_AALG_SHA2_256, "hmac-sha2-256", },
#endif
#ifdef SADB_X_AALG_SHA2_384
	{ SADB_X_AALG_SHA2_384, "hmac-sha2-384", },
#endif
#ifdef SADB_X_AALG_SHA2_512
	{ SADB_X_AALG_SHA2_512, "hmac-sha2-512", },
#endif
#ifdef SADB_X_AALG_RIPEMD160HMAC
	{ SADB_X_AALG_RIPEMD160HMAC, "hmac-ripemd160", },
#endif
	{ -1, NULL, },
};

static struct val2str str_alg_enc[] = {
	{ SADB_EALG_NONE, "none", },
	{ SADB_EALG_DESCBC, "des-cbc", },
	{ SADB_EALG_3DESCBC, "3des-cbc", },
	{ SADB_EALG_NULL, "null", },
#ifdef SADB_X_EALG_RC5CBC
	{ SADB_X_EALG_RC5CBC, "rc5-cbc", },
#endif
	{ SADB_X_EALG_CAST128CBC, "cast128-cbc", },
	{ SADB_X_EALG_BLOWFISHCBC, "blowfish-cbc", },
#ifdef SADB_X_EALG_RIJNDAELCBC
	{ SADB_X_EALG_RIJNDAELCBC, "rijndael-cbc", },
#endif
#ifdef SADB_X_EALG_TWOFISHCBC
	{ SADB_X_EALG_TWOFISHCBC, "twofish-cbc", },
#endif
	{ -1, NULL, },
};

static struct val2str str_alg_comp[] = {
	{ SADB_X_CALG_NONE, "none", },
	{ SADB_X_CALG_OUI, "oui", },
	{ SADB_X_CALG_DEFLATE, "deflate", },
	{ SADB_X_CALG_LZS, "lzs", },
	{ -1, NULL, },
};

/*
 * dump SADB_MSG formated.  For debugging, you should use kdebug_sadb().
 */
void
pfkey_sadump(m)
	struct sadb_msg *m;
{
	caddr_t mhp[SADB_EXT_MAX + 1];
	struct sadb_sa *m_sa;
	struct sadb_x_sa2 *m_sa2;
	struct sadb_lifetime *m_lftc, *m_lfth, *m_lfts;
	struct sadb_address *m_saddr, *m_daddr, *m_paddr;
	struct sadb_key *m_auth, *m_enc;
	struct sadb_ident *m_sid, *m_did;
	struct sadb_sens *m_sens;

	/* check pfkey message. */
	if (pfkey_align(m, mhp)) {
		printf("%s\n", ipsec_strerror());
		return;
	}
	if (pfkey_check(mhp)) {
		printf("%s\n", ipsec_strerror());
		return;
	}

	m_sa = (struct sadb_sa *)mhp[SADB_EXT_SA];
	m_sa2 = (struct sadb_x_sa2 *)mhp[SADB_X_EXT_SA2];
	m_lftc = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_CURRENT];
	m_lfth = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_HARD];
	m_lfts = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_SOFT];
	m_saddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];
	m_daddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	m_paddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_PROXY];
	m_auth = (struct sadb_key *)mhp[SADB_EXT_KEY_AUTH];
	m_enc = (struct sadb_key *)mhp[SADB_EXT_KEY_ENCRYPT];
	m_sid = (struct sadb_ident *)mhp[SADB_EXT_IDENTITY_SRC];
	m_did = (struct sadb_ident *)mhp[SADB_EXT_IDENTITY_DST];
	m_sens = (struct sadb_sens *)mhp[SADB_EXT_SENSITIVITY];

	/* source address */
	if (m_saddr == NULL) {
		printf("no ADDRESS_SRC extension.\n");
		return;
	}
	printf("%s ", str_ipaddr((struct sockaddr *)(m_saddr + 1)));

	/* destination address */
	if (m_daddr == NULL) {
		printf("no ADDRESS_DST extension.\n");
		return;
	}
	printf("%s ", str_ipaddr((struct sockaddr *)(m_daddr + 1)));

	/* SA type */
	if (m_sa == NULL) {
		printf("no SA extension.\n");
		return;
	}
	if (m_sa2 == NULL) {
		printf("no SA2 extension.\n");
		return;
	}
	printf("\n\t");

	GETMSGSTR(str_satype, m->sadb_msg_satype);

	printf("mode=");
	GETMSGSTR(str_mode, m_sa2->sadb_x_sa2_mode);

	printf("spi=%u(0x%08x) reqid=%u(0x%08x)\n",
		(u_int32_t)ntohl(m_sa->sadb_sa_spi),
		(u_int32_t)ntohl(m_sa->sadb_sa_spi),
		(u_int32_t)m_sa2->sadb_x_sa2_reqid,
		(u_int32_t)m_sa2->sadb_x_sa2_reqid);

	/* encryption key */
	if (m->sadb_msg_satype == SADB_X_SATYPE_IPCOMP) {
		printf("\tC: ");
		GETMSGV2S(str_alg_comp, m_sa->sadb_sa_encrypt);
	} else if (m->sadb_msg_satype == SADB_SATYPE_ESP) {
		if (m_enc != NULL) {
			printf("\tE: ");
			GETMSGV2S(str_alg_enc, m_sa->sadb_sa_encrypt);
			ipsec_hexdump((caddr_t)m_enc + sizeof(*m_enc),
				      m_enc->sadb_key_bits / 8);
			printf("\n");
		}
	}

	/* authentication key */
	if (m_auth != NULL) {
		printf("\tA: ");
		GETMSGV2S(str_alg_auth, m_sa->sadb_sa_auth);
		ipsec_hexdump((caddr_t)m_auth + sizeof(*m_auth),
		              m_auth->sadb_key_bits / 8);
		printf("\n");
	}

	/* replay windoe size & flags */
	printf("\tseq=0x%08x replay=%u flags=0x%08x ",
		m_sa2->sadb_x_sa2_sequence,
		m_sa->sadb_sa_replay,
		m_sa->sadb_sa_flags);

	/* state */
	printf("state=");
	GETMSGSTR(str_state, m_sa->sadb_sa_state);
	printf("\n");

	/* lifetime */
	if (m_lftc != NULL) {
		time_t tmp_time = time(0);

		printf("\tcreated: %s",
			str_time(m_lftc->sadb_lifetime_addtime));
		printf("\tcurrent: %s\n", str_time(tmp_time));
		printf("\tdiff: %lu(s)",
			(u_long)(m_lftc->sadb_lifetime_addtime == 0 ?
			0 : (tmp_time - m_lftc->sadb_lifetime_addtime)));

		printf("\thard: %lu(s)",
			(u_long)(m_lfth == NULL ?
			0 : m_lfth->sadb_lifetime_addtime));
		printf("\tsoft: %lu(s)\n",
			(u_long)(m_lfts == NULL ?
			0 : m_lfts->sadb_lifetime_addtime));

		printf("\tlast: %s",
			str_time(m_lftc->sadb_lifetime_usetime));
		printf("\thard: %lu(s)",
			(u_long)(m_lfth == NULL ?
			0 : m_lfth->sadb_lifetime_usetime));
		printf("\tsoft: %lu(s)\n",
			(u_long)(m_lfts == NULL ?
			0 : m_lfts->sadb_lifetime_usetime));

		str_lifetime_byte(m_lftc, "current");
		str_lifetime_byte(m_lfth, "hard");
		str_lifetime_byte(m_lfts, "soft");
		printf("\n");

		printf("\tallocated: %lu",
			(unsigned long)m_lftc->sadb_lifetime_allocations);
		printf("\thard: %lu",
			(u_long)(m_lfth == NULL ?
			0 : m_lfth->sadb_lifetime_allocations));
		printf("\tsoft: %lu\n",
			(u_long)(m_lfts == NULL ?
			0 : m_lfts->sadb_lifetime_allocations));
	}

	printf("\tsadb_seq=%lu pid=%lu ",
		(u_long)m->sadb_msg_seq,
		(u_long)m->sadb_msg_pid);

	/* XXX DEBUG */
	printf("refcnt=%u\n", m->sadb_msg_reserved);

	return;
}

void
pfkey_spdump(m)
	struct sadb_msg *m;
{
	char pbuf[NI_MAXSERV];
	caddr_t mhp[SADB_EXT_MAX + 1];
	struct sadb_address *m_saddr, *m_daddr;
	struct sadb_x_policy *m_xpl;
	struct sadb_lifetime *m_lft = NULL;
	struct sockaddr *sa;
	u_int16_t port;

	/* check pfkey message. */
	if (pfkey_align(m, mhp)) {
		printf("%s\n", ipsec_strerror());
		return;
	}
	if (pfkey_check(mhp)) {
		printf("%s\n", ipsec_strerror());
		return;
	}

	m_saddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];
	m_daddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	m_xpl = (struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];
	m_lft = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_HARD];

	/* source address */
	if (m_saddr == NULL) {
		printf("no ADDRESS_SRC extension.\n");
		return;
	}
	sa = (struct sockaddr *)(m_saddr + 1);
	switch (sa->sa_family) {
	case AF_INET:
	case AF_INET6:
		if (getnameinfo(sa, sa->sa_len, NULL, 0, pbuf, sizeof(pbuf),
		    NI_NUMERICSERV) != 0)
			port = 0;	/*XXX*/
		else
			port = atoi(pbuf);
		printf("%s%s ", str_ipaddr(sa),
			str_prefport(sa->sa_family,
			    m_saddr->sadb_address_prefixlen, port));
		break;
	default:
		printf("unknown-af ");
		break;
	}

	/* destination address */
	if (m_daddr == NULL) {
		printf("no ADDRESS_DST extension.\n");
		return;
	}
	sa = (struct sockaddr *)(m_daddr + 1);
	switch (sa->sa_family) {
	case AF_INET:
	case AF_INET6:
		if (getnameinfo(sa, sa->sa_len, NULL, 0, pbuf, sizeof(pbuf),
		    NI_NUMERICSERV) != 0)
			port = 0;	/*XXX*/
		else
			port = atoi(pbuf);
		printf("%s%s ", str_ipaddr(sa),
			str_prefport(sa->sa_family,
			    m_daddr->sadb_address_prefixlen, port));
		break;
	default:
		printf("unknown-af ");
		break;
	}

	/* upper layer protocol */
	if (m_saddr->sadb_address_proto != m_daddr->sadb_address_proto) {
		printf("upper layer protocol mismatched.\n");
		return;
	}
	if (m_saddr->sadb_address_proto == IPSEC_ULPROTO_ANY)
		printf("any");
	else
		GETMSGSTR(str_upper, m_saddr->sadb_address_proto);

	/* policy */
    {
	char *d_xpl;

	if (m_xpl == NULL) {
		printf("no X_POLICY extension.\n");
		return;
	}
	d_xpl = ipsec_dump_policy((char *)m_xpl, "\n\t");

	/* dump SPD */
	printf("\n\t%s\n", d_xpl);
	free(d_xpl);
    }

	/* lifetime */
	if (m_lft) {
		printf("\tlifetime:%lu validtime:%lu\n",
			(u_long)m_lft->sadb_lifetime_addtime,
			(u_long)m_lft->sadb_lifetime_usetime);
	}

	printf("\tspid=%ld seq=%ld pid=%ld\n",
		(u_long)m_xpl->sadb_x_policy_id,
		(u_long)m->sadb_msg_seq,
		(u_long)m->sadb_msg_pid);

	/* XXX TEST */
	printf("\trefcnt=%u\n", m->sadb_msg_reserved);

	return;
}

/*
 * set "ipaddress" to buffer.
 */
static char *
str_ipaddr(sa)
	struct sockaddr *sa;
{
	static char buf[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	const int niflag = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
	const int niflag = NI_NUMERICHOST;
#endif

	if (sa == NULL)
		return "";

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0, niflag) == 0)
		return buf;
	return NULL;
}

/*
 * set "/prefix[port number]" to buffer.
 */
static char *
str_prefport(family, pref, port)
	u_int family, pref, port;
{
	static char buf[128];
	char prefbuf[128];
	char portbuf[128];
	int plen;

	switch (family) {
	case AF_INET:
		plen = sizeof(struct in_addr) << 3;
		break;
	case AF_INET6:
		plen = sizeof(struct in6_addr) << 3;
		break;
	default:
		return "?";
	}

	if (pref == plen)
		prefbuf[0] = '\0';
	else
		snprintf(prefbuf, sizeof(prefbuf), "/%u", pref);

	if (port == IPSEC_PORT_ANY)
		snprintf(portbuf, sizeof(portbuf), "[%s]", "any");
	else
		snprintf(portbuf, sizeof(portbuf), "[%u]", port);

	snprintf(buf, sizeof(buf), "%s%s", prefbuf, portbuf);

	return buf;
}

/*
 * set "Mon Day Time Year" to buffer
 */
static char *
str_time(t)
	time_t t;
{
	static char buf[128];

	if (t == 0) {
		int i = 0;
		for (;i < 20;) buf[i++] = ' ';
	} else {
		char *t0;
		t0 = ctime(&t);
		memcpy(buf, t0 + 4, 20);
	}

	buf[20] = '\0';

	return(buf);
}

static void
str_lifetime_byte(x, str)
	struct sadb_lifetime *x;
	char *str;
{
	double y;
	char *unit;
	int w;

	if (x == NULL) {
		printf("\t%s: 0(bytes)", str);
		return;
	}

#if 0
	if ((x->sadb_lifetime_bytes) / 1024 / 1024) {
		y = (x->sadb_lifetime_bytes) * 1.0 / 1024 / 1024;
		unit = "M";
		w = 1;
	} else if ((x->sadb_lifetime_bytes) / 1024) {
		y = (x->sadb_lifetime_bytes) * 1.0 / 1024;
		unit = "K";
		w = 1;
	} else {
		y = (x->sadb_lifetime_bytes) * 1.0;
		unit = "";
		w = 0;
	}
#else
	y = (x->sadb_lifetime_bytes) * 1.0;
	unit = "";
	w = 0;
#endif
	printf("\t%s: %.*f(%sbytes)", str, w, y, unit);
}
