/*
 * BSS table
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
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
#include "drivers/driver.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "notify.h"
#include "scan.h"
#include "bss.h"


/**
 * WPA_BSS_EXPIRATION_PERIOD - Period of expiration run in seconds
 */
#define WPA_BSS_EXPIRATION_PERIOD 10

/**
 * WPA_BSS_EXPIRATION_AGE - BSS entry age after which it can be expired
 *
 * This value control the time in seconds after which a BSS entry gets removed
 * if it has not been updated or is not in use.
 */
#define WPA_BSS_EXPIRATION_AGE 180

/**
 * WPA_BSS_EXPIRATION_SCAN_COUNT - Expire BSS after number of scans
 *
 * If the BSS entry has not been seen in this many scans, it will be removed.
 * Value 1 means that the entry is removed after the first scan without the
 * BSSID being seen. Larger values can be used to avoid BSS entries
 * disappearing if they are not visible in every scan (e.g., low signal quality
 * or interference).
 */
#define WPA_BSS_EXPIRATION_SCAN_COUNT 2

#define WPA_BSS_FREQ_CHANGED_FLAG	BIT(0)
#define WPA_BSS_SIGNAL_CHANGED_FLAG	BIT(1)
#define WPA_BSS_PRIVACY_CHANGED_FLAG	BIT(2)
#define WPA_BSS_MODE_CHANGED_FLAG	BIT(3)
#define WPA_BSS_WPAIE_CHANGED_FLAG	BIT(4)
#define WPA_BSS_RSNIE_CHANGED_FLAG	BIT(5)
#define WPA_BSS_WPS_CHANGED_FLAG	BIT(6)
#define WPA_BSS_RATES_CHANGED_FLAG	BIT(7)
#define WPA_BSS_IES_CHANGED_FLAG	BIT(8)


static void wpa_bss_remove(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	dl_list_del(&bss->list);
	dl_list_del(&bss->list_id);
	wpa_s->num_bss--;
	wpa_printf(MSG_DEBUG, "BSS: Remove id %u BSSID " MACSTR " SSID '%s'",
		   bss->id, MAC2STR(bss->bssid),
		   wpa_ssid_txt(bss->ssid, bss->ssid_len));
	wpas_notify_bss_removed(wpa_s, bss->bssid, bss->id);
	os_free(bss);
}


struct wpa_bss * wpa_bss_get(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     const u8 *ssid, size_t ssid_len)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (os_memcmp(bss->bssid, bssid, ETH_ALEN) == 0 &&
		    bss->ssid_len == ssid_len &&
		    os_memcmp(bss->ssid, ssid, ssid_len) == 0)
			return bss;
	}
	return NULL;
}


static void wpa_bss_copy_res(struct wpa_bss *dst, struct wpa_scan_res *src)
{
	os_time_t usec;

	dst->flags = src->flags;
	os_memcpy(dst->bssid, src->bssid, ETH_ALEN);
	dst->freq = src->freq;
	dst->beacon_int = src->beacon_int;
	dst->caps = src->caps;
	dst->qual = src->qual;
	dst->noise = src->noise;
	dst->level = src->level;
	dst->tsf = src->tsf;

	os_get_time(&dst->last_update);
	dst->last_update.sec -= src->age / 1000;
	usec = (src->age % 1000) * 1000;
	if (dst->last_update.usec < usec) {
		dst->last_update.sec--;
		dst->last_update.usec += 1000000;
	}
	dst->last_update.usec -= usec;
}


static void wpa_bss_add(struct wpa_supplicant *wpa_s,
			const u8 *ssid, size_t ssid_len,
			struct wpa_scan_res *res)
{
	struct wpa_bss *bss;

	bss = os_zalloc(sizeof(*bss) + res->ie_len + res->beacon_ie_len);
	if (bss == NULL)
		return;
	bss->id = wpa_s->bss_next_id++;
	bss->last_update_idx = wpa_s->bss_update_idx;
	wpa_bss_copy_res(bss, res);
	os_memcpy(bss->ssid, ssid, ssid_len);
	bss->ssid_len = ssid_len;
	bss->ie_len = res->ie_len;
	bss->beacon_ie_len = res->beacon_ie_len;
	os_memcpy(bss + 1, res + 1, res->ie_len + res->beacon_ie_len);

