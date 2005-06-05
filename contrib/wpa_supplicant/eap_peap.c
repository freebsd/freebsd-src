/*
 * WPA Supplicant / EAP-PEAP (draft-josefsson-pppext-eap-tls-eap-07.txt)
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
#include "eap_tls_common.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "tls.h"
#include "eap_tlv.h"


/* Maximum supported PEAP version
 * 0 = Microsoft's PEAP version 0; draft-kamath-pppext-peapv0-00.txt
 * 1 = draft-josefsson-ppext-eap-tls-eap-05.txt
 * 2 = draft-josefsson-ppext-eap-tls-eap-07.txt
 */
#define EAP_PEAP_VERSION 1


static void eap_peap_deinit(struct eap_sm *sm, void *priv);


struct eap_peap_data {
	struct eap_ssl_data ssl;

	int peap_version, force_peap_version, force_new_label;

	const struct eap_method *phase2_method;
	void *phase2_priv;
	int phase2_success;

	u8 phase2_type;
	u8 *phase2_types;
	size_t num_phase2_types;

	int peap_outer_success; /* 0 = PEAP terminated on Phase 2 inner
				 * EAP-Success
				 * 1 = reply with tunneled EAP-Success to inner
				 * EAP-Success and expect AS to send outer
				 * (unencrypted) EAP-Success after this
				 * 2 = reply with PEAP/TLS ACK to inner
				 * EAP-Success and expect AS to send outer
				 * (unencrypted) EAP-Success after this */
	int resuming; /* starting a resumed session */
	u8 *key_data;

	u8 *pending_phase2_req;
	size_t pending_phase2_req_len;
};


static void * eap_peap_init(struct eap_sm *sm)
{
	struct eap_peap_data *data;
	struct wpa_ssid *config = eap_get_config(sm);

	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	sm->peap_done = FALSE;
	memset(data, 0, sizeof(*data));
	data->peap_version = EAP_PEAP_VERSION;
	data->force_peap_version = -1;
	data->peap_outer_success = 2;

	if (config && config->phase1) {
		char *pos = strstr(config->phase1, "peapver=");
		if (pos) {
			data->force_peap_version = atoi(pos + 8);
			data->peap_version = data->force_peap_version;
			wpa_printf(MSG_DEBUG, "EAP-PEAP: Forced PEAP version "
				   "%d", data->force_peap_version);
		}

		if (strstr(config->phase1, "peaplabel=1")) {
			data->force_new_label = 1;
			wpa_printf(MSG_DEBUG, "EAP-PEAP: Force new label for "
				   "key derivation");
		}

		if (strstr(config->phase1, "peap_outer_success=0")) {
			data->peap_outer_success = 0;
			wpa_printf(MSG_DEBUG, "EAP-PEAP: terminate "
				   "authentication on tunneled EAP-Success");
		} else if (strstr(config->phase1, "peap_outer_success=1")) {
			data->peap_outer_success = 1;
			wpa_printf(MSG_DEBUG, "EAP-PEAP: send tunneled "
				   "EAP-Success after receiving tunneled "
				   "EAP-Success");
		} else if (strstr(config->phase1, "peap_outer_success=2")) {
			data->peap_outer_success = 2;
			wpa_printf(MSG_DEBUG, "EAP-PEAP: send PEAP/TLS ACK "
				   "after receiving tunneled EAP-Success");
		}
	}

	if (config && config->phase2) {
		char *start, *pos, *buf;
		u8 method, *methods = NULL, *_methods;
		size_t num_methods = 0;
		start = buf = strdup(config->phase2);
		if (buf == NULL) {
			eap_peap_deinit(sm, data);
			return NULL;
		}
		while (start && *start != '\0') {
			pos = strstr(start, "auth=");
			if (pos == NULL)
				break;
			if (start != pos && *(pos - 1) != ' ') {
				start = pos + 5;
				continue;
			}

			start = pos + 5;
			pos = strchr(start, ' ');
			if (pos)
				*pos++ = '\0';
			method = eap_get_phase2_type(start);
			if (method == EAP_TYPE_NONE) {
				wpa_printf(MSG_ERROR, "EAP-PEAP: Unsupported "
					   "Phase2 method '%s'", start);
			} else {
				num_methods++;
				_methods = realloc(methods, num_methods);
				if (_methods == NULL) {
					free(methods);
					eap_peap_deinit(sm, data);
					return NULL;
				}
				methods = _methods;
				methods[num_methods - 1] = method;
			}

			start = pos;
		}
		free(buf);
		data->phase2_types = methods;
		data->num_phase2_types = num_methods;
	}
	if (data->phase2_types == NULL) {
		data->phase2_types =
			eap_get_phase2_types(config, &data->num_phase2_types);
	}
	if (data->phase2_types == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PEAP: No Phase2 method available");
		eap_peap_deinit(sm, data);
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-PEAP: Phase2 EAP types",
		    data->phase2_types, data->num_phase2_types);
	data->phase2_type = EAP_TYPE_NONE;

	if (eap_tls_ssl_init(sm, &data->ssl, config)) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Failed to initialize SSL.");
		eap_peap_deinit(sm, data);
		return NULL;
	}

	return data;
}


