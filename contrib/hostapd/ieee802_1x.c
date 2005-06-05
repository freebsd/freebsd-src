/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / IEEE 802.1X Authenticator
 * Copyright (c) 2002-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "hostapd.h"
#include "ieee802_1x.h"
#include "accounting.h"
#include "radius.h"
#include "radius_client.h"
#include "eapol_sm.h"
#include "md5.h"
#include "rc4.h"
#include "eloop.h"
#include "sta_info.h"
#include "wpa.h"
#include "driver.h"
#include "eap.h"


static void ieee802_1x_new_auth_session(struct hostapd_data *hapd,
					struct sta_info *sta);


static void ieee802_1x_send(hostapd *hapd, struct sta_info *sta, u8 type,
			    u8 *data, size_t datalen)
{
	u8 *buf;
	struct ieee802_1x_hdr *xhdr;
	size_t len;
	int encrypt = 0;

	len = sizeof(*xhdr) + datalen;
	buf = malloc(len);
	if (buf == NULL) {
		printf("malloc() failed for ieee802_1x_send(len=%lu)\n",
		       (unsigned long) len);
		return;
	}

	memset(buf, 0, len);
#if 0
	/* TODO:
	 * According to IEEE 802.1aa/D4 EAPOL-Key should be sent before any
	 * remaining EAP frames, if possible. This would allow rest of the
	 * frames to be encrypted. This code could be used to request
	 * encryption from the kernel driver. */
	if (sta->eapol_sm &&
	    sta->eapol_sm->be_auth.state == BE_AUTH_SUCCESS &&
	    sta->eapol_sm->keyTxEnabled)
		encrypt = 1;
#endif

	xhdr = (struct ieee802_1x_hdr *) buf;
	xhdr->version = EAPOL_VERSION;
	xhdr->type = type;
	xhdr->length = htons(datalen);

	if (datalen > 0 && data != NULL)
		memcpy(xhdr + 1, data, datalen);

	if (sta->wpa_sm && sta->wpa_sm->pairwise_set)
		encrypt = 1;
	if (sta->flags & WLAN_STA_PREAUTH) {
		rsn_preauth_send(hapd, sta, buf, len);
	} else {
		hostapd_send_eapol(hapd, sta->addr, buf, len, encrypt);
	}

	free(buf);
}


void ieee802_1x_set_sta_authorized(hostapd *hapd, struct sta_info *sta,
				   int authorized)
{
	if (sta->flags & WLAN_STA_PREAUTH)
		return;

	if (authorized) {
		sta->flags |= WLAN_STA_AUTHORIZED;
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG, "authorizing port");
	} else {
		sta->flags &= ~WLAN_STA_AUTHORIZED;
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG, "unauthorizing port");
	}

	hostapd_set_sta_authorized(hapd, sta->addr, authorized);
	if (authorized)
		accounting_sta_start(hapd, sta);
}


static void ieee802_1x_eap_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct sta_info *sta = eloop_ctx;
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;
	hostapd_logger(sm->hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_DEBUG, "EAP timeout");
	sm->eapTimeout = TRUE;
	eapol_sm_step(sm);
}


void ieee802_1x_request_identity(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	u8 *buf;
	struct eap_hdr *eap;
	int extra, tlen;
	u8 *pos;
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (hapd->conf->eap_authenticator) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "IEEE 802.1X: Integrated EAP Authenticator in "
			      "use - do not generate EAP-Request/Identity\n");
		return;
	}

	if (sm == NULL || !sm->auth_pae.eapRestart)
		return;

	ieee802_1x_new_auth_session(hapd, sta);

	if (hapd->conf->eap_req_id_text)
		extra = strlen(hapd->conf->eap_req_id_text);
	else
		extra = 0;

	tlen = sizeof(*eap) + 1 + extra;

	buf = malloc(tlen);
	if (buf == NULL) {
		printf("Could not allocate memory for identity request\n");
		return;
	}

	memset(buf, 0, tlen);

	eap = (struct eap_hdr *) buf;
	eap->code = EAP_CODE_REQUEST;
	eap->identifier = ++sm->currentId;
	eap->length = htons(tlen);
	pos = (u8 *) (eap + 1);
	*pos++ = EAP_TYPE_IDENTITY;
	if (hapd->conf->eap_req_id_text)
		memcpy(pos, hapd->conf->eap_req_id_text, extra);

	sm->be_auth.eapReq = TRUE;
	free(sm->last_eap_radius);
	sm->last_eap_radius = buf;
	sm->last_eap_radius_len = tlen;

	eloop_cancel_timeout(ieee802_1x_eap_timeout, sta, NULL);
	eloop_register_timeout(30, 0, ieee802_1x_eap_timeout, sta, NULL);
	sm->eapTimeout = FALSE;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: Generated EAP Request-Identity for " MACSTR
		      " (identifier %d, timeout 30)\n", MAC2STR(sta->addr),
		      eap->identifier);

	sm->auth_pae.eapRestart = FALSE;
}


void ieee802_1x_tx_canned_eap(struct hostapd_data *hapd, struct sta_info *sta,
			      int success)
{
	struct eap_hdr eap;
	struct eapol_state_machine *sm = sta->eapol_sm;

	memset(&eap, 0, sizeof(eap));

	eap.code = success ? EAP_CODE_SUCCESS : EAP_CODE_FAILURE;
	eap.identifier = 1;
	if (sm && sm->last_eap_radius) {
		struct eap_hdr *hdr = (struct eap_hdr *) sm->last_eap_radius;
		eap.identifier = hdr->identifier + 1;
	}
	eap.length = htons(sizeof(eap));

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: Sending canned EAP packet %s to " MACSTR
		      " (identifier %d)\n", success ? "SUCCESS" : "FAILURE",
		      MAC2STR(sta->addr), eap.identifier);
	ieee802_1x_send(hapd, sta, IEEE802_1X_TYPE_EAP_PACKET, (u8 *) &eap,
			sizeof(eap));
	if (sm)
		sm->dot1xAuthEapolFramesTx++;
}


void ieee802_1x_tx_req(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct eap_hdr *eap;
	struct eapol_state_machine *sm = sta->eapol_sm;
	u8 *type;
	if (sm == NULL)
		return;

	if (sm->last_eap_radius == NULL) {
		printf("Error: TxReq called for station " MACSTR ", but there "
		       "is no EAP request from the authentication server\n",
		       MAC2STR(sm->addr));
		return;
	}

	eap = (struct eap_hdr *) sm->last_eap_radius;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: Sending EAP Packet to " MACSTR
		      " (identifier %d)\n", MAC2STR(sm->addr),
		      eap->identifier);
	sm->currentId = eap->identifier;
	ieee802_1x_send(hapd, sta, IEEE802_1X_TYPE_EAP_PACKET,
			sm->last_eap_radius, sm->last_eap_radius_len);
	sm->dot1xAuthEapolFramesTx++;
	type = (u8 *) (eap + 1);
	if (sm->last_eap_radius_len > sizeof(*eap) &&
	    *type == EAP_TYPE_IDENTITY)
		sm->dot1xAuthEapolReqIdFramesTx++;
	else
		sm->dot1xAuthEapolReqFramesTx++;
}


