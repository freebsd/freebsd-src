/*
 * hostapd / EAP-SIM (draft-haverinen-pppext-eap-sim-15.txt)
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "hostapd.h"
#include "common.h"
#include "crypto.h"
#include "eap_i.h"
#include "eap_sim_common.h"
#include "eap_sim_db.h"


#define EAP_SIM_VERSION 1

/* EAP-SIM Subtypes */
#define EAP_SIM_SUBTYPE_START 10
#define EAP_SIM_SUBTYPE_CHALLENGE 11
#define EAP_SIM_SUBTYPE_NOTIFICATION 12
#define EAP_SIM_SUBTYPE_REAUTHENTICATION 13
#define EAP_SIM_SUBTYPE_CLIENT_ERROR 14

/* AT_CLIENT_ERROR_CODE error codes */
#define EAP_SIM_UNABLE_TO_PROCESS_PACKET 0
#define EAP_SIM_UNSUPPORTED_VERSION 1
#define EAP_SIM_INSUFFICIENT_NUM_OF_CHAL 2
#define EAP_SIM_RAND_NOT_FRESH 3

#define KC_LEN 8
#define SRES_LEN 4
#define EAP_SIM_MAX_FAST_REAUTHS 1000

#define EAP_SIM_MAX_CHAL 3

struct eap_sim_data {
	u8 mk[EAP_SIM_MK_LEN];
	u8 nonce_mt[EAP_SIM_NONCE_MT_LEN];
	u8 k_aut[EAP_SIM_K_AUT_LEN];
	u8 k_encr[EAP_SIM_K_ENCR_LEN];
	u8 msk[EAP_SIM_KEYING_DATA_LEN];
	u8 kc[EAP_SIM_MAX_CHAL][KC_LEN];
	u8 sres[EAP_SIM_MAX_CHAL][SRES_LEN];
	u8 rand[EAP_SIM_MAX_CHAL][GSM_RAND_LEN];
	int num_chal;
	enum { START, CHALLENGE, SUCCESS, FAILURE } state;
};


static const char * eap_sim_state_txt(int state)
{
	switch (state) {
	case START:
		return "START";
	case CHALLENGE:
		return "CHALLENGE";
	case SUCCESS:
		return "SUCCESS";
	case FAILURE:
		return "FAILURE";
	default:
		return "Unknown?!";
	}
}


static void eap_sim_state(struct eap_sim_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-SIM %s -> %s",
		   eap_sim_state_txt(data->state),
		   eap_sim_state_txt(state));
	data->state = state;
}


static void * eap_sim_init(struct eap_sm *sm)
{
	struct eap_sim_data *data;

	if (sm->eap_sim_db_priv == NULL) {
		wpa_printf(MSG_WARNING, "EAP-SIM: eap_sim_db not configured");
		return NULL;
	}

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = START;

	return data;
}


static void eap_sim_reset(struct eap_sm *sm, void *priv)
{
	struct eap_sim_data *data = priv;
	free(data);
}


static u8 * eap_sim_build_start(struct eap_sm *sm, struct eap_sim_data *data,
				int id, size_t *reqDataLen)
{
	struct eap_sim_msg *msg;
	u8 ver[2];

	msg = eap_sim_msg_init(EAP_CODE_REQUEST, id, EAP_TYPE_SIM,
			       EAP_SIM_SUBTYPE_START);
	if (eap_sim_db_identity_known(sm->eap_sim_db_priv, sm->identity,
				      sm->identity_len)) {
		eap_sim_msg_add(msg, EAP_SIM_AT_PERMANENT_ID_REQ, 0, NULL, 0);
	}
	ver[0] = 0;
	ver[1] = EAP_SIM_VERSION;
	eap_sim_msg_add(msg, EAP_SIM_AT_VERSION_LIST, sizeof(ver),
			ver, sizeof(ver));
	return eap_sim_msg_finish(msg, reqDataLen, NULL, NULL, 0);
}


