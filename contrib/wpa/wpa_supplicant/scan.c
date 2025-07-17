/*
 * WPA Supplicant - Scanning
 * Copyright (c) 2003-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "wps_supplicant.h"
#include "p2p_supplicant.h"
#include "p2p/p2p.h"
#include "hs20_supplicant.h"
#include "notify.h"
#include "bss.h"
#include "scan.h"
#include "mesh.h"

static struct wpabuf * wpa_supplicant_extra_ies(struct wpa_supplicant *wpa_s);


static void wpa_supplicant_gen_assoc_event(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;
	union wpa_event_data data;

	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL)
		return;

	if (wpa_s->current_ssid == NULL) {
		wpa_s->current_ssid = ssid;
		wpas_notify_network_changed(wpa_s);
	}
	wpa_supplicant_initiate_eapol(wpa_s);
	wpa_dbg(wpa_s, MSG_DEBUG, "Already associated with a configured "
		"network - generating associated event");
	os_memset(&data, 0, sizeof(data));
	wpa_supplicant_event(wpa_s, EVENT_ASSOC, &data);
}


#ifdef CONFIG_WPS
static int wpas_wps_in_use(struct wpa_supplicant *wpa_s,
			   enum wps_request_type *req_type)
{
	struct wpa_ssid *ssid;
	int wps = 0;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!(ssid->key_mgmt & WPA_KEY_MGMT_WPS))
			continue;

		wps = 1;
		*req_type = wpas_wps_get_req_type(ssid);
		if (ssid->eap.phase1 && os_strstr(ssid->eap.phase1, "pbc=1"))
			return 2;
	}

#ifdef CONFIG_P2P
	if (!wpa_s->global->p2p_disabled && wpa_s->global->p2p &&
	    !wpa_s->conf->p2p_disabled) {
		wpa_s->wps->dev.p2p = 1;
		if (!wps) {
			wps = 1;
			*req_type = WPS_REQ_ENROLLEE_INFO;
		}
	}
#endif /* CONFIG_P2P */

	return wps;
}
#endif /* CONFIG_WPS */


static int wpa_setup_mac_addr_rand_params(struct wpa_driver_scan_params *params,
					  const u8 *mac_addr)
{
	u8 *tmp;

	if (params->mac_addr) {
		params->mac_addr_mask = NULL;
		os_free(params->mac_addr);
		params->mac_addr = NULL;
	}

	params->mac_addr_rand = 1;

	if (!mac_addr)
		return 0;

	tmp = os_malloc(2 * ETH_ALEN);
	if (!tmp)
		return -1;

	os_memcpy(tmp, mac_addr, 2 * ETH_ALEN);
	params->mac_addr = tmp;
	params->mac_addr_mask = tmp + ETH_ALEN;
	return 0;
}


/**
 * wpa_supplicant_enabled_networks - Check whether there are enabled networks
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 if no networks are enabled, >0 if networks are enabled
 *
 * This function is used to figure out whether any networks (or Interworking
 * with enabled credentials and auto_interworking) are present in the current
 * configuration.
 */
int wpa_supplicant_enabled_networks(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid = wpa_s->conf->ssid;
	int count = 0, disabled = 0;

	if (wpa_s->p2p_mgmt)
		return 0; /* no normal network profiles on p2p_mgmt interface */

	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid))
			count++;
		else
			disabled++;
		ssid = ssid->next;
	}
	if (wpa_s->conf->cred && wpa_s->conf->interworking &&
	    wpa_s->conf->auto_interworking)
		count++;
	if (count == 0 && disabled > 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "No enabled networks (%d disabled "
			"networks)", disabled);
	}
	return count;
}


static void wpa_supplicant_assoc_try(struct wpa_supplicant *wpa_s,
				     struct wpa_ssid *ssid)
{
	int min_temp_disabled = 0;

	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid)) {
			int temp_disabled = wpas_temp_disabled(wpa_s, ssid);

			if (temp_disabled <= 0)
				break;

			if (!min_temp_disabled ||
			    temp_disabled < min_temp_disabled)
				min_temp_disabled = temp_disabled;
		}
		ssid = ssid->next;
	}

	/* ap_scan=2 mode - try to associate with each SSID. */
	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "wpa_supplicant_assoc_try: Reached "
			"end of scan list - go back to beginning");
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
		wpa_supplicant_req_scan(wpa_s, min_temp_disabled, 0);
		return;
	}
	if (ssid->next) {
		/* Continue from the next SSID on the next attempt. */
		wpa_s->prev_scan_ssid = ssid;
	} else {
		/* Start from the beginning of the SSID list. */
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
	}
	wpa_supplicant_associate(wpa_s, NULL, ssid);
}


static void wpas_trigger_scan_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_driver_scan_params *params = work->ctx;
	int ret;

	if (deinit) {
		if (!work->started) {
			wpa_scan_free_params(params);
			return;
		}
		wpa_supplicant_notify_scanning(wpa_s, 0);
		wpas_notify_scan_done(wpa_s, 0);
		wpa_s->scan_work = NULL;
		return;
	}

	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_SCAN) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(params, wpa_s->mac_addr_scan);

	if (wpas_update_random_addr_disassoc(wpa_s) < 0) {
		wpa_msg(wpa_s, MSG_INFO,
			"Failed to assign random MAC address for a scan");
		wpa_scan_free_params(params);
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCAN_FAILED "ret=-1");
		radio_work_done(work);
		return;
	}

	wpa_supplicant_notify_scanning(wpa_s, 1);

	if (wpa_s->clear_driver_scan_cache) {
		wpa_printf(MSG_DEBUG,
			   "Request driver to clear scan cache due to local BSS flush");
		params->only_new_results = 1;
	}
	ret = wpa_drv_scan(wpa_s, params);
	/*
	 * Store the obtained vendor scan cookie (if any) in wpa_s context.
	 * The current design is to allow only one scan request on each
	 * interface, hence having this scan cookie stored in wpa_s context is
	 * fine for now.
	 *
	 * Revisit this logic if concurrent scan operations per interface
	 * is supported.
	 */
	if (ret == 0)
		wpa_s->curr_scan_cookie = params->scan_cookie;
	wpa_scan_free_params(params);
	work->ctx = NULL;
	if (ret) {
		int retry = wpa_s->last_scan_req != MANUAL_SCAN_REQ &&
			!wpa_s->beacon_rep_data.token;

		if (wpa_s->disconnected)
			retry = 0;

		/* do not retry if operation is not supported */
		if (ret == -EOPNOTSUPP)
			retry = 0;

		wpa_supplicant_notify_scanning(wpa_s, 0);
		wpas_notify_scan_done(wpa_s, 0);
		if (wpa_s->wpa_state == WPA_SCANNING)
			wpa_supplicant_set_state(wpa_s,
						 wpa_s->scan_prev_wpa_state);
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCAN_FAILED "ret=%d%s",
			ret, retry ? " retry=1" : "");
		radio_work_done(work);

		if (retry) {
			/* Restore scan_req since we will try to scan again */
			wpa_s->scan_req = wpa_s->last_scan_req;
			wpa_supplicant_req_scan(wpa_s, 1, 0);
		} else if (wpa_s->scan_res_handler) {
			/* Clear the scan_res_handler */
			wpa_s->scan_res_handler = NULL;
		}

#ifndef CONFIG_NO_RRM
		if (wpa_s->beacon_rep_data.token)
			wpas_rrm_refuse_request(wpa_s);
#endif /* CONFIG_NO_RRM */

		return;
	}

	os_get_reltime(&wpa_s->scan_trigger_time);
	wpa_s->scan_runs++;
	wpa_s->normal_scans++;
	wpa_s->own_scan_requested = 1;
	wpa_s->clear_driver_scan_cache = 0;
	wpa_s->scan_work = work;
}


/**
 * wpa_supplicant_trigger_scan - Request driver to start a scan
 * @wpa_s: Pointer to wpa_supplicant data
 * @params: Scan parameters
 * @default_ies: Whether or not to use the default IEs in the Probe Request
 * frames. Note that this will free any existing IEs set in @params, so this
 * shouldn't be set if the IEs have already been set with
 * wpa_supplicant_extra_ies(). Otherwise, wpabuf_free() will lead to a
 * double-free.
 * @next: Whether or not to perform this scan as the next radio work
 * Returns: 0 on success, -1 on failure
 */
int wpa_supplicant_trigger_scan(struct wpa_supplicant *wpa_s,
				struct wpa_driver_scan_params *params,
				bool default_ies, bool next)
{
	struct wpa_driver_scan_params *ctx;
	struct wpabuf *ies = NULL;

	if (wpa_s->scan_work) {
		wpa_dbg(wpa_s, MSG_INFO, "Reject scan trigger since one is already pending");
		return -1;
	}

	if (default_ies) {
		if (params->extra_ies_len) {
			os_free((u8 *) params->extra_ies);
			params->extra_ies = NULL;
			params->extra_ies_len = 0;
		}
		ies = wpa_supplicant_extra_ies(wpa_s);
		if (ies) {
			params->extra_ies = wpabuf_head(ies);
			params->extra_ies_len = wpabuf_len(ies);
		}
	}
	ctx = wpa_scan_clone_params(params);
	if (ies) {
		wpabuf_free(ies);
		params->extra_ies = NULL;
		params->extra_ies_len = 0;
	}
	wpa_s->last_scan_all_chan = !params->freqs;
	wpa_s->last_scan_non_coloc_6ghz = params->non_coloc_6ghz;

	if (wpa_s->crossed_6ghz_dom) {
		wpa_printf(MSG_DEBUG, "First scan after crossing 6 GHz domain");
		wpa_s->crossed_6ghz_dom = false;
	}

	if (!ctx ||
	    radio_add_work(wpa_s, 0, "scan", next, wpas_trigger_scan_cb,
			   ctx) < 0) {
		wpa_scan_free_params(ctx);
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCAN_FAILED "ret=-1");
		return -1;
	}

	wpa_s->wps_scan_done = false;

	return 0;
}


static void
wpa_supplicant_delayed_sched_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_dbg(wpa_s, MSG_DEBUG, "Starting delayed sched scan");

	if (wpa_supplicant_req_sched_scan(wpa_s))
		wpa_supplicant_req_scan(wpa_s, 0, 0);
}


static void
wpa_supplicant_sched_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_dbg(wpa_s, MSG_DEBUG, "Sched scan timeout - stopping it");

	wpa_s->sched_scan_timed_out = 1;
	wpa_supplicant_cancel_sched_scan(wpa_s);
}


static int
wpa_supplicant_start_sched_scan(struct wpa_supplicant *wpa_s,
				struct wpa_driver_scan_params *params)
{
	int ret;

	wpa_supplicant_notify_scanning(wpa_s, 1);
	ret = wpa_drv_sched_scan(wpa_s, params);
	if (ret)
		wpa_supplicant_notify_scanning(wpa_s, 0);
	else
		wpa_s->sched_scanning = 1;

	return ret;
}


static int wpa_supplicant_stop_sched_scan(struct wpa_supplicant *wpa_s)
{
	int ret;

	ret = wpa_drv_stop_sched_scan(wpa_s);
	if (ret) {
		wpa_dbg(wpa_s, MSG_DEBUG, "stopping sched_scan failed!");
		/* TODO: what to do if stopping fails? */
		return -1;
	}

	return ret;
}


static struct wpa_driver_scan_filter *
wpa_supplicant_build_filter_ssids(struct wpa_config *conf, size_t *num_ssids)
{
	struct wpa_driver_scan_filter *ssids;
	struct wpa_ssid *ssid;
	size_t count;

	*num_ssids = 0;
	if (!conf->filter_ssids)
		return NULL;

	for (count = 0, ssid = conf->ssid; ssid; ssid = ssid->next) {
		if (ssid->ssid && ssid->ssid_len)
			count++;
	}
	if (count == 0)
		return NULL;
	ssids = os_calloc(count, sizeof(struct wpa_driver_scan_filter));
	if (ssids == NULL)
		return NULL;

	for (ssid = conf->ssid; ssid; ssid = ssid->next) {
		if (!ssid->ssid || !ssid->ssid_len)
			continue;
		os_memcpy(ssids[*num_ssids].ssid, ssid->ssid, ssid->ssid_len);
		ssids[*num_ssids].ssid_len = ssid->ssid_len;
		(*num_ssids)++;
	}

	return ssids;
}


static void wpa_supplicant_optimize_freqs(
	struct wpa_supplicant *wpa_s, struct wpa_driver_scan_params *params)
{
#ifdef CONFIG_P2P
	if (params->freqs == NULL && wpa_s->p2p_in_provisioning &&
	    wpa_s->go_params) {
		/* Optimize provisioning state scan based on GO information */
		if (wpa_s->p2p_in_provisioning < 5 &&
		    wpa_s->go_params->freq > 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Scan only GO "
				"preferred frequency %d MHz",
				wpa_s->go_params->freq);
			params->freqs = os_calloc(2, sizeof(int));
			if (params->freqs)
				params->freqs[0] = wpa_s->go_params->freq;
		} else if (wpa_s->p2p_in_provisioning < 8 &&
			   wpa_s->go_params->freq_list[0]) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Scan only common "
				"channels");
			int_array_concat(&params->freqs,
					 wpa_s->go_params->freq_list);
			if (params->freqs)
				int_array_sort_unique(params->freqs);
		}
		wpa_s->p2p_in_provisioning++;
	}

	if (params->freqs == NULL && wpa_s->p2p_in_invitation) {
		struct wpa_ssid *ssid = wpa_s->current_ssid;

		/*
		 * Perform a single-channel scan if the GO has already been
		 * discovered on another non-P2P interface. Note that a scan
		 * initiated by a P2P interface (e.g., the device interface)
		 * should already have sufficient IEs and scan results will be
		 * fetched on interface creation in that case.
		 */
		if (wpa_s->p2p_in_invitation == 1 && ssid) {
			struct wpa_supplicant *ifs;
			struct wpa_bss *bss = NULL;
			const u8 *bssid = ssid->bssid_set ? ssid->bssid : NULL;

			dl_list_for_each(ifs, &wpa_s->radio->ifaces,
					 struct wpa_supplicant, radio_list) {
				bss = wpa_bss_get(ifs, bssid, ssid->ssid,
						  ssid->ssid_len);
				if (bss)
					break;
			}
			if (bss && !disabled_freq(wpa_s, bss->freq)) {
				params->freqs = os_calloc(2, sizeof(int));
				if (params->freqs) {
					wpa_dbg(wpa_s, MSG_DEBUG,
						"P2P: Scan only the known GO frequency %d MHz during invitation",
						bss->freq);
					params->freqs[0] = bss->freq;
				}
			}
		}

		/*
		 * Optimize scan based on GO information during persistent
		 * group reinvocation
		 */
		if (!params->freqs && wpa_s->p2p_in_invitation < 5 &&
		    wpa_s->p2p_invite_go_freq > 0) {
			if (wpa_s->p2p_invite_go_freq == 2 ||
			    wpa_s->p2p_invite_go_freq == 5) {
				enum hostapd_hw_mode mode;

				wpa_dbg(wpa_s, MSG_DEBUG,
					"P2P: Scan only GO preferred band %d GHz during invitation",
					wpa_s->p2p_invite_go_freq);

				if (!wpa_s->hw.modes)
					return;
				mode = wpa_s->p2p_invite_go_freq == 5 ?
					HOSTAPD_MODE_IEEE80211A :
					HOSTAPD_MODE_IEEE80211G;
				if (wpa_s->p2p_in_invitation <= 2)
					wpa_add_scan_freqs_list(wpa_s, mode,
								params, false,
								false, true);
				if (!params->freqs || params->freqs[0] == 0)
					wpa_add_scan_freqs_list(wpa_s, mode,
								params, false,
								false, false);
			} else {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"P2P: Scan only GO preferred frequency %d MHz during invitation",
					wpa_s->p2p_invite_go_freq);
				params->freqs = os_calloc(2, sizeof(int));
				if (params->freqs)
					params->freqs[0] =
					    wpa_s->p2p_invite_go_freq;
			}
		}
		wpa_s->p2p_in_invitation++;
		if (wpa_s->p2p_in_invitation > 20) {
			/*
			 * This should not really happen since the variable is
			 * cleared on group removal, but if it does happen, make
			 * sure we do not get stuck in special invitation scan
			 * mode.
			 */
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Clear p2p_in_invitation");
			wpa_s->p2p_in_invitation = 0;
			wpa_s->p2p_retry_limit = 0;
		}
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_WPS
	if (params->freqs == NULL && wpa_s->after_wps && wpa_s->wps_freq) {
		/*
		 * Optimize post-provisioning scan based on channel used
		 * during provisioning.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "WPS: Scan only frequency %u MHz "
			"that was used during provisioning", wpa_s->wps_freq);
		params->freqs = os_calloc(2, sizeof(int));
		if (params->freqs)
			params->freqs[0] = wpa_s->wps_freq;
		wpa_s->after_wps--;
	} else if (wpa_s->after_wps)
		wpa_s->after_wps--;

	if (params->freqs == NULL && wpa_s->known_wps_freq && wpa_s->wps_freq)
	{
		/* Optimize provisioning scan based on already known channel */
		wpa_dbg(wpa_s, MSG_DEBUG, "WPS: Scan only frequency %u MHz",
			wpa_s->wps_freq);
		params->freqs = os_calloc(2, sizeof(int));
		if (params->freqs)
			params->freqs[0] = wpa_s->wps_freq;
		wpa_s->known_wps_freq = 0; /* only do this once */
	}
