/*
 * Copyright (C) 2001 WIDE Project.
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

/* \summary: BIND9 Lightweight Resolver protocol printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "nameser.h"

/* BIND9 lib/lwres/include/lwres */
/*
 * Use nd_uint16_t for lwres_uint16_t
 * Use nd_uint32_t for lwres_uint32_t
*/

struct lwres_lwpacket {
	nd_uint32_t		length;
	nd_uint16_t		version;
	nd_uint16_t		pktflags;
	nd_uint32_t		serial;
	nd_uint32_t		opcode;
	nd_uint32_t		result;
	nd_uint32_t		recvlength;
	nd_uint16_t		authtype;
	nd_uint16_t		authlength;
};

#define LWRES_LWPACKETFLAG_RESPONSE	0x0001U	/* if set, pkt is a response */

#define LWRES_LWPACKETVERSION_0		0

#define LWRES_FLAG_TRUSTNOTREQUIRED	0x00000001U
#define LWRES_FLAG_SECUREDATA		0x00000002U

/*
 * no-op
 */
#define LWRES_OPCODE_NOOP		0x00000000U

typedef struct {
	/* public */
	nd_uint16_t			datalength;
	/* data follows */
} lwres_nooprequest_t;

typedef struct {
	/* public */
	nd_uint16_t			datalength;
	/* data follows */
} lwres_noopresponse_t;

/*
 * get addresses by name
 */
#define LWRES_OPCODE_GETADDRSBYNAME	0x00010001U

typedef struct lwres_addr lwres_addr_t;

struct lwres_addr {
	nd_uint32_t			family;
	nd_uint16_t			length;
	/* address follows */
};
#define LWRES_ADDR_LEN			6

typedef struct {
	/* public */
	nd_uint32_t			flags;
	nd_uint32_t			addrtypes;
	nd_uint16_t			namelen;
	/* name follows */
} lwres_gabnrequest_t;
#define LWRES_GABNREQUEST_LEN		10

typedef struct {
	/* public */
	nd_uint32_t			flags;
	nd_uint16_t			naliases;
	nd_uint16_t			naddrs;
	nd_uint16_t			realnamelen;
	/* aliases follows */
	/* addrs follows */
	/* realname follows */
} lwres_gabnresponse_t;
#define LWRES_GABNRESPONSE_LEN		10

/*
 * get name by address
 */
#define LWRES_OPCODE_GETNAMEBYADDR	0x00010002U
typedef struct {
	/* public */
	nd_uint32_t			flags;
	/* addr follows */
} lwres_gnbarequest_t;
#define LWRES_GNBAREQUEST_LEN		4

typedef struct {
	/* public */
	nd_uint32_t			flags;
	nd_uint16_t			naliases;
	nd_uint16_t			realnamelen;
	/* aliases follows */
	/* realname follows */
} lwres_gnbaresponse_t;
#define LWRES_GNBARESPONSE_LEN		8

/*
 * get rdata by name
 */
#define LWRES_OPCODE_GETRDATABYNAME	0x00010003U

typedef struct {
	/* public */
	nd_uint32_t			flags;
	nd_uint16_t			rdclass;
	nd_uint16_t			rdtype;
	nd_uint16_t			namelen;
	/* name follows */
} lwres_grbnrequest_t;
#define LWRES_GRBNREQUEST_LEN		10

typedef struct {
	/* public */
	nd_uint32_t			flags;
	nd_uint16_t			rdclass;
	nd_uint16_t			rdtype;
	nd_uint32_t			ttl;
	nd_uint16_t			nrdatas;
	nd_uint16_t			nsigs;
	/* realname here (len + name) */
	/* rdata here (len + name) */
	/* signatures here (len + name) */
} lwres_grbnresponse_t;
#define LWRES_GRBNRESPONSE_LEN		16

#define LWRDATA_VALIDATED	0x00000001

#define LWRES_ADDRTYPE_V4		0x00000001U	/* ipv4 */
#define LWRES_ADDRTYPE_V6		0x00000002U	/* ipv6 */

#define LWRES_MAX_ALIASES		16		/* max # of aliases */
#define LWRES_MAX_ADDRS			64		/* max # of addrs */

