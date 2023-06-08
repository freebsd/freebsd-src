/*
 * Copyright (c) 1991, 1993, 1994, 1995, 1996, 1997
 *      The Regents of the University of California.  All rights reserved.
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
 * L2TP support contributed by Motonori Shindo (mshindo@mshindo.net)
 */

/* \summary: Layer Two Tunneling Protocol (L2TP) printer */

/* specification: RFC 2661 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

#define L2TP_FLAG_TYPE		0x8000	/* Type (0=Data, 1=Control) */
#define L2TP_FLAG_LENGTH	0x4000	/* Length */
#define L2TP_FLAG_SEQUENCE	0x0800	/* Sequence */
#define L2TP_FLAG_OFFSET	0x0200	/* Offset */
#define L2TP_FLAG_PRIORITY	0x0100	/* Priority */

#define L2TP_VERSION_MASK	0x000f	/* Version Mask */
#define L2TP_VERSION_L2F	0x0001	/* L2F */
#define L2TP_VERSION_L2TP	0x0002	/* L2TP */

#define L2TP_AVP_HDR_FLAG_MANDATORY	0x8000	/* Mandatory Flag */
#define L2TP_AVP_HDR_FLAG_HIDDEN	0x4000	/* Hidden Flag */
#define L2TP_AVP_HDR_LEN_MASK		0x03ff	/* Length Mask */

#define L2TP_FRAMING_CAP_SYNC_MASK	0x00000001	/* Synchronous */
#define L2TP_FRAMING_CAP_ASYNC_MASK	0x00000002	/* Asynchronous */

#define L2TP_FRAMING_TYPE_SYNC_MASK	0x00000001	/* Synchronous */
#define L2TP_FRAMING_TYPE_ASYNC_MASK	0x00000002	/* Asynchronous */

#define L2TP_BEARER_CAP_DIGITAL_MASK	0x00000001	/* Digital */
#define L2TP_BEARER_CAP_ANALOG_MASK	0x00000002	/* Analog */

#define L2TP_BEARER_TYPE_DIGITAL_MASK	0x00000001	/* Digital */
#define L2TP_BEARER_TYPE_ANALOG_MASK	0x00000002	/* Analog */

/* Authen Type */
#define L2TP_AUTHEN_TYPE_RESERVED	0x0000	/* Reserved */
#define L2TP_AUTHEN_TYPE_TEXTUAL	0x0001	/* Textual username/password exchange */
#define L2TP_AUTHEN_TYPE_CHAP		0x0002	/* PPP CHAP */
#define L2TP_AUTHEN_TYPE_PAP		0x0003	/* PPP PAP */
#define L2TP_AUTHEN_TYPE_NO_AUTH	0x0004	/* No Authentication */
#define L2TP_AUTHEN_TYPE_MSCHAPv1	0x0005	/* MSCHAPv1 */

#define L2TP_PROXY_AUTH_ID_MASK		0x00ff


#define	L2TP_MSGTYPE_SCCRQ	1  /* Start-Control-Connection-Request */
#define	L2TP_MSGTYPE_SCCRP	2  /* Start-Control-Connection-Reply */
#define	L2TP_MSGTYPE_SCCCN	3  /* Start-Control-Connection-Connected */
#define	L2TP_MSGTYPE_STOPCCN	4  /* Stop-Control-Connection-Notification */
#define	L2TP_MSGTYPE_HELLO	6  /* Hello */
#define	L2TP_MSGTYPE_OCRQ	7  /* Outgoing-Call-Request */
#define	L2TP_MSGTYPE_OCRP	8  /* Outgoing-Call-Reply */
#define	L2TP_MSGTYPE_OCCN	9  /* Outgoing-Call-Connected */
#define	L2TP_MSGTYPE_ICRQ	10 /* Incoming-Call-Request */
#define	L2TP_MSGTYPE_ICRP	11 /* Incoming-Call-Reply */
#define	L2TP_MSGTYPE_ICCN	12 /* Incoming-Call-Connected */
#define	L2TP_MSGTYPE_CDN	14 /* Call-Disconnect-Notify */
#define	L2TP_MSGTYPE_WEN	15 /* WAN-Error-Notify */
#define	L2TP_MSGTYPE_SLI	16 /* Set-Link-Info */

