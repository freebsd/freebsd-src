/*
 * WPA Supplicant / EAP-PSK (draft-bersani-eap-psk-05.txt)
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
#include "md5.h"
#include "aes_wrap.h"


/* draft-bersani-eap-psk-03.txt mode. This is retained for interop testing and
 * will be removed once an AS that supports draft5 becomes available. */
#define EAP_PSK_DRAFT3

#define EAP_PSK_RAND_LEN 16
#define EAP_PSK_MAC_LEN 16
#define EAP_PSK_TEK_LEN 16
#define EAP_PSK_MSK_LEN 64

#define EAP_PSK_R_FLAG_CONT 1
#define EAP_PSK_R_FLAG_DONE_SUCCESS 2
#define EAP_PSK_R_FLAG_DONE_FAILURE 3

/* EAP-PSK First Message (AS -> Supplicant) */
struct eap_psk_hdr_1 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
#ifndef EAP_PSK_DRAFT3
	u8 flags;
#endif /* EAP_PSK_DRAFT3 */
	u8 rand_s[EAP_PSK_RAND_LEN];
#ifndef EAP_PSK_DRAFT3
	/* Followed by variable length ID_S */
#endif /* EAP_PSK_DRAFT3 */
} __attribute__ ((packed));

/* EAP-PSK Second Message (Supplicant -> AS) */
struct eap_psk_hdr_2 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
#ifndef EAP_PSK_DRAFT3
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
#endif /* EAP_PSK_DRAFT3 */
	u8 rand_p[EAP_PSK_RAND_LEN];
	u8 mac_p[EAP_PSK_MAC_LEN];
	/* Followed by variable length ID_P */
} __attribute__ ((packed));

/* EAP-PSK Third Message (AS -> Supplicant) */
struct eap_psk_hdr_3 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
#ifndef EAP_PSK_DRAFT3
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
#endif /* EAP_PSK_DRAFT3 */
	u8 mac_s[EAP_PSK_MAC_LEN];
	/* Followed by variable length PCHANNEL */
} __attribute__ ((packed));

/* EAP-PSK Fourth Message (Supplicant -> AS) */
struct eap_psk_hdr_4 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
#ifndef EAP_PSK_DRAFT3
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
#endif /* EAP_PSK_DRAFT3 */
	/* Followed by variable length PCHANNEL */
} __attribute__ ((packed));



struct eap_psk_data {
	enum { PSK_INIT, PSK_MAC_SENT, PSK_DONE } state;
	u8 rand_s[EAP_PSK_RAND_LEN];
	u8 rand_p[EAP_PSK_RAND_LEN];
	u8 ak[16], kdk[16], tek[EAP_PSK_TEK_LEN];
	u8 *id_s, *id_p;
	size_t id_s_len, id_p_len;
	u8 key_data[EAP_PSK_MSK_LEN];
};


#define aes_block_size 16


static void eap_psk_key_setup(const u8 *psk, u8 *ak, u8 *kdk)
{
	memset(ak, 0, aes_block_size);
	aes_128_encrypt_block(psk, ak, ak);
	memcpy(kdk, ak, aes_block_size);
	ak[aes_block_size - 1] ^= 0x01;
	kdk[aes_block_size - 1] ^= 0x02;
	aes_128_encrypt_block(psk, ak, ak);
	aes_128_encrypt_block(psk, kdk, kdk);
}


static void eap_psk_derive_keys(const u8 *kdk, const u8 *rb, u8 *tek, u8 *msk)
{
	u8 hash[aes_block_size];
	u8 counter = 1;
	int i;

	aes_128_encrypt_block(kdk, rb, hash);

	hash[aes_block_size - 1] ^= counter;
	aes_128_encrypt_block(kdk, hash, tek);
	hash[aes_block_size - 1] ^= counter;
	counter++;

	for (i = 0; i < EAP_PSK_MSK_LEN / aes_block_size; i++) {
		hash[aes_block_size - 1] ^= counter;
		aes_128_encrypt_block(kdk, hash, &msk[i * aes_block_size]);
		hash[aes_block_size - 1] ^= counter;
		counter++;
	}
}


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
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: AK", data->ak, 16);
	wpa_hexdump_key(MSG_DEBUG, "EAP-PSK: KDK", data->kdk, 16);
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

#ifdef EAP_PSK_DRAFT3
	if (config->server_nai) {
		data->id_s = malloc(config->server_nai_len);
		if (data->id_s)
			memcpy(data->id_s, config->server_nai,
			       config->server_nai_len);
		data->id_s_len = config->server_nai_len;
	}
	if (data->id_s == NULL) {
		wpa_printf(MSG_INFO, "EAP-PSK: could not get server identity");
		free(data->id_p);
		free(data);
		return NULL;
	}
#endif /* EAP_PSK_DRAFT3 */

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
			      u8 *reqData, size_t reqDataLen,
			      size_t *respDataLen)
{
	struct eap_psk_hdr_1 *hdr1;
	struct eap_psk_hdr_2 *hdr2;
	u8 *resp, *buf, *pos;
	size_t buflen;

	wpa_printf(MSG_DEBUG, "EAP-PSK: in INIT state");

	hdr1 = (struct eap_psk_hdr_1 *) reqData;
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
#ifndef EAP_PSK_DRAFT3
	wpa_printf(MSG_DEBUG, "EAP-PSK: Flags=0x%x", hdr1->flags);
	if ((hdr1->flags & 0x03) != 0) {
		wpa_printf(MSG_INFO, "EAP-PSK: Unexpected T=%d (expected 0)",
			   hdr1->flags & 0x03);
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
#endif /* EAP_PSK_DRAFT3 */
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: RAND_S", hdr1->rand_s,
		    EAP_PSK_RAND_LEN);
	memcpy(data->rand_s, hdr1->rand_s, EAP_PSK_RAND_LEN);
#ifndef EAP_PSK_DRAFT3
	free(data->id_s);
	data->id_s_len = be_to_host16(hdr1->length) - sizeof(*hdr1);
	data->id_s = malloc(data->id_s_len);
	if (data->id_s == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PSK: Failed to allocate memory for "
			   "ID_S (len=%d)", data->id_s_len);
		ret->ignore = TRUE;
		return NULL;
	}
	memcpy(data->id_s, (u8 *) (hdr1 + 1), data->id_s_len);
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-PSK: ID_S",
			  data->id_s, data->id_s_len);
