/*
 * hostapd / DPP integration
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/dpp.h"
#include "common/gas.h"
#include "common/wpa_ctrl.h"
#include "hostapd.h"
#include "ap_drv_ops.h"
#include "gas_query_ap.h"
#include "gas_serv.h"
#include "wpa_auth.h"
#include "dpp_hostapd.h"


static void hostapd_dpp_reply_wait_timeout(void *eloop_ctx, void *timeout_ctx);
static void hostapd_dpp_auth_success(struct hostapd_data *hapd, int initiator);
static void hostapd_dpp_init_timeout(void *eloop_ctx, void *timeout_ctx);
static int hostapd_dpp_auth_init_next(struct hostapd_data *hapd);

static const u8 broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


/**
 * hostapd_dpp_qr_code - Parse and add DPP bootstrapping info from a QR Code
 * @hapd: Pointer to hostapd_data
 * @cmd: DPP URI read from a QR Code
 * Returns: Identifier of the stored info or -1 on failure
 */
int hostapd_dpp_qr_code(struct hostapd_data *hapd, const char *cmd)
{
	struct dpp_bootstrap_info *bi;
	struct dpp_authentication *auth = hapd->dpp_auth;

	bi = dpp_add_qr_code(hapd->iface->interfaces->dpp, cmd);
	if (!bi)
		return -1;

	if (auth && auth->response_pending &&
	    dpp_notify_new_qr_code(auth, bi) == 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Sending out pending authentication response");
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
			" freq=%u type=%d",
			MAC2STR(auth->peer_mac_addr), auth->curr_freq,
			DPP_PA_AUTHENTICATION_RESP);
		hostapd_drv_send_action(hapd, auth->curr_freq, 0,
					auth->peer_mac_addr,
					wpabuf_head(hapd->dpp_auth->resp_msg),
					wpabuf_len(hapd->dpp_auth->resp_msg));
	}

	return bi->id;
}


static void hostapd_dpp_auth_resp_retry_timeout(void *eloop_ctx,
						void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct dpp_authentication *auth = hapd->dpp_auth;

	if (!auth || !auth->resp_msg)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Retry Authentication Response after timeout");
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d",
		MAC2STR(auth->peer_mac_addr), auth->curr_freq,
		DPP_PA_AUTHENTICATION_RESP);
	hostapd_drv_send_action(hapd, auth->curr_freq, 500, auth->peer_mac_addr,
				wpabuf_head(auth->resp_msg),
				wpabuf_len(auth->resp_msg));
}


static void hostapd_dpp_auth_resp_retry(struct hostapd_data *hapd)
{
	struct dpp_authentication *auth = hapd->dpp_auth;
	unsigned int wait_time, max_tries;

	if (!auth || !auth->resp_msg)
		return;

	if (hapd->dpp_resp_max_tries)
		max_tries = hapd->dpp_resp_max_tries;
	else
		max_tries = 5;
	auth->auth_resp_tries++;
	if (auth->auth_resp_tries >= max_tries) {
		wpa_printf(MSG_INFO,
			   "DPP: No confirm received from initiator - stopping exchange");
		hostapd_drv_send_action_cancel_wait(hapd);
		dpp_auth_deinit(hapd->dpp_auth);
		hapd->dpp_auth = NULL;
		return;
	}

	if (hapd->dpp_resp_retry_time)
		wait_time = hapd->dpp_resp_retry_time;
	else
		wait_time = 1000;
	wpa_printf(MSG_DEBUG,
		   "DPP: Schedule retransmission of Authentication Response frame in %u ms",
		wait_time);
	eloop_cancel_timeout(hostapd_dpp_auth_resp_retry_timeout, hapd, NULL);
	eloop_register_timeout(wait_time / 1000,
			       (wait_time % 1000) * 1000,
			       hostapd_dpp_auth_resp_retry_timeout, hapd, NULL);
}


void hostapd_dpp_tx_status(struct hostapd_data *hapd, const u8 *dst,
			   const u8 *data, size_t data_len, int ok)
{
	struct dpp_authentication *auth = hapd->dpp_auth;

	wpa_printf(MSG_DEBUG, "DPP: TX status: dst=" MACSTR " ok=%d",
		   MAC2STR(dst), ok);
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX_STATUS "dst=" MACSTR
		" result=%s", MAC2STR(dst), ok ? "SUCCESS" : "FAILED");

	if (!hapd->dpp_auth) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignore TX status since there is no ongoing authentication exchange");
		return;
	}

#ifdef CONFIG_DPP2
	if (auth->connect_on_tx_status) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Complete exchange on configuration result");
		dpp_auth_deinit(hapd->dpp_auth);
		hapd->dpp_auth = NULL;
		return;
	}
#endif /* CONFIG_DPP2 */

	if (hapd->dpp_auth->remove_on_tx_status) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Terminate authentication exchange due to an earlier error");
		eloop_cancel_timeout(hostapd_dpp_init_timeout, hapd, NULL);
		eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout,
				     hapd, NULL);
		eloop_cancel_timeout(hostapd_dpp_auth_resp_retry_timeout, hapd,
				     NULL);
		hostapd_drv_send_action_cancel_wait(hapd);
		dpp_auth_deinit(hapd->dpp_auth);
		hapd->dpp_auth = NULL;
		return;
	}

	if (hapd->dpp_auth_ok_on_ack)
		hostapd_dpp_auth_success(hapd, 1);

	if (!is_broadcast_ether_addr(dst) && !ok) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unicast DPP Action frame was not ACKed");
		if (auth->waiting_auth_resp) {
			/* In case of DPP Authentication Request frame, move to
			 * the next channel immediately. */
			hostapd_drv_send_action_cancel_wait(hapd);
			hostapd_dpp_auth_init_next(hapd);
			return;
		}
		if (auth->waiting_auth_conf) {
			hostapd_dpp_auth_resp_retry(hapd);
			return;
		}
	}

	if (!is_broadcast_ether_addr(dst) && auth->waiting_auth_resp && ok) {
		/* Allow timeout handling to stop iteration if no response is
		 * received from a peer that has ACKed a request. */
		auth->auth_req_ack = 1;
	}

	if (!hapd->dpp_auth_ok_on_ack && hapd->dpp_auth->neg_freq > 0 &&
	    hapd->dpp_auth->curr_freq != hapd->dpp_auth->neg_freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Move from curr_freq %u MHz to neg_freq %u MHz for response",
			   hapd->dpp_auth->curr_freq,
			   hapd->dpp_auth->neg_freq);
		hostapd_drv_send_action_cancel_wait(hapd);

		if (hapd->dpp_auth->neg_freq !=
		    (unsigned int) hapd->iface->freq && hapd->iface->freq > 0) {
			/* TODO: Listen operation on non-operating channel */
			wpa_printf(MSG_INFO,
				   "DPP: Listen operation on non-operating channel (%d MHz) is not yet supported (operating channel: %d MHz)",
				   hapd->dpp_auth->neg_freq, hapd->iface->freq);
		}
	}

	if (hapd->dpp_auth_ok_on_ack)
		hapd->dpp_auth_ok_on_ack = 0;
}


