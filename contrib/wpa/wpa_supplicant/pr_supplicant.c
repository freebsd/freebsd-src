/*
 * Proxmity Ranging
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/proximity_ranging.h"
#include "p2p/p2p.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "notify.h"
#include "driver_i.h"
#include "pr_supplicant.h"

#ifdef CONFIG_PASN
static void wpas_pr_pasn_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpas_pr_pasn_roc_total_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpas_pr_pasn_auth_work_done(struct wpa_supplicant *wpa_s);
static void wpas_pr_pasn_auth_retry_timeout(void *eloop_ctx, void *timeout_ctx);

/* Total listen window (ms) for the PASN responder ROC */
#define PR_PASN_RESPONDER_ROC_DURATION 10000
/* Initiator PASN authentication timeout (s) */
#define PR_PASN_AUTH_TIMEOUT           10
/* Retry interval (ms) for unacked PASN Authentication frame 1 */
#define PR_PASN_AUTH1_RETRY_INTERVAL_MS 100
#endif /* CONFIG_PASN */


static u8 wpas_pr_best_edca_format_bw(u32 bw_bitmap, u32 preamble_bitmap)
{
	/* Prefer highest bandwidth first */
	if ((bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_160)) &&
	    (preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT)))
		return EDCA_FORMAT_AND_BW_VHT160_DUAL_LO;
	if ((bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80P80)) &&
	    (preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT)))
		return EDCA_FORMAT_AND_BW_VHT80P80;
	if ((bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80)) &&
	    (preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT)))
		return EDCA_FORMAT_AND_BW_VHT80;
	if ((bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_40)) &&
	    (preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT)))
		return EDCA_FORMAT_AND_BW_VHT40;
	if ((bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_40)) &&
	    (preamble_bitmap & BIT(WPA_PR_PREAMBLE_HT)))
		return EDCA_FORMAT_AND_BW_HT40;
	if ((bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_20)) &&
	    (preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT)))
		return EDCA_FORMAT_AND_BW_VHT20;
	return EDCA_FORMAT_AND_BW_INVALID;
}


static u8 wpas_pr_best_ntb_format_bw(u32 bw_bitmap, u32 preamble_bitmap)
{
	if (!(preamble_bitmap & BIT(WPA_PR_PREAMBLE_HE)))
		return NTB_FORMAT_AND_BW_INVALID;

	if (bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_160))
		return NTB_FORMAT_AND_BW_HE160_SINGLE_LO;
	if (bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80P80))
		return NTB_FORMAT_AND_BW_HE80P80;
	if (bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80))
		return NTB_FORMAT_AND_BW_HE80;
	if (bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_40))
		return NTB_FORMAT_AND_BW_HE40;
	if (bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_20))
		return NTB_FORMAT_AND_BW_HE20;
	return NTB_FORMAT_AND_BW_INVALID;
}


static bool
wpas_pr_edca_is_valid_op_class(u32 bw_bitmap, u32 preamble_bitmap,
			       const struct oper_class_map *op_class_map)
{
	if (!op_class_map)
		return false;

	switch (op_class_map->bw) {
	case BW20:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_20)) &&
			!!(preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT));
	case BW40PLUS:
	case BW40MINUS:
	case BW40:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_40)) &&
			!!(preamble_bitmap & (BIT(WPA_PR_PREAMBLE_VHT) |
					      BIT(WPA_PR_PREAMBLE_HT)));
	case BW80:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80)) &&
			!!(preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT));
	case BW80P80:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80P80)) &&
			!!(preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT));
	case BW160:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_160)) &&
			!!(preamble_bitmap & BIT(WPA_PR_PREAMBLE_VHT));
	default:
		return false;
	}
}


static bool
wpas_pr_ntb_is_valid_op_class(u32 bw_bitmap, u32 preamble_bitmap,
			      const struct oper_class_map *op_class_map)
{
	if (!op_class_map)
		return false;

	/* NTB ranging requires HE preamble */
	if (!(preamble_bitmap & BIT(WPA_PR_PREAMBLE_HE)))
		return false;

	switch (op_class_map->bw) {
	case BW20:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_20));
	case BW40PLUS:
	case BW40MINUS:
	case BW40:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_40));
	case BW80:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80));
	case BW80P80:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_80P80));
	case BW160:
		return !!(bw_bitmap & BIT(WPA_PR_CHAN_WIDTH_160));
	default:
		return false;
	}
}


/**
 * wpas_pr_op_class_to_chan_params - Derive center freq and width from op_class
 */
static int wpas_pr_op_class_to_chan_params(u8 op_class, u8 op_channel,
					   u32 *center_freq1,
					   u32 *center_freq2,
					   u16 *channel_width)
{
	int control_freq, bw, offset = 0;
	int half_bw, block_pos;

	if (!center_freq1 || !center_freq2 || !channel_width)
		return -1;

	*center_freq1 = 0;
	*center_freq2 = 0;
	*channel_width = 0;

	/* 80+80 MHz: center_freq2 is unknown from primary channel alone */
	if (op_class == 130 || op_class == 135) {
		wpa_printf(MSG_DEBUG,
			   "PR: op_class %u (80+80 MHz) not supported, center_freq2 cannot be determined",
			   op_class);
		return -1;
	}

	control_freq = ieee80211_chan_to_freq(NULL, op_class, op_channel);
	if (control_freq < 0) {
		wpa_printf(MSG_DEBUG, "PR: Invalid op_class=%u channel=%u",
			   op_class, op_channel);
		return -1;
	}

	bw = op_class_to_bandwidth(op_class);

	/*
	 * 2.4 GHz 40 MHz: channels are 5 MHz apart so the generic offset
	 * formula does not apply. Handle upper (op_class 83) and lower
	 * (op_class 84) secondary channel directions explicitly.
	 */
	if (op_class == 83) {
		*center_freq1 = control_freq + 10;
		*channel_width = 40;
		return 0;
	}
	if (op_class == 84) {
		*center_freq1 = control_freq - 10;
		*channel_width = 40;
		return 0;
	}

	if (bw == 20) {
		*center_freq1 = control_freq;
		*channel_width = 20;
		return 0;
	}

	/*
	 * For 5 GHz and 6 GHz wider bandwidths, compute the primary channel's
	 * position (in 20 MHz steps) within its band segment, then derive
	 * center_freq1 using the formula:
	 *   center_freq1 = control_freq + (bw/2 - 10) -
	 *			(offset & (bw/20 - 1)) * 20
	 */
	if (control_freq >= 5955)
		offset = (control_freq - 5955) / 20;
	else if (control_freq >= 5745)
		offset = (control_freq - 5745) / 20;
	else if (control_freq >= 5180)
		offset = (control_freq - 5180) / 20;

	/* Distance from the primary channel to the center of the BW block */
	half_bw = bw / 2 - 10;

	/* Position of the primary channel within its BW block (in 20 MHz steps)
	 */
	block_pos = offset & (bw / 20 - 1);

	/* Center = primary channel + half-BW offset - position within block */
	*center_freq1 = control_freq + half_bw - block_pos * 20;
	*channel_width = bw;

	wpa_printf(MSG_DEBUG, "PR: op_class=%u ch=%u -> freq=%d cf1=%u bw=%d",
		   op_class, op_channel, control_freq, *center_freq1, bw);

	return 0;
}


