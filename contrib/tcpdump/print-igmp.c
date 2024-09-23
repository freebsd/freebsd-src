/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994, 1995, 1996
 *  The Regents of the University of California.  All rights reserved.
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
 */

/* \summary: Internet Group Management Protocol (IGMP) printer */

/*
 * specification:
 *
 *	RFC 2236 for IGMPv2
 *	RFC 3376 for IGMPv3
 *	draft-asaeda-mboned-mtrace-v2 for the mtrace message
 */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#ifndef IN_CLASSD
#define IN_CLASSD(i) (((int32_t)(i) & 0xf0000000) == 0xe0000000)
#endif


/* (following from ipmulti/mrouted/prune.h) */

/*
 * The packet format for a traceroute request.
 */
struct tr_query {
    nd_uint32_t  tr_src;        /* traceroute source */
    nd_uint32_t  tr_dst;        /* traceroute destination */
    nd_uint32_t  tr_raddr;      /* traceroute response address */
    nd_uint8_t   tr_rttl;       /* response ttl */
    nd_uint24_t  tr_qid;        /* qid */
};

/*
 * Traceroute response format.  A traceroute response has a tr_query at the
 * beginning, followed by one tr_resp for each hop taken.
 */
struct tr_resp {
    nd_uint32_t tr_qarr;        /* query arrival time */
    nd_uint32_t tr_inaddr;      /* incoming interface address */
    nd_uint32_t tr_outaddr;     /* outgoing interface address */
    nd_uint32_t tr_rmtaddr;     /* parent address in source tree */
    nd_uint32_t tr_vifin;       /* input packet count on interface */
    nd_uint32_t tr_vifout;      /* output packet count on interface */
    nd_uint32_t tr_pktcnt;      /* total incoming packets for src-grp */
    nd_uint8_t  tr_rproto;      /* routing proto deployed on router */
    nd_uint8_t  tr_fttl;        /* ttl required to forward on outvif */
    nd_uint8_t  tr_smask;       /* subnet mask for src addr */
    nd_uint8_t  tr_rflags;      /* forwarding error codes */
};

/* defs within mtrace */
#define TR_QUERY 1
#define TR_RESP 2

/* fields for tr_rflags (forwarding error codes) */
#define TR_NO_ERR   0
#define TR_WRONG_IF 1
#define TR_PRUNED   2
#define TR_OPRUNED  3
#define TR_SCOPED   4
#define TR_NO_RTE   5
#define TR_NO_FWD   7
#define TR_NO_SPACE 0x81
#define TR_OLD_ROUTER   0x82

/* fields for tr_rproto (routing protocol) */
#define TR_PROTO_DVMRP  1
#define TR_PROTO_MOSPF  2
#define TR_PROTO_PIM    3
#define TR_PROTO_CBT    4

/* igmpv3 report types */
static const struct tok igmpv3report2str[] = {
	{ 1,	"is_in" },
	{ 2,	"is_ex" },
	{ 3,	"to_in" },
	{ 4,	"to_ex" },
	{ 5,	"allow" },
	{ 6,	"block" },
	{ 0,	NULL }
};

static void
print_mtrace(netdissect_options *ndo,
             const char *typename,
             const u_char *bp, u_int len)
{
    const struct tr_query *tr = (const struct tr_query *)(bp + 8);

    if (len < 8 + sizeof (struct tr_query)) {
	ND_PRINT(" [invalid len %u]", len);
	return;
    }
    ND_PRINT("%s %u: %s to %s reply-to %s",
        typename,
        GET_BE_U_3(tr->tr_qid),
        GET_IPADDR_STRING(tr->tr_src), GET_IPADDR_STRING(tr->tr_dst),
        GET_IPADDR_STRING(tr->tr_raddr));
    if (IN_CLASSD(GET_BE_U_4(tr->tr_raddr)))
        ND_PRINT(" with-ttl %u", GET_U_1(tr->tr_rttl));
}

static void
print_igmpv3_report(netdissect_options *ndo,
                    const u_char *bp, u_int len)
{
    u_int group, nsrcs, ngroups;
    u_int i, j;

    /* Minimum len is 16, and should be a multiple of 4 */
    if (len < 16 || len & 0x03) {
	ND_PRINT(" [invalid len %u]", len);
	return;
    }
    ngroups = GET_BE_U_2(bp + 6);
    ND_PRINT(", %u group record(s)", ngroups);
    if (ndo->ndo_vflag > 0) {
	/* Print the group records */
	group = 8;
        for (i=0; i<ngroups; i++) {
	    if (len < group+8) {
		ND_PRINT(" [invalid number of groups]");
		return;
	    }
            ND_PRINT(" [gaddr %s", GET_IPADDR_STRING(bp + group + 4));
	    ND_PRINT(" %s", tok2str(igmpv3report2str, " [v3-report-#%u]",
								GET_U_1(bp + group)));
            nsrcs = GET_BE_U_2(bp + group + 2);
	    /* Check the number of sources and print them */
	    if (len < group+8+(nsrcs<<2)) {
		ND_PRINT(" [invalid number of sources %u]", nsrcs);
		return;
	    }
            if (ndo->ndo_vflag == 1)
                ND_PRINT(", %u source(s)", nsrcs);
            else {
		/* Print the sources */
                ND_PRINT(" {");
                for (j=0; j<nsrcs; j++) {
		    ND_PRINT(" %s", GET_IPADDR_STRING(bp + group + 8 + (j << 2)));
		}
                ND_PRINT(" }");
            }
	    /* Next group record */
            group += 8 + (nsrcs << 2);
	    ND_PRINT("]");
        }
    }
}