static void hostapd_dpp_reply_wait_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct dpp_authentication *auth = hapd->dpp_auth;
	unsigned int freq;
	struct os_reltime now, diff;
	unsigned int wait_time, diff_ms;

	if (!auth || !auth->waiting_auth_resp)
		return;

	wait_time = hapd->dpp_resp_wait_time ?
		hapd->dpp_resp_wait_time : 2000;
	os_get_reltime(&now);
	os_reltime_sub(&now, &hapd->dpp_last_init, &diff);
	diff_ms = diff.sec * 1000 + diff.usec / 1000;
	wpa_printf(MSG_DEBUG,
		   "DPP: Reply wait timeout - wait_time=%u diff_ms=%u",
		   wait_time, diff_ms);

	if (auth->auth_req_ack && diff_ms >= wait_time) {
		/* Peer ACK'ed Authentication Request frame, but did not reply
		 * with Authentication Response frame within two seconds. */
		wpa_printf(MSG_INFO,
			   "DPP: No response received from responder - stopping initiation attempt");
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_AUTH_INIT_FAILED);
		hostapd_drv_send_action_cancel_wait(hapd);
		hostapd_dpp_listen_stop(hapd);
		dpp_auth_deinit(auth);
		hapd->dpp_auth = NULL;
		return;
	}

	if (diff_ms >= wait_time) {
		/* Authentication Request frame was not ACK'ed and no reply
		 * was receiving within two seconds. */
		wpa_printf(MSG_DEBUG,
			   "DPP: Continue Initiator channel iteration");
		hostapd_drv_send_action_cancel_wait(hapd);
		hostapd_dpp_listen_stop(hapd);
		hostapd_dpp_auth_init_next(hapd);
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
	hapd->dpp_in_response_listen = 1;

	if (freq != (unsigned int) hapd->iface->freq && hapd->iface->freq > 0) {
		/* TODO: Listen operation on non-operating channel */
		wpa_printf(MSG_INFO,
			   "DPP: Listen operation on non-operating channel (%d MHz) is not yet supported (operating channel: %d MHz)",
			   freq, hapd->iface->freq);
	}

	eloop_register_timeout(wait_time / 1000, (wait_time % 1000) * 1000,
			       hostapd_dpp_reply_wait_timeout, hapd, NULL);
}


static void hostapd_dpp_set_testing_options(struct hostapd_data *hapd,
					    struct dpp_authentication *auth)
{
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->dpp_config_obj_override)
		auth->config_obj_override =
			os_strdup(hapd->dpp_config_obj_override);
	if (hapd->dpp_discovery_override)
		auth->discovery_override =
			os_strdup(hapd->dpp_discovery_override);
	if (hapd->dpp_groups_override)
		auth->groups_override = os_strdup(hapd->dpp_groups_override);
	auth->ignore_netaccesskey_mismatch =
		hapd->dpp_ignore_netaccesskey_mismatch;
#endif /* CONFIG_TESTING_OPTIONS */
}


static void hostapd_dpp_init_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;

	if (!hapd->dpp_auth)
		return;
	wpa_printf(MSG_DEBUG, "DPP: Retry initiation after timeout");
	hostapd_dpp_auth_init_next(hapd);
}


static int hostapd_dpp_auth_init_next(struct hostapd_data *hapd)
{
	struct dpp_authentication *auth = hapd->dpp_auth;
	const u8 *dst;
	unsigned int wait_time, max_wait_time, freq, max_tries, used;
	struct os_reltime now, diff;

	if (!auth)
		return -1;

	if (auth->freq_idx == 0)
		os_get_reltime(&hapd->dpp_init_iter_start);

	if (auth->freq_idx >= auth->num_freq) {
		auth->num_freq_iters++;
		if (hapd->dpp_init_max_tries)
			max_tries = hapd->dpp_init_max_tries;
		else
			max_tries = 5;
		if (auth->num_freq_iters >= max_tries || auth->auth_req_ack) {
			wpa_printf(MSG_INFO,
				   "DPP: No response received from responder - stopping initiation attempt");
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				DPP_EVENT_AUTH_INIT_FAILED);
			eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout,
					     hapd, NULL);
			hostapd_drv_send_action_cancel_wait(hapd);
			dpp_auth_deinit(hapd->dpp_auth);
			hapd->dpp_auth = NULL;
			return -1;
		}
		auth->freq_idx = 0;
		eloop_cancel_timeout(hostapd_dpp_init_timeout, hapd, NULL);
		if (hapd->dpp_init_retry_time)
			wait_time = hapd->dpp_init_retry_time;
		else
			wait_time = 10000;
		os_get_reltime(&now);
		os_reltime_sub(&now, &hapd->dpp_init_iter_start, &diff);
		used = diff.sec * 1000 + diff.usec / 1000;
		if (used > wait_time)
			wait_time = 0;
		else
			wait_time -= used;
		wpa_printf(MSG_DEBUG, "DPP: Next init attempt in %u ms",
			   wait_time);
		eloop_register_timeout(wait_time / 1000,
				       (wait_time % 1000) * 1000,
				       hostapd_dpp_init_timeout, hapd,
				       NULL);
		return 0;
	}
	freq = auth->freq[auth->freq_idx++];
	auth->curr_freq = freq;

	if (is_zero_ether_addr(auth->peer_bi->mac_addr))
		dst = broadcast;
	else
		dst = auth->peer_bi->mac_addr;
	hapd->dpp_auth_ok_on_ack = 0;
	eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout, hapd, NULL);
	wait_time = 2000; /* TODO: hapd->max_remain_on_chan; */
	max_wait_time = hapd->dpp_resp_wait_time ?
		hapd->dpp_resp_wait_time : 2000;
	if (wait_time > max_wait_time)
		wait_time = max_wait_time;
	wait_time += 10; /* give the driver some extra time to complete */
	eloop_register_timeout(wait_time / 1000, (wait_time % 1000) * 1000,
			       hostapd_dpp_reply_wait_timeout, hapd, NULL);
	wait_time -= 10;
	if (auth->neg_freq > 0 && freq != auth->neg_freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Initiate on %u MHz and move to neg_freq %u MHz for response",
			   freq, auth->neg_freq);
	}
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d",
		MAC2STR(dst), freq, DPP_PA_AUTHENTICATION_REQ);
	auth->auth_req_ack = 0;
	os_get_reltime(&hapd->dpp_last_init);
	return hostapd_drv_send_action(hapd, freq, wait_time,
				       dst,
				       wpabuf_head(hapd->dpp_auth->req_msg),
				       wpabuf_len(hapd->dpp_auth->req_msg));
}


