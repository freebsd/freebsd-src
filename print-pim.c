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

/* \summary: Protocol Independent Multicast (PIM) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip.h"
#include "ip6.h"
#include "ipproto.h"

#define PIMV1_TYPE_QUERY           0
#define PIMV1_TYPE_REGISTER        1
#define PIMV1_TYPE_REGISTER_STOP   2
#define PIMV1_TYPE_JOIN_PRUNE      3
#define PIMV1_TYPE_RP_REACHABILITY 4
#define PIMV1_TYPE_ASSERT          5
#define PIMV1_TYPE_GRAFT           6
#define PIMV1_TYPE_GRAFT_ACK       7

static const struct tok pimv1_type_str[] = {
	{ PIMV1_TYPE_QUERY,           "Query"         },
	{ PIMV1_TYPE_REGISTER,        "Register"      },
	{ PIMV1_TYPE_REGISTER_STOP,   "Register-Stop" },
	{ PIMV1_TYPE_JOIN_PRUNE,      "Join/Prune"    },
	{ PIMV1_TYPE_RP_REACHABILITY, "RP-reachable"  },
	{ PIMV1_TYPE_ASSERT,          "Assert"        },
	{ PIMV1_TYPE_GRAFT,           "Graft"         },
	{ PIMV1_TYPE_GRAFT_ACK,       "Graft-ACK"     },
	{ 0, NULL }
};

#define PIMV2_TYPE_HELLO         0
#define PIMV2_TYPE_REGISTER      1
#define PIMV2_TYPE_REGISTER_STOP 2
#define PIMV2_TYPE_JOIN_PRUNE    3
#define PIMV2_TYPE_BOOTSTRAP     4
#define PIMV2_TYPE_ASSERT        5
#define PIMV2_TYPE_GRAFT         6
#define PIMV2_TYPE_GRAFT_ACK     7
#define PIMV2_TYPE_CANDIDATE_RP  8
#define PIMV2_TYPE_PRUNE_REFRESH 9
#define PIMV2_TYPE_DF_ELECTION   10
#define PIMV2_TYPE_ECMP_REDIRECT 11

static const struct tok pimv2_type_values[] = {
    { PIMV2_TYPE_HELLO,         "Hello" },
    { PIMV2_TYPE_REGISTER,      "Register" },
    { PIMV2_TYPE_REGISTER_STOP, "Register Stop" },
    { PIMV2_TYPE_JOIN_PRUNE,    "Join / Prune" },
    { PIMV2_TYPE_BOOTSTRAP,     "Bootstrap" },
    { PIMV2_TYPE_ASSERT,        "Assert" },
    { PIMV2_TYPE_GRAFT,         "Graft" },
    { PIMV2_TYPE_GRAFT_ACK,     "Graft Acknowledgement" },
    { PIMV2_TYPE_CANDIDATE_RP,  "Candidate RP Advertisement" },
    { PIMV2_TYPE_PRUNE_REFRESH, "Prune Refresh" },
    { PIMV2_TYPE_DF_ELECTION,   "DF Election" },
    { PIMV2_TYPE_ECMP_REDIRECT, "ECMP Redirect" },
    { 0, NULL}
};

#define PIMV2_HELLO_OPTION_HOLDTIME             1
#define PIMV2_HELLO_OPTION_LANPRUNEDELAY        2
#define PIMV2_HELLO_OPTION_DR_PRIORITY_OLD     18
#define PIMV2_HELLO_OPTION_DR_PRIORITY         19
#define PIMV2_HELLO_OPTION_GENID               20
#define PIMV2_HELLO_OPTION_REFRESH_CAP         21
#define PIMV2_HELLO_OPTION_BIDIR_CAP           22
#define PIMV2_HELLO_OPTION_ADDRESS_LIST        24
#define PIMV2_HELLO_OPTION_ADDRESS_LIST_OLD 65001

static const struct tok pimv2_hello_option_values[] = {
    { PIMV2_HELLO_OPTION_HOLDTIME,         "Hold Time" },
    { PIMV2_HELLO_OPTION_LANPRUNEDELAY,    "LAN Prune Delay" },
    { PIMV2_HELLO_OPTION_DR_PRIORITY_OLD,  "DR Priority (Old)" },
    { PIMV2_HELLO_OPTION_DR_PRIORITY,      "DR Priority" },
    { PIMV2_HELLO_OPTION_GENID,            "Generation ID" },
    { PIMV2_HELLO_OPTION_REFRESH_CAP,      "State Refresh Capability" },
    { PIMV2_HELLO_OPTION_BIDIR_CAP,        "Bi-Directional Capability" },
    { PIMV2_HELLO_OPTION_ADDRESS_LIST,     "Address List" },
    { PIMV2_HELLO_OPTION_ADDRESS_LIST_OLD, "Address List (Old)" },
    { 0, NULL}
};

#define PIMV2_REGISTER_FLAG_LEN      4
#define PIMV2_REGISTER_FLAG_BORDER 0x80000000
#define PIMV2_REGISTER_FLAG_NULL   0x40000000

static const struct tok pimv2_register_flag_values[] = {
    { PIMV2_REGISTER_FLAG_BORDER, "Border" },
    { PIMV2_REGISTER_FLAG_NULL, "Null" },
    { 0, NULL}
};

#define PIMV2_DF_ELECTION_OFFER                  1
#define PIMV2_DF_ELECTION_WINNER                 2
#define PIMV2_DF_ELECTION_BACKOFF                3
#define PIMV2_DF_ELECTION_PASS                   4

static const struct tok pimv2_df_election_flag_values[] = {
    { PIMV2_DF_ELECTION_OFFER, "Offer" },
    { PIMV2_DF_ELECTION_WINNER, "Winner" },
    { PIMV2_DF_ELECTION_BACKOFF, "Backoff" },
    { PIMV2_DF_ELECTION_PASS, "Pass" },
    { 0, NULL}
};

#define PIMV2_DF_ELECTION_PASS_BACKOFF_STR(x)   ( \
      x == PIMV2_DF_ELECTION_BACKOFF ? "offer" : "new winner" )


/*
 * XXX: We consider a case where IPv6 is not ready yet for portability,
 * but PIM dependent definitions should be independent of IPv6...
 */

