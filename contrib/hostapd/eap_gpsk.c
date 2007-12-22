/*
 * hostapd / EAP-GPSK (draft-ietf-emu-eap-gpsk-03.txt) server
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

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
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
	u8 *id_client;
	size_t id_client_len;
	u8 *id_server;
	size_t id_server_len;
#define MAX_NUM_CSUITES 2
	struct eap_gpsk_csuite csuite_list[MAX_NUM_CSUITES];
	size_t csuite_count;
	int vendor; /* CSuite/Vendor */
	int specifier; /* CSuite/Specifier */
};


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


static void eap_gpsk_state(struct eap_gpsk_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-GPSK: %s -> %s",
		   eap_gpsk_state_txt(data->state),
		   eap_gpsk_state_txt(state));
	data->state = state;
}


static void * eap_gpsk_init(struct eap_sm *sm)
{
	struct eap_gpsk_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = GPSK_1;

	/* TODO: add support for configuring ID_Server */
	data->id_server = (u8 *) strdup("hostapd");
	if (data->id_server)
		data->id_server_len = strlen((char *) data->id_server);

	data->csuite_count = 0;
	if (eap_gpsk_supported_ciphersuite(EAP_GPSK_VENDOR_IETF,
					   EAP_GPSK_CIPHER_AES)) {
		WPA_PUT_BE24(data->csuite_list[data->csuite_count].vendor,
			     EAP_GPSK_VENDOR_IETF);
		WPA_PUT_BE24(data->csuite_list[data->csuite_count].specifier,
			     EAP_GPSK_CIPHER_AES);
		data->csuite_count++;
	}
	if (eap_gpsk_supported_ciphersuite(EAP_GPSK_VENDOR_IETF,
					   EAP_GPSK_CIPHER_SHA256)) {
		WPA_PUT_BE24(data->csuite_list[data->csuite_count].vendor,
			     EAP_GPSK_VENDOR_IETF);
		WPA_PUT_BE24(data->csuite_list[data->csuite_count].specifier,
			     EAP_GPSK_CIPHER_SHA256);
		data->csuite_count++;
	}

	return data;
}


static void eap_gpsk_reset(struct eap_sm *sm, void *priv)
{
	struct eap_gpsk_data *data = priv;
	free(data->id_server);
	free(data->id_client);
	free(data);
}


static u8 * eap_gpsk_build_gpsk_1(struct eap_sm *sm,
				  struct eap_gpsk_data *data,
				  int id, size_t *reqDataLen)
{
	u8 *pos;
	size_t len;
	struct eap_hdr *req;

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Request/GPSK-1");

	if (hostapd_get_rand(data->rand_server, EAP_GPSK_RAND_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-GPSK: Failed to get random data");
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}
	wpa_hexdump(MSG_MSGDUMP, "EAP-GPSK: RAND_Server",
		    data->rand_server, EAP_GPSK_RAND_LEN);

	len = 1 + 2 + data->id_server_len + EAP_GPSK_RAND_LEN + 2 +
		data->csuite_count * sizeof(struct eap_gpsk_csuite);
	req = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_GPSK, reqDataLen,
			    len, EAP_CODE_REQUEST, id, &pos);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-GPSK: Failed to allocate memory "
			   "for request/GPSK-1");
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}

	*pos++ = EAP_GPSK_OPCODE_GPSK_1;

	WPA_PUT_BE16(pos, data->id_server_len);
	pos += 2;
	if (data->id_server)
		memcpy(pos, data->id_server, data->id_server_len);
	pos += data->id_server_len;

	memcpy(pos, data->rand_server, EAP_GPSK_RAND_LEN);
	pos += EAP_GPSK_RAND_LEN;

	WPA_PUT_BE16(pos, data->csuite_count * sizeof(struct eap_gpsk_csuite));
	pos += 2;
	memcpy(pos, data->csuite_list,
	       data->csuite_count * sizeof(struct eap_gpsk_csuite));

	return (u8 *) req;
}


