/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / IEEE 802.1X Authenticator - EAPOL state machine
 * Copyright (c) 2002-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include "ieee802_1x.h"
#include "eapol_sm.h"
#include "eloop.h"
#include "wpa.h"
#include "sta_info.h"
#include "eap.h"

static struct eapol_callbacks eapol_cb;

/* EAPOL state machines are described in IEEE Std 802.1X-REV-d11, Chap. 8.2 */

#define setPortAuthorized() \
ieee802_1x_set_sta_authorized(sm->hapd, sm->sta, 1)
#define setPortUnauthorized() \
ieee802_1x_set_sta_authorized(sm->hapd, sm->sta, 0)

/* procedures */
#define txCannedFail() ieee802_1x_tx_canned_eap(sm->hapd, sm->sta, 0)
#define txCannedSuccess() ieee802_1x_tx_canned_eap(sm->hapd, sm->sta, 1)
#define txReq() ieee802_1x_tx_req(sm->hapd, sm->sta)
#define sendRespToServer() ieee802_1x_send_resp_to_server(sm->hapd, sm->sta)
#define abortAuth() ieee802_1x_abort_auth(sm->hapd, sm->sta)
#define txKey() ieee802_1x_tx_key(sm->hapd, sm->sta)
#define processKey() do { } while (0)


/* Definitions for clarifying state machine implementation */
#define SM_STATE(machine, state) \
static void sm_ ## machine ## _ ## state ## _Enter(struct eapol_state_machine \
*sm)

#define SM_ENTRY(machine, _state, _data) \
sm->_data.state = machine ## _ ## _state; \
if (sm->hapd->conf->debug >= HOSTAPD_DEBUG_MINIMAL) \
	printf("IEEE 802.1X: " MACSTR " " #machine " entering state " #_state \
		"\n", MAC2STR(sm->addr));

#define SM_ENTER(machine, state) sm_ ## machine ## _ ## state ## _Enter(sm)

#define SM_STEP(machine) \
static void sm_ ## machine ## _Step(struct eapol_state_machine *sm)

#define SM_STEP_RUN(machine) sm_ ## machine ## _Step(sm)


static void eapol_sm_step_run(struct eapol_state_machine *sm);
static void eapol_sm_step_cb(void *eloop_ctx, void *timeout_ctx);


/* Port Timers state machine - implemented as a function that will be called
 * once a second as a registered event loop timeout */

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

	eapol_sm_step_run(state);

	eloop_register_timeout(1, 0, eapol_port_timers_tick, eloop_ctx, state);
}



/* Authenticator PAE state machine */

SM_STATE(AUTH_PAE, INITIALIZE)
{
	SM_ENTRY(AUTH_PAE, INITIALIZE, auth_pae);
	sm->auth_pae.portMode = Auto;

	sm->currentId = 255;
}


SM_STATE(AUTH_PAE, DISCONNECTED)
{
	int from_initialize = sm->auth_pae.state == AUTH_PAE_INITIALIZE;

	if (sm->auth_pae.eapolLogoff) {
		if (sm->auth_pae.state == AUTH_PAE_CONNECTING)
			sm->auth_pae.authEapLogoffsWhileConnecting++;
		else if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATED)
			sm->auth_pae.authAuthEapLogoffWhileAuthenticated++;
	}

	SM_ENTRY(AUTH_PAE, DISCONNECTED, auth_pae);

	sm->authPortStatus = Unauthorized;
	setPortUnauthorized();
	sm->auth_pae.reAuthCount = 0;
	sm->auth_pae.eapolLogoff = FALSE;
	if (!from_initialize) {
		if (sm->flags & EAPOL_SM_PREAUTH)
			rsn_preauth_finished(sm->hapd, sm->sta, 0);
		else
			ieee802_1x_finished(sm->hapd, sm->sta, 0);
	}
}


SM_STATE(AUTH_PAE, RESTART)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATED) {
		if (sm->reAuthenticate)
			sm->auth_pae.authAuthReauthsWhileAuthenticated++;
		if (sm->auth_pae.eapolStart)
			sm->auth_pae.authAuthEapStartsWhileAuthenticated++;
		if (sm->auth_pae.eapolLogoff)
			sm->auth_pae.authAuthEapLogoffWhileAuthenticated++;
	}

	SM_ENTRY(AUTH_PAE, RESTART, auth_pae);

	sm->auth_pae.eapRestart = TRUE;
	ieee802_1x_request_identity(sm->hapd, sm->sta);
}


