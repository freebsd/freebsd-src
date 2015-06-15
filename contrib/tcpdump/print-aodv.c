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

#define NETDISSECT_REWORKED
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

/* for offsetof */
#include <stddef.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */


struct aodv_rreq {
	uint8_t		rreq_type;	/* AODV message type (1) */
	uint8_t		rreq_flags;	/* various flags */
	uint8_t		rreq_zero0;	/* reserved, set to zero */
	uint8_t		rreq_hops;	/* number of hops from originator */
	uint32_t	rreq_id;	/* request ID */
	uint32_t	rreq_da;	/* destination IPv4 address */
	uint32_t	rreq_ds;	/* destination sequence number */
	uint32_t	rreq_oa;	/* originator IPv4 address */
	uint32_t	rreq_os;	/* originator sequence number */
};
#ifdef INET6
struct aodv_rreq6 {
	uint8_t		rreq_type;	/* AODV message type (1) */
	uint8_t		rreq_flags;	/* various flags */
	uint8_t		rreq_zero0;	/* reserved, set to zero */
	uint8_t		rreq_hops;	/* number of hops from originator */
	uint32_t	rreq_id;	/* request ID */
	struct in6_addr	rreq_da;	/* destination IPv6 address */
	uint32_t	rreq_ds;	/* destination sequence number */
	struct in6_addr	rreq_oa;	/* originator IPv6 address */
	uint32_t	rreq_os;	/* originator sequence number */
};
struct aodv_rreq6_draft_01 {
	uint8_t		rreq_type;	/* AODV message type (16) */
	uint8_t		rreq_flags;	/* various flags */
	uint8_t		rreq_zero0;	/* reserved, set to zero */
	uint8_t		rreq_hops;	/* number of hops from originator */
	uint32_t	rreq_id;	/* request ID */
	uint32_t	rreq_ds;	/* destination sequence number */
	uint32_t	rreq_os;	/* originator sequence number */
	struct in6_addr	rreq_da;	/* destination IPv6 address */
	struct in6_addr	rreq_oa;	/* originator IPv6 address */
};
#endif

#define	RREQ_JOIN	0x80		/* join (reserved for multicast */
#define	RREQ_REPAIR	0x40		/* repair (reserved for multicast */
#define	RREQ_GRAT	0x20		/* gratuitous RREP */
#define	RREQ_DEST	0x10		/* destination only */
#define	RREQ_UNKNOWN	0x08		/* unknown destination sequence num */
#define	RREQ_FLAGS_MASK	0xF8		/* mask for rreq_flags */

struct aodv_rrep {
	uint8_t		rrep_type;	/* AODV message type (2) */
	uint8_t		rrep_flags;	/* various flags */
	uint8_t		rrep_ps;	/* prefix size */
	uint8_t		rrep_hops;	/* number of hops from o to d */
	uint32_t	rrep_da;	/* destination IPv4 address */
	uint32_t	rrep_ds;	/* destination sequence number */
	uint32_t	rrep_oa;	/* originator IPv4 address */
	uint32_t	rrep_life;	/* lifetime of this route */
};
#ifdef INET6
struct aodv_rrep6 {
	uint8_t		rrep_type;	/* AODV message type (2) */
	uint8_t		rrep_flags;	/* various flags */
	uint8_t		rrep_ps;	/* prefix size */
	uint8_t		rrep_hops;	/* number of hops from o to d */
	struct in6_addr	rrep_da;	/* destination IPv6 address */
	uint32_t	rrep_ds;	/* destination sequence number */
	struct in6_addr	rrep_oa;	/* originator IPv6 address */
	uint32_t	rrep_life;	/* lifetime of this route */
};
struct aodv_rrep6_draft_01 {
	uint8_t		rrep_type;	/* AODV message type (17) */
	uint8_t		rrep_flags;	/* various flags */
	uint8_t		rrep_ps;	/* prefix size */
	uint8_t		rrep_hops;	/* number of hops from o to d */
	uint32_t	rrep_ds;	/* destination sequence number */
	struct in6_addr	rrep_da;	/* destination IPv6 address */
	struct in6_addr	rrep_oa;	/* originator IPv6 address */
	uint32_t	rrep_life;	/* lifetime of this route */
};
#endif

