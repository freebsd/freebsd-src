/*
 * WPA Supplicant / EAP-PSK (draft-bersani-eap-psk-09.txt)
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
 *
 * Note: EAP-PSK is an EAP authentication method and as such, completely
 * different from WPA-PSK. This file is not needed for WPA-PSK functionality.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "eap_i.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "md5.h"
#include "aes_wrap.h"
#include "eap_psk_common.h"


struct eap_psk_data {
	enum { PSK_INIT, PSK_MAC_SENT, PSK_DONE } state;
	u8 rand_p[EAP_PSK_RAND_LEN];
	u8 ak[EAP_PSK_AK_LEN], kdk[EAP_PSK_KDK_LEN], tek[EAP_PSK_TEK_LEN];
	u8 *id_s, *id_p;
	size_t id_s_len, id_p_len;
	u8 key_data[EAP_PSK_MSK_LEN];
};


static void * eap_psk_init(struct eap_sm *sm)
{
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_psk_data *data;

	if (config == NULL || !config->eappsk) {
		wpa_printf(MSG_INFO, "EAP-PSK: pre-shared key not configured");
		return NULL;
	}

	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));
	eap_psk_key_setup(config->eappsk, data->ak, data->kdk);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: AK", data->ak, EAP_PSK_AK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: KDK", data->kdk, EAP_PSK_KDK_LEN);
	data->state = PSK_INIT;

	if (config->nai) {
		data->id_p = malloc(config->nai_len);
		if (data->id_p)
			memcpy(data->id_p, config->nai, config->nai_len);
		data->id_p_len = config->nai_len;
	}
	if (data->id_p == NULL) {
		wpa_printf(MSG_INFO, "EAP-PSK: could not get own identity");
		free(data);
		return NULL;
	}

	return data;
}


static void eap_psk_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_psk_data *data = priv;
	free(data->id_s);
	free(data->id_p);
	free(data);
}


static u8 * eap_psk_process_1(struct eap_sm *sm, struct eap_psk_data *data,
			      struct eap_method_ret *ret,
			      const u8 *reqData, size_t reqDataLen,
			      size_t *respDataLen)
{
	const struct eap_psk_hdr_1 *hdr1;
	struct eap_psk_hdr_2 *hdr2;
	u8 *resp, *buf, *pos;
	size_t buflen;

	wpa_printf(MSG_DEBUG, "EAP-PSK: in INIT state");

	hdr1 = (const struct eap_psk_hdr_1 *) reqData;
	if (reqDataLen < sizeof(*hdr1) ||
	    be_to_host16(hdr1->length) < sizeof(*hdr1) ||
	    be_to_host16(hdr1->length) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-PSK: Invalid first message "
			   "length (%lu %d; expected %lu or more)",
			   (unsigned long) reqDataLen,
			   be_to_host16(hdr1->length),
			   (unsigned long) sizeof(*hdr1));
		ret->ignore = TRUE;
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "EAP-PSK: Flags=0x%x", hdr1->flags);
	if ((hdr1->flags & 0x03) != 0) {
		wpa_printf(MSG_INFO, "EAP-PSK: Unexpected T=%d (expected 0)",
			   hdr1->flags & 0x03);
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: RAND_S", hdr1->rand_s,
		    EAP_PSK_RAND_LEN);
	free(data->id_s);
	data->id_s_len = be_to_host16(hdr1->length) - sizeof(*hdr1);
	data->id_s = malloc(data->id_s_len);
	if (data->id_s == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PSK: Failed to allocate memory for "
			   "ID_S (len=%lu)", (unsigned long) data->id_s_len);
		ret->ignore = TRUE;
		return NULL;
	}
	memcpy(data->id_s, (u8 *) (hdr1 + 1), data->id_s_len);
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-PSK: ID_S",
			  data->id_s, data->id_s_len);

	if (hostapd_get_rand(data->rand_p, EAP_PSK_RAND_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-PSK: Failed to get random data");
		ret->ignore = TRUE;
		return NULL;
	}

	*respDataLen = sizeof(*hdr2) + data->id_p_len;
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	hdr2 = (struct eap_psk_hdr_2 *) resp;
	hdr2->code = EAP_CODE_RESPONSE;
	hdr2->identifier = hdr1->identifier;
	hdr2->length = host_to_be16(*respDataLen);
	hdr2->type = EAP_TYPE_PSK;
	hdr2->flags = 1; /* T=1 */
	memcpy(hdr2->rand_s, hdr1->rand_s, EAP_PSK_RAND_LEN);
	memcpy(hdr2->rand_p, data->rand_p, EAP_PSK_RAND_LEN);
	memcpy((u8 *) (hdr2 + 1), data->id_p, data->id_p_len);
	/* MAC_P = OMAC1-AES-128(AK, ID_P||ID_S||RAND_S||RAND_P) */
	buflen = data->id_p_len + data->id_s_len + 2 * EAP_PSK_RAND_LEN;
	buf = malloc(buflen);
	if (buf == NULL) {
		free(resp);
		return NULL;
	}
	memcpy(buf, data->id_p, data->id_p_len);
	pos = buf + data->id_p_len;
	memcpy(pos, data->id_s, data->id_s_len);
	pos += data->id_s_len;
	memcpy(pos, hdr1->rand_s, EAP_PSK_RAND_LEN);
	pos += EAP_PSK_RAND_LEN;
	memcpy(pos, data->rand_p, EAP_PSK_RAND_LEN);
	omac1_aes_128(data->ak, buf, buflen, hdr2->mac_p);
	free(buf);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: RAND_P", hdr2->rand_p,
		    EAP_PSK_RAND_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: MAC_P", hdr2->mac_p, EAP_PSK_MAC_LEN);
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-PSK: ID_P",
			  (u8 *) (hdr2 + 1), data->id_p_len);

	data->state = PSK_MAC_SENT;

	return resp;
}


