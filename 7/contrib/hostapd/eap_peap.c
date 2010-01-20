/*
 * hostapd / EAP-PEAP (draft-josefsson-pppext-eap-tls-eap-07.txt)
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
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
#include "eap_tls_common.h"
#include "tls.h"


/* Maximum supported PEAP version
 * 0 = Microsoft's PEAP version 0; draft-kamath-pppext-peapv0-00.txt
 * 1 = draft-josefsson-ppext-eap-tls-eap-05.txt
 * 2 = draft-josefsson-ppext-eap-tls-eap-07.txt
 */
#define EAP_PEAP_VERSION 1


static void eap_peap_reset(struct eap_sm *sm, void *priv);


struct eap_peap_data {
	struct eap_ssl_data ssl;
	enum {
		START, PHASE1, PHASE2_START, PHASE2_ID, PHASE2_METHOD,
		PHASE2_TLV, SUCCESS_REQ, FAILURE_REQ, SUCCESS, FAILURE
	} state;

	int peap_version;
	const struct eap_method *phase2_method;
	void *phase2_priv;
	int force_version;
};


static const char * eap_peap_state_txt(int state)
{
	switch (state) {
	case START:
		return "START";
	case PHASE1:
		return "PHASE1";
	case PHASE2_START:
		return "PHASE2_START";
	case PHASE2_ID:
		return "PHASE2_ID";
	case PHASE2_METHOD:
		return "PHASE2_METHOD";
	case PHASE2_TLV:
		return "PHASE2_TLV";
	case SUCCESS_REQ:
		return "SUCCESS_REQ";
	case FAILURE_REQ:
		return "FAILURE_REQ";
	case SUCCESS:
		return "SUCCESS";
	case FAILURE:
		return "FAILURE";
	default:
		return "Unknown?!";
	}
}


static void eap_peap_state(struct eap_peap_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-PEAP: %s -> %s",
		   eap_peap_state_txt(data->state),
		   eap_peap_state_txt(state));
	data->state = state;
}


static EapType eap_peap_req_success(struct eap_sm *sm,
				    struct eap_peap_data *data)
{
	if (data->state == FAILURE || data->state == FAILURE_REQ) {
		eap_peap_state(data, FAILURE);
		return EAP_TYPE_NONE;
	}

	if (data->peap_version == 0) {
		sm->tlv_request = TLV_REQ_SUCCESS;
		eap_peap_state(data, PHASE2_TLV);
		return EAP_TYPE_TLV;
	} else {
		eap_peap_state(data, SUCCESS_REQ);
		return EAP_TYPE_NONE;
	}
}


static EapType eap_peap_req_failure(struct eap_sm *sm,
				    struct eap_peap_data *data)
{
	if (data->state == FAILURE || data->state == FAILURE_REQ ||
	    data->state == SUCCESS_REQ ||
	    (data->phase2_method &&
	     data->phase2_method->method == EAP_TYPE_TLV)) {
		eap_peap_state(data, FAILURE);
		return EAP_TYPE_NONE;
	}

	if (data->peap_version == 0) {
		sm->tlv_request = TLV_REQ_FAILURE;
		eap_peap_state(data, PHASE2_TLV);
		return EAP_TYPE_TLV;
	} else {
		eap_peap_state(data, FAILURE_REQ);
		return EAP_TYPE_NONE;
	}
}


static void * eap_peap_init(struct eap_sm *sm)
{
	struct eap_peap_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->peap_version = EAP_PEAP_VERSION;
	data->force_version = -1;
	if (sm->user && sm->user->force_version >= 0) {
		data->force_version = sm->user->force_version;
		wpa_printf(MSG_DEBUG, "EAP-PEAP: forcing version %d",
			   data->force_version);
		data->peap_version = data->force_version;
	}
	data->state = START;

	if (eap_tls_ssl_init(sm, &data->ssl, 0)) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Failed to initialize SSL.");
		eap_peap_reset(sm, data);
		return NULL;
	}

	return data;
}


static void eap_peap_reset(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	if (data == NULL)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->reset(sm, data->phase2_priv);
	eap_tls_ssl_deinit(sm, &data->ssl);
	free(data);
}