struct pim {
	nd_uint8_t	pim_typever;
			/* upper 4bit: PIM version number; 2 for PIMv2 */
			/* lower 4bit: the PIM message type, currently they are:
			 * Hello, Register, Register-Stop, Join/Prune,
			 * Bootstrap, Assert, Graft (PIM-DM only),
			 * Graft-Ack (PIM-DM only), C-RP-Adv
			 */
#define PIM_VER(x)	(((x) & 0xf0) >> 4)
#define PIM_TYPE(x)	((x) & 0x0f)
	nd_uint8_t	pim_rsv;	/* Reserved in v1, subtype+address length in v2 */
#define PIM_SUBTYPE(x)  (((x) & 0xf0) >> 4)
	nd_uint16_t	pim_cksum;	/* IP style check sum */
};

static void pimv2_print(netdissect_options *, const u_char *bp, u_int len, const u_char *);

static void
pimv1_join_prune_print(netdissect_options *ndo,
                       const u_char *bp, u_int len)
{
	u_int ngroups, njoin, nprune;
	u_int njp;

	/* If it's a single group and a single source, use 1-line output. */
	if (ND_TTEST_LEN(bp, 30) && GET_U_1(bp + 11) == 1 &&
	    ((njoin = GET_BE_U_2(bp + 20)) + GET_BE_U_2(bp + 22)) == 1) {
		u_int hold;

		ND_PRINT(" RPF %s ", GET_IPADDR_STRING(bp));
		hold = GET_BE_U_2(bp + 6);
		if (hold != 180) {
			ND_PRINT("Hold ");
			unsigned_relts_print(ndo, hold);
		}
		ND_PRINT("%s (%s/%u, %s", njoin ? "Join" : "Prune",
		GET_IPADDR_STRING(bp + 26), GET_U_1(bp + 25) & 0x3f,
		GET_IPADDR_STRING(bp + 12));
		if (GET_BE_U_4(bp + 16) != 0xffffffff)
			ND_PRINT("/%s", GET_IPADDR_STRING(bp + 16));
		ND_PRINT(") %s%s %s",
		    (GET_U_1(bp + 24) & 0x01) ? "Sparse" : "Dense",
		    (GET_U_1(bp + 25) & 0x80) ? " WC" : "",
		    (GET_U_1(bp + 25) & 0x40) ? "RP" : "SPT");
		return;
	}

	if (len < sizeof(nd_ipv4))
		goto trunc;
	if (ndo->ndo_vflag > 1)
		ND_PRINT("\n");
	ND_PRINT(" Upstream Nbr: %s", GET_IPADDR_STRING(bp));
	bp += 4;
	len -= 4;
	if (len < 4)
		goto trunc;
	if (ndo->ndo_vflag > 1)
		ND_PRINT("\n");
	ND_PRINT(" Hold time: ");
	unsigned_relts_print(ndo, GET_BE_U_2(bp + 2));
	if (ndo->ndo_vflag < 2)
		return;
	bp += 4;
	len -= 4;

	if (len < 4)
		goto trunc;
	ngroups = GET_U_1(bp + 3);
	bp += 4;
	len -= 4;
	while (ngroups != 0) {
		/*
		 * XXX - does the address have length "addrlen" and the
		 * mask length "maddrlen"?
		 */
		if (len < 4)
			goto trunc;
		ND_PRINT("\n\tGroup: %s", GET_IPADDR_STRING(bp));
		bp += 4;
		len -= 4;
		if (len < 4)
			goto trunc;
		if (GET_BE_U_4(bp) != 0xffffffff)
			ND_PRINT("/%s", GET_IPADDR_STRING(bp));
		bp += 4;
		len -= 4;
		if (len < 4)
			goto trunc;
		njoin = GET_BE_U_2(bp);
		nprune = GET_BE_U_2(bp + 2);
		ND_PRINT(" joined: %u pruned: %u", njoin, nprune);
		bp += 4;
		len -= 4;
		for (njp = 0; njp < (njoin + nprune); njp++) {
			const char *type;

			if (njp < njoin)
				type = "Join ";
			else
				type = "Prune";
			if (len < 6)
				goto trunc;
			ND_PRINT("\n\t%s %s%s%s%s/%u", type,
			    (GET_U_1(bp) & 0x01) ? "Sparse " : "Dense ",
			    (GET_U_1(bp + 1) & 0x80) ? "WC " : "",
			    (GET_U_1(bp + 1) & 0x40) ? "RP " : "SPT ",
			    GET_IPADDR_STRING(bp + 2),
			    GET_U_1(bp + 1) & 0x3f);
			bp += 6;
			len -= 6;
		}
		ngroups--;
	}
	return;
trunc:
	nd_print_trunc(ndo);
}

