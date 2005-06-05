/*
 * hostapd / EAP-Identity
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


struct eap_identity_data {
	enum { CONTINUE, SUCCESS, FAILURE } state;
	int pick_up;
};


static void * eap_identity_init(struct eap_sm *sm)
{
	struct eap_identity_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = CONTINUE;

	return data;
}


static void * eap_identity_initPickUp(struct eap_sm *sm)
{
	struct eap_identity_data *data;
	data = eap_identity_init(sm);
	if (data) {
		data->pick_up = 1;
	}
	return data;
}


static void eap_identity_reset(struct eap_sm *sm, void *priv)
{
	struct eap_identity_data *data = priv;
	free(data);
}


static u8 * eap_identity_buildReq(struct eap_sm *sm, void *priv, int id,
			     size_t *reqDataLen)
{
	struct eap_identity_data *data = priv;
	struct eap_hdr *req;
	u8 *pos;

	*reqDataLen = sizeof(*req) + 1;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-Identity: Failed to allocate "
			   "memory for request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos = EAP_TYPE_IDENTITY;

	return (u8 *) req;
}


static Boolean eap_identity_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;
	size_t len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 1 || *pos != EAP_TYPE_IDENTITY ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-Identity: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_identity_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_identity_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos;
	int len;

	if (data->pick_up) {
		if (eap_identity_check(sm, data, respData, respDataLen)) {
			wpa_printf(MSG_DEBUG, "EAP-Identity: failed to pick "
				   "up already started negotiation");
			data->state = FAILURE;
			return;
		}
		data->pick_up = 0;
	}

	resp = (struct eap_hdr *) respData;
	len = ntohs(resp->length);
	pos = (u8 *) (resp + 1);
	pos++;
	len -= sizeof(*resp) + 1;
	if (len < 0) {
		data->state = FAILURE;
		return;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-Identity: Peer identity", pos, len);
	free(sm->identity);
	sm->identity = malloc(len);
	if (sm->identity == NULL) {
		data->state = FAILURE;
	} else {
		memcpy(sm->identity, pos, len);
		sm->identity_len = len;
		data->state = SUCCESS;
	}
}


static Boolean eap_identity_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_identity_data *data = priv;
	return data->state != CONTINUE;
}


static Boolean eap_identity_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_identity_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_identity =
{
	.method = EAP_TYPE_IDENTITY,
	.name = "Identity",
	.init = eap_identity_init,
	.initPickUp = eap_identity_initPickUp,
	.reset = eap_identity_reset,
	.buildReq = eap_identity_buildReq,
	.check = eap_identity_check,
	.process = eap_identity_process,
	.isDone = eap_identity_isDone,
	.isSuccess = eap_identity_isSuccess,
};
