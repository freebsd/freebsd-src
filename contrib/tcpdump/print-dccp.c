/*
 * Copyright (C) Arnaldo Carvalho de Melo 2004
 * Copyright (C) Ian McDonald 2005
 * Copyright (C) Yoshifumi Nishida 2005
 *
 * This software may be distributed either under the terms of the
 * BSD-style license that accompanies tcpdump or the GNU GPL version 2
 */

/* \summary: Datagram Congestion Control Protocol (DCCP) printer */

/* specification: RFC 4340 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"
#include "ip.h"
#include "ip6.h"
#include "ipproto.h"

/* RFC4340: Datagram Congestion Control Protocol (DCCP) */

/**
 * struct dccp_hdr - generic part of DCCP packet header, with a 24-bit
 * sequence number
 *
 * @dccph_sport - Relevant port on the endpoint that sent this packet
 * @dccph_dport - Relevant port on the other endpoint
 * @dccph_doff - Data Offset from the start of the DCCP header, in 32-bit words
 * @dccph_ccval - Used by the HC-Sender CCID
 * @dccph_cscov - Parts of the packet that are covered by the Checksum field
 * @dccph_checksum - Internet checksum, depends on dccph_cscov
 * @dccph_x - 0 = 24 bit sequence number, 1 = 48
 * @dccph_type - packet type, see DCCP_PKT_ prefixed macros
 * @dccph_seq - 24-bit sequence number
 */
struct dccp_hdr {
	nd_uint16_t	dccph_sport,
			dccph_dport;
	nd_uint8_t	dccph_doff;
	nd_uint8_t	dccph_ccval_cscov;
	nd_uint16_t	dccph_checksum;
	nd_uint8_t	dccph_xtr;
	nd_uint24_t	dccph_seq;
};

/**
 * struct dccp_hdr_ext - generic part of DCCP packet header, with a 48-bit
 * sequence number
 *
 * @dccph_sport - Relevant port on the endpoint that sent this packet
 * @dccph_dport - Relevant port on the other endpoint
 * @dccph_doff - Data Offset from the start of the DCCP header, in 32-bit words
 * @dccph_ccval - Used by the HC-Sender CCID
 * @dccph_cscov - Parts of the packet that are covered by the Checksum field
 * @dccph_checksum - Internet checksum, depends on dccph_cscov
 * @dccph_x - 0 = 24 bit sequence number, 1 = 48
 * @dccph_type - packet type, see DCCP_PKT_ prefixed macros
 * @dccph_seq - 48-bit sequence number
 */
struct dccp_hdr_ext {
	nd_uint16_t	dccph_sport,
			dccph_dport;
	nd_uint8_t	dccph_doff;
	nd_uint8_t	dccph_ccval_cscov;
	nd_uint16_t	dccph_checksum;
	nd_uint8_t	dccph_xtr;
	nd_uint8_t	reserved;
	nd_uint48_t	dccph_seq;
};

#define DCCPH_CCVAL(dh)	((GET_U_1((dh)->dccph_ccval_cscov) >> 4) & 0xF)
#define DCCPH_CSCOV(dh)	(GET_U_1((dh)->dccph_ccval_cscov) & 0xF)

#define DCCPH_X(dh)	(GET_U_1((dh)->dccph_xtr) & 1)
#define DCCPH_TYPE(dh)	((GET_U_1((dh)->dccph_xtr) >> 1) & 0xF)

/**
 * struct dccp_hdr_request - Connection initiation request header
 *
 * @dccph_req_service - Service to which the client app wants to connect
 */
struct dccp_hdr_request {
	nd_uint32_t	dccph_req_service;
};

/**
 * struct dccp_hdr_response - Connection initiation response header
 *
 * @dccph_resp_ack - 48 bit ack number, contains GSR
 * @dccph_resp_service - Echoes the Service Code on a received DCCP-Request
 */
