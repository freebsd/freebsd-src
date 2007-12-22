/*
 * hostapd / EAP-TLV (draft-josefsson-pppext-eap-tls-eap-07.txt)
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


/* EAP-TLV TLVs (draft-josefsson-ppext-eap-tls-eap-07.txt) */
#define EAP_TLV_RESULT_TLV 3 /* Acknowledged Result */
#define EAP_TLV_NAK_TLV 4
#define EAP_TLV_CRYPTO_BINDING_TLV 5
#define EAP_TLV_CONNECTION_BINDING_TLV 6
#define EAP_TLV_VENDOR_SPECIFIC_TLV 7
#define EAP_TLV_URI_TLV 8
#define EAP_TLV_EAP_PAYLOAD_TLV 9
#define EAP_TLV_INTERMEDIATE_RESULT_TLV 10

#define EAP_TLV_RESULT_SUCCESS 1
#define EAP_TLV_RESULT_FAILURE 2


struct eap_tlv_data {
	enum { CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_tlv_init(struct eap_sm *sm)
{
	struct eap_tlv_data *data;

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = CONTINUE;

	return data;
}


static void eap_tlv_reset(struct eap_sm *sm, void *priv)
{
	struct eap_tlv_data *data = priv;
	free(data);
}


static u8 * eap_tlv_buildReq(struct eap_sm *sm, void *priv, int id,
			     size_t *reqDataLen)
{
	struct eap_hdr *req;
	u8 *pos;
	u16 status;

	if (sm->tlv_request == TLV_REQ_SUCCESS) {
		status = EAP_TLV_RESULT_SUCCESS;
	} else {
		status = EAP_TLV_RESULT_FAILURE;
	}

	*reqDataLen = sizeof(struct eap_hdr) + 1 + 6;
	req = malloc(*reqDataLen);
	if (req == NULL)
		return NULL;

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = host_to_be16(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_TLV;
	*pos++ = 0x80; /* Mandatory */
	*pos++ = EAP_TLV_RESULT_TLV;
	/* Length */
	*pos++ = 0;
	*pos++ = 2;
	/* Status */
	*pos++ = status >> 8;
	*pos++ = status & 0xff;

	return (u8 *) req;
}


static Boolean eap_tlv_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 1 || *pos != EAP_TYPE_TLV ||
	    (ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-TLV: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_tlv_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_tlv_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos;
	size_t left;
	u8 *result_tlv = NULL;
	size_t result_tlv_len = 0;
	int tlv_type, mandatory, tlv_len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);

	/* Parse TLVs */
	left = be_to_host16(resp->length) - sizeof(struct eap_hdr) - 1;
	pos = (u8 *) (resp + 1);
	pos++;
	wpa_hexdump(MSG_DEBUG, "EAP-TLV: Received TLVs", pos, left);
	while (left >= 4) {
		mandatory = !!(pos[0] & 0x80);
		tlv_type = pos[0] & 0x3f;
		tlv_type = (tlv_type << 8) | pos[1];
		tlv_len = ((int) pos[2] << 8) | pos[3];
		pos += 4;
		left -= 4;
		if ((size_t) tlv_len > left) {
			wpa_printf(MSG_DEBUG, "EAP-TLV: TLV underrun "
				   "(tlv_len=%d left=%lu)", tlv_len,
				   (unsigned long) left);
			data->state = FAILURE;
			return;
		}
		switch (tlv_type) {
		case EAP_TLV_RESULT_TLV:
			result_tlv = pos;
			result_tlv_len = tlv_len;
			break;
		default:
			wpa_printf(MSG_DEBUG, "EAP-TLV: Unsupported TLV Type "
				   "%d%s", tlv_type,
				   mandatory ? " (mandatory)" : "");
			if (mandatory) {
				data->state = FAILURE;
				return;
			}
			/* Ignore this TLV, but process other TLVs */
			break;
		}

		pos += tlv_len;
		left -= tlv_len;
	}
	if (left) {
		wpa_printf(MSG_DEBUG, "EAP-TLV: Last TLV too short in "
			   "Request (left=%lu)", (unsigned long) left);
		data->state = FAILURE;
		return;
	}

	/* Process supported TLVs */
	if (result_tlv) {
		int status;
		const char *requested;

		wpa_hexdump(MSG_DEBUG, "EAP-TLV: Result TLV",
			    result_tlv, result_tlv_len);
		if (result_tlv_len < 2) {
			wpa_printf(MSG_INFO, "EAP-TLV: Too short Result TLV "
				   "(len=%lu)",
				   (unsigned long) result_tlv_len);
			data->state = FAILURE;
			return;
		}
		requested = sm->tlv_request == TLV_REQ_SUCCESS ? "Success" :
			"Failure";
		status = ((int) result_tlv[0] << 8) | result_tlv[1];
		if (status == EAP_TLV_RESULT_SUCCESS) {
			wpa_printf(MSG_INFO, "EAP-TLV: TLV Result - Success "
				   "- requested %s", requested);
			if (sm->tlv_request == TLV_REQ_SUCCESS)
				data->state = SUCCESS;
			else
				data->state = FAILURE;
			
		} else if (status == EAP_TLV_RESULT_FAILURE) {
			wpa_printf(MSG_INFO, "EAP-TLV: TLV Result - Failure - "
				   "requested %s", requested);
			if (sm->tlv_request == TLV_REQ_FAILURE)
				data->state = SUCCESS;
			else
				data->state = FAILURE;
		} else {
			wpa_printf(MSG_INFO, "EAP-TLV: Unknown TLV Result "
				   "Status %d", status);
			data->state = FAILURE;
		}
	}
}


static Boolean eap_tlv_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_tlv_data *data = priv;
	return data->state != CONTINUE;
}


static Boolean eap_tlv_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_tlv_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_tlv_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_TLV, "TLV");
	if (eap == NULL)
		return -1;

	eap->init = eap_tlv_init;
	eap->reset = eap_tlv_reset;
	eap->buildReq = eap_tlv_buildReq;
	eap->check = eap_tlv_check;
	eap->process = eap_tlv_process;
	eap->isDone = eap_tlv_isDone;
	eap->isSuccess = eap_tlv_isSuccess;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}