static const struct tok l2tp_msgtype2str[] = {
	{ L2TP_MSGTYPE_SCCRQ,	"SCCRQ" },
	{ L2TP_MSGTYPE_SCCRP,	"SCCRP" },
	{ L2TP_MSGTYPE_SCCCN,	"SCCCN" },
	{ L2TP_MSGTYPE_STOPCCN,	"StopCCN" },
	{ L2TP_MSGTYPE_HELLO,	"HELLO" },
	{ L2TP_MSGTYPE_OCRQ,	"OCRQ" },
	{ L2TP_MSGTYPE_OCRP,	"OCRP" },
	{ L2TP_MSGTYPE_OCCN,	"OCCN" },
	{ L2TP_MSGTYPE_ICRQ,	"ICRQ" },
	{ L2TP_MSGTYPE_ICRP,	"ICRP" },
	{ L2TP_MSGTYPE_ICCN,	"ICCN" },
	{ L2TP_MSGTYPE_CDN,	"CDN" },
	{ L2TP_MSGTYPE_WEN,	"WEN" },
	{ L2TP_MSGTYPE_SLI,	"SLI" },
	{ 0,			NULL }
};

#define L2TP_AVP_MSGTYPE		0  /* Message Type */
#define L2TP_AVP_RESULT_CODE		1  /* Result Code */
#define L2TP_AVP_PROTO_VER		2  /* Protocol Version */
#define L2TP_AVP_FRAMING_CAP		3  /* Framing Capabilities */
#define L2TP_AVP_BEARER_CAP		4  /* Bearer Capabilities */
#define L2TP_AVP_TIE_BREAKER		5  /* Tie Breaker */
#define L2TP_AVP_FIRM_VER		6  /* Firmware Revision */
#define L2TP_AVP_HOST_NAME		7  /* Host Name */
#define L2TP_AVP_VENDOR_NAME		8  /* Vendor Name */
#define L2TP_AVP_ASSND_TUN_ID		9  /* Assigned Tunnel ID */
#define L2TP_AVP_RECV_WIN_SIZE		10 /* Receive Window Size */
#define L2TP_AVP_CHALLENGE		11 /* Challenge */
#define L2TP_AVP_Q931_CC		12 /* Q.931 Cause Code */
#define L2TP_AVP_CHALLENGE_RESP		13 /* Challenge Response */
#define L2TP_AVP_ASSND_SESS_ID		14 /* Assigned Session ID */
#define L2TP_AVP_CALL_SER_NUM		15 /* Call Serial Number */
#define L2TP_AVP_MINIMUM_BPS		16 /* Minimum BPS */
#define L2TP_AVP_MAXIMUM_BPS		17 /* Maximum BPS */
#define L2TP_AVP_BEARER_TYPE		18 /* Bearer Type */
#define L2TP_AVP_FRAMING_TYPE		19 /* Framing Type */
#define L2TP_AVP_PACKET_PROC_DELAY	20 /* Packet Processing Delay (OBSOLETE) */
#define L2TP_AVP_CALLED_NUMBER		21 /* Called Number */
#define L2TP_AVP_CALLING_NUMBER		22 /* Calling Number */
#define L2TP_AVP_SUB_ADDRESS		23 /* Sub-Address */
#define L2TP_AVP_TX_CONN_SPEED		24 /* (Tx) Connect Speed */
#define L2TP_AVP_PHY_CHANNEL_ID		25 /* Physical Channel ID */
#define L2TP_AVP_INI_RECV_LCP		26 /* Initial Received LCP CONFREQ */
#define L2TP_AVP_LAST_SENT_LCP		27 /* Last Sent LCP CONFREQ */
#define L2TP_AVP_LAST_RECV_LCP		28 /* Last Received LCP CONFREQ */
#define L2TP_AVP_PROXY_AUTH_TYPE	29 /* Proxy Authen Type */
#define L2TP_AVP_PROXY_AUTH_NAME	30 /* Proxy Authen Name */
#define L2TP_AVP_PROXY_AUTH_CHAL	31 /* Proxy Authen Challenge */
#define L2TP_AVP_PROXY_AUTH_ID		32 /* Proxy Authen ID */
#define L2TP_AVP_PROXY_AUTH_RESP	33 /* Proxy Authen Response */
#define L2TP_AVP_CALL_ERRORS		34 /* Call Errors */
#define L2TP_AVP_ACCM			35 /* ACCM */
#define L2TP_AVP_RANDOM_VECTOR		36 /* Random Vector */
#define L2TP_AVP_PRIVATE_GRP_ID		37 /* Private Group ID */
#define L2TP_AVP_RX_CONN_SPEED		38 /* (Rx) Connect Speed */
#define L2TP_AVP_SEQ_REQUIRED		39 /* Sequencing Required */
#define L2TP_AVP_PPP_DISCON_CC		46 /* PPP Disconnect Cause Code - RFC 3145 */

