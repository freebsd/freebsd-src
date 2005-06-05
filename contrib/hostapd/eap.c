/*
 * hostapd / EAP Standalone Authenticator state machine
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
 * $FreeBSD$
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include "hostapd.h"
#include "eloop.h"
#include "sta_info.h"
#include "eap_i.h"


extern const struct eap_method eap_method_identity;
#ifdef EAP_MD5
extern const struct eap_method eap_method_md5;
#endif /* EAP_MD5 */
#ifdef EAP_TLS
extern const struct eap_method eap_method_tls;
#endif /* EAP_TLS */
#ifdef EAP_MSCHAPv2
extern const struct eap_method eap_method_mschapv2;
#endif /* EAP_MSCHAPv2 */
#ifdef EAP_PEAP
extern const struct eap_method eap_method_peap;
#endif /* EAP_PEAP */
#ifdef EAP_TLV
extern const struct eap_method eap_method_tlv;
#endif /* EAP_TLV */
#ifdef EAP_GTC
extern const struct eap_method eap_method_gtc;
#endif /* EAP_GTC */
#ifdef EAP_TTLS
extern const struct eap_method eap_method_ttls;
#endif /* EAP_TTLS */
#ifdef EAP_SIM
extern const struct eap_method eap_method_sim;
#endif /* EAP_SIM */

static const struct eap_method *eap_methods[] =
{
	&eap_method_identity,
#ifdef EAP_MD5
	&eap_method_md5,
#endif /* EAP_MD5 */
#ifdef EAP_TLS
	&eap_method_tls,
#endif /* EAP_TLS */
#ifdef EAP_MSCHAPv2
	&eap_method_mschapv2,
#endif /* EAP_MSCHAPv2 */
#ifdef EAP_PEAP
	&eap_method_peap,
#endif /* EAP_PEAP */
#ifdef EAP_TTLS
	&eap_method_ttls,
#endif /* EAP_TTLS */
#ifdef EAP_TLV
	&eap_method_tlv,
#endif /* EAP_TLV */
#ifdef EAP_GTC
	&eap_method_gtc,
#endif /* EAP_GTC */
#ifdef EAP_SIM
	&eap_method_sim,
#endif /* EAP_SIM */
};
#define NUM_EAP_METHODS (sizeof(eap_methods) / sizeof(eap_methods[0]))


const struct eap_method * eap_sm_get_eap_methods(int method)
{
	int i;
	for (i = 0; i < NUM_EAP_METHODS; i++) {
		if (eap_methods[i]->method == method)
			return eap_methods[i];
	}
	return NULL;
}

static void eap_user_free(struct eap_user *user);


/* EAP state machines are described in draft-ietf-eap-statemachine-05.txt */

static int eap_sm_calculateTimeout(struct eap_sm *sm, int retransCount,
				   int eapSRTT, int eapRTTVAR,
				   int methodTimeout);
static void eap_sm_parseEapResp(struct eap_sm *sm, u8 *resp, size_t len);
static u8 * eap_sm_buildSuccess(struct eap_sm *sm, int id, size_t *len);
static u8 * eap_sm_buildFailure(struct eap_sm *sm, int id, size_t *len);
static int eap_sm_nextId(struct eap_sm *sm, int id);
static void eap_sm_Policy_update(struct eap_sm *sm, u8 *nak_list, size_t len);
static EapType eap_sm_Policy_getNextMethod(struct eap_sm *sm);
static int eap_sm_Policy_getDecision(struct eap_sm *sm);
static Boolean eap_sm_Policy_doPickUp(struct eap_sm *sm, EapType method);


/* Definitions for clarifying state machine implementation */
#define SM_STATE(machine, state) \
static void sm_ ## machine ## _ ## state ## _Enter(struct eap_sm *sm, \
	int global)

