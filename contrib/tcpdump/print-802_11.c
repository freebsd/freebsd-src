/*
 * Copyright (c) 2001
 *	Fortress Technologies, Inc.  All rights reserved.
 *      Charlie Lenahan (clenahan@fortresstech.com)
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

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-802_11.c,v 1.22.2.6 2003/12/10 09:52:33 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <pcap.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"

#include "extract.h"

#include "ieee802_11.h"

#define PRINT_RATES(p) \
do { \
	int z; \
	const char *sep = " ["; \
	for (z = 0; z < p.rates.length ; z++) { \
		printf("%s%2.1f", sep, (.5 * (p.rates.rate[z] & 0x7f))); \
		if (p.rates.rate[z] & 0x80) printf("*"); \
		sep = " "; \
	} \
	if (p.rates.length != 0) \
		printf(" Mbit]"); \
} while (0)

static const char *auth_alg_text[]={"Open System","Shared Key","EAP"};
static const char *subtype_text[]={
	"Assoc Request",
	"Assoc Response",
	"ReAssoc Request",
	"ReAssoc Response",
	"Probe Request",
	"Probe Response",
	"",
	"",
	"Beacon",
	"ATIM",
	"Disassociation",
	"Authentication",
	"DeAuthentication",
	"",
	""
};

static const char *status_text[] = {
	"Succesful",  /*  0  */
	"Unspecified failure",  /*  1  */
	"Reserved",	  /*  2  */
	"Reserved",	  /*  3  */
	"Reserved",	  /*  4  */
	"Reserved",	  /*  5  */
	"Reserved",	  /*  6  */
	"Reserved",	  /*  7  */
	"Reserved",	  /*  8  */
	"Reserved",	  /*  9  */
	"Cannot Support all requested capabilities in the Capability Information field",	  /*  10  */
	"Reassociation denied due to inability to confirm that association exists",	  /*  11  */
	"Association denied due to reason outside the scope of the standard",	  /*  12  */
	"Responding station does not support the specified authentication algorithm ",	  /*  13  */
	"Received an Authentication frame with authentication transaction " \
		"sequence number out of expected sequence",	  /*  14  */
	"Authentication rejected because of challenge failure",	  /*  15 */
	"Authentication rejected due to timeout waiting for next frame in sequence",	  /*  16 */
	"Association denied because AP is unable to handle additional associated stations",	  /*  17 */
	"Association denied due to requesting station not supporting all of the " \
		"data rates in BSSBasicRateSet parameter",	  /*  18 */
	NULL
};

static const char *reason_text[] = {
	"Reserved", /* 0 */
	"Unspecified reason", /* 1 */
	"Previous authentication no longer valid",  /* 2 */
	"Deauthenticated because sending station is leaving (or has left) IBSS or ESS", /* 3 */
	"Disassociated due to inactivity", /* 4 */
	"Disassociated because AP is unable to handle all currently associated stations", /* 5 */
	"Class 2 frame receivedfrom nonauthenticated station", /* 6 */
	"Class 3 frame received from nonassociated station", /* 7 */
	"Disassociated because sending station is leaving (or has left) BSS", /* 8 */
	"Station requesting (re)association is not authenticated with responding station", /* 9 */
	NULL
};

static int
wep_print(const u_char *p)
{
	u_int32_t iv;

	if (!TTEST2(*p, IEEE802_11_IV_LEN + IEEE802_11_KID_LEN))
		return 0;
	iv = EXTRACT_LE_32BITS(p);

	printf("Data IV:%3x Pad %x KeyID %x", IV_IV(iv), IV_PAD(iv),
	    IV_KEYID(iv));

	return 1;
}

