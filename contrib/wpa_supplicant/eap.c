/*
 * WPA Supplicant / EAP state machines
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
#include "tls.h"
#include "md5.h"


#define EAP_MAX_AUTH_ROUNDS 50


#ifdef EAP_MD5
extern const struct eap_method eap_method_md5;
#endif
#ifdef EAP_TLS
extern const struct eap_method eap_method_tls;
#endif
#ifdef EAP_MSCHAPv2
extern const struct eap_method eap_method_mschapv2;
#endif
#ifdef EAP_PEAP
extern const struct eap_method eap_method_peap;
#endif
#ifdef EAP_TTLS
extern const struct eap_method eap_method_ttls;
#endif
#ifdef EAP_GTC
extern const struct eap_method eap_method_gtc;
#endif
#ifdef EAP_OTP
extern const struct eap_method eap_method_otp;
#endif
#ifdef EAP_SIM
extern const struct eap_method eap_method_sim;
#endif
#ifdef EAP_LEAP
extern const struct eap_method eap_method_leap;
#endif
#ifdef EAP_PSK
extern const struct eap_method eap_method_psk;
#endif
#ifdef EAP_AKA
extern const struct eap_method eap_method_aka;
#endif
#ifdef EAP_FAST
extern const struct eap_method eap_method_fast;
#endif

static const struct eap_method *eap_methods[] =
{
#ifdef EAP_MD5
	&eap_method_md5,
#endif
#ifdef EAP_TLS
	&eap_method_tls,
#endif
#ifdef EAP_MSCHAPv2
	&eap_method_mschapv2,
#endif
#ifdef EAP_PEAP
	&eap_method_peap,
#endif
#ifdef EAP_TTLS
	&eap_method_ttls,
#endif
#ifdef EAP_GTC
	&eap_method_gtc,
#endif
#ifdef EAP_OTP
	&eap_method_otp,
#endif
#ifdef EAP_SIM
	&eap_method_sim,
#endif
#ifdef EAP_LEAP
	&eap_method_leap,
#endif
#ifdef EAP_PSK
	&eap_method_psk,
#endif
#ifdef EAP_AKA
	&eap_method_aka,
#endif
#ifdef EAP_FAST
	&eap_method_fast,
#endif
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


static Boolean eap_sm_allowMethod(struct eap_sm *sm, EapType method);
static u8 * eap_sm_buildNak(struct eap_sm *sm, int id, size_t *len);
static void eap_sm_processIdentity(struct eap_sm *sm, u8 *req, size_t len);
static void eap_sm_processNotify(struct eap_sm *sm, u8 *req, size_t len);
static u8 * eap_sm_buildNotify(struct eap_sm *sm, int id, size_t *len);
static void eap_sm_parseEapReq(struct eap_sm *sm, u8 *req, size_t len);
static const char * eap_sm_method_state_txt(int state);
static const char * eap_sm_decision_txt(int decision);


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


static unsigned int eapol_get_int(struct eap_sm *sm, enum eapol_int_var var)
{
	return sm->eapol_cb->get_int(sm->eapol_ctx, var);
}


static void eapol_set_int(struct eap_sm *sm, enum eapol_int_var var,
			  unsigned int value)
{
	sm->eapol_cb->set_int(sm->eapol_ctx, var, value);
}


static u8 * eapol_get_eapReqData(struct eap_sm *sm, size_t *len)
{
	return sm->eapol_cb->get_eapReqData(sm->eapol_ctx, len);
}


static void eap_deinit_prev_method(struct eap_sm *sm, const char *txt)
{
	if (sm->m == NULL || sm->eap_method_priv == NULL)
		return;

	wpa_printf(MSG_DEBUG, "EAP: deinitialize previously used EAP method "
		   "(%d, %s) at %s", sm->selectedMethod, sm->m->name, txt);
	sm->m->deinit(sm, sm->eap_method_priv);
	sm->eap_method_priv = NULL;
	sm->m = NULL;
}


SM_STATE(EAP, INITIALIZE)
{
	SM_ENTRY(EAP, INITIALIZE);
	if (sm->fast_reauth && sm->m && sm->m->has_reauth_data &&
	    sm->m->has_reauth_data(sm, sm->eap_method_priv)) {
		wpa_printf(MSG_DEBUG, "EAP: maintaining EAP method data for "
			   "fast reauthentication");
		sm->m->deinit_for_reauth(sm, sm->eap_method_priv);
	} else {
		eap_deinit_prev_method(sm, "INITIALIZE");
	}
	sm->selectedMethod = EAP_TYPE_NONE;
	sm->methodState = METHOD_NONE;
	sm->allowNotifications = TRUE;
	sm->decision = DECISION_FAIL;
	eapol_set_int(sm, EAPOL_idleWhile, sm->ClientTimeout);
	eapol_set_bool(sm, EAPOL_eapSuccess, FALSE);
	eapol_set_bool(sm, EAPOL_eapFail, FALSE);
	free(sm->eapKeyData);
	sm->eapKeyData = NULL;
	sm->eapKeyAvailable = FALSE;
	eapol_set_bool(sm, EAPOL_eapRestart, FALSE);
	sm->lastId = -1; /* new session - make sure this does not match with
			  * the first EAP-Packet */
	/* draft-ietf-eap-statemachine-02.pdf does not reset eapResp and
	 * eapNoResp here. However, this seemed to be able to trigger cases
	 * where both were set and if EAPOL state machine uses eapNoResp first,
	 * it may end up not sending a real reply correctly. This occurred
	 * when the workaround in FAIL state set eapNoResp = TRUE.. Maybe that
	 * workaround needs to be fixed to do something else(?) */
	eapol_set_bool(sm, EAPOL_eapResp, FALSE);
	eapol_set_bool(sm, EAPOL_eapNoResp, FALSE);
	sm->num_rounds = 0;
}


