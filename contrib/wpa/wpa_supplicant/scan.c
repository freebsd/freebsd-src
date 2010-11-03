/*
 * WPA Supplicant - Scanning
 * Copyright (c) 2003-2010, Jouni Malinen <j@w1.fi>
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

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "mlme.h"
#include "wps_supplicant.h"
#include "notify.h"
#include "bss.h"
#include "scan.h"


static void wpa_supplicant_gen_assoc_event(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;
	union wpa_event_data data;

	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL)
		return;

	if (wpa_s->current_ssid == NULL) {
		wpa_s->current_ssid = ssid;
		if (wpa_s->current_ssid != NULL)
			wpas_notify_network_changed(wpa_s);
	}
	wpa_supplicant_initiate_eapol(wpa_s);
	wpa_printf(MSG_DEBUG, "Already associated with a configured network - "
		   "generating associated event");
	os_memset(&data, 0, sizeof(data));
	wpa_supplicant_event(wpa_s, EVENT_ASSOC, &data);
}


#ifdef CONFIG_WPS
static int wpas_wps_in_use(struct wpa_config *conf,
			   enum wps_request_type *req_type)
{
	struct wpa_ssid *ssid;
	int wps = 0;

	for (ssid = conf->ssid; ssid; ssid = ssid->next) {
		if (!(ssid->key_mgmt & WPA_KEY_MGMT_WPS))
			continue;

		wps = 1;
		*req_type = wpas_wps_get_req_type(ssid);
		if (!ssid->eap.phase1)
			continue;

		if (os_strstr(ssid->eap.phase1, "pbc=1"))
			return 2;
	}

	return wps;
}
#endif /* CONFIG_WPS */


int wpa_supplicant_enabled_networks(struct wpa_config *conf)
{
	struct wpa_ssid *ssid = conf->ssid;
	while (ssid) {
		if (!ssid->disabled)
			return 1;
		ssid = ssid->next;
	}
	return 0;
}


static void wpa_supplicant_assoc_try(struct wpa_supplicant *wpa_s,
				     struct wpa_ssid *ssid)
{
	while (ssid) {
		if (!ssid->disabled)
			break;
		ssid = ssid->next;
	}

	/* ap_scan=2 mode - try to associate with each SSID. */
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "wpa_supplicant_scan: Reached "
			   "end of scan list - go back to beginning");
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
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


static int int_array_len(const int *a)
{
	int i;
	for (i = 0; a && a[i]; i++)
		;
	return i;
}


static void int_array_concat(int **res, const int *a)
{
	int reslen, alen, i;
	int *n;

	reslen = int_array_len(*res);
	alen = int_array_len(a);

	n = os_realloc(*res, (reslen + alen + 1) * sizeof(int));
	if (n == NULL) {
		os_free(*res);
		*res = NULL;
		return;
	}
	for (i = 0; i <= alen; i++)
		n[reslen + i] = a[i];
	*res = n;
}


static int freq_cmp(const void *a, const void *b)
{
	int _a = *(int *) a;
	int _b = *(int *) b;

	if (_a == 0)
		return 1;
	if (_b == 0)
		return -1;
	return _a - _b;
}


static void int_array_sort_unique(int *a)
{
	int alen;
	int i, j;

	if (a == NULL)
		return;

	alen = int_array_len(a);
	qsort(a, alen, sizeof(int), freq_cmp);

	i = 0;
	j = 1;
	while (a[i] && a[j]) {
		if (a[i] == a[j]) {
			j++;
			continue;
		}
		a[++i] = a[j++];
	}
	if (a[i])
		i++;
	a[i] = 0;
}


int wpa_supplicant_trigger_scan(struct wpa_supplicant *wpa_s,
				struct wpa_driver_scan_params *params)
{
	int ret;

	wpa_supplicant_notify_scanning(wpa_s, 1);

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME)
		ret = ieee80211_sta_req_scan(wpa_s, params);
	else
		ret = wpa_drv_scan(wpa_s, params);

	if (ret) {
		wpa_supplicant_notify_scanning(wpa_s, 0);
		wpas_notify_scan_done(wpa_s, 0);
	} else
		wpa_s->scan_runs++;

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
	ssids = os_zalloc(count * sizeof(struct wpa_driver_scan_filter));
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


