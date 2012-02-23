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
    "@(#) $Header: /tcpdump/master/tcpdump/print-802_11.c,v 1.49 2007-12-29 23:25:02 guy Exp $ (LBL)";
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

#include "cpack.h"

#include "ieee802_11.h"
#include "ieee802_11_radio.h"

#define PRINT_SSID(p) \
	if (p.ssid_present) { \
		printf(" ("); \
		fn_print(p.ssid.ssid, NULL); \
		printf(")"); \
	}

#define PRINT_RATE(_sep, _r, _suf) \
	printf("%s%2.1f%s", _sep, (.5 * ((_r) & 0x7f)), _suf)
#define PRINT_RATES(p) \
	if (p.rates_present) { \
		int z; \
		const char *sep = " ["; \
		for (z = 0; z < p.rates.length ; z++) { \
			PRINT_RATE(sep, p.rates.rate[z], \
				(p.rates.rate[z] & 0x80 ? "*" : "")); \
			sep = " "; \
		} \
		if (p.rates.length != 0) \
			printf(" Mbit]"); \
	}

#define PRINT_DS_CHANNEL(p) \
	if (p.ds_present) \
		printf(" CH: %u", p.ds.channel); \
	printf("%s", \
	    CAPABILITY_PRIVACY(p.capability_info) ? ", PRIVACY" : "" );

static const int ieee80211_htrates[16] = {
	13,		/* IFM_IEEE80211_MCS0 */
	26,		/* IFM_IEEE80211_MCS1 */
	39,		/* IFM_IEEE80211_MCS2 */
	52,		/* IFM_IEEE80211_MCS3 */
	78,		/* IFM_IEEE80211_MCS4 */
	104,		/* IFM_IEEE80211_MCS5 */
	117,		/* IFM_IEEE80211_MCS6 */
	130,		/* IFM_IEEE80211_MCS7 */
	26,		/* IFM_IEEE80211_MCS8 */
	52,		/* IFM_IEEE80211_MCS9 */
	78,		/* IFM_IEEE80211_MCS10 */
	104,		/* IFM_IEEE80211_MCS11 */
	156,		/* IFM_IEEE80211_MCS12 */
	208,		/* IFM_IEEE80211_MCS13 */
	234,		/* IFM_IEEE80211_MCS14 */
	260,		/* IFM_IEEE80211_MCS15 */
};
#define PRINT_HT_RATE(_sep, _r, _suf) \
	printf("%s%.1f%s", _sep, (.5 * ieee80211_htrates[(_r) & 0xf]), _suf)

static const char *auth_alg_text[]={"Open System","Shared Key","EAP"};
#define NUM_AUTH_ALGS	(sizeof auth_alg_text / sizeof auth_alg_text[0])

static const char *status_text[] = {
	"Succesful",						/*  0 */
	"Unspecified failure",					/*  1 */
	"Reserved",						/*  2 */
	"Reserved",						/*  3 */
	"Reserved",						/*  4 */
	"Reserved",						/*  5 */
	"Reserved",						/*  6 */
	"Reserved",						/*  7 */
	"Reserved",						/*  8 */
	"Reserved",						/*  9 */
	"Cannot Support all requested capabilities in the Capability "
	  "Information field",	  				/* 10 */
	"Reassociation denied due to inability to confirm that association "
	  "exists",						/* 11 */
	"Association denied due to reason outside the scope of the "
	  "standard",						/* 12 */
	"Responding station does not support the specified authentication "
	  "algorithm ",						/* 13 */
	"Received an Authentication frame with authentication transaction "
	  "sequence number out of expected sequence",		/* 14 */
	"Authentication rejected because of challenge failure",	/* 15 */
	"Authentication rejected due to timeout waiting for next frame in "
	  "sequence",	  					/* 16 */
	"Association denied because AP is unable to handle additional"
	  "associated stations",	  			/* 17 */
	"Association denied due to requesting station not supporting all of "
	  "the data rates in BSSBasicRateSet parameter",	/* 18 */
	"Association denied due to requesting station not supporting "
	  "short preamble operation",				/* 19 */
	"Association denied due to requesting station not supporting "
	  "PBCC encoding",					/* 20 */
	"Association denied due to requesting station not supporting "
	  "channel agility",					/* 21 */
	"Association request rejected because Spectrum Management "
	  "capability is required",				/* 22 */
	"Association request rejected because the information in the "
	  "Power Capability element is unacceptable",		/* 23 */
	"Association request rejected because the information in the "
	  "Supported Channels element is unacceptable",		/* 24 */
	"Association denied due to requesting station not supporting "
	  "short slot operation",				/* 25 */
	"Association denied due to requesting station not supporting "
	  "DSSS-OFDM operation",				/* 26 */
	"Association denied because the requested STA does not support HT "
	  "features",						/* 27 */
	"Reserved",						/* 28 */
	"Association denied because the requested STA does not support "
	  "the PCO transition time required by the AP",		/* 29 */
	"Reserved",						/* 30 */
	"Reserved",						/* 31 */
	"Unspecified, QoS-related failure",			/* 32 */
	"Association denied due to QAP having insufficient bandwidth "
	  "to handle another QSTA",				/* 33 */
	"Association denied due to excessive frame loss rates and/or "
	  "poor conditions on current operating channel",	/* 34 */
	"Association (with QBSS) denied due to requesting station not "
	  "supporting the QoS facility",			/* 35 */
	"Association denied due to requesting station not supporting "
	  "Block Ack",						/* 36 */
	"The request has been declined",			/* 37 */
	"The request has not been successful as one or more parameters "
	  "have invalid values",				/* 38 */
	"The TS has not been created because the request cannot be honored. "
	  "However, a suggested TSPEC is provided so that the initiating QSTA"
	  "may attempt to set another TS with the suggested changes to the "
	  "TSPEC",						/* 39 */
	"Invalid Information Element",				/* 40 */
	"Group Cipher is not valid",				/* 41 */
	"Pairwise Cipher is not valid",				/* 42 */
	"AKMP is not valid",					/* 43 */
	"Unsupported RSN IE version",				/* 44 */
	"Invalid RSN IE Capabilities",				/* 45 */
	"Cipher suite is rejected per security policy",		/* 46 */
	"The TS has not been created. However, the HC may be capable of "
	  "creating a TS, in response to a request, after the time indicated "
	  "in the TS Delay element",				/* 47 */
	"Direct Link is not allowed in the BSS by policy",	/* 48 */
	"Destination STA is not present within this QBSS.",	/* 49 */
	"The Destination STA is not a QSTA.",			/* 50 */

};
#define NUM_STATUSES	(sizeof status_text / sizeof status_text[0])

