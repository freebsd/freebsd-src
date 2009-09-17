/*
 * hostapd / IEEE 802.1X-2004 Authenticator - EAPOL state machine
 * Copyright (c) 2002-2008, Jouni Malinen <j@w1.fi>
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
#include "ieee802_1x.h"
#include "eapol_sm.h"
#include "eloop.h"
#include "wpa.h"
#include "preauth.h"
#include "sta_info.h"
#include "eap_server/eap.h"
#include "state_machine.h"
#include "eap_common/eap_common.h"

#define STATE_MACHINE_DATA struct eapol_state_machine
#define STATE_MACHINE_DEBUG_PREFIX "IEEE 802.1X"
#define STATE_MACHINE_ADDR sm->addr

static struct eapol_callbacks eapol_cb;

/* EAPOL state machines are described in IEEE Std 802.1X-2004, Chap. 8.2 */

#define setPortAuthorized() \
sm->eapol->cb.set_port_authorized(sm->hapd, sm->sta, 1)
#define setPortUnauthorized() \
sm->eapol->cb.set_port_authorized(sm->hapd, sm->sta, 0)

/* procedures */
#define txCannedFail() eapol_auth_tx_canned_eap(sm, 0)
#define txCannedSuccess() eapol_auth_tx_canned_eap(sm, 1)
#define txReq() eapol_auth_tx_req(sm)
#define abortAuth() sm->eapol->cb.abort_auth(sm->hapd, sm->sta)
#define txKey() sm->eapol->cb.tx_key(sm->hapd, sm->sta)
#define processKey() do { } while (0)


static void eapol_sm_step_run(struct eapol_state_machine *sm);
static void eapol_sm_step_cb(void *eloop_ctx, void *timeout_ctx);


static void eapol_auth_logger(struct eapol_authenticator *eapol,
			      const u8 *addr, logger_level level,
			      const char *txt)
{
	if (eapol->cb.logger == NULL)
		return;
	eapol->cb.logger(eapol->conf.hapd, addr, level, txt);
}


static void eapol_auth_vlogger(struct eapol_authenticator *eapol,
			       const u8 *addr, logger_level level,
			       const char *fmt, ...)
{
	char *format;
	int maxlen;
	va_list ap;

	if (eapol->cb.logger == NULL)
		return;

	maxlen = os_strlen(fmt) + 100;
	format = os_malloc(maxlen);
	if (!format)
		return;

	va_start(ap, fmt);
	vsnprintf(format, maxlen, fmt, ap);
	va_end(ap);

	eapol_auth_logger(eapol, addr, level, format);

	os_free(format);
}


static void eapol_auth_tx_canned_eap(struct eapol_state_machine *sm,
				     int success)
{
	struct eap_hdr eap;

	os_memset(&eap, 0, sizeof(eap));

	eap.code = success ? EAP_CODE_SUCCESS : EAP_CODE_FAILURE;
	eap.identifier = ++sm->last_eap_id;
	eap.length = host_to_be16(sizeof(eap));

	eapol_auth_vlogger(sm->eapol, sm->addr, EAPOL_LOGGER_DEBUG,
			   "Sending canned EAP packet %s (identifier %d)",
			   success ? "SUCCESS" : "FAILURE", eap.identifier);
	sm->eapol->cb.eapol_send(sm->hapd, sm->sta, IEEE802_1X_TYPE_EAP_PACKET,
				 (u8 *) &eap, sizeof(eap));
	sm->dot1xAuthEapolFramesTx++;
}


static void eapol_auth_tx_req(struct eapol_state_machine *sm)
{
	if (sm->eap_if->eapReqData == NULL ||
	    wpabuf_len(sm->eap_if->eapReqData) < sizeof(struct eap_hdr)) {
		eapol_auth_logger(sm->eapol, sm->addr,
				  EAPOL_LOGGER_DEBUG,
				  "TxReq called, but there is no EAP request "
				  "from authentication server");
		return;
	}

	if (sm->flags & EAPOL_SM_WAIT_START) {
		wpa_printf(MSG_DEBUG, "EAPOL: Drop EAPOL TX to " MACSTR
			   " while waiting for EAPOL-Start",
			   MAC2STR(sm->addr));
		return;
	}

	sm->last_eap_id = eap_get_id(sm->eap_if->eapReqData);
	eapol_auth_vlogger(sm->eapol, sm->addr, EAPOL_LOGGER_DEBUG,
			   "Sending EAP Packet (identifier %d)",
			   sm->last_eap_id);
	sm->eapol->cb.eapol_send(sm->hapd, sm->sta, IEEE802_1X_TYPE_EAP_PACKET,
				 wpabuf_head(sm->eap_if->eapReqData),
				 wpabuf_len(sm->eap_if->eapReqData));
	sm->dot1xAuthEapolFramesTx++;
	if (eap_get_type(sm->eap_if->eapReqData) == EAP_TYPE_IDENTITY)
		sm->dot1xAuthEapolReqIdFramesTx++;
	else
		sm->dot1xAuthEapolReqFramesTx++;
}


/**
 * eapol_port_timers_tick - Port Timers state machine
 * @eloop_ctx: struct eapol_state_machine *
 * @timeout_ctx: Not used
 *
 * This statemachine is implemented as a function that will be called
 * once a second as a registered event loop timeout.
 */
static void eapol_port_timers_tick(void *eloop_ctx, void *timeout_ctx)
{
	struct eapol_state_machine *state = timeout_ctx;

	if (state->aWhile > 0) {
		state->aWhile--;
		if (state->aWhile == 0) {
			wpa_printf(MSG_DEBUG, "IEEE 802.1X: " MACSTR
				   " - aWhile --> 0",
				   MAC2STR(state->addr));
		}
	}

	if (state->quietWhile > 0) {
		state->quietWhile--;
		if (state->quietWhile == 0) {
			wpa_printf(MSG_DEBUG, "IEEE 802.1X: " MACSTR
				   " - quietWhile --> 0",
				   MAC2STR(state->addr));
		}
	}

	if (state->reAuthWhen > 0) {
		state->reAuthWhen--;
		if (state->reAuthWhen == 0) {
			wpa_printf(MSG_DEBUG, "IEEE 802.1X: " MACSTR
				   " - reAuthWhen --> 0",
				   MAC2STR(state->addr));
		}
	}

	if (state->eap_if->retransWhile > 0) {
		state->eap_if->retransWhile--;
		if (state->eap_if->retransWhile == 0) {
			wpa_printf(MSG_DEBUG, "IEEE 802.1X: " MACSTR
				   " - (EAP) retransWhile --> 0",
				   MAC2STR(state->addr));
		}
	}

	eapol_sm_step_run(state);

	eloop_register_timeout(1, 0, eapol_port_timers_tick, eloop_ctx, state);
}