void hostapd_get_ntp_timestamp(u8 *buf)
{
	struct timeval now;
	u32 sec, usec;

	/* 64-bit NTP timestamp (time from 1900-01-01 00:00:00) */
	gettimeofday(&now, NULL);
	sec = htonl(now.tv_sec + 2208988800U); /* Epoch to 1900 */
	/* Estimate 2^32/10^6 = 4295 - 1/32 - 1/512 */
	usec = now.tv_usec;
	usec = htonl(4295 * usec - (usec >> 5) - (usec >> 9));
	memcpy(buf, (u8 *) &sec, 4);
	memcpy(buf + 4, (u8 *) &usec, 4);
}


static void ieee802_1x_tx_key_one(hostapd *hapd, struct sta_info *sta,
				  int index, int broadcast,
				  u8 *key_data, size_t key_len)
{
	u8 *buf, *ekey;
	struct ieee802_1x_hdr *hdr;
	struct ieee802_1x_eapol_key *key;
	size_t len, ekey_len;
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (sm == NULL)
		return;

	len = sizeof(*key) + key_len;
	buf = malloc(sizeof(*hdr) + len);
	if (buf == NULL)
		return;

	memset(buf, 0, sizeof(*hdr) + len);
	hdr = (struct ieee802_1x_hdr *) buf;
	key = (struct ieee802_1x_eapol_key *) (hdr + 1);
	key->type = EAPOL_KEY_TYPE_RC4;
	key->key_length = htons(key_len);
	hostapd_get_ntp_timestamp(key->replay_counter);

	if (hostapd_get_rand(key->key_iv, sizeof(key->key_iv))) {
		printf("Could not get random numbers\n");
		free(buf);
		return;
	}

	key->key_index = index | (broadcast ? 0 : BIT(7));
	if (hapd->conf->eapol_key_index_workaround) {
		/* According to some information, WinXP Supplicant seems to
		 * interpret bit7 as an indication whether the key is to be
		 * activated, so make it possible to enable workaround that
		 * sets this bit for all keys. */
		key->key_index |= BIT(7);
	}

	/* Key is encrypted using "Key-IV + sm->eapol_key_crypt" as the
	 * RC4-key */
	memcpy((u8 *) (key + 1), key_data, key_len);
	ekey_len = sizeof(key->key_iv) + sm->eapol_key_crypt_len;
	ekey = malloc(ekey_len);
	if (ekey == NULL) {
		printf("Could not encrypt key\n");
		free(buf);
		return;
	}
	memcpy(ekey, key->key_iv, sizeof(key->key_iv));
	memcpy(ekey + sizeof(key->key_iv), sm->eapol_key_crypt,
	       sm->eapol_key_crypt_len);
	rc4((u8 *) (key + 1), key_len, ekey, ekey_len);
	free(ekey);

	/* This header is needed here for HMAC-MD5, but it will be regenerated
	 * in ieee802_1x_send() */
	hdr->version = EAPOL_VERSION;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(len);
	hmac_md5(sm->eapol_key_sign, sm->eapol_key_sign_len,
		 buf, sizeof(*hdr) + len,
		 key->key_signature);

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: Sending EAPOL-Key to " MACSTR
		      " (%s index=%d)\n", MAC2STR(sm->addr),
		      broadcast ? "broadcast" : "unicast", index);
	ieee802_1x_send(hapd, sta, IEEE802_1X_TYPE_EAPOL_KEY, (u8 *) key, len);
	if (sta->eapol_sm)
		sta->eapol_sm->dot1xAuthEapolFramesTx++;
	free(buf);
}


void ieee802_1x_tx_key(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (sm == NULL || !sm->eapol_key_sign || !sm->eapol_key_crypt)
		return;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: Sending EAPOL-Key(s) to " MACSTR "\n",
		      MAC2STR(sta->addr));

	if (hapd->default_wep_key)
		ieee802_1x_tx_key_one(hapd, sta, hapd->default_wep_key_idx, 1,
				      hapd->default_wep_key,
				      hapd->conf->default_wep_key_len);

	if (hapd->conf->individual_wep_key_len > 0) {
		u8 *ikey;
		ikey = malloc(hapd->conf->individual_wep_key_len);
		if (ikey == NULL ||
		    hostapd_get_rand(ikey,
				     hapd->conf->individual_wep_key_len)) {
			printf("Could not generate random individual WEP "
			       "key.\n");
			free(ikey);
			return;
		}

		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL))
			hostapd_hexdump("Individual WEP key",
					ikey,
					hapd->conf->individual_wep_key_len);

		ieee802_1x_tx_key_one(hapd, sta, 0, 0, ikey,
				      hapd->conf->individual_wep_key_len);

		/* TODO: set encryption in TX callback, i.e., only after STA
		 * has ACKed EAPOL-Key frame */
		if (hostapd_set_encryption(hapd, "WEP", sta->addr, 0, ikey,
					   hapd->conf->
					   individual_wep_key_len)) {
			printf("Could not set individual WEP encryption.\n");
		}

		free(ikey);
	}
}