int hostapd_dpp_auth_init(struct hostapd_data *hapd, const char *cmd)
{
	const char *pos;
	struct dpp_bootstrap_info *peer_bi, *own_bi = NULL;
	u8 allowed_roles = DPP_CAPAB_CONFIGURATOR;
	unsigned int neg_freq = 0;

	pos = os_strstr(cmd, " peer=");
	if (!pos)
		return -1;
	pos += 6;
	peer_bi = dpp_bootstrap_get_id(hapd->iface->interfaces->dpp, atoi(pos));
	if (!peer_bi) {
		wpa_printf(MSG_INFO,
			   "DPP: Could not find bootstrapping info for the identified peer");
		return -1;
	}

	pos = os_strstr(cmd, " own=");
	if (pos) {
		pos += 5;
		own_bi = dpp_bootstrap_get_id(hapd->iface->interfaces->dpp,
					      atoi(pos));
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

	pos = os_strstr(cmd, " neg_freq=");
	if (pos)
		neg_freq = atoi(pos + 10);

	if (hapd->dpp_auth) {
		eloop_cancel_timeout(hostapd_dpp_init_timeout, hapd, NULL);
		eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout,
				     hapd, NULL);
		eloop_cancel_timeout(hostapd_dpp_auth_resp_retry_timeout, hapd,
				     NULL);
		hostapd_drv_send_action_cancel_wait(hapd);
		dpp_auth_deinit(hapd->dpp_auth);
	}

	hapd->dpp_auth = dpp_auth_init(hapd->msg_ctx, peer_bi, own_bi,
				       allowed_roles, neg_freq,
				       hapd->iface->hw_features,
				       hapd->iface->num_hw_features);
	if (!hapd->dpp_auth)
		goto fail;
	hostapd_dpp_set_testing_options(hapd, hapd->dpp_auth);
	if (dpp_set_configurator(hapd->iface->interfaces->dpp, hapd->msg_ctx,
				 hapd->dpp_auth, cmd) < 0) {
		dpp_auth_deinit(hapd->dpp_auth);
		hapd->dpp_auth = NULL;
		goto fail;
	}

	hapd->dpp_auth->neg_freq = neg_freq;

	if (!is_zero_ether_addr(peer_bi->mac_addr))
		os_memcpy(hapd->dpp_auth->peer_mac_addr, peer_bi->mac_addr,
			  ETH_ALEN);

	return hostapd_dpp_auth_init_next(hapd);
fail:
	return -1;
}


int hostapd_dpp_listen(struct hostapd_data *hapd, const char *cmd)
{
	int freq;

	freq = atoi(cmd);
	if (freq <= 0)
		return -1;

	if (os_strstr(cmd, " role=configurator"))
		hapd->dpp_allowed_roles = DPP_CAPAB_CONFIGURATOR;
	else if (os_strstr(cmd, " role=enrollee"))
		hapd->dpp_allowed_roles = DPP_CAPAB_ENROLLEE;
	else
		hapd->dpp_allowed_roles = DPP_CAPAB_CONFIGURATOR |
			DPP_CAPAB_ENROLLEE;
	hapd->dpp_qr_mutual = os_strstr(cmd, " qr=mutual") != NULL;

	if (freq != hapd->iface->freq && hapd->iface->freq > 0) {
		/* TODO: Listen operation on non-operating channel */
		wpa_printf(MSG_INFO,
			   "DPP: Listen operation on non-operating channel (%d MHz) is not yet supported (operating channel: %d MHz)",
			   freq, hapd->iface->freq);
		return -1;
	}

	return 0;
}


void hostapd_dpp_listen_stop(struct hostapd_data *hapd)
{
	/* TODO: Stop listen operation on non-operating channel */
}


static void hostapd_dpp_rx_auth_req(struct hostapd_data *hapd, const u8 *src,
				    const u8 *hdr, const u8 *buf, size_t len,
				    unsigned int freq)
{
	const u8 *r_bootstrap, *i_bootstrap;
	u16 r_bootstrap_len, i_bootstrap_len;
	struct dpp_bootstrap_info *own_bi = NULL, *peer_bi = NULL;

	if (!hapd->iface->interfaces->dpp)
		return;

	wpa_printf(MSG_DEBUG, "DPP: Authentication Request from " MACSTR,
		   MAC2STR(src));

	r_bootstrap = dpp_get_attr(buf, len, DPP_ATTR_R_BOOTSTRAP_KEY_HASH,
				   &r_bootstrap_len);
	if (!r_bootstrap || r_bootstrap_len != SHA256_MAC_LEN) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Responder Bootstrapping Key Hash attribute");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Responder Bootstrapping Key Hash",
		    r_bootstrap, r_bootstrap_len);

	i_bootstrap = dpp_get_attr(buf, len, DPP_ATTR_I_BOOTSTRAP_KEY_HASH,
				   &i_bootstrap_len);
	if (!i_bootstrap || i_bootstrap_len != SHA256_MAC_LEN) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Initiator Bootstrapping Key Hash attribute");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Initiator Bootstrapping Key Hash",
		    i_bootstrap, i_bootstrap_len);

	/* Try to find own and peer bootstrapping key matches based on the
	 * received hash values */
	dpp_bootstrap_find_pair(hapd->iface->interfaces->dpp, i_bootstrap,
				r_bootstrap, &own_bi, &peer_bi);
#ifdef CONFIG_DPP2
	if (!own_bi) {
		if (dpp_relay_rx_action(hapd->iface->interfaces->dpp,
					src, hdr, buf, len, freq, i_bootstrap,
					r_bootstrap) == 0)
			return;
	}
#endif /* CONFIG_DPP2 */
	if (!own_bi) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"No matching own bootstrapping key found - ignore message");
		return;
	}

	if (hapd->dpp_auth) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Already in DPP authentication exchange - ignore new one");
		return;
	}

	hapd->dpp_auth_ok_on_ack = 0;
	hapd->dpp_auth = dpp_auth_req_rx(hapd->msg_ctx, hapd->dpp_allowed_roles,
					 hapd->dpp_qr_mutual,
					 peer_bi, own_bi, freq, hdr, buf, len);
	if (!hapd->dpp_auth) {
		wpa_printf(MSG_DEBUG, "DPP: No response generated");
		return;
	}
	hostapd_dpp_set_testing_options(hapd, hapd->dpp_auth);
	if (dpp_set_configurator(hapd->iface->interfaces->dpp, hapd->msg_ctx,
				 hapd->dpp_auth,
				 hapd->dpp_configurator_params) < 0) {
		dpp_auth_deinit(hapd->dpp_auth);
		hapd->dpp_auth = NULL;
		return;
	}
	os_memcpy(hapd->dpp_auth->peer_mac_addr, src, ETH_ALEN);

	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d",
		MAC2STR(src), hapd->dpp_auth->curr_freq,
		DPP_PA_AUTHENTICATION_RESP);
	hostapd_drv_send_action(hapd, hapd->dpp_auth->curr_freq, 0,
				src, wpabuf_head(hapd->dpp_auth->resp_msg),
				wpabuf_len(hapd->dpp_auth->resp_msg));
}


