/*	$FreeBSD: src/usr.sbin/setkey/parse.y,v 1.1.2.1 2000/07/15 07:37:03 kris Exp $	*/
/*	$KAME: parse.y,v 1.29 2000/06/10 14:17:44 sakane Exp $	*/

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

%{
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <net/route.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>
#include <netkey/key_var.h>
#include <netinet6/ipsec.h>
#include <arpa/inet.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>

#include "libpfkey.h"
#include "vchar.h"

#define ATOX(c) \
  (isdigit(c) ? (c - '0') : (isupper(c) ? (c - 'A' + 10) : (c - 'a' + 10) ))

u_int p_type;
u_int32_t p_spi;
struct sockaddr *p_src, *p_dst;
u_int p_prefs, p_prefd, p_upper;
u_int p_satype, p_ext, p_alg_enc, p_alg_auth, p_replay, p_mode;
u_int32_t p_reqid;
u_int p_key_enc_len, p_key_auth_len;
caddr_t p_key_enc, p_key_auth;
time_t p_lt_hard, p_lt_soft;

u_int p_policy_len;
char *p_policy;

/* temporary buffer */
static struct sockaddr *pp_addr;
static u_int pp_prefix;
static u_int pp_port;
static caddr_t pp_key;

extern u_char m_buf[BUFSIZ];
extern int m_len;
extern char cmdarg[8192];
extern int f_debug;

int setkeymsg __P((void));
static struct addrinfo *parse_addr __P((char *, char *, int));
static int setvarbuf __P((int *, struct sadb_ext *, int, caddr_t, int));
void parse_init __P((void));
void free_buffer __P((void));

extern int setkeymsg __P((void));
extern int sendkeymsg __P((void));

extern int yylex __P((void));
extern void yyfatal __P((const char *));
extern void yyerror __P((const char *));
%}

%union {
	unsigned long num;
	vchar_t val;
}

%token EOT
%token ADD GET DELETE FLUSH DUMP
%token ADDRESS PREFIX PORT PORTANY
%token UP_PROTO PR_ESP PR_AH PR_IPCOMP
%token F_PROTOCOL F_AUTH F_ENC F_REPLAY F_COMP F_RAWCPI
%token F_MODE MODE F_REQID
%token F_EXT EXTENSION NOCYCLICSEQ
%token ALG_AUTH ALG_ENC ALG_ENC_DESDERIV ALG_ENC_DES32IV ALG_COMP
%token F_LIFETIME_HARD F_LIFETIME_SOFT
%token DECSTRING QUOTEDSTRING HEXSTRING ANY
	/* SPD management */
%token SPDADD SPDDELETE SPDDUMP SPDFLUSH
%token F_POLICY PL_REQUESTS

%type <num> PORT PREFIX EXTENSION MODE
%type <num> UP_PROTO PR_ESP PR_AH PR_IPCOMP
%type <num> ALG_AUTH ALG_ENC ALG_ENC_DESDERIV ALG_ENC_DES32IV ALG_COMP
%type <num> DECSTRING
%type <val> ADDRESS PL_REQUESTS
%type <val> key_string policy_requests
%type <val> QUOTEDSTRING HEXSTRING

%%
commands
	:	/*NOTHING*/
	|	commands command
		{
			if (f_debug) {
				printf("cmdarg:\n%s\n", cmdarg);
			} else {
				setkeymsg();
				sendkeymsg();
			}
			free_buffer();
			parse_init();
		}
	;

command
	:	add_command
	|	get_command
	|	delete_command
	|	flush_command
	|	dump_command
	|	spdadd_command
	|	spddelete_command
	|	spddump_command
	|	spdflush_command
	;
	/* commands concerned with management, there is in tail of this file. */

	/* add command */
add_command
	:	ADD { p_type = SADB_ADD; }
		sa_selector_spec extension_spec algorithm_spec EOT
	;

	/* delete */
delete_command
	:	DELETE { p_type = SADB_DELETE; }
		sa_selector_spec extension_spec
		{
			if (p_mode != IPSEC_MODE_ANY)
				yyerror("WARNING: mode is obsoleted.");
		}
		EOT
	;

	/* get command */
