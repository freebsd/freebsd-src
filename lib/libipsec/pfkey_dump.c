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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet6/ipsec.h>
#include <net/pfkeyv2.h>
#include <netkey/key_var.h>
#include <netkey/key_debug.h>

#include <netinet/in.h>
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/in6.h>
#endif
#include <arpa/inet.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ipsec_strerror.h"

#define	GETMSGSTR(str, num) \
{ \
	if (sizeof((str)[0]) == 0 \
	 || num >= sizeof(str)/sizeof((str)[0])) \
		printf("%d ", (num)); \
	else if (strlen((str)[(num)]) == 0) \
		printf("%d ", (num)); \
	else \
		printf("%s ", (str)[(num)]); \
}

#define	GETAF(p) \
	(((struct sockaddr *)(p))->sa_family)

static char *_str_ipaddr __P((u_int family, caddr_t addr));
static char *_str_prefport __P((u_int family, u_int pref, u_int port));
static char *_str_time __P((time_t t));
static void _str_lifetime_byte __P((struct sadb_lifetime *x, char *str));

/*
 * Must to be re-written about following strings.
 */
static char *_str_satype[] = {
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

static char *_str_mode[] = {
	"any",
	"transport",
	"tunnel",
};

static char *_str_upper[] = {
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

static char *_str_state[] = {
	"larval",
	"mature",
	"dying",
	"dead",
};

static char *_str_alg_auth[] = {
	"none",
	"hmac-md5",
	"hmac-sha1",
	"md5",
	"sha",
	"null",
};

static char *_str_alg_enc[] = {
	"none",
	"des-cbc",
	"3des-cbc",
	"null",
	"blowfish-cbc",
	"cast128-cbc",
	"rc5-cbc",
};

static char *_str_alg_comp[] = {
	"none",
	"oui",
	"deflate",
	"lzs",
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
	m_lftc = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_CURRENT];
	m_lfth = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_HARD];
	m_lfts = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_SOFT];
	m_saddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];
	m_daddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	m_paddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_PROXY];
	m_auth = (struct sadb_key *)mhp[SADB_EXT_KEY_AUTH];
	m_enc = (struct sadb_key *)mhp[SADB_EXT_KEY_ENCRYPT];
	m_sid = (struct sadb_ident *)mhp[SADB_EXT_IDENTITY_SRC];
	m_did = (struct sadb_ident *)mhp[SADB_EXT_IDENTITY_SRC];
	m_sens = (struct sadb_sens *)mhp[SADB_EXT_SENSITIVITY];

	/* source address */
	if (m_saddr == NULL) {
		printf("no ADDRESS_SRC extension.\n");
		return;
	}
	printf("%s ",
		_str_ipaddr(GETAF(m_saddr + 1), _INADDRBYSA(m_saddr + 1)));

	/* destination address */
	if (m_daddr == NULL) {
		printf("no ADDRESS_DST extension.\n");
		return;
	}
	printf("%s ",
		_str_ipaddr(GETAF(m_daddr + 1), _INADDRBYSA(m_daddr + 1)));

	/* SA type */
	if (m_sa == NULL) {
		printf("no SA extension.\n");
		return;
	}
	printf("\n\t");

	GETMSGSTR(_str_satype, m->sadb_msg_satype);

	printf("mode=");
	GETMSGSTR(_str_mode, m->sadb_msg_mode);

	printf("spi=%u(0x%08x) replay=%u flags=0x%08x\n",
		(u_int32_t)ntohl(m_sa->sadb_sa_spi),
		(u_int32_t)ntohl(m_sa->sadb_sa_spi),
		m_sa->sadb_sa_replay,
		m_sa->sadb_sa_flags);

	/* encryption key */
	if (m->sadb_msg_satype == SADB_X_SATYPE_IPCOMP) {
		printf("\tC: ");
		GETMSGSTR(_str_alg_comp, m_sa->sadb_sa_encrypt);
	} else if (m->sadb_msg_satype == SADB_SATYPE_ESP) {
		if (m_enc != NULL) {
			printf("\tE: ");
			GETMSGSTR(_str_alg_enc, m_sa->sadb_sa_encrypt);
			ipsec_hexdump((caddr_t)m_enc + sizeof(*m_enc),
				      m_enc->sadb_key_bits / 8);
			printf("\n");
		}
	}

	/* authentication key */
	if (m_auth != NULL) {
		printf("\tA: ");
		GETMSGSTR(_str_alg_auth, m_sa->sadb_sa_auth);
		ipsec_hexdump((caddr_t)m_auth + sizeof(*m_auth),
		              m_auth->sadb_key_bits / 8);
		printf("\n");
	}

	/* state */
	printf("\tstate=");
	GETMSGSTR(_str_state, m_sa->sadb_sa_state);

	printf("seq=%lu pid=%lu\n",
		(u_long)m->sadb_msg_seq,
		(u_long)m->sadb_msg_pid);

	/* lifetime */
	if (m_lftc != NULL) {
		time_t tmp_time = time(0);

		printf("\tcreated: %s",
			_str_time(m_lftc->sadb_lifetime_addtime));
		printf("\tcurrent: %s\n", _str_time(tmp_time));
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
			_str_time(m_lftc->sadb_lifetime_usetime));
		printf("\thard: %lu(s)",
			(u_long)(m_lfth == NULL ?
			0 : m_lfth->sadb_lifetime_usetime));
		printf("\tsoft: %lu(s)\n",
			(u_long)(m_lfts == NULL ?
			0 : m_lfts->sadb_lifetime_usetime));

		_str_lifetime_byte(m_lftc, "current");
		_str_lifetime_byte(m_lfth, "hard");
		_str_lifetime_byte(m_lfts, "soft");
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

	/* XXX DEBUG */
	printf("\trefcnt=%d\n", m->sadb_msg_reserved);

	return;
}