static const char *reason_text[] = {
	"Reserved",						/* 0 */
	"Unspecified reason",					/* 1 */
	"Previous authentication no longer valid",  		/* 2 */
	"Deauthenticated because sending station is leaving (or has left) "
	  "IBSS or ESS",					/* 3 */
	"Disassociated due to inactivity",			/* 4 */
	"Disassociated because AP is unable to handle all currently "
	  " associated stations",				/* 5 */
	"Class 2 frame received from nonauthenticated station", /* 6 */
	"Class 3 frame received from nonassociated station",	/* 7 */
	"Disassociated because sending station is leaving "
	  "(or has left) BSS",					/* 8 */
	"Station requesting (re)association is not authenticated with "
	  "responding station",					/* 9 */
	"Disassociated because the information in the Power Capability "
	  "element is unacceptable",				/* 10 */
	"Disassociated because the information in the SupportedChannels "
	  "element is unacceptable",				/* 11 */
	"Invalid Information Element",				/* 12 */
	"Reserved",						/* 13 */
	"Michael MIC failure",					/* 14 */
	"4-Way Handshake timeout",				/* 15 */
	"Group key update timeout",				/* 16 */
	"Information element in 4-Way Handshake different from (Re)Association"
	  "Request/Probe Response/Beacon",			/* 17 */
	"Group Cipher is not valid",				/* 18 */
	"AKMP is not valid",					/* 20 */
	"Unsupported RSN IE version",				/* 21 */
	"Invalid RSN IE Capabilities",				/* 22 */
	"IEEE 802.1X Authentication failed",			/* 23 */
	"Cipher suite is rejected per security policy",		/* 24 */
	"Reserved",						/* 25 */
	"Reserved",						/* 26 */
	"Reserved",						/* 27 */
	"Reserved",						/* 28 */
	"Reserved",						/* 29 */
	"Reserved",						/* 30 */
	"TS deleted because QoS AP lacks sufficient bandwidth for this "
	  "QoS STA due to a change in BSS service characteristics or "
	  "operational mode (e.g. an HT BSS change from 40 MHz channel "
	  "to 20 MHz channel)",					/* 31 */
	"Disassociated for unspecified, QoS-related reason",	/* 32 */
	"Disassociated because QoS AP lacks sufficient bandwidth for this "
	  "QoS STA",						/* 33 */
	"Disassociated because of excessive number of frames that need to be "
          "acknowledged, but are not acknowledged for AP transmissions "
	  "and/or poor channel conditions",			/* 34 */
	"Disassociated because STA is transmitting outside the limits "
	  "of its TXOPs",					/* 35 */
	"Requested from peer STA as the STA is leaving the BSS "
	  "(or resetting)",					/* 36 */
	"Requested from peer STA as it does not want to use the "
	  "mechanism",						/* 37 */
	"Requested from peer STA as the STA received frames using the "
	  "mechanism for which a set up is required",		/* 38 */
	"Requested from peer STA due to time out",		/* 39 */
	"Reserved",						/* 40 */
	"Reserved",						/* 41 */
	"Reserved",						/* 42 */
	"Reserved",						/* 43 */
	"Reserved",						/* 44 */
	"Peer STA does not support the requested cipher suite",	/* 45 */
	"Association denied due to requesting STA not supporting HT "
	  "features",						/* 46 */
};
#define NUM_REASONS	(sizeof reason_text / sizeof reason_text[0])

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
parse_elements(struct mgmt_body_t *pbody, const u_char *p, int offset,
    u_int length)
{
	struct ssid_t ssid;
	struct challenge_t challenge;
	struct rates_t rates;
	struct ds_t ds;
	struct cf_t cf;
	struct tim_t tim;

	/*
	 * We haven't seen any elements yet.
	 */
	pbody->challenge_present = 0;
	pbody->ssid_present = 0;
	pbody->rates_present = 0;
	pbody->ds_present = 0;
	pbody->cf_present = 0;
	pbody->tim_present = 0;

	while (length != 0) {
		if (!TTEST2(*(p + offset), 1))
			return 0;
		if (length < 1)
			return 0;
		switch (*(p + offset)) {
		case E_SSID:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			if (length < 2)
				return 0;
			memcpy(&ssid, p + offset, 2);
			offset += 2;
			length -= 2;
			if (ssid.length != 0) {
				if (ssid.length > sizeof(ssid.ssid) - 1)
					return 0;
				if (!TTEST2(*(p + offset), ssid.length))
					return 0;
				if (length < ssid.length)
					return 0;
				memcpy(&ssid.ssid, p + offset, ssid.length);
				offset += ssid.length;
				length -= ssid.length;
			}
			ssid.ssid[ssid.length] = '\0';
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen an SSID IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->ssid_present) {
				pbody->ssid = ssid;
				pbody->ssid_present = 1;
			}
			break;
		case E_CHALLENGE:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			if (length < 2)
				return 0;
			memcpy(&challenge, p + offset, 2);
			offset += 2;
			length -= 2;
			if (challenge.length != 0) {
				if (challenge.length >
				    sizeof(challenge.text) - 1)
					return 0;
				if (!TTEST2(*(p + offset), challenge.length))
					return 0;
				if (length < challenge.length)
					return 0;
				memcpy(&challenge.text, p + offset,
				    challenge.length);
				offset += challenge.length;
				length -= challenge.length;
			}
			challenge.text[challenge.length] = '\0';
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a challenge IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->challenge_present) {
				pbody->challenge = challenge;
				pbody->challenge_present = 1;
			}
			break;
		case E_RATES:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			if (length < 2)
				return 0;
			memcpy(&rates, p + offset, 2);
			offset += 2;
			length -= 2;
			if (rates.length != 0) {
				if (rates.length > sizeof rates.rate)
					return 0;
				if (!TTEST2(*(p + offset), rates.length))
					return 0;
				if (length < rates.length)
					return 0;
				memcpy(&rates.rate, p + offset, rates.length);
				offset += rates.length;
				length -= rates.length;
			}
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a rates IE,
			 * copy this one if it's not zero-length,
			 * otherwise ignore this one, so we later
			 * report the first one we saw.
			 *
			 * We ignore zero-length rates IEs as some
			 * devices seem to put a zero-length rates
			 * IE, followed by an SSID IE, followed by
			 * a non-zero-length rates IE into frames,
			 * even though IEEE Std 802.11-2007 doesn't
			 * seem to indicate that a zero-length rates
			 * IE is valid.
			 */
			if (!pbody->rates_present && rates.length != 0) {
				pbody->rates = rates;
				pbody->rates_present = 1;
			}
			break;
		case E_DS:
			if (!TTEST2(*(p + offset), 3))
				return 0;
			if (length < 3)
				return 0;
			memcpy(&ds, p + offset, 3);
			offset += 3;
			length -= 3;
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a DS IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->ds_present) {
				pbody->ds = ds;
				pbody->ds_present = 1;
			}
			break;
		case E_CF:
			if (!TTEST2(*(p + offset), 8))
				return 0;
			if (length < 8)
				return 0;
			memcpy(&cf, p + offset, 8);
			offset += 8;
			length -= 8;
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a CF IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->cf_present) {
				pbody->cf = cf;
				pbody->cf_present = 1;
			}
			break;
		case E_TIM:
			if (!TTEST2(*(p + offset), 2))
				return 0;
			if (length < 2)
				return 0;
			memcpy(&tim, p + offset, 2);
			offset += 2;
			length -= 2;
			if (!TTEST2(*(p + offset), 3))
				return 0;
			if (length < 3)
				return 0;
			memcpy(&tim.count, p + offset, 3);
			offset += 3;
			length -= 3;

			if (tim.length <= 3)
				break;
			if (tim.length - 3 > (int)sizeof tim.bitmap)
				return 0;
			if (!TTEST2(*(p + offset), tim.length - 3))
				return 0;
			if (length < (u_int)(tim.length - 3))
				return 0;
			memcpy(tim.bitmap, p + (tim.length - 3),
			    (tim.length - 3));
			offset += tim.length - 3;
			length -= tim.length - 3;
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a TIM IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->tim_present) {
				pbody->tim = tim;
				pbody->tim_present = 1;
			}
			break;
		default:
