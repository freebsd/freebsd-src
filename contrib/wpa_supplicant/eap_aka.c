/*
 * WPA Supplicant / EAP-AKA (draft-arkko-pppext-eap-aka-12.txt)
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
#include "sha1.h"
#include "pcsc_funcs.h"
#include "eap_sim_common.h"

/* EAP-AKA Subtypes */
#define EAP_AKA_SUBTYPE_CHALLENGE 1
#define EAP_AKA_SUBTYPE_AUTHENTICATION_REJECT 2
#define EAP_AKA_SUBTYPE_SYNCHRONIZATION_FAILURE 4
#define EAP_AKA_SUBTYPE_IDENTITY 5
#define EAP_AKA_SUBTYPE_NOTIFICATION 12
#define EAP_AKA_SUBTYPE_REAUTHENTICATION 13
#define EAP_AKA_SUBTYPE_CLIENT_ERROR 14

/* AT_CLIENT_ERROR_CODE error codes */
#define EAP_AKA_UNABLE_TO_PROCESS_PACKET 0

#define AKA_AUTS_LEN 14
#define RES_MAX_LEN 16
#define IK_LEN 16
#define CK_LEN 16
#define EAP_AKA_MAX_FAST_REAUTHS 1000

struct eap_aka_data {
	u8 ik[IK_LEN], ck[CK_LEN], res[RES_MAX_LEN];
	size_t res_len;
	u8 nonce_s[EAP_SIM_NONCE_S_LEN];
	u8 mk[EAP_SIM_MK_LEN];
	u8 k_aut[EAP_SIM_K_AUT_LEN];
	u8 k_encr[EAP_SIM_K_ENCR_LEN];
	u8 msk[EAP_SIM_KEYING_DATA_LEN];
	u8 rand[AKA_RAND_LEN], autn[AKA_AUTN_LEN];
	u8 auts[AKA_AUTS_LEN];

	int num_id_req, num_notification;
	u8 *pseudonym;
	size_t pseudonym_len;
	u8 *reauth_id;
	size_t reauth_id_len;
	int reauth;
	unsigned int counter, counter_too_small;
	u8 *last_eap_identity;
	size_t last_eap_identity_len;
	enum { CONTINUE, SUCCESS, FAILURE } state;
};


static void * eap_aka_init(struct eap_sm *sm)
{
	struct eap_aka_data *data;
	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));

	data->state = CONTINUE;

	return data;
}


static void eap_aka_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_aka_data *data = priv;
	if (data) {
		free(data->pseudonym);
		free(data->reauth_id);
		free(data->last_eap_identity);
		free(data);
	}
}


static int eap_aka_umts_auth(struct eap_sm *sm, struct eap_aka_data *data)
{
	wpa_printf(MSG_DEBUG, "EAP-AKA: UMTS authentication algorithm");
#ifdef PCSC_FUNCS
	return scard_umts_auth(sm->scard_ctx, data->rand,
			       data->autn, data->res, &data->res_len,
			       data->ik, data->ck, data->auts);
#else /* PCSC_FUNCS */
	/* These hardcoded Kc and SRES values are used for testing.
	 * Could consider making them configurable. */
	memset(data->res, '2', RES_MAX_LEN);
	data->res_len = 16;
	memset(data->ik, '3', IK_LEN);
	memset(data->ck, '4', CK_LEN);
	{
		u8 autn[AKA_AUTN_LEN];
		memset(autn, '1', AKA_AUTN_LEN);
		if (memcmp(autn, data->autn, AKA_AUTN_LEN) != 0) {
			wpa_printf(MSG_WARNING, "EAP-AKA: AUTN did not match "
				   "with expected value");
			return -1;
		}
	}
	return 0;
#endif /* PCSC_FUNCS */
}


