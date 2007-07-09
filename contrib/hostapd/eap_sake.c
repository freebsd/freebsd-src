/*
 * hostapd / EAP-SAKE (RFC 4763) server
 * Copyright (c) 2006, Jouni Malinen <j@w1.fi>
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
#include "eap_sake_common.h"


struct eap_sake_data {
	enum { IDENTITY, CHALLENGE, CONFIRM, SUCCESS, FAILURE } state;
	u8 rand_s[EAP_SAKE_RAND_LEN];
	u8 rand_p[EAP_SAKE_RAND_LEN];
	struct {
		u8 auth[EAP_SAKE_TEK_AUTH_LEN];
		u8 cipher[EAP_SAKE_TEK_CIPHER_LEN];
	} tek;
	u8 msk[EAP_MSK_LEN];
	u8 emsk[EAP_EMSK_LEN];
	u8 session_id;
	u8 *peerid;
	size_t peerid_len;
	u8 *serverid;
	size_t serverid_len;
};


static const char * eap_sake_state_txt(int state)
{
	switch (state) {
	case IDENTITY:
		return "IDENTITY";
	case CHALLENGE:
		return "CHALLENGE";
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


static void eap_sake_state(struct eap_sake_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-SAKE: %s -> %s",
		   eap_sake_state_txt(data->state),
		   eap_sake_state_txt(state));
	data->state = state;
}


static void * eap_sake_init(struct eap_sm *sm)
{
	struct eap_sake_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = CHALLENGE;

	if (hostapd_get_rand(&data->session_id, 1)) {
		wpa_printf(MSG_ERROR, "EAP-SAKE: Failed to get random data");
		os_free(data);
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "EAP-SAKE: Initialized Session ID %d",
		   data->session_id);

	/* TODO: add support for configuring SERVERID */
	data->serverid = (u8 *) strdup("hostapd");
	if (data->serverid)
		data->serverid_len = strlen((char *) data->serverid);

	return data;
}


static void eap_sake_reset(struct eap_sm *sm, void *priv)
{
	struct eap_sake_data *data = priv;
	os_free(data->serverid);
	os_free(data->peerid);
	os_free(data);
}


static u8 * eap_sake_build_msg(struct eap_sake_data *data, u8 **payload,
			       int id, size_t *length, u8 subtype)
{
	struct eap_sake_hdr *req;
	u8 *msg;

	*length += sizeof(struct eap_sake_hdr);

	msg = wpa_zalloc(*length);
	if (msg == NULL) {
		wpa_printf(MSG_ERROR, "EAP-SAKE: Failed to allocate memory "
			   "request");
		return NULL;
	}

	req = (struct eap_sake_hdr *) msg;
	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons((u16) *length);
	req->type = EAP_TYPE_SAKE;
	req->version = EAP_SAKE_VERSION;
	req->session_id = data->session_id;
	req->subtype = subtype;
	*payload = (u8 *) (req + 1);

	return msg;
}