static u8 * eap_gpsk_build_gpsk_3(struct eap_sm *sm,
				  struct eap_gpsk_data *data,
				  int id, size_t *reqDataLen)
{
	u8 *pos, *start;
	size_t len, miclen;
	struct eap_gpsk_csuite *csuite;
	struct eap_hdr *req;

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Request/GPSK-3");

	miclen = eap_gpsk_mic_len(data->vendor, data->specifier);
	len = 1 + 2 * EAP_GPSK_RAND_LEN + sizeof(struct eap_gpsk_csuite) + 2 +
		miclen;
	req = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_GPSK, reqDataLen,
			    len, EAP_CODE_REQUEST, id, &pos);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-GPSK: Failed to allocate memory "
			   "for request/GPSK-3");
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}

	*pos++ = EAP_GPSK_OPCODE_GPSK_3;
	start = pos;

	memcpy(pos, data->rand_client, EAP_GPSK_RAND_LEN);
	pos += EAP_GPSK_RAND_LEN;
	memcpy(pos, data->rand_server, EAP_GPSK_RAND_LEN);
	pos += EAP_GPSK_RAND_LEN;
	csuite = (struct eap_gpsk_csuite *) pos;
	WPA_PUT_BE24(csuite->vendor, data->vendor);
	WPA_PUT_BE24(csuite->specifier, data->specifier);
	pos += sizeof(*csuite);

	/* no PD_Payload_2 */
	WPA_PUT_BE16(pos, 0);
	pos += 2;

	if (eap_gpsk_compute_mic(data->sk, data->sk_len, data->vendor,
				 data->specifier, start, pos - start, pos) < 0)
	{
		free(req);
		eap_gpsk_state(data, FAILURE);
		return NULL;
	}

	return (u8 *) req;
}


static u8 * eap_gpsk_buildReq(struct eap_sm *sm, void *priv, int id,
			      size_t *reqDataLen)
{
	struct eap_gpsk_data *data = priv;

	switch (data->state) {
	case GPSK_1:
		return eap_gpsk_build_gpsk_1(sm, data, id, reqDataLen);
	case GPSK_3:
		return eap_gpsk_build_gpsk_3(sm, data, id, reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Unknown state %d in buildReq",
			   data->state);
		break;
	}
	return NULL;
}


static Boolean eap_gpsk_check(struct eap_sm *sm, void *priv,
			      u8 *respData, size_t respDataLen)
{
	struct eap_gpsk_data *data = priv;
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_GPSK,
			       respData, respDataLen, &len);
	if (pos == NULL || len < 1) {
		wpa_printf(MSG_INFO, "EAP-GPSK: Invalid frame");
		return TRUE;
	}

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Received frame: opcode=%d", *pos);

	if (data->state == GPSK_1 && *pos == EAP_GPSK_OPCODE_GPSK_2)
		return FALSE;

	if (data->state == GPSK_3 && *pos == EAP_GPSK_OPCODE_GPSK_4)
		return FALSE;

	wpa_printf(MSG_INFO, "EAP-GPSK: Unexpected opcode=%d in state=%d",
		   *pos, data->state);

	return TRUE;
}