SM_STATE(AUTH_PAE, CONNECTING)
{
	if (sm->auth_pae.state != AUTH_PAE_CONNECTING)
		sm->auth_pae.authEntersConnecting++;

	SM_ENTRY(AUTH_PAE, CONNECTING, auth_pae);

	sm->reAuthenticate = FALSE;
	sm->auth_pae.reAuthCount++;
}


SM_STATE(AUTH_PAE, HELD)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATING && sm->authFail)
		sm->auth_pae.authAuthFailWhileAuthenticating++;

	SM_ENTRY(AUTH_PAE, HELD, auth_pae);

	sm->authPortStatus = Unauthorized;
	setPortUnauthorized();
	sm->quietWhile = sm->auth_pae.quietPeriod;
	sm->auth_pae.eapolLogoff = FALSE;

	hostapd_logger(sm->hapd, sm->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_WARNING, "authentication failed");
	if (sm->flags & EAPOL_SM_PREAUTH)
		rsn_preauth_finished(sm->hapd, sm->sta, 0);
	else
		ieee802_1x_finished(sm->hapd, sm->sta, 0);
}


SM_STATE(AUTH_PAE, AUTHENTICATED)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATING && sm->authSuccess)
		sm->auth_pae.authAuthSuccessesWhileAuthenticating++;
							
	SM_ENTRY(AUTH_PAE, AUTHENTICATED, auth_pae);

	sm->authPortStatus = Authorized;
	setPortAuthorized();
	sm->auth_pae.reAuthCount = 0;
	hostapd_logger(sm->hapd, sm->addr, HOSTAPD_MODULE_IEEE8021X,
		       HOSTAPD_LEVEL_INFO, "authenticated");
	if (sm->flags & EAPOL_SM_PREAUTH)
		rsn_preauth_finished(sm->hapd, sm->sta, 1);
	else
		ieee802_1x_finished(sm->hapd, sm->sta, 1);
}


SM_STATE(AUTH_PAE, AUTHENTICATING)
{
	if (sm->auth_pae.state == AUTH_PAE_CONNECTING && sm->rx_identity) {
		sm->auth_pae.authEntersAuthenticating++;
		sm->rx_identity = FALSE;
	}

	SM_ENTRY(AUTH_PAE, AUTHENTICATING, auth_pae);

	sm->auth_pae.eapolStart = FALSE;
	sm->authSuccess = FALSE;
	sm->authFail = FALSE;
	sm->authTimeout = FALSE;
	sm->authStart = TRUE;
	sm->keyRun = FALSE;
	sm->keyDone = FALSE;
}


SM_STATE(AUTH_PAE, ABORTING)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATING) {
		if (sm->authTimeout)
			sm->auth_pae.authAuthTimeoutsWhileAuthenticating++;
		if (sm->auth_pae.eapolStart)
			sm->auth_pae.authAuthEapStartsWhileAuthenticating++;
		if (sm->auth_pae.eapolLogoff)
			sm->auth_pae.authAuthEapLogoffWhileAuthenticating++;
	}

	SM_ENTRY(AUTH_PAE, ABORTING, auth_pae);

	sm->authAbort = TRUE;
	sm->keyRun = FALSE;
	sm->keyDone = FALSE;
}


SM_STATE(AUTH_PAE, FORCE_AUTH)
{
	SM_ENTRY(AUTH_PAE, FORCE_AUTH, auth_pae);

	sm->authPortStatus = Authorized;
	setPortAuthorized();
	sm->auth_pae.portMode = ForceAuthorized;
	sm->auth_pae.eapolStart = FALSE;
	txCannedSuccess();
}


SM_STATE(AUTH_PAE, FORCE_UNAUTH)
{
	SM_ENTRY(AUTH_PAE, FORCE_UNAUTH, auth_pae);

	sm->authPortStatus = Unauthorized;
	setPortUnauthorized();
	sm->auth_pae.portMode = ForceUnauthorized;
	sm->auth_pae.eapolStart = FALSE;
	txCannedFail();
}


