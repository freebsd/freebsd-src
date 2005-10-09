/*
 * hostapd / EAP-MD5 server
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
#include "md5.h"


#define CHALLENGE_LEN 16

struct eap_md5_data {
	u8 challenge[CHALLENGE_LEN];
	enum { CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_md5_init(struct eap_sm *sm)
{
	struct eap_md5_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
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

	*reqDataLen = sizeof(*req) + 2 + CHALLENGE_LEN;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-MD5: Failed to allocate memory for "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_MD5;
	*pos++ = CHALLENGE_LEN;
	memcpy(pos, data->challenge, CHALLENGE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-MD5: Challenge", pos, CHALLENGE_LEN);

	data->state = CONTINUE;

	return (u8 *) req;
}


static Boolean eap_md5_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;
	size_t len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 2 || *pos != EAP_TYPE_MD5 ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-MD5: Invalid frame");
		return TRUE;
	}
	pos++;
	if (*pos != MD5_MAC_LEN ||
	    sizeof(*resp) + 2 + MD5_MAC_LEN > len) {
		wpa_printf(MSG_INFO, "EAP-MD5: Invalid response "
			   "(response_len=%d respDataLen=%lu",
			   *pos, (unsigned long) respDataLen);
		return TRUE;
	}

	return FALSE;
}


static void eap_md5_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_md5_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos;
	MD5_CTX context;
	u8 hash[MD5_MAC_LEN];

	if (sm->user == NULL || sm->user->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-MD5: Password not configured");
		data->state = FAILURE;
		return;
	}

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	pos += 2; /* Skip type and len */
	wpa_hexdump(MSG_MSGDUMP, "EAP-MD5: Response", pos, MD5_MAC_LEN);

	MD5Init(&context);
	MD5Update(&context, &resp->identifier, 1);
	MD5Update(&context, sm->user->password, sm->user->password_len);
	MD5Update(&context, data->challenge, CHALLENGE_LEN);
	MD5Final(hash, &context);

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


const struct eap_method eap_method_md5 =
{
	.method = EAP_TYPE_MD5,
	.name = "MD5",
	.init = eap_md5_init,
	.reset = eap_md5_reset,
	.buildReq = eap_md5_buildReq,
	.check = eap_md5_check,
	.process = eap_md5_process,
	.isDone = eap_md5_isDone,
	.isSuccess = eap_md5_isSuccess,
};