static u8 * eap_peap_build_start(struct eap_sm *sm, struct eap_peap_data *data,
				 int id, size_t *reqDataLen)
{
	struct eap_hdr *req;
	u8 *pos;

	*reqDataLen = sizeof(*req) + 2;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PEAP: Failed to allocate memory for"
			   " request");
		eap_peap_state(data, FAILURE);
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_PEAP;
	*pos = EAP_TLS_FLAGS_START | data->peap_version;

	eap_peap_state(data, PHASE1);

	return (u8 *) req;
}


static u8 * eap_peap_build_req(struct eap_sm *sm, struct eap_peap_data *data,
			       int id, size_t *reqDataLen)
{
	int res;
	u8 *req;

	res = eap_tls_buildReq_helper(sm, &data->ssl, EAP_TYPE_PEAP,
				      data->peap_version, id, &req,
				      reqDataLen);

	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase1 done, starting "
			   "Phase2");
		eap_peap_state(data, PHASE2_START);
	}

	if (res == 1)
		return eap_tls_build_ack(reqDataLen, id, EAP_TYPE_PEAP,
					 data->peap_version);
	return req;
}


static u8 * eap_peap_encrypt(struct eap_sm *sm, struct eap_peap_data *data,
			     int id, u8 *plain, size_t plain_len,
			     size_t *out_len)
{
	int res;
	u8 *pos;
	struct eap_hdr *req;

	/* TODO: add support for fragmentation, if needed. This will need to
	 * add TLS Message Length field, if the frame is fragmented. */
	req = malloc(sizeof(struct eap_hdr) + 2 + data->ssl.tls_out_limit);
	if (req == NULL)
		return NULL;

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;

	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_PEAP;
	*pos++ = data->peap_version;

	res = tls_connection_encrypt(sm->ssl_ctx, data->ssl.conn,
				     plain, plain_len,
				     pos, data->ssl.tls_out_limit);
	if (res < 0) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Failed to encrypt Phase 2 "
			   "data");
		free(req);
		return NULL;
	}

	*out_len = sizeof(struct eap_hdr) + 2 + res;
	req->length = host_to_be16(*out_len);
	return (u8 *) req;
}


static u8 * eap_peap_build_phase2_req(struct eap_sm *sm,
				      struct eap_peap_data *data,
				      int id, size_t *reqDataLen)
{
	u8 *req, *buf, *encr_req;
	size_t req_len;

	buf = req = data->phase2_method->buildReq(sm, data->phase2_priv, id,
						  &req_len);
	if (req == NULL)
		return NULL;

	wpa_hexdump_key(MSG_DEBUG, "EAP-PEAP: Encrypting Phase 2 data",
			req, req_len);

	if (data->peap_version == 0 &&
	    data->phase2_method->method != EAP_TYPE_TLV) {
		req += sizeof(struct eap_hdr);
		req_len -= sizeof(struct eap_hdr);
	}

	encr_req = eap_peap_encrypt(sm, data, id, req, req_len, reqDataLen);
	free(buf);

	return encr_req;
}


static u8 * eap_peap_build_phase2_term(struct eap_sm *sm,
				       struct eap_peap_data *data,
				       int id, size_t *reqDataLen, int success)
{
	u8 *encr_req;
	size_t req_len;
	struct eap_hdr *hdr;

	req_len = sizeof(*hdr);
	hdr = wpa_zalloc(req_len);
	if (hdr == NULL)
		return NULL;

	hdr->code = success ? EAP_CODE_SUCCESS : EAP_CODE_FAILURE;
	hdr->identifier = id;
	hdr->length = htons(req_len);

	wpa_hexdump_key(MSG_DEBUG, "EAP-PEAP: Encrypting Phase 2 data",
			(u8 *) hdr, req_len);

	encr_req = eap_peap_encrypt(sm, data, id, (u8 *) hdr, req_len,
				    reqDataLen);
	free(hdr);

	return encr_req;
}


