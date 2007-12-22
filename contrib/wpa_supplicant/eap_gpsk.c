/*
 * EAP peer method: EAP-GPSK (draft-ietf-emu-eap-gpsk-03.txt)
 * Copyright (c) 2006-2007, Jouni Malinen <j@w1.fi>
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

#include "common.h"
#include "eap_i.h"
#include "config_ssid.h"
#include "eap_gpsk_common.h"

struct eap_gpsk_data {
	enum { GPSK_1, GPSK_3, SUCCESS, FAILURE } state;
	u8 rand_server[EAP_GPSK_RAND_LEN];
	u8 rand_client[EAP_GPSK_RAND_LEN];
	u8 msk[EAP_MSK_LEN];
	u8 emsk[EAP_EMSK_LEN];
	u8 sk[EAP_GPSK_MAX_SK_LEN];
	size_t sk_len;
	u8 pk[EAP_GPSK_MAX_PK_LEN];
	size_t pk_len;
	u8 session_id;
	int session_id_set;
	u8 *id_client;
	size_t id_client_len;
	u8 *id_server;
	size_t id_server_len;
	int vendor; /* CSuite/Specifier */
	int specifier; /* CSuite/Specifier */
	u8 *psk;
	size_t psk_len;
};


#ifndef CONFIG_NO_STDOUT_DEBUG
static const char * eap_gpsk_state_txt(int state)
{
	switch (state) {
	case GPSK_1:
		return "GPSK-1";
	case GPSK_3:
		return "GPSK-3";
	case SUCCESS:
		return "SUCCESS";
	case FAILURE:
		return "FAILURE";
	default:
		return "?";
	}
}
#endif /* CONFIG_NO_STDOUT_DEBUG */


static void eap_gpsk_state(struct eap_gpsk_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-GPSK: %s -> %s",
		   eap_gpsk_state_txt(data->state),
		   eap_gpsk_state_txt(state));
	data->state = state;
}


static void eap_gpsk_deinit(struct eap_sm *sm, void *priv);


static void * eap_gpsk_init(struct eap_sm *sm)
{
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_gpsk_data *data;

	if (config == NULL) {
		wpa_printf(MSG_INFO, "EAP-GPSK: No configuration found");
		return NULL;
	}

	if (config->eappsk == NULL) {
		wpa_printf(MSG_INFO, "EAP-GPSK: No key (eappsk) configured");
		return NULL;
	}

	data = os_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = GPSK_1;

	if (config->nai) {
		data->id_client = os_malloc(config->nai_len);
		if (data->id_client == NULL) {
			eap_gpsk_deinit(sm, data);
			return NULL;
		}
		os_memcpy(data->id_client, config->nai, config->nai_len);
		data->id_client_len = config->nai_len;
	}

	data->psk = os_malloc(config->eappsk_len);
	if (data->psk == NULL) {
		eap_gpsk_deinit(sm, data);
		return NULL;
	}
	os_memcpy(data->psk, config->eappsk, config->eappsk_len);
	data->psk_len = config->eappsk_len;

	return data;
}


static void eap_gpsk_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_gpsk_data *data = priv;
	os_free(data->id_server);
	os_free(data->id_client);
	os_free(data->psk);
	os_free(data);
}