static void
wpas_pr_setup_edca_channels(struct wpa_supplicant *wpa_s,
			    struct pr_channels *chan,
			    u32 bw_bitmap, u32 preamble_bitmap,
			    bool allow_6ghz)
{
	struct hostapd_hw_modes *mode;
	int cla = 0, i;

	for (i = 0; global_op_class[i].op_class; i++) {
		unsigned int ch;
		struct pr_op_class *op = NULL;
		const struct oper_class_map *o = &global_op_class[i];

		mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, o->mode,
				is_6ghz_op_class(o->op_class));
		if (!mode || (!allow_6ghz && is_6ghz_op_class(o->op_class)) ||
		    !wpas_pr_edca_is_valid_op_class(bw_bitmap, preamble_bitmap,
						    o))
			continue;

		for (ch = o->min_chan; ch <= o->max_chan; ch += o->inc) {
			enum chan_allowed res;

			/* Check for non-continuous jump in channel index
			 * increment.
			 */
			if (o->op_class >= 128 && o->op_class <= 130 &&
			    ch < 149 && ch + o->inc > 149)
				ch = 149;

			res = verify_channel(mode, o->op_class, ch, o->bw);

			if (res == ALLOWED) {
				if (!op) {
					if (cla == PR_MAX_OP_CLASSES)
						continue;

					wpa_printf(MSG_DEBUG,
						   "PR: Add operating class: %u (EDCA)",
						   o->op_class);
					op = &chan->op_class[cla];
					cla++;
					op->op_class = o->op_class;
				}
				if (op->channels == PR_MAX_OP_CLASS_CHANNELS)
					continue;
				op->channel[op->channels] = ch;
				op->channels++;
			}
		}

		if (op)
			wpa_hexdump(MSG_DEBUG, "PR: Channels (EDCA)",
				    op->channel, op->channels);
	}

	chan->op_classes = cla;
}


static void
wpas_pr_setup_ntb_channels(struct wpa_supplicant *wpa_s,
			   struct pr_channels *chan,
			   u32 bw_bitmap, u32 preamble_bitmap,
			   bool allow_6ghz)
{
	int cla = 0, i;
	struct hostapd_hw_modes *mode;

	for (i = 0; global_op_class[i].op_class; i++) {
		unsigned int ch;
		struct pr_op_class *op = NULL;
		const struct oper_class_map *o = &global_op_class[i];

		mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, o->mode,
				is_6ghz_op_class(o->op_class));
		if (!mode || (!allow_6ghz && is_6ghz_op_class(o->op_class)) ||
		    !wpas_pr_ntb_is_valid_op_class(bw_bitmap, preamble_bitmap,
						   o))
			continue;

		for (ch = o->min_chan; ch <= o->max_chan; ch += o->inc) {
			enum chan_allowed res;

			/* Check for non-continuous jump in channel index
			 * increment.
			 */
			if (o->op_class >= 128 && o->op_class <= 130 &&
			    ch < 149 && ch + o->inc > 149)
				ch = 149;

			res = verify_channel(mode, o->op_class, ch, o->bw);

			if (res == ALLOWED) {
				if (!op) {
					if (cla == PR_MAX_OP_CLASSES)
						continue;
					wpa_printf(MSG_DEBUG,
						   "PR: Add operating class: %u (NTB)",
						   o->op_class);
					op = &chan->op_class[cla];
					cla++;
					op->op_class = o->op_class;
				}
				if (op->channels == PR_MAX_OP_CLASS_CHANNELS)
					continue;
				op->channel[op->channels] = ch;
				op->channels++;
			}
		}
		if (op) {
			wpa_hexdump(MSG_DEBUG, "PR: Channels (NTB)",
				    op->channel, op->channels);
		}
	}

	chan->op_classes = cla;
}


static int wpas_pr_pasn_send_mgmt(void *ctx, const u8 *data, size_t data_len,
				  int noack, unsigned int freq,
				  unsigned int wait)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_send_mlme(wpa_s, data, data_len, noack, freq, wait);
}


static void wpas_pr_device_found(void *ctx, const struct pr_device *dev)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_pr_device_found(wpa_s, dev);
}


static void wpas_pr_pasn_negotiation_started(void *ctx, const u8 *peer_addr,
					     u8 role, u8 protocol_type)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_pr_negotiation_started(wpa_s, peer_addr, role,
					   protocol_type);
}


static void wpas_pr_pasn_result(void *ctx, u8 role, u8 protocol_type,
				u8 op_class, u8 op_channel, const char *country)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_pr_pasn_result(wpa_s, role, protocol_type, op_class,
				   op_channel, country);
}


static void wpas_pr_clear_ranging_params(struct pr_data *pr)
{
	if (pr) {
		os_free(pr->pr_pasn_params);
		pr->pr_pasn_params = NULL;
		pr->ranging_final_received = false;
	}
}


static void wpas_pr_ranging_session_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct pr_data *pr = wpa_s->global->pr;

	wpa_printf(MSG_DEBUG, "PR: Ranging session timeout - cleaning up");

	/* Stop peer measurement and cleanup ranging socket */
	wpa_drv_stop_peer_measurement(wpa_s);

	/* Free ranging params */
	wpas_pr_clear_ranging_params(pr);

	wpas_pr_pd_stop(wpa_s);
	wpas_notify_pr_ranging_terminated(wpa_s, PR_SESSION_END_TIMEOUT);
}


/**
 * wpas_pr_trigger_ranging - Trigger FTM ranging after successful PASN auth
 */
static int wpas_pr_trigger_ranging(struct wpa_supplicant *wpa_s,
				   const u8 *peer_addr, int freq, u8 op_class,
				   u8 op_channel, u8 format_bw,
				   u8 protocol_type)
{
	struct pr_data *pr = wpa_s->global->pr;
	struct pr_pasn_ranging_params *params;
	u16 channel_width = 0;
	u32 center_freq1 = 0, center_freq2 = 0;
	int ret;

	if (!pr || !pr->pr_pasn_params) {
		wpa_printf(MSG_DEBUG,
			   "PR: No ranging params available for " MACSTR,
			   MAC2STR(peer_addr));
		return -1;
	}

	params = pr->pr_pasn_params;

	wpa_printf(MSG_DEBUG,
		   "PR: Triggering ranging for " MACSTR " freq=%d ch=%u",
		   MAC2STR(peer_addr), freq, op_channel);

	/* Derive center frequencies and channel width (MHz) from op_class */
	ret = wpas_pr_op_class_to_chan_params(op_class, op_channel,
					      &center_freq1, &center_freq2,
					      &channel_width);
	if (ret < 0) {
		wpa_printf(MSG_INFO,
			   "PR: Failed to derive channel params for op_class=%u ch=%u",
			   op_class, op_channel);
		goto fail;
	}

	/* Populate ranging params */
	params->ranging_op_class = op_class;
	params->channel_width = channel_width;
	params->format_bw = format_bw;
	params->center_freq1 = center_freq1;
	params->center_freq2 = center_freq2;

	/* Validate format_bw is within enum limits before setting preamble */
	if (protocol_type & PR_EDCA_BASED_RANGING) {
		if (format_bw < EDCA_FORMAT_AND_BW_VHT20 ||
		    format_bw > EDCA_FORMAT_AND_BW_VHT160_SINGLE_LO) {
			wpa_printf(MSG_INFO,
				   "PR: Invalid EDCA format_bw %u (valid range: %u-%u)",
				   format_bw, EDCA_FORMAT_AND_BW_VHT20,
				   EDCA_FORMAT_AND_BW_VHT160_SINGLE_LO);
			goto fail;
		}
	} else if (protocol_type & (PR_NTB_SECURE_LTF_BASED_RANGING |
				    PR_NTB_OPEN_BASED_RANGING)) {
		if (format_bw > NTB_FORMAT_AND_BW_HE160_SINGLE_LO) {
			wpa_printf(MSG_INFO,
				   "PR: Invalid NTB format_bw %u (valid range: 0-%u)",
				   format_bw,
				   NTB_FORMAT_AND_BW_HE160_SINGLE_LO);
			goto fail;
		}
	} else {
		wpa_printf(MSG_INFO, "PR: Unknown protocol_type %u",
			   protocol_type);
		goto fail;
	}

	wpa_printf(MSG_DEBUG,
		   "PR: Ranging params - op_class=%u, ch_width=%u, format_bw=%u, cf1=%u, cf2=%u",
		   params->ranging_op_class, params->channel_width,
		   params->format_bw, params->center_freq1,
		   params->center_freq2);

	/* Call driver operation to start peer measurement */
	if (wpa_drv_start_peer_measurement(wpa_s, peer_addr, freq, op_channel,
					   channel_width, params) < 0) {
		wpa_printf(MSG_INFO, "PR: Failed to start peer measurement");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "PR: Successfully triggered ranging measurement");

	/* Start session timeout timer if continuous ranging session time is set
	 */
	if (params->continuous_ranging_session_time > 0) {
		unsigned int timeout_sec, timeout_usec;

		wpa_printf(MSG_DEBUG,
			   "PR: Starting ranging session timeout timer for %u ms",
			   params->continuous_ranging_session_time);

		timeout_sec = params->continuous_ranging_session_time / 1000;
		timeout_usec = (params->continuous_ranging_session_time %
				1000) * 1000;
		eloop_cancel_timeout(wpas_pr_ranging_session_timeout,
				     wpa_s, NULL);
		eloop_register_timeout(timeout_sec, timeout_usec,
				       wpas_pr_ranging_session_timeout,
				       wpa_s, NULL);
	}

	return 0;

fail:
	wpas_pr_clear_ranging_params(pr);
	return -1;
}