/* Authenticator PAE state machine */

SM_STATE(AUTH_PAE, INITIALIZE)
{
	SM_ENTRY_MA(AUTH_PAE, INITIALIZE, auth_pae);
	sm->portMode = Auto;
}


SM_STATE(AUTH_PAE, DISCONNECTED)
{
	int from_initialize = sm->auth_pae_state == AUTH_PAE_INITIALIZE;

	if (sm->eapolLogoff) {
		if (sm->auth_pae_state == AUTH_PAE_CONNECTING)
			sm->authEapLogoffsWhileConnecting++;
		else if (sm->auth_pae_state == AUTH_PAE_AUTHENTICATED)
			sm->authAuthEapLogoffWhileAuthenticated++;
	}

	SM_ENTRY_MA(AUTH_PAE, DISCONNECTED, auth_pae);

	sm->authPortStatus = Unauthorized;
	setPortUnauthorized();
	sm->reAuthCount = 0;
	sm->eapolLogoff = FALSE;
	if (!from_initialize) {
		sm->eapol->cb.finished(sm->hapd, sm->sta, 0,
				       sm->flags & EAPOL_SM_PREAUTH);
	}
}


SM_STATE(AUTH_PAE, RESTART)
{
	if (sm->auth_pae_state == AUTH_PAE_AUTHENTICATED) {
		if (sm->reAuthenticate)
			sm->authAuthReauthsWhileAuthenticated++;
		if (sm->eapolStart)
			sm->authAuthEapStartsWhileAuthenticated++;
		if (sm->eapolLogoff)
			sm->authAuthEapLogoffWhileAuthenticated++;
	}

	SM_ENTRY_MA(AUTH_PAE, RESTART, auth_pae);

	sm->eap_if->eapRestart = TRUE;
}


SM_STATE(AUTH_PAE, CONNECTING)
{
	if (sm->auth_pae_state != AUTH_PAE_CONNECTING)
		sm->authEntersConnecting++;

	SM_ENTRY_MA(AUTH_PAE, CONNECTING, auth_pae);

	sm->reAuthenticate = FALSE;
	sm->reAuthCount++;
}


SM_STATE(AUTH_PAE, HELD)
{
	if (sm->auth_pae_state == AUTH_PAE_AUTHENTICATING && sm->authFail)
		sm->authAuthFailWhileAuthenticating++;

	SM_ENTRY_MA(AUTH_PAE, HELD, auth_pae);

	sm->authPortStatus = Unauthorized;
	setPortUnauthorized();
	sm->quietWhile = sm->quietPeriod;
	sm->eapolLogoff = FALSE;

	eapol_auth_vlogger(sm->eapol, sm->addr, EAPOL_LOGGER_WARNING,
			   "authentication failed - EAP type: %d (%s)",
			   sm->eap_type_authsrv,
			   eap_type_text(sm->eap_type_authsrv));
	if (sm->eap_type_authsrv != sm->eap_type_supp) {
		eapol_auth_vlogger(sm->eapol, sm->addr, EAPOL_LOGGER_INFO,
				   "Supplicant used different EAP type: "
				   "%d (%s)", sm->eap_type_supp,
				   eap_type_text(sm->eap_type_supp));
	}
	sm->eapol->cb.finished(sm->hapd, sm->sta, 0,
			       sm->flags & EAPOL_SM_PREAUTH);
}


SM_STATE(AUTH_PAE, AUTHENTICATED)
{
	char *extra = "";

	if (sm->auth_pae_state == AUTH_PAE_AUTHENTICATING && sm->authSuccess)
		sm->authAuthSuccessesWhileAuthenticating++;
							
	SM_ENTRY_MA(AUTH_PAE, AUTHENTICATED, auth_pae);

	sm->authPortStatus = Authorized;
	setPortAuthorized();
	sm->reAuthCount = 0;
	if (sm->flags & EAPOL_SM_PREAUTH)
		extra = " (pre-authentication)";
	else if (wpa_auth_sta_get_pmksa(sm->sta->wpa_sm))
		extra = " (PMKSA cache)";
	eapol_auth_vlogger(sm->eapol, sm->addr, EAPOL_LOGGER_INFO,
			   "authenticated - EAP type: %d (%s)%s",
			   sm->eap_type_authsrv,
			   eap_type_text(sm->eap_type_authsrv), extra);
	sm->eapol->cb.finished(sm->hapd, sm->sta, 1,
			       sm->flags & EAPOL_SM_PREAUTH);
}


SM_STATE(AUTH_PAE, AUTHENTICATING)
{
	SM_ENTRY_MA(AUTH_PAE, AUTHENTICATING, auth_pae);

	sm->eapolStart = FALSE;
	sm->authSuccess = FALSE;
	sm->authFail = FALSE;
	sm->authTimeout = FALSE;
	sm->authStart = TRUE;
	sm->keyRun = FALSE;
	sm->keyDone = FALSE;
}


SM_STATE(AUTH_PAE, ABORTING)
{
	if (sm->auth_pae_state == AUTH_PAE_AUTHENTICATING) {
		if (sm->authTimeout)
			sm->authAuthTimeoutsWhileAuthenticating++;
		if (sm->eapolStart)
			sm->authAuthEapStartsWhileAuthenticating++;
		if (sm->eapolLogoff)
			sm->authAuthEapLogoffWhileAuthenticating++;
	}

	SM_ENTRY_MA(AUTH_PAE, ABORTING, auth_pae);

	sm->authAbort = TRUE;
	sm->keyRun = FALSE;
	sm->keyDone = FALSE;
}


SM_STATE(AUTH_PAE, FORCE_AUTH)
{
	SM_ENTRY_MA(AUTH_PAE, FORCE_AUTH, auth_pae);

	sm->authPortStatus = Authorized;
	setPortAuthorized();
	sm->portMode = ForceAuthorized;
	sm->eapolStart = FALSE;
	txCannedSuccess();
}