#endif /* CONFIG_WPS */
}


#ifdef CONFIG_INTERWORKING
static void wpas_add_interworking_elements(struct wpa_supplicant *wpa_s,
					   struct wpabuf *buf)
{
	wpabuf_put_u8(buf, WLAN_EID_INTERWORKING);
	wpabuf_put_u8(buf, is_zero_ether_addr(wpa_s->conf->hessid) ? 1 :
		      1 + ETH_ALEN);
	wpabuf_put_u8(buf, wpa_s->conf->access_network_type);
	/* No Venue Info */
	if (!is_zero_ether_addr(wpa_s->conf->hessid))
		wpabuf_put_data(buf, wpa_s->conf->hessid, ETH_ALEN);
}
#endif /* CONFIG_INTERWORKING */


#ifdef CONFIG_MBO
static void wpas_fils_req_param_add_max_channel(struct wpa_supplicant *wpa_s,
						struct wpabuf **ie)
{
	if (wpabuf_resize(ie, 5)) {
		wpa_printf(MSG_DEBUG,
			   "Failed to allocate space for FILS Request Parameters element");
		return;
	}

	/* FILS Request Parameters element */
	wpabuf_put_u8(*ie, WLAN_EID_EXTENSION);
	wpabuf_put_u8(*ie, 3); /* FILS Request attribute length */
	wpabuf_put_u8(*ie, WLAN_EID_EXT_FILS_REQ_PARAMS);
	/* Parameter control bitmap */
	wpabuf_put_u8(*ie, 0);
	/* Max Channel Time field - contains the value of MaxChannelTime
	 * parameter of the MLME-SCAN.request primitive represented in units of
	 * TUs, as an unsigned integer. A Max Channel Time field value of 255
	 * is used to indicate any duration of more than 254 TUs, or an
	 * unspecified or unknown duration. (IEEE Std 802.11ai-2016, 9.4.2.178)
	 */
	wpabuf_put_u8(*ie, 255);
}
#endif /* CONFIG_MBO */


void wpa_supplicant_set_default_scan_ies(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *default_ies = NULL;
	u8 ext_capab[18];
	int ext_capab_len, frame_id;
	enum wpa_driver_if_type type = WPA_IF_STATION;

#ifdef CONFIG_P2P
	if (wpa_s->p2p_group_interface == P2P_GROUP_INTERFACE_CLIENT)
		type = WPA_IF_P2P_CLIENT;
#endif /* CONFIG_P2P */

	wpa_drv_get_ext_capa(wpa_s, type);

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab,
					     sizeof(ext_capab), NULL);
	if (ext_capab_len > 0 &&
	    wpabuf_resize(&default_ies, ext_capab_len) == 0)
		wpabuf_put_data(default_ies, ext_capab, ext_capab_len);

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		wpas_fils_req_param_add_max_channel(wpa_s, &default_ies);
	/* Send MBO and OCE capabilities */
	if (wpabuf_resize(&default_ies, 12) == 0)
		wpas_mbo_scan_ie(wpa_s, default_ies);
#endif /* CONFIG_MBO */

	if (type == WPA_IF_P2P_CLIENT)
		frame_id = VENDOR_ELEM_PROBE_REQ_P2P;
	else
		frame_id = VENDOR_ELEM_PROBE_REQ;

	if (wpa_s->vendor_elem[frame_id]) {
		size_t len;

		len = wpabuf_len(wpa_s->vendor_elem[frame_id]);
		if (len > 0 && wpabuf_resize(&default_ies, len) == 0)
			wpabuf_put_buf(default_ies,
				       wpa_s->vendor_elem[frame_id]);
	}

	if (default_ies)
		wpa_drv_set_default_scan_ies(wpa_s, wpabuf_head(default_ies),
					     wpabuf_len(default_ies));
	wpabuf_free(default_ies);
}


static struct wpabuf * wpa_supplicant_ml_probe_ie(int mld_id, u16 links)
{
	struct wpabuf *extra_ie;
	u16 control = MULTI_LINK_CONTROL_TYPE_PROBE_REQ;
	size_t len = 3 + 4 + 4 * MAX_NUM_MLD_LINKS;
	u8 link_id;
	u8 *len_pos;

	if (mld_id >= 0) {
		control |= EHT_ML_PRES_BM_PROBE_REQ_AP_MLD_ID;
		len++;
	}

	extra_ie = wpabuf_alloc(len);
	if (!extra_ie)
		return NULL;

	wpabuf_put_u8(extra_ie, WLAN_EID_EXTENSION);
	len_pos = wpabuf_put(extra_ie, 1);
	wpabuf_put_u8(extra_ie, WLAN_EID_EXT_MULTI_LINK);

	wpabuf_put_le16(extra_ie, control);

	/* common info length and MLD ID (if requested) */
	if (mld_id >= 0) {
		wpabuf_put_u8(extra_ie, 2);
		wpabuf_put_u8(extra_ie, mld_id);

		wpa_printf(MSG_DEBUG, "MLD: ML probe targeted at MLD ID %d",
			   mld_id);
	} else {
		wpabuf_put_u8(extra_ie, 1);

		wpa_printf(MSG_DEBUG, "MLD: ML probe targeted at receiving AP");
	}

	if (!links)
		wpa_printf(MSG_DEBUG, "MLD: Probing all links");
	else
		wpa_printf(MSG_DEBUG, "MLD: Probing links 0x%04x", links);

	for_each_link(links, link_id) {
		wpabuf_put_u8(extra_ie, EHT_ML_SUB_ELEM_PER_STA_PROFILE);

		/* Subelement length includes only the control */
		wpabuf_put_u8(extra_ie, 2);

		control = link_id | EHT_PER_STA_CTRL_COMPLETE_PROFILE_MSK;

		wpabuf_put_le16(extra_ie, control);
	}

	*len_pos = (u8 *) wpabuf_put(extra_ie, 0) - len_pos - 1;

	return extra_ie;
}


static struct wpabuf * wpa_supplicant_extra_ies(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *extra_ie = NULL;
	u8 ext_capab[18];
	int ext_capab_len;
#ifdef CONFIG_WPS
	int wps = 0;
	enum wps_request_type req_type = WPS_REQ_ENROLLEE_INFO;
#endif /* CONFIG_WPS */

	if (!is_zero_ether_addr(wpa_s->ml_probe_bssid)) {
		extra_ie = wpa_supplicant_ml_probe_ie(wpa_s->ml_probe_mld_id,
						      wpa_s->ml_probe_links);

		/* No other elements should be included in the probe request */
		wpa_printf(MSG_DEBUG, "MLD: Scan including only ML element");
		return extra_ie;
	}

#ifdef CONFIG_P2P
	if (wpa_s->p2p_group_interface == P2P_GROUP_INTERFACE_CLIENT)
		wpa_drv_get_ext_capa(wpa_s, WPA_IF_P2P_CLIENT);
	else
#endif /* CONFIG_P2P */
		wpa_drv_get_ext_capa(wpa_s, WPA_IF_STATION);

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab,
					     sizeof(ext_capab), NULL);
	if (ext_capab_len > 0 &&
	    wpabuf_resize(&extra_ie, ext_capab_len) == 0)
		wpabuf_put_data(extra_ie, ext_capab, ext_capab_len);

#ifdef CONFIG_INTERWORKING
	if (wpa_s->conf->interworking &&
	    wpabuf_resize(&extra_ie, 100) == 0)
		wpas_add_interworking_elements(wpa_s, extra_ie);
#endif /* CONFIG_INTERWORKING */

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		wpas_fils_req_param_add_max_channel(wpa_s, &extra_ie);
#endif /* CONFIG_MBO */

#ifdef CONFIG_WPS
	wps = wpas_wps_in_use(wpa_s, &req_type);

	if (wps) {
		struct wpabuf *wps_ie;
		wps_ie = wps_build_probe_req_ie(wps == 2 ? DEV_PW_PUSHBUTTON :
						DEV_PW_DEFAULT,
						&wpa_s->wps->dev,
						wpa_s->wps->uuid, req_type,
						0, NULL);
		if (wps_ie) {
			if (wpabuf_resize(&extra_ie, wpabuf_len(wps_ie)) == 0)
				wpabuf_put_buf(extra_ie, wps_ie);
			wpabuf_free(wps_ie);
		}
	}

#ifdef CONFIG_P2P
	if (wps) {
		size_t ielen = p2p_scan_ie_buf_len(wpa_s->global->p2p);
		if (wpabuf_resize(&extra_ie, ielen) == 0)
			wpas_p2p_scan_ie(wpa_s, extra_ie);
	}
#endif /* CONFIG_P2P */

	wpa_supplicant_mesh_add_scan_ie(wpa_s, &extra_ie);

#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20
	if (wpa_s->conf->hs20 && wpabuf_resize(&extra_ie, 9) == 0)
		wpas_hs20_add_indication(extra_ie, -1, 0);
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	if (wpa_s->fst_ies &&
	    wpabuf_resize(&extra_ie, wpabuf_len(wpa_s->fst_ies)) == 0)
		wpabuf_put_buf(extra_ie, wpa_s->fst_ies);
#endif /* CONFIG_FST */

#ifdef CONFIG_MBO
	/* Send MBO and OCE capabilities */
	if (wpabuf_resize(&extra_ie, 12) == 0)
		wpas_mbo_scan_ie(wpa_s, extra_ie);
#endif /* CONFIG_MBO */

	if (wpa_s->vendor_elem[VENDOR_ELEM_PROBE_REQ]) {
		struct wpabuf *buf = wpa_s->vendor_elem[VENDOR_ELEM_PROBE_REQ];

		if (wpabuf_resize(&extra_ie, wpabuf_len(buf)) == 0)
			wpabuf_put_buf(extra_ie, buf);
	}

	return extra_ie;
}


#ifdef CONFIG_P2P

/*
 * Check whether there are any enabled networks or credentials that could be
 * used for a non-P2P connection.
 */
static int non_p2p_network_enabled(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (wpas_network_disabled(wpa_s, ssid))
			continue;
		if (!ssid->p2p_group)
			return 1;
	}

	if (wpa_s->conf->cred && wpa_s->conf->interworking &&
	    wpa_s->conf->auto_interworking)
		return 1;

	return 0;
}

#endif /* CONFIG_P2P */


int wpa_add_scan_freqs_list(struct wpa_supplicant *wpa_s,
			    enum hostapd_hw_mode band,
			    struct wpa_driver_scan_params *params,
			    bool is_6ghz, bool only_6ghz_psc,
			    bool exclude_radar)
{
	/* Include only supported channels for the specified band */
	struct hostapd_hw_modes *mode;
	int num_chans = 0;
	int *freqs, i;

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, band, is_6ghz);
	if (!mode || !mode->num_channels)
		return -1;

	if (params->freqs) {
		while (params->freqs[num_chans])
			num_chans++;
	}

	freqs = os_realloc(params->freqs,
			   (num_chans + mode->num_channels + 1) * sizeof(int));
	if (!freqs)
		return -1;

	params->freqs = freqs;
	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (exclude_radar &&
		    (mode->channels[i].flag & HOSTAPD_CHAN_RADAR))
			continue;

		if (is_6ghz && only_6ghz_psc &&
		    !is_6ghz_psc_frequency(mode->channels[i].freq))
			continue;

		params->freqs[num_chans++] = mode->channels[i].freq;
	}
	params->freqs[num_chans] = 0;

	return 0;
}


static void wpa_setband_scan_freqs(struct wpa_supplicant *wpa_s,
				   struct wpa_driver_scan_params *params)
{
	if (wpa_s->hw.modes == NULL)
		return; /* unknown what channels the driver supports */
	if (params->freqs)
		return; /* already using a limited channel set */

	if (wpa_s->setband_mask & WPA_SETBAND_5G)
		wpa_add_scan_freqs_list(wpa_s, HOSTAPD_MODE_IEEE80211A, params,
					false, false, false);
	if (wpa_s->setband_mask & WPA_SETBAND_2G)
		wpa_add_scan_freqs_list(wpa_s, HOSTAPD_MODE_IEEE80211G, params,
					false, false, false);
	if (wpa_s->setband_mask & WPA_SETBAND_6G)
		wpa_add_scan_freqs_list(wpa_s, HOSTAPD_MODE_IEEE80211A, params,
					true, false, false);
}


static void wpa_add_scan_ssid(struct wpa_supplicant *wpa_s,
			      struct wpa_driver_scan_params *params,
			      size_t max_ssids, const u8 *ssid, size_t ssid_len)
{
	unsigned int j;

	for (j = 0; j < params->num_ssids; j++) {
		if (params->ssids[j].ssid_len == ssid_len &&
		    params->ssids[j].ssid &&
		    os_memcmp(params->ssids[j].ssid, ssid, ssid_len) == 0)
			return; /* already in the list */
	}

	if (params->num_ssids + 1 > max_ssids) {
		wpa_printf(MSG_DEBUG, "Over max scan SSIDs for manual request");
		return;
	}

	wpa_printf(MSG_DEBUG, "Scan SSID (manual request): %s",
		   wpa_ssid_txt(ssid, ssid_len));

	params->ssids[params->num_ssids].ssid = ssid;
	params->ssids[params->num_ssids].ssid_len = ssid_len;
	params->num_ssids++;
}


static void wpa_add_owe_scan_ssid(struct wpa_supplicant *wpa_s,
				  struct wpa_driver_scan_params *params,
				  struct wpa_ssid *ssid, size_t max_ssids)
{
#ifdef CONFIG_OWE
	struct wpa_bss *bss;

	if (!(ssid->key_mgmt & WPA_KEY_MGMT_OWE))
		return;

	wpa_printf(MSG_DEBUG, "OWE: Look for transition mode AP. ssid=%s",
		   wpa_ssid_txt(ssid->ssid, ssid->ssid_len));

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		const u8 *owe, *pos, *end;
		const u8 *owe_ssid;
		size_t owe_ssid_len;

		if (bss->ssid_len != ssid->ssid_len ||
		    os_memcmp(bss->ssid, ssid->ssid, ssid->ssid_len) != 0)
			continue;

		owe = wpa_bss_get_vendor_ie(bss, OWE_IE_VENDOR_TYPE);
		if (!owe || owe[1] < 4)
			continue;

		pos = owe + 6;
		end = owe + 2 + owe[1];

		/* Must include BSSID and ssid_len */
		if (end - pos < ETH_ALEN + 1)
			return;

		/* Skip BSSID */
		pos += ETH_ALEN;
		owe_ssid_len = *pos++;
		owe_ssid = pos;

		if ((size_t) (end - pos) < owe_ssid_len ||
		    owe_ssid_len > SSID_MAX_LEN)
			return;

		wpa_printf(MSG_DEBUG,
			   "OWE: scan_ssids: transition mode OWE ssid=%s",
			   wpa_ssid_txt(owe_ssid, owe_ssid_len));

		wpa_add_scan_ssid(wpa_s, params, max_ssids,
				  owe_ssid, owe_ssid_len);
		return;
	}
#endif /* CONFIG_OWE */
}


static void wpa_set_scan_ssids(struct wpa_supplicant *wpa_s,
			       struct wpa_driver_scan_params *params,
			       size_t max_ssids)
{
	unsigned int i;
	struct wpa_ssid *ssid;

	/*
	 * For devices with max_ssids greater than 1, leave the last slot empty
	 * for adding the wildcard scan entry.
	 */
	max_ssids = max_ssids > 1 ? max_ssids - 1 : max_ssids;

	for (i = 0; i < wpa_s->scan_id_count; i++) {
		ssid = wpa_config_get_network(wpa_s->conf, wpa_s->scan_id[i]);
		if (!ssid)
			continue;
		if (ssid->scan_ssid)
			wpa_add_scan_ssid(wpa_s, params, max_ssids,
					  ssid->ssid, ssid->ssid_len);
		/*
		 * Also add the SSID of the OWE BSS, to allow discovery of
		 * transition mode APs more quickly.
		 */
		wpa_add_owe_scan_ssid(wpa_s, params, ssid, max_ssids);
	}

	wpa_s->scan_id_count = 0;
}


