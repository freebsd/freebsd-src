/*
 * Copyright (c) 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
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

/* \summary: Distance Vector Multicast Routing Protocol printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"

/*
 * See: RFC 1075 and draft-ietf-idmr-dvmrp-v3
 *
 * DVMRP message types and flag values shamelessly stolen from
 * mrouted/dvmrp.h.
 */
#define DVMRP_PROBE		1	/* for finding neighbors */
#define DVMRP_REPORT		2	/* for reporting some or all routes */
#define DVMRP_ASK_NEIGHBORS	3	/* sent by mapper, asking for a list */
					/* of this router's neighbors */
#define DVMRP_NEIGHBORS		4	/* response to such a request */
#define DVMRP_ASK_NEIGHBORS2	5	/* as above, want new format reply */
#define DVMRP_NEIGHBORS2	6
#define DVMRP_PRUNE		7	/* prune message */
#define DVMRP_GRAFT		8	/* graft message */
#define DVMRP_GRAFT_ACK		9	/* graft acknowledgement */
static const struct tok dvmrp_msgtype_str[] = {
	{ DVMRP_PROBE,          "Probe"              },
	{ DVMRP_REPORT,         "Report"             },
	{ DVMRP_ASK_NEIGHBORS,  "Ask-neighbors(old)" },
	{ DVMRP_NEIGHBORS,      "Neighbors(old)"     },
	{ DVMRP_ASK_NEIGHBORS2, "Ask-neighbors2"     },
	{ DVMRP_NEIGHBORS2,     "Neighbors2"         },
	{ DVMRP_PRUNE,          "Prune"              },
	{ DVMRP_GRAFT,          "Graft"              },
	{ DVMRP_GRAFT_ACK,      "Graft-ACK"          },
	{ 0, NULL }
};

/*
 * 'flags' byte values in DVMRP_NEIGHBORS2 reply.
 */
#define DVMRP_NF_TUNNEL		0x01	/* neighbors reached via tunnel */
#define DVMRP_NF_SRCRT		0x02	/* tunnel uses IP source routing */
#define DVMRP_NF_DOWN		0x10	/* kernel state of interface */
#define DVMRP_NF_DISABLED	0x20	/* administratively disabled */
#define DVMRP_NF_QUERIER	0x40	/* I am the subnet's querier */

static void print_probe(netdissect_options *, const u_char *, u_int);
static void print_report(netdissect_options *, const u_char *, u_int);
static void print_neighbors(netdissect_options *, const u_char *, u_int);
static void print_neighbors2(netdissect_options *, const u_char *, u_int, uint8_t, uint8_t);