static u8 * eap_gpsk_process_gpsk_1(struct eap_sm *sm,
				    struct eap_gpsk_data *data,
				    struct eap_method_ret *ret,
				    const u8 *reqData, size_t reqDataLen,
				    const u8 *payload, size_t payload_len,
				    size_t *respDataLen)
{
	size_t len, csuite_list_len, miclen;
	struct eap_hdr *resp;
	u8 *rpos, *start;
	const u8 *csuite_list, *pos, *end;
	const struct eap_hdr *req;
	struct eap_gpsk_csuite *csuite;
	u16 alen;
	int i, count;

	if (data->state != GPSK_1) {
		ret->ignore = TRUE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Received Request/GPSK-1");

	req = (const struct eap_hdr *) reqData;
	pos = payload;
	end = payload + payload_len;

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short GPSK-1 packet");
		return NULL;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: ID_Server overflow");
		return NULL;
	}
	os_free(data->id_server);
	data->id_server = os_malloc(alen);
	if (data->id_server == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: No memory for ID_Server");
		return NULL;
	}
	os_memcpy(data->id_server, pos, alen);
	data->id_server_len = alen;
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-GPSK: ID_Server",
			  data->id_server, data->id_server_len);
	pos += alen;

	if (end - pos < EAP_GPSK_RAND_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: RAND_Server overflow");
		return NULL;
	}
	os_memcpy(data->rand_server, pos, EAP_GPSK_RAND_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Server",
		    data->rand_server, EAP_GPSK_RAND_LEN);
	pos += EAP_GPSK_RAND_LEN;

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short GPSK-1 packet");
		return NULL;
	}
	csuite_list_len = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < (int) csuite_list_len) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: CSuite_List overflow");
		return NULL;
	}
	csuite_list = pos;

	if (csuite_list_len == 0 ||
	    csuite_list_len % sizeof(struct eap_gpsk_csuite)) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Invalid CSuite_List len %d",
			   csuite_list_len);
		return NULL;
	}
	count = csuite_list_len / sizeof(struct eap_gpsk_csuite);
	data->vendor = EAP_GPSK_VENDOR_IETF;
	data->specifier = EAP_GPSK_CIPHER_RESERVED;
	csuite = (struct eap_gpsk_csuite *) csuite_list;
	for (i = 0; i < count; i++) {
		int vendor, specifier;
		vendor = WPA_GET_BE24(csuite->vendor);
		specifier = WPA_GET_BE24(csuite->specifier);
		wpa_printf(MSG_DEBUG, "EAP-GPSK: CSuite[%d]: %d:%d",
			   i, vendor, specifier);
		if (data->vendor == EAP_GPSK_VENDOR_IETF &&
		    data->specifier == EAP_GPSK_CIPHER_RESERVED &&
		    eap_gpsk_supported_ciphersuite(vendor, specifier)) {
			data->vendor = vendor;
			data->specifier = specifier;
		}
		csuite++;
	}
	if (data->vendor == EAP_GPSK_VENDOR_IETF &&
	    data->specifier == EAP_GPSK_CIPHER_RESERVED) {
		wpa_msg(sm->msg_ctx, MSG_INFO, "EAP-GPSK: No supported "
			"ciphersuite found");
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "EAP-GPSK: Selected ciphersuite %d:%d",
		   data->vendor, data->specifier);

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Sending Response/GPSK-2");

	miclen = eap_gpsk_mic_len(data->vendor, data->specifier);
	len = 1 + 2 + data->id_client_len + 2 + data->id_server_len +
		2 * EAP_GPSK_RAND_LEN + 2 + csuite_list_len +
		sizeof(struct eap_gpsk_csuite) + 2 + miclen;

	resp = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_GPSK, respDataLen, len,
			     EAP_CODE_RESPONSE, req->identifier, &rpos);
	if (resp == NULL)
		return NULL;

	*rpos++ = EAP_GPSK_OPCODE_GPSK_2;
	start = rpos;

	wpa_hexdump_ascii(MSG_DEBUG, "EAP-GPSK: ID_Client",
			  data->id_client, data->id_client_len);
	WPA_PUT_BE16(rpos, data->id_client_len);
	rpos += 2;
	if (data->id_client)
		os_memcpy(rpos, data->id_client, data->id_client_len);
	rpos += data->id_client_len;

	WPA_PUT_BE16(rpos, data->id_server_len);
	rpos += 2;
	if (data->id_server)
		os_memcpy(rpos, data->id_server, data->id_server_len);
	rpos += data->id_server_len;

	if (os_get_random(data->rand_client, EAP_GPSK_RAND_LEN)) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Failed to get random data "
			   "for RAND_Client");
		eap_gpsk_state(data, FAILURE);
		os_free(resp);
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Client",
		    data->rand_client, EAP_GPSK_RAND_LEN);
	os_memcpy(rpos, data->rand_client, EAP_GPSK_RAND_LEN);
	rpos += EAP_GPSK_RAND_LEN;

	os_memcpy(rpos, data->rand_server, EAP_GPSK_RAND_LEN);
	rpos += EAP_GPSK_RAND_LEN;

	WPA_PUT_BE16(rpos, csuite_list_len);
	rpos += 2;
	os_memcpy(rpos, csuite_list, csuite_list_len);
	rpos += csuite_list_len;

	csuite = (struct eap_gpsk_csuite *) rpos;
	WPA_PUT_BE24(csuite->vendor, data->vendor);
	WPA_PUT_BE24(csuite->specifier, data->specifier);
	rpos = (u8 *) (csuite + 1);

	if (eap_gpsk_derive_keys(data->psk, data->psk_len,
				 data->vendor, data->specifier,
				 data->rand_client, data->rand_server,
				 data->id_client, data->id_client_len,
				 data->id_server, data->id_server_len,
				 data->msk, data->emsk,
				 data->sk, &data->sk_len,
				 data->pk, &data->pk_len) < 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Failed to derive keys");
		eap_gpsk_state(data, FAILURE);
		os_free(resp);
		return NULL;
	}

	/* No PD_Payload_1 */
	WPA_PUT_BE16(rpos, 0);
	rpos += 2;

	if (eap_gpsk_compute_mic(data->sk, data->sk_len, data->vendor,
				 data->specifier, start, rpos - start, rpos) <
	    0) {
		eap_gpsk_state(data, FAILURE);
		os_free(resp);
		return NULL;
	}

	eap_gpsk_state(data, GPSK_3);

	return (u8 *) resp;
}


