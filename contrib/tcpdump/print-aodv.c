/*
 * Copyright (c) 2003 Bruce M. Simpson <bms@spc.org>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Bruce M. Simpson.
 * 4. Neither the name of Bruce M. Simpson nor the names of co-
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bruce M. Simpson AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL Bruce M. Simpson OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* \summary: Ad hoc On-Demand Distance Vector (AODV) Routing printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

/*
 * RFC 3561
 */
struct aodv_rreq {
	nd_uint8_t	rreq_type;	/* AODV message type (1) */
	nd_uint8_t	rreq_flags;	/* various flags */
	nd_uint8_t	rreq_zero0;	/* reserved, set to zero */
	nd_uint8_t	rreq_hops;	/* number of hops from originator */
	nd_uint32_t	rreq_id;	/* request ID */
	nd_ipv4		rreq_da;	/* destination IPv4 address */
	nd_uint32_t	rreq_ds;	/* destination sequence number */
	nd_ipv4		rreq_oa;	/* originator IPv4 address */
	nd_uint32_t	rreq_os;	/* originator sequence number */
};
struct aodv_rreq6 {
	nd_uint8_t	rreq_type;	/* AODV message type (1) */
	nd_uint8_t	rreq_flags;	/* various flags */
	nd_uint8_t	rreq_zero0;	/* reserved, set to zero */
	nd_uint8_t	rreq_hops;	/* number of hops from originator */
	nd_uint32_t	rreq_id;	/* request ID */
	nd_ipv6		rreq_da;	/* destination IPv6 address */
	nd_uint32_t	rreq_ds;	/* destination sequence number */
	nd_ipv6		rreq_oa;	/* originator IPv6 address */
	nd_uint32_t	rreq_os;	/* originator sequence number */
};
struct aodv_rreq6_draft_01 {
	nd_uint8_t	rreq_type;	/* AODV message type (16) */
	nd_uint8_t	rreq_flags;	/* various flags */
	nd_uint8_t	rreq_zero0;	/* reserved, set to zero */
	nd_uint8_t	rreq_hops;	/* number of hops from originator */
	nd_uint32_t	rreq_id;	/* request ID */
	nd_uint32_t	rreq_ds;	/* destination sequence number */
	nd_uint32_t	rreq_os;	/* originator sequence number */
	nd_ipv6		rreq_da;	/* destination IPv6 address */
	nd_ipv6		rreq_oa;	/* originator IPv6 address */
};

#define	RREQ_JOIN	0x80		/* join (reserved for multicast */
#define	RREQ_REPAIR	0x40		/* repair (reserved for multicast */
#define	RREQ_GRAT	0x20		/* gratuitous RREP */
#define	RREQ_DEST	0x10		/* destination only */
#define	RREQ_UNKNOWN	0x08		/* unknown destination sequence num */
#define	RREQ_FLAGS_MASK	0xF8		/* mask for rreq_flags */

struct aodv_rrep {
	nd_uint8_t	rrep_type;	/* AODV message type (2) */
	nd_uint8_t	rrep_flags;	/* various flags */
	nd_uint8_t	rrep_ps;	/* prefix size */
	nd_uint8_t	rrep_hops;	/* number of hops from o to d */
	nd_ipv4		rrep_da;	/* destination IPv4 address */
	nd_uint32_t	rrep_ds;	/* destination sequence number */
	nd_ipv4		rrep_oa;	/* originator IPv4 address */
	nd_uint32_t	rrep_life;	/* lifetime of this route */
};
struct aodv_rrep6 {
	nd_uint8_t	rrep_type;	/* AODV message type (2) */
	nd_uint8_t	rrep_flags;	/* various flags */
	nd_uint8_t	rrep_ps;	/* prefix size */
	nd_uint8_t	rrep_hops;	/* number of hops from o to d */
	nd_ipv6		rrep_da;	/* destination IPv6 address */
	nd_uint32_t	rrep_ds;	/* destination sequence number */
	nd_ipv6		rrep_oa;	/* originator IPv6 address */
	nd_uint32_t	rrep_life;	/* lifetime of this route */
};
struct aodv_rrep6_draft_01 {
	nd_uint8_t	rrep_type;	/* AODV message type (17) */
	nd_uint8_t	rrep_flags;	/* various flags */
	nd_uint8_t	rrep_ps;	/* prefix size */
	nd_uint8_t	rrep_hops;	/* number of hops from o to d */
	nd_uint32_t	rrep_ds;	/* destination sequence number */
	nd_ipv6		rrep_da;	/* destination IPv6 address */
	nd_ipv6		rrep_oa;	/* originator IPv6 address */
	nd_uint32_t	rrep_life;	/* lifetime of this route */
};