SM_STEP(AUTH_PAE)
{
	if ((sm->portControl == Auto &&
	     sm->auth_pae.portMode != sm->portControl) ||
	    sm->initialize || !sm->portEnabled)
		SM_ENTER(AUTH_PAE, INITIALIZE);
	else if (sm->portControl == ForceAuthorized &&
		 sm->auth_pae.portMode != sm->portControl &&
		 !(sm->initialize || !sm->portEnabled))
		SM_ENTER(AUTH_PAE, FORCE_AUTH);
	else if (sm->portControl == ForceUnauthorized &&
		 sm->auth_pae.portMode != sm->portControl &&
		 !(sm->initialize || !sm->portEnabled))
		SM_ENTER(AUTH_PAE, FORCE_UNAUTH);
	else {
		switch (sm->auth_pae.state) {
		case AUTH_PAE_INITIALIZE:
			SM_ENTER(AUTH_PAE, DISCONNECTED);
			break;
		case AUTH_PAE_DISCONNECTED:
			SM_ENTER(AUTH_PAE, RESTART);
			break;
		case AUTH_PAE_RESTART:
			if (!sm->auth_pae.eapRestart)
				SM_ENTER(AUTH_PAE, CONNECTING);
			break;
		case AUTH_PAE_HELD:
			if (sm->quietWhile == 0)
				SM_ENTER(AUTH_PAE, RESTART);
			break;
		case AUTH_PAE_CONNECTING:
			if (sm->auth_pae.eapolLogoff ||
			    sm->auth_pae.reAuthCount > sm->auth_pae.reAuthMax)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			else if ((sm->be_auth.eapReq &&
				  sm->auth_pae.reAuthCount <=
				  sm->auth_pae.reAuthMax) ||
				 sm->eapSuccess || sm->eapFail)
				SM_ENTER(AUTH_PAE, AUTHENTICATING);
			break;
		case AUTH_PAE_AUTHENTICATED:
			if (sm->auth_pae.eapolStart || sm->reAuthenticate)
				SM_ENTER(AUTH_PAE, RESTART);
			else if (sm->auth_pae.eapolLogoff || !sm->portValid)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			break;
		case AUTH_PAE_AUTHENTICATING:
			if (sm->authSuccess && sm->portValid)
				SM_ENTER(AUTH_PAE, AUTHENTICATED);
			else if (sm->authFail ||
				 (sm->keyDone && !sm->portValid))
				SM_ENTER(AUTH_PAE, HELD);
			else if (sm->auth_pae.eapolStart ||
				 sm->auth_pae.eapolLogoff || sm->authTimeout)
				SM_ENTER(AUTH_PAE, ABORTING);
			break;
		case AUTH_PAE_ABORTING:
			if (sm->auth_pae.eapolLogoff && !sm->authAbort)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			else if (!sm->auth_pae.eapolLogoff && !sm->authAbort)
				SM_ENTER(AUTH_PAE, RESTART);
			break;
		case AUTH_PAE_FORCE_AUTH:
			if (sm->auth_pae.eapolStart)
				SM_ENTER(AUTH_PAE, FORCE_AUTH);
			break;
		case AUTH_PAE_FORCE_UNAUTH:
			if (sm->auth_pae.eapolStart)
				SM_ENTER(AUTH_PAE, FORCE_UNAUTH);
			break;
		}
	}
}



/* Backend Authentication state machine */

SM_STATE(BE_AUTH, INITIALIZE)
{
	SM_ENTRY(BE_AUTH, INITIALIZE, be_auth);

	abortAuth();
	sm->be_auth.eapNoReq = FALSE;
	sm->authAbort = FALSE;
}


SM_STATE(BE_AUTH, REQUEST)
{
	SM_ENTRY(BE_AUTH, REQUEST, be_auth);

	txReq();
	sm->be_auth.eapReq = FALSE;
	sm->be_auth.backendOtherRequestsToSupplicant++;

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
	SM_ENTRY(BE_AUTH, RESPONSE, be_auth);

	sm->authTimeout = FALSE;
	sm->eapolEap = FALSE;
	sm->be_auth.eapNoReq = FALSE;
	sm->aWhile = sm->be_auth.serverTimeout;
	sm->be_auth.eapResp = TRUE;
	sendRespToServer();
	sm->be_auth.backendResponses++;
}


SM_STATE(BE_AUTH, SUCCESS)
{
	SM_ENTRY(BE_AUTH, SUCCESS, be_auth);

	txReq();
	sm->authSuccess = TRUE;
	sm->keyRun = TRUE;
}


SM_STATE(BE_AUTH, FAIL)
{
	SM_ENTRY(BE_AUTH, FAIL, be_auth);

	/* Note: IEEE 802.1X-REV-d11 has unconditional txReq() here.
	 * txCannelFail() is used as a workaround for the case where
	 * authentication server does not include EAP-Message with
	 * Access-Reject. */
	if (sm->last_eap_radius == NULL)
		txCannedFail();
	else
		txReq();
	sm->authFail = TRUE;
}


SM_STATE(BE_AUTH, TIMEOUT)
{
	SM_ENTRY(BE_AUTH, TIMEOUT, be_auth);

	sm->authTimeout = TRUE;
}


SM_STATE(BE_AUTH, IDLE)
{
	SM_ENTRY(BE_AUTH, IDLE, be_auth);

	sm->authStart = FALSE;
}


