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
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-802_11.c,v 1.6.4.1 2002/05/13 08:34:50 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

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
	char *sep = " ["; \
	for (z = 0; z < p.rates.length ; z++) { \
		printf("%s%2.1f", sep, (.5 * (p.rates.rate[z] & 0x7f))); \
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
	"RESERVED",
	"RESERVED",
	"Beacon",
	"ATIM",
	"Disassociation",
	"Authentication",
	"DeAuthentication",
	"RESERVED",
	"RESERVED"
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

static int wep_print(const u_char *p,u_int length)
{
	u_int32_t iv;

	if (!TTEST2(*p, 4))
		return 0;
	iv = EXTRACT_LE_32BITS(p);

	printf("Data IV:%3x Pad %x KeyID %x", IV_IV(iv), IV_PAD(iv),
	    IV_KEYID(iv));

	return 1;
}


static int parse_elements(struct mgmt_body_t *pbody,const u_char *p,int offset)
{
	for (;;) {
		if (!TTEST2(*(p + offset), 1))
			return 1;
		switch (*(p + offset)) {
		case E_SSID:
			if (!TTEST2(*(p+offset), 2))
				return 0;
			memcpy(&(pbody->ssid),p+offset,2); offset += 2;
			if (pbody->ssid.length > 0)
			{
				if (!TTEST2(*(p+offset), pbody->ssid.length))
					return 0;
				memcpy(&(pbody->ssid.ssid),p+offset,pbody->ssid.length); offset += pbody->ssid.length;
				pbody->ssid.ssid[pbody->ssid.length]='\0';
			}
			break;
		case E_CHALLENGE:
			if (!TTEST2(*(p+offset), 2))
				return 0;
			memcpy(&(pbody->challenge),p+offset,2); offset += 2;
			if (pbody->challenge.length > 0)
			{
				if (!TTEST2(*(p+offset), pbody->challenge.length))
					return 0;
				memcpy(&(pbody->challenge.text),p+offset,pbody->challenge.length); offset += pbody->challenge.length;
				pbody->challenge.text[pbody->challenge.length]='\0';
			}
			break;
		case E_RATES:
			if (!TTEST2(*(p+offset), 2))
				return 0;
			memcpy(&(pbody->rates),p+offset,2); offset += 2;
			if (pbody->rates.length > 0) {
				if (!TTEST2(*(p+offset), pbody->rates.length))
					return 0;
				memcpy(&(pbody->rates.rate),p+offset,pbody->rates.length); offset += pbody->rates.length;
			}
			break;
		case E_DS:
			if (!TTEST2(*(p+offset), 3))
				return 0;
			memcpy(&(pbody->ds),p+offset,3); offset +=3;
			break;
		case E_CF:
			if (!TTEST2(*(p+offset), 8))
				return 0;
			memcpy(&(pbody->cf),p+offset,8); offset +=8;
			break;
		case E_TIM:
			if (!TTEST2(*(p+offset), 2))
				return 0;
			memcpy(&(pbody->tim),p+offset,2); offset +=2;
			if (!TTEST2(*(p+offset), 3))
				return 0;
			memcpy(&(pbody->tim.count),p+offset,3); offset +=3;

			if ((pbody->tim.length -3) > 0)
			{
				if (!TTEST2(*(p+offset), pbody->tim.length -3))
					return 0;
				memcpy((pbody->tim.bitmap),p+(pbody->tim.length -3),(pbody->tim.length -3));
				offset += pbody->tim.length -3;
			}

			break;
		default:
#if 0
			printf("(1) unhandled element_id (%d)  ", *(p+offset) );
#endif
			offset+= *(p+offset+1) + 2;
			break;
		}
	}
	return 1;
}

/*********************************************************************************
 * Print Handle functions for the management frame types
 *********************************************************************************/

static int handle_beacon(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 12))
		return 0;
	memcpy(&pbody.timestamp, p, 8);
	offset += 8;
	pbody.beacon_interval = EXTRACT_LE_16BITS(p+offset);
	offset += 2;
	pbody.capability_info = EXTRACT_LE_16BITS(p+offset);
	offset += 2;

	if (!parse_elements(&pbody,p,offset))
		return 0;

	printf("%s (", subtype_text[FC_SUBTYPE(fc)]);
	fn_print(pbody.ssid.ssid, NULL);
	printf(")");
	PRINT_RATES(pbody);
	printf(" %s CH: %u %s",
	    CAPABILITY_ESS(pbody.capability_info) ? "ESS" : "IBSS",
	    pbody.ds.channel,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? ", PRIVACY" : "" );

	return 1;
}

