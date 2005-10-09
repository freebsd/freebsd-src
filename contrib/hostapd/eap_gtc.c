/*
 * hostapd / EAP-GTC (RFC 3748)
 * Copyright (c) 2004, Jouni Malinen <jkmaline@cc.hut.fi>
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


struct eap_gtc_data {
	enum { CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_gtc_init(struct eap_sm *sm)
{
	struct eap_gtc_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = CONTINUE;

	return data;
}


static void eap_gtc_reset(struct eap_sm *sm, void *priv)
{
	struct eap_gtc_data *data = priv;
	free(data);
}


static u8 * eap_gtc_buildReq(struct eap_sm *sm, void *priv, int id,
			     size_t *reqDataLen)
{
	struct eap_gtc_data *data = priv;
	struct eap_hdr *req;
	u8 *pos;
	char *msg = "Password";
	size_t msg_len;

	msg_len = strlen(msg);
	*reqDataLen = sizeof(*req) + 1 + msg_len;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-GTC: Failed to allocate memory for "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_GTC;
	memcpy(pos, msg, msg_len);

	data->state = CONTINUE;

	return (u8 *) req;
}


static Boolean eap_gtc_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;
	size_t len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 2 || *pos != EAP_TYPE_GTC ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-GTC: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_gtc_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_gtc_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos;
	size_t rlen;

	if (sm->user == NULL || sm->user->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-GTC: Password not configured");
		data->state = FAILURE;
		return;
	}

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	pos++;
	rlen = ntohs(resp->length) - sizeof(*resp) - 1;
	wpa_hexdump_key(MSG_MSGDUMP, "EAP-GTC: Response", pos, rlen);

	if (rlen != sm->user->password_len ||
	    memcmp(pos, sm->user->password, rlen) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-GTC: Done - Failure");
		data->state = FAILURE;
	} else {
		wpa_printf(MSG_DEBUG, "EAP-GTC: Done - Success");
		data->state = SUCCESS;
	}
}


static Boolean eap_gtc_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_gtc_data *data = priv;
	return data->state != CONTINUE;
}


static Boolean eap_gtc_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_gtc_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_gtc =
{
	.method = EAP_TYPE_GTC,
	.name = "GTC",
	.init = eap_gtc_init,
	.reset = eap_gtc_reset,
	.buildReq = eap_gtc_buildReq,
	.check = eap_gtc_check,
	.process = eap_gtc_process,
	.isDone = eap_gtc_isDone,
	.isSuccess = eap_gtc_isSuccess,
};