static int wpa_set_ssids_from_scan_req(struct wpa_supplicant *wpa_s,
				       struct wpa_driver_scan_params *params,
				       size_t max_ssids)
{
	unsigned int i;

	if (wpa_s->ssids_from_scan_req == NULL ||
	    wpa_s->num_ssids_from_scan_req == 0)
		return 0;

	if (wpa_s->num_ssids_from_scan_req > max_ssids) {
		wpa_s->num_ssids_from_scan_req = max_ssids;
		wpa_printf(MSG_DEBUG, "Over max scan SSIDs from scan req: %u",
			   (unsigned int) max_ssids);
	}

	for (i = 0; i < wpa_s->num_ssids_from_scan_req; i++) {
		params->ssids[i].ssid = wpa_s->ssids_from_scan_req[i].ssid;
		params->ssids[i].ssid_len =
			wpa_s->ssids_from_scan_req[i].ssid_len;
		wpa_hexdump_ascii(MSG_DEBUG, "specific SSID",
				  params->ssids[i].ssid,
				  params->ssids[i].ssid_len);
	}

	params->num_ssids = wpa_s->num_ssids_from_scan_req;
	wpa_s->num_ssids_from_scan_req = 0;
	return 1;
}


static void wpa_supplicant_scan(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_ssid *ssid;
	int ret, p2p_in_prog;
	struct wpabuf *extra_ie = NULL;
	struct wpa_driver_scan_params params;
	struct wpa_driver_scan_params *scan_params;
	size_t max_ssids;
	int connect_without_scan = 0;

	wpa_s->ignore_post_flush_scan_res = 0;

	if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Skip scan - interface disabled");
		return;
	}

	if (wpa_s->disconnected && wpa_s->scan_req == NORMAL_SCAN_REQ) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Disconnected - do not scan");
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		return;
	}

	if (wpa_s->scanning) {
		/*
		 * If we are already in scanning state, we shall reschedule the
		 * the incoming scan request.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Already scanning - Reschedule the incoming scan req");
		wpa_supplicant_req_scan(wpa_s, 1, 0);
		return;
	}

	if (!wpa_supplicant_enabled_networks(wpa_s) &&
	    wpa_s->scan_req == NORMAL_SCAN_REQ) {
		wpa_dbg(wpa_s, MSG_DEBUG, "No enabled networks - do not scan");
		wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);
		return;
	}

	if (wpa_s->conf->ap_scan != 0 &&
	    (wpa_s->drv_flags & WPA_DRIVER_FLAGS_WIRED)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Using wired authentication - "
			"overriding ap_scan configuration");
		wpa_s->conf->ap_scan = 0;
		wpas_notify_ap_scan_changed(wpa_s);
	}

	if (wpa_s->conf->ap_scan == 0) {
		wpa_supplicant_gen_assoc_event(wpa_s);
		return;
	}

	ssid = NULL;
	if (wpa_s->scan_req != MANUAL_SCAN_REQ &&
	    wpa_s->connect_without_scan) {
		connect_without_scan = 1;
		for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
			if (ssid == wpa_s->connect_without_scan)
				break;
		}
	}

	p2p_in_prog = wpas_p2p_in_progress(wpa_s);
	if (p2p_in_prog && p2p_in_prog != 2 &&
	    (!ssid ||
	     (ssid->mode != WPAS_MODE_AP && ssid->mode != WPAS_MODE_P2P_GO))) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Delay station mode scan while P2P operation is in progress");
		wpa_supplicant_req_scan(wpa_s, 5, 0);
		return;
	}

	/*
	 * Don't cancel the scan based on ongoing PNO; defer it. Some scans are
	 * used for changing modes inside wpa_supplicant (roaming,
	 * auto-reconnect, etc). Discarding the scan might hurt these processes.
	 * The normal use case for PNO is to suspend the host immediately after
	 * starting PNO, so the periodic 100 ms attempts to run the scan do not
	 * normally happen in practice multiple times, i.e., this is simply
	 * restarting scanning once the host is woken up and PNO stopped.
	 */
	if (wpa_s->pno || wpa_s->pno_sched_pending) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Defer scan - PNO is in progress");
		wpa_supplicant_req_scan(wpa_s, 0, 100000);
		return;
	}

	if (wpa_s->conf->ap_scan == 2)
		max_ssids = 1;
	else {
		max_ssids = wpa_s->max_scan_ssids;
		if (max_ssids > WPAS_MAX_SCAN_SSIDS)
			max_ssids = WPAS_MAX_SCAN_SSIDS;
	}

	wpa_s->last_scan_req = wpa_s->scan_req;
	wpa_s->scan_req = NORMAL_SCAN_REQ;

	if (connect_without_scan) {
		wpa_s->connect_without_scan = NULL;
		if (ssid) {
			wpa_printf(MSG_DEBUG, "Start a pre-selected network "
				   "without scan step");
			wpa_supplicant_associate(wpa_s, NULL, ssid);
			return;
		}
	}

	os_memset(&params, 0, sizeof(params));

	wpa_s->scan_prev_wpa_state = wpa_s->wpa_state;
	if (wpa_s->wpa_state == WPA_DISCONNECTED ||
	    wpa_s->wpa_state == WPA_INACTIVE)
		wpa_supplicant_set_state(wpa_s, WPA_SCANNING);

	/*
	 * If autoscan has set its own scanning parameters
	 */
	if (wpa_s->autoscan_params != NULL) {
		scan_params = wpa_s->autoscan_params;
		goto scan;
	}

	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
	    wpa_set_ssids_from_scan_req(wpa_s, &params, max_ssids)) {
		wpa_printf(MSG_DEBUG, "Use specific SSIDs from SCAN command");
		goto ssid_list_set;
	}

#ifdef CONFIG_P2P
	if ((wpa_s->p2p_in_provisioning || wpa_s->show_group_started) &&
	    wpa_s->go_params && !wpa_s->conf->passive_scan) {
		wpa_printf(MSG_DEBUG, "P2P: Use specific SSID for scan during P2P group formation (p2p_in_provisioning=%d show_group_started=%d)",
			   wpa_s->p2p_in_provisioning,
			   wpa_s->show_group_started);
		params.ssids[0].ssid = wpa_s->go_params->ssid;
		params.ssids[0].ssid_len = wpa_s->go_params->ssid_len;
		params.num_ssids = 1;
		params.bssid = wpa_s->go_params->peer_interface_addr;
		wpa_printf(MSG_DEBUG, "P2P: Use specific BSSID " MACSTR
			   " (peer interface address) for scan",
			   MAC2STR(params.bssid));
		goto ssid_list_set;
	}

	if (wpa_s->p2p_in_invitation) {
		if (wpa_s->current_ssid) {
			wpa_printf(MSG_DEBUG, "P2P: Use specific SSID for scan during invitation");
			params.ssids[0].ssid = wpa_s->current_ssid->ssid;
			params.ssids[0].ssid_len =
				wpa_s->current_ssid->ssid_len;
			params.num_ssids = 1;
			if (wpa_s->current_ssid->bssid_set) {
				params.bssid = wpa_s->current_ssid->bssid;
				wpa_printf(MSG_DEBUG, "P2P: Use specific BSSID "
					   MACSTR " for scan",
					   MAC2STR(params.bssid));
			}
		} else {
			wpa_printf(MSG_DEBUG, "P2P: No specific SSID known for scan during invitation");
		}
		goto ssid_list_set;
	}
#endif /* CONFIG_P2P */

	/* Find the starting point from which to continue scanning */
	ssid = wpa_s->conf->ssid;
	if (wpa_s->prev_scan_ssid != WILDCARD_SSID_SCAN) {
		while (ssid) {
			if (ssid == wpa_s->prev_scan_ssid) {
				ssid = ssid->next;
				break;
			}
			ssid = ssid->next;
		}
	}

	if (wpa_s->last_scan_req != MANUAL_SCAN_REQ &&
#ifdef CONFIG_AP
	    !wpa_s->ap_iface &&
#endif /* CONFIG_AP */
	    wpa_s->conf->ap_scan == 2) {
		wpa_s->connect_without_scan = NULL;
		wpa_s->prev_scan_wildcard = 0;
		wpa_supplicant_assoc_try(wpa_s, ssid);
		return;
	} else if (wpa_s->conf->ap_scan == 2) {
		/*
		 * User-initiated scan request in ap_scan == 2; scan with
		 * wildcard SSID.
		 */
		ssid = NULL;
	} else if (wpa_s->reattach && wpa_s->current_ssid != NULL) {
		/*
		 * Perform single-channel single-SSID scan for
		 * reassociate-to-same-BSS operation.
		 */
		/* Setup SSID */
		ssid = wpa_s->current_ssid;
		wpa_hexdump_ascii(MSG_DEBUG, "Scan SSID",
				  ssid->ssid, ssid->ssid_len);
		params.ssids[0].ssid = ssid->ssid;
		params.ssids[0].ssid_len = ssid->ssid_len;
		params.num_ssids = 1;

		/*
		 * Allocate memory for frequency array, allocate one extra
		 * slot for the zero-terminator.
		 */
		params.freqs = os_malloc(sizeof(int) * 2);
		if (params.freqs) {
			params.freqs[0] = wpa_s->assoc_freq;
			params.freqs[1] = 0;
		}

		/*
		 * Reset the reattach flag so that we fall back to full scan if
		 * this scan fails.
		 */
		wpa_s->reattach = 0;
	} else {
		struct wpa_ssid *start = ssid, *tssid;
		int freqs_set = 0;
		if (ssid == NULL && max_ssids > 1)
			ssid = wpa_s->conf->ssid;
		while (ssid) {
			if (!wpas_network_disabled(wpa_s, ssid) &&
			    ssid->scan_ssid) {
				wpa_hexdump_ascii(MSG_DEBUG, "Scan SSID",
						  ssid->ssid, ssid->ssid_len);
				params.ssids[params.num_ssids].ssid =
					ssid->ssid;
				params.ssids[params.num_ssids].ssid_len =
					ssid->ssid_len;
				params.num_ssids++;
				if (params.num_ssids + 1 >= max_ssids)
					break;
			}

			if (!wpas_network_disabled(wpa_s, ssid)) {
				/*
				 * Also add the SSID of the OWE BSS, to allow
				 * discovery of transition mode APs more
				 * quickly.
				 */
				wpa_add_owe_scan_ssid(wpa_s, &params, ssid,
						      max_ssids);
			}

			ssid = ssid->next;
			if (ssid == start)
				break;
			if (ssid == NULL && max_ssids > 1 &&
			    start != wpa_s->conf->ssid)
				ssid = wpa_s->conf->ssid;
		}

		if (wpa_s->scan_id_count &&
		    wpa_s->last_scan_req == MANUAL_SCAN_REQ)
			wpa_set_scan_ssids(wpa_s, &params, max_ssids);

		for (tssid = wpa_s->conf->ssid;
		     wpa_s->last_scan_req != MANUAL_SCAN_REQ && tssid;
		     tssid = tssid->next) {
			if (wpas_network_disabled(wpa_s, tssid))
				continue;
			if (((params.freqs || !freqs_set) &&
			     tssid->scan_freq) &&
			    int_array_len(params.freqs) < 100) {
				int_array_concat(&params.freqs,
						 tssid->scan_freq);
			} else {
				os_free(params.freqs);
				params.freqs = NULL;
			}
			freqs_set = 1;
		}
		int_array_sort_unique(params.freqs);
	}

	if (ssid && max_ssids == 1) {
		/*
		 * If the driver is limited to 1 SSID at a time interleave
		 * wildcard SSID scans with specific SSID scans to avoid
		 * waiting a long time for a wildcard scan.
		 */
		if (!wpa_s->prev_scan_wildcard) {
			params.ssids[0].ssid = NULL;
			params.ssids[0].ssid_len = 0;
			wpa_s->prev_scan_wildcard = 1;
			wpa_dbg(wpa_s, MSG_DEBUG, "Starting AP scan for "
				"wildcard SSID (Interleave with specific)");
		} else {
			wpa_s->prev_scan_ssid = ssid;
			wpa_s->prev_scan_wildcard = 0;
			wpa_dbg(wpa_s, MSG_DEBUG,
				"Starting AP scan for specific SSID: %s",
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
		}
	} else if (ssid) {
		/* max_ssids > 1 */

		wpa_s->prev_scan_ssid = ssid;
		wpa_dbg(wpa_s, MSG_DEBUG, "Include wildcard SSID in "
			"the scan request");
		params.num_ssids++;
	} else if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
		   wpa_s->manual_scan_passive && params.num_ssids == 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Use passive scan based on manual request");
	} else if (wpa_s->conf->passive_scan) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Use passive scan based on configuration");
	} else {
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
		params.num_ssids++;
		wpa_dbg(wpa_s, MSG_DEBUG, "Starting AP scan for wildcard "
			"SSID");
	}

ssid_list_set:
	wpa_supplicant_optimize_freqs(wpa_s, &params);
	extra_ie = wpa_supplicant_extra_ies(wpa_s);

	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
	    wpa_s->manual_scan_only_new) {
		wpa_printf(MSG_DEBUG,
			   "Request driver to clear scan cache due to manual only_new=1 scan");
		params.only_new_results = 1;
	}

	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ && params.freqs == NULL &&
	    wpa_s->manual_scan_freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Limit manual scan to specified channels");
		params.freqs = wpa_s->manual_scan_freqs;
		wpa_s->manual_scan_freqs = NULL;
	}

	if (params.freqs == NULL && wpa_s->select_network_scan_freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Limit select_network scan to specified channels");
		params.freqs = wpa_s->select_network_scan_freqs;
		wpa_s->select_network_scan_freqs = NULL;
	}

	if (params.freqs == NULL && wpa_s->next_scan_freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Optimize scan based on previously "
			"generated frequency list");
		params.freqs = wpa_s->next_scan_freqs;
	} else
		os_free(wpa_s->next_scan_freqs);
	wpa_s->next_scan_freqs = NULL;
	wpa_setband_scan_freqs(wpa_s, &params);

	/* See if user specified frequencies. If so, scan only those. */
	if (wpa_s->last_scan_req == INITIAL_SCAN_REQ &&
	    wpa_s->conf->initial_freq_list && !params.freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Optimize scan based on conf->initial_freq_list");
		int_array_concat(&params.freqs, wpa_s->conf->initial_freq_list);
	} else if (wpa_s->conf->freq_list && !params.freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Optimize scan based on conf->freq_list");
		int_array_concat(&params.freqs, wpa_s->conf->freq_list);
	}

	/* Use current associated channel? */
	if (wpa_s->conf->scan_cur_freq && !params.freqs) {
		unsigned int num = wpa_s->num_multichan_concurrent;

		params.freqs = os_calloc(num + 1, sizeof(int));
		if (params.freqs) {
			num = get_shared_radio_freqs(wpa_s, params.freqs, num,
						     false);
			if (num > 0) {
				wpa_dbg(wpa_s, MSG_DEBUG, "Scan only the "
					"current operating channels since "
					"scan_cur_freq is enabled");
			} else {
				os_free(params.freqs);
				params.freqs = NULL;
			}
		}
	}

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		params.oce_scan = 1;
#endif /* CONFIG_MBO */

	params.filter_ssids = wpa_supplicant_build_filter_ssids(
		wpa_s->conf, &params.num_filter_ssids);
	if (extra_ie) {
		params.extra_ies = wpabuf_head(extra_ie);
		params.extra_ies_len = wpabuf_len(extra_ie);
	}

#ifdef CONFIG_P2P
	if (wpa_s->p2p_in_provisioning || wpa_s->p2p_in_invitation ||
	    (wpa_s->show_group_started && wpa_s->go_params)) {
		/*
		 * The interface may not yet be in P2P mode, so we have to
		 * explicitly request P2P probe to disable CCK rates.
		 */
		params.p2p_probe = 1;
	}
#endif /* CONFIG_P2P */

	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_SCAN) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(&params, wpa_s->mac_addr_scan);

	if (!is_zero_ether_addr(wpa_s->next_scan_bssid)) {
		struct wpa_bss *bss;

		params.bssid = wpa_s->next_scan_bssid;
		bss = wpa_bss_get_bssid_latest(wpa_s, params.bssid);
		if (!wpa_s->next_scan_bssid_wildcard_ssid &&
		    bss && bss->ssid_len && params.num_ssids == 1 &&
		    params.ssids[0].ssid_len == 0) {
			params.ssids[0].ssid = bss->ssid;
			params.ssids[0].ssid_len = bss->ssid_len;
			wpa_dbg(wpa_s, MSG_DEBUG,
				"Scan a previously specified BSSID " MACSTR
				" and SSID %s",
				MAC2STR(params.bssid),
				wpa_ssid_txt(bss->ssid, bss->ssid_len));
		} else {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"Scan a previously specified BSSID " MACSTR,
				MAC2STR(params.bssid));
		}
	} else if (!is_zero_ether_addr(wpa_s->ml_probe_bssid)) {
		wpa_printf(MSG_DEBUG, "Scanning for ML probe request");
		params.bssid = wpa_s->ml_probe_bssid;
		params.min_probe_req_content = true;
	}


	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
	    wpa_s->manual_non_coloc_6ghz) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Collocated 6 GHz logic is disabled");
		params.non_coloc_6ghz = 1;
	}

	scan_params = &params;