static u8 * eap_gpsk_process_gpsk_3(struct eap_sm *sm,
				    struct eap_gpsk_data *data,
				    struct eap_method_ret *ret,
				    const u8 *reqData, size_t reqDataLen,
				    const u8 *payload, size_t payload_len,
				    size_t *respDataLen)
{
	size_t len, miclen;
	struct eap_hdr *resp;
	u8 *rpos, *start;
	const struct eap_hdr *req;
	const u8 *pos, *end;
	u16 alen;
	int vendor, specifier;
	const struct eap_gpsk_csuite *csuite;
	u8 mic[EAP_GPSK_MAX_MIC_LEN];

	if (data->state != GPSK_3) {
		ret->ignore = TRUE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Received Request/GPSK-3");

	req = (const struct eap_hdr *) reqData;
	pos = payload;
	end = payload + payload_len;

	if (end - pos < EAP_GPSK_RAND_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for "
			   "RAND_Client");
		return NULL;
	}
	if (os_memcmp(pos, data->rand_client, EAP_GPSK_RAND_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: RAND_Client in GPSK-2 and "
			   "GPSK-3 did not match");
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Client in GPSK-2",
			    data->rand_client, EAP_GPSK_RAND_LEN);
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Client in GPSK-3",
			    pos, EAP_GPSK_RAND_LEN);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	pos += EAP_GPSK_RAND_LEN;

	if (end - pos < EAP_GPSK_RAND_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for "
			   "RAND_Server");
		return NULL;
	}
	if (os_memcmp(pos, data->rand_server, EAP_GPSK_RAND_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: RAND_Server in GPSK-1 and "
			   "GPSK-3 did not match");
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Server in GPSK-1",
			    data->rand_server, EAP_GPSK_RAND_LEN);
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Server in GPSK-3",
			    pos, EAP_GPSK_RAND_LEN);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	pos += EAP_GPSK_RAND_LEN;

	if (end - pos < (int) sizeof(*csuite)) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for "
			   "CSuite_Sel");
		return NULL;
	}
	csuite = (const struct eap_gpsk_csuite *) pos;
	vendor = WPA_GET_BE24(csuite->vendor);
	specifier = WPA_GET_BE24(csuite->specifier);
	pos += sizeof(*csuite);
	if (vendor != data->vendor || specifier != data->specifier) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: CSuite_Sel (%d:%d) does not "
			   "match with the one sent in GPSK-2 (%d:%d)",
			   vendor, specifier, data->vendor, data->specifier);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for "
			   "PD_Payload_2 length");
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for "
			   "%d-octet PD_Payload_2", alen);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-GPSK: PD_Payload_2", pos, alen);
	pos += alen;
	miclen = eap_gpsk_mic_len(data->vendor, data->specifier);
	if (end - pos < (int) miclen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for MIC "
			   "(left=%d miclen=%d)", end - pos, miclen);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	if (eap_gpsk_compute_mic(data->sk, data->sk_len, data->vendor,
				 data->specifier, payload, pos - payload, mic)
	    < 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Failed to compute MIC");
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	if (os_memcmp(mic, pos, miclen) != 0) {
		wpa_printf(MSG_INFO, "EAP-GPSK: Incorrect MIC in GPSK-3");
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: Received MIC", pos, miclen);
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: Computed MIC", mic, miclen);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	pos += miclen;

	if (pos != end) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Ignored %d bytes of extra "
			   "data in the end of GPSK-2", end - pos);
	}

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Sending Response/GPSK-4");

	len = 1 + 2 + miclen;

	resp = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_GPSK, respDataLen, len,
			     EAP_CODE_RESPONSE, req->identifier, &rpos);
	if (resp == NULL)
		return NULL;

	*rpos++ = EAP_GPSK_OPCODE_GPSK_4;
	start = rpos;

	/* No PD_Payload_3 */
	WPA_PUT_BE16(rpos, 0);
	rpos += 2;

	if (eap_gpsk_compute_mic(data->sk, data->sk_len, data->vendor,
				 data->specifier, start, rpos - start, rpos) <
	    0) {
		eap_gpsk_state(data, FAILURE);
		os_free(resp);
		return NULL;
	}

	eap_gpsk_state(data, SUCCESS);
	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_UNCOND_SUCC;

	return (u8 *) resp;
}