static void eap_gpsk_process_gpsk_2(struct eap_sm *sm,
				    struct eap_gpsk_data *data,
				    u8 *respData, size_t respDataLen,
				    const u8 *payload, size_t payloadlen)
{
	const u8 *pos, *end;
	u16 alen;
	const struct eap_gpsk_csuite *csuite;
	size_t i, miclen;
	u8 mic[EAP_GPSK_MAX_MIC_LEN];

	if (data->state != GPSK_1)
		return;

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Received Response/GPSK-2");

	pos = payload;
	end = payload + payloadlen;

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "ID_Client length");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "ID_Client");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	free(data->id_client);
	data->id_client = malloc(alen);
	if (data->id_client == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Not enough memory to store "
			   "%d-octet ID_Client", alen);
		return;
	}
	memcpy(data->id_client, pos, alen);
	data->id_client_len = alen;
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-GPSK: ID_Client",
			  data->id_client, data->id_client_len);
	pos += alen;

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "ID_Server length");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "ID_Server");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (alen != data->id_server_len ||
	    memcmp(pos, data->id_server, alen) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: ID_Server in GPSK-1 and "
			   "GPSK-2 did not match");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	pos += alen;

	if (end - pos < EAP_GPSK_RAND_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "RAND_Client");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	memcpy(data->rand_client, pos, EAP_GPSK_RAND_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Client",
		    data->rand_client, EAP_GPSK_RAND_LEN);
	pos += EAP_GPSK_RAND_LEN;

	if (end - pos < EAP_GPSK_RAND_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "RAND_Server");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (memcmp(data->rand_server, pos, EAP_GPSK_RAND_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: RAND_Server in GPSK-1 and "
			   "GPSK-2 did not match");
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Server in GPSK-1",
			    data->rand_server, EAP_GPSK_RAND_LEN);
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: RAND_Server in GPSK-2",
			    pos, EAP_GPSK_RAND_LEN);
		eap_gpsk_state(data, FAILURE);
		return;
	}
	pos += EAP_GPSK_RAND_LEN;

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "CSuite_List length");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "CSuite_List");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (alen != data->csuite_count * sizeof(struct eap_gpsk_csuite) ||
	    memcmp(pos, data->csuite_list, alen) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: CSuite_List in GPSK-1 and "
			   "GPSK-2 did not match");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	pos += alen;

	if (end - pos < (int) sizeof(*csuite)) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "CSuite_Sel");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	csuite = (const struct eap_gpsk_csuite *) pos;
	for (i = 0; i < data->csuite_count; i++) {
		if (memcmp(csuite, &data->csuite_list[i], sizeof(*csuite)) ==
		    0)
			break;
	}
	if (i == data->csuite_count) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Peer selected unsupported "
			   "ciphersuite %d:%d",
			   WPA_GET_BE24(csuite->vendor),
			   WPA_GET_BE24(csuite->specifier));
		eap_gpsk_state(data, FAILURE);
		return;
	}
	data->vendor = WPA_GET_BE24(csuite->vendor);
	data->specifier = WPA_GET_BE24(csuite->specifier);
	wpa_printf(MSG_DEBUG, "EAP-GPSK: CSuite_Sel %d:%d",
		   data->vendor, data->specifier);
	pos += sizeof(*csuite);	

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "PD_Payload_1 length");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "PD_Payload_1");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-GPSK: PD_Payload_1", pos, alen);
	pos += alen;

	if (sm->user == NULL || sm->user->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-GPSK: No PSK/password configured "
			   "for the user");
		eap_gpsk_state(data, FAILURE);
		return;
	}

	if (eap_gpsk_derive_keys(sm->user->password, sm->user->password_len,
				 data->vendor, data->specifier,
				 data->rand_client, data->rand_server,
				 data->id_client, data->id_client_len,
				 data->id_server, data->id_server_len,
				 data->msk, data->emsk,
				 data->sk, &data->sk_len,
				 data->pk, &data->pk_len) < 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Failed to derive keys");
		eap_gpsk_state(data, FAILURE);
		return;
	}

	miclen = eap_gpsk_mic_len(data->vendor, data->specifier);
	if (end - pos < (int) miclen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for MIC "
			   "(left=%d miclen=%d)", end - pos, miclen);
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (eap_gpsk_compute_mic(data->sk, data->sk_len, data->vendor,
				 data->specifier, payload, pos - payload, mic)
	    < 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Failed to compute MIC");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (memcmp(mic, pos, miclen) != 0) {
		wpa_printf(MSG_INFO, "EAP-GPSK: Incorrect MIC in GPSK-2");
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: Received MIC", pos, miclen);
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: Computed MIC", mic, miclen);
		eap_gpsk_state(data, FAILURE);
		return;
	}
	pos += miclen;

	if (pos != end) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Ignored %d bytes of extra "
			   "data in the end of GPSK-2", end - pos);
	}

	eap_gpsk_state(data, GPSK_3);
}


