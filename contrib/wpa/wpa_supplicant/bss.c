/*
 * BSS table
 * Copyright (c) 2009-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "eap_peer/eap.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "notify.h"
#include "scan.h"
#include "bssid_ignore.h"
#include "bss.h"

static void wpa_bss_set_hessid(struct wpa_bss *bss)
{
#ifdef CONFIG_INTERWORKING
	const u8 *ie = wpa_bss_get_ie(bss, WLAN_EID_INTERWORKING);
	if (ie == NULL || (ie[1] != 7 && ie[1] != 9)) {
		os_memset(bss->hessid, 0, ETH_ALEN);
		return;
	}
	if (ie[1] == 7)
		os_memcpy(bss->hessid, ie + 3, ETH_ALEN);
	else
		os_memcpy(bss->hessid, ie + 5, ETH_ALEN);
#endif /* CONFIG_INTERWORKING */
}


/**
 * wpa_bss_anqp_alloc - Allocate ANQP data structure for a BSS entry
 * Returns: Allocated ANQP data structure or %NULL on failure
 *
 * The allocated ANQP data structure has its users count set to 1. It may be
 * shared by multiple BSS entries and each shared entry is freed with
 * wpa_bss_anqp_free().
 */
struct wpa_bss_anqp * wpa_bss_anqp_alloc(void)
{
	struct wpa_bss_anqp *anqp;
	anqp = os_zalloc(sizeof(*anqp));
	if (anqp == NULL)
		return NULL;
#ifdef CONFIG_INTERWORKING
	dl_list_init(&anqp->anqp_elems);
#endif /* CONFIG_INTERWORKING */
	anqp->users = 1;
	return anqp;
}


/**
 * wpa_bss_anqp_clone - Clone an ANQP data structure
 * @anqp: ANQP data structure from wpa_bss_anqp_alloc()
 * Returns: Cloned ANQP data structure or %NULL on failure
 */
static struct wpa_bss_anqp * wpa_bss_anqp_clone(struct wpa_bss_anqp *anqp)
{
	struct wpa_bss_anqp *n;

	n = os_zalloc(sizeof(*n));
	if (n == NULL)
		return NULL;

#define ANQP_DUP(f) if (anqp->f) n->f = wpabuf_dup(anqp->f)
#ifdef CONFIG_INTERWORKING
	dl_list_init(&n->anqp_elems);
	ANQP_DUP(capability_list);
	ANQP_DUP(venue_name);
	ANQP_DUP(network_auth_type);
	ANQP_DUP(roaming_consortium);
	ANQP_DUP(ip_addr_type_availability);
	ANQP_DUP(nai_realm);
	ANQP_DUP(anqp_3gpp);
	ANQP_DUP(domain_name);
	ANQP_DUP(fils_realm_info);
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	ANQP_DUP(hs20_capability_list);
	ANQP_DUP(hs20_operator_friendly_name);
	ANQP_DUP(hs20_wan_metrics);
	ANQP_DUP(hs20_connection_capability);
	ANQP_DUP(hs20_operating_class);
	ANQP_DUP(hs20_osu_providers_list);
	ANQP_DUP(hs20_operator_icon_metadata);
	ANQP_DUP(hs20_osu_providers_nai_list);
#endif /* CONFIG_HS20 */
#undef ANQP_DUP

	return n;
}


/**
 * wpa_bss_anqp_unshare_alloc - Unshare ANQP data (if shared) in a BSS entry
 * @bss: BSS entry
 * Returns: 0 on success, -1 on failure
 *
 * This function ensures the specific BSS entry has an ANQP data structure that
 * is not shared with any other BSS entry.
 */
int wpa_bss_anqp_unshare_alloc(struct wpa_bss *bss)
{
	struct wpa_bss_anqp *anqp;

	if (bss->anqp && bss->anqp->users > 1) {
		/* allocated, but shared - clone an unshared copy */
		anqp = wpa_bss_anqp_clone(bss->anqp);
		if (anqp == NULL)
			return -1;
		anqp->users = 1;
		bss->anqp->users--;
		bss->anqp = anqp;
		return 0;
	}

	if (bss->anqp)
		return 0; /* already allocated and not shared */

	/* not allocated - allocate a new storage area */
	bss->anqp = wpa_bss_anqp_alloc();
	return bss->anqp ? 0 : -1;
}


/**
 * wpa_bss_anqp_free - Free an ANQP data structure
 * @anqp: ANQP data structure from wpa_bss_anqp_alloc() or wpa_bss_anqp_clone()
 */
static void wpa_bss_anqp_free(struct wpa_bss_anqp *anqp)
{
#ifdef CONFIG_INTERWORKING
	struct wpa_bss_anqp_elem *elem;
#endif /* CONFIG_INTERWORKING */

	if (anqp == NULL)
		return;

	anqp->users--;
	if (anqp->users > 0) {
		/* Another BSS entry holds a pointer to this ANQP info */
		return;
	}

#ifdef CONFIG_INTERWORKING
	wpabuf_free(anqp->capability_list);
	wpabuf_free(anqp->venue_name);
	wpabuf_free(anqp->network_auth_type);
	wpabuf_free(anqp->roaming_consortium);
	wpabuf_free(anqp->ip_addr_type_availability);
	wpabuf_free(anqp->nai_realm);
	wpabuf_free(anqp->anqp_3gpp);
	wpabuf_free(anqp->domain_name);
	wpabuf_free(anqp->fils_realm_info);

	while ((elem = dl_list_first(&anqp->anqp_elems,
				     struct wpa_bss_anqp_elem, list))) {
		dl_list_del(&elem->list);
		wpabuf_free(elem->payload);
		os_free(elem);
	}
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	wpabuf_free(anqp->hs20_capability_list);
	wpabuf_free(anqp->hs20_operator_friendly_name);
	wpabuf_free(anqp->hs20_wan_metrics);
	wpabuf_free(anqp->hs20_connection_capability);
	wpabuf_free(anqp->hs20_operating_class);
	wpabuf_free(anqp->hs20_osu_providers_list);
	wpabuf_free(anqp->hs20_operator_icon_metadata);
	wpabuf_free(anqp->hs20_osu_providers_nai_list);
#endif /* CONFIG_HS20 */

	os_free(anqp);
}


static struct wpa_connect_work *
wpa_bss_check_pending_connect(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_radio_work *work;
	struct wpa_connect_work *cwork;

	work = radio_work_pending(wpa_s, "sme-connect");
	if (!work)
		work = radio_work_pending(wpa_s, "connect");
	if (!work)
		return NULL;

	cwork = work->ctx;
	if (cwork->bss != bss)
		return NULL;

	return cwork;
}


static void wpa_bss_update_pending_connect(struct wpa_connect_work *cwork,
					   struct wpa_bss *new_bss)
{
	wpa_printf(MSG_DEBUG,
		   "Update BSS pointer for the pending connect radio work");
	cwork->bss = new_bss;
	if (!new_bss)
		cwork->bss_removed = 1;
}


