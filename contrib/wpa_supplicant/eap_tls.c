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
	    ((sm->init_phase2 ? config->private_key2 : config->private_key)
	    == NULL && config->engine == 0)) {
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
		if (config->engine) {
			wpa_printf(MSG_DEBUG, "EAP-TLS: Requesting Smartcard "
				   "PIN");
			eap_sm_request_pin(sm, config);
			sm->ignore = TRUE;
		} else if (config->private_key && !config->private_key_passwd)
		{
			wpa_printf(MSG_DEBUG, "EAP-TLS: Requesting private "
				   "key passphrase");
			eap_sm_request_passphrase(sm, config);
			sm->ignore = TRUE;
		}
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
			    const u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct wpa_ssid *config = eap_get_config(sm);
	const struct eap_hdr *req;
	size_t left;
	int res;
	u8 flags, *resp, id;
	const u8 *pos;
	struct eap_tls_data *data = priv;

	pos = eap_tls_process_init(sm, &data->ssl, EAP_TYPE_TLS, ret,
				   reqData, reqDataLen, &left, &flags);
	if (pos == NULL)
		return NULL;
	req = (const struct eap_hdr *) reqData;
	id = req->identifier;

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
		if (resp) {
			/* This is likely an alert message, so send it instead
			 * of just ACKing the error. */
			return resp;
		}
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

	if (res == -1) {
		/* The TLS handshake failed. So better forget the old PIN.
		 * It may be wrong, we can't be sure but trying the wrong one
		 * again might block it on the card - so better ask the user
		 * again */
		free(config->pin);
		config->pin = NULL;
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