void
dvmrp_print(netdissect_options *ndo,
            const u_char *bp, u_int len)
{
	u_char type;
	uint8_t major_version, minor_version;

	ndo->ndo_protocol = "dvmrp";
	if (len < 8) {
		ND_PRINT(" [length %u < 8]", len);
		goto invalid;
	}

	type = GET_U_1(bp + 1);

	/* Skip IGMP header */
	bp += 8;
	len -= 8;

	ND_PRINT(" %s", tok2str(dvmrp_msgtype_str, "[type %u]", type));
	switch (type) {

	case DVMRP_PROBE:
		if (ndo->ndo_vflag) {
			print_probe(ndo, bp, len);
		}
		break;

	case DVMRP_REPORT:
		if (ndo->ndo_vflag > 1) {
			print_report(ndo, bp, len);
		}
		break;

	case DVMRP_NEIGHBORS:
		print_neighbors(ndo, bp, len);
		break;

	case DVMRP_NEIGHBORS2:
		/*
		 * extract version from IGMP group address field
		 */
		bp -= 4;
		major_version = GET_U_1(bp + 3);
		minor_version = GET_U_1(bp + 2);
		bp += 4;
		print_neighbors2(ndo, bp, len, major_version, minor_version);
		break;

	case DVMRP_PRUNE:
		ND_PRINT(" src %s grp %s", GET_IPADDR_STRING(bp), GET_IPADDR_STRING(bp + 4));
		ND_PRINT(" timer ");
		unsigned_relts_print(ndo, GET_BE_U_4(bp + 8));
		break;

	case DVMRP_GRAFT:
		ND_PRINT(" src %s grp %s", GET_IPADDR_STRING(bp), GET_IPADDR_STRING(bp + 4));
		break;

	case DVMRP_GRAFT_ACK:
		ND_PRINT(" src %s grp %s", GET_IPADDR_STRING(bp), GET_IPADDR_STRING(bp + 4));
		break;
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static void
print_report(netdissect_options *ndo,
             const u_char *bp,
             u_int len)
{
	uint32_t mask, origin;
	u_int metric, done;
	u_int i, width;

	while (len > 0) {
		if (len < 3) {
			ND_PRINT(" [length %u < 3]", len);
			goto invalid;
		}
		mask = (uint32_t)0xff << 24 | GET_U_1(bp) << 16 |
			GET_U_1(bp + 1) << 8 | GET_U_1(bp + 2);
		width = 1;
		if (GET_U_1(bp))
			width = 2;
		if (GET_U_1(bp + 1))
			width = 3;
		if (GET_U_1(bp + 2))
			width = 4;

		ND_PRINT("\n\tMask %s", intoa(htonl(mask)));
		bp += 3;
		len -= 3;
		do {
			if (len < width + 1) {
				ND_PRINT("\n\t  [Truncated Report]");
				goto invalid;
			}
			origin = 0;
			for (i = 0; i < width; ++i) {
				origin = origin << 8 | GET_U_1(bp);
				bp++;
			}
			for ( ; i < 4; ++i)
				origin <<= 8;

			metric = GET_U_1(bp);
			bp++;
			done = metric & 0x80;
			metric &= 0x7f;
			ND_PRINT("\n\t  %s metric %u", intoa(htonl(origin)),
				metric);
			len -= width + 1;
		} while (!done);
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static void
print_probe(netdissect_options *ndo,
            const u_char *bp,
            u_int len)
{
	if (len < 4) {
		ND_PRINT(" [full length %u < 4]", len);
		goto invalid;
	}
	ND_PRINT(ndo->ndo_vflag > 1 ? "\n\t" : " ");
	ND_PRINT("genid %u", GET_BE_U_4(bp));
	if (ndo->ndo_vflag < 2)
		return;

	bp += 4;
	len -= 4;
	while (len > 0) {
		if (len < 4) {
			ND_PRINT("[remaining length %u < 4]", len);
			goto invalid;
		}
		ND_PRINT("\n\tneighbor %s", GET_IPADDR_STRING(bp));
		bp += 4; len -= 4;
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static void
print_neighbors(netdissect_options *ndo,
                const u_char *bp,
                u_int len)
{
	const u_char *laddr;
	u_char metric;
	u_char thresh;
	int ncount;

	while (len > 0) {
		if (len < 7) {
			ND_PRINT(" [length %u < 7]", len);
			goto invalid;
		}
		laddr = bp;
		bp += 4;
		metric = GET_U_1(bp);
		bp++;
		thresh = GET_U_1(bp);
		bp++;
		ncount = GET_U_1(bp);
		bp++;
		len -= 7;
		while (--ncount >= 0) {
			if (len < 4) {
				ND_PRINT(" [length %u < 4]", len);
				goto invalid;
			}
			ND_PRINT(" [%s ->", GET_IPADDR_STRING(laddr));
			ND_PRINT(" %s, (%u/%u)]",
				   GET_IPADDR_STRING(bp), metric, thresh);
			bp += 4;
			len -= 4;
		}
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static void
print_neighbors2(netdissect_options *ndo,
                 const u_char *bp,
                 u_int len, uint8_t major_version,
                 uint8_t minor_version)
{
	const u_char *laddr;
	u_char metric, thresh, flags;
	int ncount;

	ND_PRINT(" (v %u.%u):", major_version, minor_version);

	while (len > 0) {
		if (len < 8) {
			ND_PRINT(" [length %u < 8]", len);
			goto invalid;
		}
		laddr = bp;
		bp += 4;
		metric = GET_U_1(bp);
		bp++;
		thresh = GET_U_1(bp);
		bp++;
		flags = GET_U_1(bp);
		bp++;
		ncount = GET_U_1(bp);
		bp++;
		len -= 8;
		while (--ncount >= 0 && len > 0) {
			if (len < 4) {
				ND_PRINT(" [length %u < 4]", len);
				goto invalid;
			}
			ND_PRINT(" [%s -> ", GET_IPADDR_STRING(laddr));
			ND_PRINT("%s (%u/%u", GET_IPADDR_STRING(bp),
				     metric, thresh);
			if (flags & DVMRP_NF_TUNNEL)
				ND_PRINT("/tunnel");
			if (flags & DVMRP_NF_SRCRT)
				ND_PRINT("/srcrt");
			if (flags & DVMRP_NF_QUERIER)
				ND_PRINT("/querier");
			if (flags & DVMRP_NF_DISABLED)
				ND_PRINT("/disabled");
			if (flags & DVMRP_NF_DOWN)
				ND_PRINT("/down");
			ND_PRINT(")]");
			bp += 4;
			len -= 4;
		}
		if (ncount != -1) {
			ND_PRINT(" [invalid ncount]");
			goto invalid;
		}
	}
	return;

invalid:
	nd_print_invalid(ndo);
}