void wpa_bss_remove(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
		    const char *reason)
{
	struct wpa_connect_work *cwork;

	if (wpa_s->last_scan_res) {
		unsigned int i;
		for (i = 0; i < wpa_s->last_scan_res_used; i++) {
			if (wpa_s->last_scan_res[i] == bss) {
				os_memmove(&wpa_s->last_scan_res[i],
					   &wpa_s->last_scan_res[i + 1],
					   (wpa_s->last_scan_res_used - i - 1)
					   * sizeof(struct wpa_bss *));
				wpa_s->last_scan_res_used--;
				break;
			}
		}
	}
	cwork = wpa_bss_check_pending_connect(wpa_s, bss);
	if (cwork)
		wpa_bss_update_pending_connect(cwork, NULL);
	dl_list_del(&bss->list);
	dl_list_del(&bss->list_id);
	wpa_s->num_bss--;
	wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Remove id %u BSSID " MACSTR
		" SSID '%s' due to %s", bss->id, MAC2STR(bss->bssid),
		wpa_ssid_txt(bss->ssid, bss->ssid_len), reason);
	wpas_notify_bss_removed(wpa_s, bss->bssid, bss->id);
	wpa_bss_anqp_free(bss->anqp);
	os_free(bss);
}


/**
 * wpa_bss_get - Fetch a BSS table entry based on BSSID and SSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID, or %NULL to match any BSSID
 * @ssid: SSID
 * @ssid_len: Length of @ssid
 * Returns: Pointer to the BSS entry or %NULL if not found
 */
struct wpa_bss * wpa_bss_get(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     const u8 *ssid, size_t ssid_len)
{
	struct wpa_bss *bss;

	if (bssid && !wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if ((!bssid || ether_addr_equal(bss->bssid, bssid)) &&
		    bss->ssid_len == ssid_len &&
		    os_memcmp(bss->ssid, ssid, ssid_len) == 0)
			return bss;
	}
	return NULL;
}


void calculate_update_time(const struct os_reltime *fetch_time,
			   unsigned int age_ms,
			   struct os_reltime *update_time)
{
	os_time_t usec;

	update_time->sec = fetch_time->sec;
	update_time->usec = fetch_time->usec;
	update_time->sec -= age_ms / 1000;
	usec = (age_ms % 1000) * 1000;
	if (update_time->usec < usec) {
		update_time->sec--;
		update_time->usec += 1000000;
	}
	update_time->usec -= usec;
}


static void wpa_bss_copy_res(struct wpa_bss *dst, struct wpa_scan_res *src,
			     struct os_reltime *fetch_time)
{
	dst->flags = src->flags;
	os_memcpy(dst->bssid, src->bssid, ETH_ALEN);
	dst->freq = src->freq;
	dst->max_cw = src->max_cw;
	dst->beacon_int = src->beacon_int;
	dst->caps = src->caps;
	dst->qual = src->qual;
	dst->noise = src->noise;
	dst->level = src->level;
	dst->tsf = src->tsf;
	dst->beacon_newer = src->beacon_newer;
	dst->est_throughput = src->est_throughput;
	dst->snr = src->snr;

	calculate_update_time(fetch_time, src->age, &dst->last_update);
}


static int wpa_bss_is_wps_candidate(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss)
{
#ifdef CONFIG_WPS
	struct wpa_ssid *ssid;
	struct wpabuf *wps_ie;
	int pbc = 0, ret;

	wps_ie = wpa_bss_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
	if (!wps_ie)
		return 0;

	if (wps_is_selected_pbc_registrar(wps_ie)) {
		pbc = 1;
	} else if (!wps_is_addr_authorized(wps_ie, wpa_s->own_addr, 1)) {
		wpabuf_free(wps_ie);
		return 0;
	}

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!(ssid->key_mgmt & WPA_KEY_MGMT_WPS))
			continue;
		if (ssid->ssid_len &&
		    (ssid->ssid_len != bss->ssid_len ||
		     os_memcmp(ssid->ssid, bss->ssid, ssid->ssid_len) != 0))
			continue;

		if (pbc)
			ret = eap_is_wps_pbc_enrollee(&ssid->eap);
		else
			ret = eap_is_wps_pin_enrollee(&ssid->eap);
		wpabuf_free(wps_ie);
		return ret;
	}
	wpabuf_free(wps_ie);
#endif /* CONFIG_WPS */

	return 0;
}


static bool is_p2p_pending_bss(struct wpa_supplicant *wpa_s,
			       struct wpa_bss *bss)
{
#ifdef CONFIG_P2P
	u8 addr[ETH_ALEN];

	if (ether_addr_equal(bss->bssid, wpa_s->pending_join_iface_addr))
		return true;
	if (!is_zero_ether_addr(wpa_s->pending_join_dev_addr) &&
	    p2p_parse_dev_addr(wpa_bss_ie_ptr(bss), bss->ie_len, addr) == 0 &&
	    ether_addr_equal(addr, wpa_s->pending_join_dev_addr))
		return true;
#endif /* CONFIG_P2P */
	return false;
}


static int wpa_bss_known(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_ssid *ssid;

	if (is_p2p_pending_bss(wpa_s, bss))
		return 1;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (ssid->ssid == NULL || ssid->ssid_len == 0)
			continue;
		if (ssid->ssid_len == bss->ssid_len &&
		    os_memcmp(ssid->ssid, bss->ssid, ssid->ssid_len) == 0)
			return 1;
	}

	return 0;
}


static int wpa_bss_in_use(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	int i;

	if (bss == wpa_s->current_bss)
		return 1;

	if (bss == wpa_s->ml_connect_probe_bss)
		return 1;

#ifdef CONFIG_WNM
	if (bss == wpa_s->wnm_target_bss)
		return 1;
#endif /* CONFIG_WNM */

	if (wpa_s->current_bss &&
	    (bss->ssid_len != wpa_s->current_bss->ssid_len ||
	     os_memcmp(bss->ssid, wpa_s->current_bss->ssid,
		       bss->ssid_len) != 0))
		return 0; /* SSID has changed */

	if (!is_zero_ether_addr(bss->bssid) &&
	    (ether_addr_equal(bss->bssid, wpa_s->bssid) ||
	     ether_addr_equal(bss->bssid, wpa_s->pending_bssid)))
		return 1;

	if (!wpa_s->valid_links)
		return 0;

	for_each_link(wpa_s->valid_links, i) {
		if (ether_addr_equal(bss->bssid, wpa_s->links[i].bssid))
			return 1;
	}

	return 0;
}


static int wpa_bss_remove_oldest_unknown(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!wpa_bss_known(wpa_s, bss) &&
		    !wpa_bss_is_wps_candidate(wpa_s, bss)) {
			wpa_bss_remove(wpa_s, bss, __func__);
			return 0;
		}
	}

	return -1;
}