static void wpas_pr_ranging_params(void *ctx, const u8 *dev_addr,
				   const u8 *peer_addr, u8 ranging_role,
				   u8 protocol_type, u8 op_class, u8 op_channel,
				   u8 self_format_bw, u8 peer_format_bw)
{
	struct wpa_supplicant *wpa_s = ctx;
	int bw, format_bw, freq;

	bw = op_class_to_bandwidth(op_class);
	format_bw = self_format_bw < peer_format_bw ?
		self_format_bw : peer_format_bw;
	freq = ieee80211_chan_to_freq(NULL, op_class, op_channel);

	wpas_notify_pr_ranging_params(wpa_s, dev_addr, peer_addr, ranging_role,
				      protocol_type, freq, op_channel, bw,
				      format_bw);

	/*
	 * PASN succeeded - cancel the PASN timeout so it does not fire and
	 * prematurely stop the PD wdev while ranging is in progress.
	 * Cleanup happens on COMPLETE event or session timeout.
	 */
	eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);
	wpas_pr_pasn_auth_work_done(wpa_s);

	/* Trigger ranging measurement after successful PASN authentication */
	if (wpas_pr_trigger_ranging(wpa_s, peer_addr, freq, op_class,
				    op_channel, format_bw, protocol_type) < 0) {
		wpa_printf(MSG_INFO,
			   "PR: Failed to trigger ranging, stopping PD wdev");
		wpas_pr_pd_stop(wpa_s);
	}
}


static int wpas_pr_pasn_set_keys(void *ctx, const u8 *own_addr,
				 const u8 *peer_addr, int cipher, int akmp,
				 struct wpa_ptk *ptk)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_driver_set_key_params params;

	wpa_printf(MSG_DEBUG, "PR PASN: Set secure ranging context for " MACSTR,
		   MAC2STR(peer_addr));

	if (!wpa_s->driver->set_key)
		return -1;

	os_memset(&params, 0, sizeof(params));
	params.ifname = wpa_s->ifname;
	params.own_addr = own_addr;
	params.alg = wpa_cipher_to_alg(cipher);
	params.addr = peer_addr;
	params.key_idx = 0;
	params.set_tx = 1;
	params.key = ptk->tk;
	params.key_len = ptk->tk_len;
	params.key_flag = KEY_FLAG_PAIRWISE_RX_TX;
	params.link_id = -1;
	params.ltf_keyseed = ptk->ltf_keyseed;
	params.ltf_keyseed_len = ptk->ltf_keyseed_len;

	if (wpa_s->driver->set_key(wpa_s->drv_priv, &params) < 0) {
		wpa_printf(MSG_INFO, "PR PASN: Failed to set TK");
		return -1;
	}

	return 0;
}


static void wpas_pr_pasn_clear_keys(void *ctx, const u8 *own_addr,
				    const u8 *peer_addr)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_driver_set_key_params params;

	wpa_printf(MSG_DEBUG, "PR PASN: Clear secure ranging context for "
		   MACSTR, MAC2STR(peer_addr));

	if (!wpa_s->driver->set_key)
		return;

	os_memset(&params, 0, sizeof(params));
	params.ifname = wpa_s->ifname;
	params.own_addr = own_addr;
	params.alg = WPA_ALG_NONE;
	params.addr = peer_addr;
	params.key_idx = 0;
	params.link_id = -1;

	if (wpa_s->driver->set_key(wpa_s->drv_priv, &params) < 0)
		wpa_printf(MSG_INFO, "PR PASN: Failed to clear TK");
}


struct wpabuf * wpas_pr_usd_elems(struct wpa_supplicant *wpa_s,
				  const u8 *src_addr)
{
	if (!wpa_s->global->pr)
		return NULL;

	return pr_prepare_usd_elems(wpa_s->global->pr, src_addr);
}


void wpas_pr_process_usd_elems(struct wpa_supplicant *wpa_s, const u8 *buf,
			       u16 buf_len, const u8 *peer_addr,
			       unsigned int freq)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr)
		return;
	pr_process_usd_elems(pr, buf, buf_len, peer_addr, freq);
}