static const struct tok opcode[] = {
	{ LWRES_OPCODE_NOOP,		"noop", },
	{ LWRES_OPCODE_GETADDRSBYNAME,	"getaddrsbyname", },
	{ LWRES_OPCODE_GETNAMEBYADDR,	"getnamebyaddr", },
	{ LWRES_OPCODE_GETRDATABYNAME,	"getrdatabyname", },
	{ 0,				NULL, },
};

/* print-domain.c */
extern const struct tok ns_type2str[];
extern const struct tok ns_class2str[];

static unsigned
lwres_printname(netdissect_options *ndo,
                u_int l, const u_char *p0)
{
	ND_PRINT(" ");
	(void)nd_printn(ndo, p0, l, NULL);
	p0 += l;
	if (GET_U_1(p0))
		ND_PRINT(" (not NUL-terminated!)");
	return l + 1;
}

static unsigned
lwres_printnamelen(netdissect_options *ndo,
                   const u_char *p)
{
	uint16_t l;
	int advance;

	l = GET_BE_U_2(p);
	advance = lwres_printname(ndo, l, p + 2);
	return 2 + advance;
}

static unsigned
lwres_printbinlen(netdissect_options *ndo,
                  const u_char *p0)
{
	const u_char *p;
	uint16_t l;
	int i;

	p = p0;
	l = GET_BE_U_2(p);
	p += 2;
	for (i = 0; i < l; i++) {
		ND_PRINT("%02x", GET_U_1(p));
		p++;
	}
	return 2 + l;
}

static int
lwres_printaddr(netdissect_options *ndo,
                const u_char *p0)
{
	const u_char *p;
	const lwres_addr_t *ap;
	uint16_t l;
	int i;

	p = p0;
	ap = (const lwres_addr_t *)p;
	l = GET_BE_U_2(ap->length);
	p += LWRES_ADDR_LEN;
	ND_TCHECK_LEN(p, l);

	switch (GET_BE_U_4(ap->family)) {
	case 1:	/* IPv4 */
		if (l < 4)
			return -1;
		ND_PRINT(" %s", GET_IPADDR_STRING(p));
		p += sizeof(nd_ipv4);
		break;
	case 2:	/* IPv6 */
		if (l < 16)
			return -1;
		ND_PRINT(" %s", GET_IP6ADDR_STRING(p));
		p += sizeof(nd_ipv6);
		break;
	default:
		ND_PRINT(" %u/", GET_BE_U_4(ap->family));
		for (i = 0; i < l; i++) {
			ND_PRINT("%02x", GET_U_1(p));
			p++;
		}
	}

	return ND_BYTES_BETWEEN(p0, p);
}

