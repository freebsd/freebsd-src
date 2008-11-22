/*
 * hostapd / EAP-PSK (draft-bersani-eap-psk-09.txt) server
 * Copyright (c) 2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * Note: EAP-PSK is an EAP authentication method and as such, completely
 * different from WPA-PSK. This file is not needed for WPA-PSK functionality.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
#include "aes_wrap.h"
#include "eap_psk_common.h"


struct eap_psk_data {
	enum { PSK_1, PSK_3, SUCCESS, FAILURE } state;
	u8 rand_s[EAP_PSK_RAND_LEN];
	u8 rand_p[EAP_PSK_RAND_LEN];
	u8 *id_p, *id_s;
	size_t id_p_len, id_s_len;
	u8 ak[EAP_PSK_AK_LEN], kdk[EAP_PSK_KDK_LEN], tek[EAP_PSK_TEK_LEN];
	u8 msk[EAP_PSK_MSK_LEN];
};


static void * eap_psk_init(struct eap_sm *sm)
{
	struct eap_psk_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = PSK_1;
	data->id_s = "hostapd";
	data->id_s_len = 7;

	return data;
}


static void eap_psk_reset(struct eap_sm *sm, void *priv)
{
	struct eap_psk_data *data = priv;
	free(data->id_p);
	free(data);
}


static u8 * eap_psk_build_1(struct eap_sm *sm, struct eap_psk_data *data,
			    int id, size_t *reqDataLen)
{
	struct eap_psk_hdr_1 *req;

	wpa_printf(MSG_DEBUG, "EAP-PSK: PSK-1 (sending)");

	if (hostapd_get_rand(data->rand_s, EAP_PSK_RAND_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-PSK: Failed to get random data");
		data->state = FAILURE;
		return NULL;
	}
	wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: RAND_S (server rand)",
		    data->rand_s, EAP_PSK_RAND_LEN);

	*reqDataLen = sizeof(*req) + data->id_s_len;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PSK: Failed to allocate memory "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_PSK;
	req->flags = 0; /* T=0 */
	memcpy(req->rand_s, data->rand_s, EAP_PSK_RAND_LEN);
	memcpy((u8 *) (req + 1), data->id_s, data->id_s_len);

	return (u8 *) req;
}


static u8 * eap_psk_build_3(struct eap_sm *sm, struct eap_psk_data *data,
			    int id, size_t *reqDataLen)
{
	struct eap_psk_hdr_3 *req;
	u8 *buf, *pchannel, nonce[16];
	size_t buflen;

	wpa_printf(MSG_DEBUG, "EAP-PSK: PSK-3 (sending)");

	*reqDataLen = sizeof(*req) + 4 + 16 + 1;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PSK: Failed to allocate memory "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_PSK;
	req->flags = 2; /* T=2 */
	memcpy(req->rand_s, data->rand_s, EAP_PSK_RAND_LEN);

	/* MAC_S = OMAC1-AES-128(AK, ID_S||RAND_P) */
	buflen = data->id_s_len + EAP_PSK_RAND_LEN;
	buf = malloc(buflen);
	if (buf == NULL) {
		data->state = FAILURE;
		return NULL;
	}
	memcpy(buf, data->id_s, data->id_s_len);
	memcpy(buf + data->id_s_len, data->rand_p, EAP_PSK_RAND_LEN);
	omac1_aes_128(data->ak, buf, buflen, req->mac_s);
	free(buf);

	eap_psk_derive_keys(data->kdk, data->rand_p, data->tek, data->msk);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: TEK", data->tek, EAP_PSK_TEK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: MSK", data->msk, EAP_PSK_MSK_LEN);

	memset(nonce, 0, sizeof(nonce));
	pchannel = (u8 *) (req + 1);
	memcpy(pchannel, nonce + 12, 4);
	memset(pchannel + 4, 0, 16); /* Tag */
	pchannel[4 + 16] = EAP_PSK_R_FLAG_DONE_SUCCESS << 6;
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: PCHANNEL (plaintext)",
		    pchannel, 4 + 16 + 1);
	aes_128_eax_encrypt(data->tek, nonce, sizeof(nonce), (u8 *) req, 22,
			    pchannel + 4 + 16, 1, pchannel + 4);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: PCHANNEL (encrypted)",
		    pchannel, 4 + 16 + 1);

	return (u8 *) req;
}