static void eap_peap_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	if (data == NULL)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->deinit(sm, data->phase2_priv);
	free(data->phase2_types);
	eap_tls_ssl_deinit(sm, &data->ssl);
	free(data->key_data);
	free(data->pending_phase2_req);
	free(data);
}


static int eap_peap_encrypt(struct eap_sm *sm, struct eap_peap_data *data,
			    int id, u8 *plain, size_t plain_len,
			    u8 **out_data, size_t *out_len)
{
	int res;
	u8 *pos;
	struct eap_hdr *resp;

	/* TODO: add support for fragmentation, if needed. This will need to
	 * add TLS Message Length field, if the frame is fragmented.
	 * Note: Microsoft IAS did not seem to like TLS Message Length with
	 * PEAP/MSCHAPv2. */
	resp = malloc(sizeof(struct eap_hdr) + 2 + data->ssl.tls_out_limit);
	if (resp == NULL)
		return -1;

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;

	pos = (u8 *) (resp + 1);
	*pos++ = EAP_TYPE_PEAP;
	*pos++ = data->peap_version;

	res = tls_connection_encrypt(sm->ssl_ctx, data->ssl.conn,
				     plain, plain_len,
				     pos, data->ssl.tls_out_limit);
	if (res < 0) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Failed to encrypt Phase 2 "
			   "data");
		free(resp);
		return -1;
	}

	*out_len = sizeof(struct eap_hdr) + 2 + res;
	resp->length = host_to_be16(*out_len);
	*out_data = (u8 *) resp;
	return 0;
}


static int eap_peap_phase2_nak(struct eap_sm *sm,
			       struct eap_peap_data *data,
			       struct eap_hdr *hdr,
			       u8 **resp, size_t *resp_len)
{
	struct eap_hdr *resp_hdr;
	u8 *pos = (u8 *) (hdr + 1);

	wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase 2 Request: Nak type=%d", *pos);
	wpa_hexdump(MSG_DEBUG, "EAP-PEAP: Allowed Phase2 EAP types",
		    data->phase2_types, data->num_phase2_types);
	*resp_len = sizeof(struct eap_hdr) + 1 + data->num_phase2_types;
	*resp = malloc(*resp_len);
	if (*resp == NULL)
		return -1;

	resp_hdr = (struct eap_hdr *) (*resp);
	resp_hdr->code = EAP_CODE_RESPONSE;
	resp_hdr->identifier = hdr->identifier;
	resp_hdr->length = host_to_be16(*resp_len);
	pos = (u8 *) (resp_hdr + 1);
	*pos++ = EAP_TYPE_NAK;
	memcpy(pos, data->phase2_types, data->num_phase2_types);

	return 0;
}