SM_STATE(EAP, DISABLED)
{
	SM_ENTRY(EAP, DISABLED);
	sm->num_rounds = 0;
}


SM_STATE(EAP, IDLE)
{
	SM_ENTRY(EAP, IDLE);
}


SM_STATE(EAP, RECEIVED)
{
	u8 *eapReqData;
	size_t eapReqDataLen;

	SM_ENTRY(EAP, RECEIVED);
	eapReqData = eapol_get_eapReqData(sm, &eapReqDataLen);
	/* parse rxReq, rxSuccess, rxFailure, reqId, reqMethod */
	eap_sm_parseEapReq(sm, eapReqData, eapReqDataLen);
	sm->num_rounds++;
}


SM_STATE(EAP, GET_METHOD)
{
	SM_ENTRY(EAP, GET_METHOD);
	if (eap_sm_allowMethod(sm, sm->reqMethod)) {
		int reinit = 0;
		if (sm->fast_reauth &&
		    sm->m && sm->m->method == sm->reqMethod &&
		    sm->m->has_reauth_data &&
		    sm->m->has_reauth_data(sm, sm->eap_method_priv)) {
			wpa_printf(MSG_DEBUG, "EAP: using previous method data"
				   " for fast re-authentication");
			reinit = 1;
		} else
			eap_deinit_prev_method(sm, "GET_METHOD");
		sm->selectedMethod = sm->reqMethod;
		if (sm->m == NULL)
			sm->m = eap_sm_get_eap_methods(sm->selectedMethod);
		if (sm->m) {
			wpa_printf(MSG_DEBUG, "EAP: initialize selected EAP "
				   "method (%d, %s)",
				   sm->selectedMethod, sm->m->name);
			if (reinit)
				sm->eap_method_priv = sm->m->init_for_reauth(
					sm, sm->eap_method_priv);
			else
				sm->eap_method_priv = sm->m->init(sm);
			if (sm->eap_method_priv == NULL) {
				wpa_printf(MSG_DEBUG, "EAP: Failed to "
					   "initialize EAP method %d",
					   sm->selectedMethod);
				sm->m = NULL;
				sm->methodState = METHOD_NONE;
				sm->selectedMethod = EAP_TYPE_NONE;
			} else {
				sm->methodState = METHOD_INIT;
				return;
			}
		}
	}

	free(sm->eapRespData);
	sm->eapRespData = eap_sm_buildNak(sm, sm->reqId, &sm->eapRespDataLen);
}


SM_STATE(EAP, METHOD)
{
	u8 *eapReqData;
	size_t eapReqDataLen;
	struct eap_method_ret ret;

	SM_ENTRY(EAP, METHOD);
	if (sm->m == NULL) {
		wpa_printf(MSG_WARNING, "EAP::METHOD - method not selected");
		return;
	}

	eapReqData = eapol_get_eapReqData(sm, &eapReqDataLen);

	/* Get ignore, methodState, decision, allowNotifications, and
	 * eapRespData. */
	memset(&ret, 0, sizeof(ret));
	ret.ignore = sm->ignore;
	ret.methodState = sm->methodState;
	ret.decision = sm->decision;
	ret.allowNotifications = sm->allowNotifications;
	free(sm->eapRespData);
	sm->eapRespData = sm->m->process(sm, sm->eap_method_priv, &ret,
					 eapReqData, eapReqDataLen,
					 &sm->eapRespDataLen);
	wpa_printf(MSG_DEBUG, "EAP: method process -> ignore=%s "
		   "methodState=%s decision=%s",
		   ret.ignore ? "TRUE" : "FALSE",
		   eap_sm_method_state_txt(ret.methodState),
		   eap_sm_decision_txt(ret.decision));

	sm->ignore = ret.ignore;
	if (sm->ignore)
		return;
	sm->methodState = ret.methodState;
	sm->decision = ret.decision;
	sm->allowNotifications = ret.allowNotifications;

	if (sm->m->isKeyAvailable && sm->m->getKey &&
	    sm->m->isKeyAvailable(sm, sm->eap_method_priv)) {
		free(sm->eapKeyData);
		sm->eapKeyData = sm->m->getKey(sm, sm->eap_method_priv,
					       &sm->eapKeyDataLen);
	}
}