#define	RREP_REPAIR		0x80	/* repair (reserved for multicast */
#define	RREP_ACK		0x40	/* acknowledgement required */
#define	RREP_FLAGS_MASK		0xC0	/* mask for rrep_flags */
#define	RREP_PREFIX_MASK	0x1F	/* mask for prefix size */

struct rerr_unreach {
	uint32_t	u_da;	/* IPv4 address */
	uint32_t	u_ds;	/* sequence number */
};
#ifdef INET6
struct rerr_unreach6 {
	struct in6_addr	u_da;	/* IPv6 address */
	uint32_t	u_ds;	/* sequence number */
};
struct rerr_unreach6_draft_01 {
	struct in6_addr	u_da;	/* IPv6 address */
	uint32_t	u_ds;	/* sequence number */
};
#endif

struct aodv_rerr {
	uint8_t		rerr_type;	/* AODV message type (3 or 18) */
	uint8_t		rerr_flags;	/* various flags */
	uint8_t		rerr_zero0;	/* reserved, set to zero */
	uint8_t		rerr_dc;	/* destination count */
	union {
		struct	rerr_unreach dest[1];
#ifdef INET6
		struct	rerr_unreach6 dest6[1];
		struct	rerr_unreach6_draft_01 dest6_draft_01[1];
#endif
	} r;
};

#define RERR_NODELETE		0x80	/* don't delete the link */
#define RERR_FLAGS_MASK		0x80	/* mask for rerr_flags */

struct aodv_rrep_ack {
	uint8_t		ra_type;
	uint8_t		ra_zero0;
};

union aodv {
	struct aodv_rreq rreq;
	struct aodv_rrep rrep;
	struct aodv_rerr rerr;
	struct aodv_rrep_ack rrep_ack;
#ifdef INET6
	struct aodv_rreq6 rreq6;
	struct aodv_rreq6_draft_01 rreq6_draft_01;
	struct aodv_rrep6 rrep6;
	struct aodv_rrep6_draft_01 rrep6_draft_01;
#endif
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
	uint8_t		type;		/* extension type */
	uint8_t		length;		/* extension length */
};

struct aodv_hello {
	struct	aodv_ext	eh;		/* extension header */
	uint8_t			interval[4];	/* expect my next hello in
						 * (n) ms
						 * NOTE: this is not aligned */
};

#define	AODV_EXT_HELLO	1

static void
aodv_extension(netdissect_options *ndo,
               const struct aodv_ext *ep, u_int length)
{
	u_int i;
	const struct aodv_hello *ah;

	switch (ep->type) {
	case AODV_EXT_HELLO:
		if (ndo->ndo_snapend < (u_char *) ep) {
			ND_PRINT((ndo, " [|hello]"));
			return;
		}
		i = min(length, (u_int)(ndo->ndo_snapend - (u_char *)ep));
		if (i < sizeof(struct aodv_hello)) {
			ND_PRINT((ndo, " [|hello]"));
			return;
		}
		i -= sizeof(struct aodv_hello);
		ah = (void *)ep;
		ND_PRINT((ndo, "\n\text HELLO %ld ms",
		    (unsigned long)EXTRACT_32BITS(&ah->interval)));
		break;

	default:
		ND_PRINT((ndo, "\n\text %u %u", ep->type, ep->length));
		break;
	}
}

static void
aodv_rreq(netdissect_options *ndo,
          const union aodv *ap, const u_char *dat, u_int length)
{
	u_int i;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < sizeof(ap->rreq)) {
		ND_PRINT((ndo, " [|rreq]"));
		return;
	}
	i -= sizeof(ap->rreq);
	ND_PRINT((ndo, " rreq %u %s%s%s%s%shops %u id 0x%08lx\n"
	    "\tdst %s seq %lu src %s seq %lu", length,
	    ap->rreq.rreq_type & RREQ_JOIN ? "[J]" : "",
	    ap->rreq.rreq_type & RREQ_REPAIR ? "[R]" : "",
	    ap->rreq.rreq_type & RREQ_GRAT ? "[G]" : "",
	    ap->rreq.rreq_type & RREQ_DEST ? "[D]" : "",
	    ap->rreq.rreq_type & RREQ_UNKNOWN ? "[U] " : " ",
	    ap->rreq.rreq_hops,
	    (unsigned long)EXTRACT_32BITS(&ap->rreq.rreq_id),
	    ipaddr_string(ndo, &ap->rreq.rreq_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq.rreq_ds),
	    ipaddr_string(ndo, &ap->rreq.rreq_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq.rreq_os)));
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (void *)(&ap->rreq + 1), i);
}