static u8 * eap_psk_buildReq(struct eap_sm *sm, void *priv, int id,
				  size_t *reqDataLen)
{
	struct eap_psk_data *data = priv;

	switch (data->state) {
	case PSK_1:
		return eap_psk_build_1(sm, data, id, reqDataLen);
	case PSK_3:
		return eap_psk_build_3(sm, data, id, reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-PSK: Unknown state %d in buildReq",
			   data->state);
		break;
	}
	return NULL;
}


static Boolean eap_psk_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_psk_data *data = priv;
	struct eap_psk_hdr *resp;
	size_t len;
	u8 t;

	resp = (struct eap_psk_hdr *) respData;
	if (respDataLen < sizeof(*resp) || resp->type != EAP_TYPE_PSK ||
	    (len = ntohs(resp->length)) > respDataLen ||
	    len < sizeof(*resp)) {
		wpa_printf(MSG_INFO, "EAP-PSK: Invalid frame");
		return TRUE;
	}
	t = resp->flags & 0x03;

	wpa_printf(MSG_DEBUG, "EAP-PSK: received frame: T=%d", t);

	if (data->state == PSK_1 && t != 1) {
		wpa_printf(MSG_DEBUG, "EAP-PSK: Expected PSK-2 - "
			   "ignore T=%d", t);
		return TRUE;
	}

	if (data->state == PSK_3 && t != 3) {
		wpa_printf(MSG_DEBUG, "EAP-PSK: Expected PSK-4 - "
			   "ignore T=%d", t);
		return TRUE;
	}

	if ((t == 1 && len < sizeof(struct eap_psk_hdr_2)) ||
	    (t == 3 && len < sizeof(struct eap_psk_hdr_4))) {
		wpa_printf(MSG_DEBUG, "EAP-PSK: Too short frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_psk_process_2(struct eap_sm *sm,
			      struct eap_psk_data *data,
			      u8 *respData, size_t respDataLen)
{
	struct eap_psk_hdr_2 *resp;
	u8 *pos, mac[EAP_PSK_MAC_LEN], *buf;
	size_t len, left, buflen;
	int i;

	if (data->state != PSK_1)
		return;

	wpa_printf(MSG_DEBUG, "EAP-PSK: Received PSK-2");

	resp = (struct eap_psk_hdr_2 *) respData;
	len = ntohs(resp->length);
	pos = (u8 *) (resp + 1);
	left = len - sizeof(*resp);

	free(data->id_p);
	data->id_p = malloc(left);
	if (data->id_p == NULL) {
		wpa_printf(MSG_INFO, "EAP-PSK: Failed to allocate memory for "
			   "ID_P");
		return;
	}
	memcpy(data->id_p, pos, left);
	data->id_p_len = left;
	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-PSK: ID_P",
			  data->id_p, data->id_p_len);

	if (eap_user_get(sm, data->id_p, data->id_p_len, 0) < 0) {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-PSK: unknown ID_P",
				  data->id_p, data->id_p_len);
		data->state = FAILURE;
		return;
	}

	for (i = 0;
	     i < EAP_MAX_METHODS && sm->user->methods[i] != EAP_TYPE_NONE;
	     i++) {
		if (sm->user->methods[i] == EAP_TYPE_PSK)
			break;
	}

	if (sm->user->methods[i] != EAP_TYPE_PSK) {
		wpa_hexdump_ascii(MSG_DEBUG,
				  "EAP-PSK: EAP-PSK not enabled for ID_P",
				  data->id_p, data->id_p_len);
		data->state = FAILURE;
		return;
	}

	if (sm->user->password == NULL ||
	    sm->user->password_len != EAP_PSK_PSK_LEN) {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-PSK: invalid password in "
				  "user database for ID_P",
				  data->id_p, data->id_p_len);
		data->state = FAILURE;
		return;
	}
	eap_psk_key_setup(sm->user->password, data->ak, data->kdk);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: AK", data->ak, EAP_PSK_AK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: KDK", data->kdk, EAP_PSK_KDK_LEN);

	wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: RAND_P (client rand)",
		    resp->rand_p, EAP_PSK_RAND_LEN);
	memcpy(data->rand_p, resp->rand_p, EAP_PSK_RAND_LEN);

	/* MAC_P = OMAC1-AES-128(AK, ID_P||ID_S||RAND_S||RAND_P) */
	buflen = data->id_p_len + data->id_s_len + 2 * EAP_PSK_RAND_LEN;
	buf = malloc(buflen);
	if (buf == NULL) {
		data->state = FAILURE;
		return;
	}
	memcpy(buf, data->id_p, data->id_p_len);
	pos = buf + data->id_p_len;
	memcpy(pos, data->id_s, data->id_s_len);
	pos += data->id_s_len;
	memcpy(pos, data->rand_s, EAP_PSK_RAND_LEN);
	pos += EAP_PSK_RAND_LEN;
	memcpy(pos, data->rand_p, EAP_PSK_RAND_LEN);
	omac1_aes_128(data->ak, buf, buflen, mac);
	free(buf);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: MAC_P", resp->mac_p, EAP_PSK_MAC_LEN);
	if (memcmp(mac, resp->mac_p, EAP_PSK_MAC_LEN) != 0) {
		wpa_printf(MSG_INFO, "EAP-PSK: Invalid MAC_P");
		wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: Expected MAC_P",
			    mac, EAP_PSK_MAC_LEN);
		data->state = FAILURE;
		return;
	}

	data->state = PSK_3;
}