static void hostapd_dpp_handle_config_obj(struct hostapd_data *hapd,
					  struct dpp_authentication *auth)
{
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_RECEIVED);
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONFOBJ_AKM "%s",
		dpp_akm_str(auth->akm));
	if (auth->ssid_len)
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONFOBJ_SSID "%s",
			wpa_ssid_txt(auth->ssid, auth->ssid_len));
	if (auth->connector) {
		/* TODO: Save the Connector and consider using a command
		 * to fetch the value instead of sending an event with
		 * it. The Connector could end up being larger than what
		 * most clients are ready to receive as an event
		 * message. */
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONNECTOR "%s",
			auth->connector);
	} else if (auth->passphrase[0]) {
		char hex[64 * 2 + 1];

		wpa_snprintf_hex(hex, sizeof(hex),
				 (const u8 *) auth->passphrase,
				 os_strlen(auth->passphrase));
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONFOBJ_PASS "%s",
			hex);
	} else if (auth->psk_set) {
		char hex[PMK_LEN * 2 + 1];

		wpa_snprintf_hex(hex, sizeof(hex), auth->psk, PMK_LEN);
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONFOBJ_PSK "%s",
			hex);
	}
	if (auth->c_sign_key) {
		char *hex;
		size_t hexlen;

		hexlen = 2 * wpabuf_len(auth->c_sign_key) + 1;
		hex = os_malloc(hexlen);
		if (hex) {
			wpa_snprintf_hex(hex, hexlen,
					 wpabuf_head(auth->c_sign_key),
					 wpabuf_len(auth->c_sign_key));
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				DPP_EVENT_C_SIGN_KEY "%s", hex);
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
				wpa_msg(hapd->msg_ctx, MSG_INFO,
					DPP_EVENT_NET_ACCESS_KEY "%s %lu", hex,
					(unsigned long)
					auth->net_access_key_expiry);
			else
				wpa_msg(hapd->msg_ctx, MSG_INFO,
					DPP_EVENT_NET_ACCESS_KEY "%s", hex);
			os_free(hex);
		}
	}
}


static void hostapd_dpp_gas_resp_cb(void *ctx, const u8 *addr, u8 dialog_token,
				    enum gas_query_ap_result result,
				    const struct wpabuf *adv_proto,
				    const struct wpabuf *resp, u16 status_code)
{
	struct hostapd_data *hapd = ctx;
	const u8 *pos;
	struct dpp_authentication *auth = hapd->dpp_auth;
	enum dpp_status_error status = DPP_STATUS_CONFIG_REJECTED;

	if (!auth || !auth->auth_success) {
		wpa_printf(MSG_DEBUG, "DPP: No matching exchange in progress");
		return;
	}
	if (!resp || status_code != WLAN_STATUS_SUCCESS) {
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

	if (dpp_conf_resp_rx(auth, resp) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Configuration attempt failed");
		goto fail;
	}

	hostapd_dpp_handle_config_obj(hapd, auth);
	status = DPP_STATUS_OK;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_REJECT_CONFIG) {
		wpa_printf(MSG_INFO, "DPP: TESTING - Reject Config Object");
		status = DPP_STATUS_CONFIG_REJECTED;
	}
#endif /* CONFIG_TESTING_OPTIONS */
fail:
	if (status != DPP_STATUS_OK)
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_FAILED);
#ifdef CONFIG_DPP2
	if (auth->peer_version >= 2 &&
	    auth->conf_resp_status == DPP_STATUS_OK) {
		struct wpabuf *msg;

		wpa_printf(MSG_DEBUG, "DPP: Send DPP Configuration Result");
		msg = dpp_build_conf_result(auth, status);
		if (!msg)
			goto fail2;

		wpa_msg(hapd->msg_ctx, MSG_INFO,
			DPP_EVENT_TX "dst=" MACSTR " freq=%u type=%d",
			MAC2STR(addr), auth->curr_freq,
			DPP_PA_CONFIGURATION_RESULT);
		hostapd_drv_send_action(hapd, auth->curr_freq, 0,
					addr, wpabuf_head(msg),
					wpabuf_len(msg));
		wpabuf_free(msg);

		/* This exchange will be terminated in the TX status handler */
		auth->connect_on_tx_status = 1;
		return;
	}
fail2:
#endif /* CONFIG_DPP2 */
	dpp_auth_deinit(hapd->dpp_auth);
	hapd->dpp_auth = NULL;
}


static void hostapd_dpp_start_gas_client(struct hostapd_data *hapd)
{
	struct dpp_authentication *auth = hapd->dpp_auth;
	struct wpabuf *buf;
	char json[100];
	int res;
	int netrole_ap = 1;

	os_snprintf(json, sizeof(json),
		    "{\"name\":\"Test\","
		    "\"wi-fi_tech\":\"infra\","
		    "\"netRole\":\"%s\"}",
		    netrole_ap ? "ap" : "sta");
	wpa_printf(MSG_DEBUG, "DPP: GAS Config Attributes: %s", json);

	buf = dpp_build_conf_req(auth, json);
	if (!buf) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No configuration request data available");
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: GAS request to " MACSTR " (freq %u MHz)",
		   MAC2STR(auth->peer_mac_addr), auth->curr_freq);

	res = gas_query_ap_req(hapd->gas, auth->peer_mac_addr, auth->curr_freq,
			       buf, hostapd_dpp_gas_resp_cb, hapd);
	if (res < 0) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			"GAS: Failed to send Query Request");
		wpabuf_free(buf);
	} else {
		wpa_printf(MSG_DEBUG,
			   "DPP: GAS query started with dialog token %u", res);
	}
}


static void hostapd_dpp_auth_success(struct hostapd_data *hapd, int initiator)
{
	wpa_printf(MSG_DEBUG, "DPP: Authentication succeeded");
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_AUTH_SUCCESS "init=%d",
		initiator);
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_AUTH_CONF) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at Authentication Confirm");
		if (hapd->dpp_auth->configurator) {
			/* Prevent GAS response */
			hapd->dpp_auth->auth_success = 0;
		}
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (!hapd->dpp_auth->configurator)
		hostapd_dpp_start_gas_client(hapd);
}


static void hostapd_dpp_rx_auth_resp(struct hostapd_data *hapd, const u8 *src,
				     const u8 *hdr, const u8 *buf, size_t len,
				     unsigned int freq)
{
	struct dpp_authentication *auth = hapd->dpp_auth;
	struct wpabuf *msg;

	wpa_printf(MSG_DEBUG, "DPP: Authentication Response from " MACSTR,
		   MAC2STR(src));

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

	eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout, hapd, NULL);

	if (auth->curr_freq != freq && auth->neg_freq == freq) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Responder accepted request for different negotiation channel");
		auth->curr_freq = freq;
	}

	eloop_cancel_timeout(hostapd_dpp_init_timeout, hapd, NULL);
	msg = dpp_auth_resp_rx(auth, hdr, buf, len);
	if (!msg) {
		if (auth->auth_resp_status == DPP_STATUS_RESPONSE_PENDING) {
			wpa_printf(MSG_DEBUG, "DPP: Wait for full response");
			return;
		}
		wpa_printf(MSG_DEBUG, "DPP: No confirm generated");
		return;
	}
	os_memcpy(auth->peer_mac_addr, src, ETH_ALEN);

	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d", MAC2STR(src), auth->curr_freq,
		DPP_PA_AUTHENTICATION_CONF);
	hostapd_drv_send_action(hapd, auth->curr_freq, 0, src,
				wpabuf_head(msg), wpabuf_len(msg));
	wpabuf_free(msg);
	hapd->dpp_auth_ok_on_ack = 1;
}