SM_STATE(BE_AUTH, IGNORE)
{
	SM_ENTRY(BE_AUTH, IGNORE, be_auth);

	sm->be_auth.eapNoReq = FALSE;
}


SM_STEP(BE_AUTH)
{
	if (sm->portControl != Auto || sm->initialize || sm->authAbort) {
		SM_ENTER(BE_AUTH, INITIALIZE);
		return;
	}

	switch (sm->be_auth.state) {
	case BE_AUTH_INITIALIZE:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_REQUEST:
		if (sm->eapolEap)
			SM_ENTER(BE_AUTH, RESPONSE);
		else if (sm->be_auth.eapReq)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->eapTimeout)
			SM_ENTER(BE_AUTH, TIMEOUT);
		break;
	case BE_AUTH_RESPONSE:
		if (sm->be_auth.eapNoReq)
			SM_ENTER(BE_AUTH, IGNORE);
		if (sm->be_auth.eapReq) {
			sm->be_auth.backendAccessChallenges++;
			SM_ENTER(BE_AUTH, REQUEST);
		} else if (sm->aWhile == 0)
			SM_ENTER(BE_AUTH, TIMEOUT);
		else if (sm->eapFail) {
			sm->be_auth.backendAuthFails++;
			SM_ENTER(BE_AUTH, FAIL);
		} else if (sm->eapSuccess) {
			sm->be_auth.backendAuthSuccesses++;
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
		if (sm->eapFail && sm->authStart)
			SM_ENTER(BE_AUTH, FAIL);
		else if (sm->be_auth.eapReq && sm->authStart)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->eapSuccess && sm->authStart)
			SM_ENTER(BE_AUTH, SUCCESS);
		break;
	case BE_AUTH_IGNORE:
		if (sm->eapolEap)
			SM_ENTER(BE_AUTH, RESPONSE);
		else if (sm->be_auth.eapReq)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->eapTimeout)
			SM_ENTER(BE_AUTH, TIMEOUT);
		break;
	}
}



/* Reauthentication Timer state machine */

SM_STATE(REAUTH_TIMER, INITIALIZE)
{
	SM_ENTRY(REAUTH_TIMER, INITIALIZE, reauth_timer);

	sm->reAuthWhen = sm->reauth_timer.reAuthPeriod;
}


SM_STATE(REAUTH_TIMER, REAUTHENTICATE)
{
	SM_ENTRY(REAUTH_TIMER, REAUTHENTICATE, reauth_timer);

	sm->reAuthenticate = TRUE;
	wpa_sm_event(sm->hapd, sm->sta, WPA_REAUTH_EAPOL);
}