static void ieee802_1x_encapsulate_radius(hostapd *hapd, struct sta_info *sta,
					  u8 *eap, size_t len)
{
	struct radius_msg *msg;
	char buf[128];
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (sm == NULL)
		return;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "Encapsulating EAP message into a RADIUS packet\n");

	sm->radius_identifier = radius_client_get_id(hapd->radius);
	msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST,
			     sm->radius_identifier);
	if (msg == NULL) {
		printf("Could not create net RADIUS packet\n");
		return;
	}

	radius_msg_make_authenticator(msg, (u8 *) sta, sizeof(*sta));

	if (sm->identity &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME,
				 sm->identity, sm->identity_len)) {
		printf("Could not add User-Name\n");
		goto fail;
	}

	if (!radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
				 (u8 *) &hapd->conf->own_ip_addr, 4)) {
		printf("Could not add NAS-IP-Address\n");
		goto fail;
	}

	if (hapd->conf->nas_identifier &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IDENTIFIER,
				 (u8 *) hapd->conf->nas_identifier,
				 strlen(hapd->conf->nas_identifier))) {
		printf("Could not add NAS-Identifier\n");
		goto fail;
	}

	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT, sta->aid)) {
		printf("Could not add NAS-Port\n");
		goto fail;
	}

	snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT ":%s",
		 MAC2STR(hapd->own_addr), hapd->conf->ssid);
	if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLED_STATION_ID,
				 (u8 *) buf, strlen(buf))) {
		printf("Could not add Called-Station-Id\n");
		goto fail;
	}

	snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
		 MAC2STR(sta->addr));
	if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLING_STATION_ID,
				 (u8 *) buf, strlen(buf))) {
		printf("Could not add Calling-Station-Id\n");
		goto fail;
	}

	/* TODO: should probably check MTU from driver config; 2304 is max for
	 * IEEE 802.11, but use 1400 to avoid problems with too large packets
	 */
	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_FRAMED_MTU, 1400)) {
		printf("Could not add Framed-MTU\n");
		goto fail;
	}

	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT_TYPE,
				       RADIUS_NAS_PORT_TYPE_IEEE_802_11)) {
		printf("Could not add NAS-Port-Type\n");
		goto fail;
	}

	snprintf(buf, sizeof(buf), "CONNECT 11Mbps 802.11b");
	if (!radius_msg_add_attr(msg, RADIUS_ATTR_CONNECT_INFO,
				 (u8 *) buf, strlen(buf))) {
		printf("Could not add Connect-Info\n");
		goto fail;
	}

	if (eap && !radius_msg_add_eap(msg, eap, len)) {
		printf("Could not add EAP-Message\n");
		goto fail;
	}

	/* State attribute must be copied if and only if this packet is
	 * Access-Request reply to the previous Access-Challenge */
	if (sm->last_recv_radius && sm->last_recv_radius->hdr->code ==
	    RADIUS_CODE_ACCESS_CHALLENGE) {
		int res = radius_msg_copy_attr(msg, sm->last_recv_radius,
					       RADIUS_ATTR_STATE);
		if (res < 0) {
			printf("Could not copy State attribute from previous "
			       "Access-Challenge\n");
			goto fail;
		}
		if (res > 0) {
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "  Copied RADIUS State Attribute\n");
		}
	}

	radius_client_send(hapd->radius, msg, RADIUS_AUTH, sta->addr);
	return;

 fail:
	radius_msg_free(msg);
	free(msg);
}


static char *eap_type_text(u8 type)
{
	switch (type) {
	case EAP_TYPE_IDENTITY: return "Identity";
	case EAP_TYPE_NOTIFICATION: return "Notification";
	case EAP_TYPE_NAK: return "Nak";
	case EAP_TYPE_MD5: return "MD5-Challenge";
	case EAP_TYPE_OTP: return "One-Time Password";
	case EAP_TYPE_GTC: return "Generic Token Card";
	case EAP_TYPE_TLS: return "TLS";
	case EAP_TYPE_TTLS: return "TTLS";
	case EAP_TYPE_PEAP: return "PEAP";
	default: return "Unknown";
	}
}


static void handle_eap_response(hostapd *hapd, struct sta_info *sta,
				struct eap_hdr *eap, u8 *data, size_t len)
{
	u8 type;
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;

	if (eap->identifier != sm->currentId) {
		hostapd_logger(hapd, sm->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG,
			       "EAP Identifier of the Response-Identity does "
			       "not match (was %d, expected %d) - ignored",
			       eap->identifier, sm->currentId);
		return;
	}

	if (len < 1) {
		printf("handle_eap_response: too short response data\n");
		return;
	}

	eloop_cancel_timeout(ieee802_1x_eap_timeout, sta, NULL);

	free(sm->last_eap_supp);
	sm->last_eap_supp_len = sizeof(*eap) + len;
	sm->last_eap_supp = (u8 *) malloc(sm->last_eap_supp_len);
	if (sm->last_eap_supp == NULL) {
		printf("Could not alloc memory for last EAP Response\n");
		return;
	}
	memcpy(sm->last_eap_supp, eap, sizeof(*eap));
	memcpy(sm->last_eap_supp + sizeof(*eap), data, len);

	type = data[0];
	data++;
	len--;

	hostapd_logger(hapd, sm->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_DEBUG, "received EAP packet (code=%d "
		       "id=%d len=%d) from STA: EAP Response-%s (%d)",
		       eap->code, eap->identifier, ntohs(eap->length),
		       eap_type_text(type), type);

	if (type == EAP_TYPE_IDENTITY) {
		char *buf, *pos;
		int i;
		buf = malloc(4 * len + 1);
		if (buf) {
			pos = buf;
			for (i = 0; i < len; i++) {
				if (data[i] >= 32 && data[i] < 127)
					*pos++ = data[i];
				else {
					snprintf(pos, 5, "{%02x}", data[i]);
					pos += 4;
				}
			}
			*pos = '\0';
			hostapd_logger(hapd, sm->addr,
				       HOSTAPD_MODULE_IEEE8021X,
				       HOSTAPD_LEVEL_DEBUG,
				       "STA identity '%s'", buf);
			free(buf);
		}

		sm->rx_identity = TRUE;
		sm->dot1xAuthEapolRespIdFramesRx++;

		/* Save station identity for future RADIUS packets */
		free(sm->identity);
		sm->identity = malloc(len + 1);
		if (sm->identity) {
			memcpy(sm->identity, data, len);
			sm->identity[len] = '\0';
			sm->identity_len = len;
		}
	} else
		sm->dot1xAuthEapolRespFramesRx++;

	sm->eapolEap = TRUE;
}


/* Process incoming EAP packet from Supplicant */
static void handle_eap(hostapd *hapd, struct sta_info *sta, u8 *buf,
		       size_t len)
{
	struct eap_hdr *eap;
	u16 eap_len;

	if (len < sizeof(*eap)) {
		printf("   too short EAP packet\n");
		return;
	}

	eap = (struct eap_hdr *) buf;

	eap_len = ntohs(eap->length);
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "   EAP: code=%d identifier=%d length=%d",
		      eap->code, eap->identifier, eap_len);
	if (eap_len < sizeof(*eap)) {
		printf("   Invalid EAP length\n");
		return;
	} else if (eap_len > len) {
		printf("   Too short frame to contain this EAP packet\n");
		return;
	} else if (eap_len < len) {
		printf("   Ignoring %lu extra bytes after EAP packet\n",
		       (unsigned long) len - eap_len);
	}

	eap_len -= sizeof(*eap);

	switch (eap->code) {
	case EAP_CODE_REQUEST:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, " (request)\n");
		return;
	case EAP_CODE_RESPONSE:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, " (response)\n");
		handle_eap_response(hapd, sta, eap, (u8 *) (eap + 1), eap_len);
		break;
	case EAP_CODE_SUCCESS:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, " (success)\n");
		return;
	case EAP_CODE_FAILURE:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, " (failure)\n");
		return;
	default:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, " (unknown code)\n");
		return;
	}
}