	dl_list_add_tail(&wpa_s->bss, &bss->list);
	dl_list_add_tail(&wpa_s->bss_id, &bss->list_id);
	wpa_s->num_bss++;
	wpa_printf(MSG_DEBUG, "BSS: Add new id %u BSSID " MACSTR " SSID '%s'",
		   bss->id, MAC2STR(bss->bssid), wpa_ssid_txt(ssid, ssid_len));
	wpas_notify_bss_added(wpa_s, bss->bssid, bss->id);
	if (wpa_s->num_bss > wpa_s->conf->bss_max_count) {
		/* Remove the oldest entry */
		wpa_bss_remove(wpa_s, dl_list_first(&wpa_s->bss,
						    struct wpa_bss, list));
	}
}


static int are_ies_equal(const struct wpa_bss *old,
			 const struct wpa_scan_res *new, u32 ie)
{
	const u8 *old_ie, *new_ie;
	struct wpabuf *old_ie_buff = NULL;
	struct wpabuf *new_ie_buff = NULL;
	int new_ie_len, old_ie_len, ret, is_multi;

	switch (ie) {
	case WPA_IE_VENDOR_TYPE:
		old_ie = wpa_bss_get_vendor_ie(old, ie);
		new_ie = wpa_scan_get_vendor_ie(new, ie);
		is_multi = 0;
		break;
	case WPS_IE_VENDOR_TYPE:
		old_ie_buff = wpa_bss_get_vendor_ie_multi(old, ie);
		new_ie_buff = wpa_scan_get_vendor_ie_multi(new, ie);
		is_multi = 1;
		break;
	case WLAN_EID_RSN:
	case WLAN_EID_SUPP_RATES:
	case WLAN_EID_EXT_SUPP_RATES:
		old_ie = wpa_bss_get_ie(old, ie);
		new_ie = wpa_scan_get_ie(new, ie);
		is_multi = 0;
		break;
	default:
		wpa_printf(MSG_DEBUG, "bss: %s: cannot compare IEs", __func__);
		return 0;
	}

	if (is_multi) {
		/* in case of multiple IEs stored in buffer */
		old_ie = old_ie_buff ? wpabuf_head_u8(old_ie_buff) : NULL;
		new_ie = new_ie_buff ? wpabuf_head_u8(new_ie_buff) : NULL;
		old_ie_len = old_ie_buff ? wpabuf_len(old_ie_buff) : 0;
		new_ie_len = new_ie_buff ? wpabuf_len(new_ie_buff) : 0;
	} else {
		/* in case of single IE */
		old_ie_len = old_ie ? old_ie[1] + 2 : 0;
		new_ie_len = new_ie ? new_ie[1] + 2 : 0;
	}

	ret = (old_ie_len == new_ie_len &&
	       os_memcmp(old_ie, new_ie, old_ie_len) == 0);

	wpabuf_free(old_ie_buff);
	wpabuf_free(new_ie_buff);

	return ret;
}


static u32 wpa_bss_compare_res(const struct wpa_bss *old,
			       const struct wpa_scan_res *new)
{
	u32 changes = 0;
	int caps_diff = old->caps ^ new->caps;

	if (old->freq != new->freq)
		changes |= WPA_BSS_FREQ_CHANGED_FLAG;

	if (old->level != new->level)
		changes |= WPA_BSS_SIGNAL_CHANGED_FLAG;

	if (caps_diff & IEEE80211_CAP_PRIVACY)
		changes |= WPA_BSS_PRIVACY_CHANGED_FLAG;

	if (caps_diff & IEEE80211_CAP_IBSS)
		changes |= WPA_BSS_MODE_CHANGED_FLAG;

	if (old->ie_len == new->ie_len &&
	    os_memcmp(old + 1, new + 1, old->ie_len) == 0)
		return changes;
	changes |= WPA_BSS_IES_CHANGED_FLAG;

	if (!are_ies_equal(old, new, WPA_IE_VENDOR_TYPE))
		changes |= WPA_BSS_WPAIE_CHANGED_FLAG;

	if (!are_ies_equal(old, new, WLAN_EID_RSN))
		changes |= WPA_BSS_RSNIE_CHANGED_FLAG;

	if (!are_ies_equal(old, new, WPS_IE_VENDOR_TYPE))
		changes |= WPA_BSS_WPS_CHANGED_FLAG;

	if (!are_ies_equal(old, new, WLAN_EID_SUPP_RATES) ||
	    !are_ies_equal(old, new, WLAN_EID_EXT_SUPP_RATES))
		changes |= WPA_BSS_RATES_CHANGED_FLAG;

	return changes;
}