int wpas_pr_init(struct wpa_global *global, struct wpa_supplicant *wpa_s,
		 const struct wpa_driver_capa *capa)
{
	struct pr_config pr;

	if (global->pr)
		return 0;

	os_memset(&pr, 0, sizeof(pr));

	os_memcpy(pr.dev_addr, wpa_s->own_addr, ETH_ALEN);
	pr.cb_ctx = wpa_s;
	pr.dev_name = wpa_s->conf->device_name;
	pr.pasn_type = wpa_s->conf->pr_pasn_type ?
		wpa_s->conf->pr_pasn_type :
		(int) (PR_PASN_DH19_UNAUTH | PR_PASN_DH19_AUTH);
	pr.preferred_ranging_role = wpa_s->conf->pr_preferred_role;

	pr.edca_format_and_bw =
		wpas_pr_best_edca_format_bw(capa->pd_bandwidths,
					    capa->pd_preambles);
	pr.edca_ista_support = capa->ista.support_edca &&
		capa->asap_support &&
		pr.edca_format_and_bw != EDCA_FORMAT_AND_BW_INVALID;
	pr.edca_rsta_support = capa->rsta.support_edca &&
		capa->asap_support &&
		pr.edca_format_and_bw != EDCA_FORMAT_AND_BW_INVALID;
	pr.pd_format_bw_bitmap = capa->pd_bandwidths;
	pr.pd_preamble_bitmap = capa->pd_preambles;
	pr.max_rx_antenna = capa->max_rx_antenna;
	pr.max_tx_antenna = capa->max_tx_antenna;

	wpas_pr_setup_edca_channels(wpa_s, &pr.edca_channels,
				    capa->pd_bandwidths,
				    capa->pd_preambles,
				    pr.support_6ghz);
	pr.ntb_format_and_bw =
		wpas_pr_best_ntb_format_bw(capa->pd_bandwidths,
					   capa->pd_preambles);
	pr.ntb_ista_support = capa->ista.support_ntb &&
		pr.ntb_format_and_bw != NTB_FORMAT_AND_BW_INVALID;
	pr.ntb_rsta_support = capa->rsta.support_ntb &&
		pr.ntb_format_and_bw != NTB_FORMAT_AND_BW_INVALID;
	pr.max_tx_ltf_repetations = capa->max_tx_ltf_repetations;
	pr.max_rx_ltf_repetations = capa->max_rx_ltf_repetations;
	pr.max_tx_ltf_total = capa->max_tx_ltf_total;
	pr.max_rx_ltf_total = capa->max_rx_ltf_total;
	pr.max_rx_sts_le_80 = capa->max_rx_sts_le_80;
	pr.max_rx_sts_gt_80 = capa->max_rx_sts_gt_80;
	pr.max_tx_sts_le_80 = capa->max_tx_sts_le_80;
	pr.max_tx_sts_gt_80 = capa->max_tx_sts_gt_80;

	pr.edca_min_ranging_interval = capa->edca_min_ranging_interval;
	pr.ntb_min_ranging_interval = capa->ntb_min_ranging_interval;
	pr.concurrent_ista_rsta = capa->concurrent_ista_rsta;
	pr.pmsr_max_peers = capa->pmsr_max_peers;
	pr.pr_max_peer_ista_role = capa->ista.max_peers;
	pr.pr_max_peer_rsta_role = capa->rsta.max_peers;
	pr.max_ftms_per_burst = capa->max_ftms_per_burst;

	pr.support_6ghz = capa->support_6ghz;

	pr.pasn_send_mgmt = wpas_pr_pasn_send_mgmt;
	pr.negotiation_started = wpas_pr_pasn_negotiation_started;
	pr.pasn_result = wpas_pr_pasn_result;
	pr.get_ranging_params = wpas_pr_ranging_params;
	pr.set_keys = wpas_pr_pasn_set_keys;
	pr.device_found = wpas_pr_device_found;
	pr.clear_keys = wpas_pr_pasn_clear_keys;

	pr.secure_he_ltf = wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA;

	wpas_pr_setup_ntb_channels(wpa_s, &pr.ntb_channels,
				   capa->pd_bandwidths, capa->pd_preambles,
				   pr.support_6ghz);

	if (wpa_s->conf->country[0] && wpa_s->conf->country[1]) {
		os_memcpy(pr.country, wpa_s->conf->country, 2);
		pr.country[2] = 0x04;
	} else {
		os_memcpy(pr.country, "XX\x04", 3);
	}

	if (wpa_s->conf->dik &&
	    wpabuf_len(wpa_s->conf->dik) <= DEVICE_IDENTITY_KEY_LEN) {
		pr.dik_cipher = wpa_s->conf->dik_cipher;
		pr.dik_len = wpabuf_len(wpa_s->conf->dik);
		os_memcpy(pr.dik_data, wpabuf_head(wpa_s->conf->dik),
			  pr.dik_len);
		pr.expiration = 24; /* hours */
	} else {
		pr.dik_cipher = DIRA_CIPHER_VERSION_128;
		pr.dik_len = DEVICE_IDENTITY_KEY_LEN;
		pr.expiration = 24; /* hours */
		if (os_get_random(pr.dik_data, pr.dik_len) < 0)
			return -1;

		wpa_s->conf->dik =
			wpabuf_alloc_copy(pr.dik_data, pr.dik_len);
		if (!wpa_s->conf->dik)
			return -1;

		wpa_s->conf->dik_cipher = pr.dik_cipher;

		wpa_printf(MSG_DEBUG, "PR: PR init new DIRA set");

		if (wpa_s->conf->update_config &&
		    wpa_config_write(wpa_s->confname, wpa_s->conf))
			wpa_printf(MSG_DEBUG,
				   "PR: Failed to update configuration");
	}

	global->pr = pr_init(&pr);
	if (!global->pr) {
		wpa_printf(MSG_DEBUG, "PR: Failed to init PR");
		return -1;
	}
	global->pr_init_wpa_s = wpa_s;

	return 0;
}


void wpas_pr_flush(struct wpa_supplicant *wpa_s)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (pr)
		pr_flush(pr);
}

void wpas_pr_deinit(struct wpa_supplicant *wpa_s)
{
	if (wpa_s == wpa_s->global->pr_init_wpa_s) {
		pr_deinit(wpa_s->global->pr);
		wpa_s->global->pr = NULL;
		wpa_s->global->pr_init_wpa_s = NULL;
	}

#ifdef CONFIG_PASN
	eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_pr_pasn_roc_total_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_pr_pasn_auth_retry_timeout, wpa_s, NULL);
#endif /* CONFIG_PASN */
	eloop_cancel_timeout(wpas_pr_ranging_session_timeout, wpa_s, NULL);
}


void wpas_pr_pd_stop(struct wpa_supplicant *wpa_s)
{
	struct pr_data *pr = wpa_s->global->pr;

	/* Cancel ranging session timeout and stop peer measurement */
	eloop_cancel_timeout(wpas_pr_ranging_session_timeout, wpa_s, NULL);
	wpa_drv_stop_peer_measurement(wpa_s);

	if (is_zero_ether_addr(wpa_s->pd_addr)) {
		wpa_printf(MSG_DEBUG, "PR: pd_stop: no active PD wdev");
		return;
	}

	wpa_printf(MSG_DEBUG, "PR: Stopping PD wdev addr=" MACSTR,
		   MAC2STR(wpa_s->pd_addr));

	wpa_drv_pd_stop(wpa_s);
	os_memset(wpa_s->pd_addr, 0, ETH_ALEN);

	/* Restore dev_addr to station MAC now that PD wdev is gone */
	if (pr)
		pr_set_dev_addr(pr, wpa_s->own_addr);

	wpa_printf(MSG_DEBUG, "PR: PD wdev stopped, dev_addr restored to "
		   MACSTR, MAC2STR(wpa_s->own_addr));
}


void wpas_pr_update_dev_addr(struct wpa_supplicant *wpa_s)
{
	pr_set_dev_addr(wpa_s->global->pr, wpa_s->own_addr);
}


void wpas_pr_clear_dev_iks(struct wpa_supplicant *wpa_s)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr)
		return;

	pr_clear_dev_iks(pr);
}


void wpas_pr_set_dev_ik(struct wpa_supplicant *wpa_s, const u8 *dik,
			const char *password, const u8 *pmk, size_t pmk_len,
			bool own)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr || !dik)
		return;

	pr_add_dev_ik(pr, dik, password, pmk, pmk_len, own);
}


void wpas_pr_measurement_complete(struct wpa_supplicant *wpa_s,
				  struct peer_measurement_complete *complete)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!complete) {
		wpa_printf(MSG_INFO,
			   "PR: Invalid measurement complete event");
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "PR: Peer measurement complete cookie=%llu",
		   (unsigned long long) complete->cookie);

	/* Validate cookie if we have a pending ranging request */
	if (pr && pr->pr_pasn_params && complete->cookie != 0 &&
	    pr->pr_pasn_params->cookie != complete->cookie) {
		wpa_printf(MSG_INFO,
			   "PR: Complete cookie mismatch - expected %llu, got %llu. Ignoring.",
			   (unsigned long long) pr->pr_pasn_params->cookie,
			   (unsigned long long) complete->cookie);
		return;
	}

	wpas_notify_pr_ranging_complete(wpa_s, complete->cookie);
	wpas_pr_clear_ranging_params(pr);
	wpas_pr_pd_stop(wpa_s);
	wpas_notify_pr_ranging_terminated(wpa_s,
					  PR_SESSION_END_PEER_COMPLETE);
}


void wpas_pr_measurement_result(struct wpa_supplicant *wpa_s,
				struct peer_measurement_result *result)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!result) {
		wpa_printf(MSG_INFO, "PR: Invalid measurement result");
		return;
	}

	/* Drop results after final has been received */
	if (pr && pr->ranging_final_received) {
		wpa_printf(MSG_DEBUG,
			   "PR: Ignoring result after final for " MACSTR,
			   MAC2STR(result->addr));
		return;
	}

	/* Validate cookie if we have a pending ranging request */
	if (pr && pr->pr_pasn_params && result->cookie != 0 &&
	    pr->pr_pasn_params->cookie != result->cookie) {
		wpa_printf(MSG_INFO,
			   "PR: Cookie mismatch - expected %llu, got %llu. Ignoring result.",
			   (unsigned long long) pr->pr_pasn_params->cookie,
			   (unsigned long long) result->cookie);
		return;
	}

	/* Forward result to upper layer - includes failures and final */
	if (result->ftm.has_data || result->ftm.fail)
		wpas_notify_pr_measurement_result(wpa_s, result);

	/* After final result, mark session done - no more results accepted */
	if (result->final) {
		wpa_printf(MSG_DEBUG,
			   "PR: Final result received for " MACSTR
			   " - no further results will be processed",
			   MAC2STR(result->addr));
		if (pr)
			pr->ranging_final_received = true;
	}
}


