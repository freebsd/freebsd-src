/*
 * wpa_supplicant - DPP
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/ip_addr.h"
#include "utils/base64.h"
#include "common/dpp.h"
#include "common/gas.h"
#include "common/gas_server.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/pmksa_cache.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "driver_i.h"
#include "offchannel.h"
#include "gas_query.h"
#include "bss.h"
#include "scan.h"
#include "notify.h"
#include "dpp_supplicant.h"


static int wpas_dpp_listen_start(struct wpa_supplicant *wpa_s,
				 unsigned int freq);
static void wpas_dpp_reply_wait_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpas_dpp_auth_conf_wait_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpas_dpp_auth_success(struct wpa_supplicant *wpa_s, int initiator);
static void wpas_dpp_tx_status(struct wpa_supplicant *wpa_s,
			       unsigned int freq, const u8 *dst,
			       const u8 *src, const u8 *bssid,
			       const u8 *data, size_t data_len,
			       enum offchannel_send_action_result result);
static void wpas_dpp_init_timeout(void *eloop_ctx, void *timeout_ctx);
static int wpas_dpp_auth_init_next(struct wpa_supplicant *wpa_s);
static void
wpas_dpp_tx_pkex_status(struct wpa_supplicant *wpa_s,
			unsigned int freq, const u8 *dst,
			const u8 *src, const u8 *bssid,
			const u8 *data, size_t data_len,
			enum offchannel_send_action_result result);
#ifdef CONFIG_DPP2
static void wpas_dpp_reconfig_reply_wait_timeout(void *eloop_ctx,
						 void *timeout_ctx);
static void wpas_dpp_start_gas_client(struct wpa_supplicant *wpa_s);
static int wpas_dpp_process_conf_obj(void *ctx,
				     struct dpp_authentication *auth);
#endif /* CONFIG_DPP2 */

static const u8 broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Use a hardcoded Transaction ID 1 in Peer Discovery frames since there is only
 * a single transaction in progress at any point in time. */
static const u8 TRANSACTION_ID = 1;


/**
 * wpas_dpp_qr_code - Parse and add DPP bootstrapping info from a QR Code
 * @wpa_s: Pointer to wpa_supplicant data
 * @cmd: DPP URI read from a QR Code
 * Returns: Identifier of the stored info or -1 on failure
 */
int wpas_dpp_qr_code(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct dpp_bootstrap_info *bi;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	bi = dpp_add_qr_code(wpa_s->dpp, cmd);
	if (!bi)
		return -1;

	if (auth && auth->response_pending &&
	    dpp_notify_new_qr_code(auth, bi) == 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Sending out pending authentication response");
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
			" freq=%u type=%d",
			MAC2STR(auth->peer_mac_addr), auth->curr_freq,
			DPP_PA_AUTHENTICATION_RESP);
		offchannel_send_action(wpa_s, auth->curr_freq,
				       auth->peer_mac_addr, wpa_s->own_addr,
				       broadcast,
				       wpabuf_head(auth->resp_msg),
				       wpabuf_len(auth->resp_msg),
				       500, wpas_dpp_tx_status, 0);
	}

#ifdef CONFIG_DPP2
	dpp_controller_new_qr_code(wpa_s->dpp, bi);
#endif /* CONFIG_DPP2 */

	return bi->id;
}


/**
 * wpas_dpp_nfc_uri - Parse and add DPP bootstrapping info from NFC Tag (URI)
 * @wpa_s: Pointer to wpa_supplicant data
 * @cmd: DPP URI read from a NFC Tag (URI NDEF message)
 * Returns: Identifier of the stored info or -1 on failure
 */
int wpas_dpp_nfc_uri(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct dpp_bootstrap_info *bi;

	bi = dpp_add_nfc_uri(wpa_s->dpp, cmd);
	if (!bi)
		return -1;

	return bi->id;
}


int wpas_dpp_nfc_handover_req(struct wpa_supplicant *wpa_s, const char *cmd)
{
	const char *pos;
	struct dpp_bootstrap_info *peer_bi, *own_bi;

	pos = os_strstr(cmd, " own=");
	if (!pos)
		return -1;
	pos += 5;
	own_bi = dpp_bootstrap_get_id(wpa_s->dpp, atoi(pos));
	if (!own_bi)
		return -1;
	own_bi->nfc_negotiated = 1;

	pos = os_strstr(cmd, " uri=");
	if (!pos)
		return -1;
	pos += 5;
	peer_bi = dpp_add_nfc_uri(wpa_s->dpp, pos);
	if (!peer_bi) {
		wpa_printf(MSG_INFO,
			   "DPP: Failed to parse URI from NFC Handover Request");
		return -1;
	}

	if (dpp_nfc_update_bi(own_bi, peer_bi) < 0)
		return -1;

	return peer_bi->id;
}


int wpas_dpp_nfc_handover_sel(struct wpa_supplicant *wpa_s, const char *cmd)
{
	const char *pos;
	struct dpp_bootstrap_info *peer_bi, *own_bi;

	pos = os_strstr(cmd, " own=");
	if (!pos)
		return -1;
	pos += 5;
	own_bi = dpp_bootstrap_get_id(wpa_s->dpp, atoi(pos));
	if (!own_bi)
		return -1;
	own_bi->nfc_negotiated = 1;

	pos = os_strstr(cmd, " uri=");
	if (!pos)
		return -1;
	pos += 5;
	peer_bi = dpp_add_nfc_uri(wpa_s->dpp, pos);
	if (!peer_bi) {
		wpa_printf(MSG_INFO,
			   "DPP: Failed to parse URI from NFC Handover Select");
		return -1;
	}

	if (peer_bi->curve != own_bi->curve) {
		wpa_printf(MSG_INFO,
			   "DPP: Peer (NFC Handover Selector) used different curve");
		return -1;
	}

	return peer_bi->id;
}


static void wpas_dpp_auth_resp_retry_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth || !auth->resp_msg)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Retry Authentication Response after timeout");
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d",
		MAC2STR(auth->peer_mac_addr), auth->curr_freq,
		DPP_PA_AUTHENTICATION_RESP);
	offchannel_send_action(wpa_s, auth->curr_freq, auth->peer_mac_addr,
			       wpa_s->own_addr, broadcast,
			       wpabuf_head(auth->resp_msg),
			       wpabuf_len(auth->resp_msg),
			       500, wpas_dpp_tx_status, 0);
}


static void wpas_dpp_auth_resp_retry(struct wpa_supplicant *wpa_s)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	unsigned int wait_time, max_tries;

	if (!auth || !auth->resp_msg)
		return;

	if (wpa_s->dpp_resp_max_tries)
		max_tries = wpa_s->dpp_resp_max_tries;
	else
		max_tries = 5;
	auth->auth_resp_tries++;
	if (auth->auth_resp_tries >= max_tries) {
		wpa_printf(MSG_INFO, "DPP: No confirm received from initiator - stopping exchange");
		offchannel_send_action_done(wpa_s);
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
		return;
	}

	if (wpa_s->dpp_resp_retry_time)
		wait_time = wpa_s->dpp_resp_retry_time;
	else
		wait_time = 1000;
	wpa_printf(MSG_DEBUG,
		   "DPP: Schedule retransmission of Authentication Response frame in %u ms",
		wait_time);
	eloop_cancel_timeout(wpas_dpp_auth_resp_retry_timeout, wpa_s, NULL);
	eloop_register_timeout(wait_time / 1000,
			       (wait_time % 1000) * 1000,
			       wpas_dpp_auth_resp_retry_timeout, wpa_s, NULL);
}


static void wpas_dpp_try_to_connect(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "DPP: Trying to connect to the new network");
	wpa_s->suitable_network = 0;
	wpa_s->no_suitable_network = 0;
	wpa_s->disconnected = 0;
	wpa_s->reassociate = 1;
	wpa_s->scan_runs = 0;
	wpa_s->normal_scans = 0;
	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


#ifdef CONFIG_DPP2

static void wpas_dpp_stop_listen_for_tx(struct wpa_supplicant *wpa_s,
					unsigned int freq,
					unsigned int wait_time)
{
	struct os_reltime now, res;
	unsigned int remaining;

	if (!wpa_s->dpp_listen_freq)
		return;

	os_get_reltime(&now);
	if (os_reltime_before(&now, &wpa_s->dpp_listen_end)) {
		os_reltime_sub(&wpa_s->dpp_listen_end, &now, &res);
		remaining = res.sec * 1000 + res.usec / 1000;
	} else {
		remaining = 0;
	}
	if (wpa_s->dpp_listen_freq == freq && remaining > wait_time)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Stop listen on %u MHz ending in %u ms to allow immediate TX on %u MHz for %u ms",
		   wpa_s->dpp_listen_freq, remaining, freq, wait_time);
	wpas_dpp_listen_stop(wpa_s);

	/* TODO: Restart listen in some cases after TX? */
}


static void wpas_dpp_conn_status_result_timeout(void *eloop_ctx,
						void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	enum dpp_status_error result;

	if (!auth || !auth->conn_status_requested)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Connection timeout - report Connection Status Result");
	if (wpa_s->suitable_network)
		result = DPP_STATUS_AUTH_FAILURE;
	else if (wpa_s->no_suitable_network)
		result = DPP_STATUS_NO_AP;
	else
		result = 255; /* What to report here for unexpected state? */
	if (wpa_s->wpa_state == WPA_SCANNING)
		wpas_abort_ongoing_scan(wpa_s);
	wpas_dpp_send_conn_status_result(wpa_s, result);
}


static char * wpas_dpp_scan_channel_list(struct wpa_supplicant *wpa_s)
{
	char *str, *end, *pos;
	size_t len;
	unsigned int i;
	u8 last_op_class = 0;
	int res;

	if (!wpa_s->last_scan_freqs || !wpa_s->num_last_scan_freqs)
		return NULL;

	len = wpa_s->num_last_scan_freqs * 8;
	str = os_zalloc(len);
	if (!str)
		return NULL;
	end = str + len;
	pos = str;

	for (i = 0; i < wpa_s->num_last_scan_freqs; i++) {
		enum hostapd_hw_mode mode;
		u8 op_class, channel;

		mode = ieee80211_freq_to_channel_ext(wpa_s->last_scan_freqs[i],
						     0, 0, &op_class, &channel);
		if (mode == NUM_HOSTAPD_MODES)
			continue;
		if (op_class == last_op_class)
			res = os_snprintf(pos, end - pos, ",%d", channel);
		else
			res = os_snprintf(pos, end - pos, "%s%d/%d",
					  pos == str ? "" : ",",
					  op_class, channel);
		if (os_snprintf_error(end - pos, res)) {
			*pos = '\0';
			break;
		}
		pos += res;
		last_op_class = op_class;
	}

	if (pos == str) {
		os_free(str);
		str = NULL;
	}
	return str;
}


void wpas_dpp_send_conn_status_result(struct wpa_supplicant *wpa_s,
				      enum dpp_status_error result)
{
	struct wpabuf *msg;
	const char *channel_list = NULL;
	char *channel_list_buf = NULL;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	eloop_cancel_timeout(wpas_dpp_conn_status_result_timeout, wpa_s, NULL);

	if (!auth || !auth->conn_status_requested)
		return;
	auth->conn_status_requested = 0;
	wpa_printf(MSG_DEBUG, "DPP: Report connection status result %d",
		   result);

	if (result == DPP_STATUS_NO_AP) {
		channel_list_buf = wpas_dpp_scan_channel_list(wpa_s);
		channel_list = channel_list_buf;
	}

	msg = dpp_build_conn_status_result(auth, result,
					   ssid ? ssid->ssid :
					   wpa_s->dpp_last_ssid,
					   ssid ? ssid->ssid_len :
					   wpa_s->dpp_last_ssid_len,
					   channel_list);
	os_free(channel_list_buf);
	if (!msg) {
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
		return;
	}

	wpa_msg(wpa_s, MSG_INFO,
		DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(auth->peer_mac_addr), auth->curr_freq,
		DPP_PA_CONNECTION_STATUS_RESULT);
	offchannel_send_action(wpa_s, auth->curr_freq,
			       auth->peer_mac_addr, wpa_s->own_addr, broadcast,
			       wpabuf_head(msg), wpabuf_len(msg),
			       500, wpas_dpp_tx_status, 0);
	wpabuf_free(msg);

	/* This exchange will be terminated in the TX status handler */
	auth->remove_on_tx_status = 1;

	return;
}


void wpas_dpp_connected(struct wpa_supplicant *wpa_s)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (auth && auth->conn_status_requested)
		wpas_dpp_send_conn_status_result(wpa_s, DPP_STATUS_OK);
}

#endif /* CONFIG_DPP2 */


static void wpas_dpp_tx_status(struct wpa_supplicant *wpa_s,
			       unsigned int freq, const u8 *dst,
			       const u8 *src, const u8 *bssid,
			       const u8 *data, size_t data_len,
			       enum offchannel_send_action_result result)
{
	const char *res_txt;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	res_txt = result == OFFCHANNEL_SEND_ACTION_SUCCESS ? "SUCCESS" :
		(result == OFFCHANNEL_SEND_ACTION_NO_ACK ? "no-ACK" :
		 "FAILED");
	wpa_printf(MSG_DEBUG, "DPP: TX status: freq=%u dst=" MACSTR
		   " result=%s", freq, MAC2STR(dst), res_txt);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX_STATUS "dst=" MACSTR
		" freq=%u result=%s", MAC2STR(dst), freq, res_txt);

	if (!wpa_s->dpp_auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore TX status since there is no ongoing authentication exchange");
		return;
	}

#ifdef CONFIG_DPP2
	if (auth->connect_on_tx_status) {
		auth->connect_on_tx_status = 0;
		wpa_printf(MSG_DEBUG,
			   "DPP: Try to connect after completed configuration result");
		wpas_dpp_try_to_connect(wpa_s);
		if (auth->conn_status_requested) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Start 15 second timeout for reporting connection status result");
			eloop_cancel_timeout(
				wpas_dpp_conn_status_result_timeout,
				wpa_s, NULL);
			eloop_register_timeout(
				15, 0, wpas_dpp_conn_status_result_timeout,
				wpa_s, NULL);
		} else {
			dpp_auth_deinit(wpa_s->dpp_auth);
			wpa_s->dpp_auth = NULL;
		}
		return;
	}
#endif /* CONFIG_DPP2 */

	if (wpa_s->dpp_auth->remove_on_tx_status) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Terminate authentication exchange due to a request to do so on TX status");
		eloop_cancel_timeout(wpas_dpp_init_timeout, wpa_s, NULL);
		eloop_cancel_timeout(wpas_dpp_reply_wait_timeout, wpa_s, NULL);
		eloop_cancel_timeout(wpas_dpp_auth_conf_wait_timeout, wpa_s,
				     NULL);
		eloop_cancel_timeout(wpas_dpp_auth_resp_retry_timeout, wpa_s,
				     NULL);
#ifdef CONFIG_DPP2
		eloop_cancel_timeout(wpas_dpp_reconfig_reply_wait_timeout,
				     wpa_s, NULL);
#endif /* CONFIG_DPP2 */
		offchannel_send_action_done(wpa_s);
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
		return;
	}

	if (wpa_s->dpp_auth_ok_on_ack)
		wpas_dpp_auth_success(wpa_s, 1);

	if (!is_broadcast_ether_addr(dst) &&
	    result != OFFCHANNEL_SEND_ACTION_SUCCESS) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unicast DPP Action frame was not ACKed");
		if (auth->waiting_auth_resp) {
			/* In case of DPP Authentication Request frame, move to
			 * the next channel immediately. */
			offchannel_send_action_done(wpa_s);
			wpas_dpp_auth_init_next(wpa_s);
			return;
		}
		if (auth->waiting_auth_conf) {
			wpas_dpp_auth_resp_retry(wpa_s);
			return;
		}
	}

	if (auth->waiting_auth_conf &&
	    auth->auth_resp_status == DPP_STATUS_OK) {
		/* Make sure we do not get stuck waiting for Auth Confirm
		 * indefinitely after successfully transmitted Auth Response to
		 * allow new authentication exchanges to be started. */
		eloop_cancel_timeout(wpas_dpp_auth_conf_wait_timeout, wpa_s,
				     NULL);
		eloop_register_timeout(1, 0, wpas_dpp_auth_conf_wait_timeout,
				       wpa_s, NULL);
	}

	if (!is_broadcast_ether_addr(dst) && auth->waiting_auth_resp &&
	    result == OFFCHANNEL_SEND_ACTION_SUCCESS) {
		/* Allow timeout handling to stop iteration if no response is
		 * received from a peer that has ACKed a request. */
		auth->auth_req_ack = 1;
	}

	if (!wpa_s->dpp_auth_ok_on_ack && wpa_s->dpp_auth->neg_freq > 0 &&
	    wpa_s->dpp_auth->curr_freq != wpa_s->dpp_auth->neg_freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Move from curr_freq %u MHz to neg_freq %u MHz for response",
			   wpa_s->dpp_auth->curr_freq,
			   wpa_s->dpp_auth->neg_freq);
		offchannel_send_action_done(wpa_s);
		wpas_dpp_listen_start(wpa_s, wpa_s->dpp_auth->neg_freq);
	}

	if (wpa_s->dpp_auth_ok_on_ack)
		wpa_s->dpp_auth_ok_on_ack = 0;
}