static u8 * eap_sake_build_identity(struct eap_sm *sm,
				    struct eap_sake_data *data,
				    int id, size_t *reqDataLen)
{
	u8 *msg, *pos;

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Request/Identity");

	*reqDataLen = 4;
	if (data->serverid)
		*reqDataLen += 2 + data->serverid_len;
	msg = eap_sake_build_msg(data, &pos, id, reqDataLen,
				 EAP_SAKE_SUBTYPE_IDENTITY);
	if (msg == NULL) {
		data->state = FAILURE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-SAKE: * AT_PERM_ID_REQ");
	*pos++ = EAP_SAKE_AT_PERM_ID_REQ;
	*pos++ = 4;
	*pos++ = 0;
	*pos++ = 0;

	if (data->serverid) {
		wpa_printf(MSG_DEBUG, "EAP-SAKE: * AT_SERVERID");
		*pos++ = EAP_SAKE_AT_SERVERID;
		*pos++ = 2 + data->serverid_len;
		os_memcpy(pos, data->serverid, data->serverid_len);
	}

	return msg;
}


static u8 * eap_sake_build_challenge(struct eap_sm *sm,
				     struct eap_sake_data *data,
				     int id, size_t *reqDataLen)
{
	u8 *msg, *pos;

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Request/Challenge");

	if (hostapd_get_rand(data->rand_s, EAP_SAKE_RAND_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-SAKE: Failed to get random data");
		data->state = FAILURE;
		return NULL;
	}
	wpa_hexdump(MSG_MSGDUMP, "EAP-SAKE: RAND_S (server rand)",
		    data->rand_s, EAP_SAKE_RAND_LEN);

	*reqDataLen = 2 + EAP_SAKE_RAND_LEN;
	if (data->serverid)
		*reqDataLen += 2 + data->serverid_len;
	msg = eap_sake_build_msg(data, &pos, id, reqDataLen,
				 EAP_SAKE_SUBTYPE_CHALLENGE);
	if (msg == NULL) {
		data->state = FAILURE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-SAKE: * AT_RAND_S");
	*pos++ = EAP_SAKE_AT_RAND_S;
	*pos++ = 2 + EAP_SAKE_RAND_LEN;
	os_memcpy(pos, data->rand_s, EAP_SAKE_RAND_LEN);
	pos += EAP_SAKE_RAND_LEN;

	if (data->serverid) {
		wpa_printf(MSG_DEBUG, "EAP-SAKE: * AT_SERVERID");
		*pos++ = EAP_SAKE_AT_SERVERID;
		*pos++ = 2 + data->serverid_len;
		os_memcpy(pos, data->serverid, data->serverid_len);
	}

	return msg;
}


static u8 * eap_sake_build_confirm(struct eap_sm *sm,
				   struct eap_sake_data *data,
				   int id, size_t *reqDataLen)
{
	u8 *msg, *pos;

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Request/Confirm");

	*reqDataLen = 2 + EAP_SAKE_MIC_LEN;
	msg = eap_sake_build_msg(data, &pos, id, reqDataLen,
				 EAP_SAKE_SUBTYPE_CONFIRM);
	if (msg == NULL) {
		data->state = FAILURE;
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-SAKE: * AT_MIC_S");
	*pos++ = EAP_SAKE_AT_MIC_S;
	*pos++ = 2 + EAP_SAKE_MIC_LEN;
	if (eap_sake_compute_mic(data->tek.auth, data->rand_s, data->rand_p,
				 data->serverid, data->serverid_len,
				 data->peerid, data->peerid_len, 0,
				 msg, *reqDataLen, pos, pos)) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Failed to compute MIC");
		data->state = FAILURE;
		os_free(msg);
		return NULL;
	}

	return msg;
}


static u8 * eap_sake_buildReq(struct eap_sm *sm, void *priv, int id,
			      size_t *reqDataLen)
{
	struct eap_sake_data *data = priv;

	switch (data->state) {
	case IDENTITY:
		return eap_sake_build_identity(sm, data, id, reqDataLen);
	case CHALLENGE:
		return eap_sake_build_challenge(sm, data, id, reqDataLen);
	case CONFIRM:
		return eap_sake_build_confirm(sm, data, id, reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Unknown state %d in buildReq",
			   data->state);
		break;
	}
	return NULL;
}


static Boolean eap_sake_check(struct eap_sm *sm, void *priv,
			      u8 *respData, size_t respDataLen)
{
	struct eap_sake_data *data = priv;
	struct eap_sake_hdr *resp;
	size_t len;
	u8 version, session_id, subtype;

	resp = (struct eap_sake_hdr *) respData;
	if (respDataLen < sizeof(*resp) ||
	    resp->type != EAP_TYPE_SAKE ||
	    (len = ntohs(resp->length)) > respDataLen ||
	    len < sizeof(*resp)) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Invalid frame");
		return TRUE;
	}
	version = resp->version;
	session_id = resp->session_id;
	subtype = resp->subtype;

	if (version != EAP_SAKE_VERSION) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Unknown version %d", version);
		return TRUE;
	}

	if (session_id != data->session_id) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Session ID mismatch (%d,%d)",
			   session_id, data->session_id);
		return TRUE;
	}

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Received frame: subtype=%d", subtype);

	if (data->state == IDENTITY && subtype == EAP_SAKE_SUBTYPE_IDENTITY)
		return FALSE;

	if (data->state == CHALLENGE && subtype == EAP_SAKE_SUBTYPE_CHALLENGE)
		return FALSE;

	if (data->state == CONFIRM && subtype == EAP_SAKE_SUBTYPE_CONFIRM)
		return FALSE;

	if (subtype == EAP_SAKE_SUBTYPE_AUTH_REJECT)
		return FALSE;

	wpa_printf(MSG_INFO, "EAP-SAKE: Unexpected subtype=%d in state=%d",
		   subtype, data->state);

	return TRUE;
}


static void eap_sake_process_identity(struct eap_sm *sm,
				      struct eap_sake_data *data,
				      u8 *respData, size_t respDataLen,
				      u8 *payload, size_t payloadlen)
{
	if (data->state != IDENTITY)
		return;

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Received Response/Identity");
	/* TODO: update identity and select new user data */
	eap_sake_state(data, CHALLENGE);
}