void
lwres_print(netdissect_options *ndo,
            const u_char *bp, u_int length)
{
	const u_char *p;
	const struct lwres_lwpacket *np;
	uint32_t v;
	const u_char *s;
	int response;
	int advance;
	int unsupported = 0;

	ndo->ndo_protocol = "lwres";
	np = (const struct lwres_lwpacket *)bp;
	ND_TCHECK_2(np->authlength);

	ND_PRINT(" lwres");
	v = GET_BE_U_2(np->version);
	if (ndo->ndo_vflag || v != LWRES_LWPACKETVERSION_0)
		ND_PRINT(" v%u", v);
	if (v != LWRES_LWPACKETVERSION_0) {
		uint32_t pkt_len = GET_BE_U_4(np->length);
		ND_TCHECK_LEN(bp, pkt_len);
		s = bp + pkt_len;
		goto tail;
	}

	response = GET_BE_U_2(np->pktflags) & LWRES_LWPACKETFLAG_RESPONSE;

	/* opcode and pktflags */
	v = GET_BE_U_4(np->opcode);
	ND_PRINT(" %s%s", tok2str(opcode, "#0x%x", v), response ? "" : "?");

	/* pktflags */
	v = GET_BE_U_2(np->pktflags);
	if (v & ~LWRES_LWPACKETFLAG_RESPONSE)
		ND_PRINT("[0x%x]", v);

	if (ndo->ndo_vflag > 1) {
		ND_PRINT(" (");	/*)*/
		ND_PRINT("serial:0x%x", GET_BE_U_4(np->serial));
		ND_PRINT(" result:0x%x", GET_BE_U_4(np->result));
		ND_PRINT(" recvlen:%u", GET_BE_U_4(np->recvlength));
		/* BIND910: not used */
		if (ndo->ndo_vflag > 2) {
			ND_PRINT(" authtype:0x%x", GET_BE_U_2(np->authtype));
			ND_PRINT(" authlen:%u", GET_BE_U_2(np->authlength));
		}
		/*(*/
		ND_PRINT(")");
	}

	/* per-opcode content */
	if (!response) {
		/*
		 * queries
		 */
		const lwres_gabnrequest_t *gabn;
		const lwres_gnbarequest_t *gnba;
		const lwres_grbnrequest_t *grbn;
		uint32_t l;

		gabn = NULL;
		gnba = NULL;
		grbn = NULL;

		p = (const u_char *)(np + 1);
		switch (GET_BE_U_4(np->opcode)) {
		case LWRES_OPCODE_NOOP:
			s = p;
			break;
		case LWRES_OPCODE_GETADDRSBYNAME:
			gabn = (const lwres_gabnrequest_t *)p;
			ND_TCHECK_2(gabn->namelen);

			/* BIND910: not used */
			if (ndo->ndo_vflag > 2) {
				ND_PRINT(" flags:0x%x",
				    GET_BE_U_4(gabn->flags));
			}

			v = GET_BE_U_4(gabn->addrtypes);
			switch (v & (LWRES_ADDRTYPE_V4 | LWRES_ADDRTYPE_V6)) {
			case LWRES_ADDRTYPE_V4:
				ND_PRINT(" IPv4");
				break;
			case LWRES_ADDRTYPE_V6:
				ND_PRINT(" IPv6");
				break;
			case LWRES_ADDRTYPE_V4 | LWRES_ADDRTYPE_V6:
				ND_PRINT(" IPv4/6");
				break;
			}
			if (v & ~(LWRES_ADDRTYPE_V4 | LWRES_ADDRTYPE_V6))
				ND_PRINT("[0x%x]", v);

			s = p + LWRES_GABNREQUEST_LEN;
			l = GET_BE_U_2(gabn->namelen);
			advance = lwres_printname(ndo, l, s);
			s += advance;
			break;
		case LWRES_OPCODE_GETNAMEBYADDR:
			gnba = (const lwres_gnbarequest_t *)p;
			ND_TCHECK_4(gnba->flags);

			/* BIND910: not used */
			if (ndo->ndo_vflag > 2) {
				ND_PRINT(" flags:0x%x",
				    GET_BE_U_4(gnba->flags));
			}

			s = p + LWRES_GNBAREQUEST_LEN;
			advance = lwres_printaddr(ndo, s);
			if (advance < 0)
				goto invalid;
			s += advance;
			break;
		case LWRES_OPCODE_GETRDATABYNAME:
			/* XXX no trace, not tested */
			grbn = (const lwres_grbnrequest_t *)p;
			ND_TCHECK_2(grbn->namelen);

			/* BIND910: not used */
			if (ndo->ndo_vflag > 2) {
				ND_PRINT(" flags:0x%x",
				    GET_BE_U_4(grbn->flags));
			}

			ND_PRINT(" %s", tok2str(ns_type2str, "Type%u",
			    GET_BE_U_2(grbn->rdtype)));
			if (GET_BE_U_2(grbn->rdclass) != C_IN) {
				ND_PRINT(" %s", tok2str(ns_class2str, "Class%u",
				    GET_BE_U_2(grbn->rdclass)));
			}

			s = p + LWRES_GRBNREQUEST_LEN;
			l = GET_BE_U_2(grbn->namelen);
			advance = lwres_printname(ndo, l, s);
			s += advance;
			break;
		default:
			s = p;
			unsupported++;
			break;
		}
	} else {
		/*
		 * responses
		 */
		const lwres_gabnresponse_t *gabn;
		const lwres_gnbaresponse_t *gnba;
		const lwres_grbnresponse_t *grbn;
		uint32_t l, na;
		uint32_t i;

		gabn = NULL;
		gnba = NULL;
		grbn = NULL;

		p = (const u_char *)(np + 1);
		switch (GET_BE_U_4(np->opcode)) {
		case LWRES_OPCODE_NOOP:
			s = p;
			break;
		case LWRES_OPCODE_GETADDRSBYNAME:
			gabn = (const lwres_gabnresponse_t *)p;
			ND_TCHECK_2(gabn->realnamelen);

			/* BIND910: not used */
			if (ndo->ndo_vflag > 2) {
				ND_PRINT(" flags:0x%x",
				    GET_BE_U_4(gabn->flags));
			}

			ND_PRINT(" %u/%u", GET_BE_U_2(gabn->naliases),
				  GET_BE_U_2(gabn->naddrs));

			s = p + LWRES_GABNRESPONSE_LEN;
			l = GET_BE_U_2(gabn->realnamelen);
			advance = lwres_printname(ndo, l, s);
			s += advance;

			/* aliases */
			na = GET_BE_U_2(gabn->naliases);
			for (i = 0; i < na; i++) {
				advance = lwres_printnamelen(ndo, s);
				s += advance;
			}

			/* addrs */
			na = GET_BE_U_2(gabn->naddrs);
			for (i = 0; i < na; i++) {
				advance = lwres_printaddr(ndo, s);
				if (advance < 0)
					goto invalid;
				s += advance;
			}
			break;
		case LWRES_OPCODE_GETNAMEBYADDR:
			gnba = (const lwres_gnbaresponse_t *)p;
			ND_TCHECK_2(gnba->realnamelen);

			/* BIND910: not used */
			if (ndo->ndo_vflag > 2) {
				ND_PRINT(" flags:0x%x",
				    GET_BE_U_4(gnba->flags));
			}

			ND_PRINT(" %u", GET_BE_U_2(gnba->naliases));

			s = p + LWRES_GNBARESPONSE_LEN;
			l = GET_BE_U_2(gnba->realnamelen);
			advance = lwres_printname(ndo, l, s);
			s += advance;

			/* aliases */
			na = GET_BE_U_2(gnba->naliases);
			for (i = 0; i < na; i++) {
				advance = lwres_printnamelen(ndo, s);
				s += advance;
			}
			break;
		case LWRES_OPCODE_GETRDATABYNAME:
			/* XXX no trace, not tested */
			grbn = (const lwres_grbnresponse_t *)p;
			ND_TCHECK_2(grbn->nsigs);

			/* BIND910: not used */
			if (ndo->ndo_vflag > 2) {
				ND_PRINT(" flags:0x%x",
				    GET_BE_U_4(grbn->flags));
			}

			ND_PRINT(" %s", tok2str(ns_type2str, "Type%u",
			    GET_BE_U_2(grbn->rdtype)));
			if (GET_BE_U_2(grbn->rdclass) != C_IN) {
				ND_PRINT(" %s", tok2str(ns_class2str, "Class%u",
				    GET_BE_U_2(grbn->rdclass)));
			}
			ND_PRINT(" TTL ");
			unsigned_relts_print(ndo,
					     GET_BE_U_4(grbn->ttl));
			ND_PRINT(" %u/%u", GET_BE_U_2(grbn->nrdatas),
				  GET_BE_U_2(grbn->nsigs));

			s = p + LWRES_GRBNRESPONSE_LEN;
			advance = lwres_printnamelen(ndo, s);
			s += advance;

			/* rdatas */
			na = GET_BE_U_2(grbn->nrdatas);
			for (i = 0; i < na; i++) {
				/* XXX should decode resource data */
				advance = lwres_printbinlen(ndo, s);
				s += advance;
			}

			/* sigs */
			na = GET_BE_U_2(grbn->nsigs);
			for (i = 0; i < na; i++) {
				/* XXX how should we print it? */
				advance = lwres_printbinlen(ndo, s);
				s += advance;
			}
			break;
		default:
			s = p;
			unsupported++;
			break;
		}
	}

  tail:
	/* length mismatch */
	if (GET_BE_U_4(np->length) != length) {
		ND_PRINT(" [len: %u != %u]", GET_BE_U_4(np->length),
			  length);
	}
	if (!unsupported && ND_BYTES_BETWEEN(bp, s) < GET_BE_U_4(np->length))
		ND_PRINT("[extra]");
	return;

  invalid:
	nd_print_invalid(ndo);
}
