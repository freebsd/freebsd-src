/*
 * hostapd / EAP Standalone Authenticator state machine (RFC 4137)
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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

#include "includes.h"

#include "hostapd.h"
#include "sta_info.h"
#include "eap_i.h"
#include "state_machine.h"

#define STATE_MACHINE_DATA struct eap_sm
#define STATE_MACHINE_DEBUG_PREFIX "EAP"

#define EAP_MAX_AUTH_ROUNDS 50

static void eap_user_free(struct eap_user *user);


/* EAP state machines are described in RFC 4137 */

static int eap_sm_calculateTimeout(struct eap_sm *sm, int retransCount,
				   int eapSRTT, int eapRTTVAR,
				   int methodTimeout);
static void eap_sm_parseEapResp(struct eap_sm *sm, u8 *resp, size_t len);
static u8 * eap_sm_buildSuccess(struct eap_sm *sm, int id, size_t *len);
static u8 * eap_sm_buildFailure(struct eap_sm *sm, int id, size_t *len);
static int eap_sm_nextId(struct eap_sm *sm, int id);
static void eap_sm_Policy_update(struct eap_sm *sm, u8 *nak_list, size_t len);
static EapType eap_sm_Policy_getNextMethod(struct eap_sm *sm, int *vendor);
static int eap_sm_Policy_getDecision(struct eap_sm *sm);
static Boolean eap_sm_Policy_doPickUp(struct eap_sm *sm, EapType method);


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


/**
 * eap_user_get - Fetch user information from the database
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 * @identity: Identity (User-Name) of the user
 * @identity_len: Length of identity in bytes
 * @phase2: 0 = EAP phase1 user, 1 = EAP phase2 (tunneled) user
 * Returns: 0 on success, or -1 on failure
 *
 * This function is used to fetch user information for EAP. The user will be
 * selected based on the specified identity. sm->user and
 * sm->user_eap_method_index are updated for the new user when a matching user
 * is found. sm->user can be used to get user information (e.g., password).
 */