static void wpas_dpp_reply_wait_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	unsigned int freq;
	struct os_reltime now, diff;
	unsigned int wait_time, diff_ms;

	if (!auth || !auth->waiting_auth_resp)
		return;

	wait_time = wpa_s->dpp_resp_wait_time ?
		wpa_s->dpp_resp_wait_time : 2000;
	os_get_reltime(&now);
	os_reltime_sub(&now, &wpa_s->dpp_last_init, &diff);
	diff_ms = diff.sec * 1000 + diff.usec / 1000;
	wpa_printf(MSG_DEBUG,
		   "DPP: Reply wait timeout - wait_time=%u diff_ms=%u",
		   wait_time, diff_ms);

	if (auth->auth_req_ack && diff_ms >= wait_time) {
		/* Peer ACK'ed Authentication Request frame, but did not reply
		 * with Authentication Response frame within two seconds. */
		wpa_printf(MSG_INFO,
			   "DPP: No response received from responder - stopping initiation attempt");
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_AUTH_INIT_FAILED);
		offchannel_send_action_done(wpa_s);
		wpas_dpp_listen_stop(wpa_s);
		dpp_auth_deinit(auth);
		wpa_s->dpp_auth = NULL;
		return;
	}

	if (diff_ms >= wait_time) {
		/* Authentication Request frame was not ACK'ed and no reply
		 * was receiving within two seconds. */
		wpa_printf(MSG_DEBUG,
			   "DPP: Continue Initiator channel iteration");
		offchannel_send_action_done(wpa_s);
		wpas_dpp_listen_stop(wpa_s);
		wpas_dpp_auth_init_next(wpa_s);
		return;
	}

	/* Driver did not support 2000 ms long wait_time with TX command, so
	 * schedule listen operation to continue waiting for the response.
	 *
	 * DPP listen operations continue until stopped, so simply schedule a
	 * new call to this function at the point when the two second reply
	 * wait has expired. */
	wait_time -= diff_ms;

	freq = auth->curr_freq;
	if (auth->neg_freq > 0)
		freq = auth->neg_freq;
	wpa_printf(MSG_DEBUG,
		   "DPP: Continue reply wait on channel %u MHz for %u ms",
		   freq, wait_time);
	wpa_s->dpp_in_response_listen = 1;
	wpas_dpp_listen_start(wpa_s, freq);

	eloop_register_timeout(wait_time / 1000, (wait_time % 1000) * 1000,
			       wpas_dpp_reply_wait_timeout, wpa_s, NULL);
}


static void wpas_dpp_auth_conf_wait_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth || !auth->waiting_auth_conf)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Terminate authentication exchange due to Auth Confirm timeout");
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL "No Auth Confirm received");
	offchannel_send_action_done(wpa_s);
	dpp_auth_deinit(auth);
	wpa_s->dpp_auth = NULL;
}


static void wpas_dpp_set_testing_options(struct wpa_supplicant *wpa_s,
					 struct dpp_authentication *auth)
{
#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->dpp_config_obj_override)
		auth->config_obj_override =
			os_strdup(wpa_s->dpp_config_obj_override);
	if (wpa_s->dpp_discovery_override)
		auth->discovery_override =
			os_strdup(wpa_s->dpp_discovery_override);
	if (wpa_s->dpp_groups_override)
		auth->groups_override =
			os_strdup(wpa_s->dpp_groups_override);
	auth->ignore_netaccesskey_mismatch =
		wpa_s->dpp_ignore_netaccesskey_mismatch;
#endif /* CONFIG_TESTING_OPTIONS */
}


static void wpas_dpp_init_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (!wpa_s->dpp_auth)
		return;
	wpa_printf(MSG_DEBUG, "DPP: Retry initiation after timeout");
	wpas_dpp_auth_init_next(wpa_s);
}


static int wpas_dpp_auth_init_next(struct wpa_supplicant *wpa_s)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	const u8 *dst;
	unsigned int wait_time, max_wait_time, freq, max_tries, used;
	struct os_reltime now, diff;

	wpa_s->dpp_in_response_listen = 0;
	if (!auth)
		return -1;

	if (auth->freq_idx == 0)
		os_get_reltime(&wpa_s->dpp_init_iter_start);

	if (auth->freq_idx >= auth->num_freq) {
		auth->num_freq_iters++;
		if (wpa_s->dpp_init_max_tries)
			max_tries = wpa_s->dpp_init_max_tries;
		else
			max_tries = 5;
		if (auth->num_freq_iters >= max_tries || auth->auth_req_ack) {
			wpa_printf(MSG_INFO,
				   "DPP: No response received from responder - stopping initiation attempt");
			wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_AUTH_INIT_FAILED);
			eloop_cancel_timeout(wpas_dpp_reply_wait_timeout,
					     wpa_s, NULL);
			offchannel_send_action_done(wpa_s);
			dpp_auth_deinit(wpa_s->dpp_auth);
			wpa_s->dpp_auth = NULL;
			return -1;
		}
		auth->freq_idx = 0;
		eloop_cancel_timeout(wpas_dpp_init_timeout, wpa_s, NULL);
		if (wpa_s->dpp_init_retry_time)
			wait_time = wpa_s->dpp_init_retry_time;
		else
			wait_time = 10000;
		os_get_reltime(&now);
		os_reltime_sub(&now, &wpa_s->dpp_init_iter_start, &diff);
		used = diff.sec * 1000 + diff.usec / 1000;
		if (used > wait_time)
			wait_time = 0;
		else
			wait_time -= used;
		wpa_printf(MSG_DEBUG, "DPP: Next init attempt in %u ms",
			   wait_time);
		eloop_register_timeout(wait_time / 1000,
				       (wait_time % 1000) * 1000,
				       wpas_dpp_init_timeout, wpa_s,
				       NULL);
		return 0;
	}
	freq = auth->freq[auth->freq_idx++];
	auth->curr_freq = freq;

	if (!is_zero_ether_addr(auth->peer_mac_addr))
		dst = auth->peer_mac_addr;
	else if (is_zero_ether_addr(auth->peer_bi->mac_addr))
		dst = broadcast;
	else
		dst = auth->peer_bi->mac_addr;
	wpa_s->dpp_auth_ok_on_ack = 0;
	eloop_cancel_timeout(wpas_dpp_reply_wait_timeout, wpa_s, NULL);
	wait_time = wpa_s->max_remain_on_chan;
	max_wait_time = wpa_s->dpp_resp_wait_time ?
		wpa_s->dpp_resp_wait_time : 2000;
	if (wait_time > max_wait_time)
		wait_time = max_wait_time;
	wait_time += 10; /* give the driver some extra time to complete */
	eloop_register_timeout(wait_time / 1000, (wait_time % 1000) * 1000,
			       wpas_dpp_reply_wait_timeout,
			       wpa_s, NULL);
	wait_time -= 10;
	if (auth->neg_freq > 0 && freq != auth->neg_freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Initiate on %u MHz and move to neg_freq %u MHz for response",
			   freq, auth->neg_freq);
	}
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(dst), freq, DPP_PA_AUTHENTICATION_REQ);
	auth->auth_req_ack = 0;
	os_get_reltime(&wpa_s->dpp_last_init);
	return offchannel_send_action(wpa_s, freq, dst,
				      wpa_s->own_addr, broadcast,
				      wpabuf_head(auth->req_msg),
				      wpabuf_len(auth->req_msg),
				      wait_time, wpas_dpp_tx_status, 0);
}


int wpas_dpp_auth_init(struct wpa_supplicant *wpa_s, const char *cmd)
{
	const char *pos;
	struct dpp_bootstrap_info *peer_bi, *own_bi = NULL;
	struct dpp_authentication *auth;
	u8 allowed_roles = DPP_CAPAB_CONFIGURATOR;
	unsigned int neg_freq = 0;
	int tcp = 0;
#ifdef CONFIG_DPP2
	int tcp_port = DPP_TCP_PORT;
	struct hostapd_ip_addr ipaddr;
	char *addr;
#endif /* CONFIG_DPP2 */

	wpa_s->dpp_gas_client = 0;

	pos = os_strstr(cmd, " peer=");
	if (!pos)
		return -1;
	pos += 6;
	peer_bi = dpp_bootstrap_get_id(wpa_s->dpp, atoi(pos));
	if (!peer_bi) {
		wpa_printf(MSG_INFO,
			   "DPP: Could not find bootstrapping info for the identified peer");
		return -1;
	}

#ifdef CONFIG_DPP2
	pos = os_strstr(cmd, " tcp_port=");
	if (pos) {
		pos += 10;
		tcp_port = atoi(pos);
	}

	addr = get_param(cmd, " tcp_addr=");
	if (addr) {
		int res;

		res = hostapd_parse_ip_addr(addr, &ipaddr);
		os_free(addr);
		if (res)
			return -1;
		tcp = 1;
	}
#endif /* CONFIG_DPP2 */

	pos = os_strstr(cmd, " own=");
	if (pos) {
		pos += 5;
		own_bi = dpp_bootstrap_get_id(wpa_s->dpp, atoi(pos));
		if (!own_bi) {
			wpa_printf(MSG_INFO,
				   "DPP: Could not find bootstrapping info for the identified local entry");
			return -1;
		}

		if (peer_bi->curve != own_bi->curve) {
			wpa_printf(MSG_INFO,
				   "DPP: Mismatching curves in bootstrapping info (peer=%s own=%s)",
				   peer_bi->curve->name, own_bi->curve->name);
			return -1;
		}
	}

	pos = os_strstr(cmd, " role=");
	if (pos) {
		pos += 6;
		if (os_strncmp(pos, "configurator", 12) == 0)
			allowed_roles = DPP_CAPAB_CONFIGURATOR;
		else if (os_strncmp(pos, "enrollee", 8) == 0)
			allowed_roles = DPP_CAPAB_ENROLLEE;
		else if (os_strncmp(pos, "either", 6) == 0)
			allowed_roles = DPP_CAPAB_CONFIGURATOR |
				DPP_CAPAB_ENROLLEE;
		else
			goto fail;
	}

	pos = os_strstr(cmd, " netrole=");
	if (pos) {
		pos += 9;
		if (os_strncmp(pos, "ap", 2) == 0)
			wpa_s->dpp_netrole = DPP_NETROLE_AP;
		else if (os_strncmp(pos, "configurator", 12) == 0)
			wpa_s->dpp_netrole = DPP_NETROLE_CONFIGURATOR;
		else
			wpa_s->dpp_netrole = DPP_NETROLE_STA;
	} else {
		wpa_s->dpp_netrole = DPP_NETROLE_STA;
	}

	pos = os_strstr(cmd, " neg_freq=");
	if (pos)
		neg_freq = atoi(pos + 10);

	if (!tcp && wpa_s->dpp_auth) {
		eloop_cancel_timeout(wpas_dpp_init_timeout, wpa_s, NULL);
		eloop_cancel_timeout(wpas_dpp_reply_wait_timeout, wpa_s, NULL);
		eloop_cancel_timeout(wpas_dpp_auth_conf_wait_timeout, wpa_s,
				     NULL);
		eloop_cancel_timeout(wpas_dpp_auth_resp_retry_timeout, wpa_s,
				     NULL);
#ifdef CONFIG_DPP2
		eloop_cancel_timeout(wpas_dpp_reconfig_reply_wait_timeout,
				     wpa_s, NULL);
#endif /* CONFIG_DPP2 */
		offchannel_send_action_done(wpa_s);
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
	}

	auth = dpp_auth_init(wpa_s->dpp, wpa_s, peer_bi, own_bi, allowed_roles,
			     neg_freq, wpa_s->hw.modes, wpa_s->hw.num_modes);
	if (!auth)
		goto fail;
	wpas_dpp_set_testing_options(wpa_s, auth);
	if (dpp_set_configurator(auth, cmd) < 0) {
		dpp_auth_deinit(auth);
		goto fail;
	}

	auth->neg_freq = neg_freq;

	if (!is_zero_ether_addr(peer_bi->mac_addr))
		os_memcpy(auth->peer_mac_addr, peer_bi->mac_addr, ETH_ALEN);

#ifdef CONFIG_DPP2
	if (tcp)
		return dpp_tcp_init(wpa_s->dpp, auth, &ipaddr, tcp_port,
				    wpa_s->conf->dpp_name, DPP_NETROLE_STA,
				    wpa_s, wpa_s, wpas_dpp_process_conf_obj);
#endif /* CONFIG_DPP2 */

	wpa_s->dpp_auth = auth;
	return wpas_dpp_auth_init_next(wpa_s);
fail:
	return -1;
}


struct wpas_dpp_listen_work {
	unsigned int freq;
	unsigned int duration;
	struct wpabuf *probe_resp_ie;
};


static void wpas_dpp_listen_work_free(struct wpas_dpp_listen_work *lwork)
{
	if (!lwork)
		return;
	os_free(lwork);
}


static void wpas_dpp_listen_work_done(struct wpa_supplicant *wpa_s)
{
	struct wpas_dpp_listen_work *lwork;

	if (!wpa_s->dpp_listen_work)
		return;

	lwork = wpa_s->dpp_listen_work->ctx;
	wpas_dpp_listen_work_free(lwork);
	radio_work_done(wpa_s->dpp_listen_work);
	wpa_s->dpp_listen_work = NULL;
}


static void dpp_start_listen_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpas_dpp_listen_work *lwork = work->ctx;

	if (deinit) {
		if (work->started) {
			wpa_s->dpp_listen_work = NULL;
			wpas_dpp_listen_stop(wpa_s);
		}
		wpas_dpp_listen_work_free(lwork);
		return;
	}

	wpa_s->dpp_listen_work = work;

	wpa_s->dpp_pending_listen_freq = lwork->freq;

	if (wpa_drv_remain_on_channel(wpa_s, lwork->freq,
				      wpa_s->max_remain_on_chan) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to request the driver to remain on channel (%u MHz) for listen",
			   lwork->freq);
		wpa_s->dpp_listen_freq = 0;
		wpas_dpp_listen_work_done(wpa_s);
		wpa_s->dpp_pending_listen_freq = 0;
		return;
	}
	wpa_s->off_channel_freq = 0;
	wpa_s->roc_waiting_drv_freq = lwork->freq;
	wpa_drv_dpp_listen(wpa_s, true);
}


static int wpas_dpp_listen_start(struct wpa_supplicant *wpa_s,
				 unsigned int freq)
{
	struct wpas_dpp_listen_work *lwork;

	if (wpa_s->dpp_listen_work) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Reject start_listen since dpp_listen_work already exists");
		return -1;
	}

	if (wpa_s->dpp_listen_freq)
		wpas_dpp_listen_stop(wpa_s);
	wpa_s->dpp_listen_freq = freq;

	lwork = os_zalloc(sizeof(*lwork));
	if (!lwork)
		return -1;
	lwork->freq = freq;

	if (radio_add_work(wpa_s, freq, "dpp-listen", 0, dpp_start_listen_cb,
			   lwork) < 0) {
		wpas_dpp_listen_work_free(lwork);
		return -1;
	}

	return 0;
}


int wpas_dpp_listen(struct wpa_supplicant *wpa_s, const char *cmd)
{
	int freq;

	freq = atoi(cmd);
	if (freq <= 0)
		return -1;

	if (os_strstr(cmd, " role=configurator"))
		wpa_s->dpp_allowed_roles = DPP_CAPAB_CONFIGURATOR;
	else if (os_strstr(cmd, " role=enrollee"))
		wpa_s->dpp_allowed_roles = DPP_CAPAB_ENROLLEE;
	else
		wpa_s->dpp_allowed_roles = DPP_CAPAB_CONFIGURATOR |
			DPP_CAPAB_ENROLLEE;
	wpa_s->dpp_qr_mutual = os_strstr(cmd, " qr=mutual") != NULL;
	if (os_strstr(cmd, " netrole=ap"))
		wpa_s->dpp_netrole = DPP_NETROLE_AP;
	else if (os_strstr(cmd, " netrole=configurator"))
		wpa_s->dpp_netrole = DPP_NETROLE_CONFIGURATOR;
	else
		wpa_s->dpp_netrole = DPP_NETROLE_STA;
	if (wpa_s->dpp_listen_freq == (unsigned int) freq) {
		wpa_printf(MSG_DEBUG, "DPP: Already listening on %u MHz",
			   freq);
		return 0;
	}

	return wpas_dpp_listen_start(wpa_s, freq);
}