#ifdef CONFIG_PASN

static int wpas_pr_start_pd(struct wpa_supplicant *wpa_s, const u8 *src_addr)
{
	u8 pd_addr[ETH_ALEN];

	if (!src_addr || is_zero_ether_addr(src_addr)) {
		wpa_printf(MSG_INFO, "PR: Invalid MAC address for PD wdev");
		return -1;
	}

	if (!is_zero_ether_addr(wpa_s->pd_addr)) {
		wpa_printf(MSG_INFO, "PR: PD wdev already active addr=" MACSTR,
			   MAC2STR(wpa_s->pd_addr));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PR: Creating PD wdev with MAC address " MACSTR,
		   MAC2STR(src_addr));

	os_memset(pd_addr, 0, ETH_ALEN);
	if (wpa_drv_pd_start(wpa_s, src_addr, pd_addr) < 0) {
		wpa_printf(MSG_ERROR, "PR: Failed to create PD wdev");
		return -1;
	}

	os_memcpy(wpa_s->pd_addr, pd_addr, ETH_ALEN);
	pr_set_dev_addr(wpa_s->global->pr, pd_addr);

	wpa_printf(MSG_DEBUG, "PR: PD wdev created addr=" MACSTR,
		   MAC2STR(pd_addr));
	return 0;
}


struct wpa_pr_pasn_auth_work {
	u8 peer_addr[ETH_ALEN];
	u8 auth_mode;
	int freq;
	enum pr_pasn_role role;
	u8 ranging_role;
	u8 ranging_type;
	u8 *ssid;
	size_t ssid_len;
	u8 bssid[ETH_ALEN];
	int forced_pr_freq;
};


struct wpa_pr_pasn_roc_work {
	unsigned int freq;
	u8 src_addr[ETH_ALEN];
};


static void wpas_pr_pasn_free_auth_work(struct wpa_pr_pasn_auth_work *awork)
{
	if (!awork)
		return;
	os_free(awork->ssid);
	os_free(awork);
}


static void wpas_pr_pasn_cancel_auth_work(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "PR PASN: Cancel pr-pasn-start-auth work");

	/* Remove pending/started work */
	radio_remove_works(wpa_s, "pr-pasn-start-auth", 0);
}


/**
 * wpas_pr_pasn_auth_work_done - Release PASN auth radio work
 */
static void wpas_pr_pasn_auth_work_done(struct wpa_supplicant *wpa_s)
{
	struct wpa_pr_pasn_auth_work *awork;

	if (!wpa_s->pr_pasn_auth_work)
		return;

	awork = wpa_s->pr_pasn_auth_work->ctx;
	wpas_pr_pasn_free_auth_work(awork);
	wpa_s->pr_pasn_auth_work->ctx = NULL;
	radio_work_done(wpa_s->pr_pasn_auth_work);
	wpa_s->pr_pasn_auth_work = NULL;
	eloop_cancel_timeout(wpas_pr_pasn_auth_retry_timeout, wpa_s, NULL);
}


/**
 * wpas_pr_pasn_roc_work_done - Idempotent helper to complete ROC radio work
 */
static void wpas_pr_pasn_roc_work_done(struct wpa_supplicant *wpa_s)
{
	struct wpa_pr_pasn_roc_work *rwork;

	if (!wpa_s->pr_roc_work)
		return;

	rwork = wpa_s->pr_roc_work->ctx;
	os_free(rwork);
	wpa_s->pr_roc_work->ctx = NULL;
	radio_work_done(wpa_s->pr_roc_work);
	wpa_s->pr_roc_work = NULL;
}


/**
 * wpas_pr_cancel_roc - Stop all responder ROC activity, both active and queued
 *
 * Cancels any in-progress driver ROC, releases the active radio work item,
 * and removes any pending pr-pasn-roc work items that have not yet started.
 * Safe to call when no ROC is active; all sub-operations are idempotent.
 * Must NOT be called from within a pr-pasn-roc radio work callback (deinit
 * path) as that would cause re-entrant radio_remove_works() ->
 * radio_work_free().
 */
static void wpas_pr_cancel_roc(struct wpa_supplicant *wpa_s)
{
	wpa_drv_cancel_remain_on_channel(wpa_s);
	wpa_s->off_channel_freq = 0;
	wpa_s->roc_waiting_drv_freq = 0;
	wpas_pr_pasn_roc_work_done(wpa_s);
	radio_remove_works(wpa_s, "pr-pasn-roc", 0);
}


/**
 * wpas_pr_pasn_abort_responder - Cancel the responder PASN session and clean
 * up all associated state. Safe to call from any responder error path; calling
 * eloop_cancel_timeout() on an already-expired timer is a no-op.
 */
static void wpas_pr_pasn_abort_responder(struct wpa_supplicant *wpa_s)
{
	eloop_cancel_timeout(wpas_pr_pasn_roc_total_timeout, wpa_s, NULL);
	wpa_s->pr_responder_mode = false;
	os_memset(wpa_s->pr_responder_src_addr, 0, ETH_ALEN);
	wpas_pr_clear_ranging_params(wpa_s->global->pr);
}


/**
 * wpas_pr_pasn_roc_total_timeout - Total ROC budget expiry; stop responder
 */
static void wpas_pr_pasn_roc_total_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_printf(MSG_DEBUG,
		   "PR PASN: Total ROC budget expired, stopping responder listen");

	wpas_pr_cancel_roc(wpa_s);
	wpas_pr_pasn_abort_responder(wpa_s);
	wpas_notify_pr_ranging_terminated(wpa_s, PR_SESSION_END_NEG_FAILED);
}


/**
 * wpas_pr_pasn_roc_start_cb - Radio work callback to start the responder ROC
 */
static void wpas_pr_pasn_roc_start_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_pr_pasn_roc_work *rwork = work->ctx;
	unsigned int chunk_ms;

	if (deinit) {
		if (work->started) {
			/*
			 * ROC was already started but the work is being
			 * cancelled (e.g., interface removal). Cancel the
			 * driver ROC and clear the channel state.
			 */
			wpa_s->pr_roc_work = NULL;
			wpa_drv_cancel_remain_on_channel(wpa_s);
			wpa_s->off_channel_freq = 0;
			wpa_s->roc_waiting_drv_freq = 0;
		}
		/*
		 * Clear responder state and cancel the total-budget timer
		 * regardless of whether the work was started or not - the
		 * ROC will never fire now.
		 */
		wpas_pr_pasn_abort_responder(wpa_s);
		os_free(rwork);
		work->ctx = NULL;
		return;
	}

	wpa_s->pr_roc_work = work;

	/* Use max_remain_on_chan as per-chunk duration, matching DPP/P2P */
	chunk_ms = wpa_s->max_remain_on_chan;

	wpa_printf(MSG_DEBUG,
		   "PR PASN: Starting ROC chunk at freq %u MHz duration %u ms%s",
		   rwork->freq, chunk_ms,
		   is_zero_ether_addr(rwork->src_addr) ? "" :
		   " with MAC filter");

	if (wpa_drv_remain_on_channel(wpa_s, rwork->freq, chunk_ms,
				      is_zero_ether_addr(rwork->src_addr) ?
				      NULL : rwork->src_addr) < 0) {
		wpa_printf(MSG_ERROR,
			   "PR PASN: Failed to start ROC for responder");
		wpas_pr_pasn_roc_work_done(wpa_s);
		wpas_pr_pasn_abort_responder(wpa_s);
		wpas_notify_pr_ranging_terminated(wpa_s,
						  PR_SESSION_END_NEG_FAILED);
		return;
	}

	wpa_s->off_channel_freq = 0;
	wpa_s->roc_waiting_drv_freq = rwork->freq;
}


