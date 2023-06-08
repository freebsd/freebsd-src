/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997
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
 *
 * OSPF support contributed by Jeffrey Honig (jch@mitchell.cit.cornell.edu)
 */

/* \summary: IPv6 Open Shortest Path First (OSPFv3) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ospf.h"

#define	OSPF_TYPE_HELLO         1	/* Hello */
#define	OSPF_TYPE_DD            2	/* Database Description */
#define	OSPF_TYPE_LS_REQ        3	/* Link State Request */
#define	OSPF_TYPE_LS_UPDATE     4	/* Link State Update */
#define	OSPF_TYPE_LS_ACK        5	/* Link State Ack */

/* Options *_options	*/
#define OSPF6_OPTION_V6	0x01	/* V6 bit: A bit for peeping tom */
#define OSPF6_OPTION_E	0x02	/* E bit: External routes advertised	*/
#define OSPF6_OPTION_MC	0x04	/* MC bit: Multicast capable */
#define OSPF6_OPTION_N	0x08	/* N bit: For type-7 LSA */
#define OSPF6_OPTION_R	0x10	/* R bit: Router bit */
#define OSPF6_OPTION_DC	0x20	/* DC bit: Demand circuits */
/* The field is actually 24-bit (RFC5340 Section A.2). */
#define OSPF6_OPTION_AF	0x0100	/* AF bit: Multiple address families */
#define OSPF6_OPTION_L	0x0200	/* L bit: Link-local signaling (LLS) */
#define OSPF6_OPTION_AT	0x0400	/* AT bit: Authentication trailer */


/* db_flags	*/
#define	OSPF6_DB_INIT		0x04	    /*	*/
#define	OSPF6_DB_MORE		0x02
#define	OSPF6_DB_MASTER		0x01
#define	OSPF6_DB_M6		0x10  /* IPv6 MTU */

/* ls_type	*/
#define	LS_TYPE_ROUTER		1   /* router link */
#define	LS_TYPE_NETWORK		2   /* network link */
#define	LS_TYPE_INTER_AP	3   /* Inter-Area-Prefix */
#define	LS_TYPE_INTER_AR	4   /* Inter-Area-Router */
#define	LS_TYPE_ASE		5   /* ASE */
#define	LS_TYPE_GROUP		6   /* Group membership */
#define	LS_TYPE_NSSA		7   /* NSSA */
#define	LS_TYPE_LINK		8   /* Link LSA */
#define	LS_TYPE_INTRA_AP	9   /* Intra-Area-Prefix */
#define LS_TYPE_INTRA_ATE       10  /* Intra-Area-TE */
#define LS_TYPE_GRACE           11  /* Grace LSA */
#define LS_TYPE_RI		12  /* Router information */
#define LS_TYPE_INTER_ASTE	13  /* Inter-AS-TE */
#define LS_TYPE_L1VPN		14  /* L1VPN */
#define LS_TYPE_MASK		0x1fff

#define LS_SCOPE_LINKLOCAL	0x0000
#define LS_SCOPE_AREA		0x2000
#define LS_SCOPE_AS		0x4000
#define LS_SCOPE_MASK		0x6000
#define LS_SCOPE_U              0x8000

/* rla_link.link_type	*/
#define	RLA_TYPE_ROUTER		1   /* point-to-point to another router	*/
#define	RLA_TYPE_TRANSIT	2   /* connection to transit network	*/
#define RLA_TYPE_VIRTUAL	4   /* virtual link			*/

/* rla_flags	*/
#define	RLA_FLAG_B	0x01
#define	RLA_FLAG_E	0x02
#define	RLA_FLAG_V	0x04
#define	RLA_FLAG_W	0x08
#define	RLA_FLAG_Nt	0x10

/* lsa_prefix options */
#define LSA_PREFIX_OPT_NU 0x01
#define LSA_PREFIX_OPT_LA 0x02
#define LSA_PREFIX_OPT_MC 0x04
#define LSA_PREFIX_OPT_P  0x08
#define LSA_PREFIX_OPT_DN 0x10
#define LSA_PREFIX_OPT_N  0x20

/* sla_tosmetric breakdown	*/
#define	SLA_MASK_TOS		0x7f000000
#define	SLA_MASK_METRIC		0x00ffffff
#define SLA_SHIFT_TOS		24

/* asla_metric */
#define ASLA_FLAG_FWDADDR	0x02000000
#define ASLA_FLAG_ROUTETAG	0x01000000
#define	ASLA_MASK_METRIC	0x00ffffff

/* RFC6506 Section 4.1 */
#define OSPF6_AT_HDRLEN             16U
#define OSPF6_AUTH_TYPE_HMAC        0x0001

typedef nd_uint32_t rtrid_t;

/* link state advertisement header */
struct lsa6_hdr {
    nd_uint16_t ls_age;
    nd_uint16_t ls_type;
    rtrid_t ls_stateid;
    rtrid_t ls_router;
    nd_uint32_t ls_seq;
    nd_uint16_t ls_chksum;
    nd_uint16_t ls_length;
};