void wpas_dpp_listen_stop(struct wpa_supplicant *wpa_s)
{
	wpa_s->dpp_in_response_listen = 0;
	if (!wpa_s->dpp_listen_freq)
		return;

	wpa_printf(MSG_DEBUG, "DPP: Stop listen on %u MHz",
		   wpa_s->dpp_listen_freq);
	wpa_drv_cancel_remain_on_channel(wpa_s);
	wpa_drv_dpp_listen(wpa_s, false);
	wpa_s->dpp_listen_freq = 0;
	wpas_dpp_listen_work_done(wpa_s);
}


void wpas_dpp_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				   unsigned int freq, unsigned int duration)
{
	if (wpa_s->dpp_listen_freq != freq)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Remain-on-channel started for listen on %u MHz for %u ms",
		   freq, duration);
	os_get_reltime(&wpa_s->dpp_listen_end);
	wpa_s->dpp_listen_end.usec += duration * 1000;
	while (wpa_s->dpp_listen_end.usec >= 1000000) {
		wpa_s->dpp_listen_end.sec++;
		wpa_s->dpp_listen_end.usec -= 1000000;
	}
}


void wpas_dpp_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					  unsigned int freq)
{
	wpas_dpp_listen_work_done(wpa_s);

	if (wpa_s->dpp_auth && wpa_s->dpp_in_response_listen) {
		unsigned int new_freq;

		/* Continue listen with a new remain-on-channel */
		if (wpa_s->dpp_auth->neg_freq > 0)
			new_freq = wpa_s->dpp_auth->neg_freq;
		else
			new_freq = wpa_s->dpp_auth->curr_freq;
		wpa_printf(MSG_DEBUG,
			   "DPP: Continue wait on %u MHz for the ongoing DPP provisioning session",
			   new_freq);
		wpas_dpp_listen_start(wpa_s, new_freq);
		return;
	}

	if (wpa_s->dpp_listen_freq) {
		/* Continue listen with a new remain-on-channel */
		wpas_dpp_listen_start(wpa_s, wpa_s->dpp_listen_freq);
	}
}


static void wpas_dpp_rx_auth_req(struct wpa_supplicant *wpa_s, const u8 *src,
				 const u8 *hdr, const u8 *buf, size_t len,
				 unsigned int freq)
{
	const u8 *r_bootstrap, *i_bootstrap;
	u16 r_bootstrap_len, i_bootstrap_len;
	struct dpp_bootstrap_info *own_bi = NULL, *peer_bi = NULL;

	if (!wpa_s->dpp)
		return;

	wpa_printf(MSG_DEBUG, "DPP: Authentication Request from " MACSTR,
		   MAC2STR(src));

#ifdef CONFIG_DPP2
	wpas_dpp_chirp_stop(wpa_s);
#endif /* CONFIG_DPP2 */

	r_bootstrap = dpp_get_attr(buf, len, DPP_ATTR_R_BOOTSTRAP_KEY_HASH,
				   &r_bootstrap_len);
	if (!r_bootstrap || r_bootstrap_len != SHA256_MAC_LEN) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Responder Bootstrapping Key Hash attribute");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Responder Bootstrapping Key Hash",
		    r_bootstrap, r_bootstrap_len);

	i_bootstrap = dpp_get_attr(buf, len, DPP_ATTR_I_BOOTSTRAP_KEY_HASH,
				   &i_bootstrap_len);
	if (!i_bootstrap || i_bootstrap_len != SHA256_MAC_LEN) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Initiator Bootstrapping Key Hash attribute");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Initiator Bootstrapping Key Hash",
		    i_bootstrap, i_bootstrap_len);

	/* Try to find own and peer bootstrapping key matches based on the
	 * received hash values */
	dpp_bootstrap_find_pair(wpa_s->dpp, i_bootstrap, r_bootstrap,
				&own_bi, &peer_bi);
	if (!own_bi) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"No matching own bootstrapping key found - ignore message");
		return;
	}

	if (wpa_s->dpp_auth) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"Already in DPP authentication exchange - ignore new one");
		return;
	}

	wpa_s->dpp_gas_client = 0;
	wpa_s->dpp_auth_ok_on_ack = 0;
	wpa_s->dpp_auth = dpp_auth_req_rx(wpa_s->dpp, wpa_s,
					  wpa_s->dpp_allowed_roles,
					  wpa_s->dpp_qr_mutual,
					  peer_bi, own_bi, freq, hdr, buf, len);
	if (!wpa_s->dpp_auth) {
		wpa_printf(MSG_DEBUG, "DPP: No response generated");
		return;
	}
	wpas_dpp_set_testing_options(wpa_s, wpa_s->dpp_auth);
	if (dpp_set_configurator(wpa_s->dpp_auth,
				 wpa_s->dpp_configurator_params) < 0) {
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
		return;
	}
	os_memcpy(wpa_s->dpp_auth->peer_mac_addr, src, ETH_ALEN);

	if (wpa_s->dpp_listen_freq &&
	    wpa_s->dpp_listen_freq != wpa_s->dpp_auth->curr_freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Stop listen on %u MHz to allow response on the request %u MHz",
			   wpa_s->dpp_listen_freq, wpa_s->dpp_auth->curr_freq);
		wpas_dpp_listen_stop(wpa_s);
	}

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), wpa_s->dpp_auth->curr_freq,
		DPP_PA_AUTHENTICATION_RESP);
	offchannel_send_action(wpa_s, wpa_s->dpp_auth->curr_freq,
			       src, wpa_s->own_addr, broadcast,
			       wpabuf_head(wpa_s->dpp_auth->resp_msg),
			       wpabuf_len(wpa_s->dpp_auth->resp_msg),
			       500, wpas_dpp_tx_status, 0);
}


static void wpas_dpp_start_gas_server(struct wpa_supplicant *wpa_s)
{
	/* TODO: stop wait and start ROC */
}


static struct wpa_ssid * wpas_dpp_add_network(struct wpa_supplicant *wpa_s,
					      struct dpp_authentication *auth,
					      struct dpp_config_obj *conf)
{
	struct wpa_ssid *ssid;

#ifdef CONFIG_DPP2
	if (conf->akm == DPP_AKM_SAE) {
#ifdef CONFIG_SAE
		struct wpa_driver_capa capa;
		int res;

		res = wpa_drv_get_capa(wpa_s, &capa);
		if (res == 0 &&
		    !(capa.key_mgmt_iftype[WPA_IF_STATION] &
		      WPA_DRIVER_CAPA_KEY_MGMT_SAE) &&
		    !(wpa_s->drv_flags & WPA_DRIVER_FLAGS_SAE)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: SAE not supported by the driver");
			return NULL;
		}
#else /* CONFIG_SAE */
		wpa_printf(MSG_DEBUG, "DPP: SAE not supported in the build");
		return NULL;
#endif /* CONFIG_SAE */
	}
#endif /* CONFIG_DPP2 */

	ssid = wpa_config_add_network(wpa_s->conf);
	if (!ssid)
		return NULL;
	wpas_notify_network_added(wpa_s, ssid);
	wpa_config_set_network_defaults(ssid);
	ssid->disabled = 1;

	ssid->ssid = os_malloc(conf->ssid_len);
	if (!ssid->ssid)
		goto fail;
	os_memcpy(ssid->ssid, conf->ssid, conf->ssid_len);
	ssid->ssid_len = conf->ssid_len;

	if (conf->connector) {
		if (dpp_akm_dpp(conf->akm)) {
			ssid->key_mgmt = WPA_KEY_MGMT_DPP;
			ssid->ieee80211w = MGMT_FRAME_PROTECTION_REQUIRED;
		}
		ssid->dpp_connector = os_strdup(conf->connector);
		if (!ssid->dpp_connector)
			goto fail;
	}

	if (conf->c_sign_key) {
		ssid->dpp_csign = os_malloc(wpabuf_len(conf->c_sign_key));
		if (!ssid->dpp_csign)
			goto fail;
		os_memcpy(ssid->dpp_csign, wpabuf_head(conf->c_sign_key),
			  wpabuf_len(conf->c_sign_key));
		ssid->dpp_csign_len = wpabuf_len(conf->c_sign_key);
	}

	if (conf->pp_key) {
		ssid->dpp_pp_key = os_malloc(wpabuf_len(conf->pp_key));
		if (!ssid->dpp_pp_key)
			goto fail;
		os_memcpy(ssid->dpp_pp_key, wpabuf_head(conf->pp_key),
			  wpabuf_len(conf->pp_key));
		ssid->dpp_pp_key_len = wpabuf_len(conf->pp_key);
	}

	if (auth->net_access_key) {
		ssid->dpp_netaccesskey =
			os_malloc(wpabuf_len(auth->net_access_key));
		if (!ssid->dpp_netaccesskey)
			goto fail;
		os_memcpy(ssid->dpp_netaccesskey,
			  wpabuf_head(auth->net_access_key),
			  wpabuf_len(auth->net_access_key));
		ssid->dpp_netaccesskey_len = wpabuf_len(auth->net_access_key);
		ssid->dpp_netaccesskey_expiry = auth->net_access_key_expiry;
	}

	if (!conf->connector || dpp_akm_psk(conf->akm) ||
	    dpp_akm_sae(conf->akm)) {
		if (!conf->connector || !dpp_akm_dpp(conf->akm))
			ssid->key_mgmt = 0;
		if (dpp_akm_psk(conf->akm))
			ssid->key_mgmt |= WPA_KEY_MGMT_PSK |
				WPA_KEY_MGMT_PSK_SHA256 | WPA_KEY_MGMT_FT_PSK;
		if (dpp_akm_sae(conf->akm))
			ssid->key_mgmt |= WPA_KEY_MGMT_SAE |
				WPA_KEY_MGMT_FT_SAE;
		ssid->ieee80211w = MGMT_FRAME_PROTECTION_OPTIONAL;
		if (conf->passphrase[0]) {
			if (wpa_config_set_quoted(ssid, "psk",
						  conf->passphrase) < 0)
				goto fail;
			wpa_config_update_psk(ssid);
			ssid->export_keys = 1;
		} else {
			ssid->psk_set = conf->psk_set;
			os_memcpy(ssid->psk, conf->psk, PMK_LEN);
		}
	}

#if defined(CONFIG_DPP2) && defined(IEEE8021X_EAPOL)
	if (conf->akm == DPP_AKM_DOT1X) {
		int i;
		char name[100], blobname[128];
		struct wpa_config_blob *blob;

		ssid->key_mgmt = WPA_KEY_MGMT_IEEE8021X |
			WPA_KEY_MGMT_IEEE8021X_SHA256 |
			WPA_KEY_MGMT_IEEE8021X_SHA256;
		ssid->ieee80211w = MGMT_FRAME_PROTECTION_OPTIONAL;

		if (conf->cacert) {
			/* caCert is DER-encoded X.509v3 certificate for the
			 * server certificate if that is different from the
			 * trust root included in certBag. */
			/* TODO: ssid->eap.cert.ca_cert */
		}

		if (conf->certs) {
			for (i = 0; ; i++) {
				os_snprintf(name, sizeof(name), "dpp-certs-%d",
					    i);
				if (!wpa_config_get_blob(wpa_s->conf, name))
					break;
			}

			blob = os_zalloc(sizeof(*blob));
			if (!blob)
				goto fail;
			blob->len = wpabuf_len(conf->certs);
			blob->name = os_strdup(name);
			blob->data = os_malloc(blob->len);
			if (!blob->name || !blob->data) {
				wpa_config_free_blob(blob);
				goto fail;
			}
			os_memcpy(blob->data, wpabuf_head(conf->certs),
				  blob->len);
			os_snprintf(blobname, sizeof(blobname), "blob://%s",
				    name);
			wpa_config_set_blob(wpa_s->conf, blob);
			wpa_printf(MSG_DEBUG, "DPP: Added certificate blob %s",
				   name);
			ssid->eap.cert.client_cert = os_strdup(blobname);
			if (!ssid->eap.cert.client_cert)
				goto fail;

			/* TODO: ssid->eap.identity from own certificate */
			if (wpa_config_set(ssid, "identity", "\"dpp-ent\"",
					   0) < 0)
				goto fail;
		}

		if (auth->priv_key) {
			for (i = 0; ; i++) {
				os_snprintf(name, sizeof(name), "dpp-key-%d",
					    i);
				if (!wpa_config_get_blob(wpa_s->conf, name))
					break;
			}

			blob = os_zalloc(sizeof(*blob));
			if (!blob)
				goto fail;
			blob->len = wpabuf_len(auth->priv_key);
			blob->name = os_strdup(name);
			blob->data = os_malloc(blob->len);
			if (!blob->name || !blob->data) {
				wpa_config_free_blob(blob);
				goto fail;
			}
			os_memcpy(blob->data, wpabuf_head(auth->priv_key),
				  blob->len);
			os_snprintf(blobname, sizeof(blobname), "blob://%s",
				    name);
			wpa_config_set_blob(wpa_s->conf, blob);
			wpa_printf(MSG_DEBUG, "DPP: Added private key blob %s",
				   name);
			ssid->eap.cert.private_key = os_strdup(blobname);
			if (!ssid->eap.cert.private_key)
				goto fail;
		}

		if (conf->server_name) {
			ssid->eap.cert.domain_suffix_match =
				os_strdup(conf->server_name);
			if (!ssid->eap.cert.domain_suffix_match)
				goto fail;
		}

		/* TODO: Use entCreds::eapMethods */
		if (wpa_config_set(ssid, "eap", "TLS", 0) < 0)
			goto fail;
	}
#endif /* CONFIG_DPP2 && IEEE8021X_EAPOL */

	os_memcpy(wpa_s->dpp_last_ssid, conf->ssid, conf->ssid_len);
	wpa_s->dpp_last_ssid_len = conf->ssid_len;

	return ssid;
fail:
	wpas_notify_network_removed(wpa_s, ssid);
	wpa_config_remove_network(wpa_s->conf, ssid->id);
	return NULL;
}


static int wpas_dpp_process_config(struct wpa_supplicant *wpa_s,
				   struct dpp_authentication *auth,
				   struct dpp_config_obj *conf)
{
	struct wpa_ssid *ssid;

	if (wpa_s->conf->dpp_config_processing < 1)
		return 0;

	ssid = wpas_dpp_add_network(wpa_s, auth, conf);
	if (!ssid)
		return -1;

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_NETWORK_ID "%d", ssid->id);
	if (wpa_s->conf->dpp_config_processing == 2)
		ssid->disabled = 0;

#ifndef CONFIG_NO_CONFIG_WRITE
	if (wpa_s->conf->update_config &&
	    wpa_config_write(wpa_s->confname, wpa_s->conf))
		wpa_printf(MSG_DEBUG, "DPP: Failed to update configuration");
#endif /* CONFIG_NO_CONFIG_WRITE */

	return 0;
}


static void wpas_dpp_post_process_config(struct wpa_supplicant *wpa_s,
					 struct dpp_authentication *auth)
{
#ifdef CONFIG_DPP2
	if (auth->reconfig && wpa_s->dpp_reconfig_ssid &&
	    wpa_config_get_network(wpa_s->conf, wpa_s->dpp_reconfig_ssid_id) ==
	    wpa_s->dpp_reconfig_ssid) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Remove reconfigured network profile");
		wpas_notify_network_removed(wpa_s, wpa_s->dpp_reconfig_ssid);
		wpa_config_remove_network(wpa_s->conf,
					  wpa_s->dpp_reconfig_ssid_id);
		wpa_s->dpp_reconfig_ssid = NULL;
		wpa_s->dpp_reconfig_ssid_id = -1;
	}
#endif /* CONFIG_DPP2 */

	if (wpa_s->conf->dpp_config_processing < 2)
		return;

#ifdef CONFIG_DPP2
	if (auth->peer_version >= 2) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Postpone connection attempt to wait for completion of DPP Configuration Result");
		auth->connect_on_tx_status = 1;
		return;
	}
#endif /* CONFIG_DPP2 */

	wpas_dpp_try_to_connect(wpa_s);
}