/* Process the EAPOL frames from the Supplicant */
void ieee802_1x_receive(hostapd *hapd, u8 *sa, u8 *buf, size_t len)
{
	struct sta_info *sta;
	struct ieee802_1x_hdr *hdr;
	struct ieee802_1x_eapol_key *key;
	u16 datalen;

	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa)
		return;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: %lu bytes from " MACSTR "\n",
		      (unsigned long) len, MAC2STR(sa));
	sta = ap_get_sta(hapd, sa);
	if (!sta) {
		printf("   no station information available\n");
		return;
	}

	if (len < sizeof(*hdr)) {
		printf("   too short IEEE 802.1X packet\n");
		return;
	}

	hdr = (struct ieee802_1x_hdr *) buf;
	datalen = ntohs(hdr->length);
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "   IEEE 802.1X: version=%d type=%d length=%d\n",
		      hdr->version, hdr->type, datalen);

	if (len - sizeof(*hdr) < datalen) {
		printf("   frame too short for this IEEE 802.1X packet\n");
		if (sta->eapol_sm)
			sta->eapol_sm->dot1xAuthEapLengthErrorFramesRx++;
		return;
	}
	if (len - sizeof(*hdr) > datalen) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "   ignoring %lu extra octets after IEEE 802.1X "
			      "packet\n",
			      (unsigned long) len - sizeof(*hdr) - datalen);
	}

	if (sta->eapol_sm) {
		sta->eapol_sm->dot1xAuthLastEapolFrameVersion = hdr->version;
		sta->eapol_sm->dot1xAuthEapolFramesRx++;
	}

	key = (struct ieee802_1x_eapol_key *) (hdr + 1);
	if (datalen >= sizeof(struct ieee802_1x_eapol_key) &&
	    hdr->type == IEEE802_1X_TYPE_EAPOL_KEY &&
	    (key->type == EAPOL_KEY_TYPE_WPA ||
	     key->type == EAPOL_KEY_TYPE_RSN)) {
		wpa_receive(hapd, sta, (u8 *) hdr, sizeof(*hdr) + datalen);
		return;
	}

	if (!hapd->conf->ieee802_1x || sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
		return;

	if (!sta->eapol_sm) {
		sta->eapol_sm = eapol_sm_alloc(hapd, sta);
		if (!sta->eapol_sm)
			return;
	}

	/* since we support version 1, we can ignore version field and proceed
	 * as specified in version 1 standard [IEEE Std 802.1X-2001, 7.5.5] */
	/* TODO: actually, we are not version 1 anymore.. However, Version 2
	 * does not change frame contents, so should be ok to process frames
	 * more or less identically. Some changes might be needed for
	 * verification of fields. */

	switch (hdr->type) {
	case IEEE802_1X_TYPE_EAP_PACKET:
		handle_eap(hapd, sta, (u8 *) (hdr + 1), datalen);
		break;

	case IEEE802_1X_TYPE_EAPOL_START:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG, "received EAPOL-Start "
			       "from STA");
		if (sta->pmksa) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG, "cached PMKSA "
				       "available - ignore it since "
				       "STA sent EAPOL-Start");
			sta->pmksa = NULL;
		}
		sta->eapol_sm->auth_pae.eapolStart = TRUE;
		sta->eapol_sm->dot1xAuthEapolStartFramesRx++;
		wpa_sm_event(hapd, sta, WPA_REAUTH_EAPOL);
		eapol_sm_step(sta->eapol_sm);
		break;

	case IEEE802_1X_TYPE_EAPOL_LOGOFF:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG, "received EAPOL-Logoff "
			       "from STA");
		sta->acct_terminate_cause =
			RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
		sta->eapol_sm->auth_pae.eapolLogoff = TRUE;
		sta->eapol_sm->dot1xAuthEapolLogoffFramesRx++;
		eapol_sm_step(sta->eapol_sm);
		break;

	case IEEE802_1X_TYPE_EAPOL_KEY:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "   EAPOL-Key\n");
		if (!(sta->flags & WLAN_STA_AUTHORIZED)) {
			printf("   Dropped key data from unauthorized "
			       "Supplicant\n");
			break;
		}
		break;

	case IEEE802_1X_TYPE_EAPOL_ENCAPSULATED_ASF_ALERT:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "   EAPOL-Encapsulated-ASF-Alert\n");
		/* TODO: implement support for this; show data */
		break;

	default:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "   unknown IEEE 802.1X packet type\n");
		sta->eapol_sm->dot1xAuthInvalidEapolFramesRx++;
		break;
	}

	eapol_sm_step(sta->eapol_sm);
}


void ieee802_1x_new_station(hostapd *hapd, struct sta_info *sta)
{
	if (!hapd->conf->ieee802_1x || sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
		return;

	if (sta->eapol_sm) {
		sta->eapol_sm->portEnabled = TRUE;
		eapol_sm_step(sta->eapol_sm);
		return;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_DEBUG, "start authentication");
	sta->eapol_sm = eapol_sm_alloc(hapd, sta);
	if (sta->eapol_sm)
		sta->eapol_sm->portEnabled = TRUE;
}


void ieee802_1x_free_station(struct sta_info *sta)
{
	struct eapol_state_machine *sm = sta->eapol_sm;

	eloop_cancel_timeout(ieee802_1x_eap_timeout, sta, NULL);

	if (sm == NULL)
		return;

	sta->eapol_sm = NULL;

	if (sm->last_recv_radius) {
		radius_msg_free(sm->last_recv_radius);
		free(sm->last_recv_radius);
	}

	free(sm->last_eap_supp);
	free(sm->last_eap_radius);
	free(sm->identity);
	free(sm->radius_class);
	free(sm->eapol_key_sign);
	free(sm->eapol_key_crypt);
	eapol_sm_free(sm);
}


static void ieee802_1x_decapsulate_radius(hostapd *hapd, struct sta_info *sta)
{
	u8 *eap;
	size_t len;
	struct eap_hdr *hdr;
	int eap_type = -1;
	char buf[64];
	struct radius_msg *msg;
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (sm == NULL || sm->last_recv_radius == NULL) {
		if (sm)
			sm->be_auth.eapNoReq = TRUE;
		return;
	}

	msg = sm->last_recv_radius;

	eap = radius_msg_get_eap(msg, &len);
	if (eap == NULL) {
		/* draft-aboba-radius-rfc2869bis-20.txt, Chap. 2.6.3:
		 * RADIUS server SHOULD NOT send Access-Reject/no EAP-Message
		 * attribute */
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_WARNING, "could not extract "
			       "EAP-Message from RADIUS message");
		free(sm->last_eap_radius);
		sm->last_eap_radius = NULL;
		sm->last_eap_radius_len = 0;
		sm->be_auth.eapNoReq = TRUE;
		return;
	}

	if (len < sizeof(*hdr)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_WARNING, "too short EAP packet "
			       "received from authentication server");
		free(eap);
		sm->be_auth.eapNoReq = TRUE;
		return;
	}

	if (len > sizeof(*hdr))
		eap_type = eap[sizeof(*hdr)];

	hdr = (struct eap_hdr *) eap;
	switch (hdr->code) {
	case EAP_CODE_REQUEST:
		snprintf(buf, sizeof(buf), "EAP-Request-%s (%d)",
			 eap_type >= 0 ? eap_type_text(eap_type) : "??",
			 eap_type);
		break;
	case EAP_CODE_RESPONSE:
		snprintf(buf, sizeof(buf), "EAP Response-%s (%d)",
			 eap_type >= 0 ? eap_type_text(eap_type) : "??",
			 eap_type);
		break;
	case EAP_CODE_SUCCESS:
		snprintf(buf, sizeof(buf), "EAP Success");
		break;
	case EAP_CODE_FAILURE:
		snprintf(buf, sizeof(buf), "EAP Failure");
		break;
	default:
		snprintf(buf, sizeof(buf), "unknown EAP code");
		break;
	}
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_DEBUG, "decapsulated EAP packet (code=%d "
		       "id=%d len=%d) from RADIUS server: %s",
		      hdr->code, hdr->identifier, ntohs(hdr->length), buf);
	sm->be_auth.eapReq = TRUE;

	free(sm->last_eap_radius);
	sm->last_eap_radius = eap;
	sm->last_eap_radius_len = len;
}