#define	RREP_REPAIR		0x80	/* repair (reserved for multicast */
#define	RREP_ACK		0x40	/* acknowledgement required */
#define	RREP_FLAGS_MASK		0xC0	/* mask for rrep_flags */
#define	RREP_PREFIX_MASK	0x1F	/* mask for prefix size */

struct rerr_unreach {
	nd_ipv4		u_da;	/* IPv4 address */
	nd_uint32_t	u_ds;	/* sequence number */
};
struct rerr_unreach6 {
	nd_ipv6		u_da;	/* IPv6 address */
	nd_uint32_t	u_ds;	/* sequence number */
};
struct rerr_unreach6_draft_01 {
	nd_ipv6		u_da;	/* IPv6 address */
	nd_uint32_t	u_ds;	/* sequence number */
};

struct aodv_rerr {
	nd_uint8_t	rerr_type;	/* AODV message type (3 or 18) */
	nd_uint8_t	rerr_flags;	/* various flags */
	nd_uint8_t	rerr_zero0;	/* reserved, set to zero */
	nd_uint8_t	rerr_dc;	/* destination count */
};

#define RERR_NODELETE		0x80	/* don't delete the link */
#define RERR_FLAGS_MASK		0x80	/* mask for rerr_flags */

struct aodv_rrep_ack {
	nd_uint8_t	ra_type;
	nd_uint8_t	ra_zero0;
};

#define	AODV_RREQ		1	/* route request */
#define	AODV_RREP		2	/* route response */
#define	AODV_RERR		3	/* error report */
#define	AODV_RREP_ACK		4	/* route response acknowledgement */

#define AODV_V6_DRAFT_01_RREQ		16	/* IPv6 route request */
#define AODV_V6_DRAFT_01_RREP		17	/* IPv6 route response */
#define AODV_V6_DRAFT_01_RERR		18	/* IPv6 error report */
#define AODV_V6_DRAFT_01_RREP_ACK	19	/* IPV6 route response acknowledgment */

struct aodv_ext {
	nd_uint8_t	type;		/* extension type */
	nd_uint8_t	length;		/* extension length */
};

struct aodv_hello {
	struct	aodv_ext	eh;		/* extension header */
	nd_uint32_t		interval;	/* expect my next hello in
						 * (n) ms
						 * NOTE: this is not aligned */
};

#define	AODV_EXT_HELLO	1