SM_STATE(EAP, SEND_RESPONSE)
{
	SM_ENTRY(EAP, SEND_RESPONSE);
	free(sm->lastRespData);
	if (sm->eapRespData) {
		if (sm->workaround)
			memcpy(sm->last_md5, sm->req_md5, 16);
		sm->lastId = sm->reqId;
		sm->lastRespData = malloc(sm->eapRespDataLen);
		if (sm->lastRespData) {
			memcpy(sm->lastRespData, sm->eapRespData,
			       sm->eapRespDataLen);
			sm->lastRespDataLen = sm->eapRespDataLen;
		}
		eapol_set_bool(sm, EAPOL_eapResp, TRUE);
	} else
		sm->lastRespData = NULL;
	eapol_set_bool(sm, EAPOL_eapReq, FALSE);
	eapol_set_int(sm, EAPOL_idleWhile, sm->ClientTimeout);
}


SM_STATE(EAP, DISCARD)
{
	SM_ENTRY(EAP, DISCARD);
	eapol_set_bool(sm, EAPOL_eapReq, FALSE);
	eapol_set_bool(sm, EAPOL_eapNoResp, TRUE);
}


SM_STATE(EAP, IDENTITY)
{
	u8 *eapReqData;
	size_t eapReqDataLen;

	SM_ENTRY(EAP, IDENTITY);
	eapReqData = eapol_get_eapReqData(sm, &eapReqDataLen);
	eap_sm_processIdentity(sm, eapReqData, eapReqDataLen);
	free(sm->eapRespData);
	sm->eapRespData = eap_sm_buildIdentity(sm, sm->reqId,
					       &sm->eapRespDataLen, 0);
}


SM_STATE(EAP, NOTIFICATION)
{
	u8 *eapReqData;
	size_t eapReqDataLen;

	SM_ENTRY(EAP, NOTIFICATION);
	eapReqData = eapol_get_eapReqData(sm, &eapReqDataLen);
	eap_sm_processNotify(sm, eapReqData, eapReqDataLen);
	free(sm->eapRespData);
	sm->eapRespData = eap_sm_buildNotify(sm, sm->reqId,
					     &sm->eapRespDataLen);
}


SM_STATE(EAP, RETRANSMIT)
{
	SM_ENTRY(EAP, RETRANSMIT);
	free(sm->eapRespData);
	if (sm->lastRespData) {
		sm->eapRespData = malloc(sm->lastRespDataLen);
		if (sm->eapRespData) {
			memcpy(sm->eapRespData, sm->lastRespData,
			       sm->lastRespDataLen);
			sm->eapRespDataLen = sm->lastRespDataLen;
		}
	} else
		sm->eapRespData = NULL;
}


SM_STATE(EAP, SUCCESS)
{
	SM_ENTRY(EAP, SUCCESS);
	if (sm->eapKeyData != NULL)
		sm->eapKeyAvailable = TRUE;
	eapol_set_bool(sm, EAPOL_eapSuccess, TRUE);
	/* draft-ietf-eap-statemachine-02.pdf does not clear eapReq here, but
	 * this seems to be required to avoid processing the same request
	 * twice when state machine is initialized. */
	eapol_set_bool(sm, EAPOL_eapReq, FALSE);
	/* draft-ietf-eap-statemachine-02.pdf does not set eapNoResp here, but
	 * this seems to be required to get EAPOL Supplicant backend state
	 * machine into SUCCESS state. In addition, either eapResp or eapNoResp
	 * is required to be set after processing the received EAP frame. */
	eapol_set_bool(sm, EAPOL_eapNoResp, TRUE);
}


SM_STATE(EAP, FAILURE)
{
	SM_ENTRY(EAP, FAILURE);
	eapol_set_bool(sm, EAPOL_eapFail, TRUE);
	/* draft-ietf-eap-statemachine-02.pdf does not clear eapReq here, but
	 * this seems to be required to avoid processing the same request
	 * twice when state machine is initialized. */
	eapol_set_bool(sm, EAPOL_eapReq, FALSE);
	/* draft-ietf-eap-statemachine-02.pdf does not set eapNoResp here.
	 * However, either eapResp or eapNoResp is required to be set after
	 * processing the received EAP frame. */
	eapol_set_bool(sm, EAPOL_eapNoResp, TRUE);
}