static void ieee802_1x_get_keys(hostapd *hapd, struct sta_info *sta,
				struct radius_msg *msg, struct radius_msg *req,
				u8 *shared_secret, size_t shared_secret_len)
{
	struct radius_ms_mppe_keys *keys;
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;

	keys = radius_msg_get_ms_keys(msg, req, shared_secret,
				      shared_secret_len);

	if (keys) {
		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL) && keys->send) {
			size_t i;
			printf("MS-MPPE-Send-Key (len=%lu):",
			       (unsigned long) keys->send_len);
			for (i = 0; i < keys->send_len; i++)
				printf(" %02x", keys->send[i]);
			printf("\n");
		}
		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL) && keys->recv) {
			size_t i;
			printf("MS-MPPE-Recv-Key (len=%lu):",
			       (unsigned long) keys->recv_len);
			for (i = 0; i < keys->recv_len; i++)
				printf(" %02x", keys->recv[i]);
			printf("\n");
		}

		if (keys->send && keys->recv) {
			free(sm->eapol_key_sign);
			free(sm->eapol_key_crypt);
			sm->eapol_key_sign = keys->send;
			sm->eapol_key_sign_len = keys->send_len;
			sm->eapol_key_crypt = keys->recv;
			sm->eapol_key_crypt_len = keys->recv_len;
			if (hapd->default_wep_key ||
			    hapd->conf->individual_wep_key_len > 0 ||
			    hapd->conf->wpa)
				sta->eapol_sm->keyAvailable = TRUE;
		} else {
			free(keys->send);
			free(keys->recv);
		}
		free(keys);
	}
}


static void ieee802_1x_store_radius_class(struct hostapd_data *hapd,
					  struct sta_info *sta,
					  struct radius_msg *msg)
{
	u8 *class;
	size_t class_len;
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (!hapd->conf->acct_server || hapd->radius == NULL || sm == NULL)
		return;

	if (radius_msg_get_attr_ptr(msg, RADIUS_ATTR_CLASS, &class,
				    &class_len) < 0 ||
	    class_len < 1) {
		free(sm->radius_class);
		sm->radius_class = NULL;
		sm->radius_class_len = 0;
		return;
	}

	if (sm->radius_class == NULL ||
	    sm->radius_class_len < class_len) {
		free(sm->radius_class);
		sm->radius_class = malloc(class_len);
		if (sm->radius_class == NULL) {
			sm->radius_class_len = 0;
			return;
		}
	}

	memcpy(sm->radius_class, class, class_len);
	sm->radius_class_len = class_len;
}


struct sta_id_search {
	u8 identifier;
	struct eapol_state_machine *sm;
};


static int ieee802_1x_select_radius_identifier(struct hostapd_data *hapd,
					       struct sta_info *sta,
					       void *ctx)
{
	struct sta_id_search *id_search = ctx;
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (sm && sm->radius_identifier >= 0 &&
	    sm->radius_identifier == id_search->identifier) {
		id_search->sm = sm;
		return 1;
	}
	return 0;
}


static struct eapol_state_machine *
ieee802_1x_search_radius_identifier(struct hostapd_data *hapd, u8 identifier)
{
	struct sta_id_search id_search;
	id_search.identifier = identifier;
	id_search.sm = NULL;
	ap_for_each_sta(hapd, ieee802_1x_select_radius_identifier, &id_search);
	return id_search.sm;
}