static void eap_aka_derive_mk(struct eap_aka_data *data,
			      const u8 *identity, size_t identity_len)
{
	const u8 *addr[3];
	size_t len[3];

	addr[0] = identity;
	len[0] = identity_len;
	addr[1] = data->ik;
	len[1] = IK_LEN;
	addr[2] = data->ck;
	len[2] = CK_LEN;

	/* MK = SHA1(Identity|IK|CK) */
	sha1_vector(3, addr, len, data->mk);
	wpa_hexdump_key(MSG_DEBUG, "EAP-AKA: IK", data->ik, IK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-AKA: CK", data->ck, CK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-AKA: MK", data->mk, EAP_SIM_MK_LEN);
}


#define CLEAR_PSEUDONYM	0x01
#define CLEAR_REAUTH_ID	0x02
#define CLEAR_EAP_ID	0x04

static void eap_aka_clear_identities(struct eap_aka_data *data, int id)
{
	wpa_printf(MSG_DEBUG, "EAP-AKA: forgetting old%s%s%s",
		   id & CLEAR_PSEUDONYM ? " pseudonym" : "",
		   id & CLEAR_REAUTH_ID ? " reauth_id" : "",
		   id & CLEAR_EAP_ID ? " eap_id" : "");
	if (id & CLEAR_PSEUDONYM) {
		free(data->pseudonym);
		data->pseudonym = NULL;
		data->pseudonym_len = 0;
	}
	if (id & CLEAR_REAUTH_ID) {
		free(data->reauth_id);
		data->reauth_id = NULL;
		data->reauth_id_len = 0;
	}
	if (id & CLEAR_EAP_ID) {
		free(data->last_eap_identity);
		data->last_eap_identity = NULL;
		data->last_eap_identity_len = 0;
	}
}


static int eap_aka_learn_ids(struct eap_aka_data *data,
			     struct eap_sim_attrs *attr)
{
	if (attr->next_pseudonym) {
		free(data->pseudonym);
		data->pseudonym = malloc(attr->next_pseudonym_len);
		if (data->pseudonym == NULL) {
			wpa_printf(MSG_INFO, "EAP-AKA: (encr) No memory for "
				   "next pseudonym");
			return -1;
		}
		memcpy(data->pseudonym, attr->next_pseudonym,
		       attr->next_pseudonym_len);
		data->pseudonym_len = attr->next_pseudonym_len;
		wpa_hexdump_ascii(MSG_DEBUG,
				  "EAP-AKA: (encr) AT_NEXT_PSEUDONYM",
				  data->pseudonym,
				  data->pseudonym_len);
	}

	if (attr->next_reauth_id) {
		free(data->reauth_id);
		data->reauth_id = malloc(attr->next_reauth_id_len);
		if (data->reauth_id == NULL) {
			wpa_printf(MSG_INFO, "EAP-AKA: (encr) No memory for "
				   "next reauth_id");
			return -1;
		}
		memcpy(data->reauth_id, attr->next_reauth_id,
		       attr->next_reauth_id_len);
		data->reauth_id_len = attr->next_reauth_id_len;
		wpa_hexdump_ascii(MSG_DEBUG,
				  "EAP-AKA: (encr) AT_NEXT_REAUTH_ID",
				  data->reauth_id,
				  data->reauth_id_len);
	}

	return 0;
}


static u8 * eap_aka_client_error(struct eap_sm *sm, struct eap_aka_data *data,
				 struct eap_hdr *req,
				 size_t *respDataLen, int err)
{
	struct eap_sim_msg *msg;

	data->state = FAILURE;
	data->num_id_req = 0;
	data->num_notification = 0;

	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CLIENT_ERROR);
	eap_sim_msg_add(msg, EAP_SIM_AT_CLIENT_ERROR_CODE, err, NULL, 0);
	return eap_sim_msg_finish(msg, respDataLen, NULL, NULL, 0);
}