SM_STATE(AUTH_PAE, FORCE_UNAUTH)
{
	SM_ENTRY_MA(AUTH_PAE, FORCE_UNAUTH, auth_pae);

	sm->authPortStatus = Unauthorized;
	setPortUnauthorized();
	sm->portMode = ForceUnauthorized;
	sm->eapolStart = FALSE;
	txCannedFail();
}


SM_STEP(AUTH_PAE)
{
	if ((sm->portControl == Auto && sm->portMode != sm->portControl) ||
	    sm->initialize || !sm->eap_if->portEnabled)
		SM_ENTER_GLOBAL(AUTH_PAE, INITIALIZE);
	else if (sm->portControl == ForceAuthorized &&
		 sm->portMode != sm->portControl &&
		 !(sm->initialize || !sm->eap_if->portEnabled))
		SM_ENTER_GLOBAL(AUTH_PAE, FORCE_AUTH);
	else if (sm->portControl == ForceUnauthorized &&
		 sm->portMode != sm->portControl &&
		 !(sm->initialize || !sm->eap_if->portEnabled))
		SM_ENTER_GLOBAL(AUTH_PAE, FORCE_UNAUTH);
	else {
		switch (sm->auth_pae_state) {
		case AUTH_PAE_INITIALIZE:
			SM_ENTER(AUTH_PAE, DISCONNECTED);
			break;
		case AUTH_PAE_DISCONNECTED:
			SM_ENTER(AUTH_PAE, RESTART);
			break;
		case AUTH_PAE_RESTART:
			if (!sm->eap_if->eapRestart)
				SM_ENTER(AUTH_PAE, CONNECTING);
			break;
		case AUTH_PAE_HELD:
			if (sm->quietWhile == 0)
				SM_ENTER(AUTH_PAE, RESTART);
			break;
		case AUTH_PAE_CONNECTING:
			if (sm->eapolLogoff || sm->reAuthCount > sm->reAuthMax)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			else if ((sm->eap_if->eapReq &&
				  sm->reAuthCount <= sm->reAuthMax) ||
				 sm->eap_if->eapSuccess || sm->eap_if->eapFail)
				SM_ENTER(AUTH_PAE, AUTHENTICATING);
			break;
		case AUTH_PAE_AUTHENTICATED:
			if (sm->eapolStart || sm->reAuthenticate)
				SM_ENTER(AUTH_PAE, RESTART);
			else if (sm->eapolLogoff || !sm->portValid)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			break;
		case AUTH_PAE_AUTHENTICATING:
			if (sm->authSuccess && sm->portValid)
				SM_ENTER(AUTH_PAE, AUTHENTICATED);
			else if (sm->authFail ||
				 (sm->keyDone && !sm->portValid))
				SM_ENTER(AUTH_PAE, HELD);
			else if (sm->eapolStart || sm->eapolLogoff ||
				 sm->authTimeout)
				SM_ENTER(AUTH_PAE, ABORTING);
			break;
		case AUTH_PAE_ABORTING:
			if (sm->eapolLogoff && !sm->authAbort)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			else if (!sm->eapolLogoff && !sm->authAbort)
				SM_ENTER(AUTH_PAE, RESTART);
			break;
		case AUTH_PAE_FORCE_AUTH:
			if (sm->eapolStart)
				SM_ENTER(AUTH_PAE, FORCE_AUTH);
			break;
		case AUTH_PAE_FORCE_UNAUTH:
			if (sm->eapolStart)
				SM_ENTER(AUTH_PAE, FORCE_UNAUTH);
			break;
		}
	}
}



/* Backend Authentication state machine */

SM_STATE(BE_AUTH, INITIALIZE)
{
	SM_ENTRY_MA(BE_AUTH, INITIALIZE, be_auth);

	abortAuth();
	sm->eap_if->eapNoReq = FALSE;
	sm->authAbort = FALSE;
}


SM_STATE(BE_AUTH, REQUEST)
{
	SM_ENTRY_MA(BE_AUTH, REQUEST, be_auth);

	txReq();
	sm->eap_if->eapReq = FALSE;
	sm->backendOtherRequestsToSupplicant++;

	/*
	 * Clearing eapolEap here is not specified in IEEE Std 802.1X-2004, but
	 * it looks like this would be logical thing to do there since the old
	 * EAP response would not be valid anymore after the new EAP request
	 * was sent out.
	 *
	 * A race condition has been reported, in which hostapd ended up
	 * sending out EAP-Response/Identity as a response to the first
	 * EAP-Request from the main EAP method. This can be avoided by
	 * clearing eapolEap here.
	 */
	sm->eapolEap = FALSE;
}


SM_STATE(BE_AUTH, RESPONSE)
{
	SM_ENTRY_MA(BE_AUTH, RESPONSE, be_auth);

	sm->authTimeout = FALSE;
	sm->eapolEap = FALSE;
	sm->eap_if->eapNoReq = FALSE;
	sm->aWhile = sm->serverTimeout;
	sm->eap_if->eapResp = TRUE;
	/* sendRespToServer(); */
	sm->backendResponses++;
}


SM_STATE(BE_AUTH, SUCCESS)
{
	SM_ENTRY_MA(BE_AUTH, SUCCESS, be_auth);

	txReq();
	sm->authSuccess = TRUE;
	sm->keyRun = TRUE;
}


SM_STATE(BE_AUTH, FAIL)
{
	SM_ENTRY_MA(BE_AUTH, FAIL, be_auth);

	txReq();
	sm->authFail = TRUE;
}


SM_STATE(BE_AUTH, TIMEOUT)
{
	SM_ENTRY_MA(BE_AUTH, TIMEOUT, be_auth);

	sm->authTimeout = TRUE;
}


SM_STATE(BE_AUTH, IDLE)
{
	SM_ENTRY_MA(BE_AUTH, IDLE, be_auth);

	sm->authStart = FALSE;
}


SM_STATE(BE_AUTH, IGNORE)
{
	SM_ENTRY_MA(BE_AUTH, IGNORE, be_auth);

	sm->eap_if->eapNoReq = FALSE;
}