static void hostapd_dpp_rx_auth_conf(struct hostapd_data *hapd, const u8 *src,
				     const u8 *hdr, const u8 *buf, size_t len)
{
	struct dpp_authentication *auth = hapd->dpp_auth;

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

	if (dpp_auth_conf_rx(auth, hdr, buf, len) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Authentication failed");
		return;
	}

	hostapd_dpp_auth_success(hapd, 0);
}


#ifdef CONFIG_DPP2

static void hostapd_dpp_config_result_wait_timeout(void *eloop_ctx,
						   void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct dpp_authentication *auth = hapd->dpp_auth;

	if (!auth || !auth->waiting_conf_result)
		return;

	wpa_printf(MSG_DEBUG,
		   "DPP: Timeout while waiting for Configuration Result");
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_FAILED);
	dpp_auth_deinit(auth);
	hapd->dpp_auth = NULL;
}


static void hostapd_dpp_rx_conf_result(struct hostapd_data *hapd, const u8 *src,
				       const u8 *hdr, const u8 *buf, size_t len)
{
	struct dpp_authentication *auth = hapd->dpp_auth;
	enum dpp_status_error status;

	wpa_printf(MSG_DEBUG, "DPP: Configuration Result from " MACSTR,
		   MAC2STR(src));

	if (!auth || !auth->waiting_conf_result) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No DPP Configuration waiting for result - drop");
		return;
	}

	if (os_memcmp(src, auth->peer_mac_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "DPP: MAC address mismatch (expected "
			   MACSTR ") - drop", MAC2STR(auth->peer_mac_addr));
		return;
	}

	status = dpp_conf_result_rx(auth, hdr, buf, len);

	hostapd_drv_send_action_cancel_wait(hapd);
	hostapd_dpp_listen_stop(hapd);
	if (status == DPP_STATUS_OK)
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_SENT);
	else
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_FAILED);
	dpp_auth_deinit(auth);
	hapd->dpp_auth = NULL;
	eloop_cancel_timeout(hostapd_dpp_config_result_wait_timeout, hapd,
			     NULL);
}

#endif /* CONFIG_DPP2 */


static void hostapd_dpp_send_peer_disc_resp(struct hostapd_data *hapd,
					    const u8 *src, unsigned int freq,
					    u8 trans_id,
					    enum dpp_status_error status)
{
	struct wpabuf *msg;

	msg = dpp_alloc_msg(DPP_PA_PEER_DISCOVERY_RESP,
			    5 + 5 + 4 + os_strlen(hapd->conf->dpp_connector));
	if (!msg)
		return;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_TRANSACTION_ID_PEER_DISC_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Transaction ID");
		goto skip_trans_id;
	}
	if (dpp_test == DPP_TEST_INVALID_TRANSACTION_ID_PEER_DISC_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Transaction ID");
		trans_id ^= 0x01;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Transaction ID */
	wpabuf_put_le16(msg, DPP_ATTR_TRANSACTION_ID);
	wpabuf_put_le16(msg, 1);
	wpabuf_put_u8(msg, trans_id);

#ifdef CONFIG_TESTING_OPTIONS
skip_trans_id:
	if (dpp_test == DPP_TEST_NO_STATUS_PEER_DISC_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Status");
		goto skip_status;
	}
	if (dpp_test == DPP_TEST_INVALID_STATUS_PEER_DISC_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Status");
		status = 254;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* DPP Status */
	wpabuf_put_le16(msg, DPP_ATTR_STATUS);
	wpabuf_put_le16(msg, 1);
	wpabuf_put_u8(msg, status);

#ifdef CONFIG_TESTING_OPTIONS
skip_status:
	if (dpp_test == DPP_TEST_NO_CONNECTOR_PEER_DISC_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Connector");
		goto skip_connector;
	}
	if (status == DPP_STATUS_OK &&
	    dpp_test == DPP_TEST_INVALID_CONNECTOR_PEER_DISC_RESP) {
		char *connector;

		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Connector");
		connector = dpp_corrupt_connector_signature(
			hapd->conf->dpp_connector);
		if (!connector) {
			wpabuf_free(msg);
			return;
		}
		wpabuf_put_le16(msg, DPP_ATTR_CONNECTOR);
		wpabuf_put_le16(msg, os_strlen(connector));
		wpabuf_put_str(msg, connector);
		os_free(connector);
		goto skip_connector;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* DPP Connector */
	if (status == DPP_STATUS_OK) {
		wpabuf_put_le16(msg, DPP_ATTR_CONNECTOR);
		wpabuf_put_le16(msg, os_strlen(hapd->conf->dpp_connector));
		wpabuf_put_str(msg, hapd->conf->dpp_connector);
	}

#ifdef CONFIG_TESTING_OPTIONS
skip_connector:
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_printf(MSG_DEBUG, "DPP: Send Peer Discovery Response to " MACSTR
		   " status=%d", MAC2STR(src), status);
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d status=%d", MAC2STR(src), freq,
		DPP_PA_PEER_DISCOVERY_RESP, status);
	hostapd_drv_send_action(hapd, freq, 0, src,
				wpabuf_head(msg), wpabuf_len(msg));
	wpabuf_free(msg);
}


static void hostapd_dpp_rx_peer_disc_req(struct hostapd_data *hapd,
					 const u8 *src,
					 const u8 *buf, size_t len,
					 unsigned int freq)
{
	const u8 *connector, *trans_id;
	u16 connector_len, trans_id_len;
	struct os_time now;
	struct dpp_introduction intro;
	os_time_t expire;
	int expiration;
	enum dpp_status_error res;

	wpa_printf(MSG_DEBUG, "DPP: Peer Discovery Request from " MACSTR,
		   MAC2STR(src));
	if (!hapd->wpa_auth ||
	    !(hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) ||
	    !(hapd->conf->wpa & WPA_PROTO_RSN)) {
		wpa_printf(MSG_DEBUG, "DPP: DPP AKM not in use");
		return;
	}

	if (!hapd->conf->dpp_connector || !hapd->conf->dpp_netaccesskey ||
	    !hapd->conf->dpp_csign) {
		wpa_printf(MSG_DEBUG, "DPP: No own Connector/keys set");
		return;
	}

	os_get_time(&now);

	if (hapd->conf->dpp_netaccesskey_expiry &&
	    (os_time_t) hapd->conf->dpp_netaccesskey_expiry < now.sec) {
		wpa_printf(MSG_INFO, "DPP: Own netAccessKey expired");
		return;
	}

	trans_id = dpp_get_attr(buf, len, DPP_ATTR_TRANSACTION_ID,
			       &trans_id_len);
	if (!trans_id || trans_id_len != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Peer did not include Transaction ID");
		return;
	}

	connector = dpp_get_attr(buf, len, DPP_ATTR_CONNECTOR, &connector_len);
	if (!connector) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Peer did not include its Connector");
		return;
	}

	res = dpp_peer_intro(&intro, hapd->conf->dpp_connector,
			     wpabuf_head(hapd->conf->dpp_netaccesskey),
			     wpabuf_len(hapd->conf->dpp_netaccesskey),
			     wpabuf_head(hapd->conf->dpp_csign),
			     wpabuf_len(hapd->conf->dpp_csign),
			     connector, connector_len, &expire);
	if (res == 255) {
		wpa_printf(MSG_INFO,
			   "DPP: Network Introduction protocol resulted in internal failure (peer "
			   MACSTR ")", MAC2STR(src));
		return;
	}
	if (res != DPP_STATUS_OK) {
		wpa_printf(MSG_INFO,
			   "DPP: Network Introduction protocol resulted in failure (peer "
			   MACSTR " status %d)", MAC2STR(src), res);
		hostapd_dpp_send_peer_disc_resp(hapd, src, freq, trans_id[0],
						res);
		return;
	}

	if (!expire || (os_time_t) hapd->conf->dpp_netaccesskey_expiry < expire)
		expire = hapd->conf->dpp_netaccesskey_expiry;
	if (expire)
		expiration = expire - now.sec;
	else
		expiration = 0;

	if (wpa_auth_pmksa_add2(hapd->wpa_auth, src, intro.pmk, intro.pmk_len,
				intro.pmkid, expiration,
				WPA_KEY_MGMT_DPP) < 0) {
		wpa_printf(MSG_ERROR, "DPP: Failed to add PMKSA cache entry");
		return;
	}

	hostapd_dpp_send_peer_disc_resp(hapd, src, freq, trans_id[0],
					DPP_STATUS_OK);
}