static u8 * eap_aka_authentication_reject(struct eap_sm *sm,
					  struct eap_aka_data *data,
					  struct eap_hdr *req,
					  size_t *respDataLen)
{
	struct eap_sim_msg *msg;

	data->state = FAILURE;
	data->num_id_req = 0;
	data->num_notification = 0;

	wpa_printf(MSG_DEBUG, "Generating EAP-AKA Authentication-Reject "
		   "(id=%d)", req->identifier);
	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA,
			       EAP_AKA_SUBTYPE_AUTHENTICATION_REJECT);
	return eap_sim_msg_finish(msg, respDataLen, NULL, NULL, 0);
}


static u8 * eap_aka_synchronization_failure(struct eap_sm *sm,
					    struct eap_aka_data *data,
					    struct eap_hdr *req,
					    size_t *respDataLen)
{
	struct eap_sim_msg *msg;

	data->state = FAILURE;
	data->num_id_req = 0;
	data->num_notification = 0;

	wpa_printf(MSG_DEBUG, "Generating EAP-AKA Synchronization-Failure "
		   "(id=%d)", req->identifier);
	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA,
			       EAP_AKA_SUBTYPE_SYNCHRONIZATION_FAILURE);
	wpa_printf(MSG_DEBUG, "   AT_AUTS");
	eap_sim_msg_add_full(msg, EAP_SIM_AT_AUTS, data->auts, AKA_AUTS_LEN);
	return eap_sim_msg_finish(msg, respDataLen, NULL, NULL, 0);
}


static u8 * eap_aka_response_identity(struct eap_sm *sm,
				      struct eap_aka_data *data,
				      struct eap_hdr *req,
				      size_t *respDataLen,
				      enum eap_sim_id_req id_req)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *identity = NULL;
	size_t identity_len = 0;
	struct eap_sim_msg *msg;

	data->reauth = 0;
	if (id_req == ANY_ID && data->reauth_id) {
		identity = data->reauth_id;
		identity_len = data->reauth_id_len;
		data->reauth = 1;
	} else if ((id_req == ANY_ID || id_req == FULLAUTH_ID) &&
		   data->pseudonym) {
		identity = data->pseudonym;
		identity_len = data->pseudonym_len;
		eap_aka_clear_identities(data, CLEAR_REAUTH_ID);
	} else if (id_req != NO_ID_REQ && config && config->identity) {
		identity = config->identity;
		identity_len = config->identity_len;
		eap_aka_clear_identities(data,
					 CLEAR_PSEUDONYM | CLEAR_REAUTH_ID);
	}
	if (id_req != NO_ID_REQ)
		eap_aka_clear_identities(data, CLEAR_EAP_ID);

	wpa_printf(MSG_DEBUG, "Generating EAP-AKA Identity (id=%d)",
		req->identifier);
	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY);

	if (identity) {
		wpa_hexdump_ascii(MSG_DEBUG, "   AT_IDENTITY",
				  identity, identity_len);
		eap_sim_msg_add(msg, EAP_SIM_AT_IDENTITY, identity_len,
				identity, identity_len);
	}

	return eap_sim_msg_finish(msg, respDataLen, NULL, NULL, 0);
}


static u8 * eap_aka_response_challenge(struct eap_sm *sm,
				       struct eap_aka_data *data,
				       struct eap_hdr *req,
				       size_t *respDataLen)
{
	struct eap_sim_msg *msg;

	wpa_printf(MSG_DEBUG, "Generating EAP-AKA Challenge (id=%d)",
		req->identifier);
	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CHALLENGE);
	wpa_printf(MSG_DEBUG, "   AT_RES");
	eap_sim_msg_add(msg, EAP_SIM_AT_RES, data->res_len,
			data->res, data->res_len);
	wpa_printf(MSG_DEBUG, "   AT_MAC");
	eap_sim_msg_add_mac(msg, EAP_SIM_AT_MAC);
	return eap_sim_msg_finish(msg, respDataLen, data->k_aut, (u8 *) "", 0);
}