static u8 * eap_sim_build_challenge(struct eap_sm *sm,
				    struct eap_sim_data *data,
				    int id, size_t *reqDataLen)
{
	struct eap_sim_msg *msg;

	msg = eap_sim_msg_init(EAP_CODE_REQUEST, id, EAP_TYPE_SIM,
			       EAP_SIM_SUBTYPE_CHALLENGE);
	eap_sim_msg_add(msg, EAP_SIM_AT_RAND, 0, (u8 *) data->rand,
			data->num_chal * GSM_RAND_LEN);
	eap_sim_msg_add_mac(msg, EAP_SIM_AT_MAC);
	return eap_sim_msg_finish(msg, reqDataLen, data->k_aut, data->nonce_mt,
				  EAP_SIM_NONCE_MT_LEN);
}


static u8 * eap_sim_buildReq(struct eap_sm *sm, void *priv, int id,
			     size_t *reqDataLen)
{
	struct eap_sim_data *data = priv;

	switch (data->state) {
	case START:
		return eap_sim_build_start(sm, data, id, reqDataLen);
	case CHALLENGE:
		return eap_sim_build_challenge(sm, data, id, reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-SIM: Unknown state %d in "
			   "buildReq", data->state);
		break;
	}
	return NULL;
}


static Boolean eap_sim_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_sim_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos, subtype;
	size_t len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 4 || *pos != EAP_TYPE_SIM ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-SIM: Invalid frame");
		return TRUE;
	}
	subtype = pos[1];

	if (subtype == EAP_SIM_SUBTYPE_CLIENT_ERROR)
		return FALSE;

	switch (data->state) {
	case START:
		if (subtype != EAP_SIM_SUBTYPE_START) {
			wpa_printf(MSG_INFO, "EAP-SIM: Unexpected response "
				   "subtype %d", subtype);
			return TRUE;
		}
		break;
	case CHALLENGE:
		if (subtype != EAP_SIM_SUBTYPE_CHALLENGE) {
			wpa_printf(MSG_INFO, "EAP-SIM: Unexpected response "
				   "subtype %d", subtype);
			return TRUE;
		}
		break;
	default:
		wpa_printf(MSG_INFO, "EAP-SIM: Unexpected state (%d) for "
			   "processing a response", data->state);
		return TRUE;
	}

	return FALSE;
}


static int eap_sim_supported_ver(struct eap_sim_data *data, int version)
{
	return version == EAP_SIM_VERSION;
}


static void eap_sim_derive_mk(struct eap_sim_data *data,
			      const u8 *identity, size_t identity_len,
			      const u8 *nonce_mt, int selected_version,
			      int num_chal, const u8 *kc)
{
	u8 sel_ver[2], ver_list[2];
	const unsigned char *addr[5];
	size_t len[5];

	addr[0] = identity;
	addr[1] = kc;
	addr[2] = nonce_mt;
	addr[3] = ver_list;
	addr[4] = sel_ver;

	len[0] = identity_len;
	len[1] = num_chal * KC_LEN;
	len[2] = EAP_SIM_NONCE_MT_LEN;
	len[3] = sizeof(ver_list);
	len[4] = sizeof(sel_ver);

	ver_list[0] = 0;
	ver_list[1] = EAP_SIM_VERSION;
	sel_ver[0] = selected_version >> 8;
	sel_ver[1] = selected_version & 0xff;

	/* MK = SHA1(Identity|n*Kc|NONCE_MT|Version List|Selected Version) */
	sha1_vector(5, addr, len, data->mk);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: MK", data->mk, EAP_SIM_MK_LEN);
}


static void eap_sim_process_start(struct eap_sm *sm,
				  struct eap_sim_data *data,
				  u8 *respData, size_t respDataLen,
				  struct eap_sim_attrs *attr)
{
	wpa_printf(MSG_DEBUG, "EAP-SIM: Receive start response");

	if (attr->nonce_mt == NULL || attr->selected_version < 0) {
		wpa_printf(MSG_DEBUG, "EAP-SIM: Start/Response missing "
			   "required attributes");
		eap_sim_state(data, FAILURE);
		return;
	}

	if (!eap_sim_supported_ver(data, attr->selected_version)) {
		wpa_printf(MSG_DEBUG, "EAP-SIM: Peer selected unsupported "
			   "version %d", attr->selected_version);
		eap_sim_state(data, FAILURE);
		return;
	}

	if (attr->identity) {
		free(sm->identity);
		sm->identity = malloc(attr->identity_len);
		if (sm->identity) {
			memcpy(sm->identity, attr->identity,
			       attr->identity_len);
			sm->identity_len = attr->identity_len;
		}
	}

