/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: resolve.h 14773 2005-04-12 11:29:18Z lha $ */

#ifndef __RESOLVE_H__
#define __RESOLVE_H__

#ifndef ROKEN_LIB_FUNCTION
#ifdef _WIN32
#define ROKEN_LIB_FUNCTION _stdcall
#else
#define ROKEN_LIB_FUNCTION
#endif
#endif

typedef enum {
	rk_ns_t_invalid = 0,	/* Cookie. */
	rk_ns_t_a = 1,		/* Host address. */
	rk_ns_t_ns = 2,		/* Authoritative server. */
	rk_ns_t_md = 3,		/* Mail destination. */
	rk_ns_t_mf = 4,		/* Mail forwarder. */
	rk_ns_t_cname = 5,	/* Canonical name. */
	rk_ns_t_soa = 6,	/* Start of authority zone. */
	rk_ns_t_mb = 7,		/* Mailbox domain name. */
	rk_ns_t_mg = 8,		/* Mail group member. */
	rk_ns_t_mr = 9,		/* Mail rename name. */
	rk_ns_t_null = 10,	/* Null resource record. */
	rk_ns_t_wks = 11,	/* Well known service. */
	rk_ns_t_ptr = 12,	/* Domain name pointer. */
	rk_ns_t_hinfo = 13,	/* Host information. */
	rk_ns_t_minfo = 14,	/* Mailbox information. */
	rk_ns_t_mx = 15,	/* Mail routing information. */
	rk_ns_t_txt = 16,	/* Text strings. */
	rk_ns_t_rp = 17,	/* Responsible person. */
	rk_ns_t_afsdb = 18,	/* AFS cell database. */
	rk_ns_t_x25 = 19,	/* X_25 calling address. */
	rk_ns_t_isdn = 20,	/* ISDN calling address. */
	rk_ns_t_rt = 21,	/* Router. */
	rk_ns_t_nsap = 22,	/* NSAP address. */
	rk_ns_t_nsap_ptr = 23,	/* Reverse NSAP lookup (deprecated). */
	rk_ns_t_sig = 24,	/* Security signature. */
	rk_ns_t_key = 25,	/* Security key. */
	rk_ns_t_px = 26,	/* X.400 mail mapping. */
	rk_ns_t_gpos = 27,	/* Geographical position (withdrawn). */
	rk_ns_t_aaaa = 28,	/* Ip6 Address. */
	rk_ns_t_loc = 29,	/* Location Information. */
	rk_ns_t_nxt = 30,	/* Next domain (security). */
	rk_ns_t_eid = 31,	/* Endpoint identifier. */
	rk_ns_t_nimloc = 32,	/* Nimrod Locator. */
	rk_ns_t_srv = 33,	/* Server Selection. */
	rk_ns_t_atma = 34,	/* ATM Address */
	rk_ns_t_naptr = 35,	/* Naming Authority PoinTeR */
	rk_ns_t_kx = 36,	/* Key Exchange */
	rk_ns_t_cert = 37,	/* Certification record */
	rk_ns_t_a6 = 38,	/* IPv6 address (deprecates AAAA) */
	rk_ns_t_dname = 39,	/* Non-terminal DNAME (for IPv6) */
	rk_ns_t_sink = 40,	/* Kitchen sink (experimentatl) */
	rk_ns_t_opt = 41,	/* EDNS0 option (meta-RR) */
	rk_ns_t_apl = 42,	/* Address prefix list (RFC 3123) */
	rk_ns_t_ds = 43,	/* Delegation Signer (RFC 3658) */
	rk_ns_t_sshfp = 44,	/* SSH fingerprint */
	rk_ns_t_tkey = 249,	/* Transaction key */
	rk_ns_t_tsig = 250,	/* Transaction signature. */
	rk_ns_t_ixfr = 251,	/* Incremental zone transfer. */
	rk_ns_t_axfr = 252,	/* Transfer zone of authority. */
	rk_ns_t_mailb = 253,	/* Transfer mailbox records. */
	rk_ns_t_maila = 254,	/* Transfer mail agent records. */
	rk_ns_t_any = 255,	/* Wildcard match. */
	rk_ns_t_zxfr = 256,	/* BIND-specific, nonstandard. */
	rk_ns_t_max = 65536
} rk_ns_type;

/* We use these, but they are not always present in <arpa/nameser.h> */

#ifndef C_IN
#define C_IN		1
#endif