SM_STEP(REAUTH_TIMER)
{
	if (sm->portControl != Auto || sm->initialize ||
	    sm->authPortStatus == Unauthorized ||
	    !sm->reauth_timer.reAuthEnabled) {
		SM_ENTER(REAUTH_TIMER, INITIALIZE);
		return;
	}

	switch (sm->reauth_timer.state) {
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
	SM_ENTRY(AUTH_KEY_TX, NO_KEY_TRANSMIT, auth_key_tx);
}


SM_STATE(AUTH_KEY_TX, KEY_TRANSMIT)
{
	SM_ENTRY(AUTH_KEY_TX, KEY_TRANSMIT, auth_key_tx);

	txKey();
	sm->keyAvailable = FALSE;
	sm->keyDone = TRUE;
}


SM_STEP(AUTH_KEY_TX)
{
	if (sm->initialize || sm->portControl != Auto) {
		SM_ENTER(AUTH_KEY_TX, NO_KEY_TRANSMIT);
		return;
	}

	switch (sm->auth_key_tx.state) {
	case AUTH_KEY_TX_NO_KEY_TRANSMIT:
		if (sm->keyTxEnabled && sm->keyAvailable && sm->keyRun &&
		    !sm->sta->wpa)
			SM_ENTER(AUTH_KEY_TX, KEY_TRANSMIT);
		break;
	case AUTH_KEY_TX_KEY_TRANSMIT:
		if (!sm->keyTxEnabled || !sm->keyRun)
			SM_ENTER(AUTH_KEY_TX, NO_KEY_TRANSMIT);
		else if (sm->keyAvailable)
			SM_ENTER(AUTH_KEY_TX, KEY_TRANSMIT);
		break;
	}
}



/* Key Receive state machine */

SM_STATE(KEY_RX, NO_KEY_RECEIVE)
{
	SM_ENTRY(KEY_RX, NO_KEY_RECEIVE, key_rx);
}


SM_STATE(KEY_RX, KEY_RECEIVE)
{
	SM_ENTRY(KEY_RX, KEY_RECEIVE, key_rx);

	processKey();
	sm->key_rx.rxKey = FALSE;
}


SM_STEP(KEY_RX)
{
	if (sm->initialize || !sm->portEnabled) {
		SM_ENTER(KEY_RX, NO_KEY_RECEIVE);
		return;
	}

	switch (sm->key_rx.state) {
	case KEY_RX_NO_KEY_RECEIVE:
		if (sm->key_rx.rxKey)
			SM_ENTER(KEY_RX, KEY_RECEIVE);
		break;
	case KEY_RX_KEY_RECEIVE:
		if (sm->key_rx.rxKey)
			SM_ENTER(KEY_RX, KEY_RECEIVE);
		break;
	}
}



/* Controlled Directions state machine */

SM_STATE(CTRL_DIR, FORCE_BOTH)
{
	SM_ENTRY(CTRL_DIR, FORCE_BOTH, ctrl_dir);
	sm->ctrl_dir.operControlledDirections = Both;
}


SM_STATE(CTRL_DIR, IN_OR_BOTH)
{
	SM_ENTRY(CTRL_DIR, IN_OR_BOTH, ctrl_dir);
	sm->ctrl_dir.operControlledDirections =
		sm->ctrl_dir.adminControlledDirections;
}


SM_STEP(CTRL_DIR)
{
	if (sm->initialize) {
		SM_ENTER(CTRL_DIR, IN_OR_BOTH);
		return;
	}

	switch (sm->ctrl_dir.state) {
	case CTRL_DIR_FORCE_BOTH:
		if (sm->portEnabled && sm->ctrl_dir.operEdge)
			SM_ENTER(CTRL_DIR, IN_OR_BOTH);
		break;
	case CTRL_DIR_IN_OR_BOTH:
		if (sm->ctrl_dir.operControlledDirections !=
		    sm->ctrl_dir.adminControlledDirections)
			SM_ENTER(CTRL_DIR, IN_OR_BOTH);
		if (!sm->portEnabled || !sm->ctrl_dir.operEdge)
			SM_ENTER(CTRL_DIR, FORCE_BOTH);
		break;
	}
}



struct eapol_state_machine *
eapol_sm_alloc(hostapd *hapd, struct sta_info *sta)
{
	struct eapol_state_machine *sm;

	sm = (struct eapol_state_machine *) malloc(sizeof(*sm));
	if (sm == NULL) {
		printf("IEEE 802.1X port state allocation failed\n");
		return NULL;
	}
	memset(sm, 0, sizeof(*sm));
	sm->radius_identifier = -1;
	memcpy(sm->addr, sta->addr, ETH_ALEN);
	if (sta->flags & WLAN_STA_PREAUTH)
		sm->flags |= EAPOL_SM_PREAUTH;

	sm->hapd = hapd;
	sm->sta = sta;

	/* Set default values for state machine constants */
	sm->auth_pae.state = AUTH_PAE_INITIALIZE;
	sm->auth_pae.quietPeriod = AUTH_PAE_DEFAULT_quietPeriod;
	sm->auth_pae.reAuthMax = AUTH_PAE_DEFAULT_reAuthMax;

	sm->be_auth.state = BE_AUTH_INITIALIZE;
	sm->be_auth.serverTimeout = BE_AUTH_DEFAULT_serverTimeout;

	sm->reauth_timer.state = REAUTH_TIMER_INITIALIZE;
	sm->reauth_timer.reAuthPeriod = hapd->conf->eap_reauth_period;
	sm->reauth_timer.reAuthEnabled = hapd->conf->eap_reauth_period > 0 ?
		TRUE : FALSE;

	sm->auth_key_tx.state = AUTH_KEY_TX_NO_KEY_TRANSMIT;

	sm->key_rx.state = KEY_RX_NO_KEY_RECEIVE;

	sm->ctrl_dir.state = CTRL_DIR_IN_OR_BOTH;

	sm->portEnabled = FALSE;
	sm->portControl = Auto;

	sm->keyAvailable = FALSE;
	if (!hapd->conf->wpa &&
	    (hapd->default_wep_key || hapd->conf->individual_wep_key_len > 0))
		sm->keyTxEnabled = TRUE;
	else
		sm->keyTxEnabled = FALSE;
	if (hapd->conf->wpa)
		sm->portValid = FALSE;
	else
		sm->portValid = TRUE;

	if (hapd->conf->eap_server) {
		struct eap_config eap_conf;
		memset(&eap_conf, 0, sizeof(eap_conf));
		eap_conf.ssl_ctx = hapd->ssl_ctx;
		eap_conf.eap_sim_db_priv = hapd->eap_sim_db_priv;
		sm->eap = eap_sm_init(sm, &eapol_cb, &eap_conf);
		if (sm->eap == NULL) {
			eapol_sm_free(sm);
			return NULL;
		}
	}

	eapol_sm_initialize(sm);

	return sm;
}


void eapol_sm_free(struct eapol_state_machine *sm)
{
	if (sm == NULL)
		return;

	eloop_cancel_timeout(eapol_port_timers_tick, sm->hapd, sm);
	eloop_cancel_timeout(eapol_sm_step_cb, sm, NULL);
	if (sm->eap)
		eap_sm_deinit(sm->eap);
	free(sm);
}


static int eapol_sm_sta_entry_alive(struct hostapd_data *hapd, u8 *addr)
{
	struct sta_info *sta;
	sta = ap_get_sta(hapd, addr);
	if (sta == NULL || sta->eapol_sm == NULL)
		return 0;
	return 1;
}


static void eapol_sm_step_run(struct eapol_state_machine *sm)
{
	struct hostapd_data *hapd = sm->hapd;
	u8 addr[ETH_ALEN];
	int prev_auth_pae, prev_be_auth, prev_reauth_timer, prev_auth_key_tx,
		prev_key_rx, prev_ctrl_dir;
	int max_steps = 100;

	memcpy(addr, sm->sta->addr, ETH_ALEN);

	/*
	 * Allow EAPOL state machines to run as long as there are state
	 * changes, but exit and return here through event loop if more than
	 * 100 steps is needed as a precaution against infinite loops inside
	 * eloop callback.
	 */
restart:
	prev_auth_pae = sm->auth_pae.state;
	prev_be_auth = sm->be_auth.state;
	prev_reauth_timer = sm->reauth_timer.state;
	prev_auth_key_tx = sm->auth_key_tx.state;
	prev_key_rx = sm->key_rx.state;
	prev_ctrl_dir = sm->ctrl_dir.state;

	SM_STEP_RUN(AUTH_PAE);
	if (sm->initializing || eapol_sm_sta_entry_alive(hapd, addr))
		SM_STEP_RUN(BE_AUTH);
	if (sm->initializing || eapol_sm_sta_entry_alive(hapd, addr))
		SM_STEP_RUN(REAUTH_TIMER);
	if (sm->initializing || eapol_sm_sta_entry_alive(hapd, addr))
		SM_STEP_RUN(AUTH_KEY_TX);
	if (sm->initializing || eapol_sm_sta_entry_alive(hapd, addr))
		SM_STEP_RUN(KEY_RX);
	if (sm->initializing || eapol_sm_sta_entry_alive(hapd, addr))
		SM_STEP_RUN(CTRL_DIR);

	if (prev_auth_pae != sm->auth_pae.state ||
	    prev_be_auth != sm->be_auth.state ||
	    prev_reauth_timer != sm->reauth_timer.state ||
	    prev_auth_key_tx != sm->auth_key_tx.state ||
	    prev_key_rx != sm->key_rx.state ||
	    prev_ctrl_dir != sm->ctrl_dir.state) {
		if (--max_steps > 0)
			goto restart;
		/* Re-run from eloop timeout */
		eapol_sm_step(sm);
		return;
	}

	if (eapol_sm_sta_entry_alive(hapd, addr) && sm->eap) {
		if (eap_sm_step(sm->eap)) {
			if (--max_steps > 0)
				goto restart;
			/* Re-run from eloop timeout */
			eapol_sm_step(sm);
			return;
		}
	}

	if (eapol_sm_sta_entry_alive(hapd, addr))
		wpa_sm_notify(sm->hapd, sm->sta);
}


static void eapol_sm_step_cb(void *eloop_ctx, void *timeout_ctx)
{
	struct eapol_state_machine *sm = eloop_ctx;
	eapol_sm_step_run(sm);
}


void eapol_sm_step(struct eapol_state_machine *sm)
{
	/*
	 * Run eapol_sm_step_run from a registered timeout to make sure that
	 * other possible timeouts/events are processed and to avoid long
	 * function call chains.
	 */

	eloop_register_timeout(0, 0, eapol_sm_step_cb, sm, NULL);
}


void eapol_sm_initialize(struct eapol_state_machine *sm)
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
	eloop_cancel_timeout(eapol_port_timers_tick, sm->hapd, sm);
	eloop_register_timeout(1, 0, eapol_port_timers_tick, sm->hapd, sm);
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


void eapol_sm_dump_state(FILE *f, const char *prefix,
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
		_SB(sm->eapFail), _SB(sm->eapolEap),
		prefix, _SB(sm->eapSuccess), _SB(sm->eapTimeout),
		_SB(sm->initialize), _SB(sm->keyAvailable),
		prefix, _SB(sm->keyDone), _SB(sm->keyRun),
		_SB(sm->keyTxEnabled), port_type_txt(sm->portControl),
		prefix, _SB(sm->portEnabled), _SB(sm->portValid),
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
		prefix, prefix, auth_pae_state_txt(sm->auth_pae.state), prefix,
		_SB(sm->auth_pae.eapolLogoff), _SB(sm->auth_pae.eapolStart),
		_SB(sm->auth_pae.eapRestart), prefix,
		port_type_txt(sm->auth_pae.portMode), sm->auth_pae.reAuthCount,
		prefix, sm->auth_pae.quietPeriod, sm->auth_pae.reAuthMax,
		prefix, sm->auth_pae.authEntersConnecting,
		prefix, sm->auth_pae.authEapLogoffsWhileConnecting,
		prefix, sm->auth_pae.authEntersAuthenticating,
		prefix, sm->auth_pae.authAuthSuccessesWhileAuthenticating,
		prefix, sm->auth_pae.authAuthTimeoutsWhileAuthenticating,
		prefix, sm->auth_pae.authAuthFailWhileAuthenticating,
		prefix, sm->auth_pae.authAuthEapStartsWhileAuthenticating,
		prefix, sm->auth_pae.authAuthEapLogoffWhileAuthenticating,
		prefix, sm->auth_pae.authAuthReauthsWhileAuthenticated,
		prefix, sm->auth_pae.authAuthEapStartsWhileAuthenticated,
		prefix, sm->auth_pae.authAuthEapLogoffWhileAuthenticated);

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
		be_auth_state_txt(sm->be_auth.state),
		prefix, _SB(sm->be_auth.eapNoReq), _SB(sm->be_auth.eapReq),
		_SB(sm->be_auth.eapResp),
		prefix, sm->be_auth.serverTimeout,
		prefix, sm->be_auth.backendResponses,
		prefix, sm->be_auth.backendAccessChallenges,
		prefix, sm->be_auth.backendOtherRequestsToSupplicant,
		prefix, sm->be_auth.backendAuthSuccesses,
		prefix, sm->be_auth.backendAuthFails);

	fprintf(f, "%s  Reauthentication Timer:\n"
		"%s    state=%s\n"
		"%s    reAuthPeriod=%d reAuthEnabled=%s\n", prefix, prefix,
		reauth_timer_state_txt(sm->reauth_timer.state), prefix,
		sm->reauth_timer.reAuthPeriod,
		_SB(sm->reauth_timer.reAuthEnabled));

	fprintf(f, "%s  Authenticator Key Transmit:\n"
		"%s    state=%s\n", prefix, prefix,
		auth_key_tx_state_txt(sm->auth_key_tx.state));

	fprintf(f, "%s  Key Receive:\n"
		"%s    state=%s\n"
		"%s    rxKey=%s\n", prefix, prefix,
		key_rx_state_txt(sm->key_rx.state),
		prefix, _SB(sm->key_rx.rxKey));

	fprintf(f, "%s  Controlled Directions:\n"
		"%s    state=%s\n"
		"%s    adminControlledDirections=%s "
		"operControlledDirections=%s\n"
		"%s    operEdge=%s\n", prefix, prefix,
		ctrl_dir_state_txt(sm->ctrl_dir.state),
		prefix, ctrl_dir_txt(sm->ctrl_dir.adminControlledDirections),
		ctrl_dir_txt(sm->ctrl_dir.operControlledDirections),
		prefix, _SB(sm->ctrl_dir.operEdge));