static int wpa_bss_remove_oldest(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;

	/*
	 * Remove the oldest entry that does not match with any configured
	 * network.
	 */
	if (wpa_bss_remove_oldest_unknown(wpa_s) == 0)
		return 0;

	/*
	 * Remove the oldest entry that isn't currently in use.
	 */
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!wpa_bss_in_use(wpa_s, bss)) {
			wpa_bss_remove(wpa_s, bss, __func__);
			return 0;
		}
	}

	return -1;
}


static struct wpa_bss * wpa_bss_add(struct wpa_supplicant *wpa_s,
				    const u8 *ssid, size_t ssid_len,
				    struct wpa_scan_res *res,
				    struct os_reltime *fetch_time)
{
	struct wpa_bss *bss;
	char extra[100];
	const u8 *ml_ie;
	char *pos, *end;
	int ret = 0;
	const u8 *mld_addr;

	bss = os_zalloc(sizeof(*bss) + res->ie_len + res->beacon_ie_len);
	if (bss == NULL)
		return NULL;
	bss->id = wpa_s->bss_next_id++;
	bss->last_update_idx = wpa_s->bss_update_idx;
	wpa_bss_copy_res(bss, res, fetch_time);
	os_memcpy(bss->ssid, ssid, ssid_len);
	bss->ssid_len = ssid_len;
	bss->ie_len = res->ie_len;
	bss->beacon_ie_len = res->beacon_ie_len;
	os_memcpy(bss->ies, res + 1, res->ie_len + res->beacon_ie_len);
	wpa_bss_set_hessid(bss);

	os_memset(bss->mld_addr, 0, ETH_ALEN);
	ml_ie = wpa_scan_get_ml_ie(res, MULTI_LINK_CONTROL_TYPE_BASIC);
	if (ml_ie) {
		mld_addr = get_basic_mle_mld_addr(&ml_ie[3], ml_ie[1] - 1);
		if (mld_addr)
			os_memcpy(bss->mld_addr, mld_addr, ETH_ALEN);
	}

	if (wpa_s->num_bss + 1 > wpa_s->conf->bss_max_count &&
	    wpa_bss_remove_oldest(wpa_s) != 0) {
		wpa_printf(MSG_ERROR, "Increasing the MAX BSS count to %d "
			   "because all BSSes are in use. We should normally "
			   "not get here!", (int) wpa_s->num_bss + 1);
		wpa_s->conf->bss_max_count = wpa_s->num_bss + 1;
	}

	dl_list_add_tail(&wpa_s->bss, &bss->list);
	dl_list_add_tail(&wpa_s->bss_id, &bss->list_id);
	wpa_s->num_bss++;

	extra[0] = '\0';
	pos = extra;
	end = pos + sizeof(extra);
	if (!is_zero_ether_addr(bss->hessid))
		ret = os_snprintf(pos, end - pos, " HESSID " MACSTR,
				  MAC2STR(bss->hessid));

	if (!is_zero_ether_addr(bss->mld_addr) &&
	    !os_snprintf_error(end - pos, ret)) {
		pos += ret;
		ret = os_snprintf(pos, end - pos, " MLD ADDR " MACSTR,
				  MAC2STR(bss->mld_addr));
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Add new id %u BSSID " MACSTR
		" SSID '%s' freq %d%s",
		bss->id, MAC2STR(bss->bssid), wpa_ssid_txt(ssid, ssid_len),
		bss->freq, extra);
	wpas_notify_bss_added(wpa_s, bss->bssid, bss->id);
	return bss;
}


static int are_ies_equal(const struct wpa_bss *old,
			 const struct wpa_scan_res *new_res, u32 ie)
{
	const u8 *old_ie, *new_ie;
	struct wpabuf *old_ie_buff = NULL;
	struct wpabuf *new_ie_buff = NULL;
	int new_ie_len, old_ie_len, ret, is_multi;

	switch (ie) {
	case WPA_IE_VENDOR_TYPE:
		old_ie = wpa_bss_get_vendor_ie(old, ie);
		new_ie = wpa_scan_get_vendor_ie(new_res, ie);
		is_multi = 0;
		break;
	case WPS_IE_VENDOR_TYPE:
		old_ie_buff = wpa_bss_get_vendor_ie_multi(old, ie);
		new_ie_buff = wpa_scan_get_vendor_ie_multi(new_res, ie);
		is_multi = 1;
		break;
	case WLAN_EID_RSN:
	case WLAN_EID_SUPP_RATES:
	case WLAN_EID_EXT_SUPP_RATES:
		old_ie = wpa_bss_get_ie(old, ie);
		new_ie = wpa_scan_get_ie(new_res, ie);
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

	if (!old_ie || !new_ie)
		ret = !old_ie && !new_ie;
	else
		ret = (old_ie_len == new_ie_len &&
		       os_memcmp(old_ie, new_ie, old_ie_len) == 0);

	wpabuf_free(old_ie_buff);
	wpabuf_free(new_ie_buff);

	return ret;
}


static u32 wpa_bss_compare_res(const struct wpa_bss *old,
			       const struct wpa_scan_res *new_res)
{
	u32 changes = 0;
	int caps_diff = old->caps ^ new_res->caps;

	if (old->freq != new_res->freq)
		changes |= WPA_BSS_FREQ_CHANGED_FLAG;

	if (old->level != new_res->level)
		changes |= WPA_BSS_SIGNAL_CHANGED_FLAG;

	if (caps_diff & IEEE80211_CAP_PRIVACY)
		changes |= WPA_BSS_PRIVACY_CHANGED_FLAG;

	if (caps_diff & IEEE80211_CAP_IBSS)
		changes |= WPA_BSS_MODE_CHANGED_FLAG;

	if (old->ie_len == new_res->ie_len &&
	    os_memcmp(wpa_bss_ie_ptr(old), new_res + 1, old->ie_len) == 0)
		return changes;
	changes |= WPA_BSS_IES_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WPA_IE_VENDOR_TYPE))
		changes |= WPA_BSS_WPAIE_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WLAN_EID_RSN))
		changes |= WPA_BSS_RSNIE_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WPS_IE_VENDOR_TYPE))
		changes |= WPA_BSS_WPS_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WLAN_EID_SUPP_RATES) ||
	    !are_ies_equal(old, new_res, WLAN_EID_EXT_SUPP_RATES))
		changes |= WPA_BSS_RATES_CHANGED_FLAG;

	return changes;
}


void notify_bss_changes(struct wpa_supplicant *wpa_s, u32 changes,
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

	wpas_notify_bss_seen(wpa_s, bss->id);
}


static struct wpa_bss *
wpa_bss_update(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
	       struct wpa_scan_res *res, struct os_reltime *fetch_time)
{
	u32 changes;