#ifndef T_A
#define T_A		1
#endif
#ifndef T_NS
#define T_NS		2
#endif
#ifndef T_CNAME
#define T_CNAME		5
#endif
#ifndef T_SOA
#define T_SOA		5
#endif
#ifndef T_PTR
#define T_PTR		12
#endif
#ifndef T_MX
#define T_MX		15
#endif
#ifndef T_TXT
#define T_TXT		16
#endif
#ifndef T_AFSDB
#define T_AFSDB		18
#endif
#ifndef T_SIG
#define T_SIG		24
#endif
#ifndef T_KEY
#define T_KEY		25
#endif
#ifndef T_AAAA
#define T_AAAA		28
#endif
#ifndef T_SRV
#define T_SRV		33
#endif
#ifndef T_NAPTR
#define T_NAPTR		35
#endif
#ifndef T_CERT
#define T_CERT		37
#endif
#ifndef T_SSHFP
#define T_SSHFP		44
#endif

#ifndef MAXDNAME
#define MAXDNAME	1025
#endif

#define dns_query		rk_dns_query
#define mx_record		rk_mx_record
#define srv_record		rk_srv_record
#define key_record		rk_key_record
#define sig_record		rk_sig_record
#define cert_record		rk_cert_record
#define sshfp_record		rk_sshfp_record
#define resource_record		rk_resource_record
#define dns_reply		rk_dns_reply

#define dns_lookup		rk_dns_lookup
#define dns_free_data		rk_dns_free_data
#define dns_string_to_type	rk_dns_string_to_type
#define dns_type_to_string	rk_dns_type_to_string
#define dns_srv_order		rk_dns_srv_order

struct dns_query{
    char *domain;
    unsigned type;
    unsigned class;
};

struct mx_record{
    unsigned  preference;
    char domain[1];
};

struct srv_record{
    unsigned priority;
    unsigned weight;
    unsigned port;
    char target[1];
};

struct key_record {
    unsigned flags;
    unsigned protocol;
    unsigned algorithm;
    size_t   key_len;
    u_char   key_data[1];
};

struct sig_record {
    unsigned type;
    unsigned algorithm;
    unsigned labels;
    unsigned orig_ttl;
    unsigned sig_expiration;
    unsigned sig_inception;
    unsigned key_tag;
    char     *signer;
    unsigned sig_len;
    char     sig_data[1];	/* also includes signer */
};

struct cert_record {
    unsigned type;
    unsigned tag;
    unsigned algorithm;
    size_t   cert_len;
    u_char   cert_data[1];
};

struct sshfp_record {
    unsigned algorithm;
    unsigned type;
    size_t   sshfp_len;
    u_char   sshfp_data[1];
};

struct ds_record {
    unsigned key_tag;
    unsigned algorithm;
    unsigned digest_type;
    unsigned digest_len;
    u_char digest_data[1];
};

struct resource_record{
    char *domain;
    unsigned type;
    unsigned class;
    unsigned ttl;
    unsigned size;
    union {
	void *data;
	struct mx_record *mx;
	struct mx_record *afsdb; /* mx and afsdb are identical */
	struct srv_record *srv;
	struct in_addr *a;
	char *txt;
	struct key_record *key;
	struct cert_record *cert;
	struct sig_record *sig;
	struct sshfp_record *sshfp;
	struct ds_record *ds;
    }u;
    struct resource_record *next;
};

#define rk_DNS_MAX_PACKET_SIZE		0xffff

struct dns_header {
    unsigned id;
    unsigned flags;
#define rk_DNS_HEADER_RESPONSE_FLAG		1
#define rk_DNS_HEADER_AUTHORITIVE_ANSWER	2
#define rk_DNS_HEADER_TRUNCATED_MESSAGE		4
#define rk_DNS_HEADER_RECURSION_DESIRED		8
#define rk_DNS_HEADER_RECURSION_AVAILABLE	16
#define rk_DNS_HEADER_AUTHENTIC_DATA		32
#define rk_DNS_HEADER_CHECKING_DISABLED		64
    unsigned opcode;
    unsigned response_code;
    unsigned qdcount;
    unsigned ancount;
    unsigned nscount;
    unsigned arcount;
};

struct dns_reply{
    struct dns_header h;
    struct dns_query q;
    struct resource_record *head;
};


struct dns_reply* ROKEN_LIB_FUNCTION
	dns_lookup(const char *, const char *);
void ROKEN_LIB_FUNCTION
	dns_free_data(struct dns_reply *);
int ROKEN_LIB_FUNCTION
	dns_string_to_type(const char *name);
const char *ROKEN_LIB_FUNCTION
	dns_type_to_string(int type);
void ROKEN_LIB_FUNCTION
	dns_srv_order(struct dns_reply*);

#endif /* __RESOLVE_H__ */