#if 0
			printf("(1) unhandled element_id (%d)  ",
			    *(p + offset));
#endif
			if (!TTEST2(*(p + offset), 2))
				return 0;
			if (length < 2)
				return 0;
			if (!TTEST2(*(p + offset + 2), *(p + offset + 1)))
				return 0;
			if (length < (u_int)(*(p + offset + 1) + 2))
				return 0;
			offset += *(p + offset + 1) + 2;
			length -= *(p + offset + 1) + 2;
			break;
		}
	}

	/* No problems found. */
	return 1;
}

/*********************************************************************************
 * Print Handle functions for the management frame types
 *********************************************************************************/

static int
handle_beacon(const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN))
		return 0;
	if (length < IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN)
		return 0;
	memcpy(&pbody.timestamp, p, IEEE802_11_TSTAMP_LEN);
	offset += IEEE802_11_TSTAMP_LEN;
	length -= IEEE802_11_TSTAMP_LEN;
	pbody.beacon_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_BCNINT_LEN;
	length -= IEEE802_11_BCNINT_LEN;
	pbody.capability_info = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;

	ret = parse_elements(&pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);
	printf(" %s",
	    CAPABILITY_ESS(pbody.capability_info) ? "ESS" : "IBSS");
	PRINT_DS_CHANNEL(pbody);

	return ret;
}

static int
handle_assoc_request(const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN))
		return 0;
	if (length < IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN)
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;
	pbody.listen_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_LISTENINT_LEN;
	length -= IEEE802_11_LISTENINT_LEN;

	ret = parse_elements(&pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);
	return ret;
}

static int
handle_assoc_response(const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_CAPINFO_LEN + IEEE802_11_STATUS_LEN +
	    IEEE802_11_AID_LEN))
		return 0;
	if (length < IEEE802_11_CAPINFO_LEN + IEEE802_11_STATUS_LEN +
	    IEEE802_11_AID_LEN)
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;
	pbody.status_code = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_STATUS_LEN;
	length -= IEEE802_11_STATUS_LEN;
	pbody.aid = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_AID_LEN;
	length -= IEEE802_11_AID_LEN;

	ret = parse_elements(&pbody, p, offset, length);

	printf(" AID(%x) :%s: %s", ((u_int16_t)(pbody.aid << 2 )) >> 2 ,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? " PRIVACY " : "",
	    (pbody.status_code < NUM_STATUSES
		? status_text[pbody.status_code]
		: "n/a"));

	return ret;
}

