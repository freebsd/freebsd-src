/*
 * EAP peer method: Test method for vendor specific (expanded) EAP type
 * Copyright (c) 2005-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file implements a vendor specific test method using EAP expanded types.
 * This is only for test use and must not be used for authentication since no
 * security is provided.
 */

#include "includes.h"

#include "common.h"
#include "eap_i.h"
#include "eloop.h"


#define EAP_VENDOR_ID EAP_VENDOR_HOSTAP
#define EAP_VENDOR_TYPE 0xfcfbfaf9


struct eap_vendor_test_data {
	enum { INIT, CONFIRM, SUCCESS } state;
	int first_try;
	int test_pending_req;
};


static void * eap_vendor_test_init(struct eap_sm *sm)
{
	struct eap_vendor_test_data *data;
	const u8 *password;
	size_t password_len;

	data = os_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = INIT;
	data->first_try = 1;

	password = eap_get_config_password(sm, &password_len);
	data->test_pending_req = password && password_len == 7 &&
		os_memcmp(password, "pending", 7) == 0;

	return data;
}


static void eap_vendor_test_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_vendor_test_data *data = priv;
	os_free(data);
}


static void eap_vendor_ready(void *eloop_ctx, void *timeout_ctx)
{
	struct eap_sm *sm = eloop_ctx;
	wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: Ready to re-process pending "
		   "request");
	eap_notify_pending(sm);
}


static struct wpabuf * eap_vendor_test_process(struct eap_sm *sm, void *priv,
					       struct eap_method_ret *ret,
					       const struct wpabuf *reqData)
{
	struct eap_vendor_test_data *data = priv;
	struct wpabuf *resp;
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_ID, EAP_VENDOR_TYPE, reqData, &len);
	if (pos == NULL || len < 1) {
		ret->ignore = true;
		return NULL;
	}

	if (data->state == INIT && *pos != 1) {
		wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: Unexpected message "
			   "%d in INIT state", *pos);
		ret->ignore = true;
		return NULL;
	}

	if (data->state == CONFIRM && *pos != 3) {
		wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: Unexpected message "
			   "%d in CONFIRM state", *pos);
		ret->ignore = true;
		return NULL;
	}

	if (data->state == SUCCESS) {
		wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: Unexpected message "
			   "in SUCCESS state");
		ret->ignore = true;
		return NULL;
	}

	if (data->state == CONFIRM) {
		if (data->test_pending_req && data->first_try) {
			data->first_try = 0;
			wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: Testing "
				   "pending request");
			ret->ignore = true;
			eloop_register_timeout(1, 0, eap_vendor_ready, sm,
					       NULL);
			return NULL;
		}
	}

	ret->ignore = false;

	wpa_printf(MSG_DEBUG, "EAP-VENDOR-TEST: Generating Response");
	ret->allowNotifications = true;

	resp = eap_msg_alloc(EAP_VENDOR_ID, EAP_VENDOR_TYPE, 1,
			     EAP_CODE_RESPONSE, eap_get_id(reqData));
	if (resp == NULL)
		return NULL;

	if (data->state == INIT) {
		wpabuf_put_u8(resp, 2);
		data->state = CONFIRM;
		ret->methodState = METHOD_CONT;
		ret->decision = DECISION_FAIL;
	} else {
		wpabuf_put_u8(resp, 4);
		data->state = SUCCESS;
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_UNCOND_SUCC;
	}

	return resp;
}


static bool eap_vendor_test_isKeyAvailable(struct eap_sm *sm, void *priv)
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

	key = os_malloc(key_len);
	if (key == NULL)
		return NULL;

	os_memset(key, 0x11, key_len / 2);
	os_memset(key + key_len / 2, 0x22, key_len / 2);
	*len = key_len;

	return key;
}


int eap_peer_vendor_test_register(void)
{
	struct eap_method *eap;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,
				    EAP_VENDOR_ID, EAP_VENDOR_TYPE,
				    "VENDOR-TEST");
	if (eap == NULL)
		return -1;

	eap->init = eap_vendor_test_init;
	eap->deinit = eap_vendor_test_deinit;
	eap->process = eap_vendor_test_process;
	eap->isKeyAvailable = eap_vendor_test_isKeyAvailable;
	eap->getKey = eap_vendor_test_getKey;

	return eap_peer_method_register(eap);
}