/* Process the RADIUS frames from Authentication Server */
static RadiusRxResult
ieee802_1x_receive_auth(struct radius_msg *msg, struct radius_msg *req,
			u8 *shared_secret, size_t shared_secret_len,
			void *data)
{
	struct hostapd_data *hapd = data;
	struct sta_info *sta;
	u32 session_timeout, termination_action, acct_interim_interval;
	int session_timeout_set;
	int eap_timeout;
	struct eapol_state_machine *sm;
	int override_eapReq = 0;

	sm = ieee802_1x_search_radius_identifier(hapd, msg->hdr->identifier);
	if (sm == NULL) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "IEEE 802.1X: Could not "
			      "find matching station for this RADIUS "
			      "message\n");
		return RADIUS_RX_UNKNOWN;
	}
	sta = sm->sta;

	/* RFC 2869, Ch. 5.13: valid Message-Authenticator attribute MUST be
	 * present when packet contains an EAP-Message attribute */
	if (msg->hdr->code == RADIUS_CODE_ACCESS_REJECT &&
	    radius_msg_get_attr(msg, RADIUS_ATTR_MESSAGE_AUTHENTICATOR, NULL,
				0) < 0 &&
	    radius_msg_get_attr(msg, RADIUS_ATTR_EAP_MESSAGE, NULL, 0) < 0) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Allowing RADIUS "
			      "Access-Reject without Message-Authenticator "
			      "since it does not include EAP-Message\n");
	} else if (radius_msg_verify(msg, shared_secret, shared_secret_len,
				     req)) {
		printf("Incoming RADIUS packet did not have correct "
		       "Message-Authenticator - dropped\n");
		return RADIUS_RX_INVALID_AUTHENTICATOR;
	}

	if (msg->hdr->code != RADIUS_CODE_ACCESS_ACCEPT &&
	    msg->hdr->code != RADIUS_CODE_ACCESS_REJECT &&
	    msg->hdr->code != RADIUS_CODE_ACCESS_CHALLENGE) {
		printf("Unknown RADIUS message code\n");
		return RADIUS_RX_UNKNOWN;
	}

	sm->radius_identifier = -1;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "RADIUS packet matching with station " MACSTR "\n",
		      MAC2STR(sta->addr));

	if (sm->last_recv_radius) {
		radius_msg_free(sm->last_recv_radius);
		free(sm->last_recv_radius);
	}

	sm->last_recv_radius = msg;

	session_timeout_set =
		!radius_msg_get_attr_int32(msg, RADIUS_ATTR_SESSION_TIMEOUT,
					   &session_timeout);
	if (radius_msg_get_attr_int32(msg, RADIUS_ATTR_TERMINATION_ACTION,
				      &termination_action))
		termination_action = RADIUS_TERMINATION_ACTION_DEFAULT;

	if (hapd->conf->radius_acct_interim_interval == 0 &&
	    msg->hdr->code == RADIUS_CODE_ACCESS_ACCEPT &&
	    radius_msg_get_attr_int32(msg, RADIUS_ATTR_ACCT_INTERIM_INTERVAL,
				      &acct_interim_interval) == 0) {
		if (acct_interim_interval < 60) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE8021X,
				       HOSTAPD_LEVEL_INFO,
				       "ignored too small "
				       "Acct-Interim-Interval %d",
				       acct_interim_interval);
		} else
			sta->acct_interim_interval = acct_interim_interval;
	}


	switch (msg->hdr->code) {
	case RADIUS_CODE_ACCESS_ACCEPT:
		/* RFC 3580, Ch. 3.17 */
		if (session_timeout_set && termination_action ==
		    RADIUS_TERMINATION_ACTION_RADIUS_REQUEST) {
			sm->reauth_timer.reAuthPeriod = session_timeout;
		} else if (session_timeout_set)
			ap_sta_session_timeout(hapd, sta, session_timeout);

		sm->eapSuccess = TRUE;
		override_eapReq = 1;
		ieee802_1x_get_keys(hapd, sta, msg, req, shared_secret,
				    shared_secret_len);
		if (sm->keyAvailable) {
			pmksa_cache_add(hapd, sta, sm->eapol_key_crypt,
					session_timeout_set ?
					session_timeout : -1);
		}
		break;
	case RADIUS_CODE_ACCESS_REJECT:
		sm->eapFail = TRUE;
		override_eapReq = 1;
		break;
	case RADIUS_CODE_ACCESS_CHALLENGE:
		if (session_timeout_set) {
			/* RFC 2869, Ch. 2.3.2; RFC 3580, Ch. 3.17 */
			eap_timeout = session_timeout;
		} else
			eap_timeout = 30;
		hostapd_logger(hapd, sm->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG,
			       "using EAP timeout of %d seconds%s",
			       eap_timeout,
			       session_timeout_set ? " (from RADIUS)" : "");
		eloop_cancel_timeout(ieee802_1x_eap_timeout, sta, NULL);
		eloop_register_timeout(eap_timeout, 0, ieee802_1x_eap_timeout,
				       sta, NULL);
		sm->eapTimeout = FALSE;
		break;
	}

	ieee802_1x_store_radius_class(hapd, sta, msg);
	ieee802_1x_decapsulate_radius(hapd, sta);
	if (override_eapReq)
		sm->be_auth.eapReq = FALSE;

	eapol_sm_step(sm);

	return RADIUS_RX_QUEUED;
}


/* Handler for EAPOL Backend Authentication state machine sendRespToServer.
 * Forward the EAP Response from Supplicant to Authentication Server. */
void ieee802_1x_send_resp_to_server(hostapd *hapd, struct sta_info *sta)
{
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;

	if (hapd->conf->eap_authenticator) {
		eap_set_eapRespData(sm->eap, sm->last_eap_supp,
				    sm->last_eap_supp_len);
	} else {
		ieee802_1x_encapsulate_radius(hapd, sta, sm->last_eap_supp,
					      sm->last_eap_supp_len);
	}
}


void ieee802_1x_abort_auth(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_DEBUG, "aborting authentication");

	if (sm->last_recv_radius) {
		radius_msg_free(sm->last_recv_radius);
		free(sm->last_recv_radius);
		sm->last_recv_radius = NULL;
	}
	free(sm->last_eap_supp);
	sm->last_eap_supp = NULL;
	sm->last_eap_supp_len = 0;
	free(sm->last_eap_radius);
	sm->last_eap_radius = NULL;
	sm->last_eap_radius_len = 0;
	free(sm->radius_class);
	sm->radius_class = NULL;
	sm->radius_class_len = 0;
}


void ieee802_1x_set_port_enabled(hostapd *hapd, struct sta_info *sta,
				 int enabled)
{
	if (!sta->eapol_sm)
		return;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: station " MACSTR " port %s\n",
		      MAC2STR(sta->addr), enabled ? "enabled" : "disabled");
	sta->eapol_sm->portEnabled = enabled ? TRUE : FALSE;
	eapol_sm_step(sta->eapol_sm);
}


#ifdef HOSTAPD_DUMP_STATE
void ieee802_1x_dump_state(FILE *f, const char *prefix, struct sta_info *sta)
{
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;

	fprintf(f, "%sIEEE 802.1X:\n", prefix);

	if (sm->identity) {
		size_t i;
		fprintf(f, "%sidentity=", prefix);
		for (i = 0; i < sm->identity_len; i++)
			fprint_char(f, sm->identity[i]);
		fprintf(f, "\n");
	}

	fprintf(f, "%scached_packets=%s%s%s\n", prefix,
		sm->last_recv_radius ? "[RX RADIUS]" : "",
		sm->last_eap_radius ? "[EAP RADIUS]" : "",
		sm->last_eap_supp ? "[EAP SUPPLICANT]" : "");

	eapol_sm_dump_state(f, prefix, sm);
}
#endif /* HOSTAPD_DUMP_STATE */


static int ieee802_1x_rekey_broadcast(hostapd *hapd)
{
	if (hapd->conf->default_wep_key_len < 1)
		return 0;

	free(hapd->default_wep_key);
	hapd->default_wep_key = malloc(hapd->conf->default_wep_key_len);
	if (hapd->default_wep_key == NULL ||
	    hostapd_get_rand(hapd->default_wep_key,
			     hapd->conf->default_wep_key_len)) {
		printf("Could not generate random WEP key.\n");
		free(hapd->default_wep_key);
		hapd->default_wep_key = NULL;
		return -1;
	}

	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
		hostapd_hexdump("IEEE 802.1X: New default WEP key",
				hapd->default_wep_key,
				hapd->conf->default_wep_key_len);
	}

	return 0;
}


static int ieee802_1x_sta_key_available(struct hostapd_data *hapd,
					struct sta_info *sta, void *ctx)
{
	if (sta->eapol_sm) {
		sta->eapol_sm->keyAvailable = TRUE;
		eapol_sm_step(sta->eapol_sm);
	}
	return 0;
}