static const struct tok l2tp_avp2str[] = {
	{ L2TP_AVP_MSGTYPE,		"MSGTYPE" },
	{ L2TP_AVP_RESULT_CODE,		"RESULT_CODE" },
	{ L2TP_AVP_PROTO_VER,		"PROTO_VER" },
	{ L2TP_AVP_FRAMING_CAP,		"FRAMING_CAP" },
	{ L2TP_AVP_BEARER_CAP,		"BEARER_CAP" },
	{ L2TP_AVP_TIE_BREAKER,		"TIE_BREAKER" },
	{ L2TP_AVP_FIRM_VER,		"FIRM_VER" },
	{ L2TP_AVP_HOST_NAME,		"HOST_NAME" },
	{ L2TP_AVP_VENDOR_NAME,		"VENDOR_NAME" },
	{ L2TP_AVP_ASSND_TUN_ID,	"ASSND_TUN_ID" },
	{ L2TP_AVP_RECV_WIN_SIZE,	"RECV_WIN_SIZE" },
	{ L2TP_AVP_CHALLENGE,		"CHALLENGE" },
	{ L2TP_AVP_Q931_CC,		"Q931_CC", },
	{ L2TP_AVP_CHALLENGE_RESP,	"CHALLENGE_RESP" },
	{ L2TP_AVP_ASSND_SESS_ID,	"ASSND_SESS_ID" },
	{ L2TP_AVP_CALL_SER_NUM,	"CALL_SER_NUM" },
	{ L2TP_AVP_MINIMUM_BPS,		"MINIMUM_BPS" },
	{ L2TP_AVP_MAXIMUM_BPS,		"MAXIMUM_BPS" },
	{ L2TP_AVP_BEARER_TYPE,		"BEARER_TYPE" },
	{ L2TP_AVP_FRAMING_TYPE,	"FRAMING_TYPE" },
	{ L2TP_AVP_PACKET_PROC_DELAY,	"PACKET_PROC_DELAY" },
	{ L2TP_AVP_CALLED_NUMBER,	"CALLED_NUMBER" },
	{ L2TP_AVP_CALLING_NUMBER,	"CALLING_NUMBER" },
	{ L2TP_AVP_SUB_ADDRESS,		"SUB_ADDRESS" },
	{ L2TP_AVP_TX_CONN_SPEED,	"TX_CONN_SPEED" },
	{ L2TP_AVP_PHY_CHANNEL_ID,	"PHY_CHANNEL_ID" },
	{ L2TP_AVP_INI_RECV_LCP,	"INI_RECV_LCP" },
	{ L2TP_AVP_LAST_SENT_LCP,	"LAST_SENT_LCP" },
	{ L2TP_AVP_LAST_RECV_LCP,	"LAST_RECV_LCP" },
	{ L2TP_AVP_PROXY_AUTH_TYPE,	"PROXY_AUTH_TYPE" },
	{ L2TP_AVP_PROXY_AUTH_NAME,	"PROXY_AUTH_NAME" },
	{ L2TP_AVP_PROXY_AUTH_CHAL,	"PROXY_AUTH_CHAL" },
	{ L2TP_AVP_PROXY_AUTH_ID,	"PROXY_AUTH_ID" },
	{ L2TP_AVP_PROXY_AUTH_RESP,	"PROXY_AUTH_RESP" },
	{ L2TP_AVP_CALL_ERRORS,		"CALL_ERRORS" },
	{ L2TP_AVP_ACCM,		"ACCM" },
	{ L2TP_AVP_RANDOM_VECTOR,	"RANDOM_VECTOR" },
	{ L2TP_AVP_PRIVATE_GRP_ID,	"PRIVATE_GRP_ID" },
	{ L2TP_AVP_RX_CONN_SPEED,	"RX_CONN_SPEED" },
	{ L2TP_AVP_SEQ_REQUIRED,	"SEQ_REQUIRED" },
	{ L2TP_AVP_PPP_DISCON_CC,	"PPP_DISCON_CC" },
	{ 0,				NULL }
};