static int
parse_elements(struct mgmt_body_t *pbody, const u_char *p, int offset)
{
	for (;;) {
		if (!TTEST2(*(p + offset), 1))
			return 1;
		switch (*(p + offset)) {
		case E_SSID:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			memcpy(&pbody->ssid, p + offset, 2);
			offset += 2;
			if (pbody->ssid.length <= 0)
				break;
			if (!TTEST2(*(p + offset), pbody->ssid.length))
				return 0;
			memcpy(&pbody->ssid.ssid, p + offset,
			    pbody->ssid.length);
			offset += pbody->ssid.length;
			pbody->ssid.ssid[pbody->ssid.length] = '\0';
			break;
		case E_CHALLENGE:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			memcpy(&pbody->challenge, p + offset, 2);
			offset += 2;
			if (pbody->challenge.length <= 0)
				break;
			if (!TTEST2(*(p + offset), pbody->challenge.length))
				return 0;
			memcpy(&pbody->challenge.text, p + offset,
			    pbody->challenge.length);
			offset += pbody->challenge.length;
			pbody->challenge.text[pbody->challenge.length] = '\0';
			break;
		case E_RATES:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			memcpy(&(pbody->rates), p + offset, 2);
			offset += 2;
			if (pbody->rates.length <= 0)
				break;
			if (!TTEST2(*(p + offset), pbody->rates.length))
				return 0;
			memcpy(&pbody->rates.rate, p + offset,
			    pbody->rates.length);
			offset += pbody->rates.length;
			break;
		case E_DS:
			if (!TTEST2(*(p + offset), 3))
				return 0;
			memcpy(&pbody->ds, p + offset, 3);
			offset += 3;
			break;
		case E_CF:
			if (!TTEST2(*(p + offset), 8))
				return 0;
			memcpy(&pbody->cf, p + offset, 8);
			offset += 8;
			break;
		case E_TIM:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			memcpy(&pbody->tim, p + offset, 2);
			offset += 2;
			if (!TTEST2(*(p + offset), 3))
				return 0;
			memcpy(&pbody->tim.count, p + offset, 3);
			offset += 3;

			if (pbody->tim.length <= 3)
				break;
			if (!TTEST2(*(p + offset), pbody->tim.length - 3))
				return 0;
			memcpy(pbody->tim.bitmap, p + (pbody->tim.length - 3),
			    (pbody->tim.length - 3));
			offset += pbody->tim.length - 3;
			break;
		default:
#if 0
			printf("(1) unhandled element_id (%d)  ",
			    *(p + offset) );
#endif
			offset += *(p + offset + 1) + 2;
			break;
		}
	}
	return 1;
}

/*********************************************************************************
 * Print Handle functions for the management frame types
 *********************************************************************************/

static int
handle_beacon(const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN))
		return 0;
	memcpy(&pbody.timestamp, p, 8);
	offset += IEEE802_11_TSTAMP_LEN;
	pbody.beacon_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_BCNINT_LEN;
	pbody.capability_info = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_CAPINFO_LEN;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf(" (");
	fn_print(pbody.ssid.ssid, NULL);
	printf(")");
	PRINT_RATES(pbody);
	printf(" %s CH: %u%s",
	    CAPABILITY_ESS(pbody.capability_info) ? "ESS" : "IBSS",
	    pbody.ds.channel,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? ", PRIVACY" : "" );

	return 1;
}

static int
handle_assoc_request(const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN))
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_CAPINFO_LEN;
	pbody.listen_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_LISTENINT_LEN;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf(" (");
	fn_print(pbody.ssid.ssid, NULL);
	printf(")");
	PRINT_RATES(pbody);
	return 1;
}

static int
handle_assoc_response(const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_CAPINFO_LEN + IEEE802_11_STATUS_LEN +
	    IEEE802_11_AID_LEN))
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_CAPINFO_LEN;
	pbody.status_code = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_STATUS_LEN;
	pbody.aid = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_AID_LEN;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf(" AID(%x) :%s: %s", ((u_int16_t)(pbody.aid << 2 )) >> 2 ,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? " PRIVACY " : "",
	    (pbody.status_code < 19 ? status_text[pbody.status_code] : "n/a"));

	return 1;
}