scan:
#ifdef CONFIG_P2P
	/*
	 * If the driver does not support multi-channel concurrency and a
	 * virtual interface that shares the same radio with the wpa_s interface
	 * is operating there may not be need to scan other channels apart from
	 * the current operating channel on the other virtual interface. Filter
	 * out other channels in case we are trying to find a connection for a
	 * station interface when we are not configured to prefer station
	 * connection and a concurrent operation is already in process.
	 */
	if (wpa_s->scan_for_connection &&
	    wpa_s->last_scan_req == NORMAL_SCAN_REQ &&
	    !scan_params->freqs && !params.freqs &&
	    wpas_is_p2p_prioritized(wpa_s) &&
	    wpa_s->p2p_group_interface == NOT_P2P_GROUP_INTERFACE &&
	    non_p2p_network_enabled(wpa_s)) {
		unsigned int num = wpa_s->num_multichan_concurrent;

		params.freqs = os_calloc(num + 1, sizeof(int));
		if (params.freqs) {
			/*
			 * Exclude the operating frequency of the current
			 * interface since we're looking to transition off of
			 * it.
			 */
			num = get_shared_radio_freqs(wpa_s, params.freqs, num,
						     true);
			if (num > 0 && num == wpa_s->num_multichan_concurrent) {
				wpa_dbg(wpa_s, MSG_DEBUG, "Scan only the current operating channels since all channels are already used");
			} else {
				os_free(params.freqs);
				params.freqs = NULL;
			}
		}
	}

	if (!params.freqs && wpas_is_6ghz_supported(wpa_s, true) &&
	    (wpa_s->p2p_in_invitation || wpa_s->p2p_in_provisioning))
		wpas_p2p_scan_freqs(wpa_s, &params, true);
#endif /* CONFIG_P2P */

	ret = wpa_supplicant_trigger_scan(wpa_s, scan_params, false, false);

	if (ret && wpa_s->last_scan_req == MANUAL_SCAN_REQ && params.freqs &&
	    !wpa_s->manual_scan_freqs) {
		/* Restore manual_scan_freqs for the next attempt */
		wpa_s->manual_scan_freqs = params.freqs;
		params.freqs = NULL;
	}

	wpabuf_free(extra_ie);
	os_free(params.freqs);
	os_free(params.filter_ssids);
	os_free(params.mac_addr);

	if (ret) {
		wpa_msg(wpa_s, MSG_WARNING, "Failed to initiate AP scan");
		if (wpa_s->scan_prev_wpa_state != wpa_s->wpa_state)
			wpa_supplicant_set_state(wpa_s,
						 wpa_s->scan_prev_wpa_state);
		/* Restore scan_req since we will try to scan again */
		wpa_s->scan_req = wpa_s->last_scan_req;
		wpa_supplicant_req_scan(wpa_s, 1, 0);
	} else {
		wpa_s->scan_for_connection = 0;
#ifdef CONFIG_INTERWORKING
		wpa_s->interworking_fast_assoc_tried = 0;
#endif /* CONFIG_INTERWORKING */
		wpa_s->next_scan_bssid_wildcard_ssid = 0;
		if (params.bssid)
			os_memset(wpa_s->next_scan_bssid, 0, ETH_ALEN);
	}

	wpa_s->ml_probe_mld_id = -1;
	wpa_s->ml_probe_links = 0;
	os_memset(wpa_s->ml_probe_bssid, 0, sizeof(wpa_s->ml_probe_bssid));
}


void wpa_supplicant_update_scan_int(struct wpa_supplicant *wpa_s, int sec)
{
	struct os_reltime remaining, new_int;
	int cancelled;

	cancelled = eloop_cancel_timeout_one(wpa_supplicant_scan, wpa_s, NULL,
					     &remaining);

	new_int.sec = sec;
	new_int.usec = 0;
	if (cancelled && os_reltime_before(&remaining, &new_int)) {
		new_int.sec = remaining.sec;
		new_int.usec = remaining.usec;
	}

	if (cancelled) {
		eloop_register_timeout(new_int.sec, new_int.usec,
				       wpa_supplicant_scan, wpa_s, NULL);
	}
	wpa_s->scan_interval = sec;
}


/**
 * wpa_supplicant_req_scan - Schedule a scan for neighboring access points
 * @wpa_s: Pointer to wpa_supplicant data
 * @sec: Number of seconds after which to scan
 * @usec: Number of microseconds after which to scan
 *
 * This function is used to schedule a scan for neighboring access points after
 * the specified time.
 */
void wpa_supplicant_req_scan(struct wpa_supplicant *wpa_s, int sec, int usec)
{
	int res;

	if (wpa_s->p2p_mgmt) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Ignore scan request (%d.%06d sec) on p2p_mgmt interface",
			sec, usec);
		return;
	}

	res = eloop_deplete_timeout(sec, usec, wpa_supplicant_scan, wpa_s,
				    NULL);
	if (res == 1) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Rescheduling scan request: %d.%06d sec",
			sec, usec);
	} else if (res == 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Ignore new scan request for %d.%06d sec since an earlier request is scheduled to trigger sooner",
			sec, usec);
	} else {
		wpa_dbg(wpa_s, MSG_DEBUG, "Setting scan request: %d.%06d sec",
			sec, usec);
		eloop_register_timeout(sec, usec, wpa_supplicant_scan, wpa_s, NULL);
	}
}


/**
 * wpa_supplicant_delayed_sched_scan - Request a delayed scheduled scan
 * @wpa_s: Pointer to wpa_supplicant data
 * @sec: Number of seconds after which to scan
 * @usec: Number of microseconds after which to scan
 * Returns: 0 on success or -1 otherwise
 *
 * This function is used to schedule periodic scans for neighboring
 * access points after the specified time.
 */
int wpa_supplicant_delayed_sched_scan(struct wpa_supplicant *wpa_s,
				      int sec, int usec)
{
	if (!wpa_s->sched_scan_supported)
		return -1;

	eloop_register_timeout(sec, usec,
			       wpa_supplicant_delayed_sched_scan_timeout,
			       wpa_s, NULL);

	return 0;
}


static void
wpa_scan_set_relative_rssi_params(struct wpa_supplicant *wpa_s,
				  struct wpa_driver_scan_params *params)
{
	if (wpa_s->wpa_state != WPA_COMPLETED ||
	    !(wpa_s->drv_flags & WPA_DRIVER_FLAGS_SCHED_SCAN_RELATIVE_RSSI) ||
	    wpa_s->srp.relative_rssi_set == 0)
		return;

	params->relative_rssi_set = 1;
	params->relative_rssi = wpa_s->srp.relative_rssi;

	if (wpa_s->srp.relative_adjust_rssi == 0)
		return;

	params->relative_adjust_band = wpa_s->srp.relative_adjust_band;
	params->relative_adjust_rssi = wpa_s->srp.relative_adjust_rssi;
}


/**
 * wpa_supplicant_req_sched_scan - Start a periodic scheduled scan
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 is sched_scan was started or -1 otherwise
 *
 * This function is used to schedule periodic scans for neighboring
 * access points repeating the scan continuously.
 */
int wpa_supplicant_req_sched_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_driver_scan_params params;
	struct wpa_driver_scan_params *scan_params;
	enum wpa_states prev_state;
	struct wpa_ssid *ssid = NULL;
	struct wpabuf *extra_ie = NULL;
	int ret;
	unsigned int max_sched_scan_ssids;
	int wildcard = 0;
	int need_ssids;
	struct sched_scan_plan scan_plan;

	if (!wpa_s->sched_scan_supported)
		return -1;

	if (wpa_s->max_sched_scan_ssids > WPAS_MAX_SCAN_SSIDS)
		max_sched_scan_ssids = WPAS_MAX_SCAN_SSIDS;
	else
		max_sched_scan_ssids = wpa_s->max_sched_scan_ssids;
	if (max_sched_scan_ssids < 1 || wpa_s->conf->disable_scan_offload)
		return -1;

	wpa_s->sched_scan_stop_req = 0;

	if (wpa_s->sched_scanning) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Already sched scanning");
		return 0;
	}

	need_ssids = 0;
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!wpas_network_disabled(wpa_s, ssid) && !ssid->scan_ssid) {
			/* Use wildcard SSID to find this network */
			wildcard = 1;
		} else if (!wpas_network_disabled(wpa_s, ssid) &&
			   ssid->ssid_len)
			need_ssids++;

#ifdef CONFIG_WPS
		if (!wpas_network_disabled(wpa_s, ssid) &&
		    ssid->key_mgmt == WPA_KEY_MGMT_WPS) {
			/*
			 * Normal scan is more reliable and faster for WPS
			 * operations and since these are for short periods of
			 * time, the benefit of trying to use sched_scan would
			 * be limited.
			 */
			wpa_dbg(wpa_s, MSG_DEBUG, "Use normal scan instead of "
				"sched_scan for WPS");
			return -1;
		}
#endif /* CONFIG_WPS */
	}
	if (wildcard)
		need_ssids++;

	if (wpa_s->normal_scans < 3 &&
	    (need_ssids <= wpa_s->max_scan_ssids ||
	     wpa_s->max_scan_ssids >= (int) max_sched_scan_ssids)) {
		/*
		 * When normal scan can speed up operations, use that for the
		 * first operations before starting the sched_scan to allow
		 * user space sleep more. We do this only if the normal scan
		 * has functionality that is suitable for this or if the
		 * sched_scan does not have better support for multiple SSIDs.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Use normal scan instead of "
			"sched_scan for initial scans (normal_scans=%d)",
			wpa_s->normal_scans);
		return -1;
	}

	os_memset(&params, 0, sizeof(params));

	/* If we can't allocate space for the filters, we just don't filter */
	params.filter_ssids = os_calloc(wpa_s->max_match_sets,
					sizeof(struct wpa_driver_scan_filter));

	prev_state = wpa_s->wpa_state;
	if (wpa_s->wpa_state == WPA_DISCONNECTED ||
	    wpa_s->wpa_state == WPA_INACTIVE)
		wpa_supplicant_set_state(wpa_s, WPA_SCANNING);

	if (wpa_s->autoscan_params != NULL) {
		scan_params = wpa_s->autoscan_params;
		goto scan;
	}

	/* Find the starting point from which to continue scanning */
	ssid = wpa_s->conf->ssid;
	if (wpa_s->prev_sched_ssid) {
		while (ssid) {
			if (ssid == wpa_s->prev_sched_ssid) {
				ssid = ssid->next;
				break;
			}
			ssid = ssid->next;
		}
	}

	if (!ssid || !wpa_s->prev_sched_ssid) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Beginning of SSID list");
		wpa_s->sched_scan_timeout = max_sched_scan_ssids * 2;
		wpa_s->first_sched_scan = 1;
		ssid = wpa_s->conf->ssid;
		wpa_s->prev_sched_ssid = ssid;
	}

	if (wildcard) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Add wildcard SSID to sched_scan");
		params.num_ssids++;
	}

	while (ssid) {
		if (wpas_network_disabled(wpa_s, ssid))
			goto next;

		if (params.num_filter_ssids < wpa_s->max_match_sets &&
		    params.filter_ssids && ssid->ssid && ssid->ssid_len) {
			wpa_dbg(wpa_s, MSG_DEBUG, "add to filter ssid: %s",
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
			os_memcpy(params.filter_ssids[params.num_filter_ssids].ssid,
				  ssid->ssid, ssid->ssid_len);
			params.filter_ssids[params.num_filter_ssids].ssid_len =
				ssid->ssid_len;
			params.num_filter_ssids++;
		} else if (params.filter_ssids && ssid->ssid && ssid->ssid_len)
		{
			wpa_dbg(wpa_s, MSG_DEBUG, "Not enough room for SSID "
				"filter for sched_scan - drop filter");
			os_free(params.filter_ssids);
			params.filter_ssids = NULL;
			params.num_filter_ssids = 0;
		}

		if (ssid->scan_ssid && ssid->ssid && ssid->ssid_len) {
			if (params.num_ssids == max_sched_scan_ssids)
				break; /* only room for broadcast SSID */
			wpa_dbg(wpa_s, MSG_DEBUG,
				"add to active scan ssid: %s",
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
			params.ssids[params.num_ssids].ssid =
				ssid->ssid;
			params.ssids[params.num_ssids].ssid_len =
				ssid->ssid_len;
			params.num_ssids++;
			if (params.num_ssids >= max_sched_scan_ssids) {
				wpa_s->prev_sched_ssid = ssid;
				do {
					ssid = ssid->next;
				} while (ssid &&
					 (wpas_network_disabled(wpa_s, ssid) ||
					  !ssid->scan_ssid));
				break;
			}
		}

	next:
		wpa_s->prev_sched_ssid = ssid;
		ssid = ssid->next;
	}

	if (params.num_filter_ssids == 0) {
		os_free(params.filter_ssids);
		params.filter_ssids = NULL;
	}

	extra_ie = wpa_supplicant_extra_ies(wpa_s);
	if (extra_ie) {
		params.extra_ies = wpabuf_head(extra_ie);
		params.extra_ies_len = wpabuf_len(extra_ie);
	}

	if (wpa_s->conf->filter_rssi)
		params.filter_rssi = wpa_s->conf->filter_rssi;

	/* See if user specified frequencies. If so, scan only those. */
	if (wpa_s->conf->freq_list && !params.freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Optimize scan based on conf->freq_list");
		int_array_concat(&params.freqs, wpa_s->conf->freq_list);
	}

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		params.oce_scan = 1;
#endif /* CONFIG_MBO */

	scan_params = &params;

scan:
	wpa_s->sched_scan_timed_out = 0;

	/*
	 * We cannot support multiple scan plans if the scan request includes
	 * too many SSID's, so in this case use only the last scan plan and make
	 * it run infinitely. It will be stopped by the timeout.
	 */
	if (wpa_s->sched_scan_plans_num == 1 ||
	    (wpa_s->sched_scan_plans_num && !ssid && wpa_s->first_sched_scan)) {
		params.sched_scan_plans = wpa_s->sched_scan_plans;
		params.sched_scan_plans_num = wpa_s->sched_scan_plans_num;
	} else if (wpa_s->sched_scan_plans_num > 1) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Too many SSIDs. Default to using single scheduled_scan plan");
		params.sched_scan_plans =
			&wpa_s->sched_scan_plans[wpa_s->sched_scan_plans_num -
						 1];
		params.sched_scan_plans_num = 1;
	} else {
		if (wpa_s->conf->sched_scan_interval)
			scan_plan.interval = wpa_s->conf->sched_scan_interval;
		else
			scan_plan.interval = 10;

		if (scan_plan.interval > wpa_s->max_sched_scan_plan_interval) {
			wpa_printf(MSG_WARNING,
				   "Scan interval too long(%u), use the maximum allowed(%u)",
				   scan_plan.interval,
				   wpa_s->max_sched_scan_plan_interval);
			scan_plan.interval =
				wpa_s->max_sched_scan_plan_interval;
		}

		scan_plan.iterations = 0;
		params.sched_scan_plans = &scan_plan;
		params.sched_scan_plans_num = 1;
	}

	params.sched_scan_start_delay = wpa_s->conf->sched_scan_start_delay;

	if (ssid || !wpa_s->first_sched_scan) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Starting sched scan after %u seconds: interval %u timeout %d",
			params.sched_scan_start_delay,
			params.sched_scan_plans[0].interval,
			wpa_s->sched_scan_timeout);
	} else {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Starting sched scan after %u seconds (no timeout)",
			params.sched_scan_start_delay);
	}

	wpa_setband_scan_freqs(wpa_s, scan_params);

	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_SCHED_SCAN) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(&params,
					       wpa_s->mac_addr_sched_scan);

	wpa_scan_set_relative_rssi_params(wpa_s, scan_params);

	ret = wpa_supplicant_start_sched_scan(wpa_s, scan_params);
	wpabuf_free(extra_ie);
	os_free(params.filter_ssids);
	os_free(params.mac_addr);
	if (ret) {
		wpa_msg(wpa_s, MSG_WARNING, "Failed to initiate sched scan");
		if (prev_state != wpa_s->wpa_state)
			wpa_supplicant_set_state(wpa_s, prev_state);
		return ret;
	}

	/* If we have more SSIDs to scan, add a timeout so we scan them too */
	if (ssid || !wpa_s->first_sched_scan) {
		wpa_s->sched_scan_timed_out = 0;
		eloop_register_timeout(wpa_s->sched_scan_timeout, 0,
				       wpa_supplicant_sched_scan_timeout,
				       wpa_s, NULL);
		wpa_s->first_sched_scan = 0;
		wpa_s->sched_scan_timeout /= 2;
		params.sched_scan_plans[0].interval *= 2;
		if ((unsigned int) wpa_s->sched_scan_timeout <
		    params.sched_scan_plans[0].interval ||
		    params.sched_scan_plans[0].interval >
		    wpa_s->max_sched_scan_plan_interval) {
			params.sched_scan_plans[0].interval = 10;
			wpa_s->sched_scan_timeout = max_sched_scan_ssids * 2;
		}
	}

	/* If there is no more ssids, start next time from the beginning */
	if (!ssid)
		wpa_s->prev_sched_ssid = NULL;

	return 0;
}