static const struct tok l2tp_authentype2str[] = {
	{ L2TP_AUTHEN_TYPE_RESERVED,	"Reserved" },
	{ L2TP_AUTHEN_TYPE_TEXTUAL,	"Textual" },
	{ L2TP_AUTHEN_TYPE_CHAP,	"CHAP" },
	{ L2TP_AUTHEN_TYPE_PAP,		"PAP" },
	{ L2TP_AUTHEN_TYPE_NO_AUTH,	"No Auth" },
	{ L2TP_AUTHEN_TYPE_MSCHAPv1,	"MS-CHAPv1" },
	{ 0,				NULL }
};

#define L2TP_PPP_DISCON_CC_DIRECTION_GLOBAL	0
#define L2TP_PPP_DISCON_CC_DIRECTION_AT_PEER	1
#define L2TP_PPP_DISCON_CC_DIRECTION_AT_LOCAL	2

static const struct tok l2tp_cc_direction2str[] = {
	{ L2TP_PPP_DISCON_CC_DIRECTION_GLOBAL,	"global error" },
	{ L2TP_PPP_DISCON_CC_DIRECTION_AT_PEER,	"at peer" },
	{ L2TP_PPP_DISCON_CC_DIRECTION_AT_LOCAL,"at local" },
	{ 0,					NULL }
};

#if 0
static char *l2tp_result_code_StopCCN[] = {
         "Reserved",
         "General request to clear control connection",
         "General error--Error Code indicates the problem",
         "Control channel already exists",
         "Requester is not authorized to establish a control channel",
         "The protocol version of the requester is not supported",
         "Requester is being shut down",
         "Finite State Machine error"
#define L2TP_MAX_RESULT_CODE_STOPCC_INDEX	8
};
#endif

#if 0
static char *l2tp_result_code_CDN[] = {
	"Reserved",
	"Call disconnected due to loss of carrier",
	"Call disconnected for the reason indicated in error code",
	"Call disconnected for administrative reasons",
	"Call failed due to lack of appropriate facilities being "
	"available (temporary condition)",
	"Call failed due to lack of appropriate facilities being "
	"available (permanent condition)",
	"Invalid destination",
	"Call failed due to no carrier detected",
	"Call failed due to detection of a busy signal",
	"Call failed due to lack of a dial tone",
	"Call was not established within time allotted by LAC",
	"Call was connected but no appropriate framing was detected"
#define L2TP_MAX_RESULT_CODE_CDN_INDEX	12
};
#endif