static int eap_peap_phase2_request(struct eap_sm *sm,
				   struct eap_peap_data *data,
				   struct eap_method_ret *ret,
				   struct eap_hdr *req,
				   struct eap_hdr *hdr,
				   u8 **resp, size_t *resp_len)
{
	size_t len = be_to_host16(hdr->length);
	u8 *pos;
	struct eap_method_ret iret;
	struct wpa_ssid *config = eap_get_config(sm);

	if (len <= sizeof(struct eap_hdr)) {
		wpa_printf(MSG_INFO, "EAP-PEAP: too short "
			   "Phase 2 request (len=%lu)", (unsigned long) len);
		return -1;
	}
	pos = (u8 *) (hdr + 1);
	wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase 2 Request: type=%d", *pos);
	switch (*pos) {
	case EAP_TYPE_IDENTITY:
		*resp = eap_sm_buildIdentity(sm, req->identifier, resp_len, 1);
		break;
	case EAP_TYPE_TLV:
		memset(&iret, 0, sizeof(iret));
		if (eap_tlv_process(sm, &iret, hdr, resp, resp_len)) {
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			return -1;
		}
		if (iret.methodState == METHOD_DONE ||
		    iret.methodState == METHOD_MAY_CONT) {
			ret->methodState = iret.methodState;
			ret->decision = iret.decision;
			data->phase2_success = 1;
		}
		break;
	default:
		if (data->phase2_type == EAP_TYPE_NONE) {
			int i;
			for (i = 0; i < data->num_phase2_types; i++) {
				if (data->phase2_types[i] != *pos)
					continue;

				data->phase2_type = *pos;
				wpa_printf(MSG_DEBUG, "EAP-PEAP: Selected "
					   "Phase 2 EAP method %d",
					   data->phase2_type);
				break;
			}
		}
		if (*pos != data->phase2_type || *pos == EAP_TYPE_NONE) {
			if (eap_peap_phase2_nak(sm, data, hdr, resp, resp_len))
				return -1;
			return 0;
		}

		if (data->phase2_priv == NULL) {
			data->phase2_method = eap_sm_get_eap_methods(*pos);
			if (data->phase2_method) {
				sm->init_phase2 = 1;
				data->phase2_priv =
					data->phase2_method->init(sm);
				sm->init_phase2 = 0;
			}
		}
		if (data->phase2_priv == NULL || data->phase2_method == NULL) {
			wpa_printf(MSG_INFO, "EAP-PEAP: failed to initialize "
				   "Phase 2 EAP method %d", *pos);
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			return -1;
		}
		memset(&iret, 0, sizeof(iret));
		*resp = data->phase2_method->process(sm, data->phase2_priv,
						     &iret, (u8 *) hdr, len,
						     resp_len);
		if ((iret.methodState == METHOD_DONE ||
		     iret.methodState == METHOD_MAY_CONT) &&
		    (iret.decision == DECISION_UNCOND_SUCC ||
		     iret.decision == DECISION_COND_SUCC)) {
			data->phase2_success = 1;
		}
		break;
	}

	if (*resp == NULL &&
	    (config->pending_req_identity || config->pending_req_password ||
	     config->pending_req_otp)) {
		free(data->pending_phase2_req);
		data->pending_phase2_req = malloc(len);
		if (data->pending_phase2_req) {
			memcpy(data->pending_phase2_req, hdr, len);
			data->pending_phase2_req_len = len;
		}
	}

	return 0;
}


static int eap_peap_decrypt(struct eap_sm *sm,
			    struct eap_peap_data *data,
			    struct eap_method_ret *ret,
			    struct eap_hdr *req,
			    u8 *in_data, size_t in_len,
			    u8 **out_data, size_t *out_len)
{
	u8 *in_decrypted;
	int buf_len, len_decrypted, len, skip_change = 0, res;
	struct eap_hdr *hdr, *rhdr;
	u8 *resp = NULL;
	size_t resp_len;

	wpa_printf(MSG_DEBUG, "EAP-PEAP: received %lu bytes encrypted data for"
		   " Phase 2", (unsigned long) in_len);

	if (data->pending_phase2_req) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Pending Phase 2 request - "
			   "skip decryption and use old data");
		in_decrypted = data->pending_phase2_req;
		data->pending_phase2_req = NULL;
		len_decrypted = data->pending_phase2_req_len;
		skip_change = 1;
		goto continue_req;
	}

	res = eap_tls_data_reassemble(sm, &data->ssl, &in_data, &in_len);
	if (res < 0 || res == 1)
		return res;

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
		return -1;
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
		return 0;
	}

