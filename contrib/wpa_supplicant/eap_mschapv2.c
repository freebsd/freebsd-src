/*
 * WPA Supplicant / EAP-MSCHAPV2 (draft-kamath-pppext-eap-mschapv2-00.txt)
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


struct eap_mschapv2_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_MSCHAPV2 */
	u8 op_code; /* MSCHAPV2_OP_* */
	u8 mschapv2_id; /* usually same as identifier */
	u8 ms_length[2]; /* Note: misaligned; length - 5 */
	/* followed by data */
} __attribute__ ((packed));

#define MSCHAPV2_OP_CHALLENGE 1
#define MSCHAPV2_OP_RESPONSE 2
#define MSCHAPV2_OP_SUCCESS 3
#define MSCHAPV2_OP_FAILURE 4
#define MSCHAPV2_OP_CHANGE_PASSWORD 7

#define MSCHAPV2_RESP_LEN 49

#define ERROR_RESTRICTED_LOGON_HOURS 646
#define ERROR_ACCT_DISABLED 647
#define ERROR_PASSWD_EXPIRED 648
#define ERROR_NO_DIALIN_PERMISSION 649
#define ERROR_AUTHENTICATION_FAILURE 691
#define ERROR_CHANGING_PASSWORD 709

#define PASSWD_CHANGE_CHAL_LEN 16
#define MSCHAPV2_KEY_LEN 16


struct eap_mschapv2_data {
	u8 auth_response[20];
	int auth_response_valid;

	int prev_error;
	u8 passwd_change_challenge[PASSWD_CHANGE_CHAL_LEN];
	int passwd_change_challenge_valid;
	int passwd_change_version;

	/* Optional challenge values generated in EAP-FAST Phase 1 negotiation
	 */
	u8 *peer_challenge;
	u8 *auth_challenge;

	int phase2;
	u8 master_key[16];
	int master_key_valid;
	int success;
};


static void eap_mschapv2_deinit(struct eap_sm *sm, void *priv);


static void * eap_mschapv2_init(struct eap_sm *sm)
{
	struct eap_mschapv2_data *data;
	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));

	if (sm->peer_challenge) {
		data->peer_challenge = malloc(16);
		if (data->peer_challenge == NULL) {
			eap_mschapv2_deinit(sm, data);
			return NULL;
		}
		memcpy(data->peer_challenge, sm->peer_challenge, 16);
	}

	if (sm->auth_challenge) {
		data->auth_challenge = malloc(16);
		if (data->auth_challenge == NULL) {
			eap_mschapv2_deinit(sm, data);
			return NULL;
		}
		memcpy(data->auth_challenge, sm->auth_challenge, 16);
	}

	data->phase2 = sm->init_phase2;

	return data;
}


static void eap_mschapv2_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_mschapv2_data *data = priv;
	free(data->peer_challenge);
	free(data->auth_challenge);
	free(data);
}