static u8 * eap_psk_process_3(struct eap_sm *sm, struct eap_psk_data *data,
			      struct eap_method_ret *ret,
			      const u8 *reqData, size_t reqDataLen,
			      size_t *respDataLen)
{
	const struct eap_psk_hdr_3 *hdr3;
	struct eap_psk_hdr_4 *hdr4;
	u8 *resp, *buf, *rpchannel, nonce[16], *decrypted;
	const u8 *pchannel, *tag, *msg;
	u8 mac[EAP_PSK_MAC_LEN];
	size_t buflen, left, data_len;
	int failed = 0;

	wpa_printf(MSG_DEBUG, "EAP-PSK: in MAC_SENT state");

	hdr3 = (const struct eap_psk_hdr_3 *) reqData;
	left = be_to_host16(hdr3->length);
	if (left < sizeof(*hdr3) || reqDataLen < left) {
		wpa_printf(MSG_INFO, "EAP-PSK: Invalid third message "
			   "length (%lu %d; expected %lu)",
			   (unsigned long) reqDataLen,
			   be_to_host16(hdr3->length),
			   (unsigned long) sizeof(*hdr3));
		ret->ignore = TRUE;
		return NULL;
	}
	left -= sizeof(*hdr3);
	pchannel = (const u8 *) (hdr3 + 1);
	wpa_printf(MSG_DEBUG, "EAP-PSK: Flags=0x%x", hdr3->flags);
	if ((hdr3->flags & 0x03) != 2) {
		wpa_printf(MSG_INFO, "EAP-PSK: Unexpected T=%d (expected 2)",
			   hdr3->flags & 0x03);
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: RAND_S", hdr3->rand_s,
		    EAP_PSK_RAND_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: MAC_S", hdr3->mac_s, EAP_PSK_MAC_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: PCHANNEL", pchannel, left);

	if (left < 4 + 16 + 1) {
		wpa_printf(MSG_INFO, "EAP-PSK: Too short PCHANNEL data in "
			   "third message (len=%lu, expected 21)",
			   (unsigned long) left);
		ret->ignore = TRUE;
		return NULL;
	}

	/* MAC_S = OMAC1-AES-128(AK, ID_S||RAND_P) */
	buflen = data->id_s_len + EAP_PSK_RAND_LEN;
	buf = malloc(buflen);
	if (buf == NULL)
		return NULL;
	memcpy(buf, data->id_s, data->id_s_len);
	memcpy(buf + data->id_s_len, data->rand_p, EAP_PSK_RAND_LEN);
	omac1_aes_128(data->ak, buf, buflen, mac);
	free(buf);
	if (memcmp(mac, hdr3->mac_s, EAP_PSK_MAC_LEN) != 0) {
		wpa_printf(MSG_WARNING, "EAP-PSK: Invalid MAC_S in third "
			   "message");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "EAP-PSK: MAC_S verified successfully");

	eap_psk_derive_keys(data->kdk, data->rand_p, data->tek,
			    data->key_data);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: TEK", data->tek, EAP_PSK_TEK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: MSK", data->key_data,
			EAP_PSK_MSK_LEN);

	memset(nonce, 0, 12);
	memcpy(nonce + 12, pchannel, 4);
	pchannel += 4;
	left -= 4;

	tag = pchannel;
	pchannel += 16;
	left -= 16;

	msg = pchannel;

	wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: PCHANNEL - nonce",
		    nonce, sizeof(nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: PCHANNEL - hdr", reqData, 5);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PSK: PCHANNEL - cipher msg", msg, left);

	decrypted = malloc(left);
	if (decrypted == NULL) {
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
	memcpy(decrypted, msg, left);

	if (aes_128_eax_decrypt(data->tek, nonce, sizeof(nonce),
				reqData, 22, decrypted, left, tag)) {
		wpa_printf(MSG_WARNING, "EAP-PSK: PCHANNEL decryption failed");
		free(decrypted);
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: Decrypted PCHANNEL message",
		    decrypted, left);

	/* Verify R flag */
	switch (decrypted[0] >> 6) {
	case EAP_PSK_R_FLAG_CONT:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - CONT - unsupported");
		failed = 1;
		break;
	case EAP_PSK_R_FLAG_DONE_SUCCESS:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - DONE_SUCCESS");
		break;
	case EAP_PSK_R_FLAG_DONE_FAILURE:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - DONE_FAILURE");
		wpa_printf(MSG_INFO, "EAP-PSK: Authentication server rejected "
			   "authentication");
		failed = 1;
		break;
	}

	*respDataLen = sizeof(*hdr4) + 4 + 16 + 1;
	resp = malloc(*respDataLen + 1);
	if (resp == NULL) {
		free(decrypted);
		return NULL;
	}
	hdr4 = (struct eap_psk_hdr_4 *) resp;
	hdr4->code = EAP_CODE_RESPONSE;
	hdr4->identifier = hdr3->identifier;
	hdr4->length = host_to_be16(*respDataLen);
	hdr4->type = EAP_TYPE_PSK;
	hdr4->flags = 3; /* T=3 */
	memcpy(hdr4->rand_s, hdr3->rand_s, EAP_PSK_RAND_LEN);
	rpchannel = (u8 *) (hdr4 + 1);

	/* nonce++ */
	inc_byte_array(nonce, sizeof(nonce));
	memcpy(rpchannel, nonce + 12, 4);

	data_len = 1;
	if (decrypted[0] & EAP_PSK_E_FLAG) {
		wpa_printf(MSG_DEBUG, "EAP-PSK: Unsupported E (Ext) flag");
		failed = 1;
		rpchannel[4 + 16] = (EAP_PSK_R_FLAG_DONE_FAILURE << 6) |
			EAP_PSK_E_FLAG;
		if (left > 1) {
			/* Add empty EXT_Payload with same EXT_Type */
			(*respDataLen)++;
			hdr4->length = host_to_be16(*respDataLen);
			rpchannel[4 + 16 + 1] = decrypted[1];
			data_len++;
		}
	} else if (failed)
		rpchannel[4 + 16] = EAP_PSK_R_FLAG_DONE_FAILURE << 6;
	else
		rpchannel[4 + 16] = EAP_PSK_R_FLAG_DONE_SUCCESS << 6;

	wpa_hexdump(MSG_DEBUG, "EAP-PSK: reply message (plaintext)",
		    rpchannel + 4 + 16, data_len);
	aes_128_eax_encrypt(data->tek, nonce, sizeof(nonce), resp, 22,
			    rpchannel + 4 + 16, data_len, rpchannel + 4);
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: reply message (PCHANNEL)",
		    rpchannel, 4 + 16 + data_len);

	wpa_printf(MSG_DEBUG, "EAP-PSK: Completed %ssuccessfully",
		   failed ? "un" : "");
	data->state = PSK_DONE;
	ret->methodState = METHOD_DONE;
	ret->decision = failed ? DECISION_FAIL : DECISION_UNCOND_SUCC;

	free(decrypted);

	return resp;
}


