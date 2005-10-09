/*
 * WPA Supplicant / EAP-TLS (RFC 2716)
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
#include "eap_tls_common.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "tls.h"


static void eap_tls_deinit(struct eap_sm *sm, void *priv);


struct eap_tls_data {
	struct eap_ssl_data ssl;
	u8 *key_data;
};


static void * eap_tls_init(struct eap_sm *sm)
{
	struct eap_tls_data *data;
	struct wpa_ssid *config = eap_get_config(sm);
	if (config == NULL ||
	    (sm->init_phase2 ? config->private_key2 : config->private_key)
	    == NULL) {
		wpa_printf(MSG_INFO, "EAP-TLS: Private key not configured");
		return NULL;
	}

	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));

	if (eap_tls_ssl_init(sm, &data->ssl, config)) {
		wpa_printf(MSG_INFO, "EAP-TLS: Failed to initialize SSL.");
		eap_tls_deinit(sm, data);
		return NULL;
	}

	return data;
}


static void eap_tls_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	if (data == NULL)
		return;
	eap_tls_ssl_deinit(sm, &data->ssl);
	free(data->key_data);
	free(data);
}


static u8 * eap_tls_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_hdr *req;
	int left, res;
	unsigned int tls_msg_len;
	u8 flags, *pos, *resp, id;
	struct eap_tls_data *data = priv;

	if (tls_get_errors(sm->ssl_ctx)) {
		wpa_printf(MSG_INFO, "EAP-TLS: TLS errors detected");
		ret->ignore = TRUE;
		return NULL;
	}

	req = (struct eap_hdr *) reqData;
	pos = (u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 2 || *pos != EAP_TYPE_TLS) {
		wpa_printf(MSG_INFO, "EAP-TLS: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}
	id = req->identifier;
	pos++;
	flags = *pos++;
	left = be_to_host16(req->length) - sizeof(struct eap_hdr) - 2;
	wpa_printf(MSG_DEBUG, "EAP-TLS: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) reqDataLen, flags);
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-TLS: Short frame with TLS "
				   "length");
			ret->ignore = TRUE;
			return NULL;
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

	ret->ignore = FALSE;

	ret->methodState = METHOD_CONT;
	ret->decision = DECISION_COND_SUCC;
	ret->allowNotifications = TRUE;

	if (flags & EAP_TLS_FLAGS_START) {
		wpa_printf(MSG_DEBUG, "EAP-TLS: Start");
		left = 0; /* make sure that this frame is empty, even though it
			   * should always be, anyway */
	}

	resp = NULL;
	res = eap_tls_process_helper(sm, &data->ssl, EAP_TYPE_TLS, 0, id, pos,
				     left, &resp, respDataLen);

	if (res < 0) {
		wpa_printf(MSG_DEBUG, "EAP-TLS: TLS processing failed");
		ret->methodState = METHOD_MAY_CONT;
		ret->decision = DECISION_FAIL;
		return eap_tls_build_ack(&data->ssl, respDataLen, id,
					 EAP_TYPE_TLS, 0);
	}

	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
		wpa_printf(MSG_DEBUG, "EAP-TLS: Done");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_UNCOND_SUCC;
		free(data->key_data);
		data->key_data = eap_tls_derive_key(sm, &data->ssl,
						    "client EAP encryption",
						    EAP_TLS_KEY_LEN);
		if (data->key_data) {
			wpa_hexdump_key(MSG_DEBUG, "EAP-TLS: Derived key",
					data->key_data, EAP_TLS_KEY_LEN);
		} else {
			wpa_printf(MSG_DEBUG, "EAP-TLS: Failed to derive key");
		}
	}

	if (res == 1) {
		return eap_tls_build_ack(&data->ssl, respDataLen, id,
					 EAP_TYPE_TLS, 0);
	}
	return resp;
}


static Boolean eap_tls_has_reauth_data(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	return tls_connection_established(sm->ssl_ctx, data->ssl.conn);
}


static void eap_tls_deinit_for_reauth(struct eap_sm *sm, void *priv)
{
}


static void * eap_tls_init_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	free(data->key_data);
	data->key_data = NULL;
	if (eap_tls_reauth_init(sm, &data->ssl)) {
		free(data);
		return NULL;
	}
	return priv;
}


static int eap_tls_get_status(struct eap_sm *sm, void *priv, char *buf,
			      size_t buflen, int verbose)
{
	struct eap_tls_data *data = priv;
	return eap_tls_status(sm, &data->ssl, buf, buflen, verbose);
}


static Boolean eap_tls_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_tls_data *data = priv;
	return data->key_data != NULL;
}


static u8 * eap_tls_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_tls_data *data = priv;
	u8 *key;

	if (data->key_data == NULL)
		return NULL;

	key = malloc(EAP_TLS_KEY_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_TLS_KEY_LEN;
	memcpy(key, data->key_data, EAP_TLS_KEY_LEN);

	return key;
}


const struct eap_method eap_method_tls =
{
	.method = EAP_TYPE_TLS,
	.name = "TLS",
	.init = eap_tls_init,
	.deinit = eap_tls_deinit,
	.process = eap_tls_process,
	.isKeyAvailable = eap_tls_isKeyAvailable,
	.getKey = eap_tls_getKey,
	.get_status = eap_tls_get_status,
	.has_reauth_data = eap_tls_has_reauth_data,
	.deinit_for_reauth = eap_tls_deinit_for_reauth,
	.init_for_reauth = eap_tls_init_for_reauth,
};