static u8 * eap_mschapv2_challenge(struct eap_sm *sm,
				   struct eap_mschapv2_data *data,
				   struct eap_method_ret *ret,
				   struct eap_mschapv2_hdr *req,
				   size_t *respDataLen)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *challenge, *peer_challenge, *username, *pos;
	int i, ms_len;
	size_t len, challenge_len, username_len;
	struct eap_mschapv2_hdr *resp;
	u8 password_hash[16], password_hash_hash[16];

	/* MSCHAPv2 does not include optional domain name in the
	 * challenge-response calculation, so remove domain prefix
	 * (if present). */
	username = config->identity;
	username_len = config->identity_len;
	for (i = 0; i < username_len; i++) {
		if (username[i] == '\\') {
			username_len -= i + 1;
			username += i + 1;
			break;
		}
	}

	wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Received challenge");
	len = be_to_host16(req->length);
	pos = (u8 *) (req + 1);
	challenge_len = *pos++;
	if (challenge_len != 16) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Invalid challenge length "
			   "%d", challenge_len);
		ret->ignore = TRUE;
		return NULL;
	}

	if (len < 10 || len - 10 < challenge_len) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Too short challenge"
			   " packet: len=%lu challenge_len=%d",
			   (unsigned long) len, challenge_len);
		ret->ignore = TRUE;
		return NULL;
	}

	challenge = pos;
	pos += challenge_len;
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-MSCHAPV2: Authentication Servername",
		    pos, len - challenge_len - 10);

	ret->ignore = FALSE;
	ret->methodState = METHOD_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Generating Challenge Response");

	*respDataLen = sizeof(*resp) + 1 + MSCHAPV2_RESP_LEN +
		config->identity_len;
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	memset(resp, 0, *respDataLen);
	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(*respDataLen);
	resp->type = EAP_TYPE_MSCHAPV2;
	resp->op_code = MSCHAPV2_OP_RESPONSE;
	resp->mschapv2_id = req->mschapv2_id;
	ms_len = *respDataLen - 5;
	resp->ms_length[0] = ms_len >> 8;
	resp->ms_length[1] = ms_len & 0xff;
	pos = (u8 *) (resp + 1);
	*pos++ = MSCHAPV2_RESP_LEN; /* Value-Size */

	/* Response */
	peer_challenge = pos;
	if (data->peer_challenge) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: peer_challenge generated "
			   "in Phase 1");
		peer_challenge = data->peer_challenge;
	} else if (hostapd_get_rand(peer_challenge, 16)) {
		free(resp);
		return NULL;
	}
	pos += 16;
	pos += 8; /* Reserved, must be zero */
	if (data->auth_challenge) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: auth_challenge generated "
			   "in Phase 1");
		challenge = data->auth_challenge;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-MSCHAPV2: auth_challenge", challenge, 16);
	wpa_hexdump(MSG_DEBUG, "EAP-MSCHAPV2: peer_challenge",
		    peer_challenge, 16);
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-MSCHAPV2: username",
			  username, username_len);
	wpa_hexdump_ascii_key(MSG_DEBUG, "EAP-MSCHAPV2: password",
			      config->password, config->password_len);
	generate_nt_response(challenge, peer_challenge,
			     username, username_len,
			     config->password, config->password_len,
			     pos);
	wpa_hexdump(MSG_DEBUG, "EAP-MSCHAPV2: response", pos, 24);
	/* Authenticator response is not really needed yet, but calculate it
	 * here so that challenges need not be saved. */
	generate_authenticator_response(config->password, config->password_len,
					peer_challenge, challenge,
					username, username_len, pos,
					data->auth_response);
	data->auth_response_valid = 1;

	/* Likewise, generate master_key here since we have the needed data
	 * available. */
	nt_password_hash(config->password, config->password_len,
			 password_hash);
	hash_nt_password_hash(password_hash, password_hash_hash);
	get_master_key(password_hash_hash, pos /* nt_response */,
		       data->master_key);
	data->master_key_valid = 1;

	pos += 24;
	pos++; /* Flag / reserved, must be zero */

	memcpy(pos, config->identity, config->identity_len);
	return (u8 *) resp;
}


static u8 * eap_mschapv2_success(struct eap_sm *sm,
				 struct eap_mschapv2_data *data,
				 struct eap_method_ret *ret,
				 struct eap_mschapv2_hdr *req,
				 size_t *respDataLen)
{
	struct eap_mschapv2_hdr *resp;
	u8 *pos, recv_response[20];
	int len, left;

	wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Received success");
	len = be_to_host16(req->length);
	pos = (u8 *) (req + 1);
	if (!data->auth_response_valid || len < sizeof(*req) + 42 ||
	    pos[0] != 'S' || pos[1] != '=' ||
	    hexstr2bin((char *) (pos + 2), recv_response, 20) ||
	    memcmp(data->auth_response, recv_response, 20) != 0) {
		wpa_printf(MSG_WARNING, "EAP-MSCHAPV2: Invalid authenticator "
			   "response in success request");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
	pos += 42;
	left = len - sizeof(*req) - 42;
	while (left > 0 && *pos == ' ') {
		pos++;
		left--;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-MSCHAPV2: Success message",
			  pos, left);
	wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Authentication succeeded");
	*respDataLen = 6;
	resp = malloc(6);
	if (resp == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Failed to allocate "
			   "buffer for success response");
		ret->ignore = TRUE;
		return NULL;
	}

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(6);
	resp->type = EAP_TYPE_MSCHAPV2;
	resp->op_code = MSCHAPV2_OP_SUCCESS;

	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_UNCOND_SUCC;
	ret->allowNotifications = FALSE;
	data->success = 1;

	return (u8 *) resp;
}