int eap_user_get(struct eap_sm *sm, const u8 *identity, size_t identity_len,
		 int phase2)
{
	struct eap_user *user;

	if (sm == NULL || sm->eapol_cb == NULL ||
	    sm->eapol_cb->get_eap_user == NULL)
		return -1;

	eap_user_free(sm->user);
	sm->user = NULL;

	user = wpa_zalloc(sizeof(*user));
	if (user == NULL)
	    return -1;

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
	sm->num_rounds = 0;
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

	/*
	 * This is not defined in RFC 4137, but method state needs to be
	 * reseted here so that it does not remain in success state when
	 * re-authentication starts.
	 */
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
	sm->num_rounds = 0;
	sm->method_pending = METHOD_PENDING_NONE;
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
		sm->m = eap_sm_get_eap_methods(EAP_VENDOR_IETF,
					       sm->currentMethod);
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
	sm->num_rounds++;
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
	int vendor;
	EapType type;

	SM_ENTRY(EAP, PROPOSE_METHOD);

	type = eap_sm_Policy_getNextMethod(sm, &vendor);
	if (vendor == EAP_VENDOR_IETF)
		sm->currentMethod = type;
	else
		sm->currentMethod = EAP_TYPE_EXPANDED;
	if (sm->m && sm->eap_method_priv) {
		sm->m->reset(sm, sm->eap_method_priv);
		sm->eap_method_priv = NULL;
	}
	sm->m = eap_sm_get_eap_methods(vendor, type);
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
	else if (sm->num_rounds > EAP_MAX_AUTH_ROUNDS) {
		if (sm->num_rounds == EAP_MAX_AUTH_ROUNDS + 1) {
			wpa_printf(MSG_DEBUG, "EAP: more than %d "
				   "authentication rounds - abort",
				   EAP_MAX_AUTH_ROUNDS);
			sm->num_rounds++;
			SM_ENTER_GLOBAL(EAP, FAILURE);
		}
	} else switch (sm->EAP_state) {
	case EAP_INITIALIZE:
		if (sm->backend_auth) {
			if (!sm->rxResp)
				SM_ENTER(EAP, SELECT_ACTION);
			else if (sm->rxResp &&
				 (sm->respMethod == EAP_TYPE_NAK ||
				  (sm->respMethod == EAP_TYPE_EXPANDED &&
				   sm->respVendor == EAP_VENDOR_IETF &&
				   sm->respVendorMethod == EAP_TYPE_NAK)))
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
		     (sm->respMethod == EAP_TYPE_EXPANDED &&
		      sm->respVendor == EAP_VENDOR_IETF &&
		      sm->respVendorMethod == EAP_TYPE_NAK))
		    && (sm->methodState == METHOD_PROPOSED))
			SM_ENTER(EAP, NAK);
		else if (sm->rxResp && (sm->respId == sm->currentId) &&
			 ((sm->respMethod == sm->currentMethod) ||
			  (sm->respMethod == EAP_TYPE_EXPANDED &&
			   sm->respVendor == EAP_VENDOR_IETF &&
			   sm->respVendorMethod == sm->currentMethod)))
			SM_ENTER(EAP, INTEGRITY_CHECK);
		else {
			wpa_printf(MSG_DEBUG, "EAP: RECEIVED->DISCARD: "
				   "rxResp=%d respId=%d currentId=%d "
				   "respMethod=%d currentMethod=%d",
				   sm->rxResp, sm->respId, sm->currentId,
				   sm->respMethod, sm->currentMethod);
			SM_ENTER(EAP, DISCARD);
		}
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
		/*
		 * Note: Mechanism to allow EAP methods to wait while going
		 * through pending processing is an extension to RFC 4137
		 * which only defines the transits to SELECT_ACTION and
		 * METHOD_REQUEST from this METHOD_RESPONSE state.
		 */
		if (sm->methodState == METHOD_END)
			SM_ENTER(EAP, SELECT_ACTION);
		else if (sm->method_pending == METHOD_PENDING_WAIT) {
			wpa_printf(MSG_DEBUG, "EAP: Method has pending "
				   "processing - wait before proceeding to "
				   "METHOD_REQUEST state");
		} else if (sm->method_pending == METHOD_PENDING_CONT) {
			wpa_printf(MSG_DEBUG, "EAP: Method has completed "
				   "pending processing - reprocess pending "
				   "EAP message");
			sm->method_pending = METHOD_PENDING_NONE;
			SM_ENTER(EAP, METHOD_RESPONSE);
		} else
			SM_ENTER(EAP, METHOD_REQUEST);
		break;
	case EAP_PROPOSE_METHOD:
		/*
		 * Note: Mechanism to allow EAP methods to wait while going
		 * through pending processing is an extension to RFC 4137
		 * which only defines the transit to METHOD_REQUEST from this
		 * PROPOSE_METHOD state.
		 */
		if (sm->method_pending == METHOD_PENDING_WAIT) {
			wpa_printf(MSG_DEBUG, "EAP: Method has pending "
				   "processing - wait before proceeding to "
				   "METHOD_REQUEST state");
			if (sm->user_eap_method_index > 0)
				sm->user_eap_method_index--;
		} else if (sm->method_pending == METHOD_PENDING_CONT) {
			wpa_printf(MSG_DEBUG, "EAP: Method has completed "
				   "pending processing - reprocess pending "
				   "EAP message");
			sm->method_pending = METHOD_PENDING_NONE;
			SM_ENTER(EAP, PROPOSE_METHOD);
		} else
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
	sm->respVendor = EAP_VENDOR_IETF;
	sm->respVendorMethod = EAP_TYPE_NONE;

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

	if (plen > sizeof(*hdr)) {
		u8 *pos = (u8 *) (hdr + 1);
		sm->respMethod = *pos++;
		if (sm->respMethod == EAP_TYPE_EXPANDED) {
			if (plen < sizeof(*hdr) + 8) {
				wpa_printf(MSG_DEBUG, "EAP: Ignored truncated "
					   "expanded EAP-Packet (plen=%lu)",
					   (unsigned long) plen);
				return;
			}
			sm->respVendor = WPA_GET_BE24(pos);
			pos += 3;
			sm->respVendorMethod = WPA_GET_BE32(pos);
		}
	}

	wpa_printf(MSG_DEBUG, "EAP: parseEapResp: rxResp=%d respId=%d "
		   "respMethod=%u respVendor=%u respVendorMethod=%u",
		   sm->rxResp, sm->respId, sm->respMethod, sm->respVendor,
		   sm->respVendorMethod);
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
		/* RFC 3748 Ch 4.1: recommended to initialize Identifier with a
		 * random number */
		id = rand() & 0xff;
		if (id != sm->lastId)
			return id;
	}
	return (id + 1) & 0xff;
}


