/*
 * hostapd / EAP-MD5 server
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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

#include "includes.h"

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
#include "md5.h"
#include "crypto.h"


#define CHALLENGE_LEN 16

struct eap_md5_data {
	u8 challenge[CHALLENGE_LEN];
	enum { CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_md5_init(struct eap_sm *sm)
{
	struct eap_md5_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = CONTINUE;

	return data;
}


static void eap_md5_reset(struct eap_sm *sm, void *priv)
{
	struct eap_md5_data *data = priv;
	free(data);
}


static u8 * eap_md5_buildReq(struct eap_sm *sm, void *priv, int id,
			     size_t *reqDataLen)
{
	struct eap_md5_data *data = priv;
	struct eap_hdr *req;
	u8 *pos;

	if (hostapd_get_rand(data->challenge, CHALLENGE_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-MD5: Failed to get random data");
		data->state = FAILURE;
		return NULL;
	}

	req = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_MD5, reqDataLen,
			    1 + CHALLENGE_LEN, EAP_CODE_REQUEST, id, &pos);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-MD5: Failed to allocate memory for "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	*pos++ = CHALLENGE_LEN;
	memcpy(pos, data->challenge, CHALLENGE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-MD5: Challenge", pos, CHALLENGE_LEN);

	data->state = CONTINUE;

	return (u8 *) req;
}


static Boolean eap_md5_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_MD5,
			       respData, respDataLen, &len);
	if (pos == NULL || len < 1) {
		wpa_printf(MSG_INFO, "EAP-MD5: Invalid frame");
		return TRUE;
	}
	if (*pos != MD5_MAC_LEN || 1 + MD5_MAC_LEN > len) {
		wpa_printf(MSG_INFO, "EAP-MD5: Invalid response "
			   "(response_len=%d payload_len=%lu",
			   *pos, (unsigned long) len);
		return TRUE;
	}

	return FALSE;
}


static void eap_md5_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_md5_data *data = priv;
	struct eap_hdr *resp;
	const u8 *pos;
	const u8 *addr[3];
	size_t len[3], plen;
	u8 hash[MD5_MAC_LEN];

	if (sm->user == NULL || sm->user->password == NULL ||
	    sm->user->password_hash) {
		wpa_printf(MSG_INFO, "EAP-MD5: Plaintext password not "
			   "configured");
		data->state = FAILURE;
		return;
	}

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_MD5,
			       respData, respDataLen, &plen);
	if (pos == NULL || *pos != MD5_MAC_LEN || plen < 1 + MD5_MAC_LEN)
		return; /* Should not happen - frame already validated */

	pos++; /* Skip response len */
	wpa_hexdump(MSG_MSGDUMP, "EAP-MD5: Response", pos, MD5_MAC_LEN);

	resp = (struct eap_hdr *) respData;
	addr[0] = &resp->identifier;
	len[0] = 1;
	addr[1] = sm->user->password;
	len[1] = sm->user->password_len;
	addr[2] = data->challenge;
	len[2] = CHALLENGE_LEN;
	md5_vector(3, addr, len, hash);

	if (memcmp(hash, pos, MD5_MAC_LEN) == 0) {
		wpa_printf(MSG_DEBUG, "EAP-MD5: Done - Success");
		data->state = SUCCESS;
	} else {
		wpa_printf(MSG_DEBUG, "EAP-MD5: Done - Failure");
		data->state = FAILURE;
	}
}


static Boolean eap_md5_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_md5_data *data = priv;
	return data->state != CONTINUE;
}


static Boolean eap_md5_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_md5_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_md5_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_MD5, "MD5");
	if (eap == NULL)
		return -1;

	eap->init = eap_md5_init;
	eap->reset = eap_md5_reset;
	eap->buildReq = eap_md5_buildReq;
	eap->check = eap_md5_check;
	eap->process = eap_md5_process;
	eap->isDone = eap_md5_isDone;
	eap->isSuccess = eap_md5_isSuccess;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