static int
handle_reassoc_request(const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN +
	    IEEE802_11_AP_LEN))
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_CAPINFO_LEN;
	pbody.listen_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_LISTENINT_LEN;
	memcpy(&pbody.ap, p+offset, IEEE802_11_AP_LEN);
	offset += IEEE802_11_AP_LEN;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf(" (");
	fn_print(pbody.ssid.ssid, NULL);
	printf(") AP : %s", etheraddr_string( pbody.ap ));

	return 1;
}

static int
handle_reassoc_response(const u_char *p)
{
	/* Same as a Association Reponse */
	return handle_assoc_response(p);
}

static int
handle_probe_request(const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf(" (");
	fn_print(pbody.ssid.ssid, NULL);
	printf(")");
	PRINT_RATES(pbody);

	return 1;
}

static int
handle_probe_response(const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN))
		return 0;

	memcpy(&pbody.timestamp, p, IEEE802_11_TSTAMP_LEN);
	offset += IEEE802_11_TSTAMP_LEN;
	pbody.beacon_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_BCNINT_LEN;
	pbody.capability_info = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_CAPINFO_LEN;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf(" (");
	fn_print(pbody.ssid.ssid, NULL);
	printf(") ");
	PRINT_RATES(pbody);
	printf(" CH: %u%s", pbody.ds.channel,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? ", PRIVACY" : "" );

	return 1;
}

static int
handle_atim(void)
{
	/* the frame body for ATIM is null. */
	return 1;
}

static int
handle_disassoc(const u_char *p)
{
	struct mgmt_body_t  pbody;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_REASON_LEN))
		return 0;
	pbody.reason_code = EXTRACT_LE_16BITS(p);

	printf(": %s",
	    (pbody.reason_code < 10) ? reason_text[pbody.reason_code]
	                             : "Reserved" );

	return 1;
}

static int
handle_auth(const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 6))
		return 0;
	pbody.auth_alg = EXTRACT_LE_16BITS(p);
	offset += 2;
	pbody.auth_trans_seq_num = EXTRACT_LE_16BITS(p + offset);
	offset += 2;
	pbody.status_code = EXTRACT_LE_16BITS(p + offset);
	offset += 2;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	if ((pbody.auth_alg == 1) &&
	    ((pbody.auth_trans_seq_num == 2) ||
	     (pbody.auth_trans_seq_num == 3))) {
		printf(" (%s)-%x [Challenge Text] %s",
		    (pbody.auth_alg < 4) ? auth_alg_text[pbody.auth_alg]
		                         : "Reserved",
		    pbody.auth_trans_seq_num,
		    ((pbody.auth_trans_seq_num % 2)
		        ? ((pbody.status_code < 19)
			       ? status_text[pbody.status_code]
			       : "n/a") : ""));
		return 1;
	}
	printf(" (%s)-%x: %s",
	    (pbody.auth_alg < 4) ? auth_alg_text[pbody.auth_alg] : "Reserved",
	    pbody.auth_trans_seq_num,
	    (pbody.auth_trans_seq_num % 2)
	        ? ((pbody.status_code < 19) ? status_text[pbody.status_code]
	                                    : "n/a")
	        : "");

	return 1;
}

static int
handle_deauth(const struct mgmt_header_t *pmh, const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	const char *reason = NULL;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_REASON_LEN))
		return 0;
	pbody.reason_code = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_REASON_LEN;

	reason = (pbody.reason_code < 10) ? reason_text[pbody.reason_code]
	                                  : "Reserved";

	if (eflag) {
		printf(": %s", reason);
	} else {
		printf(" (%s): %s", etheraddr_string(pmh->sa), reason);
	}
	return 1;
}


/*********************************************************************************
 * Print Body funcs
 *********************************************************************************/