#endif /* EAP_PSK_DRAFT3 */

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
#ifndef EAP_PSK_DRAFT3
	hdr2->flags = 1; /* T=1 */
	memcpy(hdr2->rand_s, hdr1->rand_s, EAP_PSK_RAND_LEN);
#endif /* EAP_PSK_DRAFT3 */
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
	memcpy(pos, data->rand_s, EAP_PSK_RAND_LEN);
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
			      u8 *reqData, size_t reqDataLen,
			      size_t *respDataLen)
{
	struct eap_psk_hdr_3 *hdr3;
	struct eap_psk_hdr_4 *hdr4;
	u8 *resp, *buf, *pchannel, *tag, *msg, nonce[16];
	u8 mac[EAP_PSK_MAC_LEN];
	size_t buflen, left;
	int failed = 0;

	wpa_printf(MSG_DEBUG, "EAP-PSK: in MAC_SENT state");

	hdr3 = (struct eap_psk_hdr_3 *) reqData;
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
	pchannel = (u8 *) (hdr3 + 1);
#ifndef EAP_PSK_DRAFT3
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
	/* TODO: would not need to store RAND_S since it is available in this
	 * message. For now, since we store this anyway, verify that it matches
	 * with whatever the server is sending. */
	if (memcmp(hdr3->rand_s, data->rand_s, EAP_PSK_RAND_LEN) != 0) {
		wpa_printf(MSG_ERROR, "EAP-PSK: RAND_S did not match");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}
#endif /* EAP_PSK_DRAFT3 */
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

#ifdef EAP_PSK_DRAFT3
	if (aes_128_eax_decrypt(data->tek, nonce, sizeof(nonce),
				reqData, 5, msg, left, tag))
#else /* EAP_PSK_DRAFT3 */
	if (aes_128_eax_decrypt(data->tek, nonce, sizeof(nonce),
				reqData, 22, msg, left, tag))
#endif /* EAP_PSK_DRAFT3 */
	{
		wpa_printf(MSG_WARNING, "EAP-PSK: PCHANNEL decryption failed");
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: Decrypted PCHANNEL message",
		    msg, left);

	/* Verify R flag */
	switch (msg[0] >> 6) {
	case EAP_PSK_R_FLAG_CONT:
		wpa_printf(MSG_DEBUG, "EAP-PSK: R flag - CONT - unsupported");
		return NULL;
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
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	hdr4 = (struct eap_psk_hdr_4 *) resp;
	hdr4->code = EAP_CODE_RESPONSE;
	hdr4->identifier = hdr3->identifier;
	hdr4->length = host_to_be16(*respDataLen);
	hdr4->type = EAP_TYPE_PSK;
#ifndef EAP_PSK_DRAFT3
	hdr4->flags = 3; /* T=3 */
	memcpy(hdr4->rand_s, hdr3->rand_s, EAP_PSK_RAND_LEN);
#endif /* EAP_PSK_DRAFT3 */
	pchannel = (u8 *) (hdr4 + 1);

	/* nonce++ */
	inc_byte_array(nonce, sizeof(nonce));
	memcpy(pchannel, nonce + 12, 4);

	pchannel[4 + 16] = EAP_PSK_R_FLAG_DONE_SUCCESS << 6;

	wpa_hexdump(MSG_DEBUG, "EAP-PSK: reply message (plaintext)",
		    pchannel + 4 + 16, 1);
#ifdef EAP_PSK_DRAFT3
	aes_128_eax_encrypt(data->tek, nonce, sizeof(nonce), resp, 5,
			    pchannel + 4 + 16, 1, pchannel + 4);
#else /* EAP_PSK_DRAFT3 */
	aes_128_eax_encrypt(data->tek, nonce, sizeof(nonce), resp, 22,
			    pchannel + 4 + 16, 1, pchannel + 4);
#endif /* EAP_PSK_DRAFT3 */
	wpa_hexdump(MSG_DEBUG, "EAP-PSK: reply message (PCHANNEL)",
		    pchannel, 4 + 16 + 1);

	wpa_printf(MSG_DEBUG, "EAP-PSK: Completed %ssuccessfully",
		   failed ? "un" : "");
	data->state = PSK_DONE;
	ret->methodState = METHOD_DONE;
	ret->decision = failed ? DECISION_FAIL : DECISION_UNCOND_SUCC;

	return resp;
}


static u8 * eap_psk_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_psk_data *data = priv;
	struct eap_hdr *req;
	u8 *pos, *resp = NULL;
	size_t len;

	req = (struct eap_hdr *) reqData;
	pos = (u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 1 || *pos != EAP_TYPE_PSK ||
	    (len = be_to_host16(req->length)) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-PSK: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}

	ret->ignore = FALSE;
	ret->methodState = METHOD_CONT;
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