continue_req:
	wpa_hexdump(MSG_DEBUG, "EAP-PEAP: Decrypted Phase 2 EAP", in_decrypted,
		    len_decrypted);

	hdr = (struct eap_hdr *) in_decrypted;
	if (len_decrypted == 5 && hdr->code == EAP_CODE_REQUEST &&
	    be_to_host16(hdr->length) == 5 &&
	    in_decrypted[4] == EAP_TYPE_IDENTITY) {
		/* At least FreeRADIUS seems to send full EAP header with
		 * EAP Request Identity */
		skip_change = 1;
	}
	if (len_decrypted >= 5 && hdr->code == EAP_CODE_REQUEST &&
	    in_decrypted[4] == EAP_TYPE_TLV) {
		skip_change = 1;
	}

	if (data->peap_version == 0 && !skip_change) {
		struct eap_hdr *nhdr = malloc(sizeof(struct eap_hdr) +
					      len_decrypted);
		if (nhdr == NULL) {
			free(in_decrypted);
			return 0;
		}
		memcpy((u8 *) (nhdr + 1), in_decrypted, len_decrypted);
		free(in_decrypted);
		nhdr->code = req->code;
		nhdr->identifier = req->identifier;
		nhdr->length = host_to_be16(sizeof(struct eap_hdr) +
					    len_decrypted);

		len_decrypted += sizeof(struct eap_hdr);
		in_decrypted = (u8 *) nhdr;
	}
	hdr = (struct eap_hdr *) in_decrypted;
	if (len_decrypted < sizeof(*hdr)) {
		free(in_decrypted);
		wpa_printf(MSG_INFO, "EAP-PEAP: Too short Phase 2 "
			   "EAP frame (len=%d)", len_decrypted);
		return 0;
	}
	len = be_to_host16(hdr->length);
	if (len > len_decrypted) {
		free(in_decrypted);
		wpa_printf(MSG_INFO, "EAP-PEAP: Length mismatch in "
			   "Phase 2 EAP frame (len=%d hdr->length=%d)",
			   len_decrypted, len);
		return 0;
	}
	if (len < len_decrypted) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Odd.. Phase 2 EAP header has "
			   "shorter length than full decrypted data (%d < %d)",
			   len, len_decrypted);
		if (sm->workaround && len == 4 && len_decrypted == 5 &&
		    in_decrypted[4] == EAP_TYPE_IDENTITY) {
			/* Radiator 3.9 seems to set Phase 2 EAP header to use
			 * incorrect length for the EAP-Request Identity
			 * packet, so fix the inner header to interoperate..
			 * This was fixed in 2004-06-23 patch for Radiator and
			 * this workaround can be removed at some point. */
			wpa_printf(MSG_INFO, "EAP-PEAP: workaround -> replace "
				   "Phase 2 EAP header len (%d) with real "
				   "decrypted len (%d)", len, len_decrypted);
			len = len_decrypted;
			hdr->length = host_to_be16(len);
		}
	}
	wpa_printf(MSG_DEBUG, "EAP-PEAP: received Phase 2: code=%d "
		   "identifier=%d length=%d", hdr->code, hdr->identifier, len);
	switch (hdr->code) {
	case EAP_CODE_REQUEST:
		if (eap_peap_phase2_request(sm, data, ret, req, hdr,
					    &resp, &resp_len)) {
			free(in_decrypted);
			wpa_printf(MSG_INFO, "EAP-PEAP: Phase2 Request "
				   "processing failed");
			return 0;
		}
		break;
	case EAP_CODE_SUCCESS:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase 2 Success");
		if (data->peap_version == 1) {
			/* EAP-Success within TLS tunnel is used to indicate
			 * shutdown of the TLS channel. The authentication has
			 * been completed. */
			wpa_printf(MSG_DEBUG, "EAP-PEAP: Version 1 - "
				   "EAP-Success within TLS tunnel - "
				   "authentication completed");
			ret->decision = DECISION_UNCOND_SUCC;
			ret->methodState = METHOD_DONE;
			data->phase2_success = 1;
			if (data->peap_outer_success == 2) {
				free(in_decrypted);
				wpa_printf(MSG_DEBUG, "EAP-PEAP: Use TLS ACK "
					   "to finish authentication");
				return 1;
			} else if (data->peap_outer_success == 1) {
				/* Reply with EAP-Success within the TLS
				 * channel to complete the authentication. */
				resp_len = sizeof(struct eap_hdr);
				resp = malloc(resp_len);
				if (resp) {
					memset(resp, 0, resp_len);
					rhdr = (struct eap_hdr *) resp;
					rhdr->code = EAP_CODE_SUCCESS;
					rhdr->identifier = hdr->identifier;
					rhdr->length = host_to_be16(resp_len);
				}
			} else {
				/* No EAP-Success expected for Phase 1 (outer,
				 * unencrypted auth), so force EAP state
				 * machine to SUCCESS state. */
				sm->peap_done = TRUE;
			}
		} else {
			/* FIX: ? */
		}
		break;
	case EAP_CODE_FAILURE:
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Phase 2 Failure");
		ret->decision = DECISION_FAIL;
		ret->methodState = METHOD_MAY_CONT;
		ret->allowNotifications = FALSE;
		/* Reply with EAP-Failure within the TLS channel to complete
		 * failure reporting. */
		resp_len = sizeof(struct eap_hdr);
		resp = malloc(resp_len);
		if (resp) {
			memset(resp, 0, resp_len);
			rhdr = (struct eap_hdr *) resp;
			rhdr->code = EAP_CODE_FAILURE;
			rhdr->identifier = hdr->identifier;
			rhdr->length = host_to_be16(resp_len);
		}
		break;
	default:
		wpa_printf(MSG_INFO, "EAP-PEAP: Unexpected code=%d in "
			   "Phase 2 EAP header", hdr->code);
		break;
	}

	free(in_decrypted);

	if (resp) {
		u8 *resp_pos;
		size_t resp_send_len;
		int skip_change = 0;

		wpa_hexdump_key(MSG_DEBUG, "EAP-PEAP: Encrypting Phase 2 data",
				resp, resp_len);
		/* PEAP version changes */
		if (resp_len >= 5 && resp[0] == EAP_CODE_RESPONSE &&
		    resp[4] == EAP_TYPE_TLV)
			skip_change = 1;
		if (data->peap_version == 0 && !skip_change) {
			resp_pos = resp + sizeof(struct eap_hdr);
			resp_send_len = resp_len - sizeof(struct eap_hdr);
		} else {
			resp_pos = resp;
			resp_send_len = resp_len;
		}

		if (eap_peap_encrypt(sm, data, req->identifier,
				     resp_pos, resp_send_len,
				     out_data, out_len)) {
			wpa_printf(MSG_INFO, "EAP-PEAP: Failed to encrypt "
				   "a Phase 2 frame");
		}
		free(resp);
	}

	return 0;
}