void
pimv1_print(netdissect_options *ndo,
            const u_char *bp, u_int len)
{
	u_char type;

	ndo->ndo_protocol = "pimv1";
	type = GET_U_1(bp + 1);

	ND_PRINT(" %s", tok2str(pimv1_type_str, "[type %u]", type));
	switch (type) {
	case PIMV1_TYPE_QUERY:
		if (ND_TTEST_1(bp + 8)) {
			switch (GET_U_1(bp + 8) >> 4) {
			case 0:
				ND_PRINT(" Dense-mode");
				break;
			case 1:
				ND_PRINT(" Sparse-mode");
				break;
			case 2:
				ND_PRINT(" Sparse-Dense-mode");
				break;
			default:
				ND_PRINT(" mode-%u", GET_U_1(bp + 8) >> 4);
				break;
			}
		}
		if (ndo->ndo_vflag) {
			ND_PRINT(" (Hold-time ");
			unsigned_relts_print(ndo, GET_BE_U_2(bp + 10));
			ND_PRINT(")");
		}
		break;

	case PIMV1_TYPE_REGISTER:
		ND_TCHECK_LEN(bp + 8, 20);			/* ip header */
		ND_PRINT(" for %s > %s", GET_IPADDR_STRING(bp + 20),
			  GET_IPADDR_STRING(bp + 24));
		break;
	case PIMV1_TYPE_REGISTER_STOP:
		ND_PRINT(" for %s > %s", GET_IPADDR_STRING(bp + 8),
			  GET_IPADDR_STRING(bp + 12));
		break;
	case PIMV1_TYPE_RP_REACHABILITY:
		if (ndo->ndo_vflag) {
			ND_PRINT(" group %s", GET_IPADDR_STRING(bp + 8));
			if (GET_BE_U_4(bp + 12) != 0xffffffff)
				ND_PRINT("/%s", GET_IPADDR_STRING(bp + 12));
			ND_PRINT(" RP %s hold ", GET_IPADDR_STRING(bp + 16));
			unsigned_relts_print(ndo, GET_BE_U_2(bp + 22));
		}
		break;
	case PIMV1_TYPE_ASSERT:
		ND_PRINT(" for %s > %s", GET_IPADDR_STRING(bp + 16),
			  GET_IPADDR_STRING(bp + 8));
		if (GET_BE_U_4(bp + 12) != 0xffffffff)
			ND_PRINT("/%s", GET_IPADDR_STRING(bp + 12));
		ND_PRINT(" %s pref %u metric %u",
		    (GET_U_1(bp + 20) & 0x80) ? "RP-tree" : "SPT",
		    GET_BE_U_4(bp + 20) & 0x7fffffff,
		    GET_BE_U_4(bp + 24));
		break;
	case PIMV1_TYPE_JOIN_PRUNE:
	case PIMV1_TYPE_GRAFT:
	case PIMV1_TYPE_GRAFT_ACK:
		if (ndo->ndo_vflag) {
			if (len < 8)
				goto trunc;
			pimv1_join_prune_print(ndo, bp + 8, len - 8);
		}
		break;
	}
	if ((GET_U_1(bp + 4) >> 4) != 1)
		ND_PRINT(" [v%u]", GET_U_1(bp + 4) >> 4);
	return;

trunc:
	nd_print_trunc(ndo);
}

/*
 * auto-RP is a cisco protocol, documented at
 * ftp://ftpeng.cisco.com/ipmulticast/specs/pim-autorp-spec01.txt
 *
 * This implements version 1+, dated Sept 9, 1998.
 */