static int eap_mschapv2_failure_txt(struct eap_sm *sm,
				    struct eap_mschapv2_data *data, char *txt)
{
	char *pos, *msg = "";
	int retry = 1;
	struct wpa_ssid *config = eap_get_config(sm);

	/* For example:
	 * E=691 R=1 C=<32 octets hex challenge> V=3 M=Authentication Failure
	 */

	pos = txt;

	if (pos && strncmp(pos, "E=", 2) == 0) {
		pos += 2;
		data->prev_error = atoi(pos);
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: error %d",
			   data->prev_error);
		pos = strchr(pos, ' ');
		if (pos)
			pos++;
	}

	if (pos && strncmp(pos, "R=", 2) == 0) {
		pos += 2;
		retry = atoi(pos);
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: retry is %sallowed",
			   retry == 1 ? "" : "not ");
		pos = strchr(pos, ' ');
		if (pos)
			pos++;
	}

	if (pos && strncmp(pos, "C=", 2) == 0) {
		int hex_len;
		pos += 2;
		hex_len = strchr(pos, ' ') - (char *) pos;
		if (hex_len == PASSWD_CHANGE_CHAL_LEN * 2) {
			if (hexstr2bin(pos, data->passwd_change_challenge,
				       PASSWD_CHANGE_CHAL_LEN)) {
				wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: invalid "
					   "failure challenge");
			} else {
				wpa_hexdump(MSG_DEBUG, "EAP-MSCHAPV2: failure "
					    "challenge",
					    data->passwd_change_challenge,
					    PASSWD_CHANGE_CHAL_LEN);
				data->passwd_change_challenge_valid = 1;
			}
		} else {
			wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: invalid failure "
				   "challenge len %d", hex_len);
		}
		pos = strchr(pos, ' ');
		if (pos)
			pos++;
	} else {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: required challenge field "
			   "was not present in failure message");
	}

	if (pos && strncmp(pos, "V=", 2) == 0) {
		pos += 2;
		data->passwd_change_version = atoi(pos);
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: password changing "
			   "protocol version %d", data->passwd_change_version);
		pos = strchr(pos, ' ');
		if (pos)
			pos++;
	}

	if (pos && strncmp(pos, "M=", 2) == 0) {
		pos += 2;
		msg = pos;
	}
	wpa_msg(sm->msg_ctx, MSG_WARNING,
		"EAP-MSCHAPV2: failure message: '%s' (retry %sallowed, error "
		"%d)",
		msg, retry == 1 ? "" : "not ", data->prev_error);
	if (retry == 1 && config) {
		/* TODO: could prevent the current password from being used
		 * again at least for some period of time */
		eap_sm_request_identity(sm, config);
		eap_sm_request_password(sm, config);
	} else if (config) {
		/* TODO: prevent retries using same username/password */
	}

	return retry == 1;
}


static u8 * eap_mschapv2_failure(struct eap_sm *sm,
				 struct eap_mschapv2_data *data,
				 struct eap_method_ret *ret,
				 struct eap_mschapv2_hdr *req,
				 size_t *respDataLen)
{
	struct eap_mschapv2_hdr *resp;
	u8 *msdata = (u8 *) (req + 1);
	char *buf;
	int len = be_to_host16(req->length) - sizeof(*req);
	int retry = 0;

	wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Received failure");
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-MSCHAPV2: Failure data",
			  msdata, len);
	buf = malloc(len + 1);
	if (buf) {
		memcpy(buf, msdata, len);
		buf[len] = '\0';
		retry = eap_mschapv2_failure_txt(sm, data, buf);
		free(buf);
	}

	ret->ignore = FALSE;
	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = FALSE;

	if (retry) {
		/* TODO: could try to retry authentication, e.g, after having
		 * changed the username/password. In this case, EAP MS-CHAP-v2
		 * Failure Response would not be sent here. */
	}

	*respDataLen = 6;
	resp = malloc(6);
	if (resp == NULL) {
		return NULL;
	}

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(6);
	resp->type = EAP_TYPE_MSCHAPV2;
	resp->op_code = MSCHAPV2_OP_FAILURE;

	return (u8 *) resp;
}


