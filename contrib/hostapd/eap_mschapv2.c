/*
 * hostapd / EAP-MSCHAPv2 (draft-kamath-pppext-eap-mschapv2-00.txt) server
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
#include <netinet/in.h>

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
#include "ms_funcs.h"


struct eap_mschapv2_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_MSCHAPV2 */
	u8 op_code; /* MSCHAPV2_OP_* */
	u8 mschapv2_id; /* must be changed for challenges, but not for
			 * success/failure */
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


#define CHALLENGE_LEN 16

struct eap_mschapv2_data {
	u8 auth_challenge[CHALLENGE_LEN];
	u8 auth_response[20];
	enum { CHALLENGE, SUCCESS_REQ, FAILURE_REQ, SUCCESS, FAILURE } state;
	u8 resp_mschapv2_id;
};


static void * eap_mschapv2_init(struct eap_sm *sm)
{
	struct eap_mschapv2_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = CHALLENGE;

	return data;
}


static void eap_mschapv2_reset(struct eap_sm *sm, void *priv)
{
	struct eap_mschapv2_data *data = priv;
	free(data);
}


static u8 * eap_mschapv2_build_challenge(struct eap_sm *sm,
					 struct eap_mschapv2_data *data,
					 int id, size_t *reqDataLen)
{
	struct eap_mschapv2_hdr *req;
	u8 *pos;
	char *name = "hostapd"; /* TODO: make this configurable */

	if (hostapd_get_rand(data->auth_challenge, CHALLENGE_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-MSCHAPV2: Failed to get random "
			   "data");
		data->state = FAILURE;
		return NULL;
	}

	*reqDataLen = sizeof(*req) + 1 + CHALLENGE_LEN + strlen(name);
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-MSCHAPV2: Failed to allocate memory"
			   " for request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_MSCHAPV2;
	req->op_code = MSCHAPV2_OP_CHALLENGE;
	req->mschapv2_id = id;
	req->ms_length[0] = (*reqDataLen - 5) >> 8;
	req->ms_length[1] = (*reqDataLen - 5) & 0xff;
	pos = (u8 *) (req + 1);
	*pos++ = CHALLENGE_LEN;
	memcpy(pos, data->auth_challenge, CHALLENGE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-MSCHAPV2: Challenge", pos,
		    CHALLENGE_LEN);
	pos += CHALLENGE_LEN;
	memcpy(pos, name, strlen(name));

	return (u8 *) req;
}


static u8 * eap_mschapv2_build_success_req(struct eap_sm *sm,
					   struct eap_mschapv2_data *data,
					   int id, size_t *reqDataLen)
{
	struct eap_mschapv2_hdr *req;
	u8 *pos, *msg, *end;
	char *message = "OK";
	size_t msg_len;
	int i;

	msg_len = 2 + 2 * sizeof(data->auth_response) + 3 + strlen(message);
	*reqDataLen = sizeof(*req) + msg_len;
	req = malloc(*reqDataLen + 1);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-MSCHAPV2: Failed to allocate memory"
			   " for request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_MSCHAPV2;
	req->op_code = MSCHAPV2_OP_SUCCESS;
	req->mschapv2_id = data->resp_mschapv2_id;
	req->ms_length[0] = (*reqDataLen - 5) >> 8;
	req->ms_length[1] = (*reqDataLen - 5) & 0xff;

	msg = pos = (u8 *) (req + 1);
	end = ((u8 *) req) + *reqDataLen + 1;

	pos += snprintf((char *) pos, end - pos, "S=");
	for (i = 0; i < sizeof(data->auth_response); i++) {
		pos += snprintf((char *) pos, end - pos, "%02X",
				data->auth_response[i]);
	}
	pos += snprintf((char *) pos, end - pos, " M=%s", message);

	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-MSCHAPV2: Success Request Message",
			  msg, msg_len);

	return (u8 *) req;
}