static int
mgmt_body_print(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	printf("%s", subtype_text[FC_SUBTYPE(fc)]);

	switch (FC_SUBTYPE(fc)) {
	case ST_ASSOC_REQUEST:
		return handle_assoc_request(p);
	case ST_ASSOC_RESPONSE:
		return handle_assoc_response(p);
	case ST_REASSOC_REQUEST:
		return handle_reassoc_request(p);
	case ST_REASSOC_RESPONSE:
		return handle_reassoc_response(p);
	case ST_PROBE_REQUEST:
		return handle_probe_request(p);
	case ST_PROBE_RESPONSE:
		return handle_probe_response(p);
	case ST_BEACON:
		return handle_beacon(p);
	case ST_ATIM:
		return handle_atim();
	case ST_DISASSOC:
		return handle_disassoc(p);
	case ST_AUTH:
		if (!TTEST2(*p, 3))
			return 0;
		if ((p[0] == 0 ) && (p[1] == 0) && (p[2] == 0)) {
			printf("Authentication (Shared-Key)-3 ");
			return wep_print(p);
		}
		return handle_auth(p);
	case ST_DEAUTH:
		return handle_deauth(pmh, p);
		break;
	default:
		printf("Unhandled Management subtype(%x)",
		    FC_SUBTYPE(fc));
		return 1;
	}
}


/*********************************************************************************
 * Handles printing all the control frame types
 *********************************************************************************/

static int
ctrl_body_print(u_int16_t fc, const u_char *p)
{
	switch (FC_SUBTYPE(fc)) {
	case CTRL_PS_POLL:
		printf("Power Save-Poll");
		if (!TTEST2(*p, CTRL_PS_POLL_HDRLEN))
			return 0;
		printf(" AID(%x)",
		    EXTRACT_LE_16BITS(&(((const struct ctrl_ps_poll_t *)p)->aid)));
		break;
	case CTRL_RTS:
		printf("Request-To-Send");
		if (!TTEST2(*p, CTRL_RTS_HDRLEN))
			return 0;
		if (!eflag)
			printf(" TA:%s ",
			    etheraddr_string(((const struct ctrl_rts_t *)p)->ta));
		break;
	case CTRL_CTS:
		printf("Clear-To-Send");
		if (!TTEST2(*p, CTRL_CTS_HDRLEN))
			return 0;
		if (!eflag)
			printf(" RA:%s ",
			    etheraddr_string(((const struct ctrl_cts_t *)p)->ra));
		break;
	case CTRL_ACK:
		printf("Acknowledgment");
		if (!TTEST2(*p, CTRL_ACK_HDRLEN))
			return 0;
		if (!eflag)
			printf(" RA:%s ",
			    etheraddr_string(((const struct ctrl_ack_t *)p)->ra));
		break;
	case CTRL_CF_END:
		printf("CF-End");
		if (!TTEST2(*p, CTRL_END_HDRLEN))
			return 0;
		if (!eflag)
			printf(" RA:%s ",
			    etheraddr_string(((const struct ctrl_end_t *)p)->ra));
		break;
	case CTRL_END_ACK:
		printf("CF-End+CF-Ack");
		if (!TTEST2(*p, CTRL_END_ACK_HDRLEN))
			return 0;
		if (!eflag)
			printf(" RA:%s ",
			    etheraddr_string(((const struct ctrl_end_ack_t *)p)->ra));
		break;
	default:
		printf("Unknown Ctrl Subtype");
	}
	return 1;
}

/*
 * Print Header funcs
 */

/*
 *  Data Frame - Address field contents
 *
 *  To Ds  | From DS | Addr 1 | Addr 2 | Addr 3 | Addr 4
 *    0    |  0      |  DA    | SA     | BSSID  | n/a
 *    0    |  1      |  DA    | BSSID  | SA     | n/a
 *    1    |  0      |  BSSID | SA     | DA     | n/a
 *    1    |  1      |  RA    | TA     | DA     | SA
 */