static int wpas_dpp_handle_config_obj(struct wpa_supplicant *wpa_s,
				      struct dpp_authentication *auth,
				      struct dpp_config_obj *conf)
{
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_RECEIVED);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONFOBJ_AKM "%s",
		dpp_akm_str(conf->akm));
	if (conf->ssid_len)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONFOBJ_SSID "%s",
			wpa_ssid_txt(conf->ssid, conf->ssid_len));
	if (conf->ssid_charset)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONFOBJ_SSID_CHARSET "%d",
			conf->ssid_charset);
	if (conf->connector) {
		/* TODO: Save the Connector and consider using a command
		 * to fetch the value instead of sending an event with
		 * it. The Connector could end up being larger than what
		 * most clients are ready to receive as an event
		 * message. */
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONNECTOR "%s",
			conf->connector);
	}
	if (conf->passphrase[0]) {
		char hex[64 * 2 + 1];

		wpa_snprintf_hex(hex, sizeof(hex),
				 (const u8 *) conf->passphrase,
				 os_strlen(conf->passphrase));
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONFOBJ_PASS "%s",
			hex);
	} else if (conf->psk_set) {
		char hex[PMK_LEN * 2 + 1];

		wpa_snprintf_hex(hex, sizeof(hex), conf->psk, PMK_LEN);
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONFOBJ_PSK "%s",
			hex);
	}
	if (conf->c_sign_key) {
		char *hex;
		size_t hexlen;

		hexlen = 2 * wpabuf_len(conf->c_sign_key) + 1;
		hex = os_malloc(hexlen);
		if (hex) {
			wpa_snprintf_hex(hex, hexlen,
					 wpabuf_head(conf->c_sign_key),
					 wpabuf_len(conf->c_sign_key));
			wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_C_SIGN_KEY "%s",
				hex);
			os_free(hex);
		}
	}
	if (conf->pp_key) {
		char *hex;
		size_t hexlen;

		hexlen = 2 * wpabuf_len(conf->pp_key) + 1;
		hex = os_malloc(hexlen);
		if (hex) {
			wpa_snprintf_hex(hex, hexlen,
					 wpabuf_head(conf->pp_key),
					 wpabuf_len(conf->pp_key));
			wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_PP_KEY "%s", hex);
			os_free(hex);
		}
	}
	if (auth->net_access_key) {
		char *hex;
		size_t hexlen;

		hexlen = 2 * wpabuf_len(auth->net_access_key) + 1;
		hex = os_malloc(hexlen);
		if (hex) {
			wpa_snprintf_hex(hex, hexlen,
					 wpabuf_head(auth->net_access_key),
					 wpabuf_len(auth->net_access_key));
			if (auth->net_access_key_expiry)
				wpa_msg(wpa_s, MSG_INFO,
					DPP_EVENT_NET_ACCESS_KEY "%s %lu", hex,
					(long unsigned)
					auth->net_access_key_expiry);
			else
				wpa_msg(wpa_s, MSG_INFO,
					DPP_EVENT_NET_ACCESS_KEY "%s", hex);
			os_free(hex);
		}
	}

#ifdef CONFIG_DPP2
	if (conf->certbag) {
		char *b64;

		b64 = base64_encode_no_lf(wpabuf_head(conf->certbag),
					  wpabuf_len(conf->certbag), NULL);
		if (b64)
			wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CERTBAG "%s", b64);
		os_free(b64);
	}

	if (conf->cacert) {
		char *b64;

		b64 = base64_encode_no_lf(wpabuf_head(conf->cacert),
					  wpabuf_len(conf->cacert), NULL);
		if (b64)
			wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CACERT "%s", b64);
		os_free(b64);
	}

	if (conf->server_name)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_SERVER_NAME "%s",
			conf->server_name);
#endif /* CONFIG_DPP2 */

	return wpas_dpp_process_config(wpa_s, auth, conf);
}


static int wpas_dpp_handle_key_pkg(struct wpa_supplicant *wpa_s,
				   struct dpp_asymmetric_key *key)
{
#ifdef CONFIG_DPP2
	int res;

	if (!key)
		return 0;

	wpa_printf(MSG_DEBUG, "DPP: Received Configurator backup");
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_RECEIVED);
	wpa_s->dpp_conf_backup_received = true;

	while (key) {
		res = dpp_configurator_from_backup(wpa_s->dpp, key);
		if (res < 0)
			return -1;
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONFIGURATOR_ID "%d",
			res);
		key = key->next;
	}
#endif /* CONFIG_DPP2 */

	return 0;
}


#ifdef CONFIG_DPP2
static void wpas_dpp_build_csr(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth || !auth->csrattrs)
		return;

	wpa_printf(MSG_DEBUG, "DPP: Build CSR");
	wpabuf_free(auth->csr);
	/* TODO: Additional information needed for CSR based on csrAttrs */
	auth->csr = dpp_build_csr(auth, wpa_s->conf->dpp_name ?
				  wpa_s->conf->dpp_name : "Test");
	if (!auth->csr) {
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
		return;
	}

	wpas_dpp_start_gas_client(wpa_s);
}
#endif /* CONFIG_DPP2 */


static void wpas_dpp_gas_resp_cb(void *ctx, const u8 *addr, u8 dialog_token,
				 enum gas_query_result result,
				 const struct wpabuf *adv_proto,
				 const struct wpabuf *resp, u16 status_code)
{
	struct wpa_supplicant *wpa_s = ctx;
	const u8 *pos;
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	int res;
	enum dpp_status_error status = DPP_STATUS_CONFIG_REJECTED;
	unsigned int i;

	wpa_s->dpp_gas_dialog_token = -1;

	if (!auth || (!auth->auth_success && !auth->reconfig_success) ||
	    os_memcmp(addr, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: No matching exchange in progress");
		return;
	}
	if (result != GAS_QUERY_SUCCESS ||
	    !resp || status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "DPP: GAS query did not succeed");
		goto fail;
	}

	wpa_hexdump_buf(MSG_DEBUG, "DPP: Configuration Response adv_proto",
			adv_proto);
	wpa_hexdump_buf(MSG_DEBUG, "DPP: Configuration Response (GAS response)",
			resp);

	if (wpabuf_len(adv_proto) != 10 ||
	    !(pos = wpabuf_head(adv_proto)) ||
	    pos[0] != WLAN_EID_ADV_PROTO ||
	    pos[1] != 8 ||
	    pos[3] != WLAN_EID_VENDOR_SPECIFIC ||
	    pos[4] != 5 ||
	    WPA_GET_BE24(&pos[5]) != OUI_WFA ||
	    pos[8] != 0x1a ||
	    pos[9] != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Not a DPP Advertisement Protocol ID");
		goto fail;
	}

	res = dpp_conf_resp_rx(auth, resp);
#ifdef CONFIG_DPP2
	if (res == -2) {
		wpa_printf(MSG_DEBUG, "DPP: CSR needed");
		eloop_register_timeout(0, 0, wpas_dpp_build_csr, wpa_s, NULL);
		return;
	}
#endif /* CONFIG_DPP2 */
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Configuration attempt failed");
		goto fail;
	}

	wpa_s->dpp_conf_backup_received = false;
	for (i = 0; i < auth->num_conf_obj; i++) {
		res = wpas_dpp_handle_config_obj(wpa_s, auth,
						 &auth->conf_obj[i]);
		if (res < 0)
			goto fail;
	}
	if (auth->num_conf_obj)
		wpas_dpp_post_process_config(wpa_s, auth);
	if (wpas_dpp_handle_key_pkg(wpa_s, auth->conf_key_pkg) < 0)
		goto fail;

	status = DPP_STATUS_OK;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_REJECT_CONFIG) {
		wpa_printf(MSG_INFO, "DPP: TESTING - Reject Config Object");
		status = DPP_STATUS_CONFIG_REJECTED;
	}
#endif /* CONFIG_TESTING_OPTIONS */
fail:
	if (status != DPP_STATUS_OK)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_FAILED);
#ifdef CONFIG_DPP2
	if (auth->peer_version >= 2 &&
	    auth->conf_resp_status == DPP_STATUS_OK) {
		struct wpabuf *msg;

		wpa_printf(MSG_DEBUG, "DPP: Send DPP Configuration Result");
		msg = dpp_build_conf_result(auth, status);
		if (!msg)
			goto fail2;

		wpa_msg(wpa_s, MSG_INFO,
			DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
			MAC2STR(addr), auth->curr_freq,
			DPP_PA_CONFIGURATION_RESULT);
		offchannel_send_action(wpa_s, auth->curr_freq,
				       addr, wpa_s->own_addr, broadcast,
				       wpabuf_head(msg),
				       wpabuf_len(msg),
				       500, wpas_dpp_tx_status, 0);
		wpabuf_free(msg);

		/* This exchange will be terminated in the TX status handler */
		if (wpa_s->conf->dpp_config_processing < 2 ||
		    wpa_s->dpp_conf_backup_received)
			auth->remove_on_tx_status = 1;
		return;
	}
fail2:
#endif /* CONFIG_DPP2 */
	dpp_auth_deinit(wpa_s->dpp_auth);
	wpa_s->dpp_auth = NULL;
}


static void wpas_dpp_start_gas_client(struct wpa_supplicant *wpa_s)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	struct wpabuf *buf;
	int res;
	int *supp_op_classes;

	wpa_s->dpp_gas_client = 1;
	offchannel_send_action_done(wpa_s);
	wpas_dpp_listen_stop(wpa_s);

	supp_op_classes = wpas_supp_op_classes(wpa_s);
	buf = dpp_build_conf_req_helper(auth, wpa_s->conf->dpp_name,
					wpa_s->dpp_netrole,
					wpa_s->conf->dpp_mud_url,
					supp_op_classes);
	os_free(supp_op_classes);
	if (!buf) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No configuration request data available");
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: GAS request to " MACSTR " (freq %u MHz)",
		   MAC2STR(auth->peer_mac_addr), auth->curr_freq);

	res = gas_query_req(wpa_s->gas, auth->peer_mac_addr, auth->curr_freq,
			    1, 1, buf, wpas_dpp_gas_resp_cb, wpa_s);
	if (res < 0) {
		wpa_msg(wpa_s, MSG_DEBUG, "GAS: Failed to send Query Request");
		wpabuf_free(buf);
	} else {
		wpa_printf(MSG_DEBUG,
			   "DPP: GAS query started with dialog token %u", res);
		wpa_s->dpp_gas_dialog_token = res;
	}
}


static void wpas_dpp_auth_success(struct wpa_supplicant *wpa_s, int initiator)
{
	wpa_printf(MSG_DEBUG, "DPP: Authentication succeeded");
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_AUTH_SUCCESS "init=%d", initiator);
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_AUTH_CONF) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at Authentication Confirm");
		if (wpa_s->dpp_auth->configurator) {
			/* Prevent GAS response */
			wpa_s->dpp_auth->auth_success = 0;
		}
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (wpa_s->dpp_auth->configurator)
		wpas_dpp_start_gas_server(wpa_s);
	else
		wpas_dpp_start_gas_client(wpa_s);
}


static void wpas_dpp_rx_auth_resp(struct wpa_supplicant *wpa_s, const u8 *src,
				  const u8 *hdr, const u8 *buf, size_t len,
				  unsigned int freq)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	struct wpabuf *msg;

	wpa_printf(MSG_DEBUG, "DPP: Authentication Response from " MACSTR
		   " (freq %u MHz)", MAC2STR(src), freq);

	if (!auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No DPP Authentication in progress - drop");
		return;
	}

	if (!is_zero_ether_addr(auth->peer_mac_addr) &&
	    os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: MAC address mismatch (expected "
			   MACSTR ") - drop", MAC2STR(auth->peer_mac_addr));
		return;
	}

	eloop_cancel_timeout(wpas_dpp_reply_wait_timeout, wpa_s, NULL);

	if (auth->curr_freq != freq && auth->neg_freq == freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Responder accepted request for different negotiation channel");
		auth->curr_freq = freq;
	}

	eloop_cancel_timeout(wpas_dpp_init_timeout, wpa_s, NULL);
	msg = dpp_auth_resp_rx(auth, hdr, buf, len);
	if (!msg) {
		if (auth->auth_resp_status == DPP_STATUS_RESPONSE_PENDING) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Start wait for full response");
			offchannel_send_action_done(wpa_s);
			wpas_dpp_listen_start(wpa_s, auth->curr_freq);
			return;
		}
		wpa_printf(MSG_DEBUG, "DPP: No confirm generated");
		return;
	}
	os_memcpy(auth->peer_mac_addr, src, ETH_ALEN);

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), auth->curr_freq, DPP_PA_AUTHENTICATION_CONF);
	offchannel_send_action(wpa_s, auth->curr_freq,
			       src, wpa_s->own_addr, broadcast,
			       wpabuf_head(msg), wpabuf_len(msg),
			       500, wpas_dpp_tx_status, 0);
	wpabuf_free(msg);
	wpa_s->dpp_auth_ok_on_ack = 1;
}


static void wpas_dpp_rx_auth_conf(struct wpa_supplicant *wpa_s, const u8 *src,
				  const u8 *hdr, const u8 *buf, size_t len)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	wpa_printf(MSG_DEBUG, "DPP: Authentication Confirmation from " MACSTR,
		   MAC2STR(src));

	if (!auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No DPP Authentication in progress - drop");
		return;
	}

	if (os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: MAC address mismatch (expected "
			   MACSTR ") - drop", MAC2STR(auth->peer_mac_addr));
		return;
	}

	eloop_cancel_timeout(wpas_dpp_auth_conf_wait_timeout, wpa_s, NULL);

	if (dpp_auth_conf_rx(auth, hdr, buf, len) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Authentication failed");
		return;
	}

	wpas_dpp_auth_success(wpa_s, 0);
}


#ifdef CONFIG_DPP2

static void wpas_dpp_config_result_wait_timeout(void *eloop_ctx,
						void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth || !auth->waiting_conf_result)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Timeout while waiting for Configuration Result");
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_FAILED);
	dpp_auth_deinit(auth);
	wpa_s->dpp_auth = NULL;
}


static void wpas_dpp_conn_status_result_wait_timeout(void *eloop_ctx,
						     void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth || !auth->waiting_conn_status_result)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Timeout while waiting for Connection Status Result");
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONN_STATUS_RESULT "timeout");
	wpas_dpp_listen_stop(wpa_s);
	dpp_auth_deinit(auth);
	wpa_s->dpp_auth = NULL;
}


static void wpas_dpp_rx_conf_result(struct wpa_supplicant *wpa_s, const u8 *src,
				    const u8 *hdr, const u8 *buf, size_t len)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	enum dpp_status_error status;

	wpa_printf(MSG_DEBUG, "DPP: Configuration Result from " MACSTR,
		   MAC2STR(src));

	if (!auth || !auth->waiting_conf_result) {
		if (auth &&
		    os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) == 0 &&
		    gas_server_response_sent(wpa_s->gas_server,
					     auth->gas_server_ctx)) {
			/* This could happen if the TX status event gets delayed
			 * long enough for the Enrollee to have time to send
			 * the next frame before the TX status gets processed
			 * locally. */
			wpa_printf(MSG_DEBUG,
				   "DPP: GAS response was sent but TX status not yet received - assume it was ACKed since the Enrollee sent the next frame in the sequence");
			auth->waiting_conf_result = 1;
		} else {
			wpa_printf(MSG_DEBUG,
				   "DPP: No DPP Configuration waiting for result - drop");
			return;
		}
	}

	if (os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: MAC address mismatch (expected "
			   MACSTR ") - drop", MAC2STR(auth->peer_mac_addr));
		return;
	}

	status = dpp_conf_result_rx(auth, hdr, buf, len);

	if (status == DPP_STATUS_OK && auth->send_conn_status) {
		wpa_msg(wpa_s, MSG_INFO,
			DPP_EVENT_CONF_SENT "wait_conn_status=1");
		wpa_printf(MSG_DEBUG, "DPP: Wait for Connection Status Result");
		eloop_cancel_timeout(wpas_dpp_config_result_wait_timeout,
				     wpa_s, NULL);
		auth->waiting_conn_status_result = 1;
		eloop_cancel_timeout(wpas_dpp_conn_status_result_wait_timeout,
				     wpa_s, NULL);
		eloop_register_timeout(16, 0,
				       wpas_dpp_conn_status_result_wait_timeout,
				       wpa_s, NULL);
		offchannel_send_action_done(wpa_s);
		wpas_dpp_listen_start(wpa_s, auth->neg_freq ? auth->neg_freq :
				      auth->curr_freq);
		return;
	}
	offchannel_send_action_done(wpa_s);
	wpas_dpp_listen_stop(wpa_s);
	if (status == DPP_STATUS_OK)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_SENT);
	else
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_FAILED);
	dpp_auth_deinit(auth);
	wpa_s->dpp_auth = NULL;
	eloop_cancel_timeout(wpas_dpp_config_result_wait_timeout, wpa_s, NULL);
}


static void wpas_dpp_rx_conn_status_result(struct wpa_supplicant *wpa_s,
					   const u8 *src, const u8 *hdr,
					   const u8 *buf, size_t len)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	enum dpp_status_error status;
	u8 ssid[SSID_MAX_LEN];
	size_t ssid_len = 0;
	char *channel_list = NULL;

	wpa_printf(MSG_DEBUG, "DPP: Connection Status Result");

	if (!auth || !auth->waiting_conn_status_result) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No DPP Configuration waiting for connection status result - drop");
		return;
	}

	status = dpp_conn_status_result_rx(auth, hdr, buf, len,
					   ssid, &ssid_len, &channel_list);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONN_STATUS_RESULT
		"result=%d ssid=%s channel_list=%s",
		status, wpa_ssid_txt(ssid, ssid_len),
		channel_list ? channel_list : "N/A");
	os_free(channel_list);
	offchannel_send_action_done(wpa_s);
	wpas_dpp_listen_stop(wpa_s);
	dpp_auth_deinit(auth);
	wpa_s->dpp_auth = NULL;
	eloop_cancel_timeout(wpas_dpp_conn_status_result_wait_timeout,
			     wpa_s, NULL);
}