	if (sm->identity == NULL || sm->identity_len < 1 ||
	    sm->identity[0] != '1') {
		wpa_printf(MSG_DEBUG, "EAP-SIM: Could not get proper permanent"
			   " user name");
		eap_sim_state(data, FAILURE);
		return;
	}

	wpa_hexdump_ascii(MSG_DEBUG, "EAP-SIM: Identity",
			  sm->identity, sm->identity_len);

	data->num_chal = eap_sim_db_get_gsm_triplets(
		sm->eap_sim_db_priv, sm->identity, sm->identity_len,
		EAP_SIM_MAX_CHAL,
		(u8 *) data->rand, (u8 *) data->kc, (u8 *) data->sres);
	if (data->num_chal < 2) {
		wpa_printf(MSG_INFO, "EAP-SIM: Failed to get GSM "
			   "authentication triplets for the peer");
		eap_sim_state(data, FAILURE);
		return;
	}

	memcpy(data->nonce_mt, attr->nonce_mt, EAP_SIM_NONCE_MT_LEN);
	eap_sim_derive_mk(data, sm->identity, sm->identity_len, attr->nonce_mt,
			  attr->selected_version, data->num_chal,
			  (u8 *) data->kc);
	eap_sim_derive_keys(data->mk, data->k_encr, data->k_aut, data->msk);

	eap_sim_state(data, CHALLENGE);
}


static void eap_sim_process_challenge(struct eap_sm *sm,
				      struct eap_sim_data *data,
				      u8 *respData, size_t respDataLen,
				      struct eap_sim_attrs *attr)
{
	if (attr->mac == NULL ||
	    eap_sim_verify_mac(data->k_aut, respData, respDataLen, attr->mac,
			       (u8 *) data->sres, data->num_chal * SRES_LEN)) {
		wpa_printf(MSG_WARNING, "EAP-SIM: Challenge message "
			   "did not include valid AT_MAC");
		eap_sim_state(data, FAILURE);
		return;
	}

	wpa_printf(MSG_DEBUG, "EAP-SIM: Challenge response includes the "
		   "correct AT_MAC");
	eap_sim_state(data, SUCCESS);
}


static void eap_sim_process_client_error(struct eap_sm *sm,
					 struct eap_sim_data *data,
					 u8 *respData, size_t respDataLen,
					 struct eap_sim_attrs *attr)
{
	wpa_printf(MSG_DEBUG, "EAP-SIM: Client reported error %d",
		   attr->client_error_code);
	eap_sim_state(data, FAILURE);
}


static void eap_sim_process(struct eap_sm *sm, void *priv,
			    u8 *respData, size_t respDataLen)
{
	struct eap_sim_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos, subtype;
	size_t len;
	struct eap_sim_attrs attr;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	subtype = pos[1];
	len = ntohs(resp->length);
	pos += 4;

	if (eap_sim_parse_attr(pos, respData + len, &attr, 0, 0)) {
		wpa_printf(MSG_DEBUG, "EAP-SIM: Failed to parse attributes");
		eap_sim_state(data, FAILURE);
		return;
	}

	if (subtype == EAP_SIM_SUBTYPE_CLIENT_ERROR) {
		eap_sim_process_client_error(sm, data, respData, len, &attr);
		return;
	}

	switch (data->state) {
	case START:
		eap_sim_process_start(sm, data, respData, len, &attr);
		break;
	case CHALLENGE:
		eap_sim_process_challenge(sm, data, respData, len, &attr);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-SIM: Unknown state %d in "
			   "process", data->state);
		break;
	}
}


static Boolean eap_sim_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_sim_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_sim_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_sim_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(EAP_SIM_KEYING_DATA_LEN);
	if (key == NULL)
		return NULL;
	memcpy(key, data->msk, EAP_SIM_KEYING_DATA_LEN);
	*len = EAP_SIM_KEYING_DATA_LEN;
	return key;
}


static Boolean eap_sim_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_sim_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_sim =
{
	.method = EAP_TYPE_SIM,
	.name = "SIM",
	.init = eap_sim_init,
	.reset = eap_sim_reset,
	.buildReq = eap_sim_buildReq,
	.check = eap_sim_check,
	.process = eap_sim_process,
	.isDone = eap_sim_isDone,
	.getKey = eap_sim_getKey,
	.isSuccess = eap_sim_isSuccess,
};