static int eap_success_workaround(struct eap_sm *sm, int reqId, int lastId)
{
	/*
	 * At least Microsoft IAS and Meetinghouse Aegis seem to be sending
	 * EAP-Success/Failure with lastId + 1 even though RFC 3748 and
	 * draft-ietf-eap-statemachine-05.pdf require that reqId == lastId.
	 * In addition, it looks like Ringmaster v2.1.2.0 would be using
	 * lastId + 2 in EAP-Success.
	 *
	 * Accept this kind of Id if EAP workarounds are enabled. These are
	 * unauthenticated plaintext messages, so this should have minimal
	 * security implications (bit easier to fake EAP-Success/Failure).
	 */
	if (sm->workaround && (reqId == ((lastId + 1) & 0xff) ||
			       reqId == ((lastId + 2) & 0xff))) {
		wpa_printf(MSG_DEBUG, "EAP: Workaround for unexpected "
			   "identifier field in EAP Success: "
			   "reqId=%d lastId=%d (these are supposed to be "
			   "same)", reqId, lastId);
		return 1;
	}
	wpa_printf(MSG_DEBUG, "EAP: EAP-Success Id mismatch - reqId=%d "
		   "lastId=%d", reqId, lastId);
	return 0;
}


SM_STEP(EAP)
{
	int duplicate;

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
		SM_ENTER(EAP, IDLE);
		break;
	case EAP_DISABLED:
		if (eapol_get_bool(sm, EAPOL_portEnabled))
			SM_ENTER(EAP, INITIALIZE);
		break;
	case EAP_IDLE:
		if (eapol_get_bool(sm, EAPOL_eapReq))
			SM_ENTER(EAP, RECEIVED);
		else if ((eapol_get_bool(sm, EAPOL_altAccept) &&
			  sm->decision != DECISION_FAIL) ||
			 (eapol_get_int(sm, EAPOL_idleWhile) == 0 &&
			  sm->decision == DECISION_UNCOND_SUCC))
			SM_ENTER(EAP, SUCCESS);
		else if (eapol_get_bool(sm, EAPOL_altReject) ||
			 (eapol_get_int(sm, EAPOL_idleWhile) == 0 &&
			  sm->decision != DECISION_UNCOND_SUCC) ||
			 (eapol_get_bool(sm, EAPOL_altAccept) &&
			  sm->methodState != METHOD_CONT &&
			  sm->decision == DECISION_FAIL))
			SM_ENTER(EAP, FAILURE);
		else if (sm->selectedMethod == EAP_TYPE_LEAP &&
			 sm->leap_done && sm->decision != DECISION_FAIL &&
			 sm->methodState == METHOD_DONE)
			SM_ENTER(EAP, SUCCESS);
		else if (sm->selectedMethod == EAP_TYPE_PEAP &&
			 sm->peap_done && sm->decision != DECISION_FAIL &&
			 sm->methodState == METHOD_DONE)
			SM_ENTER(EAP, SUCCESS);
		break;
	case EAP_RECEIVED:
		duplicate = sm->reqId == sm->lastId;
		if (sm->workaround && duplicate &&
		    memcmp(sm->req_md5, sm->last_md5, 16) != 0) {
			/* draft-ietf-eap-statemachine-05.txt uses
			 * (reqId == lastId) as the only verification for
			 * duplicate EAP requests. However, this misses cases
			 * where the AS is incorrectly using the same id again;
			 * and unfortunately, such implementations exist. Use
			 * MD5 hash as an extra verification for the packets
			 * being duplicate to workaround these issues. */
			wpa_printf(MSG_DEBUG, "EAP: AS used the same Id again,"
				   " but EAP packets were not identical");
			wpa_printf(MSG_DEBUG, "EAP: workaround - assume this "
				   "is not a duplicate packet");
			duplicate = 0;
		}

		if (sm->rxSuccess &&
		    (sm->reqId == sm->lastId ||
		     eap_success_workaround(sm, sm->reqId, sm->lastId)) &&
		    sm->decision != DECISION_FAIL)
			SM_ENTER(EAP, SUCCESS);
		else if (sm->methodState != METHOD_CONT &&
			 ((sm->rxFailure &&
			   sm->decision != DECISION_UNCOND_SUCC) ||
			  (sm->rxSuccess && sm->decision == DECISION_FAIL)) &&
			 (sm->reqId == sm->lastId ||
			  eap_success_workaround(sm, sm->reqId, sm->lastId)))
			SM_ENTER(EAP, FAILURE);
		else if (sm->rxReq && duplicate)
			SM_ENTER(EAP, RETRANSMIT);
		else if (sm->rxReq && !duplicate &&
			 sm->reqMethod == EAP_TYPE_NOTIFICATION &&
			 sm->allowNotifications)
			SM_ENTER(EAP, NOTIFICATION);
		else if (sm->rxReq && !duplicate &&
			 sm->selectedMethod == EAP_TYPE_NONE &&
			 sm->reqMethod == EAP_TYPE_IDENTITY)
			SM_ENTER(EAP, IDENTITY);
		else if (sm->rxReq && !duplicate &&
			 sm->selectedMethod == EAP_TYPE_NONE &&
			 sm->reqMethod != EAP_TYPE_IDENTITY &&
			 sm->reqMethod != EAP_TYPE_NOTIFICATION)
			SM_ENTER(EAP, GET_METHOD);
		else if (sm->rxReq && !duplicate &&
			 sm->reqMethod == sm->selectedMethod &&
			 sm->methodState != METHOD_DONE)
			SM_ENTER(EAP, METHOD);
		else if (sm->selectedMethod == EAP_TYPE_LEAP &&
			 (sm->rxSuccess || sm->rxResp))
			SM_ENTER(EAP, METHOD);
		else
			SM_ENTER(EAP, DISCARD);
		break;
	case EAP_GET_METHOD:
		if (sm->selectedMethod == sm->reqMethod)
			SM_ENTER(EAP, METHOD);
		else
			SM_ENTER(EAP, SEND_RESPONSE);
		break;
	case EAP_METHOD:
		if (sm->ignore)
			SM_ENTER(EAP, DISCARD);
		else
			SM_ENTER(EAP, SEND_RESPONSE);
		break;
	case EAP_SEND_RESPONSE:
		SM_ENTER(EAP, IDLE);
		break;
	case EAP_DISCARD:
		SM_ENTER(EAP, IDLE);
		break;
	case EAP_IDENTITY:
		SM_ENTER(EAP, SEND_RESPONSE);
		break;
	case EAP_NOTIFICATION:
		SM_ENTER(EAP, SEND_RESPONSE);
		break;
	case EAP_RETRANSMIT:
		SM_ENTER(EAP, SEND_RESPONSE);
		break;
	case EAP_SUCCESS:
		break;
	case EAP_FAILURE:
		break;
	}
}