	if (bss->last_update_idx == wpa_s->bss_update_idx) {
		struct os_reltime update_time;

		/*
		 * Some drivers (e.g., cfg80211) include multiple BSS entries
		 * for the same BSS if that BSS's channel changes. The BSS list
		 * implementation in wpa_supplicant does not do that and we need
		 * to filter out the obsolete results here to make sure only the
		 * most current BSS information remains in the table.
		 */
		wpa_printf(MSG_DEBUG, "BSS: " MACSTR
			   " has multiple entries in the scan results - select the most current one",
			   MAC2STR(bss->bssid));
		calculate_update_time(fetch_time, res->age, &update_time);
		wpa_printf(MSG_DEBUG,
			   "Previous last_update: %u.%06u (freq %d%s)",
			   (unsigned int) bss->last_update.sec,
			   (unsigned int) bss->last_update.usec,
			   bss->freq,
			   (bss->flags & WPA_BSS_ASSOCIATED) ? " assoc" : "");
		wpa_printf(MSG_DEBUG, "New last_update: %u.%06u (freq %d%s)",
			   (unsigned int) update_time.sec,
			   (unsigned int) update_time.usec,
			   res->freq,
			   (res->flags & WPA_SCAN_ASSOCIATED) ? " assoc" : "");
		if ((bss->flags & WPA_BSS_ASSOCIATED) ||
		    (!(res->flags & WPA_SCAN_ASSOCIATED) &&
		     !os_reltime_before(&bss->last_update, &update_time))) {
			wpa_printf(MSG_DEBUG,
				   "Ignore this BSS entry since the previous update looks more current");
			return bss;
		}
		wpa_printf(MSG_DEBUG,
			   "Accept this BSS entry since it looks more current than the previous update");
	}

	changes = wpa_bss_compare_res(bss, res);
	if (changes & WPA_BSS_FREQ_CHANGED_FLAG)
		wpa_printf(MSG_DEBUG, "BSS: " MACSTR " changed freq %d --> %d",
			   MAC2STR(bss->bssid), bss->freq, res->freq);
	bss->scan_miss_count = 0;
	bss->last_update_idx = wpa_s->bss_update_idx;
	wpa_bss_copy_res(bss, res, fetch_time);
	/* Move the entry to the end of the list */
	dl_list_del(&bss->list);
#ifdef CONFIG_P2P
	if (wpa_bss_get_vendor_ie(bss, P2P_IE_VENDOR_TYPE) &&
	    !wpa_scan_get_vendor_ie(res, P2P_IE_VENDOR_TYPE) &&
	    !(changes & WPA_BSS_FREQ_CHANGED_FLAG)) {
		/*
		 * This can happen when non-P2P station interface runs a scan
		 * without P2P IE in the Probe Request frame. P2P GO would reply
		 * to that with a Probe Response that does not include P2P IE.
		 * Do not update the IEs in this BSS entry to avoid such loss of
		 * information that may be needed for P2P operations to
		 * determine group information.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Do not update scan IEs for "
			MACSTR " since that would remove P2P IE information",
			MAC2STR(bss->bssid));
	} else
#endif /* CONFIG_P2P */
	if (bss->ie_len + bss->beacon_ie_len >=
	    res->ie_len + res->beacon_ie_len) {
		os_memcpy(bss->ies, res + 1, res->ie_len + res->beacon_ie_len);
		bss->ie_len = res->ie_len;
		bss->beacon_ie_len = res->beacon_ie_len;
	} else {
		struct wpa_bss *nbss;
		struct dl_list *prev = bss->list_id.prev;
		struct wpa_connect_work *cwork;
		unsigned int i;
		bool update_current_bss = wpa_s->current_bss == bss;
		bool update_ml_probe_bss = wpa_s->ml_connect_probe_bss == bss;

		cwork = wpa_bss_check_pending_connect(wpa_s, bss);

		for (i = 0; i < wpa_s->last_scan_res_used; i++) {
			if (wpa_s->last_scan_res[i] == bss)
				break;
		}

		dl_list_del(&bss->list_id);
		nbss = os_realloc(bss, sizeof(*bss) + res->ie_len +
				  res->beacon_ie_len);
		if (nbss) {
			if (i != wpa_s->last_scan_res_used)
				wpa_s->last_scan_res[i] = nbss;

			if (update_current_bss)
				wpa_s->current_bss = nbss;

			if (update_ml_probe_bss)
				wpa_s->ml_connect_probe_bss = nbss;

			if (cwork)
				wpa_bss_update_pending_connect(cwork, nbss);

			bss = nbss;
			os_memcpy(bss->ies, res + 1,
				  res->ie_len + res->beacon_ie_len);
			bss->ie_len = res->ie_len;
			bss->beacon_ie_len = res->beacon_ie_len;
		}
		dl_list_add(prev, &bss->list_id);
	}
	if (changes & WPA_BSS_IES_CHANGED_FLAG) {
		const u8 *ml_ie, *mld_addr;

		wpa_bss_set_hessid(bss);
		os_memset(bss->mld_addr, 0, ETH_ALEN);
		ml_ie = wpa_scan_get_ml_ie(res, MULTI_LINK_CONTROL_TYPE_BASIC);
		if (ml_ie) {
			mld_addr = get_basic_mle_mld_addr(&ml_ie[3],
							  ml_ie[1] - 1);
			if (mld_addr)
				os_memcpy(bss->mld_addr, mld_addr, ETH_ALEN);
		}
	}
	dl_list_add_tail(&wpa_s->bss, &bss->list);

	notify_bss_changes(wpa_s, changes, bss);

	return bss;
}


/**
 * wpa_bss_update_start - Start a BSS table update from scan results
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is called at the start of each BSS table update round for new
 * scan results. The actual scan result entries are indicated with calls to
 * wpa_bss_update_scan_res() and the update round is finished with a call to
 * wpa_bss_update_end().
 */
void wpa_bss_update_start(struct wpa_supplicant *wpa_s)
{
	wpa_s->bss_update_idx++;
	wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Start scan result update %u",
		wpa_s->bss_update_idx);
	wpa_s->last_scan_res_used = 0;
}


/**
 * wpa_bss_update_scan_res - Update a BSS table entry based on a scan result
 * @wpa_s: Pointer to wpa_supplicant data
 * @res: Scan result
 * @fetch_time: Time when the result was fetched from the driver
 *
 * This function updates a BSS table entry (or adds one) based on a scan result.
 * This is called separately for each scan result between the calls to
 * wpa_bss_update_start() and wpa_bss_update_end().
 */
void wpa_bss_update_scan_res(struct wpa_supplicant *wpa_s,
			     struct wpa_scan_res *res,
			     struct os_reltime *fetch_time)
{
	const u8 *ssid, *p2p, *mesh;
	struct wpa_bss *bss;

	if (wpa_s->conf->ignore_old_scan_res) {
		struct os_reltime update;
		calculate_update_time(fetch_time, res->age, &update);
		if (os_reltime_before(&update, &wpa_s->scan_trigger_time)) {
			struct os_reltime age;
			os_reltime_sub(&wpa_s->scan_trigger_time, &update,
				       &age);
			wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Ignore driver BSS "
				"table entry that is %u.%06u seconds older "
				"than our scan trigger",
				(unsigned int) age.sec,
				(unsigned int) age.usec);
			return;
		}
	}

	ssid = wpa_scan_get_ie(res, WLAN_EID_SSID);
	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "BSS: No SSID IE included for "
			MACSTR, MAC2STR(res->bssid));
		return;
	}
	if (ssid[1] > SSID_MAX_LEN) {
		wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Too long SSID IE included for "
			MACSTR, MAC2STR(res->bssid));
		return;
	}

	p2p = wpa_scan_get_vendor_ie(res, P2P_IE_VENDOR_TYPE);