static void wpa_supplicant_scan(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_ssid *ssid;
	int scan_req = 0, ret;
	struct wpabuf *wps_ie = NULL;
#ifdef CONFIG_WPS
	int wps = 0;
	enum wps_request_type req_type = WPS_REQ_ENROLLEE_INFO;
#endif /* CONFIG_WPS */
	struct wpa_driver_scan_params params;
	size_t max_ssids;
	enum wpa_states prev_state;

	if (wpa_s->disconnected && !wpa_s->scan_req) {
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		return;
	}

	if (!wpa_supplicant_enabled_networks(wpa_s->conf) &&
	    !wpa_s->scan_req) {
		wpa_printf(MSG_DEBUG, "No enabled networks - do not scan");
		wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);
		return;
	}

	if (wpa_s->conf->ap_scan != 0 &&
	    (wpa_s->drv_flags & WPA_DRIVER_FLAGS_WIRED)) {
		wpa_printf(MSG_DEBUG, "Using wired authentication - "
			   "overriding ap_scan configuration");
		wpa_s->conf->ap_scan = 0;
		wpas_notify_ap_scan_changed(wpa_s);
	}

	if (wpa_s->conf->ap_scan == 0) {
		wpa_supplicant_gen_assoc_event(wpa_s);
		return;
	}

	if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME) ||
	    wpa_s->conf->ap_scan == 2)
		max_ssids = 1;
	else {
		max_ssids = wpa_s->max_scan_ssids;
		if (max_ssids > WPAS_MAX_SCAN_SSIDS)
			max_ssids = WPAS_MAX_SCAN_SSIDS;
	}

#ifdef CONFIG_WPS
	wps = wpas_wps_in_use(wpa_s->conf, &req_type);