#undef _SB
}
#endif /* HOSTAPD_DUMP_STATE */


static Boolean eapol_sm_get_bool(void *ctx, enum eapol_bool_var variable)
{
	struct eapol_state_machine *sm = ctx;
	if (sm == NULL)
		return FALSE;
	switch (variable) {
	case EAPOL_eapSuccess:
		return sm->eapSuccess;
	case EAPOL_eapRestart:
		return sm->auth_pae.eapRestart;
	case EAPOL_eapFail:
		return sm->eapFail;
	case EAPOL_eapResp:
		return sm->be_auth.eapResp;
	case EAPOL_eapReq:
		return sm->be_auth.eapReq;
	case EAPOL_eapNoReq:
		return sm->be_auth.eapNoReq;
	case EAPOL_portEnabled:
		return sm->portEnabled;
	case EAPOL_eapTimeout:
		return sm->eapTimeout;
	}
	return FALSE;
}


static void eapol_sm_set_bool(void *ctx, enum eapol_bool_var variable,
			      Boolean value)
{
	struct eapol_state_machine *sm = ctx;
	if (sm == NULL)
		return;
	switch (variable) {
	case EAPOL_eapSuccess:
		sm->eapSuccess = value;
		break;
	case EAPOL_eapRestart:
		sm->auth_pae.eapRestart = value;
		break;
	case EAPOL_eapFail:
		sm->eapFail = value;
		break;
	case EAPOL_eapResp:
		sm->be_auth.eapResp = value;
		break;
	case EAPOL_eapReq:
		sm->be_auth.eapReq = value;
		break;
	case EAPOL_eapNoReq:
		sm->be_auth.eapNoReq = value;
		break;
	case EAPOL_portEnabled:
		sm->portEnabled = value;
		break;
	case EAPOL_eapTimeout:
		sm->eapTimeout = value;
		break;
	}
}