get_command
	:	GET { p_type = SADB_GET; }
		sa_selector_spec extension_spec
		{
			if (p_mode != IPSEC_MODE_ANY)
				yyerror("WARNING: mode is obsoleted.");
		}
		EOT
	;

	/* flush */
flush_command
	:	FLUSH { p_type = SADB_FLUSH; }
		protocol_spec EOT
	;

	/* dump */
dump_command
	:	DUMP { p_type = SADB_DUMP; }
		protocol_spec EOT
	;

	/* sa_selector_spec */
sa_selector_spec
	:	ipaddress { p_src = pp_addr; }
		ipaddress { p_dst = pp_addr; }
		protocol_spec spi
	;

protocol_spec
	:	/*NOTHING*/ { p_satype = SADB_SATYPE_UNSPEC; }
	|	PR_ESP
		{
			p_satype = SADB_SATYPE_ESP;
			if ($1 == 1)
				p_ext |= SADB_X_EXT_OLD;
			else
				p_ext &= ~SADB_X_EXT_OLD;
		}
	|	PR_AH
		{
			p_satype = SADB_SATYPE_AH;
			if ($1 == 1)
				p_ext |= SADB_X_EXT_OLD;
			else
				p_ext &= ~SADB_X_EXT_OLD;
		}
	|	PR_IPCOMP
		{
			p_satype = SADB_X_SATYPE_IPCOMP;
		}
	;
	
spi
	:	DECSTRING { p_spi = $1; }
	|	HEXSTRING
		{
			caddr_t bp;
			caddr_t yp = $1.buf;
			char buf0[4], buf[4];
			int i, j;

			/* sanity check */
			if ($1.len > 4) {
				yyerror("SPI too big.");
				free($1.buf);
				return -1;
			}

			bp = buf0;
			while (*yp) {
				*bp = (ATOX(yp[0]) << 4) | ATOX(yp[1]);
				yp += 2, bp++;
			}

			/* initialize */
			for (i = 0; i < 4; i++) buf[i] = 0;

			for (j = $1.len - 1, i = 3; j >= 0; j--, i--)
				buf[i] = buf0[j];

			/* XXX: endian */
			p_spi = ntohl(*(u_int32_t *)buf);

			free($1.buf);
		}
	;

algorithm_spec
	:	esp_spec
	|	ah_spec
	|	ipcomp_spec
	;

esp_spec
	:	F_ENC enc_alg enc_key F_AUTH auth_alg auth_key
	|	F_ENC enc_alg enc_key
	;

ah_spec
	:	F_AUTH auth_alg auth_key
	;

ipcomp_spec
	:	F_COMP ALG_COMP { p_alg_enc = $2; }
	|	F_COMP ALG_COMP { p_alg_enc = $2; }
		F_RAWCPI { p_ext |= SADB_X_EXT_RAWCPI; }
	;

enc_alg
	:	ALG_ENC { p_alg_enc = $1; }
	|	ALG_ENC_DESDERIV
		{
			p_alg_enc = $1;
			if (p_ext & SADB_X_EXT_OLD) {
				yyerror("algorithm mismatched.");
				return -1;
			}
			p_ext |= SADB_X_EXT_DERIV;
		}
	|	ALG_ENC_DES32IV
		{
			p_alg_enc = $1;
			if (!(p_ext & SADB_X_EXT_OLD)) {
				yyerror("algorithm mismatched.");
				return -1;
			}
			p_ext |= SADB_X_EXT_IV4B;
		}
	;