static int wpas_dpp_process_conf_obj(void *ctx,
				     struct dpp_authentication *auth)
{
	struct wpa_supplicant *wpa_s = ctx;
	unsigned int i;
	int res = -1;

	for (i = 0; i < auth->num_conf_obj; i++) {
		res = wpas_dpp_handle_config_obj(wpa_s, auth,
						 &auth->conf_obj[i]);
		if (res)
			break;
	}
	if (!res)
		wpas_dpp_post_process_config(wpa_s, auth);

	return res;
}


static void wpas_dpp_remove_bi(void *ctx, struct dpp_bootstrap_info *bi)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (bi == wpa_s->dpp_chirp_bi)
		wpas_dpp_chirp_stop(wpa_s);
}


static void
wpas_dpp_rx_presence_announcement(struct wpa_supplicant *wpa_s, const u8 *src,
				  const u8 *hdr, const u8 *buf, size_t len,
				  unsigned int freq)
{
	const u8 *r_bootstrap;
	u16 r_bootstrap_len;
	struct dpp_bootstrap_info *peer_bi;
	struct dpp_authentication *auth;

	if (!wpa_s->dpp)
		return;

	if (wpa_s->dpp_auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore Presence Announcement during ongoing Authentication");
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Presence Announcement from " MACSTR,
		   MAC2STR(src));

	r_bootstrap = dpp_get_attr(buf, len, DPP_ATTR_R_BOOTSTRAP_KEY_HASH,
				   &r_bootstrap_len);
	if (!r_bootstrap || r_bootstrap_len != SHA256_MAC_LEN) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Responder Bootstrapping Key Hash attribute");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Responder Bootstrapping Key Hash",
		    r_bootstrap, r_bootstrap_len);
	peer_bi = dpp_bootstrap_find_chirp(wpa_s->dpp, r_bootstrap);
	dpp_notify_chirp_received(wpa_s, peer_bi ? (int) peer_bi->id : -1, src,
				  freq, r_bootstrap);
	if (!peer_bi) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No matching bootstrapping information found");
		return;
	}

	auth = dpp_auth_init(wpa_s->dpp, wpa_s, peer_bi, NULL,
			     DPP_CAPAB_CONFIGURATOR, freq, NULL, 0);
	if (!auth)
		return;
	wpas_dpp_set_testing_options(wpa_s, auth);
	if (dpp_set_configurator(auth, wpa_s->dpp_configurator_params) < 0) {
		dpp_auth_deinit(auth);
		return;
	}

	auth->neg_freq = freq;

	/* The source address of the Presence Announcement frame overrides any
	 * MAC address information from the bootstrapping information. */
	os_memcpy(auth->peer_mac_addr, src, ETH_ALEN);

	wpa_s->dpp_auth = auth;
	if (wpas_dpp_auth_init_next(wpa_s) < 0) {
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
	}
}


static void wpas_dpp_reconfig_reply_wait_timeout(void *eloop_ctx,
						 void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth)
		return;

	wpa_printf(MSG_DEBUG, "DPP: Reconfig Reply wait timeout");
	offchannel_send_action_done(wpa_s);
	wpas_dpp_listen_stop(wpa_s);
	dpp_auth_deinit(auth);
	wpa_s->dpp_auth = NULL;
}


static void
wpas_dpp_rx_reconfig_announcement(struct wpa_supplicant *wpa_s, const u8 *src,
				  const u8 *hdr, const u8 *buf, size_t len,
				  unsigned int freq)
{
	const u8 *csign_hash, *fcgroup, *a_nonce, *e_id;
	u16 csign_hash_len, fcgroup_len, a_nonce_len, e_id_len;
	struct dpp_configurator *conf;
	struct dpp_authentication *auth;
	unsigned int wait_time, max_wait_time;
	u16 group;

	if (!wpa_s->dpp)
		return;

	if (wpa_s->dpp_auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore Reconfig Announcement during ongoing Authentication");
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Reconfig Announcement from " MACSTR,
		   MAC2STR(src));

	csign_hash = dpp_get_attr(buf, len, DPP_ATTR_C_SIGN_KEY_HASH,
				  &csign_hash_len);
	if (!csign_hash || csign_hash_len != SHA256_MAC_LEN) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Configurator C-sign key Hash attribute");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Configurator C-sign key Hash (kid)",
		    csign_hash, csign_hash_len);
	conf = dpp_configurator_find_kid(wpa_s->dpp, csign_hash);
	if (!conf) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No matching Configurator information found");
		return;
	}

	fcgroup = dpp_get_attr(buf, len, DPP_ATTR_FINITE_CYCLIC_GROUP,
			       &fcgroup_len);
	if (!fcgroup || fcgroup_len != 2) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Finite Cyclic Group attribute");
		return;
	}
	group = WPA_GET_LE16(fcgroup);
	wpa_printf(MSG_DEBUG, "DPP: Enrollee finite cyclic group: %u", group);

	a_nonce = dpp_get_attr(buf, len, DPP_ATTR_A_NONCE, &a_nonce_len);
	e_id = dpp_get_attr(buf, len, DPP_ATTR_E_PRIME_ID, &e_id_len);

	auth = dpp_reconfig_init(wpa_s->dpp, wpa_s, conf, freq, group,
				 a_nonce, a_nonce_len, e_id, e_id_len);
	if (!auth)
		return;
	wpas_dpp_set_testing_options(wpa_s, auth);
	if (dpp_set_configurator(auth, wpa_s->dpp_configurator_params) < 0) {
		dpp_auth_deinit(auth);
		return;
	}

	os_memcpy(auth->peer_mac_addr, src, ETH_ALEN);
	wpa_s->dpp_auth = auth;

	wpa_s->dpp_in_response_listen = 0;
	wpa_s->dpp_auth_ok_on_ack = 0;
	wait_time = wpa_s->max_remain_on_chan;
	max_wait_time = wpa_s->dpp_resp_wait_time ?
		wpa_s->dpp_resp_wait_time : 2000;
	if (wait_time > max_wait_time)
		wait_time = max_wait_time;
	wait_time += 10; /* give the driver some extra time to complete */
	eloop_register_timeout(wait_time / 1000, (wait_time % 1000) * 1000,
			       wpas_dpp_reconfig_reply_wait_timeout,
			       wpa_s, NULL);
	wait_time -= 10;

	wpas_dpp_stop_listen_for_tx(wpa_s, freq, wait_time);

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, DPP_PA_RECONFIG_AUTH_REQ);
	if (offchannel_send_action(wpa_s, freq, src, wpa_s->own_addr, broadcast,
				   wpabuf_head(auth->reconfig_req_msg),
				   wpabuf_len(auth->reconfig_req_msg),
				   wait_time, wpas_dpp_tx_status, 0) < 0) {
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
	}
}


static void
wpas_dpp_rx_reconfig_auth_req(struct wpa_supplicant *wpa_s, const u8 *src,
			      const u8 *hdr, const u8 *buf, size_t len,
			      unsigned int freq)
{
	struct wpa_ssid *ssid;
	struct dpp_authentication *auth;

	wpa_printf(MSG_DEBUG, "DPP: Reconfig Authentication Request from "
		   MACSTR, MAC2STR(src));

	if (!wpa_s->dpp)
		return;
	if (wpa_s->dpp_auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Not ready for reconfiguration - pending authentication exchange in progress");
		return;
	}
	if (!wpa_s->dpp_reconfig_ssid) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Not ready for reconfiguration - not requested");
		return;
	}
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (ssid == wpa_s->dpp_reconfig_ssid &&
		    ssid->id == wpa_s->dpp_reconfig_ssid_id)
			break;
	}
	if (!ssid || !ssid->dpp_connector || !ssid->dpp_netaccesskey ||
	    !ssid->dpp_csign) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Not ready for reconfiguration - no matching network profile with Connector found");
		return;
	}

	auth = dpp_reconfig_auth_req_rx(wpa_s->dpp, wpa_s, ssid->dpp_connector,
					ssid->dpp_netaccesskey,
					ssid->dpp_netaccesskey_len,
					ssid->dpp_csign, ssid->dpp_csign_len,
					freq, hdr, buf, len);
	if (!auth)
		return;
	os_memcpy(auth->peer_mac_addr, src, ETH_ALEN);
	wpa_s->dpp_auth = auth;

	wpas_dpp_chirp_stop(wpa_s);

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, DPP_PA_RECONFIG_AUTH_RESP);
	if (offchannel_send_action(wpa_s, freq, src, wpa_s->own_addr, broadcast,
				   wpabuf_head(auth->reconfig_resp_msg),
				   wpabuf_len(auth->reconfig_resp_msg),
				   500, wpas_dpp_tx_status, 0) < 0) {
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
	}
}


static void
wpas_dpp_rx_reconfig_auth_resp(struct wpa_supplicant *wpa_s, const u8 *src,
			       const u8 *hdr, const u8 *buf, size_t len,
			       unsigned int freq)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	struct wpabuf *conf;

	wpa_printf(MSG_DEBUG, "DPP: Reconfig Authentication Response from "
		   MACSTR, MAC2STR(src));

	if (!auth || !auth->reconfig || !auth->configurator) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No DPP Reconfig Authentication in progress - drop");
		return;
	}

	if (os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: MAC address mismatch (expected "
			   MACSTR ") - drop", MAC2STR(auth->peer_mac_addr));
		return;
	}

	conf = dpp_reconfig_auth_resp_rx(auth, hdr, buf, len);
	if (!conf)
		return;

	eloop_cancel_timeout(wpas_dpp_reconfig_reply_wait_timeout, wpa_s, NULL);

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, DPP_PA_RECONFIG_AUTH_CONF);
	if (offchannel_send_action(wpa_s, freq, src, wpa_s->own_addr, broadcast,
				   wpabuf_head(conf), wpabuf_len(conf),
				   500, wpas_dpp_tx_status, 0) < 0) {
		wpabuf_free(conf);
		dpp_auth_deinit(wpa_s->dpp_auth);
		wpa_s->dpp_auth = NULL;
		return;
	}
	wpabuf_free(conf);

	wpas_dpp_start_gas_server(wpa_s);
}


static void
wpas_dpp_rx_reconfig_auth_conf(struct wpa_supplicant *wpa_s, const u8 *src,
			       const u8 *hdr, const u8 *buf, size_t len,
			       unsigned int freq)
{
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	wpa_printf(MSG_DEBUG, "DPP: Reconfig Authentication Confirm from "
		   MACSTR, MAC2STR(src));

	if (!auth || !auth->reconfig || auth->configurator) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No DPP Reconfig Authentication in progress - drop");
		return;
	}

	if (os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: MAC address mismatch (expected "
			   MACSTR ") - drop", MAC2STR(auth->peer_mac_addr));
		return;
	}

	if (dpp_reconfig_auth_conf_rx(auth, hdr, buf, len) < 0)
		return;

	wpas_dpp_start_gas_client(wpa_s);
}

#endif /* CONFIG_DPP2 */


static void wpas_dpp_rx_peer_disc_resp(struct wpa_supplicant *wpa_s,
				       const u8 *src,
				       const u8 *buf, size_t len)
{
	struct wpa_ssid *ssid;
	const u8 *connector, *trans_id, *status;
	u16 connector_len, trans_id_len, status_len;
#ifdef CONFIG_DPP2
	const u8 *version;
	u16 version_len;
#endif /* CONFIG_DPP2 */
	u8 peer_version = 1;
	struct dpp_introduction intro;
	struct rsn_pmksa_cache_entry *entry;
	struct os_time now;
	struct os_reltime rnow;
	os_time_t expiry;
	unsigned int seconds;
	enum dpp_status_error res;

	wpa_printf(MSG_DEBUG, "DPP: Peer Discovery Response from " MACSTR,
		   MAC2STR(src));
	if (is_zero_ether_addr(wpa_s->dpp_intro_bssid) ||
	    os_memcmp(src, wpa_s->dpp_intro_bssid, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: Not waiting for response from "
			   MACSTR " - drop", MAC2STR(src));
		return;
	}
	offchannel_send_action_done(wpa_s);

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (ssid == wpa_s->dpp_intro_network)
			break;
	}
	if (!ssid || !ssid->dpp_connector || !ssid->dpp_netaccesskey ||
	    !ssid->dpp_csign) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Profile not found for network introduction");
		return;
	}

	trans_id = dpp_get_attr(buf, len, DPP_ATTR_TRANSACTION_ID,
			       &trans_id_len);
	if (!trans_id || trans_id_len != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Peer did not include Transaction ID");
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
			" fail=missing_transaction_id", MAC2STR(src));
		goto fail;
	}
	if (trans_id[0] != TRANSACTION_ID) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore frame with unexpected Transaction ID %u",
			   trans_id[0]);
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
			" fail=transaction_id_mismatch", MAC2STR(src));
		goto fail;
	}

	status = dpp_get_attr(buf, len, DPP_ATTR_STATUS, &status_len);
	if (!status || status_len != 1) {
		wpa_printf(MSG_DEBUG, "DPP: Peer did not include Status");
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
			" fail=missing_status", MAC2STR(src));
		goto fail;
	}
	if (status[0] != DPP_STATUS_OK) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Peer rejected network introduction: Status %u",
			   status[0]);
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
			" status=%u", MAC2STR(src), status[0]);
#ifdef CONFIG_DPP2
		wpas_dpp_send_conn_status_result(wpa_s, status[0]);
#endif /* CONFIG_DPP2 */
		goto fail;
	}

	connector = dpp_get_attr(buf, len, DPP_ATTR_CONNECTOR, &connector_len);
	if (!connector) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Peer did not include its Connector");
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
			" fail=missing_connector", MAC2STR(src));
		goto fail;
	}

	res = dpp_peer_intro(&intro, ssid->dpp_connector,
			     ssid->dpp_netaccesskey,
			     ssid->dpp_netaccesskey_len,
			     ssid->dpp_csign,
			     ssid->dpp_csign_len,
			     connector, connector_len, &expiry);
	if (res != DPP_STATUS_OK) {
		wpa_printf(MSG_INFO,
			   "DPP: Network Introduction protocol resulted in failure");
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
			" fail=peer_connector_validation_failed", MAC2STR(src));
#ifdef CONFIG_DPP2
		wpas_dpp_send_conn_status_result(wpa_s, res);
#endif /* CONFIG_DPP2 */
		goto fail;
	}

	entry = os_zalloc(sizeof(*entry));
	if (!entry)
		goto fail;
	os_memcpy(entry->aa, src, ETH_ALEN);
	os_memcpy(entry->pmkid, intro.pmkid, PMKID_LEN);
	os_memcpy(entry->pmk, intro.pmk, intro.pmk_len);
	entry->pmk_len = intro.pmk_len;
	entry->akmp = WPA_KEY_MGMT_DPP;
#ifdef CONFIG_DPP2
	version = dpp_get_attr(buf, len, DPP_ATTR_PROTOCOL_VERSION,
			       &version_len);
	if (version && version_len >= 1)
		peer_version = version[0];
	entry->dpp_pfs = peer_version >= 2;
#endif /* CONFIG_DPP2 */
	if (expiry) {
		os_get_time(&now);
		seconds = expiry - now.sec;
	} else {
		seconds = 86400 * 7;
	}
	os_get_reltime(&rnow);
	entry->expiration = rnow.sec + seconds;
	entry->reauth_time = rnow.sec + seconds;
	entry->network_ctx = ssid;
	wpa_sm_pmksa_cache_add_entry(wpa_s->wpa, entry);

	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_INTRO "peer=" MACSTR
		" status=%u version=%u", MAC2STR(src), status[0], peer_version);

	wpa_printf(MSG_DEBUG,
		   "DPP: Try connection again after successful network introduction");
	if (wpa_supplicant_fast_associate(wpa_s) != 1) {
		wpa_supplicant_cancel_sched_scan(wpa_s);
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
fail:
	os_memset(&intro, 0, sizeof(intro));
}


static int wpas_dpp_allow_ir(struct wpa_supplicant *wpa_s, unsigned int freq)
{
	int i, j;

	if (!wpa_s->hw.modes)
		return -1;

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		struct hostapd_hw_modes *mode = &wpa_s->hw.modes[i];

		for (j = 0; j < mode->num_channels; j++) {
			struct hostapd_channel_data *chan = &mode->channels[j];

			if (chan->freq != (int) freq)
				continue;

			if (chan->flag & (HOSTAPD_CHAN_DISABLED |
					  HOSTAPD_CHAN_NO_IR |
					  HOSTAPD_CHAN_RADAR))
				continue;

			return 1;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "DPP: Frequency %u MHz not supported or does not allow PKEX initiation in the current channel list",
		   freq);

	return 0;
}