static u8 * eap_mschapv2_build_failure_req(struct eap_sm *sm,
					   struct eap_mschapv2_data *data,
					   int id, size_t *reqDataLen)
{
	struct eap_mschapv2_hdr *req;
	u8 *pos;
	char *message = "E=691 R=0 C=00000000000000000000000000000000 V=3 "
		"M=FAILED";
	size_t msg_len;

	msg_len = strlen(message);
	*reqDataLen = sizeof(*req) + msg_len;
	req = malloc(*reqDataLen + 1);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-MSCHAPV2: Failed to allocate memory"
			   " for request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_MSCHAPV2;
	req->op_code = MSCHAPV2_OP_FAILURE;
	req->mschapv2_id = data->resp_mschapv2_id;
	req->ms_length[0] = (*reqDataLen - 5) >> 8;
	req->ms_length[1] = (*reqDataLen - 5) & 0xff;

	pos = (u8 *) (req + 1);
	memcpy(pos, message, msg_len);

	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-MSCHAPV2: Failure Request Message",
			  (u8 *) message, msg_len);

	return (u8 *) req;
}


static u8 * eap_mschapv2_buildReq(struct eap_sm *sm, void *priv, int id,
				  size_t *reqDataLen)
{
	struct eap_mschapv2_data *data = priv;

	switch (data->state) {
	case CHALLENGE:
		return eap_mschapv2_build_challenge(sm, data, id, reqDataLen);
	case SUCCESS_REQ:
		return eap_mschapv2_build_success_req(sm, data, id,
						      reqDataLen);
	case FAILURE_REQ:
		return eap_mschapv2_build_failure_req(sm, data, id,
						      reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Unknown state %d in "
			   "buildReq", data->state);
		break;
	}
	return NULL;
}


static Boolean eap_mschapv2_check(struct eap_sm *sm, void *priv,
				  u8 *respData, size_t respDataLen)
{
	struct eap_mschapv2_data *data = priv;
	struct eap_mschapv2_hdr *resp;
	u8 *pos;
	size_t len;

	resp = (struct eap_mschapv2_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < 6 || resp->type != EAP_TYPE_MSCHAPV2 ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Invalid frame");
		return TRUE;
	}

	if (data->state == CHALLENGE &&
	    resp->op_code != MSCHAPV2_OP_RESPONSE) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Expected Response - "
			   "ignore op %d", resp->op_code);
		return TRUE;
	}

	if (data->state == SUCCESS_REQ &&
	    resp->op_code != MSCHAPV2_OP_SUCCESS &&
	    resp->op_code != MSCHAPV2_OP_FAILURE) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Expected Success or "
			   "Failure - ignore op %d", resp->op_code);
		return TRUE;
	}

	if (data->state == FAILURE_REQ &&
	    resp->op_code != MSCHAPV2_OP_FAILURE) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Expected Failure "
			   "- ignore op %d", resp->op_code);
		return TRUE;
	}

	return FALSE;
}