#ifdef CONFIG_P2P
	if (p2p == NULL &&
	    wpa_s->p2p_group_interface != NOT_P2P_GROUP_INTERFACE) {
		/*
		 * If it's a P2P specific interface, then don't update
		 * the scan result without a P2P IE.
		 */
		wpa_printf(MSG_DEBUG, "BSS: No P2P IE - skipping BSS " MACSTR
			   " update for P2P interface", MAC2STR(res->bssid));
		return;
	}
#endif /* CONFIG_P2P */
	if (p2p && ssid[1] == P2P_WILDCARD_SSID_LEN &&
	    os_memcmp(ssid + 2, P2P_WILDCARD_SSID, P2P_WILDCARD_SSID_LEN) == 0)
		return; /* Skip P2P listen discovery results here */

	/* TODO: add option for ignoring BSSes we are not interested in
	 * (to save memory) */

	mesh = wpa_scan_get_ie(res, WLAN_EID_MESH_ID);
	if (mesh && mesh[1] <= SSID_MAX_LEN)
		ssid = mesh;

	bss = wpa_bss_get(wpa_s, res->bssid, ssid + 2, ssid[1]);
	if (bss == NULL)
		bss = wpa_bss_add(wpa_s, ssid + 2, ssid[1], res, fetch_time);
	else {
		bss = wpa_bss_update(wpa_s, bss, res, fetch_time);
		if (wpa_s->last_scan_res) {
			unsigned int i;
			for (i = 0; i < wpa_s->last_scan_res_used; i++) {
				if (bss == wpa_s->last_scan_res[i]) {
					/* Already in the list */
					return;
				}
			}
		}
	}

	if (bss == NULL)
		return;
	if (wpa_s->last_scan_res_used >= wpa_s->last_scan_res_size) {
		struct wpa_bss **n;
		unsigned int siz;
		if (wpa_s->last_scan_res_size == 0)
			siz = 32;
		else
			siz = wpa_s->last_scan_res_size * 2;
		n = os_realloc_array(wpa_s->last_scan_res, siz,
				     sizeof(struct wpa_bss *));
		if (n == NULL)
			return;
		wpa_s->last_scan_res = n;
		wpa_s->last_scan_res_size = siz;
	}

	if (wpa_s->last_scan_res)
		wpa_s->last_scan_res[wpa_s->last_scan_res_used++] = bss;
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


/**
 * wpa_bss_update_end - End a BSS table update from scan results
 * @wpa_s: Pointer to wpa_supplicant data
 * @info: Information about scan parameters
 * @new_scan: Whether this update round was based on a new scan
 *
 * This function is called at the end of each BSS table update round for new
 * scan results. The start of the update was indicated with a call to
 * wpa_bss_update_start().
 */
void wpa_bss_update_end(struct wpa_supplicant *wpa_s, struct scan_info *info,
			int new_scan)
{
	struct wpa_bss *bss, *n;

	os_get_reltime(&wpa_s->last_scan);
	if ((info && info->aborted) || !new_scan)
		return; /* do not expire entries without new scan */

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;
		if (!wpa_bss_included_in_scan(bss, info))
			continue; /* expire only BSSes that were scanned */
		if (bss->last_update_idx < wpa_s->bss_update_idx)
			bss->scan_miss_count++;
		if (bss->scan_miss_count >=
		    wpa_s->conf->bss_expiration_scan_count) {
			wpa_bss_remove(wpa_s, bss, "no match in scan");
		}
	}

	wpa_printf(MSG_DEBUG, "BSS: last_scan_res_used=%zu/%zu",
		   wpa_s->last_scan_res_used, wpa_s->last_scan_res_size);
}


/**
 * wpa_bss_flush_by_age - Flush old BSS entries
 * @wpa_s: Pointer to wpa_supplicant data
 * @age: Maximum entry age in seconds
 *
 * Remove BSS entries that have not been updated during the last @age seconds.
 */
void wpa_bss_flush_by_age(struct wpa_supplicant *wpa_s, int age)
{
	struct wpa_bss *bss, *n;
	struct os_reltime t;

	if (dl_list_empty(&wpa_s->bss))
		return;

	os_get_reltime(&t);

	if (t.sec < age)
		return; /* avoid underflow; there can be no older entries */

	t.sec -= age;

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;

		if (wpa_s->reassoc_same_ess &&
		    wpa_s->wpa_state != WPA_COMPLETED &&
		    wpa_s->last_ssid &&
		    bss->ssid_len == wpa_s->last_ssid->ssid_len &&
		    os_memcmp(bss->ssid, wpa_s->last_ssid->ssid,
			      bss->ssid_len) == 0)
			continue;

		if (os_reltime_before(&bss->last_update, &t)) {
			wpa_bss_remove(wpa_s, bss, __func__);
		} else
			break;
	}
}


/**
 * wpa_bss_init - Initialize BSS table
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * This prepares BSS table lists and timer for periodic updates. The BSS table
 * is deinitialized with wpa_bss_deinit() once not needed anymore.
 */
int wpa_bss_init(struct wpa_supplicant *wpa_s)
{
	dl_list_init(&wpa_s->bss);
	dl_list_init(&wpa_s->bss_id);
	return 0;
}


/**
 * wpa_bss_flush - Flush all unused BSS entries
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_bss_flush(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss, *n;

	wpa_s->clear_driver_scan_cache = 1;

	if (wpa_s->bss.next == NULL)
		return; /* BSS table not yet initialized */

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;
		wpa_bss_remove(wpa_s, bss, __func__);
	}
}


/**
 * wpa_bss_deinit - Deinitialize BSS table
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_bss_deinit(struct wpa_supplicant *wpa_s)
{
	wpa_bss_flush(wpa_s);
}


/**
 * wpa_bss_get_bssid - Fetch a BSS table entry based on BSSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID
 * Returns: Pointer to the BSS entry or %NULL if not found
 */