#if 0
static char *l2tp_error_code_general[] = {
	"No general error",
	"No control connection exists yet for this LAC-LNS pair",
	"Length is wrong",
	"One of the field values was out of range or "
	"reserved field was non-zero"
	"Insufficient resources to handle this operation now",
	"The Session ID is invalid in this context",
	"A generic vendor-specific error occurred in the LAC",
	"Try another"
#define L2TP_MAX_ERROR_CODE_GENERAL_INDEX	8
};
#endif

/******************************/
/* generic print out routines */
/******************************/
static void
print_string(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	for (i=0; i<length; i++) {
		fn_print_char(ndo, GET_U_1(dat));
		dat++;
	}
}

static void
print_octets(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int i;
	for (i=0; i<length; i++) {
		ND_PRINT("%02x", GET_U_1(dat));
		dat++;
	}
}

static void
print_16bits_val(netdissect_options *ndo, const uint8_t *dat)
{
	ND_PRINT("%u", GET_BE_U_2(dat));
}

static void
print_32bits_val(netdissect_options *ndo, const uint8_t *dat)
{
	ND_PRINT("%u", GET_BE_U_4(dat));
}

/***********************************/
/* AVP-specific print out routines */
/***********************************/
static void
l2tp_msgtype_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	ND_PRINT("%s", tok2str(l2tp_msgtype2str, "MSGTYPE-#%u",
	    GET_BE_U_2(dat)));
}

static void
l2tp_result_code_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	/* Result Code */
	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	ND_PRINT("%u", GET_BE_U_2(dat));
	dat += 2;
	length -= 2;

	/* Error Code (opt) */
	if (length == 0)
		return;
	if (length < 2) {
		ND_PRINT(" AVP too short");
		return;
	}
	ND_PRINT("/%u", GET_BE_U_2(dat));
	dat += 2;
	length -= 2;

	/* Error Message (opt) */
	if (length == 0)
		return;
	ND_PRINT(" ");
	print_string(ndo, dat, length);
}

static void
l2tp_proto_ver_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	ND_PRINT("%u.%u", (GET_BE_U_2(dat) >> 8),
		  (GET_BE_U_2(dat) & 0xff));
}

static void
l2tp_framing_cap_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	if (GET_BE_U_4(dat) &  L2TP_FRAMING_CAP_ASYNC_MASK) {
		ND_PRINT("A");
	}
	if (GET_BE_U_4(dat) &  L2TP_FRAMING_CAP_SYNC_MASK) {
		ND_PRINT("S");
	}
}

static void
l2tp_bearer_cap_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	if (GET_BE_U_4(dat) &  L2TP_BEARER_CAP_ANALOG_MASK) {
		ND_PRINT("A");
	}
	if (GET_BE_U_4(dat) &  L2TP_BEARER_CAP_DIGITAL_MASK) {
		ND_PRINT("D");
	}
}

static void
l2tp_q931_cc_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 3) {
		ND_PRINT("AVP too short");
		return;
	}
	print_16bits_val(ndo, dat);
	ND_PRINT(", %02x", GET_U_1(dat + 2));
	dat += 3;
	length -= 3;
	if (length != 0) {
		ND_PRINT(" ");
		print_string(ndo, dat, length);
	}
}

static void
l2tp_bearer_type_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	if (GET_BE_U_4(dat) &  L2TP_BEARER_TYPE_ANALOG_MASK) {
		ND_PRINT("A");
	}
	if (GET_BE_U_4(dat) &  L2TP_BEARER_TYPE_DIGITAL_MASK) {
		ND_PRINT("D");
	}
}