static void
hostapd_dpp_rx_pkex_exchange_req(struct hostapd_data *hapd, const u8 *src,
				 const u8 *buf, size_t len,
				 unsigned int freq)
{
	struct wpabuf *msg;

	wpa_printf(MSG_DEBUG, "DPP: PKEX Exchange Request from " MACSTR,
		   MAC2STR(src));

	/* TODO: Support multiple PKEX codes by iterating over all the enabled
	 * values here */

	if (!hapd->dpp_pkex_code || !hapd->dpp_pkex_bi) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No PKEX code configured - ignore request");
		return;
	}

	if (hapd->dpp_pkex) {
		/* TODO: Support parallel operations */
		wpa_printf(MSG_DEBUG,
			   "DPP: Already in PKEX session - ignore new request");
		return;
	}

	hapd->dpp_pkex = dpp_pkex_rx_exchange_req(hapd->msg_ctx,
						  hapd->dpp_pkex_bi,
						  hapd->own_addr, src,
						  hapd->dpp_pkex_identifier,
						  hapd->dpp_pkex_code,
						  buf, len);
	if (!hapd->dpp_pkex) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to process the request - ignore it");
		return;
	}

	msg = hapd->dpp_pkex->exchange_resp;
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d", MAC2STR(src), freq,
		DPP_PA_PKEX_EXCHANGE_RESP);
	hostapd_drv_send_action(hapd, freq, 0, src,
				wpabuf_head(msg), wpabuf_len(msg));
	if (hapd->dpp_pkex->failed) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Terminate PKEX exchange due to an earlier error");
		if (hapd->dpp_pkex->t > hapd->dpp_pkex->own_bi->pkex_t)
			hapd->dpp_pkex->own_bi->pkex_t = hapd->dpp_pkex->t;
		dpp_pkex_free(hapd->dpp_pkex);
		hapd->dpp_pkex = NULL;
	}
}


static void
hostapd_dpp_rx_pkex_exchange_resp(struct hostapd_data *hapd, const u8 *src,
				  const u8 *buf, size_t len, unsigned int freq)
{
	struct wpabuf *msg;

	wpa_printf(MSG_DEBUG, "DPP: PKEX Exchange Response from " MACSTR,
		   MAC2STR(src));

	/* TODO: Support multiple PKEX codes by iterating over all the enabled
	 * values here */

	if (!hapd->dpp_pkex || !hapd->dpp_pkex->initiator ||
	    hapd->dpp_pkex->exchange_done) {
		wpa_printf(MSG_DEBUG, "DPP: No matching PKEX session");
		return;
	}

	msg = dpp_pkex_rx_exchange_resp(hapd->dpp_pkex, src, buf, len);
	if (!msg) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to process the response");
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Send PKEX Commit-Reveal Request to " MACSTR,
		   MAC2STR(src));

	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d", MAC2STR(src), freq,
		DPP_PA_PKEX_COMMIT_REVEAL_REQ);
	hostapd_drv_send_action(hapd, freq, 0, src,
				wpabuf_head(msg), wpabuf_len(msg));
	wpabuf_free(msg);
}


static void
hostapd_dpp_rx_pkex_commit_reveal_req(struct hostapd_data *hapd, const u8 *src,
				      const u8 *hdr, const u8 *buf, size_t len,
				      unsigned int freq)
{
	struct wpabuf *msg;
	struct dpp_pkex *pkex = hapd->dpp_pkex;
	struct dpp_bootstrap_info *bi;

	wpa_printf(MSG_DEBUG, "DPP: PKEX Commit-Reveal Request from " MACSTR,
		   MAC2STR(src));

	if (!pkex || pkex->initiator || !pkex->exchange_done) {
		wpa_printf(MSG_DEBUG, "DPP: No matching PKEX session");
		return;
	}

	msg = dpp_pkex_rx_commit_reveal_req(pkex, hdr, buf, len);
	if (!msg) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to process the request");
		if (hapd->dpp_pkex->failed) {
			wpa_printf(MSG_DEBUG, "DPP: Terminate PKEX exchange");
			if (hapd->dpp_pkex->t > hapd->dpp_pkex->own_bi->pkex_t)
				hapd->dpp_pkex->own_bi->pkex_t =
					hapd->dpp_pkex->t;
			dpp_pkex_free(hapd->dpp_pkex);
			hapd->dpp_pkex = NULL;
		}
		return;
	}

	wpa_printf(MSG_DEBUG, "DPP: Send PKEX Commit-Reveal Response to "
		   MACSTR, MAC2STR(src));

	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
		" freq=%u type=%d", MAC2STR(src), freq,
		DPP_PA_PKEX_COMMIT_REVEAL_RESP);
	hostapd_drv_send_action(hapd, freq, 0, src,
				wpabuf_head(msg), wpabuf_len(msg));
	wpabuf_free(msg);

	bi = dpp_pkex_finish(hapd->iface->interfaces->dpp, pkex, src, freq);
	if (!bi)
		return;
	hapd->dpp_pkex = NULL;
}


static void
hostapd_dpp_rx_pkex_commit_reveal_resp(struct hostapd_data *hapd, const u8 *src,
				       const u8 *hdr, const u8 *buf, size_t len,
				       unsigned int freq)
{
	int res;
	struct dpp_bootstrap_info *bi;
	struct dpp_pkex *pkex = hapd->dpp_pkex;
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

	bi = dpp_pkex_finish(hapd->iface->interfaces->dpp, pkex, src, freq);
	if (!bi)
		return;
	hapd->dpp_pkex = NULL;

	os_snprintf(cmd, sizeof(cmd), " peer=%u %s",
		    bi->id,
		    hapd->dpp_pkex_auth_cmd ? hapd->dpp_pkex_auth_cmd : "");
	wpa_printf(MSG_DEBUG,
		   "DPP: Start authentication after PKEX with parameters: %s",
		   cmd);
	if (hostapd_dpp_auth_init(hapd, cmd) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Authentication initialization failed");
		return;
	}
}