SM_STEP(BE_AUTH)
{
	if (sm->portControl != Auto || sm->initialize || sm->authAbort) {
		SM_ENTER_GLOBAL(BE_AUTH, INITIALIZE);
		return;
	}

	switch (sm->be_auth_state) {
	case BE_AUTH_INITIALIZE:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_REQUEST:
		if (sm->eapolEap)
			SM_ENTER(BE_AUTH, RESPONSE);
		else if (sm->eap_if->eapReq)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->eap_if->eapTimeout)
			SM_ENTER(BE_AUTH, TIMEOUT);
		break;
	case BE_AUTH_RESPONSE:
		if (sm->eap_if->eapNoReq)
			SM_ENTER(BE_AUTH, IGNORE);
		if (sm->eap_if->eapReq) {
			sm->backendAccessChallenges++;
			SM_ENTER(BE_AUTH, REQUEST);
		} else if (sm->aWhile == 0)
			SM_ENTER(BE_AUTH, TIMEOUT);
		else if (sm->eap_if->eapFail) {
			sm->backendAuthFails++;
			SM_ENTER(BE_AUTH, FAIL);
		} else if (sm->eap_if->eapSuccess) {
			sm->backendAuthSuccesses++;
			SM_ENTER(BE_AUTH, SUCCESS);
		}
		break;
	case BE_AUTH_SUCCESS:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_FAIL:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_TIMEOUT:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_IDLE:
		if (sm->eap_if->eapFail && sm->authStart)
			SM_ENTER(BE_AUTH, FAIL);
		else if (sm->eap_if->eapReq && sm->authStart)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->eap_if->eapSuccess && sm->authStart)
			SM_ENTER(BE_AUTH, SUCCESS);
		break;
	case BE_AUTH_IGNORE:
		if (sm->eapolEap)
			SM_ENTER(BE_AUTH, RESPONSE);
		else if (sm->eap_if->eapReq)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->eap_if->eapTimeout)
			SM_ENTER(BE_AUTH, TIMEOUT);
		break;
	}
}



/* Reauthentication Timer state machine */

SM_STATE(REAUTH_TIMER, INITIALIZE)
{
	SM_ENTRY_MA(REAUTH_TIMER, INITIALIZE, reauth_timer);

	sm->reAuthWhen = sm->reAuthPeriod;
}


SM_STATE(REAUTH_TIMER, REAUTHENTICATE)
{
	SM_ENTRY_MA(REAUTH_TIMER, REAUTHENTICATE, reauth_timer);

	sm->reAuthenticate = TRUE;
	wpa_auth_sm_event(sm->sta->wpa_sm, WPA_REAUTH_EAPOL);
}


SM_STEP(REAUTH_TIMER)
{
	if (sm->portControl != Auto || sm->initialize ||
	    sm->authPortStatus == Unauthorized || !sm->reAuthEnabled) {
		SM_ENTER_GLOBAL(REAUTH_TIMER, INITIALIZE);
		return;
	}

	switch (sm->reauth_timer_state) {
	case REAUTH_TIMER_INITIALIZE:
		if (sm->reAuthWhen == 0)
			SM_ENTER(REAUTH_TIMER, REAUTHENTICATE);
		break;
	case REAUTH_TIMER_REAUTHENTICATE:
		SM_ENTER(REAUTH_TIMER, INITIALIZE);
		break;
	}
}



/* Authenticator Key Transmit state machine */

SM_STATE(AUTH_KEY_TX, NO_KEY_TRANSMIT)
{
	SM_ENTRY_MA(AUTH_KEY_TX, NO_KEY_TRANSMIT, auth_key_tx);
}


SM_STATE(AUTH_KEY_TX, KEY_TRANSMIT)
{
	SM_ENTRY_MA(AUTH_KEY_TX, KEY_TRANSMIT, auth_key_tx);

	txKey();
	sm->eap_if->eapKeyAvailable = FALSE;
	sm->keyDone = TRUE;
}


SM_STEP(AUTH_KEY_TX)
{
	if (sm->initialize || sm->portControl != Auto) {
		SM_ENTER_GLOBAL(AUTH_KEY_TX, NO_KEY_TRANSMIT);
		return;
	}

	switch (sm->auth_key_tx_state) {
	case AUTH_KEY_TX_NO_KEY_TRANSMIT:
		if (sm->keyTxEnabled && sm->eap_if->eapKeyAvailable &&
		    sm->keyRun && !wpa_auth_sta_wpa_version(sm->sta->wpa_sm))
			SM_ENTER(AUTH_KEY_TX, KEY_TRANSMIT);
		break;
	case AUTH_KEY_TX_KEY_TRANSMIT:
		if (!sm->keyTxEnabled || !sm->keyRun)
			SM_ENTER(AUTH_KEY_TX, NO_KEY_TRANSMIT);
		else if (sm->eap_if->eapKeyAvailable)
			SM_ENTER(AUTH_KEY_TX, KEY_TRANSMIT);
		break;
	}
}



/* Key Receive state machine */

SM_STATE(KEY_RX, NO_KEY_RECEIVE)
{
	SM_ENTRY_MA(KEY_RX, NO_KEY_RECEIVE, key_rx);
}


SM_STATE(KEY_RX, KEY_RECEIVE)
{
	SM_ENTRY_MA(KEY_RX, KEY_RECEIVE, key_rx);

	processKey();
	sm->rxKey = FALSE;
}


SM_STEP(KEY_RX)
{
	if (sm->initialize || !sm->eap_if->portEnabled) {
		SM_ENTER_GLOBAL(KEY_RX, NO_KEY_RECEIVE);
		return;
	}

	switch (sm->key_rx_state) {
	case KEY_RX_NO_KEY_RECEIVE:
		if (sm->rxKey)
			SM_ENTER(KEY_RX, KEY_RECEIVE);
		break;
	case KEY_RX_KEY_RECEIVE:
		if (sm->rxKey)
			SM_ENTER(KEY_RX, KEY_RECEIVE);
		break;
	}
}



/* Controlled Directions state machine */

SM_STATE(CTRL_DIR, FORCE_BOTH)
{
	SM_ENTRY_MA(CTRL_DIR, FORCE_BOTH, ctrl_dir);
	sm->operControlledDirections = Both;
}