struct wpa_bss * wpa_bss_get_bssid(struct wpa_supplicant *wpa_s,
				   const u8 *bssid)
{
	struct wpa_bss *bss;
	if (!wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each_reverse(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (ether_addr_equal(bss->bssid, bssid))
			return bss;
	}
	return NULL;
}


/**
 * wpa_bss_get_bssid_latest - Fetch the latest BSS table entry based on BSSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function is like wpa_bss_get_bssid(), but full BSS table is iterated to
 * find the entry that has the most recent update. This can help in finding the
 * correct entry in cases where the SSID of the AP may have changed recently
 * (e.g., in WPS reconfiguration cases).
 */
struct wpa_bss * wpa_bss_get_bssid_latest(struct wpa_supplicant *wpa_s,
					  const u8 *bssid)
{
	struct wpa_bss *bss, *found = NULL;
	if (!wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each_reverse(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!ether_addr_equal(bss->bssid, bssid))
			continue;
		if (found == NULL ||
		    os_reltime_before(&found->last_update, &bss->last_update))
			found = bss;
	}
	return found;
}


#ifdef CONFIG_P2P
/**
 * wpa_bss_get_p2p_dev_addr - Fetch the latest BSS table entry based on P2P Device Addr
 * @wpa_s: Pointer to wpa_supplicant data
 * @dev_addr: P2P Device Address of the GO
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function tries to find the entry that has the most recent update. This
 * can help in finding the correct entry in cases where the SSID of the P2P
 * Device may have changed recently.
 */
struct wpa_bss * wpa_bss_get_p2p_dev_addr(struct wpa_supplicant *wpa_s,
					  const u8 *dev_addr)
{
	struct wpa_bss *bss, *found = NULL;
	dl_list_for_each_reverse(bss, &wpa_s->bss, struct wpa_bss, list) {
		u8 addr[ETH_ALEN];
		if (p2p_parse_dev_addr(wpa_bss_ie_ptr(bss), bss->ie_len,
				       addr) != 0 ||
		    !ether_addr_equal(addr, dev_addr))
			continue;
		if (!found ||
		    os_reltime_before(&found->last_update, &bss->last_update))
			found = bss;
	}
	return found;
}
#endif /* CONFIG_P2P */


/**
 * wpa_bss_get_id - Fetch a BSS table entry based on identifier
 * @wpa_s: Pointer to wpa_supplicant data
 * @id: Unique identifier (struct wpa_bss::id) assigned for the entry
 * Returns: Pointer to the BSS entry or %NULL if not found
 */
struct wpa_bss * wpa_bss_get_id(struct wpa_supplicant *wpa_s, unsigned int id)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bss->id == id)
			return bss;
	}
	return NULL;
}


/**
 * wpa_bss_get_id_range - Fetch a BSS table entry based on identifier range
 * @wpa_s: Pointer to wpa_supplicant data
 * @idf: Smallest allowed identifier assigned for the entry
 * @idf: Largest allowed identifier assigned for the entry
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function is similar to wpa_bss_get_id() but allows a BSS entry with the
 * smallest id value to be fetched within the specified range without the
 * caller having to know the exact id.
 */
struct wpa_bss * wpa_bss_get_id_range(struct wpa_supplicant *wpa_s,
				      unsigned int idf, unsigned int idl)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss_id, struct wpa_bss, list_id) {
		if (bss->id >= idf && bss->id <= idl)
			return bss;
	}
	return NULL;
}


/**
 * wpa_bss_get_ie - Fetch a specified information element from a BSS entry
 * @bss: BSS table entry
 * @ie: Information element identitifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 */
const u8 * wpa_bss_get_ie(const struct wpa_bss *bss, u8 ie)
{
	return get_ie(wpa_bss_ie_ptr(bss), bss->ie_len, ie);
}


/**
 * wpa_bss_get_ie_beacon - Fetch a specified information element from a BSS entry
 * @bss: BSS table entry
 * @ie: Information element identitifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 *
 * This function is like wpa_bss_get_ie(), but uses IE buffer only from Beacon
 * frames instead of either Beacon or Probe Response frames.
 */
const u8 * wpa_bss_get_ie_beacon(const struct wpa_bss *bss, u8 ie)
{
	const u8 *ies;

	if (bss->beacon_ie_len == 0)
		return NULL;

	ies = wpa_bss_ie_ptr(bss);
	ies += bss->ie_len;
	return get_ie(ies, bss->beacon_ie_len, ie);
}


/**
 * wpa_bss_get_ie_ext - Fetch a specified extended IE from a BSS entry
 * @bss: BSS table entry
 * @ext: Information element extension identifier (WLAN_EID_EXT_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 */
const u8 * wpa_bss_get_ie_ext(const struct wpa_bss *bss, u8 ext)
{
	return get_ie_ext(wpa_bss_ie_ptr(bss), bss->ie_len, ext);
}


/**
 * wpa_bss_get_vendor_ie - Fetch a vendor information element from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 */
