/*
 * EAP peer method: EAP-PAX (RFC 4746)
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
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
#include "eap_pax_common.h"
#include "sha1.h"
#include "crypto.h"

/*
 * Note: only PAX_STD subprotocol is currently supported
 *
 * TODO: Add support with PAX_SEC with the mandatory to implement ciphersuite
 * (HMAC_SHA1_128, IANA DH Group 14 (2048 bits), RSA-PKCS1-V1_5) and
 * recommended ciphersuite (HMAC_SHA256_128, IANA DH Group 15 (3072 bits),
 * RSAES-OAEP).
 */

struct eap_pax_data {
	enum { PAX_INIT, PAX_STD_2_SENT, PAX_DONE } state;
	u8 mac_id, dh_group_id, public_key_id;
	union {
		u8 e[2 * EAP_PAX_RAND_LEN];
		struct {
			u8 x[EAP_PAX_RAND_LEN]; /* server rand */
			u8 y[EAP_PAX_RAND_LEN]; /* client rand */
		} r;
	} rand;
	char *cid;
	size_t cid_len;
	u8 ak[EAP_PAX_AK_LEN];
	u8 mk[EAP_PAX_MK_LEN];
	u8 ck[EAP_PAX_CK_LEN];
	u8 ick[EAP_PAX_ICK_LEN];
};


static void eap_pax_deinit(struct eap_sm *sm, void *priv);


static void * eap_pax_init(struct eap_sm *sm)
{
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_pax_data *data;

	if (config == NULL || !config->nai ||
	    (!config->eappsk && !config->password)) {
		wpa_printf(MSG_INFO, "EAP-PAX: CID (nai) or key "
			   "(eappsk/password) not configured");
		return NULL;
	}

	if (config->eappsk && config->eappsk_len != EAP_PAX_AK_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: incorrect key length (eappsk); "
			   "expected %d", EAP_PAX_AK_LEN);
		return NULL;
	}

	data = os_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = PAX_INIT;

	data->cid = os_malloc(config->nai_len);
	if (data->cid == NULL) {
		eap_pax_deinit(sm, data);
		return NULL;
	}
	os_memcpy(data->cid, config->nai, config->nai_len);
	data->cid_len = config->nai_len;

	if (config->eappsk) {
		os_memcpy(data->ak, config->eappsk, EAP_PAX_AK_LEN);
	} else {
		u8 hash[SHA1_MAC_LEN];
		const unsigned char *addr[1];
		size_t len[1];
		addr[0] = config->password;
		len[0] = config->password_len;
		sha1_vector(1, addr, len, hash);
		os_memcpy(data->ak, hash, EAP_PAX_AK_LEN);
	}

	return data;
}


static void eap_pax_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_pax_data *data = priv;
	os_free(data->cid);
	os_free(data);
}


static struct eap_pax_hdr * eap_pax_alloc_resp(const struct eap_pax_hdr *req,
					       u16 resp_len, u8 op_code)
{
	struct eap_pax_hdr *resp;

	resp = os_malloc(resp_len);
	if (resp == NULL)
		return NULL;
	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = req->identifier;
	resp->length = host_to_be16(resp_len);
	resp->type = EAP_TYPE_PAX;
	resp->op_code = op_code;
	resp->flags = 0;
	resp->mac_id = req->mac_id;
	resp->dh_group_id = req->dh_group_id;
	resp->public_key_id = req->public_key_id;
	return resp;
}