/* Length of an IPv6 address, in bytes. */
#define IPV6_ADDR_LEN_BYTES (128/8)

struct lsa6_prefix {
    nd_uint8_t lsa_p_len;
    nd_uint8_t lsa_p_opt;
    nd_uint16_t lsa_p_metric;
    nd_byte lsa_p_prefix[IPV6_ADDR_LEN_BYTES]; /* maximum length */
};

/* link state advertisement */
struct lsa6 {
    struct lsa6_hdr ls_hdr;

    /* Link state types */
    union {
	/* Router links advertisements */
	struct {
	    union {
		nd_uint8_t flg;
		nd_uint32_t opt;
	    } rla_flgandopt;
#define rla_flags	rla_flgandopt.flg
#define rla_options	rla_flgandopt.opt
	    struct rlalink6 {
		nd_uint8_t link_type;
		nd_byte link_zero;
		nd_uint16_t link_metric;
		nd_uint32_t link_ifid;
		nd_uint32_t link_nifid;
		rtrid_t link_nrtid;
	    } rla_link[1];		/* may repeat	*/
	} un_rla;

	/* Network links advertisements */
	struct {
	    nd_uint32_t nla_options;
	    rtrid_t nla_router[1];	/* may repeat	*/
	} un_nla;

	/* Inter Area Prefix LSA */
	struct {
	    nd_uint32_t inter_ap_metric;
	    struct lsa6_prefix inter_ap_prefix[1];
	} un_inter_ap;

	/* AS external links advertisements */
	struct {
	    nd_uint32_t asla_metric;
	    struct lsa6_prefix asla_prefix[1];
	    /* some optional fields follow */
	} un_asla;

#if 0
	/* Summary links advertisements */
	struct {
	    nd_ipv4     sla_mask;
	    nd_uint32_t sla_tosmetric[1];	/* may repeat	*/
	} un_sla;

	/* Multicast group membership */
	struct mcla {
	    nd_uint32_t mcla_vtype;
	    nd_ipv4     mcla_vid;
	} un_mcla[1];
#endif

	/* Type 7 LSA */

	/* Link LSA */
	struct llsa {
	    union {
		nd_uint8_t pri;
		nd_uint32_t opt;
	    } llsa_priandopt;
#define llsa_priority	llsa_priandopt.pri
#define llsa_options	llsa_priandopt.opt
	    nd_ipv6	llsa_lladdr;
	    nd_uint32_t llsa_nprefix;
	    struct lsa6_prefix llsa_prefix[1];
	} un_llsa;

	/* Intra-Area-Prefix */
	struct {
	    nd_uint16_t intra_ap_nprefix;
	    nd_uint16_t intra_ap_lstype;
	    rtrid_t intra_ap_lsid;
	    rtrid_t intra_ap_rtid;
	    struct lsa6_prefix intra_ap_prefix[1];
	} un_intra_ap;
    } lsa_un;
};

/*
 * the main header
 */
struct ospf6hdr {
    nd_uint8_t ospf6_version;
    nd_uint8_t ospf6_type;
    nd_uint16_t ospf6_len;
    rtrid_t ospf6_routerid;
    rtrid_t ospf6_areaid;
    nd_uint16_t ospf6_chksum;
    nd_uint8_t ospf6_instanceid;
    nd_uint8_t ospf6_rsvd;
};

/*
 * The OSPF6 header length is 16 bytes, regardless of how your compiler
 * might choose to pad the above structure.
 */
#define OSPF6HDR_LEN    16

/* Hello packet */
struct hello6 {
    nd_uint32_t hello_ifid;
    union {
	nd_uint8_t pri;
	nd_uint32_t opt;
    } hello_priandopt;
#define hello_priority	hello_priandopt.pri
#define hello_options	hello_priandopt.opt
    nd_uint16_t hello_helloint;
    nd_uint16_t hello_deadint;
    rtrid_t hello_dr;
    rtrid_t hello_bdr;
    rtrid_t hello_neighbor[1]; /* may repeat	*/
};

/* Database Description packet */
struct dd6 {
    nd_uint32_t db_options;
    nd_uint16_t db_mtu;
    nd_uint8_t db_mbz;
    nd_uint8_t db_flags;
    nd_uint32_t db_seq;
    struct lsa6_hdr db_lshdr[1]; /* may repeat	*/
};

/* Link State Request */
struct lsr6 {
    nd_uint16_t ls_mbz;
    nd_uint16_t ls_type;
    rtrid_t ls_stateid;
    rtrid_t ls_router;
};

/* Link State Update */
struct lsu6 {
    nd_uint32_t lsu_count;
    struct lsa6 lsu_lsa[1]; /* may repeat	*/
};