/**
 * wpas_pr_schedule_responder_roc - Queue next ROC chunk for the responder
 */
static void wpas_pr_schedule_responder_roc(struct wpa_supplicant *wpa_s,
					   unsigned int freq)
{
	struct wpa_pr_pasn_roc_work *rwork;

	rwork = os_zalloc(sizeof(*rwork));
	if (!rwork) {
		wpa_printf(MSG_INFO, "PR PASN: OOM restarting ROC");
		goto fail;
	}
	rwork->freq = freq;

	if (wpa_s->pr_responder_mode &&
	    !is_zero_ether_addr(wpa_s->pr_responder_src_addr))
		os_memcpy(rwork->src_addr, wpa_s->pr_responder_src_addr,
			  ETH_ALEN);

	if (!radio_add_work(wpa_s, freq, "pr-pasn-roc", 0,
			    wpas_pr_pasn_roc_start_cb, rwork)) {
		wpa_printf(MSG_INFO, "PR PASN: Failed to reschedule ROC");
		os_free(rwork);
		goto fail;
	}
	return;

fail:
	wpas_pr_pasn_abort_responder(wpa_s);
}


/**
 * wpas_pr_cancel_remain_on_channel_cb - ROC cancel/expiry callback for PR
 */
void wpas_pr_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					 unsigned int freq)
{
	wpa_printf(MSG_DEBUG, "PR PASN: Remain on channel cancel for %u MHz",
		   freq);

	if (!wpa_s->pr_roc_work)
		return;

	wpas_pr_pasn_roc_work_done(wpa_s);

	if (wpa_s->pr_responder_mode) {
		/* Total-budget timer still live — restart another chunk */
		wpa_printf(MSG_DEBUG,
			   "PR PASN: ROC chunk expired, restarting for next chunk");
		wpas_pr_schedule_responder_roc(wpa_s, freq);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "PR PASN: ROC total timeout reached, responder done");
}


static void wpas_pr_pasn_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	eloop_cancel_timeout(wpas_pr_pasn_auth_retry_timeout, wpa_s, NULL);

	if (wpa_s->pr_pasn_auth_work) {
		wpas_pr_pasn_cancel_auth_work(wpa_s);
		wpa_s->pr_pasn_auth_work = NULL;
	}

	/*
	 * Stop the PD wdev only after radio_work_done() has fully returned.
	 * Calling wpas_pr_pd_stop() from inside the radio-work deinit callback
	 * would trigger a re-entrant radio_remove_works() -> radio_work_free()
	 * on the same work item, causing a use-after-free / SIGSEGV.
	 */
	wpas_pr_pd_stop(wpa_s);

	wpas_pr_clear_ranging_params(wpa_s->global->pr);

	wpa_printf(MSG_DEBUG, "PR: PASN timed out");
	wpas_notify_pr_ranging_terminated(wpa_s, PR_SESSION_END_NEG_FAILED);
}


static void wpas_pr_pasn_auth_start_cb(struct wpa_radio_work *work, int deinit)
{
	int ret;
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_pr_pasn_auth_work *awork = work->ctx;
	struct pr_data *pr = wpa_s->global->pr;
	const u8 *peer_addr = NULL;

	if (deinit) {
		if (!work->started)
			eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);

		wpas_pr_pasn_free_auth_work(awork);
		work->ctx = NULL;
		return;
	}

	if (!is_zero_ether_addr(awork->peer_addr))
		peer_addr = awork->peer_addr;

	ret = pr_initiate_pasn_auth(pr, peer_addr, awork->freq,
				    awork->auth_mode, awork->ranging_role,
				    awork->ranging_type, awork->forced_pr_freq);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Failed to start PASN authentication");
		goto fail;
	}

	eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);
	eloop_register_timeout(PR_PASN_AUTH_TIMEOUT, 0, wpas_pr_pasn_timeout,
			       wpa_s, NULL);
	wpa_s->pr_pasn_auth_work = work;
	return;

fail:
	wpas_pr_pasn_free_auth_work(awork);
	work->ctx = NULL;
	radio_work_done(work);
	/* Stop PD wdev after radio_work_done() to avoid use-after-free */
	wpas_pr_pd_stop(wpa_s);
	wpas_pr_clear_ranging_params(wpa_s->global->pr);
}


int wpas_pr_initiate_pasn_auth(struct wpa_supplicant *wpa_s,
			       const u8 *peer_addr, int freq, u8 auth_mode,
			       u8 ranging_role, u8 ranging_type,
			       int forced_pr_freq, const u8 *src_addr,
			       enum pr_pasn_role pasn_role)
{
	struct wpa_pr_pasn_auth_work *awork;
	struct pr_data *pr = wpa_s->global->pr;

	/* Add OOB peer if not already in the discovery list */
	if (pr && pr_ensure_oob_peer(pr, peer_addr, freq) < 0)
		return -1;

	if (pasn_role == PR_ROLE_PASN_RESPONDER) {
		struct wpa_pr_pasn_roc_work *rwork;
		unsigned int roc_time_ms = PR_PASN_RESPONDER_ROC_DURATION;
		bool has_src_addr = src_addr && !is_zero_ether_addr(src_addr);

		wpa_printf(MSG_DEBUG,
			   "PR PASN: Scheduling ROC at freq %d for responder role%s",
			   freq, has_src_addr ? " with custom MAC" : "");

		rwork = os_zalloc(sizeof(*rwork));
		if (!rwork)
			return -1;

		rwork->freq = freq;
		if (has_src_addr)
			os_memcpy(rwork->src_addr, src_addr, ETH_ALEN);
		/* else rwork->src_addr stays all-zeros (no MAC filter on ROC)
		 */

		/*
		 * Store state so wpas_pr_pasn_auth_rx() can create the PD
		 * interface when M1 arrives. When no custom MAC address is
		 * given the PD wdev is skipped and the existing interface is
		 * used.
		 */
		wpa_s->pr_responder_mode = true;
		if (has_src_addr)
			os_memcpy(wpa_s->pr_responder_src_addr, src_addr,
				  ETH_ALEN);
		/* else pr_responder_src_addr stays all-zeros */

		if (!radio_add_work(wpa_s, freq, "pr-pasn-roc", 0,
				    wpas_pr_pasn_roc_start_cb, rwork)) {
			wpa_printf(MSG_INFO,
				   "PR PASN: Failed to schedule ROC for responder");
			os_free(rwork);
			wpa_s->pr_responder_mode = false;
			os_memset(wpa_s->pr_responder_src_addr, 0, ETH_ALEN);
			return -1;
		}

		/*
		 * Register the total-budget timer. When it fires it clears
		 * pr_responder_mode so the cancel callback stops restarting
		 * chunks. Defaults to PR_PASN_RESPONDER_ROC_DURATION;
		 * overridden by continuous_ranging_session_time when non-zero.
		 */
		if (pr && pr->pr_pasn_params &&
		    pr->pr_pasn_params->continuous_ranging_session_time > 0)
			roc_time_ms = pr->pr_pasn_params->continuous_ranging_session_time;

		eloop_register_timeout(roc_time_ms / 1000,
				       (roc_time_ms % 1000) * 1000,
				       wpas_pr_pasn_roc_total_timeout,
				       wpa_s, NULL);
		return 0;
	}

	/*
	 * PASN initiator role: create the PD wdev if src_addr is provided,
	 * then queue the radio work to send M1.
	 */
	if (src_addr && !is_zero_ether_addr(src_addr)) {
		if (wpas_pr_start_pd(wpa_s, src_addr) < 0) {
			wpa_printf(MSG_INFO,
				   "PR PASN: Failed to create PD wdev");
			return -1;
		}
	}