static u8 * eap_aka_response_reauth(struct eap_sm *sm,
				    struct eap_aka_data *data,
				    struct eap_hdr *req,
				    size_t *respDataLen, int counter_too_small)
{
	struct eap_sim_msg *msg;
	unsigned int counter;

	wpa_printf(MSG_DEBUG, "Generating EAP-AKA Reauthentication (id=%d)",
		   req->identifier);
	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA,
			       EAP_AKA_SUBTYPE_REAUTHENTICATION);
	wpa_printf(MSG_DEBUG, "   AT_IV");
	wpa_printf(MSG_DEBUG, "   AT_ENCR_DATA");
	eap_sim_msg_add_encr_start(msg, EAP_SIM_AT_IV, EAP_SIM_AT_ENCR_DATA);

	if (counter_too_small) {
		wpa_printf(MSG_DEBUG, "   *AT_COUNTER_TOO_SMALL");
		eap_sim_msg_add(msg, EAP_SIM_AT_COUNTER_TOO_SMALL, 0, NULL, 0);
		counter = data->counter_too_small;
	} else
		counter = data->counter;

	wpa_printf(MSG_DEBUG, "   *AT_COUNTER %d", counter);
	eap_sim_msg_add(msg, EAP_SIM_AT_COUNTER, counter, NULL, 0);

	if (eap_sim_msg_add_encr_end(msg, data->k_encr, EAP_SIM_AT_PADDING)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Failed to encrypt "
			   "AT_ENCR_DATA");
		eap_sim_msg_free(msg);
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "   AT_MAC");
	eap_sim_msg_add_mac(msg, EAP_SIM_AT_MAC);
	return eap_sim_msg_finish(msg, respDataLen, data->k_aut, data->nonce_s,
				  EAP_SIM_NONCE_S_LEN);
}


static u8 * eap_aka_response_notification(struct eap_sm *sm,
					  struct eap_aka_data *data,
					  struct eap_hdr *req,
					  size_t *respDataLen,
					  u16 notification)
{
	struct eap_sim_msg *msg;
	u8 *k_aut = (notification & 0x4000) == 0 ? data->k_aut : NULL;

	wpa_printf(MSG_DEBUG, "Generating EAP-AKA Notification (id=%d)",
		   req->identifier);
	msg = eap_sim_msg_init(EAP_CODE_RESPONSE, req->identifier,
			       EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION);
	wpa_printf(MSG_DEBUG, "   AT_NOTIFICATION");
	eap_sim_msg_add(msg, EAP_SIM_AT_NOTIFICATION, notification, NULL, 0);
	if (k_aut && data->reauth) {
		wpa_printf(MSG_DEBUG, "   AT_IV");
		wpa_printf(MSG_DEBUG, "   AT_ENCR_DATA");
		eap_sim_msg_add_encr_start(msg, EAP_SIM_AT_IV,
					   EAP_SIM_AT_ENCR_DATA);
		wpa_printf(MSG_DEBUG, "   *AT_COUNTER %d", data->counter);
		eap_sim_msg_add(msg, EAP_SIM_AT_COUNTER, data->counter,
				NULL, 0);
		if (eap_sim_msg_add_encr_end(msg, data->k_encr,
					     EAP_SIM_AT_PADDING)) {
			wpa_printf(MSG_WARNING, "EAP-AKA: Failed to encrypt "
				   "AT_ENCR_DATA");
			eap_sim_msg_free(msg);
			return NULL;
		}
	}
	if (k_aut) {
		wpa_printf(MSG_DEBUG, "   AT_MAC");
		eap_sim_msg_add_mac(msg, EAP_SIM_AT_MAC);
	}
	return eap_sim_msg_finish(msg, respDataLen, k_aut, (u8 *) "", 0);
}


static u8 * eap_aka_process_identity(struct eap_sm *sm,
				     struct eap_aka_data *data,
				     struct eap_hdr *req, size_t reqDataLen,
				     size_t *respDataLen,
				     struct eap_sim_attrs *attr)
{
	int id_error;