static u8 * eap_pax_process_std_1(struct eap_pax_data *data,
				  struct eap_method_ret *ret,
				  const u8 *reqData, size_t reqDataLen,
				  size_t *respDataLen)
{
	const struct eap_pax_hdr *req;
	struct eap_pax_hdr *resp;
	const u8 *pos;
	u8 *rpos;
	size_t left;

	wpa_printf(MSG_DEBUG, "EAP-PAX: PAX_STD-1 (received)");
	req = (const struct eap_pax_hdr *) reqData;

	if (data->state != PAX_INIT) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-1 received in "
			   "unexpected state (%d) - ignored", data->state);
		ret->ignore = TRUE;
		return NULL;
	}

	if (req->flags & EAP_PAX_FLAGS_CE) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-1 with CE flag set - "
			   "ignored");
		ret->ignore = TRUE;
		return NULL;
	}

	left = reqDataLen - sizeof(*req);

	if (left < 2 + EAP_PAX_RAND_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-1 with too short "
			   "payload");
		ret->ignore = TRUE;
		return NULL;
	}

	pos = (const u8 *) (req + 1);
	if (WPA_GET_BE16(pos) != EAP_PAX_RAND_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-1 with incorrect A "
			   "length %d (expected %d)",
			   WPA_GET_BE16(pos), EAP_PAX_RAND_LEN);
		ret->ignore = TRUE;
		return NULL;
	}

	pos += 2;
	left -= 2;
	os_memcpy(data->rand.r.x, pos, EAP_PAX_RAND_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: X (server rand)",
		    data->rand.r.x, EAP_PAX_RAND_LEN);
	pos += EAP_PAX_RAND_LEN;
	left -= EAP_PAX_RAND_LEN;

	if (left > 0) {
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ignored extra payload",
			    pos, left);
	}

	if (hostapd_get_rand(data->rand.r.y, EAP_PAX_RAND_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-PAX: Failed to get random data");
		ret->ignore = TRUE;
		return NULL;
	}
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: Y (client rand)",
		    data->rand.r.y, EAP_PAX_RAND_LEN);

	if (eap_pax_initial_key_derivation(req->mac_id, data->ak, data->rand.e,
					   data->mk, data->ck, data->ick) < 0)
	{
		ret->ignore = TRUE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-PAX: PAX_STD-2 (sending)");

	*respDataLen = sizeof(*resp) + 2 + EAP_PAX_RAND_LEN +
		2 + data->cid_len + 2 + EAP_PAX_MAC_LEN + EAP_PAX_ICV_LEN;
	resp = eap_pax_alloc_resp(req, *respDataLen, EAP_PAX_OP_STD_2);
	if (resp == NULL)
		return NULL;

	rpos = (u8 *) (resp + 1);
	*rpos++ = 0;
	*rpos++ = EAP_PAX_RAND_LEN;
	os_memcpy(rpos, data->rand.r.y, EAP_PAX_RAND_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: B = Y (client rand)",
		    rpos, EAP_PAX_RAND_LEN);
	rpos += EAP_PAX_RAND_LEN;

	WPA_PUT_BE16(rpos, data->cid_len);
	rpos += 2;
	os_memcpy(rpos, data->cid, data->cid_len);
	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-PAX: CID", rpos, data->cid_len);
	rpos += data->cid_len;

	*rpos++ = 0;
	*rpos++ = EAP_PAX_MAC_LEN;
	eap_pax_mac(req->mac_id, data->ck, EAP_PAX_CK_LEN,
		    data->rand.r.x, EAP_PAX_RAND_LEN,
		    data->rand.r.y, EAP_PAX_RAND_LEN,
		    (u8 *) data->cid, data->cid_len, rpos);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: MAC_CK(A, B, CID)",
		    rpos, EAP_PAX_MAC_LEN);
	rpos += EAP_PAX_MAC_LEN;

	/* Optional ADE could be added here, if needed */

	eap_pax_mac(req->mac_id, data->ick, EAP_PAX_ICK_LEN,
		    (u8 *) resp, *respDataLen - EAP_PAX_ICV_LEN,
		    NULL, 0, NULL, 0, rpos);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", rpos, EAP_PAX_ICV_LEN);
	rpos += EAP_PAX_ICV_LEN;

	data->state = PAX_STD_2_SENT;
	data->mac_id = req->mac_id;
	data->dh_group_id = req->dh_group_id;
	data->public_key_id = req->public_key_id;

	return (u8 *) resp;
}


