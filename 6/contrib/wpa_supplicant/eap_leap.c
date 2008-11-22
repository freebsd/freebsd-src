/*
 * WPA Supplicant / EAP-LEAP
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <string.h>

#include "common.h"
#include "eap_i.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "ms_funcs.h"
#include "crypto.h"

#define LEAP_VERSION 1
#define LEAP_CHALLENGE_LEN 8
#define LEAP_RESPONSE_LEN 24
#define LEAP_KEY_LEN 16


struct eap_leap_data {
	enum {
		LEAP_WAIT_CHALLENGE,
		LEAP_WAIT_SUCCESS,
		LEAP_WAIT_RESPONSE,
		LEAP_DONE
	} state;

	u8 peer_challenge[LEAP_CHALLENGE_LEN];
	u8 peer_response[LEAP_RESPONSE_LEN];

	u8 ap_challenge[LEAP_CHALLENGE_LEN];
	u8 ap_response[LEAP_RESPONSE_LEN];
};


static void * eap_leap_init(struct eap_sm *sm)
{
	struct eap_leap_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));
	data->state = LEAP_WAIT_CHALLENGE;

	sm->leap_done = FALSE;
	return data;
}


static void eap_leap_deinit(struct eap_sm *sm, void *priv)
{
	free(priv);
}


static u8 * eap_leap_process_request(struct eap_sm *sm, void *priv,
				     struct eap_method_ret *ret,
				     const u8 *reqData, size_t reqDataLen,
				     size_t *respDataLen)
{
	struct eap_leap_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	const struct eap_hdr *req;
	struct eap_hdr *resp;
	const u8 *pos, *challenge;
	u8 challenge_len, *rpos;

	wpa_printf(MSG_DEBUG, "EAP-LEAP: Processing EAP-Request");

	req = (const struct eap_hdr *) reqData;
	pos = (const u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 4 || *pos != EAP_TYPE_LEAP) {
		wpa_printf(MSG_INFO, "EAP-LEAP: Invalid EAP-Request frame");
		ret->ignore = TRUE;
		return NULL;
	}
	pos++;

	if (*pos != LEAP_VERSION) {
		wpa_printf(MSG_WARNING, "EAP-LEAP: Unsupported LEAP version "
			   "%d", *pos);
		ret->ignore = TRUE;
		return NULL;
	}
	pos++;

	pos++; /* skip unused byte */

	challenge_len = *pos++;
	if (challenge_len != LEAP_CHALLENGE_LEN ||
	    challenge_len > reqDataLen - sizeof(*req) - 4) {
		wpa_printf(MSG_INFO, "EAP-LEAP: Invalid challenge "
			   "(challenge_len=%d reqDataLen=%lu",
			   challenge_len, (unsigned long) reqDataLen);
		ret->ignore = TRUE;
		return NULL;
	}
	challenge = pos;
	memcpy(data->peer_challenge, challenge, LEAP_CHALLENGE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-LEAP: Challenge from AP",
		    challenge, LEAP_CHALLENGE_LEN);

	wpa_printf(MSG_DEBUG, "EAP-LEAP: Generating Challenge Response");

	*respDataLen = sizeof(struct eap_hdr) + 1 + 3 + LEAP_RESPONSE_LEN +
		config->identity_len;
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(*respDataLen);
	rpos = (u8 *) (resp + 1);
	*rpos++ = EAP_TYPE_LEAP;
	*rpos++ = LEAP_VERSION;
	*rpos++ = 0; /* unused */
	*rpos++ = LEAP_RESPONSE_LEN;
	nt_challenge_response(challenge,
			      config->password, config->password_len, rpos);
	memcpy(data->peer_response, rpos, LEAP_RESPONSE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-LEAP: Response",
		    rpos, LEAP_RESPONSE_LEN);
	rpos += LEAP_RESPONSE_LEN;
	memcpy(rpos, config->identity, config->identity_len);

	data->state = LEAP_WAIT_SUCCESS;

	return (u8 *) resp;
}