static void eapol_sm_set_eapReqData(void *ctx, const u8 *eapReqData,
				    size_t eapReqDataLen)
{
	struct eapol_state_machine *sm = ctx;
	if (sm == NULL)
		return;

	free(sm->last_eap_radius);
	sm->last_eap_radius = malloc(eapReqDataLen);
	if (sm->last_eap_radius == NULL)
		return;
	memcpy(sm->last_eap_radius, eapReqData, eapReqDataLen);
	sm->last_eap_radius_len = eapReqDataLen;
}


static void eapol_sm_set_eapKeyData(void *ctx, const u8 *eapKeyData,
				    size_t eapKeyDataLen)
{
	struct eapol_state_machine *sm = ctx;
	struct hostapd_data *hapd;

	if (sm == NULL)
		return;

	hapd = sm->hapd;

	if (eapKeyData && eapKeyDataLen >= 64) {
		free(sm->eapol_key_sign);
		free(sm->eapol_key_crypt);
		sm->eapol_key_crypt = malloc(32);
		if (sm->eapol_key_crypt) {
			memcpy(sm->eapol_key_crypt, eapKeyData, 32);
			sm->eapol_key_crypt_len = 32;
		}
		sm->eapol_key_sign = malloc(32);
		if (sm->eapol_key_sign) {
			memcpy(sm->eapol_key_sign, eapKeyData + 32, 32);
			sm->eapol_key_sign_len = 32;
		}
		if (hapd->default_wep_key ||
		    hapd->conf->individual_wep_key_len > 0 ||
		    hapd->conf->wpa)
			sm->keyAvailable = TRUE;
	} else {
		free(sm->eapol_key_sign);
		free(sm->eapol_key_crypt);
		sm->eapol_key_sign = NULL;
		sm->eapol_key_crypt = NULL;
		sm->eapol_key_sign_len = 0;
		sm->eapol_key_crypt_len = 0;
		sm->keyAvailable = FALSE;
	}
}