enc_key
	:	/*NOTHING*/
		{
			if (p_alg_enc != SADB_EALG_NULL) {
				yyerror("no key found.");
				return -1;
			}
		}
	|	key_string
		{
			p_key_enc_len = $1.len;
			p_key_enc = pp_key;

			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_ENCRYPT,
					p_alg_enc,
					PFKEY_UNUNIT64(p_key_enc_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
	;

auth_alg
	:	ALG_AUTH { p_alg_auth = $1; }
	;

auth_key
	:	/*NOTHING*/
		{
			if (p_alg_auth != SADB_AALG_NULL) {
				yyerror("no key found.");
				return -1;
			}
		}
	|	key_string
		{
			p_key_auth_len = $1.len;
			p_key_auth = pp_key;

			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_AUTH,
					p_alg_auth,
					PFKEY_UNUNIT64(p_key_auth_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
	;

key_string
	:	QUOTEDSTRING
		{
			pp_key = $1.buf;
			/* free pp_key later */
		}
	|	HEXSTRING
		{
			caddr_t bp;
			caddr_t yp = $1.buf;

			if ((pp_key = malloc($1.len)) == 0) {
				free($1.buf);
				yyerror("not enough core");
				return -1;
			}
			memset(pp_key, 0, $1.len);

			bp = pp_key;
			while (*yp) {
				*bp = (ATOX(yp[0]) << 4) | ATOX(yp[1]);
				yp += 2, bp++;
			}

			free($1.buf);
		}
	;

extension_spec
	:	/*NOTHING*/
	|	extension_spec extension
	;

extension
	:	F_EXT EXTENSION { p_ext |= $2; }
	|	F_EXT NOCYCLICSEQ { p_ext &= ~SADB_X_EXT_CYCSEQ; }
	|	F_MODE MODE { p_mode = $2; }
	|	F_MODE ANY { p_mode = IPSEC_MODE_ANY; }
	|	F_REQID DECSTRING { p_reqid = $2; }
	|	F_REPLAY DECSTRING
		{
			if (p_ext & SADB_X_EXT_OLD) {
				yyerror("replay prevention "
				        "only use on new spec.");
				return -1;
			}
			p_replay = $2;
		}
	|	F_LIFETIME_HARD DECSTRING { p_lt_hard = $2; }
	|	F_LIFETIME_SOFT DECSTRING { p_lt_soft = $2; }
	;

	/* definition about command for SPD management */
	/* spdadd */
spdadd_command
	:	SPDADD
		{
			p_type = SADB_X_SPDADD;
			p_satype = SADB_SATYPE_UNSPEC;
		}
		sp_selector_spec policy_spec EOT
	;

spddelete_command:
		SPDDELETE
		{
			p_type = SADB_X_SPDDELETE;
			p_satype = SADB_SATYPE_UNSPEC;
		}
		sp_selector_spec policy_spec EOT
	;

spddump_command:
		SPDDUMP
		{
			p_type = SADB_X_SPDDUMP;
			p_satype = SADB_SATYPE_UNSPEC;
		}
		EOT
	;

spdflush_command:
		SPDFLUSH
		{
			p_type = SADB_X_SPDFLUSH;
			p_satype = SADB_SATYPE_UNSPEC;
		}
		EOT
	;

	/* sp_selector_spec */
sp_selector_spec
	:	ipaddress { p_src = pp_addr; }
		prefix { p_prefs = pp_prefix; }
		port
		{
			switch (p_src->sa_family) {
			case AF_INET:
				((struct sockaddr_in *)p_src)->sin_port =
				    htons(pp_port);
				break;
#ifdef INET6
			case AF_INET6:
				((struct sockaddr_in6 *)p_src)->sin6_port =
				    htons(pp_port);
				break;
#endif
			default:
				exit(1); /*XXX*/
			}
		}
		ipaddress { p_dst = pp_addr; }
		prefix { p_prefd = pp_prefix; }
		port
		{
			switch (p_dst->sa_family) {
			case AF_INET:
				((struct sockaddr_in *)p_dst)->sin_port =
				    htons(pp_port);
				break;
#ifdef INET6
			case AF_INET6:
				((struct sockaddr_in6 *)p_dst)->sin6_port =
				    htons(pp_port);
				break;
#endif
			default:
				exit(1); /*XXX*/
			}
		}
		upper_spec
		{
			/* XXX is it something userland should check? */
#if 0
			switch (p_upper) {
			case IPPROTO_ICMP:
			case IPPROTO_ICMPV6:
				if (_INPORTBYSA(p_src) != IPSEC_PORT_ANY
				 || _INPORTBYSA(p_dst) != IPSEC_PORT_ANY) {
					yyerror("port number must be \"any\".");
					return -1;
				}
				if ((pp_addr->sa_family == AF_INET6
				  && p_upper == IPPROTO_ICMP)
				 || (pp_addr->sa_family == AF_INET
				  && p_upper == IPPROTO_ICMPV6)) {
					yyerror("upper layer protocol "
						"mismatched.\n");
					return -1;
				}
				break;
			default:
				break;
			}
#endif
		}
	;

ipaddress
	:	ADDRESS
		{
			struct addrinfo *res;

			res = parse_addr($1.buf, NULL, AI_NUMERICHOST);
			if (res == NULL) {
				free($1.buf);
				return -1;
			}
			pp_addr = (struct sockaddr *)malloc(res->ai_addrlen);
			if (!pp_addr) {
				yyerror("not enough core");
				goto end;
			}

			memcpy(pp_addr, res->ai_addr, res->ai_addrlen);
		    end:
			freeaddrinfo(res);
			free($1.buf);
		}
	;

prefix
	:	/*NOTHING*/ { pp_prefix = ~0; }
	|	PREFIX { pp_prefix = $1; }
	;

port
	:	/*NOTHING*/ { pp_port = IPSEC_PORT_ANY; }
	|	PORT { pp_port = $1; }
	|	PORTANY { pp_port = IPSEC_PORT_ANY; }
	;

upper_spec
	:	DECSTRING { p_upper = $1; }
	|	UP_PROTO { p_upper = $1; }
	|	PR_ESP { p_upper = IPPROTO_ESP; };
	|	PR_AH { p_upper = IPPROTO_AH; };
	|	PR_IPCOMP { p_upper = IPPROTO_IPCOMP; };
	|	ANY { p_upper = IPSEC_ULPROTO_ANY; }
	;

policy_spec
	:	F_POLICY policy_requests
		{
			p_policy = ipsec_set_policy($2.buf, $2.len);
			if (p_policy == NULL) {
				free($2.buf);
				p_policy = NULL;
				yyerror(ipsec_strerror());
				return -1;
			}

			p_policy_len = ipsec_get_policylen(p_policy);

			free($2.buf);
		}
	;

policy_requests
	:	PL_REQUESTS { $$ = $1; }
	;

%%

int
setkeymsg()
{
	struct sadb_msg m_msg;

	m_msg.sadb_msg_version = PF_KEY_V2;
	m_msg.sadb_msg_type = p_type;
	m_msg.sadb_msg_errno = 0;
	m_msg.sadb_msg_satype = p_satype;
	m_msg.sadb_msg_reserved = 0;
	m_msg.sadb_msg_seq = 0;
	m_msg.sadb_msg_pid = getpid();

	m_len = sizeof(struct sadb_msg);
	memcpy(m_buf, &m_msg, m_len);

	switch (p_type) {
	case SADB_FLUSH:
	case SADB_DUMP:
		break;

	case SADB_ADD:
		/* set encryption algorithm, if present. */
		if (p_satype != SADB_X_SATYPE_IPCOMP && p_alg_enc != SADB_EALG_NONE) {
			struct sadb_key m_key;

			m_key.sadb_key_len =
				PFKEY_UNIT64(sizeof(m_key)
				           + PFKEY_ALIGN8(p_key_enc_len));
			m_key.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
			m_key.sadb_key_bits = p_key_enc_len * 8;
			m_key.sadb_key_reserved = 0;

			setvarbuf(&m_len,
				(struct sadb_ext *)&m_key, sizeof(m_key),
				(caddr_t)p_key_enc, p_key_enc_len);
		}

		/* set authentication algorithm, if present. */
		if (p_alg_auth != SADB_AALG_NONE) {
			struct sadb_key m_key;

			m_key.sadb_key_len =
				PFKEY_UNIT64(sizeof(m_key)
				           + PFKEY_ALIGN8(p_key_auth_len));
			m_key.sadb_key_exttype = SADB_EXT_KEY_AUTH;
			m_key.sadb_key_bits = p_key_auth_len * 8;
			m_key.sadb_key_reserved = 0;

			setvarbuf(&m_len,
				(struct sadb_ext *)&m_key, sizeof(m_key),
				(caddr_t)p_key_auth, p_key_auth_len);
		}

		/* set lifetime for HARD */
		if (p_lt_hard != 0) {
			struct sadb_lifetime m_lt;
			u_int len = sizeof(struct sadb_lifetime);

			m_lt.sadb_lifetime_len = PFKEY_UNIT64(len);
			m_lt.sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
			m_lt.sadb_lifetime_allocations = 0;
			m_lt.sadb_lifetime_bytes = 0;
			m_lt.sadb_lifetime_addtime = p_lt_hard;
			m_lt.sadb_lifetime_usetime = 0;

			memcpy(m_buf + m_len, &m_lt, len);
			m_len += len;
		}

		/* set lifetime for SOFT */
		if (p_lt_soft != 0) {
			struct sadb_lifetime m_lt;
			u_int len = sizeof(struct sadb_lifetime);

			m_lt.sadb_lifetime_len = PFKEY_UNIT64(len);
			m_lt.sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
			m_lt.sadb_lifetime_allocations = 0;
			m_lt.sadb_lifetime_bytes = 0;
			m_lt.sadb_lifetime_addtime = p_lt_soft;
			m_lt.sadb_lifetime_usetime = 0;

			memcpy(m_buf + m_len, &m_lt, len);
			m_len += len;
		}
		/* FALLTHROUGH */

	case SADB_DELETE:
	case SADB_GET:
	    {
		struct sadb_sa m_sa;
		struct sadb_x_sa2 m_sa2;
		struct sadb_address m_addr;
		u_int len;

		len = sizeof(struct sadb_sa);
		m_sa.sadb_sa_len = PFKEY_UNIT64(len);
		m_sa.sadb_sa_exttype = SADB_EXT_SA;
		m_sa.sadb_sa_spi = htonl(p_spi);
		m_sa.sadb_sa_replay = p_replay;
		m_sa.sadb_sa_state = 0;
		m_sa.sadb_sa_auth = p_alg_auth;
		m_sa.sadb_sa_encrypt = p_alg_enc;
		m_sa.sadb_sa_flags = p_ext;

		memcpy(m_buf + m_len, &m_sa, len);
		m_len += len;

		len = sizeof(struct sadb_x_sa2);
		m_sa2.sadb_x_sa2_len = PFKEY_UNIT64(len);
		m_sa2.sadb_x_sa2_exttype = SADB_X_EXT_SA2;
		m_sa2.sadb_x_sa2_mode = p_mode;
		m_sa2.sadb_x_sa2_reqid = p_reqid;

		memcpy(m_buf + m_len, &m_sa2, len);
		m_len += len;

		/* set src */
		m_addr.sadb_address_len =
			PFKEY_UNIT64(sizeof(m_addr)
			           + PFKEY_ALIGN8(p_src->sa_len));
		m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
		m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
		switch (p_src->sa_family) {
		case AF_INET:
			m_addr.sadb_address_prefixlen =
			    sizeof(struct in_addr) << 3;
			break;
#ifdef INET6
		case AF_INET6:
			m_addr.sadb_address_prefixlen =
			    sizeof(struct in6_addr) << 3;
			break;
#endif
		default:
			yyerror("unsupported address family");
			exit(1);	/*XXX*/
		}
		m_addr.sadb_address_reserved = 0;

		setvarbuf(&m_len,
			(struct sadb_ext *)&m_addr, sizeof(m_addr),
			(caddr_t)p_src, p_src->sa_len);

		/* set dst */
		m_addr.sadb_address_len =
			PFKEY_UNIT64(sizeof(m_addr)
			           + PFKEY_ALIGN8(p_dst->sa_len));
		m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
		m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
		switch (p_dst->sa_family) {
		case AF_INET:
			m_addr.sadb_address_prefixlen =
			    sizeof(struct in_addr) << 3;
			break;
#ifdef INET6
		case AF_INET6:
			m_addr.sadb_address_prefixlen =
			    sizeof(struct in6_addr) << 3;
			break;
#endif
		default:
			yyerror("unsupported address family");
			exit(1);	/*XXX*/
		}
		m_addr.sadb_address_reserved = 0;

		setvarbuf(&m_len,
			(struct sadb_ext *)&m_addr, sizeof(m_addr),
			(caddr_t)p_dst, p_dst->sa_len);
	    }
		break;

	/* for SPD management */
	case SADB_X_SPDFLUSH:
	case SADB_X_SPDDUMP:
		break;

	case SADB_X_SPDADD:
	case SADB_X_SPDDELETE:
	    {
		struct sadb_address m_addr;
		u_int8_t plen;

		memcpy(m_buf + m_len, p_policy, p_policy_len);
		m_len += p_policy_len;
		free(p_policy);
		p_policy = NULL;

		/* set src */
		m_addr.sadb_address_len =
			PFKEY_UNIT64(sizeof(m_addr)
			           + PFKEY_ALIGN8(p_src->sa_len));
		m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
		m_addr.sadb_address_proto = p_upper;
		switch (p_src->sa_family) {
		case AF_INET:
			plen = sizeof(struct in_addr) << 3;
			break;
#ifdef INET6
		case AF_INET6:
			plen = sizeof(struct in6_addr) << 3;
			break;
#endif
		default:
			yyerror("unsupported address family");
			exit(1);	/*XXX*/
		}
		m_addr.sadb_address_prefixlen =
		    (p_prefs != ~0 ? p_prefs : plen);
		m_addr.sadb_address_reserved = 0;

		setvarbuf(&m_len,
			(struct sadb_ext *)&m_addr, sizeof(m_addr),
			(caddr_t)p_src, p_src->sa_len);

		/* set dst */
		m_addr.sadb_address_len =
			PFKEY_UNIT64(sizeof(m_addr)
			           + PFKEY_ALIGN8(p_dst->sa_len));
		m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
		m_addr.sadb_address_proto = p_upper;
		switch (p_dst->sa_family) {
		case AF_INET:
			plen = sizeof(struct in_addr) << 3;
			break;
#ifdef INET6
		case AF_INET6:
			plen = sizeof(struct in6_addr) << 3;
			break;
#endif
		default:
			yyerror("unsupported address family");
			exit(1);	/*XXX*/
		}
		m_addr.sadb_address_prefixlen =
		    (p_prefd != ~0 ? p_prefd : plen);
		m_addr.sadb_address_reserved = 0;

		setvarbuf(&m_len,
			(struct sadb_ext *)&m_addr, sizeof(m_addr),
			(caddr_t)p_dst, p_dst->sa_len);
	    }
		break;
	}

	((struct sadb_msg *)m_buf)->sadb_msg_len = PFKEY_UNIT64(m_len);

	return 0;
}

static struct addrinfo *
parse_addr(host, port, flag)
	char *host;
	char *port;
	int flag;
{
	struct addrinfo hints, *res = NULL;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = flag;
	error = getaddrinfo(host, port, &hints, &res);
	if (error != 0) {
		yyerror(gai_strerror(error));
		return NULL;
	}
	if (res->ai_next != NULL) {
		yyerror(gai_strerror(error));
	}
	return res;
}

static int
setvarbuf(off, ebuf, elen, vbuf, vlen)
	caddr_t vbuf;
	struct sadb_ext *ebuf;
	int *off, elen, vlen;
{
	memset(m_buf + *off, 0, PFKEY_UNUNIT64(ebuf->sadb_ext_len));
	memcpy(m_buf + *off, (caddr_t)ebuf, elen);
	memcpy(m_buf + *off + elen, vbuf, vlen);
	(*off) += PFKEY_ALIGN8(elen + vlen);

	return 0;
}

void
parse_init()
{
	p_type = 0;
	p_spi = 0;

	p_src = 0, p_dst = 0;
	pp_prefix = p_prefs = p_prefd = ~0;
	pp_port = IPSEC_PORT_ANY;
	p_upper = 0;

	p_satype = 0;
	p_ext = SADB_X_EXT_CYCSEQ;
	p_alg_enc = SADB_EALG_NONE;
	p_alg_auth = SADB_AALG_NONE;
	p_mode = IPSEC_MODE_ANY;
	p_reqid = 0;
	p_replay = 0;
	p_key_enc_len = p_key_auth_len = 0;
	p_key_enc = p_key_auth = 0;
	p_lt_hard = p_lt_soft = 0;

	p_policy_len = 0;
	p_policy = NULL;

	memset(cmdarg, 0, sizeof(cmdarg));

	return;
}

void
free_buffer()
{
	if (p_src) free(p_src);
	if (p_dst) free(p_dst);
	if (p_key_enc) free(p_key_enc);
	if (p_key_auth) free(p_key_auth);

	return;
}