	wpa_printf(MSG_DEBUG, "EAP-AKA: subtype Identity");

	id_error = 0;
	switch (attr->id_req) {
	case NO_ID_REQ:
		break;
	case ANY_ID:
		if (data->num_id_req > 0)
			id_error++;
		data->num_id_req++;
		break;
	case FULLAUTH_ID:
		if (data->num_id_req > 1)
			id_error++;
		data->num_id_req++;
		break;
	case PERMANENT_ID:
		if (data->num_id_req > 2)
			id_error++;
		data->num_id_req++;
		break;
	}
	if (id_error) {
		wpa_printf(MSG_INFO, "EAP-AKA: Too many ID requests "
			   "used within one authentication");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	return eap_aka_response_identity(sm, data, req, respDataLen,
					 attr->id_req);
}


static u8 * eap_aka_process_challenge(struct eap_sm *sm,
				      struct eap_aka_data *data,
				      struct eap_hdr *req, size_t reqDataLen,
				      size_t *respDataLen,
				      struct eap_sim_attrs *attr)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *identity;
	size_t identity_len;
	int res;
	struct eap_sim_attrs eattr;

	wpa_printf(MSG_DEBUG, "EAP-AKA: subtype Challenge");
	data->reauth = 0;
	if (!attr->mac || !attr->rand || !attr->autn) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Challenge message "
			   "did not include%s%s%s",
			   !attr->mac ? " AT_MAC" : "",
			   !attr->rand ? " AT_RAND" : "",
			   !attr->autn ? " AT_AUTN" : "");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}
	memcpy(data->rand, attr->rand, AKA_RAND_LEN);
	memcpy(data->autn, attr->autn, AKA_AUTN_LEN);

	res = eap_aka_umts_auth(sm, data);
	if (res == -1) {
		wpa_printf(MSG_WARNING, "EAP-AKA: UMTS authentication "
			   "failed (AUTN)");
		return eap_aka_authentication_reject(sm, data, req,
						     respDataLen);
	} else if (res == -2) {
		wpa_printf(MSG_WARNING, "EAP-AKA: UMTS authentication "
			   "failed (AUTN seq# -> AUTS)");
		return eap_aka_synchronization_failure(sm, data, req,
						       respDataLen);
	} else if (res) {
		wpa_printf(MSG_WARNING, "EAP-AKA: UMTS authentication failed");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}
	if (data->last_eap_identity) {
		identity = data->last_eap_identity;
		identity_len = data->last_eap_identity_len;
	} else if (data->pseudonym) {
		identity = data->pseudonym;
		identity_len = data->pseudonym_len;
	} else {
		identity = config->identity;
		identity_len = config->identity_len;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-AKA: Selected identity for MK "
			  "derivation", identity, identity_len);
	eap_aka_derive_mk(data, identity, identity_len);
	eap_sim_derive_keys(data->mk, data->k_encr, data->k_aut, data->msk);
	if (eap_sim_verify_mac(data->k_aut, (u8 *) req, reqDataLen, attr->mac,
			       (u8 *) "", 0)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Challenge message "
			   "used invalid AT_MAC");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	/* Old reauthentication and pseudonym identities must not be used
	 * anymore. In other words, if no new identities are received, full
	 * authentication will be used on next reauthentication. */
	eap_aka_clear_identities(data, CLEAR_PSEUDONYM | CLEAR_REAUTH_ID |
				 CLEAR_EAP_ID);

	if (attr->encr_data) {
		if (eap_sim_parse_encr(data->k_encr, attr->encr_data,
				       attr->encr_data_len, attr->iv, &eattr,
				       0)) {
			return eap_aka_client_error(
				sm, data, req, respDataLen,
				EAP_AKA_UNABLE_TO_PROCESS_PACKET);
		}
		eap_aka_learn_ids(data, &eattr);
	}

	if (data->state != FAILURE)
		data->state = SUCCESS;

	data->num_id_req = 0;
	data->num_notification = 0;
	/* draft-arkko-pppext-eap-aka-12.txt specifies that counter
	 * is initialized to one after fullauth, but initializing it to
	 * zero makes it easier to implement reauth verification. */
	data->counter = 0;
	return eap_aka_response_challenge(sm, data, req, respDataLen);
}