/**
 * wpa_supplicant_cancel_scan - Cancel a scheduled scan request
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to cancel a scan request scheduled with
 * wpa_supplicant_req_scan().
 */
void wpa_supplicant_cancel_scan(struct wpa_supplicant *wpa_s)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling scan request");
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
}


/**
 * wpa_supplicant_cancel_delayed_sched_scan - Stop a delayed scheduled scan
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to stop a delayed scheduled scan.
 */
void wpa_supplicant_cancel_delayed_sched_scan(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->sched_scan_supported)
		return;

	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling delayed sched scan");
	eloop_cancel_timeout(wpa_supplicant_delayed_sched_scan_timeout,
			     wpa_s, NULL);
}


/**
 * wpa_supplicant_cancel_sched_scan - Stop running scheduled scans
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to stop a periodic scheduled scan.
 */
void wpa_supplicant_cancel_sched_scan(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->sched_scanning)
		return;

	if (wpa_s->sched_scanning)
		wpa_s->sched_scan_stop_req = 1;

	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling sched scan");
	eloop_cancel_timeout(wpa_supplicant_sched_scan_timeout, wpa_s, NULL);
	wpa_supplicant_stop_sched_scan(wpa_s);
}


/**
 * wpa_supplicant_notify_scanning - Indicate possible scan state change
 * @wpa_s: Pointer to wpa_supplicant data
 * @scanning: Whether scanning is currently in progress
 *
 * This function is to generate scanning notifycations. It is called whenever
 * there may have been a change in scanning (scan started, completed, stopped).
 * wpas_notify_scanning() is called whenever the scanning state changed from the
 * previously notified state.
 */
void wpa_supplicant_notify_scanning(struct wpa_supplicant *wpa_s,
				    int scanning)
{
	if (wpa_s->scanning != scanning) {
		wpa_s->scanning = scanning;
		wpas_notify_scanning(wpa_s);
	}
}