#define SM_ENTRY(machine, state) \
if (!global || sm->machine ## _state != machine ## _ ## state) { \
	sm->changed = TRUE; \
	wpa_printf(MSG_DEBUG, "EAP: " #machine " entering state " #state); \
} \
sm->machine ## _state = machine ## _ ## state;

#define SM_ENTER(machine, state) \
sm_ ## machine ## _ ## state ## _Enter(sm, 0)
#define SM_ENTER_GLOBAL(machine, state) \
sm_ ## machine ## _ ## state ## _Enter(sm, 1)

#define SM_STEP(machine) \
static void sm_ ## machine ## _Step(struct eap_sm *sm)

#define SM_STEP_RUN(machine) sm_ ## machine ## _Step(sm)


static Boolean eapol_get_bool(struct eap_sm *sm, enum eapol_bool_var var)
{
	return sm->eapol_cb->get_bool(sm->eapol_ctx, var);
}


static void eapol_set_bool(struct eap_sm *sm, enum eapol_bool_var var,
			   Boolean value)
{
	sm->eapol_cb->set_bool(sm->eapol_ctx, var, value);
}


static void eapol_set_eapReqData(struct eap_sm *sm,
				 const u8 *eapReqData, size_t eapReqDataLen)
{
	wpa_hexdump(MSG_MSGDUMP, "EAP: eapReqData -> EAPOL",
		    sm->eapReqData, sm->eapReqDataLen);
	sm->eapol_cb->set_eapReqData(sm->eapol_ctx, eapReqData, eapReqDataLen);
}


static void eapol_set_eapKeyData(struct eap_sm *sm,
				 const u8 *eapKeyData, size_t eapKeyDataLen)
{
	wpa_hexdump(MSG_MSGDUMP, "EAP: eapKeyData -> EAPOL",
		    sm->eapKeyData, sm->eapKeyDataLen);
	sm->eapol_cb->set_eapKeyData(sm->eapol_ctx, eapKeyData, eapKeyDataLen);
}


int eap_user_get(struct eap_sm *sm, const u8 *identity, size_t identity_len,
		 int phase2)
{
	struct eap_user *user;

	if (sm == NULL || sm->eapol_cb == NULL ||
	    sm->eapol_cb->get_eap_user == NULL)
		return -1;

	eap_user_free(sm->user);
	sm->user = NULL;

	user = malloc(sizeof(*user));
	if (user == NULL)
	    return -1;
	memset(user, 0, sizeof(*user));

	if (sm->eapol_cb->get_eap_user(sm->eapol_ctx, identity,
				       identity_len, phase2, user) != 0) {
		eap_user_free(user);
		return -1;
	}

	sm->user = user;
	sm->user_eap_method_index = 0;

	return 0;
}


SM_STATE(EAP, DISABLED)
{
	SM_ENTRY(EAP, DISABLED);
}


SM_STATE(EAP, INITIALIZE)
{
	SM_ENTRY(EAP, INITIALIZE);

	sm->currentId = -1;
	eapol_set_bool(sm, EAPOL_eapSuccess, FALSE);
	eapol_set_bool(sm, EAPOL_eapFail, FALSE);
	eapol_set_bool(sm, EAPOL_eapTimeout, FALSE);
	free(sm->eapKeyData);
	sm->eapKeyData = NULL;
	sm->eapKeyDataLen = 0;
	/* eapKeyAvailable = FALSE */
	eapol_set_bool(sm, EAPOL_eapRestart, FALSE);

	/* This is not defined in draft-ietf-eap-statemachine-05.txt, but
	 * method state needs to be reseted here so that it does not remain in
	 * success state when re-authentication starts. */
	if (sm->m && sm->eap_method_priv) {
		sm->m->reset(sm, sm->eap_method_priv);
		sm->eap_method_priv = NULL;
	}
	sm->m = NULL;
	sm->user_eap_method_index = 0;

	if (sm->backend_auth) {
		sm->currentMethod = EAP_TYPE_NONE;
		/* parse rxResp, respId, respMethod */
		eap_sm_parseEapResp(sm, sm->eapRespData, sm->eapRespDataLen);
		if (sm->rxResp) {
			sm->currentId = sm->respId;
		}
	}
}


SM_STATE(EAP, PICK_UP_METHOD)
{
	SM_ENTRY(EAP, PICK_UP_METHOD);

	if (eap_sm_Policy_doPickUp(sm, sm->respMethod)) {
		sm->currentMethod = sm->respMethod;
		if (sm->m && sm->eap_method_priv) {
			sm->m->reset(sm, sm->eap_method_priv);
			sm->eap_method_priv = NULL;
		}
		sm->m = eap_sm_get_eap_methods(sm->currentMethod);
		if (sm->m && sm->m->initPickUp) {
			sm->eap_method_priv = sm->m->initPickUp(sm);
			if (sm->eap_method_priv == NULL) {
				wpa_printf(MSG_DEBUG, "EAP: Failed to "
					   "initialize EAP method %d",
					   sm->currentMethod);
				sm->m = NULL;
				sm->currentMethod = EAP_TYPE_NONE;
			}
		} else {
			sm->m = NULL;
			sm->currentMethod = EAP_TYPE_NONE;
		}
	}
}


SM_STATE(EAP, IDLE)
{
	SM_ENTRY(EAP, IDLE);

	sm->retransWhile = eap_sm_calculateTimeout(sm, sm->retransCount,
						   sm->eapSRTT, sm->eapRTTVAR,
						   sm->methodTimeout);
}


SM_STATE(EAP, RETRANSMIT)
{
	SM_ENTRY(EAP, RETRANSMIT);

	/* TODO: Is this needed since EAPOL state machines take care of
	 * retransmit? */
}


SM_STATE(EAP, RECEIVED)
{
	SM_ENTRY(EAP, RECEIVED);

	/* parse rxResp, respId, respMethod */
	eap_sm_parseEapResp(sm, sm->eapRespData, sm->eapRespDataLen);
}


SM_STATE(EAP, DISCARD)
{
	SM_ENTRY(EAP, DISCARD);
	eapol_set_bool(sm, EAPOL_eapResp, FALSE);
	eapol_set_bool(sm, EAPOL_eapNoReq, TRUE);
}


SM_STATE(EAP, SEND_REQUEST)
{
	SM_ENTRY(EAP, SEND_REQUEST);

	sm->retransCount = 0;
	if (sm->eapReqData) {
		eapol_set_eapReqData(sm, sm->eapReqData, sm->eapReqDataLen);
		free(sm->lastReqData);
		sm->lastReqData = sm->eapReqData;
		sm->lastReqDataLen = sm->eapReqDataLen;
		sm->eapReqData = NULL;
		sm->eapReqDataLen = 0;
		eapol_set_bool(sm, EAPOL_eapResp, FALSE);
		eapol_set_bool(sm, EAPOL_eapReq, TRUE);
	} else {
		wpa_printf(MSG_INFO, "EAP: SEND_REQUEST - no eapReqData");
		eapol_set_bool(sm, EAPOL_eapResp, FALSE);
		eapol_set_bool(sm, EAPOL_eapReq, FALSE);
		eapol_set_bool(sm, EAPOL_eapNoReq, TRUE);
	}
}


SM_STATE(EAP, INTEGRITY_CHECK)
{
	SM_ENTRY(EAP, INTEGRITY_CHECK);

	if (sm->m->check) {
		sm->ignore = sm->m->check(sm, sm->eap_method_priv,
					  sm->eapRespData, sm->eapRespDataLen);
	}
}


SM_STATE(EAP, METHOD_REQUEST)
{
	SM_ENTRY(EAP, METHOD_REQUEST);

	if (sm->m == NULL) {
		wpa_printf(MSG_DEBUG, "EAP: method not initialized");
		return;
	}

	sm->currentId = eap_sm_nextId(sm, sm->currentId);
	wpa_printf(MSG_DEBUG, "EAP: building EAP-Request: Identifier %d",
		   sm->currentId);
	sm->lastId = sm->currentId;
	free(sm->eapReqData);
	sm->eapReqData = sm->m->buildReq(sm, sm->eap_method_priv,
					 sm->currentId, &sm->eapReqDataLen);
	if (sm->m->getTimeout)
		sm->methodTimeout = sm->m->getTimeout(sm, sm->eap_method_priv);
	else
		sm->methodTimeout = 0;
}


SM_STATE(EAP, METHOD_RESPONSE)
{
	SM_ENTRY(EAP, METHOD_RESPONSE);

	sm->m->process(sm, sm->eap_method_priv, sm->eapRespData,
		       sm->eapRespDataLen);
	if (sm->m->isDone(sm, sm->eap_method_priv)) {
		eap_sm_Policy_update(sm, NULL, 0);
		free(sm->eapKeyData);
		if (sm->m->getKey) {
			sm->eapKeyData = sm->m->getKey(sm, sm->eap_method_priv,
						       &sm->eapKeyDataLen);
		} else {
			sm->eapKeyData = NULL;
			sm->eapKeyDataLen = 0;
		}
		sm->methodState = METHOD_END;
	} else {
		sm->methodState = METHOD_CONTINUE;
	}
}


SM_STATE(EAP, PROPOSE_METHOD)
{
	SM_ENTRY(EAP, PROPOSE_METHOD);

	sm->currentMethod = eap_sm_Policy_getNextMethod(sm);
	if (sm->m && sm->eap_method_priv) {
		sm->m->reset(sm, sm->eap_method_priv);
		sm->eap_method_priv = NULL;
	}
	sm->m = eap_sm_get_eap_methods(sm->currentMethod);
	if (sm->m) {
		sm->eap_method_priv = sm->m->init(sm);
		if (sm->eap_method_priv == NULL) {
			wpa_printf(MSG_DEBUG, "EAP: Failed to initialize EAP "
				   "method %d", sm->currentMethod);
			sm->m = NULL;
			sm->currentMethod = EAP_TYPE_NONE;
		}
	}
	if (sm->currentMethod == EAP_TYPE_IDENTITY ||
	    sm->currentMethod == EAP_TYPE_NOTIFICATION)
		sm->methodState = METHOD_CONTINUE;
	else
		sm->methodState = METHOD_PROPOSED;
}


SM_STATE(EAP, NAK)
{
	struct eap_hdr *nak;
	size_t len = 0;
	u8 *pos, *nak_list = NULL;

	SM_ENTRY(EAP, NAK);

	if (sm->eap_method_priv) {
		sm->m->reset(sm, sm->eap_method_priv);
		sm->eap_method_priv = NULL;
	}
	sm->m = NULL;

	nak = (struct eap_hdr *) sm->eapRespData;
	if (nak && sm->eapRespDataLen > sizeof(*nak)) {
		len = ntohs(nak->length);
		if (len > sm->eapRespDataLen)
			len = sm->eapRespDataLen;
		pos = (u8 *) (nak + 1);
		len -= sizeof(*nak);
		if (*pos == EAP_TYPE_NAK) {
			pos++;
			len--;
			nak_list = pos;
		}
	}
	eap_sm_Policy_update(sm, nak_list, len);
}


SM_STATE(EAP, SELECT_ACTION)
{
	SM_ENTRY(EAP, SELECT_ACTION);

	sm->decision = eap_sm_Policy_getDecision(sm);
}


SM_STATE(EAP, TIMEOUT_FAILURE)
{
	SM_ENTRY(EAP, TIMEOUT_FAILURE);

	eapol_set_bool(sm, EAPOL_eapTimeout, TRUE);
}


SM_STATE(EAP, FAILURE)
{
	SM_ENTRY(EAP, FAILURE);

	free(sm->eapReqData);
	sm->eapReqData = eap_sm_buildFailure(sm, sm->currentId,
					     &sm->eapReqDataLen);
	if (sm->eapReqData) {
		eapol_set_eapReqData(sm, sm->eapReqData, sm->eapReqDataLen);
		free(sm->eapReqData);
		sm->eapReqData = NULL;
		sm->eapReqDataLen = 0;
	}
	free(sm->lastReqData);
	sm->lastReqData = NULL;
	sm->lastReqDataLen = 0;
	eapol_set_bool(sm, EAPOL_eapFail, TRUE);
}


SM_STATE(EAP, SUCCESS)
{
	SM_ENTRY(EAP, SUCCESS);

	free(sm->eapReqData);
	sm->eapReqData = eap_sm_buildSuccess(sm, sm->currentId,
					     &sm->eapReqDataLen);
	if (sm->eapReqData) {
		eapol_set_eapReqData(sm, sm->eapReqData, sm->eapReqDataLen);
		free(sm->eapReqData);
		sm->eapReqData = NULL;
		sm->eapReqDataLen = 0;
	}
	free(sm->lastReqData);
	sm->lastReqData = NULL;
	sm->lastReqDataLen = 0;
	if (sm->eapKeyData) {
		eapol_set_eapKeyData(sm, sm->eapKeyData, sm->eapKeyDataLen);
	}
	eapol_set_bool(sm, EAPOL_eapSuccess, TRUE);
}


SM_STEP(EAP)
{
	if (eapol_get_bool(sm, EAPOL_eapRestart) &&
	    eapol_get_bool(sm, EAPOL_portEnabled))
		SM_ENTER_GLOBAL(EAP, INITIALIZE);
	else if (!eapol_get_bool(sm, EAPOL_portEnabled))
		SM_ENTER_GLOBAL(EAP, DISABLED);
	else switch (sm->EAP_state) {
	case EAP_INITIALIZE:
		if (sm->backend_auth) {
			if (!sm->rxResp)
				SM_ENTER(EAP, SELECT_ACTION);
			else if (sm->rxResp &&
				 (sm->respMethod == EAP_TYPE_NAK ||
				  sm->respMethod == EAP_TYPE_EXPANDED_NAK))
				SM_ENTER(EAP, NAK);
			else
				SM_ENTER(EAP, PICK_UP_METHOD);
		} else {
			SM_ENTER(EAP, SELECT_ACTION);
		}
		break;
	case EAP_PICK_UP_METHOD:
		if (sm->currentMethod == EAP_TYPE_NONE) {
			SM_ENTER(EAP, SELECT_ACTION);
		} else {
			SM_ENTER(EAP, METHOD_RESPONSE);
		}
		break;
	case EAP_DISABLED:
		if (eapol_get_bool(sm, EAPOL_portEnabled))
			SM_ENTER(EAP, INITIALIZE);
		break;
	case EAP_IDLE:
		if (sm->retransWhile == 0)
			SM_ENTER(EAP, RETRANSMIT);
		else if (eapol_get_bool(sm, EAPOL_eapResp))
			SM_ENTER(EAP, RECEIVED);
		break;
	case EAP_RETRANSMIT:
		if (sm->retransCount > sm->MaxRetrans)
			SM_ENTER(EAP, TIMEOUT_FAILURE);
		else
			SM_ENTER(EAP, IDLE);
		break;
	case EAP_RECEIVED:
		if (sm->rxResp && (sm->respId == sm->currentId) &&
		    (sm->respMethod == EAP_TYPE_NAK ||
		     sm->respMethod == EAP_TYPE_EXPANDED_NAK)
		    && (sm->methodState == METHOD_PROPOSED))
			SM_ENTER(EAP, NAK);
		else if (sm->rxResp && (sm->respId == sm->currentId) &&
			 (sm->respMethod == sm->currentMethod))
			SM_ENTER(EAP, INTEGRITY_CHECK);
		else
			SM_ENTER(EAP, DISCARD);
		break;
	case EAP_DISCARD:
		SM_ENTER(EAP, IDLE);
		break;
	case EAP_SEND_REQUEST:
		SM_ENTER(EAP, IDLE);
		break;
	case EAP_INTEGRITY_CHECK:
		if (sm->ignore)
			SM_ENTER(EAP, DISCARD);
		else
			SM_ENTER(EAP, METHOD_RESPONSE);
		break;
	case EAP_METHOD_REQUEST:
		SM_ENTER(EAP, SEND_REQUEST);
		break;
	case EAP_METHOD_RESPONSE:
		if (sm->methodState == METHOD_END)
			SM_ENTER(EAP, SELECT_ACTION);
		else
			SM_ENTER(EAP, METHOD_REQUEST);
		break;
	case EAP_PROPOSE_METHOD:
		SM_ENTER(EAP, METHOD_REQUEST);
		break;
	case EAP_NAK:
		SM_ENTER(EAP, SELECT_ACTION);
		break;
	case EAP_SELECT_ACTION:
		if (sm->decision == DECISION_FAILURE)
			SM_ENTER(EAP, FAILURE);
		else if (sm->decision == DECISION_SUCCESS)
			SM_ENTER(EAP, SUCCESS);
		else
			SM_ENTER(EAP, PROPOSE_METHOD);
		break;
	case EAP_TIMEOUT_FAILURE:
		break;
	case EAP_FAILURE:
		break;
	case EAP_SUCCESS:
		break;
	}
}


static int eap_sm_calculateTimeout(struct eap_sm *sm, int retransCount,
				   int eapSRTT, int eapRTTVAR,
				   int methodTimeout)
{
	/* For now, retransmission is done in EAPOL state machines, so make
	 * sure EAP state machine does not end up trying to retransmit packets.
	 */
	return 1;
}


static void eap_sm_parseEapResp(struct eap_sm *sm, u8 *resp, size_t len)
{
	struct eap_hdr *hdr;
	size_t plen;

	/* parse rxResp, respId, respMethod */
	sm->rxResp = FALSE;
	sm->respId = -1;
	sm->respMethod = EAP_TYPE_NONE;

	if (resp == NULL || len < sizeof(*hdr))
		return;

	hdr = (struct eap_hdr *) resp;
	plen = ntohs(hdr->length);
	if (plen > len) {
		wpa_printf(MSG_DEBUG, "EAP: Ignored truncated EAP-Packet "
			   "(len=%lu plen=%lu)", (unsigned long) len,
			   (unsigned long) plen);
		return;
	}

	sm->respId = hdr->identifier;

	if (hdr->code == EAP_CODE_RESPONSE)
		sm->rxResp = TRUE;

	if (len > sizeof(*hdr))
		sm->respMethod = *((u8 *) (hdr + 1));

	wpa_printf(MSG_DEBUG, "EAP: parseEapResp: rxResp=%d respId=%d "
		   "respMethod=%d", sm->rxResp, sm->respId, sm->respMethod);
}


static u8 * eap_sm_buildSuccess(struct eap_sm *sm, int id, size_t *len)
{
	struct eap_hdr *resp;
	wpa_printf(MSG_DEBUG, "EAP: Building EAP-Success (id=%d)", id);

	*len = sizeof(*resp);
	resp = malloc(*len);
	if (resp == NULL)
		return NULL;
	resp->code = EAP_CODE_SUCCESS;
	resp->identifier = id;
	resp->length = htons(*len);

	return (u8 *) resp;
}


static u8 * eap_sm_buildFailure(struct eap_sm *sm, int id, size_t *len)
{
	struct eap_hdr *resp;
	wpa_printf(MSG_DEBUG, "EAP: Building EAP-Failure (id=%d)", id);

	*len = sizeof(*resp);
	resp = malloc(*len);
	if (resp == NULL)
		return NULL;
	resp->code = EAP_CODE_FAILURE;
	resp->identifier = id;
	resp->length = htons(*len);

	return (u8 *) resp;
}


static int eap_sm_nextId(struct eap_sm *sm, int id)
{
	if (id < 0) {
		/* RFC 3748 Ch 4.1: recommended to initalize Identifier with a
		 * random number */
		id = rand() & 0xff;
		if (id != sm->lastId)
			return id;
	}
	return (id + 1) & 0xff;
}


void eap_sm_process_nak(struct eap_sm *sm, u8 *nak_list, size_t len)
{
	int i, j;

	wpa_printf(MSG_MSGDUMP, "EAP: processing NAK (current EAP method "
		   "index %d)", sm->user_eap_method_index);

	wpa_hexdump(MSG_MSGDUMP, "EAP: configured methods",
		    sm->user->methods, EAP_MAX_METHODS);
	wpa_hexdump(MSG_MSGDUMP, "EAP: list of methods supported by the peer",
		    nak_list, len);

	i = sm->user_eap_method_index;
	while (i < EAP_MAX_METHODS && sm->user->methods[i] != EAP_TYPE_NONE) {
		for (j = 0; j < len; j++) {
			if (nak_list[j] == sm->user->methods[i]) {
				break;
			}
		}

		if (j < len) {
			/* found */
			i++;
			continue;
		}

		/* not found - remove from the list */
		memmove(&sm->user->methods[i], &sm->user->methods[i + 1],
			EAP_MAX_METHODS - i - 1);
		sm->user->methods[EAP_MAX_METHODS - 1] = EAP_TYPE_NONE;
	}

	wpa_hexdump(MSG_MSGDUMP, "EAP: new list of configured methods",
		    sm->user->methods, EAP_MAX_METHODS);
}


static void eap_sm_Policy_update(struct eap_sm *sm, u8 *nak_list, size_t len)
{
	if (nak_list == NULL || sm == NULL || sm->user == NULL)
		return;

	if (sm->user->phase2) {
		wpa_printf(MSG_DEBUG, "EAP: EAP-Nak received after Phase2 user"
			   " info was selected - reject");
		sm->decision = DECISION_FAILURE;
		return;
	}

	eap_sm_process_nak(sm, nak_list, len);
}


static EapType eap_sm_Policy_getNextMethod(struct eap_sm *sm)
{
	EapType next;

	/* In theory, there should be no problems with starting
	 * re-authentication with something else than EAP-Request/Identity and
	 * this does indeed work with wpa_supplicant. However, at least Funk
	 * Supplicant seemed to ignore re-auth if it skipped
	 * EAP-Request/Identity.
	 * Re-auth sets currentId == -1, so that can be used here to select
	 * whether Identity needs to be requested again. */
	if (sm->identity == NULL || sm->currentId == -1) {
		next = EAP_TYPE_IDENTITY;
		sm->update_user = TRUE;
	} else if (sm->user && sm->user_eap_method_index < EAP_MAX_METHODS &&
		   sm->user->methods[sm->user_eap_method_index] !=
		   EAP_TYPE_NONE) {
		next = sm->user->methods[sm->user_eap_method_index++];
	} else {
		next = EAP_TYPE_NONE;
	}
	wpa_printf(MSG_DEBUG, "EAP: getNextMethod: type %d", next);
	return next;
}


static int eap_sm_Policy_getDecision(struct eap_sm *sm)
{
	if (sm->m && sm->currentMethod != EAP_TYPE_IDENTITY &&
	    sm->m->isSuccess(sm, sm->eap_method_priv)) {
		wpa_printf(MSG_DEBUG, "EAP: getDecision: method succeeded -> "
			   "SUCCESS");
		sm->update_user = TRUE;
		return DECISION_SUCCESS;
	}

	if (sm->m && sm->m->isDone(sm, sm->eap_method_priv) &&
	    !sm->m->isSuccess(sm, sm->eap_method_priv)) {
		wpa_printf(MSG_DEBUG, "EAP: getDecision: method failed -> "
			   "FAILURE");
		sm->update_user = TRUE;
		return DECISION_FAILURE;
	}

	if ((sm->user == NULL || sm->update_user) && sm->identity) {
		if (eap_user_get(sm, sm->identity, sm->identity_len, 0) != 0) {
			wpa_printf(MSG_DEBUG, "EAP: getDecision: user not "
				   "found from database -> FAILURE");
			return DECISION_FAILURE;
		}
		sm->update_user = FALSE;
	}

	if (sm->user && sm->user_eap_method_index < EAP_MAX_METHODS &&
	    sm->user->methods[sm->user_eap_method_index] != EAP_TYPE_NONE) {
		wpa_printf(MSG_DEBUG, "EAP: getDecision: another method "
			   "available -> CONTINUE");
		return DECISION_CONTINUE;
	}

	if (sm->identity == NULL || sm->currentId == -1) {
		wpa_printf(MSG_DEBUG, "EAP: getDecision: no identity known "
			   "yet -> CONTINUE");
		return DECISION_CONTINUE;
	}

	wpa_printf(MSG_DEBUG, "EAP: getDecision: no more methods available -> "
		   "FAILURE");
	return DECISION_FAILURE;
}


static Boolean eap_sm_Policy_doPickUp(struct eap_sm *sm, EapType method)
{
	return method == EAP_TYPE_IDENTITY ? TRUE : FALSE;
}


int eap_sm_step(struct eap_sm *sm)
{
	int res = 0;
	do {
		sm->changed = FALSE;
		SM_STEP_RUN(EAP);
		if (sm->changed)
			res = 1;
	} while (sm->changed);
	return res;
}


u8 eap_get_type(const char *name)
{
	int i;
	for (i = 0; i < NUM_EAP_METHODS; i++) {
		if (strcmp(eap_methods[i]->name, name) == 0)
			return eap_methods[i]->method;
	}
	return EAP_TYPE_NONE;
}


void eap_set_eapRespData(struct eap_sm *sm, const u8 *eapRespData,
			 size_t eapRespDataLen)
{
	if (sm == NULL)
		return;
	free(sm->eapRespData);
	sm->eapRespData = malloc(eapRespDataLen);
	if (sm->eapRespData == NULL)
		return;
	memcpy(sm->eapRespData, eapRespData, eapRespDataLen);
	sm->eapRespDataLen = eapRespDataLen;
	wpa_hexdump(MSG_MSGDUMP, "EAP: EAP-Response received",
		    eapRespData, eapRespDataLen);
}


static void eap_user_free(struct eap_user *user)
{
	if (user == NULL)
		return;
	free(user->password);
	user->password = NULL;
	free(user);
}


struct eap_sm * eap_sm_init(void *eapol_ctx, struct eapol_callbacks *eapol_cb,
			    struct eap_config *eap_conf)
{
	struct eap_sm *sm;

	sm = malloc(sizeof(*sm));
	if (sm == NULL)
		return NULL;
	memset(sm, 0, sizeof(*sm));
	sm->eapol_ctx = eapol_ctx;
	sm->eapol_cb = eapol_cb;
	sm->MaxRetrans = 10;
	sm->ssl_ctx = eap_conf->ssl_ctx;
	sm->eap_sim_db_priv = eap_conf->eap_sim_db_priv;
	sm->backend_auth = eap_conf->backend_auth;

	wpa_printf(MSG_DEBUG, "EAP: State machine created");

	return sm;
}


void eap_sm_deinit(struct eap_sm *sm)
{
	if (sm == NULL)
		return;
	wpa_printf(MSG_DEBUG, "EAP: State machine removed");
	if (sm->m && sm->eap_method_priv)
		sm->m->reset(sm, sm->eap_method_priv);
	free(sm->eapReqData);
	free(sm->eapKeyData);
	free(sm->lastReqData);
	free(sm->eapRespData);
	free(sm->identity);
	eap_user_free(sm->user);
	free(sm);
}

void eap_sm_notify_cached(struct eap_sm *sm)
{
	if (sm == NULL)
		return;

	sm->EAP_state = EAP_SUCCESS;
}