void
cisco_autorp_print(netdissect_options *ndo,
                   const u_char *bp, u_int len)
{
	u_int type;
	u_int numrps;
	u_int hold;

	ndo->ndo_protocol = "cisco_autorp";
	if (len < 8)
		goto trunc;
	ND_PRINT(" auto-rp ");
	type = GET_U_1(bp);
	switch (type) {
	case 0x11:
		ND_PRINT("candidate-advert");
		break;
	case 0x12:
		ND_PRINT("mapping");
		break;
	default:
		ND_PRINT("type-0x%02x", type);
		break;
	}

	numrps = GET_U_1(bp + 1);

	ND_PRINT(" Hold ");
	hold = GET_BE_U_2(bp + 2);
	if (hold)
		unsigned_relts_print(ndo, GET_BE_U_2(bp + 2));
	else
		ND_PRINT("FOREVER");

	/* Next 4 bytes are reserved. */

	bp += 8; len -= 8;

	/*XXX skip unless -v? */

	/*
	 * Rest of packet:
	 * numrps entries of the form:
	 * 32 bits: RP
	 * 6 bits: reserved
	 * 2 bits: PIM version supported, bit 0 is "supports v1", 1 is "v2".
	 * 8 bits: # of entries for this RP
	 * each entry: 7 bits: reserved, 1 bit: negative,
	 *	       8 bits: mask 32 bits: source
	 * lather, rinse, repeat.
	 */
	while (numrps != 0) {
		u_int nentries;
		char s;

		if (len < 4)
			goto trunc;
		ND_PRINT(" RP %s", GET_IPADDR_STRING(bp));
		bp += 4;
		len -= 4;
		if (len < 1)
			goto trunc;
		switch (GET_U_1(bp) & 0x3) {
		case 0: ND_PRINT(" PIMv?");
			break;
		case 1:	ND_PRINT(" PIMv1");
			break;
		case 2:	ND_PRINT(" PIMv2");
			break;
		case 3:	ND_PRINT(" PIMv1+2");
			break;
		}
		if (GET_U_1(bp) & 0xfc)
			ND_PRINT(" [rsvd=0x%02x]", GET_U_1(bp) & 0xfc);
		bp += 1;
		len -= 1;
		if (len < 1)
			goto trunc;
		nentries = GET_U_1(bp);
		bp += 1;
		len -= 1;
		s = ' ';
		while (nentries != 0) {
			if (len < 6)
				goto trunc;
			ND_PRINT("%c%s%s/%u", s, GET_U_1(bp) & 1 ? "!" : "",
			          GET_IPADDR_STRING(bp + 2), GET_U_1(bp + 1));
			if (GET_U_1(bp) & 0x02) {
				ND_PRINT(" bidir");
			}
			if (GET_U_1(bp) & 0xfc) {
				ND_PRINT("[rsvd=0x%02x]", GET_U_1(bp) & 0xfc);
			}
			s = ',';
			bp += 6; len -= 6;
			nentries--;
		}
		numrps--;
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

void
pim_print(netdissect_options *ndo,
          const u_char *bp, u_int len, const u_char *bp2)
{
	const struct pim *pim = (const struct pim *)bp;
	uint8_t pim_typever;

	ndo->ndo_protocol = "pim";

	pim_typever = GET_U_1(pim->pim_typever);
	switch (PIM_VER(pim_typever)) {
	case 2:
		if (!ndo->ndo_vflag) {
			ND_PRINT("PIMv%u, %s, length %u",
			          PIM_VER(pim_typever),
			          tok2str(pimv2_type_values,"Unknown Type",PIM_TYPE(pim_typever)),
			          len);
			return;
		} else {
			ND_PRINT("PIMv%u, length %u\n\t%s",
			          PIM_VER(pim_typever),
			          len,
			          tok2str(pimv2_type_values,"Unknown Type",PIM_TYPE(pim_typever)));
			pimv2_print(ndo, bp, len, bp2);
		}
		break;
	default:
		ND_PRINT("PIMv%u, length %u",
		          PIM_VER(pim_typever),
		          len);
		break;
	}
}

/*
 * PIMv2 uses encoded address representations.
 *
 * The last PIM-SM I-D before RFC2117 was published specified the
 * following representation for unicast addresses.  However, RFC2117
 * specified no encoding for unicast addresses with the unicast
 * address length specified in the header.  Therefore, we have to
 * guess which encoding is being used (Cisco's PIMv2 implementation
 * uses the non-RFC encoding).  RFC2117 turns a previously "Reserved"
 * field into a 'unicast-address-length-in-bytes' field.  We guess
 * that it's the draft encoding if this reserved field is zero.
 *
 * RFC2362 goes back to the encoded format, and calls the addr length
 * field "reserved" again.
 *
 * The first byte is the address family, from:
 *
 *    0    Reserved
 *    1    IP (IP version 4)
 *    2    IP6 (IP version 6)
 *    3    NSAP
 *    4    HDLC (8-bit multidrop)
 *    5    BBN 1822
 *    6    802 (includes all 802 media plus Ethernet "canonical format")
 *    7    E.163
 *    8    E.164 (SMDS, Frame Relay, ATM)
 *    9    F.69 (Telex)
 *   10    X.121 (X.25, Frame Relay)
 *   11    IPX
 *   12    Appletalk
 *   13    Decnet IV
 *   14    Banyan Vines
 *   15    E.164 with NSAP format subaddress
 *
 * In addition, the second byte is an "Encoding".  0 is the default
 * encoding for the address family, and no other encodings are currently
 * specified.
 *
 */

enum pimv2_addrtype {
	pimv2_unicast, pimv2_group, pimv2_source
};

/*  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Addr Family   | Encoding Type |     Unicast Address           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+++++++
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Addr Family   | Encoding Type |   Reserved    |  Mask Len     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                Group multicast Address                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Addr Family   | Encoding Type | Rsrvd   |S|W|R|  Mask Len     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Source Address                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
static int
pimv2_addr_print(netdissect_options *ndo,
                 const u_char *bp, u_int len, enum pimv2_addrtype at,
                 u_int addr_len, int silent)
{
	u_int af;
	int hdrlen;

	if (addr_len == 0) {
		if (len < 2)
			goto trunc;
		switch (GET_U_1(bp)) {
		case 1:
			af = AF_INET;
			addr_len = (u_int)sizeof(nd_ipv4);
			break;
		case 2:
			af = AF_INET6;
			addr_len = (u_int)sizeof(nd_ipv6);
			break;
		default:
			return -1;
		}
		if (GET_U_1(bp + 1) != 0)
			return -1;
		hdrlen = 2;
	} else {
		switch (addr_len) {
		case sizeof(nd_ipv4):
			af = AF_INET;
			break;
		case sizeof(nd_ipv6):
			af = AF_INET6;
			break;
		default:
			return -1;
			break;
		}
		hdrlen = 0;
	}

	bp += hdrlen;
	len -= hdrlen;
	switch (at) {
	case pimv2_unicast:
		if (len < addr_len)
			goto trunc;
		ND_TCHECK_LEN(bp, addr_len);
		if (af == AF_INET) {
			if (!silent)
				ND_PRINT("%s", GET_IPADDR_STRING(bp));
		} else if (af == AF_INET6) {
			if (!silent)
				ND_PRINT("%s", GET_IP6ADDR_STRING(bp));
		}
		return hdrlen + addr_len;
	case pimv2_group:
	case pimv2_source:
		if (len < addr_len + 2)
			goto trunc;
		ND_TCHECK_LEN(bp, addr_len + 2);
		if (af == AF_INET) {
			if (!silent) {
				ND_PRINT("%s", GET_IPADDR_STRING(bp + 2));
				if (GET_U_1(bp + 1) != 32)
					ND_PRINT("/%u", GET_U_1(bp + 1));
			}
		} else if (af == AF_INET6) {
			if (!silent) {
				ND_PRINT("%s", GET_IP6ADDR_STRING(bp + 2));
				if (GET_U_1(bp + 1) != 128)
					ND_PRINT("/%u", GET_U_1(bp + 1));
			}
		}
		if (GET_U_1(bp) && !silent) {
			if (at == pimv2_group) {
				ND_PRINT("(0x%02x)", GET_U_1(bp));
			} else {
				ND_PRINT("(%s%s%s",
					GET_U_1(bp) & 0x04 ? "S" : "",
					GET_U_1(bp) & 0x02 ? "W" : "",
					GET_U_1(bp) & 0x01 ? "R" : "");
				if (GET_U_1(bp) & 0xf8) {
					ND_PRINT("+0x%02x",
						 GET_U_1(bp) & 0xf8);
				}
				ND_PRINT(")");
			}
		}
		return hdrlen + 2 + addr_len;
	default:
		return -1;
	}
trunc:
	return -1;
}

enum checksum_status {
	CORRECT,
	INCORRECT,
	UNVERIFIED
};

static enum checksum_status
pimv2_check_checksum(netdissect_options *ndo, const u_char *bp,
		     const u_char *bp2, u_int len)
{
	const struct ip *ip;
	u_int cksum;

	if (!ND_TTEST_LEN(bp, len)) {
		/* We don't have all the data. */
		return (UNVERIFIED);
	}
	ip = (const struct ip *)bp2;
	if (IP_V(ip) == 4) {
		struct cksum_vec vec[1];

		vec[0].ptr = bp;
		vec[0].len = len;
		cksum = in_cksum(vec, 1);
		return (cksum ? INCORRECT : CORRECT);
	} else if (IP_V(ip) == 6) {
		const struct ip6_hdr *ip6;

		ip6 = (const struct ip6_hdr *)bp2;
		cksum = nextproto6_cksum(ndo, ip6, bp, len, len, IPPROTO_PIM);
		return (cksum ? INCORRECT : CORRECT);
	} else {
		return (UNVERIFIED);
	}
}