static u8 * eap_gpsk_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    const u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_gpsk_data *data = priv;
	u8 *resp;
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_GPSK,
			       reqData, reqDataLen, &len);
	if (pos == NULL || len < 1) {
		ret->ignore = TRUE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Received frame: opcode %d", *pos);

	ret->ignore = FALSE;
	ret->methodState = METHOD_MAY_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = FALSE;

	switch (*pos) {
	case EAP_GPSK_OPCODE_GPSK_1:
		resp = eap_gpsk_process_gpsk_1(sm, data, ret, reqData,
					       reqDataLen, pos + 1, len - 1,
					       respDataLen);
		break;
	case EAP_GPSK_OPCODE_GPSK_3:
		resp = eap_gpsk_process_gpsk_3(sm, data, ret, reqData,
					       reqDataLen, pos + 1, len - 1,
					       respDataLen);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Ignoring message with "
			   "unknown opcode %d", *pos);
		ret->ignore = TRUE;
		return NULL;
	}

	return resp;
}


static Boolean eap_gpsk_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_gpsk_data *data = priv;
	return data->state == SUCCESS;
}


static u8 * eap_gpsk_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_gpsk_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = os_malloc(EAP_MSK_LEN);
	if (key == NULL)
		return NULL;
	os_memcpy(key, data->msk, EAP_MSK_LEN);
	*len = EAP_MSK_LEN;

	return key;
}


static u8 * eap_gpsk_get_emsk(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_gpsk_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = os_malloc(EAP_EMSK_LEN);
	if (key == NULL)
		return NULL;
	os_memcpy(key, data->emsk, EAP_EMSK_LEN);
	*len = EAP_EMSK_LEN;

	return key;
}


int eap_peer_gpsk_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,
				    EAP_VENDOR_IETF, EAP_TYPE_GPSK, "GPSK");
	if (eap == NULL)
		return -1;

	eap->init = eap_gpsk_init;
	eap->deinit = eap_gpsk_deinit;
	eap->process = eap_gpsk_process;
	eap->isKeyAvailable = eap_gpsk_isKeyAvailable;
	eap->getKey = eap_gpsk_getKey;
	eap->get_emsk = eap_gpsk_get_emsk;

	ret = eap_peer_method_register(eap);
	if (ret)
		eap_peer_method_free(eap);
	return ret;
}