static void eap_gpsk_process_gpsk_4(struct eap_sm *sm,
				    struct eap_gpsk_data *data,
				    u8 *respData, size_t respDataLen,
				    const u8 *payload, size_t payloadlen)
{
	const u8 *pos, *end;
	u16 alen;
	size_t miclen;
	u8 mic[EAP_GPSK_MAX_MIC_LEN];

	if (data->state != GPSK_3)
		return;

	wpa_printf(MSG_DEBUG, "EAP-GPSK: Received Response/GPSK-4");

	pos = payload;
	end = payload + payloadlen;

	if (end - pos < 2) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "PD_Payload_1 length");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	alen = WPA_GET_BE16(pos);
	pos += 2;
	if (end - pos < alen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Too short message for "
			   "PD_Payload_1");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-GPSK: PD_Payload_1", pos, alen);
	pos += alen;

	miclen = eap_gpsk_mic_len(data->vendor, data->specifier);
	if (end - pos < (int) miclen) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Message too short for MIC "
			   "(left=%d miclen=%d)", end - pos, miclen);
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (eap_gpsk_compute_mic(data->sk, data->sk_len, data->vendor,
				 data->specifier, payload, pos - payload, mic)
	    < 0) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Failed to compute MIC");
		eap_gpsk_state(data, FAILURE);
		return;
	}
	if (memcmp(mic, pos, miclen) != 0) {
		wpa_printf(MSG_INFO, "EAP-GPSK: Incorrect MIC in GPSK-4");
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: Received MIC", pos, miclen);
		wpa_hexdump(MSG_DEBUG, "EAP-GPSK: Computed MIC", mic, miclen);
		eap_gpsk_state(data, FAILURE);
		return;
	}
	pos += miclen;

	if (pos != end) {
		wpa_printf(MSG_DEBUG, "EAP-GPSK: Ignored %d bytes of extra "
			   "data in the end of GPSK-4", end - pos);
	}

	eap_gpsk_state(data, SUCCESS);
}


static void eap_gpsk_process(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_gpsk_data *data = priv;
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_GPSK,
			       respData, respDataLen, &len);
	if (pos == NULL || len < 1)
		return;

	switch (*pos) {
	case EAP_GPSK_OPCODE_GPSK_2:
		eap_gpsk_process_gpsk_2(sm, data, respData, respDataLen,
					pos + 1, len - 1);
		break;
	case EAP_GPSK_OPCODE_GPSK_4:
		eap_gpsk_process_gpsk_4(sm, data, respData, respDataLen,
					pos + 1, len - 1);
		break;
	}
}


static Boolean eap_gpsk_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_gpsk_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_gpsk_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_gpsk_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(EAP_MSK_LEN);
	if (key == NULL)
		return NULL;
	memcpy(key, data->msk, EAP_MSK_LEN);
	*len = EAP_MSK_LEN;

	return key;
}


static u8 * eap_gpsk_get_emsk(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_gpsk_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(EAP_EMSK_LEN);
	if (key == NULL)
		return NULL;
	memcpy(key, data->emsk, EAP_EMSK_LEN);
	*len = EAP_EMSK_LEN;

	return key;
}


static Boolean eap_gpsk_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_gpsk_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_gpsk_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_GPSK, "GPSK");
	if (eap == NULL)
		return -1;

	eap->init = eap_gpsk_init;
	eap->reset = eap_gpsk_reset;
	eap->buildReq = eap_gpsk_buildReq;
	eap->check = eap_gpsk_check;
	eap->process = eap_gpsk_process;
	eap->isDone = eap_gpsk_isDone;
	eap->getKey = eap_gpsk_getKey;
	eap->isSuccess = eap_gpsk_isSuccess;
	eap->get_emsk = eap_gpsk_get_emsk;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