static void notify_bss_changes(struct wpa_supplicant *wpa_s, u32 changes,
			       const struct wpa_bss *bss)
{
	if (changes & WPA_BSS_FREQ_CHANGED_FLAG)
		wpas_notify_bss_freq_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_SIGNAL_CHANGED_FLAG)
		wpas_notify_bss_signal_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_PRIVACY_CHANGED_FLAG)
		wpas_notify_bss_privacy_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_MODE_CHANGED_FLAG)
		wpas_notify_bss_mode_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_WPAIE_CHANGED_FLAG)
		wpas_notify_bss_wpaie_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_RSNIE_CHANGED_FLAG)
		wpas_notify_bss_rsnie_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_WPS_CHANGED_FLAG)
		wpas_notify_bss_wps_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_IES_CHANGED_FLAG)
		wpas_notify_bss_ies_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_RATES_CHANGED_FLAG)
		wpas_notify_bss_rates_changed(wpa_s, bss->id);
}


static void wpa_bss_update(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			   struct wpa_scan_res *res)
{
	u32 changes;

	changes = wpa_bss_compare_res(bss, res);
	bss->scan_miss_count = 0;
	bss->last_update_idx = wpa_s->bss_update_idx;
	wpa_bss_copy_res(bss, res);
	/* Move the entry to the end of the list */
	dl_list_del(&bss->list);
	if (bss->ie_len + bss->beacon_ie_len >=
	    res->ie_len + res->beacon_ie_len) {
		os_memcpy(bss + 1, res + 1, res->ie_len + res->beacon_ie_len);
		bss->ie_len = res->ie_len;
		bss->beacon_ie_len = res->beacon_ie_len;
	} else {
		struct wpa_bss *nbss;
		struct dl_list *prev = bss->list_id.prev;
		dl_list_del(&bss->list_id);
		nbss = os_realloc(bss, sizeof(*bss) + res->ie_len +
				  res->beacon_ie_len);
		if (nbss) {
			bss = nbss;
			os_memcpy(bss + 1, res + 1,
				  res->ie_len + res->beacon_ie_len);
			bss->ie_len = res->ie_len;
			bss->beacon_ie_len = res->beacon_ie_len;
		}
		dl_list_add(prev, &bss->list_id);
	}
	dl_list_add_tail(&wpa_s->bss, &bss->list);

	notify_bss_changes(wpa_s, changes, bss);
}


static int wpa_bss_in_use(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	return bss == wpa_s->current_bss ||
		os_memcmp(bss->bssid, wpa_s->bssid, ETH_ALEN) == 0 ||
		os_memcmp(bss->bssid, wpa_s->pending_bssid, ETH_ALEN) == 0;
}


void wpa_bss_update_start(struct wpa_supplicant *wpa_s)
{
	wpa_s->bss_update_idx++;
	wpa_printf(MSG_DEBUG, "BSS: Start scan result update %u",
		   wpa_s->bss_update_idx);
}


void wpa_bss_update_scan_res(struct wpa_supplicant *wpa_s,
			     struct wpa_scan_res *res)
{
	const u8 *ssid;
	struct wpa_bss *bss;

	ssid = wpa_scan_get_ie(res, WLAN_EID_SSID);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "BSS: No SSID IE included for " MACSTR,
			   MAC2STR(res->bssid));
		return;
	}
	if (ssid[1] > 32) {
		wpa_printf(MSG_DEBUG, "BSS: Too long SSID IE included for "
			   MACSTR, MAC2STR(res->bssid));
		return;
	}

	/* TODO: add option for ignoring BSSes we are not interested in
	 * (to save memory) */
	bss = wpa_bss_get(wpa_s, res->bssid, ssid + 2, ssid[1]);
	if (bss == NULL)
		wpa_bss_add(wpa_s, ssid + 2, ssid[1], res);
	else
		wpa_bss_update(wpa_s, bss, res);
}