static u8 * eap_peap_buildReq(struct eap_sm *sm, void *priv, int id,
			      size_t *reqDataLen)
{
	struct eap_peap_data *data = priv;

	switch (data->state) {
	case START:
		return eap_peap_build_start(sm, data, id, reqDataLen);
	case PHASE1:
		return eap_peap_build_req(sm, data, id, reqDataLen);
	case PHASE2_ID:
	case PHASE2_METHOD:
	case PHASE2_TLV:
		return eap_peap_build_phase2_req(sm, data, id, reqDataLen);
	case SUCCESS_REQ:
		return eap_peap_build_phase2_term(sm, data, id, reqDataLen, 1);
	case FAILURE_REQ:
		return eap_peap_build_phase2_term(sm, data, id, reqDataLen, 0);
	default:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: %s - unexpected state %d",
			   __func__, data->state);
		return NULL;
	}
}


static Boolean eap_peap_check(struct eap_sm *sm, void *priv,
			      u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 2 || *pos != EAP_TYPE_PEAP ||
	    (ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static int eap_peap_phase2_init(struct eap_sm *sm, struct eap_peap_data *data,
				EapType eap_type)
{
	if (data->phase2_priv && data->phase2_method) {
		data->phase2_method->reset(sm, data->phase2_priv);
		data->phase2_method = NULL;
		data->phase2_priv = NULL;
	}
	data->phase2_method = eap_sm_get_eap_methods(EAP_VENDOR_IETF,
						     eap_type);
	if (!data->phase2_method)
		return -1;

	sm->init_phase2 = 1;
	data->phase2_priv = data->phase2_method->init(sm);
	sm->init_phase2 = 0;
	return 0;
}


static void eap_peap_process_phase2_response(struct eap_sm *sm,
					     struct eap_peap_data *data,
					     u8 *in_data, size_t in_len)
{
	u8 next_type = EAP_TYPE_NONE;
	struct eap_hdr *hdr;
	u8 *pos;
	size_t left;

	if (data->phase2_priv == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: %s - Phase2 not "
			   "initialized?!", __func__);
		return;
	}

	hdr = (struct eap_hdr *) in_data;
	pos = (u8 *) (hdr + 1);

	if (in_len > sizeof(*hdr) && *pos == EAP_TYPE_NAK) {
		left = in_len - sizeof(*hdr);
		wpa_hexdump(MSG_DEBUG, "EAP-PEAP: Phase2 type Nak'ed; "
			    "allowed types", pos + 1, left - 1);
		eap_sm_process_nak(sm, pos + 1, left - 1);
		if (sm->user && sm->user_eap_method_index < EAP_MAX_METHODS &&
		    sm->user->methods[sm->user_eap_method_index].method !=
		    EAP_TYPE_NONE) {
			next_type = sm->user->methods[
				sm->user_eap_method_index++].method;
			wpa_printf(MSG_DEBUG, "EAP-PEAP: try EAP type %d",
				   next_type);
		} else {
			next_type = eap_peap_req_failure(sm, data);
		}
		eap_peap_phase2_init(sm, data, next_type);
		return;
	}

	if (data->phase2_method->check(sm, data->phase2_priv, in_data,
				       in_len)) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase2 check() asked to "
			   "ignore the packet");
		return;
	}

	data->phase2_method->process(sm, data->phase2_priv, in_data, in_len);

	if (!data->phase2_method->isDone(sm, data->phase2_priv))
		return;


	if (!data->phase2_method->isSuccess(sm, data->phase2_priv)) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase2 method failed");
		next_type = eap_peap_req_failure(sm, data);
		eap_peap_phase2_init(sm, data, next_type);
		return;
	}

	switch (data->state) {
	case PHASE2_ID:
		if (eap_user_get(sm, sm->identity, sm->identity_len, 1) != 0) {
			wpa_hexdump_ascii(MSG_DEBUG, "EAP_PEAP: Phase2 "
					  "Identity not found in the user "
					  "database",
					  sm->identity, sm->identity_len);
			next_type = eap_peap_req_failure(sm, data);
			break;
		}

		eap_peap_state(data, PHASE2_METHOD);
		next_type = sm->user->methods[0].method;
		sm->user_eap_method_index = 1;
		wpa_printf(MSG_DEBUG, "EAP-PEAP: try EAP type %d", next_type);
		break;
	case PHASE2_METHOD:
		next_type = eap_peap_req_success(sm, data);
		break;
	case PHASE2_TLV:
		if (sm->tlv_request == TLV_REQ_SUCCESS ||
		    data->state == SUCCESS_REQ) {
			eap_peap_state(data, SUCCESS);
		} else {
			eap_peap_state(data, FAILURE);
		}
		break;
	case FAILURE:
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: %s - unexpected state %d",
			   __func__, data->state);
		break;
	}

	eap_peap_phase2_init(sm, data, next_type);
}