static int eap_aka_process_notification_reauth(struct eap_aka_data *data,
					       struct eap_hdr *req,
					       size_t reqDataLen,
					       struct eap_sim_attrs *attr)
{
	struct eap_sim_attrs eattr;

	if (attr->encr_data == NULL || attr->iv == NULL) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Notification message after "
			   "reauth did not include encrypted data");
		return -1;
	}

	if (eap_sim_parse_encr(data->k_encr, attr->encr_data,
			       attr->encr_data_len, attr->iv, &eattr, 0)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Failed to parse encrypted "
			   "data from notification message");
		return -1;
	}

	if (eattr.counter != data->counter) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Counter in notification "
			   "message does not match with counter in reauth "
			   "message");
		return -1;
	}

	return 0;
}


static int eap_aka_process_notification_auth(struct eap_aka_data *data,
					     struct eap_hdr *req,
					     size_t reqDataLen,
					     struct eap_sim_attrs *attr)
{
	if (attr->mac == NULL) {
		wpa_printf(MSG_INFO, "EAP-AKA: no AT_MAC in after_auth "
			   "Notification message");
		return -1;
	}

	if (eap_sim_verify_mac(data->k_aut, (u8 *) req, reqDataLen, attr->mac,
			       (u8 *) "", 0)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Notification message "
			   "used invalid AT_MAC");
		return -1;
	}

	if (data->reauth &&
	    eap_aka_process_notification_reauth(data, req, reqDataLen, attr)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Invalid notification "
			   "message after reauth");
		return -1;
	}

	return 0;
}