static void
data_header_print(u_int16_t fc, const u_char *p, const u_int8_t **srcp,
    const u_int8_t **dstp)
{
	switch (FC_SUBTYPE(fc)) {
	case DATA_DATA:
	case DATA_NODATA:
		break;
	case DATA_DATA_CF_ACK:
	case DATA_NODATA_CF_ACK:
		printf("CF Ack ");
		break;
	case DATA_DATA_CF_POLL:
	case DATA_NODATA_CF_POLL:
		printf("CF Poll ");
		break;
	case DATA_DATA_CF_ACK_POLL:
	case DATA_NODATA_CF_ACK_POLL:
		printf("CF Ack/Poll ");
		break;
	}

#define ADDR1  (p + 4)
#define ADDR2  (p + 10)
#define ADDR3  (p + 16)
#define ADDR4  (p + 24)

	if (!FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
		if (srcp != NULL)
			*srcp = ADDR2;
		if (dstp != NULL)
			*dstp = ADDR1;
		if (!eflag)
			return;
		printf("DA:%s SA:%s BSSID:%s ",
		    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
		    etheraddr_string(ADDR3));
	} else if (!FC_TO_DS(fc) && FC_FROM_DS(fc)) {
		if (srcp != NULL)
			*srcp = ADDR3;
		if (dstp != NULL)
			*dstp = ADDR1;
		if (!eflag)
			return;
		printf("DA:%s BSSID:%s SA:%s ",
		    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
		    etheraddr_string(ADDR3));
	} else if (FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
		if (srcp != NULL)
			*srcp = ADDR2;
		if (dstp != NULL)
			*dstp = ADDR3;
		if (!eflag)
			return;
		printf("BSSID:%s SA:%s DA:%s ",
		    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
		    etheraddr_string(ADDR3));
	} else if (FC_TO_DS(fc) && FC_FROM_DS(fc)) {
		if (srcp != NULL)
			*srcp = ADDR4;
		if (dstp != NULL)
			*dstp = ADDR3;
		if (!eflag)
			return;
		printf("RA:%s TA:%s DA:%s SA:%s ",
		    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
		    etheraddr_string(ADDR3), etheraddr_string(ADDR4));
	}

#undef ADDR1
#undef ADDR2
#undef ADDR3
#undef ADDR4
}

static void
mgmt_header_print(const u_char *p, const u_int8_t **srcp,
    const u_int8_t **dstp)
{
	const struct mgmt_header_t *hp = (const struct mgmt_header_t *) p;

	if (srcp != NULL)
		*srcp = hp->sa;
	if (dstp != NULL)
		*dstp = hp->da;
	if (!eflag)
		return;

	printf("BSSID:%s DA:%s SA:%s ",
	    etheraddr_string((hp)->bssid), etheraddr_string((hp)->da),
	    etheraddr_string((hp)->sa));
}

static void
ctrl_header_print(u_int16_t fc, const u_char *p, const u_int8_t **srcp,
    const u_int8_t **dstp)
{
	if (srcp != NULL)
		*srcp = NULL;
	if (dstp != NULL)
		*dstp = NULL;
	if (!eflag)
		return;

	switch (FC_SUBTYPE(fc)) {
	case CTRL_PS_POLL:
		printf("BSSID:%s TA:%s ",
		    etheraddr_string(((const struct ctrl_ps_poll_t *)p)->bssid),
		    etheraddr_string(((const struct ctrl_ps_poll_t *)p)->ta));
		break;
	case CTRL_RTS:
		printf("RA:%s TA:%s ",
		    etheraddr_string(((const struct ctrl_rts_t *)p)->ra),
		    etheraddr_string(((const struct ctrl_rts_t *)p)->ta));
		break;
	case CTRL_CTS:
		printf("RA:%s ",
		    etheraddr_string(((const struct ctrl_cts_t *)p)->ra));
		break;
	case CTRL_ACK:
		printf("RA:%s ",
		    etheraddr_string(((const struct ctrl_ack_t *)p)->ra));
		break;
	case CTRL_CF_END:
		printf("RA:%s BSSID:%s ",
		    etheraddr_string(((const struct ctrl_end_t *)p)->ra),
		    etheraddr_string(((const struct ctrl_end_t *)p)->bssid));
		break;
	case CTRL_END_ACK:
		printf("RA:%s BSSID:%s ",
		    etheraddr_string(((const struct ctrl_end_ack_t *)p)->ra),
		    etheraddr_string(((const struct ctrl_end_ack_t *)p)->bssid));
		break;
	default:
		printf("(H) Unknown Ctrl Subtype");
		break;
	}
}