static u8 * eap_pax_process_std_3(struct eap_pax_data *data,
				  struct eap_method_ret *ret,
				  const u8 *reqData, size_t reqDataLen,
				  size_t *respDataLen)
{
	const struct eap_pax_hdr *req;
	struct eap_pax_hdr *resp;
	u8 *rpos, mac[EAP_PAX_MAC_LEN];
	const u8 *pos;
	size_t left;

	wpa_printf(MSG_DEBUG, "EAP-PAX: PAX_STD-3 (received)");
	req = (const struct eap_pax_hdr *) reqData;

	if (data->state != PAX_STD_2_SENT) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-3 received in "
			   "unexpected state (%d) - ignored", data->state);
		ret->ignore = TRUE;
		return NULL;
	}

	if (req->flags & EAP_PAX_FLAGS_CE) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-3 with CE flag set - "
			   "ignored");
		ret->ignore = TRUE;
		return NULL;
	}

	left = reqDataLen - sizeof(*req);

	if (left < 2 + EAP_PAX_MAC_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-3 with too short "
			   "payload");
		ret->ignore = TRUE;
		return NULL;
	}

	pos = (const u8 *) (req + 1);
	if (WPA_GET_BE16(pos) != EAP_PAX_MAC_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: PAX_STD-3 with incorrect "
			   "MAC_CK length %d (expected %d)",
			   WPA_GET_BE16(pos), EAP_PAX_MAC_LEN);
		ret->ignore = TRUE;
		return NULL;
	}
	pos += 2;
	left -= 2;
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: MAC_CK(B, CID)",
		    pos, EAP_PAX_MAC_LEN);
	eap_pax_mac(data->mac_id, data->ck, EAP_PAX_CK_LEN,
		    data->rand.r.y, EAP_PAX_RAND_LEN,
		    (u8 *) data->cid, data->cid_len, NULL, 0, mac);
	if (os_memcmp(pos, mac, EAP_PAX_MAC_LEN) != 0) {
		wpa_printf(MSG_INFO, "EAP-PAX: Invalid MAC_CK(B, CID) "
			   "received");
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: expected MAC_CK(B, CID)",
			    mac, EAP_PAX_MAC_LEN);
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return NULL;
	}

	pos += EAP_PAX_MAC_LEN;
	left -= EAP_PAX_MAC_LEN;

	if (left > 0) {
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ignored extra payload",
			    pos, left);
	}

	wpa_printf(MSG_DEBUG, "EAP-PAX: PAX-ACK (sending)");

	*respDataLen = sizeof(*resp) + EAP_PAX_ICV_LEN;
	resp = eap_pax_alloc_resp(req, *respDataLen, EAP_PAX_OP_ACK);
	if (resp == NULL)
		return NULL;

	rpos = (u8 *) (resp + 1);

	/* Optional ADE could be added here, if needed */

	eap_pax_mac(data->mac_id, data->ick, EAP_PAX_ICK_LEN,
		    (u8 *) resp, *respDataLen - EAP_PAX_ICV_LEN,
		    NULL, 0, NULL, 0, rpos);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", rpos, EAP_PAX_ICV_LEN);

	data->state = PAX_DONE;
	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_UNCOND_SUCC;
	ret->allowNotifications = FALSE;

	return (u8 *) resp;
}