void
pfkey_spdump(m)
	struct sadb_msg *m;
{
	caddr_t mhp[SADB_EXT_MAX + 1];
	struct sadb_address *m_saddr, *m_daddr;
	struct sadb_x_policy *m_xpl;

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

	/* source address */
	if (m_saddr == NULL) {
		printf("no ADDRESS_SRC extension.\n");
		return;
	}
	printf("%s%s ",
		_str_ipaddr(GETAF(m_saddr + 1), _INADDRBYSA(m_saddr + 1)),
		_str_prefport(GETAF(m_saddr + 1),
		     m_saddr->sadb_address_prefixlen,
		     _INPORTBYSA(m_saddr + 1)));

	/* destination address */
	if (m_daddr == NULL) {
		printf("no ADDRESS_DST extension.\n");
		return;
	}
	printf("%s%s ",
		_str_ipaddr(GETAF(m_daddr + 1), _INADDRBYSA(m_daddr + 1)),
		_str_prefport(GETAF(m_daddr + 1),
		     m_daddr->sadb_address_prefixlen,
		     _INPORTBYSA(m_daddr + 1)));

	/* upper layer protocol */
	if (m_saddr->sadb_address_proto != m_saddr->sadb_address_proto) {
		printf("upper layer protocol mismatched.\n");
		return;
	}
	if (m_saddr->sadb_address_proto == IPSEC_ULPROTO_ANY)
		printf("any");
	else
		GETMSGSTR(_str_upper, m_saddr->sadb_address_proto);

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

	printf("\tseq=%ld pid=%ld\n",
		(u_long)m->sadb_msg_seq,
		(u_long)m->sadb_msg_pid);

	/* XXX TEST */
	printf("\trefcnt=%d\n", m->sadb_msg_reserved);

	return;
}

/*
 * set "ipaddress" to buffer.
 */
static char *
_str_ipaddr(family, addr)
	u_int family;
	caddr_t addr;
{
	static char buf[128];
	char addrbuf[128];

	if (addr == NULL)
		return "";

	inet_ntop(family, addr, addrbuf, sizeof(addrbuf));

	snprintf(buf, sizeof(buf), "%s", addrbuf);

	return buf;
}

/*
 * set "/prefix[port number]" to buffer.
 */
static char *
_str_prefport(family, pref, port)
	u_int family, pref, port;
{
	static char buf[128];
	char prefbuf[10];
	char portbuf[10];

	if (pref == (_INALENBYAF(family) << 3))
		prefbuf[0] = '\0';
	else
		snprintf(prefbuf, sizeof(prefbuf), "/%u", pref);

	if (port == IPSEC_PORT_ANY)
		snprintf(portbuf, sizeof(portbuf), "[%s]", "any");
	else
		snprintf(portbuf, sizeof(portbuf), "[%u]", ntohs(port));

	snprintf(buf, sizeof(buf), "%s%s", prefbuf, portbuf);

	return buf;
}

/*
 * set "Mon Day Time Year" to buffer
 */
static char *
_str_time(t)
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
_str_lifetime_byte(x, str)
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

	y = (x->sadb_lifetime_bytes) * 1.0;
	unit = "";
	w = 0;
	printf("\t%s: %.*f(%sbytes)", str, w, y, unit);
}