SM_STATE(CTRL_DIR, IN_OR_BOTH)
{
	SM_ENTRY_MA(CTRL_DIR, IN_OR_BOTH, ctrl_dir);
	sm->operControlledDirections = sm->adminControlledDirections;
}


SM_STEP(CTRL_DIR)
{
	if (sm->initialize) {
		SM_ENTER_GLOBAL(CTRL_DIR, IN_OR_BOTH);
		return;
	}

	switch (sm->ctrl_dir_state) {
	case CTRL_DIR_FORCE_BOTH:
		if (sm->eap_if->portEnabled && sm->operEdge)
			SM_ENTER(CTRL_DIR, IN_OR_BOTH);
		break;
	case CTRL_DIR_IN_OR_BOTH:
		if (sm->operControlledDirections !=
		    sm->adminControlledDirections)
			SM_ENTER(CTRL_DIR, IN_OR_BOTH);
		if (!sm->eap_if->portEnabled || !sm->operEdge)
			SM_ENTER(CTRL_DIR, FORCE_BOTH);
		break;
	}
}



struct eapol_state_machine *
eapol_auth_alloc(struct eapol_authenticator *eapol, const u8 *addr,
		 int preauth, struct sta_info *sta)
{
	struct eapol_state_machine *sm;
	struct hostapd_data *hapd; /* TODO: to be removed */
	struct eap_config eap_conf;

	if (eapol == NULL)
		return NULL;
	hapd = eapol->conf.hapd;

	sm = os_zalloc(sizeof(*sm));
	if (sm == NULL) {
		wpa_printf(MSG_DEBUG, "IEEE 802.1X state machine allocation "
			   "failed");
		return NULL;
	}
	sm->radius_identifier = -1;
	os_memcpy(sm->addr, addr, ETH_ALEN);
	if (preauth)
		sm->flags |= EAPOL_SM_PREAUTH;

	sm->hapd = hapd;
	sm->eapol = eapol;
	sm->sta = sta;

	/* Set default values for state machine constants */
	sm->auth_pae_state = AUTH_PAE_INITIALIZE;
	sm->quietPeriod = AUTH_PAE_DEFAULT_quietPeriod;
	sm->reAuthMax = AUTH_PAE_DEFAULT_reAuthMax;

	sm->be_auth_state = BE_AUTH_INITIALIZE;
	sm->serverTimeout = BE_AUTH_DEFAULT_serverTimeout;

	sm->reauth_timer_state = REAUTH_TIMER_INITIALIZE;
	sm->reAuthPeriod = eapol->conf.eap_reauth_period;
	sm->reAuthEnabled = eapol->conf.eap_reauth_period > 0 ? TRUE : FALSE;

	sm->auth_key_tx_state = AUTH_KEY_TX_NO_KEY_TRANSMIT;

	sm->key_rx_state = KEY_RX_NO_KEY_RECEIVE;

	sm->ctrl_dir_state = CTRL_DIR_IN_OR_BOTH;

	sm->portControl = Auto;

	if (!eapol->conf.wpa &&
	    (hapd->default_wep_key || eapol->conf.individual_wep_key_len > 0))
		sm->keyTxEnabled = TRUE;
	else
		sm->keyTxEnabled = FALSE;
	if (eapol->conf.wpa)
		sm->portValid = FALSE;
	else
		sm->portValid = TRUE;

	os_memset(&eap_conf, 0, sizeof(eap_conf));
	eap_conf.eap_server = eapol->conf.eap_server;
	eap_conf.ssl_ctx = eapol->conf.ssl_ctx;
	eap_conf.eap_sim_db_priv = eapol->conf.eap_sim_db_priv;
	eap_conf.pac_opaque_encr_key = eapol->conf.pac_opaque_encr_key;
	eap_conf.eap_fast_a_id = eapol->conf.eap_fast_a_id;
	eap_conf.eap_fast_a_id_len = eapol->conf.eap_fast_a_id_len;
	eap_conf.eap_fast_a_id_info = eapol->conf.eap_fast_a_id_info;
	eap_conf.eap_fast_prov = eapol->conf.eap_fast_prov;
	eap_conf.pac_key_lifetime = eapol->conf.pac_key_lifetime;
	eap_conf.pac_key_refresh_time = eapol->conf.pac_key_refresh_time;
	eap_conf.eap_sim_aka_result_ind = eapol->conf.eap_sim_aka_result_ind;
	eap_conf.tnc = eapol->conf.tnc;
	eap_conf.wps = eapol->conf.wps;
	eap_conf.assoc_wps_ie = sta->wps_ie;
	sm->eap = eap_server_sm_init(sm, &eapol_cb, &eap_conf);
	if (sm->eap == NULL) {
		eapol_auth_free(sm);
		return NULL;
	}
	sm->eap_if = eap_get_interface(sm->eap);

	eapol_auth_initialize(sm);

	return sm;
}


void eapol_auth_free(struct eapol_state_machine *sm)
{
	if (sm == NULL)
		return;

	eloop_cancel_timeout(eapol_port_timers_tick, NULL, sm);
	eloop_cancel_timeout(eapol_sm_step_cb, sm, NULL);
	if (sm->eap)
		eap_server_sm_deinit(sm->eap);
	os_free(sm);
}


static int eapol_sm_sta_entry_alive(struct eapol_authenticator *eapol,
				    const u8 *addr)
{
	return eapol->cb.sta_entry_alive(eapol->conf.hapd, addr);
}