static Boolean eap_sm_allowMethod(struct eap_sm *sm, EapType method)
{
	struct wpa_ssid *config = eap_get_config(sm);
	int i;

	if (!wpa_config_allowed_eap_method(config, method))
		return FALSE;
	for (i = 0; i < NUM_EAP_METHODS; i++) {
		if (eap_methods[i]->method == method)
			return TRUE;
	}
	return FALSE;
}


static u8 *eap_sm_buildNak(struct eap_sm *sm, int id, size_t *len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_hdr *resp;
	u8 *pos;
	int i, found = 0;

	wpa_printf(MSG_DEBUG, "EAP: Building EAP-Nak (requested type %d not "
		   "allowed)", sm->reqMethod);
	*len = sizeof(struct eap_hdr) + 1;
	resp = malloc(*len + NUM_EAP_METHODS);
	if (resp == NULL)
		return NULL;

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;
	pos = (u8 *) (resp + 1);
	*pos++ = EAP_TYPE_NAK;

	for (i = 0; i < NUM_EAP_METHODS; i++) {
		if (wpa_config_allowed_eap_method(config,
						  eap_methods[i]->method)) {
			*pos++ = eap_methods[i]->method;
			(*len)++;
			found++;
		}
	}
	if (!found) {
		*pos = EAP_TYPE_NONE;
		(*len)++;
	}
	wpa_hexdump(MSG_DEBUG, "EAP: allowed methods",
		    ((u8 *) (resp + 1)) + 1, found);

	resp->length = host_to_be16(*len);

	return (u8 *) resp;
}


static void eap_sm_processIdentity(struct eap_sm *sm, u8 *req, size_t len)
{
	struct eap_hdr *hdr = (struct eap_hdr *) req;
	u8 *pos = (u8 *) (hdr + 1);
	pos++;
	/* TODO: could save displayable message so that it can be shown to the
	 * user in case of interaction is required */
	wpa_hexdump_ascii(MSG_DEBUG, "EAP: EAP-Request Identity data",
			  pos, be_to_host16(hdr->length) - 5);
}