/**
 * eap_sm_process_nak - Process EAP-Response/Nak
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 * @nak_list: Nak list (allowed methods) from the supplicant
 * @len: Length of nak_list in bytes
 *
 * This function is called when EAP-Response/Nak is received from the
 * supplicant. This can happen for both phase 1 and phase 2 authentications.
 */
void eap_sm_process_nak(struct eap_sm *sm, u8 *nak_list, size_t len)
{
	int i;
	size_t j;

	if (sm->user == NULL)
		return;

	wpa_printf(MSG_MSGDUMP, "EAP: processing NAK (current EAP method "
		   "index %d)", sm->user_eap_method_index);

	wpa_hexdump(MSG_MSGDUMP, "EAP: configured methods",
		    (u8 *) sm->user->methods,
		    EAP_MAX_METHODS * sizeof(sm->user->methods[0]));
	wpa_hexdump(MSG_MSGDUMP, "EAP: list of methods supported by the peer",
		    nak_list, len);

	i = sm->user_eap_method_index;
	while (i < EAP_MAX_METHODS &&
	       (sm->user->methods[i].vendor != EAP_VENDOR_IETF ||
		sm->user->methods[i].method != EAP_TYPE_NONE)) {
		if (sm->user->methods[i].vendor != EAP_VENDOR_IETF)
			goto not_found;
		for (j = 0; j < len; j++) {
			if (nak_list[j] == sm->user->methods[i].method) {
				break;
			}
		}

		if (j < len) {
			/* found */
			i++;
			continue;
		}

	not_found:
		/* not found - remove from the list */
		memmove(&sm->user->methods[i], &sm->user->methods[i + 1],
			(EAP_MAX_METHODS - i - 1) *
			sizeof(sm->user->methods[0]));
		sm->user->methods[EAP_MAX_METHODS - 1].vendor =
			EAP_VENDOR_IETF;
		sm->user->methods[EAP_MAX_METHODS - 1].method = EAP_TYPE_NONE;
	}

	wpa_hexdump(MSG_MSGDUMP, "EAP: new list of configured methods",
		    (u8 *) sm->user->methods, EAP_MAX_METHODS *
		    sizeof(sm->user->methods[0]));
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


static EapType eap_sm_Policy_getNextMethod(struct eap_sm *sm, int *vendor)
{
	EapType next;
	int idx = sm->user_eap_method_index;

	/* In theory, there should be no problems with starting
	 * re-authentication with something else than EAP-Request/Identity and
	 * this does indeed work with wpa_supplicant. However, at least Funk
	 * Supplicant seemed to ignore re-auth if it skipped
	 * EAP-Request/Identity.
	 * Re-auth sets currentId == -1, so that can be used here to select
	 * whether Identity needs to be requested again. */
	if (sm->identity == NULL || sm->currentId == -1) {
		*vendor = EAP_VENDOR_IETF;
		next = EAP_TYPE_IDENTITY;
		sm->update_user = TRUE;
	} else if (sm->user && idx < EAP_MAX_METHODS &&
		   (sm->user->methods[idx].vendor != EAP_VENDOR_IETF ||
		    sm->user->methods[idx].method != EAP_TYPE_NONE)) {
		*vendor = sm->user->methods[idx].vendor;
		next = sm->user->methods[idx].method;
		sm->user_eap_method_index++;
	} else {
		*vendor = EAP_VENDOR_IETF;
		next = EAP_TYPE_NONE;
	}
	wpa_printf(MSG_DEBUG, "EAP: getNextMethod: vendor %d type %d",
		   *vendor, next);
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
	    (sm->user->methods[sm->user_eap_method_index].vendor !=
	     EAP_VENDOR_IETF ||
	     sm->user->methods[sm->user_eap_method_index].method !=
	     EAP_TYPE_NONE)) {
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


/**
 * eap_sm_step - Step EAP state machine
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 * Returns: 1 if EAP state was changed or 0 if not
 *
 * This function advances EAP state machine to a new state to match with the
 * current variables. This should be called whenever variables used by the EAP
 * state machine have changed.
 */
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


/**
 * eap_set_eapRespData - Set EAP response (eapRespData)
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 * @eapRespData: EAP-Response payload from the supplicant
 * @eapRespDataLen: Length of eapRespData in bytes
 *
 * This function is called when an EAP-Response is received from a supplicant.
 */
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


/**
 * eap_sm_init - Allocate and initialize EAP state machine
 * @eapol_ctx: Context data to be used with eapol_cb calls
 * @eapol_cb: Pointer to EAPOL callback functions
 * @conf: EAP configuration
 * Returns: Pointer to the allocated EAP state machine or %NULL on failure
 *
 * This function allocates and initializes an EAP state machine.
 */
struct eap_sm * eap_sm_init(void *eapol_ctx, struct eapol_callbacks *eapol_cb,
			    struct eap_config *conf)
{
	struct eap_sm *sm;

	sm = wpa_zalloc(sizeof(*sm));
	if (sm == NULL)
		return NULL;
	sm->eapol_ctx = eapol_ctx;
	sm->eapol_cb = eapol_cb;
	sm->MaxRetrans = 10;
	sm->ssl_ctx = conf->ssl_ctx;
	sm->eap_sim_db_priv = conf->eap_sim_db_priv;
	sm->backend_auth = conf->backend_auth;

	wpa_printf(MSG_DEBUG, "EAP: State machine created");

	return sm;
}


/**
 * eap_sm_deinit - Deinitialize and free an EAP state machine
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 *
 * This function deinitializes EAP state machine and frees all allocated
 * resources.
 */
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


/**
 * eap_sm_notify_cached - Notify EAP state machine of cached PMK
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 *
 * This function is called when PMKSA caching is used to skip EAP
 * authentication.
 */
void eap_sm_notify_cached(struct eap_sm *sm)
{
	if (sm == NULL)
		return;

	sm->EAP_state = EAP_SUCCESS;
}


/**
 * eap_sm_pending_cb - EAP state machine callback for a pending EAP request
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 *
 * This function is called when data for a pending EAP-Request is received.
 */
void eap_sm_pending_cb(struct eap_sm *sm)
{
	if (sm == NULL)
		return;
	wpa_printf(MSG_DEBUG, "EAP: Callback for pending request received");
	if (sm->method_pending == METHOD_PENDING_WAIT)
		sm->method_pending = METHOD_PENDING_CONT;
}


/**
 * eap_sm_method_pending - Query whether EAP method is waiting for pending data
 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
 * Returns: 1 if method is waiting for pending data or 0 if not
 */
int eap_sm_method_pending(struct eap_sm *sm)
{
	if (sm == NULL)
		return 0;
	return sm->method_pending == METHOD_PENDING_WAIT;
}


/**
 * eap_hdr_validate - Validate EAP header
 * @vendor: Expected EAP Vendor-Id (0 = IETF)
 * @eap_type: Expected EAP type number
 * @msg: EAP frame (starting with EAP header)
 * @msglen: Length of msg
 * @plen: Pointer to variable to contain the returned payload length
 * Returns: Pointer to EAP payload (after type field), or %NULL on failure
 *
 * This is a helper function for EAP method implementations. This is usually
 * called in the beginning of struct eap_method::process() function to verify
 * that the received EAP request packet has a valid header. This function is
 * able to process both legacy and expanded EAP headers and in most cases, the
 * caller can just use the returned payload pointer (into *plen) for processing
 * the payload regardless of whether the packet used the expanded EAP header or
 * not.
 */
const u8 * eap_hdr_validate(int vendor, EapType eap_type,
			    const u8 *msg, size_t msglen, size_t *plen)
{
	const struct eap_hdr *hdr;
	const u8 *pos;
	size_t len;

	hdr = (const struct eap_hdr *) msg;

	if (msglen < sizeof(*hdr)) {
		wpa_printf(MSG_INFO, "EAP: Too short EAP frame");
		return NULL;
	}

	len = be_to_host16(hdr->length);
	if (len < sizeof(*hdr) + 1 || len > msglen) {
		wpa_printf(MSG_INFO, "EAP: Invalid EAP length");
		return NULL;
	}

	pos = (const u8 *) (hdr + 1);

	if (*pos == EAP_TYPE_EXPANDED) {
		int exp_vendor;
		u32 exp_type;
		if (len < sizeof(*hdr) + 8) {
			wpa_printf(MSG_INFO, "EAP: Invalid expanded EAP "
				   "length");
			return NULL;
		}
		pos++;
		exp_vendor = WPA_GET_BE24(pos);
		pos += 3;
		exp_type = WPA_GET_BE32(pos);
		pos += 4;
		if (exp_vendor != vendor || exp_type != (u32) eap_type) {
			wpa_printf(MSG_INFO, "EAP: Invalid expanded frame "
				   "type");
			return NULL;
		}

		*plen = len - sizeof(*hdr) - 8;
		return pos;
	} else {
		if (vendor != EAP_VENDOR_IETF || *pos != eap_type) {
			wpa_printf(MSG_INFO, "EAP: Invalid frame type");
			return NULL;
		}
		*plen = len - sizeof(*hdr) - 1;
		return pos + 1;
	}
}


/**
 * eap_msg_alloc - Allocate a buffer for an EAP message
 * @vendor: Vendor-Id (0 = IETF)
 * @type: EAP type
 * @len: Buffer for returning message length
 * @payload_len: Payload length in bytes (data after Type)
 * @code: Message Code (EAP_CODE_*)
 * @identifier: Identifier
 * @payload: Pointer to payload pointer that will be set to point to the
 * beginning of the payload or %NULL if payload pointer is not needed
 * Returns: Pointer to the allocated message buffer or %NULL on error
 *
 * This function can be used to allocate a buffer for an EAP message and fill
 * in the EAP header. This function is automatically using expanded EAP header
 * if the selected Vendor-Id is not IETF. In other words, most EAP methods do
 * not need to separately select which header type to use when using this
 * function to allocate the message buffers.
 */
struct eap_hdr * eap_msg_alloc(int vendor, EapType type, size_t *len,
			       size_t payload_len, u8 code, u8 identifier,
			       u8 **payload)
{
	struct eap_hdr *hdr;
	u8 *pos;

	*len = sizeof(struct eap_hdr) + (vendor == EAP_VENDOR_IETF ? 1 : 8) +
		payload_len;
	hdr = malloc(*len);
	if (hdr) {
		hdr->code = code;
		hdr->identifier = identifier;
		hdr->length = host_to_be16(*len);
		pos = (u8 *) (hdr + 1);
		if (vendor == EAP_VENDOR_IETF) {
			*pos++ = type;
		} else {
			*pos++ = EAP_TYPE_EXPANDED;
			WPA_PUT_BE24(pos, vendor);
			pos += 3;
			WPA_PUT_BE32(pos, type);
			pos += 4;
		}
		if (payload)
			*payload = pos;
	}

	return hdr;
}