struct dccp_hdr_response {
	nd_uint64_t	dccph_resp_ack;	/* always 8 bytes, first 2 reserved */
	nd_uint32_t	dccph_resp_service;
};

/**
 * struct dccp_hdr_reset - Unconditionally shut down a connection
 *
 * @dccph_resp_ack - 48 bit ack number
 * @dccph_reset_service - Echoes the Service Code on a received DCCP-Request
 */
struct dccp_hdr_reset {
	nd_uint64_t	dccph_reset_ack;	/* always 8 bytes, first 2 reserved */
	nd_uint8_t	dccph_reset_code;
	nd_uint8_t	dccph_reset_data1;
	nd_uint8_t	dccph_reset_data2;
	nd_uint8_t	dccph_reset_data3;
};

enum dccp_pkt_type {
	DCCP_PKT_REQUEST = 0,
	DCCP_PKT_RESPONSE,
	DCCP_PKT_DATA,
	DCCP_PKT_ACK,
	DCCP_PKT_DATAACK,
	DCCP_PKT_CLOSEREQ,
	DCCP_PKT_CLOSE,
	DCCP_PKT_RESET,
	DCCP_PKT_SYNC,
	DCCP_PKT_SYNCACK
};

static const struct tok dccp_pkt_type_str[] = {
	{ DCCP_PKT_REQUEST, "DCCP-Request" },
	{ DCCP_PKT_RESPONSE, "DCCP-Response" },
	{ DCCP_PKT_DATA, "DCCP-Data" },
	{ DCCP_PKT_ACK, "DCCP-Ack" },
	{ DCCP_PKT_DATAACK, "DCCP-DataAck" },
	{ DCCP_PKT_CLOSEREQ, "DCCP-CloseReq" },
	{ DCCP_PKT_CLOSE, "DCCP-Close" },
	{ DCCP_PKT_RESET, "DCCP-Reset" },
	{ DCCP_PKT_SYNC, "DCCP-Sync" },
	{ DCCP_PKT_SYNCACK, "DCCP-SyncAck" },
	{ 0, NULL}
};

enum dccp_reset_codes {
	DCCP_RESET_CODE_UNSPECIFIED = 0,
	DCCP_RESET_CODE_CLOSED,
	DCCP_RESET_CODE_ABORTED,
	DCCP_RESET_CODE_NO_CONNECTION,
	DCCP_RESET_CODE_PACKET_ERROR,
	DCCP_RESET_CODE_OPTION_ERROR,
	DCCP_RESET_CODE_MANDATORY_ERROR,
	DCCP_RESET_CODE_CONNECTION_REFUSED,
	DCCP_RESET_CODE_BAD_SERVICE_CODE,
	DCCP_RESET_CODE_TOO_BUSY,
	DCCP_RESET_CODE_BAD_INIT_COOKIE,
	DCCP_RESET_CODE_AGGRESSION_PENALTY,
	__DCCP_RESET_CODE_LAST
};


static const char *dccp_reset_codes[] = {
	"unspecified",
	"closed",
	"aborted",
	"no_connection",
	"packet_error",
	"option_error",
	"mandatory_error",
	"connection_refused",
	"bad_service_code",
	"too_busy",
	"bad_init_cookie",
	"aggression_penalty",
};

static const char *dccp_feature_nums[] = {
	"reserved",
	"ccid",
	"allow_short_seqno",
	"sequence_window",
	"ecn_incapable",
	"ack_ratio",
	"send_ack_vector",
	"send_ndp_count",
	"minimum checksum coverage",
	"check data checksum",
};

static u_int
dccp_csum_coverage(netdissect_options *ndo,
		   const struct dccp_hdr *dh, u_int len)
{
	u_int cov;

	if (DCCPH_CSCOV(dh) == 0)
		return len;
	cov = (GET_U_1(dh->dccph_doff) + DCCPH_CSCOV(dh) - 1) * sizeof(uint32_t);
	return (cov > len)? len : cov;
}