static u8 * eap_mschapv2_process(struct eap_sm *sm, void *priv,
				 struct eap_method_ret *ret,
				 u8 *reqData, size_t reqDataLen,
				 size_t *respDataLen)
{
	struct eap_mschapv2_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_mschapv2_hdr *req;
	int ms_len, len;

	if (config == NULL || config->identity == NULL) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Identity not configured");
		eap_sm_request_identity(sm, config);
		ret->ignore = TRUE;
		return NULL;
	}

	if (config->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Password not configured");
		eap_sm_request_password(sm, config);
		ret->ignore = TRUE;
		return NULL;
	}

	req = (struct eap_mschapv2_hdr *) reqData;
	len = be_to_host16(req->length);
	if (len < sizeof(*req) + 2 || req->type != EAP_TYPE_MSCHAPV2 ||
	    len > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}

	ms_len = ((int) req->ms_length[0] << 8) | req->ms_length[1];
	if (ms_len != len - 5) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Invalid header: len=%d "
			   "ms_len=%d", len, ms_len);
		if (sm->workaround) {
			/* Some authentication servers use invalid ms_len,
			 * ignore it for interoperability. */
			wpa_printf(MSG_INFO, "EAP-MSCHAPV2: workaround, ignore"
				   " invalid ms_len");
		} else {
			ret->ignore = TRUE;
			return NULL;
		}
	}

	switch (req->op_code) {
	case MSCHAPV2_OP_CHALLENGE:
		return eap_mschapv2_challenge(sm, data, ret, req, respDataLen);
	case MSCHAPV2_OP_SUCCESS:
		return eap_mschapv2_success(sm, data, ret, req, respDataLen);
	case MSCHAPV2_OP_FAILURE:
		return eap_mschapv2_failure(sm, data, ret, req, respDataLen);
	default:
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Unknown op %d - ignored",
			   req->op_code);
		ret->ignore = TRUE;
		return NULL;
	}
}


static Boolean eap_mschapv2_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_mschapv2_data *data = priv;
	return data->success && data->master_key_valid;
}


static u8 * eap_mschapv2_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_mschapv2_data *data = priv;
	u8 *key;
	int key_len;

	if (!data->master_key_valid || !data->success)
		return NULL;

	if (data->peer_challenge) {
		/* EAP-FAST needs both send and receive keys */
		key_len = 2 * MSCHAPV2_KEY_LEN;
	} else {
		key_len = MSCHAPV2_KEY_LEN;
	}

	key = malloc(key_len);
	if (key == NULL)
		return NULL;

	if (data->peer_challenge) {
		get_asymetric_start_key(data->master_key, key,
					MSCHAPV2_KEY_LEN, 0, 0);
		get_asymetric_start_key(data->master_key,
					key + MSCHAPV2_KEY_LEN,
					MSCHAPV2_KEY_LEN, 1, 0);
	} else {
		get_asymetric_start_key(data->master_key, key,
					MSCHAPV2_KEY_LEN, 1, 0);
	}

	wpa_hexdump_key(MSG_DEBUG, "EAP-MSCHAPV2: Derived key",
			key, key_len);

	*len = key_len;
	return key;
}


const struct eap_method eap_method_mschapv2 =
{
	.method = EAP_TYPE_MSCHAPV2,
	.name = "MSCHAPV2",
	.init = eap_mschapv2_init,
	.deinit = eap_mschapv2_deinit,
	.process = eap_mschapv2_process,
	.isKeyAvailable = eap_mschapv2_isKeyAvailable,
	.getKey = eap_mschapv2_getKey,
};