static int wpa_scan_get_max_rate(const struct wpa_scan_res *res)
{
	int rate = 0;
	const u8 *ie;
	int i;

	ie = wpa_scan_get_ie(res, WLAN_EID_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	ie = wpa_scan_get_ie(res, WLAN_EID_EXT_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	return rate;
}


/**
 * wpa_scan_get_ie - Fetch a specified information element from a scan result
 * @res: Scan result entry
 * @ie: Information element identitifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the scan
 * result.
 */
const u8 * wpa_scan_get_ie(const struct wpa_scan_res *res, u8 ie)
{
	size_t ie_len = res->ie_len;

	/* Use the Beacon frame IEs if res->ie_len is not available */
	if (!ie_len)
		ie_len = res->beacon_ie_len;

	return get_ie((const u8 *) (res + 1), ie_len, ie);
}


const u8 * wpa_scan_get_ml_ie(const struct wpa_scan_res *res, u8 type)
{
	size_t ie_len = res->ie_len;

	/* Use the Beacon frame IEs if res->ie_len is not available */
	if (!ie_len)
		ie_len = res->beacon_ie_len;

	return get_ml_ie((const u8 *) (res + 1), ie_len, type);
}


/**
 * wpa_scan_get_vendor_ie - Fetch vendor information element from a scan result
 * @res: Scan result entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the scan
 * result.
 */
const u8 * wpa_scan_get_vendor_ie(const struct wpa_scan_res *res,
				  u32 vendor_type)
{
	const u8 *ies;
	const struct element *elem;

	ies = (const u8 *) (res + 1);

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies, res->ie_len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


/**
 * wpa_scan_get_vendor_ie_beacon - Fetch vendor information from a scan result
 * @res: Scan result entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the scan
 * result.
 *
 * This function is like wpa_scan_get_vendor_ie(), but uses IE buffer only
 * from Beacon frames instead of either Beacon or Probe Response frames.
 */
const u8 * wpa_scan_get_vendor_ie_beacon(const struct wpa_scan_res *res,
					 u32 vendor_type)
{
	const u8 *ies;
	const struct element *elem;

	if (res->beacon_ie_len == 0)
		return NULL;

	ies = (const u8 *) (res + 1);
	ies += res->ie_len;

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies,
			    res->beacon_ie_len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


/**
 * wpa_scan_get_vendor_ie_multi - Fetch vendor IE data from a scan result
 * @res: Scan result entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element payload or %NULL if not found
 *
 * This function returns concatenated payload of possibly fragmented vendor
 * specific information elements in the scan result. The caller is responsible
 * for freeing the returned buffer.
 */
struct wpabuf * wpa_scan_get_vendor_ie_multi(const struct wpa_scan_res *res,
					     u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(res->ie_len);
	if (buf == NULL)
		return NULL;

	pos = (const u8 *) (res + 1);
	end = pos + res->ie_len;

	while (end - pos > 1) {
		u8 ie, len;

		ie = pos[0];
		len = pos[1];
		if (len > end - pos - 2)
			break;
		pos += 2;
		if (ie == WLAN_EID_VENDOR_SPECIFIC && len >= 4 &&
		    vendor_type == WPA_GET_BE32(pos))
			wpabuf_put_data(buf, pos + 4, len - 4);
		pos += len;
	}

	if (wpabuf_len(buf) == 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}


static int wpas_channel_width_offset(enum chan_width cw)
{
	switch (cw) {
	case CHAN_WIDTH_40:
		return 1;
	case CHAN_WIDTH_80:
		return 2;
	case CHAN_WIDTH_80P80:
	case CHAN_WIDTH_160:
		return 3;
	case CHAN_WIDTH_320:
		return 4;
	default:
		return 0;
	}
}


/**
 * wpas_channel_width_tx_pwr - Calculate the max transmit power at the channel
 * width
 * @ies: Information elements
 * @ies_len: Length of elements
 * @cw: The channel width
 * Returns: The max transmit power at the channel width, TX_POWER_NO_CONSTRAINT
 * if it is not constrained.
 *
 * This function is only used to estimate the actual signal RSSI when associated
 * based on the beacon RSSI at the STA. Beacon frames are transmitted on 20 MHz
 * channels, while the Data frames usually use higher channel width. Therefore
 * their RSSIs may be different. Assuming there is a fixed gap between the TX
 * power limit of the STA defined by the Transmit Power Envelope element and the
 * TX power of the AP, the difference in the TX power of X MHz and Y MHz at the
 * STA equals to the difference at the AP, and the difference in the signal RSSI
 * at the STA. tx_pwr is a floating point number in the standard, but the error
 * of casting to int is trivial in comparing two BSSes.
 */
static int wpas_channel_width_tx_pwr(const u8 *ies, size_t ies_len,
				     enum chan_width cw)
{
	int offset = wpas_channel_width_offset(cw);
	const struct element *elem;
	int max_tx_power = TX_POWER_NO_CONSTRAINT, tx_pwr = 0;

	for_each_element_id(elem, WLAN_EID_TRANSMIT_POWER_ENVELOPE, ies,
			    ies_len) {
		int max_tx_pwr_count;
		enum max_tx_pwr_interpretation tx_pwr_intrpn;
		enum reg_6g_client_type client_type;

		if (elem->datalen < 1)
			continue;

		/*
		 * IEEE Std 802.11ax-2021, 9.4.2.161 (Transmit Power Envelope
		 * element) defines Maximum Transmit Power Count (B0-B2),
		 * Maximum Transmit Power Interpretation (B3-B5), and Maximum
		 * Transmit Power Category (B6-B7).
		 */
		max_tx_pwr_count = elem->data[0] & 0x07;
		tx_pwr_intrpn = (elem->data[0] >> 3) & 0x07;
		client_type = (elem->data[0] >> 6) & 0x03;

		if (client_type != REG_DEFAULT_CLIENT)
			continue;

		if (tx_pwr_intrpn == LOCAL_EIRP ||
		    tx_pwr_intrpn == REGULATORY_CLIENT_EIRP) {
			int offs;

			max_tx_pwr_count = MIN(max_tx_pwr_count, 3);
			offs = MIN(offset, max_tx_pwr_count) + 1;
			if (elem->datalen <= offs)
				continue;
			tx_pwr = (signed char) elem->data[offs];
			/*
			 * Maximum Transmit Power subfield is encoded as an
			 * 8-bit 2s complement signed integer in the range -64
			 * dBm to 63 dBm with a 0.5 dB step. 63.5 dBm means no
			 * local maximum transmit power constraint.
			 */
			if (tx_pwr == 127)
				continue;
			tx_pwr /= 2;
			max_tx_power = MIN(max_tx_power, tx_pwr);
		} else if (tx_pwr_intrpn == LOCAL_EIRP_PSD ||
			   tx_pwr_intrpn == REGULATORY_CLIENT_EIRP_PSD) {
			if (elem->datalen < 2)
				continue;

			tx_pwr = (signed char) elem->data[1];
			/*
			 * Maximum Transmit PSD subfield is encoded as an 8-bit
			 * 2s complement signed integer. -128 indicates that the
			 * corresponding 20 MHz channel cannot be used for
			 * transmission. +127 indicates that no maximum PSD
			 * limit is specified for the corresponding 20 MHz
			 * channel.
			 */
			if (tx_pwr == 127 || tx_pwr == -128)
				continue;

			/*
			 * The Maximum Transmit PSD subfield indicates the
			 * maximum transmit PSD for the 20 MHz channel. Suppose
			 * the PSD value is X dBm/MHz, the TX power of N MHz is
			 * X + 10*log10(N) = X + 10*log10(20) + 10*log10(N/20) =
			 * X + 13 + 3*log2(N/20)
			 */
			tx_pwr = tx_pwr / 2 + 13 + offset * 3;
			max_tx_power = MIN(max_tx_power, tx_pwr);
		}
	}

	return max_tx_power;
}


/**
 * Estimate the RSSI bump of channel width |cw| with respect to 20 MHz channel.
 * If the TX power has no constraint, it is unable to estimate the RSSI bump.
 */
int wpas_channel_width_rssi_bump(const u8 *ies, size_t ies_len,
				 enum chan_width cw)
{
	int max_20mhz_tx_pwr = wpas_channel_width_tx_pwr(ies, ies_len,
							 CHAN_WIDTH_20);
	int max_cw_tx_pwr = wpas_channel_width_tx_pwr(ies, ies_len, cw);

	return (max_20mhz_tx_pwr == TX_POWER_NO_CONSTRAINT ||
		max_cw_tx_pwr == TX_POWER_NO_CONSTRAINT) ?
		0 : (max_cw_tx_pwr - max_20mhz_tx_pwr);
}


int wpas_adjust_snr_by_chanwidth(const u8 *ies, size_t ies_len,
				 enum chan_width max_cw, int snr)
{
	int rssi_bump = wpas_channel_width_rssi_bump(ies, ies_len, max_cw);
	/*
	 * The noise has uniform power spectral density (PSD) across the
	 * frequency band, its power is proportional to the channel width.
	 * Suppose the PSD of noise is X dBm/MHz, the noise power of N MHz is
	 * X + 10*log10(N), and the noise power bump with respect to 20 MHz is
	 * 10*log10(N) - 10*log10(20) = 10*log10(N/20) = 3*log2(N/20)
	 */
	int noise_bump = 3 * wpas_channel_width_offset(max_cw);

	return snr + rssi_bump - noise_bump;
}


/* Compare function for sorting scan results. Return >0 if @b is considered
 * better. */
static int wpa_scan_result_compar(const void *a, const void *b)
{
	struct wpa_scan_res **_wa = (void *) a;
	struct wpa_scan_res **_wb = (void *) b;
	struct wpa_scan_res *wa = *_wa;
	struct wpa_scan_res *wb = *_wb;
	int wpa_a, wpa_b;
	int snr_a, snr_b, snr_a_full, snr_b_full;
	size_t ies_len;
	const u8 *rsne_a, *rsne_b;

	/* WPA/WPA2 support preferred */
	wpa_a = wpa_scan_get_vendor_ie(wa, WPA_IE_VENDOR_TYPE) != NULL ||
		wpa_scan_get_ie(wa, WLAN_EID_RSN) != NULL;
	wpa_b = wpa_scan_get_vendor_ie(wb, WPA_IE_VENDOR_TYPE) != NULL ||
		wpa_scan_get_ie(wb, WLAN_EID_RSN) != NULL;

	if (wpa_b && !wpa_a)
		return 1;
	if (!wpa_b && wpa_a)
		return -1;

	/* privacy support preferred */
	if ((wa->caps & IEEE80211_CAP_PRIVACY) == 0 &&
	    (wb->caps & IEEE80211_CAP_PRIVACY))
		return 1;
	if ((wa->caps & IEEE80211_CAP_PRIVACY) &&
	    (wb->caps & IEEE80211_CAP_PRIVACY) == 0)
		return -1;

	if (wa->flags & wb->flags & WPA_SCAN_LEVEL_DBM) {
		/*
		 * The scan result estimates SNR over 20 MHz, while Data frames
		 * usually use wider channel width. The TX power and noise power
		 * are both affected by the channel width.
		 */
		ies_len = wa->ie_len ? wa->ie_len : wa->beacon_ie_len;
		snr_a_full = wpas_adjust_snr_by_chanwidth((const u8 *) (wa + 1),
							  ies_len, wa->max_cw,
							  wa->snr);
		snr_a = MIN(snr_a_full, GREAT_SNR);
		ies_len = wb->ie_len ? wb->ie_len : wb->beacon_ie_len;
		snr_b_full = wpas_adjust_snr_by_chanwidth((const u8 *) (wb + 1),
							  ies_len, wb->max_cw,
							  wb->snr);
		snr_b = MIN(snr_b_full, GREAT_SNR);
	} else {
		/* Level is not in dBm, so we can't calculate
		 * SNR. Just use raw level (units unknown). */
		snr_a = snr_a_full = wa->level;
		snr_b = snr_b_full = wb->level;
	}

	/* If SNR of a SAE BSS is good or at least as high as the PSK BSS,
	 * prefer SAE over PSK for mixed WPA3-Personal transition mode and
	 * WPA2-Personal deployments */
	rsne_a = wpa_scan_get_ie(wa, WLAN_EID_RSN);
	rsne_b = wpa_scan_get_ie(wb, WLAN_EID_RSN);
	if (rsne_a && rsne_b) {
		struct wpa_ie_data data;
		bool psk_a = false, psk_b = false, sae_a = false, sae_b = false;

		if (wpa_parse_wpa_ie_rsn(rsne_a, 2 + rsne_a[1], &data) == 0) {
			psk_a = wpa_key_mgmt_wpa_psk_no_sae(data.key_mgmt);
			sae_a = wpa_key_mgmt_sae(data.key_mgmt);
		}
		if (wpa_parse_wpa_ie_rsn(rsne_b, 2 + rsne_b[1], &data) == 0) {
			psk_b = wpa_key_mgmt_wpa_psk_no_sae(data.key_mgmt);
			sae_b = wpa_key_mgmt_sae(data.key_mgmt);
		}

		if (sae_a && !sae_b && psk_b &&
		    (snr_a >= GREAT_SNR || snr_a >= snr_b))
			return -1;
		if (sae_b && !sae_a && psk_a &&
		    (snr_b >= GREAT_SNR || snr_b >= snr_a))
			return 1;
	}

	/* If SNR is close, decide by max rate or frequency band. For cases
	 * involving the 6 GHz band, use the throughput estimate irrespective
	 * of the SNR difference since the LPI/VLP rules may result in
	 * significant differences in SNR for cases where the estimated
	 * throughput can be considerably higher with the lower SNR. */
	if (snr_a && snr_b && (abs(snr_b - snr_a) < 7 ||
			       is_6ghz_freq(wa->freq) ||
			       is_6ghz_freq(wb->freq))) {
		if (wa->est_throughput != wb->est_throughput)
			return (int) wb->est_throughput -
				(int) wa->est_throughput;
	}
	if ((snr_a && snr_b && abs(snr_b - snr_a) < 5) ||
	    (wa->qual && wb->qual && abs(wb->qual - wa->qual) < 10)) {
		if (is_6ghz_freq(wa->freq) ^ is_6ghz_freq(wb->freq))
			return is_6ghz_freq(wa->freq) ? -1 : 1;
		if (IS_5GHZ(wa->freq) ^ IS_5GHZ(wb->freq))
			return IS_5GHZ(wa->freq) ? -1 : 1;
	}

	/* all things being equal, use SNR; if SNRs are
	 * identical, use quality values since some drivers may only report
	 * that value and leave the signal level zero */
	if (snr_b_full == snr_a_full)
		return wb->qual - wa->qual;
	return snr_b_full - snr_a_full;
}


#ifdef CONFIG_WPS
/* Compare function for sorting scan results when searching a WPS AP for
 * provisioning. Return >0 if @b is considered better. */
static int wpa_scan_result_wps_compar(const void *a, const void *b)
{
	struct wpa_scan_res **_wa = (void *) a;
	struct wpa_scan_res **_wb = (void *) b;
	struct wpa_scan_res *wa = *_wa;
	struct wpa_scan_res *wb = *_wb;
	int uses_wps_a, uses_wps_b;
	struct wpabuf *wps_a, *wps_b;
	int res;

	/* Optimization - check WPS IE existence before allocated memory and
	 * doing full reassembly. */
	uses_wps_a = wpa_scan_get_vendor_ie(wa, WPS_IE_VENDOR_TYPE) != NULL;
	uses_wps_b = wpa_scan_get_vendor_ie(wb, WPS_IE_VENDOR_TYPE) != NULL;
	if (uses_wps_a && !uses_wps_b)
		return -1;
	if (!uses_wps_a && uses_wps_b)
		return 1;

	if (uses_wps_a && uses_wps_b) {
		wps_a = wpa_scan_get_vendor_ie_multi(wa, WPS_IE_VENDOR_TYPE);
		wps_b = wpa_scan_get_vendor_ie_multi(wb, WPS_IE_VENDOR_TYPE);
		res = wps_ap_priority_compar(wps_a, wps_b);
		wpabuf_free(wps_a);
		wpabuf_free(wps_b);
		if (res)
			return res;
	}

	/*
	 * Do not use current AP security policy as a sorting criteria during
	 * WPS provisioning step since the AP may get reconfigured at the
	 * completion of provisioning.
	 */

	/* all things being equal, use signal level; if signal levels are
	 * identical, use quality values since some drivers may only report
	 * that value and leave the signal level zero */
	if (wb->level == wa->level)
		return wb->qual - wa->qual;
	return wb->level - wa->level;
}
#endif /* CONFIG_WPS */


static void dump_scan_res(struct wpa_scan_results *scan_res)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
	size_t i;

	if (scan_res->res == NULL || scan_res->num == 0)
		return;

	wpa_printf(MSG_EXCESSIVE, "Sorted scan results");

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *r = scan_res->res[i];
		u8 *pos;
		const u8 *ssid_ie, *ssid = NULL;
		size_t ssid_len = 0;

		ssid_ie = wpa_scan_get_ie(r, WLAN_EID_SSID);
		if (ssid_ie) {
			ssid = ssid_ie + 2;
			ssid_len = ssid_ie[1];
		}

		if (r->flags & WPA_SCAN_LEVEL_DBM) {
			int noise_valid = !(r->flags & WPA_SCAN_NOISE_INVALID);

			wpa_printf(MSG_EXCESSIVE, MACSTR
				   " ssid=%s freq=%d qual=%d noise=%d%s level=%d snr=%d%s flags=0x%x age=%u est=%u",
				   MAC2STR(r->bssid),
				   wpa_ssid_txt(ssid, ssid_len),
				   r->freq, r->qual,
				   r->noise, noise_valid ? "" : "~", r->level,
				   r->snr, r->snr >= GREAT_SNR ? "*" : "",
				   r->flags,
				   r->age, r->est_throughput);
		} else {
			wpa_printf(MSG_EXCESSIVE, MACSTR
				   " ssid=%s freq=%d qual=%d noise=%d level=%d flags=0x%x age=%u est=%u",
				   MAC2STR(r->bssid),
				   wpa_ssid_txt(ssid, ssid_len),
				   r->freq, r->qual,
				   r->noise, r->level, r->flags, r->age,
				   r->est_throughput);
		}
		pos = (u8 *) (r + 1);
		if (r->ie_len)
			wpa_hexdump(MSG_EXCESSIVE, "IEs", pos, r->ie_len);
		pos += r->ie_len;
		if (r->beacon_ie_len)
			wpa_hexdump(MSG_EXCESSIVE, "Beacon IEs",
				    pos, r->beacon_ie_len);
	}
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


/**
 * wpa_supplicant_filter_bssid_match - Is the specified BSSID allowed
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to check
 * Returns: 0 if the BSSID is filtered or 1 if not
 *
 * This function is used to filter out specific BSSIDs from scan reslts mainly
 * for testing purposes (SET bssid_filter ctrl_iface command).
 */
int wpa_supplicant_filter_bssid_match(struct wpa_supplicant *wpa_s,
				      const u8 *bssid)
{
	size_t i;

	if (wpa_s->bssid_filter == NULL)
		return 1;

	for (i = 0; i < wpa_s->bssid_filter_count; i++) {
		if (ether_addr_equal(wpa_s->bssid_filter + i * ETH_ALEN, bssid))
			return 1;
	}

	return 0;
}


static void filter_scan_res(struct wpa_supplicant *wpa_s,
			    struct wpa_scan_results *res)
{
	size_t i, j;

	if (wpa_s->bssid_filter == NULL)
		return;

	for (i = 0, j = 0; i < res->num; i++) {
		if (wpa_supplicant_filter_bssid_match(wpa_s,
						      res->res[i]->bssid)) {
			res->res[j++] = res->res[i];
		} else {
			os_free(res->res[i]);
			res->res[i] = NULL;
		}
	}

	if (res->num != j) {
		wpa_printf(MSG_DEBUG, "Filtered out %d scan results",
			   (int) (res->num - j));
		res->num = j;
	}
}


void scan_snr(struct wpa_scan_res *res)
{
	if (res->flags & WPA_SCAN_NOISE_INVALID) {
		res->noise = is_6ghz_freq(res->freq) ?
			DEFAULT_NOISE_FLOOR_6GHZ :
			(IS_5GHZ(res->freq) ?
			 DEFAULT_NOISE_FLOOR_5GHZ : DEFAULT_NOISE_FLOOR_2GHZ);
	}

	if (res->flags & WPA_SCAN_LEVEL_DBM) {
		res->snr = res->level - res->noise;
	} else {
		/* Level is not in dBm, so we can't calculate
		 * SNR. Just use raw level (units unknown). */
		res->snr = res->level;
	}
}


/* Minimum SNR required to achieve a certain bitrate. */
struct minsnr_bitrate_entry {
	int minsnr;
	unsigned int bitrate; /* in Mbps */
};

/* VHT needs to be enabled in order to achieve MCS8 and MCS9 rates. */
static const int vht_mcs = 8;

static const struct minsnr_bitrate_entry vht20_table[] = {
	{ 0, 0 },
	{ 2, 6500 },   /* HT20 MCS0 */
	{ 5, 13000 },  /* HT20 MCS1 */
	{ 9, 19500 },  /* HT20 MCS2 */
	{ 11, 26000 }, /* HT20 MCS3 */
	{ 15, 39000 }, /* HT20 MCS4 */
	{ 18, 52000 }, /* HT20 MCS5 */
	{ 20, 58500 }, /* HT20 MCS6 */
	{ 25, 65000 }, /* HT20 MCS7 */
	{ 29, 78000 }, /* VHT20 MCS8 */
	{ -1, 78000 }  /* SNR > 29 */
};

static const struct minsnr_bitrate_entry vht40_table[] = {
	{ 0, 0 },
	{ 5, 13500 },   /* HT40 MCS0 */
	{ 8, 27000 },   /* HT40 MCS1 */
	{ 12, 40500 },  /* HT40 MCS2 */
	{ 14, 54000 },  /* HT40 MCS3 */
	{ 18, 81000 },  /* HT40 MCS4 */
	{ 21, 108000 }, /* HT40 MCS5 */
	{ 23, 121500 }, /* HT40 MCS6 */
	{ 28, 135000 }, /* HT40 MCS7 */
	{ 32, 162000 }, /* VHT40 MCS8 */
	{ 34, 180000 }, /* VHT40 MCS9 */
	{ -1, 180000 }  /* SNR > 34 */
};

static const struct minsnr_bitrate_entry vht80_table[] = {
	{ 0, 0 },
	{ 8, 29300 },   /* VHT80 MCS0 */
	{ 11, 58500 },  /* VHT80 MCS1 */
	{ 15, 87800 },  /* VHT80 MCS2 */
	{ 17, 117000 }, /* VHT80 MCS3 */
	{ 21, 175500 }, /* VHT80 MCS4 */
	{ 24, 234000 }, /* VHT80 MCS5 */
	{ 26, 263300 }, /* VHT80 MCS6 */
	{ 31, 292500 }, /* VHT80 MCS7 */
	{ 35, 351000 }, /* VHT80 MCS8 */
	{ 37, 390000 }, /* VHT80 MCS9 */
	{ -1, 390000 }  /* SNR > 37 */
};


static const struct minsnr_bitrate_entry vht160_table[] = {
	{ 0, 0 },
	{ 11, 58500 },  /* VHT160 MCS0 */
	{ 14, 117000 }, /* VHT160 MCS1 */
	{ 18, 175500 }, /* VHT160 MCS2 */
	{ 20, 234000 }, /* VHT160 MCS3 */
	{ 24, 351000 }, /* VHT160 MCS4 */
	{ 27, 468000 }, /* VHT160 MCS5 */
	{ 29, 526500 }, /* VHT160 MCS6 */
	{ 34, 585000 }, /* VHT160 MCS7 */
	{ 38, 702000 }, /* VHT160 MCS8 */
	{ 40, 780000 }, /* VHT160 MCS9 */
	{ -1, 780000 }  /* SNR > 37 */
};

/* EHT needs to be enabled in order to achieve MCS12 and MCS13 rates. */
#define EHT_MCS 12

static const struct minsnr_bitrate_entry he20_table[] = {
	{ 0, 0 },
	{ 2, 8600 },    /* HE20 MCS0 */
	{ 5, 17200 },   /* HE20 MCS1 */
	{ 9, 25800 },   /* HE20 MCS2 */
	{ 11, 34400 },  /* HE20 MCS3 */
	{ 15, 51600 },  /* HE20 MCS4 */
	{ 18, 68800 },  /* HE20 MCS5 */
	{ 20, 77400 },  /* HE20 MCS6 */
	{ 25, 86000 },  /* HE20 MCS7 */
	{ 29, 103200 }, /* HE20 MCS8 */
	{ 31, 114700 }, /* HE20 MCS9 */
	{ 34, 129000 }, /* HE20 MCS10 */
	{ 36, 143400 }, /* HE20 MCS11 */
	{ 39, 154900 }, /* EHT20 MCS12 */
	{ 42, 172100 }, /* EHT20 MCS13 */
	{ -1, 172100 }  /* SNR > 42 */
};

static const struct minsnr_bitrate_entry he40_table[] = {
	{ 0, 0 },
	{ 5, 17200 },   /* HE40 MCS0 */
	{ 8, 34400 },   /* HE40 MCS1 */
	{ 12, 51600 },  /* HE40 MCS2 */
	{ 14, 68800 },  /* HE40 MCS3 */
	{ 18, 103200 }, /* HE40 MCS4 */
	{ 21, 137600 }, /* HE40 MCS5 */
	{ 23, 154900 }, /* HE40 MCS6 */
	{ 28, 172100 }, /* HE40 MCS7 */
	{ 32, 206500 }, /* HE40 MCS8 */
	{ 34, 229400 }, /* HE40 MCS9 */
	{ 37, 258100 }, /* HE40 MCS10 */
	{ 39, 286800 }, /* HE40 MCS11 */
	{ 42, 309500 }, /* EHT40 MCS12 */
	{ 45, 344100 }, /* EHT40 MCS13 */
	{ -1, 344100 }  /* SNR > 45 */
};

static const struct minsnr_bitrate_entry he80_table[] = {
	{ 0, 0 },
	{ 8, 36000 },   /* HE80 MCS0 */
	{ 11, 72100 },  /* HE80 MCS1 */
	{ 15, 108100 }, /* HE80 MCS2 */
	{ 17, 144100 }, /* HE80 MCS3 */
	{ 21, 216200 }, /* HE80 MCS4 */
	{ 24, 288200 }, /* HE80 MCS5 */
	{ 26, 324300 }, /* HE80 MCS6 */
	{ 31, 360300 }, /* HE80 MCS7 */
	{ 35, 432400 }, /* HE80 MCS8 */
	{ 37, 480400 }, /* HE80 MCS9 */
	{ 40, 540400 }, /* HE80 MCS10 */
	{ 42, 600500 }, /* HE80 MCS11 */
	{ 45, 648500 }, /* EHT80 MCS12 */
	{ 48, 720600 }, /* EHT80 MCS13 */
	{ -1, 720600 }  /* SNR > 48 */
};


static const struct minsnr_bitrate_entry he160_table[] = {
	{ 0, 0 },
	{ 11, 72100 },   /* HE160 MCS0 */
	{ 14, 144100 },  /* HE160 MCS1 */
	{ 18, 216200 },  /* HE160 MCS2 */
	{ 20, 288200 },  /* HE160 MCS3 */
	{ 24, 432400 },  /* HE160 MCS4 */
	{ 27, 576500 },  /* HE160 MCS5 */
	{ 29, 648500 },  /* HE160 MCS6 */
	{ 34, 720600 },  /* HE160 MCS7 */
	{ 38, 864700 },  /* HE160 MCS8 */
	{ 40, 960800 },  /* HE160 MCS9 */
	{ 43, 1080900 }, /* HE160 MCS10 */
	{ 45, 1201000 }, /* HE160 MCS11 */
	{ 48, 1297100 }, /* EHT160 MCS12 */
	{ 51, 1441200 }, /* EHT160 MCS13 */
	{ -1, 1441200 }  /* SNR > 51 */
};

/* See IEEE P802.11be/D2.0, Table 36-86: EHT-MCSs for 4x996-tone RU, NSS,u = 1
 */
static const struct minsnr_bitrate_entry eht320_table[] = {
	{ 0, 0 },
	{ 14, 144100 },   /* EHT320 MCS0 */
	{ 17, 288200 },   /* EHT320 MCS1 */
	{ 21, 432400 },   /* EHT320 MCS2 */
	{ 23, 576500 },   /* EHT320 MCS3 */
	{ 27, 864700 },   /* EHT320 MCS4 */
	{ 30, 1152900 },  /* EHT320 MCS5 */
	{ 32, 1297100 },  /* EHT320 MCS6 */
	{ 37, 1441200 },  /* EHT320 MCS7 */
	{ 41, 1729400 },  /* EHT320 MCS8 */
	{ 43, 1921500 },  /* EHT320 MCS9 */
	{ 46, 2161800 },  /* EHT320 MCS10 */
	{ 48, 2401900 },  /* EHT320 MCS11 */
	{ 51, 2594100 },  /* EHT320 MCS12 */
	{ 54, 2882400 },  /* EHT320 MCS13 */
	{ -1, 2882400 }   /* SNR > 54 */
};

static unsigned int interpolate_rate(int snr, int snr0, int snr1,
				     int rate0, int rate1)
{
	return rate0 + (snr - snr0) * (rate1 - rate0) / (snr1 - snr0);
}


static unsigned int max_rate(const struct minsnr_bitrate_entry table[],
			     int snr, bool vht)
{
	const struct minsnr_bitrate_entry *prev, *entry = table;

	while ((entry->minsnr != -1) &&
	       (snr >= entry->minsnr) &&
	       (vht || entry - table <= vht_mcs))
		entry++;
	if (entry == table)
		return entry->bitrate;
	prev = entry - 1;
	if (entry->minsnr == -1 || (!vht && entry - table > vht_mcs))
		return prev->bitrate;
	return interpolate_rate(snr, prev->minsnr, entry->minsnr, prev->bitrate,
				entry->bitrate);
}


static unsigned int max_ht20_rate(int snr, bool vht)
{
	return max_rate(vht20_table, snr, vht);
}


static unsigned int max_ht40_rate(int snr, bool vht)
{
	return max_rate(vht40_table, snr, vht);
}


static unsigned int max_vht80_rate(int snr)
{
	return max_rate(vht80_table, snr, 1);
}


static unsigned int max_vht160_rate(int snr)
{
	return max_rate(vht160_table, snr, 1);
}


static unsigned int max_he_eht_rate(const struct minsnr_bitrate_entry table[],
				    int snr, bool eht)
{
	const struct minsnr_bitrate_entry *prev, *entry = table;

	while (entry->minsnr != -1 && snr >= entry->minsnr &&
	       (eht || entry - table <= EHT_MCS))
		entry++;
	if (entry == table)
		return 0;
	prev = entry - 1;
	if (entry->minsnr == -1 || (!eht && entry - table > EHT_MCS))
		return prev->bitrate;
	return interpolate_rate(snr, prev->minsnr, entry->minsnr,
				prev->bitrate, entry->bitrate);
}


unsigned int wpas_get_est_tpt(const struct wpa_supplicant *wpa_s,
			      const u8 *ies, size_t ies_len, int rate,
			      int snr, int freq, enum chan_width *max_cw)
{
	struct hostapd_hw_modes *hw_mode;
	unsigned int est, tmp;
	const u8 *ie;
	/*
	 * No need to apply a bump to the noise here because the
	 * minsnr_bitrate_entry tables are based on MCS tables where this has
	 * been taken into account.
	 */
	int adjusted_snr;
	bool ht40 = false, vht80 = false, vht160 = false;

	/* Limit based on estimated SNR */
	if (rate > 1 * 2 && snr < 1)
		rate = 1 * 2;
	else if (rate > 2 * 2 && snr < 4)
		rate = 2 * 2;
	else if (rate > 6 * 2 && snr < 5)
		rate = 6 * 2;
	else if (rate > 9 * 2 && snr < 6)
		rate = 9 * 2;
	else if (rate > 12 * 2 && snr < 7)
		rate = 12 * 2;
	else if (rate > 12 * 2 && snr < 8)
		rate = 14 * 2;
	else if (rate > 12 * 2 && snr < 9)
		rate = 16 * 2;
	else if (rate > 18 * 2 && snr < 10)
		rate = 18 * 2;
	else if (rate > 24 * 2 && snr < 11)
		rate = 24 * 2;
	else if (rate > 24 * 2 && snr < 12)
		rate = 27 * 2;
	else if (rate > 24 * 2 && snr < 13)
		rate = 30 * 2;
	else if (rate > 24 * 2 && snr < 14)
		rate = 33 * 2;
	else if (rate > 36 * 2 && snr < 15)
		rate = 36 * 2;
	else if (rate > 36 * 2 && snr < 16)
		rate = 39 * 2;
	else if (rate > 36 * 2 && snr < 17)
		rate = 42 * 2;
	else if (rate > 36 * 2 && snr < 18)
		rate = 45 * 2;
	else if (rate > 48 * 2 && snr < 19)
		rate = 48 * 2;
	else if (rate > 48 * 2 && snr < 20)
		rate = 51 * 2;
	else if (rate > 54 * 2 && snr < 21)
		rate = 54 * 2;
	est = rate * 500;

	hw_mode = get_mode_with_freq(wpa_s->hw.modes, wpa_s->hw.num_modes,
				     freq);

	if (hw_mode && hw_mode->ht_capab) {
		ie = get_ie(ies, ies_len, WLAN_EID_HT_CAP);
		if (ie) {
			*max_cw = CHAN_WIDTH_20;
			tmp = max_ht20_rate(snr, false);
			if (tmp > est)
				est = tmp;
		}
	}

	ie = get_ie(ies, ies_len, WLAN_EID_HT_OPERATION);
	if (ie && ie[1] >= 2 &&
	    (ie[3] & HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK))
		ht40 = true;

	if (hw_mode &&
	    (hw_mode->ht_capab & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET)) {
		if (ht40) {
			*max_cw = CHAN_WIDTH_40;
			adjusted_snr = snr +
				wpas_channel_width_rssi_bump(ies, ies_len,
							     CHAN_WIDTH_40);
			tmp = max_ht40_rate(adjusted_snr, false);
			if (tmp > est)
				est = tmp;
		}
	}

	/* Determine VHT BSS bandwidth based on IEEE Std 802.11-2020,
	 * Table 11-23 (VHT BSS bandwidth) */
	ie = get_ie(ies, ies_len, WLAN_EID_VHT_OPERATION);
	if (ie && ie[1] >= 3) {
		u8 cw = ie[2] & VHT_OPMODE_CHANNEL_WIDTH_MASK;
		u8 seg0 = ie[3];
		u8 seg1 = ie[4];

		if (cw)
			vht80 = true;
		if (cw == 2 ||
		    (cw == 3 && (seg1 > 0 && abs(seg1 - seg0) == 16)))
			vht160 = true;
		if (cw == 1 &&
		    ((seg1 > 0 && abs(seg1 - seg0) == 8) ||
		     (seg1 > 0 && abs(seg1 - seg0) == 16)))
			vht160 = true;
	}

	if (hw_mode && hw_mode->vht_capab) {
		/* Use +1 to assume VHT is always faster than HT */
		ie = get_ie(ies, ies_len, WLAN_EID_VHT_CAP);
		if (ie) {
			if (*max_cw == CHAN_WIDTH_UNKNOWN)
				*max_cw = CHAN_WIDTH_20;
			tmp = max_ht20_rate(snr, true) + 1;
			if (tmp > est)
				est = tmp;

			if (ht40) {
				*max_cw = CHAN_WIDTH_40;
				adjusted_snr = snr +
					wpas_channel_width_rssi_bump(
						ies, ies_len, CHAN_WIDTH_40);
				tmp = max_ht40_rate(adjusted_snr, true) + 1;
				if (tmp > est)
					est = tmp;
			}

			if (vht80) {
				*max_cw = CHAN_WIDTH_80;
				adjusted_snr = snr +
					wpas_channel_width_rssi_bump(
						ies, ies_len, CHAN_WIDTH_80);
				tmp = max_vht80_rate(adjusted_snr) + 1;
				if (tmp > est)
					est = tmp;
			}

			if (vht160 &&
			    (hw_mode->vht_capab &
			     (VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
			      VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ))) {
				*max_cw = CHAN_WIDTH_160;
				adjusted_snr = snr +
					wpas_channel_width_rssi_bump(
						ies, ies_len, CHAN_WIDTH_160);
				tmp = max_vht160_rate(adjusted_snr) + 1;
				if (tmp > est)
					est = tmp;
			}
		}
	}

	if (hw_mode && hw_mode->he_capab[IEEE80211_MODE_INFRA].he_supported) {
		/* Use +2 to assume HE is always faster than HT/VHT */
		struct ieee80211_he_capabilities *he;
		struct ieee80211_eht_capabilities *eht;
		struct he_capabilities *own_he;
		u8 cw, boost = 2;
		const u8 *eht_ie;
		bool is_eht = false;

		ie = get_ie_ext(ies, ies_len, WLAN_EID_EXT_HE_CAPABILITIES);
		if (!ie || (ie[1] < 1 + IEEE80211_HE_CAPAB_MIN_LEN))
			return est;
		he = (struct ieee80211_he_capabilities *) &ie[3];
		own_he = &hw_mode->he_capab[IEEE80211_MODE_INFRA];

		/* Use +3 to assume EHT is always faster than HE */
		if (hw_mode->eht_capab[IEEE80211_MODE_INFRA].eht_supported) {
			eht_ie = get_ie_ext(ies, ies_len,
					    WLAN_EID_EXT_EHT_CAPABILITIES);
			if (eht_ie &&
			    (eht_ie[1] >= 1 + IEEE80211_EHT_CAPAB_MIN_LEN)) {
				is_eht = true;
				boost = 3;
			}
		}

		if (*max_cw == CHAN_WIDTH_UNKNOWN)
			*max_cw = CHAN_WIDTH_20;
		tmp = max_he_eht_rate(he20_table, snr, is_eht) + boost;
		if (tmp > est)
			est = tmp;

		cw = he->he_phy_capab_info[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
			own_he->phy_cap[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX];
		if ((cw &
		     (IS_2P4GHZ(freq) ?
		      HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_IN_2G :
		      HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G)) && ht40) {
			if (*max_cw == CHAN_WIDTH_UNKNOWN ||
			    *max_cw < CHAN_WIDTH_40)
				*max_cw = CHAN_WIDTH_40;
			adjusted_snr = snr + wpas_channel_width_rssi_bump(
				ies, ies_len, CHAN_WIDTH_40);
			tmp = max_he_eht_rate(he40_table, adjusted_snr,
					      is_eht) + boost;
			if (tmp > est)
				est = tmp;
		}

		if (!IS_2P4GHZ(freq) &&
		    (cw & HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G) &&
		    (!IS_5GHZ(freq) || vht80)) {
			if (*max_cw == CHAN_WIDTH_UNKNOWN ||
			    *max_cw < CHAN_WIDTH_80)
				*max_cw = CHAN_WIDTH_80;
			adjusted_snr = snr + wpas_channel_width_rssi_bump(
				ies, ies_len, CHAN_WIDTH_80);
			tmp = max_he_eht_rate(he80_table, adjusted_snr,
					      is_eht) + boost;
			if (tmp > est)
				est = tmp;
		}

		if (!IS_2P4GHZ(freq) &&
		    (cw & (HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
			   HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G)) &&
		    (!IS_5GHZ(freq) || vht160)) {
			if (*max_cw == CHAN_WIDTH_UNKNOWN ||
			    *max_cw < CHAN_WIDTH_160)
				*max_cw = CHAN_WIDTH_160;
			adjusted_snr = snr + wpas_channel_width_rssi_bump(
				ies, ies_len, CHAN_WIDTH_160);
			tmp = max_he_eht_rate(he160_table, adjusted_snr,
					      is_eht) + boost;
			if (tmp > est)
				est = tmp;
		}

		if (!is_eht)
			return est;

		eht = (struct ieee80211_eht_capabilities *) &eht_ie[3];

		if (is_6ghz_freq(freq) &&
		    (eht->phy_cap[EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_IDX] &
		     EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_MASK)) {
			if (*max_cw == CHAN_WIDTH_UNKNOWN ||
			    *max_cw < CHAN_WIDTH_320)
				*max_cw = CHAN_WIDTH_320;
			adjusted_snr = snr + wpas_channel_width_rssi_bump(
				ies, ies_len, CHAN_WIDTH_320);
			tmp = max_he_eht_rate(eht320_table, adjusted_snr, true);
			if (tmp > est)
				est = tmp;
		}
	}

	return est;
}


void scan_est_throughput(struct wpa_supplicant *wpa_s,
			 struct wpa_scan_res *res)
{
	int rate; /* max legacy rate in 500 kb/s units */
	int snr = res->snr;
	const u8 *ies = (const void *) (res + 1);
	size_t ie_len = res->ie_len;

	if (res->est_throughput)
		return;

	/* Get maximum legacy rate */
	rate = wpa_scan_get_max_rate(res);

	if (!ie_len)
		ie_len = res->beacon_ie_len;
	res->est_throughput = wpas_get_est_tpt(wpa_s, ies, ie_len, rate, snr,
					       res->freq, &res->max_cw);

	/* TODO: channel utilization and AP load (e.g., from AP Beacon) */
}


/**
 * wpa_supplicant_get_scan_results - Get scan results
 * @wpa_s: Pointer to wpa_supplicant data
 * @info: Information about what was scanned or %NULL if not available
 * @new_scan: Whether a new scan was performed
 * @bssid: Return BSS entries only for a single BSSID, %NULL for all
 * Returns: Scan results, %NULL on failure
 *
 * This function request the current scan results from the driver and updates
 * the local BSS list wpa_s->bss. The caller is responsible for freeing the
 * results with wpa_scan_results_free().
 */
struct wpa_scan_results *
wpa_supplicant_get_scan_results(struct wpa_supplicant *wpa_s,
				struct scan_info *info, int new_scan,
				const u8 *bssid)
{
	struct wpa_scan_results *scan_res;
	size_t i;
	int (*compar)(const void *, const void *) = wpa_scan_result_compar;

	scan_res = wpa_drv_get_scan_results(wpa_s, bssid);
	if (scan_res == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Failed to get scan results");
		return NULL;
	}
	if (scan_res->fetch_time.sec == 0) {
		/*
		 * Make sure we have a valid timestamp if the driver wrapper
		 * does not set this.
		 */
		os_get_reltime(&scan_res->fetch_time);
	}
	filter_scan_res(wpa_s, scan_res);

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *scan_res_item = scan_res->res[i];

		scan_snr(scan_res_item);
		scan_est_throughput(wpa_s, scan_res_item);
	}

#ifdef CONFIG_WPS
	if (wpas_wps_searching(wpa_s)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WPS: Order scan results with WPS "
			"provisioning rules");
		compar = wpa_scan_result_wps_compar;
	}
#endif /* CONFIG_WPS */

	if (scan_res->res) {
		qsort(scan_res->res, scan_res->num,
		      sizeof(struct wpa_scan_res *), compar);
	}
	dump_scan_res(scan_res);

	if (wpa_s->ignore_post_flush_scan_res) {
		/* FLUSH command aborted an ongoing scan and these are the
		 * results from the aborted scan. Do not process the results to
		 * maintain flushed state. */
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Do not update BSS table based on pending post-FLUSH scan results");
		wpa_s->ignore_post_flush_scan_res = 0;
		return scan_res;
	}

	wpa_bss_update_start(wpa_s);
	for (i = 0; i < scan_res->num; i++)
		wpa_bss_update_scan_res(wpa_s, scan_res->res[i],
					&scan_res->fetch_time);
	wpa_bss_update_end(wpa_s, info, new_scan);

	return scan_res;
}


/**
 * wpa_supplicant_update_scan_results - Update scan results from the driver
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: Update BSS entries only for a single BSSID, %NULL for all
 * Returns: 0 on success, -1 on failure
 *
 * This function updates the BSS table within wpa_supplicant based on the
 * currently available scan results from the driver without requesting a new
 * scan. This is used in cases where the driver indicates an association
 * (including roaming within ESS) and wpa_supplicant does not yet have the
 * needed information to complete the connection (e.g., to perform validation
 * steps in 4-way handshake).
 */
int wpa_supplicant_update_scan_results(struct wpa_supplicant *wpa_s,
				       const u8 *bssid)
{
	struct wpa_scan_results *scan_res;
	scan_res = wpa_supplicant_get_scan_results(wpa_s, NULL, 0, bssid);
	if (scan_res == NULL)
		return -1;
	wpa_scan_results_free(scan_res);

	return 0;
}


/**
 * scan_only_handler - Reports scan results
 */
void scan_only_handler(struct wpa_supplicant *wpa_s,
		       struct wpa_scan_results *scan_res)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Scan-only results received");
	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
	    wpa_s->manual_scan_use_id && wpa_s->own_scan_running) {
		wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_SCAN_RESULTS "id=%u",
			     wpa_s->manual_scan_id);
		wpa_s->manual_scan_use_id = 0;
	} else {
		wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_SCAN_RESULTS);
	}
	wpas_notify_scan_results(wpa_s);
	wpas_notify_scan_done(wpa_s, 1);
	if (wpa_s->scan_work) {
		struct wpa_radio_work *work = wpa_s->scan_work;
		wpa_s->scan_work = NULL;
		radio_work_done(work);
	}

	if (wpa_s->wpa_state == WPA_SCANNING)
		wpa_supplicant_set_state(wpa_s, wpa_s->scan_prev_wpa_state);
}