void hostapd_dpp_rx_action(struct hostapd_data *hapd, const u8 *src,
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
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_RX "src=" MACSTR
			" freq=%u type=%d ignore=unsupported-crypto-suite",
			MAC2STR(src), freq, type);
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Received message attributes", buf, len);
	if (dpp_check_attrs(buf, len) < 0) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_RX "src=" MACSTR
			" freq=%u type=%d ignore=invalid-attributes",
			MAC2STR(src), freq, type);
		return;
	}
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_RX "src=" MACSTR
		" freq=%u type=%d", MAC2STR(src), freq, type);

#ifdef CONFIG_DPP2
	if (dpp_relay_rx_action(hapd->iface->interfaces->dpp,
				src, hdr, buf, len, freq, NULL, NULL) == 0)
		return;
#endif /* CONFIG_DPP2 */

	switch (type) {
	case DPP_PA_AUTHENTICATION_REQ:
		hostapd_dpp_rx_auth_req(hapd, src, hdr, buf, len, freq);
		break;
	case DPP_PA_AUTHENTICATION_RESP:
		hostapd_dpp_rx_auth_resp(hapd, src, hdr, buf, len, freq);
		break;
	case DPP_PA_AUTHENTICATION_CONF:
		hostapd_dpp_rx_auth_conf(hapd, src, hdr, buf, len);
		break;
	case DPP_PA_PEER_DISCOVERY_REQ:
		hostapd_dpp_rx_peer_disc_req(hapd, src, buf, len, freq);
		break;
	case DPP_PA_PKEX_EXCHANGE_REQ:
		hostapd_dpp_rx_pkex_exchange_req(hapd, src, buf, len, freq);
		break;
	case DPP_PA_PKEX_EXCHANGE_RESP:
		hostapd_dpp_rx_pkex_exchange_resp(hapd, src, buf, len, freq);
		break;
	case DPP_PA_PKEX_COMMIT_REVEAL_REQ:
		hostapd_dpp_rx_pkex_commit_reveal_req(hapd, src, hdr, buf, len,
						      freq);
		break;
	case DPP_PA_PKEX_COMMIT_REVEAL_RESP:
		hostapd_dpp_rx_pkex_commit_reveal_resp(hapd, src, hdr, buf, len,
						       freq);
		break;
#ifdef CONFIG_DPP2
	case DPP_PA_CONFIGURATION_RESULT:
		hostapd_dpp_rx_conf_result(hapd, src, hdr, buf, len);
		break;
#endif /* CONFIG_DPP2 */
	default:
		wpa_printf(MSG_DEBUG,
			   "DPP: Ignored unsupported frame subtype %d", type);
		break;
	}

	if (hapd->dpp_pkex)
		pkex_t = hapd->dpp_pkex->t;
	else if (hapd->dpp_pkex_bi)
		pkex_t = hapd->dpp_pkex_bi->pkex_t;
	else
		pkex_t = 0;
	if (pkex_t >= PKEX_COUNTER_T_LIMIT) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_PKEX_T_LIMIT "id=0");
		hostapd_dpp_pkex_remove(hapd, "*");
	}
}


struct wpabuf *
hostapd_dpp_gas_req_handler(struct hostapd_data *hapd, const u8 *sa,
			    const u8 *query, size_t query_len,
			    const u8 *data, size_t data_len)
{
	struct dpp_authentication *auth = hapd->dpp_auth;
	struct wpabuf *resp;

	wpa_printf(MSG_DEBUG, "DPP: GAS request from " MACSTR, MAC2STR(sa));
	if (!auth || !auth->auth_success ||
	    os_memcmp(sa, auth->peer_mac_addr, ETH_ALEN) != 0) {
#ifdef CONFIG_DPP2
		if (dpp_relay_rx_gas_req(hapd->iface->interfaces->dpp, sa, data,
				     data_len) == 0) {
			/* Response will be forwarded once received over TCP */
			return NULL;
		}
#endif /* CONFIG_DPP2 */
		wpa_printf(MSG_DEBUG, "DPP: No matching exchange in progress");
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG,
		    "DPP: Received Configuration Request (GAS Query Request)",
		    query, query_len);
	wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_REQ_RX "src=" MACSTR,
		MAC2STR(sa));
	resp = dpp_conf_req_rx(auth, query, query_len);
	if (!resp)
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_FAILED);
	return resp;
}


void hostapd_dpp_gas_status_handler(struct hostapd_data *hapd, int ok)
{
	struct dpp_authentication *auth = hapd->dpp_auth;

	if (!auth)
		return;

	wpa_printf(MSG_DEBUG, "DPP: Configuration exchange completed (ok=%d)",
		   ok);
	eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout, hapd, NULL);
	eloop_cancel_timeout(hostapd_dpp_auth_resp_retry_timeout, hapd, NULL);
#ifdef CONFIG_DPP2
	if (ok && auth->peer_version >= 2 &&
	    auth->conf_resp_status == DPP_STATUS_OK) {
		wpa_printf(MSG_DEBUG, "DPP: Wait for Configuration Result");
		auth->waiting_conf_result = 1;
		eloop_cancel_timeout(hostapd_dpp_config_result_wait_timeout,
				     hapd, NULL);
		eloop_register_timeout(2, 0,
				       hostapd_dpp_config_result_wait_timeout,
				       hapd, NULL);
		return;
	}
#endif /* CONFIG_DPP2 */
	hostapd_drv_send_action_cancel_wait(hapd);

	if (ok)
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_SENT);
	else
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_CONF_FAILED);
	dpp_auth_deinit(hapd->dpp_auth);
	hapd->dpp_auth = NULL;
}


int hostapd_dpp_configurator_sign(struct hostapd_data *hapd, const char *cmd)
{
	struct dpp_authentication *auth;
	int ret = -1;
	char *curve = NULL;

	auth = os_zalloc(sizeof(*auth));
	if (!auth)
		return -1;

	curve = get_param(cmd, " curve=");
	hostapd_dpp_set_testing_options(hapd, auth);
	if (dpp_set_configurator(hapd->iface->interfaces->dpp, hapd->msg_ctx,
				 auth, cmd) == 0 &&
	    dpp_configurator_own_config(auth, curve, 1) == 0) {
		hostapd_dpp_handle_config_obj(hapd, auth);
		ret = 0;
	}

	dpp_auth_deinit(auth);
	os_free(curve);

	return ret;
}