static u8 * eap_aka_process_notification(struct eap_sm *sm,
					 struct eap_aka_data *data,
					 struct eap_hdr *req,
					 size_t reqDataLen,
					 size_t *respDataLen,
					 struct eap_sim_attrs *attr)
{
	wpa_printf(MSG_DEBUG, "EAP-AKA: subtype Notification");
	if (data->num_notification > 0) {
		wpa_printf(MSG_INFO, "EAP-AKA: too many notification "
			   "rounds (only one allowed)");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}
	data->num_notification++;
	if (attr->notification == -1) {
		wpa_printf(MSG_INFO, "EAP-AKA: no AT_NOTIFICATION in "
			   "Notification message");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	if ((attr->notification & 0x4000) == 0 &&
	    eap_aka_process_notification_auth(data, req, reqDataLen, attr)) {
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	eap_sim_report_notification(sm->msg_ctx, attr->notification, 1);
	if (attr->notification >= 0 && attr->notification < 32768) {
		data->state = FAILURE;
	}
	return eap_aka_response_notification(sm, data, req, respDataLen,
					     attr->notification);
}


static u8 * eap_aka_process_reauthentication(struct eap_sm *sm,
					     struct eap_aka_data *data,
					     struct eap_hdr *req,
					     size_t reqDataLen,
					     size_t *respDataLen,
					     struct eap_sim_attrs *attr)
{
	struct eap_sim_attrs eattr;

	wpa_printf(MSG_DEBUG, "EAP-AKA: subtype Reauthentication");

	if (data->reauth_id == NULL) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Server is trying "
			   "reauthentication, but no reauth_id available");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	data->reauth = 1;
	if (eap_sim_verify_mac(data->k_aut, (u8 *) req, reqDataLen,
			       attr->mac, (u8 *) "", 0)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Reauthentication "
			   "did not have valid AT_MAC");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	if (attr->encr_data == NULL || attr->iv == NULL) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Reauthentication "
			   "message did not include encrypted data");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	if (eap_sim_parse_encr(data->k_encr, attr->encr_data,
			       attr->encr_data_len, attr->iv, &eattr, 0)) {
		wpa_printf(MSG_WARNING, "EAP-AKA: Failed to parse encrypted "
			   "data from reauthentication message");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	if (eattr.nonce_s == NULL || eattr.counter < 0) {
		wpa_printf(MSG_INFO, "EAP-AKA: (encr) No%s%s in reauth packet",
			   !eattr.nonce_s ? " AT_NONCE_S" : "",
			   eattr.counter < 0 ? " AT_COUNTER" : "");
		return eap_aka_client_error(sm, data, req, respDataLen,
					    EAP_AKA_UNABLE_TO_PROCESS_PACKET);
	}

	if (eattr.counter <= data->counter) {
		wpa_printf(MSG_INFO, "EAP-AKA: (encr) Invalid counter "
			   "(%d <= %d)", eattr.counter, data->counter);
		data->counter_too_small = eattr.counter;
		/* Reply using Re-auth w/ AT_COUNTER_TOO_SMALL. The current
		 * reauth_id must not be used to start a new reauthentication.
		 * However, since it was used in the last EAP-Response-Identity
		 * packet, it has to saved for the following fullauth to be
		 * used in MK derivation. */
		free(data->last_eap_identity);
		data->last_eap_identity = data->reauth_id;
		data->last_eap_identity_len = data->reauth_id_len;
		data->reauth_id = NULL;
		data->reauth_id_len = 0;
		return eap_aka_response_reauth(sm, data, req, respDataLen, 1);
	}
	data->counter = eattr.counter;

	memcpy(data->nonce_s, eattr.nonce_s, EAP_SIM_NONCE_S_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-AKA: (encr) AT_NONCE_S",
		    data->nonce_s, EAP_SIM_NONCE_S_LEN);

	eap_sim_derive_keys_reauth(data->counter,
				   data->reauth_id, data->reauth_id_len,
				   data->nonce_s, data->mk, data->msk);
	eap_aka_clear_identities(data, CLEAR_REAUTH_ID | CLEAR_EAP_ID);
	eap_aka_learn_ids(data, &eattr);

	if (data->state != FAILURE)
		data->state = SUCCESS;

	data->num_id_req = 0;
	data->num_notification = 0;
	if (data->counter > EAP_AKA_MAX_FAST_REAUTHS) {
		wpa_printf(MSG_DEBUG, "EAP-AKA: Maximum number of "
			   "fast reauths performed - force fullauth");
		eap_aka_clear_identities(data, CLEAR_REAUTH_ID | CLEAR_EAP_ID);
	}
	return eap_aka_response_reauth(sm, data, req, respDataLen, 0);
}


static u8 * eap_aka_process(struct eap_sm *sm, void *priv,
			    struct eap_method_ret *ret,
			    u8 *reqData, size_t reqDataLen,
			    size_t *respDataLen)
{
	struct eap_aka_data *data = priv;
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_hdr *req;
	u8 *pos, subtype, *res;
	struct eap_sim_attrs attr;
	size_t len;

	wpa_hexdump(MSG_DEBUG, "EAP-AKA: EAP data", reqData, reqDataLen);
	if (config == NULL || config->identity == NULL) {
		wpa_printf(MSG_INFO, "EAP-AKA: Identity not configured");
		eap_sm_request_identity(sm, config);
		ret->ignore = TRUE;
		return NULL;
	}

	req = (struct eap_hdr *) reqData;
	pos = (u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 4 || *pos != EAP_TYPE_AKA ||
	    (len = be_to_host16(req->length)) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-AKA: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}

	ret->ignore = FALSE;
	ret->methodState = METHOD_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	pos++;
	subtype = *pos++;
	wpa_printf(MSG_DEBUG, "EAP-AKA: Subtype=%d", subtype);
	pos += 2; /* Reserved */

	if (eap_sim_parse_attr(pos, reqData + len, &attr, 1, 0)) {
		res = eap_aka_client_error(sm, data, req, respDataLen,
					   EAP_AKA_UNABLE_TO_PROCESS_PACKET);
		goto done;
	}

	switch (subtype) {
	case EAP_AKA_SUBTYPE_IDENTITY:
		res = eap_aka_process_identity(sm, data, req, len,
					       respDataLen, &attr);
		break;
	case EAP_AKA_SUBTYPE_CHALLENGE:
		res = eap_aka_process_challenge(sm, data, req, len,
						respDataLen, &attr);
		break;
	case EAP_AKA_SUBTYPE_NOTIFICATION:
		res = eap_aka_process_notification(sm, data, req, len,
						   respDataLen, &attr);
		break;
	case EAP_AKA_SUBTYPE_REAUTHENTICATION:
		res = eap_aka_process_reauthentication(sm, data, req, len,
						       respDataLen, &attr);
		break;
	case EAP_AKA_SUBTYPE_CLIENT_ERROR:
		wpa_printf(MSG_DEBUG, "EAP-AKA: subtype Client-Error");
		res = eap_aka_client_error(sm, data, req, respDataLen,
					   EAP_AKA_UNABLE_TO_PROCESS_PACKET);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-AKA: Unknown subtype=%d", subtype);
		res = eap_aka_client_error(sm, data, req, respDataLen,
					   EAP_AKA_UNABLE_TO_PROCESS_PACKET);
		break;
	}

done:
	if (data->state == FAILURE) {
		ret->decision = DECISION_FAIL;
		ret->methodState = METHOD_DONE;
	} else if (data->state == SUCCESS) {
		ret->decision = DECISION_UNCOND_SUCC;
		ret->methodState = METHOD_DONE;
	}

	if (ret->methodState == METHOD_DONE) {
		ret->allowNotifications = FALSE;
	}

	return res;
}


static Boolean eap_aka_has_reauth_data(struct eap_sm *sm, void *priv)
{
	struct eap_aka_data *data = priv;
	return data->pseudonym || data->reauth_id;
}


static void eap_aka_deinit_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_aka_data *data = priv;
	eap_aka_clear_identities(data, CLEAR_EAP_ID);
}