u8 *eap_sm_buildIdentity(struct eap_sm *sm, int id, size_t *len,
			 int encrypted)
{
	struct wpa_ssid *config = eap_get_config(sm);
	struct eap_hdr *resp;
	u8 *pos;
	const u8 *identity;
	size_t identity_len;

	if (config == NULL) {
		wpa_printf(MSG_WARNING, "EAP: buildIdentity: configuration "
			   "was not available");
		return NULL;
	}

	if (sm->m && sm->m->get_identity &&
	    (identity = sm->m->get_identity(sm, sm->eap_method_priv,
					    &identity_len)) != NULL) {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP: using method re-auth "
				  "identity", identity, identity_len);
	} else if (!encrypted && config->anonymous_identity) {
		identity = config->anonymous_identity;
		identity_len = config->anonymous_identity_len;
		wpa_hexdump_ascii(MSG_DEBUG, "EAP: using anonymous identity",
				  identity, identity_len);
	} else {
		identity = config->identity;
		identity_len = config->identity_len;
		wpa_hexdump_ascii(MSG_DEBUG, "EAP: using real identity",
				  identity, identity_len);
	}

	if (identity == NULL) {
		wpa_printf(MSG_WARNING, "EAP: buildIdentity: identity "
			   "configuration was not available");
		eap_sm_request_identity(sm, config);
		return NULL;
	}


	*len = sizeof(struct eap_hdr) + 1 + identity_len;
	resp = malloc(*len);
	if (resp == NULL)
		return NULL;

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;
	resp->length = host_to_be16(*len);
	pos = (u8 *) (resp + 1);
	*pos++ = EAP_TYPE_IDENTITY;
	memcpy(pos, identity, identity_len);

	return (u8 *) resp;
}


static void eap_sm_processNotify(struct eap_sm *sm, u8 *req, size_t len)
{
	struct eap_hdr *hdr = (struct eap_hdr *) req;
	u8 *pos = (u8 *) (hdr + 1);
	pos++;
	/* TODO: log the Notification Request and make it available for UI */
	wpa_hexdump_ascii(MSG_DEBUG, "EAP: EAP-Request Notification data",
			  pos, be_to_host16(hdr->length) - 5);
}


static u8 *eap_sm_buildNotify(struct eap_sm *sm, int id, size_t *len)
{
	struct eap_hdr *resp;
	u8 *pos;

	wpa_printf(MSG_DEBUG, "EAP: Generating EAP-Response Notification");
	*len = sizeof(struct eap_hdr) + 1;
	resp = malloc(*len);
	if (resp == NULL)
		return NULL;

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;
	resp->length = host_to_be16(*len);
	pos = (u8 *) (resp + 1);
	*pos = EAP_TYPE_NOTIFICATION;

	return (u8 *) resp;
}


static void eap_sm_parseEapReq(struct eap_sm *sm, u8 *req, size_t len)
{
	struct eap_hdr *hdr;
	size_t plen;
	MD5_CTX context;

	sm->rxReq = sm->rxSuccess = sm->rxFailure = FALSE;
	sm->reqId = 0;
	sm->reqMethod = EAP_TYPE_NONE;

	if (req == NULL || len < sizeof(*hdr))
		return;

	hdr = (struct eap_hdr *) req;
	plen = be_to_host16(hdr->length);
	if (plen > len) {
		wpa_printf(MSG_DEBUG, "EAP: Ignored truncated EAP-Packet "
			   "(len=%lu plen=%lu)",
			   (unsigned long) len, (unsigned long) plen);
		return;
	}

	sm->reqId = hdr->identifier;

	if (sm->workaround) {
		MD5Init(&context);
		MD5Update(&context, req, len);
		MD5Final(sm->req_md5, &context);
	}

	switch (hdr->code) {
	case EAP_CODE_REQUEST:
		sm->rxReq = TRUE;
		if (plen > sizeof(*hdr))
			sm->reqMethod = *((u8 *) (hdr + 1));
		wpa_printf(MSG_DEBUG, "EAP: Received EAP-Request method=%d "
			   "id=%d", sm->reqMethod, sm->reqId);
		break;
	case EAP_CODE_RESPONSE:
		if (sm->selectedMethod == EAP_TYPE_LEAP) {
			sm->rxResp = TRUE;
			if (plen > sizeof(*hdr))
				sm->reqMethod = *((u8 *) (hdr + 1));
			wpa_printf(MSG_DEBUG, "EAP: Received EAP-Response for "
				   "LEAP method=%d id=%d",
				   sm->reqMethod, sm->reqId);
			break;
		}
		wpa_printf(MSG_DEBUG, "EAP: Ignored EAP-Response");
		break;
	case EAP_CODE_SUCCESS:
		wpa_printf(MSG_DEBUG, "EAP: Received EAP-Success");
		sm->rxSuccess = TRUE;
		break;
	case EAP_CODE_FAILURE:
		wpa_printf(MSG_DEBUG, "EAP: Received EAP-Failure");
		sm->rxFailure = TRUE;
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP: Ignored EAP-Packet with unknown "
			   "code %d", hdr->code);
		break;
	}
}


struct eap_sm *eap_sm_init(void *eapol_ctx, struct eapol_callbacks *eapol_cb,
			   void *msg_ctx)
{
	struct eap_sm *sm;

	sm = malloc(sizeof(*sm));
	if (sm == NULL)
		return NULL;
	memset(sm, 0, sizeof(*sm));
	sm->eapol_ctx = eapol_ctx;
	sm->eapol_cb = eapol_cb;
	sm->msg_ctx = msg_ctx;
	sm->ClientTimeout = 60;

	sm->ssl_ctx = tls_init();
	if (sm->ssl_ctx == NULL) {
		wpa_printf(MSG_WARNING, "SSL: Failed to initialize TLS "
			   "context.");
		free(sm);
		return NULL;
	}

	return sm;
}