static void
aodv_rrep(netdissect_options *ndo,
          const union aodv *ap, const u_char *dat, u_int length)
{
	u_int i;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < sizeof(ap->rrep)) {
		ND_PRINT((ndo, " [|rrep]"));
		return;
	}
	i -= sizeof(ap->rrep);
	ND_PRINT((ndo, " rrep %u %s%sprefix %u hops %u\n"
	    "\tdst %s dseq %lu src %s %lu ms", length,
	    ap->rrep.rrep_type & RREP_REPAIR ? "[R]" : "",
	    ap->rrep.rrep_type & RREP_ACK ? "[A] " : " ",
	    ap->rrep.rrep_ps & RREP_PREFIX_MASK,
	    ap->rrep.rrep_hops,
	    ipaddr_string(ndo, &ap->rrep.rrep_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep.rrep_ds),
	    ipaddr_string(ndo, &ap->rrep.rrep_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep.rrep_life)));
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (void *)(&ap->rrep + 1), i);
}

static void
aodv_rerr(netdissect_options *ndo,
          const union aodv *ap, const u_char *dat, u_int length)
{
	u_int i;
	const struct rerr_unreach *dp = NULL;
	int n, trunc;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < offsetof(struct aodv_rerr, r)) {
		ND_PRINT((ndo, " [|rerr]"));
		return;
	}
	i -= offsetof(struct aodv_rerr, r);
	dp = &ap->rerr.r.dest[0];
	n = ap->rerr.rerr_dc * sizeof(ap->rerr.r.dest[0]);
	ND_PRINT((ndo, " rerr %s [items %u] [%u]:",
	    ap->rerr.rerr_flags & RERR_NODELETE ? "[D]" : "",
	    ap->rerr.rerr_dc, length));
	trunc = n - (i/sizeof(ap->rerr.r.dest[0]));
	for (; i >= sizeof(ap->rerr.r.dest[0]);
	    ++dp, i -= sizeof(ap->rerr.r.dest[0])) {
		ND_PRINT((ndo, " {%s}(%ld)", ipaddr_string(ndo, &dp->u_da),
		    (unsigned long)EXTRACT_32BITS(&dp->u_ds)));
	}
	if (trunc)
		ND_PRINT((ndo, "[|rerr]"));
}

static void
#ifdef INET6
aodv_v6_rreq(netdissect_options *ndo,
             const union aodv *ap, const u_char *dat, u_int length)
#else
aodv_v6_rreq(netdissect_options *ndo,
             const union aodv *ap _U_, const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < sizeof(ap->rreq6)) {
		ND_PRINT((ndo, " [|rreq6]"));
		return;
	}
	i -= sizeof(ap->rreq6);
	ND_PRINT((ndo, " v6 rreq %u %s%s%s%s%shops %u id 0x%08lx\n"
	    "\tdst %s seq %lu src %s seq %lu", length,
	    ap->rreq6.rreq_type & RREQ_JOIN ? "[J]" : "",
	    ap->rreq6.rreq_type & RREQ_REPAIR ? "[R]" : "",
	    ap->rreq6.rreq_type & RREQ_GRAT ? "[G]" : "",
	    ap->rreq6.rreq_type & RREQ_DEST ? "[D]" : "",
	    ap->rreq6.rreq_type & RREQ_UNKNOWN ? "[U] " : " ",
	    ap->rreq6.rreq_hops,
	    (unsigned long)EXTRACT_32BITS(&ap->rreq6.rreq_id),
	    ip6addr_string(ndo, &ap->rreq6.rreq_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq6.rreq_ds),
	    ip6addr_string(ndo, &ap->rreq6.rreq_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq6.rreq_os)));
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (void *)(&ap->rreq6 + 1), i);
#else
	ND_PRINT((ndo, " v6 rreq %u", length));
#endif
}

static void
#ifdef INET6
aodv_v6_rrep(netdissect_options *ndo,
             const union aodv *ap, const u_char *dat, u_int length)
