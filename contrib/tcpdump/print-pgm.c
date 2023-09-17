/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Andy Heffernan (ahh@juniper.net)
 */

/* \summary: Pragmatic General Multicast (PGM) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "addrtostr.h"

#include "ip.h"
#include "ip6.h"
#include "ipproto.h"
#include "af.h"

/*
 * PGM header (RFC 3208)
 */
struct pgm_header {
    nd_uint16_t	pgm_sport;
    nd_uint16_t	pgm_dport;
    nd_uint8_t	pgm_type;
    nd_uint8_t	pgm_options;
    nd_uint16_t	pgm_sum;
    nd_byte	pgm_gsid[6];
    nd_uint16_t	pgm_length;
};

struct pgm_spm {
    nd_uint32_t	pgms_seq;
    nd_uint32_t	pgms_trailseq;
    nd_uint32_t	pgms_leadseq;
    nd_uint16_t	pgms_nla_afi;
    nd_uint16_t	pgms_reserved;
    /* ... uint8_t	pgms_nla[0]; */
    /* ... options */
};

struct pgm_nak {
    nd_uint32_t	pgmn_seq;
    nd_uint16_t	pgmn_source_afi;
    nd_uint16_t	pgmn_reserved;
    /* ... uint8_t	pgmn_source[0]; */
    /* ... uint16_t	pgmn_group_afi */
    /* ... uint16_t	pgmn_reserved2; */
    /* ... uint8_t	pgmn_group[0]; */
    /* ... options */
};

struct pgm_ack {
    nd_uint32_t	pgma_rx_max_seq;
    nd_uint32_t	pgma_bitmap;
    /* ... options */
};

struct pgm_poll {
    nd_uint32_t	pgmp_seq;
    nd_uint16_t	pgmp_round;
    nd_uint16_t	pgmp_subtype;
    nd_uint16_t	pgmp_nla_afi;
    nd_uint16_t	pgmp_reserved;
    /* ... uint8_t	pgmp_nla[0]; */
    /* ... options */
};

struct pgm_polr {
    nd_uint32_t	pgmp_seq;
    nd_uint16_t	pgmp_round;
    nd_uint16_t	pgmp_reserved;
    /* ... options */
};

struct pgm_data {
    nd_uint32_t	pgmd_seq;
    nd_uint32_t	pgmd_trailseq;
    /* ... options */
};

typedef enum _pgm_type {
    PGM_SPM = 0,		/* source path message */
    PGM_POLL = 1,		/* POLL Request */
    PGM_POLR = 2,		/* POLL Response */
    PGM_ODATA = 4,		/* original data */
    PGM_RDATA = 5,		/* repair data */
    PGM_NAK = 8,		/* NAK */
    PGM_NULLNAK = 9,		/* Null NAK */
    PGM_NCF = 10,		/* NAK Confirmation */
    PGM_ACK = 11,		/* ACK for congestion control */
    PGM_SPMR = 12,		/* SPM request */
    PGM_MAX = 255
} pgm_type;

#define PGM_OPT_BIT_PRESENT	0x01
#define PGM_OPT_BIT_NETWORK	0x02
#define PGM_OPT_BIT_VAR_PKTLEN	0x40
#define PGM_OPT_BIT_PARITY	0x80

#define PGM_OPT_LENGTH		0x00
#define PGM_OPT_FRAGMENT        0x01
#define PGM_OPT_NAK_LIST        0x02
#define PGM_OPT_JOIN            0x03
#define PGM_OPT_NAK_BO_IVL	0x04
#define PGM_OPT_NAK_BO_RNG	0x05

#define PGM_OPT_REDIRECT        0x07
#define PGM_OPT_PARITY_PRM      0x08
#define PGM_OPT_PARITY_GRP      0x09
#define PGM_OPT_CURR_TGSIZE     0x0A
#define PGM_OPT_NBR_UNREACH	0x0B
#define PGM_OPT_PATH_NLA	0x0C

#define PGM_OPT_SYN             0x0D
#define PGM_OPT_FIN             0x0E
#define PGM_OPT_RST             0x0F
#define PGM_OPT_CR		0x10
#define PGM_OPT_CRQST		0x11

#define PGM_OPT_PGMCC_DATA	0x12
#define PGM_OPT_PGMCC_FEEDBACK	0x13

#define PGM_OPT_MASK		0x7f