static const struct tok ospf6_option_values[] = {
	{ OSPF6_OPTION_V6,	"V6" },
	{ OSPF6_OPTION_E,	"External" },
	{ OSPF6_OPTION_MC,	"Deprecated" },
	{ OSPF6_OPTION_N,	"NSSA" },
	{ OSPF6_OPTION_R,	"Router" },
	{ OSPF6_OPTION_DC,	"Demand Circuit" },
	{ OSPF6_OPTION_AF,	"AFs Support" },
	{ OSPF6_OPTION_L,	"LLS" },
	{ OSPF6_OPTION_AT,	"Authentication Trailer" },
	{ 0,			NULL }
};

static const struct tok ospf6_rla_flag_values[] = {
	{ RLA_FLAG_B,		"ABR" },
	{ RLA_FLAG_E,		"External" },
	{ RLA_FLAG_V,		"Virtual-Link Endpoint" },
	{ RLA_FLAG_W,		"Deprecated" },
	{ RLA_FLAG_Nt,		"NSSA Translator" },
	{ 0,			NULL }
};

static const struct tok ospf6_asla_flag_values[] = {
	{ ASLA_FLAG_EXTERNAL,	"External Type 2" },
	{ ASLA_FLAG_FWDADDR,	"Forwarding" },
	{ ASLA_FLAG_ROUTETAG,	"Tag" },
	{ 0,			NULL }
};

static const struct tok ospf6_type_values[] = {
	{ OSPF_TYPE_HELLO,	"Hello" },
	{ OSPF_TYPE_DD,		"Database Description" },
	{ OSPF_TYPE_LS_REQ,	"LS-Request" },
	{ OSPF_TYPE_LS_UPDATE,	"LS-Update" },
	{ OSPF_TYPE_LS_ACK,	"LS-Ack" },
	{ 0,			NULL }
};

static const struct tok ospf6_lsa_values[] = {
	{ LS_TYPE_ROUTER,       "Router" },
	{ LS_TYPE_NETWORK,      "Network" },
	{ LS_TYPE_INTER_AP,     "Inter-Area Prefix" },
	{ LS_TYPE_INTER_AR,     "Inter-Area Router" },
	{ LS_TYPE_ASE,          "External" },
	{ LS_TYPE_GROUP,        "Deprecated" },
	{ LS_TYPE_NSSA,         "NSSA" },
	{ LS_TYPE_LINK,         "Link" },
	{ LS_TYPE_INTRA_AP,     "Intra-Area Prefix" },
        { LS_TYPE_INTRA_ATE,    "Intra-Area TE" },
        { LS_TYPE_GRACE,        "Grace" },
	{ LS_TYPE_RI,           "Router Information" },
	{ LS_TYPE_INTER_ASTE,   "Inter-AS-TE" },
	{ LS_TYPE_L1VPN,        "Layer 1 VPN" },
	{ 0,			NULL }
};

static const struct tok ospf6_ls_scope_values[] = {
	{ LS_SCOPE_LINKLOCAL,   "Link Local" },
	{ LS_SCOPE_AREA,        "Area Local" },
	{ LS_SCOPE_AS,          "Domain Wide" },
	{ 0,			NULL }
};

static const struct tok ospf6_dd_flag_values[] = {
	{ OSPF6_DB_INIT,	"Init" },
	{ OSPF6_DB_MORE,	"More" },
	{ OSPF6_DB_MASTER,	"Master" },
	{ OSPF6_DB_M6,		"IPv6 MTU" },
	{ 0,			NULL }
};

static const struct tok ospf6_lsa_prefix_option_values[] = {
        { LSA_PREFIX_OPT_NU, "No Unicast" },
        { LSA_PREFIX_OPT_LA, "Local address" },
        { LSA_PREFIX_OPT_MC, "Deprecated" },
        { LSA_PREFIX_OPT_P, "Propagate" },
        { LSA_PREFIX_OPT_DN, "Down" },
        { LSA_PREFIX_OPT_N, "N-bit" },
	{ 0, NULL }
};

static const struct tok ospf6_auth_type_str[] = {
	{ OSPF6_AUTH_TYPE_HMAC,        "HMAC" },
	{ 0, NULL }
};

static void
ospf6_print_ls_type(netdissect_options *ndo,
                    u_int ls_type, const rtrid_t *ls_stateid)
{
        ND_PRINT("\n\t    %s LSA (%u), %s Scope%s, LSA-ID %s",
               tok2str(ospf6_lsa_values, "Unknown", ls_type & LS_TYPE_MASK),
               ls_type & LS_TYPE_MASK,
               tok2str(ospf6_ls_scope_values, "Unknown", ls_type & LS_SCOPE_MASK),
               ls_type &0x8000 ? ", transitive" : "", /* U-bit */
               GET_IPADDR_STRING(ls_stateid));
}