int hostapd_dpp_pkex_add(struct hostapd_data *hapd, const char *cmd)
{
	struct dpp_bootstrap_info *own_bi;
	const char *pos, *end;

	pos = os_strstr(cmd, " own=");
	if (!pos)
		return -1;
	pos += 5;
	own_bi = dpp_bootstrap_get_id(hapd->iface->interfaces->dpp, atoi(pos));
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
	hapd->dpp_pkex_bi = own_bi;
	own_bi->pkex_t = 0; /* clear pending errors on new code */

	os_free(hapd->dpp_pkex_identifier);
	hapd->dpp_pkex_identifier = NULL;
	pos = os_strstr(cmd, " identifier=");
	if (pos) {
		pos += 12;
		end = os_strchr(pos, ' ');
		if (!end)
			return -1;
		hapd->dpp_pkex_identifier = os_malloc(end - pos + 1);
		if (!hapd->dpp_pkex_identifier)
			return -1;
		os_memcpy(hapd->dpp_pkex_identifier, pos, end - pos);
		hapd->dpp_pkex_identifier[end - pos] = '\0';
	}

	pos = os_strstr(cmd, " code=");
	if (!pos)
		return -1;
	os_free(hapd->dpp_pkex_code);
	hapd->dpp_pkex_code = os_strdup(pos + 6);
	if (!hapd->dpp_pkex_code)
		return -1;

	if (os_strstr(cmd, " init=1")) {
		struct wpabuf *msg;

		wpa_printf(MSG_DEBUG, "DPP: Initiating PKEX");
		dpp_pkex_free(hapd->dpp_pkex);
		hapd->dpp_pkex = dpp_pkex_init(hapd->msg_ctx, own_bi,
					       hapd->own_addr,
					       hapd->dpp_pkex_identifier,
					       hapd->dpp_pkex_code);
		if (!hapd->dpp_pkex)
			return -1;

		msg = hapd->dpp_pkex->exchange_req;
		/* TODO: Which channel to use? */
		wpa_msg(hapd->msg_ctx, MSG_INFO, DPP_EVENT_TX "dst=" MACSTR
			" freq=%u type=%d", MAC2STR(broadcast), 2437,
			DPP_PA_PKEX_EXCHANGE_REQ);
		hostapd_drv_send_action(hapd, 2437, 0, broadcast,
					wpabuf_head(msg), wpabuf_len(msg));
	}

	/* TODO: Support multiple PKEX info entries */

	os_free(hapd->dpp_pkex_auth_cmd);
	hapd->dpp_pkex_auth_cmd = os_strdup(cmd);

	return 1;
}


int hostapd_dpp_pkex_remove(struct hostapd_data *hapd, const char *id)
{
	unsigned int id_val;

	if (os_strcmp(id, "*") == 0) {
		id_val = 0;
	} else {
		id_val = atoi(id);
		if (id_val == 0)
			return -1;
	}

	if ((id_val != 0 && id_val != 1) || !hapd->dpp_pkex_code)
		return -1;

	/* TODO: Support multiple PKEX entries */
	os_free(hapd->dpp_pkex_code);
	hapd->dpp_pkex_code = NULL;
	os_free(hapd->dpp_pkex_identifier);
	hapd->dpp_pkex_identifier = NULL;
	os_free(hapd->dpp_pkex_auth_cmd);
	hapd->dpp_pkex_auth_cmd = NULL;
	hapd->dpp_pkex_bi = NULL;
	/* TODO: Remove dpp_pkex only if it is for the identified PKEX code */
	dpp_pkex_free(hapd->dpp_pkex);
	hapd->dpp_pkex = NULL;
	return 0;
}


void hostapd_dpp_stop(struct hostapd_data *hapd)
{
	dpp_auth_deinit(hapd->dpp_auth);
	hapd->dpp_auth = NULL;
	dpp_pkex_free(hapd->dpp_pkex);
	hapd->dpp_pkex = NULL;
}


#ifdef CONFIG_DPP2

static void hostapd_dpp_relay_tx(void *ctx, const u8 *addr, unsigned int freq,
				 const u8 *msg, size_t len)
{
	struct hostapd_data *hapd = ctx;
	u8 *buf;

	wpa_printf(MSG_DEBUG, "DPP: Send action frame dst=" MACSTR " freq=%u",
		   MAC2STR(addr), freq);
	buf = os_malloc(2 + len);
	if (!buf)
		return;
	buf[0] = WLAN_ACTION_PUBLIC;
	buf[1] = WLAN_PA_VENDOR_SPECIFIC;
	os_memcpy(buf + 2, msg, len);
	hostapd_drv_send_action(hapd, freq, 0, addr, buf, 2 + len);
	os_free(buf);
}


static void hostapd_dpp_relay_gas_resp_tx(void *ctx, const u8 *addr,
					  u8 dialog_token, int prot,
					  struct wpabuf *buf)
{
	struct hostapd_data *hapd = ctx;

	gas_serv_req_dpp_processing(hapd, addr, dialog_token, prot, buf);
}

#endif /* CONFIG_DPP2 */


static int hostapd_dpp_add_controllers(struct hostapd_data *hapd)
{
#ifdef CONFIG_DPP2
	struct dpp_controller_conf *ctrl;
	struct dpp_relay_config config;

	os_memset(&config, 0, sizeof(config));
	config.cb_ctx = hapd;
	config.tx = hostapd_dpp_relay_tx;
	config.gas_resp_tx = hostapd_dpp_relay_gas_resp_tx;
	for (ctrl = hapd->conf->dpp_controller; ctrl; ctrl = ctrl->next) {
		config.ipaddr = &ctrl->ipaddr;
		config.pkhash = ctrl->pkhash;
		if (dpp_relay_add_controller(hapd->iface->interfaces->dpp,
					     &config) < 0)
			return -1;
	}
#endif /* CONFIG_DPP2 */

	return 0;
}


int hostapd_dpp_init(struct hostapd_data *hapd)
{
	hapd->dpp_allowed_roles = DPP_CAPAB_CONFIGURATOR | DPP_CAPAB_ENROLLEE;
	hapd->dpp_init_done = 1;
	return hostapd_dpp_add_controllers(hapd);
}


void hostapd_dpp_deinit(struct hostapd_data *hapd)
{
#ifdef CONFIG_TESTING_OPTIONS
	os_free(hapd->dpp_config_obj_override);
	hapd->dpp_config_obj_override = NULL;
	os_free(hapd->dpp_discovery_override);
	hapd->dpp_discovery_override = NULL;
	os_free(hapd->dpp_groups_override);
	hapd->dpp_groups_override = NULL;
	hapd->dpp_ignore_netaccesskey_mismatch = 0;
#endif /* CONFIG_TESTING_OPTIONS */
	if (!hapd->dpp_init_done)
		return;
	eloop_cancel_timeout(hostapd_dpp_reply_wait_timeout, hapd, NULL);
	eloop_cancel_timeout(hostapd_dpp_init_timeout, hapd, NULL);
	eloop_cancel_timeout(hostapd_dpp_auth_resp_retry_timeout, hapd, NULL);
#ifdef CONFIG_DPP2
	eloop_cancel_timeout(hostapd_dpp_config_result_wait_timeout, hapd,
			     NULL);
#endif /* CONFIG_DPP2 */
	dpp_auth_deinit(hapd->dpp_auth);
	hapd->dpp_auth = NULL;
	hostapd_dpp_pkex_remove(hapd, "*");
	hapd->dpp_pkex = NULL;
	os_free(hapd->dpp_configurator_params);
	hapd->dpp_configurator_params = NULL;
}
