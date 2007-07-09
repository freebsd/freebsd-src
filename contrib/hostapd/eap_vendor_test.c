/*
 * hostapd / Test method for vendor specific (expanded) EAP type
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
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


#define EAP_VENDOR_ID 0xfffefd
#define EAP_VENDOR_TYPE 0xfcfbfaf9


struct eap_vendor_test_data {
	enum { INIT, CONFIRM, SUCCESS, FAILURE } state;
};


static const char * eap_vendor_test_state_txt(int state)
{
	switch (state) {
	case INIT:
		return "INIT";
	case CONFIRM:
		return "CONFIRM";
	case SUCCESS:
		return "SUCCESS";
	case FAILURE:
		return "FAILURE";
	default:
		return "?";
	}
}


static void eap_vendor_test_state(struct eap_vendor_test_data *data,
				  int state)
{
	wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: %s -> %s",
		   eap_vendor_test_state_txt(data->state),
		   eap_vendor_test_state_txt(state));
	data->state = state;
}


static void * eap_vendor_test_init(struct eap_sm *sm)
{
	struct eap_vendor_test_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = INIT;

	return data;
}


static void eap_vendor_test_reset(struct eap_sm *sm, void *priv)
{
	struct eap_vendor_test_data *data = priv;
	free(data);
}


static u8 * eap_vendor_test_buildReq(struct eap_sm *sm, void *priv, int id,
				     size_t *reqDataLen)
{
	struct eap_vendor_test_data *data = priv;
	struct eap_hdr *req;
	u8 *pos;

	*reqDataLen = sizeof(*req) + 8 + 1;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-VENDOR-TEST: Failed to allocate "
			   "memory for request");
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_EXPANDED;
	WPA_PUT_BE24(pos, EAP_VENDOR_ID);
	pos += 3;
	WPA_PUT_BE32(pos, EAP_VENDOR_TYPE);
	pos += 4;
	*pos = data->state == INIT ? 1 : 3;

	return (u8 *) req;
}


static Boolean eap_vendor_test_check(struct eap_sm *sm, void *priv,
				     u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;
	size_t len;
	int vendor;
	u32 method;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp))
		return TRUE;
	len = ntohs(resp->length);
	if (len > respDataLen)
		return TRUE;

	if (len < sizeof(*resp) + 8 || *pos != EAP_TYPE_EXPANDED) {
		wpa_printf(MSG_INFO, "EAP-VENDOR-TEST: Invalid frame");
		return TRUE;
	}
	pos++;

	vendor = WPA_GET_BE24(pos);
	pos += 3;
	method = WPA_GET_BE32(pos);
	pos++;

	if (vendor != EAP_VENDOR_ID || method != EAP_VENDOR_TYPE)
		return TRUE;

	return FALSE;
}


static void eap_vendor_test_process(struct eap_sm *sm, void *priv,
				    u8 *respData, size_t respDataLen)
{
	struct eap_vendor_test_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	pos += 8; /* Skip expanded header */

	if (data->state == INIT) {
		if (*pos == 2)
			eap_vendor_test_state(data, CONFIRM);
		else
			eap_vendor_test_state(data, FAILURE);
	} else if (data->state == CONFIRM) {
		if (*pos == 4)
			eap_vendor_test_state(data, SUCCESS);
		else
			eap_vendor_test_state(data, FAILURE);
	} else
		eap_vendor_test_state(data, FAILURE);
}


static Boolean eap_vendor_test_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_vendor_test_data *data = priv;
	return data->state == SUCCESS;
}


static u8 * eap_vendor_test_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_vendor_test_data *data = priv;
	u8 *key;
	const int key_len = 64;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(key_len);
	if (key == NULL)
		return NULL;

	memset(key, 0x11, key_len / 2);
	memset(key + key_len / 2, 0x22, key_len / 2);
	*len = key_len;

	return key;
}


static Boolean eap_vendor_test_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_vendor_test_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_vendor_test_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_ID, EAP_VENDOR_TYPE,
				      "VENDOR-TEST");
	if (eap == NULL)
		return -1;

	eap->init = eap_vendor_test_init;
	eap->reset = eap_vendor_test_reset;
	eap->buildReq = eap_vendor_test_buildReq;
	eap->check = eap_vendor_test_check;
	eap->process = eap_vendor_test_process;
	eap->isDone = eap_vendor_test_isDone;
	eap->getKey = eap_vendor_test_getKey;
	eap->isSuccess = eap_vendor_test_isSuccess;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