static void
l2tp_framing_type_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	if (GET_BE_U_4(dat) &  L2TP_FRAMING_TYPE_ASYNC_MASK) {
		ND_PRINT("A");
	}
	if (GET_BE_U_4(dat) &  L2TP_FRAMING_TYPE_SYNC_MASK) {
		ND_PRINT("S");
	}
}

static void
l2tp_packet_proc_delay_print(netdissect_options *ndo)
{
	ND_PRINT("obsolete");
}

static void
l2tp_proxy_auth_type_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	ND_PRINT("%s", tok2str(l2tp_authentype2str,
			     "AuthType-#%u", GET_BE_U_2(dat)));
}

static void
l2tp_proxy_auth_id_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	ND_PRINT("%u", GET_BE_U_2(dat) & L2TP_PROXY_AUTH_ID_MASK);
}

static void
l2tp_call_errors_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	uint32_t val;

	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	dat += 2;	/* skip "Reserved" */
	length -= 2;

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("CRCErr=%u ", val);

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("FrameErr=%u ", val);

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("HardOver=%u ", val);

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("BufOver=%u ", val);

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("Timeout=%u ", val);

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("AlignErr=%u ", val);
}

static void
l2tp_accm_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	uint32_t val;

	if (length < 2) {
		ND_PRINT("AVP too short");
		return;
	}
	dat += 2;	/* skip "Reserved" */
	length -= 2;

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("send=%08x ", val);

	if (length < 4) {
		ND_PRINT("AVP too short");
		return;
	}
	val = GET_BE_U_4(dat); dat += 4; length -= 4;
	ND_PRINT("recv=%08x ", val);
}

static void
l2tp_ppp_discon_cc_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	if (length < 5) {
		ND_PRINT("AVP too short");
		return;
	}
	/* Disconnect Code */
	ND_PRINT("%04x, ", GET_BE_U_2(dat));
	dat += 2;
	length -= 2;
	/* Control Protocol Number */
	ND_PRINT("%04x ",  GET_BE_U_2(dat));
	dat += 2;
	length -= 2;
	/* Direction */
	ND_PRINT("%s", tok2str(l2tp_cc_direction2str,
			     "Direction-#%u", GET_U_1(dat)));
	dat++;
	length--;

	if (length != 0) {
		ND_PRINT(" ");
		print_string(ndo, (const u_char *)dat, length);
	}
}