static void ieee802_1x_rekey(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;

	if (hapd->default_wep_key_idx >= 3)
		hapd->default_wep_key_idx =
			hapd->conf->individual_wep_key_len > 0 ? 1 : 0;
	else
		hapd->default_wep_key_idx++;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "IEEE 802.1X: New default WEP "
		      "key index %d\n", hapd->default_wep_key_idx);
		      
	if (ieee802_1x_rekey_broadcast(hapd)) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_WARNING, "failed to generate a "
			       "new broadcast key");
		free(hapd->default_wep_key);
		hapd->default_wep_key = NULL;
		return;
	}

	/* TODO: Could setup key for RX here, but change default TX keyid only
	 * after new broadcast key has been sent to all stations. */
	if (hostapd_set_encryption(hapd, "WEP", NULL,
				   hapd->default_wep_key_idx,
				   hapd->default_wep_key,
				   hapd->conf->default_wep_key_len)) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_WARNING, "failed to configure a "
			       "new broadcast key");
		free(hapd->default_wep_key);
		hapd->default_wep_key = NULL;
		return;
	}

	ap_for_each_sta(hapd, ieee802_1x_sta_key_available, NULL);

	if (hapd->conf->wep_rekeying_period > 0) {
		eloop_register_timeout(hapd->conf->wep_rekeying_period, 0,
				       ieee802_1x_rekey, hapd, NULL);
	}
}


int ieee802_1x_init(hostapd *hapd)
{
	int i;

	if ((hapd->conf->ieee802_1x || hapd->conf->wpa) &&
	    hostapd_set_ieee8021x(hapd, 1))
		return -1;

	if (radius_client_register(hapd->radius, RADIUS_AUTH,
				   ieee802_1x_receive_auth, hapd))
		return -1;

	if (hapd->conf->default_wep_key_len) {
		for (i = 0; i < 4; i++)
			hostapd_set_encryption(hapd, "none", NULL, i, NULL, 0);

		ieee802_1x_rekey(hapd, NULL);

		if (hapd->default_wep_key == NULL)
			return -1;
	}

	return 0;
}


void ieee802_1x_deinit(hostapd *hapd)
{
	if (hapd->driver != NULL &&
	    (hapd->conf->ieee802_1x || hapd->conf->wpa))
		hostapd_set_ieee8021x(hapd, 0);
}


static void ieee802_1x_new_auth_session(struct hostapd_data *hapd,
					struct sta_info *sta)
{
	struct eapol_state_machine *sm = sta->eapol_sm;
	if (sm == NULL)
		return;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "IEEE 802.1X: station " MACSTR " - new auth session, "
		      "clearing State\n", MAC2STR(sta->addr));

	if (sm->last_recv_radius) {
		radius_msg_free(sm->last_recv_radius);
		free(sm->last_recv_radius);
		sm->last_recv_radius = NULL;
	}

	sm->eapSuccess = FALSE;
	sm->eapFail = FALSE;
}


int ieee802_1x_tx_status(hostapd *hapd, struct sta_info *sta, u8 *buf,
			 size_t len, int ack)
{
	struct ieee80211_hdr *hdr;
	struct ieee802_1x_hdr *xhdr;
	struct ieee802_1x_eapol_key *key;
	u8 *pos;

	if (sta == NULL)
		return -1;
	if (len < sizeof(*hdr) + sizeof(rfc1042_header) + 2 + sizeof(*xhdr))
		return 0;

	hdr = (struct ieee80211_hdr *) buf;
	pos = (u8 *) (hdr + 1);
	if (memcmp(pos, rfc1042_header, sizeof(rfc1042_header)) != 0)
		return 0;
	pos += sizeof(rfc1042_header);
	if (((pos[0] << 8) | pos[1]) != ETH_P_PAE)
		return 0;
	pos += 2;

	xhdr = (struct ieee802_1x_hdr *) pos;
	pos += sizeof(*xhdr);

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "IEEE 802.1X: " MACSTR
		      " TX status - version=%d type=%d length=%d - ack=%d\n",
		      MAC2STR(sta->addr), xhdr->version, xhdr->type,
		      ntohs(xhdr->length), ack);

	/* EAPOL EAP-Packet packets are eventually re-sent by either Supplicant
	 * or Authenticator state machines, but EAPOL-Key packets are not
	 * retransmitted in case of failure. Try to re-sent failed EAPOL-Key
	 * packets couple of times because otherwise STA keys become
	 * unsynchronized with AP. */
	if (xhdr->type == IEEE802_1X_TYPE_EAPOL_KEY && !ack &&
	    pos + sizeof(*key) <= buf + len) {
		key = (struct ieee802_1x_eapol_key *) pos;
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE8021X,
			       HOSTAPD_LEVEL_DEBUG, "did not Ack EAPOL-Key "
			       "frame (%scast index=%d)",
			       key->key_index & BIT(7) ? "uni" : "broad",
			       key->key_index & ~BIT(7));
		/* TODO: re-send EAPOL-Key couple of times (with short delay
		 * between them?). If all attempt fail, report error and
		 * deauthenticate STA so that it will get new keys when
		 * authenticating again (e.g., after returning in range).
		 * Separate limit/transmit state needed both for unicast and
		 * broadcast keys(?) */
	}
	/* TODO: could move unicast key configuration from ieee802_1x_tx_key()
	 * to here and change the key only if the EAPOL-Key packet was Acked.
	 */

	return 1;
}


u8 * ieee802_1x_get_identity(struct eapol_state_machine *sm, size_t *len)
{
	if (sm == NULL || sm->identity == NULL)
		return NULL;

	*len = sm->identity_len;
	return sm->identity;
}


u8 * ieee802_1x_get_radius_class(struct eapol_state_machine *sm, size_t *len)
{
	if (sm == NULL || sm->radius_class == NULL)
		return NULL;

	*len = sm->radius_class_len;
	return sm->radius_class;
}


u8 * ieee802_1x_get_key_crypt(struct eapol_state_machine *sm, size_t *len)
{
	if (sm == NULL)
		return NULL;

	*len = sm->eapol_key_crypt_len;
	return sm->eapol_key_crypt;
}


void ieee802_1x_notify_port_enabled(struct eapol_state_machine *sm,
				    int enabled)
{
	if (sm == NULL)
		return;
	sm->portEnabled = enabled ? TRUE : FALSE;
}


void ieee802_1x_notify_port_valid(struct eapol_state_machine *sm,
				  int valid)
{
	if (sm == NULL)
		return;
	sm->portValid = valid ? TRUE : FALSE;
}


void ieee802_1x_notify_pre_auth(struct eapol_state_machine *sm, int pre_auth)
{
	if (sm == NULL)
		return;
	if (pre_auth)
		sm->flags |= EAPOL_SM_PREAUTH;
	else
		sm->flags &= ~EAPOL_SM_PREAUTH;
}