static int handle_assoc_request(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 4))
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += 2;
	pbody.listen_interval = EXTRACT_LE_16BITS(p+offset);
	offset += 2;

	if (!parse_elements(&pbody,p,offset))
		return 0;

	printf("%s (", subtype_text[FC_SUBTYPE(fc)]);
	fn_print(pbody.ssid.ssid, NULL);
	printf(")");
	PRINT_RATES(pbody);
	return 1;
}

static int handle_assoc_response(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 6))
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += 2;
	pbody.status_code = EXTRACT_LE_16BITS(p+offset);
	offset += 2;
	pbody.aid = EXTRACT_LE_16BITS(p+offset);
	offset += 2;

	if (!parse_elements(&pbody,p,offset))
		return 0;

	printf("%s AID(%x) :%s: %s", subtype_text[FC_SUBTYPE(fc)],
	    ((u_int16_t)(pbody.aid << 2 )) >> 2 ,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? " PRIVACY " : "",
	    (pbody.status_code < 19 ? status_text[pbody.status_code] : "n/a"));

	return 1;
}


static int handle_reassoc_request(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 10))
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += 2;
	pbody.listen_interval = EXTRACT_LE_16BITS(p+offset);
	offset += 2;
	memcpy(&pbody.ap,p+offset,6);
	offset += 6;

	if (!parse_elements(&pbody,p,offset))
		return 0;

	printf("%s (", subtype_text[FC_SUBTYPE(fc)]);
	fn_print(pbody.ssid.ssid, NULL);
	printf(") AP : %s", etheraddr_string( pbody.ap ));

	return 1;
}

static int handle_reassoc_response(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	/* Same as a Association Reponse */
	return handle_assoc_response(fc,pmh,p);
}

static int handle_probe_request(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf("%s (", subtype_text[FC_SUBTYPE(fc)]);
	fn_print(pbody.ssid.ssid, NULL);
	printf(")");
	PRINT_RATES(pbody);

	return 1;
}

static int handle_probe_response(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 12))
		return 0;
	memcpy(&pbody.timestamp,p,8);
	offset += 8;
	pbody.beacon_interval = EXTRACT_LE_16BITS(p+offset);
	offset += 2;
	pbody.capability_info = EXTRACT_LE_16BITS(p+offset);
	offset += 2;

	if (!parse_elements(&pbody, p, offset))
		return 0;

	printf("%s (", subtype_text[FC_SUBTYPE(fc)]);
	fn_print(pbody.ssid.ssid, NULL);
	printf(") ");
	PRINT_RATES(pbody);
	printf(" CH: %u%s", pbody.ds.channel,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? ", PRIVACY" : "" );

	return 1;
}

static int handle_atim(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	/* the frame body for ATIM is null. */
	printf("ATIM");
	return 1;
}

static int handle_disassoc(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 2))
		return 0;
	pbody.reason_code = EXTRACT_LE_16BITS(p);
	offset += 2;

	printf("%s: %s", subtype_text[FC_SUBTYPE(fc)],
	    pbody.reason_code < 10 ? reason_text[pbody.reason_code] : "Reserved" );

	return 1;
}

static int handle_auth(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
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

	if (!parse_elements(&pbody,p,offset))
		return 0;

	if ((pbody.auth_alg == 1) &&
	    ((pbody.auth_trans_seq_num == 2) || (pbody.auth_trans_seq_num == 3))) {
		printf("%s (%s)-%x [Challenge Text] %s",
			subtype_text[FC_SUBTYPE(fc)],
			pbody.auth_alg < 4 ? auth_alg_text[pbody.auth_alg] : "Reserved" ,
			pbody.auth_trans_seq_num,
			 ((pbody.auth_trans_seq_num % 2) ?
				(pbody.status_code < 19 ? status_text[pbody.status_code] : "n/a") : "" ));
	} else {
		printf("%s (%s)-%x: %s",
		    subtype_text[FC_SUBTYPE(fc)],
		    pbody.auth_alg < 4 ? auth_alg_text[pbody.auth_alg] : "Reserved" ,
		    pbody.auth_trans_seq_num,
		    ((pbody.auth_trans_seq_num % 2) ? (pbody.status_code < 19 ? status_text[pbody.status_code] : "n/a")  : ""));
	}

	return 1;
}

static int handle_deauth(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p)
{
	struct mgmt_body_t  pbody;
	int offset = 0;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 2))
		return 0;
	pbody.reason_code = EXTRACT_LE_16BITS(p);
	offset += 2;

	if (eflag) {
		printf("%s: %s",
		    subtype_text[FC_SUBTYPE(fc)],
		    pbody.reason_code < 10 ? reason_text[pbody.reason_code] : "Reserved" );
	} else {
		printf("%s (%s): %s",
		    subtype_text[FC_SUBTYPE(fc)], etheraddr_string(pmh->sa),
		    pbody.reason_code < 10 ? reason_text[pbody.reason_code] : "Reserved" );
	}

	return 1;
}