static int
extract_header_length(u_int16_t fc)
{
	switch (FC_TYPE(fc)) {
	case T_MGMT:
		return MGMT_HDRLEN;
	case T_CTRL:
		switch (FC_SUBTYPE(fc)) {
		case CTRL_PS_POLL:
			return CTRL_PS_POLL_HDRLEN;
		case CTRL_RTS:
			return CTRL_RTS_HDRLEN;
		case CTRL_CTS:
			return CTRL_CTS_HDRLEN;
		case CTRL_ACK:
			return CTRL_ACK_HDRLEN;
		case CTRL_CF_END:
			return CTRL_END_HDRLEN;
		case CTRL_END_ACK:
			return CTRL_END_ACK_HDRLEN;
		default:
			return 0;
		}
	case T_DATA:
		return (FC_TO_DS(fc) && FC_FROM_DS(fc)) ? 30 : 24;
	default:
		printf("unknown IEEE802.11 frame type (%d)", FC_TYPE(fc));
		return 0;
	}
}

/*
 * Print the 802.11 MAC header if eflag is set, and set "*srcp" and "*dstp"
 * to point to the source and destination MAC addresses in any case if
 * "srcp" and "dstp" aren't null.
 */
static inline void
ieee_802_11_hdr_print(u_int16_t fc, const u_char *p, const u_int8_t **srcp,
    const u_int8_t **dstp)
{
	if (vflag) {
		if (FC_MORE_DATA(fc))
			printf("More Data ");
		if (FC_MORE_FLAG(fc))
			printf("More Fragments ");
		if (FC_POWER_MGMT(fc))
			printf("Pwr Mgmt ");
		if (FC_RETRY(fc))
			printf("Retry ");
		if (FC_ORDER(fc))
			printf("Strictly Ordered ");
		if (FC_WEP(fc))
			printf("WEP Encrypted ");
		if (FC_TYPE(fc) != T_CTRL || FC_SUBTYPE(fc) != CTRL_PS_POLL)
			printf("%dus ",
			    EXTRACT_LE_16BITS(
			        &((const struct mgmt_header_t *)p)->duration));
	}

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		mgmt_header_print(p, srcp, dstp);
		break;
	case T_CTRL:
		ctrl_header_print(fc, p, srcp, dstp);
		break;
	case T_DATA:
		data_header_print(fc, p, srcp, dstp);
		break;
	default:
		printf("(header) unknown IEEE802.11 frame type (%d)",
		    FC_TYPE(fc));
		*srcp = NULL;
		*dstp = NULL;
		break;
	}
}