static int
ospf6_print_lshdr(netdissect_options *ndo,
                  const struct lsa6_hdr *lshp, const u_char *dataend)
{
	if ((const u_char *)(lshp + 1) > dataend)
		goto trunc;

	ND_PRINT("\n\t  Advertising Router %s, seq 0x%08x, age %us, length %zu",
		 GET_IPADDR_STRING(lshp->ls_router),
		 GET_BE_U_4(lshp->ls_seq),
		 GET_BE_U_2(lshp->ls_age),
		 GET_BE_U_2(lshp->ls_length)-sizeof(struct lsa6_hdr));

	ospf6_print_ls_type(ndo, GET_BE_U_2(lshp->ls_type),
			    &lshp->ls_stateid);

	return (0);
trunc:
	return (1);
}

static int
ospf6_print_lsaprefix(netdissect_options *ndo,
                      const uint8_t *tptr, u_int lsa_length)
{
	const struct lsa6_prefix *lsapp = (const struct lsa6_prefix *)tptr;
	u_int wordlen;
	nd_ipv6 prefix;

	if (lsa_length < sizeof (*lsapp) - IPV6_ADDR_LEN_BYTES)
		goto trunc;
	lsa_length -= sizeof (*lsapp) - IPV6_ADDR_LEN_BYTES;
	ND_TCHECK_LEN(lsapp, sizeof(*lsapp) - IPV6_ADDR_LEN_BYTES);
	wordlen = (GET_U_1(lsapp->lsa_p_len) + 31) / 32;
	if (wordlen * 4 > sizeof(nd_ipv6)) {
		ND_PRINT(" bogus prefixlen /%u", GET_U_1(lsapp->lsa_p_len));
		goto trunc;
	}
	if (lsa_length < wordlen * 4)
		goto trunc;
	lsa_length -= wordlen * 4;
	memset(prefix, 0, sizeof(prefix));
	GET_CPY_BYTES(prefix, lsapp->lsa_p_prefix, wordlen * 4);
	ND_PRINT("\n\t\t%s/%u", ip6addr_string(ndo, prefix), /* local buffer, not packet data; don't use GET_IP6ADDR_STRING() */
		 GET_U_1(lsapp->lsa_p_len));
        if (GET_U_1(lsapp->lsa_p_opt)) {
            ND_PRINT(", Options [%s]",
                   bittok2str(ospf6_lsa_prefix_option_values,
                              "none", GET_U_1(lsapp->lsa_p_opt)));
        }
        ND_PRINT(", metric %u", GET_BE_U_2(lsapp->lsa_p_metric));
	return sizeof(*lsapp) - IPV6_ADDR_LEN_BYTES + wordlen * 4;

trunc:
	return -1;
}


/*
 * Print a single link state advertisement.  If truncated return 1, else 0.
 */