static uint16_t dccp_cksum(netdissect_options *ndo, const struct ip *ip,
	const struct dccp_hdr *dh, u_int len)
{
	return nextproto4_cksum(ndo, ip, (const uint8_t *)(const void *)dh, len,
				dccp_csum_coverage(ndo, dh, len), IPPROTO_DCCP);
}

static uint16_t dccp6_cksum(netdissect_options *ndo, const struct ip6_hdr *ip6,
	const struct dccp_hdr *dh, u_int len)
{
	return nextproto6_cksum(ndo, ip6, (const uint8_t *)(const void *)dh, len,
				dccp_csum_coverage(ndo, dh, len), IPPROTO_DCCP);
}

static const char *dccp_reset_code(uint8_t code)
{
	if (code >= __DCCP_RESET_CODE_LAST)
		return "invalid";
	return dccp_reset_codes[code];
}

static uint64_t
dccp_seqno(netdissect_options *ndo, const u_char *bp)
{
	const struct dccp_hdr *dh = (const struct dccp_hdr *)bp;
	uint64_t seqno;

	if (DCCPH_X(dh) != 0) {
		const struct dccp_hdr_ext *dhx = (const struct dccp_hdr_ext *)bp;
		seqno = GET_BE_U_6(dhx->dccph_seq);
	} else {
		seqno = GET_BE_U_3(dh->dccph_seq);
	}

	return seqno;
}

static unsigned int
dccp_basic_hdr_len(netdissect_options *ndo, const struct dccp_hdr *dh)
{
	return DCCPH_X(dh) ? sizeof(struct dccp_hdr_ext) : sizeof(struct dccp_hdr);
}

static void dccp_print_ack_no(netdissect_options *ndo, const u_char *bp)
{
	const struct dccp_hdr *dh = (const struct dccp_hdr *)bp;
	const u_char *ackp = bp + dccp_basic_hdr_len(ndo, dh);
	uint64_t ackno;

	if (DCCPH_X(dh) != 0) {
		ackno = GET_BE_U_6(ackp + 2);
	} else {
		ackno = GET_BE_U_3(ackp + 1);
	}

	ND_PRINT("(ack=%" PRIu64 ") ", ackno);
}

static u_int dccp_print_option(netdissect_options *, const u_char *, u_int);

/**
 * dccp_print - show dccp packet
 * @bp - beginning of dccp packet
 * @data2 - beginning of enclosing
 * @len - length of ip packet
 */