const u8 * wpa_bss_get_vendor_ie(const struct wpa_bss *bss, u32 vendor_type)
{
	const u8 *ies;
	const struct element *elem;

	ies = wpa_bss_ie_ptr(bss);

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies, bss->ie_len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


/**
 * wpa_bss_get_vendor_ie_beacon - Fetch a vendor information from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 *
 * This function is like wpa_bss_get_vendor_ie(), but uses IE buffer only
 * from Beacon frames instead of either Beacon or Probe Response frames.
 */
const u8 * wpa_bss_get_vendor_ie_beacon(const struct wpa_bss *bss,
					u32 vendor_type)
{
	const u8 *ies;
	const struct element *elem;

	if (bss->beacon_ie_len == 0)
		return NULL;

	ies = wpa_bss_ie_ptr(bss);
	ies += bss->ie_len;

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies,
			    bss->beacon_ie_len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


/**
 * wpa_bss_get_vendor_ie_multi - Fetch vendor IE data from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element payload or %NULL if not found
 *
 * This function returns concatenated payload of possibly fragmented vendor
 * specific information elements in the BSS entry. The caller is responsible for
 * freeing the returned buffer.
 */
struct wpabuf * wpa_bss_get_vendor_ie_multi(const struct wpa_bss *bss,
					    u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(bss->ie_len);
	if (buf == NULL)
		return NULL;

	pos = wpa_bss_ie_ptr(bss);
	end = pos + bss->ie_len;

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


/**
 * wpa_bss_get_vendor_ie_multi_beacon - Fetch vendor IE data from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element payload or %NULL if not found
 *
 * This function returns concatenated payload of possibly fragmented vendor
 * specific information elements in the BSS entry. The caller is responsible for
 * freeing the returned buffer.
 *
 * This function is like wpa_bss_get_vendor_ie_multi(), but uses IE buffer only
 * from Beacon frames instead of either Beacon or Probe Response frames.
 */
struct wpabuf * wpa_bss_get_vendor_ie_multi_beacon(const struct wpa_bss *bss,
						   u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(bss->beacon_ie_len);
	if (buf == NULL)
		return NULL;

	pos = wpa_bss_ie_ptr(bss);
	pos += bss->ie_len;
	end = pos + bss->beacon_ie_len;

	while (end - pos > 1) {
		u8 id, len;

		id = *pos++;
		len = *pos++;
		if (len > end - pos)
			break;
		if (id == WLAN_EID_VENDOR_SPECIFIC && len >= 4 &&
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


/**
 * wpa_bss_get_max_rate - Get maximum legacy TX rate supported in a BSS
 * @bss: BSS table entry
 * Returns: Maximum legacy rate in units of 500 kbps
 */
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


/**
 * wpa_bss_get_bit_rates - Get legacy TX rates supported in a BSS
 * @bss: BSS table entry
 * @rates: Buffer for returning a pointer to the rates list (units of 500 kbps)
 * Returns: number of legacy TX rates or -1 on failure
 *
 * The caller is responsible for freeing the returned buffer with os_free() in
 * case of success.
 */
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


#ifdef CONFIG_FILS
const u8 * wpa_bss_get_fils_cache_id(const struct wpa_bss *bss)
{
	const u8 *ie;

	if (bss) {
		ie = wpa_bss_get_ie(bss, WLAN_EID_FILS_INDICATION);
		if (ie && ie[1] >= 4 && WPA_GET_LE16(ie + 2) & BIT(7))
			return ie + 4;
	}

	return NULL;
}
#endif /* CONFIG_FILS */


int wpa_bss_ext_capab(const struct wpa_bss *bss, unsigned int capab)
{
	if (!bss)
		return 0;
	return ieee802_11_ext_capab(wpa_bss_get_ie(bss, WLAN_EID_EXT_CAPAB),
				    capab);
}


static void
wpa_bss_parse_ml_rnr_ap_info(struct wpa_supplicant *wpa_s,
			     struct wpa_bss *bss, u8 mbssid_idx,
			     const struct ieee80211_neighbor_ap_info *ap_info,
			     size_t len, u16 *seen, u16 *missing,
			     struct wpa_ssid *ssid)
{
	const u8 *pos, *end;
	const u8 *mld_params;
	u8 count, mld_params_offset;
	u8 i, type, link_id;

	count = RNR_TBTT_INFO_COUNT_VAL(ap_info->tbtt_info_hdr) + 1;
	type = ap_info->tbtt_info_hdr & RNR_TBTT_INFO_HDR_TYPE_MSK;

	/* MLD information is at offset 13 or at start */
	if (type == 0 && ap_info->tbtt_info_len >= RNR_TBTT_INFO_MLD_LEN) {
		/* MLD info is appended */
		mld_params_offset = RNR_TBTT_INFO_LEN;
	} else {
		/* TODO: Support NSTR AP */
		return;
	}

	pos = (const u8 *) ap_info;
	end = pos + len;
	pos += sizeof(*ap_info);

	for (i = 0; i < count; i++) {
		u8 bss_params;

		if (end - pos < ap_info->tbtt_info_len)
			break;

		bss_params = pos[1 + ETH_ALEN + 4];
		mld_params = pos + mld_params_offset;

		link_id = *(mld_params + 1) & EHT_ML_LINK_ID_MSK;
		if (link_id >= MAX_NUM_MLD_LINKS)
			return;

		if (*mld_params != mbssid_idx) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Reported link not part of MLD");
		} else if (!(BIT(link_id) & *seen)) {
			struct wpa_bss *neigh_bss;

			if (ssid && ssid->ssid_len)
				neigh_bss = wpa_bss_get(wpa_s, pos + 1,
							ssid->ssid,
							ssid->ssid_len);
			else
				neigh_bss = wpa_bss_get_bssid(wpa_s, pos + 1);

			*seen |= BIT(link_id);
			wpa_printf(MSG_DEBUG, "MLD: mld ID=%u, link ID=%u",
				   *mld_params, link_id);

			if (!neigh_bss) {
				*missing |= BIT(link_id);
			} else if ((!ssid ||
				    (bss_params & (RNR_BSS_PARAM_SAME_SSID |
						   RNR_BSS_PARAM_CO_LOCATED)) ||
				    wpa_scan_res_match(wpa_s, 0, neigh_bss,
						       ssid, 1, 0)) &&
				   !wpa_bssid_ignore_is_listed(
					   wpa_s, neigh_bss->bssid)) {
				struct mld_link *l;

				bss->valid_links |= BIT(link_id);
				l = &bss->mld_links[link_id];
				os_memcpy(l->bssid, pos + 1, ETH_ALEN);
				l->freq = neigh_bss->freq;
				l->disabled = mld_params[2] &
					RNR_TBTT_INFO_MLD_PARAM2_LINK_DISABLED;
			}
		}

		pos += ap_info->tbtt_info_len;
	}
}


/**
 * wpa_bss_parse_basic_ml_element - Parse the Basic Multi-Link element
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: BSS table entry
 * @mld_addr: AP MLD address (or %NULL)
 * @link_info: Array to store link information (or %NULL),
 *   should be initialized and #MAX_NUM_MLD_LINKS elements long
 * @missing_links: Result bitmask of links that were not discovered (or %NULL)
 * @ssid: Target SSID (or %NULL)
 * @ap_mld_id: On return would hold the corresponding AP MLD ID (or %NULL)
 * Returns: 0 on success or -1 for non-MLD or parsing failures
 *
 * Parses the Basic Multi-Link element of the BSS into @link_info using the scan
 * information stored in the wpa_supplicant data to fill in information for
 * links where possible. The @missing_links out parameter will contain any links
 * for which no corresponding BSS was found.
 */
int wpa_bss_parse_basic_ml_element(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *bss,
				   u8 *ap_mld_addr,
				   u16 *missing_links,
				   struct wpa_ssid *ssid,
				   u8 *ap_mld_id)
{
	struct ieee802_11_elems elems;
	struct wpabuf *mlbuf;
	const struct element *elem;
	u8 mbssid_idx = 0;
	size_t ml_ie_len;
	const struct ieee80211_eht_ml *eht_ml;
	const struct eht_ml_basic_common_info *ml_basic_common_info;
	u8 i, link_id;
	const u16 control_mask =
		MULTI_LINK_CONTROL_TYPE_MASK |
		BASIC_MULTI_LINK_CTRL_PRES_LINK_ID |
		BASIC_MULTI_LINK_CTRL_PRES_BSS_PARAM_CH_COUNT |
		BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA;
	const u16 control =
		MULTI_LINK_CONTROL_TYPE_BASIC |
		BASIC_MULTI_LINK_CTRL_PRES_LINK_ID |
		BASIC_MULTI_LINK_CTRL_PRES_BSS_PARAM_CH_COUNT |
		BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA;
	u16 missing = 0;
	u16 seen;
	const u8 *ies_pos = wpa_bss_ie_ptr(bss);
	size_t ies_len = bss->ie_len ? bss->ie_len : bss->beacon_ie_len;
	int ret = -1;
	struct mld_link *l;

	if (ieee802_11_parse_elems(ies_pos, ies_len, &elems, 1) ==
	    ParseFailed) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: Failed to parse elements");
		return ret;
	}

	mlbuf = ieee802_11_defrag(elems.basic_mle, elems.basic_mle_len, true);
	if (!mlbuf) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No Multi-Link element");
		return ret;
	}

	ml_ie_len = wpabuf_len(mlbuf);

	if (ssid) {
		struct wpa_ie_data ie;

		if (!elems.rsn_ie ||
		    wpa_parse_wpa_ie(elems.rsn_ie - 2, 2 + elems.rsn_ie_len,
				     &ie)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No RSN element");
			goto out;
		}

		if (!(ie.capabilities & WPA_CAPABILITY_MFPC) ||
		    wpas_get_ssid_pmf(wpa_s, ssid) == NO_MGMT_FRAME_PROTECTION) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: No management frame protection");
			goto out;
		}

		ie.key_mgmt &= ~(WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_FT_PSK |
				 WPA_KEY_MGMT_PSK_SHA256);
		if (!(ie.key_mgmt & ssid->key_mgmt)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: No valid key management");
			goto out;
		}
	}

	/*
	 * for ext ID + 2 control + common info len + MLD address +
	 * link info
	 */
	if (ml_ie_len < 2UL + 1UL + ETH_ALEN + 1UL)
		goto out;

	eht_ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	if ((le_to_host16(eht_ml->ml_control) & control_mask) != control) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Unexpected Multi-Link element control=0x%x (mask 0x%x expected 0x%x)",
			   le_to_host16(eht_ml->ml_control), control_mask,
			   control);
		goto out;
	}

	ml_basic_common_info =
		(const struct eht_ml_basic_common_info *) eht_ml->variable;

	/* Common info length should be valid */
	if (ml_basic_common_info->len < ETH_ALEN + 1UL)
		goto out;

	/* Get the MLD address and MLD link ID */
	if (ap_mld_addr)
		os_memcpy(ap_mld_addr, ml_basic_common_info->mld_addr,
			  ETH_ALEN);

	link_id = ml_basic_common_info->variable[0] & EHT_ML_LINK_ID_MSK;

	bss->mld_link_id = link_id;
	seen = bss->valid_links = BIT(link_id);

	l = &bss->mld_links[link_id];
	os_memcpy(l->bssid, bss->bssid, ETH_ALEN);
	l->freq = bss->freq;


	/*
	 * The AP MLD ID in the RNR corresponds to the MBSSID index, see
	 * IEEE P802.11be/D4.0, 9.4.2.169.2 (Neighbor AP Information field).
	 *
	 * For the transmitting BSSID it is clear that both the MBSSID index
	 * and the AP MLD ID in the RNR are zero.
	 *
	 * For nontransmitted BSSIDs we will have a BSS generated from the
	 * MBSSID element(s) using inheritance rules. Included in the elements
	 * is the MBSSID Index Element. The RNR is copied from the Beacon/Probe
	 * Response frame that was send by the transmitting BSSID. As such, the
	 * reported AP MLD ID in the RNR will match the value in the MBSSID
	 * Index Element.
	 */
	elem = (const struct element *)
		wpa_bss_get_ie(bss, WLAN_EID_MULTIPLE_BSSID_INDEX);
	if (elem && elem->datalen >= 1)
		mbssid_idx = elem->data[0];

	for_each_element_id(elem, WLAN_EID_REDUCED_NEIGHBOR_REPORT,
			    wpa_bss_ie_ptr(bss),
			    bss->ie_len ? bss->ie_len : bss->beacon_ie_len) {
		const struct ieee80211_neighbor_ap_info *ap_info;
		const u8 *pos = elem->data;
		size_t len = elem->datalen;

		/* RNR IE may contain more than one Neighbor AP Info */
		while (sizeof(*ap_info) <= len) {
			size_t ap_info_len = sizeof(*ap_info);
			u8 count;

			ap_info = (const struct ieee80211_neighbor_ap_info *)
				pos;
			count = RNR_TBTT_INFO_COUNT_VAL(ap_info->tbtt_info_hdr) + 1;
			ap_info_len += count * ap_info->tbtt_info_len;

			if (ap_info_len > len)
				goto out;

			wpa_bss_parse_ml_rnr_ap_info(wpa_s, bss, mbssid_idx,
						     ap_info, len, &seen,
						     &missing, ssid);

			pos += ap_info_len;
			len -= ap_info_len;
		}
	}

	wpa_printf(MSG_DEBUG, "MLD: valid_links=%04hx (unresolved: 0x%04hx)",
		   bss->valid_links, missing);

	for_each_link(bss->valid_links, i) {
		wpa_printf(MSG_DEBUG, "MLD: link=%u, bssid=" MACSTR,
			   i, MAC2STR(bss->mld_links[i].bssid));
	}

	if (missing_links)
		*missing_links = missing;

	if (ap_mld_id)
		*ap_mld_id = mbssid_idx;

	ret = 0;
out:
	wpabuf_free(mlbuf);
	return ret;
}