static u8 * eap_leap_process_success(struct eap_sm *sm, void *priv,
				     struct eap_method_ret *ret,
				     const u8 *reqData, size_t reqDataLen,
				     size_t *respDataLen)
{
	struct eap_leap_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	const struct eap_hdr *req;
	struct eap_hdr *resp;
	u8 *pos;

	wpa_printf(MSG_DEBUG, "EAP-LEAP: Processing EAP-Success");

	if (data->state != LEAP_WAIT_SUCCESS) {
		wpa_printf(MSG_INFO, "EAP-LEAP: EAP-Success received in "
			   "unexpected state (%d) - ignored", data->state);
		ret->ignore = TRUE;
		return NULL;
	}

	req = (const struct eap_hdr *) reqData;

	*respDataLen = sizeof(struct eap_hdr) + 1 + 3 + LEAP_CHALLENGE_LEN +
		config->identity_len;
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	resp->code = EAP_CODE_REQUEST;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(*respDataLen);
	pos = (u8 *) (resp + 1);
	*pos++ = EAP_TYPE_LEAP;
	*pos++ = LEAP_VERSION;
	*pos++ = 0; /* unused */
	*pos++ = LEAP_CHALLENGE_LEN;
	if (hostapd_get_rand(pos, LEAP_CHALLENGE_LEN)) {
		wpa_printf(MSG_WARNING, "EAP-LEAP: Failed to read random data "
			   "for challenge");
		free(resp);
		ret->ignore = TRUE;
		return NULL;
	}
	memcpy(data->ap_challenge, pos, LEAP_CHALLENGE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-LEAP: Challenge to AP/AS", pos,
		    LEAP_CHALLENGE_LEN);
	pos += LEAP_CHALLENGE_LEN;
	memcpy(pos, config->identity, config->identity_len);

	data->state = LEAP_WAIT_RESPONSE;

	return (u8 *) resp;
}


static u8 * eap_leap_process_response(struct eap_sm *sm, void *priv,
				      struct eap_method_ret *ret,
				      const u8 *reqData, size_t reqDataLen,
				      size_t *respDataLen)
{
	struct eap_leap_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	const struct eap_hdr *resp;
	const u8 *pos;
	u8 response_len, pw_hash[16], pw_hash_hash[16],
		expected[LEAP_RESPONSE_LEN];

	wpa_printf(MSG_DEBUG, "EAP-LEAP: Processing EAP-Response");

	resp = (const struct eap_hdr *) reqData;
	pos = (const u8 *) (resp + 1);
	if (reqDataLen < sizeof(*resp) + 4 || *pos != EAP_TYPE_LEAP) {
		wpa_printf(MSG_INFO, "EAP-LEAP: Invalid EAP-Response frame");
		ret->ignore = TRUE;
		return NULL;
	}
	pos++;

	if (*pos != LEAP_VERSION) {
		wpa_printf(MSG_WARNING, "EAP-LEAP: Unsupported LEAP version "
			   "%d", *pos);
		ret->ignore = TRUE;
		return NULL;
	}
	pos++;

	pos++; /* skip unused byte */

	response_len = *pos++;
	if (response_len != LEAP_RESPONSE_LEN ||
	    response_len > reqDataLen - sizeof(*resp) - 4) {
		wpa_printf(MSG_INFO, "EAP-LEAP: Invalid response "
			   "(response_len=%d reqDataLen=%lu",
			   response_len, (unsigned long) reqDataLen);
		ret->ignore = TRUE;
		return NULL;
	}

	wpa_hexdump(MSG_DEBUG, "EAP-LEAP: Response from AP",
		    pos, LEAP_RESPONSE_LEN);
	memcpy(data->ap_response, pos, LEAP_RESPONSE_LEN);

	nt_password_hash(config->password, config->password_len, pw_hash);
	hash_nt_password_hash(pw_hash, pw_hash_hash);
	challenge_response(data->ap_challenge, pw_hash_hash, expected);

	ret->methodState = METHOD_DONE;
	ret->allowNotifications = FALSE;

	if (memcmp(pos, expected, LEAP_RESPONSE_LEN) != 0) {
		wpa_printf(MSG_WARNING, "EAP-LEAP: AP sent an invalid "
			   "response - authentication failed");
		wpa_hexdump(MSG_DEBUG, "EAP-LEAP: Expected response from AP",
			    expected, LEAP_RESPONSE_LEN);
		ret->decision = DECISION_FAIL;
		return NULL;
	}

	ret->decision = DECISION_UNCOND_SUCC;

	/* LEAP is somewhat odd method since it sends EAP-Success in the middle
	 * of the authentication. Use special variable to transit EAP state
	 * machine to SUCCESS state. */
	sm->leap_done = TRUE;
	data->state = LEAP_DONE;

	/* No more authentication messages expected; AP will send EAPOL-Key
	 * frames if encryption is enabled. */
	return NULL;
}