static int wpas_dpp_pkex_next_channel(struct wpa_supplicant *wpa_s,
				      struct dpp_pkex *pkex)
{
	if (pkex->freq == 2437)
		pkex->freq = 5745;
	else if (pkex->freq == 5745)
		pkex->freq = 5220;
	else if (pkex->freq == 5220)
		pkex->freq = 60480;
	else
		return -1; /* no more channels to try */

	if (wpas_dpp_allow_ir(wpa_s, pkex->freq) == 1) {
		wpa_printf(MSG_DEBUG, "DPP: Try to initiate on %u MHz",
			   pkex->freq);
		return 0;
	}

	/* Could not use this channel - try the next one */
	return wpas_dpp_pkex_next_channel(wpa_s, pkex);
}


static void wpas_dpp_pkex_retry_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct dpp_pkex *pkex = wpa_s->dpp_pkex;

	if (!pkex || !pkex->exchange_req)
		return;
	if (pkex->exch_req_tries >= 5) {
		if (wpas_dpp_pkex_next_channel(wpa_s, pkex) < 0) {
			wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_FAIL
				"No response from PKEX peer");
			dpp_pkex_free(pkex);
			wpa_s->dpp_pkex = NULL;
			return;
		}
		pkex->exch_req_tries = 0;
	}

	pkex->exch_req_tries++;
	wpa_printf(MSG_DEBUG, "DPP: Retransmit PKEX Exchange Request (try %u)",
		   pkex->exch_req_tries);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(broadcast), pkex->freq, DPP_PA_PKEX_EXCHANGE_REQ);
	offchannel_send_action(wpa_s, pkex->freq, broadcast,
			       wpa_s->own_addr, broadcast,
			       wpabuf_head(pkex->exchange_req),
			       wpabuf_len(pkex->exchange_req),
			       pkex->exch_req_wait_time,
			       wpas_dpp_tx_pkex_status, 0);
}


static void
wpas_dpp_tx_pkex_status(struct wpa_supplicant *wpa_s,
			unsigned int freq, const u8 *dst,
			const u8 *src, const u8 *bssid,
			const u8 *data, size_t data_len,
			enum offchannel_send_action_result result)
{
	const char *res_txt;
	struct dpp_pkex *pkex = wpa_s->dpp_pkex;

	res_txt = result == OFFCHANNEL_SEND_ACTION_SUCCESS ? "SUCCESS" :
		(result == OFFCHANNEL_SEND_ACTION_NO_ACK ? "no-ACK" :
		 "FAILED");
	wpa_printf(MSG_DEBUG, "DPP: TX status: freq=%u dst=" MACSTR
		   " result=%s (PKEX)",
		   freq, MAC2STR(dst), res_txt);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX_STATUS "dst=" MACSTR
		" freq=%u result=%s", MAC2STR(dst), freq, res_txt);

	if (!pkex) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore TX status since there is no ongoing PKEX exchange");
		return;
	}

	if (pkex->failed) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Terminate PKEX exchange due to an earlier error");
		if (pkex->t > pkex->own_bi->pkex_t)
			pkex->own_bi->pkex_t = pkex->t;
		dpp_pkex_free(pkex);
		wpa_s->dpp_pkex = NULL;
		return;
	}

	if (pkex->exch_req_wait_time && pkex->exchange_req) {
		/* Wait for PKEX Exchange Response frame and retry request if
		 * no response is seen. */
		eloop_cancel_timeout(wpas_dpp_pkex_retry_timeout, wpa_s, NULL);
		eloop_register_timeout(pkex->exch_req_wait_time / 1000,
				       (pkex->exch_req_wait_time % 1000) * 1000,
				       wpas_dpp_pkex_retry_timeout, wpa_s,
				       NULL);
	}
}


static void
wpas_dpp_rx_pkex_exchange_req(struct wpa_supplicant *wpa_s, const u8 *src,
			      const u8 *buf, size_t len, unsigned int freq)
{
	struct wpabuf *msg;
	unsigned int wait_time;

	wpa_printf(MSG_DEBUG, "DPP: PKEX Exchange Request from " MACSTR,
		   MAC2STR(src));

	/* TODO: Support multiple PKEX codes by iterating over all the enabled
	 * values here */

	if (!wpa_s->dpp_pkex_code || !wpa_s->dpp_pkex_bi) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No PKEX code configured - ignore request");
		return;
	}

	if (wpa_s->dpp_pkex) {
		/* TODO: Support parallel operations */
		wpa_printf(MSG_DEBUG,
			   "DPP: Already in PKEX session - ignore new request");
		return;
	}

	wpa_s->dpp_pkex = dpp_pkex_rx_exchange_req(wpa_s, wpa_s->dpp_pkex_bi,
						   wpa_s->own_addr, src,
						   wpa_s->dpp_pkex_identifier,
						   wpa_s->dpp_pkex_code,
						   buf, len);
	if (!wpa_s->dpp_pkex) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to process the request - ignore it");
		return;
	}

	msg = wpa_s->dpp_pkex->exchange_resp;
	wait_time = wpa_s->max_remain_on_chan;
	if (wait_time > 2000)
		wait_time = 2000;
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, DPP_PA_PKEX_EXCHANGE_RESP);
	offchannel_send_action(wpa_s, freq, src, wpa_s->own_addr,
			       broadcast,
			       wpabuf_head(msg), wpabuf_len(msg),
			       wait_time, wpas_dpp_tx_pkex_status, 0);
}


static void
wpas_dpp_rx_pkex_exchange_resp(struct wpa_supplicant *wpa_s, const u8 *src,
			       const u8 *buf, size_t len, unsigned int freq)
{
	struct wpabuf *msg;
	unsigned int wait_time;

	wpa_printf(MSG_DEBUG, "DPP: PKEX Exchange Response from " MACSTR,
		   MAC2STR(src));

	/* TODO: Support multiple PKEX codes by iterating over all the enabled
	 * values here */

	if (!wpa_s->dpp_pkex || !wpa_s->dpp_pkex->initiator ||
	    wpa_s->dpp_pkex->exchange_done) {
		wpa_printf(MSG_DEBUG, "DPP: No matching PKEX session");
		return;
	}

	eloop_cancel_timeout(wpas_dpp_pkex_retry_timeout, wpa_s, NULL);
	wpa_s->dpp_pkex->exch_req_wait_time = 0;

	msg = dpp_pkex_rx_exchange_resp(wpa_s->dpp_pkex, src, buf, len);
	if (!msg) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to process the response");
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Send PKEX Commit-Reveal Request to " MACSTR,
		   MAC2STR(src));

	wait_time = wpa_s->max_remain_on_chan;
	if (wait_time > 2000)
		wait_time = 2000;
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, DPP_PA_PKEX_COMMIT_REVEAL_REQ);
	offchannel_send_action(wpa_s, freq, src, wpa_s->own_addr,
			       broadcast,
			       wpabuf_head(msg), wpabuf_len(msg),
			       wait_time, wpas_dpp_tx_pkex_status, 0);
	wpabuf_free(msg);
}


static struct dpp_bootstrap_info *
wpas_dpp_pkex_finish(struct wpa_supplicant *wpa_s, const u8 *peer,
		     unsigned int freq)
{
	struct dpp_bootstrap_info *bi;

	bi = dpp_pkex_finish(wpa_s->dpp, wpa_s->dpp_pkex, peer, freq);
	if (!bi)
		return NULL;
	wpa_s->dpp_pkex = NULL;
	return bi;
}


static void
wpas_dpp_rx_pkex_commit_reveal_req(struct wpa_supplicant *wpa_s, const u8 *src,
				   const u8 *hdr, const u8 *buf, size_t len,
				   unsigned int freq)
{
	struct wpabuf *msg;
	unsigned int wait_time;
	struct dpp_pkex *pkex = wpa_s->dpp_pkex;

	wpa_printf(MSG_DEBUG, "DPP: PKEX Commit-Reveal Request from " MACSTR,
		   MAC2STR(src));

	if (!pkex || pkex->initiator || !pkex->exchange_done) {
		wpa_printf(MSG_DEBUG, "DPP: No matching PKEX session");
		return;
	}

	msg = dpp_pkex_rx_commit_reveal_req(pkex, hdr, buf, len);
	if (!msg) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to process the request");
		if (pkex->failed) {
			wpa_printf(MSG_DEBUG, "DPP: Terminate PKEX exchange");
			if (pkex->t > pkex->own_bi->pkex_t)
				pkex->own_bi->pkex_t = pkex->t;
			dpp_pkex_free(wpa_s->dpp_pkex);
			wpa_s->dpp_pkex = NULL;
		}
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Send PKEX Commit-Reveal Response to "
		   MACSTR, MAC2STR(src));

	wait_time = wpa_s->max_remain_on_chan;
	if (wait_time > 2000)
		wait_time = 2000;
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, DPP_PA_PKEX_COMMIT_REVEAL_RESP);
	offchannel_send_action(wpa_s, freq, src, wpa_s->own_addr,
			       broadcast,
			       wpabuf_head(msg), wpabuf_len(msg),
			       wait_time, wpas_dpp_tx_pkex_status, 0);
	wpabuf_free(msg);

	wpas_dpp_pkex_finish(wpa_s, src, freq);
}


static void
wpas_dpp_rx_pkex_commit_reveal_resp(struct wpa_supplicant *wpa_s, const u8 *src,
				    const u8 *hdr, const u8 *buf, size_t len,
				    unsigned int freq)
{
	int res;
	struct dpp_bootstrap_info *bi;
	struct dpp_pkex *pkex = wpa_s->dpp_pkex;
	char cmd[500];

	wpa_printf(MSG_DEBUG, "DPP: PKEX Commit-Reveal Response from " MACSTR,
		   MAC2STR(src));

	if (!pkex || !pkex->initiator || !pkex->exchange_done) {
		wpa_printf(MSG_DEBUG, "DPP: No matching PKEX session");
		return;
	}

	res = dpp_pkex_rx_commit_reveal_resp(pkex, hdr, buf, len);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to process the response");
		return;
	}

	bi = wpas_dpp_pkex_finish(wpa_s, src, freq);
	if (!bi)
		return;

	os_snprintf(cmd, sizeof(cmd), " peer=%u %s",
		    bi->id,
		    wpa_s->dpp_pkex_auth_cmd ? wpa_s->dpp_pkex_auth_cmd : "");
	wpa_printf(MSG_DEBUG,
		   "DPP: Start authentication after PKEX with parameters: %s",
		   cmd);
	if (wpas_dpp_auth_init(wpa_s, cmd) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Authentication initialization failed");
		offchannel_send_action_done(wpa_s);
		return;
	}
}


void wpas_dpp_rx_action(struct wpa_supplicant *wpa_s, const u8 *src,
			const u8 *buf, size_t len, unsigned int freq)
{
	u8 crypto_suite;
	enum dpp_public_action_frame_type type;
	const u8 *hdr;
	unsigned int pkex_t;

	if (len < DPP_HDR_LEN)
		return;
	if (WPA_GET_BE24(buf) != OUI_WFA || buf[3] != DPP_OUI_TYPE)
		return;
	hdr = buf;
	buf += 4;
	len -= 4;
	crypto_suite = *buf++;
	type = *buf++;
	len -= 2;

	wpa_printf(MSG_DEBUG,
		   "DPP: Received DPP Public Action frame crypto suite %u type %d from "
		   MACSTR " freq=%u",
		   crypto_suite, type, MAC2STR(src), freq);
	if (crypto_suite != 1) {
		wpa_printf(MSG_DEBUG, "DPP: Unsupported crypto suite %u",
			   crypto_suite);
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_RX "src=" MACSTR
			" freq=%u type=%d ignore=unsupported-crypto-suite",
			MAC2STR(src), freq, type);
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Received message attributes", buf, len);
	if (dpp_check_attrs(buf, len) < 0) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_RX "src=" MACSTR
			" freq=%u type=%d ignore=invalid-attributes",
			MAC2STR(src), freq, type);
		return;
	}
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_RX "src=" MACSTR " freq=%u type=%d",
		MAC2STR(src), freq, type);

	switch (type) {
	case DPP_PA_AUTHENTICATION_REQ:
		wpas_dpp_rx_auth_req(wpa_s, src, hdr, buf, len, freq);
		break;
	case DPP_PA_AUTHENTICATION_RESP:
		wpas_dpp_rx_auth_resp(wpa_s, src, hdr, buf, len, freq);
		break;
	case DPP_PA_AUTHENTICATION_CONF:
		wpas_dpp_rx_auth_conf(wpa_s, src, hdr, buf, len);
		break;
	case DPP_PA_PEER_DISCOVERY_RESP:
		wpas_dpp_rx_peer_disc_resp(wpa_s, src, buf, len);
		break;
	case DPP_PA_PKEX_EXCHANGE_REQ:
		wpas_dpp_rx_pkex_exchange_req(wpa_s, src, buf, len, freq);
		break;
	case DPP_PA_PKEX_EXCHANGE_RESP:
		wpas_dpp_rx_pkex_exchange_resp(wpa_s, src, buf, len, freq);
		break;
	case DPP_PA_PKEX_COMMIT_REVEAL_REQ:
		wpas_dpp_rx_pkex_commit_reveal_req(wpa_s, src, hdr, buf, len,
						   freq);
		break;
	case DPP_PA_PKEX_COMMIT_REVEAL_RESP:
		wpas_dpp_rx_pkex_commit_reveal_resp(wpa_s, src, hdr, buf, len,
						    freq);
		break;
#ifdef CONFIG_DPP2
	case DPP_PA_CONFIGURATION_RESULT:
		wpas_dpp_rx_conf_result(wpa_s, src, hdr, buf, len);
		break;
	case DPP_PA_CONNECTION_STATUS_RESULT:
		wpas_dpp_rx_conn_status_result(wpa_s, src, hdr, buf, len);
		break;
	case DPP_PA_PRESENCE_ANNOUNCEMENT:
		wpas_dpp_rx_presence_announcement(wpa_s, src, hdr, buf, len,
						  freq);
		break;
	case DPP_PA_RECONFIG_ANNOUNCEMENT:
		wpas_dpp_rx_reconfig_announcement(wpa_s, src, hdr, buf, len,
						  freq);
		break;
	case DPP_PA_RECONFIG_AUTH_REQ:
		wpas_dpp_rx_reconfig_auth_req(wpa_s, src, hdr, buf, len, freq);
		break;
	case DPP_PA_RECONFIG_AUTH_RESP:
		wpas_dpp_rx_reconfig_auth_resp(wpa_s, src, hdr, buf, len, freq);
		break;
	case DPP_PA_RECONFIG_AUTH_CONF:
		wpas_dpp_rx_reconfig_auth_conf(wpa_s, src, hdr, buf, len, freq);
		break;
#endif /* CONFIG_DPP2 */
	default:
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignored unsupported frame subtype %d", type);
		break;
	}

	if (wpa_s->dpp_pkex)
		pkex_t = wpa_s->dpp_pkex->t;
	else if (wpa_s->dpp_pkex_bi)
		pkex_t = wpa_s->dpp_pkex_bi->pkex_t;
	else
		pkex_t = 0;
	if (pkex_t >= PKEX_COUNTER_T_LIMIT) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_PKEX_T_LIMIT "id=0");
		wpas_dpp_pkex_remove(wpa_s, "*");
	}
}


static struct wpabuf *
wpas_dpp_gas_req_handler(void *ctx, void *resp_ctx, const u8 *sa,
			 const u8 *query, size_t query_len, u16 *comeback_delay)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	struct wpabuf *resp;

	wpa_printf(MSG_DEBUG, "DPP: GAS request from " MACSTR,
		   MAC2STR(sa));
	if (!auth || (!auth->auth_success && !auth->reconfig_success) ||
	    os_memcmp(sa, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: No matching exchange in progress");
		return NULL;
	}

	if (wpa_s->dpp_auth_ok_on_ack && auth->configurator) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Have not received ACK for Auth Confirm yet - assume it was received based on this GAS request");
		/* wpas_dpp_auth_success() would normally have been called from
		 * TX status handler, but since there was no such handler call
		 * yet, simply send out the event message and proceed with
		 * exchange. */
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_AUTH_SUCCESS "init=1");
		wpa_s->dpp_auth_ok_on_ack = 0;
	}

	wpa_hexdump(MSG_DEBUG,
		    "DPP: Received Configuration Request (GAS Query Request)",
		    query, query_len);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_REQ_RX "src=" MACSTR,
		MAC2STR(sa));
	resp = dpp_conf_req_rx(auth, query, query_len);

#ifdef CONFIG_DPP2
	if (!resp && auth->waiting_cert) {
		wpa_printf(MSG_DEBUG, "DPP: Certificate not yet ready");
		auth->cert_resp_ctx = resp_ctx;
		*comeback_delay = 500;
		return NULL;
	}
#endif /* CONFIG_DPP2 */

	if (!resp)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_FAILED);
	auth->conf_resp = resp;
	auth->gas_server_ctx = resp_ctx;
	return resp;
}