static const char * bool_txt(Boolean bool)
{
	return bool ? "TRUE" : "FALSE";
}


int ieee802_1x_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	/* TODO */
	return 0;
}


int ieee802_1x_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen)
{
	int len = 0;
	struct eapol_state_machine *sm = sta->eapol_sm;

	if (sm == NULL)
		return 0;

	len += snprintf(buf + len, buflen - len,
			"dot1xPaePortNumber=%d\n"
			"dot1xPaePortProtocolVersion=%d\n"
			"dot1xPaePortCapabilities=1\n"
			"dot1xPaePortInitialize=%d\n"
			"dot1xPaePortReauthenticate=FALSE\n",
			sta->aid,
			EAPOL_VERSION,
			sm->initialize);

	/* dot1xAuthConfigTable */
	len += snprintf(buf + len, buflen - len,
			"dot1xAuthPaeState=%d\n"
			"dot1xAuthBackendAuthState=%d\n"
			"dot1xAuthAdminControlledDirections=%d\n"
			"dot1xAuthOperControlledDirections=%d\n"
			"dot1xAuthAuthControlledPortStatus=%d\n"
			"dot1xAuthAuthControlledPortControl=%d\n"
			"dot1xAuthQuietPeriod=%u\n"
			"dot1xAuthServerTimeout=%u\n"
			"dot1xAuthReAuthPeriod=%u\n"
			"dot1xAuthReAuthEnabled=%s\n"
			"dot1xAuthKeyTxEnabled=%s\n",
			sm->auth_pae.state + 1,
			sm->be_auth.state + 1,
			sm->ctrl_dir.adminControlledDirections,
			sm->ctrl_dir.operControlledDirections,
			sm->authPortStatus,
			sm->portControl,
			sm->auth_pae.quietPeriod,
			sm->be_auth.serverTimeout,
			sm->reauth_timer.reAuthPeriod,
			bool_txt(sm->reauth_timer.reAuthEnabled),
			bool_txt(sm->keyTxEnabled));

	/* dot1xAuthStatsTable */
	len += snprintf(buf + len, buflen - len,
			"dot1xAuthEapolFramesRx=%u\n"
			"dot1xAuthEapolFramesTx=%u\n"
			"dot1xAuthEapolStartFramesRx=%u\n"
			"dot1xAuthEapolLogoffFramesRx=%u\n"
			"dot1xAuthEapolRespIdFramesRx=%u\n"
			"dot1xAuthEapolRespFramesRx=%u\n"
			"dot1xAuthEapolReqIdFramesTx=%u\n"
			"dot1xAuthEapolReqFramesTx=%u\n"
			"dot1xAuthInvalidEapolFramesRx=%u\n"
			"dot1xAuthEapLengthErrorFramesRx=%u\n"
			"dot1xAuthLastEapolFrameVersion=%u\n"
			"dot1xAuthLastEapolFrameSource=" MACSTR "\n",
			sm->dot1xAuthEapolFramesRx,
			sm->dot1xAuthEapolFramesTx,
			sm->dot1xAuthEapolStartFramesRx,
			sm->dot1xAuthEapolLogoffFramesRx,
			sm->dot1xAuthEapolRespIdFramesRx,
			sm->dot1xAuthEapolRespFramesRx,
			sm->dot1xAuthEapolReqIdFramesTx,
			sm->dot1xAuthEapolReqFramesTx,
			sm->dot1xAuthInvalidEapolFramesRx,
			sm->dot1xAuthEapLengthErrorFramesRx,
			sm->dot1xAuthLastEapolFrameVersion,
			MAC2STR(sm->addr));

	/* dot1xAuthDiagTable */
	len += snprintf(buf + len, buflen - len,
			"dot1xAuthEntersConnecting=%u\n"
			"dot1xAuthEapLogoffsWhileConnecting=%u\n"
			"dot1xAuthEntersAuthenticating=%u\n"
			"dot1xAuthAuthSuccessesWhileAuthenticating=%u\n"
			"dot1xAuthAuthTimeoutsWhileAuthenticating=%u\n"
			"dot1xAuthAuthFailWhileAuthenticating=%u\n"
			"dot1xAuthAuthEapStartsWhileAuthenticating=%u\n"
			"dot1xAuthAuthEapLogoffWhileAuthenticating=%u\n"
			"dot1xAuthAuthReauthsWhileAuthenticated=%u\n"
			"dot1xAuthAuthEapStartsWhileAuthenticated=%u\n"
			"dot1xAuthAuthEapLogoffWhileAuthenticated=%u\n"
			"dot1xAuthBackendResponses=%u\n"
			"dot1xAuthBackendAccessChallenges=%u\n"
			"dot1xAuthBackendOtherRequestsToSupplicant=%u\n"
			"dot1xAuthBackendAuthSuccesses=%u\n"
			"dot1xAuthBackendAuthFails=%u\n",
			sm->auth_pae.authEntersConnecting,
			sm->auth_pae.authEapLogoffsWhileConnecting,
			sm->auth_pae.authEntersAuthenticating,
			sm->auth_pae.authAuthSuccessesWhileAuthenticating,
			sm->auth_pae.authAuthTimeoutsWhileAuthenticating,
			sm->auth_pae.authAuthFailWhileAuthenticating,
			sm->auth_pae.authAuthEapStartsWhileAuthenticating,
			sm->auth_pae.authAuthEapLogoffWhileAuthenticating,
			sm->auth_pae.authAuthReauthsWhileAuthenticated,
			sm->auth_pae.authAuthEapStartsWhileAuthenticated,
			sm->auth_pae.authAuthEapLogoffWhileAuthenticated,
			sm->be_auth.backendResponses,
			sm->be_auth.backendAccessChallenges,
			sm->be_auth.backendOtherRequestsToSupplicant,
			sm->be_auth.backendAuthSuccesses,
			sm->be_auth.backendAuthFails);

	/* dot1xAuthSessionStatsTable */
	len += snprintf(buf + len, buflen - len,
			/* TODO: dot1xAuthSessionOctetsRx */
			/* TODO: dot1xAuthSessionOctetsTx */
			/* TODO: dot1xAuthSessionFramesRx */
			/* TODO: dot1xAuthSessionFramesTx */
			"dot1xAuthSessionId=%08X-%08X\n"
			"dot1xAuthSessionAuthenticMethod=%d\n"
			"dot1xAuthSessionTime=%u\n"
			"dot1xAuthSessionTerminateCause=999\n"
			"dot1xAuthSessionUserName=%s\n",
			sta->acct_session_id_hi, sta->acct_session_id_lo,
			sta->wpa_key_mgmt == WPA_KEY_MGMT_IEEE8021X ? 1 : 2,
			(unsigned int) (time(NULL) - sta->acct_session_start),
			sm->identity);

	return len;
}