static u_int
l2tp_avp_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	u_int len;
	uint16_t attr_type;
	int hidden = FALSE;

	ND_PRINT(" ");
	/* Flags & Length */
	len = GET_BE_U_2(dat) & L2TP_AVP_HDR_LEN_MASK;

	/* If it is not long enough to contain the header, we'll give up. */
	if (len < 6)
		goto trunc;

	/* If it goes past the end of the remaining length of the packet,
	   we'll give up. */
	if (len > (u_int)length)
		goto trunc;

	/* If it goes past the end of the remaining length of the captured
	   data, we'll give up. */
	ND_TCHECK_LEN(dat, len);

	/*
	 * After this point, we don't need to check whether we go past
	 * the length of the captured data; however, we *do* need to
	 * check whether we go past the end of the AVP.
	 */

	if (GET_BE_U_2(dat) & L2TP_AVP_HDR_FLAG_MANDATORY) {
		ND_PRINT("*");
	}
	if (GET_BE_U_2(dat) & L2TP_AVP_HDR_FLAG_HIDDEN) {
		hidden = TRUE;
		ND_PRINT("?");
	}
	dat += 2;

	if (GET_BE_U_2(dat)) {
		/* Vendor Specific Attribute */
	        ND_PRINT("VENDOR%04x:", GET_BE_U_2(dat)); dat += 2;
		ND_PRINT("ATTR%04x", GET_BE_U_2(dat)); dat += 2;
		ND_PRINT("(");
		print_octets(ndo, dat, len-6);
		ND_PRINT(")");
	} else {
		/* IETF-defined Attributes */
		dat += 2;
		attr_type = GET_BE_U_2(dat); dat += 2;
		ND_PRINT("%s", tok2str(l2tp_avp2str, "AVP-#%u", attr_type));
		ND_PRINT("(");
		if (hidden) {
			ND_PRINT("???");
		} else {
			switch (attr_type) {
			case L2TP_AVP_MSGTYPE:
				l2tp_msgtype_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_RESULT_CODE:
				l2tp_result_code_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_PROTO_VER:
				l2tp_proto_ver_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_FRAMING_CAP:
				l2tp_framing_cap_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_BEARER_CAP:
				l2tp_bearer_cap_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_TIE_BREAKER:
				if (len-6 < 8) {
					ND_PRINT("AVP too short");
					break;
				}
				print_octets(ndo, dat, 8);
				break;
			case L2TP_AVP_FIRM_VER:
			case L2TP_AVP_ASSND_TUN_ID:
			case L2TP_AVP_RECV_WIN_SIZE:
			case L2TP_AVP_ASSND_SESS_ID:
				if (len-6 < 2) {
					ND_PRINT("AVP too short");
					break;
				}
				print_16bits_val(ndo, dat);
				break;
			case L2TP_AVP_HOST_NAME:
			case L2TP_AVP_VENDOR_NAME:
			case L2TP_AVP_CALLING_NUMBER:
			case L2TP_AVP_CALLED_NUMBER:
			case L2TP_AVP_SUB_ADDRESS:
			case L2TP_AVP_PROXY_AUTH_NAME:
			case L2TP_AVP_PRIVATE_GRP_ID:
				print_string(ndo, dat, len-6);
				break;
			case L2TP_AVP_CHALLENGE:
			case L2TP_AVP_INI_RECV_LCP:
			case L2TP_AVP_LAST_SENT_LCP:
			case L2TP_AVP_LAST_RECV_LCP:
			case L2TP_AVP_PROXY_AUTH_CHAL:
			case L2TP_AVP_PROXY_AUTH_RESP:
			case L2TP_AVP_RANDOM_VECTOR:
				print_octets(ndo, dat, len-6);
				break;
			case L2TP_AVP_Q931_CC:
				l2tp_q931_cc_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_CHALLENGE_RESP:
				if (len-6 < 16) {
					ND_PRINT("AVP too short");
					break;
				}
				print_octets(ndo, dat, 16);
				break;
			case L2TP_AVP_CALL_SER_NUM:
			case L2TP_AVP_MINIMUM_BPS:
			case L2TP_AVP_MAXIMUM_BPS:
			case L2TP_AVP_TX_CONN_SPEED:
			case L2TP_AVP_PHY_CHANNEL_ID:
			case L2TP_AVP_RX_CONN_SPEED:
				if (len-6 < 4) {
					ND_PRINT("AVP too short");
					break;
				}
				print_32bits_val(ndo, dat);
				break;
			case L2TP_AVP_BEARER_TYPE:
				l2tp_bearer_type_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_FRAMING_TYPE:
				l2tp_framing_type_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_PACKET_PROC_DELAY:
				l2tp_packet_proc_delay_print(ndo);
				break;
			case L2TP_AVP_PROXY_AUTH_TYPE:
				l2tp_proxy_auth_type_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_PROXY_AUTH_ID:
				l2tp_proxy_auth_id_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_CALL_ERRORS:
				l2tp_call_errors_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_ACCM:
				l2tp_accm_print(ndo, dat, len-6);
				break;
			case L2TP_AVP_SEQ_REQUIRED:
				break;	/* No Attribute Value */
			case L2TP_AVP_PPP_DISCON_CC:
				l2tp_ppp_discon_cc_print(ndo, dat, len-6);
				break;
			default:
				break;
			}
		}
		ND_PRINT(")");
	}

	return (len);

 trunc:
	nd_print_trunc(ndo);
	return (0);
}