static u_int
ieee802_11_print(const u_char *p, u_int length, u_int caplen)
{
	u_int16_t fc;
	u_int hdrlen;
	const u_int8_t *src, *dst;
	u_short extracted_ethertype;

	if (caplen < IEEE802_11_FC_LEN) {
		printf("[|802.11]");
		return caplen;
	}

	fc = EXTRACT_LE_16BITS(p);
	hdrlen = extract_header_length(fc);

	if (caplen < hdrlen) {
		printf("[|802.11]");
		return hdrlen;
	}

	ieee_802_11_hdr_print(fc, p, &src, &dst);

	/*
	 * Go past the 802.11 header.
	 */
	length -= hdrlen;
	caplen -= hdrlen;
	p += hdrlen;

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		if (!mgmt_body_print(fc,
		    (const struct mgmt_header_t *)(p - hdrlen), p)) {
			printf("[|802.11]");
			return hdrlen;
		}
		break;
	case T_CTRL:
		if (!ctrl_body_print(fc, p - hdrlen)) {
			printf("[|802.11]");
			return hdrlen;
		}
		break;
	case T_DATA:
		/* There may be a problem w/ AP not having this bit set */
		if (FC_WEP(fc)) {
			if (!wep_print(p)) {
				printf("[|802.11]");
				return hdrlen;
			}
		} else if (llc_print(p, length, caplen, dst, src,
		    &extracted_ethertype) == 0) {
			/*
			 * Some kinds of LLC packet we cannot
			 * handle intelligently
			 */
			if (!eflag)
				ieee_802_11_hdr_print(fc, p - hdrlen, NULL,
				    NULL);
			if (extracted_ethertype)
				printf("(LLC %s) ",
				    etherproto_string(
				        htons(extracted_ethertype)));
			if (!xflag && !qflag)
				default_print(p, caplen);
		}
		break;
	default:
		printf("unknown 802.11 frame type (%d)", FC_TYPE(fc));
		break;
	}

	return hdrlen;
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the 802.11 header of the packet, 'h->ts' is the timestamp,
 * 'h->length' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
u_int
ieee802_11_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	return ieee802_11_print(p, h->len, h->caplen);
}

static u_int
ieee802_11_radio_print(const u_char *p, u_int length, u_int caplen)
{
	u_int32_t caphdr_len;

	caphdr_len = EXTRACT_32BITS(p + 4);
	if (caphdr_len < 8) {
		/*
		 * Yow!  The capture header length is claimed not
		 * to be large enough to include even the version
		 * cookie or capture header length!
		 */
		printf("[|802.11]");
		return caplen;
	}

	if (caplen < caphdr_len) {
		printf("[|802.11]");
		return caplen;
	}

	return caphdr_len + ieee802_11_print(p + caphdr_len,
	    length - caphdr_len, caplen - caphdr_len);
}

#define PRISM_HDR_LEN		144

#define WLANCAP_MAGIC_COOKIE_V1	0x80211001

/*
 * For DLT_PRISM_HEADER; like DLT_IEEE802_11, but with an extra header,
 * containing information such as radio information, which we
 * currently ignore.
 *
 * If, however, the packet begins with WLANCAP_MAGIC_COOKIE_V1, it's
 * really DLT_IEEE802_11_RADIO (currently, on Linux, there's no
 * ARPHRD_ type for DLT_IEEE802_11_RADIO, as there is a
 * ARPHRD_IEEE80211_PRISM for DLT_PRISM_HEADER, so
 * ARPHRD_IEEE80211_PRISM is used for DLT_IEEE802_11_RADIO, and
 * the first 4 bytes of the header are used to indicate which it is).
 */
u_int
prism_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;

	if (caplen < 4) {
		printf("[|802.11]");
		return caplen;
	}

	if (EXTRACT_32BITS(p) == WLANCAP_MAGIC_COOKIE_V1)
		return ieee802_11_radio_print(p, length, caplen);

	if (caplen < PRISM_HDR_LEN) {
		printf("[|802.11]");
		return caplen;
	}

	return PRISM_HDR_LEN + ieee802_11_print(p + PRISM_HDR_LEN,
	    length - PRISM_HDR_LEN, caplen - PRISM_HDR_LEN);
}

/*
 * For DLT_IEEE802_11_RADIO; like DLT_IEEE802_11, but with an extra
 * header, containing information such as radio information, which we
 * currently ignore.
 */
u_int
ieee802_11_radio_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;

	if (caplen < 8) {
		printf("[|802.11]");
		return caplen;
	}

	return ieee802_11_radio_print(p, length, caplen);
}