static void
wpas_dpp_gas_status_handler(void *ctx, struct wpabuf *resp, int ok)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct dpp_authentication *auth = wpa_s->dpp_auth;

	if (!auth) {
		wpabuf_free(resp);
		return;
	}
	if (auth->conf_resp != resp) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore GAS status report (ok=%d) for unknown response",
			ok);
		wpabuf_free(resp);
		return;
	}

#ifdef CONFIG_DPP2
	if (auth->waiting_csr && ok) {
		wpa_printf(MSG_DEBUG, "DPP: Waiting for CSR");
		wpabuf_free(resp);
		return;
	}
#endif /* CONFIG_DPP2 */

	wpa_printf(MSG_DEBUG, "DPP: Configuration exchange completed (ok=%d)",
		   ok);
	eloop_cancel_timeout(wpas_dpp_reply_wait_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_auth_conf_wait_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_auth_resp_retry_timeout, wpa_s, NULL);
#ifdef CONFIG_DPP2
	if (ok && auth->peer_version >= 2 &&
	    auth->conf_resp_status == DPP_STATUS_OK &&
	    !auth->waiting_conf_result) {
		wpa_printf(MSG_DEBUG, "DPP: Wait for Configuration Result");
		auth->waiting_conf_result = 1;
		auth->conf_resp = NULL;
		wpabuf_free(resp);
		eloop_cancel_timeout(wpas_dpp_config_result_wait_timeout,
				     wpa_s, NULL);
		eloop_register_timeout(2, 0,
				       wpas_dpp_config_result_wait_timeout,
				       wpa_s, NULL);
		return;
	}
#endif /* CONFIG_DPP2 */
	offchannel_send_action_done(wpa_s);
	wpas_dpp_listen_stop(wpa_s);
	if (ok)
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_SENT);
	else
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CONF_FAILED);
	dpp_auth_deinit(wpa_s->dpp_auth);
	wpa_s->dpp_auth = NULL;
	wpabuf_free(resp);
}


int wpas_dpp_configurator_sign(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct dpp_authentication *auth;
	int ret = -1;
	char *curve = NULL;

	auth = dpp_alloc_auth(wpa_s->dpp, wpa_s);
	if (!auth)
		return -1;

	curve = get_param(cmd, " curve=");
	wpas_dpp_set_testing_options(wpa_s, auth);
	if (dpp_set_configurator(auth, cmd) == 0 &&
	    dpp_configurator_own_config(auth, curve, 0) == 0)
		ret = wpas_dpp_handle_config_obj(wpa_s, auth,
						 &auth->conf_obj[0]);
	if (!ret)
		wpas_dpp_post_process_config(wpa_s, auth);

	dpp_auth_deinit(auth);
	os_free(curve);

	return ret;
}


static void
wpas_dpp_tx_introduction_status(struct wpa_supplicant *wpa_s,
				unsigned int freq, const u8 *dst,
				const u8 *src, const u8 *bssid,
				const u8 *data, size_t data_len,
				enum offchannel_send_action_result result)
{
	const char *res_txt;

	res_txt = result == OFFCHANNEL_SEND_ACTION_SUCCESS ? "SUCCESS" :
		(result == OFFCHANNEL_SEND_ACTION_NO_ACK ? "no-ACK" :
		 "FAILED");
	wpa_printf(MSG_DEBUG, "DPP: TX status: freq=%u dst=" MACSTR
		   " result=%s (DPP Peer Discovery Request)",
		   freq, MAC2STR(dst), res_txt);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX_STATUS "dst=" MACSTR
		" freq=%u result=%s", MAC2STR(dst), freq, res_txt);
	/* TODO: Time out wait for response more quickly in error cases? */
}


int wpas_dpp_check_connect(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
			   struct wpa_bss *bss)
{
	struct os_time now;
	struct wpabuf *msg;
	unsigned int wait_time;
	const u8 *rsn;
	struct wpa_ie_data ied;
	size_t len;

	if (!(ssid->key_mgmt & WPA_KEY_MGMT_DPP) || !bss)
		return 0; /* Not using DPP AKM - continue */
	rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	if (rsn && wpa_parse_wpa_ie(rsn, 2 + rsn[1], &ied) == 0 &&
	    !(ied.key_mgmt & WPA_KEY_MGMT_DPP))
		return 0; /* AP does not support DPP AKM - continue */
	if (wpa_sm_pmksa_exists(wpa_s->wpa, bss->bssid, ssid))
		return 0; /* PMKSA exists for DPP AKM - continue */

	if (!ssid->dpp_connector || !ssid->dpp_netaccesskey ||
	    !ssid->dpp_csign) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_MISSING_CONNECTOR
			"missing %s",
			!ssid->dpp_connector ? "Connector" :
			(!ssid->dpp_netaccesskey ? "netAccessKey" :
			 "C-sign-key"));
		return -1;
	}

	os_get_time(&now);

	if (ssid->dpp_netaccesskey_expiry &&
	    (os_time_t) ssid->dpp_netaccesskey_expiry < now.sec) {
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_MISSING_CONNECTOR
			"netAccessKey expired");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "DPP: Starting network introduction protocol to derive PMKSA for "
		   MACSTR, MAC2STR(bss->bssid));

	len = 5 + 4 + os_strlen(ssid->dpp_connector);
#ifdef CONFIG_DPP2
	len += 5;
#endif /* CONFIG_DPP2 */
	msg = dpp_alloc_msg(DPP_PA_PEER_DISCOVERY_REQ, len);
	if (!msg)
		return -1;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_TRANSACTION_ID_PEER_DISC_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Transaction ID");
		goto skip_trans_id;
	}
	if (dpp_test == DPP_TEST_INVALID_TRANSACTION_ID_PEER_DISC_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Transaction ID");
		wpabuf_put_le16(msg, DPP_ATTR_TRANSACTION_ID);
		wpabuf_put_le16(msg, 0);
		goto skip_trans_id;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Transaction ID */
	wpabuf_put_le16(msg, DPP_ATTR_TRANSACTION_ID);
	wpabuf_put_le16(msg, 1);
	wpabuf_put_u8(msg, TRANSACTION_ID);

#ifdef CONFIG_TESTING_OPTIONS
skip_trans_id:
	if (dpp_test == DPP_TEST_NO_CONNECTOR_PEER_DISC_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Connector");
		goto skip_connector;
	}
	if (dpp_test == DPP_TEST_INVALID_CONNECTOR_PEER_DISC_REQ) {
		char *connector;

		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Connector");
		connector = dpp_corrupt_connector_signature(
			ssid->dpp_connector);
		if (!connector) {
			wpabuf_free(msg);
			return -1;
		}
		wpabuf_put_le16(msg, DPP_ATTR_CONNECTOR);
		wpabuf_put_le16(msg, os_strlen(connector));
		wpabuf_put_str(msg, connector);
		os_free(connector);
		goto skip_connector;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* DPP Connector */
	wpabuf_put_le16(msg, DPP_ATTR_CONNECTOR);
	wpabuf_put_le16(msg, os_strlen(ssid->dpp_connector));
	wpabuf_put_str(msg, ssid->dpp_connector);

#ifdef CONFIG_TESTING_OPTIONS
skip_connector:
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_DPP2
	if (DPP_VERSION > 1) {
		/* Protocol Version */
		wpabuf_put_le16(msg, DPP_ATTR_PROTOCOL_VERSION);
		wpabuf_put_le16(msg, 1);
		wpabuf_put_u8(msg, DPP_VERSION);
	}
#endif /* CONFIG_DPP2 */

	/* TODO: Timeout on AP response */
	wait_time = wpa_s->max_remain_on_chan;
	if (wait_time > 2000)
		wait_time = 2000;
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(bss->bssid), bss->freq, DPP_PA_PEER_DISCOVERY_REQ);
	offchannel_send_action(wpa_s, bss->freq, bss->bssid, wpa_s->own_addr,
			       broadcast,
			       wpabuf_head(msg), wpabuf_len(msg),
			       wait_time, wpas_dpp_tx_introduction_status, 0);
	wpabuf_free(msg);

	/* Request this connection attempt to terminate - new one will be
	 * started when network introduction protocol completes */
	os_memcpy(wpa_s->dpp_intro_bssid, bss->bssid, ETH_ALEN);
	wpa_s->dpp_intro_network = ssid;
	return 1;
}


int wpas_dpp_pkex_add(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct dpp_bootstrap_info *own_bi;
	const char *pos, *end;
	unsigned int wait_time;

	pos = os_strstr(cmd, " own=");
	if (!pos)
		return -1;
	pos += 5;
	own_bi = dpp_bootstrap_get_id(wpa_s->dpp, atoi(pos));
	if (!own_bi) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Identified bootstrap info not found");
		return -1;
	}
	if (own_bi->type != DPP_BOOTSTRAP_PKEX) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Identified bootstrap info not for PKEX");
		return -1;
	}
	wpa_s->dpp_pkex_bi = own_bi;
	own_bi->pkex_t = 0; /* clear pending errors on new code */

	os_free(wpa_s->dpp_pkex_identifier);
	wpa_s->dpp_pkex_identifier = NULL;
	pos = os_strstr(cmd, " identifier=");
	if (pos) {
		pos += 12;
		end = os_strchr(pos, ' ');
		if (!end)
			return -1;
		wpa_s->dpp_pkex_identifier = os_malloc(end - pos + 1);
		if (!wpa_s->dpp_pkex_identifier)
			return -1;
		os_memcpy(wpa_s->dpp_pkex_identifier, pos, end - pos);
		wpa_s->dpp_pkex_identifier[end - pos] = '\0';
	}

	pos = os_strstr(cmd, " code=");
	if (!pos)
		return -1;
	os_free(wpa_s->dpp_pkex_code);
	wpa_s->dpp_pkex_code = os_strdup(pos + 6);
	if (!wpa_s->dpp_pkex_code)
		return -1;

	if (os_strstr(cmd, " init=1")) {
		struct dpp_pkex *pkex;
		struct wpabuf *msg;

		wpa_printf(MSG_DEBUG, "DPP: Initiating PKEX");
		dpp_pkex_free(wpa_s->dpp_pkex);
		wpa_s->dpp_pkex = dpp_pkex_init(wpa_s, own_bi, wpa_s->own_addr,
						wpa_s->dpp_pkex_identifier,
						wpa_s->dpp_pkex_code);
		pkex = wpa_s->dpp_pkex;
		if (!pkex)
			return -1;

		msg = pkex->exchange_req;
		wait_time = wpa_s->max_remain_on_chan;
		if (wait_time > 2000)
			wait_time = 2000;
		pkex->freq = 2437;
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
			" freq=%u type=%d",
			MAC2STR(broadcast), pkex->freq,
			DPP_PA_PKEX_EXCHANGE_REQ);
		offchannel_send_action(wpa_s, pkex->freq, broadcast,
				       wpa_s->own_addr, broadcast,
				       wpabuf_head(msg), wpabuf_len(msg),
				       wait_time, wpas_dpp_tx_pkex_status, 0);
		if (wait_time == 0)
			wait_time = 2000;
		pkex->exch_req_wait_time = wait_time;
		pkex->exch_req_tries = 1;
	}

	/* TODO: Support multiple PKEX info entries */

	os_free(wpa_s->dpp_pkex_auth_cmd);
	wpa_s->dpp_pkex_auth_cmd = os_strdup(cmd);

	return 1;
}


int wpas_dpp_pkex_remove(struct wpa_supplicant *wpa_s, const char *id)
{
	unsigned int id_val;

	if (os_strcmp(id, "*") == 0) {
		id_val = 0;
	} else {
		id_val = atoi(id);
		if (id_val == 0)
			return -1;
	}

	if ((id_val != 0 && id_val != 1) || !wpa_s->dpp_pkex_code)
		return -1;

	/* TODO: Support multiple PKEX entries */
	os_free(wpa_s->dpp_pkex_code);
	wpa_s->dpp_pkex_code = NULL;
	os_free(wpa_s->dpp_pkex_identifier);
	wpa_s->dpp_pkex_identifier = NULL;
	os_free(wpa_s->dpp_pkex_auth_cmd);
	wpa_s->dpp_pkex_auth_cmd = NULL;
	wpa_s->dpp_pkex_bi = NULL;
	/* TODO: Remove dpp_pkex only if it is for the identified PKEX code */
	dpp_pkex_free(wpa_s->dpp_pkex);
	wpa_s->dpp_pkex = NULL;
	return 0;
}


void wpas_dpp_stop(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->dpp_auth || wpa_s->dpp_pkex)
		offchannel_send_action_done(wpa_s);
	dpp_auth_deinit(wpa_s->dpp_auth);
	wpa_s->dpp_auth = NULL;
	dpp_pkex_free(wpa_s->dpp_pkex);
	wpa_s->dpp_pkex = NULL;
	if (wpa_s->dpp_gas_client && wpa_s->dpp_gas_dialog_token >= 0)
		gas_query_stop(wpa_s->gas, wpa_s->dpp_gas_dialog_token);
}


int wpas_dpp_init(struct wpa_supplicant *wpa_s)
{
	struct dpp_global_config config;
	u8 adv_proto_id[7];

	adv_proto_id[0] = WLAN_EID_VENDOR_SPECIFIC;
	adv_proto_id[1] = 5;
	WPA_PUT_BE24(&adv_proto_id[2], OUI_WFA);
	adv_proto_id[5] = DPP_OUI_TYPE;
	adv_proto_id[6] = 0x01;

	if (gas_server_register(wpa_s->gas_server, adv_proto_id,
				sizeof(adv_proto_id), wpas_dpp_gas_req_handler,
				wpas_dpp_gas_status_handler, wpa_s) < 0)
		return -1;

	os_memset(&config, 0, sizeof(config));
	config.cb_ctx = wpa_s;
#ifdef CONFIG_DPP2
	config.remove_bi = wpas_dpp_remove_bi;
#endif /* CONFIG_DPP2 */
	wpa_s->dpp = dpp_global_init(&config);
	return wpa_s->dpp ? 0 : -1;
}


void wpas_dpp_deinit(struct wpa_supplicant *wpa_s)
{
#ifdef CONFIG_TESTING_OPTIONS
	os_free(wpa_s->dpp_config_obj_override);
	wpa_s->dpp_config_obj_override = NULL;
	os_free(wpa_s->dpp_discovery_override);
	wpa_s->dpp_discovery_override = NULL;
	os_free(wpa_s->dpp_groups_override);
	wpa_s->dpp_groups_override = NULL;
	wpa_s->dpp_ignore_netaccesskey_mismatch = 0;
#endif /* CONFIG_TESTING_OPTIONS */
	if (!wpa_s->dpp)
		return;
	eloop_cancel_timeout(wpas_dpp_pkex_retry_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_reply_wait_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_auth_conf_wait_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_init_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_auth_resp_retry_timeout, wpa_s, NULL);
#ifdef CONFIG_DPP2
	eloop_cancel_timeout(wpas_dpp_config_result_wait_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_conn_status_result_wait_timeout,
			     wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_conn_status_result_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_reconfig_reply_wait_timeout,
			     wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_build_csr, wpa_s, NULL);
	dpp_pfs_free(wpa_s->dpp_pfs);
	wpa_s->dpp_pfs = NULL;
	wpas_dpp_chirp_stop(wpa_s);
	dpp_free_reconfig_id(wpa_s->dpp_reconfig_id);
	wpa_s->dpp_reconfig_id = NULL;
#endif /* CONFIG_DPP2 */
	offchannel_send_action_done(wpa_s);
	wpas_dpp_listen_stop(wpa_s);
	wpas_dpp_stop(wpa_s);
	wpas_dpp_pkex_remove(wpa_s, "*");
	os_memset(wpa_s->dpp_intro_bssid, 0, ETH_ALEN);
	os_free(wpa_s->dpp_configurator_params);
	wpa_s->dpp_configurator_params = NULL;
	dpp_global_clear(wpa_s->dpp);
}


#ifdef CONFIG_DPP2

int wpas_dpp_controller_start(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct dpp_controller_config config;
	const char *pos;

	os_memset(&config, 0, sizeof(config));
	config.allowed_roles = DPP_CAPAB_ENROLLEE | DPP_CAPAB_CONFIGURATOR;
	config.netrole = DPP_NETROLE_STA;
	config.msg_ctx = wpa_s;
	config.cb_ctx = wpa_s;
	config.process_conf_obj = wpas_dpp_process_conf_obj;
	if (cmd) {
		pos = os_strstr(cmd, " tcp_port=");
		if (pos) {
			pos += 10;
			config.tcp_port = atoi(pos);
		}

		pos = os_strstr(cmd, " role=");
		if (pos) {
			pos += 6;
			if (os_strncmp(pos, "configurator", 12) == 0)
				config.allowed_roles = DPP_CAPAB_CONFIGURATOR;
			else if (os_strncmp(pos, "enrollee", 8) == 0)
				config.allowed_roles = DPP_CAPAB_ENROLLEE;
			else if (os_strncmp(pos, "either", 6) == 0)
				config.allowed_roles = DPP_CAPAB_CONFIGURATOR |
					DPP_CAPAB_ENROLLEE;
			else
				return -1;
		}

		config.qr_mutual = os_strstr(cmd, " qr=mutual") != NULL;
	}
	config.configurator_params = wpa_s->dpp_configurator_params;
	return dpp_controller_start(wpa_s->dpp, &config);
}