static void * eap_aka_init_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_aka_data *data = priv;
	data->num_id_req = 0;
	data->num_notification = 0;
	data->state = CONTINUE;
	return priv;
}


static const u8 * eap_aka_get_identity(struct eap_sm *sm, void *priv,
				       size_t *len)
{
	struct eap_aka_data *data = priv;

	if (data->reauth_id) {
		*len = data->reauth_id_len;
		return data->reauth_id;
	}

	if (data->pseudonym) {
		*len = data->pseudonym_len;
		return data->pseudonym;
	}

	return NULL;
}


static Boolean eap_aka_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_aka_data *data = priv;
	return data->state == SUCCESS;
}


static u8 * eap_aka_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_aka_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(EAP_SIM_KEYING_DATA_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_SIM_KEYING_DATA_LEN;
	memcpy(key, data->msk, EAP_SIM_KEYING_DATA_LEN);

	return key;
}


const struct eap_method eap_method_aka =
{
	.method = EAP_TYPE_AKA,
	.name = "AKA",
	.init = eap_aka_init,
	.deinit = eap_aka_deinit,
	.process = eap_aka_process,
	.isKeyAvailable = eap_aka_isKeyAvailable,
	.getKey = eap_aka_getKey,
	.has_reauth_data = eap_aka_has_reauth_data,
	.deinit_for_reauth = eap_aka_deinit_for_reauth,
	.init_for_reauth = eap_aka_init_for_reauth,
	.get_identity = eap_aka_get_identity,
};