static int eapol_sm_get_eap_user(void *ctx, const u8 *identity,
				 size_t identity_len, int phase2,
				 struct eap_user *user)
{
	struct eapol_state_machine *sm = ctx;
	const struct hostapd_eap_user *eap_user;

	eap_user = hostapd_get_eap_user(sm->hapd->conf, identity,
					identity_len, phase2);
	if (eap_user == NULL)
		return -1;

	memset(user, 0, sizeof(*user));
	user->phase2 = phase2;
	memcpy(user->methods, eap_user->methods,
	       EAP_USER_MAX_METHODS > EAP_MAX_METHODS ?
	       EAP_USER_MAX_METHODS : EAP_MAX_METHODS);

	if (eap_user->password) {
		user->password = malloc(eap_user->password_len);
		if (user->password == NULL)
			return -1;
		memcpy(user->password, eap_user->password,
		       eap_user->password_len);
		user->password_len = eap_user->password_len;
	}
	user->force_version = eap_user->force_version;

	return 0;
}


static const char * eapol_sm_get_eap_req_id_text(void *ctx, size_t *len)
{
	struct eapol_state_machine *sm = ctx;
	*len = sm->hapd->conf->eap_req_id_text_len;
	return sm->hapd->conf->eap_req_id_text;
}


static struct eapol_callbacks eapol_cb =
{
	.get_bool = eapol_sm_get_bool,
	.set_bool = eapol_sm_set_bool,
	.set_eapReqData = eapol_sm_set_eapReqData,
	.set_eapKeyData = eapol_sm_set_eapKeyData,
	.get_eap_user = eapol_sm_get_eap_user,
	.get_eap_req_id_text = eapol_sm_get_eap_req_id_text,
};