static u8 * eap_peap_process(struct eap_sm *sm, void *priv,
			     struct eap_method_ret *ret,
			     u8 *reqData, size_t reqDataLen,
			     size_t *respDataLen)
{
	struct eap_hdr *req;
	int left, res;
	unsigned int tls_msg_len;
	u8 flags, *pos, *resp, id;
	struct eap_peap_data *data = priv;

	if (tls_get_errors(sm->ssl_ctx)) {
		wpa_printf(MSG_INFO, "EAP-PEAP: TLS errors detected");
		ret->ignore = TRUE;
		return NULL;
	}

	req = (struct eap_hdr *) reqData;
	pos = (u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 2 || *pos != EAP_TYPE_PEAP ||
	    (left = be_to_host16(req->length)) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-PEAP: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}
	left -= sizeof(struct eap_hdr);
	id = req->identifier;
	pos++;
	flags = *pos++;
	left -= 2;
	wpa_printf(MSG_DEBUG, "EAP-PEAP: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) reqDataLen, flags);
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-PEAP: Short frame with TLS "
				   "length");
			ret->ignore = TRUE;
			return NULL;
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

	ret->ignore = FALSE;
	ret->methodState = METHOD_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	if (flags & EAP_TLS_FLAGS_START) {
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Start (server ver=%d, own "
			   "ver=%d)", flags & EAP_PEAP_VERSION_MASK,
			data->peap_version);
		if ((flags & EAP_PEAP_VERSION_MASK) < data->peap_version)
			data->peap_version = flags & EAP_PEAP_VERSION_MASK;
		if (data->force_peap_version >= 0 &&
		    data->force_peap_version != data->peap_version) {
			wpa_printf(MSG_WARNING, "EAP-PEAP: Failed to select "
				   "forced PEAP version %d",
				   data->force_peap_version);
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			ret->allowNotifications = FALSE;
			return NULL;
		}
		wpa_printf(MSG_DEBUG, "EAP-PEAP: Using PEAP version %d",
			   data->peap_version);
		left = 0; /* make sure that this frame is empty, even though it
			   * should always be, anyway */
	}

	resp = NULL;
	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn) &&
	    !data->resuming) {
		res = eap_peap_decrypt(sm, data, ret, req, pos, left,
				       &resp, respDataLen);
	} else {
		res = eap_tls_process_helper(sm, &data->ssl, EAP_TYPE_PEAP,
					     data->peap_version, id, pos, left,
					     &resp, respDataLen);

		if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
			char *label;
			wpa_printf(MSG_DEBUG,
				   "EAP-PEAP: TLS done, proceed to Phase 2");
			free(data->key_data);
			/* draft-josefsson-ppext-eap-tls-eap-05.txt
			 * specifies that PEAPv1 would use "client PEAP
			 * encryption" as the label. However, most existing
			 * PEAPv1 implementations seem to be using the old
			 * label, "client EAP encryption", instead. Use the old
			 * label by default, but allow it to be configured with
			 * phase1 parameter peaplabel=1. */
			if (data->peap_version > 1 || data->force_new_label)
				label = "client PEAP encryption";
			else
				label = "client EAP encryption";
			wpa_printf(MSG_DEBUG, "EAP-PEAP: using label '%s' in "
				   "key derivation", label);
			data->key_data =
				eap_tls_derive_key(sm, &data->ssl, label,
						   EAP_TLS_KEY_LEN);
			if (data->key_data) {
				wpa_hexdump_key(MSG_DEBUG, 
						"EAP-PEAP: Derived key",
						data->key_data,
						EAP_TLS_KEY_LEN);
			} else {
				wpa_printf(MSG_DEBUG, "EAP-PEAP: Failed to "
					   "derive key");
			}
			data->resuming = 0;
		}
	}

	if (ret->methodState == METHOD_DONE) {
		ret->allowNotifications = FALSE;
	}

	if (res == 1) {
		return eap_tls_build_ack(&data->ssl, respDataLen, id,
					 EAP_TYPE_PEAP, data->peap_version);
	}

	return resp;
}