/*
 * wpa_bss_parse_reconf_ml_element - Parse the Reconfiguration ML element
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: BSS table entry
 * Returns: The bitmap of links that are going to be removed
 */
u16 wpa_bss_parse_reconf_ml_element(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss)
{
	struct ieee802_11_elems elems;
	struct wpabuf *mlbuf;
	const u8 *pos = wpa_bss_ie_ptr(bss);
	size_t len = bss->ie_len ? bss->ie_len : bss->beacon_ie_len;
	const struct ieee80211_eht_ml *ml;
	u16 removed_links = 0;
	u8 ml_common_len;

	if (ieee802_11_parse_elems(pos, len, &elems, 1) == ParseFailed)
		return 0;

	if (!elems.reconf_mle || !elems.reconf_mle_len)
		return 0;

	mlbuf = ieee802_11_defrag(elems.reconf_mle, elems.reconf_mle_len, true);
	if (!mlbuf)
		return 0;

	ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	len = wpabuf_len(mlbuf);

	if (len < sizeof(*ml))
		goto out;

	ml_common_len = 1;
	if (ml->ml_control & RECONF_MULTI_LINK_CTRL_PRES_MLD_MAC_ADDR)
		ml_common_len += ETH_ALEN;

	if (len < sizeof(*ml) + ml_common_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Unexpected Reconfiguration ML element length: (%zu < %zu)",
			   len, sizeof(*ml) + ml_common_len);
		goto out;
	}

	pos = ml->variable + ml_common_len;
	len -= sizeof(*ml) + ml_common_len;

	while (len >= 2 + sizeof(struct ieee80211_eht_per_sta_profile)) {
		size_t sub_elem_len = *(pos + 1);

		if (2 + sub_elem_len > len) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Invalid link info len: %zu %zu",
				   2 + sub_elem_len, len);
			goto out;
		}

		if  (*pos == EHT_ML_SUB_ELEM_PER_STA_PROFILE) {
			const struct ieee80211_eht_per_sta_profile *sta_prof =
				(const struct ieee80211_eht_per_sta_profile *)
				(pos + 2);
			u16 control = le_to_host16(sta_prof->sta_control);
			u8 link_id;

			link_id = control & EHT_PER_STA_RECONF_CTRL_LINK_ID_MSK;
			removed_links |= BIT(link_id);
		}

		pos += 2 + sub_elem_len;
		len -= 2 + sub_elem_len;
	}

	wpa_printf(MSG_DEBUG, "MLD: Reconfiguration: removed_links=0x%x",
		   removed_links);
out:
	wpabuf_free(mlbuf);
	return removed_links;
}