static int
handle_reassoc_request(const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN +
	    IEEE802_11_AP_LEN))
		return 0;
	if (length < IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN +
	    IEEE802_11_AP_LEN)
		return 0;
	pbody.capability_info = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;
	pbody.listen_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_LISTENINT_LEN;
	length -= IEEE802_11_LISTENINT_LEN;
	memcpy(&pbody.ap, p+offset, IEEE802_11_AP_LEN);
	offset += IEEE802_11_AP_LEN;
	length -= IEEE802_11_AP_LEN;

	ret = parse_elements(&pbody, p, offset, length);

	PRINT_SSID(pbody);
	printf(" AP : %s", etheraddr_string( pbody.ap ));

	return ret;
}

static int
handle_reassoc_response(const u_char *p, u_int length)
{
	/* Same as a Association Reponse */
	return handle_assoc_response(p, length);
}

static int
handle_probe_request(const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ret = parse_elements(&pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);

	return ret;
}

static int
handle_probe_response(const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN))
		return 0;
	if (length < IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN)
		return 0;
	memcpy(&pbody.timestamp, p, IEEE802_11_TSTAMP_LEN);
	offset += IEEE802_11_TSTAMP_LEN;
	length -= IEEE802_11_TSTAMP_LEN;
	pbody.beacon_interval = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_BCNINT_LEN;
	length -= IEEE802_11_BCNINT_LEN;
	pbody.capability_info = EXTRACT_LE_16BITS(p+offset);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;

	ret = parse_elements(&pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);
	PRINT_DS_CHANNEL(pbody);

	return ret;
}

static int
handle_atim(void)
{
	/* the frame body for ATIM is null. */
	return 1;
}

static int
handle_disassoc(const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_REASON_LEN))
		return 0;
	if (length < IEEE802_11_REASON_LEN)
		return 0;
	pbody.reason_code = EXTRACT_LE_16BITS(p);

	printf(": %s",
	    (pbody.reason_code < NUM_REASONS)
		? reason_text[pbody.reason_code]
		: "Reserved" );

	return 1;
}

static int
handle_auth(const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, 6))
		return 0;
	if (length < 6)
		return 0;
	pbody.auth_alg = EXTRACT_LE_16BITS(p);
	offset += 2;
	length -= 2;
	pbody.auth_trans_seq_num = EXTRACT_LE_16BITS(p + offset);
	offset += 2;
	length -= 2;
	pbody.status_code = EXTRACT_LE_16BITS(p + offset);
	offset += 2;
	length -= 2;

	ret = parse_elements(&pbody, p, offset, length);

	if ((pbody.auth_alg == 1) &&
	    ((pbody.auth_trans_seq_num == 2) ||
	     (pbody.auth_trans_seq_num == 3))) {
		printf(" (%s)-%x [Challenge Text] %s",
		    (pbody.auth_alg < NUM_AUTH_ALGS)
			? auth_alg_text[pbody.auth_alg]
			: "Reserved",
		    pbody.auth_trans_seq_num,
		    ((pbody.auth_trans_seq_num % 2)
		        ? ((pbody.status_code < NUM_STATUSES)
			       ? status_text[pbody.status_code]
			       : "n/a") : ""));
		return ret;
	}
	printf(" (%s)-%x: %s",
	    (pbody.auth_alg < NUM_AUTH_ALGS)
		? auth_alg_text[pbody.auth_alg]
		: "Reserved",
	    pbody.auth_trans_seq_num,
	    (pbody.auth_trans_seq_num % 2)
	        ? ((pbody.status_code < NUM_STATUSES)
		    ? status_text[pbody.status_code]
	            : "n/a")
	        : "");

	return ret;
}

static int
handle_deauth(const struct mgmt_header_t *pmh, const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	const char *reason = NULL;

	memset(&pbody, 0, sizeof(pbody));

	if (!TTEST2(*p, IEEE802_11_REASON_LEN))
		return 0;
	if (length < IEEE802_11_REASON_LEN)
		return 0;
	pbody.reason_code = EXTRACT_LE_16BITS(p);
	offset += IEEE802_11_REASON_LEN;
	length -= IEEE802_11_REASON_LEN;

	reason = (pbody.reason_code < NUM_REASONS)
			? reason_text[pbody.reason_code]
			: "Reserved";

	if (eflag) {
		printf(": %s", reason);
	} else {
		printf(" (%s): %s", etheraddr_string(pmh->sa), reason);
	}
	return 1;
}

#define	PRINT_HT_ACTION(v) (\
	(v) == 0 ? printf("TxChWidth") : \
	(v) == 1 ? printf("MIMOPwrSave") : \
		   printf("Act#%d", (v)) \
)
#define	PRINT_BA_ACTION(v) (\
	(v) == 0 ? printf("ADDBA Request") : \
	(v) == 1 ? printf("ADDBA Response") : \
	(v) == 2 ? printf("DELBA") : \
		   printf("Act#%d", (v)) \
)
#define	PRINT_MESHLINK_ACTION(v) (\
	(v) == 0 ? printf("Request") : \
	(v) == 1 ? printf("Report") : \
		   printf("Act#%d", (v)) \
)
#define	PRINT_MESHPEERING_ACTION(v) (\
	(v) == 0 ? printf("Open") : \
	(v) == 1 ? printf("Confirm") : \
	(v) == 2 ? printf("Close") : \
		   printf("Act#%d", (v)) \
)
#define	PRINT_MESHPATH_ACTION(v) (\
	(v) == 0 ? printf("Request") : \
	(v) == 1 ? printf("Report") : \
	(v) == 2 ? printf("Error") : \
	(v) == 3 ? printf("RootAnnouncement") : \
		   printf("Act#%d", (v)) \
)