static void eap_psk_process_4(struct eap_sm *sm,
			      struct eap_psk_data *data,
			      u8 *respData, size_t respDataLen)
{
	struct eap_psk_hdr_4 *resp;
	u8 *pos, *decrypted, nonce[16], *tag;
	size_t left;

	if (data->state != PSK_3)
		return;

	wpa_printf(MSG_DEBUG, "EAP-PSK: Received PSK-4");

	resp = (struct eap_psk_hdr_4 *) respData;
	pos = (u8 *) (resp + 1);
	left = ntohs(resp->length) - sizeof(*resp);

	wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: Encrypted PCHANNEL", pos, left);

	if (left < 4 + 16 + 1) {
		wpa_printf(MSG_INFO, "EAP-PSK: Too short PCHANNEL data in "
			   "PSK-4 (len=%lu, expected 21)",
			   (unsigned long) left);
		return;
	}

	if (pos[0] == 0 && pos[1] == 0 && pos[2] == 0 && pos[3] == 0) {
		wpa_printf(MSG_DEBUG, "EAP-PSK: Nonce did not increase");
		return;
	}

	memset(nonce, 0, 12);
	memcpy(nonce + 12, pos, 4);
	pos += 4;
	left -= 4;
	tag = pos;
	pos += 16;
	left -= 16;

	decrypted = malloc(left);
	if (decrypted == NULL)
		return;
	memcpy(decrypted, pos, left);

	if (aes_128_eax_decrypt(data->tek, nonce, sizeof(nonce),
				respData, 22, decrypted, left, tag)) {
		wpa_printf(MSG_WARNING, "EAP-PSK: PCHANNEL decryption failed");
		free(decrypted);
		data->state = FAILURE;
		return;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: Decrypted PCHANNEL message",
		    decrypted, left);

	/* Verify R flag */
	switch (decrypted[0] >> 6) {
	case EAP_PSK_R_FLAG_CONT:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - CONT - unsupported");
		data->state = FAILURE;
		break;
	case EAP_PSK_R_FLAG_DONE_SUCCESS:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - DONE_SUCCESS");
		data->state = SUCCESS;
		break;
	case EAP_PSK_R_FLAG_DONE_FAILURE:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - DONE_FAILURE");
		data->state = FAILURE;
		break;
	}
	free(decrypted);
}


static void eap_psk_process(struct eap_sm *sm, void *priv,
				 u8 *respData, size_t respDataLen)
{
	struct eap_psk_data *data = priv;
	struct eap_psk_hdr *resp;

	if (sm->user == NULL || sm->user->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-MSCHAPV2: Password not configured");
		data->state = FAILURE;
		return;
	}

	resp = (struct eap_psk_hdr *) respData;

	switch (resp->flags & 0x03) {
	case 1:
		eap_psk_process_2(sm, data, respData, respDataLen);
		break;
	case 3:
		eap_psk_process_4(sm, data, respData, respDataLen);
		break;
	}
}


static Boolean eap_psk_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_psk_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_psk_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_psk_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(EAP_PSK_MSK_LEN);
	if (key == NULL)
		return NULL;
	memcpy(key, data->msk, EAP_PSK_MSK_LEN);
	*len = EAP_PSK_MSK_LEN;

	return key;
}


static Boolean eap_psk_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_psk_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_psk =
{
	.method = EAP_TYPE_PSK,
	.name = "PSK",
	.init = eap_psk_init,
	.reset = eap_psk_reset,
	.buildReq = eap_psk_buildReq,
	.check = eap_psk_check,
	.process = eap_psk_process,
	.isDone = eap_psk_isDone,
	.getKey = eap_psk_getKey,
	.isSuccess = eap_psk_isSuccess,
};