static int
ospf6_print_lsa(netdissect_options *ndo,
                const struct lsa6 *lsap, const u_char *dataend)
{
	const struct rlalink6 *rlp;
#if 0
	const struct tos_metric *tosp;
#endif
	const rtrid_t *ap;
#if 0
	const struct aslametric *almp;
	const struct mcla *mcp;
#endif
	const struct llsa *llsap;
	const struct lsa6_prefix *lsapp;
#if 0
	const uint32_t *lp;
#endif
	u_int prefixes;
	int bytelen;
	u_int length, lsa_length;
	uint32_t flags32;
	const uint8_t *tptr;

	if (ospf6_print_lshdr(ndo, &lsap->ls_hdr, dataend))
		return (1);
        length = GET_BE_U_2(lsap->ls_hdr.ls_length);

	/*
	 * The LSA length includes the length of the header;
	 * it must have a value that's at least that length.
	 * If it does, find the length of what follows the
	 * header.
	 */
        if (length < sizeof(struct lsa6_hdr) || (const u_char *)lsap + length > dataend)
		return (1);
        lsa_length = length - sizeof(struct lsa6_hdr);
        tptr = (const uint8_t *)lsap+sizeof(struct lsa6_hdr);

	switch (GET_BE_U_2(lsap->ls_hdr.ls_type)) {
	case LS_TYPE_ROUTER | LS_SCOPE_AREA:
		if (lsa_length < sizeof (lsap->lsa_un.un_rla.rla_options))
			return (1);
		lsa_length -= sizeof (lsap->lsa_un.un_rla.rla_options);
		ND_PRINT("\n\t      Options [%s]",
		          bittok2str(ospf6_option_values, "none",
		          GET_BE_U_4(lsap->lsa_un.un_rla.rla_options)));
		ND_PRINT(", RLA-Flags [%s]",
		          bittok2str(ospf6_rla_flag_values, "none",
		          GET_U_1(lsap->lsa_un.un_rla.rla_flags)));

		rlp = lsap->lsa_un.un_rla.rla_link;
		while (lsa_length != 0) {
			if (lsa_length < sizeof (*rlp))
				return (1);
			lsa_length -= sizeof (*rlp);
			ND_TCHECK_SIZE(rlp);
			switch (GET_U_1(rlp->link_type)) {

			case RLA_TYPE_VIRTUAL:
				ND_PRINT("\n\t      Virtual Link: Neighbor Router-ID %s"
                                       "\n\t      Neighbor Interface-ID %s, Interface %s",
                                       GET_IPADDR_STRING(rlp->link_nrtid),
                                       GET_IPADDR_STRING(rlp->link_nifid),
                                       GET_IPADDR_STRING(rlp->link_ifid));
                                break;

			case RLA_TYPE_ROUTER:
				ND_PRINT("\n\t      Neighbor Router-ID %s"
                                       "\n\t      Neighbor Interface-ID %s, Interface %s",
                                       GET_IPADDR_STRING(rlp->link_nrtid),
                                       GET_IPADDR_STRING(rlp->link_nifid),
                                       GET_IPADDR_STRING(rlp->link_ifid));
				break;

			case RLA_TYPE_TRANSIT:
				ND_PRINT("\n\t      Neighbor Network-ID %s"
                                       "\n\t      Neighbor Interface-ID %s, Interface %s",
				    GET_IPADDR_STRING(rlp->link_nrtid),
				    GET_IPADDR_STRING(rlp->link_nifid),
				    GET_IPADDR_STRING(rlp->link_ifid));
				break;

			default:
				ND_PRINT("\n\t      Unknown Router Links Type 0x%02x",
				    GET_U_1(rlp->link_type));
				return (0);
			}
			ND_PRINT(", metric %u", GET_BE_U_2(rlp->link_metric));
			rlp++;
		}
		break;

	case LS_TYPE_NETWORK | LS_SCOPE_AREA:
		if (lsa_length < sizeof (lsap->lsa_un.un_nla.nla_options))
			return (1);
		lsa_length -= sizeof (lsap->lsa_un.un_nla.nla_options);
		ND_PRINT("\n\t      Options [%s]",
		          bittok2str(ospf6_option_values, "none",
		          GET_BE_U_4(lsap->lsa_un.un_nla.nla_options)));

		ND_PRINT("\n\t      Connected Routers:");
		ap = lsap->lsa_un.un_nla.nla_router;
		while (lsa_length != 0) {
			if (lsa_length < sizeof (*ap))
				return (1);
			lsa_length -= sizeof (*ap);
			ND_PRINT("\n\t\t%s", GET_IPADDR_STRING(ap));
			++ap;
		}
		break;

	case LS_TYPE_INTER_AP | LS_SCOPE_AREA:
		if (lsa_length < sizeof (lsap->lsa_un.un_inter_ap.inter_ap_metric))
			return (1);
		lsa_length -= sizeof (lsap->lsa_un.un_inter_ap.inter_ap_metric);
		ND_PRINT(", metric %u",
			GET_BE_U_4(lsap->lsa_un.un_inter_ap.inter_ap_metric) & SLA_MASK_METRIC);

		tptr = (const uint8_t *)lsap->lsa_un.un_inter_ap.inter_ap_prefix;
		while (lsa_length != 0) {
			bytelen = ospf6_print_lsaprefix(ndo, tptr, lsa_length);
			if (bytelen < 0)
				goto trunc;
			/*
			 * ospf6_print_lsaprefix() will return -1 if
			 * the length is too high, so this will not
			 * underflow.
			 */
			lsa_length -= bytelen;
			tptr += bytelen;
		}
		break;

	case LS_TYPE_ASE | LS_SCOPE_AS:
		if (lsa_length < sizeof (lsap->lsa_un.un_asla.asla_metric))
			return (1);
		lsa_length -= sizeof (lsap->lsa_un.un_asla.asla_metric);
		flags32 = GET_BE_U_4(lsap->lsa_un.un_asla.asla_metric);
		ND_PRINT("\n\t     Flags [%s]",
		          bittok2str(ospf6_asla_flag_values, "none", flags32));
		ND_PRINT(" metric %u",
		       GET_BE_U_4(lsap->lsa_un.un_asla.asla_metric) &
		       ASLA_MASK_METRIC);

		tptr = (const uint8_t *)lsap->lsa_un.un_asla.asla_prefix;
		lsapp = (const struct lsa6_prefix *)tptr;
		bytelen = ospf6_print_lsaprefix(ndo, tptr, lsa_length);
		if (bytelen < 0)
			goto trunc;
		/*
		 * ospf6_print_lsaprefix() will return -1 if
		 * the length is too high, so this will not
		 * underflow.
		 */
		lsa_length -= bytelen;
		tptr += bytelen;

		if ((flags32 & ASLA_FLAG_FWDADDR) != 0) {
			if (lsa_length < sizeof (nd_ipv6))
				return (1);
			lsa_length -= sizeof (nd_ipv6);
			ND_PRINT(" forward %s",
				 GET_IP6ADDR_STRING(tptr));
			tptr += sizeof(nd_ipv6);
		}

		if ((flags32 & ASLA_FLAG_ROUTETAG) != 0) {
			if (lsa_length < sizeof (uint32_t))
				return (1);
			lsa_length -= sizeof (uint32_t);
			ND_PRINT(" tag %s",
			       GET_IPADDR_STRING(tptr));
			tptr += sizeof(uint32_t);
		}

		if (GET_U_1(lsapp->lsa_p_metric)) {
			if (lsa_length < sizeof (uint32_t))
				return (1);
			lsa_length -= sizeof (uint32_t);
			ND_PRINT(" RefLSID: %s",
			       GET_IPADDR_STRING(tptr));
			tptr += sizeof(uint32_t);
		}
		break;

	case LS_TYPE_LINK:
		/* Link LSA */
		llsap = &lsap->lsa_un.un_llsa;
		if (lsa_length < sizeof (llsap->llsa_priandopt))
			return (1);
		lsa_length -= sizeof (llsap->llsa_priandopt);
		ND_TCHECK_SIZE(&llsap->llsa_priandopt);
		ND_PRINT("\n\t      Options [%s]",
		          bittok2str(ospf6_option_values, "none",
		          GET_BE_U_4(llsap->llsa_options)));

		if (lsa_length < sizeof (llsap->llsa_lladdr) + sizeof (llsap->llsa_nprefix))
			return (1);
		lsa_length -= sizeof (llsap->llsa_lladdr) + sizeof (llsap->llsa_nprefix);
                prefixes = GET_BE_U_4(llsap->llsa_nprefix);
		ND_PRINT("\n\t      Priority %u, Link-local address %s, Prefixes %u:",
                       GET_U_1(llsap->llsa_priority),
                       GET_IP6ADDR_STRING(llsap->llsa_lladdr),
                       prefixes);

		tptr = (const uint8_t *)llsap->llsa_prefix;
		while (prefixes > 0) {
			bytelen = ospf6_print_lsaprefix(ndo, tptr, lsa_length);
			if (bytelen < 0)
				goto trunc;
			prefixes--;
			/*
			 * ospf6_print_lsaprefix() will return -1 if
			 * the length is too high, so this will not
			 * underflow.
			 */
			lsa_length -= bytelen;
			tptr += bytelen;
		}
		break;

	case LS_TYPE_INTRA_AP | LS_SCOPE_AREA:
		/* Intra-Area-Prefix LSA */
		if (lsa_length < sizeof (lsap->lsa_un.un_intra_ap.intra_ap_rtid))
			return (1);
		lsa_length -= sizeof (lsap->lsa_un.un_intra_ap.intra_ap_rtid);
		ND_TCHECK_4(lsap->lsa_un.un_intra_ap.intra_ap_rtid);
		ospf6_print_ls_type(ndo,
			GET_BE_U_2(lsap->lsa_un.un_intra_ap.intra_ap_lstype),
			&lsap->lsa_un.un_intra_ap.intra_ap_lsid);

		if (lsa_length < sizeof (lsap->lsa_un.un_intra_ap.intra_ap_nprefix))
			return (1);
		lsa_length -= sizeof (lsap->lsa_un.un_intra_ap.intra_ap_nprefix);
                prefixes = GET_BE_U_2(lsap->lsa_un.un_intra_ap.intra_ap_nprefix);
		ND_PRINT("\n\t      Prefixes %u:", prefixes);

		tptr = (const uint8_t *)lsap->lsa_un.un_intra_ap.intra_ap_prefix;
		while (prefixes > 0) {
			bytelen = ospf6_print_lsaprefix(ndo, tptr, lsa_length);
			if (bytelen < 0)
				goto trunc;
			prefixes--;
			/*
			 * ospf6_print_lsaprefix() will return -1 if
			 * the length is too high, so this will not
			 * underflow.
			 */
			lsa_length -= bytelen;
			tptr += bytelen;
		}
		break;

        case LS_TYPE_GRACE | LS_SCOPE_LINKLOCAL:
                if (ospf_grace_lsa_print(ndo, tptr, lsa_length) == -1) {
                    return 1;
                }
                break;

        case LS_TYPE_INTRA_ATE | LS_SCOPE_LINKLOCAL:
                if (ospf_te_lsa_print(ndo, tptr, lsa_length) == -1) {
                    return 1;
                }
                break;

	default:
                if(!print_unknown_data(ndo,tptr,
                                       "\n\t      ",
                                       lsa_length)) {
                    return (1);
                }
                break;
	}

	return (0);
trunc:
	return (1);
}