void
l2tp_print(netdissect_options *ndo, const u_char *dat, u_int length)
{
	const u_char *ptr = dat;
	u_int cnt = 0;			/* total octets consumed */
	uint16_t pad;
	int flag_t, flag_l, flag_s, flag_o;
	uint16_t l2tp_len;

	ndo->ndo_protocol = "l2tp";
	flag_t = flag_l = flag_s = flag_o = FALSE;

	if ((GET_BE_U_2(ptr) & L2TP_VERSION_MASK) == L2TP_VERSION_L2TP) {
		ND_PRINT(" l2tp:");
	} else if ((GET_BE_U_2(ptr) & L2TP_VERSION_MASK) == L2TP_VERSION_L2F) {
		ND_PRINT(" l2f:");
		return;		/* nothing to do */
	} else {
		ND_PRINT(" Unknown Version, neither L2F(1) nor L2TP(2)");
		return;		/* nothing we can do */
	}

	ND_PRINT("[");
	if (GET_BE_U_2(ptr) & L2TP_FLAG_TYPE) {
		flag_t = TRUE;
		ND_PRINT("T");
	}
	if (GET_BE_U_2(ptr) & L2TP_FLAG_LENGTH) {
		flag_l = TRUE;
		ND_PRINT("L");
	}
	if (GET_BE_U_2(ptr) & L2TP_FLAG_SEQUENCE) {
		flag_s = TRUE;
		ND_PRINT("S");
	}
	if (GET_BE_U_2(ptr) & L2TP_FLAG_OFFSET) {
		flag_o = TRUE;
		ND_PRINT("O");
	}
	if (GET_BE_U_2(ptr) & L2TP_FLAG_PRIORITY)
		ND_PRINT("P");
	ND_PRINT("]");

	ptr += 2;
	cnt += 2;

	if (flag_l) {
		l2tp_len = GET_BE_U_2(ptr);
		ptr += 2;
		cnt += 2;
	} else {
		l2tp_len = 0;
	}
	/* Tunnel ID */
	ND_PRINT("(%u/", GET_BE_U_2(ptr));
	ptr += 2;
	cnt += 2;
	/* Session ID */
	ND_PRINT("%u)",  GET_BE_U_2(ptr));
	ptr += 2;
	cnt += 2;

	if (flag_s) {
		ND_PRINT("Ns=%u,", GET_BE_U_2(ptr));
		ptr += 2;
		cnt += 2;
		ND_PRINT("Nr=%u",  GET_BE_U_2(ptr));
		ptr += 2;
		cnt += 2;
	}

	if (flag_o) {	/* Offset Size */
		pad =  GET_BE_U_2(ptr);
		/* Offset padding octets in packet buffer? */
		ND_TCHECK_LEN(ptr + 2, pad);
		ptr += (2 + pad);
		cnt += (2 + pad);
	}

	if (flag_l) {
		if (length < l2tp_len) {
			ND_PRINT(" Length %u larger than packet", l2tp_len);
			return;
		}
		length = l2tp_len;
	}
	if (length < cnt) {
		ND_PRINT(" Length %u smaller than header length", length);
		return;
	}
	if (flag_t) {
		if (!flag_l) {
			ND_PRINT(" No length");
			return;
		}
		if (length - cnt == 0) {
			ND_PRINT(" ZLB");
		} else {
			/*
			 * Print AVPs.
			 */
			while (length - cnt != 0) {
				u_int avp_length;

				avp_length = l2tp_avp_print(ndo, ptr, length - cnt);
				if (avp_length == 0) {
					/*
					 * Truncated.
					 */
					break;
				}
				cnt += avp_length;
				ptr += avp_length;
			}
		}
	} else {
		ND_PRINT(" {");
		ppp_print(ndo, ptr, length - cnt);
		ND_PRINT("}");
	}
	return;
trunc:
	nd_print_trunc(ndo);
}