#else
aodv_v6_rrep(netdissect_options *ndo,
             const union aodv *ap _U_, const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < sizeof(ap->rrep6)) {
		ND_PRINT((ndo, " [|rrep6]"));
		return;
	}
	i -= sizeof(ap->rrep6);
	ND_PRINT((ndo, " rrep %u %s%sprefix %u hops %u\n"
	   "\tdst %s dseq %lu src %s %lu ms", length,
	    ap->rrep6.rrep_type & RREP_REPAIR ? "[R]" : "",
	    ap->rrep6.rrep_type & RREP_ACK ? "[A] " : " ",
	    ap->rrep6.rrep_ps & RREP_PREFIX_MASK,
	    ap->rrep6.rrep_hops,
	    ip6addr_string(ndo, &ap->rrep6.rrep_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep6.rrep_ds),
	    ip6addr_string(ndo, &ap->rrep6.rrep_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep6.rrep_life)));
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (void *)(&ap->rrep6 + 1), i);
#else
	ND_PRINT((ndo, " rrep %u", length));
#endif
}

static void
#ifdef INET6
aodv_v6_rerr(netdissect_options *ndo,
             const union aodv *ap, u_int length)
#else
aodv_v6_rerr(netdissect_options *ndo,
             const union aodv *ap _U_, u_int length)
#endif
{
#ifdef INET6
	const struct rerr_unreach6 *dp6 = NULL;
	int i, j, n, trunc;

	i = length - offsetof(struct aodv_rerr, r);
	j = sizeof(ap->rerr.r.dest6[0]);
	dp6 = &ap->rerr.r.dest6[0];
	n = ap->rerr.rerr_dc * j;
	ND_PRINT((ndo, " rerr %s [items %u] [%u]:",
	    ap->rerr.rerr_flags & RERR_NODELETE ? "[D]" : "",
	    ap->rerr.rerr_dc, length));
	trunc = n - (i/j);
	for (; i -= j >= 0; ++dp6) {
		ND_PRINT((ndo, " {%s}(%ld)", ip6addr_string(ndo, &dp6->u_da),
		    (unsigned long)EXTRACT_32BITS(&dp6->u_ds)));
	}
	if (trunc)
		ND_PRINT((ndo, "[|rerr]"));
#else
	ND_PRINT((ndo, " rerr %u", length));
#endif
}

static void
#ifdef INET6
aodv_v6_draft_01_rreq(netdissect_options *ndo,
                      const union aodv *ap, const u_char *dat, u_int length)
#else
aodv_v6_draft_01_rreq(netdissect_options *ndo,
                      const union aodv *ap _U_, const u_char *dat _U_,
    u_int length)
#endif
{
#ifdef INET6
	u_int i;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < sizeof(ap->rreq6_draft_01)) {
		ND_PRINT((ndo, " [|rreq6]"));
		return;
	}
	i -= sizeof(ap->rreq6_draft_01);
	ND_PRINT((ndo, " rreq %u %s%s%s%s%shops %u id 0x%08lx\n"
	    "\tdst %s seq %lu src %s seq %lu", length,
	    ap->rreq6_draft_01.rreq_type & RREQ_JOIN ? "[J]" : "",
	    ap->rreq6_draft_01.rreq_type & RREQ_REPAIR ? "[R]" : "",
	    ap->rreq6_draft_01.rreq_type & RREQ_GRAT ? "[G]" : "",
	    ap->rreq6_draft_01.rreq_type & RREQ_DEST ? "[D]" : "",
	    ap->rreq6_draft_01.rreq_type & RREQ_UNKNOWN ? "[U] " : " ",
	    ap->rreq6_draft_01.rreq_hops,
	    (unsigned long)EXTRACT_32BITS(&ap->rreq6_draft_01.rreq_id),
	    ip6addr_string(ndo, &ap->rreq6_draft_01.rreq_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq6_draft_01.rreq_ds),
	    ip6addr_string(ndo, &ap->rreq6_draft_01.rreq_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq6_draft_01.rreq_os)));
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (void *)(&ap->rreq6_draft_01 + 1), i);
#else
	ND_PRINT((ndo, " rreq %u", length));
#endif
}

static void
#ifdef INET6
aodv_v6_draft_01_rrep(netdissect_options *ndo,
                      const union aodv *ap, const u_char *dat, u_int length)
#else
aodv_v6_draft_01_rrep(netdissect_options *ndo,
                      const union aodv *ap _U_, const u_char *dat _U_,
    u_int length)