void
dccp_print(netdissect_options *ndo, const u_char *bp, const u_char *data2,
	   u_int len)
{
	const struct dccp_hdr *dh;
	const struct ip *ip;
	const struct ip6_hdr *ip6;
	const u_char *cp;
	u_short sport, dport;
	u_int hlen;
	u_int fixed_hdrlen;
	uint8_t	dccph_type;

	ndo->ndo_protocol = "dccp";
	dh = (const struct dccp_hdr *)bp;

	ip = (const struct ip *)data2;
	if (IP_V(ip) == 6)
		ip6 = (const struct ip6_hdr *)data2;
	else
		ip6 = NULL;

	/* make sure we have enough data to look at the X bit */
	cp = (const u_char *)(dh + 1);
	if (cp > ndo->ndo_snapend)
		goto trunc;
	if (len < sizeof(struct dccp_hdr)) {
		ND_PRINT("truncated-dccp - %zu bytes missing!",
			 sizeof(struct dccp_hdr) - len);
		return;
	}

	/* get the length of the generic header */
	fixed_hdrlen = dccp_basic_hdr_len(ndo, dh);
	if (len < fixed_hdrlen) {
		ND_PRINT("truncated-dccp - %u bytes missing!",
			  fixed_hdrlen - len);
		return;
	}
	ND_TCHECK_LEN(dh, fixed_hdrlen);

	sport = GET_BE_U_2(dh->dccph_sport);
	dport = GET_BE_U_2(dh->dccph_dport);
	hlen = GET_U_1(dh->dccph_doff) * 4;

	if (ip6) {
		ND_PRINT("%s.%u > %s.%u: ",
			  GET_IP6ADDR_STRING(ip6->ip6_src), sport,
			  GET_IP6ADDR_STRING(ip6->ip6_dst), dport);
	} else {
		ND_PRINT("%s.%u > %s.%u: ",
			  GET_IPADDR_STRING(ip->ip_src), sport,
			  GET_IPADDR_STRING(ip->ip_dst), dport);
	}

	nd_print_protocol_caps(ndo);

	if (ndo->ndo_qflag) {
		ND_PRINT(" %u", len - hlen);
		if (hlen > len) {
			ND_PRINT(" [bad hdr length %u - too long, > %u]",
				  hlen, len);
		}
		return;
	}

	/* other variables in generic header */
	if (ndo->ndo_vflag) {
		ND_PRINT(" (CCVal %u, CsCov %u", DCCPH_CCVAL(dh), DCCPH_CSCOV(dh));
	}

	/* checksum calculation */
	if (ndo->ndo_vflag && ND_TTEST_LEN(bp, len)) {
		uint16_t sum = 0, dccp_sum;

		dccp_sum = GET_BE_U_2(dh->dccph_checksum);
		ND_PRINT(", cksum 0x%04x ", dccp_sum);
		if (IP_V(ip) == 4)
			sum = dccp_cksum(ndo, ip, dh, len);
		else if (IP_V(ip) == 6)
			sum = dccp6_cksum(ndo, ip6, dh, len);
		if (sum != 0)
			ND_PRINT("(incorrect -> 0x%04x)",in_cksum_shouldbe(dccp_sum, sum));
		else
			ND_PRINT("(correct)");
	}

	if (ndo->ndo_vflag)
		ND_PRINT(")");
	ND_PRINT(" ");

	dccph_type = DCCPH_TYPE(dh);
	switch (dccph_type) {
	case DCCP_PKT_REQUEST: {
		const struct dccp_hdr_request *dhr =
			(const struct dccp_hdr_request *)(bp + fixed_hdrlen);
		fixed_hdrlen += 4;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_TCHECK_SIZE(dhr);
		ND_PRINT("%s (service=%u) ",
			  tok2str(dccp_pkt_type_str, "", dccph_type),
			  GET_BE_U_4(dhr->dccph_req_service));
		break;
	}
	case DCCP_PKT_RESPONSE: {
		const struct dccp_hdr_response *dhr =
			(const struct dccp_hdr_response *)(bp + fixed_hdrlen);
		fixed_hdrlen += 12;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_TCHECK_SIZE(dhr);
		ND_PRINT("%s (service=%u) ",
			  tok2str(dccp_pkt_type_str, "", dccph_type),
			  GET_BE_U_4(dhr->dccph_resp_service));
		break;
	}
	case DCCP_PKT_DATA:
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	case DCCP_PKT_ACK: {
		fixed_hdrlen += 8;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	}
	case DCCP_PKT_DATAACK: {
		fixed_hdrlen += 8;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	}
	case DCCP_PKT_CLOSEREQ:
		fixed_hdrlen += 8;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	case DCCP_PKT_CLOSE:
		fixed_hdrlen += 8;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	case DCCP_PKT_RESET: {
		const struct dccp_hdr_reset *dhr =
			(const struct dccp_hdr_reset *)(bp + fixed_hdrlen);
		fixed_hdrlen += 12;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_TCHECK_SIZE(dhr);
		ND_PRINT("%s (code=%s) ",
			  tok2str(dccp_pkt_type_str, "", dccph_type),
			  dccp_reset_code(GET_U_1(dhr->dccph_reset_code)));
		break;
	}
	case DCCP_PKT_SYNC:
		fixed_hdrlen += 8;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	case DCCP_PKT_SYNCACK:
		fixed_hdrlen += 8;
		if (len < fixed_hdrlen) {
			ND_PRINT("truncated-%s - %u bytes missing!",
				  tok2str(dccp_pkt_type_str, "", dccph_type),
				  fixed_hdrlen - len);
			return;
		}
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "", dccph_type));
		break;
	default:
		ND_PRINT("%s ", tok2str(dccp_pkt_type_str, "unknown-type-%u", dccph_type));
		break;
	}

	if ((DCCPH_TYPE(dh) != DCCP_PKT_DATA) &&
			(DCCPH_TYPE(dh) != DCCP_PKT_REQUEST))
		dccp_print_ack_no(ndo, bp);

	if (ndo->ndo_vflag < 2)
		return;

	ND_PRINT("seq %" PRIu64, dccp_seqno(ndo, bp));

	/* process options */
	if (hlen > fixed_hdrlen){
		u_int optlen;
		cp = bp + fixed_hdrlen;
		ND_PRINT(" <");

		hlen -= fixed_hdrlen;
		while(1){
			optlen = dccp_print_option(ndo, cp, hlen);
			if (!optlen)
				break;
			if (hlen <= optlen)
				break;
			hlen -= optlen;
			cp += optlen;
			ND_PRINT(", ");
		}
		ND_PRINT(">");
	}
	return;