void eap_sm_deinit(struct eap_sm *sm)
{
	if (sm == NULL)
		return;
	eap_deinit_prev_method(sm, "EAP deinit");
	free(sm->lastRespData);
	free(sm->eapRespData);
	free(sm->eapKeyData);
	tls_deinit(sm->ssl_ctx);
	free(sm);
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


void eap_sm_abort(struct eap_sm *sm)
{
	/* release system resources that may have been allocated for the
	 * authentication session */
	free(sm->eapRespData);
	sm->eapRespData = NULL;
	free(sm->eapKeyData);
	sm->eapKeyData = NULL;
}


static const char * eap_sm_state_txt(int state)
{
	switch (state) {
	case EAP_INITIALIZE:
		return "INITIALIZE";
	case EAP_DISABLED:
		return "DISABLED";
	case EAP_IDLE:
		return "IDLE";
	case EAP_RECEIVED:
		return "RECEIVED";
	case EAP_GET_METHOD:
		return "GET_METHOD";
	case EAP_METHOD:
		return "METHOD";
	case EAP_SEND_RESPONSE:
		return "SEND_RESPONSE";
	case EAP_DISCARD:
		return "DISCARD";
	case EAP_IDENTITY:
		return "IDENTITY";
	case EAP_NOTIFICATION:
		return "NOTIFICATION";
	case EAP_RETRANSMIT:
		return "RETRANSMIT";
	case EAP_SUCCESS:
		return "SUCCESS";
	case EAP_FAILURE:
		return "FAILURE";
	default:
		return "UNKNOWN";
	}
}


static const char * eap_sm_method_state_txt(int state)
{
	switch (state) {
	case METHOD_NONE:
		return "NONE";
	case METHOD_INIT:
		return "INIT";
	case METHOD_CONT:
		return "CONT";
	case METHOD_MAY_CONT:
		return "MAY_CONT";
	case METHOD_DONE:
		return "DONE";
	default:
		return "UNKNOWN";
	}
}


static const char * eap_sm_decision_txt(int decision)
{
	switch (decision) {
	case DECISION_FAIL:
		return "FAIL";
	case DECISION_COND_SUCC:
		return "COND_SUCC";
	case DECISION_UNCOND_SUCC:
		return "UNCOND_SUCC";
	default:
		return "UNKNOWN";
	}
}


int eap_sm_get_status(struct eap_sm *sm, char *buf, size_t buflen, int verbose)
{
	int len;

	if (sm == NULL)
		return 0;

	len = snprintf(buf, buflen,
		       "EAP state=%s\n",
		       eap_sm_state_txt(sm->EAP_state));

	if (sm->selectedMethod != EAP_TYPE_NONE) {
		const char *name;
		if (sm->m) {
			name = sm->m->name;
		} else {
			const struct eap_method *m =
				eap_sm_get_eap_methods(sm->selectedMethod);
			if (m)
				name = m->name;
			else
				name = "?";
		}
		len += snprintf(buf + len, buflen - len,
				"selectedMethod=%d (EAP-%s)\n",
				sm->selectedMethod, name);

		if (sm->m && sm->m->get_status) {
			len += sm->m->get_status(sm, sm->eap_method_priv,
						 buf + len, buflen - len,
						 verbose);
		}
	}

	if (verbose) {
		len += snprintf(buf + len, buflen - len,
				"reqMethod=%d\n"
				"methodState=%s\n"
				"decision=%s\n"
				"ClientTimeout=%d\n",
				sm->reqMethod,
				eap_sm_method_state_txt(sm->methodState),
				eap_sm_decision_txt(sm->decision),
				sm->ClientTimeout);
	}

	return len;
}


typedef enum { TYPE_IDENTITY, TYPE_PASSWORD, TYPE_OTP } eap_ctrl_req_type;

static void eap_sm_request(struct eap_sm *sm, struct wpa_ssid *config,
			   eap_ctrl_req_type type, char *msg, size_t msglen)
{
	char *buf;
	size_t buflen;
	int len;
	char *field;
	char *txt, *tmp;

	if (config == NULL || sm == NULL)
		return;

	switch (type) {
	case TYPE_IDENTITY:
		field = "IDENTITY";
		txt = "Identity";
		config->pending_req_identity++;
		break;
	case TYPE_PASSWORD:
		field = "PASSWORD";
		txt = "Password";
		config->pending_req_password++;
		break;
	case TYPE_OTP:
		field = "OTP";
		if (msg) {
			tmp = malloc(msglen + 3);
			if (tmp == NULL)
				return;
			tmp[0] = '[';
			memcpy(tmp + 1, msg, msglen);
			tmp[msglen + 1] = ']';
			tmp[msglen + 2] = '\0';
			txt = tmp;
			free(config->pending_req_otp);
			config->pending_req_otp = tmp;
			config->pending_req_otp_len = msglen + 3;
		} else {
			if (config->pending_req_otp == NULL)
				return;
			txt = config->pending_req_otp;
		}
		break;
	default:
		return;
	}

	buflen = 100 + strlen(txt) + config->ssid_len;
	buf = malloc(buflen);
	if (buf == NULL)
		return;
	len = snprintf(buf, buflen, "CTRL-REQ-%s-%d:%s needed for SSID ",
		       field, config->id, txt);
	if (config->ssid && buflen > len + config->ssid_len) {
		memcpy(buf + len, config->ssid, config->ssid_len);
		len += config->ssid_len;
		buf[len] = '\0';
	}
	wpa_msg(sm->msg_ctx, MSG_INFO, buf);
	free(buf);
}


void eap_sm_request_identity(struct eap_sm *sm, struct wpa_ssid *config)
{
	eap_sm_request(sm, config, TYPE_IDENTITY, NULL, 0);
}


void eap_sm_request_password(struct eap_sm *sm, struct wpa_ssid *config)
{
	eap_sm_request(sm, config, TYPE_PASSWORD, NULL, 0);
}


void eap_sm_request_otp(struct eap_sm *sm, struct wpa_ssid *config,
			char *msg, size_t msg_len)
{
	eap_sm_request(sm, config, TYPE_OTP, msg, msg_len);
}


void eap_sm_notify_ctrl_attached(struct eap_sm *sm)
{
	struct wpa_ssid *config = eap_get_config(sm);

	if (config == NULL)
		return;

	/* Re-send any pending requests for user data since a new control
	 * interface was added. This handles cases where the EAP authentication
	 * starts immediately after system startup when the user interface is
	 * not yet running. */
	if (config->pending_req_identity)
		eap_sm_request_identity(sm, config);
	if (config->pending_req_password)
		eap_sm_request_password(sm, config);
	if (config->pending_req_otp)
		eap_sm_request_otp(sm, config, NULL, 0);
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


static int eap_allowed_phase2_type(int type)
{
	return type != EAP_TYPE_PEAP && type != EAP_TYPE_TTLS &&
		type != EAP_TYPE_FAST;
}


u8 eap_get_phase2_type(const char *name)
{
	u8 type = eap_get_type(name);
	if (eap_allowed_phase2_type(type))
		return type;
	return EAP_TYPE_NONE;
}


u8 *eap_get_phase2_types(struct wpa_ssid *config, size_t *count)
{
	u8 *buf, method;
	int i;

	*count = 0;
	buf = malloc(NUM_EAP_METHODS);
	if (buf == NULL)
		return NULL;

	for (i = 0; i < NUM_EAP_METHODS; i++) {
		method = eap_methods[i]->method;
		if (eap_allowed_phase2_type(method)) {
			if (method == EAP_TYPE_TLS && config &&
			    config->private_key2 == NULL)
				continue;
			buf[*count] = method;
			(*count)++;
		}
	}

	return buf;
}


void eap_set_fast_reauth(struct eap_sm *sm, int enabled)
{
	sm->fast_reauth = enabled;
}


void eap_set_workaround(struct eap_sm *sm, unsigned int workaround)
{
	sm->workaround = workaround;
}


struct wpa_ssid * eap_get_config(struct eap_sm *sm)
{
	return sm->eapol_cb->get_config(sm->eapol_ctx);
}


int eap_key_available(struct eap_sm *sm)
{
	return sm ? sm->eapKeyAvailable : 0;
}


void eap_notify_success(struct eap_sm *sm)
{
	if (sm) {
		sm->decision = DECISION_COND_SUCC;
		sm->EAP_state = EAP_SUCCESS;
	}
}


u8 * eap_get_eapKeyData(struct eap_sm *sm, size_t *len)
{
	if (sm == NULL || sm->eapKeyData == NULL) {
		*len = 0;
		return NULL;
	}

	*len = sm->eapKeyDataLen;
	return sm->eapKeyData;
}


u8 * eap_get_eapRespData(struct eap_sm *sm, size_t *len)
{
	u8 *resp;

	if (sm == NULL || sm->eapRespData == NULL) {
		*len = 0;
		return NULL;
	}

	resp = sm->eapRespData;
	*len = sm->eapRespDataLen;
	sm->eapRespData = NULL;
	sm->eapRespDataLen = 0;

	return resp;
}


void eap_register_scard_ctx(struct eap_sm *sm, void *ctx)
{
	if (sm)
		sm->scard_ctx = ctx;
}