#endif
{
#ifdef INET6
	u_int i;

	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	i = min(length, (u_int)(ndo->ndo_snapend - dat));
	if (i < sizeof(ap->rrep6_draft_01)) {
		ND_PRINT((ndo, " [|rrep6]"));
		return;
	}
	i -= sizeof(ap->rrep6_draft_01);
	ND_PRINT((ndo, " rrep %u %s%sprefix %u hops %u\n"
	   "\tdst %s dseq %lu src %s %lu ms", length,
	    ap->rrep6_draft_01.rrep_type & RREP_REPAIR ? "[R]" : "",
	    ap->rrep6_draft_01.rrep_type & RREP_ACK ? "[A] " : " ",
	    ap->rrep6_draft_01.rrep_ps & RREP_PREFIX_MASK,
	    ap->rrep6_draft_01.rrep_hops,
	    ip6addr_string(ndo, &ap->rrep6_draft_01.rrep_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep6_draft_01.rrep_ds),
	    ip6addr_string(ndo, &ap->rrep6_draft_01.rrep_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep6_draft_01.rrep_life)));
	if (i >= sizeof(struct aodv_ext))
		aodv_extension(ndo, (void *)(&ap->rrep6_draft_01 + 1), i);
#else
	ND_PRINT((ndo, " rrep %u", length));
#endif
}

static void
#ifdef INET6
aodv_v6_draft_01_rerr(netdissect_options *ndo,
                      const union aodv *ap, u_int length)
#else
aodv_v6_draft_01_rerr(netdissect_options *ndo,
                      const union aodv *ap _U_, u_int length)
#endif
{
#ifdef INET6
	const struct rerr_unreach6_draft_01 *dp6 = NULL;
	int i, j, n, trunc;

	i = length - offsetof(struct aodv_rerr, r);
	j = sizeof(ap->rerr.r.dest6_draft_01[0]);
	dp6 = &ap->rerr.r.dest6_draft_01[0];
	n = ap->rerr.rerr_dc * j;
	ND_PRINT((ndo, " rerr %s [items %u] [%u]:",
	    ap->rerr.rerr_flags & RERR_NODELETE ? "[D]" : "",
	    ap->rerr.rerr_dc, length));
	trunc = n - (i/j);
	for (; i -= j >= 0; ++dp6) {
		ND_PRINT((ndo, " {%s}(%ld)", ip6addr_string(ndo, &dp6->u_da),
		    (unsigned long)EXTRACT_32BITS(&dp6->u_ds)));
	}
	if (trunc)
		ND_PRINT((ndo, "[|rerr]"));
#else
	ND_PRINT((ndo, " rerr %u", length));
#endif
}

void
aodv_print(netdissect_options *ndo,
           const u_char *dat, u_int length, int is_ip6)
{
	const union aodv *ap;

	ap = (union aodv *)dat;
	if (ndo->ndo_snapend < dat) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	if (min(length, (u_int)(ndo->ndo_snapend - dat)) < sizeof(ap->rrep_ack)) {
		ND_PRINT((ndo, " [|aodv]"));
		return;
	}
	ND_PRINT((ndo, " aodv"));

	switch (ap->rerr.rerr_type) {

	case AODV_RREQ:
		if (is_ip6)
			aodv_v6_rreq(ndo, ap, dat, length);
		else
			aodv_rreq(ndo, ap, dat, length);
		break;

	case AODV_RREP:
		if (is_ip6)
			aodv_v6_rrep(ndo, ap, dat, length);
		else
			aodv_rrep(ndo, ap, dat, length);
		break;

	case AODV_RERR:
		if (is_ip6)
			aodv_v6_rerr(ndo, ap, length);
		else
			aodv_rerr(ndo, ap, dat, length);
		break;

	case AODV_RREP_ACK:
		ND_PRINT((ndo, " rrep-ack %u", length));
		break;

	case AODV_V6_DRAFT_01_RREQ:
		aodv_v6_draft_01_rreq(ndo, ap, dat, length);
		break;

	case AODV_V6_DRAFT_01_RREP:
		aodv_v6_draft_01_rrep(ndo, ap, dat, length);
		break;

	case AODV_V6_DRAFT_01_RERR:
		aodv_v6_draft_01_rerr(ndo, ap, length);
		break;

	case AODV_V6_DRAFT_01_RREP_ACK:
		ND_PRINT((ndo, " rrep-ack %u", length));
		break;

	default:
		ND_PRINT((ndo, " %u %u", ap->rreq.rreq_type, length));
	}
}