static int
handle_action(const struct mgmt_header_t *pmh, const u_char *p, u_int length)
{
	if (!TTEST2(*p, 2))
		return 0;
	if (length < 2)
		return 0;
	if (eflag) {
		printf(": ");
	} else {
		printf(" (%s): ", etheraddr_string(pmh->sa));
	}
	switch (p[0]) {
	case 0: printf("Spectrum Management Act#%d", p[1]); break;
	case 1: printf("QoS Act#%d", p[1]); break;
	case 2: printf("DLS Act#%d", p[1]); break;
	case 3: printf("BA "); PRINT_BA_ACTION(p[1]); break;
	case 7: printf("HT "); PRINT_HT_ACTION(p[1]); break;
	case 13: printf("MeshLMetric "); PRINT_MESHLINK_ACTION(p[1]); break;
	case 15: printf("Interwork Act#%d", p[1]); break;
	case 16: printf("Resource Act#%d", p[1]); break;
	case 17: printf("Proxy Act#%d", p[1]); break;
	case 30: printf("MeshPeering "); PRINT_MESHPEERING_ACTION(p[1]); break;
	case 32: printf("MeshPath "); PRINT_MESHPATH_ACTION(p[1]); break;
	case 127: printf("Vendor Act#%d", p[1]); break;
	default:
		printf("Reserved(%d) Act#%d", p[0], p[1]);
		break;
	}
	return 1;
}


/*********************************************************************************
 * Print Body funcs
 *********************************************************************************/