static Boolean eap_peap_has_reauth_data(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	return tls_connection_established(sm->ssl_ctx, data->ssl.conn) &&
		data->phase2_success;
}


static void eap_peap_deinit_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	free(data->pending_phase2_req);
	data->pending_phase2_req = NULL;
}


static void * eap_peap_init_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	free(data->key_data);
	data->key_data = NULL;
	if (eap_tls_reauth_init(sm, &data->ssl)) {
		free(data);
		return NULL;
	}
	data->phase2_success = 0;
	data->resuming = 1;
	sm->peap_done = FALSE;
	return priv;
}


static int eap_peap_get_status(struct eap_sm *sm, void *priv, char *buf,
			       size_t buflen, int verbose)
{
	struct eap_peap_data *data = priv;
	int len;

	len = eap_tls_status(sm, &data->ssl, buf, buflen, verbose);
	if (data->phase2_method) {
		len += snprintf(buf + len, buflen - len,
				"EAP-PEAPv%d Phase2 method=%s\n",
				data->peap_version, data->phase2_method->name);
	}
	return len;
}


static Boolean eap_peap_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_peap_data *data = priv;
	return data->key_data != NULL && data->phase2_success;
}


static u8 * eap_peap_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_peap_data *data = priv;
	u8 *key;

	if (data->key_data == NULL || !data->phase2_success)
		return NULL;

	key = malloc(EAP_TLS_KEY_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_TLS_KEY_LEN;
	memcpy(key, data->key_data, EAP_TLS_KEY_LEN);

	return key;
}


const struct eap_method eap_method_peap =
{
	.method = EAP_TYPE_PEAP,
	.name = "PEAP",
	.init = eap_peap_init,
	.deinit = eap_peap_deinit,
	.process = eap_peap_process,
	.isKeyAvailable = eap_peap_isKeyAvailable,
	.getKey = eap_peap_getKey,
	.get_status = eap_peap_get_status,
	.has_reauth_data = eap_peap_has_reauth_data,
	.deinit_for_reauth = eap_peap_deinit_for_reauth,
	.init_for_reauth = eap_peap_init_for_reauth,
};