static void
aodv_extension(netdissect_options *ndo,
               const struct aodv_ext *ep, u_int length)
{
	const struct aodv_hello *ah;

	ND_TCHECK_SIZE(ep);
	switch (GET_U_1(ep->type)) {
	case AODV_EXT_HELLO:
		ah = (const struct aodv_hello *)(const void *)ep;
		ND_TCHECK_SIZE(ah);
		if (length < sizeof(struct aodv_hello))
			goto trunc;
		if (GET_U_1(ep->length) < 4) {
			ND_PRINT("\n\text HELLO - bad length %u",
				 GET_U_1(ep->length));
			break;
		}
		ND_PRINT("\n\text HELLO %u ms",
		    GET_BE_U_4(ah->interval));
		break;

	default:
		ND_PRINT("\n\text %u %u", GET_U_1(ep->type),
			 GET_U_1(ep->length));
		break;
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_rreq(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rreq *ap = (const struct aodv_rreq *)dat;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rreq %u %s%s%s%s%shops %u id 0x%08x\n"
	    "\tdst %s seq %u src %s seq %u", length,
	    GET_U_1(ap->rreq_type) & RREQ_JOIN ? "[J]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_REPAIR ? "[R]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_GRAT ? "[G]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_DEST ? "[D]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_UNKNOWN ? "[U] " : " ",
	    GET_U_1(ap->rreq_hops),
	    GET_BE_U_4(ap->rreq_id),
	    GET_IPADDR_STRING(ap->rreq_da),
	    GET_BE_U_4(ap->rreq_ds),
	    GET_IPADDR_STRING(ap->rreq_oa),
	    GET_BE_U_4(ap->rreq_os));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_rrep(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rrep *ap = (const struct aodv_rrep *)dat;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rrep %u %s%sprefix %u hops %u\n"
	    "\tdst %s dseq %u src %s %u ms", length,
	    GET_U_1(ap->rrep_type) & RREP_REPAIR ? "[R]" : "",
	    GET_U_1(ap->rrep_type) & RREP_ACK ? "[A] " : " ",
	    GET_U_1(ap->rrep_ps) & RREP_PREFIX_MASK,
	    GET_U_1(ap->rrep_hops),
	    GET_IPADDR_STRING(ap->rrep_da),
	    GET_BE_U_4(ap->rrep_ds),
	    GET_IPADDR_STRING(ap->rrep_oa),
	    GET_BE_U_4(ap->rrep_life));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_rerr(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i, dc;
	const struct aodv_rerr *ap = (const struct aodv_rerr *)dat;
	const struct rerr_unreach *dp;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rerr %s [items %u] [%u]:",
	    GET_U_1(ap->rerr_flags) & RERR_NODELETE ? "[D]" : "",
	    GET_U_1(ap->rerr_dc), length);
	dp = (const struct rerr_unreach *)(dat + sizeof(*ap));
	i = length - sizeof(*ap);
	for (dc = GET_U_1(ap->rerr_dc); dc != 0; dc--) {
		ND_TCHECK_SIZE(dp);
		if (i < sizeof(*dp))
			goto trunc;
		ND_PRINT(" {%s}(%u)", GET_IPADDR_STRING(dp->u_da),
		    GET_BE_U_4(dp->u_ds));
		dp++;
		i -= sizeof(*dp);
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_v6_rreq(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rreq6 *ap = (const struct aodv_rreq6 *)dat;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" v6 rreq %u %s%s%s%s%shops %u id 0x%08x\n"
	    "\tdst %s seq %u src %s seq %u", length,
	    GET_U_1(ap->rreq_type) & RREQ_JOIN ? "[J]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_REPAIR ? "[R]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_GRAT ? "[G]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_DEST ? "[D]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_UNKNOWN ? "[U] " : " ",
	    GET_U_1(ap->rreq_hops),
	    GET_BE_U_4(ap->rreq_id),
	    GET_IP6ADDR_STRING(ap->rreq_da),
	    GET_BE_U_4(ap->rreq_ds),
	    GET_IP6ADDR_STRING(ap->rreq_oa),
	    GET_BE_U_4(ap->rreq_os));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_v6_rrep(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rrep6 *ap = (const struct aodv_rrep6 *)dat;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rrep %u %s%sprefix %u hops %u\n"
	   "\tdst %s dseq %u src %s %u ms", length,
	    GET_U_1(ap->rrep_type) & RREP_REPAIR ? "[R]" : "",
	    GET_U_1(ap->rrep_type) & RREP_ACK ? "[A] " : " ",
	    GET_U_1(ap->rrep_ps) & RREP_PREFIX_MASK,
	    GET_U_1(ap->rrep_hops),
	    GET_IP6ADDR_STRING(ap->rrep_da),
	    GET_BE_U_4(ap->rrep_ds),
	    GET_IP6ADDR_STRING(ap->rrep_oa),
	    GET_BE_U_4(ap->rrep_life));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_v6_rerr(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i, dc;
	const struct aodv_rerr *ap = (const struct aodv_rerr *)dat;
	const struct rerr_unreach6 *dp6;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rerr %s [items %u] [%u]:",
	    GET_U_1(ap->rerr_flags) & RERR_NODELETE ? "[D]" : "",
	    GET_U_1(ap->rerr_dc), length);
	dp6 = (const struct rerr_unreach6 *)(const void *)(ap + 1);
	i = length - sizeof(*ap);
	for (dc = GET_U_1(ap->rerr_dc); dc != 0; dc--) {
		ND_TCHECK_SIZE(dp6);
		if (i < sizeof(*dp6))
			goto trunc;
		ND_PRINT(" {%s}(%u)", GET_IP6ADDR_STRING(dp6->u_da),
			 GET_BE_U_4(dp6->u_ds));
		dp6++;
		i -= sizeof(*dp6);
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_v6_draft_01_rreq(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rreq6_draft_01 *ap = (const struct aodv_rreq6_draft_01 *)dat;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rreq %u %s%s%s%s%shops %u id 0x%08x\n"
	    "\tdst %s seq %u src %s seq %u", length,
	    GET_U_1(ap->rreq_type) & RREQ_JOIN ? "[J]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_REPAIR ? "[R]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_GRAT ? "[G]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_DEST ? "[D]" : "",
	    GET_U_1(ap->rreq_type) & RREQ_UNKNOWN ? "[U] " : " ",
	    GET_U_1(ap->rreq_hops),
	    GET_BE_U_4(ap->rreq_id),
	    GET_IP6ADDR_STRING(ap->rreq_da),
	    GET_BE_U_4(ap->rreq_ds),
	    GET_IP6ADDR_STRING(ap->rreq_oa),
	    GET_BE_U_4(ap->rreq_os));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_v6_draft_01_rrep(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rrep6_draft_01 *ap = (const struct aodv_rrep6_draft_01 *)dat;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rrep %u %s%sprefix %u hops %u\n"
	   "\tdst %s dseq %u src %s %u ms", length,
	    GET_U_1(ap->rrep_type) & RREP_REPAIR ? "[R]" : "",
	    GET_U_1(ap->rrep_type) & RREP_ACK ? "[A] " : " ",
	    GET_U_1(ap->rrep_ps) & RREP_PREFIX_MASK,
	    GET_U_1(ap->rrep_hops),
	    GET_IP6ADDR_STRING(ap->rrep_da),
	    GET_BE_U_4(ap->rrep_ds),
	    GET_IP6ADDR_STRING(ap->rrep_oa),
	    GET_BE_U_4(ap->rrep_life));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
aodv_v6_draft_01_rerr(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i, dc;
	const struct aodv_rerr *ap = (const struct aodv_rerr *)dat;
	const struct rerr_unreach6_draft_01 *dp6;

	ND_TCHECK_SIZE(ap);
	if (length < sizeof(*ap))
		goto trunc;
	ND_PRINT(" rerr %s [items %u] [%u]:",
	    GET_U_1(ap->rerr_flags) & RERR_NODELETE ? "[D]" : "",
	    GET_U_1(ap->rerr_dc), length);
	dp6 = (const struct rerr_unreach6_draft_01 *)(const void *)(ap + 1);
	i = length - sizeof(*ap);
	for (dc = GET_U_1(ap->rerr_dc); dc != 0; dc--) {
		ND_TCHECK_SIZE(dp6);
		if (i < sizeof(*dp6))
			goto trunc;
		ND_PRINT(" {%s}(%u)", GET_IP6ADDR_STRING(dp6->u_da),
			 GET_BE_U_4(dp6->u_ds));
		dp6++;
		i -= sizeof(*dp6);
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

void
aodv_print(netdissect_options *ndo,
           const u_char *dat, u_int length, int is_ip6)
{
	uint8_t msg_type;

	ndo->ndo_protocol = "aodv";
	/*
	 * The message type is the first byte; make sure we have it
	 * and then fetch it.
	 */
	msg_type = GET_U_1(dat);
	ND_PRINT(" aodv");

	switch (msg_type) {

	case AODV_RREQ:
		if (is_ip6)
			aodv_v6_rreq(ndo, dat, length);
		else
			aodv_rreq(ndo, dat, length);
		break;

	case AODV_RREP:
		if (is_ip6)
			aodv_v6_rrep(ndo, dat, length);
		else
			aodv_rrep(ndo, dat, length);
		break;

	case AODV_RERR:
		if (is_ip6)
			aodv_v6_rerr(ndo, dat, length);
		else
			aodv_rerr(ndo, dat, length);
		break;

	case AODV_RREP_ACK:
		ND_PRINT(" rrep-ack %u", length);
		break;

	case AODV_V6_DRAFT_01_RREQ:
		aodv_v6_draft_01_rreq(ndo, dat, length);
		break;

	case AODV_V6_DRAFT_01_RREP:
		aodv_v6_draft_01_rrep(ndo, dat, length);
		break;

	case AODV_V6_DRAFT_01_RERR:
		aodv_v6_draft_01_rerr(ndo, dat, length);
		break;

	case AODV_V6_DRAFT_01_RREP_ACK:
		ND_PRINT(" rrep-ack %u", length);
		break;

	default:
		ND_PRINT(" type %u %u", msg_type, length);
	}
}