static int wpa_bss_included_in_scan(const struct wpa_bss *bss,
				    const struct scan_info *info)
{
	int found;
	size_t i;

	if (info == NULL)
		return 1;

	if (info->num_freqs) {
		found = 0;
		for (i = 0; i < info->num_freqs; i++) {
			if (bss->freq == info->freqs[i]) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
	}

	if (info->num_ssids) {
		found = 0;
		for (i = 0; i < info->num_ssids; i++) {
			const struct wpa_driver_scan_ssid *s = &info->ssids[i];
			if ((s->ssid == NULL || s->ssid_len == 0) ||
			    (s->ssid_len == bss->ssid_len &&
			     os_memcmp(s->ssid, bss->ssid, bss->ssid_len) ==
			     0)) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
	}

	return 1;
}


void wpa_bss_update_end(struct wpa_supplicant *wpa_s, struct scan_info *info,
			int new_scan)
{
	struct wpa_bss *bss, *n;

	if (!new_scan)
		return; /* do not expire entries without new scan */

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;
		if (!wpa_bss_included_in_scan(bss, info))
			continue; /* expire only BSSes that were scanned */
		if (bss->last_update_idx < wpa_s->bss_update_idx)
			bss->scan_miss_count++;
		if (bss->scan_miss_count >= WPA_BSS_EXPIRATION_SCAN_COUNT) {
			wpa_printf(MSG_DEBUG, "BSS: Expire BSS %u due to no "
				   "match in scan", bss->id);
			wpa_bss_remove(wpa_s, bss);
		}
	}
}


static void wpa_bss_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_bss *bss, *n;
	struct os_time t;

	if (dl_list_empty(&wpa_s->bss))
		return;

	os_get_time(&t);
	t.sec -= WPA_BSS_EXPIRATION_AGE;

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;

		if (os_time_before(&bss->last_update, &t)) {
			wpa_printf(MSG_DEBUG, "BSS: Expire BSS %u due to age",
				   bss->id);
			wpa_bss_remove(wpa_s, bss);
		} else
			break;
	}
	eloop_register_timeout(WPA_BSS_EXPIRATION_PERIOD, 0,
			       wpa_bss_timeout, wpa_s, NULL);
}


int wpa_bss_init(struct wpa_supplicant *wpa_s)
{
	dl_list_init(&wpa_s->bss);
	dl_list_init(&wpa_s->bss_id);
	eloop_register_timeout(WPA_BSS_EXPIRATION_PERIOD, 0,
			       wpa_bss_timeout, wpa_s, NULL);
	return 0;
}


void wpa_bss_deinit(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss, *n;
	eloop_cancel_timeout(wpa_bss_timeout, wpa_s, NULL);
	if (wpa_s->bss.next == NULL)
		return; /* BSS table not yet initialized */
	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list)
		wpa_bss_remove(wpa_s, bss);
}


struct wpa_bss * wpa_bss_get_bssid(struct wpa_supplicant *wpa_s,
				   const u8 *bssid)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (os_memcmp(bss->bssid, bssid, ETH_ALEN) == 0)
			return bss;
	}
	return NULL;
}


struct wpa_bss * wpa_bss_get_id(struct wpa_supplicant *wpa_s, unsigned int id)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bss->id == id)
			return bss;
	}
	return NULL;
}


const u8 * wpa_bss_get_ie(const struct wpa_bss *bss, u8 ie)
{
	const u8 *end, *pos;

	pos = (const u8 *) (bss + 1);
	end = pos + bss->ie_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


const u8 * wpa_bss_get_vendor_ie(const struct wpa_bss *bss, u32 vendor_type)
{
	const u8 *end, *pos;

	pos = (const u8 *) (bss + 1);
	end = pos + bss->ie_len;

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


struct wpabuf * wpa_bss_get_vendor_ie_multi(const struct wpa_bss *bss,
					    u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(bss->ie_len);
	if (buf == NULL)
		return NULL;

	pos = (const u8 *) (bss + 1);
	end = pos + bss->ie_len;

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


int wpa_bss_get_max_rate(const struct wpa_bss *bss)
{
	int rate = 0;
	const u8 *ie;
	int i;

	ie = wpa_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	ie = wpa_bss_get_ie(bss, WLAN_EID_EXT_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	return rate;
}


int wpa_bss_get_bit_rates(const struct wpa_bss *bss, u8 **rates)
{
	const u8 *ie, *ie2;
	int i, j;
	unsigned int len;
	u8 *r;

	ie = wpa_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	ie2 = wpa_bss_get_ie(bss, WLAN_EID_EXT_SUPP_RATES);

	len = (ie ? ie[1] : 0) + (ie2 ? ie2[1] : 0);

	r = os_malloc(len);
	if (!r)
		return -1;

	for (i = 0; ie && i < ie[1]; i++)
		r[i] = ie[i + 2] & 0x7f;

	for (j = 0; ie2 && j < ie2[1]; j++)
		r[i + j] = ie2[j + 2] & 0x7f;

	*rates = r;
	return len;
}