static u8 * eap_pax_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    const u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_pax_data *data = priv;
	const struct eap_pax_hdr *req;
	u8 *resp, icvbuf[EAP_PAX_ICV_LEN];
	const u8 *icv, *pos;
	size_t len;
	u16 flen;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_PAX,
			       reqData, reqDataLen, &len);
	if (pos == NULL || len < EAP_PAX_ICV_LEN) {
		ret->ignore = TRUE;
		return NULL;
	}
	req = (const struct eap_pax_hdr *) reqData;
	flen = be_to_host16(req->length) - EAP_PAX_ICV_LEN;

	wpa_printf(MSG_DEBUG, "EAP-PAX: received frame: op_code 0x%x "
		   "flags 0x%x mac_id 0x%x dh_group_id 0x%x "
		   "public_key_id 0x%x",
		   req->op_code, req->flags, req->mac_id, req->dh_group_id,
		   req->public_key_id);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: received payload",
		    pos, len - EAP_PAX_ICV_LEN);

	if (data->state != PAX_INIT && data->mac_id != req->mac_id) {
		wpa_printf(MSG_INFO, "EAP-PAX: MAC ID changed during "
			   "authentication (was 0x%d, is 0x%d)",
			   data->mac_id, req->mac_id);
		ret->ignore = TRUE;
		return NULL;
	}

	if (data->state != PAX_INIT && data->dh_group_id != req->dh_group_id) {
		wpa_printf(MSG_INFO, "EAP-PAX: DH Group ID changed during "
			   "authentication (was 0x%d, is 0x%d)",
			   data->dh_group_id, req->dh_group_id);
		ret->ignore = TRUE;
		return NULL;
	}

	if (data->state != PAX_INIT &&
	    data->public_key_id != req->public_key_id) {
		wpa_printf(MSG_INFO, "EAP-PAX: Public Key ID changed during "
			   "authentication (was 0x%d, is 0x%d)",
			   data->public_key_id, req->public_key_id);
		ret->ignore = TRUE;
		return NULL;
	}

	/* TODO: add support EAP_PAX_HMAC_SHA256_128 */
	if (req->mac_id != EAP_PAX_MAC_HMAC_SHA1_128) {
		wpa_printf(MSG_INFO, "EAP-PAX: Unsupported MAC ID 0x%x",
			   req->mac_id);
		ret->ignore = TRUE;
		return NULL;
	}

	if (req->dh_group_id != EAP_PAX_DH_GROUP_NONE) {
		wpa_printf(MSG_INFO, "EAP-PAX: Unsupported DH Group ID 0x%x",
			   req->dh_group_id);
		ret->ignore = TRUE;
		return NULL;
	}

	if (req->public_key_id != EAP_PAX_PUBLIC_KEY_NONE) {
		wpa_printf(MSG_INFO, "EAP-PAX: Unsupported Public Key ID 0x%x",
			   req->public_key_id);
		ret->ignore = TRUE;
		return NULL;
	}

	if (req->flags & EAP_PAX_FLAGS_MF) {
		/* TODO: add support for reassembling fragments */
		wpa_printf(MSG_INFO, "EAP-PAX: fragmentation not supported - "
			   "ignored packet");
		ret->ignore = TRUE;
		return NULL;
	}

	icv = pos + len - EAP_PAX_ICV_LEN;
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", icv, EAP_PAX_ICV_LEN);
	if (req->op_code == EAP_PAX_OP_STD_1) {
		eap_pax_mac(req->mac_id, (u8 *) "", 0,
			    reqData, flen, NULL, 0, NULL, 0, icvbuf);
	} else {
		eap_pax_mac(req->mac_id, data->ick, EAP_PAX_ICK_LEN,
			    reqData, flen, NULL, 0, NULL, 0, icvbuf);
	}
	if (os_memcmp(icv, icvbuf, EAP_PAX_ICV_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-PAX: invalid ICV - ignoring the "
			   "message");
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: expected ICV",
			    icvbuf, EAP_PAX_ICV_LEN);
		ret->ignore = TRUE;
		return NULL;
	}

	ret->ignore = FALSE;
	ret->methodState = METHOD_MAY_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	switch (req->op_code) {
	case EAP_PAX_OP_STD_1:
		resp = eap_pax_process_std_1(data, ret, reqData, flen,
					     respDataLen);
		break;
	case EAP_PAX_OP_STD_3:
		resp = eap_pax_process_std_3(data, ret, reqData, flen,
					     respDataLen);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-PAX: ignoring message with unknown "
			   "op_code %d", req->op_code);
		ret->ignore = TRUE;
		return NULL;
	}

	if (ret->methodState == METHOD_DONE) {
		ret->allowNotifications = FALSE;
	}

	return resp;
}


static Boolean eap_pax_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_pax_data *data = priv;
	return data->state == PAX_DONE;
}


static u8 * eap_pax_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_pax_data *data = priv;
	u8 *key;

	if (data->state != PAX_DONE)
		return NULL;

	key = os_malloc(EAP_MSK_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_MSK_LEN;
	eap_pax_kdf(data->mac_id, data->mk, EAP_PAX_MK_LEN,
		    "Master Session Key", data->rand.e, 2 * EAP_PAX_RAND_LEN,
		    EAP_MSK_LEN, key);

	return key;
}


static u8 * eap_pax_get_emsk(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_pax_data *data = priv;
	u8 *key;

	if (data->state != PAX_DONE)
		return NULL;

	key = os_malloc(EAP_EMSK_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_EMSK_LEN;
	eap_pax_kdf(data->mac_id, data->mk, EAP_PAX_MK_LEN,
		    "Extended Master Session Key",
		    data->rand.e, 2 * EAP_PAX_RAND_LEN,
		    EAP_EMSK_LEN, key);

	return key;
}


int eap_peer_pax_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,
				    EAP_VENDOR_IETF, EAP_TYPE_PAX, "PAX");
	if (eap == NULL)
		return -1;

	eap->init = eap_pax_init;
	eap->deinit = eap_pax_deinit;
	eap->process = eap_pax_process;
	eap->isKeyAvailable = eap_pax_isKeyAvailable;
	eap->getKey = eap_pax_getKey;
	eap->get_emsk = eap_pax_get_emsk;

	ret = eap_peer_method_register(eap);
	if (ret)
		eap_peer_method_free(eap);
	return ret;
}