#endif /* CONFIG_WPS */

	scan_req = wpa_s->scan_req;
	wpa_s->scan_req = 0;

	os_memset(&params, 0, sizeof(params));

	prev_state = wpa_s->wpa_state;
	if (wpa_s->wpa_state == WPA_DISCONNECTED ||
	    wpa_s->wpa_state == WPA_INACTIVE)
		wpa_supplicant_set_state(wpa_s, WPA_SCANNING);

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

	if (scan_req != 2 && (wpa_s->conf->ap_scan == 2 ||
			      wpa_s->connect_without_scan)) {
		wpa_s->connect_without_scan = 0;
		wpa_supplicant_assoc_try(wpa_s, ssid);
		return;
	} else if (wpa_s->conf->ap_scan == 2) {
		/*
		 * User-initiated scan request in ap_scan == 2; scan with
		 * wildcard SSID.
		 */
		ssid = NULL;
	} else {
		struct wpa_ssid *start = ssid, *tssid;
		int freqs_set = 0;
		if (ssid == NULL && max_ssids > 1)
			ssid = wpa_s->conf->ssid;
		while (ssid) {
			if (!ssid->disabled && ssid->scan_ssid) {
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
			ssid = ssid->next;
			if (ssid == start)
				break;
			if (ssid == NULL && max_ssids > 1 &&
			    start != wpa_s->conf->ssid)
				ssid = wpa_s->conf->ssid;
		}

		for (tssid = wpa_s->conf->ssid; tssid; tssid = tssid->next) {
			if (tssid->disabled)
				continue;
			if ((params.freqs || !freqs_set) && tssid->scan_freq) {
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

	if (ssid) {
		wpa_s->prev_scan_ssid = ssid;
		if (max_ssids > 1) {
			wpa_printf(MSG_DEBUG, "Include wildcard SSID in the "
				   "scan request");
			params.num_ssids++;
		}
		wpa_printf(MSG_DEBUG, "Starting AP scan for specific SSID(s)");
	} else {
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
		params.num_ssids++;
		wpa_printf(MSG_DEBUG, "Starting AP scan for wildcard SSID");
	}

#ifdef CONFIG_WPS
	if (params.freqs == NULL && wpa_s->after_wps && wpa_s->wps_freq) {
		/*
		 * Optimize post-provisioning scan based on channel used
		 * during provisioning.
		 */
		wpa_printf(MSG_DEBUG, "WPS: Scan only frequency %u MHz that "
			   "was used during provisioning", wpa_s->wps_freq);
		params.freqs = os_zalloc(2 * sizeof(int));
		if (params.freqs)
			params.freqs[0] = wpa_s->wps_freq;
		wpa_s->after_wps--;
	}

	if (wps) {
		wps_ie = wps_build_probe_req_ie(wps == 2, &wpa_s->wps->dev,
						wpa_s->wps->uuid, req_type);
		if (wps_ie) {
			params.extra_ies = wpabuf_head(wps_ie);
			params.extra_ies_len = wpabuf_len(wps_ie);
		}
	}
#endif /* CONFIG_WPS */

	params.filter_ssids = wpa_supplicant_build_filter_ssids(
		wpa_s->conf, &params.num_filter_ssids);

	ret = wpa_supplicant_trigger_scan(wpa_s, &params);

	wpabuf_free(wps_ie);
	os_free(params.freqs);
	os_free(params.filter_ssids);

	if (ret) {
		wpa_printf(MSG_WARNING, "Failed to initiate AP scan.");
		if (prev_state != wpa_s->wpa_state)
			wpa_supplicant_set_state(wpa_s, prev_state);
		wpa_supplicant_req_scan(wpa_s, 1, 0);
	}
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
	/* If there's at least one network that should be specifically scanned
	 * then don't cancel the scan and reschedule.  Some drivers do
	 * background scanning which generates frequent scan results, and that
	 * causes the specific SSID scan to get continually pushed back and
	 * never happen, which causes hidden APs to never get probe-scanned.
	 */
	if (eloop_is_timeout_registered(wpa_supplicant_scan, wpa_s, NULL) &&
	    wpa_s->conf->ap_scan == 1) {
		struct wpa_ssid *ssid = wpa_s->conf->ssid;

		while (ssid) {
			if (!ssid->disabled && ssid->scan_ssid)
				break;
			ssid = ssid->next;
		}
		if (ssid) {
			wpa_msg(wpa_s, MSG_DEBUG, "Not rescheduling scan to "
			        "ensure that specific SSID scans occur");
			return;
		}
	}

	wpa_msg(wpa_s, MSG_DEBUG, "Setting scan request: %d sec %d usec",
		sec, usec);
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
	eloop_register_timeout(sec, usec, wpa_supplicant_scan, wpa_s, NULL);
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
	wpa_msg(wpa_s, MSG_DEBUG, "Cancelling scan request");
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
}


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


const u8 * wpa_scan_get_ie(const struct wpa_scan_res *res, u8 ie)
{
	const u8 *end, *pos;

	pos = (const u8 *) (res + 1);
	end = pos + res->ie_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


const u8 * wpa_scan_get_vendor_ie(const struct wpa_scan_res *res,
				  u32 vendor_type)
{
	const u8 *end, *pos;

	pos = (const u8 *) (res + 1);
	end = pos + res->ie_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		    vendor_type == WPA_GET_BE32(&pos[2]))
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


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

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		    vendor_type == WPA_GET_BE32(&pos[2]))
			wpabuf_put_data(buf, pos + 2 + 4, pos[1] - 4);
		pos += 2 + pos[1];
	}

	if (wpabuf_len(buf) == 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}


/* Compare function for sorting scan results. Return >0 if @b is considered
 * better. */
static int wpa_scan_result_compar(const void *a, const void *b)
{
	struct wpa_scan_res **_wa = (void *) a;
	struct wpa_scan_res **_wb = (void *) b;
	struct wpa_scan_res *wa = *_wa;
	struct wpa_scan_res *wb = *_wb;
	int wpa_a, wpa_b, maxrate_a, maxrate_b;

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

	/* best/max rate preferred if signal level close enough XXX */
	if ((wa->level && wb->level && abs(wb->level - wa->level) < 5) ||
	    (wa->qual && wb->qual && abs(wb->qual - wa->qual) < 10)) {
		maxrate_a = wpa_scan_get_max_rate(wa);
		maxrate_b = wpa_scan_get_max_rate(wb);
		if (maxrate_a != maxrate_b)
			return maxrate_b - maxrate_a;
	}

	/* use freq for channel preference */

	/* all things being equal, use signal level; if signal levels are
	 * identical, use quality values since some drivers may only report
	 * that value and leave the signal level zero */
	if (wb->level == wa->level)
		return wb->qual - wa->qual;
	return wb->level - wa->level;
}


/**
 * wpa_supplicant_get_scan_results - Get scan results
 * @wpa_s: Pointer to wpa_supplicant data
 * @info: Information about what was scanned or %NULL if not available
 * @new_scan: Whether a new scan was performed
 * Returns: Scan results, %NULL on failure
 *
 * This function request the current scan results from the driver and updates
 * the local BSS list wpa_s->bss. The caller is responsible for freeing the
 * results with wpa_scan_results_free().
 */
struct wpa_scan_results *
wpa_supplicant_get_scan_results(struct wpa_supplicant *wpa_s,
				struct scan_info *info, int new_scan)
{
	struct wpa_scan_results *scan_res;
	size_t i;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME)
		scan_res = ieee80211_sta_get_scan_results(wpa_s);
	else
		scan_res = wpa_drv_get_scan_results2(wpa_s);
	if (scan_res == NULL) {
		wpa_printf(MSG_DEBUG, "Failed to get scan results");
		return NULL;
	}

	qsort(scan_res->res, scan_res->num, sizeof(struct wpa_scan_res *),
	      wpa_scan_result_compar);

	wpa_bss_update_start(wpa_s);
	for (i = 0; i < scan_res->num; i++)
		wpa_bss_update_scan_res(wpa_s, scan_res->res[i]);
	wpa_bss_update_end(wpa_s, info, new_scan);

	return scan_res;
}


int wpa_supplicant_update_scan_results(struct wpa_supplicant *wpa_s)
{
	struct wpa_scan_results *scan_res;
	scan_res = wpa_supplicant_get_scan_results(wpa_s, NULL, 0);
	if (scan_res == NULL)
		return -1;
	wpa_scan_results_free(scan_res);

	return 0;
}


void wpa_scan_results_free(struct wpa_scan_results *res)
{
	size_t i;

	if (res == NULL)
		return;

	for (i = 0; i < res->num; i++)
		os_free(res->res[i]);
	os_free(res->res);
	os_free(res);
}