static int
mgmt_body_print(u_int16_t fc, const struct mgmt_header_t *pmh,
    const u_char *p, u_int length)
{
	switch (FC_SUBTYPE(fc)) {
	case ST_ASSOC_REQUEST:
		printf("Assoc Request");
		return handle_assoc_request(p, length);
	case ST_ASSOC_RESPONSE:
		printf("Assoc Response");
		return handle_assoc_response(p, length);
	case ST_REASSOC_REQUEST:
		printf("ReAssoc Request");
		return handle_reassoc_request(p, length);
	case ST_REASSOC_RESPONSE:
		printf("ReAssoc Response");
		return handle_reassoc_response(p, length);
	case ST_PROBE_REQUEST:
		printf("Probe Request");
		return handle_probe_request(p, length);
	case ST_PROBE_RESPONSE:
		printf("Probe Response");
		return handle_probe_response(p, length);
	case ST_BEACON:
		printf("Beacon");
		return handle_beacon(p, length);
	case ST_ATIM:
		printf("ATIM");
		return handle_atim();
	case ST_DISASSOC:
		printf("Disassociation");
		return handle_disassoc(p, length);
	case ST_AUTH:
		printf("Authentication");
		if (!TTEST2(*p, 3))
			return 0;
		if ((p[0] == 0 ) && (p[1] == 0) && (p[2] == 0)) {
			printf("Authentication (Shared-Key)-3 ");
			return wep_print(p);
		}
		return handle_auth(p, length);
	case ST_DEAUTH:
		printf("DeAuthentication");
		return handle_deauth(pmh, p, length);
		break;
	case ST_ACTION:
		printf("Action");
		return handle_action(pmh, p, length);
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
	case CTRL_CONTROL_WRAPPER:
		printf("Control Wrapper");
		/* XXX - requires special handling */
		break;
	case CTRL_BAR:
		printf("BAR");
		if (!TTEST2(*p, CTRL_BAR_HDRLEN))
			return 0;
		if (!eflag)
			printf(" RA:%s TA:%s CTL(%x) SEQ(%u) ",
			    etheraddr_string(((const struct ctrl_bar_t *)p)->ra),
			    etheraddr_string(((const struct ctrl_bar_t *)p)->ta),
			    EXTRACT_LE_16BITS(&(((const struct ctrl_bar_t *)p)->ctl)),
			    EXTRACT_LE_16BITS(&(((const struct ctrl_bar_t *)p)->seq)));
		break;
	case CTRL_BA:
		printf("BA");
		if (!TTEST2(*p, CTRL_BA_HDRLEN))
			return 0;
		if (!eflag)
			printf(" RA:%s ",
			    etheraddr_string(((const struct ctrl_ba_t *)p)->ra));
		break;
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
	u_int subtype = FC_SUBTYPE(fc);

	if (DATA_FRAME_IS_CF_ACK(subtype) || DATA_FRAME_IS_CF_POLL(subtype) ||
	    DATA_FRAME_IS_QOS(subtype)) {
		printf("CF ");
		if (DATA_FRAME_IS_CF_ACK(subtype)) {
			if (DATA_FRAME_IS_CF_POLL(subtype))
				printf("Ack/Poll");
			else
				printf("Ack");
		} else {
			if (DATA_FRAME_IS_CF_POLL(subtype))
				printf("Poll");
		}
		if (DATA_FRAME_IS_QOS(subtype))
			printf("+QoS");
		printf(" ");
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
	case CTRL_BAR:
		printf(" RA:%s TA:%s CTL(%x) SEQ(%u) ",
		    etheraddr_string(((const struct ctrl_bar_t *)p)->ra),
		    etheraddr_string(((const struct ctrl_bar_t *)p)->ta),
		    EXTRACT_LE_16BITS(&(((const struct ctrl_bar_t *)p)->ctl)),
		    EXTRACT_LE_16BITS(&(((const struct ctrl_bar_t *)p)->seq)));
		break;
	case CTRL_BA:
		printf("RA:%s ",
		    etheraddr_string(((const struct ctrl_ba_t *)p)->ra));
		break;
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
	int len;

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		return MGMT_HDRLEN;
	case T_CTRL:
		switch (FC_SUBTYPE(fc)) {
		case CTRL_BAR:
			return CTRL_BAR_HDRLEN;
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
		len = (FC_TO_DS(fc) && FC_FROM_DS(fc)) ? 30 : 24;
		if (DATA_FRAME_IS_QOS(FC_SUBTYPE(fc)))
			len += 2;
		return len;
	default:
		printf("unknown IEEE802.11 frame type (%d)", FC_TYPE(fc));
		return 0;
	}
}

static int
extract_mesh_header_length(const u_char *p)
{
	return (p[0] &~ 3) ? 0 : 6*(1 + (p[0] & 3));
}

/*
 * Print the 802.11 MAC header if eflag is set, and set "*srcp" and "*dstp"
 * to point to the source and destination MAC addresses in any case if
 * "srcp" and "dstp" aren't null.
 */
static void
ieee_802_11_hdr_print(u_int16_t fc, const u_char *p, u_int hdrlen,
    u_int meshdrlen, const u_int8_t **srcp, const u_int8_t **dstp)
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
	if (meshdrlen != 0) {
		const struct meshcntl_t *mc =
		    (const struct meshcntl_t *)&p[hdrlen - meshdrlen];
		int ae = mc->flags & 3;

		printf("MeshData (AE %d TTL %u seq %u", ae, mc->ttl,
		    EXTRACT_LE_32BITS(mc->seq));
		if (ae > 0)
			printf(" A4:%s", etheraddr_string(mc->addr4));
		if (ae > 1)
			printf(" A5:%s", etheraddr_string(mc->addr5));
		if (ae > 2)
			printf(" A6:%s", etheraddr_string(mc->addr6));
		printf(") ");
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

#ifndef roundup2
#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#endif

static u_int
ieee802_11_print(const u_char *p, u_int length, u_int orig_caplen, int pad,
    u_int fcslen)
{
	u_int16_t fc;
	u_int caplen, hdrlen, meshdrlen;
	const u_int8_t *src, *dst;
	u_short extracted_ethertype;

	caplen = orig_caplen;
	/* Remove FCS, if present */
	if (length < fcslen) {
		printf("[|802.11]");
		return caplen;
	}
	length -= fcslen;
	if (caplen > length) {
		/* Amount of FCS in actual packet data, if any */
		fcslen = caplen - length;
		caplen -= fcslen;
		snapend -= fcslen;
	}

	if (caplen < IEEE802_11_FC_LEN) {
		printf("[|802.11]");
		return orig_caplen;
	}

	fc = EXTRACT_LE_16BITS(p);
	hdrlen = extract_header_length(fc);
	if (pad)
		hdrlen = roundup2(hdrlen, 4);
	if (FC_TYPE(fc) == T_DATA && DATA_FRAME_IS_QOS(FC_SUBTYPE(fc))) {
		meshdrlen = extract_mesh_header_length(p+hdrlen);
		hdrlen += meshdrlen;
	} else
		meshdrlen = 0;


	if (caplen < hdrlen) {
		printf("[|802.11]");
		return hdrlen;
	}

	ieee_802_11_hdr_print(fc, p, hdrlen, meshdrlen, &src, &dst);

	/*
	 * Go past the 802.11 header.
	 */
	length -= hdrlen;
	caplen -= hdrlen;
	p += hdrlen;

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		if (!mgmt_body_print(fc,
		    (const struct mgmt_header_t *)(p - hdrlen), p, length)) {
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
		if (DATA_FRAME_IS_NULL(FC_SUBTYPE(fc)))
			return hdrlen;	/* no-data frame */
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
				ieee_802_11_hdr_print(fc, p - hdrlen, hdrlen,
				    meshdrlen, NULL, NULL);
			if (extracted_ethertype)
				printf("(LLC %s) ",
				    etherproto_string(
				        htons(extracted_ethertype)));
			if (!suppress_default_print)
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
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
u_int
ieee802_11_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	return ieee802_11_print(p, h->len, h->caplen, 0, 0);
}

#define	IEEE80211_CHAN_FHSS \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_GFSK)
#define	IEEE80211_CHAN_A \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_B \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define	IEEE80211_CHAN_PUREG \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)

#define	IS_CHAN_FHSS(flags) \
	((flags & IEEE80211_CHAN_FHSS) == IEEE80211_CHAN_FHSS)
#define	IS_CHAN_A(flags) \
	((flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)
#define	IS_CHAN_B(flags) \
	((flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)
#define	IS_CHAN_PUREG(flags) \
	((flags & IEEE80211_CHAN_PUREG) == IEEE80211_CHAN_PUREG)
#define	IS_CHAN_G(flags) \
	((flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)
#define	IS_CHAN_ANYG(flags) \
	(IS_CHAN_PUREG(flags) || IS_CHAN_G(flags))

static void
print_chaninfo(int freq, int flags)
{
	printf("%u MHz", freq);
	if (IS_CHAN_FHSS(flags))
		printf(" FHSS");
	if (IS_CHAN_A(flags)) {
		if (flags & IEEE80211_CHAN_HALF)
			printf(" 11a/10Mhz");
		else if (flags & IEEE80211_CHAN_QUARTER)
			printf(" 11a/5Mhz");
		else
			printf(" 11a");
	}
	if (IS_CHAN_ANYG(flags)) {
		if (flags & IEEE80211_CHAN_HALF)
			printf(" 11g/10Mhz");
		else if (flags & IEEE80211_CHAN_QUARTER)
			printf(" 11g/5Mhz");
		else
			printf(" 11g");
	} else if (IS_CHAN_B(flags))
		printf(" 11b");
	if (flags & IEEE80211_CHAN_TURBO)
		printf(" Turbo");
	if (flags & IEEE80211_CHAN_HT20)
		printf(" ht/20");
	else if (flags & IEEE80211_CHAN_HT40D)
		printf(" ht/40-");
	else if (flags & IEEE80211_CHAN_HT40U)
		printf(" ht/40+");
	printf(" ");
}

static int
print_radiotap_field(struct cpack_state *s, u_int32_t bit, u_int8_t *flags)
{
	union {
		int8_t		i8;
		u_int8_t	u8;
		int16_t		i16;
		u_int16_t	u16;
		u_int32_t	u32;
		u_int64_t	u64;
	} u, u2, u3, u4;
	int rc;

	switch (bit) {
	case IEEE80211_RADIOTAP_FLAGS:
		rc = cpack_uint8(s, &u.u8);
		*flags = u.u8;
		break;
	case IEEE80211_RADIOTAP_RATE:
	case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
	case IEEE80211_RADIOTAP_DB_ANTNOISE:
	case IEEE80211_RADIOTAP_ANTENNA:
		rc = cpack_uint8(s, &u.u8);
		break;
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
	case IEEE80211_RADIOTAP_DBM_ANTNOISE:
		rc = cpack_int8(s, &u.i8);
		break;
	case IEEE80211_RADIOTAP_CHANNEL:
		rc = cpack_uint16(s, &u.u16);
		if (rc != 0)
			break;
		rc = cpack_uint16(s, &u2.u16);
		break;
	case IEEE80211_RADIOTAP_FHSS:
	case IEEE80211_RADIOTAP_LOCK_QUALITY:
	case IEEE80211_RADIOTAP_TX_ATTENUATION:
		rc = cpack_uint16(s, &u.u16);
		break;
	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
		rc = cpack_uint8(s, &u.u8);
		break;
	case IEEE80211_RADIOTAP_DBM_TX_POWER:
		rc = cpack_int8(s, &u.i8);
		break;
	case IEEE80211_RADIOTAP_TSFT:
		rc = cpack_uint64(s, &u.u64);
		break;
	case IEEE80211_RADIOTAP_XCHANNEL:
		rc = cpack_uint32(s, &u.u32);
		if (rc != 0)
			break;
		rc = cpack_uint16(s, &u2.u16);
		if (rc != 0)
			break;
		rc = cpack_uint8(s, &u3.u8);
		if (rc != 0)
			break;
		rc = cpack_uint8(s, &u4.u8);
		break;
	default:
		/* this bit indicates a field whose
		 * size we do not know, so we cannot
		 * proceed.  Just print the bit number.
		 */
		printf("[bit %u] ", bit);
		return -1;
	}

	if (rc != 0) {
		printf("[|802.11]");
		return rc;
	}

	switch (bit) {
	case IEEE80211_RADIOTAP_CHANNEL:
		print_chaninfo(u.u16, u2.u16);
		break;
	case IEEE80211_RADIOTAP_FHSS:
		printf("fhset %d fhpat %d ", u.u16 & 0xff, (u.u16 >> 8) & 0xff);
		break;
	case IEEE80211_RADIOTAP_RATE:
		if (u.u8 & 0x80)
			PRINT_HT_RATE("", u.u8, " Mb/s ");
		else
			PRINT_RATE("", u.u8, " Mb/s ");
		break;
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
		printf("%ddB signal ", u.i8);
		break;
	case IEEE80211_RADIOTAP_DBM_ANTNOISE:
		printf("%ddB noise ", u.i8);
		break;
	case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
		printf("%ddB signal ", u.u8);
		break;
	case IEEE80211_RADIOTAP_DB_ANTNOISE:
		printf("%ddB noise ", u.u8);
		break;
	case IEEE80211_RADIOTAP_LOCK_QUALITY:
		printf("%u sq ", u.u16);
		break;
	case IEEE80211_RADIOTAP_TX_ATTENUATION:
		printf("%d tx power ", -(int)u.u16);
		break;
	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
		printf("%ddB tx power ", -(int)u.u8);
		break;
	case IEEE80211_RADIOTAP_DBM_TX_POWER:
		printf("%ddBm tx power ", u.i8);
		break;
	case IEEE80211_RADIOTAP_FLAGS:
		if (u.u8 & IEEE80211_RADIOTAP_F_CFP)
			printf("cfp ");
		if (u.u8 & IEEE80211_RADIOTAP_F_SHORTPRE)
			printf("short preamble ");
		if (u.u8 & IEEE80211_RADIOTAP_F_WEP)
			printf("wep ");
		if (u.u8 & IEEE80211_RADIOTAP_F_FRAG)
			printf("fragmented ");
		if (u.u8 & IEEE80211_RADIOTAP_F_BADFCS)
			printf("bad-fcs ");
		break;
	case IEEE80211_RADIOTAP_ANTENNA:
		printf("antenna %d ", u.u8);
		break;
	case IEEE80211_RADIOTAP_TSFT:
		printf("%" PRIu64 "us tsft ", u.u64);
		break;
	case IEEE80211_RADIOTAP_XCHANNEL:
		print_chaninfo(u2.u16, u.u32);
		break;
	}
	return 0;
}

static u_int
ieee802_11_radio_print(const u_char *p, u_int length, u_int caplen)
{
#define	BITNO_32(x) (((x) >> 16) ? 16 + BITNO_16((x) >> 16) : BITNO_16((x)))
#define	BITNO_16(x) (((x) >> 8) ? 8 + BITNO_8((x) >> 8) : BITNO_8((x)))
#define	BITNO_8(x) (((x) >> 4) ? 4 + BITNO_4((x) >> 4) : BITNO_4((x)))
#define	BITNO_4(x) (((x) >> 2) ? 2 + BITNO_2((x) >> 2) : BITNO_2((x)))
#define	BITNO_2(x) (((x) & 2) ? 1 : 0)
#define	BIT(n)	(1U << n)
#define	IS_EXTENDED(__p)	\
	    (EXTRACT_LE_32BITS(__p) & BIT(IEEE80211_RADIOTAP_EXT)) != 0

	struct cpack_state cpacker;
	struct ieee80211_radiotap_header *hdr;
	u_int32_t present, next_present;
	u_int32_t *presentp, *last_presentp;
	enum ieee80211_radiotap_type bit;
	int bit0;
	const u_char *iter;
	u_int len;
	u_int8_t flags;
	int pad;
	u_int fcslen;

	if (caplen < sizeof(*hdr)) {
		printf("[|802.11]");
		return caplen;
	}

	hdr = (struct ieee80211_radiotap_header *)p;

	len = EXTRACT_LE_16BITS(&hdr->it_len);

	if (caplen < len) {
		printf("[|802.11]");
		return caplen;
	}
	for (last_presentp = &hdr->it_present;
	     IS_EXTENDED(last_presentp) &&
	     (u_char*)(last_presentp + 1) <= p + len;
	     last_presentp++);

	/* are there more bitmap extensions than bytes in header? */
	if (IS_EXTENDED(last_presentp)) {
		printf("[|802.11]");
		return caplen;
	}

	iter = (u_char*)(last_presentp + 1);

	if (cpack_init(&cpacker, (u_int8_t*)iter, len - (iter - p)) != 0) {
		/* XXX */
		printf("[|802.11]");
		return caplen;
	}

	/* Assume no flags */
	flags = 0;
	/* Assume no Atheros padding between 802.11 header and body */
	pad = 0;
	/* Assume no FCS at end of frame */
	fcslen = 0;
	for (bit0 = 0, presentp = &hdr->it_present; presentp <= last_presentp;
	     presentp++, bit0 += 32) {
		for (present = EXTRACT_LE_32BITS(presentp); present;
		     present = next_present) {
			/* clear the least significant bit that is set */
			next_present = present & (present - 1);

			/* extract the least significant bit that is set */
			bit = (enum ieee80211_radiotap_type)
			    (bit0 + BITNO_32(present ^ next_present));

			if (print_radiotap_field(&cpacker, bit, &flags) != 0)
				goto out;
		}
	}

	if (flags & IEEE80211_RADIOTAP_F_DATAPAD)
		pad = 1;	/* Atheros padding */
	if (flags & IEEE80211_RADIOTAP_F_FCS)
		fcslen = 4;	/* FCS at end of packet */
out:
	return len + ieee802_11_print(p + len, length - len, caplen - len, pad,
	    fcslen);
#undef BITNO_32
#undef BITNO_16
#undef BITNO_8
#undef BITNO_4
#undef BITNO_2
#undef BIT
}

static u_int
ieee802_11_avs_radio_print(const u_char *p, u_int length, u_int caplen)
{
	u_int32_t caphdr_len;

	if (caplen < 8) {
		printf("[|802.11]");
		return caplen;
	}

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
	    length - caphdr_len, caplen - caphdr_len, 0, 0);
}

#define PRISM_HDR_LEN		144

#define WLANCAP_MAGIC_COOKIE_BASE 0x80211000
#define WLANCAP_MAGIC_COOKIE_V1	0x80211001
#define WLANCAP_MAGIC_COOKIE_V2	0x80211002

/*
 * For DLT_PRISM_HEADER; like DLT_IEEE802_11, but with an extra header,
 * containing information such as radio information, which we
 * currently ignore.
 *
 * If, however, the packet begins with WLANCAP_MAGIC_COOKIE_V1 or
 * WLANCAP_MAGIC_COOKIE_V2, it's really DLT_IEEE802_11_RADIO_AVS
 * (currently, on Linux, there's no ARPHRD_ type for
 * DLT_IEEE802_11_RADIO_AVS, as there is a ARPHRD_IEEE80211_PRISM
 * for DLT_PRISM_HEADER, so ARPHRD_IEEE80211_PRISM is used for
 * the AVS header, and the first 4 bytes of the header are used to
 * indicate whether it's a Prism header or an AVS header).
 */
u_int
prism_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	u_int32_t msgcode;

	if (caplen < 4) {
		printf("[|802.11]");
		return caplen;
	}

	msgcode = EXTRACT_32BITS(p);
	if (msgcode == WLANCAP_MAGIC_COOKIE_V1 ||
	    msgcode == WLANCAP_MAGIC_COOKIE_V2)
		return ieee802_11_avs_radio_print(p, length, caplen);

	if (caplen < PRISM_HDR_LEN) {
		printf("[|802.11]");
		return caplen;
	}

	return PRISM_HDR_LEN + ieee802_11_print(p + PRISM_HDR_LEN,
	    length - PRISM_HDR_LEN, caplen - PRISM_HDR_LEN, 0, 0);
}

/*
 * For DLT_IEEE802_11_RADIO; like DLT_IEEE802_11, but with an extra
 * header, containing information such as radio information.
 */
u_int
ieee802_11_radio_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	return ieee802_11_radio_print(p, h->len, h->caplen);
}

/*
 * For DLT_IEEE802_11_RADIO_AVS; like DLT_IEEE802_11, but with an
 * extra header, containing information such as radio information,
 * which we currently ignore.
 */
u_int
ieee802_11_radio_avs_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	return ieee802_11_avs_radio_print(p, h->len, h->caplen);
}