static void
pimv2_print(netdissect_options *ndo,
            const u_char *bp, u_int len, const u_char *bp2)
{
	const struct pim *pim = (const struct pim *)bp;
	int advance;
	int subtype;
	enum checksum_status cksum_status;
	u_int pim_typever;
	u_int pimv2_addr_len;

	ndo->ndo_protocol = "pimv2";
	if (len < 2) {
		ND_PRINT("[length %u < 2]", len);
		nd_print_invalid(ndo);
		return;
	}
	pim_typever = GET_U_1(pim->pim_typever);
	/* RFC5015 allocates the high 4 bits of pim_rsv for "subtype". */
	pimv2_addr_len = GET_U_1(pim->pim_rsv) & 0x0f;
	if (pimv2_addr_len != 0)
		ND_PRINT(", RFC2117-encoding");

	if (len < 4) {
		ND_PRINT("[length %u < 4]", len);
		nd_print_invalid(ndo);
		return;
	}
	ND_PRINT(", cksum 0x%04x ", GET_BE_U_2(pim->pim_cksum));
	if (GET_BE_U_2(pim->pim_cksum) == 0) {
		ND_PRINT("(unverified)");
	} else {
		if (PIM_TYPE(pim_typever) == PIMV2_TYPE_REGISTER) {
			/*
			 * The checksum only covers the packet header,
			 * not the encapsulated packet.
			 */
			cksum_status = pimv2_check_checksum(ndo, bp, bp2, 8);
			if (cksum_status == INCORRECT) {
				/*
				 * To quote RFC 4601, "For interoperability
				 * reasons, a message carrying a checksum
				 * calculated over the entire PIM Register
				 * message should also be accepted."
				 */
				cksum_status = pimv2_check_checksum(ndo, bp, bp2, len);
			}
		} else {
			/*
			 * The checksum covers the entire packet.
			 */
			cksum_status = pimv2_check_checksum(ndo, bp, bp2, len);
		}
		switch (cksum_status) {

		case CORRECT:
			ND_PRINT("(correct)");
			break;

		case INCORRECT:
			ND_PRINT("(incorrect)");
			break;

		case UNVERIFIED:
			ND_PRINT("(unverified)");
			break;
		}
	}
	bp += 4;
	len -= 4;

	switch (PIM_TYPE(pim_typever)) {
	case PIMV2_TYPE_HELLO:
	    {
		uint16_t otype, olen;
		while (len > 0) {
			if (len < 4)
				goto trunc;
			otype = GET_BE_U_2(bp);
			olen = GET_BE_U_2(bp + 2);
			ND_PRINT("\n\t  %s Option (%u), length %u, Value: ",
			          tok2str(pimv2_hello_option_values, "Unknown", otype),
			          otype,
			          olen);
			bp += 4;
			len -= 4;

			if (len < olen)
				goto trunc;
			ND_TCHECK_LEN(bp, olen);
			switch (otype) {
			case PIMV2_HELLO_OPTION_HOLDTIME:
				if (olen != 2) {
					ND_PRINT("[option length %u != 2]", olen);
					nd_print_invalid(ndo);
					return;
				} else {
					unsigned_relts_print(ndo,
							     GET_BE_U_2(bp));
				}
				break;

			case PIMV2_HELLO_OPTION_LANPRUNEDELAY:
				if (olen != 4) {
					ND_PRINT("[option length %u != 4]", olen);
					nd_print_invalid(ndo);
					return;
				} else {
					char t_bit;
					uint16_t lan_delay, override_interval;
					lan_delay = GET_BE_U_2(bp);
					override_interval = GET_BE_U_2(bp + 2);
					t_bit = (lan_delay & 0x8000)? 1 : 0;
					lan_delay &= ~0x8000;
					ND_PRINT("\n\t    T-bit=%u, LAN delay %ums, Override interval %ums",
					t_bit, lan_delay, override_interval);
				}
				break;

			case PIMV2_HELLO_OPTION_DR_PRIORITY_OLD:
			case PIMV2_HELLO_OPTION_DR_PRIORITY:
				switch (olen) {
				case 0:
					ND_PRINT("Bi-Directional Capability (Old)");
					break;
				case 4:
					ND_PRINT("%u", GET_BE_U_4(bp));
					break;
				default:
					ND_PRINT("[option length %u != 4]", olen);
					nd_print_invalid(ndo);
					return;
					break;
				}
				break;

			case PIMV2_HELLO_OPTION_GENID:
				if (olen != 4) {
					ND_PRINT("[option length %u != 4]", olen);
					nd_print_invalid(ndo);
					return;
				} else {
					ND_PRINT("0x%08x", GET_BE_U_4(bp));
				}
				break;

			case PIMV2_HELLO_OPTION_REFRESH_CAP:
				if (olen != 4) {
					ND_PRINT("[option length %u != 4]", olen);
					nd_print_invalid(ndo);
					return;
				} else {
					ND_PRINT("v%u", GET_U_1(bp));
					if (GET_U_1(bp + 1) != 0) {
						ND_PRINT(", interval ");
						unsigned_relts_print(ndo,
								     GET_U_1(bp + 1));
					}
					if (GET_BE_U_2(bp + 2) != 0) {
						ND_PRINT(" ?0x%04x?",
							 GET_BE_U_2(bp + 2));
					}
				}
				break;

			case  PIMV2_HELLO_OPTION_BIDIR_CAP:
				break;

			case PIMV2_HELLO_OPTION_ADDRESS_LIST_OLD:
			case PIMV2_HELLO_OPTION_ADDRESS_LIST:
				if (ndo->ndo_vflag > 1) {
					const u_char *ptr = bp;
					u_int plen = len;
					while (ptr < (bp+olen)) {
						ND_PRINT("\n\t    ");
						advance = pimv2_addr_print(ndo, ptr, plen, pimv2_unicast, pimv2_addr_len, 0);
						if (advance < 0)
							goto trunc;
						ptr += advance;
						plen -= advance;
					}
				}
				break;
			default:
				if (ndo->ndo_vflag <= 1)
					print_unknown_data(ndo, bp, "\n\t    ", olen);
				break;
			}
			/* do we want to see an additionally hexdump ? */
			if (ndo->ndo_vflag> 1)
				print_unknown_data(ndo, bp, "\n\t    ", olen);
			bp += olen;
			len -= olen;
		}
		break;
	    }

	case PIMV2_TYPE_REGISTER:
	{
		const struct ip *ip;

		if (len < 4)
			goto trunc;
		ND_TCHECK_LEN(bp, PIMV2_REGISTER_FLAG_LEN);

		ND_PRINT(", Flags [ %s ]\n\t",
		          tok2str(pimv2_register_flag_values,
		          "none",
		          GET_BE_U_4(bp)));

		bp += 4; len -= 4;
		/* encapsulated multicast packet */
		if (len == 0)
			goto trunc;
		ip = (const struct ip *)bp;
		switch (IP_V(ip)) {
                case 0: /* Null header */
			ND_PRINT("IP-Null-header %s > %s",
			          GET_IPADDR_STRING(ip->ip_src),
			          GET_IPADDR_STRING(ip->ip_dst));
			break;

		case 4:	/* IPv4 */
			ip_print(ndo, bp, len);
			break;

		case 6:	/* IPv6 */
			ip6_print(ndo, bp, len);
			break;

		default:
			ND_PRINT("IP ver %u", IP_V(ip));
			break;
		}
		break;
	}

	case PIMV2_TYPE_REGISTER_STOP:
		ND_PRINT(" group=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		ND_PRINT(" source=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		break;

	case PIMV2_TYPE_JOIN_PRUNE:
	case PIMV2_TYPE_GRAFT:
	case PIMV2_TYPE_GRAFT_ACK:


        /*
         * 0                   1                   2                   3
         *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |PIM Ver| Type  | Addr length   |           Checksum            |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |             Unicast-Upstream Neighbor Address                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |  Reserved     | Num groups    |          Holdtime             |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |            Encoded-Multicast Group Address-1                  |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |   Number of Joined  Sources   |   Number of Pruned Sources    |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Joined Source Address-1                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                             .                                 |
         *  |                             .                                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Joined Source Address-n                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Pruned Source Address-1                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                             .                                 |
         *  |                             .                                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Pruned Source Address-n                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                           .                                   |
         *  |                           .                                   |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                Encoded-Multicast Group Address-n              |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

	    {
		uint8_t ngroup;
		uint16_t holdtime;
		uint16_t njoin;
		uint16_t nprune;
		u_int i, j;

		if (PIM_TYPE(pim_typever) != 7) {	/*not for Graft-ACK*/
			ND_PRINT(", upstream-neighbor: ");
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance; len -= advance;
		}
		if (len < 4)
			goto trunc;
		ND_TCHECK_4(bp);
		ngroup = GET_U_1(bp + 1);
		holdtime = GET_BE_U_2(bp + 2);
		ND_PRINT("\n\t  %u group(s)", ngroup);
		if (PIM_TYPE(pim_typever) != 7) {	/*not for Graft-ACK*/
			ND_PRINT(", holdtime: ");
			if (holdtime == 0xffff)
				ND_PRINT("infinite");
			else
				unsigned_relts_print(ndo, holdtime);
		}
		bp += 4; len -= 4;
		for (i = 0; i < ngroup; i++) {
			ND_PRINT("\n\t    group #%u: ", i+1);
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance; len -= advance;
			if (len < 4)
				goto trunc;
			ND_TCHECK_4(bp);
			njoin = GET_BE_U_2(bp);
			nprune = GET_BE_U_2(bp + 2);
			ND_PRINT(", joined sources: %u, pruned sources: %u", njoin, nprune);
			bp += 4; len -= 4;
			for (j = 0; j < njoin; j++) {
				ND_PRINT("\n\t      joined source #%u: ", j+1);
				if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_source, pimv2_addr_len, 0)) < 0)
					goto trunc;
				bp += advance; len -= advance;
			}
			for (j = 0; j < nprune; j++) {
				ND_PRINT("\n\t      pruned source #%u: ", j+1);
				if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_source, pimv2_addr_len, 0)) < 0)
					goto trunc;
				bp += advance; len -= advance;
			}
		}
		break;
	    }

	case PIMV2_TYPE_BOOTSTRAP:
	{
		u_int i, j, frpcnt;

		/* Fragment Tag, Hash Mask len, and BSR-priority */
		if (len < 2)
			goto trunc;
		ND_PRINT(" tag=%x", GET_BE_U_2(bp));
		bp += 2;
		len -= 2;
		if (len < 1)
			goto trunc;
		ND_PRINT(" hashmlen=%u", GET_U_1(bp));
		if (len < 2)
			goto trunc;
		ND_TCHECK_1(bp + 2);
		ND_PRINT(" BSRprio=%u", GET_U_1(bp + 1));
		bp += 2;
		len -= 2;

		/* Encoded-Unicast-BSR-Address */
		ND_PRINT(" BSR=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;

		for (i = 0; len > 0; i++) {
			/* Encoded-Group Address */
			ND_PRINT(" (group%u: ", i);
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance;
			len -= advance;

			/* RP-Count, Frag RP-Cnt, and rsvd */
			if (len < 1)
				goto trunc;
			ND_PRINT(" RPcnt=%u", GET_U_1(bp));
			if (len < 2)
				goto trunc;
			frpcnt = GET_U_1(bp + 1);
			ND_PRINT(" FRPcnt=%u", frpcnt);
			if (len < 4)
				goto trunc;
			bp += 4;
			len -= 4;

			for (j = 0; j < frpcnt && len > 0; j++) {
				/* each RP info */
				ND_PRINT(" RP%u=", j);
				if ((advance = pimv2_addr_print(ndo, bp, len,
								pimv2_unicast,
								pimv2_addr_len,
								0)) < 0)
					goto trunc;
				bp += advance;
				len -= advance;

				if (len < 2)
					goto trunc;
				ND_PRINT(",holdtime=");
				unsigned_relts_print(ndo,
						     GET_BE_U_2(bp));
				if (len < 3)
					goto trunc;
				ND_PRINT(",prio=%u", GET_U_1(bp + 2));
				if (len < 4)
					goto trunc;
				bp += 4;
				len -= 4;
			}
			ND_PRINT(")");
		}
		break;
	}
	case PIMV2_TYPE_ASSERT:
		ND_PRINT(" group=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		ND_PRINT(" src=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		if (len < 8)
			goto trunc;
		ND_TCHECK_8(bp);
		if (GET_U_1(bp) & 0x80)
			ND_PRINT(" RPT");
		ND_PRINT(" pref=%u", GET_BE_U_4(bp) & 0x7fffffff);
		ND_PRINT(" metric=%u", GET_BE_U_4(bp + 4));
		break;

	case PIMV2_TYPE_CANDIDATE_RP:
	{
		u_int i, pfxcnt;

		/* Prefix-Cnt, Priority, and Holdtime */
		if (len < 1)
			goto trunc;
		ND_PRINT(" prefix-cnt=%u", GET_U_1(bp));
		pfxcnt = GET_U_1(bp);
		if (len < 2)
			goto trunc;
		ND_PRINT(" prio=%u", GET_U_1(bp + 1));
		if (len < 4)
			goto trunc;
		ND_PRINT(" holdtime=");
		unsigned_relts_print(ndo, GET_BE_U_2(bp + 2));
		bp += 4;
		len -= 4;

		/* Encoded-Unicast-RP-Address */
		ND_PRINT(" RP=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;

		/* Encoded-Group Addresses */
		for (i = 0; i < pfxcnt && len > 0; i++) {
			ND_PRINT(" Group%u=", i);
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance;
			len -= advance;
		}
		break;
	}

	case PIMV2_TYPE_PRUNE_REFRESH:
		ND_PRINT(" src=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;
		ND_PRINT(" grp=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;
		ND_PRINT(" forwarder=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;
		if (len < 2)
			goto trunc;
		ND_PRINT(" TUNR ");
		unsigned_relts_print(ndo, GET_BE_U_2(bp));
		break;

	case PIMV2_TYPE_DF_ELECTION:
		subtype = PIM_SUBTYPE(GET_U_1(pim->pim_rsv));
		ND_PRINT("\n\t  %s,", tok2str( pimv2_df_election_flag_values,
			 "Unknown", subtype) );

		ND_PRINT(" rpa=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0) {
			goto trunc;
		}
		bp += advance;
		len -= advance;
		ND_PRINT(" sender pref=%u", GET_BE_U_4(bp) );
		ND_PRINT(" sender metric=%u", GET_BE_U_4(bp + 4));

		bp += 8;
		len -= 8;

		switch (subtype) {
		case PIMV2_DF_ELECTION_BACKOFF:
		case PIMV2_DF_ELECTION_PASS:
			ND_PRINT("\n\t  %s addr=", PIMV2_DF_ELECTION_PASS_BACKOFF_STR(subtype));
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0) {
				goto trunc;
			}
			bp += advance;
			len -= advance;

			ND_PRINT(" %s pref=%u", PIMV2_DF_ELECTION_PASS_BACKOFF_STR(subtype), GET_BE_U_4(bp) );
			ND_PRINT(" %s metric=%u", PIMV2_DF_ELECTION_PASS_BACKOFF_STR(subtype), GET_BE_U_4(bp + 4));

			bp += 8;
			len -= 8;

			if (subtype == PIMV2_DF_ELECTION_BACKOFF) {
				ND_PRINT(" interval %dms", GET_BE_U_2(bp));
			}

			break;
		default:
			break;
		}
		break;

	 default:
		ND_PRINT(" [type %u]", PIM_TYPE(pim_typever));
		break;
	}

	return;

trunc:
	nd_print_trunc(ndo);
}