static int
ospf6_decode_v3(netdissect_options *ndo,
                const struct ospf6hdr *op,
                const u_char *dataend)
{
	const rtrid_t *ap;
	const struct lsr6 *lsrp;
	const struct lsa6_hdr *lshp;
	const struct lsa6 *lsap;
	int i;

	switch (GET_U_1(op->ospf6_type)) {

	case OSPF_TYPE_HELLO: {
		const struct hello6 *hellop = (const struct hello6 *)((const uint8_t *)op + OSPF6HDR_LEN);

		ND_PRINT("\n\tOptions [%s]",
		          bittok2str(ospf6_option_values, "none",
		          GET_BE_U_4(hellop->hello_options)));

		ND_PRINT("\n\t  Hello Timer %us, Dead Timer %us, Interface-ID %s, Priority %u",
		          GET_BE_U_2(hellop->hello_helloint),
		          GET_BE_U_2(hellop->hello_deadint),
		          GET_IPADDR_STRING(hellop->hello_ifid),
		          GET_U_1(hellop->hello_priority));

		if (GET_BE_U_4(hellop->hello_dr) != 0)
			ND_PRINT("\n\t  Designated Router %s",
			    GET_IPADDR_STRING(hellop->hello_dr));
		if (GET_BE_U_4(hellop->hello_bdr) != 0)
			ND_PRINT(", Backup Designated Router %s",
			    GET_IPADDR_STRING(hellop->hello_bdr));
		if (ndo->ndo_vflag > 1) {
			ND_PRINT("\n\t  Neighbor List:");
			ap = hellop->hello_neighbor;
			while ((const u_char *)ap < dataend) {
				ND_PRINT("\n\t    %s", GET_IPADDR_STRING(ap));
				++ap;
			}
		}
		break;	/* HELLO */
	}

	case OSPF_TYPE_DD: {
		const struct dd6 *ddp = (const struct dd6 *)((const uint8_t *)op + OSPF6HDR_LEN);

		ND_PRINT("\n\tOptions [%s]",
		          bittok2str(ospf6_option_values, "none",
		          GET_BE_U_4(ddp->db_options)));
		ND_PRINT(", DD Flags [%s]",
		          bittok2str(ospf6_dd_flag_values,"none",GET_U_1(ddp->db_flags)));

		ND_PRINT(", MTU %u, DD-Sequence 0x%08x",
                       GET_BE_U_2(ddp->db_mtu),
                       GET_BE_U_4(ddp->db_seq));
		if (ndo->ndo_vflag > 1) {
			/* Print all the LS adv's */
			lshp = ddp->db_lshdr;
			while ((const u_char *)lshp < dataend) {
				if (ospf6_print_lshdr(ndo, lshp++, dataend))
					goto trunc;
			}
		}
		break;
	}

	case OSPF_TYPE_LS_REQ:
		if (ndo->ndo_vflag > 1) {
			lsrp = (const struct lsr6 *)((const uint8_t *)op + OSPF6HDR_LEN);
			while ((const u_char *)lsrp < dataend) {
				ND_TCHECK_SIZE(lsrp);
				ND_PRINT("\n\t  Advertising Router %s",
				          GET_IPADDR_STRING(lsrp->ls_router));
				ospf6_print_ls_type(ndo,
                                                    GET_BE_U_2(lsrp->ls_type),
                                                    &lsrp->ls_stateid);
				++lsrp;
			}
		}
		break;

	case OSPF_TYPE_LS_UPDATE:
		if (ndo->ndo_vflag > 1) {
			const struct lsu6 *lsup = (const struct lsu6 *)((const uint8_t *)op + OSPF6HDR_LEN);

			i = GET_BE_U_4(lsup->lsu_count);
			lsap = lsup->lsu_lsa;
			while ((const u_char *)lsap < dataend && i--) {
				if (ospf6_print_lsa(ndo, lsap, dataend))
					goto trunc;
				lsap = (const struct lsa6 *)((const u_char *)lsap +
				    GET_BE_U_2(lsap->ls_hdr.ls_length));
			}
		}
		break;

	case OSPF_TYPE_LS_ACK:
		if (ndo->ndo_vflag > 1) {
			lshp = (const struct lsa6_hdr *)((const uint8_t *)op + OSPF6HDR_LEN);
			while ((const u_char *)lshp < dataend) {
				if (ospf6_print_lshdr(ndo, lshp++, dataend))
					goto trunc;
			}
		}
		break;

	default:
		break;
	}
	return (0);
trunc:
	return (1);
}