int wpas_scan_scheduled(struct wpa_supplicant *wpa_s)
{
	return eloop_is_timeout_registered(wpa_supplicant_scan, wpa_s, NULL);
}


struct wpa_driver_scan_params *
wpa_scan_clone_params(const struct wpa_driver_scan_params *src)
{
	struct wpa_driver_scan_params *params;
	size_t i;
	u8 *n;

	params = os_zalloc(sizeof(*params));
	if (params == NULL)
		return NULL;

	for (i = 0; i < src->num_ssids; i++) {
		if (src->ssids[i].ssid) {
			n = os_memdup(src->ssids[i].ssid,
				      src->ssids[i].ssid_len);
			if (n == NULL)
				goto failed;
			params->ssids[i].ssid = n;
			params->ssids[i].ssid_len = src->ssids[i].ssid_len;
		}
	}
	params->num_ssids = src->num_ssids;

	if (src->extra_ies) {
		n = os_memdup(src->extra_ies, src->extra_ies_len);
		if (n == NULL)
			goto failed;
		params->extra_ies = n;
		params->extra_ies_len = src->extra_ies_len;
	}

	if (src->freqs) {
		int len = int_array_len(src->freqs);
		params->freqs = os_memdup(src->freqs, (len + 1) * sizeof(int));
		if (params->freqs == NULL)
			goto failed;
	}

	if (src->filter_ssids) {
		params->filter_ssids = os_memdup(src->filter_ssids,
						 sizeof(*params->filter_ssids) *
						 src->num_filter_ssids);
		if (params->filter_ssids == NULL)
			goto failed;
		params->num_filter_ssids = src->num_filter_ssids;
	}

	params->filter_rssi = src->filter_rssi;
	params->p2p_probe = src->p2p_probe;
	params->only_new_results = src->only_new_results;
	params->low_priority = src->low_priority;
	params->duration = src->duration;
	params->duration_mandatory = src->duration_mandatory;
	params->oce_scan = src->oce_scan;
	params->link_id = src->link_id;

	if (src->sched_scan_plans_num > 0) {
		params->sched_scan_plans =
			os_memdup(src->sched_scan_plans,
				  sizeof(*src->sched_scan_plans) *
				  src->sched_scan_plans_num);
		if (!params->sched_scan_plans)
			goto failed;

		params->sched_scan_plans_num = src->sched_scan_plans_num;
	}

	if (src->mac_addr_rand &&
	    wpa_setup_mac_addr_rand_params(params, src->mac_addr))
		goto failed;

	if (src->bssid) {
		u8 *bssid;

		bssid = os_memdup(src->bssid, ETH_ALEN);
		if (!bssid)
			goto failed;
		params->bssid = bssid;
	}

	params->relative_rssi_set = src->relative_rssi_set;
	params->relative_rssi = src->relative_rssi;
	params->relative_adjust_band = src->relative_adjust_band;
	params->relative_adjust_rssi = src->relative_adjust_rssi;
	params->p2p_include_6ghz = src->p2p_include_6ghz;
	params->non_coloc_6ghz = src->non_coloc_6ghz;
	params->min_probe_req_content = src->min_probe_req_content;
	return params;

failed:
	wpa_scan_free_params(params);
	return NULL;
}