#define PGM_OPT_END		0x80    /* end of options marker */

#define PGM_MIN_OPT_LEN		4

void
pgm_print(netdissect_options *ndo,
          const u_char *bp, u_int length,
          const u_char *bp2)
{
	const struct pgm_header *pgm;
	const struct ip *ip;
	uint8_t pgm_type_val;
	uint16_t sport, dport;
	u_int nla_afnum;
	char nla_buf[INET6_ADDRSTRLEN];
	const struct ip6_hdr *ip6;
	uint8_t opt_type, opt_len;
	uint32_t seq, opts_len, len, offset;

	ndo->ndo_protocol = "pgm";
	pgm = (const struct pgm_header *)bp;
	ip = (const struct ip *)bp2;
	if (IP_V(ip) == 6)
		ip6 = (const struct ip6_hdr *)bp2;
	else
		ip6 = NULL;
	if (!ND_TTEST_2(pgm->pgm_dport)) {
		if (ip6) {
			ND_PRINT("%s > %s:",
				GET_IP6ADDR_STRING(ip6->ip6_src),
				GET_IP6ADDR_STRING(ip6->ip6_dst));
		} else {
			ND_PRINT("%s > %s:",
				GET_IPADDR_STRING(ip->ip_src),
				GET_IPADDR_STRING(ip->ip_dst));
		}
		nd_print_trunc(ndo);
		return;
	}

	sport = GET_BE_U_2(pgm->pgm_sport);
	dport = GET_BE_U_2(pgm->pgm_dport);

	if (ip6) {
		if (GET_U_1(ip6->ip6_nxt) == IPPROTO_PGM) {
			ND_PRINT("%s.%s > %s.%s: ",
				GET_IP6ADDR_STRING(ip6->ip6_src),
				tcpport_string(ndo, sport),
				GET_IP6ADDR_STRING(ip6->ip6_dst),
				tcpport_string(ndo, dport));
		} else {
			ND_PRINT("%s > %s: ",
				tcpport_string(ndo, sport), tcpport_string(ndo, dport));
		}
	} else {
		if (GET_U_1(ip->ip_p) == IPPROTO_PGM) {
			ND_PRINT("%s.%s > %s.%s: ",
				GET_IPADDR_STRING(ip->ip_src),
				tcpport_string(ndo, sport),
				GET_IPADDR_STRING(ip->ip_dst),
				tcpport_string(ndo, dport));
		} else {
			ND_PRINT("%s > %s: ",
				tcpport_string(ndo, sport), tcpport_string(ndo, dport));
		}
	}

	ND_TCHECK_SIZE(pgm);

        ND_PRINT("PGM, length %u", GET_BE_U_2(pgm->pgm_length));

        if (!ndo->ndo_vflag)
            return;

	pgm_type_val = GET_U_1(pgm->pgm_type);
	ND_PRINT(" 0x%02x%02x%02x%02x%02x%02x ",
		     pgm->pgm_gsid[0],
                     pgm->pgm_gsid[1],
                     pgm->pgm_gsid[2],
		     pgm->pgm_gsid[3],
                     pgm->pgm_gsid[4],
                     pgm->pgm_gsid[5]);
	switch (pgm_type_val) {
	case PGM_SPM: {
	    const struct pgm_spm *spm;

	    spm = (const struct pgm_spm *)(pgm + 1);
	    ND_TCHECK_SIZE(spm);
	    bp = (const u_char *) (spm + 1);

	    switch (GET_BE_U_2(spm->pgms_nla_afi)) {
	    case AFNUM_INET:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		addrtostr(bp, nla_buf, sizeof(nla_buf));
		bp += sizeof(nd_ipv4);
		break;
	    case AFNUM_INET6:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
		addrtostr6(bp, nla_buf, sizeof(nla_buf));
		bp += sizeof(nd_ipv6);
		break;
	    default:
		goto trunc;
		break;
	    }

	    ND_PRINT("SPM seq %u trail %u lead %u nla %s",
			 GET_BE_U_4(spm->pgms_seq),
			 GET_BE_U_4(spm->pgms_trailseq),
			 GET_BE_U_4(spm->pgms_leadseq),
			 nla_buf);
	    break;
	}

	case PGM_POLL: {
	    const struct pgm_poll *pgm_poll;
	    uint32_t ivl, rnd, mask;

	    pgm_poll = (const struct pgm_poll *)(pgm + 1);
	    ND_TCHECK_SIZE(pgm_poll);
	    bp = (const u_char *) (pgm_poll + 1);

	    switch (GET_BE_U_2(pgm_poll->pgmp_nla_afi)) {
	    case AFNUM_INET:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		addrtostr(bp, nla_buf, sizeof(nla_buf));
		bp += sizeof(nd_ipv4);
		break;
	    case AFNUM_INET6:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
		addrtostr6(bp, nla_buf, sizeof(nla_buf));
		bp += sizeof(nd_ipv6);
		break;
	    default:
		goto trunc;
		break;
	    }

	    ivl = GET_BE_U_4(bp);
	    bp += sizeof(uint32_t);

	    rnd = GET_BE_U_4(bp);
	    bp += sizeof(uint32_t);

	    mask = GET_BE_U_4(bp);
	    bp += sizeof(uint32_t);

	    ND_PRINT("POLL seq %u round %u nla %s ivl %u rnd 0x%08x "
			 "mask 0x%08x", GET_BE_U_4(pgm_poll->pgmp_seq),
			 GET_BE_U_2(pgm_poll->pgmp_round), nla_buf, ivl, rnd,
			 mask);
	    break;
	}
	case PGM_POLR: {
	    const struct pgm_polr *polr_msg;

	    polr_msg = (const struct pgm_polr *)(pgm + 1);
	    ND_TCHECK_SIZE(polr_msg);
	    ND_PRINT("POLR seq %u round %u",
			 GET_BE_U_4(polr_msg->pgmp_seq),
			 GET_BE_U_2(polr_msg->pgmp_round));
	    bp = (const u_char *) (polr_msg + 1);
	    break;
	}
	case PGM_ODATA: {
	    const struct pgm_data *odata;

	    odata = (const struct pgm_data *)(pgm + 1);
	    ND_TCHECK_SIZE(odata);
	    ND_PRINT("ODATA trail %u seq %u",
			 GET_BE_U_4(odata->pgmd_trailseq),
			 GET_BE_U_4(odata->pgmd_seq));
	    bp = (const u_char *) (odata + 1);
	    break;
	}

	case PGM_RDATA: {
	    const struct pgm_data *rdata;

	    rdata = (const struct pgm_data *)(pgm + 1);
	    ND_TCHECK_SIZE(rdata);
	    ND_PRINT("RDATA trail %u seq %u",
			 GET_BE_U_4(rdata->pgmd_trailseq),
			 GET_BE_U_4(rdata->pgmd_seq));
	    bp = (const u_char *) (rdata + 1);
	    break;
	}

	case PGM_NAK:
	case PGM_NULLNAK:
	case PGM_NCF: {
	    const struct pgm_nak *nak;
	    char source_buf[INET6_ADDRSTRLEN], group_buf[INET6_ADDRSTRLEN];

	    nak = (const struct pgm_nak *)(pgm + 1);
	    ND_TCHECK_SIZE(nak);
	    bp = (const u_char *) (nak + 1);

	    /*
	     * Skip past the source, saving info along the way
	     * and stopping if we don't have enough.
	     */
	    switch (GET_BE_U_2(nak->pgmn_source_afi)) {
	    case AFNUM_INET:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		addrtostr(bp, source_buf, sizeof(source_buf));
		bp += sizeof(nd_ipv4);
		break;
	    case AFNUM_INET6:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
		addrtostr6(bp, source_buf, sizeof(source_buf));
		bp += sizeof(nd_ipv6);
		break;
	    default:
		goto trunc;
		break;
	    }

	    /*
	     * Skip past the group, saving info along the way
	     * and stopping if we don't have enough.
	     */
	    bp += (2 * sizeof(uint16_t));
	    switch (GET_BE_U_2(bp)) {
	    case AFNUM_INET:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		addrtostr(bp, group_buf, sizeof(group_buf));
		bp += sizeof(nd_ipv4);
		break;
	    case AFNUM_INET6:
		ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
		addrtostr6(bp, group_buf, sizeof(group_buf));
		bp += sizeof(nd_ipv6);
		break;
	    default:
		goto trunc;
		break;
	    }

	    /*
	     * Options decoding can go here.
	     */
	    switch (pgm_type_val) {
		case PGM_NAK:
		    ND_PRINT("NAK ");
		    break;
		case PGM_NULLNAK:
		    ND_PRINT("NNAK ");
		    break;
		case PGM_NCF:
		    ND_PRINT("NCF ");
		    break;
		default:
                    break;
	    }
	    ND_PRINT("(%s -> %s), seq %u",
			 source_buf, group_buf, GET_BE_U_4(nak->pgmn_seq));
	    break;
	}

	case PGM_ACK: {
	    const struct pgm_ack *ack;

	    ack = (const struct pgm_ack *)(pgm + 1);
	    ND_TCHECK_SIZE(ack);
	    ND_PRINT("ACK seq %u",
			 GET_BE_U_4(ack->pgma_rx_max_seq));
	    bp = (const u_char *) (ack + 1);
	    break;
	}

	case PGM_SPMR:
	    ND_PRINT("SPMR");
	    break;

	default:
	    ND_PRINT("UNKNOWN type 0x%02x", pgm_type_val);
	    break;

	}
	if (GET_U_1(pgm->pgm_options) & PGM_OPT_BIT_PRESENT) {

	    /*
	     * make sure there's enough for the first option header
	     */
	    ND_TCHECK_LEN(bp, PGM_MIN_OPT_LEN);

	    /*
	     * That option header MUST be an OPT_LENGTH option
	     * (see the first paragraph of section 9.1 in RFC 3208).
	     */
	    opt_type = GET_U_1(bp);
	    bp++;
	    if ((opt_type & PGM_OPT_MASK) != PGM_OPT_LENGTH) {
		ND_PRINT("[First option bad, should be PGM_OPT_LENGTH, is %u]", opt_type & PGM_OPT_MASK);
		return;
	    }
	    opt_len = GET_U_1(bp);
	    bp++;
	    if (opt_len != 4) {
		ND_PRINT("[Bad OPT_LENGTH option, length %u != 4]", opt_len);
		return;
	    }
	    opts_len = GET_BE_U_2(bp);
	    bp += sizeof(uint16_t);
	    if (opts_len < 4) {
		ND_PRINT("[Bad total option length %u < 4]", opts_len);
		return;
	    }
	    ND_PRINT(" OPTS LEN %u", opts_len);
	    opts_len -= 4;

	    while (opts_len) {
		if (opts_len < PGM_MIN_OPT_LEN) {
		    ND_PRINT("[Total option length leaves no room for final option]");
		    return;
		}
		opt_type = GET_U_1(bp);
		bp++;
		opt_len = GET_U_1(bp);
		bp++;
		if (opt_len < PGM_MIN_OPT_LEN) {
		    ND_PRINT("[Bad option, length %u < %u]", opt_len,
		        PGM_MIN_OPT_LEN);
		    break;
		}
		if (opts_len < opt_len) {
		    ND_PRINT("[Total option length leaves no room for final option]");
		    return;
		}
		ND_TCHECK_LEN(bp, opt_len - 2);

		switch (opt_type & PGM_OPT_MASK) {
		case PGM_OPT_LENGTH:
#define PGM_OPT_LENGTH_LEN	(2+2)
		    if (opt_len != PGM_OPT_LENGTH_LEN) {
			ND_PRINT("[Bad OPT_LENGTH option, length %u != %u]",
			    opt_len, PGM_OPT_LENGTH_LEN);
			return;
		    }
		    ND_PRINT(" OPTS LEN (extra?) %u", GET_BE_U_2(bp));
		    bp += 2;
		    opts_len -= PGM_OPT_LENGTH_LEN;
		    break;

		case PGM_OPT_FRAGMENT:
#define PGM_OPT_FRAGMENT_LEN	(2+2+4+4+4)
		    if (opt_len != PGM_OPT_FRAGMENT_LEN) {
			ND_PRINT("[Bad OPT_FRAGMENT option, length %u != %u]",
			    opt_len, PGM_OPT_FRAGMENT_LEN);
			return;
		    }
		    bp += 2;
		    seq = GET_BE_U_4(bp);
		    bp += 4;
		    offset = GET_BE_U_4(bp);
		    bp += 4;
		    len = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" FRAG seq %u off %u len %u", seq, offset, len);
		    opts_len -= PGM_OPT_FRAGMENT_LEN;
		    break;

		case PGM_OPT_NAK_LIST:
		    bp += 2;
		    opt_len -= 4;	/* option header */
		    ND_PRINT(" NAK LIST");
		    while (opt_len) {
			if (opt_len < 4) {
			    ND_PRINT("[Option length not a multiple of 4]");
			    return;
			}
			ND_PRINT(" %u", GET_BE_U_4(bp));
			bp += 4;
			opt_len -= 4;
			opts_len -= 4;
		    }
		    break;

		case PGM_OPT_JOIN:
#define PGM_OPT_JOIN_LEN	(2+2+4)
		    if (opt_len != PGM_OPT_JOIN_LEN) {
			ND_PRINT("[Bad OPT_JOIN option, length %u != %u]",
			    opt_len, PGM_OPT_JOIN_LEN);
			return;
		    }
		    bp += 2;
		    seq = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" JOIN %u", seq);
		    opts_len -= PGM_OPT_JOIN_LEN;
		    break;

		case PGM_OPT_NAK_BO_IVL:
#define PGM_OPT_NAK_BO_IVL_LEN	(2+2+4+4)
		    if (opt_len != PGM_OPT_NAK_BO_IVL_LEN) {
			ND_PRINT("[Bad OPT_NAK_BO_IVL option, length %u != %u]",
			    opt_len, PGM_OPT_NAK_BO_IVL_LEN);
			return;
		    }
		    bp += 2;
		    offset = GET_BE_U_4(bp);
		    bp += 4;
		    seq = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" BACKOFF ivl %u ivlseq %u", offset, seq);
		    opts_len -= PGM_OPT_NAK_BO_IVL_LEN;
		    break;

		case PGM_OPT_NAK_BO_RNG:
#define PGM_OPT_NAK_BO_RNG_LEN	(2+2+4+4)
		    if (opt_len != PGM_OPT_NAK_BO_RNG_LEN) {
			ND_PRINT("[Bad OPT_NAK_BO_RNG option, length %u != %u]",
			    opt_len, PGM_OPT_NAK_BO_RNG_LEN);
			return;
		    }
		    bp += 2;
		    offset = GET_BE_U_4(bp);
		    bp += 4;
		    seq = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" BACKOFF max %u min %u", offset, seq);
		    opts_len -= PGM_OPT_NAK_BO_RNG_LEN;
		    break;

		case PGM_OPT_REDIRECT:
#define PGM_OPT_REDIRECT_FIXED_LEN	(2+2+2+2)
		    if (opt_len < PGM_OPT_REDIRECT_FIXED_LEN) {
			ND_PRINT("[Bad OPT_REDIRECT option, length %u < %u]",
			    opt_len, PGM_OPT_REDIRECT_FIXED_LEN);
			return;
		    }
		    bp += 2;
		    nla_afnum = GET_BE_U_2(bp);
		    bp += 2+2;
		    switch (nla_afnum) {
		    case AFNUM_INET:
			if (opt_len != PGM_OPT_REDIRECT_FIXED_LEN + sizeof(nd_ipv4)) {
			    ND_PRINT("[Bad OPT_REDIRECT option, length %u != %u + address size]",
			        opt_len, PGM_OPT_REDIRECT_FIXED_LEN);
			    return;
			}
			ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
			addrtostr(bp, nla_buf, sizeof(nla_buf));
			bp += sizeof(nd_ipv4);
			opts_len -= PGM_OPT_REDIRECT_FIXED_LEN + sizeof(nd_ipv4);
			break;
		    case AFNUM_INET6:
			if (opt_len != PGM_OPT_REDIRECT_FIXED_LEN + sizeof(nd_ipv6)) {
			    ND_PRINT("[Bad OPT_REDIRECT option, length %u != %u + address size]",
			        opt_len, PGM_OPT_REDIRECT_FIXED_LEN);
			    return;
			}
			ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
			addrtostr6(bp, nla_buf, sizeof(nla_buf));
			bp += sizeof(nd_ipv6);
			opts_len -= PGM_OPT_REDIRECT_FIXED_LEN + sizeof(nd_ipv6);
			break;
		    default:
			goto trunc;
			break;
		    }

		    ND_PRINT(" REDIRECT %s",  nla_buf);
		    break;

		case PGM_OPT_PARITY_PRM:
#define PGM_OPT_PARITY_PRM_LEN	(2+2+4)
		    if (opt_len != PGM_OPT_PARITY_PRM_LEN) {
			ND_PRINT("[Bad OPT_PARITY_PRM option, length %u != %u]",
			    opt_len, PGM_OPT_PARITY_PRM_LEN);
			return;
		    }
		    bp += 2;
		    len = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" PARITY MAXTGS %u", len);
		    opts_len -= PGM_OPT_PARITY_PRM_LEN;
		    break;

		case PGM_OPT_PARITY_GRP:
#define PGM_OPT_PARITY_GRP_LEN	(2+2+4)
		    if (opt_len != PGM_OPT_PARITY_GRP_LEN) {
			ND_PRINT("[Bad OPT_PARITY_GRP option, length %u != %u]",
			    opt_len, PGM_OPT_PARITY_GRP_LEN);
			return;
		    }
		    bp += 2;
		    seq = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" PARITY GROUP %u", seq);
		    opts_len -= PGM_OPT_PARITY_GRP_LEN;
		    break;

		case PGM_OPT_CURR_TGSIZE:
#define PGM_OPT_CURR_TGSIZE_LEN	(2+2+4)
		    if (opt_len != PGM_OPT_CURR_TGSIZE_LEN) {
			ND_PRINT("[Bad OPT_CURR_TGSIZE option, length %u != %u]",
			    opt_len, PGM_OPT_CURR_TGSIZE_LEN);
			return;
		    }
		    bp += 2;
		    len = GET_BE_U_4(bp);
		    bp += 4;
		    ND_PRINT(" PARITY ATGS %u", len);
		    opts_len -= PGM_OPT_CURR_TGSIZE_LEN;
		    break;

		case PGM_OPT_NBR_UNREACH:
#define PGM_OPT_NBR_UNREACH_LEN	(2+2)
		    if (opt_len != PGM_OPT_NBR_UNREACH_LEN) {
			ND_PRINT("[Bad OPT_NBR_UNREACH option, length %u != %u]",
			    opt_len, PGM_OPT_NBR_UNREACH_LEN);
			return;
		    }
		    bp += 2;
		    ND_PRINT(" NBR_UNREACH");
		    opts_len -= PGM_OPT_NBR_UNREACH_LEN;
		    break;

		case PGM_OPT_PATH_NLA:
		    ND_PRINT(" PATH_NLA [%u]", opt_len);
		    bp += opt_len;
		    opts_len -= opt_len;
		    break;

		case PGM_OPT_SYN:
#define PGM_OPT_SYN_LEN	(2+2)
		    if (opt_len != PGM_OPT_SYN_LEN) {
			ND_PRINT("[Bad OPT_SYN option, length %u != %u]",
			    opt_len, PGM_OPT_SYN_LEN);
			return;
		    }
		    bp += 2;
		    ND_PRINT(" SYN");
		    opts_len -= PGM_OPT_SYN_LEN;
		    break;

		case PGM_OPT_FIN:
#define PGM_OPT_FIN_LEN	(2+2)
		    if (opt_len != PGM_OPT_FIN_LEN) {
			ND_PRINT("[Bad OPT_FIN option, length %u != %u]",
			    opt_len, PGM_OPT_FIN_LEN);
			return;
		    }
		    bp += 2;
		    ND_PRINT(" FIN");
		    opts_len -= PGM_OPT_FIN_LEN;
		    break;

		case PGM_OPT_RST:
#define PGM_OPT_RST_LEN	(2+2)
		    if (opt_len != PGM_OPT_RST_LEN) {
			ND_PRINT("[Bad OPT_RST option, length %u != %u]",
			    opt_len, PGM_OPT_RST_LEN);
			return;
		    }
		    bp += 2;
		    ND_PRINT(" RST");
		    opts_len -= PGM_OPT_RST_LEN;
		    break;

		case PGM_OPT_CR:
		    ND_PRINT(" CR");
		    bp += opt_len;
		    opts_len -= opt_len;
		    break;

		case PGM_OPT_CRQST:
#define PGM_OPT_CRQST_LEN	(2+2)
		    if (opt_len != PGM_OPT_CRQST_LEN) {
			ND_PRINT("[Bad OPT_CRQST option, length %u != %u]",
			    opt_len, PGM_OPT_CRQST_LEN);
			return;
		    }
		    bp += 2;
		    ND_PRINT(" CRQST");
		    opts_len -= PGM_OPT_CRQST_LEN;
		    break;

		case PGM_OPT_PGMCC_DATA:
#define PGM_OPT_PGMCC_DATA_FIXED_LEN	(2+2+4+2+2)
		    if (opt_len < PGM_OPT_PGMCC_DATA_FIXED_LEN) {
			ND_PRINT("[Bad OPT_PGMCC_DATA option, length %u < %u]",
			    opt_len, PGM_OPT_PGMCC_DATA_FIXED_LEN);
			return;
		    }
		    bp += 2;
		    offset = GET_BE_U_4(bp);
		    bp += 4;
		    nla_afnum = GET_BE_U_2(bp);
		    bp += 2+2;
		    switch (nla_afnum) {
		    case AFNUM_INET:
			if (opt_len != PGM_OPT_PGMCC_DATA_FIXED_LEN + sizeof(nd_ipv4)) {
			    ND_PRINT("[Bad OPT_PGMCC_DATA option, length %u != %u + address size]",
			        opt_len, PGM_OPT_PGMCC_DATA_FIXED_LEN);
			    return;
			}
			ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
			addrtostr(bp, nla_buf, sizeof(nla_buf));
			bp += sizeof(nd_ipv4);
			opts_len -= PGM_OPT_PGMCC_DATA_FIXED_LEN + sizeof(nd_ipv4);
			break;
		    case AFNUM_INET6:
			if (opt_len != PGM_OPT_PGMCC_DATA_FIXED_LEN + sizeof(nd_ipv6)) {
			    ND_PRINT("[Bad OPT_PGMCC_DATA option, length %u != %u + address size]",
			        opt_len, PGM_OPT_PGMCC_DATA_FIXED_LEN);
			    return;
			}
			ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
			addrtostr6(bp, nla_buf, sizeof(nla_buf));
			bp += sizeof(nd_ipv6);
			opts_len -= PGM_OPT_PGMCC_DATA_FIXED_LEN + sizeof(nd_ipv6);
			break;
		    default:
			goto trunc;
			break;
		    }

		    ND_PRINT(" PGMCC DATA %u %s", offset, nla_buf);
		    break;

		case PGM_OPT_PGMCC_FEEDBACK:
#define PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN	(2+2+4+2+2)
		    if (opt_len < PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN) {
			ND_PRINT("[Bad PGM_OPT_PGMCC_FEEDBACK option, length %u < %u]",
			    opt_len, PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN);
			return;
		    }
		    bp += 2;
		    offset = GET_BE_U_4(bp);
		    bp += 4;
		    nla_afnum = GET_BE_U_2(bp);
		    bp += 2+2;
		    switch (nla_afnum) {
		    case AFNUM_INET:
			if (opt_len != PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN + sizeof(nd_ipv4)) {
			    ND_PRINT("[Bad OPT_PGMCC_FEEDBACK option, length %u != %u + address size]",
			        opt_len, PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN);
			    return;
			}
			ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
			addrtostr(bp, nla_buf, sizeof(nla_buf));
			bp += sizeof(nd_ipv4);
			opts_len -= PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN + sizeof(nd_ipv4);
			break;
		    case AFNUM_INET6:
			if (opt_len != PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN + sizeof(nd_ipv6)) {
			    ND_PRINT("[Bad OPT_PGMCC_FEEDBACK option, length %u != %u + address size]",
			        opt_len, PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN);
			    return;
			}
			ND_TCHECK_LEN(bp, sizeof(nd_ipv6));
			addrtostr6(bp, nla_buf, sizeof(nla_buf));
			bp += sizeof(nd_ipv6);
			opts_len -= PGM_OPT_PGMCC_FEEDBACK_FIXED_LEN + sizeof(nd_ipv6);
			break;
		    default:
			goto trunc;
			break;
		    }

		    ND_PRINT(" PGMCC FEEDBACK %u %s", offset, nla_buf);
		    break;

		default:
		    ND_PRINT(" OPT_%02X [%u] ", opt_type, opt_len);
		    bp += opt_len;
		    opts_len -= opt_len;
		    break;
		}

		if (opt_type & PGM_OPT_END)
		    break;
	     }
	}

	ND_PRINT(" [%u]", length);
	if (ndo->ndo_packettype == PT_PGM_ZMTP1 &&
	    (pgm_type_val == PGM_ODATA || pgm_type_val == PGM_RDATA))
		zmtp1_datagram_print(ndo, bp,
				     GET_BE_U_2(pgm->pgm_length));

	return;

trunc:
	nd_print_trunc(ndo);
}