static void eap_mschapv2_process_response(struct eap_sm *sm,
					  struct eap_mschapv2_data *data,
					  u8 *respData, size_t respDataLen)
{
	struct eap_mschapv2_hdr *resp;
	u8 *pos;
	u8 *peer_challenge, *nt_response, flags, *name;
	size_t name_len;
	u8 expected[24];
	int i;
	u8 *username, *user;
	size_t username_len, user_len;

	resp = (struct eap_mschapv2_hdr *) respData;
	pos = (u8 *) (resp + 1);

	if (respDataLen < sizeof(resp) + 1 + 49 ||
	    resp->op_code != MSCHAPV2_OP_RESPONSE ||
	    pos[0] != 49) {
		wpa_hexdump(MSG_DEBUG, "EAP-MSCHAPV2: Invalid response",
			    respData, respDataLen);
		data->state = FAILURE;
		return;
	}
	data->resp_mschapv2_id = resp->mschapv2_id;
	pos++;
	peer_challenge = pos;
	pos += 16 + 8;
	nt_response = pos;
	pos += 24;
	flags = *pos++;
	name = pos;
	name_len = respData + respDataLen - name;

	wpa_hexdump(MSG_MSGDUMP, "EAP-MSCHAPV2: Peer-Challenge",
		    peer_challenge, 16);
	wpa_hexdump(MSG_MSGDUMP, "EAP-MSCHAPV2: NT-Response", nt_response, 24);
	wpa_printf(MSG_MSGDUMP, "EAP-MSCHAPV2: Flags 0x%x", flags);
	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-MSCHAPV2: Name", name, name_len);

	/* MSCHAPv2 does not include optional domain name in the
	 * challenge-response calculation, so remove domain prefix
	 * (if present). */
	username = sm->identity;
	username_len = sm->identity_len;
	for (i = 0; i < username_len; i++) {
		if (username[i] == '\\') {
			username_len -= i + 1;
			username += i + 1;
			break;
		}
	}

	user = name;
	user_len = name_len;
	for (i = 0; i < user_len; i++) {
		if (user[i] == '\\') {
			user_len -= i + 1;
			user += i + 1;
			break;
		}
	}

	if (username_len != user_len ||
	    memcmp(username, user, username_len) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Mismatch in user names");
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-MSCHAPV2: Expected user "
				  "name", username, username_len);
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-MSCHAPV2: Received user "
				  "name", user, user_len);
		data->state = FAILURE;
		return;
	}

	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-MSCHAPV2: User name",
			  username, username_len);

	generate_nt_response(data->auth_challenge, peer_challenge,
			     username, username_len,
			     sm->user->password, sm->user->password_len,
			     expected);

	if (memcmp(nt_response, expected, 24) == 0) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Correct NT-Response");
		data->state = SUCCESS_REQ;


		/* Authenticator response is not really needed yet, but
		 * calculate it here so that peer_challenge and username need
		 * not be saved. */
		generate_authenticator_response(sm->user->password,
						sm->user->password_len,
						peer_challenge,
						data->auth_challenge,
						username, username_len,
						nt_response,
						data->auth_response);
	} else {
		wpa_hexdump(MSG_MSGDUMP, "EAP-MSCHAPV2: Expected NT-Response",
			    expected, 24);
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Invalid NT-Response");
		data->state = FAILURE_REQ;
	}
}


static void eap_mschapv2_process_success_resp(struct eap_sm *sm,
					      struct eap_mschapv2_data *data,
					      u8 *respData, size_t respDataLen)
{
	struct eap_mschapv2_hdr *resp;

	resp = (struct eap_mschapv2_hdr *) respData;

	if (resp->op_code == MSCHAPV2_OP_SUCCESS) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Received Success Response"
			   " - authentication completed successfully");
		data->state = SUCCESS;
	} else {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Did not receive Success "
			   "Response - peer rejected authentication");
		data->state = FAILURE;
	}
}


static void eap_mschapv2_process_failure_resp(struct eap_sm *sm,
					      struct eap_mschapv2_data *data,
					      u8 *respData, size_t respDataLen)
{
	struct eap_mschapv2_hdr *resp;

	resp = (struct eap_mschapv2_hdr *) respData;

	if (resp->op_code == MSCHAPV2_OP_FAILURE) {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Received Failure Response"
			   " - authentication failed");
	} else {
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Did not receive Failure "
			   "Response - authentication failed");
	}

	data->state = FAILURE;
}


static void eap_mschapv2_process(struct eap_sm *sm, void *priv,
				 u8 *respData, size_t respDataLen)
{
	struct eap_mschapv2_data *data = priv;

	if (sm->user == NULL || sm->user->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Password not configured");
		data->state = FAILURE;
		return;
	}

	switch (data->state) {
	case CHALLENGE:
		eap_mschapv2_process_response(sm, data, respData, respDataLen);
		break;
	case SUCCESS_REQ:
		eap_mschapv2_process_success_resp(sm, data, respData,
						  respDataLen);
		break;
	case FAILURE_REQ:
		eap_mschapv2_process_failure_resp(sm, data, respData,
						  respDataLen);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-MSCHAPV2: Unknown state %d in "
			   "process", data->state);
		break;
	}
}


static Boolean eap_mschapv2_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_mschapv2_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static Boolean eap_mschapv2_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_mschapv2_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_mschapv2 =
{
	.method = EAP_TYPE_MSCHAPV2,
	.name = "MSCHAPV2",
	.init = eap_mschapv2_init,
	.reset = eap_mschapv2_reset,
	.buildReq = eap_mschapv2_buildReq,
	.check = eap_mschapv2_check,
	.process = eap_mschapv2_process,
	.isDone = eap_mschapv2_isDone,
	.isSuccess = eap_mschapv2_isSuccess,
};
