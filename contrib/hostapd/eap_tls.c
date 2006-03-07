/*
 * hostapd / EAP-TLS (RFC 2716)
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
#include "eap_tls_common.h"
#include "tls.h"


static void eap_tls_reset(struct eap_sm *sm, void *priv);


struct eap_tls_data {
	struct eap_ssl_data ssl;
	enum { START, CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_tls_init(struct eap_sm *sm)
{
	struct eap_tls_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = START;

	if (eap_tls_ssl_init(sm, &data->ssl, 1)) {
		wpa_printf(MSG_INFO, "EAP-TLS: Failed to initialize SSL.");
		eap_tls_reset(sm, data);
		return NULL;
	}

	return data;
}


static void eap_tls_reset(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	if (data == NULL)
		return;
	eap_tls_ssl_deinit(sm, &data->ssl);
	free(data);
}


static u8 * eap_tls_build_start(struct eap_sm *sm, struct eap_tls_data *data,
				int id, size_t *reqDataLen)
{
	struct eap_hdr *req;
	u8 *pos;

	*reqDataLen = sizeof(*req) + 2;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-TLS: Failed to allocate memory for "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_TLS;
	*pos = EAP_TLS_FLAGS_START;

	data->state = CONTINUE;

	return (u8 *) req;
}


static u8 * eap_tls_build_req(struct eap_sm *sm, struct eap_tls_data *data,
			      int id, size_t *reqDataLen)
{
	int res;
	u8 *req;

	res = eap_tls_buildReq_helper(sm, &data->ssl, EAP_TYPE_TLS, 0, id,
				      &req, reqDataLen);

	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
		wpa_printf(MSG_DEBUG, "EAP-TLS: Done");
		data->state = SUCCESS;
	}

	if (res == 1)
		return eap_tls_build_ack(reqDataLen, id, EAP_TYPE_TLS, 0);
	return req;
}


static u8 * eap_tls_buildReq(struct eap_sm *sm, void *priv, int id,
			     size_t *reqDataLen)
{
	struct eap_tls_data *data = priv;

	switch (data->state) {
	case START:
		return eap_tls_build_start(sm, data, id, reqDataLen);
	case CONTINUE:
		return eap_tls_build_req(sm, data, id, reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-TLS: %s - unexpected state %d",
			   __func__, data->state);
		return NULL;
	}
}


static Boolean eap_tls_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;
	size_t len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 2 || *pos != EAP_TYPE_TLS ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-TLS: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_tls_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_tls_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos, flags;
	int left;
	unsigned int tls_msg_len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	pos++;
	flags = *pos++;
	left = htons(resp->length) - sizeof(struct eap_hdr) - 2;
	wpa_printf(MSG_DEBUG, "EAP-TLS: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) respDataLen, flags);
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-TLS: Short frame with TLS "
				   "length");
			data->state = FAILURE;
			return;
		}
		tls_msg_len = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) |
			pos[3];
		wpa_printf(MSG_DEBUG, "EAP-TLS: TLS Message Length: %d",
			   tls_msg_len);
		if (data->ssl.tls_in_left == 0) {
			data->ssl.tls_in_total = tls_msg_len;
			data->ssl.tls_in_left = tls_msg_len;
			free(data->ssl.tls_in);
			data->ssl.tls_in = NULL;
			data->ssl.tls_in_len = 0;
		}
		pos += 4;
		left -= 4;
	}

	if (eap_tls_process_helper(sm, &data->ssl, pos, left) < 0) {
		wpa_printf(MSG_INFO, "EAP-TLS: TLS processing failed");
		data->state = FAILURE;
		return;
	}

	if (tls_connection_get_write_alerts(sm->ssl_ctx, data->ssl.conn) > 1) {
		wpa_printf(MSG_INFO, "EAP-TLS: Locally detected fatal error "
			   "in TLS processing");
		data->state = FAILURE;
		return;
	}
}


static Boolean eap_tls_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_tls_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_tls_data *data = priv;
	u8 *eapKeyData;

	if (data->state != SUCCESS)
		return NULL;

	eapKeyData = eap_tls_derive_key(sm, &data->ssl,
					"client EAP encryption",
					EAP_TLS_KEY_LEN);
	if (eapKeyData) {
		*len = EAP_TLS_KEY_LEN;
		wpa_hexdump(MSG_DEBUG, "EAP-TLS: Derived key",
			    eapKeyData, EAP_TLS_KEY_LEN);
	} else {
		wpa_printf(MSG_DEBUG, "EAP-TLS: Failed to derive key");
	}

	return eapKeyData;
}


static Boolean eap_tls_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_tls =
{
	.method = EAP_TYPE_TLS,
	.name = "TLS",
	.init = eap_tls_init,
	.reset = eap_tls_reset,
	.buildReq = eap_tls_buildReq,
	.check = eap_tls_check,
	.process = eap_tls_process,
	.isDone = eap_tls_isDone,
	.getKey = eap_tls_getKey,
	.isSuccess = eap_tls_isSuccess,
};