static void wpas_dpp_chirp_next(void *eloop_ctx, void *timeout_ctx);

static void wpas_dpp_chirp_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_printf(MSG_DEBUG, "DPP: No chirp response received");
	offchannel_send_action_done(wpa_s);
	wpas_dpp_chirp_next(wpa_s, NULL);
}


static void wpas_dpp_chirp_tx_status(struct wpa_supplicant *wpa_s,
				     unsigned int freq, const u8 *dst,
				     const u8 *src, const u8 *bssid,
				     const u8 *data, size_t data_len,
				     enum offchannel_send_action_result result)
{
	if (result == OFFCHANNEL_SEND_ACTION_FAILED) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to send chirp on %d MHz",
			   wpa_s->dpp_chirp_freq);
		if (eloop_register_timeout(0, 0, wpas_dpp_chirp_next,
					   wpa_s, NULL) < 0)
			wpas_dpp_chirp_stop(wpa_s);
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Chirp send completed - wait for response");
	if (eloop_register_timeout(2, 0, wpas_dpp_chirp_timeout,
				   wpa_s, NULL) < 0)
		wpas_dpp_chirp_stop(wpa_s);
}


static void wpas_dpp_chirp_start(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *msg, *announce = NULL;
	int type;

	msg = wpa_s->dpp_presence_announcement;
	type = DPP_PA_PRESENCE_ANNOUNCEMENT;
	if (!msg) {
		struct wpa_ssid *ssid = wpa_s->dpp_reconfig_ssid;

		if (ssid && wpa_s->dpp_reconfig_id &&
		    wpa_config_get_network(wpa_s->conf,
					   wpa_s->dpp_reconfig_ssid_id) ==
		    ssid) {
			announce = dpp_build_reconfig_announcement(
				ssid->dpp_csign,
				ssid->dpp_csign_len,
				ssid->dpp_netaccesskey,
				ssid->dpp_netaccesskey_len,
				wpa_s->dpp_reconfig_id);
			msg = announce;
		}
		if (!msg)
			return;
		type = DPP_PA_RECONFIG_ANNOUNCEMENT;
	}
	wpa_printf(MSG_DEBUG, "DPP: Chirp on %d MHz", wpa_s->dpp_chirp_freq);
	wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
		MAC2STR(broadcast), wpa_s->dpp_chirp_freq, type);
	if (offchannel_send_action(
		    wpa_s, wpa_s->dpp_chirp_freq, broadcast,
		    wpa_s->own_addr, broadcast,
		    wpabuf_head(msg), wpabuf_len(msg),
		    2000, wpas_dpp_chirp_tx_status, 0) < 0)
		wpas_dpp_chirp_stop(wpa_s);

	wpabuf_free(announce);
}


static void wpas_dpp_chirp_scan_res_handler(struct wpa_supplicant *wpa_s,
					    struct wpa_scan_results *scan_res)
{
	struct dpp_bootstrap_info *bi = wpa_s->dpp_chirp_bi;
	unsigned int i;
	struct hostapd_hw_modes *mode;
	int c;
	struct wpa_bss *bss;
	bool chan6;

	if (!bi && !wpa_s->dpp_reconfig_ssid)
		return;

	wpa_s->dpp_chirp_scan_done = 1;

	os_free(wpa_s->dpp_chirp_freqs);
	wpa_s->dpp_chirp_freqs = NULL;

	/* Channels from own bootstrapping info */
	if (bi) {
		for (i = 0; i < bi->num_freq; i++)
			int_array_add_unique(&wpa_s->dpp_chirp_freqs,
					     bi->freq[i]);
	}

	/* Preferred chirping channels */
	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes,
			HOSTAPD_MODE_IEEE80211G, false);
	chan6 = mode == NULL;
	if (mode) {
		for (c = 0; c < mode->num_channels; c++) {
			struct hostapd_channel_data *chan = &mode->channels[c];

			if ((chan->flag & HOSTAPD_CHAN_DISABLED) ||
			    chan->freq != 2437)
				continue;
			chan6 = true;
			break;
		}
	}
	if (chan6)
		int_array_add_unique(&wpa_s->dpp_chirp_freqs, 2437);

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes,
			HOSTAPD_MODE_IEEE80211A, false);
	if (mode) {
		int chan44 = 0, chan149 = 0;

		for (c = 0; c < mode->num_channels; c++) {
			struct hostapd_channel_data *chan = &mode->channels[c];

			if (chan->flag & (HOSTAPD_CHAN_DISABLED |
					  HOSTAPD_CHAN_RADAR))
				continue;
			if (chan->freq == 5220)
				chan44 = 1;
			if (chan->freq == 5745)
				chan149 = 1;
		}
		if (chan149)
			int_array_add_unique(&wpa_s->dpp_chirp_freqs, 5745);
		else if (chan44)
			int_array_add_unique(&wpa_s->dpp_chirp_freqs, 5220);
	}

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes,
			HOSTAPD_MODE_IEEE80211AD, false);
	if (mode) {
		for (c = 0; c < mode->num_channels; c++) {
			struct hostapd_channel_data *chan = &mode->channels[c];

			if ((chan->flag & (HOSTAPD_CHAN_DISABLED |
					   HOSTAPD_CHAN_RADAR)) ||
			    chan->freq != 60480)
				continue;
			int_array_add_unique(&wpa_s->dpp_chirp_freqs, 60480);
			break;
		}
	}

	/* Add channels from scan results for APs that advertise Configurator
	 * Connectivity element */
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_get_vendor_ie(bss, DPP_CC_IE_VENDOR_TYPE))
			int_array_add_unique(&wpa_s->dpp_chirp_freqs,
					     bss->freq);
	}

	if (!wpa_s->dpp_chirp_freqs ||
	    eloop_register_timeout(0, 0, wpas_dpp_chirp_next, wpa_s, NULL) < 0)
		wpas_dpp_chirp_stop(wpa_s);
}


static void wpas_dpp_chirp_next(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	int i;

	if (wpa_s->dpp_chirp_listen)
		wpas_dpp_listen_stop(wpa_s);

	if (wpa_s->dpp_chirp_freq == 0) {
		if (wpa_s->dpp_chirp_round % 4 == 0 &&
		    !wpa_s->dpp_chirp_scan_done) {
			if (wpas_scan_scheduled(wpa_s)) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Deferring chirp scan because another scan is planned already");
				if (eloop_register_timeout(1, 0,
							   wpas_dpp_chirp_next,
							   wpa_s, NULL) < 0) {
					wpas_dpp_chirp_stop(wpa_s);
					return;
				}
				return;
			}
			wpa_printf(MSG_DEBUG,
				   "DPP: Update channel list for chirping");
			wpa_s->scan_req = MANUAL_SCAN_REQ;
			wpa_s->scan_res_handler =
				wpas_dpp_chirp_scan_res_handler;
			wpa_supplicant_req_scan(wpa_s, 0, 0);
			return;
		}
		wpa_s->dpp_chirp_freq = wpa_s->dpp_chirp_freqs[0];
		wpa_s->dpp_chirp_round++;
		wpa_printf(MSG_DEBUG, "DPP: Start chirping round %d",
			   wpa_s->dpp_chirp_round);
	} else {
		for (i = 0; wpa_s->dpp_chirp_freqs[i]; i++)
			if (wpa_s->dpp_chirp_freqs[i] == wpa_s->dpp_chirp_freq)
				break;
		if (!wpa_s->dpp_chirp_freqs[i]) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Previous chirp freq %d not found",
				   wpa_s->dpp_chirp_freq);
			return;
		}
		i++;
		if (wpa_s->dpp_chirp_freqs[i]) {
			wpa_s->dpp_chirp_freq = wpa_s->dpp_chirp_freqs[i];
		} else {
			wpa_s->dpp_chirp_iter--;
			if (wpa_s->dpp_chirp_iter <= 0) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Chirping iterations completed");
				wpas_dpp_chirp_stop(wpa_s);
				return;
			}
			wpa_s->dpp_chirp_freq = 0;
			wpa_s->dpp_chirp_scan_done = 0;
			if (eloop_register_timeout(30, 0, wpas_dpp_chirp_next,
						   wpa_s, NULL) < 0) {
				wpas_dpp_chirp_stop(wpa_s);
				return;
			}
			if (wpa_s->dpp_chirp_listen) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Listen on %d MHz during chirp 30 second wait",
					wpa_s->dpp_chirp_listen);
				wpas_dpp_listen_start(wpa_s,
						      wpa_s->dpp_chirp_listen);
			} else {
				wpa_printf(MSG_DEBUG,
					   "DPP: Wait 30 seconds before starting the next chirping round");
			}
			return;
		}
	}

	wpas_dpp_chirp_start(wpa_s);
}


int wpas_dpp_chirp(struct wpa_supplicant *wpa_s, const char *cmd)
{
	const char *pos;
	int iter = 1, listen_freq = 0;
	struct dpp_bootstrap_info *bi;

	pos = os_strstr(cmd, " own=");
	if (!pos)
		return -1;
	pos += 5;
	bi = dpp_bootstrap_get_id(wpa_s->dpp, atoi(pos));
	if (!bi) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Identified bootstrap info not found");
		return -1;
	}

	pos = os_strstr(cmd, " iter=");
	if (pos) {
		iter = atoi(pos + 6);
		if (iter <= 0)
			return -1;
	}

	pos = os_strstr(cmd, " listen=");
	if (pos) {
		listen_freq = atoi(pos + 8);
		if (listen_freq <= 0)
			return -1;
	}

	wpas_dpp_chirp_stop(wpa_s);
	wpa_s->dpp_allowed_roles = DPP_CAPAB_ENROLLEE;
	wpa_s->dpp_qr_mutual = 0;
	wpa_s->dpp_chirp_bi = bi;
	wpa_s->dpp_presence_announcement = dpp_build_presence_announcement(bi);
	if (!wpa_s->dpp_presence_announcement)
		return -1;
	wpa_s->dpp_chirp_iter = iter;
	wpa_s->dpp_chirp_round = 0;
	wpa_s->dpp_chirp_scan_done = 0;
	wpa_s->dpp_chirp_listen = listen_freq;

	return eloop_register_timeout(0, 0, wpas_dpp_chirp_next, wpa_s, NULL);
}


void wpas_dpp_chirp_stop(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->dpp_presence_announcement ||
	    wpa_s->dpp_reconfig_ssid) {
		offchannel_send_action_done(wpa_s);
		wpa_msg(wpa_s, MSG_INFO, DPP_EVENT_CHIRP_STOPPED);
	}
	wpa_s->dpp_chirp_bi = NULL;
	wpabuf_free(wpa_s->dpp_presence_announcement);
	wpa_s->dpp_presence_announcement = NULL;
	if (wpa_s->dpp_chirp_listen)
		wpas_dpp_listen_stop(wpa_s);
	wpa_s->dpp_chirp_listen = 0;
	wpa_s->dpp_chirp_freq = 0;
	os_free(wpa_s->dpp_chirp_freqs);
	wpa_s->dpp_chirp_freqs = NULL;
	eloop_cancel_timeout(wpas_dpp_chirp_next, wpa_s, NULL);
	eloop_cancel_timeout(wpas_dpp_chirp_timeout, wpa_s, NULL);
	if (wpa_s->scan_res_handler == wpas_dpp_chirp_scan_res_handler) {
		wpas_abort_ongoing_scan(wpa_s);
		wpa_s->scan_res_handler = NULL;
	}
}


int wpas_dpp_reconfig(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct wpa_ssid *ssid;
	int iter = 1;
	const char *pos;

	ssid = wpa_config_get_network(wpa_s->conf, atoi(cmd));
	if (!ssid || !ssid->dpp_connector || !ssid->dpp_netaccesskey ||
	    !ssid->dpp_csign) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Not a valid network profile for reconfiguration");
		return -1;
	}

	pos = os_strstr(cmd, " iter=");
	if (pos) {
		iter = atoi(pos + 6);
		if (iter <= 0)
			return -1;
	}

	if (wpa_s->dpp_auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Not ready to start reconfiguration - pending authentication exchange in progress");
		return -1;
	}

	dpp_free_reconfig_id(wpa_s->dpp_reconfig_id);
	wpa_s->dpp_reconfig_id = dpp_gen_reconfig_id(ssid->dpp_csign,
						     ssid->dpp_csign_len,
						     ssid->dpp_pp_key,
						     ssid->dpp_pp_key_len);
	if (!wpa_s->dpp_reconfig_id) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to generate E-id for reconfiguration");
		return -1;
	}
	if (wpa_s->wpa_state >= WPA_AUTHENTICATING) {
		wpa_printf(MSG_DEBUG, "DPP: Disconnect for reconfiguration");
		wpa_s->own_disconnect_req = 1;
		wpa_supplicant_deauthenticate(
			wpa_s, WLAN_REASON_DEAUTH_LEAVING);
	}
	wpas_dpp_chirp_stop(wpa_s);
	wpa_s->dpp_allowed_roles = DPP_CAPAB_ENROLLEE;
	wpa_s->dpp_qr_mutual = 0;
	wpa_s->dpp_reconfig_ssid = ssid;
	wpa_s->dpp_reconfig_ssid_id = ssid->id;
	wpa_s->dpp_chirp_iter = iter;
	wpa_s->dpp_chirp_round = 0;
	wpa_s->dpp_chirp_scan_done = 0;
	wpa_s->dpp_chirp_listen = 0;

	return eloop_register_timeout(0, 0, wpas_dpp_chirp_next, wpa_s, NULL);
}


static int wpas_dpp_build_conf_resp(struct wpa_supplicant *wpa_s,
				    struct dpp_authentication *auth, bool tcp)
{
	struct wpabuf *resp;

	resp = dpp_build_conf_resp(auth, auth->e_nonce, auth->curve->nonce_len,
				   auth->e_netrole, true);
	if (!resp)
		return -1;

	if (tcp) {
		auth->conf_resp_tcp = resp;
		return 0;
	}

	if (gas_server_set_resp(wpa_s->gas_server, auth->cert_resp_ctx,
				resp) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not find pending GAS response");
		wpabuf_free(resp);
		return -1;
	}
	auth->conf_resp = resp;
	return 0;
}


int wpas_dpp_ca_set(struct wpa_supplicant *wpa_s, const char *cmd)
{
	int peer = -1;
	const char *pos, *value;
	struct dpp_authentication *auth = wpa_s->dpp_auth;
	u8 *bin;
	size_t bin_len;
	struct wpabuf *buf;
	bool tcp = false;

	pos = os_strstr(cmd, " peer=");
	if (pos) {
		peer = atoi(pos + 6);
		if (!auth || !auth->waiting_cert ||
		    (auth->peer_bi &&
		     (unsigned int) peer != auth->peer_bi->id)) {
			auth = dpp_controller_get_auth(wpa_s->dpp, peer);
			tcp = true;
		}
	}

	if (!auth || !auth->waiting_cert) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No authentication exchange waiting for certificate information");
		return -1;
	}

	if (peer >= 0 &&
	    (!auth->peer_bi ||
	     (unsigned int) peer != auth->peer_bi->id) &&
	    (!auth->tmp_peer_bi ||
	     (unsigned int) peer != auth->tmp_peer_bi->id)) {
		wpa_printf(MSG_DEBUG, "DPP: Peer mismatch");
		return -1;
	}

	pos = os_strstr(cmd, " value=");
	if (!pos)
		return -1;
	value = pos + 7;

	pos = os_strstr(cmd, " name=");
	if (!pos)
		return -1;
	pos += 6;

	if (os_strncmp(pos, "status ", 7) == 0) {
		auth->force_conf_resp_status = atoi(value);
		return wpas_dpp_build_conf_resp(wpa_s, auth, tcp);
	}

	if (os_strncmp(pos, "trustedEapServerName ", 21) == 0) {
		os_free(auth->trusted_eap_server_name);
		auth->trusted_eap_server_name = os_strdup(value);
		return auth->trusted_eap_server_name ? 0 : -1;
	}

	bin = base64_decode(value, os_strlen(value), &bin_len);
	if (!bin)
		return -1;
	buf = wpabuf_alloc_copy(bin, bin_len);
	os_free(bin);

	if (os_strncmp(pos, "caCert ", 7) == 0) {
		wpabuf_free(auth->cacert);
		auth->cacert = buf;
		return 0;
	}

	if (os_strncmp(pos, "certBag ", 8) == 0) {
		wpabuf_free(auth->certbag);
		auth->certbag = buf;
		return wpas_dpp_build_conf_resp(wpa_s, auth, tcp);
	}

	wpabuf_free(buf);
	return -1;
}

#endif /* CONFIG_DPP2 */