static u8 * eap_psk_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    const u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_psk_data *data = priv;
	const u8 *pos;
	u8 *resp = NULL;
	size_t len;

	pos = eap_hdr_validate(EAP_TYPE_PSK, reqData, reqDataLen, &len);
	if (pos == NULL) {
		ret->ignore = TRUE;
		return NULL;
	}
	len += sizeof(struct eap_hdr) + 1;

	ret->ignore = FALSE;
	ret->methodState = METHOD_MAY_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	switch (data->state) {
	case PSK_INIT:
		resp = eap_psk_process_1(sm, data, ret, reqData, len,
					 respDataLen);
		break;
	case PSK_MAC_SENT:
		resp = eap_psk_process_3(sm, data, ret, reqData, len,
					 respDataLen);
		break;
	case PSK_DONE:
		wpa_printf(MSG_DEBUG, "EAP-PSK: in DONE state - ignore "
			   "unexpected message");
		ret->ignore = TRUE;
		return NULL;
	}

	if (ret->methodState == METHOD_DONE) {
		ret->allowNotifications = FALSE;
	}

	return resp;
}


static Boolean eap_psk_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_psk_data *data = priv;
	return data->state == PSK_DONE;
}


static u8 * eap_psk_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_psk_data *data = priv;
	u8 *key;

	if (data->state != PSK_DONE)
		return NULL;

	key = malloc(EAP_PSK_MSK_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_PSK_MSK_LEN;
	memcpy(key, data->key_data, EAP_PSK_MSK_LEN);

	return key;
}


const struct eap_method eap_method_psk =
{
	.method = EAP_TYPE_PSK,
	.name = "PSK",
	.init = eap_psk_init,
	.deinit = eap_psk_deinit,
	.process = eap_psk_process,
	.isKeyAvailable = eap_psk_isKeyAvailable,
	.getKey = eap_psk_getKey,
};
