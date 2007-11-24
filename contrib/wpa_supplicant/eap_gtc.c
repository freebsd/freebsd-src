/*
 * WPA Supplicant / EAP-GTC (RFC 2284)
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


struct eap_gtc_data {
	int prefix;
};


static void * eap_gtc_init(struct eap_sm *sm)
{
	struct eap_gtc_data *data;
	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));

	if (sm->m && sm->m->method == EAP_TYPE_FAST) {
		wpa_printf(MSG_DEBUG, "EAP-GTC: EAP-FAST tunnel - use prefix "
			   "with challenge/response");
		data->prefix = 1;
	}
	return data;
}


static void eap_gtc_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_gtc_data *data = priv;
	free(data);
}


static u8 * eap_gtc_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    const u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_gtc_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	const struct eap_hdr *req;
	struct eap_hdr *resp;
	const u8 *pos, *password;
	u8 *rpos;
	size_t password_len, len;

	pos = eap_hdr_validate(EAP_TYPE_GTC, reqData, reqDataLen, &len);
	if (pos == NULL) {
		ret->ignore = TRUE;
		return NULL;
	}
	req = (const struct eap_hdr *) reqData;

	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-GTC: Request message", pos, len);
	if (data->prefix &&
	    (len < 10 || memcmp(pos, "CHALLENGE=", 10) != 0)) {
		wpa_printf(MSG_DEBUG, "EAP-GTC: Challenge did not start with "
			   "expected prefix");

		/* Send an empty response in order to allow tunneled
		 * acknowledgement of the failure. This will also cover the
		 * error case which seems to use EAP-MSCHAPv2 like error
		 * reporting with EAP-GTC inside EAP-FAST tunnel. */
		*respDataLen = sizeof(struct eap_hdr) + 1;
		resp = malloc(*respDataLen);
		if (resp == NULL)
			return NULL;
		resp->code = EAP_CODE_RESPONSE;
		resp->identifier = req->identifier;
		resp->length = host_to_be16(*respDataLen);
		rpos = (u8 *) (resp + 1);
		*rpos++ = EAP_TYPE_GTC;
		return (u8 *) resp;
	}

	if (config == NULL ||
	    (config->password == NULL && config->otp == NULL)) {
		wpa_printf(MSG_INFO, "EAP-GTC: Password not configured");
		eap_sm_request_otp(sm, config, (const char *) pos, len);
		ret->ignore = TRUE;
		return NULL;
	}

	if (config->otp) {
		password = config->otp;
		password_len = config->otp_len;
	} else {
		password = config->password;
		password_len = config->password_len;
	}

	ret->ignore = FALSE;

	ret->methodState = data->prefix ? METHOD_MAY_CONT : METHOD_DONE;
	ret->decision = DECISION_COND_SUCC;
	ret->allowNotifications = FALSE;

	*respDataLen = sizeof(struct eap_hdr) + 1 + password_len;
	if (data->prefix) {
		*respDataLen += 9 + config->identity_len + 1;
	}
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(*respDataLen);
	rpos = (u8 *) (resp + 1);
	*rpos++ = EAP_TYPE_GTC;
	if (data->prefix) {
		memcpy(rpos, "RESPONSE=", 9);
		rpos += 9;
		memcpy(rpos, config->identity, config->identity_len);
		rpos += config->identity_len;
		*rpos++ = '\0';
	}
	memcpy(rpos, password, password_len);
	wpa_hexdump_ascii_key(MSG_MSGDUMP, "EAP-GTC: Response",
			      (u8 *) (resp + 1) + 1,
			      *respDataLen - sizeof(struct eap_hdr) - 1);

	if (config->otp) {
		wpa_printf(MSG_DEBUG, "EAP-GTC: Forgetting used password");
		memset(config->otp, 0, config->otp_len);
		free(config->otp);
		config->otp = NULL;
		config->otp_len = 0;
	}

	return (u8 *) resp;
}


const struct eap_method eap_method_gtc =
{
	.method = EAP_TYPE_GTC,
	.name = "GTC",
	.init = eap_gtc_init,
	.deinit = eap_gtc_deinit,
	.process = eap_gtc_process,
};