	wpas_pr_pasn_cancel_auth_work(wpa_s);
	wpa_s->pr_pasn_auth_work = NULL;

	awork = os_zalloc(sizeof(*awork));
	if (!awork) {
		wpas_pr_pd_stop(wpa_s);
		return -1;
	}

	awork->freq = freq;
	os_memcpy(awork->peer_addr, peer_addr, ETH_ALEN);
	awork->ranging_role = ranging_role;
	awork->ranging_type = ranging_type;
	awork->auth_mode = auth_mode;
	awork->forced_pr_freq = forced_pr_freq;

	if (!radio_add_work(wpa_s, freq, "pr-pasn-start-auth", 1,
			    wpas_pr_pasn_auth_start_cb, awork)) {
		wpas_pr_pasn_free_auth_work(awork);
		wpas_pr_pd_stop(wpa_s);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "PR PASN: Authentication work successfully added");
	return 0;
}


/**
 * wpas_pr_validate_ranging_request - Validate PR ranging request parameters
 */
static int
wpas_pr_validate_ranging_request(struct wpa_supplicant *wpa_s,
				 struct pr_pasn_ranging_params *pr_pasn_params)
{
	struct pr_data *pr = wpa_s->global->pr;
	struct pr_config *cfg;
	bool is_edca, is_ntb, is_ista;

	if (!pr || !pr->cfg) {
		wpa_printf(MSG_INFO, "PR: PR not initialized");
		return -1;
	}

	cfg = pr->cfg;

	/* Peer address must not be all-zeros */
	if (is_zero_ether_addr(pr_pasn_params->peer_addr)) {
		wpa_printf(MSG_INFO, "PR: Invalid peer address (all zeros)");
		return -1;
	}

	/* Frequency must be set */
	if (!pr_pasn_params->freq) {
		wpa_printf(MSG_INFO, "PR: Invalid frequency (zero)");
		return -1;
	}

	/* ranging_type must have at least one valid bit and no unknown bits */
	if (!pr_pasn_params->ranging_type ||
	    (pr_pasn_params->ranging_type &
	     ~(PR_EDCA_BASED_RANGING | PR_NTB_SECURE_LTF_BASED_RANGING |
	       PR_NTB_OPEN_BASED_RANGING))) {
		wpa_printf(MSG_INFO, "PR: Invalid ranging_type=0x%x",
			   pr_pasn_params->ranging_type);
		return -1;
	}

	/* ranging_role must have at least one valid bit and no unknown bits */
	if (!pr_pasn_params->ranging_role ||
	    (pr_pasn_params->ranging_role &
	     ~(PR_ISTA_SUPPORT | PR_RSTA_SUPPORT))) {
		wpa_printf(MSG_INFO, "PR: Invalid ranging_role=0x%x",
			   pr_pasn_params->ranging_role);
		return -1;
	}

	is_edca = pr_pasn_params->ranging_type & PR_EDCA_BASED_RANGING;
	is_ntb = pr_pasn_params->ranging_type &
		(PR_NTB_SECURE_LTF_BASED_RANGING |
		 PR_NTB_OPEN_BASED_RANGING);
	is_ista = pr_pasn_params->ranging_role & PR_ISTA_SUPPORT;

	/* Validate ranging type against device capabilities */
	if (is_edca) {
		if (is_ista && !cfg->edca_ista_support) {
			wpa_printf(MSG_INFO,
				   "PR: EDCA ISTA ranging not supported by device");
			return -1;
		}
		if (!is_ista && !cfg->edca_rsta_support) {
			wpa_printf(MSG_INFO,
				   "PR: EDCA RSTA ranging not supported by device");
			return -1;
		}
	}

	if (is_ntb) {
		if (is_ista && !cfg->ntb_ista_support) {
			wpa_printf(MSG_INFO,
				   "PR: NTB ISTA ranging not supported by device");
			return -1;
		}
		if (!is_ista && !cfg->ntb_rsta_support) {
			wpa_printf(MSG_INFO,
				   "PR: NTB RSTA ranging not supported by device");
			return -1;
		}

		/* Secure LTF requires explicit device support */
		if ((pr_pasn_params->ranging_type &
		     PR_NTB_SECURE_LTF_BASED_RANGING) &&
		    !cfg->secure_he_ltf) {
			wpa_printf(MSG_INFO,
				   "PR: Secure HE-LTF NTB ranging not supported by device");
			return -1;
		}

		/* min must not exceed max time between measurements */
		if (pr_pasn_params->min_time_between_measurements &&
		    pr_pasn_params->max_time_between_measurements &&
		    pr_pasn_params->min_time_between_measurements >
		    pr_pasn_params->max_time_between_measurements * 100) {
			wpa_printf(MSG_INFO,
				   "PR: min_time_between_measurements=%u > max=%u (units: 100us vs 10ms)",
				   pr_pasn_params->min_time_between_measurements,
				   pr_pasn_params->max_time_between_measurements);
			return -1;
		}
	}

	/* lmr_feedback is only valid for NTB ranging */
	if (pr_pasn_params->lmr_feedback && !is_ntb) {
		wpa_printf(MSG_INFO,
			   "PR: lmr_feedback is only valid for NTB ranging");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PR: Ranging request validation successful");
	return 0;
}


/**
 * wpas_pr_pasn_trigger - Entry point to trigger PASN authentication for PR
 */
int wpas_pr_pasn_trigger(struct wpa_supplicant *wpa_s,
			 struct pr_pasn_ranging_params *pr_pasn_params)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr_pasn_params) {
		wpa_printf(MSG_DEBUG, "PR PASN: trigger: NULL params");
		return -1;
	}

	if (!pr) {
		wpa_printf(MSG_DEBUG, "PR PASN: trigger: PR not initialized");
		return -1;
	}

	if (pr->pr_pasn_params) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: auth_trigger: Already in progress");
		pr_pasn_params->pr_pasn_status = PASN_STATUS_FAILURE;
		return -1;
	}

	/* Validate request before proceeding */
	if (wpas_pr_validate_ranging_request(wpa_s, pr_pasn_params) < 0) {
		wpa_printf(MSG_DEBUG, "PR PASN: Request validation failed");
		pr_pasn_params->pr_pasn_status = PASN_STATUS_FAILURE;
		return -1;
	}

	if (pr_pasn_params->action == PR_PASN_AND_RANGING) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Triggering PASN authentication for " MACSTR
			   " type=%u role=%u mode=%u freq=%d",
			   MAC2STR(pr_pasn_params->peer_addr),
			   pr_pasn_params->ranging_type,
			   pr_pasn_params->ranging_role,
			   pr_pasn_params->auth_mode,
			   pr_pasn_params->freq);

		/* Allocate and store the params to track the request */
		pr->pr_pasn_params = os_zalloc(sizeof(*pr->pr_pasn_params));
		if (!pr->pr_pasn_params) {
			wpa_printf(MSG_INFO,
				   "PR PASN: Failed to allocate params");
			pr_pasn_params->pr_pasn_status = PASN_STATUS_FAILURE;
			return -1;
		}

		os_memcpy(pr->pr_pasn_params, pr_pasn_params,
			  sizeof(*pr->pr_pasn_params));

		/* Ensure peer device entry exists before setting credentials */
		if (pr_pasn_params->pmk_len > 0 ||
		    pr_pasn_params->password_valid) {
			if (pr_ensure_oob_peer(pr, pr_pasn_params->peer_addr,
					       pr_pasn_params->freq) < 0 ||
			    pr_set_peer_credentials(
				    pr, pr_pasn_params->peer_addr,
				    pr_pasn_params->pmk_len > 0 ?
				    pr_pasn_params->pmk : NULL,
				    pr_pasn_params->pmk_len,
				    pr_pasn_params->password_valid ?
				    pr_pasn_params->password : NULL) < 0) {
				pr_pasn_params->pr_pasn_status =
					PASN_STATUS_FAILURE;
				wpas_pr_clear_ranging_params(pr);
				return -1;
			}
		}

		/* Log EDCA parameters if applicable */
		if (pr_pasn_params->ranging_type & PR_EDCA_BASED_RANGING) {
			wpa_printf(MSG_DEBUG,
				   "PR PASN: EDCA params - burst_period=%u num_bursts_exp=%u ftms_per_burst=%u ftmr_retries=%u burst_duration=%u",
				   pr_pasn_params->burst_period,
				   pr_pasn_params->num_bursts_exp,
				   pr_pasn_params->ftms_per_burst,
				   pr_pasn_params->ftmr_retries,
				   pr_pasn_params->burst_duration);
		}

		/* Log NTB parameters if applicable */
		if (pr_pasn_params->ranging_type &
		    (PR_NTB_SECURE_LTF_BASED_RANGING |
		     PR_NTB_OPEN_BASED_RANGING)) {
			wpa_printf(MSG_DEBUG,
				   "PR PASN: NTB params - min_time=%u max_time=%u aw=%u nominal_time=%u",
				   pr_pasn_params->min_time_between_measurements,
				   pr_pasn_params->max_time_between_measurements,
				   pr_pasn_params->availability_window,
				   pr_pasn_params->nominal_time);
		}

		/* Log location request parameters */
		if (pr_pasn_params->request_lci ||
		    pr_pasn_params->request_civicloc) {
			wpa_printf(MSG_DEBUG,
				   "PR PASN: Location requests - LCI=%d CivicLoc=%d",
				   pr_pasn_params->request_lci,
				   pr_pasn_params->request_civicloc);
		}

		/* Initiate PASN authentication for the peer */
		if (wpas_pr_initiate_pasn_auth(wpa_s, pr_pasn_params->peer_addr,
					       pr_pasn_params->freq,
					       pr_pasn_params->auth_mode,
					       pr_pasn_params->ranging_role,
					       pr_pasn_params->ranging_type,
					       pr_pasn_params->forced_pr_freq,
					       pr_pasn_params->src_addr,
					       pr_pasn_params->pasn_role)) {
			wpa_printf(MSG_DEBUG,
				   "PR PASN: Failed to initiate PASN for "
				   MACSTR,
				   MAC2STR(pr_pasn_params->peer_addr));
			pr_pasn_params->pr_pasn_status = PASN_STATUS_FAILURE;
			wpas_pr_clear_ranging_params(pr);
			return -1;
		}
		return 0;
	} else {
		wpa_printf(MSG_INFO,
			   "PR PASN: Unsupported action %u, ignoring request",
			   pr_pasn_params->action);
		pr_pasn_params->pr_pasn_status = PASN_STATUS_FAILURE;
		return -1;
	}
}