static void eap_peap_process_phase2(struct eap_sm *sm,
				    struct eap_peap_data *data,
				    struct eap_hdr *resp,
				    u8 *in_data, size_t in_len)
{
	u8 *in_decrypted;
	int len_decrypted, len, res;
	struct eap_hdr *hdr;
	size_t buf_len;

	wpa_printf(MSG_DEBUG, "EAP-PEAP: received %lu bytes encrypted data for"
		   " Phase 2", (unsigned long) in_len);

	res = eap_tls_data_reassemble(sm, &data->ssl, &in_data, &in_len);
	if (res < 0 || res == 1)
		return;

	buf_len = in_len;
	if (data->ssl.tls_in_total > buf_len)
		buf_len = data->ssl.tls_in_total;
	in_decrypted = malloc(buf_len);
	if (in_decrypted == NULL) {
		free(data->ssl.tls_in);
		data->ssl.tls_in = NULL;
		data->ssl.tls_in_len = 0;
		wpa_printf(MSG_WARNING, "EAP-PEAP: failed to allocate memory "
			   "for decryption");
		return;
	}

	len_decrypted = tls_connection_decrypt(sm->ssl_ctx, data->ssl.conn,
					       in_data, in_len,
					       in_decrypted, buf_len);
	free(data->ssl.tls_in);
	data->ssl.tls_in = NULL;
	data->ssl.tls_in_len = 0;
	if (len_decrypted < 0) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Failed to decrypt Phase 2 "
			   "data");
		free(in_decrypted);
		eap_peap_state(data, FAILURE);
		return;
	}

	wpa_hexdump_key(MSG_DEBUG, "EAP-PEAP: Decrypted Phase 2 EAP",
			in_decrypted, len_decrypted);

	hdr = (struct eap_hdr *) in_decrypted;

	if (data->peap_version == 0 && data->state != PHASE2_TLV) {
		struct eap_hdr *nhdr = malloc(sizeof(struct eap_hdr) +
					      len_decrypted);
		if (nhdr == NULL) {
			free(in_decrypted);
			return;
		}
		memcpy((u8 *) (nhdr + 1), in_decrypted, len_decrypted);
		free(in_decrypted);
		nhdr->code = resp->code;
		nhdr->identifier = resp->identifier;
		nhdr->length = host_to_be16(sizeof(struct eap_hdr) +
					    len_decrypted);

		len_decrypted += sizeof(struct eap_hdr);
		in_decrypted = (u8 *) nhdr;
	}
	hdr = (struct eap_hdr *) in_decrypted;
	if (len_decrypted < (int) sizeof(*hdr)) {
		free(in_decrypted);
		wpa_printf(MSG_INFO, "EAP-PEAP: Too short Phase 2 "
			   "EAP frame (len=%d)", len_decrypted);
		eap_peap_req_failure(sm, data);
		return;
	}
	len = be_to_host16(hdr->length);
	if (len > len_decrypted) {
		free(in_decrypted);
		wpa_printf(MSG_INFO, "EAP-PEAP: Length mismatch in "
			   "Phase 2 EAP frame (len=%d hdr->length=%d)",
			   len_decrypted, len);
		eap_peap_req_failure(sm, data);
		return;
	}
	wpa_printf(MSG_DEBUG, "EAP-PEAP: received Phase 2: code=%d "
		   "identifier=%d length=%d", hdr->code, hdr->identifier, len);
	switch (hdr->code) {
	case EAP_CODE_RESPONSE:
		eap_peap_process_phase2_response(sm, data, (u8 *) hdr, len);
		break;
	case EAP_CODE_SUCCESS:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase 2 Success");
		if (data->state == SUCCESS_REQ) {
			eap_peap_state(data, SUCCESS);
		}
		break;
	case EAP_CODE_FAILURE:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase 2 Failure");
		eap_peap_state(data, FAILURE);
		break;
	default:
		wpa_printf(MSG_INFO, "EAP-PEAP: Unexpected code=%d in "
			   "Phase 2 EAP header", hdr->code);
		break;
	}

	free(in_decrypted);
 }