/* RFC5613 Section 2.2 (w/o the TLVs) */
static int
ospf6_print_lls(netdissect_options *ndo,
                const u_char *cp, const u_int len)
{
	uint16_t llsdatalen;

	if (len == 0)
		return 0;
	if (len < OSPF_LLS_HDRLEN)
		goto trunc;
	/* Checksum */
	ND_PRINT("\n\tLLS Checksum 0x%04x", GET_BE_U_2(cp));
	cp += 2;
	/* LLS Data Length */
	llsdatalen = GET_BE_U_2(cp);
	ND_PRINT(", Data Length %u", llsdatalen);
	if (llsdatalen < OSPF_LLS_HDRLEN || llsdatalen > len)
		goto trunc;
	cp += 2;
	/* LLS TLVs */
	ND_TCHECK_LEN(cp, llsdatalen - OSPF_LLS_HDRLEN);
	/* FIXME: code in print-ospf.c can be reused to decode the TLVs */

	return llsdatalen;
trunc:
	return -1;
}

/* RFC6506 Section 4.1 */
static int
ospf6_decode_at(netdissect_options *ndo,
                const u_char *cp, const u_int len)
{
	uint16_t authdatalen;

	if (len == 0)
		return 0;
	if (len < OSPF6_AT_HDRLEN)
		goto trunc;
	/* Authentication Type */
	ND_PRINT("\n\tAuthentication Type %s",
		 tok2str(ospf6_auth_type_str, "unknown (0x%04x)", GET_BE_U_2(cp)));
	cp += 2;
	/* Auth Data Len */
	authdatalen = GET_BE_U_2(cp);
	ND_PRINT(", Length %u", authdatalen);
	if (authdatalen < OSPF6_AT_HDRLEN || authdatalen > len)
		goto trunc;
	cp += 2;
	/* Reserved */
	cp += 2;
	/* Security Association ID */
	ND_PRINT(", SAID %u", GET_BE_U_2(cp));
	cp += 2;
	/* Cryptographic Sequence Number (High-Order 32 Bits) */
	ND_PRINT(", CSN 0x%08x", GET_BE_U_4(cp));
	cp += 4;
	/* Cryptographic Sequence Number (Low-Order 32 Bits) */
	ND_PRINT(":%08x", GET_BE_U_4(cp));
	cp += 4;
	/* Authentication Data */
	ND_TCHECK_LEN(cp, authdatalen - OSPF6_AT_HDRLEN);
	if (ndo->ndo_vflag > 1)
		print_unknown_data(ndo,cp, "\n\tAuthentication Data ", authdatalen - OSPF6_AT_HDRLEN);
	return 0;

trunc:
	return 1;
}