static void eap_sake_process_challenge(struct eap_sm *sm,
				       struct eap_sake_data *data,
				       u8 *respData, size_t respDataLen,
				       u8 *payload, size_t payloadlen)
{
	struct eap_sake_parse_attr attr;
	u8 mic_p[EAP_SAKE_MIC_LEN];

	if (data->state != CHALLENGE)
		return;

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Received Response/Challenge");

	if (eap_sake_parse_attributes(payload, payloadlen, &attr))
		return;

	if (!attr.rand_p || !attr.mic_p) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Response/Challenge did not "
			   "include AT_RAND_P or AT_MIC_P");
		return;
	}

	os_memcpy(data->rand_p, attr.rand_p, EAP_SAKE_RAND_LEN);

	os_free(data->peerid);
	data->peerid = NULL;
	data->peerid_len = 0;
	if (attr.peerid) {
		data->peerid = os_malloc(attr.peerid_len);
		if (data->peerid == NULL)
			return;
		os_memcpy(data->peerid, attr.peerid, attr.peerid_len);
		data->peerid_len = attr.peerid_len;
	}

	if (sm->user == NULL || sm->user->password == NULL ||
	    sm->user->password_len != 2 * EAP_SAKE_ROOT_SECRET_LEN) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Plaintext password with "
			   "%d-byte key not configured",
			   2 * EAP_SAKE_ROOT_SECRET_LEN);
		data->state = FAILURE;
		return;
	}
	eap_sake_derive_keys(sm->user->password,
			     sm->user->password + EAP_SAKE_ROOT_SECRET_LEN,
			     data->rand_s, data->rand_p,
			     (u8 *) &data->tek, data->msk, data->emsk);

	eap_sake_compute_mic(data->tek.auth, data->rand_s, data->rand_p,
			     data->serverid, data->serverid_len,
			     data->peerid, data->peerid_len, 1,
			     respData, respDataLen, attr.mic_p, mic_p);
	if (os_memcmp(attr.mic_p, mic_p, EAP_SAKE_MIC_LEN) != 0) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Incorrect AT_MIC_P");
		eap_sake_state(data, FAILURE);
		return;
	}

	eap_sake_state(data, CONFIRM);
}


static void eap_sake_process_confirm(struct eap_sm *sm,
				     struct eap_sake_data *data,
				     u8 *respData, size_t respDataLen,
				     u8 *payload, size_t payloadlen)
{
	struct eap_sake_parse_attr attr;
	u8 mic_p[EAP_SAKE_MIC_LEN];

	if (data->state != CONFIRM)
		return;

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Received Response/Confirm");

	if (eap_sake_parse_attributes(payload, payloadlen, &attr))
		return;

	if (!attr.mic_p) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Response/Confirm did not "
			   "include AT_MIC_P");
		return;
	}

	eap_sake_compute_mic(data->tek.auth, data->rand_s, data->rand_p,
			     data->serverid, data->serverid_len,
			     data->peerid, data->peerid_len, 1,
			     respData, respDataLen, attr.mic_p, mic_p);
	if (os_memcmp(attr.mic_p, mic_p, EAP_SAKE_MIC_LEN) != 0) {
		wpa_printf(MSG_INFO, "EAP-SAKE: Incorrect AT_MIC_P");
		eap_sake_state(data, FAILURE);
	} else
		eap_sake_state(data, SUCCESS);
}


static void eap_sake_process_auth_reject(struct eap_sm *sm,
					 struct eap_sake_data *data,
					 u8 *respData, size_t respDataLen,
					 u8 *payload, size_t payloadlen)
{
	wpa_printf(MSG_DEBUG, "EAP-SAKE: Received Response/Auth-Reject");
	eap_sake_state(data, FAILURE);
}


static void eap_sake_process(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_sake_data *data = priv;
	struct eap_sake_hdr *resp;
	u8 subtype, *pos, *end;

	resp = (struct eap_sake_hdr *) respData;
	subtype = resp->subtype;
	pos = (u8 *) (resp + 1);
	end = respData + ntohs(resp->length);

	wpa_hexdump(MSG_DEBUG, "EAP-SAKE: Received attributes",
		    pos, end - pos);

	switch (subtype) {
	case EAP_SAKE_SUBTYPE_IDENTITY:
		eap_sake_process_identity(sm, data, respData, respDataLen, pos,
					  end - pos);
		break;
	case EAP_SAKE_SUBTYPE_CHALLENGE:
		eap_sake_process_challenge(sm, data, respData, respDataLen,
					   pos, end - pos);
		break;
	case EAP_SAKE_SUBTYPE_CONFIRM:
		eap_sake_process_confirm(sm, data, respData, respDataLen, pos,
					 end - pos);
		break;
	case EAP_SAKE_SUBTYPE_AUTH_REJECT:
		eap_sake_process_auth_reject(sm, data, respData, respDataLen,
					     pos, end - pos);
		break;
	}
}


static Boolean eap_sake_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_sake_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_sake_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_sake_data *data = priv;
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


static u8 * eap_sake_get_emsk(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_sake_data *data = priv;
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


static Boolean eap_sake_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_sake_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_sake_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_SAKE, "SAKE");
	if (eap == NULL)
		return -1;

	eap->init = eap_sake_init;
	eap->reset = eap_sake_reset;
	eap->buildReq = eap_sake_buildReq;
	eap->check = eap_sake_check;
	eap->process = eap_sake_process;
	eap->isDone = eap_sake_isDone;
	eap->getKey = eap_sake_getKey;
	eap->isSuccess = eap_sake_isSuccess;
	eap->get_emsk = eap_sake_get_emsk;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