static void
print_igmpv3_query(netdissect_options *ndo,
                   const u_char *bp, u_int len)
{
    u_int mrc;
    u_int mrt;
    u_int nsrcs;
    u_int i;

    ND_PRINT(" v3");
    /* Minimum len is 12, and should be a multiple of 4 */
    if (len < 12 || len & 0x03) {
	ND_PRINT(" [invalid len %u]", len);
	return;
    }
    mrc = GET_U_1(bp + 1);
    if (mrc < 128) {
	mrt = mrc;
    } else {
        mrt = ((mrc & 0x0f) | 0x10) << (((mrc & 0x70) >> 4) + 3);
    }
    if (mrc != 100) {
	ND_PRINT(" [max resp time ");
        if (mrt < 600) {
            ND_PRINT("%.1fs", mrt * 0.1);
        } else {
            unsigned_relts_print(ndo, mrt / 10);
        }
	ND_PRINT("]");
    }
    if (GET_BE_U_4(bp + 4) == 0)
	return;
    ND_PRINT(" [gaddr %s", GET_IPADDR_STRING(bp + 4));
    nsrcs = GET_BE_U_2(bp + 10);
    if (nsrcs > 0) {
	if (len < 12 + (nsrcs << 2))
	    ND_PRINT(" [invalid number of sources]");
	else if (ndo->ndo_vflag > 1) {
	    ND_PRINT(" {");
	    for (i=0; i<nsrcs; i++) {
		ND_PRINT(" %s", GET_IPADDR_STRING(bp + 12 + (i << 2)));
	    }
	    ND_PRINT(" }");
	} else
	    ND_PRINT(", %u source(s)", nsrcs);
    }
    ND_PRINT("]");
}

void
igmp_print(netdissect_options *ndo,
           const u_char *bp, u_int len)
{
    struct cksum_vec vec[1];

    ndo->ndo_protocol = "igmp";
    if (ndo->ndo_qflag) {
        ND_PRINT("igmp");
        return;
    }

    switch (GET_U_1(bp)) {
    case 0x11:
        ND_PRINT("igmp query");
	if (len >= 12)
	    print_igmpv3_query(ndo, bp, len);
	else {
	    if (GET_U_1(bp + 1)) {
		ND_PRINT(" v2");
		if (GET_U_1(bp + 1) != 100)
		    ND_PRINT(" [max resp time %u]", GET_U_1(bp + 1));
	    } else
		ND_PRINT(" v1");
	    if (GET_BE_U_4(bp + 4))
                ND_PRINT(" [gaddr %s]", GET_IPADDR_STRING(bp + 4));
            if (len != 8)
                ND_PRINT(" [len %u]", len);
	}
        break;
    case 0x12:
        ND_PRINT("igmp v1 report %s", GET_IPADDR_STRING(bp + 4));
        if (len != 8)
            ND_PRINT(" [len %u]", len);
        break;
    case 0x16:
        ND_PRINT("igmp v2 report %s", GET_IPADDR_STRING(bp + 4));
        break;
    case 0x22:
        ND_PRINT("igmp v3 report");
	print_igmpv3_report(ndo, bp, len);
        break;
    case 0x17:
        ND_PRINT("igmp leave %s", GET_IPADDR_STRING(bp + 4));
        break;
    case 0x13:
        ND_PRINT("igmp dvmrp");
        if (len < 8)
            ND_PRINT(" [len %u]", len);
        else
            dvmrp_print(ndo, bp, len);
        break;
    case 0x14:
        ND_PRINT("igmp pimv1");
        pimv1_print(ndo, bp, len);
        break;
    case 0x1e:
        print_mtrace(ndo, "mresp", bp, len);
        break;
    case 0x1f:
        print_mtrace(ndo, "mtrace", bp, len);
        break;
    default:
        ND_PRINT("igmp-%u", GET_U_1(bp));
        break;
    }

    if (ndo->ndo_vflag && len >= 4 && ND_TTEST_LEN(bp, len)) {
        /* Check the IGMP checksum */
        vec[0].ptr = bp;
        vec[0].len = len;
        if (in_cksum(vec, 1))
            ND_PRINT(" bad igmp cksum %x!", GET_BE_U_2(bp + 2));
    }
}