static void eapol_sm_step_run(struct eapol_state_machine *sm)
{
	struct eapol_authenticator *eapol = sm->eapol;
	u8 addr[ETH_ALEN];
	unsigned int prev_auth_pae, prev_be_auth, prev_reauth_timer,
		prev_auth_key_tx, prev_key_rx, prev_ctrl_dir;
	int max_steps = 100;

	os_memcpy(addr, sm->addr, ETH_ALEN);

	/*
	 * Allow EAPOL state machines to run as long as there are state
	 * changes, but exit and return here through event loop if more than
	 * 100 steps is needed as a precaution against infinite loops inside
	 * eloop callback.
	 */
restart:
	prev_auth_pae = sm->auth_pae_state;
	prev_be_auth = sm->be_auth_state;
	prev_reauth_timer = sm->reauth_timer_state;
	prev_auth_key_tx = sm->auth_key_tx_state;
	prev_key_rx = sm->key_rx_state;
	prev_ctrl_dir = sm->ctrl_dir_state;

	SM_STEP_RUN(AUTH_PAE);
	if (sm->initializing || eapol_sm_sta_entry_alive(eapol, addr))
		SM_STEP_RUN(BE_AUTH);
	if (sm->initializing || eapol_sm_sta_entry_alive(eapol, addr))
		SM_STEP_RUN(REAUTH_TIMER);
	if (sm->initializing || eapol_sm_sta_entry_alive(eapol, addr))
		SM_STEP_RUN(AUTH_KEY_TX);
	if (sm->initializing || eapol_sm_sta_entry_alive(eapol, addr))
		SM_STEP_RUN(KEY_RX);
	if (sm->initializing || eapol_sm_sta_entry_alive(eapol, addr))
		SM_STEP_RUN(CTRL_DIR);

	if (prev_auth_pae != sm->auth_pae_state ||
	    prev_be_auth != sm->be_auth_state ||
	    prev_reauth_timer != sm->reauth_timer_state ||
	    prev_auth_key_tx != sm->auth_key_tx_state ||
	    prev_key_rx != sm->key_rx_state ||
	    prev_ctrl_dir != sm->ctrl_dir_state) {
		if (--max_steps > 0)
			goto restart;
		/* Re-run from eloop timeout */
		eapol_auth_step(sm);
		return;
	}

	if (eapol_sm_sta_entry_alive(eapol, addr) && sm->eap) {
		if (eap_server_sm_step(sm->eap)) {
			if (--max_steps > 0)
				goto restart;
			/* Re-run from eloop timeout */
			eapol_auth_step(sm);
			return;
		}

		/* TODO: find a better location for this */
		if (sm->eap_if->aaaEapResp) {
			sm->eap_if->aaaEapResp = FALSE;
			if (sm->eap_if->aaaEapRespData == NULL) {
				wpa_printf(MSG_DEBUG, "EAPOL: aaaEapResp set, "
					   "but no aaaEapRespData available");
				return;
			}
			sm->eapol->cb.aaa_send(
				sm->hapd, sm->sta,
				wpabuf_head(sm->eap_if->aaaEapRespData),
				wpabuf_len(sm->eap_if->aaaEapRespData));
		}
	}

	if (eapol_sm_sta_entry_alive(eapol, addr))
		wpa_auth_sm_notify(sm->sta->wpa_sm);
}


static void eapol_sm_step_cb(void *eloop_ctx, void *timeout_ctx)
{
	struct eapol_state_machine *sm = eloop_ctx;
	eapol_sm_step_run(sm);
}


/**
 * eapol_auth_step - Advance EAPOL state machines
 * @sm: EAPOL state machine
 *
 * This function is called to advance EAPOL state machines after any change
 * that could affect their state.
 */
void eapol_auth_step(struct eapol_state_machine *sm)
{
	/*
	 * Run eapol_sm_step_run from a registered timeout to make sure that
	 * other possible timeouts/events are processed and to avoid long
	 * function call chains.
	 */

	eloop_register_timeout(0, 0, eapol_sm_step_cb, sm, NULL);
}


void eapol_auth_initialize(struct eapol_state_machine *sm)
{
	sm->initializing = TRUE;
	/* Initialize the state machines by asserting initialize and then
	 * deasserting it after one step */
	sm->initialize = TRUE;
	eapol_sm_step_run(sm);
	sm->initialize = FALSE;
	eapol_sm_step_run(sm);
	sm->initializing = FALSE;

	/* Start one second tick for port timers state machine */
	eloop_cancel_timeout(eapol_port_timers_tick, NULL, sm);
	eloop_register_timeout(1, 0, eapol_port_timers_tick, NULL, sm);
}


#ifdef HOSTAPD_DUMP_STATE
static inline const char * port_type_txt(PortTypes pt)
{
	switch (pt) {
	case ForceUnauthorized: return "ForceUnauthorized";
	case ForceAuthorized: return "ForceAuthorized";
	case Auto: return "Auto";
	default: return "Unknown";
	}
}


static inline const char * port_state_txt(PortState ps)
{
	switch (ps) {
	case Unauthorized: return "Unauthorized";
	case Authorized: return "Authorized";
	default: return "Unknown";
	}
}


static inline const char * ctrl_dir_txt(ControlledDirection dir)
{
	switch (dir) {
	case Both: return "Both";
	case In: return "In";
	default: return "Unknown";
	}
}


static inline const char * auth_pae_state_txt(int s)
{
	switch (s) {
	case AUTH_PAE_INITIALIZE: return "INITIALIZE";
	case AUTH_PAE_DISCONNECTED: return "DISCONNECTED";
	case AUTH_PAE_CONNECTING: return "CONNECTING";
	case AUTH_PAE_AUTHENTICATING: return "AUTHENTICATING";
	case AUTH_PAE_AUTHENTICATED: return "AUTHENTICATED";
	case AUTH_PAE_ABORTING: return "ABORTING";
	case AUTH_PAE_HELD: return "HELD";
	case AUTH_PAE_FORCE_AUTH: return "FORCE_AUTH";
	case AUTH_PAE_FORCE_UNAUTH: return "FORCE_UNAUTH";
	case AUTH_PAE_RESTART: return "RESTART";
	default: return "Unknown";
	}
}


static inline const char * be_auth_state_txt(int s)
{
	switch (s) {
	case BE_AUTH_REQUEST: return "REQUEST";
	case BE_AUTH_RESPONSE: return "RESPONSE";
	case BE_AUTH_SUCCESS: return "SUCCESS";
	case BE_AUTH_FAIL: return "FAIL";
	case BE_AUTH_TIMEOUT: return "TIMEOUT";
	case BE_AUTH_IDLE: return "IDLE";
	case BE_AUTH_INITIALIZE: return "INITIALIZE";
	case BE_AUTH_IGNORE: return "IGNORE";
	default: return "Unknown";
	}
}


static inline const char * reauth_timer_state_txt(int s)
{
	switch (s) {
	case REAUTH_TIMER_INITIALIZE: return "INITIALIZE";
	case REAUTH_TIMER_REAUTHENTICATE: return "REAUTHENTICATE";
	default: return "Unknown";
	}
}