static void eap_peap_process(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_peap_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos, flags;
	int left;
	unsigned int tls_msg_len;
	int peer_version;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	pos++;
	flags = *pos++;
	left = htons(resp->length) - sizeof(struct eap_hdr) - 2;
	wpa_printf(MSG_DEBUG, "EAP-PEAP: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) respDataLen, flags);
	peer_version = flags & EAP_PEAP_VERSION_MASK;
	if (data->force_version >= 0 && peer_version != data->force_version) {
		wpa_printf(MSG_INFO, "EAP-PEAP: peer did not select the forced"
			   " version (forced=%d peer=%d) - reject",
			   data->force_version, peer_version);
		eap_peap_state(data, FAILURE);
		return;
	}
	if (peer_version < data->peap_version) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: peer ver=%d, own ver=%d; "
			   "use version %d",
			   peer_version, data->peap_version, peer_version);
		data->peap_version = peer_version;
	}
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-PEAP: Short frame with TLS "
				   "length");
			eap_peap_state(data, FAILURE);
			return;
		}
		tls_msg_len = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) |
			pos[3];
		wpa_printf(MSG_DEBUG, "EAP-PEAP: TLS Message Length: %d",
			   tls_msg_len);
		if (data->ssl.tls_in_left == 0) {
			data->ssl.tls_in_total = tls_msg_len;
			data->ssl.tls_in_left = tls_msg_len;
			free(data->ssl.tls_in);
			data->ssl.tls_in = NULL;
			data->ssl.tls_in_len = 0;
		}
		pos += 4;
		left -= 4;
	}

	switch (data->state) {
	case PHASE1:
		if (eap_tls_process_helper(sm, &data->ssl, pos, left) < 0) {
			wpa_printf(MSG_INFO, "EAP-PEAP: TLS processing "
				   "failed");
			eap_peap_state(data, FAILURE);
		}
		break;
	case PHASE2_START:
		eap_peap_state(data, PHASE2_ID);
		eap_peap_phase2_init(sm, data, EAP_TYPE_IDENTITY);
		break;
	case PHASE2_ID:
	case PHASE2_METHOD:
	case PHASE2_TLV:
		eap_peap_process_phase2(sm, data, resp, pos, left);
		break;
	case SUCCESS_REQ:
		eap_peap_state(data, SUCCESS);
		break;
	case FAILURE_REQ:
		eap_peap_state(data, FAILURE);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Unexpected state %d in %s",
			   data->state, __func__);
		break;
	}

	if (tls_connection_get_write_alerts(sm->ssl_ctx, data->ssl.conn) > 1) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Locally detected fatal error "
			   "in TLS processing");
		eap_peap_state(data, FAILURE);
	}
}


static Boolean eap_peap_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_peap_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_peap_data *data = priv;
	u8 *eapKeyData;

	if (data->state != SUCCESS)
		return NULL;

	/* TODO: PEAPv1 - different label in some cases */
	eapKeyData = eap_tls_derive_key(sm, &data->ssl,
					"client EAP encryption",
					EAP_TLS_KEY_LEN);
	if (eapKeyData) {
		*len = EAP_TLS_KEY_LEN;
		wpa_hexdump(MSG_DEBUG, "EAP-PEAP: Derived key",
			    eapKeyData, EAP_TLS_KEY_LEN);
	} else {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Failed to derive key");
	}

	return eapKeyData;
}


static Boolean eap_peap_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_peap_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_PEAP, "PEAP");
	if (eap == NULL)
		return -1;

	eap->init = eap_peap_init;
	eap->reset = eap_peap_reset;
	eap->buildReq = eap_peap_buildReq;
	eap->check = eap_peap_check;
	eap->process = eap_peap_process;
	eap->isDone = eap_peap_isDone;
	eap->getKey = eap_peap_getKey;
	eap->isSuccess = eap_peap_isSuccess;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