/* The trailing data may include LLS and/or AT data (in this specific order).
 * LLS data may be present only in Hello and DBDesc packets with the L-bit set.
 * AT data may be present in Hello and DBDesc packets with the AT-bit set or in
 * any other packet type, thus decode the AT data regardless of the AT-bit.
 */
static int
ospf6_decode_v3_trailer(netdissect_options *ndo,
                        const struct ospf6hdr *op, const u_char *cp, const unsigned len)
{
	uint8_t type;
	int llslen = 0;
	int lls_hello = 0;
	int lls_dd = 0;

	type = GET_U_1(op->ospf6_type);
	if (type == OSPF_TYPE_HELLO) {
		const struct hello6 *hellop = (const struct hello6 *)((const uint8_t *)op + OSPF6HDR_LEN);
		if (GET_BE_U_4(hellop->hello_options) & OSPF6_OPTION_L)
			lls_hello = 1;
	} else if (type == OSPF_TYPE_DD) {
		const struct dd6 *ddp = (const struct dd6 *)((const uint8_t *)op + OSPF6HDR_LEN);
		if (GET_BE_U_4(ddp->db_options) & OSPF6_OPTION_L)
			lls_dd = 1;
	}
	if ((lls_hello || lls_dd) && (llslen = ospf6_print_lls(ndo, cp, len)) < 0)
		goto trunc;
	return ospf6_decode_at(ndo, cp + llslen, len - llslen);

trunc:
	return 1;
}

void
ospf6_print(netdissect_options *ndo,
            const u_char *bp, u_int length)
{
	const struct ospf6hdr *op;
	const u_char *dataend;
	const char *cp;
	uint16_t datalen;

	ndo->ndo_protocol = "ospf3";
	op = (const struct ospf6hdr *)bp;

	/* If the type is valid translate it, or just print the type */
	/* value.  If it's not valid, say so and return */
	cp = tok2str(ospf6_type_values, "unknown packet type (%u)",
		     GET_U_1(op->ospf6_type));
	ND_PRINT("OSPFv%u, %s, length %u", GET_U_1(op->ospf6_version), cp,
		 length);
	if (*cp == 'u') {
		return;
	}

	if(!ndo->ndo_vflag) { /* non verbose - so lets bail out here */
		return;
	}

	/* OSPFv3 data always comes first and optional trailing data may follow. */
	datalen = GET_BE_U_2(op->ospf6_len);
	if (datalen > length) {
		ND_PRINT(" [len %u]", datalen);
		return;
	}
	dataend = bp + datalen;

	ND_PRINT("\n\tRouter-ID %s", GET_IPADDR_STRING(op->ospf6_routerid));

	if (GET_BE_U_4(op->ospf6_areaid) != 0)
		ND_PRINT(", Area %s", GET_IPADDR_STRING(op->ospf6_areaid));
	else
		ND_PRINT(", Backbone Area");
	if (GET_U_1(op->ospf6_instanceid))
		ND_PRINT(", Instance %u", GET_U_1(op->ospf6_instanceid));

	/* Do rest according to version.	 */
	switch (GET_U_1(op->ospf6_version)) {

	case 3:
		/* ospf version 3 */
		if (ospf6_decode_v3(ndo, op, dataend) ||
		    ospf6_decode_v3_trailer(ndo, op, dataend, length - datalen))
			goto trunc;
		break;
	}			/* end switch on version */

	return;
trunc:
	nd_print_trunc(ndo);
}