static u8 * eap_leap_process(struct eap_sm *sm, void *priv,
			     struct eap_method_ret *ret,
			     const u8 *reqData, size_t reqDataLen,
			     size_t *respDataLen)
{
	struct wpa_ssid *config = eap_get_config(sm);
	const struct eap_hdr *eap;
	size_t len;

	if (config == NULL || config->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-LEAP: Password not configured");
		eap_sm_request_password(sm, config);
		ret->ignore = TRUE;
		return NULL;
	}

	eap = (const struct eap_hdr *) reqData;

	if (reqDataLen < sizeof(*eap) ||
	    (len = be_to_host16(eap->length)) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-LEAP: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}

	ret->ignore = FALSE;
	ret->allowNotifications = TRUE;
	ret->methodState = METHOD_MAY_CONT;
	ret->decision = DECISION_FAIL;

	sm->leap_done = FALSE;

	switch (eap->code) {
	case EAP_CODE_REQUEST:
		return eap_leap_process_request(sm, priv, ret, reqData, len,
						respDataLen);
	case EAP_CODE_SUCCESS:
		return eap_leap_process_success(sm, priv, ret, reqData, len,
						respDataLen);
	case EAP_CODE_RESPONSE:
		return eap_leap_process_response(sm, priv, ret, reqData, len,
						 respDataLen);
	default:
		wpa_printf(MSG_INFO, "EAP-LEAP: Unexpected EAP code (%d) - "
			   "ignored", eap->code);
		ret->ignore = TRUE;
		return NULL;
	}
}


static Boolean eap_leap_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_leap_data *data = priv;
	return data->state == LEAP_DONE;
}


static u8 * eap_leap_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_leap_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *key, pw_hash_hash[16], pw_hash[16];
	const u8 *addr[5];
	size_t elen[5];

	if (data->state != LEAP_DONE)
		return NULL;

	key = malloc(LEAP_KEY_LEN);
	if (key == NULL)
		return NULL;

	nt_password_hash(config->password, config->password_len, pw_hash);
	hash_nt_password_hash(pw_hash, pw_hash_hash);
	wpa_hexdump_key(MSG_DEBUG, "EAP-LEAP: pw_hash_hash",
			pw_hash_hash, 16);
	wpa_hexdump(MSG_DEBUG, "EAP-LEAP: peer_challenge",
		    data->peer_challenge, LEAP_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-LEAP: peer_response",
		    data->peer_response, LEAP_RESPONSE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-LEAP: ap_challenge",
		    data->ap_challenge, LEAP_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-LEAP: ap_response",
		    data->ap_response, LEAP_RESPONSE_LEN);

	addr[0] = pw_hash_hash;
	elen[0] = 16;
	addr[1] = data->ap_challenge;
	elen[1] = LEAP_CHALLENGE_LEN;
	addr[2] = data->ap_response;
	elen[2] = LEAP_RESPONSE_LEN;
	addr[3] = data->peer_challenge;
	elen[3] = LEAP_CHALLENGE_LEN;
	addr[4] = data->peer_response;
	elen[4] = LEAP_RESPONSE_LEN;
	md5_vector(5, addr, elen, key);
	wpa_hexdump_key(MSG_DEBUG, "EAP-LEAP: master key", key, LEAP_KEY_LEN);
	*len = LEAP_KEY_LEN;

	return key;
}


const struct eap_method eap_method_leap =
{
	.method = EAP_TYPE_LEAP,
	.name = "LEAP",
	.init = eap_leap_init,
	.deinit = eap_leap_deinit,
	.process = eap_leap_process,
	.isKeyAvailable = eap_leap_isKeyAvailable,
	.getKey = eap_leap_getKey,
};