static inline const char * auth_key_tx_state_txt(int s)
{
	switch (s) {
	case AUTH_KEY_TX_NO_KEY_TRANSMIT: return "NO_KEY_TRANSMIT";
	case AUTH_KEY_TX_KEY_TRANSMIT: return "KEY_TRANSMIT";
	default: return "Unknown";
	}
}


static inline const char * key_rx_state_txt(int s)
{
	switch (s) {
	case KEY_RX_NO_KEY_RECEIVE: return "NO_KEY_RECEIVE";
	case KEY_RX_KEY_RECEIVE: return "KEY_RECEIVE";
	default: return "Unknown";
	}
}


static inline const char * ctrl_dir_state_txt(int s)
{
	switch (s) {
	case CTRL_DIR_FORCE_BOTH: return "FORCE_BOTH";
	case CTRL_DIR_IN_OR_BOTH: return "IN_OR_BOTH";
	default: return "Unknown";
	}
}


void eapol_auth_dump_state(FILE *f, const char *prefix,
			   struct eapol_state_machine *sm)
{
	fprintf(f, "%sEAPOL state machine:\n", prefix);
	fprintf(f, "%s  aWhile=%d quietWhile=%d reAuthWhen=%d\n", prefix,
		sm->aWhile, sm->quietWhile, sm->reAuthWhen);
#define _SB(b) ((b) ? "TRUE" : "FALSE")
	fprintf(f,
		"%s  authAbort=%s authFail=%s authPortStatus=%s authStart=%s\n"
		"%s  authTimeout=%s authSuccess=%s eapFail=%s eapolEap=%s\n"
		"%s  eapSuccess=%s eapTimeout=%s initialize=%s "
		"keyAvailable=%s\n"
		"%s  keyDone=%s keyRun=%s keyTxEnabled=%s portControl=%s\n"
		"%s  portEnabled=%s portValid=%s reAuthenticate=%s\n",
		prefix, _SB(sm->authAbort), _SB(sm->authFail),
		port_state_txt(sm->authPortStatus), _SB(sm->authStart),
		prefix, _SB(sm->authTimeout), _SB(sm->authSuccess),
		_SB(sm->eap_if->eapFail), _SB(sm->eapolEap),
		prefix, _SB(sm->eap_if->eapSuccess),
		_SB(sm->eap_if->eapTimeout),
		_SB(sm->initialize), _SB(sm->eap_if->eapKeyAvailable),
		prefix, _SB(sm->keyDone), _SB(sm->keyRun),
		_SB(sm->keyTxEnabled), port_type_txt(sm->portControl),
		prefix, _SB(sm->eap_if->portEnabled), _SB(sm->portValid),
		_SB(sm->reAuthenticate));

	fprintf(f, "%s  Authenticator PAE:\n"
		"%s    state=%s\n"
		"%s    eapolLogoff=%s eapolStart=%s eapRestart=%s\n"
		"%s    portMode=%s reAuthCount=%d\n"
		"%s    quietPeriod=%d reAuthMax=%d\n"
		"%s    authEntersConnecting=%d\n"
		"%s    authEapLogoffsWhileConnecting=%d\n"
		"%s    authEntersAuthenticating=%d\n"
		"%s    authAuthSuccessesWhileAuthenticating=%d\n"
		"%s    authAuthTimeoutsWhileAuthenticating=%d\n"
		"%s    authAuthFailWhileAuthenticating=%d\n"
		"%s    authAuthEapStartsWhileAuthenticating=%d\n"
		"%s    authAuthEapLogoffWhileAuthenticating=%d\n"
		"%s    authAuthReauthsWhileAuthenticated=%d\n"
		"%s    authAuthEapStartsWhileAuthenticated=%d\n"
		"%s    authAuthEapLogoffWhileAuthenticated=%d\n",
		prefix, prefix, auth_pae_state_txt(sm->auth_pae_state), prefix,
		_SB(sm->eapolLogoff), _SB(sm->eapolStart),
		_SB(sm->eap_if->eapRestart),
		prefix, port_type_txt(sm->portMode), sm->reAuthCount,
		prefix, sm->quietPeriod, sm->reAuthMax,
		prefix, sm->authEntersConnecting,
		prefix, sm->authEapLogoffsWhileConnecting,
		prefix, sm->authEntersAuthenticating,
		prefix, sm->authAuthSuccessesWhileAuthenticating,
		prefix, sm->authAuthTimeoutsWhileAuthenticating,
		prefix, sm->authAuthFailWhileAuthenticating,
		prefix, sm->authAuthEapStartsWhileAuthenticating,
		prefix, sm->authAuthEapLogoffWhileAuthenticating,
		prefix, sm->authAuthReauthsWhileAuthenticated,
		prefix, sm->authAuthEapStartsWhileAuthenticated,
		prefix, sm->authAuthEapLogoffWhileAuthenticated);

	fprintf(f, "%s  Backend Authentication:\n"
		"%s    state=%s\n"
		"%s    eapNoReq=%s eapReq=%s eapResp=%s\n"
		"%s    serverTimeout=%d\n"
		"%s    backendResponses=%d\n"
		"%s    backendAccessChallenges=%d\n"
		"%s    backendOtherRequestsToSupplicant=%d\n"
		"%s    backendAuthSuccesses=%d\n"
		"%s    backendAuthFails=%d\n",
		prefix, prefix,
		be_auth_state_txt(sm->be_auth_state),
		prefix, _SB(sm->eap_if->eapNoReq), _SB(sm->eap_if->eapReq),
		_SB(sm->eap_if->eapResp),
		prefix, sm->serverTimeout,
		prefix, sm->backendResponses,
		prefix, sm->backendAccessChallenges,
		prefix, sm->backendOtherRequestsToSupplicant,
		prefix, sm->backendAuthSuccesses,
		prefix, sm->backendAuthFails);

	fprintf(f, "%s  Reauthentication Timer:\n"
		"%s    state=%s\n"
		"%s    reAuthPeriod=%d reAuthEnabled=%s\n", prefix, prefix,
		reauth_timer_state_txt(sm->reauth_timer_state), prefix,
		sm->reAuthPeriod, _SB(sm->reAuthEnabled));