trunc:
	nd_print_trunc(ndo);
}

static const struct tok dccp_option_values[] = {
	{ 0, "nop" },
	{ 1, "mandatory" },
	{ 2, "slowreceiver" },
	{ 32, "change_l" },
	{ 33, "confirm_l" },
	{ 34, "change_r" },
	{ 35, "confirm_r" },
	{ 36, "initcookie" },
	{ 37, "ndp_count" },
	{ 38, "ack_vector0" },
	{ 39, "ack_vector1" },
	{ 40, "data_dropped" },
	{ 41, "timestamp" },
	{ 42, "timestamp_echo" },
	{ 43, "elapsed_time" },
	{ 44, "data_checksum" },
	{ 0, NULL }
};

static u_int
dccp_print_option(netdissect_options *ndo, const u_char *option, u_int hlen)
{
	uint8_t optlen, i;

	if (GET_U_1(option) >= 32) {
		optlen = GET_U_1(option + 1);
		if (optlen < 2) {
			if (GET_U_1(option) >= 128)
				ND_PRINT("CCID option %u optlen too short",
					 GET_U_1(option));
			else
				ND_PRINT("%s optlen too short",
					  tok2str(dccp_option_values, "Option %u", GET_U_1(option)));
			return 0;
		}
	} else
		optlen = 1;

	if (hlen < optlen) {
		if (GET_U_1(option) >= 128)
			ND_PRINT("CCID option %u optlen goes past header length",
				  GET_U_1(option));
		else
			ND_PRINT("%s optlen goes past header length",
				  tok2str(dccp_option_values, "Option %u", GET_U_1(option)));
		return 0;
	}
	ND_TCHECK_LEN(option, optlen);

	if (GET_U_1(option) >= 128) {
		ND_PRINT("CCID option %u", GET_U_1(option));
		switch (optlen) {
			case 4:
				ND_PRINT(" %u", GET_BE_U_2(option + 2));
				break;
			case 6:
				ND_PRINT(" %u", GET_BE_U_4(option + 2));
				break;
			default:
				break;
		}
	} else {
		ND_PRINT("%s",
			 tok2str(dccp_option_values, "Option %u", GET_U_1(option)));
		switch (GET_U_1(option)) {
		case 32:
		case 33:
		case 34:
		case 35:
			if (optlen < 3) {
				ND_PRINT(" optlen too short");
				return optlen;
			}
			if (GET_U_1(option + 2) < 10){
				ND_PRINT(" %s",
					 dccp_feature_nums[GET_U_1(option + 2)]);
				for (i = 0; i < optlen - 3; i++)
					ND_PRINT(" %u",
						 GET_U_1(option + 3 + i));
			}
			break;
		case 36:
			if (optlen > 2) {
				ND_PRINT(" 0x");
				for (i = 0; i < optlen - 2; i++)
					ND_PRINT("%02x",
						 GET_U_1(option + 2 + i));
			}
			break;
		case 37:
			for (i = 0; i < optlen - 2; i++)
				ND_PRINT(" %u", GET_U_1(option + 2 + i));
			break;
		case 38:
			if (optlen > 2) {
				ND_PRINT(" 0x");
				for (i = 0; i < optlen - 2; i++)
					ND_PRINT("%02x",
						 GET_U_1(option + 2 + i));
			}
			break;
		case 39:
			if (optlen > 2) {
				ND_PRINT(" 0x");
				for (i = 0; i < optlen - 2; i++)
					ND_PRINT("%02x",
						 GET_U_1(option + 2 + i));
			}
			break;
		case 40:
			if (optlen > 2) {
				ND_PRINT(" 0x");
				for (i = 0; i < optlen - 2; i++)
					ND_PRINT("%02x",
						 GET_U_1(option + 2 + i));
			}
			break;
		case 41:
		/*
		 * 13.1.  Timestamp Option
		 *
		 *  +--------+--------+--------+--------+--------+--------+
		 *  |00101001|00000110|          Timestamp Value          |
		 *  +--------+--------+--------+--------+--------+--------+
		 *   Type=41  Length=6
		 */
			if (optlen == 6)
				ND_PRINT(" %u", GET_BE_U_4(option + 2));
			else
				ND_PRINT(" [optlen != 6]");
			break;
		case 42:
		/*
		 * 13.3.  Timestamp Echo Option
		 *
		 *  +--------+--------+--------+--------+--------+--------+
		 *  |00101010|00000110|           Timestamp Echo          |
		 *  +--------+--------+--------+--------+--------+--------+
		 *   Type=42    Len=6
		 *
		 *  +--------+--------+------- ... -------+--------+--------+
		 *  |00101010|00001000|  Timestamp Echo   |   Elapsed Time  |
		 *  +--------+--------+------- ... -------+--------+--------+
		 *   Type=42    Len=8       (4 bytes)
		 *
		 *  +--------+--------+------- ... -------+------- ... -------+
		 *  |00101010|00001010|  Timestamp Echo   |    Elapsed Time   |
		 *  +--------+--------+------- ... -------+------- ... -------+
		 *   Type=42   Len=10       (4 bytes)           (4 bytes)
		 */
			switch (optlen) {
			case 6:
				ND_PRINT(" %u", GET_BE_U_4(option + 2));
				break;
			case 8:
				ND_PRINT(" %u", GET_BE_U_4(option + 2));
				ND_PRINT(" (elapsed time %u)",
					 GET_BE_U_2(option + 6));
				break;
			case 10:
				ND_PRINT(" %u", GET_BE_U_4(option + 2));
				ND_PRINT(" (elapsed time %u)",
					 GET_BE_U_4(option + 6));
				break;
			default:
				ND_PRINT(" [optlen != 6 or 8 or 10]");
				break;
			}
			break;
		case 43:
			if (optlen == 6)
				ND_PRINT(" %u", GET_BE_U_4(option + 2));
			else if (optlen == 4)
				ND_PRINT(" %u", GET_BE_U_2(option + 2));
			else
				ND_PRINT(" [optlen != 4 or 6]");
			break;
		case 44:
			if (optlen > 2) {
				ND_PRINT(" ");
				for (i = 0; i < optlen - 2; i++)
					ND_PRINT("%02x",
						 GET_U_1(option + 2 + i));
			}
			break;
		}
	}

	return optlen;
trunc:
	nd_print_trunc(ndo);
	return 0;
}