/*********************************************************************************
 * Print Body funcs
 *********************************************************************************/


static int mgmt_body_print(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p, u_int length)
{
	switch (FC_SUBTYPE(fc)) {
	case ST_ASSOC_REQUEST:
		return (handle_assoc_request(fc, pmh, p));
	case ST_ASSOC_RESPONSE:
		return (handle_assoc_response(fc, pmh, p));
	case ST_REASSOC_REQUEST:
		return (handle_reassoc_request(fc, pmh, p));
	case ST_REASSOC_RESPONSE:
		return (handle_reassoc_response(fc, pmh, p));
	case ST_PROBE_REQUEST:
		return (handle_probe_request(fc, pmh, p));
	case ST_PROBE_RESPONSE:
		return (handle_probe_response(fc, pmh, p));
	case ST_BEACON:
		return (handle_beacon(fc, pmh, p));
	case ST_ATIM:
		return (handle_atim(fc, pmh, p));
	case ST_DISASSOC:
		return (handle_disassoc(fc, pmh, p));
	case ST_AUTH:
		if (!TTEST2(*p, 3))
			return 0;
		if ((p[0] == 0 ) && (p[1] == 0) && (p[2] == 0)) {
			printf("Authentication (Shared-Key)-3 ");
			return (wep_print(p, length));
		}
		else
			return (handle_auth(fc, pmh, p));
	case ST_DEAUTH:
		return (handle_deauth(fc, pmh, p));
		break;
	default:
		printf("Unhandled Managment subtype(%x)",
		    FC_SUBTYPE(fc));
		return 1;
	}
}


/*********************************************************************************
 * Handles printing all the control frame types
 *********************************************************************************/

static int ctrl_body_print(u_int16_t fc,const u_char *p, u_int length)
{
	switch (FC_SUBTYPE(fc)) {
	case CTRL_PS_POLL:
		if (!TTEST2(*p, CTRL_PS_POLL_LEN))
			return 0;
		printf("Power Save-Poll AID(%x)",
		    EXTRACT_LE_16BITS(&(((const struct ctrl_ps_poll_t *)p)->aid)));
		break;
	case CTRL_RTS:
		if (!TTEST2(*p, CTRL_RTS_LEN))
			return 0;
		if (eflag)
			printf("Request-To-Send");
		else
			printf("Request-To-Send TA:%s ",
			    etheraddr_string(((const struct ctrl_rts_t *)p)->ta));
		break;
	case CTRL_CTS:
		if (!TTEST2(*p, CTRL_CTS_LEN))
			return 0;
		if (eflag)
			printf("Clear-To-Send");
		else
			printf("Clear-To-Send RA:%s ",
			    etheraddr_string(((const struct ctrl_cts_t *)p)->ra));
		break;
	case CTRL_ACK:
		if (!TTEST2(*p, CTRL_ACK_LEN))
			return 0;
		if (eflag)
			printf("Acknowledgment");
		else
			printf("Acknowledgment RA:%s ",
			    etheraddr_string(((const struct ctrl_ack_t *)p)->ra));
		break;
	case CTRL_CF_END:
		if (!TTEST2(*p, CTRL_END_LEN))
			return 0;
		if (eflag)
			printf("CF-End");
		else
			printf("CF-End RA:%s ",
			    etheraddr_string(((const struct ctrl_end_t *)p)->ra));
		break;
	case CTRL_END_ACK:
		if (!TTEST2(*p, CTRL_END_ACK_LEN))
			return 0;
		if (eflag)
			printf("CF-End+CF-Ack");
		else
			printf("CF-End+CF-Ack RA:%s ",
			    etheraddr_string(((const struct ctrl_end_ack_t *)p)->ra));
		break;
	default:
		printf("(B) Unknown Ctrl Subtype");
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

static void data_header_print(u_int16_t fc,const u_char *p, u_int length)
{
#define ADDR1  (p + 4)
#define ADDR2  (p + 10)
#define ADDR3  (p + 16)
#define ADDR4  (p + 24)

	if (!FC_TO_DS(fc)) {
		if (!FC_FROM_DS(fc))
			printf("DA:%s SA:%s BSSID:%s ",
			    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
			    etheraddr_string(ADDR3));
		else
			printf("DA:%s BSSID:%s SA:%s ",
			    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
			    etheraddr_string(ADDR3));
	} else {
		if (!FC_FROM_DS(fc))
			printf("BSSID:%s SA:%s DA:%s ",
			    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
			    etheraddr_string(ADDR3));
		else
			printf("RA:%s TA:%s DA:%s SA:%s ",
			    etheraddr_string(ADDR1), etheraddr_string(ADDR2),
			    etheraddr_string(ADDR3), etheraddr_string(ADDR4));
	}

#undef ADDR1
#undef ADDR2
#undef ADDR3
#undef ADDR4
}


static void mgmt_header_print(const u_char *p, u_int length)
{
	const struct mgmt_header_t *hp = (const struct mgmt_header_t *) p;

	printf("BSSID:%s DA:%s SA:%s ",
	    etheraddr_string((hp)->bssid), etheraddr_string((hp)->da),
	    etheraddr_string((hp)->sa));
}

static void ctrl_header_print(u_int16_t fc,const u_char *p, u_int length)
{
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
	}
}

static int GetHeaderLength(u_int16_t fc)
{
	int iLength=0;

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		iLength = MGMT_HEADER_LEN;
		break;
	case T_CTRL:
		switch (FC_SUBTYPE(fc)) {
		case CTRL_PS_POLL:
			iLength = CTRL_PS_POLL_LEN;
			break;
		case CTRL_RTS:
			iLength = CTRL_RTS_LEN;
			break;
		case CTRL_CTS:
			iLength = CTRL_CTS_LEN;
			break;
		case CTRL_ACK:
			iLength = CTRL_ACK_LEN;
			break;
		case CTRL_CF_END:
			iLength = CTRL_END_LEN;
			break;
		case CTRL_END_ACK:
			iLength = CTRL_END_ACK_LEN;
			break;
		default:
			iLength = 0;
			break;
		}
		break;
	case T_DATA:
		if (FC_TO_DS(fc) && FC_FROM_DS(fc))
			iLength = 30;
		else
			iLength = 24;
		break;
	default:
		printf("unknown IEEE802.11 frame type (%d)",
		    FC_TYPE(fc));
		break;
	}

	return iLength;
}