void wpa_scan_free_params(struct wpa_driver_scan_params *params)
{
	size_t i;

	if (params == NULL)
		return;

	for (i = 0; i < params->num_ssids; i++)
		os_free((u8 *) params->ssids[i].ssid);
	os_free((u8 *) params->extra_ies);
	os_free(params->freqs);
	os_free(params->filter_ssids);
	os_free(params->sched_scan_plans);

	/*
	 * Note: params->mac_addr_mask points to same memory allocation and
	 * must not be freed separately.
	 */
	os_free((u8 *) params->mac_addr);

	os_free((u8 *) params->bssid);

	os_free(params);
}


int wpas_start_pno(struct wpa_supplicant *wpa_s)
{
	int ret;
	size_t prio, i, num_ssid, num_match_ssid;
	struct wpa_ssid *ssid;
	struct wpa_driver_scan_params params;
	struct sched_scan_plan scan_plan;
	unsigned int max_sched_scan_ssids;

	if (!wpa_s->sched_scan_supported)
		return -1;

	if (wpa_s->max_sched_scan_ssids > WPAS_MAX_SCAN_SSIDS)
		max_sched_scan_ssids = WPAS_MAX_SCAN_SSIDS;
	else
		max_sched_scan_ssids = wpa_s->max_sched_scan_ssids;
	if (max_sched_scan_ssids < 1)
		return -1;

	if (wpa_s->pno || wpa_s->pno_sched_pending)
		return 0;

	if ((wpa_s->wpa_state > WPA_SCANNING) &&
	    (wpa_s->wpa_state < WPA_COMPLETED)) {
		wpa_printf(MSG_ERROR, "PNO: In assoc process");
		return -EAGAIN;
	}

	if (wpa_s->wpa_state == WPA_SCANNING) {
		wpa_supplicant_cancel_scan(wpa_s);
		if (wpa_s->sched_scanning) {
			wpa_printf(MSG_DEBUG, "Schedule PNO on completion of "
				   "ongoing sched scan");
			wpa_supplicant_cancel_sched_scan(wpa_s);
			wpa_s->pno_sched_pending = 1;
			return 0;
		}
	}

	if (wpa_s->sched_scan_stop_req) {
		wpa_printf(MSG_DEBUG,
			   "Schedule PNO after previous sched scan has stopped");
		wpa_s->pno_sched_pending = 1;
		return 0;
	}

	os_memset(&params, 0, sizeof(params));

	num_ssid = num_match_ssid = 0;
	ssid = wpa_s->conf->ssid;
	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid)) {
			num_match_ssid++;
			if (ssid->scan_ssid)
				num_ssid++;
		}
		ssid = ssid->next;
	}

	if (num_match_ssid == 0) {
		wpa_printf(MSG_DEBUG, "PNO: No configured SSIDs");
		return -1;
	}

	if (num_match_ssid > num_ssid) {
		params.num_ssids++; /* wildcard */
		num_ssid++;
	}

	if (num_ssid > max_sched_scan_ssids) {
		wpa_printf(MSG_DEBUG, "PNO: Use only the first %u SSIDs from "
			   "%u", max_sched_scan_ssids, (unsigned int) num_ssid);
		num_ssid = max_sched_scan_ssids;
	}

	if (num_match_ssid > wpa_s->max_match_sets) {
		num_match_ssid = wpa_s->max_match_sets;
		wpa_dbg(wpa_s, MSG_DEBUG, "PNO: Too many SSIDs to match");
	}
	params.filter_ssids = os_calloc(num_match_ssid,
					sizeof(struct wpa_driver_scan_filter));
	if (params.filter_ssids == NULL)
		return -1;

	i = 0;
	prio = 0;
	ssid = wpa_s->conf->pssid[prio];
	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid)) {
			if (ssid->scan_ssid && params.num_ssids < num_ssid) {
				params.ssids[params.num_ssids].ssid =
					ssid->ssid;
				params.ssids[params.num_ssids].ssid_len =
					 ssid->ssid_len;
				params.num_ssids++;
			}
			os_memcpy(params.filter_ssids[i].ssid, ssid->ssid,
				  ssid->ssid_len);
			params.filter_ssids[i].ssid_len = ssid->ssid_len;
			params.num_filter_ssids++;
			i++;
			if (i == num_match_ssid)
				break;
		}
		if (ssid->pnext)
			ssid = ssid->pnext;
		else if (prio + 1 == wpa_s->conf->num_prio)
			break;
		else
			ssid = wpa_s->conf->pssid[++prio];
	}

	if (wpa_s->conf->filter_rssi)
		params.filter_rssi = wpa_s->conf->filter_rssi;

	if (wpa_s->sched_scan_plans_num) {
		params.sched_scan_plans = wpa_s->sched_scan_plans;
		params.sched_scan_plans_num = wpa_s->sched_scan_plans_num;
	} else {
		/* Set one scan plan that will run infinitely */
		if (wpa_s->conf->sched_scan_interval)
			scan_plan.interval = wpa_s->conf->sched_scan_interval;
		else
			scan_plan.interval = 10;

		scan_plan.iterations = 0;
		params.sched_scan_plans = &scan_plan;
		params.sched_scan_plans_num = 1;
	}

	params.sched_scan_start_delay = wpa_s->conf->sched_scan_start_delay;

	if (params.freqs == NULL && wpa_s->manual_sched_scan_freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Limit sched scan to specified channels");
		params.freqs = wpa_s->manual_sched_scan_freqs;
	}

	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_PNO) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(&params, wpa_s->mac_addr_pno);

	wpa_scan_set_relative_rssi_params(wpa_s, &params);

	ret = wpa_supplicant_start_sched_scan(wpa_s, &params);
	os_free(params.filter_ssids);
	os_free(params.mac_addr);
	if (ret == 0)
		wpa_s->pno = 1;
	else
		wpa_msg(wpa_s, MSG_ERROR, "Failed to schedule PNO");
	return ret;
}


int wpas_stop_pno(struct wpa_supplicant *wpa_s)
{
	int ret = 0;

	if (!wpa_s->pno)
		return 0;

	ret = wpa_supplicant_stop_sched_scan(wpa_s);
	wpa_s->sched_scan_stop_req = 1;

	wpa_s->pno = 0;
	wpa_s->pno_sched_pending = 0;

	if (wpa_s->wpa_state == WPA_SCANNING)
		wpa_supplicant_req_scan(wpa_s, 0, 0);

	return ret;
}


void wpas_mac_addr_rand_scan_clear(struct wpa_supplicant *wpa_s,
				    unsigned int type)
{
	type &= MAC_ADDR_RAND_ALL;
	wpa_s->mac_addr_rand_enable &= ~type;

	if (type & MAC_ADDR_RAND_SCAN) {
		os_free(wpa_s->mac_addr_scan);
		wpa_s->mac_addr_scan = NULL;
	}

	if (type & MAC_ADDR_RAND_SCHED_SCAN) {
		os_free(wpa_s->mac_addr_sched_scan);
		wpa_s->mac_addr_sched_scan = NULL;
	}

	if (type & MAC_ADDR_RAND_PNO) {
		os_free(wpa_s->mac_addr_pno);
		wpa_s->mac_addr_pno = NULL;
	}
}


int wpas_mac_addr_rand_scan_set(struct wpa_supplicant *wpa_s,
				unsigned int type, const u8 *addr,
				const u8 *mask)
{
	u8 *tmp = NULL;

	if ((wpa_s->mac_addr_rand_supported & type) != type ) {
		wpa_printf(MSG_INFO,
			   "scan: MAC randomization type %u != supported=%u",
			   type, wpa_s->mac_addr_rand_supported);
		return -1;
	}

	wpas_mac_addr_rand_scan_clear(wpa_s, type);

	if (addr) {
		tmp = os_malloc(2 * ETH_ALEN);
		if (!tmp)
			return -1;
		os_memcpy(tmp, addr, ETH_ALEN);
		os_memcpy(tmp + ETH_ALEN, mask, ETH_ALEN);
	}

	if (type == MAC_ADDR_RAND_SCAN) {
		wpa_s->mac_addr_scan = tmp;
	} else if (type == MAC_ADDR_RAND_SCHED_SCAN) {
		wpa_s->mac_addr_sched_scan = tmp;
	} else if (type == MAC_ADDR_RAND_PNO) {
		wpa_s->mac_addr_pno = tmp;
	} else {
		wpa_printf(MSG_INFO,
			   "scan: Invalid MAC randomization type=0x%x",
			   type);
		os_free(tmp);
		return -1;
	}

	wpa_s->mac_addr_rand_enable |= type;
	return 0;
}


int wpas_mac_addr_rand_scan_get_mask(struct wpa_supplicant *wpa_s,
				     unsigned int type, u8 *mask)
{
	const u8 *to_copy;

	if ((wpa_s->mac_addr_rand_enable & type) != type)
		return -1;

	if (type == MAC_ADDR_RAND_SCAN) {
		to_copy = wpa_s->mac_addr_scan;
	} else if (type == MAC_ADDR_RAND_SCHED_SCAN) {
		to_copy = wpa_s->mac_addr_sched_scan;
	} else if (type == MAC_ADDR_RAND_PNO) {
		to_copy = wpa_s->mac_addr_pno;
	} else {
		wpa_printf(MSG_DEBUG,
			   "scan: Invalid MAC randomization type=0x%x",
			   type);
		return -1;
	}

	os_memcpy(mask, to_copy + ETH_ALEN, ETH_ALEN);
	return 0;
}


int wpas_abort_ongoing_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_radio_work *work;
	struct wpa_radio *radio = wpa_s->radio;

	dl_list_for_each(work, &radio->work, struct wpa_radio_work, list) {
		if (work->wpa_s != wpa_s || !work->started ||
		    (os_strcmp(work->type, "scan") != 0 &&
		     os_strcmp(work->type, "p2p-scan") != 0))
			continue;
		wpa_dbg(wpa_s, MSG_DEBUG, "Abort an ongoing scan");
		return wpa_drv_abort_scan(wpa_s, wpa_s->curr_scan_cookie);
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "No ongoing scan/p2p-scan found to abort");
	return -1;
}


int wpas_sched_scan_plans_set(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct sched_scan_plan *scan_plans = NULL;
	const char *token, *context = NULL;
	unsigned int num = 0;

	if (!cmd)
		return -1;

	if (!cmd[0]) {
		wpa_printf(MSG_DEBUG, "Clear sched scan plans");
		os_free(wpa_s->sched_scan_plans);
		wpa_s->sched_scan_plans = NULL;
		wpa_s->sched_scan_plans_num = 0;
		return 0;
	}

	while ((token = cstr_token(cmd, " ", &context))) {
		int ret;
		struct sched_scan_plan *scan_plan, *n;

		n = os_realloc_array(scan_plans, num + 1, sizeof(*scan_plans));
		if (!n)
			goto fail;

		scan_plans = n;
		scan_plan = &scan_plans[num];
		num++;

		ret = sscanf(token, "%u:%u", &scan_plan->interval,
			     &scan_plan->iterations);
		if (ret <= 0 || ret > 2 || !scan_plan->interval) {
			wpa_printf(MSG_ERROR,
				   "Invalid sched scan plan input: %s", token);
			goto fail;
		}

		if (scan_plan->interval > wpa_s->max_sched_scan_plan_interval) {
			wpa_printf(MSG_WARNING,
				   "scan plan %u: Scan interval too long(%u), use the maximum allowed(%u)",
				   num, scan_plan->interval,
				   wpa_s->max_sched_scan_plan_interval);
			scan_plan->interval =
				wpa_s->max_sched_scan_plan_interval;
		}

		if (ret == 1) {
			scan_plan->iterations = 0;
			break;
		}

		if (!scan_plan->iterations) {
			wpa_printf(MSG_ERROR,
				   "scan plan %u: Number of iterations cannot be zero",
				   num);
			goto fail;
		}

		if (scan_plan->iterations >
		    wpa_s->max_sched_scan_plan_iterations) {
			wpa_printf(MSG_WARNING,
				   "scan plan %u: Too many iterations(%u), use the maximum allowed(%u)",
				   num, scan_plan->iterations,
				   wpa_s->max_sched_scan_plan_iterations);
			scan_plan->iterations =
				wpa_s->max_sched_scan_plan_iterations;
		}

		wpa_printf(MSG_DEBUG,
			   "scan plan %u: interval=%u iterations=%u",
			   num, scan_plan->interval, scan_plan->iterations);
	}

	if (!scan_plans) {
		wpa_printf(MSG_ERROR, "Invalid scan plans entry");
		goto fail;
	}

	if (cstr_token(cmd, " ", &context) || scan_plans[num - 1].iterations) {
		wpa_printf(MSG_ERROR,
			   "All scan plans but the last must specify a number of iterations");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "scan plan %u (last plan): interval=%u",
		   num, scan_plans[num - 1].interval);

	if (num > wpa_s->max_sched_scan_plans) {
		wpa_printf(MSG_WARNING,
			   "Too many scheduled scan plans (only %u supported)",
			   wpa_s->max_sched_scan_plans);
		wpa_printf(MSG_WARNING,
			   "Use only the first %u scan plans, and the last one (in infinite loop)",
			   wpa_s->max_sched_scan_plans - 1);
		os_memcpy(&scan_plans[wpa_s->max_sched_scan_plans - 1],
			  &scan_plans[num - 1], sizeof(*scan_plans));
		num = wpa_s->max_sched_scan_plans;
	}

	os_free(wpa_s->sched_scan_plans);
	wpa_s->sched_scan_plans = scan_plans;
	wpa_s->sched_scan_plans_num = num;

	return 0;

fail:
	os_free(scan_plans);
	wpa_printf(MSG_ERROR, "invalid scan plans list");
	return -1;
}


/**
 * wpas_scan_reset_sched_scan - Reset sched_scan state
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to cancel a running scheduled scan and to reset an
 * internal scan state to continue with a regular scan on the following
 * wpa_supplicant_req_scan() calls.
 */
void wpas_scan_reset_sched_scan(struct wpa_supplicant *wpa_s)
{
	wpa_s->normal_scans = 0;
	if (wpa_s->sched_scanning) {
		wpa_s->sched_scan_timed_out = 0;
		wpa_s->prev_sched_ssid = NULL;
		wpa_supplicant_cancel_sched_scan(wpa_s);
	}
}


void wpas_scan_restart_sched_scan(struct wpa_supplicant *wpa_s)
{
	/* simulate timeout to restart the sched scan */
	wpa_s->sched_scan_timed_out = 1;
	wpa_s->prev_sched_ssid = NULL;
	wpa_supplicant_cancel_sched_scan(wpa_s);
}
