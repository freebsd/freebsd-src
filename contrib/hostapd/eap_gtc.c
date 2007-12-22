/*
 * hostapd / EAP-GTC (RFC 3748)
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


struct eap_gtc_data {
	enum { CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_gtc_init(struct eap_sm *sm)
{
	struct eap_gtc_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
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
	req = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_GTC, reqDataLen,
			    msg_len, EAP_CODE_REQUEST, id, &pos);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-GTC: Failed to allocate memory for "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	memcpy(pos, msg, msg_len);

	data->state = CONTINUE;

	return (u8 *) req;
}


static Boolean eap_gtc_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_GTC,
			       respData, respDataLen, &len);
	if (pos == NULL || len < 1) {
		wpa_printf(MSG_INFO, "EAP-GTC: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_gtc_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_gtc_data *data = priv;
	const u8 *pos;
	size_t rlen;

	if (sm->user == NULL || sm->user->password == NULL ||
	    sm->user->password_hash) {
		wpa_printf(MSG_INFO, "EAP-GTC: Plaintext password not "
			   "configured");
		data->state = FAILURE;
		return;
	}

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_GTC,
			       respData, respDataLen, &rlen);
	if (pos == NULL || rlen < 1)
		return; /* Should not happen - frame already validated */

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


int eap_server_gtc_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_GTC, "GTC");
	if (eap == NULL)
		return -1;

	eap->init = eap_gtc_init;
	eap->reset = eap_gtc_reset;
	eap->buildReq = eap_gtc_buildReq;
	eap->check = eap_gtc_check;
	eap->process = eap_gtc_process;
	eap->isDone = eap_gtc_isDone;
	eap->isSuccess = eap_gtc_isSuccess;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