	fprintf(f, "%s  Authenticator Key Transmit:\n"
		"%s    state=%s\n", prefix, prefix,
		auth_key_tx_state_txt(sm->auth_key_tx_state));

	fprintf(f, "%s  Key Receive:\n"
		"%s    state=%s\n"
		"%s    rxKey=%s\n", prefix, prefix,
		key_rx_state_txt(sm->key_rx_state), prefix, _SB(sm->rxKey));

	fprintf(f, "%s  Controlled Directions:\n"
		"%s    state=%s\n"
		"%s    adminControlledDirections=%s "
		"operControlledDirections=%s\n"
		"%s    operEdge=%s\n", prefix, prefix,
		ctrl_dir_state_txt(sm->ctrl_dir_state),
		prefix, ctrl_dir_txt(sm->adminControlledDirections),
		ctrl_dir_txt(sm->operControlledDirections),
		prefix, _SB(sm->operEdge));
#undef _SB
}
#endif /* HOSTAPD_DUMP_STATE */


static int eapol_sm_get_eap_user(void *ctx, const u8 *identity,
				 size_t identity_len, int phase2,
				 struct eap_user *user)
{
	struct eapol_state_machine *sm = ctx;
	return sm->eapol->cb.get_eap_user(sm->hapd, identity, identity_len,
					  phase2, user);
}


static const char * eapol_sm_get_eap_req_id_text(void *ctx, size_t *len)
{
	struct eapol_state_machine *sm = ctx;
	*len = sm->eapol->conf.eap_req_id_text_len;
	return sm->eapol->conf.eap_req_id_text;
}


static struct eapol_callbacks eapol_cb =
{
	.get_eap_user = eapol_sm_get_eap_user,
	.get_eap_req_id_text = eapol_sm_get_eap_req_id_text,
};


int eapol_auth_eap_pending_cb(struct eapol_state_machine *sm, void *ctx)
{
	if (sm == NULL || ctx != sm->eap)
		return -1;

	eap_sm_pending_cb(sm->eap);
	eapol_auth_step(sm);

	return 0;
}


static int eapol_auth_conf_clone(struct eapol_auth_config *dst,
				 struct eapol_auth_config *src)
{
	dst->hapd = src->hapd;
	dst->eap_reauth_period = src->eap_reauth_period;
	dst->wpa = src->wpa;
	dst->individual_wep_key_len = src->individual_wep_key_len;
	dst->eap_server = src->eap_server;
	dst->ssl_ctx = src->ssl_ctx;
	dst->eap_sim_db_priv = src->eap_sim_db_priv;
	os_free(dst->eap_req_id_text);
	if (src->eap_req_id_text) {
		dst->eap_req_id_text = os_malloc(src->eap_req_id_text_len);
		if (dst->eap_req_id_text == NULL)
			return -1;
		os_memcpy(dst->eap_req_id_text, src->eap_req_id_text,
			  src->eap_req_id_text_len);
		dst->eap_req_id_text_len = src->eap_req_id_text_len;
	} else {
		dst->eap_req_id_text = NULL;
		dst->eap_req_id_text_len = 0;
	}
	if (src->pac_opaque_encr_key) {
		dst->pac_opaque_encr_key = os_malloc(16);
		os_memcpy(dst->pac_opaque_encr_key, src->pac_opaque_encr_key,
			  16);
	} else
		dst->pac_opaque_encr_key = NULL;
	if (src->eap_fast_a_id) {
		dst->eap_fast_a_id = os_malloc(src->eap_fast_a_id_len);
		if (dst->eap_fast_a_id == NULL) {
			os_free(dst->eap_req_id_text);
			return -1;
		}
		os_memcpy(dst->eap_fast_a_id, src->eap_fast_a_id,
			  src->eap_fast_a_id_len);
		dst->eap_fast_a_id_len = src->eap_fast_a_id_len;
	} else
		dst->eap_fast_a_id = NULL;
	if (src->eap_fast_a_id_info) {
		dst->eap_fast_a_id_info = os_strdup(src->eap_fast_a_id_info);
		if (dst->eap_fast_a_id_info == NULL) {
			os_free(dst->eap_req_id_text);
			os_free(dst->eap_fast_a_id);
			return -1;
		}
	} else
		dst->eap_fast_a_id_info = NULL;
	dst->eap_fast_prov = src->eap_fast_prov;
	dst->pac_key_lifetime = src->pac_key_lifetime;
	dst->pac_key_refresh_time = src->pac_key_refresh_time;
	dst->eap_sim_aka_result_ind = src->eap_sim_aka_result_ind;
	dst->tnc = src->tnc;
	dst->wps = src->wps;
	return 0;
}


static void eapol_auth_conf_free(struct eapol_auth_config *conf)
{
	os_free(conf->eap_req_id_text);
	conf->eap_req_id_text = NULL;
	os_free(conf->pac_opaque_encr_key);
	conf->pac_opaque_encr_key = NULL;
	os_free(conf->eap_fast_a_id);
	conf->eap_fast_a_id = NULL;
	os_free(conf->eap_fast_a_id_info);
	conf->eap_fast_a_id_info = NULL;
}


struct eapol_authenticator * eapol_auth_init(struct eapol_auth_config *conf,
					     struct eapol_auth_cb *cb)
{
	struct eapol_authenticator *eapol;

	eapol = os_zalloc(sizeof(*eapol));
	if (eapol == NULL)
		return NULL;

	if (eapol_auth_conf_clone(&eapol->conf, conf) < 0) {
		os_free(eapol);
		return NULL;
	}

	eapol->cb.eapol_send = cb->eapol_send;
	eapol->cb.aaa_send = cb->aaa_send;
	eapol->cb.finished = cb->finished;
	eapol->cb.get_eap_user = cb->get_eap_user;
	eapol->cb.sta_entry_alive = cb->sta_entry_alive;
	eapol->cb.logger = cb->logger;
	eapol->cb.set_port_authorized = cb->set_port_authorized;
	eapol->cb.abort_auth = cb->abort_auth;
	eapol->cb.tx_key = cb->tx_key;

	return eapol;
}


void eapol_auth_deinit(struct eapol_authenticator *eapol)
{
	if (eapol == NULL)
		return;

	eapol_auth_conf_free(&eapol->conf);
	os_free(eapol);
}