int wpas_pr_pasn_auth_tx_status(struct wpa_supplicant *wpa_s, const u8 *data,
				size_t data_len, bool acked)
{
	struct pr_data *pr = wpa_s->global->pr;
	int ret;

	if (!wpa_s->pr_pasn_auth_work && is_zero_ether_addr(wpa_s->pd_addr))
		return -1;

	ret = pr_pasn_auth_tx_status(pr, data, data_len, acked);
	if (ret == 2) {
		eloop_cancel_timeout(wpas_pr_pasn_auth_retry_timeout, wpa_s,
				     NULL);
		eloop_register_timeout(0,
				       PR_PASN_AUTH1_RETRY_INTERVAL_MS * 1000,
				       wpas_pr_pasn_auth_retry_timeout,
				       wpa_s, NULL);
		return 0;
	}

	return ret;
}


static void wpas_pr_pasn_auth_retry_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr || !pr->pr_pasn_params)
		return;

	pr_pasn_auth_retransmit(pr, pr->pr_pasn_params->peer_addr);
}


static int wpas_pr_check_pd_wdev_create(struct wpa_supplicant *wpa_s)
{
	if (is_zero_ether_addr(wpa_s->pr_responder_src_addr)) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: M1 received in responder mode, no custom MAC - using existing interface");
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "PR PASN: M1 received in responder mode, creating PD wdev");

	if (wpas_pr_start_pd(wpa_s, wpa_s->pr_responder_src_addr) < 0) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to create PD wdev for responder");
		return -1;
	}

	return 0;
}


int wpas_pr_pasn_auth_rx(struct wpa_supplicant *wpa_s,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 int freq)
{
	struct pr_data *pr = wpa_s->global->pr;
	int ret;

	if (!pr)
		return -2;

	/*
	 * Responder path: when we are waiting for PASN M1 on the parent
	 * interface ROC, create the PD wdev on first receipt of Auth1 before
	 * handing the frame to the common layer.
	 */
	if (wpa_s->pr_responder_mode &&
	    len >= offsetof(struct ieee80211_mgmt, u.auth.variable)) {
		u16 auth_transaction;

		auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);

		if (auth_transaction == WLAN_AUTH_TR_SEQ_PASN_AUTH1) {
			if (wpas_pr_check_pd_wdev_create(wpa_s) < 0)
				return -1;

			/*
			 * Cancel the total-budget timer first so it does not
			 * fire after we have already handed off to the PASN
			 * layer.
			 */
			eloop_cancel_timeout(wpas_pr_pasn_roc_total_timeout,
					     wpa_s, NULL);

			/*
			 * Cancel ROC on the listening interface; the dedicated
			 * PR interface (if created) will handle all subsequent
			 * frames. wpas_pr_cancel_roc() also flushes any pending
			 * pr-pasn-roc chunks that were queued but not yet
			 * started (M1 can arrive between two ROC chunks when
			 * pr_roc_work is already NULL but a next chunk is
			 * already queued).
			 */
			wpas_pr_cancel_roc(wpa_s);

			/* Clear responder mode */
			wpa_s->pr_responder_mode = false;
			os_memset(wpa_s->pr_responder_src_addr, 0, ETH_ALEN);

			wpa_printf(MSG_DEBUG,
				   "PR PASN: M1 processed, proceeding with PASN");
		}
	}

	ret = pr_pasn_auth_rx(pr, mgmt, len, freq);

	if (ret < 0) {
		eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);
		wpas_pr_pasn_auth_work_done(wpa_s);
		wpas_pr_pd_stop(wpa_s);
		wpas_pr_clear_ranging_params(wpa_s->global->pr);
		wpas_notify_pr_ranging_terminated(wpa_s,
						  PR_SESSION_END_NEG_FAILED);
	}

	return ret;
}


void wpas_pr_abort_ranging(struct wpa_supplicant *wpa_s)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr) {
		wpa_printf(MSG_DEBUG, "PR: abort_ranging: PR not initialized");
		return;
	}

	/* Check if there's an active ranging session */
	if (is_zero_ether_addr(wpa_s->pd_addr) && !pr->pr_pasn_params) {
		wpa_printf(MSG_DEBUG,
			   "PR: abort_ranging: no active ranging session");
		return;
	}

	wpa_printf(MSG_DEBUG, "PR: Aborting ranging session");

	/*
	 * Cancel PASN and ROC in case PD wdev is not yet created
	 * (PASN still in progress or responder ROC active).
	 */
	wpas_pr_pasn_cancel_auth_work(wpa_s);
	wpa_s->pr_pasn_auth_work = NULL;
	if (wpa_s->pr_responder_mode) {
		wpas_pr_cancel_roc(wpa_s);
		wpas_pr_pasn_abort_responder(wpa_s);
	}

	/* Stop PD wdev and cleanup all ranging resources */
	wpas_pr_pd_stop(wpa_s);

	eloop_cancel_timeout(wpas_pr_pasn_auth_retry_timeout, wpa_s, NULL);
	/* Free ranging params so a new session can be started */
	wpas_pr_clear_ranging_params(pr);
	wpas_notify_pr_ranging_terminated(wpa_s, PR_SESSION_END_USER_ABORT);
}

#endif /* CONFIG_PASN */