/*
 * Print the 802.11 MAC header
 */
static inline void
ieee_802_11_print(u_int16_t fc, const u_char *p, u_int length)
{
	switch (FC_TYPE(fc)) {
	case T_MGMT:
		mgmt_header_print(p, length);
		break;

	case T_CTRL:
		ctrl_header_print(fc, p, length);
		break;

	case T_DATA:
		data_header_print(fc, p, length);
		break;

	default:
		printf("(header) unknown IEEE802.11 frame type (%d)",
		    FC_TYPE(fc));
		break;
	}
}

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the ether header of the packet, 'h->tv' is the timestamp,
 * 'h->length' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
ieee802_11_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	u_int16_t fc;
	u_int HEADER_LENGTH;
	u_short extracted_ethertype;

	++infodelay;
	ts_print(&h->ts);

	if (caplen < IEEE802_11_FC_LEN) {
		printf("[|802.11]");
		goto out;
	}

	fc=EXTRACT_LE_16BITS(p);

	if (eflag)
		ieee_802_11_print(fc, p, length);

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	HEADER_LENGTH=GetHeaderLength(fc);

	length -= HEADER_LENGTH;
	caplen -= HEADER_LENGTH;
	p += HEADER_LENGTH;

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		if (!mgmt_body_print(fc, (const struct mgmt_header_t *)packetp,
		    p, length)) {
			printf("[|802.11]");
			goto out;
		}
		break;

	case T_CTRL:
		if (!ctrl_body_print(fc, p - HEADER_LENGTH,
		    length + HEADER_LENGTH)) {
			printf("[|802.11]");
			goto out;
		}
		break;

	case T_DATA:
		/* There may be a problem w/ AP not having this bit set */
 		if (FC_WEP(fc)) {
			if (!wep_print(p,length)) {
				printf("[|802.11]");
				goto out;
			}
		} else {
			if (llc_print(p, length, caplen, packetp + 10,
			    packetp + 4, &extracted_ethertype) == 0) {
				/*
				 * Some kinds of LLC packet we cannot
				 * handle intelligently
				 */
				if (!eflag)
					ieee_802_11_print(fc, p - HEADER_LENGTH,
					    length + HEADER_LENGTH);
				if (extracted_ethertype) {
					printf("(LLC %s) ",
					    etherproto_string(htons(extracted_ethertype)));
				}
				if (!xflag && !qflag)
					default_print(p, caplen);
			}
		}
		break;

	default:
		printf("(body) unhandled IEEE802.11 frame type (%d)",
		    FC_TYPE(fc));
		break;
	}

	if (xflag)
		default_print(p, caplen);
 out:
	putchar('\n');
	--infodelay;
	if (infoprint)
		info(0);
}
